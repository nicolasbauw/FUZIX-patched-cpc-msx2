/* 
 * CPC standard RAM bank memory expansions ramdisc driver, based on platform zxdiv48.
 */

#include <kernel.h>
#include <kdata.h>
#include <printf.h>

#if defined EXTENDED_RAM_512 || defined EXTENDED_RAM_1024 || defined EXTENDED_RAM_3584
#include <devrd.h>
#undef DEBUG

static int rd_transfer(uint8_t is_read, uint8_t rawflag)
{
    nblock = udata.u_nblock;
    block = udata.u_block;
    rd_dptr = udata.u_dptr;
    rd_wr = is_read;
    uint16_t page, section, bank, blk;
    uint8_t ct = 0;

    #ifdef DEBUG
        kprintf("u_dptr %p Block %u u_nblock %u rd_wr %u\n",udata.u_dptr, udata.u_block, udata.u_nblock, rd_wr);
    #endif

    /* It's a disk but only for swapping (and rd_io isn't general purpose) */
    if (((block + nblock) > (TOTAL_SWAP_BLOCKS - 1)) || (rawflag == 1))  {
        udata.u_error = EIO;
            kprintf("dev_rd_EIO");
        return -1;
    }

    /* udata could change under us so keep variables privately */
    while (ct < nblock) {
        /* Page (16K) -> (section, bank, block) on the "by section" memory
         * expansion protocol: 7 ports (0x78-0x7E, one per section) x 8
         * banks x 4 sub-banks of 16K = 56 groups of 64K, the addressable
         * maximum this platform's memory emulation implements (port 0x7F
         * stays the plain Gate Array RAM config register, never a section -
         * see amstrad_cpc/core/src/memory.rs). This replaces the previous
         * formula, which only reached 15 of those 56 groups by overflowing
         * into a single section past the standard port's 8-bit register. */
        page = block >> 5;
        section = page / 32;
        bank = (page % 32) / 4;
        blk = page & 3;
        rd_swap_mem_port_h = 0x7e - section;
        /* Bit 2 stays forced to 1: the real Gate Array only decodes bits
         * 15-14 of the port address, so this write also lands on the
         * standard RAM config register (value & 0x07) as a side effect.
         * Config 2 remaps page 0 (0x0000-0x3FFF) to bank 4, which is where
         * the kernel's own globals live while this routine runs - forcing
         * bit 2 keeps the low 3 bits in 4-7, all of which leave page 0/2
         * alone (see amstrad_cpc/core/src/memory.rs standard_bank_mapping),
         * exactly like the previous formula did by construction. */
        rd_swap_bank = 0xc4 | (bank << 3) | blk;
        rd_swap_bank_addr = ((block & 31) << BLKSHIFT) + 0x4000;
        switch (rawflag){
            case 0:
                rd_proc_bank = 0xc1;
                rd_proc_bank_addr = (uint16_t)rd_dptr;
                break;
            case 2:
                rd_proc_bank = ((uint16_t)rd_dptr / 0x4000) + 0xc4;
                rd_proc_bank_addr = ((uint16_t)rd_dptr & 0x3fff) + 0x4000;
                break;
        }
        
        #ifdef DEBUG
            if (nblock == 1)
                kprintf("swap_bank %p swap_addr %p proc_bank %p proc_addr %p count %u\n", rd_swap_bank, rd_swap_bank_addr, rd_proc_bank, rd_proc_bank_addr, ct);
        #endif
        rd_io();        
        block++;
        rd_dptr += BLKSIZE;
        ct++;
    }
    return ct << BLKSHIFT; /*Total bytes transferred*/
}

int rd_open(uint8_t minor, uint16_t flag)
{
    flag;
    if(minor != 0) {
        udata.u_error = ENODEV;
        return -1;
    }
    return 0;
}

int rd_read(uint8_t minor, uint8_t rawflag, uint8_t flag)
{
    flag;minor;
    return rd_transfer(true, rawflag);
}

int rd_write(uint8_t minor, uint8_t rawflag, uint8_t flag)
{
    flag;minor;
    return rd_transfer(false, rawflag);
}
#endif
