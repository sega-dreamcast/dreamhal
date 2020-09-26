// ---- simple_print.c - Simple Print Module ----
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

#include "simple_print.h"
#include "memfuncs.h"

// Enable the rounding mode of float_to_string(). On by default because it helps
// to hide its inaccuracy a little bit.
#define FLOAT_ROUNDING

const char * dec_hex_data = "0123456789abcdef";

// This does the same thing as strlen, but is made specially for this file so
// that it is standalone.
static int stringlength(char* string)
{
  int length = 0;

  while(*string)
  {
    length++;
    string++;
  }

  return length;
}

// Convert hexadecimal data to string
// 'out_string' buffer is assumed to be large enough.
// Requires an 11-byte output buffer for the string (9 bytes for data + null, 2 bytes for 0x)
// Returns pointer to out_string.
char * hex_to_string(unsigned int in_number, char* out_string)
{
	int i;

	out_string[0] = '0';
	out_string[1] = 'x';

  out_string[10] = '\0'; // Null term

	for(i = 9; i >= 2; i--)
	{
		out_string[i] = dec_hex_data[in_number & 0x0f];
		in_number >>= 4;
	}

  return out_string;
}

// Convert unsigned int to string
// 'out_string' buffer is assumed to be large enough.
// Requires an 11-byte output buffer for the string.
// The biggest decimal number is 4294967295, which is 10 characters‬ (excluding null term).
// Returns pointer to out_string.
char * uint_to_string(unsigned int in_number, char* out_string)
{
	int i;

  out_string[10] = '\0'; // Null term

	for(i = 9; i >= 0; i--)
	{
		if((!in_number) && (i < 9))
		{
      memmove(out_string, &(out_string[i + 1]), 11-i); // realign string with beginning
      return out_string;
		}
		else
		{
			out_string[i] = dec_hex_data[in_number % 10];
		}
		in_number /= 10;
	}

  return out_string;
}

// Convert signed int to string
// 'out_string' buffer is assumed to be large enough.
// Requires a 12-byte output buffer for the string.
// The longest signed decimal numbers are 10 characters‬ (excluding null term and sign).
// Returns pointer to out_string.
char * int_to_string(int in_number, char* out_string)
{
	int i;
	int need_neg = 0;
  int neg = 0;

  out_string[11] = '\0'; // Null term

	if(in_number < 0)
	{
		need_neg = 1;
    neg = 1;
	}

	// 10 digits for the number, plus one char for the potential (-) sign
	for(i = 10; i >= 0; i--)
	{
		if(need_neg && (!in_number) && (i < 10))
		{
			out_string[i] = '-';
			need_neg = 0;
		}
		else if((!in_number) && (i < 10))
		{
      memmove(out_string, &(out_string[i + 1]), 12-i); // realign string with beginning
			return out_string;
		}
		else
		{
      if(neg)
      {
        out_string[i] = dec_hex_data[-(in_number % 10)]; // mod of negative number is still negative
      }
      else
      {
        out_string[i] = dec_hex_data[in_number % 10];
      }
		}
		in_number /= 10;
	}

  return out_string;
}

// WARNING: This is not super accurate. The printed value may be too small by up to 0.002.
//
// This function converts a single-precision mantissa to string, specifically
// for the case of SH4, which cannot do 64-bit integer division easily (would
// take over 128 instructions)... and really struggles with 32-bit integer
// division (would take over 64 instructions). This is despite the fact that it
// can do both single- and double-precision floating-point division, however.
// But IMO using the FPU to do this defeats the purpose, as that would dump
// the floating point registers to memory and reload them every time. That would
// be worse for performance than this insanity already is.
//
// Thankfully, GCC is smart enough to be able to do 32-bit division by a constant
// without dividing (by making good use of multiplies and shifts), which is great
// otherwise I'd have to write things like x/100000 as (x*2814749768) >> 48.
//
// Some resources that I think would be helpful in understanding what I did here:
// - Really great paper on everything there is to know about floats:
//  http://www.validlab.com/goldberg/paper.pdf
// - Here's an answer on stack overflow that I think does a good job demonstrating
//  the idea of manipulating a float using its constituent parts:
//  https://stackoverflow.com/questions/20302904/converting-int-to-float-or-float-to-int-using-bitwise-operations-software-float
// - An online calculator that converts IEEE-754 to hex, binary, and decimal (and back):
//  https://www.h-schmidt.net/FloatConverter/IEEE754.html
// - It also helps to know how to handle binary and hexadecimal fractions.
//
// Even though this isn't perfect, at least I give you base-10 digits and didn't
// just leave it at 1.hexdigits x2^power, although, wouldn't that have been fun? :P
// (It technically would be more accurate than decimals to print the raw mantissa as hex...)
// Returns pointer to out_string.
static char * mantissa_to_string(unsigned int mantissa, unsigned int decimal_point_digits, unsigned char * exponent_add, char* out_string)
{
	int j;
	int shifter = 22;
	// A 64-bit int can't hold 10^23. Only up to 1.84x10^19... so 10^18 is our cutoff.
	// This means the printed value will be off by up to 31/8388608...

  // ...Well, at least it would be if I had been able to figure out a way to do 64-bit
  // integer division on SH4. It's definitely doable with double-precision divides,
  // but those divides are like 30 cycles each and thus *very* slow. It would be possible
  // to do this whole thing with floats, actually, but the issue there is that, in order to
  // print one float, this function would need to dump many floats to memory and then
  // reload them all (same with doubles). That could cause some major issues when paired
  // with sh4_math.h. NOTE: GCC CAN do 64-bit integer divides, but it calls special math
  // functions to do it, which by themselves take up a solid few kB. I'm trying to avoid
  // overhead and dependencies like that. And, actually, it seems to do the same for 32-bit
  // division, probably to avoid using div1 over and over again in a loop.

  // A 32-bit int is all we get then, and 32-bit only holds up to 4.29x10^9... so 10^8 is
  // our actual cutoff. The printed value may be too small by up to 0.002.

  unsigned int temp_output = 0;
  unsigned int fivecount = 5;
  int int_cutoff = 8;
  int digit_bound = decimal_point_digits - 1;

  out_string[decimal_point_digits] = '\0'; // Null term

  if(!decimal_point_digits)
  {
    out_string[0] = '\0';
    return out_string;
  }

  if(!mantissa)
  {
    for(j = digit_bound; j >= 0; j--)
    {
      out_string[j] = '0'; // all zeroes
    }

    return out_string;
  }

	while(shifter >= (22 - int_cutoff)) // Need to stop at 10^18 (4, i.e. 19 iterations) for 64-bit, 10^8 (14, i.e. 9 iterations) for 32-bit...
	{
		if((mantissa >> shifter) & 0x1)
		{
			// first bit is 0.5 (1/2), second is 0.25 (1/4), third is 0.125 (1/8)...
			// last is 0.11920928955078125 × 10^-6 (1/2^23 = 1/8388608)
			// So start with 5, multiply it by 10, add 25, mult by 10, add 125... to get the decimal points
			temp_output += fivecount;
		}

    if(shifter == (22 - int_cutoff))
    {
      // prevent overflow
      break;
    }
    fivecount *= 5;
    temp_output *= 10;
		shifter--;
	}

  // Ensure exponent_add is in a known state
  exponent_add[0] = 0;

  // Gotta hardcode all these divides so that GCC can figure out a way to avoid division.
  // Saves me having to write out multiply-and-shift magic numbers.

  // I *know* there are gonna be people who will bug me about it if there's no rounding.
  // ...I'm probably also gonna get people who bug me that 3 digits isn't enough.
#ifndef FLOAT_ROUNDING

  // Leave just the decimals we want
  if(decimal_point_digits == 1)
  {
    temp_output /= 100000000U; // 10^8
  }
  else if(decimal_point_digits == 2)
  {
    temp_output /= 10000000U; // 10^7
  }
  else if(decimal_point_digits == 3)
  {
    temp_output /= 1000000U; // 10^6
  }

#else

  // Need to divide temp_output by the power of ten that will give rightmost desired digit
  // But if we stop one short we can do rounding.
  if(decimal_point_digits == 1)
  {
    temp_output /= 10000000U; // 10^7
  }
  else if(decimal_point_digits == 2)
  {
    temp_output /= 1000000U; // 10^6
  }
  else if(decimal_point_digits == 3)
  {
    temp_output /= 100000U; // 10^5
  }

  // Round away from zero, like in normal math (at least, that's what my TI-84 does)
  unsigned int round_check = temp_output % 10U;
  if(round_check >= 5U)
  {
    temp_output += 10U - round_check;

    // Quick, silly integer power of 10
    unsigned int tenpow = 10U;
    for(unsigned int sillypow = 0; sillypow < decimal_point_digits; sillypow++)
    {
      tenpow *= 10U;
    } // It's 100 for 1 digit, 1000 for 2 digits, 10000 for 3 digits

    if(temp_output == tenpow)
    {
      exponent_add[0] = 1;

      for(j = digit_bound; j >= 0; j--)
      {
        out_string[j] = '0'; // all zeroes
      }

      return out_string;
    }
  }

  // Alright, time to get rid of that rounding term
  temp_output /= 10U;
#endif

  // Finally, convert mantissa to string, and I never want to have to work with
  // floats in their raw bit format ever again.
	for(j = digit_bound; j >= 0; j--)
	{
		if((!temp_output) && (j < digit_bound))
		{
      out_string[j] = '0'; // leading zeroes
		}
		else
		{
			out_string[j] = dec_hex_data[temp_output % 10U];
		}
		temp_output /= 10U;
	}

  return out_string;
}

// Return 1 if NaN, 0 if not -- just like __builtin_isnan is supposed to do.
// But apparently __builtin_isnan() doesn't use fcmp/eq, which can check for NaNs,
// and instead uses software FP emulation...
static int compare_isnan(float input)
{
  int output;

  asm volatile (
    "fcmp/eq %[in_f], %[in_f]\n\t" // If it's != to itself, it's a NaN
    "mov #-1, %[out]\n\t" // Don't have movrt, so this is the next best thing.
    "negc %[out], %[out]\n" // This is given as an example in the negc instruction description, too.
  : [out] "=r" (output) // outputs
  : [in_f] "f" (input) // inputs
  : "t" // clobbers
  );

  return output;
}

typedef struct __attribute__((packed)) {
	unsigned int mantissa : 23;
	unsigned int exponent : 8;
	unsigned int sign : 1;
} FLOAT_FORMAT_STRUCT;

static char temp_buffer_exp[12] = {0};
static char temp_buffer_mant[4 + 7] = {0}; // Needs to hold "1." and "x2^" in addition to sign + null

// Convert float to string (will be in 1.23456x2^789 format instead of the more familiar 1.23456x10^789)
// 'out_string' buffer is assumed to be large enough.
// decimal_point_digits is # of digits displayed after the decimal (min 1, max 3)
// output buffer size is 11 + decimal_point_digits (meaning min buffer is 12, max 14)
// Single-precision only
// Returns pointer to out_string.
char * float_to_string(float in_float, unsigned int decimal_point_digits, char* out_string)
{
	int denormal = 0;
	FLOAT_FORMAT_STRUCT float_struct = *(FLOAT_FORMAT_STRUCT*)&in_float;
  unsigned char exponent_add_1 = 0;

  // For some reason GCC's builtins use software FP emulation. Very strange,
  // considering the SH4 has hardware support for IEEE-754.

//	if(in_float != in_float) // Check for NaN
  if(compare_isnan(in_float))
	{
		out_string[0] = 'N';
	 	out_string[1] = 'a';
		out_string[2] = 'N';
		out_string[3] = '\0';
		return out_string;
	}
	else if(in_float == 0.0f) // Check for 0
	{
		out_string[0] = '0';
		out_string[1] = '.';
		out_string[2] = '0';
		out_string[3] = '\0';
		return out_string;
	}
	else if(float_struct.sign && (float_struct.exponent == 0xff)) // Check for -Inf
//	else if(__builtin_isinf_sign(in_float) == -1) // Check for -Inf
	{
		out_string[0] = '-';
		out_string[1] = 'I';
		out_string[2] = 'n';
		out_string[3] = 'f';
		out_string[4] = '\0';
		return out_string;
	}
	else if(float_struct.exponent == 0xff) // Check for Inf
//	else if(__builtin_isinf_sign(in_float) == 1) // Check for Inf
	{
		out_string[0] = 'I';
		out_string[1] = 'n';
		out_string[2] = 'f';
		out_string[3] = '\0';
		return out_string;
	}
	else if(float_struct.exponent == 0) // check for denormals
	{
		denormal = 1;
  }

	// get mantissa
	if(float_struct.sign)
	{
		// in_float *= -1; // We know it's negative, so we can now make it positive to make life easier
		// Actually, we don't need to do that. That operation just flips the sign bit.
		// (floats are sign-magnitude)
		mantissa_to_string(float_struct.mantissa, decimal_point_digits, &exponent_add_1, &(temp_buffer_mant[3]));

    // If rounding causes a denormal to overflow, it becomes 1.x
    if( denormal && (!(exponent_add_1)) )
		{
			temp_buffer_mant[0] = '-';
			temp_buffer_mant[1] = '0';
			temp_buffer_mant[2] = '.';
		}
		else
		{
			temp_buffer_mant[0] = '-';
			temp_buffer_mant[1] = '1';
			temp_buffer_mant[2] = '.';
		}
	}
	else
	{
		mantissa_to_string(float_struct.mantissa, decimal_point_digits, &exponent_add_1, &(temp_buffer_mant[2]));

    // If rounding causes a denormal to overflow, it becomes 1.x
		if( denormal && (!(exponent_add_1)) )
		{
      temp_buffer_mant[0] = '0';
			temp_buffer_mant[1] = '.';
		}
		else
		{
			temp_buffer_mant[0] = '1';
			temp_buffer_mant[1] = '.';
		}
	}

  // append power-of-2 notation to coefficient
	unsigned int mantlen = stringlength(temp_buffer_mant);
	temp_buffer_mant[mantlen++] = 'x';
	temp_buffer_mant[mantlen++] = '2';
	temp_buffer_mant[mantlen++] = '^';
	temp_buffer_mant[mantlen] = '\0';

  // denormals use the minimum possible exponent + 1
  // if rounding make the answer bigger we need to account for that, too
  if(denormal)
  {
    temp_buffer_exp[0] = '-';
    temp_buffer_exp[1] = '1';
    temp_buffer_exp[2] = '2';
    temp_buffer_exp[3] = '6';
    temp_buffer_exp[4] = '\0';
  }
  else
  {
    // get exponent
    // don't forget to subtract bias (underflow OK)
    int_to_string(((unsigned char)float_struct.exponent - 127 + exponent_add_1), temp_buffer_exp);
  }

  // sign is 1, mantissa max is 3 digits, exponent is 4 (sign + 3), "1." + "x2^" + null term is 6
	return append_string(temp_buffer_mant, temp_buffer_exp, out_string);
}

// Append string 2 to string 1 (both must be null-terminated) and put the combined output into out_string
// 'out_string' buffer is assumed to be large enough.
// Returns pointer to out_string.
char * append_string(char* string1, char* string2, char* out_string)
{
	unsigned int len1 = stringlength(string1); // Don't want null term
	unsigned int len2 = stringlength(string2) + 1;

	memmove(out_string, string1, len1);
	memmove(out_string + len1, string2, len2);

  return out_string;
}
