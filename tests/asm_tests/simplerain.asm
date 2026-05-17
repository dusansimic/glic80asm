;===============================================================================
; Prikazuje kišu na ekranu na osnovu definicije karaktera. Koristi primitivan
; RND generator na osnovu Refresh registra pa ovo treba zameniti pravim
; generatorom slučajnih brojeva za bolju distribuciju kapi da se ne vidi 
; patern brojača.
;===============================================================================

	jr	start
	db	"Simple Rain",0
start:
	ld	hl,$7800
	
loop_char:
	ld	a,r
	bit	6,a
	ld	de,char
	jr	nz,skip
	ld	de,bgchar
skip:	ld	b,8

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

char:	db	$00,$70,$fc,$fe,$fc,$70,$00,$00
bgchar:	db	$00,$00,$00,$10,$00,$00,$00,$00
