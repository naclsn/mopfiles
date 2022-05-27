; parses the command line arguments
; * [arg0] name sais it
; * [fn] file name provided or default "#scratch#"
;        note that is the only argument which does
;        not get a name starting with `arg_`
;
; NOTE: aborts on unknown argument, may not want that...

%ifndef ARGS_ASM
%define ARGS_ASM

section .data
	usage_part_1:		db "Usage: "
	usage_part_1_len:	equ $-usage_part_1
	usage_part_2:		db                 \
" [...] [[--] <filename>]",                    10, \
"  -h, --help     show this help",             10, \
"  -V, --version  print version successfully", 10, \
"  -R             start in readonly mode",     10, \
"  -w <filename>  write typed into file",      10
	usage_part_2_len:	equ $-usage_part_2

	version_part_1:	 	db                        \
"The uh text editor, version $version ($build_info)", 10, \
"License: $license",                                  10
	version_part_1_len:	equ $-version_part_1

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

; @rem [rsp]: argc, [rsp+8]: args[0]
;   r10: k in argv[k]
;   r9: argv[k]
;   bx (when used): first 2 char at [r9]
%macro args_main 0
	jmp	args_start
args_done:
%endmacro

section .text
args_start:
	mov	r10, 1
	; grab program name from args[0]
	mov	rdi, [rsp+r10*8]; -> rdi: args[0]
	mov	[arg0], rdi
	call	str_len		; -> rdx: len
	mov	[arg0_len], rdx

args_next:
	cmp	[rsp], r10
	jz	args_done	; no more arguments

	inc	r10
	mov	r9, [rsp+r10*8]	; -> r9: args[k]
	mov	ebx, [r9]	; ebx first 4 bytes (bx first 2)

	; --he[lp]
	cmp	bx, '-h'
	jz	args_usage
	cmp	ebx, '--he'
	jz	args_usage

	; --ve[rsion]
	cmp	bx, '-V'
	jz	args_version
	cmp	ebx, '--ve'
	jz	args_version

	; -R
	cmp	bx, '-R'
	jnz	_args_was_not_readonly
	;mov	[arg_readonly], 1
	jmp	args_next
_args_was_not_readonly:

	; -w <filename>
	cmp	bx, '-w'
	jnz	_args_was_not_writetyped
	; XXX: <filename> could be attached to -w
	;      for that need to check if \0 after
	;inc	r10
	;mov	r9, [rsp+r10*8]	; -> r9: <filename>
	;mov	[arg_writetyped], r9
	jmp	args_next
_args_was_not_writetyped:

	; -- <filename>
	cmp	bx, '--'
	jnz	_args_was_not_fnescape
	inc	r10
	jmp	args_last	; reset is <filename>
_args_was_not_fnescape:

	; <filename>
	cmp	[rsp], r10	; is it last argument
	jz	args_last

	; unknown arg
	sys_write 2, unk_arg_part_1, unk_arg_part_1_len
	mov	rdi, r9
	call	str_len		; -> rdx: len
	sys_write 2, r9, rdx
	sys_write 2, unk_arg_part_2, unk_arg_part_2_len
	;jmp	args_usage	; (XXX: may not want to abort)

args_usage:
	sys_write 2, usage_part_1, usage_part_1_len
	sys_write 2, [arg0], [arg0_len]
	sys_write 2, usage_part_2, usage_part_2_len
	sys_exit 22
	jmp	_panic

args_version:
	sys_write 1, version_part_1, version_part_1_len
	sys_exit 0
	jmp	_panic

args_last:
	; <filename>
	; TODO: concatenate ...args into fn
	mov	rdi, [rsp+r10*8]; -> rd: args[-1]

	mov	[fn], rdi
	call	str_len		; -> rdx: len
	mov	[fn_len], rdx

	; XXX: move checking and opening file here
	;      and when called with no specified file
	;      it simply jumps to `args_done` below
	;      (don't want to try and open a file named
	;      "#scratch#" or something)
	; for some obscure reason, for now that works
	; as it does not seem to want to stat a file
	; which name starts with '#'

	jmp	args_done
; args_done:

%endif ; ARGS_ASM
