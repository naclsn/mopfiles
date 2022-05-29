%ifndef STR_ASM
%define STR_ASM

section .text
; rdi: char* str -> rdx: len
str_len:
	xor	rcx, rcx
	not	ecx
	xor	al, al
	cld
repnz	scasb
	not	ecx
	lea	rdx, [rcx-1]
	ret

; al: char, rdi: char* str, rcx: len -> rdx: at
str_ch:
	cld
	mov	rdx, rcx
repnz	scasb
	not	rcx
	add	rdx, rcx
	ret

; rdi: char* a, rsi: char* b, rcx: len -> zf: set if match
str_cmp:
	cld
repe	cmpsb
	ret

; rsi: src, rdi: dest, rcx: len
str_mov:
	cld
rep	movsb
	ret

%endif ; STR_ASM
