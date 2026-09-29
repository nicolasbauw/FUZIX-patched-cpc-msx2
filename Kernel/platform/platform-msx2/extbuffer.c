/*
 *	Disk buffers in their own mapper page (CONFIG_BLKBUF_EXTERNAL)
 *
 *	The top 16K page, where the buffers used to be, belongs to each process
 *	(it holds its common copy and udata), so every process carried its own
 *	copy of the buffer cache. Mapper page 0, free once the kernel is up,
 *	now holds all of them (32 x 512 bytes), shared. A buffer's __bf_data is
 *	its address when that page is mapped at 0x4000 (see map_init).
 *
 *	The copies run with interrupts off, through extbuf.s in the low 16K.
 */

#include <kernel.h>
#include <kdata.h>
#include <printf.h>
#include "msx2.h"

extern uint8_t *xb_src;
extern uint8_t *xb_dst;
extern uint16_t xb_len;
extern uint8_t xb_page1;
extern uint8_t xb_page2;
extern void extbuf_copy(void);
extern void extbuf_zero(void);

/* Kernel data never sits in 0x4000-0x7FFF (kernel code, and the window the
   buffer page is mapped into): catch a caller breaking that rule */
static void kcheck(void *kaddr, uint16_t len)
{
    if ((uint16_t)kaddr + len > 0x4000 && (uint16_t)kaddr < 0x8000)
        panic("extbuf");
}

static void kcopy(uint8_t *src, uint8_t *dst, uint16_t len)
{
    irqflags_t irq = di();
    xb_page1 = BUFFER_PAGE;
    xb_page2 = 1;
    xb_src = src;
    xb_dst = dst;
    xb_len = len;
    extbuf_copy();
    irqrestore(irq);
}

void blktok(void *kaddr, struct blkbuf *buf, uint16_t off, uint16_t len)
{
    kcheck(kaddr, len);
    kcopy(buf->__bf_data + off, kaddr, len);
}

void blkfromk(void *kaddr, struct blkbuf *buf, uint16_t off, uint16_t len)
{
    kcheck(kaddr, len);
    kcopy(kaddr, buf->__bf_data + off, len);
}

/* User memory: map the process page holding each chunk at 0x8000, the
   buffer page at 0x4000, one 16K user page at a time */
static void ucopy(uint8_t *uaddr, uint8_t *bufaddr, uint16_t len, uint8_t to_user)
{
    uint8_t *pages = (uint8_t *)&udata.u_page;
    while (len) {
        uint16_t o = (uint16_t)uaddr & 0x3FFF;
        uint16_t n = 0x4000 - o;
        irqflags_t irq;
        if (n > len)
            n = len;
        irq = di();
        xb_page1 = BUFFER_PAGE;
        xb_page2 = pages[(uint16_t)uaddr >> 14];
        if (to_user) {
            xb_src = bufaddr;
            xb_dst = (uint8_t *)(0x8000 + o);
        } else {
            xb_src = (uint8_t *)(0x8000 + o);
            xb_dst = bufaddr;
        }
        xb_len = n;
        extbuf_copy();
        irqrestore(irq);
        uaddr += n;
        bufaddr += n;
        len -= n;
    }
}

void blktou(void *uaddr, struct blkbuf *buf, uint16_t off, uint16_t len)
{
    ucopy(uaddr, buf->__bf_data + off, len, 1);
}

void blkfromu(void *uaddr, struct blkbuf *buf, uint16_t off, uint16_t len)
{
    ucopy(uaddr, buf->__bf_data + off, len, 0);
}

/* The kernel only reads small structures through this (directory entries,
   inodes, block numbers): a copy is enough */
extern uint8_t blkscratch[64];

void *blkptr(struct blkbuf *buf, uint16_t offset, uint16_t len)
{
    if (len > sizeof(blkscratch))
        panic("blkptr");
    kcopy(buf->__bf_data + offset, blkscratch, len);
    return blkscratch;
}

void blkzero(struct blkbuf *buf)
{
    irqflags_t irq = di();
    xb_page1 = BUFFER_PAGE;
    xb_dst = buf->__bf_data;
    extbuf_zero();
    irqrestore(irq);
}

/*
 *	Scratch buffers for the kernel (exec arguments, IDE identify...), no
 *	longer carved out of the buffer cache. They sit in the top page, so each
 *	process has its own copy: fine, a scratch buffer never outlives the
 *	system call that took it.
 */
extern uint8_t workbuf[2][BLKSIZE];
static uint8_t tfree = 3;

void *tmpbuf(void)
{
    if (tfree & 1) {
        tfree &= ~1;
        return workbuf[0];
    }
    if (tfree & 2) {
        tfree &= ~2;
        return workbuf[1];
    }
    panic("tmpbuf");
    return NULL;
}

void tmpfree(void *p)
{
    if (p == workbuf[0])
        tfree |= 1;
    else if (p == workbuf[1])
        tfree |= 2;
    else
        panic("tmpfree");
}
