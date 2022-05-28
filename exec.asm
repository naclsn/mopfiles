; TODO: doc and such
; uses exmb to get user input, argsplit it then execve with
; the first arg as the program
;
; NOTE/TODO: process some escape sequences, perform argument
;            concatenation and quote removal by copying back
;            the input onto itself with an increasing offset

%ifndef EXEC_ASM
%define EXEC_ASM

	minibuf_cap:	equ 4096
struc minibuf
	mb_buf:		resb minibuf_cap
	mb_back:	resb minibuf_cap
	mb_args:	resq 32
endstruc

section .bss
	exmb:		resb minibuf_size

%macro exec_main 0
	jmp	exec_start
exec_done:
%endmacro

section .text
exec_start:
	; YYY: temporary event 'loop' here
	sys_read 0, exmb+mb_buf, minibuf_cap ; -> rax: length
	mov	[exmb+mb_buf+rax], byte 0

	call	exec_parse
	call	exec_print ; *-*
	call	exec_fork
	jmp	exec_start

	jmp	exec_done

; rax: length
exec_parse:
	mov	rsi, exmb+mb_buf-1
	mov	rdi, exmb+mb_args
	mov	r10, exmb+mb_back
	; bl (and bh): 0 blank, 1 char, 2/3 quoted (simple/double)
	xor	bl, bl
__exec_parse_next_char:
	shl	bx, 8
	inc	rsi
	mov	cl, [rsi]
	dec	rax
	jz	__exec_parse_done

	mov	[r10], cl	; build the copy minibuf
	inc	r10

	; is it in a simple quoted string
	cmp	bh, 2
	jnz	__exec_parse_not_in_simpleq
	cmp	cl, "'"
	jz	__exec_parse_arg_end	; FIXME: this parses string concatenations wrong
	shr	bx, 8
	jmp	__exec_parse_next_char
__exec_parse_not_in_simpleq:

	; is it in a double quoted string
	cmp	bh, 3
	jnz	__exec_parse_not_in_doubleq
	cmp	cl, '"'
	jz	__exec_parse_arg_end	; FIXME: this parses string concatenations wrong
					; TODO: escapes (at least a few)
	shr	bx, 8
	jmp	__exec_parse_next_char
__exec_parse_not_in_doubleq:

	; blank characters
	cmp	cl, ' '
	jz	__exec_parse_maybe_arg
	cmp	cl, 9
	jz	__exec_parse_maybe_arg
	cmp	cl, 10
	jz	__exec_parse_maybe_arg

	; other characters
	inc	bl
__exec_parse_maybe_arg:
	cmp	bh, bl
	jz	__exec_parse_next_char
	jc	__exec_parse_arg_new

__exec_parse_arg_end:
	; 1 -> 0 end argument
	mov	[rsi], byte 0
	jmp	__exec_parse_next_char

__exec_parse_arg_new:
	; 0 -> 1 new argument

	; test if it starts a quoted string
	cmp	cl, '"'
	jnz	__exec_parse_not_doubleq
	inc	bl
	jmp	__exec_parse_is_qstr
__exec_parse_not_doubleq:
	cmp	cl, "'"
	jnz	__exec_parse_not_simpleq
__exec_parse_is_qstr:
	inc	bl
	inc	rsi	; YYY: this and the dec rsi below to skip the quote
	mov	[rdi], rsi
	dec	rsi
	add	rdi, 8
	jmp	__exec_parse_next_char
__exec_parse_not_simpleq:

	mov	[rdi], rsi
	add	rdi, 8
	jmp	__exec_parse_next_char

__exec_parse_done:
	mov	[rsi], byte 0
	mov	[r10], byte 0

	ret

exec_print: ; *-*
	sub	rdi, exmb+mb_args
	shr	rdi, 3
	push	rdi
	debug_put_number rdi
	debug_put_bytes {" argument(s):", 10}
	debug_send

	pop	r15		; current arg number
	dec	r15
__exec_print_args:
	debug_put_bytes "exmb.args["
	debug_put_number r15
	debug_put_bytes "]: "

	mov	rdi, [exmb+mb_args+r15*8]
	push	rdi
	call	str_len		; -> rdx: length
	pop	rdi
	debug_put_buffer rdi, rdx
	debug_put_byte 10
	debug_send

	sub	r15, 1
	jnc	__exec_print_args

	debug_put_bytes "copy (exmb.back): #"
	mov	rdi, exmb+mb_back
	push	rdi
	call	str_len		; -> rdx: length
	pop	rdi
	debug_put_buffer rdi, rdx
	debug_put_bytes {"#", 10}
	debug_send

	ret

exec_fork:
	mov	rax, -38
	jmp	_panic

; exec_done:

%endif ; EXEC_ASM
