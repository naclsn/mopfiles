%include "sys/calls.asm"
%include "sys/strucs.asm"
%include "sys/errno.asm"
%include "sys/defs.asm"

%include "hlp/str.asm"
%include "hlp/iota.asm"
%include "hlp/debug.asm"

%include "args.asm"		; -> args_main
%include "exec.asm"
%include "text.asm"		; -> text_main, ... tbc
%include "file.asm"		; -> file_main
%include "view.asm"		; -> view_main

section .data
	panic_msg:		db "Panic, exiting", 10
	parent_panic_msg_len:	equ $-panic_msg
				db "(no such program)", 10
	child_panic_msg_len:	equ $-panic_msg
	panic_msg_len:		dq parent_panic_msg_len

section .text
	global _start

_start:
	args_main		; argc, argv -> [fn], [fn_len], ...tbc
	exec_main		; envp -> [exmb], ...tbc
	; objs_main or something to inits the machines and such
	text_main		; -> [txt], ...tbc
	file_main		; [fn], [txt] -> [fst], [txt], ...tbc
	view_main

	; DEBUG: ask input and execve it
_loop:
	call	view_update

	sys_read 0, exmb+mb_buf, minibuf_cap ; -> rax: length
	mov	byte[exmb+mb_buf+rax], 0

	call	exec_parse

	mov	rdx, [exmb+mb_args+0]
	test	rdx, rdx	; no argument given
	jz	_loop
	mov	ebx, [rdx]
	cmp	ebx, 'quit'
	jz	_quit

	call	exec_fork
	jmp	_loop

_quit:
	sys_exit 0

_panic:
	neg	rax
	mov	r10, rax
	sys_write 2, panic_msg, [panic_msg_len]
	sys_exit r10
	jmp	_panic
