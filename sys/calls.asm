%ifndef CALLS_ASM
%define CALLS_ASM

%macro sys_read 3 ; (fd, *buf, count)
	mov	rdx, %3
	mov	rsi, %2
	mov	rdi, %1
	mov	rax, 0
	syscall
%endmacro

%macro sys_write 3 ; (fd, *buf, count)
	mov	rdx, %3
	mov	rsi, %2
	mov	rdi, %1
	mov	rax, 1
	syscall
%endmacro

%macro sys_open 2 ; (filename, mode)
	mov	rsi, %2
	mov	rdi, %1
	mov	rax, 2
	syscall
%endmacro

%macro sys_close 1 ; (fd)
	mov	rdi, %1
	mov	rax, 3
	syscall
%endmacro

%macro sys_stat 2 ; (filename, *statbuf)
	mov	rsi, %2
	mov	rdi, %1
	mov	rax, 4
	syscall
%endmacro

%macro sys_mmap 6 ; (addr, length, prot, flags, fd, offset)
	mov	r9, %6
	mov	r8, %5
	mov	r10, %4
	mov	rdx, %3
	mov	rsi, %2
	mov	rdi, %1
	mov	rax, 9
	syscall
%endmacro

%macro sys_brk 1 ; (brk)
	mov	rdi, %1
	mov	rax, 12
	syscall
%endmacro

%macro sys_ioctl 3 ; (fd, request, argp)
	mov	rdx, %3
	mov	rsi, %2
	mov	rdi, %1
	mov	rax, 16
	syscall
%endmacro

%macro sys_exit 1 ; (status)
	mov	rdi, %1
	mov	rax, 60
	syscall
%endmacro

%endif ; CALLS_ASM
