;===============================================================================
; FULL Z80 SYSTEM TEST
; Test 32K RAM memorije, OLED ekrana, RP2040 generisanja slike i tastera.
; Ako je sve u redu OLED je beo i svi LED segmenti su upaljeni,
; a pritisci na tastere A, B, C i NAV/Push gase LED segmente za test tastera.
; Ako postoji greška tokom testa RAM memorije na LED piše "E".
;===============================================================================

	jr	start
	db	"System Test V2.0",0
start:
	;-----------------------------------------------------------------------
	; Test svih segmenata na LED ekranu:

	ld	a,11111111b
	out	(0),a
	
	;-----------------------------------------------------------------------
	; Test memorije:

	ld	hl,ramstart
ram_loop:
	; Test 1:
	
	ld	(hl),255
	ld	a,(hl)
	cp	255
	jr	nz,error

	; Test 0:
	
	ld	(hl),0
	ld	a,(hl)
	cp	0
	jr	nz,error
	
	; Nastavi:
	
	inc	hl
	bit	7,h
	jr	z,ram_loop
	
	;-----------------------------------------------------------------------
	; Test OLED ekrana
	
	ld	hl,$7800
	ld	bc,2048
oled_loop:
	ld	(hl),$ff
	inc	hl
	dec	bc
	ld	a,b
	or	c
	jr	nz,oled_loop
	
	;-----------------------------------------------------------------------
	; Test tastera:
	
keys_loop:
	in	a,(0)
	cpl
	out	(0),a
	jr	keys_loop
	
	;-----------------------------------------------------------------------
	; Greška tokom RAM testa ("E" na LED):
	
error:
	ld	a,01111001b
	out	(0),a
	halt
	
ramstart:
