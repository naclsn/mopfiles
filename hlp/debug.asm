%ifndef DEBUG_ASM
%define DEBUG_ASM

; @usage: (note: these are macros)
;	debug_put_[]	; accumulates in the debug_buf
;	debug_send
;
; for the _put_[] procs:
;	rax: what to put

%macro debug_send 0
	mov	rax, [debug_buf_c]
	sub	rax, debug_buf
	sys_write 2, debug_buf, rax
	mov	qword[debug_buf_c], debug_buf
%endmacro

%macro debug_put_byte 1
	mov	al, %1
	mov	rsi, [debug_buf_c]
	mov	byte[rsi], al
	inc	rsi
	mov	[debug_buf_c], rsi
%endmacro

%macro debug_put_bytes 1
section	.data
	%%buf	db %1
	%%len	equ $-%%buf
section .text
	debug_put_buffer %%buf, %%len
%endmacro

%macro debug_put_buffer 2
	mov	rsi, %1
	mov	rdi, [debug_buf_c]
	mov	rcx, %2
	add	[debug_buf_c], rcx
	call	str_mov
%endmacro

%macro debug_put_address 1
	mov	rax, %1
	call	__debug_put_address
%endmacro

%macro debug_put_number 1
	mov	rax, %1
	call	__debug_put_number
%endmacro

section .bss
	debug_BUFSIZE		equ 8192
	debug_buf		resb debug_BUFSIZE

section .data
	debug_buf_c		dq debug_buf

section .text
__debug_put_address:
	push	rax
	; debug_put_bytes "0x"
	debug_put_byte '0'
	debug_put_byte 'x'

	pop	rax
	mov	rdi, 16		; base (hexadecimal)
	mov	rsi, [debug_buf_c]
	call	iota		; -> rdx: len

	mov	rax, [debug_buf_c]
	add	rax, rdx
	mov	[debug_buf_c], rax
	ret

__debug_put_number:
	;(mov	rax, rax)
	mov	rdi, 10		; base (decimal)
	mov	rsi, [debug_buf_c]
	call	iota		; -> rdx: len

	mov	rax, [debug_buf_c]
	add	rax, rdx
	mov	[debug_buf_c], rax
	ret

%endif ; DEBUG_ASM
