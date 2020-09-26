// ---- print.h - Print Module Header ----
//
// Version 1.0.0
//
// This file is part of the DreamHAL project, a hardware abstraction library
// primarily intended for use on the SH7091 found in hardware such as the SEGA
// Dreamcast game console.
//
// This module provides printf and sprintf (and related) functions. It is licensed
// according to the 3-Clause BSD license below.
//
// This module requires the dcload module, but only for printf and vprintf since
// they write via a dcload syscall.
//
// --Moopthehedgehog, March 2020
//
/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1986, 1988, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)subr_prf.c	8.3 (Berkeley) 1/21/94
 */

#ifndef __PRINT_H_
#define __PRINT_H_

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

//------------------------------------------------------------------------------
// printf()-style functions
//------------------------------------------------------------------------------
//
// LIMITATIONS:
// - No 64-bit data types (e.g. can't print a 64-bit int).
// - Only bases of 2 (binary), 10 (decimal), 16 (hexadecimal) and 8 (octal) are supported.
//  You'll get a ? if you try to use anything else. (Full-fat printfs can support bases from 2 to 36)
// - This printf() and friends can't do floats. Please use float_to_string() to convert floats into
//  strings.

// dcload-ip is the limiting factor, with a a max size of 1460 (max packet
// payload after dcload header). Use 1024 because it's an easy, round 1kB.
#define PRINT_BUFFER_SIZE 1024

// We don't have malloc, so here's an array to use instead.
// This gets used by printf() and vprintf().
extern char print_buffer[PRINT_BUFFER_SIZE];

int printf(const char *fmt, ...);
int sprintf(char *buf, const char *cfmt, ...);

int vprintf(const char *fmt, va_list ap);
int vsprintf(char *buf, const char *cfmt, va_list ap);

int snprintf(char *str, size_t size, const char *format, ...);
int vsnprintf(char *str, size_t size, const char *format, va_list ap);

int vsnrprintf(char *str, size_t size, int radix, const char *format, va_list ap);

int kvprintf(char const *fmt, void (*func)(int, void*), void *arg, int radix, va_list ap);

#endif
