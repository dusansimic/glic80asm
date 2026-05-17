;===============================================================================
; Crta scrca prema difiniciji karaktera na celom ekranu.
; Promeniti definiciju karaktera za drugi simbol.
;===============================================================================

	jr	code
	db	"Hearts",0
code:
	ld	hl,$7800

loop_char:
	ld	de,char
	ld	b,8
loop_byte:
	ld	a,(de)
	ld	(hl),a
	inc	de
	inc	hl
	djnz	loop_byte

	ld	a,h
	cp	$80
	jr	nz,loop_char

	halt

char:	db	$1c,$3e,$7e,$fc,$7e,$3e,$1c,$00
