// ---- cachefuncs.h - Cache Module ----
//
// Version 1.0.1
//
// This file is part of the DreamHAL project, a hardware abstraction library
// primarily intended for use on the SH7091 found in hardware such as the SEGA
// Dreamcast game console.
//
// This module provides cache-related functions needed to manage the CPU caches.
// Cache management is super important when aiming to reach maximum performance!
// It is hereby released into the public domain in the hope that it may prove useful.
//
// --Moopthehedgehog, March 2020

#ifndef __CACHEFUNCS_H
#define __CACHEFUNCS_H

// Notes:
// - GCC provides __builtin_prefetch(const void * addr), which is the
//	recommended way to use the prefetch instruction. GCC will actually know about
//	it via that macro, whereas this asm code GCC knows nothing about.
// - All functions will align the input address to the next lowest 32-byte aligned
//	address for internal use (except CACHE_movcal()), as this is the cache block
//	base address
// - The return value for all functions, except CACHE_movcal(), is this 32-byte
//	aligned cache block base address
// - count_32Bytes is the number of contiguous 32-byte blocks to operate on
//	(e.g. (numbytes + 31) / 32)

// The following functions are available.
// Please see their definitions for any further important usage information.
/*
	//------------------------------------------------------------------------------
	// PREF - Data Prefetch
	//------------------------------------------------------------------------------

	// Prefetch data into the operand cache
	void * CACHE_Prefetch(const void * address)

	//------------------------------------------------------------------------------
	// MOVCA.L - On operand cache miss, allocate cache and write without data read
	//------------------------------------------------------------------------------

	// Standard "mov.l-like" version
	void * CACHE_movcal(void * dest, unsigned int data)

	// "Base of cache block" version
	void * CACHE_Block_movcal(void * dest, unsigned int data)

	//------------------------------------------------------------------------------
	// OCBP - Operand cache write back and invalidate (purge)
	//------------------------------------------------------------------------------

	// Write back operand cache blocks and invalidate them
	void * CACHE_Block_Purge(const void * address, unsigned int count_32Bytes)

	//------------------------------------------------------------------------------
	// OCBI - Operand cache invalidate
	//------------------------------------------------------------------------------

	// Invalidate operand cache blocks, but don't write the blocks back
	void * CACHE_Block_Invalidate(const void * address, unsigned int count_32Bytes)

	//------------------------------------------------------------------------------
	// OCBWB - Operand cache writeback
	//------------------------------------------------------------------------------

	// Write operand cache blocks back, but don't invalidate them
	void * CACHE_Block_Writeback(const void * address, unsigned int count_32Bytes)
*/

//------------------------------------------------------------------------------
// PREF - Data Prefetch
//------------------------------------------------------------------------------
//
// Prefetch data into the operand cache
//
// The Opcache is 16kB (8kB if using OCRAM), so this really only makes sense when
// needed data is known to be further than 16kB away.
//

static inline __attribute__((always_inline)) void * CACHE_Prefetch(const void * address)
{
	// Align 'address' to 32-bytes for return: the pref instruction actually ignores
	// the lower 5 bits internally anyways per the SH7750 Software Manual.
	unsigned int __32_byte_ptr = (unsigned int)address & 0xffffffe0;

	asm volatile ("pref @%[ptr]\n"
		: // outputs
		: [ptr] "r" (__32_byte_ptr) // inputs
		: // clobbers
	);

	return (void*)__32_byte_ptr;
}

//------------------------------------------------------------------------------
// MOVCA.L - On operand cache miss, allocate cache and write without data read
//------------------------------------------------------------------------------
//
// Allocate an operand cache block on cache miss and write to it without reading
// that block into cache
//

//
// On a cache miss, movca.l will allocate a cache block and will *not* read the
// missed data, so the entire 32-byte block will get clobbered. For write-only
// buffers, that's not a problem and it eliminates the cache read, but any data
// in that 32-byte cache block not explicitly written to will be undefined. On a
// cache hit, it behaves like a normal mov.l.
//
// NOTES:
// - Destination MUST be 4-byte aligned
// - movca.l can only write 4 bytes at a time
// - If movca.l is used at the base of a 32-byte block (e.g. by using
//	CACHE_Block_movcal()), 7x mov.l can be used afterwards to iterate through the
//	same block in contiguous fashion. It is not necessary to use 7x movca.l (good
//	thing, too, since movca.l can only write data that's in R0!)
//

// Standard "mov.l-like" version
//
// Unlike the other operand cache functions in this file, this does not align to
// the base of the input address's cache block. As a result, the return value is
// just the destination passed in.
static inline __attribute__((always_inline)) void * CACHE_movcal(void * dest, unsigned int data)
{
  asm volatile ("movca.l %[data], @%[out]\n"
    : // outputs
    : [data] "z" (data), [out] "r" ((unsigned int)dest) // inputs, movca.l requires data to be in R0
    : "memory" // clobbers
  );

  return dest;
}

// "Base of cache block" version
static inline __attribute__((always_inline)) void * CACHE_Block_movcal(void * dest, unsigned int data)
{
	// Align 'dest' to 32-bytes
	unsigned int __32_byte_ptr = (unsigned int)dest & 0xffffffe0;

	asm volatile ("movca.l %[data], @%[out]\n"
    : // outputs
    : [data] "z" (data), [out] "r" (__32_byte_ptr) // inputs, movca.l requires data to be in R0
    : "memory" // clobbers
  );

  return (void*)__32_byte_ptr;
}

//------------------------------------------------------------------------------
// OCBP - Operand cache write back and invalidate (purge)
//------------------------------------------------------------------------------
//
// Write back operand cache blocks and invalidate them
//

static inline __attribute__((always_inline)) void * CACHE_Block_Purge(const void * address, unsigned int count_32Bytes)
{
	// Align 'address' to 32-bytes
	unsigned int __32_byte_ptr = (unsigned int)address & 0xffffffe0;
	unsigned int ret_base = __32_byte_ptr;

	while(count_32Bytes)
	{
		asm volatile ("ocbp @%[ptr]\n"
			: // outputs
			: [ptr] "r" (__32_byte_ptr) // inputs
			: "memory" // clobbers memory
		);

		count_32Bytes -= 1;
		__32_byte_ptr += 32;
	}

	return (void*)ret_base;
}

//------------------------------------------------------------------------------
// OCBI - Operand cache invalidate
//------------------------------------------------------------------------------
//
// Invalidate operand cache blocks, but don't write the blocks back
//

static inline __attribute__((always_inline)) void * CACHE_Block_Invalidate(const void * address, unsigned int count_32Bytes)
{
	// Align 'address' to 32-bytes
	unsigned int __32_byte_ptr = (unsigned int)address & 0xffffffe0;
	unsigned int ret_base = __32_byte_ptr;

	while(count_32Bytes)
	{
		asm volatile ("ocbi @%[ptr]\n"
			: // outputs
			: [ptr] "r" (__32_byte_ptr) // inputs
			: // clobbers
		);

		count_32Bytes -= 1;
		__32_byte_ptr += 32;
	}

	return (void*)ret_base;
}

//------------------------------------------------------------------------------
// OCBWB - Operand cache writeback
//------------------------------------------------------------------------------
//
// Write operand cache blocks back, but don't invalidate them
//

static inline __attribute__((always_inline)) void * CACHE_Block_Writeback(const void * address, unsigned int count_32Bytes)
{
	// Align 'address' to 32-bytes
	unsigned int __32_byte_ptr = (unsigned int)address & 0xffffffe0;
	unsigned int ret_base = __32_byte_ptr;

	while(count_32Bytes)
	{
		asm volatile ("ocbwb @%[ptr]\n"
			: // outputs
			: [ptr] "r" (__32_byte_ptr) // inputs
			: "memory" // clobbers memory
		);

		count_32Bytes -= 1;
		__32_byte_ptr += 32;
	}

	return (void*)ret_base;
}

#endif /* __CACHEFUNCS_H */
