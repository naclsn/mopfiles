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
; * iter: iterate through the text as characters and apply
;         arbitrary procedure
;         text_iter (rax: anchor, rdx: direction, rex: proc to call)
;                   -> rax: anchor, rcx: counter
;         NOTE: this procedure will likely be reworked into
;               something like a macro that simply fetches next
;               so the loop would essentially be done by caller
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
	nd_b:		resq 1 ; : char*
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

section .bss
	txt:		resb mesh_size

%macro text_main 0
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
	add	rax, node_size
	mov	[txt+ms_stack_top], rax
	mov	rsp, rax

	jmp	text_done

text_init:
	mov	[txt+ms_chain+ch_node1+nd_a], rax
	add	rax, rcx
	mov	[txt+ms_chain+ch_node1+nd_b], rax
	ret

text_done:
%endmacro

%endif ; TEXT_ASM
