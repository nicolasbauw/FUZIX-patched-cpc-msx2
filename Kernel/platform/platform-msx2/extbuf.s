;
;	Disk buffer page helpers (see extbuffer.c)
;
;	The disk buffers live in their own mapper page, which is only mapped
;	while a buffer is copied. These run from the low 16K of the kernel,
;	which never moves, so they can remap 0x4000-0xBFFF under their own
;	feet. Called with interrupts off: the interrupt path runs kernel code
;	that may sit in the pages remapped here.
;

	.module extbuf

	.globl _extbuf_copy
	.globl _copy_common
	.globl _extbuf_zero
	.globl _xb_src
	.globl _xb_dst
	.globl _xb_len
	.globl _xb_page1
	.globl _xb_page2

	.area _CODE1

;
;	Copy xb_len bytes from xb_src to xb_dst, with page xb_page1 mapped at
;	0x4000 and page xb_page2 at 0x8000, then put the kernel back
;
_extbuf_copy:
	ld a,(_xb_page1)
	out (0xFD),a
	ld a,(_xb_page2)
	out (0xFE),a
	ld hl,(_xb_src)
	ld de,(_xb_dst)
	ld bc,(_xb_len)
	ld a,b
	or c
	jr z, extbuf_done
	ldir
extbuf_done:
	ld a,#2			; kernel map is 3,2,1 (plus the common)
	out (0xFD),a
	ld a,#1
	out (0xFE),a
	ret

;
;	Clear the 512 byte buffer at xb_dst, page xb_page1 mapped at 0x4000
;
_extbuf_zero:
	ld a,(_xb_page1)
	out (0xFD),a
	ld hl,(_xb_dst)
	ld (hl),#0
	ld d,h
	ld e,l
	inc de
	ld bc,#511
	ldir
	jr extbuf_done

;
;	copy_common(page): make page a top page by copying the common area
;	(0xF200 up, the part not swapped) into it, for a process coming back
;	from swap (see get_common in bank16k.c). 0xFFFF is left out: with the
;	RAM on the expanded slot 3, it reads as the secondary slot register.
;
_copy_common:
	pop bc
	pop de			; E = page
	push de
	push bc
	ld a,i			; P/V = interrupts were enabled
	push af
	di
	ld a,e
	out (0xFD),a		; the page at 0x4000
	ld hl,#0xF200
	ld de,#0x7200
	ld bc,#0x0DFF
	ldir
	ld a,#2
	out (0xFD),a
	pop af
	ret po
	ei
	ret

	.area _COMMONMEM

_xb_src:
	.dw 0
_xb_dst:
	.dw 0
_xb_len:
	.dw 0
_xb_page1:
	.db 0
_xb_page2:
	.db 1
