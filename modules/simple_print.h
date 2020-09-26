// ---- simple_print.h - Simple Print Module Header ----
//
// Version 1.0.0
//
// This file is part of the DreamHAL project, a hardware abstraction library
// primarily intended for use on the SH7091 found in hardware such as the SEGA
// Dreamcast game console.
//
// This module provides some simple string conversion functions, as opposed to
// the behemoth that is printf() (and friends). It is hereby released into the
// public domain in the hope that it may prove useful.
//
// --Moopthehedgehog, March 2020

#ifndef __SIMPLE_PRINT_H_
#define __SIMPLE_PRINT_H_

//------------------------------------------------------------------------------
// Simple string-making functions
//------------------------------------------------------------------------------

// Convert hexadecimal data to string
// 'out_string' buffer is assumed to be large enough.
// Requires an 11-byte output buffer for the string (9 bytes for data + null, 2 bytes for 0x)
// Returns pointer to out_string.
char * hex_to_string(unsigned int in_number, char * out_string);

// Convert unsigned int to string
// 'out_string' buffer is assumed to be large enough.
// Requires an 11-byte output buffer for the string.
// The longest decimal number is 4294967295, which is 10 characters‬ (excluding null term).
// Returns pointer to out_string.
char * uint_to_string(unsigned int in_number, char *out_string);

// Convert signed int to string
// 'out_string' buffer is assumed to be large enough.
// Requires a 12-byte output buffer for the string.
// The longest signed decimal numbers are 10 characters‬ (excluding null term and sign).
// Returns pointer to out_string.
char * int_to_string(int in_number, char *out_string);

// Convert float to string (will be in 1.23456x2^789 format instead of the more familiar 1.23456x10^789)
// 'out_string' buffer is assumed to be large enough.
// decimal_point_digits is # of digits displayed after the decimal (min 1, max 3)
// output buffer size is 11 + decimal_point_digits (meaning min buffer is 12, max 14)
// Single-precision only
// Returns pointer to out_string.
//
// WARNING: This is not super accurate. The printed value may be too small by up to 0.002.
char * float_to_string(float in_float, unsigned int decimal_point_digits, char * out_string);

// Append string 2 to string 1 (both must be null-terminated) and put the combined output into out_string
// 'out_string' buffer is assumed to be large enough.
// Returns pointer to out_string.
char * append_string(char * string1, char * string2, char * out_string);

#endif /* __SIMPLE_PRINT_H_ */
