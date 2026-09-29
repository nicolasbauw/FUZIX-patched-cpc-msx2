#include <kernel.h>
#include <kdata.h>
#include <devsd.h>
#include <devide_sunrise.h>
#include <printf.h>
#include <vt.h>
#include <tty.h>
#include "msx2.h"
#include "devtty.h"
#include "kbdmatrix.h"

extern int megasd_probe(void);

void device_init(void)
{
    uint8_t kbdtype;

#ifdef CONFIG_RTC
    inittod();
#endif

    if (machine_type == MACHINE_MSX1) {
	panic("MSX1 not supported\n");
    } else if (machine_type == MACHINE_MSX2) {
	kprintf("MSX2");
    } else if (machine_type == MACHINE_MSX2P) {
        kprintf("MSX2+");
    } else if (machine_type == MACHINE_MSXTR) {
	kprintf("MSX TurboR");
    }

    /* keyboard layout initialization: default is international,
     * localized variations overlay on top */
    memcpy(keyboard, keyboard_int, sizeof(keyboard_int));
    memcpy(shiftkeyboard, shiftkeyboard_int, sizeof(shiftkeyboard_int));

    /* The keyboard type is the BIOS byte at 0x002C, the high byte of
       infobits (the low byte is 0x002B: character set and video frequency) */
    kbdtype = (infobits >> 8) & KBDTYPE_MASK;
    if (kbdtype == KBDTYPE_JPN) {
	kprintf("(JPN)");
	memcpy(keyboard, keyboard_jp, sizeof(keyboard_jp));
	memcpy(shiftkeyboard, shiftkeyboard_jp, sizeof(shiftkeyboard_jp));
    } else if (kbdtype == KBDTYPE_FR) {
	kprintf("(FR)");
	memcpy(keyboard, keyboard_fr, sizeof(keyboard_fr));
	memcpy(shiftkeyboard, shiftkeyboard_fr, sizeof(shiftkeyboard_fr));
	memcpy(codekeyboard, codekeyboard_fr, sizeof(codekeyboard_fr));
	memcpy(shiftcodekeyboard, shiftcodekeyboard_fr, sizeof(shiftcodekeyboard_fr));
    } else if (kbdtype == KBDTYPE_UK) {
	kprintf("(UK)");
	memcpy(&shiftkeyboard[2][0],shiftkeyboard_uk, sizeof(shiftkeyboard_uk));
    } else if (kbdtype == KBDTYPE_ES) {
	kprintf("(ES)");
	memcpy(&keyboard[1][0], keyboard_es, sizeof(keyboard_es));
	memcpy(&shiftkeyboard[1][0], shiftkeyboard_es, sizeof(shiftkeyboard_es));
    }
    kputs(" with ");
    if ((infobits & INTFREQ_MASK) == INTFREQ_60Hz) {
	kputs("NTSC ");
	ticks_per_dsecond = 6;
    } else {
	kputs("PAL ");
    }
    kputs("video.\n");

    /* Default key repeat values in 10ths of seconds */
    /* These are the 50/60 Hz keyrepeat values observed for MSX2 and above */
    keyrepeat.first = ticks_per_dsecond == 6 ? 40 : 32;
    keyrepeat.continual = 2;

    sunrise_probe();
    if (megasd_probe()) {
        /* probe for megaflash rom sd */
        devsd_init();
    }

#ifdef BOOT_LINE
    /* Last, once the clock and the disks are up: set_boot_line() waits a
       moment through pause() and gives up if a key is already pressed, so
       holding a key at boot still brings the bootdev: prompt back */
    set_boot_line(BOOT_LINE);
#endif
}

void map_init(void)
{
}
