// ---- fs_dcload.h - Dreamcast dcload Interface Module Header ----
//
// Version 1.0.0
//
// This module provides dcload-interfacing functionality, and has been adapted
// from KOS. This module header file is KOS-licensed as below.
//
// --Moopthehedgehog, March 2020
//
/* KallistiOS 2.1.0

   kernel/arch/dreamcast/include/dc/fs_dcload.h
   Copyright (c)2002 Andrew Kieschnick

* This program is free software; you can redistribute it and/or modify
* it under the terms of the KOS License.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* KOS License for more details (DreamHAL note: it's in the LICENSE file).
*
* You should have received a copy of the KOS License along with this
* program; if not, please visit Cryptic Allusion Game Dev at:
*
*   http://gamedev.allusion.net/
*/

/** \file   dc/fs_dcload.h
    \brief  Implementation of dcload "filesystem".

    This file contains declarations related to using dcload, both in its -ip and
    -serial forms. This is only used for dcload-ip support if the internal
    network stack is not initialized at start via KOS_INIT_FLAGS().

    \author Andrew Kieschnick
    \see    dc/fs_dclsocket.h
*/

// Note 3/1/2020:
// This file has been modified in order to work in a freestanding environment.
// This stuff is only useful if STARTUP_dcload_present == 1.
// --Moopthehedgehog

#ifndef __DC_FS_DCLOAD_H
#define __DC_FS_DCLOAD_H

/* Definitions for the "dcload" file system */

/* dcload magic value */
/** \brief  The dcload magic value! */
#define DCLOADMAGICVALUE 0xdeadbeef

/** \brief  The address of the dcload magic value */
#define DCLOADMAGICADDR (unsigned int *)0x8c004004

/* Are we using dc-load-serial or dc-load-ip? */
#define DCLOAD_TYPE_NONE    -1      /**< \brief No dcload connection */
#define DCLOAD_TYPE_SER     0       /**< \brief dcload-serial connection */
#define DCLOAD_TYPE_IP      1       /**< \brief dcload-ip connection */

/** \brief  What type of dcload connection do we have? */
extern int DCLOAD_type;
// Changed this to match DreamHAL naming convention
// --Moopthehedgehog

/* Available dcload console commands */

#define DCLOAD_READ 0
#define DCLOAD_WRITE 1
#define DCLOAD_OPEN 2
#define DCLOAD_CLOSE 3
#define DCLOAD_CREAT 4
#define DCLOAD_LINK 5
#define DCLOAD_UNLINK 6
#define DCLOAD_CHDIR 7
#define DCLOAD_CHMOD 8
#define DCLOAD_LSEEK 9
#define DCLOAD_FSTAT 10
#define DCLOAD_TIME 11
#define DCLOAD_STAT 12
#define DCLOAD_UTIME 13
#define DCLOAD_ASSIGNWRKMEM 14
#define DCLOAD_EXIT 15
#define DCLOAD_OPENDIR 16
#define DCLOAD_CLOSEDIR 17
#define DCLOAD_READDIR 18
#define DCLOAD_GETHOSTINFO 19
#define DCLOAD_GDBPACKET 20
#define DCLOAD_REWINDDIR 21

/* dcload dirent */

struct dcload_dirent {
    long            d_ino;  /* inode number */
    long           d_off;  /* offset to the next dirent */
    unsigned short  d_reclen;/* length of this record */
    unsigned char   d_type;         /* type of file */
    char            d_name[256];    /* filename */
};

typedef struct dcload_dirent dcload_dirent_t;

/* dcload stat */

struct  dcload_stat {
    unsigned short st_dev;
    unsigned short st_ino;
    int st_mode;
    unsigned short st_nlink;
    unsigned short st_uid;
    unsigned short st_gid;
    unsigned short st_rdev;
    long st_size;
    long st_atime;
    long st_spare1;
    long st_mtime;
    long st_spare2;
    long st_ctime;
    long st_spare3;
    long st_blksize;
    long st_blocks;
    long st_spare4[2];
};

typedef struct dcload_stat dcload_stat_t;

//
// DreamHAL notes from Moopthehedgehog:
//
//==============================================================================
// -- SYSCALLS NOTE: --
//==============================================================================
//
// ...Unfortunately I can't add the syscalls like read/write/etc. here, otherwise
// I'd probably get attacked by GPL ninjas.
//
// To call a syscall, do dcloadsyscall(console command, args for function)
// You can see what functions are available in the example-src folder in
// the dcload-ip source. (Note: DCLOAD_ASSIGNWRKMEM is only for dcload-serial to
// use, so don't use that one except as noted in the IMPORTANT DCLOAD-SERIAL NOTE
// at the bottom of this file. All the others should be fine.)
//
// Example: here's how to print plain text to the dc-tool console:
// dcloadsyscall(DCLOAD_WRITE, 1, string, string_length); // 1 is stdout's file descriptor
//
// NOTE: The original dcloadsyscall function is not ABI-safe. So if someone used
// Renesas/Hitachi calling convention, dcload would break because the variadic
// arguments would get passed in completely differently. So here are two new ones
// that solve this problem and preserve dcload's original syntax. Only one of the
// two is usable at a time, so the unused method should be commented out here and
// in fs_dcload.c to avoid conflicts.
//

//------------------------------------------------------------------------------
// This one uses variadic arguments in fs_dcload.c:
//------------------------------------------------------------------------------

/*
int dcloadsyscall(unsigned int syscall, ...);
*/

//------------------------------------------------------------------------------
// This one does not use variadic arguments in fs_dcload.c:
//------------------------------------------------------------------------------
//
// Based on a cool trick depicted here, see the answer by user netcoder:
// https://stackoverflow.com/questions/11761703/overloading-macro-on-number-of-arguments
// Note that dcloadsyscall always takes at least one argument, and all arguments of all
// syscalls are 4 bytes each (size of an int or unsigned int on SH4).
//
// Same syntax as the normal 'int dcloadsyscall(unsigned int syscall, ...)', and the
// macros make it work. This removes the need to use variadic arguments in
// the function itself, which saves a few hundred bytes of extaneous assembly
// instructions that GCC adds when not using -mrenesas/-mhitachi. Ergo, this is faster.

// Don't call this wrapper directly, just call dcloadsyscall(syscall, ...) as a
// normal dcload syscall would. That's what the below macros take care of!
int dcloadsyscall_wrapper(unsigned int syscall, unsigned int arg1, unsigned int arg2, unsigned int arg3);

// These macros kinda look like a staircase. :P
#define _GET_DEFINE_IN_LAST_POSITION(_0, _1, _2, _3, _LAST, ...) _LAST
#define _DCLOAD_1_ARG(syscall) dcloadsyscall_wrapper(syscall, 0, 0, 0)
#define _DCLOAD_2_ARG(syscall, arg1) dcloadsyscall_wrapper(syscall, (unsigned int)arg1, 0, 0)
#define _DCLOAD_3_ARG(syscall, arg1, arg2) dcloadsyscall_wrapper(syscall, (unsigned int)arg1, (unsigned int)arg2, 0)
#define _DCLOAD_4_ARG(syscall, arg1, arg2, arg3) dcloadsyscall_wrapper(syscall, (unsigned int)arg1, (unsigned int)arg2, (unsigned int)arg3)
#define dcloadsyscall(...) _GET_DEFINE_IN_LAST_POSITION(__VA_ARGS__, _DCLOAD_4_ARG, _DCLOAD_3_ARG, _DCLOAD_2_ARG, _DCLOAD_1_ARG)(__VA_ARGS__)
// ^^ This macro is the one that gets invoked when using dcloadsyscall(syscall, ...) with this method.

//==============================================================================
// -- IMPORTANT DCLOAD-SERIAL NOTE: --
//==============================================================================
//
// dcload-serial can use a 64kB work area to do data compression. However, there
// is no malloc here. So in order to enable that functionality, define an
// 8-byte-aligned global array like this at the top of dc_main.c:
//
//    __attribute__((aligned(8))) unsigned char dcload_serial_workmem[65536];
//
// and at the very beginning of the dreamcast_main() function do this:
//
//  dcloadsyscall(DCLOAD_ASSIGNWRKMEM, dcload_serial_workmem);
//
// and that will give dcload-serial a 64kB work area. Adding these will not have
// any effect on dcload-ip, and it will make the binary 64kB larger.
//

#endif /* __DC_FS_DCLOAD_H */
