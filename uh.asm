%include "sys/calls.asm"
%include "sys/strucs.asm"
%include "sys/errno.asm"
%include "sys/defs.asm"

%include "hlp/str.asm"
%include "hlp/iota.asm"
%include "hlp/debug.asm"

%include "args.asm"

section .data

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

	; TODO: init stack with a first empty node (to brk, len 0)

;uh_see_file:
	sys_stat [fn], fst

	test	rax, rax
	jz	uh_open_file	; edit file if can

	neg	rax		; file or dir not exist is also ok (new)
	cmp	rax, ENOENT
	jz	uh_edit_file
	cmp	rax, ENOTDIR
	jz	uh_edit_file

	jmp uh_panic		; could not see file fsr

uh_open_file:
	sys_open [fn], O_RDONLY
	cmp	rax, -1
	jz	uh_panic	; could not open file fsr
	mov	[fd], rax

	sys_mmap [brk], [fst+st_size], PROT_READ, MAP_PRIVATE, [fd], 0
	cmp	rax, -1
	jz	uh_panic	; could not mmap file fsr
	mov	[addr], rax

	sys_close [fd]		; for now automatically close the fd jic
				; ignores errors on close, should panic?

	; TODO: fill first node on stack with info and such

	mov	rax, [addr]
	call	debug_show_address
	sys_write 2, [addr], 6	; FIXME: nothing, same on [brk], brainfried

uh_edit_file:

	sys_exit -1

uh_panic:
	sys_exit rax
	ret

