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
;         text_iter (rax: anchor*, rdx: direction) -> rax: anchor*, rcx: len
;
; to specify a location in the structure, it is necessary to use
; the provided `anchor` struc
;
; a 'range' is always specified with `r11` and `r12` holding
; pointers to `anchor` struc instances
;
; outside of the `anchor` struc data type, the described procedures
; and the presence of a [txt] memory cell, the rest is internal and
; implementation details
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
	an_at:		resq 1 ; : (technically) char*
endstruc

section .bss
	txt:		resb mesh_size

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
	sub	rax, node_size
	mov	[txt+ms_stack_top], rax
	mov	rsp, rax

	jmp	text_done

; rax: char*, rcx: length
text_init:
	; XXX: should completely init, ie. single node and stack_top/bot etc...
	mov	r9, [txt+ms_chain+ch_node1]
	mov	[r9+nd_a], rax
	add	rax, rcx
	mov	[r9+nd_b], rax
	ret

; rax: anchor*, rdx: direction -> rcx: length
;   push	r9		; curr_node
text_iter:
	mov	r9, [rax+an_node]

	; such invalid anchor marks the end of iteration
	test	r9, r9
	jnz	_text_iter_proceed
	xor	rcx, rcx
	ret

_text_iter_proceed:
	mov	rcx, [rax+an_at]

	cmp	dl, 0
	jc	_text_iter_backward

_text_iter_forward:
	sub	rcx, [r9+nd_b]
	neg	rcx

	; move anchor to next node (even if none)
	mov	r9, [r9+nd_next]
	mov	[rax+an_node], r9
	mov	r9, [r9+nd_a]
	mov	[rax+an_at], r9

	ret

_text_iter_backward:
	sub	rcx, [r9+nd_a]

	; move anchor to prev node (even if none)
	mov	r9, [r9+nd_prev]
	mov	[rax+an_node], r9
	mov	r9, [r9+nd_a]
	mov	[rax+an_at], r9

	ret

	jmp	text_done
; text_done:

%endif ; TEXT_ASM
