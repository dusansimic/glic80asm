;===============================================================================
; Popunjava ceo RAM sa punih 32K. Ako je sve u redu na kraju video memorije se
; nalazi kvadrat. Ovo je granični test za transfer i učitavanje tačno 32K.
;===============================================================================

	halt
	ds	32759
	db	$FF,$81,$81,$81,$81,$81,$81,$FF
