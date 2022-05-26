%ifndef IOTA_ASM
%define IOTA_ASM

; rax: value, rdi: uint base, rsi: char* buf -> rdx: len
; uses rcx: counter
iota:
	xor	rcx, rcx
	cmp	rax, 0
	jz	iota_zero
	jns	iota_repeate
	neg	rax
	mov	word[rsi], '-'
iota_repeate:
	inc	rcx
	xor	rdx, rdx
	idiv	rdi		; rdx:rax // 10
	push	rdx		; remainder
	test	rax, rax	; quotient
	jnz	iota_repeate
	mov	rdx, rcx	; get the final length
	mov	rax, [rsi]	; adds 1 if negative
	cmp	al, '-'
	jnz	iota_reverse
	inc	rsi
	inc	rdx
iota_reverse:
	pop	rax
	cmp	al, 10
	js	iota_decimal
	add	al, 'A'-'0'-10
iota_decimal:
	add	al, '0'
	mov	[rsi], al
	inc	rsi
	dec	rcx
	jnz	iota_reverse
	ret
iota_zero:
	mov	dword[rsi], '0'
	mov	rdx, 1
	ret

%endif ; IOTA_ASM
