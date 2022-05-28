%ifndef VIEW_ASM
%define VIEW_ASM

section .bss
	tmp_anchor:		resb anchor_size

%macro view_main 0
	jmp	view_start
view_done:
%endmacro

section .text
view_start:
	; DEBUG: code below shows last 20 characters
	lea	rsi, [txt1]
	lea	rdi, [tmp_anchor]
	mov	rcx, anchor_size
	call	str_mov		; weird attempt at copying a struc

	mov	rax, tmp_anchor
	mov	rbx, [rax+an_at]
	mov	dl, 1
	call	text_iter	; -> rcx: length

	sub	rcx, 20
	jc	__less_than_20
	add	rbx, rcx
	debug_put_buffer rbx, 20
	jmp	__done_with_that
__less_than_20:
	add	rcx, 20
	debug_put_buffer rbx, rcx
	debug_send
__done_with_that:

	; DEBUG: printf("%s (%db)", [fn], [fst+st_size])
	debug_put_buffer [fn], [fn_len]
	debug_put_bytes " ("
	debug_put_number [fst+st_size]
	debug_put_bytes {')', 10}
	debug_send

	jmp	view_done
; view_done:

%endif ; VIEW_ASM
