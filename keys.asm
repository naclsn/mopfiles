%ifndef KEYS_ASM
%define KEYS_ASM

%macro kat 2
	times (%1)-($-keymap)/8 dq 0
	dq %2
%endmacro

section .data
	keymap:
	kat	'!', exec_fork ; TODO: not accurate, place-holder
	kat	'l', move_forward
	kat	'q', _quit

	kat	256, 0

%unmacro kat 2

%endif ; KEYS_ASM
