; the move_xyz procedures move the [foc] global
; (but maybe it should move some other focus struc
; so that it can implement focus expansion)
%ifndef MOVE_ASM
%define MOVE_ASM

	MHS_STANDBY:	equ 0
	MHS_RUNNING:	equ 1
	MHS_LOCKED:	equ 2
	MHS_DISABLED:	equ 3
struc machine
	mh_focus:	resb focus_size
	mh_state:	resq 1
	mh_begin:	resq 1
	mh_begin_len:	resq 1
	mh_end:		resq 1
	mh_end_len:	resq 1
endstruc

section .bss
	mach_array:	resq 1 ; machine*[]
	mach_array_len	resq 1

	; temporary in that not final solution
	tmp_mach_array_buf:	resb 4096

%macro move_main 0
	jmp	move_start
move_done:
%endmacro

section .text
move_start:
	; TODO: some hard-coded machines for now
	xor	rax, rax

	jmp	move_done

move_forward:
	cmp	qword[foc_level], FCL_CHAR
	jnz	__move_foward_notchar

	; when at the level of characters, foward is:
	; begin1 <- begin2
	; begin2 <- end1
	; end1 <- end2
	; end2 is moved 1 character
__move_forward_char:
	; backup end2 before _iter
	struc_mov anchor, [head_anchor], [foc+fc_end2]
	mov	dl, 1
	lea	rax, [foc+fc_end2]
	call	text_cast	; -> rcx: length
	test	rcx, rcx	; no char left here apparently
	jnz	__move_foward_char_can
	call	text_iter	; .. will try with next one
	call	text_cast	; -> rcx: length
	test	rcx, rcx	; still none means EOF
	jnz	__move_foward_char_can
	struc_mov anchor, [foc+fc_end2], [head_anchor]
	ret
__move_foward_char_can:
	struc_mov anchor, [foc+fc_begin1], [foc+fc_begin2]
	struc_mov anchor, [foc+fc_begin2], [foc+fc_end1]
	struc_mov anchor, [foc+fc_end1], [head_anchor]
	mov	rax, [foc+fc_end2+an_at]
	inc	rax
	mov	[foc+fc_end2+an_at], rax
	ret
__move_foward_notchar:

	sys_exit 42 ; not implemented
	ret

move_backward:
	ret
move_outward:
	ret
move_inward:
	ret

; move_done:

%endif ; MOVE_ASM
