%ifndef ARGS_ASM
%define ARGS_ASM

section .data
	usage_part_1:		db "Usage: "
	usage_part_1_len:	equ $-usage_part_1
	usage_part_2:		db " <filename>", 10
	usage_part_2_len:	equ $-usage_part_2

	unk_arg_part_1:	 	db "Unknown argument: '"
	unk_arg_part_1_len:	equ $-unk_arg_part_1
	unk_arg_part_2:		db "'", 10
	unk_arg_part_2_len:	equ $-unk_arg_part_2

	fn:			dq $+8
				db "#scratch#"
	fn_len:			dq $-fn

section .bss
	arg0:			resq 1
	arg0_len:		resq 1

; @rem [rsp]: argc, [rsp+8]: args[0] // XXX move to sys/arch
;   r10: k in argv[k]
;   r9: argv[k]
;   bx (when used): first 2 char at [r9]
; TODO: write what does (better)
%macro args_main 0
	mov	r10, 1
	; grab program name from args[0]
	mov	r9, [rsp+r10*8]	; -> r9: args[0]
	mov	[arg0], r9
	mov	rdi, r9
	call	str_len		; -> rdx: len
	mov	[arg0_len], rdx

args_next:
	cmp	[rsp], r10
	jnz	args_process_one

	; no argument (for now do nothing, keep [fn] to default)
	jmp	args_done

args_process_one:
	inc	r10

	; need to test here for flags that may not be followed by anything
	mov	r9, [rsp+r10*8]	; -> r9: args[k]
	mov	bx, [r9]

	; -h
	cmp	bx, '-h'
	jz	args_usage

	; if this is the last arg at this point, should be filename
	cmp	[rsp], r10
	jz	args_last

	; tests here for most arguments (not last and not [0])
	mov	r9, [rsp+r10*8]	; -> r9: args[k]
	mov	bx, [r9]

	; -h
	cmp	bx, '-h'
	jz	args_usage

	; --
	cmp	bx, '--'
	jz	args_before_last

	; unknown arg
	sys_write 2, unk_arg_part_1, unk_arg_part_1_len
	mov	rdi, r9
	call	str_len		; -> rdx: len
	sys_write 2, r9, rdx
	sys_write 2, unk_arg_part_2, unk_arg_part_2_len
	jmp	args_usage	; unknown argument (XXX: may not want to abort)

args_usage:
	sys_write 2, usage_part_1, usage_part_1_len
	sys_write 2, [arg0], [arg0_len]
	sys_write 2, usage_part_2, usage_part_2_len
	sys_exit 22
	jmp	_panic

args_before_last:
	inc	r10
args_last:
	; <filename>
	mov	r9, [rsp+r10*8]	; -> r9: args[-1]

	mov	[fn], r9
	mov	rdi, r9
	call	str_len	; -> rdx: len
	mov	[fn_len], rdx

	; XXX: move checking and opening file here
	;      and when called with no specified file
	;      it simply jumps to `args_done` below
	;      (don't want to try and open a file named
	;      "#scratch#" or something)
	; for some obscure reason, for now that works
	; as it does not seem to want to stat a file
	; which name starts with '#'

args_done:
%endmacro

%endif ; ARGS_ASM
