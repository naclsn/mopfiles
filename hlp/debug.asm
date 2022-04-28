%ifndef DEBUG_ASM
%define DEBUG_ASM

; yes, all of that is very bad and inefficient..
; TODO: a form of shallow 'printf'
; in the mean time:
;   put: only the thing
;   show: the thing and a new line

section .bss
	debug_BUFSIZE		equ 1024
	debug_buf		resb debug_BUFSIZE

section .text
; rax: value
debug_put_address:
	mov	rsi, debug_buf
	mov	word[rsi], '0'
	inc	rsi
	mov	word[rsi], 'x'
	inc	rsi

	mov	rdi, 16		; base (hexadecimal)
	call	iota		; -> rdx: len

	mov	rax, 1		; write
	mov	rdi, 2		; stderr
	mov	rsi, debug_buf
	syscall

	ret

; rax: value
debug_put_number:
	mov	rsi, debug_buf

	mov	rdi, 10		; base (decimal)
	call	iota		; -> rdx: len

	mov	rax, 1		; write
	mov	rdi, 2		; stderr
	mov	rsi, debug_buf
	syscall

	ret

; rax: value
debug_put_char:
	mov	[debug_buf], al

	mov	rax, 1		; write
	mov	rdi, 2		; stderr
	mov	rdx, 1
	mov	rsi, debug_buf
	syscall

	ret

; rax: value
debug_show_address:
	call	debug_put_address
	mov	rax, 10
	call	debug_put_char

	ret

; rax: value
debug_show_number:
	call	debug_put_number
	mov	rax, 10
	call	debug_put_char

	ret

%endif ; DEBUG_ASM

