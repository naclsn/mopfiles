%include "sys/calls.asm"
%include "sys/strucs.asm"
%include "sys/errno.asm"
%include "sys/defs.asm"

%include "hlp/str.asm"
%include "hlp/iota.asm"
%include "hlp/debug.asm"

%include "args.asm"

section .data
	panic_message:		db "Panic, exiting", 10
	panic_message_len:	equ $-panic_message

section .bss
	fst:		resb STRUC_STAT_SIZE
	fd:		resq 1
	brk:		resq 1
	addr:		resq 1

section .text
	global _start

_start:
	args_parse

	sys_write 2, [fn], [fn_len]; DEBUG: write file name
	mov	rax, 10
	call	debug_put_char

	sys_brk 0		; invalid address, querry only (or smth)
	mov	[brk], rax
	call	debug_show_address

	mov	rax, rsp
	call	debug_show_address

	; TODO: init stack with a first empty node (to brk, len 0)

uh_see_file:
	sys_stat [fn], fst

	test	rax, rax
	jz	uh_open_file	; edit file if can

	neg	rax		; file or dir not exist is also ok (new)
	cmp	rax, ENOENT
	jz	uh_edit_file
	cmp	rax, ENOTDIR
	jz	uh_edit_file

	neg	rax
	jmp	uh_panic	; could not see file

uh_open_file:
	sys_open [fn], O_RDONLY
	test	rax, rax
	js	uh_panic	; could not open file
	mov	[fd], rax

	mov	rbx, [fst+st_size]
	test	rbx, rbx
	jz	uh_close_file	; if file has size 0 (cannot mmap)

	; XXX: not sure good where it puts it with passing NULL...
	;      (likely to put is on the stack apparently, should?)
	sys_mmap 0, rbx, PROT_READ, MAP_PRIVATE, rax, 0
	test	rax, rax
	js	uh_panic	; could not mmap file
	mov	[addr], rax

uh_close_file:
	sys_close [fd]		; for now automatically close the fd
				; ignores errors on close, should panic?

	; TODO: fill first node on stack with info and such

	mov	rax, [addr]
	call	debug_show_address
	sys_write 2, [addr], [fst+st_size]

uh_edit_file:

	sys_exit 0

uh_panic:
	neg	rax
	mov	r10, rax
	sys_write 2, panic_message, panic_message_len
	sys_exit r10
	ret

