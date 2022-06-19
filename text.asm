; create the data structure (a `mesh`) which enables operations on text:
; * init: initialize the data structure to only contain the provided
;         text_init (rax: char*, rcx: len) -> range
; * edit: override a section of text, the provided source
;         is not duplicated and hence should not be touched
;         text_edit (range, rax: char*, rcx: len) -> range
; * undo: find latest edit in range
;         text_undo (range) -> range
; * redo: find ealriest undo in range
;         text_redo (range) -> range
; * iter: get the characters at `anchor` given by rax, then advance it
;         in the direction given by rdx
;         text_iter (rax: anchor*, rdx: direction) -> rax, rcx: len
;
; to specify a location in the structure, it is necessary to use
; the provided `anchor` struc
;
; a 'range' is always specified with `r11` and `r12` holding
; pointers to `anchor` struc instances
;
; outside of the `anchor` struc data type, the rest is internal and
; implementation details
;
; [txt] can be tested for being null (probably)
; the [txt1] anchor always points at the very beginning
; the [txt2] anchor always points at the very end
; YYY: (make sure not to change their values, always make a copy before
;      eg text_iter)
;
; XXX: for now, none of this differenciates between a character and
;      a byte (only handling of ASCII), but this should be chaned
;      to handle basic UTF-8 (if possible)
;      ((also this is application wide))

%ifndef TEXT_ASM
%define TEXT_ASM

struc node
	nd_prev:	resq 1 ; : node*
	nd_next:	resq 1 ; : node*
	nd_above:	resq 1 ; : chain*
	nd_below:	resq 1 ; : chain*
	nd_a:		resq 1 ; : char*
	nd_b:		resq 1 ; : char* (note: inclusive too)
endstruc

struc chain
	ch_node1:	resq 1 ; : node*
	ch_node2:	resq 1 ; : node*
endstruc

struc mesh
	ms_chain:	resb chain_size
	ms_stack_bot:	resq 1 ; : node*
	ms_stack_top:	resq 1 ; : node*
	ms_sources:	resq 1 ; : char*
	ms_sources_c:	resq 1 ; : char*
endstruc

struc anchor
	an_node:	resq 1
	an_at:		resq 1 ; : char*
endstruc

struc focus
	fc_begin1:	resb anchor_size
	fc_begin2:	resb anchor_size
	fc_end1:	resb anchor_size
	fc_end2:	resb anchor_size
endstruc

	FCL_CHAR:	equ 1
	FCL_WORD:	equ 2
	FCL_OBJECT:	equ 3
section .bss
	txt:		resb mesh_size
	txt1:		resb anchor_size
	txt2:		resb anchor_size
	foc:		resb focus_size
	foc_level:	resq 1

%macro text_main 0
	jmp	text_start
text_done:
%endmacro

section .text
text_start:
	sys_brk 0		; query brk
	; sources are on the traditional heap
	mov	[txt+ms_sources], rax
	mov	[txt+ms_sources_c], rax

	mov	rax, rsp	; query sp
	; nodes are on the traditional stack
	mov	[txt+ms_stack_bot], rax

	; make new node atop stack
	sub	rax, node_size
	mov	qword[rax+nd_prev], 0
	mov	qword[rax+nd_next], 0
	mov	qword[rax+nd_above], 0
	mov	qword[rax+nd_below], 0
	mov	qword[rax+nd_a], 0	; node is empty
	mov	qword[rax+nd_b], 0	; (span from 0 to 0)

	; the [txt] mesh is a chain of this single node
	mov	[txt+ms_chain+ch_node1], rax
	mov	[txt+ms_chain+ch_node2], rax

	; move stack top
	mov	[txt+ms_stack_top], rax
	mov	rsp, rax

	jmp	text_done

; rax: char*, rcx: length
text_init:
	; NOTE: does not completely init, only changes the first node's source
	mov	r11, [txt+ms_chain+ch_node1]
	mov	[r11+nd_a], rax
	add	rax, rcx
	mov	[r11+nd_b], rax

	mov	r12, [txt+ms_chain+ch_node2]

__text_update_ends:
	mov	[txt1+an_node], r11
	mov	rax, [r11+nd_a]
	mov	[txt1+an_at], rax

	mov	[txt2+an_node], r12
	mov	rax, [r12+nd_b]
	mov	[txt2+an_at], rax

	ret

; rax: anchor*, rdx: direction -> rcx: length
;   push	r9		; curr_node
text_cast:
	mov	r9, [rax+an_node]

	; such invalid anchor marks the end of iteration
	test	r9, r9
	jnz	__text_cast_proceed
	xor	rcx, rcx
	ret

__text_cast_proceed:
	mov	rcx, [rax+an_at]

	cmp	dl, 0
	jc	__text_cast_backward

__text_cast_forward:
	sub	rcx, [r9+nd_b]
	neg	rcx

__text_cast_backward:
	sub	rcx, [r9+nd_a]

__text_cast_ret:
	ret

; rax: anchor*, rdx: direction -> rcx: length
;   push	r9		; curr_node
text_iter:
	mov	r9, [rax+an_node]

	; such invalid anchor marks the end of iteration
	test	r9, r9
	jnz	__text_iter_proceed
	xor	rcx, rcx
	ret

__text_iter_proceed:
	mov	rcx, [rax+an_at]

	cmp	dl, 0
	jc	__text_iter_backward

__text_iter_forward:
	sub	rcx, [r9+nd_b]
	neg	rcx

	; move anchor to next node (even if none)
	mov	r9, [r9+nd_next]
	mov	[rax+an_node], r9
	test	r9, r9
	jz	__text_iter_ret
	mov	r9, [r9+nd_a]
	mov	[rax+an_at], r9

	ret

__text_iter_backward:
	sub	rcx, [r9+nd_a]

	; move anchor to prev node (even if none)
	mov	r9, [r9+nd_prev]
	mov	[rax+an_node], r9
	test	r9, r9
	jz	__text_iter_ret
	mov	r9, [r9+nd_b]
	mov	[rax+an_at], r9

__text_iter_ret:
	ret

	jmp	text_done
; text_done:

%endif ; TEXT_ASM
