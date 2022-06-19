%ifndef STRUC_ASM
%define STRUC_ASM

%macro struc_mov 3
	mov	rcx, %1 %+ _size
	lea	rdi, %2
	lea	rsi, %3
	cld
rep	movsb
%endmacro

%endif ; STRUC_ASM
