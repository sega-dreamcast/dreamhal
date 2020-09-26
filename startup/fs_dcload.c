// ---- fs_dcload.c - Dreamcast dcload Interface Module ----
//
// Version 1.0.0
//
// This module provides dcload-interfacing functionality. This file is hereby
// released into the public domain in the hope that it may prove useful.
//
// --Moopthehedgehog, April 2020
//

#include "fs_dcload.h"

// Memory area to hold PR so that jsr'ing to the syscall doesn't clobber it.
// This needs to be in .data or .bss, which is why it's a [static] global.
// Making it a single-member array also allows for convenient argument passing
// into the inline asm. There is another benefit of avoiding GCC's emitting an
// undesired stack push and/or extraneous register modification, which it does
// when not using an array for some strange reason.
static unsigned int DCLOAD_temp_pr[1] = {0};

// Switching between the variadic arg and non-variadic arg version requires
// modifying fs_dcload.h, too. Only use one or the other!

//------------------------------------------------------------------------------
// Variadic arg version
//------------------------------------------------------------------------------

/*
// Thankfully stdarg.h is available in freestanding environments!
#include <stdarg.h>

int dcloadsyscall(unsigned int syscall, ...)
{
  // -mrenesas and -mhitachi both pass varargs on the stack.
  // GNU ABI uses r4-r7 for the first 4 args like dcload expects.
  // So we gotta ensure the correct calling convention is used when calling a dcload syscall.

  // Package up the args so that no matter what the ABI is, dcload will still work right.
  int output;
  // Unfortunately r5-r7 aren't so easy...
  // But at least dcload syscalls are guaranteed to be no more than 3 args max,
  // and all args are 4 bytes in size (either ints/uints or pointers).
  // This definitely makes things easier than they could have been. :D
  va_list ap;
  va_start(ap, syscall);

  // Even if only some of these are used and the rest are garbage, dcload will
  // only use the args belonging to the desired syscall and just drop the garbage.
  register unsigned int syscall_number __asm__("r4") = syscall;
  register unsigned int syscall_arg1 __asm__("r5") = va_arg(ap, unsigned int);
  register unsigned int syscall_arg2 __asm__("r6") = va_arg(ap, unsigned int);
  register unsigned int syscall_arg3 __asm__("r7") = va_arg(ap, unsigned int);
  // NOTE: va_arg moves to the next arg based on the type of the prior arg, so need a 4-byte type like int or uint here for each arg.

  asm volatile (
    // Save PR
    "sts pr, %[out]\n\t" // Get PR
    "mov.l %[out], @%[temp_pr_address]\n\t" // Save PR so that jsr doesn't clobber it

    // dcload function call
    "mov.l DCLOAD_syscall_address, %[out]\n\t" // Get dcload syscall storage address (0x8c004008)
    "mov.l @%[out], %[out]\n\t" // Get the dcload syscall address
    "jsr @%[out]\n\t" // Call the syscall
    " nop\n\t"

    // Restore PR (part 1/2)
    "mov.l DCLOAD_temp_pr_addr, %[temp_pr_address]\n\t" // Get address of PR storage unit
    "bra 0f\n\t" // Jump over the embedded data
    " mov.l @%[temp_pr_address], %[temp_pr_address]\n" // Get prior PR value

    // Read-only data
  ".align 2\n" // NOTE: GCC will try to reuse the same register for the pointer to the global, not realizing that a function call just happened.
  "DCLOAD_temp_pr_addr:\n\t" // Address of C variable
    ".long _DCLOAD_temp_pr\n" // This is the address of the above static global C variable
  "DCLOAD_syscall_address:\n\t" // dcload hardcodes the syscall function's address here
    ".long 0x8c004008\n"

    // Restore PR (part 2/2)
  "0:\n\t"
    "lds %[temp_pr_address], pr\n" // Finally, load prior PR value back into PR so that nothing is amiss
    : [out] "=&z" (output)  // outputs
    : [temp_pr_address] "r" ((unsigned int)DCLOAD_temp_pr),
    "r" (syscall_number), "r" (syscall_arg1), "r" (syscall_arg2), "r" (syscall_arg3) // inputs
    : // clobbers
   );

  va_end(ap);

  return output;
}
*/

//------------------------------------------------------------------------------
// Non-variadic arg version
//------------------------------------------------------------------------------

int dcloadsyscall_wrapper(unsigned int syscall, unsigned int arg1, unsigned int arg2, unsigned int arg3)
{
  // -mrenesas and -mhitachi both pass varargs on the stack.
  // GNU ABI uses r4-r7 for the first 4 args like dcload expects.
  // So we gotta ensure the correct calling convention is used when calling a dcload syscall.

  // Package up the args so that no matter what the ABI is, dcload will still work right.
  int output;
  register unsigned int syscall_number __asm__("r4") = syscall;
  register unsigned int syscall_arg1 __asm__("r5") = arg1;
  register unsigned int syscall_arg2 __asm__("r6") = arg2;
  register unsigned int syscall_arg3 __asm__("r7") = arg3;

  asm volatile (
    // Save PR
    "sts pr, %[out]\n\t" // Get PR
    "mov.l %[out], @%[temp_pr_address]\n\t" // Save PR so that jsr doesn't clobber it

    // dcload function call
    "mov.l DCLOAD_syscall_address, %[out]\n\t" // Get dcload syscall storage address (0x8c004008)
    "mov.l @%[out], %[out]\n\t" // Get the dcload syscall address
    "jsr @%[out]\n\t" // Call the syscall
    " nop\n\t"

    // Restore PR (part 1/2)
    "mov.l DCLOAD_temp_pr_addr, %[temp_pr_address]\n\t" // Get address of PR storage unit
    "bra 0f\n\t" // Jump over the embedded data
    " mov.l @%[temp_pr_address], %[temp_pr_address]\n" // Get prior PR value

    // Read-only data
  ".align 2\n" // NOTE: GCC will try to reuse the same register for the pointer to the global, not realizing that a function call just happened.
  "DCLOAD_temp_pr_addr:\n\t" // Address of C variable
    ".long _DCLOAD_temp_pr\n" // This is the address of the above static global C variable
  "DCLOAD_syscall_address:\n\t" // dcload hardcodes the syscall function's address here
    ".long 0x8c004008\n"

    // Restore PR (part 2/2)
  "0:\n\t"
    "lds %[temp_pr_address], pr\n" // Finally, load prior PR value back into PR so that nothing is amiss
    : [out] "=&z" (output)  // outputs
    : [temp_pr_address] "r" ((unsigned int)DCLOAD_temp_pr),
    "r" (syscall_number), "r" (syscall_arg1), "r" (syscall_arg2), "r" (syscall_arg3) // inputs
    : // clobbers
   );

  return output;
}
