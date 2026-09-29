#include <kernel.h>
#include <kdata.h>
#include <printf.h>
#include <timer.h>
#include <blkdev.h>
#include <devide_sunrise.h>
#include <msx2.h>

extern uint16_t ide_error;
extern uint16_t ide_base;
extern uint8_t ide_slot;
extern uint16_t ide_addr;
extern uint32_t ide_lba;
extern uint8_t ide_is_read;
uint8_t *devide_buf;
/* Bounce buffer for a sector straddling two 16K pages of a process
   (msx2.s, in the top page next to the scratch buffers) */
extern uint8_t ide_bounce[BLKSIZE];

uint8_t sunrise_transfer_sector(void)
{
    uint8_t drive = (blk_op.blkdev->driver_data & IDE_DRIVE_NR_MASK);
    uint8_t mask = drive ? 0xF0 : 0xE0;
    uint8_t *addr = blk_op.addr;
    uint16_t status;

    ide_lba = blk_op.lba;
    ide_is_read = blk_op.is_read;

    if (!ide_is_read)
        blk_op.blkdev->driver_data |= FLAG_CACHE_DIRTY;

    irqflags_t irq = di();
    if (blk_op.is_user == 2) {
        /* Swap: the process page is blk_op.swap_page, the address is where
           it sits in the process map (swap_map, config.h): bring the page in
           at 0x8000, the IDE registers being at 0x4000 */
        ide_addr = ((uint16_t) addr & 0x3FFF) + 0x8000;
        RAM_PAGE2 = blk_op.swap_page;
        status = do_ide_xfer(mask);
        RAM_PAGE2 = 1;
    } else if (blk_op.is_user) {
        /* The process page holding the sector goes at 0x8000 */
        uint8_t *pages = (uint8_t *)&udata.u_page;
        uint16_t off = (uint16_t) addr & 0x3FFF;
        uint8_t page = pages[(uint16_t) addr >> 14];
        if (off <= 0x4000 - BLKSIZE) {
            ide_addr = off + 0x8000;
            RAM_PAGE2 = page;
            status = do_ide_xfer(mask);
        } else {
            /* The sector straddles two pages: transfer through the bounce
               buffer, and copy each part from or to its own page. (This
               used to spill into 0xC000-0xC1FF, the current process's own
               memory.) */
            uint16_t first = 0x4000 - off;
            uint8_t next = pages[((uint16_t) addr >> 14) + 1];
            if (!ide_is_read) {
                RAM_PAGE2 = page;
                memcpy(ide_bounce, (uint8_t *)0x8000 + off, first);
                RAM_PAGE2 = next;
                memcpy(ide_bounce + first, (uint8_t *)0x8000, BLKSIZE - first);
            }
            ide_addr = (uint16_t) ide_bounce;
            status = do_ide_xfer(mask);
            if (ide_is_read) {
                RAM_PAGE2 = page;
                memcpy((uint8_t *)0x8000 + off, ide_bounce, first);
                RAM_PAGE2 = next;
                memcpy((uint8_t *)0x8000, ide_bounce + first, BLKSIZE - first);
            }
        }
        RAM_PAGE2 = 1;
    } else if ((uint16_t)addr >= 0x4000 && (uint16_t)addr < 0x8000) {
        /* A disk buffer (__bf_data is its address with the buffer page at
           0x4000, where kernel data never sits, see extbuffer.c): the page
           goes at 0x8000 instead, the IDE registers being at 0x4000 */
        ide_addr = (uint16_t) addr + 0x4000;
        RAM_PAGE2 = BUFFER_PAGE;
        status = do_ide_xfer(mask);
        RAM_PAGE2 = 1;
    } else {
        /* Other kernel memory (tmpbuf for the partition table...) */
        ide_addr = (uint16_t) addr;
        status = do_ide_xfer(mask);
    }
    irqrestore(irq);

    if (status != 0) {
        if (ide_error == 0xFF)
            kprintf("ide%d: timeout.\n", drive);
        else
            kprintf("ide%d: status %x\n", drive, ide_error);
        return 0;
    }
    return 1;
}

int sunrise_flush_cache(void)
{
    uint8_t drive;

    drive = blk_op.blkdev->driver_data & IDE_DRIVE_NR_MASK;
    /* check drive has a cache and was written to since the last flush */
    if((blk_op.blkdev->driver_data & (FLAG_WRITE_CACHE | FLAG_CACHE_DIRTY))
		                 != (FLAG_WRITE_CACHE | FLAG_CACHE_DIRTY))
        return 0;

    if (do_ide_flush_cache(drive ? 0xF0 : 0xE0)) {
        udata.u_error = EIO;
        return -1;
    }
    return 0;
}
