%ifndef FILE_ASM
%define FILE_ASM

section .bss
	fst:		resb stat_size
	fd:		resq 1
	fma:		resq 1	; TODO: figure out

; TODO: write what does
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

	mov	rax, [fma]
	mov	rcx, [fst+st_size]
	call	text_init	; fill first node on stack

file_done:
%endmacro

%endif ; FILE_ASM
