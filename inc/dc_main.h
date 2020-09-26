#ifndef __DC_MAIN_H_
#define __DC_MAIN_H_

// In freestanding mode, the only available standard header files are: <float.h>,
// <iso646.h>, <limits.h>, <stdarg.h>, <stdbool.h>, <stddef.h>, and <stdint.h>
// (C99 standard 4.6).

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <float.h>
#include <stdarg.h>

// GCC needs memmove/memcpy/memset/memcmp to be provided in a freestanding environment.
// "memfuncs" also has a lot of highly-optimized versions of these functions, as well.
#include "memfuncs.h"

// DreamHAL Modules:
// Recommended to always have (especially startup_support and fs_dcload):
#include "startup_support.h"
#include "sh4_math.h"
#include "fs_dcload.h"

// Just needed for the random test code in dc_main.c:
// Remember to put these modules in the "inc" folder to use them!
#include "print.h"
#include "simple_print.h"

int dreamcast_main(void);

#endif /* __DC_MAIN_H_ */
