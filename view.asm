%ifndef VIEW_ASM
%define VIEW_ASM

section .bss
	top_anchor:		resb anchor_size
	head_anchor:		resb anchor_size ; printing head

	; this buffer is temporary in that
	; it will not be the final solution
	tmp_ab:		resb 4096

%macro view_main 0
	jmp	view_start
view_done:
%endmacro

section .text
view_start:
	; get top_anchor to the very beginning of the text
	lea	rsi, [txt1]
	lea	rdi, [top_anchor]
	mov	rcx, anchor_size
	call	str_mov

	jmp	view_done

view_update:
	; shows 20 lines from top_anchor
	; TODO: get terminal size and such

	; move the head back to the top
	lea	rsi, [top_anchor]
	lea	rdi, [head_anchor]
	mov	rcx, anchor_size
	call	str_mov

	xor	r10, r10	; index in ab
	mov	r11, 20		; line counter

__view_update_advance:
	; save node starting location
	mov	rbx, [head_anchor+an_at]
	; after the call to test_iter,
	; [rbx] to [rbx+rcx] is the printable buffer

	mov	rax, head_anchor
	mov	dl, 1
	call	text_iter	; -> rcx: length, updates [rax]

__view_update_nextch:
	mov	dl, [rbx]
	mov	[tmp_ab+r10], dl

	cmp	dl, 10
	lahf
	shr	ax, 6+8		; 6th flag (ZF)
	and	rax, 1
	sub	r11, rax
	jz	__view_update_donech

	inc	rbx
	inc	r10
	dec	rcx
	jnz	__view_update_nextch
__view_update_donech:

	; TODO: proper status bar
	;       for now: printf("--- %s\n:", fn)
	inc	r10
	mov	byte[tmp_ab+r10], '-'
	inc	r10
	mov	byte[tmp_ab+r10], '-'
	inc	r10
	mov	byte[tmp_ab+r10], '-'
	inc	r10
	mov	byte[tmp_ab+r10], ' '
	inc	r10

	mov	rsi, [fn]
	lea	rdi, [tmp_ab+r10]
	mov	rcx, [fn_len]
	add	r10, rcx
	call	str_mov

	mov	byte[tmp_ab+r10], 10
	inc	r10
	mov	byte[tmp_ab+r10], ':'
	inc	r10

	sys_write 1, tmp_ab, r10

	ret

; view_done:

%endif ; VIEW_ASM
