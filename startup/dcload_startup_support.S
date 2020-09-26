! ---- dcload_startup_support.S - Dreamcast dcload Startup Support Module ----
!
! Version 1.0.0
!
! This module provides complements startup.S and checks for the type of dcload
! present in the system. This is part of the DreamHAL Startup Support Module.
! This stuff is only useful if STARTUP_dcload_present == 1.
!
! --Moopthehedgehog, April 2020
!

	.section .text
	.global	dcload_type_check
	.global _DCLOAD_type

! Startup.S uses this function to detect what kind of dcload is loaded in the
! same style that KOS does in fs_dcload.c
!
! KOS uses assign_wrkmem to test whether dcload is ip or serial.
! Unfortunately, a return value is only defined if detection fails (= is dcload-ip)
! because in dcload_serial the syscall returns void. So as long as 0xffffffff is
! never in r0 after the syscall, this won't have a problem...
dcload_type_check:
	! Save pr so jsr doesn't wreck it
	mov.l	temp_pr_addr,r0
	sts	pr,r1
	mov.l	r1,@r0
	! Actually check type
	mov #14,r4 ! DCLOAD_ASSIGNWRKMEM
	mov.l	dcloadsyscall_addr,r3
	mov.l @r3,r3
	mov #0, r0 ! Just in case r0 is never used in the syscall and dcload-serial is running, ensure r0 != -1
	jsr	@r3
	 mov #0,r5 ! Send a null pointer to check type
		! Since we don't have malloc, we can't assign any work memory for data compression now.
		! We could use a static array, but that would always consume 64kB no matter what.
		! Regardless, if we did have a work area (like a static array), C code can assign
		! that address later by using dcloadsyscall(DCLOAD_ASSIGNWRKMEM, ptr_to_workmem)
		! with an 8-byte aligned workmem area of size 65536. This MUST be done before
		! invoking any dcload syscalls. See the bottom of fs_dcload.h for a template.

	! We're back, restore pr
	mov.l	temp_pr,r2
	cmp/eq #-1,r0 ! dcload-ip doesn't have the wrkmem syscall, so it returns -1
	lds	r2,pr
	movt r0 ! dcload-ip type is 1, dcload-serial type is 0. dcload-none type is -1 (default, because this subroutine won't get run if there's no dcload)
	mov.l dcload_type_var,r1
	rts
	 mov.l r0,@r1

.align 2
dcloadsyscall_addr:
	.long	0x8c004008
dcload_type_var:
	.long _DCLOAD_type
temp_pr_addr:
	.long temp_pr
temp_pr:
	.long 0

	.section .data
.align 2
_DCLOAD_type:
! default to -1, which is DCLOAD_TYPE_NONE
	.long 0xffffffff
