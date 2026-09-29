/*
 * TEXT2 mode VT: set up at boot only, so kept in the discard area (see
 * vdp.c). vt_offset starts at zero with the rest of the kernel data.
 */

#include <kernel.h>
#include <vt.h>
#include <v99xx.h>
#include <printf.h>

#define VT_BASE 0x0000
#define VT_BASE_FONT 0x1000
#define VT_BASE_BLINK 0x800
#define VT_BUFSIZE 80
#define VT_OFFSET 0x2000

extern void *fontdata_6x8;

void vdpinit(void)
{
    uint8_t i;

    v99xx_set_mode(MODE_TEXT2);
    v99xx_set_color(15, 4);
    v99xx_copy_to_vram(VT_BASE_FONT + 32*8, (uint8_t *)&fontdata_6x8, 768);
    v99xx_set_blink_color(15, 8);
    v99xx_set_blink_period(4, 4);

    /* clean vram for all vt's */
    for (i = 0; i < MAX_VT; i++) {
        v99xx_set_vram_page(i/2);
        v99xx_memset_vram(VT_BASE + i * VT_OFFSET, ' ',  VT_HEIGHT * VT_WIDTH);
        v99xx_memset_vram(VT_BASE_BLINK + i * VT_OFFSET, 0,
                VT_HEIGHT * VT_WIDTH / 8);
    }
}
