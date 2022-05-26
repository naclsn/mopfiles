%ifndef FILE_ASM
%define FILE_ASM

section .bss
	fst:		resb stat_size
	fd:		resq 1
	fma:		resq 1	; TODO: figure out

%macro file_main 0
	sys_stat [fn], fst

	test	rax, rax
	jz	file_open	; edit file if can

	neg	rax		; file or dir not exist is also ok (new)
	test	rax, ENOENT
	jz	file_done
	test	rax, ENOTDIR
	jz	file_done

	neg	rax
	jmp	_panic	; could not see file

file_open:
	sys_open [fn], O_RDONLY
	test	rax, rax
	js	_panic	; could not open file
	mov	[fd], rax

	mov	rbx, [fst+st_size]
	test	rbx, rbx
	jz	file_close	; if file has size 0 (cannot mmap)

	sys_mmap 0, rbx, PROT_READ, MAP_PRIVATE, rax, 0
	test	rax, rax
	js	_panic	; could not mmap file
	mov	[fma], rax

file_close:
	sys_close [fd]		; for now automatically close the fd
				; ignores errors on close, should panic?

	; TODO: fill first node on stack with info and such

	; DEBUG: code below shows last 20 characters (does not check for size tho)
	mov	rax, [fma]
	mov	rbx, [fst+st_size]
	sub	rbx, 20
	add	rax, rbx
	debug_put_buffer rax, 20
	debug_send

file_done:
%endmacro

%endif ; FILE_ASM
