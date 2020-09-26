# DreamHAL
A hardware abstraction library (HAL) for Dreamcast/SH4 (the SH7750 family in little endian mode, in particular)

The primary aim of DreamHAL is to expose SH4 hardware functionality in a modular way, with emphasis placed on achieving maximum possible performance on supported systems. The modular design aims to make it easy to use only the components an embedded application needs, and the focus on performance aims to get the hardware out of the way such that the need for code optimization is minimized as much as possible.

Please note that, due to the low-level nature of this HAL, basic familiarity with the SH4 processor line is assumed (meaning, this is not meant for beginners!).

The Renesas SH7750 group hardware manual is recommended reading, and it is available for download from the [Renesas website](https://www.renesas.com/us/en/). It is in the "Documents" section of the SH7750, SH7750S, and SH7750R processors' product pages (the SH7750 group software manual is also a good reference). [Oleg Endo's SuperH instruction set reference](http://www.shared-ptr.com/sh_insns.html) is another excellent resource, and for more information on the special assembly instructions ``fsca`` and ``fsrra``, refer to the SH7780 software manual (this is an SH4A chip), as these instructions are not technically documented on SH4 processors.

## Current Modules
 - SH4 System Registers
 - Math (FPU)
 - Startup support & Dreamcast video modes
 - Performance counters
 - Cache Management
 - Print (printf and friends)
 - Simple Print (lightweight conversions to string)
 - dcload (to make use of dcload's syscall interface on Dreamcast)

## Generic Compiler and Linker Requirements
(When in doubt, check Compile.sh!)

GCC 9.2.0 + Binutils 2.33.1 or later is required for full functionality. Any explicit support for older versions is provided purely as a courtesy, and interoperability with such older versions is not guaranteed.

Compiler requirements to use individual DreamHAL modules:
 - Target SH4-ELF (not sh4-linux-gnu), little endian
 - Use GNU C extensions (use --std=gnuXX instead of --std=cXX)
 - Use -fno-strict-aliasing
 - Use either -m4-single-only or -mrenesas (or both). Note that -m4-single-only is mandatory to use the matrix functions in the math module.

## Using the Provided Compile Script
(You don't need to do this if you just want the individual modules.)

Build GCC 9.2.0 and Binutils 2.33.1 like so (not for the faint of heart, this is not an easy thing to do):

Quick notes:
- To use newer versions, just change the version numbers as appropriate.
- I use this with Windows Subsystem for Linux v2, Debian distribution, with a pre-existing [KallistiOS](https://github.com/KallistiOS/KallistiOS) toolchain (this is entirely independent of KOS, however).
- I have a folder called C:\DreamcastKOS\, and this repository is cloned there.
- The directory structure of correctly set up configuration for use with this repository as-is looks like this (**bold** folders can be deleted when done, as they take up a lot of space):
 - C:\DreamcastKOS\
   - DreamHAL\ <-- This repo
   - **binutils-2.33.1\\** <-- The Binutils 2.33.1 source folder
   - **binutils-objdir\\** <-- Where the commands to build binutils are run within
   - binutils-sh4\ <-- The output folder for binutils binaries, which binutils will automatically create with the provided command line arguments
   - **gcc-9.2.0\\** <-- The GCC 9.2.0 source folder
   - **gcc-objdir\\** <-- Where the commands to build GCC are run within
   - gcc-sh4\ <-- The output folder for GCC binaries, which GCC will automatically create with the provided command line arguments

**Binutils:**  

Must do binutils first before even touching GCC; remember to make the binutils-objdir folder outside of the binutils source folder and run these lines from there:
```
$PWD/../binutils-2.33.1/configure -v --build=x86_64-linux-gnu --host=x86_64-linux-gnu --target=sh4-elf --prefix=$PWD/../binutils-sh4 --disable-libssp --disable-tls --disable-shared --enable-static --disable-nls

make
make install
```
Then run:
```
sh4-elf-env-for-gcc.sh
```
to set up the environment variables before configuring and making GCC. This sets up bash for building GCC with sh4-elf binutils.  
Need to run this next so that GCC can find the sh4 binutils that don't have the prefixes:
```
export PATH=/mnt/c/DreamcastKOS/binutils-sh4/bin:/mnt/c/DreamcastKOS/binutils-sh4/sh4-elf/bin:$PATH
```
**GCC:**

First, this MUST be run from inside the GCC source code folder:
```
contrib/download_prerequisites
```
Then make the gcc-objdir folder outside of the GCC source code folder and run this from there:
```
$PWD/../gcc-9.2.0/configure -v --build=x86_64-linux-gnu --host=x86_64-linux-gnu --target=sh4-elf --prefix=$PWD/../gcc-sh4 --enable-checking=release --enable-languages=c --with-cpu=m4-single-only --with-multilib-list=m4-single-only,m4-nofpu,m4 --with-endian=little --without-headers --disable-libssp --disable-tls --disable-shared --enable-static --disable-nls
make all-gcc
make all-target-libgcc
make install-gcc
make install-target-libgcc
```
Before invoking GCC, run this so that GCC can find the sh4 binutils:  
(Note: this is actually done in Compile.sh, so you don't need to run it manually. Just thought it might be worth mentioning.)
```
export PATH=/mnt/c/DreamcastKOS/binutils-sh4/bin:/mnt/c/DreamcastKOS/binutils-sh4/sh4-elf/bin:$PATH
```

Now Compile.sh will work. In Terminal, ``cd`` into ``/mnt/c/DreamcastKOS/DreamHAL`` to run it.

## Making Things

Put whatever you want in dc_main.c's ``dreamcast_main`` function, then run ``Compile.sh``:

```
// main function
int dreamcast_main(void)
{
  // First things first: let's get a more modern color mode up
  // Set up color mode and resolution (cable type and console region are auto-detected by this)
  STARTUP_Init_Video(FB_RGB0888, USE_640x480);
  //  STARTUP_Init_Video(FB_RGB0888, USE_320x240);


  /* Put whatever you want here. */


  // Reset video mode for dcload
  STARTUP_Set_Video(FB_RGB0555, USE_640x480);

  return 0; // startup.S doesn't care what this is
}
```

Any headers go into the ``inc`` folder, and any source files go into the ``src`` folder. To use a DreamHAL module, just move the source and header files for the module out of the ``modules`` folder and into ``src``/``inc``. Easy! Note that the startup support and dcload modules permanently live in ``startup`` since ``Startup.S`` uses them both, and that the math module is already out of the ``modules`` folder.

The binary that results from compilation is called ``program.bin`` and will be in the same directory as ``Compile.sh``. The ``program.elf`` file is the same thing as the raw binary except in ELF format, and either one can be used with a loader like [dcload-ip](https://github.com/Moopthehedgehog/dcload-ip) or [dcload-serial](https://github.com/sizious/dcload-serial). Note that ``program.bin`` is unscrambled, so to boot it via CD-R on an actual Dreamcast it would need to be scrambled and bundled with a bootstrap file. My current personal preference for making a bootable image is using [BootDreams 1.0.6c](https://code.google.com/archive/p/bootdreams/downloads) to make a data/data CDI, and then burning it with [this tool](https://www.imgburn.com/) with the [CDI plugin (it's at the bottom of the download page)](https://www.imgburn.com/index.php?act=download). Burn success rate is very nearly, if not actually, 100% by doing it this way.

## License

See the LICENSE file. I promise it doesn't bite!

## Supported Dreamcast Video Modes

From inc/startup_support.h:

```
-- Overview of Available Modes --

"PVR 32x32" means the framebuffer is a multiple of 32x32. Modes without that
label are "raw modes" and have no blank pixels or extra pixels, which may be
useful for compatibility testing or software renderers. Modes that only have
a PVR 32x32 version are actually raw modes with framebuffers already multiples
of 32x32, and no extra modification was needed for them.

60Hz:

848x480 @ 60Hz (16:9, DMT, but using a slightly-too-short hsync)
848x480 @ 60Hz (16:9, DMT, but using a slightly-too-short hsync) - PVR 32x32
800x600 @ 60Hz (4:3, DMT, but using a slightly-too-short hsync)
800x600 @ 60Hz (4:3, DMT, but using a slightly-too-short hsync) - PVR 32x32
800x600 @ 60Hz (4:3, CVT)
800x600 @ 60Hz (4:3, CVT) - PVR 32x32
1024x768 @ 60Hz (4:3, DMT)
1024x768 @ 60Hz (4:3, DMT) - PVR 32x32
1152x864 @ 60Hz (4:3, CVT)
1152x864 @ 60Hz (4:3, CVT) - PVR 32x32
720p60 (16:9, DMT & CTA-861) - for HDTVs
720p60 (16:9, DMT & CTA-861) - for HDTVs - PVR 32x32
1280x720 (16:9, CVT) - for monitors that need this instead of HDTV 720p60
1280x720 (16:9, CVT) - for monitors that need this instead of HDTV 720p60 - PVR 32x32
1280x800 @ 60Hz (16:10, DMT & CVT)
1280x800 @ 60Hz (16:10, DMT & CVT) - PVR 32x32
1280x960 @ 60Hz (4:3, DMT) - PVR 32x32
1440x900 @ 60Hz (16:10, DMT & CVT)
1440x900 @ 60Hz (16:10, DMT & CVT) - PVR 32x32

75Hz:

640x480 @ 75Hz (4:3, DMT)
640x480 @ 75Hz (4:3, DMT) - PVR 32x32
800x600 @ 75Hz (4:3, DMT)
800x600 @ 75Hz (4:3, DMT) - PVR 32x32
1024x768 @ 75Hz (4:3, DMT) - PVR 32x32
1152x864 @ 75Hz (4:3, DMT) - PVR 32x32

120Hz (LCD Only - Untested):

480p @ 120Hz (4:3, CTA-861, 720x480) - for HDTVs - PVR 32x32
640x480 @ 120Hz (4:3, CVT, RB) - for monitors that need this instead of HDTV 480p120
640x480 @ 120Hz (4:3, CVT, RB) - PVR 32x32
800x600 @ 120Hz (4:3, DMT & CVT, RB)
1024x768 @ 120Hz (4:3, DMT & CVT, RB)

240Hz (LCD Only - Untested):

480p @ 240Hz (4:3, CTA-861, 720x480) - PVR 32x32
480p @ 239.76Hz (4:3, CTA-861, 720x480) - PVR 32x32

CVT RBv2 Native Modes (LCD Only, probably need one newer than 2013):

640x480 @ 75Hz (4:3, CVT, RBv2) - PVR 32x32
848x480 @ 60Hz (16:9, CVT, RBv2) - PVR 32x32**

** Well, ok, this one's actually 832x480 internally, but the horizontal
"stretch" is just 2%. It's the closest we can get to a real native 16:9 mode,
anyways, because the next step up is 864 wide... Which is too big for 848x480
to work. So if you're looking for a true native widescreen, this is about as
good as it gets.
```
See startup_support.h & startup_support.c for more details, including info on the cool analog video trick that makes all the non-native modes work.

### To use startup_support.c/.h with KOS projects for the extra video modes:

Remove these from startup_support.c:
```
// Enable or disable 8kB onchip RAM
const uint32_t STARTUP_use_ocram = ENABLE_OCRAM;

// Set by startup.S depending on the state of dcload (0 = none, 1 = present with console, 2 = present without console)
uint32_t STARTUP_dcload_present = 0;
```

Remove these from startup_support.h:
```
// In startup_support.c, use these with 'use_ocram' to enable the 8kB onchip RAM
// and halve the operand cache to 8kB. Disabling it keeps the full 16kB operand
// cache intact.
#define ENABLE_OCRAM 1
#define DISABLE_OCRAM 0

// Enable 8kB onchip RAM or use full 16kB cache as just cache
// NOTE: This value is set in startup_support.c; please don't mess with this extern
extern const uint32_t STARTUP_use_ocram;

#define DCLOAD_NOT_PRESENT 0
#define DCLOAD_CONSOLE 1
#define DCLOAD_NO_CONSOLE 2

// Set by startup.S depending on the state of dcload (0 = none, 1 = present with console, 2 = present without console)
extern uint32_t STARTUP_dcload_present;
```

And follow this simple guideline:
```
// Include the DreamHAL startup support module header
#include "startup_support.h"

int main() // KOS main
{
  // STARTUP_Init_Video() will call STARTUP_Set_Video() the first time and set a
  // 640x480 or 320x240-in-640x480 mode, as well as some global init parameters
  STARTUP_Init_Video(color mode, USE_640x480 or USE 320x240);

  // Need to overwrite KOS's settings for these specific parameters
  vid_mode->width = STARTUP_video_params.fb_width;
  vid_mode->height = STARTUP_video_params.fb_height;
  vid_mode->pm = STARTUP_video_params.video_color_type;

  /*
    Do whatever you want.
    After this point you can set one of the extra video modes, but you'll need to do this:

    // Need to overwrite KOS's settings for these specific parameters
    vid_mode->width = STARTUP_video_params.fb_width;
    vid_mode->height = STARTUP_video_params.fb_height;
    vid_mode->pm = STARTUP_video_params.video_color_type;

    After any call to change the video mode via an extra mode or a call to STARTUP_Set_Video().
  */

  // Return to dcload or something
  STARTUP_Set_Video(FB_RGB0555, USE_640x480);
  return 0; // startup.S doesn't care what int this returns
}
```

If you get a compiler warning about FPSCR-related things, you can just remove the "FPSCR Support" sections from startup_support.h and startup_support.c. If you don't know what FPSCR is, how to work with it, and how GCC makes important assumptions based on it, you have no reason to keep that section or otherwise worry about FPSCR.

## Acknowledgements
- The [KallistiOS project](https://github.com/KallistiOS/KallistiOS/) and all its contributors over the years
- [dcemulation.org](https://dcemulation.org/phpBB/) for its wealth of information and being an all-around great place to communicate with homebrew and indie Dreamcast developers
- Oleg Endo for [an awesome SuperH instruction set reference](https://github.com/shared-ptr/sh_insns)
- [Sega](https://www.sega.com/) for the ultimate gaming system
- [Renesas](https://www.renesas.com/us/en/), [Hitachi](https://www.hitachi.com/), [Mitsubishi](https://www.mitsubishi.com/en/), and [STMicro](https://www.st.com/content/st_com/en.html) for inventing SH4
- [mrneo240](https://github.com/mrneo240) for finding a bug in sh4_math.h because fsrra actually does NOT work on negatives
- [Protofall](https://github.com/Protofall) for suggesting video mode KOS integration
- [nymus](https://dcemulation.org/phpBB/memberlist.php?mode=viewprofile&u=13916) for [finding a video scale bug](https://dcemulation.org/phpBB/viewtopic.php?f=29&t=105441&p=1057459)
- The Dreamcast community at large, which ardently refuses to let the Dreamcast die or otherwise languish in obscurity. Now let's make some awesome stuff!
