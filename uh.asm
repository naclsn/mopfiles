%include "sys/calls.asm"
%include "sys/strucs.asm"
%include "sys/errno.asm"
%include "sys/defs.asm"

%include "hlp/str.asm"
%include "hlp/iota.asm"
%include "hlp/debug.asm"

%include "args.asm"		; -> args_main
%include "file.asm"		; -> file_main

section .data
	panic_msg:		db "Panic, exiting", 10
	panic_msg_len:		equ $-panic_msg

section .bss
	brk:		resq 1
	addr:		resq 1

section .text
	global _start

_start:
	args_main		; -> [fn], [fn_len], ...tbc

	sys_brk 0		; query brk (k? why there? why now?)
	mov	[brk], rax	;           ((see for actual usage and
				;           storage of user input))

	; TODO: init stack with a first empty node (to NULL, len 0)
	; probably will do without and just have [head] directly
	; be NULL (0)

	file_main		; [fn] -> [fst], [addr], ...tbc

	; DEBUG: printf("%s (%db)", [fn], [fst+st_size])
	sys_write 2, [fn], [fn_len]
	mov	rax, ' '
	call	debug_put_char
	mov	rax, '('
	call	debug_put_char
	mov	rax, [fst+st_size]
	call	debug_put_number
	mov	rax, ')'
	call	debug_put_char
	mov	rax, 10
	call	debug_put_char
	; yes, character by character...
	; ((all that begs the need for at least a shallow 'printf', no?))

	sys_exit 0
	mov	rax, 1

_panic:
	neg	rax
	mov	r10, rax
	sys_write 2, panic_msg, panic_msg_len
	sys_exit r10
	ret

