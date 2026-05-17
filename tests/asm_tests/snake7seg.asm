;===============================================================================
; Vrti u krug sve segmente bez srednjeg i tacke na 7-segment displeju.
;===============================================================================

	jr	start
	db	"Snake 7-Seg LED",0
start:
   	ld	sp,$00FF
   	im	1
   	ei
clear:	ld	a,$01
	ld	c,6
loop:	out	(0),a
   	rlca
   	ld	b,$04		; delay B*50ms
delay:	halt
   	djnz	delay
	dec	c
   	jr	nz,loop
	jr	clear

   	forg	$0038
	ei
   	reti
	