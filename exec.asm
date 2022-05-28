; TODO: doc and such
; uses exmb to get user input, argsplit it then execve with
; the first arg as the program
;
; NOTE/TODO: process some escape sequences, perform argument
;            concatenation and quote removal by copying back
;            the input onto itself with an increasing offset

%ifndef EXEC_ASM
%define EXEC_ASM

; NOTE: the minibufer is designed to hold enough information
;       to facilitate a syscall to execve (so it is probably
;       not the right struc to use if for a simple buffer)
	minibuf_cap:	equ 4096
struc minibuf
	mb_gap:		resb minibuf_cap/8 ; YYY: idk, just don't have dumb long path in your PATH env var
	mb_buf:		resb minibuf_cap
	mb_back:	resb minibuf_cap
	mb_args:	resq 32 ; char* (each) ; YYY: one can only say so many args before becoming a pirate...
	mb_paths:	resq 32 ; char* (each) ; YYY: one can only have so many paths before being lost...
	mb_envp:	resq 1 ; char* []
endstruc

section .data
	config_dir_gap:	times 32 db 0 ; YYY: one's way home can only be so long before they sleep at work
			db "/"
	config_dir_s:	db ".config/uh", 0
	config_dir:	dq config_dir_s

section .bss
	exmb:		resb minibuf_size

%macro exec_main 0
	jmp	exec_start
exec_done:
%endmacro

section .text
; @rem [rsp]: argc, [rsp+argc+1+1]: envp[0]
; YYY: maybe irrelevant
;   r10: k in envp[k]
;   r9: envp[k]
;   ebx: first 4 char at [r9]
; YYY: out of flemme, only checks the 4 first characters
exec_start:
	; DOING:
	; get HOME and set first path thingy
	; get PATH and parse it into exmb.path

	mov	r10, [rsp]	; -> argc
	; +16 is to skip the null past argv
	lea	r9, [rsp+r10*8+16]
	mov	[exmb+mb_envp], r9

	; interesting env vars are: HOME, PATH [, TERM?, SHELL?, EDITOR?]
__exec_env_next:
	mov	r9, [rsp+r10*8+16]
	test	r9, r9
	jz	exec_done	; was last env var

	mov	ebx, [r9]	; -> envp[0][0..4]
	inc	r10

	; HOME[=]
	cmp	ebx, 'HOME'
	jnz	__exec_env_not_HOME
	add	r9, 5		; skip 'HOME='
	mov	rdi, r9
	call	str_len		; -> rdx
	mov	rcx, rdx	; copy it right before config_dir_s
	mov	rsi, r9
	neg	rdx
	lea	rdi, [config_dir_s-1+rdx]
	mov	[config_dir], rdi
	call	str_mov
	jmp	__exec_env_next
__exec_env_not_HOME:

	; PATH[=]
	cmp	ebx, 'PATH'
	jnz	__exec_env_not_PATH
	; *-*
	jmp	__exec_env_next
__exec_env_not_PATH:

	jmp	__exec_env_next

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
	; TODO/REM:
	; if file contains '/' skip search
	; execve can yield:
	;  EACCES                try next, notice
	;  ENOENT/ESTALE/ENOTDIR try next
	;  ENODEV/ETIMEDOUT      try next
	;  default               abort, notice
	sys_execve [exmb+mb_args+0], exmb+mb_args, [exmb+mb_envp]
	jmp	_panic

; exec_done:

%endif ; EXEC_ASM
