;===============================================================================
; Skrolira patern na sve cetiri strane za po jedan piksel. Ovde su rutine za
; fino skroliranje koje se mogu iskoristiti za druge programe. Horizontalni
; troši oko 15% CPU vremena u frejmu, a vertikalni oko 68% CPU vremena u frejmu.
; Oko 11% frejma ode na preuzimanje VRAM za ispis na OLED pa je preostalo vreme
; za ostali kod 100-11-15=74% kod horizontalnog, a 100-11-68=21% kod vertikalnog. 
;===============================================================================

	jr	start
	db	"Scrollaround",0
start:
   	ld	sp,$7700
   	im	1
   	ei
	nop
	halt

	call	fill_pattern
	halt
	jr	repeat_scrolls
	
;----------------------------------------------------------------------

   	forg	$0038
	org	$0038
		
	ei
   	reti	

;----------------------------------------------------------------------

repeat_scrolls:
	ld	b,64
loop_r:
	call	scroll_right
	halt
	djnz	loop_r

	ld	b,64
loop_d:
	call	scroll_down
	halt
	djnz	loop_d

	ld	b,64
loop_l:
	call	scroll_left
	halt
	djnz	loop_l
	
	ld	b,64
loop_u:
	call	scroll_up
	halt
	djnz	loop_u

	jp	repeat_scrolls
	
;----------------------------------------------------------------------

scroll_up:

	push	af
	push	bc
	push	de
	push	hl

	; Pomeranje svih bitova na gore red po red
	; sa prepisivanjem bita 0 iz reda ispod:

	ld	hl,30720	; prvi bajt VRAM-a
	ld	de,30848	; prvi + 128
	ld	bc,1920		; brojac bajtova VRAM-a bez donjih 128
_usloop1:
	push	bc		; sacuvaj brojac bajtova
	ld	a,(hl)		; ucitaj bajt za scroll u A
	srl	a		; pomeri sve bitove na gore i ocisti bit 7
	ld	b,a		; prebaci privremeno u B
	ld	a,(de)		; ucitaj bajt ispod za uzimanje bita 0
	rrca			; rotiraj da se bit 0 prebaci u bit 7
	and	%10000000	; ocisti sve osim bita 7 
	or	b		; dodaj bitove 0 do 6 iz B na bit 7 u A 
	ld	(hl),a		; snimi vrednost nazad na (HL)
	pop	bc	 	; vrati brojac bajtova
	inc	hl		; nova lokacija za scroll
	inc	de		; nova lokacija za pozajmljivanje bita 7
	dec	bc		; smanji brojac bajtova
	ld	a,b
	or	c
	jp	nz,_usloop1

	; Pomeranje svih bitova na gore u redu na dnu
	; bez prepisivanja bita iz reda ispod jer ne postoji:

	ld	b,128		; brojac preostalih bajtova u donjem redu
_usloop2:
	ld	a,(hl)		; ucitaj bajt za scroll
	sra	a		; pomeri sve bitove na gore
	and	%01111111	; ocisti bit 7 (SRA ga zadrzava!)
	ld	(hl),a		; snimi vrednost nazad u (HL)
	inc	hl		; nova lokacija za scroll
	djnz	_usloop2

	pop	hl
	pop	de
	pop	bc
	pop	af
	ret


;----------------------------------------------------------------------

scroll_down:

	push	af
	push	bc
	push	de
	push	hl

	; Pomeranje svih bitova na dole red po red
	; sa prepisivanjem bita 7 iz reda iznad:

	ld	hl,32767	; poslednji bajt VRAM-a
	ld	de,32639	; poslednji - 128
	ld	bc,1920		; brojac bajtova VRAM-a bez gornjih 128
_dsloop1:
	push	bc		; sacuvaj brojac bajtova
	ld	a,(hl)		; ucitaj bajt za scroll u A
	sla	a		; pomeri sve bitove na dole i ocisti bit 0
	ld	b,a		; prebaci privremeno u B
	ld	a,(de)		; ucitaj bajt iznad za uzimanje bita 7
	rlca			; rotiraj da se bit 7 prebaci u bit 0
	and	$00000001	; ocisti sve osim bita 0
	or	b		; dodaj bitove 1 do 7 iz B na bit 0 u A 
	ld	(hl),a		; snimi vrednost nazad na (HL)
	pop	bc	 	; vrati brojac bajtova
	dec	hl		; nova lokacija za scroll
	dec	de		; nova lokacija za pozajmljivanje bita 7
	dec	bc		; smanji brojac bajtova
	ld	a,b
	or	c
	jp	nz,_dsloop1

	; Pomeranje svih bitova na dole u redu na vrhu
	; bez prepisivanja bita iz reda iznad jer ne postoji:

	ld	b,128		; brojac preostalih bajtova u gornjem redu
_dsloop2:
	ld	a,(hl)		; ucitaj bajt za scroll
	sla	a		; pomeri sve bitove na dole i ocisti bit 0
	ld	(hl),a		; snimi vrednost nazad u (HL)
	dec	hl		; nova lokacija za scroll
	djnz	_dsloop2

	pop	hl
	pop	de
	pop	bc
	pop	af
	ret
	
;----------------------------------------------------------------------

scroll_left:

	push	af
	push	bc
	push	de
	push	hl

	ld	de,30720	; prvi bajt VRAM-a
	ld	hl,30721	; drugi bajt VRAM-a	
	ld	a,16		; brojac redova
_lsloop:
	ld	bc,127		; brojac bajtova u redu -1
	ldir			; prepakuj bajtove (de)<-(hl) x 127
	dec	hl		; HL jedan bajt napred jer nema ld (de),0
	ld	(hl),0		; ocisti kranji desni
	inc	hl		; uskladjivanje jednog koji je preskocen
	inc	hl
	inc	de
	dec	a		; smanji brojac redova
	jp	nz,_lsloop	; ako ima još nastavi

	pop	hl
	pop	de
	pop	bc
	pop	af
	ret

;----------------------------------------------------------------------

scroll_right:

	push	af
	push	bc
	push	de
	push	hl

	ld	de,32767	; poslednji bajt VRAM-a
	ld	hl,32766	; pretposlednji bajt VRAM-a	
	ld	a,16		; brojac redova
_rsloop:
	ld	bc,127		; brojac bajtova u redu -1
	lddr			; prepakuj bajtove (hl)->(de) x 127
	inc	hl		; HL jedan bajt nazad jer nema ld (de),0
	ld	(hl),0		; ocisti kranji levi
	dec	hl		; uskladjivanje jednog koji je preskocen
	dec	hl
	dec	de
	dec	a		; smanji brojac redova
	jp	nz,_rsloop	; ako ima još nastavi

	pop	hl
	pop	de
	pop	bc
	pop	af
	ret

;----------------------------------------------------------------------

fill_pattern:

	push	af
	push	bc
	push	de
	push	hl

	ld	hl,$7800
_restart_loop:
	ld	de,_pattern
	ld	b,8
_fill_loop:
	ld	a,(de)
	ld	(hl),a
	inc	de
	inc	hl
	djnz	_fill_loop

	ld	a,h
	cp	$80
	jp	nz,_restart_loop

	pop	hl
	pop	de
	pop	bc
	pop	af
	ret

_pattern:
	db	%00001000
	db	%00011100
	db	%00111110
	db	%01111111
	db	%00111110
	db	%00011100
	db	%00001000
	db	%00000000

