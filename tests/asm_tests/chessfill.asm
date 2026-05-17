;===============================================================================
; Popunjava ceo VRAM od 2K šahovskim paternom.
;===============================================================================

	jr	code
	db	"Chess Pattern",0
code:
	ld	hl,$7800	; VRAM start
	ld	bc,$0800	; 2K brojač
	ld	d,$AA
loop:	ld	(hl),d
	rlc	d
	inc	hl
	dec	bc
	ld	a,b
	or	c
	jr	nz,loop
	halt
