;===============================================================================
; Popunjava ceo RAM sa punih 32K plus dodatni kod. Ako je sve primljeno i prvih 
; 32K na kraju video memorije se nalazi kvadrat. Ako je došlo do rollover koda
; on će se prepisati preko početka i izvršiti tako da napiše OVER! u tekst VRAM.
; Tako trenutno radi prijem koda sa COM porta, sve preko 32K prepisuje u krug.
; Ovo je test učitavanja preko maksimalnih 32K.
;===============================================================================

	halt
	ds	32759
	db	$FF,$81,$81,$81,$81,$81,$81,$FF
	
	ld	hl,$7700
	ld	(hl),'O'
	inc	hl
	ld	(hl),'V'
	inc	hl
	ld	(hl),'E'
	inc	hl
	ld	(hl),'R'
	inc	hl
	ld	(hl),'!'
	halt
	