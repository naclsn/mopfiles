%ifndef DEBUG_ASM
%define DEBUG_ASM

section .bss
	BUFSIZE		equ 1024
	buf		resb BUFSIZE

section .text
; rax: value
debug_show_address:
	mov	rsi, buf
	mov	word[rsi], '0'
	inc	rsi
	mov	word[rsi], 'x'
	inc	rsi

	mov	rdi, 16		; base (decimal)
	call	iota		; -> rdx: len

	inc	rdx
	inc	rdx
	mov	word[buf+rdx], 10
	inc	rdx

	mov	rax, 1		; write
	mov	rdi, 2		; stderr
	mov	rsi, buf
	syscall

	ret

; rax: value
debug_put_char:
	mov	[buf], al

	mov	rax, 1		; write
	mov	rdi, 2		; stderr
	mov	rdx, 1
	mov	rsi, buf
	syscall

	ret

%endif ; DEBUG_ASM

