#include <kernel.h>
#include <kdata.h>
#include <printf.h>
#include <stdbool.h>
#include <keycode.h>
#include <vt.h>
#include <tty.h>
#include <input.h>
#include <devinput.h>
#include <devtty.h>

/* buffer for port scan procedure */
uint8_t keybuf[10];
/* Previous state */
uint8_t keymap[10];

/* Counted in timer ticks, and this platform ticks at 300Hz - six times the
   rate of the 50Hz ports these numbers look borrowed from. The old { 25, 2 }
   meant a key started repeating after 83ms and then fired every 6.7ms, some
   150 characters a second, which makes the console barely usable. The zx port
   uses { 50, 5 } at 50Hz, i.e. one second then 100ms; 255 is as close to that
   first delay as the uint8_t counter reaches. */
struct vt_repeat keyrepeat = { 255, 30 };

static uint8_t kbd_timer;

static uint8_t keybyte, keybit;
static uint8_t newkey;
static int8_t keysdown = 0;

#ifdef CONFIG_KBD_AZERTY
/* French AZERTY matrix, taken straight from the key translation tables of a
   real French OS ROM (OS6128-AZERTY): the unshifted table sits at offset
   0x1EEF and the shifted one at 0x1F3F, 80 bytes each, indexed exactly like
   the arrays below (key number = line * 8 + bit). They were located by
   searching the ROM for the letter positions, which are the same in every
   locale, and the match is unique.

   The one thing worth knowing before reading this table: the French ROM is
   ISO-646-FR. It emits ordinary ASCII and its *font* draws some of those
   codes as accented letters, so the keycap legends and the bytes disagree
   in a way that looks alarming until you know the rule:

	byte 0x7C "|" is drawn as "ù"    byte 0x7B "{" is drawn as "é"
	byte 0x7D "}" is drawn as "è"    byte 0x5C "\" is drawn as "ç"
	byte 0x40 "@" is drawn as "à"    byte 0x5D "]" is drawn as "§"
	byte 0x5B "[" is drawn as "°"

   That is why the "ù" key next to "m" is the RSX prefix in BASIC: it always
   was the pipe, 0x7C, just painted differently. Which is a gift here -
   FUZIX draws its own ASCII font, so every symbol a shell needs (| \ @ [ ]
   { } ! # $ * < >) is reachable from a French keyboard after all.

   Rows 0, 1 and 9 are left as the UK table has them: the ROM holds firmware
   control codes there (cursor keys, the numeric keypad, and the joystick
   lines that alias onto row 9), which FUZIX translates its own way and
   which do not differ between locales. */
uint8_t keyboard[10][8] = {
	{KEY_UP, KEY_RIGHT, KEY_DOWN, '9', '6', '3',13, '.'},
	{KEY_LEFT ,KEY_COPY , '7', '8', '5', '1', '2', '0'},
	{KEY_DEL, '*', 13, '#', '4', 0/*SHIFT*/, '$', 0/*CONTROL*/ },
	{'-', ')', '^', 'p', '|'/*drawn "ù"*/, 'm', '=', ':'},
	{'@'/*drawn "à"*/, '\\'/*drawn "ç"*/, 'o', 'i', 'l', 'k', ',', ';'},
	{'!', '}'/*drawn "è"*/, 'u', 'y', 'h', 'j', 'n', ' '},
	{']'/*drawn "§"*/, '(', 'r', 't', 'g', 'f', 'b', 'v'},
	{'\'', '"', 'e', 'z', 's', 'd', 'c', 'x'},
	{'&', '{'/*drawn "é"*/, KEY_ESC, 'a', KEY_TAB, 'q', KEY_CAPSLOCK, 'w'},
	{KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, 13, ' ', 0, KEY_BS}
};

/* SHIFT MODE */
uint8_t shiftkeyboard[10][8] = {
	{KEY_UP, KEY_RIGHT, KEY_DOWN, KEY_F9, KEY_F6, KEY_F3,KEY_ENTER, KEY_F11},
	{KEY_LEFT ,KEY_PASTE , KEY_F7, KEY_F8, KEY_F5, KEY_F1, KEY_F2, KEY_F10},
	{KEY_DEL, '<', 13, '>', KEY_F4, 0/*SHIFT*/, '@', 0/*CONTROL*/ },
	{'_', '['/*drawn "°"*/, '|', 'P', '%', 'M', '+', '/'},
	{'0', '9', 'O', 'I', 'L', 'K', '?', '.'},
	{'8', '7', 'U', 'Y', 'H', 'J', 'N', ' '},
	{'6', '5', 'R', 'T', 'G', 'F', 'B', 'V'},
	{'4', '3', 'E', 'Z', 'S', 'D', 'C', 'X'},
	{'1', '2', KEY_ESC, 'A', KEY_TAB, 'Q', 0/*CAPSLOCK*/, 'W'},
	{KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, 13, ' ', 0, KEY_BS}
};
#else
uint8_t keyboard[10][8] = {
	{KEY_UP, KEY_RIGHT, KEY_DOWN, '9', '6', '3',13, '.'},
	{KEY_LEFT ,KEY_COPY , '7', '8', '5', '1', '2', '0'},
	{KEY_DEL, '[', 13, ']', '4', 0/*SHIFT*/, '\\', 0/*CONTROL*/ },
	{'^', '-', '@', 'p', ';', ':', '/', '.'},
	{'0', '9', 'o', 'i', 'l', 'k', 'm', ','},
	{'8', '7', 'u', 'y', 'h', 'j', 'n', ' '},
	{'6', '5', 'r', 't', 'g', 'f', 'b', 'v'},
	{'4', '3', 'e', 'w', 's', 'd', 'c', 'x'},
	{'1', '2', KEY_ESC, 'q', KEY_TAB, 'a', KEY_CAPSLOCK, 'z'},
	{KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, 13, ' ', 0, KEY_BS}
};

/* SHIFT MODE */
uint8_t shiftkeyboard[10][8] = {
	{KEY_UP, KEY_RIGHT, KEY_DOWN, KEY_F9, KEY_F6, KEY_F3,KEY_ENTER, KEY_F11},
	{KEY_LEFT ,KEY_PASTE , KEY_F7, KEY_F8, KEY_F5, KEY_F1, KEY_F2, KEY_F10},
	{KEY_DEL, '{', 13, '}', KEY_F4, 0/*SHIFT*/, '`', 0/*CONTROL*/ },
	{'~', '=', '|', 'P', '+', '*', '?', '>'},
	{'_', ')', 'O', 'I', 'L', 'K', 'M', '<'},
	{'(', 39/*'*/, 'U', 'Y', 'H', 'J', 'N', ' '},
	{'&', '%', 'R', 'T', 'G', 'F', 'B', 'V'},
	{'$', '#', 'E', 'W', 'S', 'D', 'C', 'X'},
	{'!', '"', KEY_ESC, 'Q', KEY_TAB, 'A', 0/*CAPSLOCK*/, 'Z'},
	{KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, 13, ' ', 0, KEY_BS}
};
#endif


/* Key translation of the host firmware, handed over by the loader in page
   zero 0x80-0xFF (free: set_vectors() only programs the first 0x80 bytes,
   and the kernel starts at 0x100). Two hand-overs exist:

   "KB" + 2 x 56 bytes: the ROM loader (tools/fuzix-rom/loader.s in the
	ByteBox tree) runs while the firmware is still alive and asks it for
	every key of rows 2-8, with and without SHIFT (KM GET TRANSLATE / KM
	GET SHIFT). Whatever OS ROM is installed then decides what the keys
	type, exactly as in BASIC.

   "KL" + 1 byte: the floppy boot sector has no room for 112 bytes (it must
	fit in 512 and clears all memory before loading the kernel), so it
	passes the character the firmware gives to key 67 (row 8, bit 3): "q"
	on an English ROM, "a" on a French one. An English ROM selects the
	table below, copied from the key translation tables of the 6128
	English OS ROM (OS6128-EN, offsets 0x1EEF and 0x1F3F, rows 2-8). A
	French one is what is compiled in above.

   Only printable ASCII is taken over (0 in the tables below means "keep"):
   the firmware's own codes for DEL, TAB, ESC, CAPS LOCK, SHIFT, CONTROL and
   the function keys differ from FUZIX's, so those entries keep their
   compiled-in values. Rows 0, 1 and 9 are never touched, for the same
   reason. Without a signature the compiled-in table stays in force. */
#define KBD_HANDOFF ((volatile uint8_t *)0x0080)

/* Rows 2-8 of the English firmware tables, unshifted then shifted. */
static const uint8_t en_keys[56] = {
	0x00, 0x5B, 0x00, 0x5D, 0x00, 0x00, 0x5C, 0x00,
	0x5E, 0x2D, 0x40, 0x70, 0x3B, 0x3A, 0x2F, 0x2E,
	0x30, 0x39, 0x6F, 0x69, 0x6C, 0x6B, 0x6D, 0x2C,
	0x38, 0x37, 0x75, 0x79, 0x68, 0x6A, 0x6E, 0x20,
	0x36, 0x35, 0x72, 0x74, 0x67, 0x66, 0x62, 0x76,
	0x34, 0x33, 0x65, 0x77, 0x73, 0x64, 0x63, 0x78,
	0x31, 0x32, 0x00, 0x71, 0x00, 0x61, 0x00, 0x7A,
};
static const uint8_t en_shift_keys[56] = {
	0x00, 0x7B, 0x00, 0x7D, 0x00, 0x00, 0x60, 0x00,
	0x00, 0x3D, 0x7C, 0x50, 0x2B, 0x2A, 0x3F, 0x3E,
	0x5F, 0x29, 0x4F, 0x49, 0x4C, 0x4B, 0x4D, 0x3C,
	0x28, 0x27, 0x55, 0x59, 0x48, 0x4A, 0x4E, 0x20,
	0x26, 0x25, 0x52, 0x54, 0x47, 0x46, 0x42, 0x56,
	0x24, 0x23, 0x45, 0x57, 0x53, 0x44, 0x43, 0x58,
	0x21, 0x22, 0x00, 0x51, 0x00, 0x41, 0x00, 0x5A,
};

static void kbd_apply(const uint8_t *normal, const uint8_t *shifted)
{
	uint8_t i, c;

	for (i = 0; i < 56; i++) {
		c = normal[i];
		if (c >= 0x20 && c < 0x7F)
			keyboard[2 + (i >> 3)][i & 7] = c;
		c = shifted[i];
		if (c >= 0x20 && c < 0x7F)
			shiftkeyboard[2 + (i >> 3)][i & 7] = c;
	}
}

void kbd_import_firmware_tables(void)
{
	volatile uint8_t *h = KBD_HANDOFF;

	if (h[0] == 'K' && h[1] == 'B') {
		kbd_apply((const uint8_t *)h + 2, (const uint8_t *)h + 58);
	} else if (h[0] == 'K' && h[1] == 'L' && h[2] == 'q') {
		kbd_apply(en_keys, en_shift_keys);
	}
	h[0] = 0;
}

static uint8_t shiftmask[10] = { 0, 0, 0xA0, 0, 0, 0, 0, 0, 0, 0 };
static uint8_t capslock = 0;

static void keydecode(void)
{
	uint8_t m = 0;
	uint8_t c;

	uint8_t sh = keymap[2] & 0x20;	/* SHIFT */
	uint8_t ct = keymap[2] & 0x80;	/* CONTROL */

	if (sh) {
		m = KEYPRESS_SHIFT;
		c = shiftkeyboard[keybyte][keybit];
	} else {
		c = keyboard[keybyte][keybit];
	}
	
	if (c == KEY_CAPSLOCK) {
		capslock = 1 - capslock;
		return;
	}
	if (capslock && c >= 'a' && c <= 'z')
		c -= 'a' - 'A';
	
	if (ct) {
#ifdef CONFIG_VT_MULTI
		if ((c >= KEY_F1) && (c <= KEY_F4)){
			cpckbd_conswitch(c - KEY_F1 + 1);
			return;
		}
#endif
		m |= KEYPRESS_CTRL;
		if (c >= 'a' && c <= 'z')
				c = CTRL(c - ('a' - 'A'));
		else 
				c = CTRL(c);
	}
	if (c) {
		switch (keyboard_grab) {
		case 0:
#ifdef CONFIG_VT_MULTI		
			vt_inproc(inputtty, c);
#else
			vt_inproc(1, c);
#endif
			break;
		case 1:
			if (!input_match_meta(c)) {
#ifdef CONFIG_VT_MULTI		
				vt_inproc(inputtty, c);
#else
				vt_inproc(1, c);
#endif
				break;
			}
			/* Fall through */
		case 2:
			queue_input(KEYPRESS_DOWN);
			queue_input(c);
			break;
		case 3:
			/* Queue an event giving the base key (unshifted)
			   and the state of shift/ctrl/alt */
			queue_input(KEYPRESS_DOWN | m);
			queue_input(keyboard[keybyte][keybit]);
			break;
		}
	}
}


void tty_poll(void)
{
	int8_t i;

	newkey = 0;

	/* Nothing changed, and chance of key repeat work - so done */
	if (!update_keyboard() && !keysdown)
		return;

	for (i = 0; i < 10; i++) {
		int8_t n;
		uint8_t key = (~keybuf[i]) ^ keymap[i];
		if (key) {
			uint8_t m = 0x80;
			for (n = 7; n >=0; n--) {
				if ((key & m) && (keymap[i] & m))
					if (!(shiftmask[i] & m)) {
						if (keyboard_grab == 3) {
							queue_input(KEYPRESS_UP);
							queue_input(keyboard[i][n]);
						}
						keysdown--;
					}

				if ((key & m) && !(keymap[i] & m)) {
					if (!(shiftmask[i] & m)) {
						keysdown++;
						newkey = 1;
						keybyte = i;
						keybit = n;
					}
				}
				m >>= 1;
			}
		}
		keymap[i] = ~keybuf[i];
	}
	if (newkey && keysdown && keysdown < 3) {
		keydecode();
		kbd_timer = keyrepeat.first;
	}
}

void tty_pollirq (void){

	tty_poll();
    if (keysdown && keysdown < 3) {
        if (!--kbd_timer) {
            keydecode();
            kbd_timer = keyrepeat.continual;
        }
    }
}

static uint8_t update_keyboard(void) __naked
{
/*from https://github.com/lronaldo/cpctelera/blob/master/cpctelera/src/keyboard/cpct_scanKeyboard_if.s 
and https://github.com/lronaldo/cpctelera/blob/master/cpctelera/src/keyboard/cpct_isAnyKeyPressed_f.s*/
	__asm

		;; Configure PPI: Select Register 14 (the one connected with keyboard status) and set it for reading
		;;

		ex af, af'
		push af
		ex af, af'
		ld  bc,  #0xF782         ;; [3] Configure PPI 8255: Set Both Port A and Port C as Output. 
		out (c), c               ;; [4] 82 = 1000 0010 : (B7=1)=> I/O Mode,       (B6-5=00)=> Mode 1,          
									;;                       (B4=0)=> Port A=Output,  (B3=0)=> Port Cu=Output, 
									;;                       (B2=0)=> Group B, Mode 0,(B1=1)=> Port B=Input, (B0=0)=> Port Cl=Output
		ld  bc,  #0xF40E         ;; [3] Write (0Eh = 14) on PPI 8255 Port A (F4h): the register we want to select on AY-3-8912
		ld  e, b                 ;; [1] Save F4h into E to use it later in the loop
		out (c), c               ;; [4]

		ld  bc,  #0xF6C0         ;; [3] Write (C0h = 11 000000b) on PPI Port C (F6h): operation > select register 
		ld  d, b                 ;; [1] Save F6h into D to use it later in the loop
		out (c), c               ;; [4]
		.dw #0x71ED ; out (c), 0 ;; [4] out (C), 0 => Write 0 on PPI's Port C to put PSG's in inactive mode 
									;; .... (required in between different operations)
		ld  bc,  #0xF792         ;; [3] Configure PPI 8255: Set Port A = Input, Port C = Output. 
		out (c), c               ;; [4] 92h= 1001 0010 : (B7=1)=> I/O Mode,        (B6-5=00)=> Mode 1,          
									;;                       (B4=1)=> Port A=Input,    (B3=0)=> Port Cu=Output, 
									;;                       (B2=0)=> Group B, Mode 0, (B1=1)=> Port B=Input, (B0=0)=> Port Cl=Output
		;; Read Loop (Unrolled version): We read the 10-bytes that define the pressed/not pressed status
		;;
		ld    a, #0x40           ;; [2] A refers to the next keyboard line to be read (40h to 49h)
		ld   hl, #_keybuf		 ;; [3] HL Points to the start of the keyboardBuffer, 
												;; ... where scanned data will be stored

		;; Read line 40h
		ld    b, d               ;; [1] B = F6h => Write the value of A to PPI's Port C to select next Matrix Line
		out (c), a               ;; [4] 
		ld    b, e               ;; [1] B = F4h => Read from PPI's Port A: Pressed/Not Pressed Values from PSG
		ini                      ;; [5] The read value is written to (HL), then HL<-HL+1 and B<-B-1
		ex af, af'
		ld a, (hl)
		ex af, af'
		inc   a                  ;; [1] Loop: Increment A => Next Matrix Line. 

		;; Read line 41h
		ld    b, d               ;; [1] Same for line 41h
		out (c), a               ;; [4] 
		ld    b, e               ;; [1]
		ini                      ;; [5]
		ex af, af'
		and (hl)
		ex af, af'		
		inc   a                  ;; [1]

		;; Read line 42h
		ld    b, d               ;; [1] Same for line 42h
		out (c), a               ;; [4] 
		ld    b, e               ;; [1]
		ini                      ;; [5]
		ex af, af'
		and (hl)
		ex af, af'		
		inc   a                  ;; [1]

		;; Read line 43h
		ld    b, d               ;; [1] Same for line 43h
		out (c), a               ;; [4] 
		ld    b, e               ;; [1]
		ini                      ;; [5]
		ex af, af'
		and (hl)
		ex af, af'		
		inc   a                  ;; [1]

		;; Read line 44h
		ld    b, d               ;; [1] Same for line 44h
		out (c), a               ;; [4] 
		ld    b, e               ;; [1]
		ini                      ;; [5]
		ex af, af'
		and (hl)
		ex af, af'		
		inc   a                  ;; [1]
		
		;; Read line 45h
		ld    b, d               ;; [1] Same for line 45h
		out (c), a               ;; [4] 
		ld    b, e               ;; [1]
		ini                      ;; [5]
		ex af, af'
		and (hl)
		ex af, af'		
		inc   a                  ;; [1]

		;; Read line 46h
		ld    b, d               ;; [1] Same for line 46h
		out (c), a               ;; [4] 
		ld    b, e               ;; [1]
		ini                      ;; [5]
		ex af, af'
		and (hl)
		ex af, af'		
		inc   a                  ;; [1]

		;; Read line 47h
		ld    b, d               ;; [1] Same for line 47h
		out (c), a               ;; [4] 
		ld    b, e               ;; [1]
		ini                      ;; [5]
		ex af, af'
		and (hl)
		ex af, af'		
		inc   a                  ;; [1]

		;; Read line 48h
		ld    b, d               ;; [1] Same for line 48h
		out (c), a               ;; [4] 
		ld    b, e               ;; [1]
		ini                      ;; [5]
		ex af, af'
		and (hl)
		ex af, af'		
		inc   a                  ;; [1]

		;; Read line 49h
		ld    b, d               ;; [1] Same for line 49h (Except we dont have to increase a anymore)
		out (c), a               ;; [4] 
		ld    b, e               ;; [1]
		ini                      ;; [5]
								;; Restore PPI status to Port A=Output, Port C=Output
								;;
		ld  bc,  #0xF782         ;; [3] Put again PPI in Output/Output mode for Ports A/C.
		out (c), c               ;; [4]
		
		ex af, af'
		and (hl)
		inc   a              ;; [1] A holds the result of ANDing the 10 bytes. If no key is pressed, all bits should
								;; ... be 1, so A=0xFF. If we add 1, A=0, we return FALSE (no key is pressed).
								;; ... If any key is pressed, some bit will be 0, so A != 0xFF, which means A+1 != 0, and 
								;; ... we will be returning TRUE (A > 0)
		ld    l, a           ;; [1] L = A (Set return value for C calls in L)
		; The return value is the HL pair, not L alone: the compiled caller
		; tests it with "ld a,h / or a,l". H still holds the high byte of the
		; _keybuf pointer walked above, which is never zero, so the caller
		; always saw a true result and the "nothing changed" fast path never
		; triggered - on every single keyboard poll.
		ld    h, #0
		pop af
		ex af, af'

		ret                  ;; [3] Return

	__endasm;
}
