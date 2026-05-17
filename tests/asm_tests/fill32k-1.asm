;===============================================================================
; Popunjava ceo RAM sa 32K minus jedan bajt. Ako je sve primljeno na kraju
; video memorije se nalazi kvadrat ali pomeren za jedan piksel ulevo.
; Ovo je test učitavanja za bajt manje od maksimalnih 32K.
;===============================================================================

	halt
	ds	32758
	db	$FF,$81,$81,$81,$81,$81,$81,$FF

	