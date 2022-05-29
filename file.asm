; given the name of the edited file in [fn], it will load said
; load said file in memory (mmap, XXX: may need fallback) and
; init the text data structre [txt] (which should already exist)
;
; the following are made available and accurate:
; * [fd] the file descriptor (NOTE: for now it is closed right away)
; * [fst] to a stat struc (especially [fst+st_size])
; * [fma] the pointer to the mmap (or bytes of the file anyway)

%ifndef FILE_ASM
%define FILE_ASM

section .bss
	fst:		resb stat_size
	fd:		resq 1
	fma:		resq 1	; TODO: figure out

%macro file_main 0
	jmp	file_start
file_done:
%endmacro

section .text
file_start:
	cmp	qword[fn], fn_default
	jz	file_none	; skip if no file given

	sys_stat [fn], fst

	test	rax, rax
	jz	file_open	; edit file if can

	neg	rax		; file or dir not exist is also ok (new)
	test	rax, ENOENT
	jz	file_none
	test	rax, ENOTDIR
	jz	file_none

	neg	rax
	jmp	_panic		; could not see file

file_open:
	sys_open [fn], O_RDONLY
	test	rax, rax
	js	_panic		; could not open file
	mov	[fd], rax

	mov	rbx, [fst+st_size]
	test	rbx, rbx
	jz	file_close	; if file has size 0 (cannot mmap)

	sys_mmap 0, rbx, PROT_READ, MAP_PRIVATE, rax, 0
	test	rax, rax
	js	_panic		; could not mmap file
	mov	[fma], rax

file_close:
	sys_close [fd]		; for now automatically close the fd

	; inits the text data structure hereafter

file_none:
	; note that when no file, it gets init to an empty text
	; as [fma] and [fst] are still 0 (from .bss)
	mov	rax, [fma]
	mov	rcx, [fst+st_size]
	call	text_init

	jmp	file_done
; file_done:

%endif ; FILE_ASM
