%ifndef STRUCS_ASM
%define STRUCS_ASM

struc stat
	st_dev:		resb 8 ; offset: 0
	st_ino:		resb 8 ; offset: 8
	st_nlink:	resb 8 ; offset: 16
	st_mode:	resb 4 ; offset: 24
	st_uid:		resb 4 ; offset: 28
	st_gid:		resb 4 ; offset: 32
	___st_pad0:	resb 4
	st_rdev:	resb 8 ; offset: 40
	st_size:	resb 8 ; offset: 48
	st_blksize:	resb 8 ; offset: 56
	st_blocks:	resb 8 ; offset: 64
	st_atime:	resb 8 ; offset: 72
	st_mtime:	resb 8 ; offset: 88
	st_ctime:	resb 8 ; offset: 104
	STRUC_STAT_SIZE:resb 0
endstruc

%endif ; STRUCS_ASM

