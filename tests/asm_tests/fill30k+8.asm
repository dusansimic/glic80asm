;===============================================================================
; Popunjava RAM sa 30K plus 8 bajtova. Ako je sve primljeno na početku
; video memorije se nalazi kvadrat. Ovo je test učitavanja bez celog VRAM-a.
;===============================================================================

	halt
	ds	30719
	db	$FF,$81,$81,$81,$81,$81,$81,$FF

	