! KallistiOS 2.1.0
!
! startup.s
! (c)2000-2001 Dan Potter
!
! This program is free software; you can redistribute it and/or modify
! it under the terms of the KOS License.
!
! This program is distributed in the hope that it will be useful,
! but WITHOUT ANY WARRANTY; without even the implied warranty of
! MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
! KOS License for more details (DreamHAL note: it's in the LICENSE file).
!
! You should have received a copy of the KOS License along with this
! program; if not, please visit Cryptic Allusion Game Dev at:
!
!   http://gamedev.allusion.net/
!
! This file must appear FIRST in your linking order, or your program won't
! work correctly as a raw binary.
!
! This is very loosely based on Marcus' crt0.s/startup.s
!
! Note 1/3/2020:
! This file has been modified in order to work in a freestanding environment.
! --Moopthehedgehog
!

.globl start
.globl _start
.globl _arch_real_exit
.globl __arch_old_sr
.globl __arch_old_vbr
.globl __arch_old_stack
.globl __arch_old_fpscr

.extern dcload_type_check

_start:
start:
	! Disable interrupts (if they're enabled)
	mov.l	old_sr_addr,r0
	stc	sr,r1
	mov.l	r1,@r0
	mov.l	init_sr,r0
	ldc	r0,sr

	! Run in the P2 area
	mov.l	setup_cache_addr,r0
	mov.l	p2_mask,r1
	or	r1,r0
	jmp	@r0
	 nop

setup_cache:
	! Now that we are in P2, it's safe to enable the cache
	! Check to see if we should enable OCRAM.
	mov.l	enable_ocram, r0
	mov.l @r0,r0 ! read use_ocram C variable
	tst	#1, r0
	bf	.L_setup_cache_L0
	mov.w	ccr_data,r1
	bra	.L_setup_cache_L1
	 nop
.L_setup_cache_L0:
	mov.w	ccr_data_ocram,r1
.L_setup_cache_L1:
	mov.l	ccr_addr,r0
	mov.l	r1,@r0

	! After changing CCR, eight instructions must be executed before
	! it's safe to enter a cached area such as P0
	nop			! 1
	nop			! 2
	nop			! 3
	nop			! 4
	nop			! 5 (d-cache now safe)
	nop			! 6
	mov.l	init_addr,r0	! 7
	mov	#0,r1		! 8
	jmp	@r0		! go
	 mov	r1,r0

init:
	! Save old PR on old stack so we can get to it later
	sts.l	pr,@-r15

	! Save the current stack, and set a new stack (higher up in RAM)
	mov.l	old_stack_addr,r0
	mov.l	r15,@r0
	mov.l	new_stack,r15

	! Save VBR
	mov.l	old_vbr_addr,r0
	stc	vbr,r1
	mov.l	r1,@r0

	! Save FPSCR
	mov.l	old_fpscr_addr,r0
	sts	fpscr,r1
	mov.l	r1,@r0

	! Reset FPSCR
	mov	#4,r4		! Use 00040000 (DN=1)
	mov.l	fpscr_addr,r0
	jsr	@r0
	shll16	r4

	! zero out the bss area
	mov.l bss_start,r0
	mov.l bss_end,r1
	cmp/hi r0,r1 ! if r1 > r0, 1 -> T
	bf dcload_check ! branch if no bss section
	mov #0,r2
zero_out_bss:
	mov.l r2,@r0
	cmp/hi r0,r1
	bt/s zero_out_bss
	 add #4,r0

dcload_check:
	! check if dcload is present
	mov.l	dcload_magic_addr,r0
	mov.l	@r0,r0
	mov.l	dcload_magic_value,r1
	mov.l set_dcload_present,r3
	cmp/eq r0,r1
	! The T bit will be 1 if we have console, else 0
	bt/s enable_exceptions
	 movt r2
	mov.l	dcload_noconsole_magic_value,r1
	cmp/eq r0,r1
	! If T is set, we have dcload but no console
	bt/s enable_exceptions
		add #1,r2
	! T bit was never set, so no dcload present and therefore no exception handler
	mov #0,r2
	bf/s go_main
enable_exceptions:
	 mov.l r2,@r3 ! set dcload_present C variable, 0 = none, 1 = present with console, 2 = present without console
	mov.l	init_sr_exceptions,r1
	ldc	r1,sr

! last thing: check if dcload is dcload-serial or dcload-ip
	mov.l	dcload_type_check_addr,r0
	jsr	@r0
 	 nop

go_main:
	! Setup a sentinel value for frame pointer in case we're using
	! FRAME_POINTERS for stack tracing.
	mov	#-1,r14

	! Jump to the kernel main
	mov.l	main_addr,r0
	jsr	@r0
	 nop

	! Program can return here (not likely) or jump here directly
	! from anywhere in it to go straight back to the monitor
_arch_real_exit:
	! Reset SR
	mov.l	old_sr,r0
	ldc	r0,sr

	! Disable MMU, invalidate TLB
	mov	#4,r0
	mov.l	mmu_addr,r1
	mov.l	r0,@r1

	! Wait (just in case)
	nop				! 1
	nop				! 2
	nop				! 3
	nop				! 4
	nop				! 5
	nop				! 6
	nop				! 7
	nop				! 8

	! Restore VBR
	mov.l	old_vbr,r0
	ldc	r0,vbr

	! If we're working under dcload, call its EXIT syscall
	mov.l	dcload_magic_addr,r0
	mov.l	@r0,r0
	mov.l	dcload_magic_value,r1
	cmp/eq	r0,r1
	bf	normal_exit

	mov.l	dcload_syscall,r0
	mov.l	@r0,r0
	jsr	@r0
	 mov #15,r4 ! dcload's exit syscall is syscall 15

	! Set back the stack and return (presumably to a serial debug)
normal_exit:
	mov.l	old_stack,r15
	lds.l	@r15+,pr
	rts
	 nop

! Misc variables
	.align	4
dcload_magic_addr:
	.long	0x8c004004
dcload_magic_value:
	.long	0xdeadbeef
dcload_noconsole_magic_value:
	.long	0xfeedface
dcload_syscall:
	.long	0x8c004008
set_dcload_present:
	.long _STARTUP_dcload_present
__arch_old_sr:
old_sr:
	.long	0
__arch_old_vbr:
old_vbr:
	.long	0
__arch_old_fpscr:
old_fpscr:
	.long	0
init_sr:
	.long	0x500000f0
init_sr_exceptions:
	.long 0x400000f0
old_sr_addr:
	.long	old_sr
old_vbr_addr:
	.long	old_vbr
old_fpscr_addr:
	.long	old_fpscr
old_stack_addr:
	.long	old_stack
__arch_old_stack:
old_stack:
	.long	0
new_stack:
	.long	0x8d000000
p2_mask:
	.long	0xa0000000
setup_cache_addr:
	.long	setup_cache
init_addr:
	.long	init
dcload_type_check_addr:
! This routine is in dcload_startup_support.S
	.long	dcload_type_check
main_addr:
	.long	_dreamcast_main
mmu_addr:
	.long	0xff000010
fpscr_addr:
	.long	___call_builtin_sh_set_fpscr ! need this since builtins can't be called from asm directly
enable_ocram:
	.long	_STARTUP_use_ocram
ccr_addr:
	.long	0xff00001c
! CCR: Make P1 write-through and P0/P3/U0 write-back/copy-back. This way, since
! the linkerscript maps RAM to P1, by default things get stored into WT memory
! to minimize stale caches going awry. Manually specifying an address in P0
! activates copy-back enhancements as-needed.
ccr_data:
	.word	0x0909
ccr_data_ocram:
	.word	0x0929
bss_start:
	.long __bss_start
bss_end:
	.long _end
