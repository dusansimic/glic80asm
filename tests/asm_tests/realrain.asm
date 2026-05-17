;===============================================================================
; Kiša sa finim skroliranjem ekrana za jedan piksel.
;===============================================================================

	jr	start
	db	"Real Rain",0
start:
   	ld	sp,$7700	; Inicijalizacija steka
	call	seedRandom	; Inicijalizacija generatora slučajnih brojeva
   	im	1		
   	ei
	jp	main

;------------------------------------------------------------------------------- 
; Interapt routina. Ne radi ništa, služi samo da HALT može da sačeka 1/20s.
;------------------------------------------------------------------------------- 

   	forg	$0038
	org	$0038
	ei
   	reti
	
;------------------------------------------------------------------------------- 
; Glavni program koji u krug prikazuje prvu liniju sa random kapima i
; skrolira ekran na dole sinhronizovano sa interaptom.
;------------------------------------------------------------------------------- 

main:	
	call	generateDroplet
	
	ld	b,4
.scroll:
	call	scrollLine
	halt
	djnz	.scroll

	jp	main
	
;------------------------------------------------------------------------------- 
; Generiše prvu liniju sa random pozicijom kapljica.
;------------------------------------------------------------------------------- 

generateDroplet:

	ld	hl,$7800	; Start video memorije (VRAM)
	call	generateRandom	; Random broj u A 0..255
	and	%01111000	; Svedi ga na ofset karaktera za kolone 0..15
	ld	l,a		; Pomeri u L da formira HL adresu u prvom redu
	ld	de,.drplt	; Adresa definicije znaka za ispis
	ld	b,8		; Brojač bajtova u definiciji karaktera
.loop:	
	ld	a,(de)		; Uzmi prvi bajt definicije u A
	ld	(hl),a		; Smesti ga u VRAM
	inc	de		; Sledeći bajt definicije
	inc	hl		; Sledeći bajt u VRAM
	djnz	.loop		; Nastavi dok se ne ispiše ceo karakter

	ret

.drplt:	db	$00,$70,$fc,$fe,$fc,$70,$00,$00

;------------------------------------------------------------------------------- 
; Pomera sadržaj ekrana za jedan red na dole (8 piksela, ceo bajt).
;------------------------------------------------------------------------------- 
	
scrollRow:

	; Skroliranje na dole ekrana osim prvog reda:

	ld	bc,1920		; Broj bajtova za pomeranje (2048-128)
	ld	de,$FFFF	; Poslednji bajt poslednjeg reda u VRAM
	ld	hl,$FF7F	; Poslednji bajt pretposlednjeg reda u VRAM
	lddr			; pomeri sve na dole osim prvog reda
	
	; Čišćenje prvog reda. Zgodno je da je adresa u DE nakon LDDR
	; poslednji bajt u prvom redu pa može da se iskoristi za početnu adresu:
	
	ld	b,128		; Broj bajtova u prvom redu za čišćenje
	ld	a,0		; Vrednost za upis u sve bajtove prvog reda
.loop:
	ld	(de),a		; Smesti vrednost
	dec	de		; Prethodni bajt
	djnz	.loop		; Ponavljaj za ceo prvi red
	
	ret
	
;------------------------------------------------------------------------------- 
; Pomera sadržaj ekrana za jednu liniju piksela na dole (unutar bajta).
;------------------------------------------------------------------------------- 

scrollLine:

	push	af
	push	bc
	push	de
	push	hl

	; Pomeranje svih bitova na dole red po red sa
	; prepisivanjem bita 7 iz reda iznad:

	ld	hl,32767	; poslednji bajt VRAM-a
	ld	de,32639	; poslednji - 128
	ld	bc,1920		; brojac bajtova VRAM-a bez gornjih 128
.dsloop1:
	push	bc		; sacuvaj brojac bajtova
	ld	a,(hl)		; ucitaj bajt za scroll u A
	sla	a		; pomeri sve bitove na dole i ocisti bit 0
	ld	b,a		; prebaci privremeno u B
	ld	a,(de)		; ucitaj bajt iznad za uzimanje bita 7
	rlca			; rotiraj da se bit 7 prebaci u bit 0
	and	1		; ocisti sve osim bita 0
	or	b		; dodaj bitove 1 do 7 iz B na bit 0 u A 
	ld	(hl),a		; snimi vrednost nazad na (HL)
	pop	bc	 	; vrati brojac bajtova
	dec	hl		; nova lokacija za scroll
	dec	de		; nova lokacija za pozajmljivanje bita 7
	dec	bc		; smanji brojac bajtova
	ld	a,b
	or	c
	jp	nz,.dsloop1

	; Pomeranje svih bitova na dole u redu na vrhu
	; bez prepisivanja bita iz reda iznad jer ne postoji:

	ld	b,128		; brojac preostalih bajtova u gornjem redu
.dsloop2:
	ld	a,(hl)		; ucitaj bajt za scroll
	sla	a		; pomeri sve bitove na dole i ocisti bit 0
	ld	(hl),a		; snimi vrednost nazad u (HL)
	dec	hl		; nova lokacija za scroll
	djnz	.dsloop2

	pop	hl
	pop	de
	pop	bc
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

generateRandom:

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

seedRandom:

	push	af
	ld	a,r
	ld	(_rnd+4),a
	pop	af
	ret


	