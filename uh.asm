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

	; DEBUG: code below shows last 20 characters (does not check for size)
	; TODO: change into using the [txt] mesh
	;       when enough procedures to do so
	mov	rax, [fma]	; (yes, this /is/ the reason it segfaults on ENOENTs)
	mov	rbx, [fst+st_size]
	sub	rbx, 20
	add	rax, rbx
	debug_put_buffer rax, 20
	debug_send

	; DEBUG: printf("%s (%db)", [fn], [fst+st_size])
	debug_put_buffer [fn], [fn_len]
	debug_put_bytes " ("
	debug_put_number [fst+st_size]
	debug_put_bytes {')', 10}
	debug_send

	sys_exit 0
	mov	rax, 1

_panic:
	neg	rax
	mov	r10, rax
	sys_write 2, panic_msg, panic_msg_len
	sys_exit r10
	ret
