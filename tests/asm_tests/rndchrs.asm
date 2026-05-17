;===============================================================================
; Random Characters sa 20 FPS.
;===============================================================================

	jr	code
	db	"Rnd Chars 10 FPS",0
code:
   	ld	sp,$7700	; Inicijalizacija steka
   	im	1		
   	ei
	
	jr	main		; Idemo na glavni deo...
	
;------------------------------------------------------------------------------- 
; Interapt routina.
;------------------------------------------------------------------------------- 

   	forg	$0038
	org	$0038
	ei
   	reti

;------------------------------------------------------------------------------- 
; Raspoređivanje tačaka.
;------------------------------------------------------------------------------- 

main:	
	; Obriši sve sa ekrana:
	
	ld	hl,$7700	; Adresa Text VRAM-a
	ld	b,0		; Brojač bajtova
.cls_loop:
	ld	(hl),0		; Obriši karakter
	inc	hl		; Pređi na sledeći karakter
	djnz	.cls_loop	; Nastavi za sve karaktere
	
	; Rasporedi random tačke:
	
	ld	hl,$7700	; Adresa Text VRAM-a
	ld	b,16		; Brojač redova

	; Rasporedi tačke u svaki red do zadatog broja:

.prepare_row:
	ld	c,4		; Spremi gustinu tačaka u redu
.prepare_mine:
	call	rnd_offset	; Generiši random ofset u DE
	push	hl		; Sačuvaj poziciju početka reda
	add	hl,de		; Izračunaj poziciju tačke
	call	generate_random	; Random broj u A
	and	%00000011	; Izdvoj 4 karaktera
	add	$17		; od $17 do $1A
	ld	(hl),a		; Stavi karakter u TextVRAM
	pop	hl		; Obnovi poziciju početka reda
	dec	c
	jr	nz,.prepare_mine

	ld	de,16		; Dodaj ofset za prelaz u sledeći red
	add	hl,de		; Pređi u sledeći red
	djnz	.prepare_row

	; Pauza i ponovo iz početka:
	
	halt
	halt
	
	jr	main
	
;------------------------------------------------------------------------------- 
; Generisanje random ofset u jednom redu u intervalu 0..15. 
; Rezultat:
;	DE - vrednost ofseta
;------------------------------------------------------------------------------- 

rnd_offset:

	push	af
.generate:
	call	generate_random	; Generiši random broj u A
	and	%00001111	; Ostavi samo donja četiri bita za ofset 0..15
	ld	d,0		; Pripremi ofset u DE
	ld	e,a

	pop	af
	ret

;==============================================================================
; lib_random.asm
; Originally appeared as a post by Patrik Rak on WoSF.
;
; generateRandom - generates random number
; seedRandom - seeds random number generator
;===============================================================================

;------------------------------------------------------------------------------- 
; Generates random number.
;
; Input: 
;   NONE
; Output: 
;   A  - generated random number
;------------------------------------------------------------------------------- 

generate_random:

	push	hl
	push	de
	
_rnd:	ld	hl,0xA280   ; xz -> yw
	ld	de,0xC0DE   ; yw -> zt

	ld	(_rnd+1),de ; x = y, z = w
	ld 	a,e         ; w = w ^ ( w << 3 )
	add	a,a
	add	a,a
	add	a,a
	xor	e
	ld	e,a
	ld	a,h         ; t = x ^ (x << 1)
	add	a,a
	xor	h
	ld	d,a
	rra                 ; t = t ^ (t >> 1) ^ w
	xor	d
	xor	e
	ld	h,l         ; y = z
	ld	l,a         ; w = t
	ld	(_rnd+4),hl

	pop	de
	pop	hl
	ret
		
;------------------------------------------------------------------------------- 
; Seeds random number generator with R register. Not originally posted by
; Patrik Rak, added later by Ivan Glisin to provide different initial values. 
;
; Input: 
;   NONE
; Output: 
;   NONE
;------------------------------------------------------------------------------- 

seed_random:

	push	af
	ld	a,r
	ld	(_rnd+4),a
	pop	af
	ret
