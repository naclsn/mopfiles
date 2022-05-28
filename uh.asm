%include "sys/calls.asm"
%include "sys/strucs.asm"
%include "sys/errno.asm"
%include "sys/defs.asm"

%include "hlp/str.asm"
%include "hlp/iota.asm"
%include "hlp/debug.asm"

%include "args.asm"		; -> args_main
%include "text.asm"		; -> text_main, ... tbc
%include "file.asm"		; -> file_main
%include "view.asm"		; -> view_main
%include "exec.asm"

section .data
	panic_msg:		db "Panic, exiting", 10
	panic_msg_len:		equ $-panic_msg

section .text
	global _start

_start:
	args_main		; -> [fn], [fn_len], ...tbc
	; objs_main or something to inits the machines and such
	text_main		; -> [txt], ...tbc
	file_main		; [fn], [txt] -> [fst], [txt], ...tbc
	view_main
	exec_main

	sys_exit 0

_panic:
	neg	rax
	mov	r10, rax
	sys_write 2, panic_msg, panic_msg_len
	sys_exit r10
	jmp	_panic
