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
	minibuf_cap:	equ 256
	minibuf_hist_cap:equ 32
struc minibuf
	mb_gap:		resb 64 ; gap is used to fit eg /usr/bin
	mb_buf:		resb minibuf_cap
	mb_args:	resq 32 ; char* (each) (\0 terminated)
	mb_paths:	resq 32 ; char* (each) (may contain empty string)
	mb_envp:	resq 1 ; char* []
	mb_hist_buf:	resb minibuf_cap*minibuf_hist_cap
	mb_hist_ents:	resq minibuf_hist_cap
endstruc

section .data
	config_dir_gap:	times 32 db 0 ; gap is used to fit /home/...
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
	; TODO: check for the '='
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
	; TODO: check for the '='
	add	r9, 5		; skip 'PATH='
	xor	r11, r11	; index in exmb.paths
	mov	rdi, r9
	call	str_len		; -> rdx
	push	rdx		; push PATH length
__exec_env_path_next:
	pop	rcx		; get leftover lenght to be scanned
	mov	rbx, rcx	; save it to be substracted from later
	mov	al, ':'
	mov	rdi, r9
	call	str_ch		; length to next ':' or end of PATH
	;cmp	[r9+rdx-1], '\'
	;jz	-3		; TODO: "\:" is part of the name
	mov	[exmb+mb_paths+r11*8], r9
	inc	r11
	inc	rdx		; include the ':'
	sub	rbx, rdx	; remove until ':' from leftover
	jz	__exec_env_path_done
	push	rbx		; push new leftover
	add	r9, rdx		; move r9 past ':'
	jmp	__exec_env_path_next
__exec_env_path_done:
	; append an empty string after last (used to compute length of last)
	lea	r9, [r9+rdx+1]	; move r9 past \0
	mov	bl, [r9-2]	; could still be ':' if was trailing
	cmp	bl, ':'		; really is \0, str_ch will have stopped right before
	jnz	__exec_env_path_was0
	dec	r9		; if not, str_ch will have stopped on it; do not include this ':'
__exec_env_path_was0:
	mov	[exmb+mb_paths+r11*8], r9
	; append a null pointer to mark the end of the array
	inc	r11
	mov	qword[exmb+mb_paths+r11*8], 0
	jmp	__exec_env_next
__exec_env_not_PATH:

	jmp	__exec_env_next

; rax: length
exec_parse:
	mov	rsi, exmb+mb_buf-1
	mov	rdi, exmb+mb_args
	mov	r10, exmb+mb_hist_buf ; TODO

	test	rax, rax ; (ie ^D)
	jz	__exec_parse_done ; *-*

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
	mov	byte[rsi], 0 ; \0 byte at the end of last arg value
	mov	byte[r10], 0 ; same way, end the copy (hist) with \0
	mov	qword[rdi], 0 ; null pointer after last arg in argv

	ret

; exec_print: ; *-*
; 	sub	rdi, exmb+mb_args
; 	shr	rdi, 3
; 	push	rdi
; 	debug_put_number rdi
; 	debug_put_bytes {" argument(s):", 10}
; 	debug_send

; 	pop	r15		; current arg number
; 	dec	r15
; __exec_print_args:
; 	debug_put_bytes "exmb.args["
; 	debug_put_number r15
; 	debug_put_bytes "]: "

; 	mov	rdi, [exmb+mb_args+r15*8]
; 	push	rdi
; 	call	str_len		; -> rdx: length
; 	pop	rdi
; 	debug_put_buffer rdi, rdx
; 	debug_put_byte 10
; 	debug_send

; 	sub	r15, 1
; 	jnc	__exec_print_args

; 	debug_put_bytes "copy (exmb.back): #"
; 	mov	rdi, exmb+mb_hist_buf
; 	push	rdi
; 	call	str_len		; -> rdx: length
; 	pop	rdi
; 	debug_put_buffer rdi, rdx
; 	debug_put_bytes {"#", 10}
; 	debug_send

; 	ret

exec_fork:
	sys_fork
	test	rax, rax
	jz	__exec_fork_child
	jc	__exec_fork_abort
	ret			; parent: everything good
__exec_fork_abort:
	jmp	_panic		; parent: something wrong
__exec_fork_child:
				; child: everything good
	mov	qword[panic_msg_len], child_panic_msg_len
	mov	byte[panic_msg+parent_panic_msg_len-1], ' '

	xor	r10, r10	; index into exmb.paths

	; if file contains '/' skip search
	mov	rdi, [exmb+mb_args+0]
	call	str_len
	mov	rcx, rdx
	mov	al, '/'
	mov	rdi, [exmb+mb_args+0]
	call	str_ch
	mov	rax, [exmb+mb_args+0]
	cmp	byte[rax+rdx], '/'
	jnz	__exec_fork_try; did not find any '/'

	sys_execve [exmb+mb_args+0], exmb+mb_args, [exmb+mb_envp]
	jmp	_panic

__exec_fork_try:
	mov	rdi, [exmb+mb_args+0]
	dec	rdi
	mov	byte[rdi], '/'
	mov	[exmb+mb_args+0], rdi
__exec_fork_next:
	mov	r12, [exmb+mb_paths+(r10+1)*8]
	test	r12, r12	; -> ZF: end is null pointer (stops)
	jz	__exec_fork_fail
	mov	r11, [exmb+mb_paths+(r10+0)*8]
	inc	r10		; exmb.paths[k++] -> r11: string
	sub	r12, r11	; -> r12: length
	dec	r12		; -> ZF: str is empty string (skips)
	jz	__exec_fork_next
	; at this point: r11 is the string, r12 its length

	; build full path by prepending to exmb.args[0]
	mov	rdi, [exmb+mb_args+0]
	sub	rdi, r12
	mov	rsi, r11
	mov	rcx, r12
	call	str_mov

	mov	rax, [exmb+mb_args+0]
	sub	rax, r12
	; with rdx length of prepended path
	sys_execve rax, exmb+mb_args, [exmb+mb_envp]

	; the execution can only continue here if execve failed
	; execve can yield:
	;  EACCES                try next (should notice)
	;  ENOENT/ESTALE/ENOTDIR try next
	;  ENODEV/ETIMEDOUT      try next
	;  default               abort, notice
	mov	rcx, rax
	cmp	rax, EACCES
	;cmovz	[did_find_something], true
	setnz	al

%macro or_err 1
	cmp	rcx, %1
	lahf
	shr	ah, 6		; 6th flag (ZF)
	or	al, ah
%endmacro
	or_err ENOENT
	or_err ESTALE
	or_err ENOTDIR
	or_err ENODEV
	or_err ETIMEDOUT
%unmacro or_err 1

	test	al, 1
	mov	rax, rcx	; restore original err
	jnz	__exec_fork_next

__exec_fork_fail:
	; the execution can only get here if all failed

	jmp	_panic

; exec_done:

%endif ; EXEC_ASM
