%ifndef TEXT_ASM
%define TEXT_ASM

struc node
	nd_prev:	resb 8 ; : node*
	nd_next:	resb 8 ; : node*
	nd_above:	resb 8 ; : chain*
	nd_below:	resb 8 ; : chain*
	nd_a:		resb 8 ; : char*
	nd_b:		resb 8 ; : char*
endstruc

struc chain
	ch_node1:	resb 8 ; : node*
	ch_node2:	resb 8 ; : node*
endstruc

struc mesh
	ms_chain:	resb chain_size
	ms_stack_bot:	resb 8 ; : node*
	ms_stack_top:	resb 8 ; : node*
	ms_sources:	resb 8 ; : char*
	ms_sources_c:	resb 8 ; : char*
endstruc

section .bss
	txt:		resb mesh_size

; TODO: write what does
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

text_init:			; used to fixup when loading file
	mov	[txt+ms_chain+ch_node1+nd_a], rax
	add	rax, rcx
	mov	[txt+ms_chain+ch_node1+nd_b], rax
	ret

text_done:
%endmacro

%endif ; TEXT_ASM
