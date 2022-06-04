%ifndef VIEW_ASM
%define VIEW_ASM

section .bss
	vwsize:			resb winsize_size

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

	call	view_initial

	jmp	view_done

view_initial:
	; on this draw (and only this one)
	; it is ensured to be at the file's very beginning
	; ie. [txt1] === [top_anchor]

	; initialize the focus on the first [object? entity?]
	; TODO: use machine search (mach_forward)
	;       for now simply (no) scans to next space
	; move the head back to the top
	lea	rsi, [top_anchor]
	lea	rdi, [head_anchor]
	mov	rcx, anchor_size
	call	str_mov
__view_initial_advance:
	; save node starting location
	mov	rbx, [head_anchor+an_at]
	; after the call to test_iter,
	; [rbx] to [rbx+rcx] is the printable buffer
	test	rbx, rbx	; previous was last node
	jz	__view_initial_donech
	mov	rax, head_anchor
	mov	dl, 1
	call	text_iter	; -> rcx: length, updates [rax]
__view_initial_nextch:
	mov	dl, [rbx]
	inc	rbx
	cmp	dl, ' '		; found the first space character
	jz	__view_initial_foundspace
	dec	rcx
	jnz	__view_initial_nextch
	jmp	__view_initial_donech
__view_initial_foundspace:
	; the focus's begin is set to be txt1..txt1
	mov	rax, [txt1+an_node]
	mov	[foc+fc_begin1+an_node], rax
	mov	rax, [txt1+an_at]
	mov	[foc+fc_begin1+an_at], rax
	mov	rax, [txt1+an_node]
	mov	[foc+fc_begin2+an_node], rax
	mov	rax, [txt1+an_at]
	mov	[foc+fc_begin2+an_at], rax
	; and its end is the head_anchor-1..head_anchor
	mov	rax, [txt1+an_node]
	mov	[foc+fc_end1+an_node], rax
	lea	rax, [rbx-1]
	mov	[foc+fc_end1+an_at], rax
	mov	rax, [txt1+an_node]
	mov	[foc+fc_end2+an_node], rax
	mov	rax, rbx
	mov	[foc+fc_end2+an_at], rax
__view_initial_donech:

	; TODO: (waaay later) highlight first pass
	;       find first range

	jmp	__view_update_common

view_update:
	; start by prepending a cls sequence
	mov	dword[tmp_ab+0], `\e[1J`
	mov	dword[tmp_ab+4], `\e[H `
	; NOTE: cleaner sequence would be eg.
	;       "\e[H\e[2J\e[3J"
__view_update_common:

	; get terminal size (does not use signal)
	; NOTE: uses the stdout fd because that makes the most sense
	sys_ioctl 1, TIOCGWINSZ, vwsize
	test	rax, rax
	jz	__view_update_isterm
	jmp	_panic		; not a terminal
__view_update_isterm:

	; move the head back to the top
	lea	rsi, [top_anchor]
	lea	rdi, [head_anchor]
	mov	rcx, anchor_size
	call	str_mov

	mov	r10, 7		; index in ab (start after the cls)
	xor	r11, r11	; line counter
	mov	r11w, [vwsize+ws_row]
	dec	r11		; *-*
	dec	r11		; account for the temporary "---\n:"
	dec	r11		; YYY: when user press <return> (also tmp)

__view_update_advance:
	; save node starting location
	mov	rbx, [head_anchor+an_at]
	; after the call to test_iter,
	; [rbx] to [rbx+rcx] is the printable buffer

	test	rbx, rbx	; previous was last node
	jz	__view_update_donech

	mov	rax, head_anchor
	mov	dl, 1
	call	text_iter	; -> rcx: length, updates [rax]

__view_update_nextch:
	mov	dl, [rbx]

	; (TODO: optionaly make it cleaner)
	; if (here == focus open bound)
	;   invert (7)
	cmp	rbx, [foc+fc_begin1+an_at]
	jnz	__view_update_notbeginopenbound
	mov	dword[tmp_ab+r10], `\e[7m`
	add	r10, 4
__view_update_notbeginopenbound:
	cmp	rbx, [foc+fc_end1+an_at]
	jnz	__view_update_notendopenbound
	mov	dword[tmp_ab+r10], `\e[7m`
	add	r10, 4
__view_update_notendopenbound:
	; else if (here == focus close bound)
	;   revert (27)
	cmp	rbx, [foc+fc_begin2+an_at]
	jnz	__view_update_notbeginclosebound
	mov	dword[tmp_ab+r10], `\e[27`
	add	r10, 4
	mov	byte[tmp_ab+r10], 'm'
	inc	r10
__view_update_notbeginclosebound:
	cmp	rbx, [foc+fc_end2+an_at]
	jnz	__view_update_notendclosebound
	mov	dword[tmp_ab+r10], `\e[27`
	add	r10, 4
	mov	byte[tmp_ab+r10], 'm'
	inc	r10
__view_update_notendclosebound:

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

	test	r11, r11
	jz	__view_update_isfilled
	; pad with ~ lines
__view_update_padempty:
	mov	word[tmp_ab+r10], `~\n`
	add	r10, 2
	dec	r11
	jnz	__view_update_padempty
	inc	r10
__view_update_isfilled:

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
