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
	panic_msg_len:		equ $-panic_msg

section .text
	global _start

_start:
	args_main		; argc, argv -> [fn], [fn_len], ...tbc
	exec_main		; envp -> [exmb], ...tbc
	; objs_main or something to inits the machines and such
	text_main		; -> [txt], ...tbc
	file_main		; [fn], [txt] -> [fst], [txt], ...tbc
	view_main

	; DEBUG: print path entries
	debug_put_bytes {"found PATH entries:", 10}
	debug_send
	xor	r15, r15
__path_print_next:
	mov	rbx, [exmb+mb_paths+(r15+1)*8]
	test	rbx, rbx	; -> ZF: end is null pointer (should stop)
	jz	__path_print_done
	mov	rax, [exmb+mb_paths+(r15+0)*8]
	sub	rbx, rax
	inc	r15
	dec	rbx		; -> ZF: str is empty string (should skip)
	jz	__path_print_next
	debug_put_buffer rax, rbx
	debug_put_byte 10
	debug_send
	jmp	__path_print_next
__path_print_done:

	; DEBUG: print config_dir
	debug_put_bytes "config_dir: "
	mov	rdi, [config_dir]
	call	str_len
	debug_put_buffer [config_dir], rdx
	debug_put_byte 10
	debug_send

	; DEBUG: ask input and execve it (no forking yet)
__test_loop_repeat:
	sys_read 0, exmb+mb_buf, minibuf_cap ; -> rax: length
	mov	[exmb+mb_buf+rax], byte 0

	call	exec_parse

	mov	rdx, [exmb+mb_args+0]
	mov	ebx, [rdx]
	cmp	ebx, 'quit'
	jz	__test_loop_quit

	call	exec_print ; *-*
	call	exec_fork
	jmp	__test_loop_repeat
__test_loop_quit:

	sys_exit 0

_panic:
	neg	rax
	mov	r10, rax
	sys_write 2, panic_msg, panic_msg_len
	sys_exit r10
	jmp	_panic
