%ifndef STR_ASM
%define STR_ASM

section .text
; rdi: char* str -> rdx: len
str_len:
	xor	rcx, rcx
	not	ecx
	xor	al, al
	cld
repne	scasb
	not	ecx
	lea	rdx, [rcx-1]
	ret

; rdi: char* a, rsi: char* b, rcx: len -> zf: set if match
str_cmp:
repe	cmpsb
	ret

; rsi: src, rdi: dest, rcx: len
str_mov:
rep	movsb
	ret

%endif ; STR_ASM
