// ---- startup_support.c - Dreamcast Startup Support Module ----
//
// Version 1.1.2
//
// This file is part of the DreamHAL project, a hardware abstraction library
// primarily intended for use on the SH7091 found in hardware such as the SEGA
// Dreamcast game console.
//
// This support module is hereby released into the public domain in the hope that it
// may prove useful.
//
// --Moopthehedgehog, January 2020
//
// This file contains some C code that is required by startup.S for setting FPSCR
// in addition to video setup support functions
//

#include "startup_support.h"

//==============================================================================
// System Support
//==============================================================================

// Enable or disable 8kB onchip RAM
const uint32_t STARTUP_use_ocram = ENABLE_OCRAM;

// Set by startup.S depending on the state of dcload (0 = none, 1 = present with console, 2 = present without console)
uint32_t STARTUP_dcload_present = 0;

// Set by STARTUP_Init_Video().
// This is a global cable type value for user reference (0 = VGA, 2 = RGB, 3 = Composite/S-Video)
uint32_t STARTUP_cable_type = 0;

// Set by STARTUP_Init_Video().
// This is a global console region type for user reference (0 = JP, 1 = NA, 2 = PAL)
uint32_t STARTUP_console_region = 0;

//==============================================================================
// FPSCR Support
//==============================================================================

// startup.S needs this to set FPSCR since GCC deprecated __set_fpscr and
// __get_fpscr and replaced them with builtins.
//
// void __builtin_sh_set_fpscr(uint32_t val) doesn't affect SZ, PR, and FR,
// unlike the old void __set_fpscr(uint32_t val) macro.
//
// uint32_t __builtin_sh_get_fpscr(void), on the other hand, behaves the same as
// the old uint32_t __get_fpscr(void) macro.
//
// Also: the old macros were in libgcc, and the new ones are not (yay!).
//

// GCC <= 4 needs to link with -lgcc
#if __GNUC__ <= 4
extern void __set_fpscr(uint32_t value);
extern uint32_t __get_fpscr(void);

void __call_builtin_sh_set_fpscr(uint32_t value)
{
  __set_fpscr(value);
}

uint32_t __call_builtin_sh_get_fpscr(void)
{
  return __get_fpscr();
}

#else

void __call_builtin_sh_set_fpscr(uint32_t value)
{
  __builtin_sh_set_fpscr(value);
}

uint32_t __call_builtin_sh_get_fpscr(void)
{
  return __builtin_sh_get_fpscr();
}

#endif

//==============================================================================
// Video Support
//==============================================================================
//
// After days of examining a disassembly I made of my own Dreamcast BootROM and
// testing on actual Dreamcast hardware, this should be everything there is to
// set up correct video output modes.
//
// The framebuffer address will always be 0xa5000000 after any of these run.
//

// Auxiliary Reading Material:
//
// These links are provided as a courtesy in the event one wants to learn about
// what these registers actually do, and because they're all cool projects and
// documentation that you should totally check out anyways:
//
// - A register mapping effort done here:
//    http://dev.dcemulation.org/tutorials/registermap.htm
// - Lars Olsson's descriptions of the Dreamcast video subsystem:
//    https://www.ludd.ltu.se/~jlo/dc/powervr-reg.txt
// - Lars Olsson's attempt to reconstruct a Dreamcast BootROM:
//    https://www.ludd.ltu.se/~jlo/dc/bootROM.c
// - Marcus Comstedt's descriptions of the Dreamcast video subsystem:
//    http://mc.pp.se/dc/pvr.html
// - The Renesas SH7750 series hardware manual (in the documents section, it's
//  called "SH7750, SH7750S, SH7750R Group User's Manual: Hardware"), which
//  describes the GPIO port settings:
//    https://www.renesas.com/us/en/products/microcontrollers-microprocessors/superh/sh7750/sh7750r.html
// - KallistiOS 2.1.0 video.c file:
//    https://github.com/KallistiOS/KallistiOS/
// - 240p Test Suite v1.25 vmode.c file:
//    https://sourceforge.net/p/testsuite240p/
//

// Notes:
// - The color mode in 0xa05f8048 should (maybe?) match the framebuffer mode for
//   the graphics chip to render directly to the screen
// - For SH4 to draw directly to the framebuffer,0xa05f8048 doesn't matter
// - Bit 23 of 0xa05f8044 selects between 13.5MHz (NTSC/PAL) and 27MHz (VGA)
//  as required by the BU142x series of video encoders, which are apparently
//  used by the Dreamcast: https://www.digchip.com/datasheets/parts/datasheet/406/BU1425AK.php
//  (the Dreamcast has the exact same video DAC model as the Naomi 2, which uses
//  this series of chips per https://segaretro.org/Sega_NAOMI_2. Notice that
//  27MHz (VGA) is 54MHz/2 and 13.5MHz (NTSC/PAL) is 54MHz/4).
// - Cable types: 0 is VGA, 1 is undefined, 2 is SCART, 3 is composite
// - Framebuffer color modes: 16bit = 2Bpp (RGB0555, RGB565), 24bit = 3Bpp
//  (RGB888), 32bit = 4Bpp (RGB0888)
// - Apparently the AICA needs to know the cable type, and it turns out it does
//  have a pin--it's actually the AICA's JTAG TDO--that it uses to communicate
//  to the video DAC. This AICA register also seems to have a reset bit, so it
//  appears to be the AICA's JTAG control register being written to here. Dear
//  Sega engineers: nice one, using an output pin that's always an output
//  instead of a GPIO pin whose state gets reset to pulled-up input every boot.
//  Probably solved a graphical glitch or monitor compatibility issue that way. :)
//

VIDEO_PARAMS_STRUCT STARTUP_video_params = {1.0f, 1.0f, 640, 480, 60, 0, 640, 480, 2};

// These get set by STARTUP_Init_Video() for use only by STARTUP_Set_Video() and
// extra video mode setup functions.
static uint32_t cable_mode = 0;
static uint32_t video_region = 0;

// Video mode is automatically determined based on cable type and console region
// This sets up everything related to Dreamcast video modes.
void STARTUP_Init_Video(uint8_t fbuffer_color_mode, uint8_t use_320x240)
{
  // Set cable type to hardware pin setting
  // Need to read port 8 and 9 data (bits 8 & 9 in PDTRA), so set them as input
  // direction via PCTRA (necessary per SH7750 hardware manual):
  *(volatile uint32_t*)0xff80002c = ( (*(volatile uint32_t*)0xff80002c) & 0xfff0ffff ) | 0x000a0000;

  // According to the BootROM, cable data is on PORT8/9 GPIO pins.
  // Read them and then write them to somewhere in AICA memory (refer to notes
  // section for an explanation and a theory as to why this might be necessary):
  cable_mode = (uint32_t)( (*(volatile uint16_t*)0xff800030) & 0x300 );
  *(volatile uint32_t*)0xa0702c00 = ( (*(volatile uint32_t*)0xa0702c00) & 0xfffffcff ) | cable_mode;
  // Per the SH7750 manual, there are 16 data regs, hence a 16-bit read should
  // be used on PDTRA.

  // Store global cable type (0 = VGA, 2 = RGB, 3 = Composite/S-Video)
  STARTUP_cable_type = cable_mode >> 8;
  // Store global console region type (0 = JP, 1 = NA, 2 = PAL)
  STARTUP_console_region = (*(uint8_t*)0x8c000072) - 0x30;

  // Store video output region (0 = NTSC, 1 = PAL)
  video_region = (*(uint8_t*)0x8c000074) - 0x30;

  // Reset graphics subsystem (PVR2), but keep the graphics memory bus on ..or
  // else things will hang when writing to graphics memory since it got disabled!
  *(volatile uint32_t*)0xa05f8008 = 0x00000003;
  // Re-enable PVR, TA
  *(volatile uint32_t*)0xa05f8008 = 0x00000000;

  STARTUP_Set_Video(fbuffer_color_mode, use_320x240);
}

// Set a standard Dreamcast video mode
// These are the plain 640x480 modes (VGA and composite/S-Video), and are based
// on how the BootROM does things. 320x240 is a linedoubled + pixeldoubled mode,
// so the framebuffer is 320x240xColorBpp and the output frame is standard 640x480.
void STARTUP_Set_Video(uint8_t fbuffer_color_mode, uint8_t use_320x240)
{
  // Set global scale factors
  STARTUP_video_params.video_scale = 1.0f;
  STARTUP_video_params.video_scale_multiplier = 1.0f;

  // Set global video output mode parameters
  STARTUP_video_params.video_width = 640;
  STARTUP_video_params.video_height = 480;
  STARTUP_video_params.video_color_type = fbuffer_color_mode;
  STARTUP_video_params.video_refresh_rate = 60;

  uint32_t horiz_active_area = 640;
  uint32_t vert_active_area = 480;
  // {RGB0555, RGB565} = 2Bpp, {RGB888} = 3Bpp, {RGB0888} = 4Bpp
  uint32_t bpp_mode_size = fbuffer_color_mode + 1 + (0x1 ^ ((fbuffer_color_mode & 0x1) | (fbuffer_color_mode >> 1))); // Add another 1 only if 0b00

  // NOTE:
  // Unlike the extra video modes that set framebuffer parameters here, this
  // function sets the framebuffer parameters at the end to account for the
  // scaled 320x240 modes.

  if((!cable_mode) && (!use_320x240)) // VGA 640x480 @ 60Hz
  {
    // Set registers the same way that the BootROM does
    *(volatile uint32_t*)0xa05f80e8 = 0x00160008;
    *(volatile uint32_t*)0xa05f8044 = 0x00800000 | (fbuffer_color_mode << 2);

  //  *(volatile uint32_t*)0xa05f8048 = ( (*(volatile uint32_t*)0xa05f8048) & 0xfffffff8 ) | 0 -> 0,3 / 1 -> 1 / 2 -> 4 / 3 -> 5,6; ( x -> 2 is ARGB4444, doesn't match any display mode)
    *(volatile uint32_t*)0xa05f804c = (horiz_active_area * bpp_mode_size) / 8; // for PVR to know active area width
    *(volatile uint32_t*)0xa05f8040 = 0x00000000; // Border color in RGB0888 format
    *(volatile uint32_t*)0xa05f805c = (1 << 20) | ((vert_active_area - 1) << 10) | (((horiz_active_area * bpp_mode_size) / 4) - 1); // progressive scan has a 1 since no lines are skipped
    *(volatile uint32_t*)0xa05f80ec = 0x000000a8;
    *(volatile uint32_t*)0xa05f80f0 = 0x00280028;
    *(volatile uint32_t*)0xa05f80c8 = 0x03450000;
    *(volatile uint32_t*)0xa05f80cc = 0x00150208;
    *(volatile uint32_t*)0xa05f80d0 = 0x00000100;
    *(volatile uint32_t*)0xa05f80d4 = 0x007e0345;
    *(volatile uint32_t*)0xa05f80d8 = 0x020c0359;
    *(volatile uint32_t*)0xa05f80dc = 0x00280208;
    *(volatile uint32_t*)0xa05f80e0 = 0x03f1933f;

    uint32_t scan_area_size = horiz_active_area * vert_active_area;
    uint32_t scan_area_size_bytes = scan_area_size * bpp_mode_size; // This will always be divisible by 4

    // Reset framebuffer address
    *(volatile uint32_t*)0xa05f8050 = 0x00000000; // BootROM sets this to 0x00200000 (framebuffer base is 0xa5000000 + this)
    *(volatile uint32_t*)0xa05f8054 = 0x00000000; // Same for progressive, resetting the offset gets us 2MB VRAM back after BootROM is done with it

    // zero out framebuffer area
    for(uint32_t pixel_or_two = 0; pixel_or_two < scan_area_size_bytes; pixel_or_two += 4)
    {
      *(uint32_t*)(0xa5000000 + pixel_or_two) = 0;
    }

    // re-enable video
    *(volatile uint32_t*)0xa05f80e8 &= ~8;
    *(volatile uint32_t*)0xa05f8044 |= 1;
  }
  else if((!cable_mode) && (use_320x240)) // VGA 320x240 @ 60Hz (linedoubled + pixeldoubled to 640x480)
  {
    // Modify what BootROM does to get 320x240 in VGA, formatted just like 640x480
    horiz_active_area /= 2;
    vert_active_area /= 2;

    *(volatile uint32_t*)0xa05f80e8 = 0x00160008 | 0x100; // 0x100 for pixeldouble (each horiz pixel gets drawn twice; 2x horiz scale)
    *(volatile uint32_t*)0xa05f8044 = 0x00800000 | (fbuffer_color_mode << 2) | 0x2; // 0x2 for linedouble (each line gets drawn twice; 2x vert scale)

    *(volatile uint32_t*)0xa05f804c = (horiz_active_area * bpp_mode_size) / 8; // for PVR to know active area width
    *(volatile uint32_t*)0xa05f8040 = 0x00000000; // Border color in RGB0888 format
    *(volatile uint32_t*)0xa05f805c = (1 << 20) | ((vert_active_area - 1) << 10) | (((horiz_active_area * bpp_mode_size) / 4) - 1); // progressive scan has a 1 since no lines are skipped
    *(volatile uint32_t*)0xa05f80ec = 0x000000a8;
    *(volatile uint32_t*)0xa05f80f0 = 0x00280028;
    *(volatile uint32_t*)0xa05f80c8 = 0x03450000;
    *(volatile uint32_t*)0xa05f80cc = 0x00150208;
    *(volatile uint32_t*)0xa05f80d0 = 0x00000100;
    *(volatile uint32_t*)0xa05f80d4 = 0x007e0345;
    *(volatile uint32_t*)0xa05f80d8 = 0x020c0359;
    *(volatile uint32_t*)0xa05f80dc = 0x00280208;
    *(volatile uint32_t*)0xa05f80e0 = 0x03f1933f;

    uint32_t scan_area_size = horiz_active_area * vert_active_area;
    uint32_t scan_area_size_bytes = scan_area_size * bpp_mode_size; // This will always be divisible by 4

    // Reset framebuffer address
    *(volatile uint32_t*)0xa05f8050 = 0x00000000; // BootROM sets this to 0x00200000 (framebuffer base is 0xa5000000 + this)
    *(volatile uint32_t*)0xa05f8054 = 0x00000000; // Same for progressive, resetting the offset gets us 2MB VRAM back after BootROM is done with it

    // zero out framebuffer area
    for(uint32_t pixel_or_two = 0; pixel_or_two < scan_area_size_bytes; pixel_or_two += 4)
    {
      *(uint32_t*)(0xa5000000 + pixel_or_two) = 0;
    }

    // re-enable video
    *(volatile uint32_t*)0xa05f80e8 &= ~8;
    *(volatile uint32_t*)0xa05f8044 |= 1;
  }
  else if((!video_region) && (!use_320x240)) // NTSC (480i)
  {
    // Set registers the same way that the BootROM does
    *(volatile uint32_t*)0xa05f80e8 = 0x00160008;
    *(volatile uint32_t*)0xa05f8044 = 0x00000000 | (fbuffer_color_mode << 2);

    *(volatile uint32_t*)0xa05f804c = (horiz_active_area * bpp_mode_size) / 8; // for PVR to know active area width
    *(volatile uint32_t*)0xa05f8040 = 0x00000000; // Border color in RGB0888 format
    *(volatile uint32_t*)0xa05f805c = ((((horiz_active_area * bpp_mode_size) / 4) + 1) << 20) | (((vert_active_area / 2) - 1) << 10) | (((horiz_active_area * bpp_mode_size) / 4) - 1);
    *(volatile uint32_t*)0xa05f80ec = 0x000000a4;
    *(volatile uint32_t*)0xa05f80f0 = 0x00120012;
    *(volatile uint32_t*)0xa05f80c8 = 0x03450000;
    *(volatile uint32_t*)0xa05f80cc = 0x00150104;
    *(volatile uint32_t*)0xa05f80d0 = 0x00000150;
    *(volatile uint32_t*)0xa05f80d4 = 0x007e0345;
    *(volatile uint32_t*)0xa05f80d8 = 0x020c0359;
    *(volatile uint32_t*)0xa05f80dc = 0x00240204;
    *(volatile uint32_t*)0xa05f80e0 = 0x07d6c63f;

    uint32_t scan_area_size = horiz_active_area * vert_active_area;
    uint32_t scan_area_size_bytes = scan_area_size * bpp_mode_size; // This will always be divisible by 4

    // Reset framebuffer address
    *(volatile uint32_t*)0xa05f8050 = 0x00000000; // BootROM sets this to 0x00200000 (framebuffer base is 0xa5000000 + this)
    *(volatile uint32_t*)0xa05f8054 = horiz_active_area * bpp_mode_size; // This is for interlaced, resetting the offset gets us 2MB VRAM back after BootROM is done with it

    // zero out framebuffer area
    for(uint32_t pixel_or_two = 0; pixel_or_two < scan_area_size_bytes; pixel_or_two += 4)
    {
      *(uint32_t*)(0xa5000000 + pixel_or_two) = 0;
    }

    // re-enable video
    *(volatile uint32_t*)0xa05f80e8 &= ~8;
    *(volatile uint32_t*)0xa05f8044 |= 1;
  }
  else if((!video_region) && (use_320x240)) // NTSC (240p, but we need to wrap it in 480i for most TVs to care)
  {
    horiz_active_area /= 2;
    vert_active_area /= 2;

    *(volatile uint32_t*)0xa05f80e8 = 0x00160008 | 0x100; // 0x100 for pixeldouble (each horiz pixel gets drawn twice; 2x horiz scale)
    *(volatile uint32_t*)0xa05f8044 = 0x00000000 | (fbuffer_color_mode << 2) | 0x2; // 0x2 for linedouble (each line gets drawn twice; 2x vert scale)

    *(volatile uint32_t*)0xa05f804c = (horiz_active_area * bpp_mode_size) / 8; // for PVR to know active area width
    *(volatile uint32_t*)0xa05f8040 = 0x00000000; // Border color in RGB0888 format
    *(volatile uint32_t*)0xa05f805c = ((((horiz_active_area * bpp_mode_size) / 4) + 1) << 20) | ((vert_active_area - 1) << 10) | (((horiz_active_area * bpp_mode_size) / 4) - 1);
    *(volatile uint32_t*)0xa05f80ec = 0x000000a4;
    *(volatile uint32_t*)0xa05f80f0 = 0x00120012;
    *(volatile uint32_t*)0xa05f80c8 = 0x03450000;
    *(volatile uint32_t*)0xa05f80cc = 0x00150104;
    *(volatile uint32_t*)0xa05f80d0 = 0x00000140 | 0x10; // Interlaced
    *(volatile uint32_t*)0xa05f80d4 = 0x007e0345;
    *(volatile uint32_t*)0xa05f80d8 = 0x020c0359;
    *(volatile uint32_t*)0xa05f80dc = 0x00240204;
    *(volatile uint32_t*)0xa05f80e0 = 0x07d6c63f; // Really we need 240p-in-480i

    uint32_t scan_area_size = horiz_active_area * vert_active_area;
    uint32_t scan_area_size_bytes = scan_area_size * bpp_mode_size; // This will always be divisible by 4

    // Reset framebuffer address
    *(volatile uint32_t*)0xa05f8050 = 0x00000000; // BootROM sets this to 0x00200000 (framebuffer base is 0xa5000000 + this)
    *(volatile uint32_t*)0xa05f8054 = horiz_active_area * bpp_mode_size; // Resetting the framebuffer offset gets us 2MB VRAM back after BootROM is done with it

    // zero out framebuffer area
    for(uint32_t pixel_or_two = 0; pixel_or_two < scan_area_size_bytes; pixel_or_two += 4)
    {
      *(uint32_t*)(0xa5000000 + pixel_or_two) = 0;
    }

    // re-enable video
    *(volatile uint32_t*)0xa05f80e8 &= ~8;
    *(volatile uint32_t*)0xa05f8044 |= 1;
  }
  else if(use_320x240) // PAL (would be 288p, actually 264p according to 240p Test Suite, but we need to wrap it in 576i for modern TVs to care)
  {
    horiz_active_area /= 2;
    vert_active_area /= 2;
    #ifdef PAL_EXTRA_LINES
      // Low-res PAL can actually have 264 active vertical lines on the Dreamcast (24 more than NTSC!)
      // In theory a developer could hide a secret message for PAL players in this area
      vert_active_area += 24;
    #endif

    *(volatile uint32_t*)0xa05f80e8 = 0x00160008 | 0x100; // 0x100 for pixeldouble (each horiz pixel gets drawn twice; 2x horiz scale)
    *(volatile uint32_t*)0xa05f8044 = 0x00000000 | (fbuffer_color_mode << 2) | 0x2; // 0x2 for linedouble (each line gets drawn twice; 2x vert scale)

    *(volatile uint32_t*)0xa05f804c = (horiz_active_area * bpp_mode_size) / 8; // for PVR to know active area width
    *(volatile uint32_t*)0xa05f8040 = 0x00000000; // Border color in RGB0888 format
    *(volatile uint32_t*)0xa05f805c = ((((horiz_active_area * bpp_mode_size) / 4) + 1) << 20) | ((vert_active_area - 1) << 10) | (((horiz_active_area * bpp_mode_size) / 4) - 1);
    *(volatile uint32_t*)0xa05f80ec = 0x000000ae;
    *(volatile uint32_t*)0xa05f80f0 = 0x002e002d;
    *(volatile uint32_t*)0xa05f80c8 = 0x034b0000;
    *(volatile uint32_t*)0xa05f80cc = 0x00150136;
    *(volatile uint32_t*)0xa05f80d0 = 0x00000180 | 0x10; // Interlaced
    *(volatile uint32_t*)0xa05f80d4 = 0x008d034b;
    *(volatile uint32_t*)0xa05f80d8 = 0x0270035f;
    *(volatile uint32_t*)0xa05f80dc = 0x002c026c;
    *(volatile uint32_t*)0xa05f80e0 = 0x07d6a53f; // Really we need 288p-in-576i

    uint32_t scan_area_size = horiz_active_area * vert_active_area;
    uint32_t scan_area_size_bytes = scan_area_size * bpp_mode_size; // This will always be divisible by 4

    // Reset framebuffer address
    *(volatile uint32_t*)0xa05f8050 = 0x00000000; // BootROM sets this to 0x00200000 (framebuffer base is 0xa5000000 + this)
    *(volatile uint32_t*)0xa05f8054 = horiz_active_area * bpp_mode_size; // Resetting the framebuffer offset gets us 2MB VRAM back after BootROM is done with it

    // zero out framebuffer area
    for(uint32_t pixel_or_two = 0; pixel_or_two < scan_area_size_bytes; pixel_or_two += 4)
    {
      *(uint32_t*)(0xa5000000 + pixel_or_two) = 0;
    }

    // re-enable video
    *(volatile uint32_t*)0xa05f80e8 &= ~8;
    *(volatile uint32_t*)0xa05f8044 |= 1;
  }
  else // PAL (576i)
  {
    #ifdef PAL_EXTRA_LINES
      // Interlaced PAL can actually have 528 active vertical lines on the Dreamcast (48 more than NTSC!)
      // In theory a developer could hide a secret message for PAL players in this area
      vert_active_area += 48;
    #endif

    // Set registers the same way that the BootROM does
    *(volatile uint32_t*)0xa05f80e8 = 0x00160008;
    *(volatile uint32_t*)0xa05f8044 = 0x00000000 | (fbuffer_color_mode << 2);

    *(volatile uint32_t*)0xa05f804c = (horiz_active_area * bpp_mode_size) / 8; // for PVR to know active area width
    *(volatile uint32_t*)0xa05f8040 = 0x00000000; // Border color in RGB0888 format
    *(volatile uint32_t*)0xa05f805c = ((((horiz_active_area * bpp_mode_size) / 4) + 1) << 20) | (((vert_active_area / 2) - 1) << 10) | (((horiz_active_area * bpp_mode_size) / 4) - 1);
    *(volatile uint32_t*)0xa05f80ec = 0x000000ae;
    *(volatile uint32_t*)0xa05f80f0 = 0x002e002d;
    *(volatile uint32_t*)0xa05f80c8 = 0x034b0000;
    *(volatile uint32_t*)0xa05f80cc = 0x00150136;
    *(volatile uint32_t*)0xa05f80d0 = 0x00000190;
    *(volatile uint32_t*)0xa05f80d4 = 0x008d034b;
    *(volatile uint32_t*)0xa05f80d8 = 0x0270035f;
    *(volatile uint32_t*)0xa05f80dc = 0x002c026c;
    *(volatile uint32_t*)0xa05f80e0 = 0x07d6a53f;

    uint32_t scan_area_size = horiz_active_area * vert_active_area;
    uint32_t scan_area_size_bytes = scan_area_size * bpp_mode_size; // This will always be divisible by 4

    // Reset framebuffer address
    *(volatile uint32_t*)0xa05f8050 = 0x00000000; // BootROM sets this to 0x00200000 (framebuffer base is 0xa5000000 + this)
    *(volatile uint32_t*)0xa05f8054 = horiz_active_area * bpp_mode_size; // This is for interlaced, resetting the offset gets us 2MB VRAM back after BootROM is done with it

    // zero out framebuffer area
    for(uint32_t pixel_or_two = 0; pixel_or_two < scan_area_size_bytes; pixel_or_two += 4)
    {
      *(uint32_t*)(0xa5000000 + pixel_or_two) = 0;
    }

    // re-enable video
    *(volatile uint32_t*)0xa05f80e8 &= ~8;
    *(volatile uint32_t*)0xa05f8044 |= 1;
  }

  // Set global framebuffer parameters
  STARTUP_video_params.fb_width = horiz_active_area;
  STARTUP_video_params.fb_height = vert_active_area;
  STARTUP_video_params.fb_color_bytes = bpp_mode_size;
}

//==============================================================================
// Extra Video Modes
//==============================================================================
//
// These can be used to set up extra video modes after running STARTUP_Init_Video().
//
// The framebuffer address will always be set to 0xa5000000 after any of these run.
//
// Modes marked "PVR 32x32" have a framebuffer that's an integer multiple of 32x32.
//

//------------------------------------------------------------------------------
// 60Hz Modes
//------------------------------------------------------------------------------
//
// Scaled 60Hz video modes
//

// 848x480 @ 60Hz (16:9, DMT, but using a slightly-too-short hsync)
// Framebuffer: 678x480
// Horizontal scale: 0.799528302x
// DMT specifies a 3.32usec hsync width, which is 90 Dreamcast pixels when scaled
// for 848x480. The Dreamcast maxes at an hsync width of 64 pixels, which is
// 2.37usec. This may not really cause a problem (no issues with any of the 5
// different LCDs I tried), but it is worth pointing out.
void STARTUP_848x480_VGA(uint8_t fbuffer_color_mode)
{
  // Set global scale factors
  STARTUP_video_params.video_scale = 848.0f / 678.0f;
  STARTUP_video_params.video_scale_multiplier = 678.0f / 848.0f;

  // Set global video output mode parameters
  STARTUP_video_params.video_width = 848;
  STARTUP_video_params.video_height = 480;
  STARTUP_video_params.video_color_type = fbuffer_color_mode;
  STARTUP_video_params.video_refresh_rate = 60;

  // 1088 wide scaled to 870 wide (field)
  // 848 wide scaled to 678 wide (visible frame)
  uint32_t horiz_active_area = 678;
  uint32_t vert_active_area = 480;
  // {RGB0555, RGB565} = 2Bpp, {RGB888} = 3Bpp, {RGB0888} = 4Bpp
  uint32_t bpp_mode_size = fbuffer_color_mode + 1 + (0x1 ^ ((fbuffer_color_mode & 0x1) | (fbuffer_color_mode >> 1))); // Add another 1 only if 0b00

  // Set global framebuffer parameters
  STARTUP_video_params.fb_width = horiz_active_area;
  STARTUP_video_params.fb_height = vert_active_area;
  STARTUP_video_params.fb_color_bytes = bpp_mode_size;

  if(!cable_mode) // VGA 60Hz
  {
    *(volatile uint32_t*)0xa05f80e8 = 0x00160008;
    *(volatile uint32_t*)0xa05f8044 = 0x00800000 | (fbuffer_color_mode << 2);

    *(volatile uint32_t*)0xa05f804c = (horiz_active_area * bpp_mode_size) / 8; // for PVR to know active area width
    *(volatile uint32_t*)0xa05f8040 = 0x00000000; // Border color in RGB0888 format (this mode has no border)
    *(volatile uint32_t*)0xa05f805c = (1 << 20) | ((vert_active_area - 1) << 10) | (((horiz_active_area * bpp_mode_size) / 4) - 1); // progressive scan has a 1 since no lines are skipped

    *(volatile uint32_t*)0xa05f80ec = 0x000000b3; // ok: 179 ? 168, default: 0x000000a8 (horiz)
    *(volatile uint32_t*)0xa05f80f0 = 0x001f001f; // ok: 31 & 31 ? 40 & 40, default: 0x00280028 (vert)
    *(volatile uint32_t*)0xa05f80c8 = 0x03590000; // ok: 857 ? 837, default: 0x03450000 (horiz)
    *(volatile uint32_t*)0xa05f80cc = 0x001f01ff; // ok: 31, 511 ? 21, 520, default: 0x00150208 (vert)
    *(volatile uint32_t*)0xa05f80d0 = 0x00000100;
    *(volatile uint32_t*)0xa05f80d4 = 0x00b30359; // ok: 179, 857 ? 126, 837, default: 0x007e0345 (horiz)
    *(volatile uint32_t*)0xa05f80d8 = 0x02040365; // ok: 869x516 (870x517) ? 857x524 (858x525), default: 0x020c0359
    *(volatile uint32_t*)0xa05f80dc = 0x001f01ff; // ok: 31, 511 ? 40, 520, default: 0x00280208 (vert)
    *(volatile uint32_t*)0xa05f80e0 = 0x03f2583f; // ok: 15 (16,- 1), 805 (870 - 64,- 1), 8 (per DMT), 63 (64,- 1) ? 15, 793, 3, 63, default: 0x03f1933f
    // scaled hsync should be 112/1.25 = 90, but max is 64... May/may not work with everything as a result.
    // the 16 should be 22 for 89.6/4, but, since it's 64, using 16 instead.

    uint32_t scan_area_size = horiz_active_area * vert_active_area;
    uint32_t scan_area_size_bytes = scan_area_size * bpp_mode_size; // This will always be divisible by 4

    // Reset framebuffer address
    *(volatile uint32_t*)0xa05f8050 = 0x00000000; // BootROM sets this to 0x00200000 (framebuffer base is 0xa5000000 + this)
    *(volatile uint32_t*)0xa05f8054 = 0x00000000; // Same for progressive, resetting the offset gets us 2MB VRAM back after BootROM is done with it

    // zero out framebuffer area
    for(uint32_t pixel_or_two = 0; pixel_or_two < scan_area_size_bytes; pixel_or_two += 4)
    {
      *(uint32_t*)(0xa5000000 + pixel_or_two) = 0;
    }

    // re-enable video
    *(volatile uint32_t*)0xa05f80e8 &= ~8;
    *(volatile uint32_t*)0xa05f8044 |= 1;
  }
}

// 848x480 @ 60Hz (16:9, DMT, but using a slightly-too-short hsync) - PVR 32x32
// Framebuffer: 672x480
// Horizontal scale: 0.79245283x
// Same caveat as 848x480 VGA, but this mode has been shrunken by 6 columns for
// 32x32 framebuffer compatibility. As a result, there may be 6 total columns of
// blank pixels on the horizontal sides.
void STARTUP_848x480_VGA_PVR(uint8_t fbuffer_color_mode)
{
  // Set global scale factors
  STARTUP_video_params.video_scale = 848.0f / 672.0f;
  STARTUP_video_params.video_scale_multiplier = 672.0f / 848.0f;

  // Set global video output mode parameters
  STARTUP_video_params.video_width = 848;
  STARTUP_video_params.video_height = 480;
  STARTUP_video_params.video_color_type = fbuffer_color_mode;
  STARTUP_video_params.video_refresh_rate = 60;

  // 1088 wide scaled to 870 wide (field)
  // 848 wide scaled to 678 wide (visible frame)
  uint32_t horiz_active_area = 672;
  uint32_t vert_active_area = 480;
  // {RGB0555, RGB565} = 2Bpp, {RGB888} = 3Bpp, {RGB0888} = 4Bpp
  uint32_t bpp_mode_size = fbuffer_color_mode + 1 + (0x1 ^ ((fbuffer_color_mode & 0x1) | (fbuffer_color_mode >> 1))); // Add another 1 only if 0b00

  // Set global framebuffer parameters
  STARTUP_video_params.fb_width = horiz_active_area;
  STARTUP_video_params.fb_height = vert_active_area;
  STARTUP_video_params.fb_color_bytes = bpp_mode_size;

  if(!cable_mode) // VGA 60Hz
  {
    *(volatile uint32_t*)0xa05f80e8 = 0x00160008;
    *(volatile uint32_t*)0xa05f8044 = 0x00800000 | (fbuffer_color_mode << 2);

    *(volatile uint32_t*)0xa05f804c = (horiz_active_area * bpp_mode_size) / 8; // for PVR to know active area width
    *(volatile uint32_t*)0xa05f8040 = 0x00000000; // Border color in RGB0888 format
    *(volatile uint32_t*)0xa05f805c = (1 << 20) | ((vert_active_area - 1) << 10) | (((horiz_active_area * bpp_mode_size) / 4) - 1); // progressive scan has a 1 since no lines are skipped

    *(volatile uint32_t*)0xa05f80ec = 0x000000b6; // ok: 182 ? 168, default: 0x000000a8 (horiz)
    *(volatile uint32_t*)0xa05f80f0 = 0x001f001f; // ok: 31 & 31 ? 40 & 40, default: 0x00280028 (vert)
    *(volatile uint32_t*)0xa05f80c8 = 0x03590000; // ok: 857 ? 837, default: 0x03450000 (horiz)
    *(volatile uint32_t*)0xa05f80cc = 0x001f01ff; // ok: 31, 511 ? 21, 520, default: 0x00150208 (vert)
    *(volatile uint32_t*)0xa05f80d0 = 0x00000100;
    *(volatile uint32_t*)0xa05f80d4 = 0x00b30359; // ok: 179, 857 ? 126, 837, default: 0x007e0345 (horiz)
    *(volatile uint32_t*)0xa05f80d8 = 0x02040365; // ok: 869x516 (870x517) ? 857x524 (858x525), default: 0x020c0359
    *(volatile uint32_t*)0xa05f80dc = 0x001f01ff; // ok: 31, 511 ? 40, 520, default: 0x00280208 (vert)
    *(volatile uint32_t*)0xa05f80e0 = 0x03f2583f; // ok: 15 (16,- 1), 805 (870 - 64,- 1), 8 (per DMT), 63 (64,- 1) ? 15, 793, 3, 63, default: 0x03f1933f
    // scaled hsync should be 112/1.25 = 90, but max is 64... May/may not work with everything as a result.
    // the 16 should be 22 for 89.6/4, but, since it's 64, using 16 instead.

    uint32_t scan_area_size = horiz_active_area * vert_active_area;
    uint32_t scan_area_size_bytes = scan_area_size * bpp_mode_size; // This will always be divisible by 4

    // Reset framebuffer address
    *(volatile uint32_t*)0xa05f8050 = 0x00000000; // BootROM sets this to 0x00200000 (framebuffer base is 0xa5000000 + this)
    *(volatile uint32_t*)0xa05f8054 = 0x00000000; // Same for progressive, resetting the offset gets us 2MB VRAM back after BootROM is done with it

    // zero out framebuffer area
    for(uint32_t pixel_or_two = 0; pixel_or_two < scan_area_size_bytes; pixel_or_two += 4)
    {
      *(uint32_t*)(0xa5000000 + pixel_or_two) = 0;
    }

    // re-enable video
    *(volatile uint32_t*)0xa05f80e8 &= ~8;
    *(volatile uint32_t*)0xa05f8044 |= 1;
  }
}

// 800x600 @ 60Hz (4:3, DMT, but using a slightly-too-short hsync)
// Framebuffer: 540x600
// Horizontal scale: 0.675x (exact)
// DMT specifies a 3.20usec hsync width, which is 86 Dreamcast pixels when scaled
// for 800x600. The Dreamcast maxes at an hsync width of 64 pixels, which is
// 2.37usec. This may not really cause a problem (no issues with any of the 5
// different LCDs I tried), but it is worth pointing out.
void STARTUP_800x600_VGA(uint8_t fbuffer_color_mode)
{
  // Set global scale factors
  STARTUP_video_params.video_scale = 800.0f / 540.0f;
  STARTUP_video_params.video_scale_multiplier = 540.0f / 800.0f;

  // Set global video output mode parameters
  STARTUP_video_params.video_width = 800;
  STARTUP_video_params.video_height = 600;
  STARTUP_video_params.video_color_type = fbuffer_color_mode;
  STARTUP_video_params.video_refresh_rate = 60;

  // 1056 wide scaled to 713 wide (field)
  // 800 wide scaled to 540 wide (visible frame)
  uint32_t horiz_active_area = 540;
  uint32_t vert_active_area = 600;
  // {RGB0555, RGB565} = 2Bpp, {RGB888} = 3Bpp, {RGB0888} = 4Bpp
  uint32_t bpp_mode_size = fbuffer_color_mode + 1 + (0x1 ^ ((fbuffer_color_mode & 0x1) | (fbuffer_color_mode >> 1))); // Add another 1 only if 0b00

  // Set global framebuffer parameters
  STARTUP_video_params.fb_width = horiz_active_area;
  STARTUP_video_params.fb_height = vert_active_area;
  STARTUP_video_params.fb_color_bytes = bpp_mode_size;

  if(!cable_mode) // VGA 60Hz
  {
    *(volatile uint32_t*)0xa05f80e8 = 0x00160008;
    *(volatile uint32_t*)0xa05f8044 = 0x00800000 | (fbuffer_color_mode << 2);

    *(volatile uint32_t*)0xa05f804c = (horiz_active_area * bpp_mode_size) / 8; // for PVR to know active area width
    *(volatile uint32_t*)0xa05f8040 = 0x00000000; // Border color in RGB0888 format (this mode has no border)
    *(volatile uint32_t*)0xa05f805c = (1 << 20) | ((vert_active_area - 1) << 10) | (((horiz_active_area * bpp_mode_size) / 4) - 1); // progressive scan has a 1 since no lines are skipped

    *(volatile uint32_t*)0xa05f80ec = 0x00000092; // ok: 146 ? 168, default: 0x000000a8 (horiz)
    *(volatile uint32_t*)0xa05f80f0 = 0x001b001b; // ok: 27 & 27 ? 40 & 40, default: 0x00280028 (vert)
    *(volatile uint32_t*)0xa05f80c8 = 0x02ae0000; // ok: 686 ? 837, default: 0x03450000 (horiz)
    *(volatile uint32_t*)0xa05f80cc = 0x001b0273; // ok: 27, 627 ? 21, 520, default: 0x00150208 (vert)
    *(volatile uint32_t*)0xa05f80d0 = 0x00000100;
    *(volatile uint32_t*)0xa05f80d4 = 0x009202ae; // ok: 146, 686 ? 126, 837, default: 0x007e0345 (horiz)
    *(volatile uint32_t*)0xa05f80d8 = 0x027302c8; // ok: 712x627 (713x628) ? 857x524 (858x525), default: 0x020c0359
    *(volatile uint32_t*)0xa05f80dc = 0x001b0273; // ok: 27, 627 ? 40, 520, default: 0x00280208 (vert)
    *(volatile uint32_t*)0xa05f80e0 = 0x03e8843f; // ok: 15 (16,- 1), 648 (713 - 64,- 1), 4 (per DMT), 63 (64,- 1) ? 15, 793, 3, 63, default: 0x03f1933f
    // scaled hsync should be 128/1.48 = 86, but max is 64... May/may not work with everything as a result.
    // the 16 should be 22 for 86.4/4, but, since it's 64, using 16 instead.

    uint32_t scan_area_size = horiz_active_area * vert_active_area;
    uint32_t scan_area_size_bytes = scan_area_size * bpp_mode_size; // This will always be divisible by 4

    // Reset framebuffer address
    *(volatile uint32_t*)0xa05f8050 = 0x00000000; // BootROM sets this to 0x00200000 (framebuffer base is 0xa5000000 + this)
    *(volatile uint32_t*)0xa05f8054 = 0x00000000; // Same for progressive, resetting the offset gets us 2MB VRAM back after BootROM is done with it

    // zero out framebuffer area
    for(uint32_t pixel_or_two = 0; pixel_or_two < scan_area_size_bytes; pixel_or_two += 4)
    {
      *(uint32_t*)(0xa5000000 + pixel_or_two) = 0;
    }

    // re-enable video
    *(volatile uint32_t*)0xa05f80e8 &= ~8;
    *(volatile uint32_t*)0xa05f8044 |= 1;
  }
}

// 800x600 @ 60Hz (4:3, DMT, but using a slightly-too-short hsync) - PVR 32x32
// Framebuffer: 544x608
// Horizontal scale: 0.68x (exact)
// Same caveat as standard 800x600 VGA regarding slightly-too-short hsync width.
// Here, the framebuffer is slightly expanded to 544x608, which is an exact
// multiple of 32x32 for Dreamcast Tile Accelerator compatibility. Note that 8
// lines in the vertical direction may simply be cut off by a monitor as the
// monitor will think this is 800x600. Likewise, 4 columns of pixels may be cut
// off. No idea which side; horizontally it may even be 2 left and 2 right
// depending on the monitor.
void STARTUP_800x600_VGA_PVR(uint8_t fbuffer_color_mode)
{
  // Set global scale factors
  STARTUP_video_params.video_scale = 800.0f / 544.0f;
  STARTUP_video_params.video_scale_multiplier = 544.0f / 800.0f;

  // Set global video output mode parameters
  STARTUP_video_params.video_width = 800;
  STARTUP_video_params.video_height = 600;
  STARTUP_video_params.video_color_type = fbuffer_color_mode;
  STARTUP_video_params.video_refresh_rate = 60;

  // 1056 wide scaled to 713 wide (field)
  // 800 wide scaled to 540 wide (visible frame)
  uint32_t horiz_active_area = 544;
  uint32_t vert_active_area = 608;
  // {RGB0555, RGB565} = 2Bpp, {RGB888} = 3Bpp, {RGB0888} = 4Bpp
  uint32_t bpp_mode_size = fbuffer_color_mode + 1 + (0x1 ^ ((fbuffer_color_mode & 0x1) | (fbuffer_color_mode >> 1))); // Add another 1 only if 0b00

  // Set global framebuffer parameters
  STARTUP_video_params.fb_width = horiz_active_area;
  STARTUP_video_params.fb_height = vert_active_area;
  STARTUP_video_params.fb_color_bytes = bpp_mode_size;

  if(!cable_mode) // VGA 60Hz
  {
    *(volatile uint32_t*)0xa05f80e8 = 0x00160008;
    *(volatile uint32_t*)0xa05f8044 = 0x00800000 | (fbuffer_color_mode << 2);

    *(volatile uint32_t*)0xa05f804c = (horiz_active_area * bpp_mode_size) / 8; // for PVR to know active area width
    *(volatile uint32_t*)0xa05f8040 = 0x00000000; // Border color in RGB0888 format (this mode has no border)
    *(volatile uint32_t*)0xa05f805c = (1 << 20) | ((vert_active_area - 1) << 10) | (((horiz_active_area * bpp_mode_size) / 4) - 1); // progressive scan has a 1 since no lines are skipped

    *(volatile uint32_t*)0xa05f80ec = 0x00000090; // ok: 144 ? 168, default: 0x000000a8 (horiz)
    *(volatile uint32_t*)0xa05f80f0 = 0x00130013; // ok: 19 & 19 ? 40 & 40, default: 0x00280028 (vert)
    *(volatile uint32_t*)0xa05f80c8 = 0x02b00000; // ok: 688 ? 837, default: 0x03450000 (horiz)
    *(volatile uint32_t*)0xa05f80cc = 0x00130273; // ok: 19, 627 ? 21, 520, default: 0x00150208 (vert)
    *(volatile uint32_t*)0xa05f80d0 = 0x00000100;
    *(volatile uint32_t*)0xa05f80d4 = 0x009002b0; // ok: 144, 688 ? 126, 837, default: 0x007e0345 (horiz)
    *(volatile uint32_t*)0xa05f80d8 = 0x027302c8; // ok: 712x627 (713x628) ? 857x524 (858x525), default: 0x020c0359
    *(volatile uint32_t*)0xa05f80dc = 0x00130273; // ok: 19, 627 ? 40, 520, default: 0x00280208 (vert)
    *(volatile uint32_t*)0xa05f80e0 = 0x03e8843f; // ok: 15 (16,- 1), 648 (713 - 64,- 1), 4 (per DMT), 63 (64,- 1) ? 15, 793, 3, 63, default: 0x03f1933f
    // scaled hsync should be 128/1.48 = 86, but max is 64... May/may not work with everything as a result.
    // the 16 should be 22 for 86.4/4, but, since it's 64, using 16 instead.

    uint32_t scan_area_size = horiz_active_area * vert_active_area;
    uint32_t scan_area_size_bytes = scan_area_size * bpp_mode_size; // This will always be divisible by 4

    // Reset framebuffer address
    *(volatile uint32_t*)0xa05f8050 = 0x00000000; // BootROM sets this to 0x00200000 (framebuffer base is 0xa5000000 + this)
    *(volatile uint32_t*)0xa05f8054 = 0x00000000; // Same for progressive, resetting the offset gets us 2MB VRAM back after BootROM is done with it

    // zero out framebuffer area
    for(uint32_t pixel_or_two = 0; pixel_or_two < scan_area_size_bytes; pixel_or_two += 4)
    {
      *(uint32_t*)(0xa5000000 + pixel_or_two) = 0;
    }

    // re-enable video
    *(volatile uint32_t*)0xa05f80e8 &= ~8;
    *(volatile uint32_t*)0xa05f8044 |= 1;
  }
}

// 800x600 @ 60Hz (4:3, CVT)
// Framebuffer: 565x600
// Horizontal scale: 0.70625x (exact)
// This won't work with all monitors due to Dreamcast sync polarity: Dreamcast
// sync polarity has to be -h -v or the video DAC won't work, while CVT asks for
// -h +v polarity. Some monitors don't care, some do; those that do may think
// this signal is 1440x576. This is why most of these modes are following DMT
// standards, since DMT modes can be either -h -v or +h +v and it doesn't really
// matter which.
void STARTUP_800x600_VGA_CVT(uint8_t fbuffer_color_mode)
{
  // Set global scale factors
  STARTUP_video_params.video_scale = 800.0f / 565.0f;
  STARTUP_video_params.video_scale_multiplier = 565.0f / 800.0f;

  // Set global video output mode parameters
  STARTUP_video_params.video_width = 800;
  STARTUP_video_params.video_height = 600;
  STARTUP_video_params.video_color_type = fbuffer_color_mode;
  STARTUP_video_params.video_refresh_rate = 60;

  // 1024 wide scaled to 723 wide (field)
  // 800 wide scaled to 565 wide (visible frame)
  uint32_t horiz_active_area = 565;
  uint32_t vert_active_area = 600;
  // {RGB0555, RGB565} = 2Bpp, {RGB888} = 3Bpp, {RGB0888} = 4Bpp
  uint32_t bpp_mode_size = fbuffer_color_mode + 1 + (0x1 ^ ((fbuffer_color_mode & 0x1) | (fbuffer_color_mode >> 1))); // Add another 1 only if 0b00

  // Set global framebuffer parameters
  STARTUP_video_params.fb_width = horiz_active_area;
  STARTUP_video_params.fb_height = vert_active_area;
  STARTUP_video_params.fb_color_bytes = bpp_mode_size;

  if(!cable_mode) // VGA 60Hz
  {
    *(volatile uint32_t*)0xa05f80e8 = 0x00160008;
    *(volatile uint32_t*)0xa05f8044 = 0x00800000 | (fbuffer_color_mode << 2);

    *(volatile uint32_t*)0xa05f804c = (horiz_active_area * bpp_mode_size) / 8; // for PVR to know active area width
    *(volatile uint32_t*)0xa05f8040 = 0x00000000; // Border color in RGB0888 format (this mode has no border)
    *(volatile uint32_t*)0xa05f805c = (1 << 20) | ((vert_active_area - 1) << 10) | (((horiz_active_area * bpp_mode_size) / 4) - 1); // progressive scan has a 1 since no lines are skipped

    *(volatile uint32_t*)0xa05f80ec = 0x00000087; // ok: 135 ? 168, default: 0x000000a8 (horiz)
    *(volatile uint32_t*)0xa05f80f0 = 0x00150015; // ok: 21 & 21 ? 40 & 40, default: 0x00280028 (vert)
    *(volatile uint32_t*)0xa05f80c8 = 0x02bc0000; // ok: 700 ? 837, default: 0x03450000 (horiz)
    *(volatile uint32_t*)0xa05f80cc = 0x0015026d; // ok: 21, 621 ? 21, 520, default: 0x00150208 (vert)
    *(volatile uint32_t*)0xa05f80d0 = 0x00000100;
    *(volatile uint32_t*)0xa05f80d4 = 0x008702bc; // ok: 135, 700 ? 126, 837, default: 0x007e0345 (horiz)
    *(volatile uint32_t*)0xa05f80d8 = 0x026f02d2; // ok: 722x623 (723x624) ? 857x524 (858x525), default: 0x020c0359
    *(volatile uint32_t*)0xa05f80dc = 0x0015026d; // ok: 21, 621 ? 40, 520, default: 0x00280208 (vert)
    *(volatile uint32_t*)0xa05f80e0 = 0x0369a437; // ok: 13 (14,- 1), 666 (723 - 56,- 1), 4 (4:3), 55 (56,- 1) ? 15, 793, 3, 63, default: 0x03f1933f

    uint32_t scan_area_size = horiz_active_area * vert_active_area;
    uint32_t scan_area_size_bytes = scan_area_size * bpp_mode_size; // This will always be divisible by 4

    // Reset framebuffer address
    *(volatile uint32_t*)0xa05f8050 = 0x00000000; // BootROM sets this to 0x00200000 (framebuffer base is 0xa5000000 + this)
    *(volatile uint32_t*)0xa05f8054 = 0x00000000; // Same for progressive, resetting the offset gets us 2MB VRAM back after BootROM is done with it

    // zero out framebuffer area
    for(uint32_t pixel_or_two = 0; pixel_or_two < scan_area_size_bytes; pixel_or_two += 4)
    {
      *(uint32_t*)(0xa5000000 + pixel_or_two) = 0;
    }

    // re-enable video
    *(volatile uint32_t*)0xa05f80e8 &= ~8;
    *(volatile uint32_t*)0xa05f8044 |= 1;
  }
}

// 800x600 @ 60Hz (4:3, CVT) - PVR 32x32
// Framebuffer: 544x608
// Horizontal scale: 0.68x (exact)
// Same caveat as 800x600 VGA CVT.
// This mode has been shrunken by 11 columns and expanded by 8 rows for 32x32
// framebuffer compatibility. As a result, there may be 11 total columns of blank
// pixels on the horizontal sides, and 8 total rows of pixels cut off on the
// vertical sides.
void STARTUP_800x600_VGA_CVT_PVR(uint8_t fbuffer_color_mode)
{
  // Set global scale factors
  STARTUP_video_params.video_scale = 800.0f / 544.0f;
  STARTUP_video_params.video_scale_multiplier = 544.0f / 800.0f;

  // Set global video output mode parameters
  STARTUP_video_params.video_width = 800;
  STARTUP_video_params.video_height = 600;
  STARTUP_video_params.video_color_type = fbuffer_color_mode;
  STARTUP_video_params.video_refresh_rate = 60;

  // 1024 wide scaled to 723 wide (field)
  // 800 wide scaled to 565 wide (visible frame)
  uint32_t horiz_active_area = 544;
  uint32_t vert_active_area = 608;
  // {RGB0555, RGB565} = 2Bpp, {RGB888} = 3Bpp, {RGB0888} = 4Bpp
  uint32_t bpp_mode_size = fbuffer_color_mode + 1 + (0x1 ^ ((fbuffer_color_mode & 0x1) | (fbuffer_color_mode >> 1))); // Add another 1 only if 0b00

  // Set global framebuffer parameters
  STARTUP_video_params.fb_width = horiz_active_area;
  STARTUP_video_params.fb_height = vert_active_area;
  STARTUP_video_params.fb_color_bytes = bpp_mode_size;

  if(!cable_mode) // VGA 60Hz
  {
    *(volatile uint32_t*)0xa05f80e8 = 0x00160008;
    *(volatile uint32_t*)0xa05f8044 = 0x00800000 | (fbuffer_color_mode << 2);

    *(volatile uint32_t*)0xa05f804c = (horiz_active_area * bpp_mode_size) / 8; // for PVR to know active area width
    *(volatile uint32_t*)0xa05f8040 = 0x00000000; // Border color in RGB0888 format
    *(volatile uint32_t*)0xa05f805c = (1 << 20) | ((vert_active_area - 1) << 10) | (((horiz_active_area * bpp_mode_size) / 4) - 1); // progressive scan has a 1 since no lines are skipped

    // Push right by 11 (back border), so front porch side left by 0.. Which centers the image.
    // Well, alright then.
    *(volatile uint32_t*)0xa05f80ec = 0x00000092; // ok: 146 ? 168, default: 0x000000a8 (horiz)
    *(volatile uint32_t*)0xa05f80f0 = 0x000d000d; // ok: 13 & 13 ? 40 & 40, default: 0x00280028 (vert)
    *(volatile uint32_t*)0xa05f80c8 = 0x02bc0000; // ok: 700 ? 837, default: 0x03450000 (horiz)
    *(volatile uint32_t*)0xa05f80cc = 0x000d026d; // ok: 13, 621 ? 21, 520, default: 0x00150208 (vert)
    *(volatile uint32_t*)0xa05f80d0 = 0x00000100;
    *(volatile uint32_t*)0xa05f80d4 = 0x008702bc; // ok: 135, 700 ? 126, 837, default: 0x007e0345 (horiz)
    *(volatile uint32_t*)0xa05f80d8 = 0x026f02d2; // ok: 722x623 (723x624) ? 857x524 (858x525), default: 0x020c0359
    *(volatile uint32_t*)0xa05f80dc = 0x000d026d; // ok: 13, 621 ? 40, 520, default: 0x00280208 (vert)
    *(volatile uint32_t*)0xa05f80e0 = 0x0369a437; // ok: 13 (14,- 1), 666 (723 - 56,- 1), 4 (4:3), 55 (56,- 1) ? 15, 793, 3, 63, default: 0x03f1933f

    uint32_t scan_area_size = horiz_active_area * vert_active_area;
    uint32_t scan_area_size_bytes = scan_area_size * bpp_mode_size; // This will always be divisible by 4

    // Reset framebuffer address
    *(volatile uint32_t*)0xa05f8050 = 0x00000000; // BootROM sets this to 0x00200000 (framebuffer base is 0xa5000000 + this)
    *(volatile uint32_t*)0xa05f8054 = 0x00000000; // Same for progressive, resetting the offset gets us 2MB VRAM back after BootROM is done with it

    // zero out framebuffer area
    for(uint32_t pixel_or_two = 0; pixel_or_two < scan_area_size_bytes; pixel_or_two += 4)
    {
      *(uint32_t*)(0xa5000000 + pixel_or_two) = 0;
    }

    // re-enable video
    *(volatile uint32_t*)0xa05f80e8 &= ~8;
    *(volatile uint32_t*)0xa05f8044 |= 1;
  }
}

// 1024x768 @ 60Hz (4:3, DMT)
// Framebuffer: 425x768
// Horizontal scale: 0.415039063x
// This one actually uses negative/negative polarity, too!
void STARTUP_1024x768_VGA(uint8_t fbuffer_color_mode)
{
  // Set global scale factors
  STARTUP_video_params.video_scale = 1024.0f / 425.0f;
  STARTUP_video_params.video_scale_multiplier = 425.0f / 1024.0f;

  // Set global video output mode parameters
  STARTUP_video_params.video_width = 1024;
  STARTUP_video_params.video_height = 768;
  STARTUP_video_params.video_color_type = fbuffer_color_mode;
  STARTUP_video_params.video_refresh_rate = 60;

  // 1344 wide scaled to 558 wide (field)
  // 1024 wide scaled to 425 wide (visible frame)
  uint32_t horiz_active_area = 425;
  uint32_t vert_active_area = 768;
  // {RGB0555, RGB565} = 2Bpp, {RGB888} = 3Bpp, {RGB0888} = 4Bpp
  uint32_t bpp_mode_size = fbuffer_color_mode + 1 + (0x1 ^ ((fbuffer_color_mode & 0x1) | (fbuffer_color_mode >> 1))); // Add another 1 only if 0b00

  // Set global framebuffer parameters
  STARTUP_video_params.fb_width = horiz_active_area;
  STARTUP_video_params.fb_height = vert_active_area;
  STARTUP_video_params.fb_color_bytes = bpp_mode_size;

  if(!cable_mode) // VGA 60Hz
  {
    *(volatile uint32_t*)0xa05f80e8 = 0x00160008;
    *(volatile uint32_t*)0xa05f8044 = 0x00800000 | (fbuffer_color_mode << 2);

    *(volatile uint32_t*)0xa05f804c = (horiz_active_area * bpp_mode_size) / 8; // for PVR to know active area width
    *(volatile uint32_t*)0xa05f8040 = 0x00000000; // Border color in RGB0888 format (this mode has no border)
    *(volatile uint32_t*)0xa05f805c = (1 << 20) | ((vert_active_area - 1) << 10) | (((horiz_active_area * bpp_mode_size) / 4) - 1); // progressive scan has a 1 since no lines are skipped

    *(volatile uint32_t*)0xa05f80ec = 0x0000007b; // ok: 123 ? 168, default: 0x000000a8 (horiz)
    *(volatile uint32_t*)0xa05f80f0 = 0x00230023; // ok: 35 & 35 ? 40 & 40, default: 0x00280028 (vert)
    *(volatile uint32_t*)0xa05f80c8 = 0x02240000; // ok: 548 ? 837, default: 0x03450000 (horiz)
    *(volatile uint32_t*)0xa05f80cc = 0x00230323; // ok: 35, 803 ? 21, 520, default: 0x00150208 (vert)
    *(volatile uint32_t*)0xa05f80d0 = 0x00000100;
    *(volatile uint32_t*)0xa05f80d4 = 0x007b0224; // ok: 123, 548 ? 126, 837, default: 0x007e0345 (horiz)
    *(volatile uint32_t*)0xa05f80d8 = 0x0325022D; // ok: 557x805 (558x806) ? 857x524 (858x525), default: 0x020c0359
    *(volatile uint32_t*)0xa05f80dc = 0x00230323; // ok: 35, 803 ? 40, 520, default: 0x00280208 (vert)
    *(volatile uint32_t*)0xa05f80e0 = 0x035f5637; // ok: 13 (14,- 1), 501 (558 - 56,- 1), 6 (per DMT), 55 (56,- 1) ? 15, 793, 3, 63, default: 0x03f1933f

    uint32_t scan_area_size = horiz_active_area * vert_active_area;
    uint32_t scan_area_size_bytes = scan_area_size * bpp_mode_size; // This will always be divisible by 4

    // Reset framebuffer address
    *(volatile uint32_t*)0xa05f8050 = 0x00000000; // BootROM sets this to 0x00200000 (framebuffer base is 0xa5000000 + this)
    *(volatile uint32_t*)0xa05f8054 = 0x00000000; // Same for progressive, resetting the offset gets us 2MB VRAM back after BootROM is done with it

    // zero out framebuffer area
    for(uint32_t pixel_or_two = 0; pixel_or_two < scan_area_size_bytes; pixel_or_two += 4)
    {
      *(uint32_t*)(0xa5000000 + pixel_or_two) = 0;
    }

    // re-enable video
    *(volatile uint32_t*)0xa05f80e8 &= ~8;
    *(volatile uint32_t*)0xa05f8044 |= 1;
  }
}

// 1024x768 @ 60Hz (4:3, DMT) - PVR 32x32
// Framebuffer: 416x768
// Horizontal scale: 0.40625x (exact)
// This mode has been shrunken by 9 columns for 32x32 framebuffer compatibility.
// As a result, there may be 9 total columns of blank pixels on the horizontal
// sides.
void STARTUP_1024x768_VGA_PVR(uint8_t fbuffer_color_mode)
{
  // Set global scale factors
  STARTUP_video_params.video_scale = 1024.0f / 416.0f;
  STARTUP_video_params.video_scale_multiplier = 416.0f / 1024.0f;

  // Set global video output mode parameters
  STARTUP_video_params.video_width = 1024;
  STARTUP_video_params.video_height = 768;
  STARTUP_video_params.video_color_type = fbuffer_color_mode;
  STARTUP_video_params.video_refresh_rate = 60;

  // 1344 wide scaled to 558 wide (field)
  // 1024 wide scaled to 425 wide (visible frame)
  uint32_t horiz_active_area = 416;
  uint32_t vert_active_area = 768;
  // {RGB0555, RGB565} = 2Bpp, {RGB888} = 3Bpp, {RGB0888} = 4Bpp
  uint32_t bpp_mode_size = fbuffer_color_mode + 1 + (0x1 ^ ((fbuffer_color_mode & 0x1) | (fbuffer_color_mode >> 1))); // Add another 1 only if 0b00

  // Set global framebuffer parameters
  STARTUP_video_params.fb_width = horiz_active_area;
  STARTUP_video_params.fb_height = vert_active_area;
  STARTUP_video_params.fb_color_bytes = bpp_mode_size;

  if(!cable_mode) // VGA 60Hz
  {
    *(volatile uint32_t*)0xa05f80e8 = 0x00160008;
    *(volatile uint32_t*)0xa05f8044 = 0x00800000 | (fbuffer_color_mode << 2);

    *(volatile uint32_t*)0xa05f804c = (horiz_active_area * bpp_mode_size) / 8; // for PVR to know active area width
    *(volatile uint32_t*)0xa05f8040 = 0x00000000; // Border color in RGB0888 format
    *(volatile uint32_t*)0xa05f805c = (1 << 20) | ((vert_active_area - 1) << 10) | (((horiz_active_area * bpp_mode_size) / 4) - 1); // progressive scan has a 1 since no lines are skipped

    // Push right by 4 (back border), so front porch side left by 5 appears to actually center the image
    *(volatile uint32_t*)0xa05f80ec = 0x0000007f; // ok: 127 ? 168, default: 0x000000a8 (horiz)
    *(volatile uint32_t*)0xa05f80f0 = 0x00230023; // ok: 35 & 35 ? 40 & 40, default: 0x00280028 (vert)
    *(volatile uint32_t*)0xa05f80c8 = 0x02240000; // ok: 548 ? 837, default: 0x03450000 (horiz)
    *(volatile uint32_t*)0xa05f80cc = 0x00230323; // ok: 35, 803 ? 21, 520, default: 0x00150208 (vert)
    *(volatile uint32_t*)0xa05f80d0 = 0x00000100;
    *(volatile uint32_t*)0xa05f80d4 = 0x007b0224; // ok: 123, 548 ? 126, 837, default: 0x007e0345 (horiz)
    *(volatile uint32_t*)0xa05f80d8 = 0x0325022D; // ok: 557x805 (558x806) ? 857x524 (858x525), default: 0x020c0359
    *(volatile uint32_t*)0xa05f80dc = 0x00230323; // ok: 35, 803 ? 40, 520, default: 0x00280208 (vert)
    *(volatile uint32_t*)0xa05f80e0 = 0x035f5637; // ok: 13 (14,- 1), 501 (558 - 56,- 1), 6 (per DMT), 55 (56,- 1) ? 15, 793, 3, 63, default: 0x03f1933f

    uint32_t scan_area_size = horiz_active_area * vert_active_area;
    uint32_t scan_area_size_bytes = scan_area_size * bpp_mode_size; // This will always be divisible by 4

    // Reset framebuffer address
    *(volatile uint32_t*)0xa05f8050 = 0x00000000; // BootROM sets this to 0x00200000 (framebuffer base is 0xa5000000 + this)
    *(volatile uint32_t*)0xa05f8054 = 0x00000000; // Same for progressive, resetting the offset gets us 2MB VRAM back after BootROM is done with it

    // zero out framebuffer area
    for(uint32_t pixel_or_two = 0; pixel_or_two < scan_area_size_bytes; pixel_or_two += 4)
    {
      *(uint32_t*)(0xa5000000 + pixel_or_two) = 0;
    }

    // re-enable video
    *(volatile uint32_t*)0xa05f80e8 &= ~8;
    *(volatile uint32_t*)0xa05f8044 |= 1;
  }
}

// 1152x864 @ 60Hz (4:3, CVT)
// Framebuffer: 380x864
// Horizontal scale: 0.329861111x
void STARTUP_1152x864_VGA(uint8_t fbuffer_color_mode)
{
  // Set global scale factors
  STARTUP_video_params.video_scale = 1152.0f / 380.0f;
  STARTUP_video_params.video_scale_multiplier = 380.0f / 1152.0f;

  // Set global video output mode parameters
  STARTUP_video_params.video_width = 1152;
  STARTUP_video_params.video_height = 864;
  STARTUP_video_params.video_color_type = fbuffer_color_mode;
  STARTUP_video_params.video_refresh_rate = 60;

  // 1520 wide scaled to 502 wide (field)
  // 1152 wide scaled to 380 wide (visible frame)
  uint32_t horiz_active_area = 380;
  uint32_t vert_active_area = 864;
  // {RGB0555, RGB565} = 2Bpp, {RGB888} = 3Bpp, {RGB0888} = 4Bpp
  uint32_t bpp_mode_size = fbuffer_color_mode + 1 + (0x1 ^ ((fbuffer_color_mode & 0x1) | (fbuffer_color_mode >> 1))); // Add another 1 only if 0b00

  // Set global framebuffer parameters
  STARTUP_video_params.fb_width = horiz_active_area;
  STARTUP_video_params.fb_height = vert_active_area;
  STARTUP_video_params.fb_color_bytes = bpp_mode_size;

  if(!cable_mode) // VGA 60Hz
  {
    *(volatile uint32_t*)0xa05f80e8 = 0x00160008;
    *(volatile uint32_t*)0xa05f8044 = 0x00800000 | (fbuffer_color_mode << 2);

    *(volatile uint32_t*)0xa05f804c = (horiz_active_area * bpp_mode_size) / 8; // for PVR to know active area width
    *(volatile uint32_t*)0xa05f8040 = 0x00000000; // Border color in RGB0888 format (this mode has no border)
    *(volatile uint32_t*)0xa05f805c = (1 << 20) | ((vert_active_area - 1) << 10) | (((horiz_active_area * bpp_mode_size) / 4) - 1); // progressive scan has a 1 since no lines are skipped

    *(volatile uint32_t*)0xa05f80ec = 0x00000065; // ok: 101 ? 168, default: 0x000000a8 (horiz)
    *(volatile uint32_t*)0xa05f80f0 = 0x001e001e; // ok: 30 & 30 ? 40 & 40, default: 0x00280028 (vert)
    *(volatile uint32_t*)0xa05f80c8 = 0x01e10000; // ok: 481 ? 837, default: 0x03450000 (horiz)
    *(volatile uint32_t*)0xa05f80cc = 0x001e037e; // ok: 30, 894 ? 21, 520, default: 0x00150208 (vert)
    *(volatile uint32_t*)0xa05f80d0 = 0x00000100;
    *(volatile uint32_t*)0xa05f80d4 = 0x006501e1; // ok: 101, 481 ? 126, 837, default: 0x007e0345 (horiz)
    *(volatile uint32_t*)0xa05f80d8 = 0x038001f5; // ok: 501x896 (502x897) ? 857x524 (858x525), default: 0x020c0359
    *(volatile uint32_t*)0xa05f80dc = 0x001e037e; // ok: 30, 894 ? 40, 520, default: 0x00280208 (vert)
    *(volatile uint32_t*)0xa05f80e0 = 0x025cd427; // ok: 9 (10,- 1), 461 (502 - 40,- 1), 4 (4:3), 39 (40,- 1) ? 15, 793, 3, 63, default: 0x03f1933f

    uint32_t scan_area_size = horiz_active_area * vert_active_area;
    uint32_t scan_area_size_bytes = scan_area_size * bpp_mode_size; // This will always be divisible by 4

    // Reset framebuffer address
    *(volatile uint32_t*)0xa05f8050 = 0x00000000; // BootROM sets this to 0x00200000 (framebuffer base is 0xa5000000 + this)
    *(volatile uint32_t*)0xa05f8054 = 0x00000000; // Same for progressive, resetting the offset gets us 2MB VRAM back after BootROM is done with it

    // zero out framebuffer area
    for(uint32_t pixel_or_two = 0; pixel_or_two < scan_area_size_bytes; pixel_or_two += 4)
    {
      *(uint32_t*)(0xa5000000 + pixel_or_two) = 0;
    }

    // re-enable video
    *(volatile uint32_t*)0xa05f80e8 &= ~8;
    *(volatile uint32_t*)0xa05f8044 |= 1;
  }
}

// 1152x864 @ 60Hz (4:3, CVT) - PVR 32x32
// Framebuffer: 384x864
// Horizontal scale: 0.333333333x (1/3x)
// This mode has been expanded by 4 columns for 32x32 framebuffer compatibility.
// As a result, there may be 4 columns of pixels cut off (2 on either side or 4
// on one side).
void STARTUP_1152x864_VGA_PVR(uint8_t fbuffer_color_mode)
{
  // Set global scale factors
  STARTUP_video_params.video_scale = 1152.0f / 384.0f;
  STARTUP_video_params.video_scale_multiplier = 384.0f / 1152.0f;

  // Set global video output mode parameters
  STARTUP_video_params.video_width = 1152;
  STARTUP_video_params.video_height = 864;
  STARTUP_video_params.video_color_type = fbuffer_color_mode;
  STARTUP_video_params.video_refresh_rate = 60;

  // 1520 wide scaled to 502 wide (field)
  // 1152 wide scaled to 380 wide (visible frame)
  uint32_t horiz_active_area = 384;
  uint32_t vert_active_area = 864;
  // {RGB0555, RGB565} = 2Bpp, {RGB888} = 3Bpp, {RGB0888} = 4Bpp
  uint32_t bpp_mode_size = fbuffer_color_mode + 1 + (0x1 ^ ((fbuffer_color_mode & 0x1) | (fbuffer_color_mode >> 1))); // Add another 1 only if 0b00

  // Set global framebuffer parameters
  STARTUP_video_params.fb_width = horiz_active_area;
  STARTUP_video_params.fb_height = vert_active_area;
  STARTUP_video_params.fb_color_bytes = bpp_mode_size;

  if(!cable_mode) // VGA 60Hz
  {
    *(volatile uint32_t*)0xa05f80e8 = 0x00160008;
    *(volatile uint32_t*)0xa05f8044 = 0x00800000 | (fbuffer_color_mode << 2);

    *(volatile uint32_t*)0xa05f804c = (horiz_active_area * bpp_mode_size) / 8; // for PVR to know active area width
    *(volatile uint32_t*)0xa05f8040 = 0x00000000; // Border color in RGB0888 format (this mode has no border)
    *(volatile uint32_t*)0xa05f805c = (1 << 20) | ((vert_active_area - 1) << 10) | (((horiz_active_area * bpp_mode_size) / 4) - 1); // progressive scan has a 1 since no lines are skipped

    *(volatile uint32_t*)0xa05f80ec = 0x00000063; // ok: 99 ? 168, default: 0x000000a8 (horiz)
    *(volatile uint32_t*)0xa05f80f0 = 0x001e001e; // ok: 30 & 30 ? 40 & 40, default: 0x00280028 (vert)
    *(volatile uint32_t*)0xa05f80c8 = 0x01e30000; // ok: 483 ? 837, default: 0x03450000 (horiz)
    *(volatile uint32_t*)0xa05f80cc = 0x001e037e; // ok: 30, 894 ? 21, 520, default: 0x00150208 (vert)
    *(volatile uint32_t*)0xa05f80d0 = 0x00000100;
    *(volatile uint32_t*)0xa05f80d4 = 0x006301e3; // ok: 99, 483 ? 126, 837, default: 0x007e0345 (horiz)
    *(volatile uint32_t*)0xa05f80d8 = 0x038001f5; // ok: 501x896 (502x897) ? 857x524 (858x525), default: 0x020c0359
    *(volatile uint32_t*)0xa05f80dc = 0x001e037e; // ok: 30, 894 ? 40, 520, default: 0x00280208 (vert)
    *(volatile uint32_t*)0xa05f80e0 = 0x025cd427; // ok: 9 (10,- 1), 461 (502 - 40,- 1), 4 (4:3), 39 (40,- 1) ? 15, 793, 3, 63, default: 0x03f1933f

    uint32_t scan_area_size = horiz_active_area * vert_active_area;
    uint32_t scan_area_size_bytes = scan_area_size * bpp_mode_size; // This will always be divisible by 4

    // Reset framebuffer address
    *(volatile uint32_t*)0xa05f8050 = 0x00000000; // BootROM sets this to 0x00200000 (framebuffer base is 0xa5000000 + this)
    *(volatile uint32_t*)0xa05f8054 = 0x00000000; // Same for progressive, resetting the offset gets us 2MB VRAM back after BootROM is done with it

    // zero out framebuffer area
    for(uint32_t pixel_or_two = 0; pixel_or_two < scan_area_size_bytes; pixel_or_two += 4)
    {
      *(uint32_t*)(0xa5000000 + pixel_or_two) = 0;
    }

    // re-enable video
    *(volatile uint32_t*)0xa05f80e8 &= ~8;
    *(volatile uint32_t*)0xa05f8044 |= 1;
  }
}

// 720p60 (16:9, DMT & CTA-861) - for HDTVs
// Framebuffer: 465x720
// Horizontal scale: 0.36328125x (exact)
void STARTUP_720p_VGA(uint8_t fbuffer_color_mode)
{
  // Set global scale factors
  STARTUP_video_params.video_scale = 1280.0f / 465.0f;
  STARTUP_video_params.video_scale_multiplier = 465.0f / 1280.0f;

  // Set global video output mode parameters
  STARTUP_video_params.video_width = 1280;
  STARTUP_video_params.video_height = 720;
  STARTUP_video_params.video_color_type = fbuffer_color_mode;
  STARTUP_video_params.video_refresh_rate = 60;

  // 1650 wide scaled to 600 wide (field)
  // 1280 wide scaled to 465 wide (visible frame)
  uint32_t horiz_active_area = 465;
  uint32_t vert_active_area = 720;
  // {RGB0555, RGB565} = 2Bpp, {RGB888} = 3Bpp, {RGB0888} = 4Bpp
  uint32_t bpp_mode_size = fbuffer_color_mode + 1 + (0x1 ^ ((fbuffer_color_mode & 0x1) | (fbuffer_color_mode >> 1))); // Add another 1 only if 0b00

  // Set global framebuffer parameters
  STARTUP_video_params.fb_width = horiz_active_area;
  STARTUP_video_params.fb_height = vert_active_area;
  STARTUP_video_params.fb_color_bytes = bpp_mode_size;

  if(!cable_mode) // VGA 60Hz
  {
    *(volatile uint32_t*)0xa05f80e8 = 0x00160008;
    *(volatile uint32_t*)0xa05f8044 = 0x00800000 | (fbuffer_color_mode << 2);

    *(volatile uint32_t*)0xa05f804c = (horiz_active_area * bpp_mode_size) / 8; // for PVR to know active area width
    *(volatile uint32_t*)0xa05f8040 = 0x00000000; // Border color in RGB0888 format (this mode has no border)
    *(volatile uint32_t*)0xa05f805c = (1 << 20) | ((vert_active_area - 1) << 10) | (((horiz_active_area * bpp_mode_size) / 4) - 1); // progressive scan has a 1 since no lines are skipped

    *(volatile uint32_t*)0xa05f80ec = 0x0000005f; // ok: 95 ? 168, default: 0x000000a8 (horiz)
    *(volatile uint32_t*)0xa05f80f0 = 0x00190019; // ok: 25 & 25 ? 40 & 40, default: 0x00280028 (vert)
    *(volatile uint32_t*)0xa05f80c8 = 0x02300000; // ok: 560 ? 837, default: 0x03450000 (horiz)
    *(volatile uint32_t*)0xa05f80cc = 0x001902e9; // ok: 25, 745 ? 21, 520, default: 0x00150208 (vert)
    *(volatile uint32_t*)0xa05f80d0 = 0x00000100;
    *(volatile uint32_t*)0xa05f80d4 = 0x005f0230; // ok: 95, 560 ? 126, 837, default: 0x007e0345 (horiz)
    *(volatile uint32_t*)0xa05f80d8 = 0x02ed0257; // ok: 599x749 (600x750) ? 857x524 (858x525), default: 0x020c0359
    *(volatile uint32_t*)0xa05f80dc = 0x001902e9; // ok: 25, 745 ? 40, 520, default: 0x00280208 (vert)
    *(volatile uint32_t*)0xa05f80e0 = 0x00e4850e; // ok: 3 (4,- 1), 584 (600 - 15,- 1), 5 (16:9), 14 (15,- 1) ? 15, 793, 3, 63, default: 0x03f1933f

    uint32_t scan_area_size = horiz_active_area * vert_active_area;
    uint32_t scan_area_size_bytes = scan_area_size * bpp_mode_size; // This will always be divisible by 4

    // Reset framebuffer address
    *(volatile uint32_t*)0xa05f8050 = 0x00000000; // BootROM sets this to 0x00200000 (framebuffer base is 0xa5000000 + this)
    *(volatile uint32_t*)0xa05f8054 = 0x00000000; // Same for progressive, resetting the offset gets us 2MB VRAM back after BootROM is done with it

    // zero out framebuffer area
    for(uint32_t pixel_or_two = 0; pixel_or_two < scan_area_size_bytes; pixel_or_two += 4)
    {
      *(uint32_t*)(0xa5000000 + pixel_or_two) = 0;
    }

    // re-enable video
    *(volatile uint32_t*)0xa05f80e8 &= ~8;
    *(volatile uint32_t*)0xa05f8044 |= 1;
  }
}

// 720p60 (16:9, DMT & CTA-861) - for HDTVs - PVR 32x32
// Framebuffer: 448x704
// Horizontal scale: 0.35x (exact)
// This mode has been shrunken by 17 columns and 16 rows for 32x32 framebuffer
// compatibility. As a result, there may be 17 total columns of blank pixels on
// the horizontal sides and 16 rows of blank pixels on the vertical sides.
// Fun fact: The Sega Saturn's max resolution is 704x448, which is the reverse
// of this mode's framebuffer.
void STARTUP_720p_VGA_PVR(uint8_t fbuffer_color_mode)
{
  // Set global scale factors
  STARTUP_video_params.video_scale = 1280.0f / 448.0f;
  STARTUP_video_params.video_scale_multiplier = 448.0f / 1280.0f;

  // Set global video output mode parameters
  STARTUP_video_params.video_width = 1280;
  STARTUP_video_params.video_height = 720;
  STARTUP_video_params.video_color_type = fbuffer_color_mode;
  STARTUP_video_params.video_refresh_rate = 60;

  // 1650 wide scaled to 600 wide (field)
  // 1280 wide scaled to 465 wide (visible frame)
  uint32_t horiz_active_area = 448;
  uint32_t vert_active_area = 704;
  // {RGB0555, RGB565} = 2Bpp, {RGB888} = 3Bpp, {RGB0888} = 4Bpp
  uint32_t bpp_mode_size = fbuffer_color_mode + 1 + (0x1 ^ ((fbuffer_color_mode & 0x1) | (fbuffer_color_mode >> 1))); // Add another 1 only if 0b00

  // Set global framebuffer parameters
  STARTUP_video_params.fb_width = horiz_active_area;
  STARTUP_video_params.fb_height = vert_active_area;
  STARTUP_video_params.fb_color_bytes = bpp_mode_size;

  if(!cable_mode) // VGA 60Hz
  {
    *(volatile uint32_t*)0xa05f80e8 = 0x00160008;
    *(volatile uint32_t*)0xa05f8044 = 0x00800000 | (fbuffer_color_mode << 2);

    *(volatile uint32_t*)0xa05f804c = (horiz_active_area * bpp_mode_size) / 8; // for PVR to know active area width
    *(volatile uint32_t*)0xa05f8040 = 0x00000000; // Border color in RGB0888 format
    *(volatile uint32_t*)0xa05f805c = (1 << 20) | ((vert_active_area - 1) << 10) | (((horiz_active_area * bpp_mode_size) / 4) - 1); // progressive scan has a 1 since no lines are skipped

    // Push right by 8 (back border), so h front porch side left by 9 appears to actually center the image
    // Push down by 8 (back border), v front porch side up by 8 appears to actually center the image
    *(volatile uint32_t*)0xa05f80ec = 0x00000067; // ok: 103 ? 168, default: 0x000000a8 (horiz)
    *(volatile uint32_t*)0xa05f80f0 = 0x00210021; // ok: 33 & 33 ? 40 & 40, default: 0x00280028 (vert)
    *(volatile uint32_t*)0xa05f80c8 = 0x02300000; // ok: 560 ? 837, default: 0x03450000 (horiz)
    *(volatile uint32_t*)0xa05f80cc = 0x001902e9; // ok: 25, 745 ? 21, 520, default: 0x00150208 (vert)
    *(volatile uint32_t*)0xa05f80d0 = 0x00000100;
    *(volatile uint32_t*)0xa05f80d4 = 0x005f0230; // ok: 95, 560 ? 126, 837, default: 0x007e0345 (horiz)
    *(volatile uint32_t*)0xa05f80d8 = 0x02ed0257; // ok: 599x749 (600x750) ? 857x524 (858x525), default: 0x020c0359
    *(volatile uint32_t*)0xa05f80dc = 0x001902e9; // ok: 25, 745 ? 40, 520, default: 0x00280208 (vert)
    *(volatile uint32_t*)0xa05f80e0 = 0x00e4850e; // ok: 3 (4,- 1), 584 (600 - 15,- 1), 5 (16:9), 14 (15,- 1) ? 15, 793, 3, 63, default: 0x03f1933f

    uint32_t scan_area_size = horiz_active_area * vert_active_area;
    uint32_t scan_area_size_bytes = scan_area_size * bpp_mode_size; // This will always be divisible by 4

    // Reset framebuffer address
    *(volatile uint32_t*)0xa05f8050 = 0x00000000; // BootROM sets this to 0x00200000 (framebuffer base is 0xa5000000 + this)
    *(volatile uint32_t*)0xa05f8054 = 0x00000000; // Same for progressive, resetting the offset gets us 2MB VRAM back after BootROM is done with it

    // zero out framebuffer area
    for(uint32_t pixel_or_two = 0; pixel_or_two < scan_area_size_bytes; pixel_or_two += 4)
    {
      *(uint32_t*)(0xa5000000 + pixel_or_two) = 0;
    }

    // re-enable video
    *(volatile uint32_t*)0xa05f80e8 &= ~8;
    *(volatile uint32_t*)0xa05f8044 |= 1;
  }
}

// 1280x720 (16:9, CVT) - for monitors that need this instead of HDTV 720p60
// Framebuffer: 464x720
// Horizontal scale: 0.3625x (exact)
void STARTUP_1280x720_VGA(uint8_t fbuffer_color_mode)
{
  // Set global scale factors
  STARTUP_video_params.video_scale = 1280.0f / 464.0f;
  STARTUP_video_params.video_scale_multiplier = 464.0f / 1280.0f;

  // Set global video output mode parameters
  STARTUP_video_params.video_width = 1280;
  STARTUP_video_params.video_height = 720;
  STARTUP_video_params.video_color_type = fbuffer_color_mode;
  STARTUP_video_params.video_refresh_rate = 60;

  // 1664 wide scaled to 603 wide (field)
  // 1280 wide scaled to 464 wide (visible frame)
  uint32_t horiz_active_area = 464;
  uint32_t vert_active_area = 720;
  // {RGB0555, RGB565} = 2Bpp, {RGB888} = 3Bpp, {RGB0888} = 4Bpp
  uint32_t bpp_mode_size = fbuffer_color_mode + 1 + (0x1 ^ ((fbuffer_color_mode & 0x1) | (fbuffer_color_mode >> 1))); // Add another 1 only if 0b00

  // Set global framebuffer parameters
  STARTUP_video_params.fb_width = horiz_active_area;
  STARTUP_video_params.fb_height = vert_active_area;
  STARTUP_video_params.fb_color_bytes = bpp_mode_size;

  if(!cable_mode) // VGA 60Hz
  {
    *(volatile uint32_t*)0xa05f80e8 = 0x00160008;
    *(volatile uint32_t*)0xa05f8044 = 0x00800000 | (fbuffer_color_mode << 2);

    *(volatile uint32_t*)0xa05f804c = (horiz_active_area * bpp_mode_size) / 8; // for PVR to know active area width
    *(volatile uint32_t*)0xa05f8040 = 0x00000000; // Border color in RGB0888 format (this mode has no border)
    *(volatile uint32_t*)0xa05f805c = (1 << 20) | ((vert_active_area - 1) << 10) | (((horiz_active_area * bpp_mode_size) / 4) - 1); // progressive scan has a 1 since no lines are skipped

    *(volatile uint32_t*)0xa05f80ec = 0x00000074; // ok: 116 ? 168, default: 0x000000a8 (horiz)
    *(volatile uint32_t*)0xa05f80f0 = 0x00190019; // ok: 25 & 25 ? 40 & 40, default: 0x00280028 (vert)
    *(volatile uint32_t*)0xa05f80c8 = 0x02440000; // ok: 580 ? 837, default: 0x03450000 (horiz)
    *(volatile uint32_t*)0xa05f80cc = 0x001902e9; // ok: 25, 745 ? 21, 520, default: 0x00150208 (vert)
    *(volatile uint32_t*)0xa05f80d0 = 0x00000100;
    *(volatile uint32_t*)0xa05f80d4 = 0x00740244; // ok: 116, 580 ? 126, 837, default: 0x007e0345 (horiz)
    *(volatile uint32_t*)0xa05f80d8 = 0x02eb025a; // ok: 602x747 (603x748) ? 857x524 (858x525), default: 0x020c0359
    *(volatile uint32_t*)0xa05f80dc = 0x001902e9; // ok: 25, 745 ? 40, 520, default: 0x00280208 (vert)
    *(volatile uint32_t*)0xa05f80e0 = 0x02e2c52d; // ok: 11 (12,- 1), 556 (603 - 46,- 1), 5 (16:9), 45 (46,- 1) ? 15, 793, 3, 63, default: 0x03f1933f

    uint32_t scan_area_size = horiz_active_area * vert_active_area;
    uint32_t scan_area_size_bytes = scan_area_size * bpp_mode_size; // This will always be divisible by 4

    // Reset framebuffer address
    *(volatile uint32_t*)0xa05f8050 = 0x00000000; // BootROM sets this to 0x00200000 (framebuffer base is 0xa5000000 + this)
    *(volatile uint32_t*)0xa05f8054 = 0x00000000; // Same for progressive, resetting the offset gets us 2MB VRAM back after BootROM is done with it

    // zero out framebuffer area
    for(uint32_t pixel_or_two = 0; pixel_or_two < scan_area_size_bytes; pixel_or_two += 4)
    {
      *(uint32_t*)(0xa5000000 + pixel_or_two) = 0;
    }

    // re-enable video
    *(volatile uint32_t*)0xa05f80e8 &= ~8;
    *(volatile uint32_t*)0xa05f8044 |= 1;
  }
}

// 1280x720 (16:9, CVT) - for monitors that need this instead of HDTV 720p60 - PVR 32x32
// Framebuffer: 448x704
// Horizontal scale: 0.35x (exact)
// This mode has been shrunken by 16 columns and 16 rows for 32x32 framebuffer
// compatibility. As a result, there may be 16 total columns of blank pixels on
// the horizontal sides and 16 rows of blank pixels on the vertical sides.
void STARTUP_1280x720_VGA_PVR(uint8_t fbuffer_color_mode)
{
  // Set global scale factors
  STARTUP_video_params.video_scale = 1280.0f / 448.0f;
  STARTUP_video_params.video_scale_multiplier = 448.0f / 1280.0f;

  // Set global video output mode parameters
  STARTUP_video_params.video_width = 1280;
  STARTUP_video_params.video_height = 720;
  STARTUP_video_params.video_color_type = fbuffer_color_mode;
  STARTUP_video_params.video_refresh_rate = 60;

  // 1664 wide scaled to 603 wide (field)
  // 1280 wide scaled to 464 wide (visible frame)
  uint32_t horiz_active_area = 448;
  uint32_t vert_active_area = 704;
  // {RGB0555, RGB565} = 2Bpp, {RGB888} = 3Bpp, {RGB0888} = 4Bpp
  uint32_t bpp_mode_size = fbuffer_color_mode + 1 + (0x1 ^ ((fbuffer_color_mode & 0x1) | (fbuffer_color_mode >> 1))); // Add another 1 only if 0b00

  // Set global framebuffer parameters
  STARTUP_video_params.fb_width = horiz_active_area;
  STARTUP_video_params.fb_height = vert_active_area;
  STARTUP_video_params.fb_color_bytes = bpp_mode_size;

  if(!cable_mode) // VGA 60Hz
  {
    *(volatile uint32_t*)0xa05f80e8 = 0x00160008;
    *(volatile uint32_t*)0xa05f8044 = 0x00800000 | (fbuffer_color_mode << 2);

    *(volatile uint32_t*)0xa05f804c = (horiz_active_area * bpp_mode_size) / 8; // for PVR to know active area width
    *(volatile uint32_t*)0xa05f8040 = 0x00000000; // Border color in RGB0888 format
    *(volatile uint32_t*)0xa05f805c = (1 << 20) | ((vert_active_area - 1) << 10) | (((horiz_active_area * bpp_mode_size) / 4) - 1); // progressive scan has a 1 since no lines are skipped

    // Push right by 8 (back border), so h front porch side left by 8 appears to actually center the image
    // Push down by 8 (back border), v front porch side up by 8 appears to actually center the image
    *(volatile uint32_t*)0xa05f80ec = 0x0000007c; // ok: 124 ? 168, default: 0x000000a8 (horiz)
    *(volatile uint32_t*)0xa05f80f0 = 0x00210021; // ok: 33 & 33 ? 40 & 40, default: 0x00280028 (vert)
    *(volatile uint32_t*)0xa05f80c8 = 0x02440000; // ok: 580 ? 837, default: 0x03450000 (horiz)
    *(volatile uint32_t*)0xa05f80cc = 0x001902e9; // ok: 25, 745 ? 21, 520, default: 0x00150208 (vert)
    *(volatile uint32_t*)0xa05f80d0 = 0x00000100;
    *(volatile uint32_t*)0xa05f80d4 = 0x00740244; // ok: 116, 580 ? 126, 837, default: 0x007e0345 (horiz)
    *(volatile uint32_t*)0xa05f80d8 = 0x02eb025a; // ok: 602x747 (603x748) ? 857x524 (858x525), default: 0x020c0359
    *(volatile uint32_t*)0xa05f80dc = 0x001902e9; // ok: 25, 745 ? 40, 520, default: 0x00280208 (vert)
    *(volatile uint32_t*)0xa05f80e0 = 0x02e2c52d; // ok: 11 (12,- 1), 556 (603 - 46,- 1), 5 (16:9), 45 (46,- 1) ? 15, 793, 3, 63, default: 0x03f1933f

    uint32_t scan_area_size = horiz_active_area * vert_active_area;
    uint32_t scan_area_size_bytes = scan_area_size * bpp_mode_size; // This will always be divisible by 4

    // Reset framebuffer address
    *(volatile uint32_t*)0xa05f8050 = 0x00000000; // BootROM sets this to 0x00200000 (framebuffer base is 0xa5000000 + this)
    *(volatile uint32_t*)0xa05f8054 = 0x00000000; // Same for progressive, resetting the offset gets us 2MB VRAM back after BootROM is done with it

    // zero out framebuffer area
    for(uint32_t pixel_or_two = 0; pixel_or_two < scan_area_size_bytes; pixel_or_two += 4)
    {
      *(uint32_t*)(0xa5000000 + pixel_or_two) = 0;
    }

    // re-enable video
    *(volatile uint32_t*)0xa05f80e8 &= ~8;
    *(volatile uint32_t*)0xa05f8044 |= 1;
  }
}

// 1280x800 @ 60Hz (16:10, DMT & CVT)
// Framebuffer: 414x800
// Horizontal scale: 0.3234375x (exact)
void STARTUP_1280x800_VGA(uint8_t fbuffer_color_mode)
{
  // Set global scale factors
  STARTUP_video_params.video_scale = 1280.0f / 414.0f;
  STARTUP_video_params.video_scale_multiplier = 414.0f / 1280.0f;

  // Set global video output mode parameters
  STARTUP_video_params.video_width = 1280;
  STARTUP_video_params.video_height = 800;
  STARTUP_video_params.video_color_type = fbuffer_color_mode;
  STARTUP_video_params.video_refresh_rate = 60;

  // 1680 wide scaled to 543 wide (field)
  // 1280 wide scaled to 414 wide (visible frame)
  uint32_t horiz_active_area = 414;
  uint32_t vert_active_area = 800;
  // {RGB0555, RGB565} = 2Bpp, {RGB888} = 3Bpp, {RGB0888} = 4Bpp
  uint32_t bpp_mode_size = fbuffer_color_mode + 1 + (0x1 ^ ((fbuffer_color_mode & 0x1) | (fbuffer_color_mode >> 1))); // Add another 1 only if 0b00

  // Set global framebuffer parameters
  STARTUP_video_params.fb_width = horiz_active_area;
  STARTUP_video_params.fb_height = vert_active_area;
  STARTUP_video_params.fb_color_bytes = bpp_mode_size;

  if(!cable_mode) // VGA 60Hz
  {
    *(volatile uint32_t*)0xa05f80e8 = 0x00160008;
    *(volatile uint32_t*)0xa05f8044 = 0x00800000 | (fbuffer_color_mode << 2);

    *(volatile uint32_t*)0xa05f804c = (horiz_active_area * bpp_mode_size) / 8; // for PVR to know active area width
    *(volatile uint32_t*)0xa05f8040 = 0x00000000; // Border color in RGB0888 format (this mode has no border)
    *(volatile uint32_t*)0xa05f805c = (1 << 20) | ((vert_active_area - 1) << 10) | (((horiz_active_area * bpp_mode_size) / 4) - 1); // progressive scan has a 1 since no lines are skipped

    *(volatile uint32_t*)0xa05f80ec = 0x0000006a; // ok: 106 ? 168, default: 0x000000a8 (horiz)
    *(volatile uint32_t*)0xa05f80f0 = 0x001c001c; // ok: 28 & 28 ? 40 & 40, default: 0x00280028 (vert)
    *(volatile uint32_t*)0xa05f80c8 = 0x02080000; // ok: 520 ? 837, default: 0x03450000 (horiz)
    *(volatile uint32_t*)0xa05f80cc = 0x001c033c; // ok: 28, 828 ? 21, 520, default: 0x00150208 (vert)
    *(volatile uint32_t*)0xa05f80d0 = 0x00000100;
    *(volatile uint32_t*)0xa05f80d4 = 0x006a0208; // ok: 106, 520 ? 126, 837, default: 0x007e0345 (horiz)
    *(volatile uint32_t*)0xa05f80d8 = 0x033e021e; // ok: 542x830 (543x831) ? 857x524 (858x525), default: 0x020c0359
    *(volatile uint32_t*)0xa05f80dc = 0x001c033c; // ok: 28, 828 ? 40, 520, default: 0x00280208 (vert)
    *(volatile uint32_t*)0xa05f80e0 = 0x025f5628; // ok: 9 (10,- 1), 501 (543 - 41,- 1), 6 (16:10), 40 (41,- 1) ? 15, 793, 3, 63, default: 0x03f1933f

    uint32_t scan_area_size = horiz_active_area * vert_active_area;
    uint32_t scan_area_size_bytes = scan_area_size * bpp_mode_size; // This will always be divisible by 4

    // Reset framebuffer address
    *(volatile uint32_t*)0xa05f8050 = 0x00000000; // BootROM sets this to 0x00200000 (framebuffer base is 0xa5000000 + this)
    *(volatile uint32_t*)0xa05f8054 = 0x00000000; // Same for progressive, resetting the offset gets us 2MB VRAM back after BootROM is done with it

    // zero out framebuffer area
    for(uint32_t pixel_or_two = 0; pixel_or_two < scan_area_size_bytes; pixel_or_two += 4)
    {
      *(uint32_t*)(0xa5000000 + pixel_or_two) = 0;
    }

    // re-enable video
    *(volatile uint32_t*)0xa05f80e8 &= ~8;
    *(volatile uint32_t*)0xa05f8044 |= 1;
  }
}

// 1280x800 @ 60Hz (16:10, DMT & CVT) - PVR 32x32
// Framebuffer: 416x800
// Horizontal scale: 0.325x (exact)
// Here, the framebuffer is slightly expanded to 416x800, which is an exact
// multiple of 32x32 for Dreamcast Tile Accelerator compatibility. Note that it
// is possible some monitors may cut off 2 columns of pixels. No idea which side,
// it might even be 1 on each side depending on the monitor.
void STARTUP_1280x800_VGA_PVR(uint8_t fbuffer_color_mode)
{
  // Set global scale factors
  STARTUP_video_params.video_scale = 1280.0f / 416.0f;
  STARTUP_video_params.video_scale_multiplier = 416.0f / 1280.0f;

  // Set global video output mode parameters
  STARTUP_video_params.video_width = 1280;
  STARTUP_video_params.video_height = 800;
  STARTUP_video_params.video_color_type = fbuffer_color_mode;
  STARTUP_video_params.video_refresh_rate = 60;

  // 1680 wide scaled to 543 wide (field)
  // 1280 wide scaled to 414 wide (visible frame)
  uint32_t horiz_active_area = 416;
  uint32_t vert_active_area = 800;
  // {RGB0555, RGB565} = 2Bpp, {RGB888} = 3Bpp, {RGB0888} = 4Bpp
  uint32_t bpp_mode_size = fbuffer_color_mode + 1 + (0x1 ^ ((fbuffer_color_mode & 0x1) | (fbuffer_color_mode >> 1))); // Add another 1 only if 0b00

  // Set global framebuffer parameters
  STARTUP_video_params.fb_width = horiz_active_area;
  STARTUP_video_params.fb_height = vert_active_area;
  STARTUP_video_params.fb_color_bytes = bpp_mode_size;

  if(!cable_mode) // VGA 60Hz
  {
    *(volatile uint32_t*)0xa05f80e8 = 0x00160008;
    *(volatile uint32_t*)0xa05f8044 = 0x00800000 | (fbuffer_color_mode << 2);

    *(volatile uint32_t*)0xa05f804c = (horiz_active_area * bpp_mode_size) / 8; // for PVR to know active area width
    *(volatile uint32_t*)0xa05f8040 = 0x00000000; // Border color in RGB0888 format (this mode has no border)
    *(volatile uint32_t*)0xa05f805c = (1 << 20) | ((vert_active_area - 1) << 10) | (((horiz_active_area * bpp_mode_size) / 4) - 1); // progressive scan has a 1 since no lines are skipped

    *(volatile uint32_t*)0xa05f80ec = 0x00000069; // ok: 105 ? 168, default: 0x000000a8 (horiz)
    *(volatile uint32_t*)0xa05f80f0 = 0x001c001c; // ok: 28 & 28 ? 40 & 40, default: 0x00280028 (vert)
    *(volatile uint32_t*)0xa05f80c8 = 0x02090000; // ok: 521 ? 837, default: 0x03450000 (horiz)
    *(volatile uint32_t*)0xa05f80cc = 0x001c033c; // ok: 28, 828 ? 21, 520, default: 0x00150208 (vert)
    *(volatile uint32_t*)0xa05f80d0 = 0x00000100;
    *(volatile uint32_t*)0xa05f80d4 = 0x00690209; // ok: 105, 521 ? 126, 837, default: 0x007e0345 (horiz)
    *(volatile uint32_t*)0xa05f80d8 = 0x033e021e; // ok: 542x830 (543x831) ? 857x524 (858x525), default: 0x020c0359
    *(volatile uint32_t*)0xa05f80dc = 0x001c033c; // ok: 28, 828 ? 40, 520, default: 0x00280208 (vert)
    *(volatile uint32_t*)0xa05f80e0 = 0x025f5628; // ok: 9 (10,- 1), 501 (543 - 41,- 1), 6 (16:10), 40 (41,- 1) ? 15, 793, 3, 63, default: 0x03f1933f

    uint32_t scan_area_size = horiz_active_area * vert_active_area;
    uint32_t scan_area_size_bytes = scan_area_size * bpp_mode_size; // This will always be divisible by 4

    // Reset framebuffer address
    *(volatile uint32_t*)0xa05f8050 = 0x00000000; // BootROM sets this to 0x00200000 (framebuffer base is 0xa5000000 + this)
    *(volatile uint32_t*)0xa05f8054 = 0x00000000; // Same for progressive, resetting the offset gets us 2MB VRAM back after BootROM is done with it

    // zero out framebuffer area
    for(uint32_t pixel_or_two = 0; pixel_or_two < scan_area_size_bytes; pixel_or_two += 4)
    {
      *(uint32_t*)(0xa5000000 + pixel_or_two) = 0;
    }

    // re-enable video
    *(volatile uint32_t*)0xa05f80e8 &= ~8;
    *(volatile uint32_t*)0xa05f8044 |= 1;
  }
}

// 1280x960 @ 60Hz (4:3, DMT) - PVR 32x32
// Framebuffer: 320x960
// Horizontal scale: 0.25x (exact)
void STARTUP_1280x960_VGA(uint8_t fbuffer_color_mode)
{
  // Set global scale factors
  STARTUP_video_params.video_scale = 4.0f;
  STARTUP_video_params.video_scale_multiplier = 0.25f;

  // Set global video output mode parameters
  STARTUP_video_params.video_width = 1280;
  STARTUP_video_params.video_height = 960;
  STARTUP_video_params.video_color_type = fbuffer_color_mode;
  STARTUP_video_params.video_refresh_rate = 60;

  // 1800 wide scaled to 450 wide (field)
  // 1280 wide scaled to 320 wide (visible frame)
  uint32_t horiz_active_area = 320;
  uint32_t vert_active_area = 960;
  // {RGB0555, RGB565} = 2Bpp, {RGB888} = 3Bpp, {RGB0888} = 4Bpp
  uint32_t bpp_mode_size = fbuffer_color_mode + 1 + (0x1 ^ ((fbuffer_color_mode & 0x1) | (fbuffer_color_mode >> 1))); // Add another 1 only if 0b00

  // Set global framebuffer parameters
  STARTUP_video_params.fb_width = horiz_active_area;
  STARTUP_video_params.fb_height = vert_active_area;
  STARTUP_video_params.fb_color_bytes = bpp_mode_size;

  if(!cable_mode) // VGA 60Hz
  {
    *(volatile uint32_t*)0xa05f80e8 = 0x00160008;
    *(volatile uint32_t*)0xa05f8044 = 0x00800000 | (fbuffer_color_mode << 2);

    *(volatile uint32_t*)0xa05f804c = (horiz_active_area * bpp_mode_size) / 8; // for PVR to know active area width
    *(volatile uint32_t*)0xa05f8040 = 0x00000000; // Border color in RGB0888 format (this mode has no border)
    *(volatile uint32_t*)0xa05f805c = (1 << 20) | ((vert_active_area - 1) << 10) | (((horiz_active_area * bpp_mode_size) / 4) - 1); // progressive scan has a 1 since no lines are skipped

    *(volatile uint32_t*)0xa05f80ec = 0x0000006a; // ok: 106 ? 168, default: 0x000000a8 (horiz)
    *(volatile uint32_t*)0xa05f80f0 = 0x00270027; // ok: 39 & 39 ? 40 & 40, default: 0x00280028 (vert)
    *(volatile uint32_t*)0xa05f80c8 = 0x01aa0000; // ok: 426 ? 837, default: 0x03450000 (horiz)
    *(volatile uint32_t*)0xa05f80cc = 0x002703e7; // ok: 39, 999 ? 21, 520, default: 0x00150208 (vert)
    *(volatile uint32_t*)0xa05f80d0 = 0x00000100;
    *(volatile uint32_t*)0xa05f80d4 = 0x006a01aa; // ok: 106, 426 ? 126, 837, default: 0x007e0345 (horiz)
    *(volatile uint32_t*)0xa05f80d8 = 0x03e701c1; // ok: 449x999 (450x1000) ? 857x524 (858x525), default: 0x020c0359
    *(volatile uint32_t*)0xa05f80dc = 0x002703e7; // ok: 39, 999 ? 40, 520, default: 0x00280208 (vert)
    *(volatile uint32_t*)0xa05f80e0 = 0x019a531b; // ok: 6 (7,- 1), 421 (450 - 28,- 1), 3 (DMT/GTF/legacy), 27 (28,- 1) ? 15, 793, 3, 63, default: 0x03f1933f

    uint32_t scan_area_size = horiz_active_area * vert_active_area;
    uint32_t scan_area_size_bytes = scan_area_size * bpp_mode_size; // This will always be divisible by 4

    // Reset framebuffer address
    *(volatile uint32_t*)0xa05f8050 = 0x00000000; // BootROM sets this to 0x00200000 (framebuffer base is 0xa5000000 + this)
    *(volatile uint32_t*)0xa05f8054 = 0x00000000; // Same for progressive, resetting the offset gets us 2MB VRAM back after BootROM is done with it

    // zero out framebuffer area
    for(uint32_t pixel_or_two = 0; pixel_or_two < scan_area_size_bytes; pixel_or_two += 4)
    {
      *(uint32_t*)(0xa5000000 + pixel_or_two) = 0;
    }

    // re-enable video
    *(volatile uint32_t*)0xa05f80e8 &= ~8;
    *(volatile uint32_t*)0xa05f8044 |= 1;
  }
}

// 1440x900 @ 60Hz (16:10, DMT & CVT)
// Framebuffer: 365x900
// Horizontal scale: 0.253472222x
void STARTUP_1440x900_VGA(uint8_t fbuffer_color_mode)
{
  // Set global scale factors
  STARTUP_video_params.video_scale = 1440.0f / 365.0f;
  STARTUP_video_params.video_scale_multiplier = 365.0f / 1440.0f;

  // Set global video output mode parameters
  STARTUP_video_params.video_width = 1440;
  STARTUP_video_params.video_height = 900;
  STARTUP_video_params.video_color_type = fbuffer_color_mode;
  STARTUP_video_params.video_refresh_rate = 60;

  // 1904 wide scaled to 483 wide (field)
  // 1440 wide scaled to 365 wide (visible frame)
  uint32_t horiz_active_area = 365;
  uint32_t vert_active_area = 900;
  // {RGB0555, RGB565} = 2Bpp, {RGB888} = 3Bpp, {RGB0888} = 4Bpp
  uint32_t bpp_mode_size = fbuffer_color_mode + 1 + (0x1 ^ ((fbuffer_color_mode & 0x1) | (fbuffer_color_mode >> 1))); // Add another 1 only if 0b00

  // Set global framebuffer parameters
  STARTUP_video_params.fb_width = horiz_active_area;
  STARTUP_video_params.fb_height = vert_active_area;
  STARTUP_video_params.fb_color_bytes = bpp_mode_size;

  if(!cable_mode) // VGA 60Hz
  {
    *(volatile uint32_t*)0xa05f80e8 = 0x00160008;
    *(volatile uint32_t*)0xa05f8044 = 0x00800000 | (fbuffer_color_mode << 2);

    *(volatile uint32_t*)0xa05f804c = (horiz_active_area * bpp_mode_size) / 8; // for PVR to know active area width
    *(volatile uint32_t*)0xa05f8040 = 0x00000000; // Border color in RGB0888 format (this mode has no border)
    *(volatile uint32_t*)0xa05f805c = (1 << 20) | ((vert_active_area - 1) << 10) | (((horiz_active_area * bpp_mode_size) / 4) - 1); // progressive scan has a 1 since no lines are skipped

    *(volatile uint32_t*)0xa05f80ec = 0x00000062; // ok: 98 ? 168, default: 0x000000a8 (horiz)
    *(volatile uint32_t*)0xa05f80f0 = 0x001f001f; // ok: 31 & 31 ? 40 & 40, default: 0x00280028 (vert)
    *(volatile uint32_t*)0xa05f80c8 = 0x01cf0000; // ok: 463 ? 837, default: 0x03450000 (horiz)
    *(volatile uint32_t*)0xa05f80cc = 0x001f03a3; // ok: 31, 931 ? 21, 520, default: 0x00150208 (vert)
    *(volatile uint32_t*)0xa05f80d0 = 0x00000100;
    *(volatile uint32_t*)0xa05f80d4 = 0x006201cf; // ok: 98, 463 ? 126, 837, default: 0x007e0345 (horiz)
    *(volatile uint32_t*)0xa05f80d8 = 0x03a501e2; // ok: 482x933 (483x934) ? 857x524 (858x525), default: 0x020c0359
    *(volatile uint32_t*)0xa05f80dc = 0x001f03a3; // ok: 31, 931 ? 40, 520, default: 0x00280208 (vert)
    *(volatile uint32_t*)0xa05f80e0 = 0x025bc626; // ok: 9 (10,- 1), 444 (483 - 38,- 1), 6 (16:10), 38 (39,- 1) ? 15, 793, 3, 63, default: 0x03f1933f

    uint32_t scan_area_size = horiz_active_area * vert_active_area;
    uint32_t scan_area_size_bytes = scan_area_size * bpp_mode_size; // This will always be divisible by 4

    // Reset framebuffer address
    *(volatile uint32_t*)0xa05f8050 = 0x00000000; // BootROM sets this to 0x00200000 (framebuffer base is 0xa5000000 + this)
    *(volatile uint32_t*)0xa05f8054 = 0x00000000; // Same for progressive, resetting the offset gets us 2MB VRAM back after BootROM is done with it

    // zero out framebuffer area
    for(uint32_t pixel_or_two = 0; pixel_or_two < scan_area_size_bytes; pixel_or_two += 4)
    {
      *(uint32_t*)(0xa5000000 + pixel_or_two) = 0;
    }

    // re-enable video
    *(volatile uint32_t*)0xa05f80e8 &= ~8;
    *(volatile uint32_t*)0xa05f8044 |= 1;
  }
}

// 1440x900 @ 60Hz (16:10, DMT & CVT) - PVR 32x32
// Framebuffer: 352x896
// Horizontal scale: 0.244444444x
// This mode has been shrunken by 13 columns and 4 rows for 32x32 framebuffer
// compatibility. As a result, there may be 13 total columns of blank pixels on
// the horizontal sides and 4 rows of blank pixels on the vertical sides.
void STARTUP_1440x900_VGA_PVR(uint8_t fbuffer_color_mode)
{
  // Set global scale factors
  STARTUP_video_params.video_scale = 1440.0f / 352.0f;
  STARTUP_video_params.video_scale_multiplier = 352.0f / 1440.0f;

  // Set global video output mode parameters
  STARTUP_video_params.video_width = 1440;
  STARTUP_video_params.video_height = 900;
  STARTUP_video_params.video_color_type = fbuffer_color_mode;
  STARTUP_video_params.video_refresh_rate = 60;

  // 1904 wide scaled to 483 wide (field)
  // 1440 wide scaled to 365 wide (visible frame)
  uint32_t horiz_active_area = 352;
  uint32_t vert_active_area = 896;
  // {RGB0555, RGB565} = 2Bpp, {RGB888} = 3Bpp, {RGB0888} = 4Bpp
  uint32_t bpp_mode_size = fbuffer_color_mode + 1 + (0x1 ^ ((fbuffer_color_mode & 0x1) | (fbuffer_color_mode >> 1))); // Add another 1 only if 0b00

  // Set global framebuffer parameters
  STARTUP_video_params.fb_width = horiz_active_area;
  STARTUP_video_params.fb_height = vert_active_area;
  STARTUP_video_params.fb_color_bytes = bpp_mode_size;

  if(!cable_mode) // VGA 60Hz
  {
    *(volatile uint32_t*)0xa05f80e8 = 0x00160008;
    *(volatile uint32_t*)0xa05f8044 = 0x00800000 | (fbuffer_color_mode << 2);

    *(volatile uint32_t*)0xa05f804c = (horiz_active_area * bpp_mode_size) / 8; // for PVR to know active area width
    *(volatile uint32_t*)0xa05f8040 = 0x00000000; // Border color in RGB0888 format
    *(volatile uint32_t*)0xa05f805c = (1 << 20) | ((vert_active_area - 1) << 10) | (((horiz_active_area * bpp_mode_size) / 4) - 1); // progressive scan has a 1 since no lines are skipped

    // Push right by 6 (back border), so h front porch side left by 7 appears to actually center the image
    // Push down by 2 (back border), v front porch side up by 2 appears to actually center the image
    *(volatile uint32_t*)0xa05f80ec = 0x00000068; // ok: 104 ? 168, default: 0x000000a8 (horiz)
    *(volatile uint32_t*)0xa05f80f0 = 0x00210021; // ok: 33 & 33 ? 40 & 40, default: 0x00280028 (vert)
    *(volatile uint32_t*)0xa05f80c8 = 0x01cf0000; // ok: 463 ? 837, default: 0x03450000 (horiz)
    *(volatile uint32_t*)0xa05f80cc = 0x001f03a3; // ok: 31, 931 ? 21, 520, default: 0x00150208 (vert)
    *(volatile uint32_t*)0xa05f80d0 = 0x00000100;
    *(volatile uint32_t*)0xa05f80d4 = 0x006201cf; // ok: 98, 463 ? 126, 837, default: 0x007e0345 (horiz)
    *(volatile uint32_t*)0xa05f80d8 = 0x03a501e2; // ok: 482x933 (483x934) ? 857x524 (858x525), default: 0x020c0359
    *(volatile uint32_t*)0xa05f80dc = 0x001f03a3; // ok: 31, 931 ? 40, 520, default: 0x00280208 (vert)
    *(volatile uint32_t*)0xa05f80e0 = 0x025bc626; // ok: 9 (10,- 1), 444 (483 - 38,- 1), 6 (16:10), 38 (39,- 1) ? 15, 793, 3, 63, default: 0x03f1933f

    uint32_t scan_area_size = horiz_active_area * vert_active_area;
    uint32_t scan_area_size_bytes = scan_area_size * bpp_mode_size; // This will always be divisible by 4

    // Reset framebuffer address
    *(volatile uint32_t*)0xa05f8050 = 0x00000000; // BootROM sets this to 0x00200000 (framebuffer base is 0xa5000000 + this)
    *(volatile uint32_t*)0xa05f8054 = 0x00000000; // Same for progressive, resetting the offset gets us 2MB VRAM back after BootROM is done with it

    // zero out framebuffer area
    for(uint32_t pixel_or_two = 0; pixel_or_two < scan_area_size_bytes; pixel_or_two += 4)
    {
      *(uint32_t*)(0xa5000000 + pixel_or_two) = 0;
    }

    // re-enable video
    *(volatile uint32_t*)0xa05f80e8 &= ~8;
    *(volatile uint32_t*)0xa05f8044 |= 1;
  }
}

//------------------------------------------------------------------------------
// 75Hz Modes
//------------------------------------------------------------------------------
//
// Scaled 75Hz video modes
//
// These are surprisingly widely supported.
//

// 640x480 @ 75Hz (4:3, DMT)
// Framebuffer: 548x480
// Horizontal scale: 0.85625x (exact)
void STARTUP_640x480_VGA_75(uint8_t fbuffer_color_mode)
{
  // Set global scale factors
  STARTUP_video_params.video_scale = 640.0f / 548.0f;
  STARTUP_video_params.video_scale_multiplier = 548.0f / 640.0f;

  // Set global video output mode parameters
  STARTUP_video_params.video_width = 640;
  STARTUP_video_params.video_height = 480;
  STARTUP_video_params.video_color_type = fbuffer_color_mode;
  STARTUP_video_params.video_refresh_rate = 75;

  // 840 wide scaled to 720 wide (field)
  // 640 wide scaled to 548 wide (visible frame)
  uint32_t horiz_active_area = 548;
  uint32_t vert_active_area = 480;
  // {RGB0555, RGB565} = 2Bpp, {RGB888} = 3Bpp, {RGB0888} = 4Bpp
  uint32_t bpp_mode_size = fbuffer_color_mode + 1 + (0x1 ^ ((fbuffer_color_mode & 0x1) | (fbuffer_color_mode >> 1))); // Add another 1 only if 0b00

  // Set global framebuffer parameters
  STARTUP_video_params.fb_width = horiz_active_area;
  STARTUP_video_params.fb_height = vert_active_area;
  STARTUP_video_params.fb_color_bytes = bpp_mode_size;

  if(!cable_mode) // VGA 75Hz
  {
    *(volatile uint32_t*)0xa05f80e8 = 0x00160008;
    *(volatile uint32_t*)0xa05f8044 = 0x00800000 | (fbuffer_color_mode << 2);

    *(volatile uint32_t*)0xa05f804c = (horiz_active_area * bpp_mode_size) / 8; // for PVR to know active area width
    *(volatile uint32_t*)0xa05f8040 = 0x00000000; // Border color in RGB0888 format (this mode has no border)
    *(volatile uint32_t*)0xa05f805c = (1 << 20) | ((vert_active_area - 1) << 10) | (((horiz_active_area * bpp_mode_size) / 4) - 1); // progressive scan has a 1 since no lines are skipped

    *(volatile uint32_t*)0xa05f80ec = 0x0000009e; // ok: 158 ? 168, default: 0x000000a8 (horiz)
    *(volatile uint32_t*)0xa05f80f0 = 0x00130013; // ok: 19 & 19 ? 40 & 40, default: 0x00280028 (vert)
    *(volatile uint32_t*)0xa05f80c8 = 0x02c20000; // ok: 706 ? 837, default: 0x03450000 (horiz)
    *(volatile uint32_t*)0xa05f80cc = 0x001301f3; // ok: 19, 499 ? 21, 520, default: 0x00150208 (vert)
    *(volatile uint32_t*)0xa05f80d0 = 0x00000100;
    *(volatile uint32_t*)0xa05f80d4 = 0x009e02c2; // ok: 158, 706 ? 126, 837, default: 0x007e0345 (horiz)
    *(volatile uint32_t*)0xa05f80d8 = 0x01f302cf; // ok: 719x499 (720x500) ? 857x524 (858x525), default: 0x020c0359
    *(volatile uint32_t*)0xa05f80dc = 0x001301f3; // ok: 19, 499 ? 40, 520, default: 0x00280208 (vert)
    *(volatile uint32_t*)0xa05f80e0 = 0x03698336; // ok: 13 (14,- 1), 664 (720 - 55,- 1), 3 (DMT/GTF/legacy), 54 (55,- 1) ? 15, 793, 3, 63, default: 0x03f1933f

    uint32_t scan_area_size = horiz_active_area * vert_active_area;
    uint32_t scan_area_size_bytes = scan_area_size * bpp_mode_size; // This will always be divisible by 4

    // Reset framebuffer address
    *(volatile uint32_t*)0xa05f8050 = 0x00000000; // BootROM sets this to 0x00200000 (framebuffer base is 0xa5000000 + this)
    *(volatile uint32_t*)0xa05f8054 = 0x00000000; // Same for progressive, resetting the offset gets us 2MB VRAM back after BootROM is done with it

    // zero out framebuffer area
    for(uint32_t pixel_or_two = 0; pixel_or_two < scan_area_size_bytes; pixel_or_two += 4)
    {
      *(uint32_t*)(0xa5000000 + pixel_or_two) = 0;
    }

    // re-enable video
    *(volatile uint32_t*)0xa05f80e8 &= ~8;
    *(volatile uint32_t*)0xa05f8044 |= 1;
  }
}

// 640x480 @ 75Hz (4:3, DMT) - PVR 32x32
// Framebuffer: 544x480
// Horizontal scale: 0.85x (exact)
// This mode has been shrunken by 4 columns for 32x32 framebuffer compatibility.
// As a result, there may be 4 columns of blank pixels (2 on either side or 4 on
// one side).
void STARTUP_640x480_VGA_75_PVR(uint8_t fbuffer_color_mode)
{
  // Set global scale factors
  STARTUP_video_params.video_scale = 640.0f / 544.0f;
  STARTUP_video_params.video_scale_multiplier = 544.0f / 640.0f;

  // Set global video output mode parameters
  STARTUP_video_params.video_width = 640;
  STARTUP_video_params.video_height = 480;
  STARTUP_video_params.video_color_type = fbuffer_color_mode;
  STARTUP_video_params.video_refresh_rate = 75;

  // 840 wide scaled to 720 wide (field)
  // 640 wide scaled to 548 wide (visible frame)
  uint32_t horiz_active_area = 544;
  uint32_t vert_active_area = 480;
  // {RGB0555, RGB565} = 2Bpp, {RGB888} = 3Bpp, {RGB0888} = 4Bpp
  uint32_t bpp_mode_size = fbuffer_color_mode + 1 + (0x1 ^ ((fbuffer_color_mode & 0x1) | (fbuffer_color_mode >> 1))); // Add another 1 only if 0b00

  // Set global framebuffer parameters
  STARTUP_video_params.fb_width = horiz_active_area;
  STARTUP_video_params.fb_height = vert_active_area;
  STARTUP_video_params.fb_color_bytes = bpp_mode_size;

  if(!cable_mode) // VGA 75Hz
  {
    *(volatile uint32_t*)0xa05f80e8 = 0x00160008;
    *(volatile uint32_t*)0xa05f8044 = 0x00800000 | (fbuffer_color_mode << 2);

    *(volatile uint32_t*)0xa05f804c = (horiz_active_area * bpp_mode_size) / 8; // for PVR to know active area width
    *(volatile uint32_t*)0xa05f8040 = 0x00000000; // Border color in RGB0888 format
    *(volatile uint32_t*)0xa05f805c = (1 << 20) | ((vert_active_area - 1) << 10) | (((horiz_active_area * bpp_mode_size) / 4) - 1); // progressive scan has a 1 since no lines are skipped

    *(volatile uint32_t*)0xa05f80ec = 0x000000a0; // ok: 160 ? 168, default: 0x000000a8 (horiz)
    *(volatile uint32_t*)0xa05f80f0 = 0x00130013; // ok: 19 & 19 ? 40 & 40, default: 0x00280028 (vert)
    *(volatile uint32_t*)0xa05f80c8 = 0x02c20000; // ok: 706 ? 837, default: 0x03450000 (horiz)
    *(volatile uint32_t*)0xa05f80cc = 0x001301f3; // ok: 19, 499 ? 21, 520, default: 0x00150208 (vert)
    *(volatile uint32_t*)0xa05f80d0 = 0x00000100;
    *(volatile uint32_t*)0xa05f80d4 = 0x009e02c2; // ok: 158, 706 ? 126, 837, default: 0x007e0345 (horiz)
    *(volatile uint32_t*)0xa05f80d8 = 0x01f302cf; // ok: 719x499 (720x500) ? 857x524 (858x525), default: 0x020c0359
    *(volatile uint32_t*)0xa05f80dc = 0x001301f3; // ok: 19, 499 ? 40, 520, default: 0x00280208 (vert)
    *(volatile uint32_t*)0xa05f80e0 = 0x03698336; // ok: 13 (14,- 1), 664 (720 - 55,- 1), 3 (DMT/GTF/legacy), 54 (55,- 1) ? 15, 793, 3, 63, default: 0x03f1933f

    uint32_t scan_area_size = horiz_active_area * vert_active_area;
    uint32_t scan_area_size_bytes = scan_area_size * bpp_mode_size; // This will always be divisible by 4

    // Reset framebuffer address
    *(volatile uint32_t*)0xa05f8050 = 0x00000000; // BootROM sets this to 0x00200000 (framebuffer base is 0xa5000000 + this)
    *(volatile uint32_t*)0xa05f8054 = 0x00000000; // Same for progressive, resetting the offset gets us 2MB VRAM back after BootROM is done with it

    // zero out framebuffer area
    for(uint32_t pixel_or_two = 0; pixel_or_two < scan_area_size_bytes; pixel_or_two += 4)
    {
      *(uint32_t*)(0xa5000000 + pixel_or_two) = 0;
    }

    // re-enable video
    *(volatile uint32_t*)0xa05f80e8 &= ~8;
    *(volatile uint32_t*)0xa05f8044 |= 1;
  }
}

// 800x600 @ 75Hz (4:3, DMT)
// Framebuffer: 436x600
// Horizontal scale: 0.545x (exact)
void STARTUP_800x600_VGA_75(uint8_t fbuffer_color_mode)
{
  // Set global scale factors
  STARTUP_video_params.video_scale = 800.0f / 436.0f;
  STARTUP_video_params.video_scale_multiplier = 436.0f / 800.0f;

  // Set global video output mode parameters
  STARTUP_video_params.video_width = 800;
  STARTUP_video_params.video_height = 600;
  STARTUP_video_params.video_color_type = fbuffer_color_mode;
  STARTUP_video_params.video_refresh_rate = 75;

  // 1056 wide scaled to 576 wide (field)
  // 800 wide scaled to 436 wide (visible frame)
  uint32_t horiz_active_area = 436;
  uint32_t vert_active_area = 600;
  // {RGB0555, RGB565} = 2Bpp, {RGB888} = 3Bpp, {RGB0888} = 4Bpp
  uint32_t bpp_mode_size = fbuffer_color_mode + 1 + (0x1 ^ ((fbuffer_color_mode & 0x1) | (fbuffer_color_mode >> 1))); // Add another 1 only if 0b00

  // Set global framebuffer parameters
  STARTUP_video_params.fb_width = horiz_active_area;
  STARTUP_video_params.fb_height = vert_active_area;
  STARTUP_video_params.fb_color_bytes = bpp_mode_size;

  if(!cable_mode) // VGA 75Hz
  {
    *(volatile uint32_t*)0xa05f80e8 = 0x00160008;
    *(volatile uint32_t*)0xa05f8044 = 0x00800000 | (fbuffer_color_mode << 2);

    *(volatile uint32_t*)0xa05f804c = (horiz_active_area * bpp_mode_size) / 8; // for PVR to know active area width
    *(volatile uint32_t*)0xa05f8040 = 0x00000000; // Border color in RGB0888 format (this mode has no border)
    *(volatile uint32_t*)0xa05f805c = (1 << 20) | ((vert_active_area - 1) << 10) | (((horiz_active_area * bpp_mode_size) / 4) - 1); // progressive scan has a 1 since no lines are skipped

    *(volatile uint32_t*)0xa05f80ec = 0x00000083; // ok: 131 ? 168, default: 0x000000a8 (horiz)
    *(volatile uint32_t*)0xa05f80f0 = 0x00180018; // ok: 24 & 24 ? 40 & 40, default: 0x00280028 (vert)
    *(volatile uint32_t*)0xa05f80c8 = 0x02370000; // ok: 567 ? 837, default: 0x03450000 (horiz)
    *(volatile uint32_t*)0xa05f80cc = 0x00180270; // ok: 24, 624 ? 21, 520, default: 0x00150208 (vert)
    *(volatile uint32_t*)0xa05f80d0 = 0x00000100;
    *(volatile uint32_t*)0xa05f80d4 = 0x00830237; // ok: 131, 567 ? 126, 837, default: 0x007e0345 (horiz)
    *(volatile uint32_t*)0xa05f80d8 = 0x0270023f; // ok: 575x624 (576x625) ? 857x524 (858x525), default: 0x020c0359
    *(volatile uint32_t*)0xa05f80dc = 0x00180270; // ok: 24, 624 ? 40, 520, default: 0x00280208 (vert)
    *(volatile uint32_t*)0xa05f80e0 = 0x02a1332b; // ok: 10 (11,- 1), 531 (576 - 44,- 1), 3 (DMT/GTF/legacy), 43 (44,- 1) ? 15, 793, 3, 63, default: 0x03f1933f

    uint32_t scan_area_size = horiz_active_area * vert_active_area;
    uint32_t scan_area_size_bytes = scan_area_size * bpp_mode_size; // This will always be divisible by 4

    // Reset framebuffer address
    *(volatile uint32_t*)0xa05f8050 = 0x00000000; // BootROM sets this to 0x00200000 (framebuffer base is 0xa5000000 + this)
    *(volatile uint32_t*)0xa05f8054 = 0x00000000; // Same for progressive, resetting the offset gets us 2MB VRAM back after BootROM is done with it

    // zero out framebuffer area
    for(uint32_t pixel_or_two = 0; pixel_or_two < scan_area_size_bytes; pixel_or_two += 4)
    {
      *(uint32_t*)(0xa5000000 + pixel_or_two) = 0;
    }

    // re-enable video
    *(volatile uint32_t*)0xa05f80e8 &= ~8;
    *(volatile uint32_t*)0xa05f8044 |= 1;
  }
}

// 800x600 @ 75Hz (4:3, DMT) - PVR 32x32
// Framebuffer: 416x608
// Horizontal scale: 0.52x (exact)
// This mode has been shrunken by 20 columns and expanded by 8 rows for 32x32
// framebuffer compatibility. As a result, there may be 20 total columns of blank
// pixels on the horizontal sides, and 8 total rows of pixels cut off on the
// vertical sides.
void STARTUP_800x600_VGA_75_PVR(uint8_t fbuffer_color_mode)
{
  // Set global scale factors
  STARTUP_video_params.video_scale = 800.0f / 416.0f;
  STARTUP_video_params.video_scale_multiplier = 416.0f / 800.0f;

  // Set global video output mode parameters
  STARTUP_video_params.video_width = 800;
  STARTUP_video_params.video_height = 600;
  STARTUP_video_params.video_color_type = fbuffer_color_mode;
  STARTUP_video_params.video_refresh_rate = 75;

  // 1056 wide scaled to 576 wide (field)
  // 800 wide scaled to 436 wide (visible frame)
  uint32_t horiz_active_area = 416;
  uint32_t vert_active_area = 608;
  // {RGB0555, RGB565} = 2Bpp, {RGB888} = 3Bpp, {RGB0888} = 4Bpp
  uint32_t bpp_mode_size = fbuffer_color_mode + 1 + (0x1 ^ ((fbuffer_color_mode & 0x1) | (fbuffer_color_mode >> 1))); // Add another 1 only if 0b00

  // Set global framebuffer parameters
  STARTUP_video_params.fb_width = horiz_active_area;
  STARTUP_video_params.fb_height = vert_active_area;
  STARTUP_video_params.fb_color_bytes = bpp_mode_size;

  if(!cable_mode) // VGA 75Hz
  {
    *(volatile uint32_t*)0xa05f80e8 = 0x00160008;
    *(volatile uint32_t*)0xa05f8044 = 0x00800000 | (fbuffer_color_mode << 2);

    *(volatile uint32_t*)0xa05f804c = (horiz_active_area * bpp_mode_size) / 8; // for PVR to know active area width
    *(volatile uint32_t*)0xa05f8040 = 0x00000000; // Border color in RGB0888 format
    *(volatile uint32_t*)0xa05f805c = (1 << 20) | ((vert_active_area - 1) << 10) | (((horiz_active_area * bpp_mode_size) / 4) - 1); // progressive scan has a 1 since no lines are skipped

    // Push right by 10 (back border), so front porch side left by 10 appears to actually center the image
    *(volatile uint32_t*)0xa05f80ec = 0x0000008d; // ok: 141 ? 168, default: 0x000000a8 (horiz)
    *(volatile uint32_t*)0xa05f80f0 = 0x00100010; // ok: 16 & 16 ? 40 & 40, default: 0x00280028 (vert)
    *(volatile uint32_t*)0xa05f80c8 = 0x02370000; // ok: 567 ? 837, default: 0x03450000 (horiz)
    *(volatile uint32_t*)0xa05f80cc = 0x00100270; // ok: 16, 624 ? 21, 520, default: 0x00150208 (vert)
    *(volatile uint32_t*)0xa05f80d0 = 0x00000100;
    *(volatile uint32_t*)0xa05f80d4 = 0x00830237; // ok: 131, 567 ? 126, 837, default: 0x007e0345 (horiz)
    *(volatile uint32_t*)0xa05f80d8 = 0x0270023f; // ok: 575x624 (576x625) ? 857x524 (858x525), default: 0x020c0359
    *(volatile uint32_t*)0xa05f80dc = 0x00100270; // ok: 16, 624 ? 40, 520, default: 0x00280208 (vert)
    *(volatile uint32_t*)0xa05f80e0 = 0x02a1332b; // ok: 10 (11,- 1), 531 (576 - 44,- 1), 3 (DMT/GTF/legacy), 43 (44,- 1) ? 15, 793, 3, 63, default: 0x03f1933f

    uint32_t scan_area_size = horiz_active_area * vert_active_area;
    uint32_t scan_area_size_bytes = scan_area_size * bpp_mode_size; // This will always be divisible by 4

    // Reset framebuffer address
    *(volatile uint32_t*)0xa05f8050 = 0x00000000; // BootROM sets this to 0x00200000 (framebuffer base is 0xa5000000 + this)
    *(volatile uint32_t*)0xa05f8054 = 0x00000000; // Same for progressive, resetting the offset gets us 2MB VRAM back after BootROM is done with it

    // zero out framebuffer area
    for(uint32_t pixel_or_two = 0; pixel_or_two < scan_area_size_bytes; pixel_or_two += 4)
    {
      *(uint32_t*)(0xa5000000 + pixel_or_two) = 0;
    }

    // re-enable video
    *(volatile uint32_t*)0xa05f80e8 &= ~8;
    *(volatile uint32_t*)0xa05f8044 |= 1;
  }
}

// 1024x768 @ 75Hz (4:3, DMT) - PVR 32x32
// Framebuffer: 352x768
// Horizontal scale: 0.34375x (exact)
void STARTUP_1024x768_VGA_75(uint8_t fbuffer_color_mode)
{
  // Set global scale factors
  STARTUP_video_params.video_scale = 1024.0f / 352.0f;
  STARTUP_video_params.video_scale_multiplier = 352.0f / 1024.0f;

  // Set global video output mode parameters
  STARTUP_video_params.video_width = 1024;
  STARTUP_video_params.video_height = 768;
  STARTUP_video_params.video_color_type = fbuffer_color_mode;
  STARTUP_video_params.video_refresh_rate = 75;

  // 1312 wide scaled to 450 wide (field)
  // 1024 wide scaled to 352 wide (visible frame)
  uint32_t horiz_active_area = 352;
  uint32_t vert_active_area = 768;
  // {RGB0555, RGB565} = 2Bpp, {RGB888} = 3Bpp, {RGB0888} = 4Bpp
  uint32_t bpp_mode_size = fbuffer_color_mode + 1 + (0x1 ^ ((fbuffer_color_mode & 0x1) | (fbuffer_color_mode >> 1))); // Add another 1 only if 0b00

  // Set global framebuffer parameters
  STARTUP_video_params.fb_width = horiz_active_area;
  STARTUP_video_params.fb_height = vert_active_area;
  STARTUP_video_params.fb_color_bytes = bpp_mode_size;

  if(!cable_mode) // VGA 75Hz
  {
    *(volatile uint32_t*)0xa05f80e8 = 0x00160008;
    *(volatile uint32_t*)0xa05f8044 = 0x00800000 | (fbuffer_color_mode << 2);

    *(volatile uint32_t*)0xa05f804c = (horiz_active_area * bpp_mode_size) / 8; // for PVR to know active area width
    *(volatile uint32_t*)0xa05f8040 = 0x00000000; // Border color in RGB0888 format (this mode has no border)
    *(volatile uint32_t*)0xa05f805c = (1 << 20) | ((vert_active_area - 1) << 10) | (((horiz_active_area * bpp_mode_size) / 4) - 1); // progressive scan has a 1 since no lines are skipped

    *(volatile uint32_t*)0xa05f80ec = 0x0000005d; // ok: 93 ? 168, default: 0x000000a8 (horiz)
    *(volatile uint32_t*)0xa05f80f0 = 0x001f001f; // ok: 31 & 31 ? 40 & 40, default: 0x00280028 (vert)
    *(volatile uint32_t*)0xa05f80c8 = 0x01bd0000; // ok: 445 ? 837, default: 0x03450000 (horiz)
    *(volatile uint32_t*)0xa05f80cc = 0x001f031f; // ok: 31, 799 ? 21, 520, default: 0x00150208 (vert)
    *(volatile uint32_t*)0xa05f80d0 = 0x00000100;
    *(volatile uint32_t*)0xa05f80d4 = 0x005d01bd; // ok: 93, 445 ? 126, 837, default: 0x007e0345 (horiz)
    *(volatile uint32_t*)0xa05f80d8 = 0x031f01c1; // ok: 449x799 (450x800) ? 857x524 (858x525), default: 0x020c0359
    *(volatile uint32_t*)0xa05f80dc = 0x001f031f; // ok: 31, 799 ? 40, 520, default: 0x00280208 (vert)
    *(volatile uint32_t*)0xa05f80e0 = 0x01da0320; // ok: 7 (8,- 1), 416 (450 - 33,- 1), 3 (DMT/GTF/legacy), 32 (33,- 1) ? 15, 793, 3, 63, default: 0x03f1933f

    uint32_t scan_area_size = horiz_active_area * vert_active_area;
    uint32_t scan_area_size_bytes = scan_area_size * bpp_mode_size; // This will always be divisible by 4

    // Reset framebuffer address
    *(volatile uint32_t*)0xa05f8050 = 0x00000000; // BootROM sets this to 0x00200000 (framebuffer base is 0xa5000000 + this)
    *(volatile uint32_t*)0xa05f8054 = 0x00000000; // Same for progressive, resetting the offset gets us 2MB VRAM back after BootROM is done with it

    // zero out framebuffer area
    for(uint32_t pixel_or_two = 0; pixel_or_two < scan_area_size_bytes; pixel_or_two += 4)
    {
      *(uint32_t*)(0xa5000000 + pixel_or_two) = 0;
    }

    // re-enable video
    *(volatile uint32_t*)0xa05f80e8 &= ~8;
    *(volatile uint32_t*)0xa05f8044 |= 1;
  }
}

// 1152x864 @ 75Hz (4:3, DMT) - PVR 32x32
// Framebuffer 288x864
// Horizontal scale: 0.25x (exact)
// This is actually a standard, widely supported mode!
void STARTUP_1152x864_VGA_75(uint8_t fbuffer_color_mode)
{
  // Set global scale factors
  STARTUP_video_params.video_scale = 4.0f;
  STARTUP_video_params.video_scale_multiplier = 0.25f;

  // Set global video output mode parameters
  STARTUP_video_params.video_width = 1152;
  STARTUP_video_params.video_height = 864;
  STARTUP_video_params.video_color_type = fbuffer_color_mode;
  STARTUP_video_params.video_refresh_rate = 75;

  // 1600 wide scaled to 400 wide (field)
  // 1152 wide scaled to 288 wide (visible frame)
  uint32_t horiz_active_area = 288;
  uint32_t vert_active_area = 864;
  // {RGB0555, RGB565} = 2Bpp, {RGB888} = 3Bpp, {RGB0888} = 4Bpp
  uint32_t bpp_mode_size = fbuffer_color_mode + 1 + (0x1 ^ ((fbuffer_color_mode & 0x1) | (fbuffer_color_mode >> 1))); // Add another 1 only if 0b00

  // Set global framebuffer parameters
  STARTUP_video_params.fb_width = horiz_active_area;
  STARTUP_video_params.fb_height = vert_active_area;
  STARTUP_video_params.fb_color_bytes = bpp_mode_size;

  if(!cable_mode) // VGA 75Hz
  {
    *(volatile uint32_t*)0xa05f80e8 = 0x00160008;
    *(volatile uint32_t*)0xa05f8044 = 0x00800000 | (fbuffer_color_mode << 2);

    *(volatile uint32_t*)0xa05f804c = (horiz_active_area * bpp_mode_size) / 8; // for PVR to know active area width
    *(volatile uint32_t*)0xa05f8040 = 0x00000000; // Border color in RGB0888 format (this mode has no border)
    *(volatile uint32_t*)0xa05f805c = (1 << 20) | ((vert_active_area - 1) << 10) | (((horiz_active_area * bpp_mode_size) / 4) - 1); // progressive scan has a 1 since no lines are skipped

    *(volatile uint32_t*)0xa05f80ec = 0x00000060; // ok: 96 ? 168, default: 0x000000a8 (horiz)
    *(volatile uint32_t*)0xa05f80f0 = 0x00230023; // ok: 35 & 35 ? 40 & 40, default: 0x00280028 (vert)
    *(volatile uint32_t*)0xa05f80c8 = 0x01800000; // ok: 384 ? 837, default: 0x03450000 (horiz)
    *(volatile uint32_t*)0xa05f80cc = 0x00230383; // ok: 35, 899 ? 21, 520, default: 0x00150208 (vert)
    *(volatile uint32_t*)0xa05f80d0 = 0x00000100;
    *(volatile uint32_t*)0xa05f80d4 = 0x00600180; // ok: 96, 384 ? 126, 837, default: 0x007e0345 (horiz)
    *(volatile uint32_t*)0xa05f80d8 = 0x0383018f; // ok: 399x899 (400x900) ? 857x524 (858x525), default: 0x020c0359
    *(volatile uint32_t*)0xa05f80dc = 0x00230383; // ok: 35, 899 ? 40, 520, default: 0x00280208 (vert)
    *(volatile uint32_t*)0xa05f80e0 = 0x01d6f31f; // ok: 7 (8,- 1), 367 (400 - 32,- 1), 3 (DMT/GTF/legacy), 31 (32,- 1) ? 15, 793, 3, 63, default: 0x03f1933f

    uint32_t scan_area_size = horiz_active_area * vert_active_area;
    uint32_t scan_area_size_bytes = scan_area_size * bpp_mode_size; // This will always be divisible by 4

    // Reset framebuffer address
    *(volatile uint32_t*)0xa05f8050 = 0x00000000; // BootROM sets this to 0x00200000 (framebuffer base is 0xa5000000 + this)
    *(volatile uint32_t*)0xa05f8054 = 0x00000000; // Same for progressive, resetting the offset gets us 2MB VRAM back after BootROM is done with it

    // zero out framebuffer area
    for(uint32_t pixel_or_two = 0; pixel_or_two < scan_area_size_bytes; pixel_or_two += 4)
    {
      *(uint32_t*)(0xa5000000 + pixel_or_two) = 0;
    }

    // re-enable video
    *(volatile uint32_t*)0xa05f80e8 &= ~8;
    *(volatile uint32_t*)0xa05f8044 |= 1;
  }
}

//------------------------------------------------------------------------------
// 120Hz Modes - LCD DISPLAYS ONLY!
//------------------------------------------------------------------------------
//
// Scaled 120Hz video modes
//
// Untested. I don't own a 120Hz monitor with VGA input (though they do exist,
// like certain models of Acer XB240H) nor a 120Hz TV. The timings appear to be
// correct, though.
//

// 480p @ 120Hz (4:3, CTA-861, 720x480) - for HDTVs - PVR 32x32
// Framebuffer: 320x480
// Horizontal scale: 0.5x (exact)
void STARTUP_480p_VGA_120(uint8_t fbuffer_color_mode)
{
  // Set global scale factors
  STARTUP_video_params.video_scale = 2.0f;
  STARTUP_video_params.video_scale_multiplier = 0.5f;

  // Set global video output mode parameters
  STARTUP_video_params.video_width = 640;
  STARTUP_video_params.video_height = 480;
  STARTUP_video_params.video_color_type = fbuffer_color_mode;
  STARTUP_video_params.video_refresh_rate = 120;

  // 858 wide scaled to 429 wide (field)
  // 640 wide scaled to 320 wide (visible frame)
  uint32_t horiz_active_area = 320;
  uint32_t vert_active_area = 480;
  // {RGB0555, RGB565} = 2Bpp, {RGB888} = 3Bpp, {RGB0888} = 4Bpp
  uint32_t bpp_mode_size = fbuffer_color_mode + 1 + (0x1 ^ ((fbuffer_color_mode & 0x1) | (fbuffer_color_mode >> 1))); // Add another 1 only if 0b00

  // Set global framebuffer parameters
  STARTUP_video_params.fb_width = horiz_active_area;
  STARTUP_video_params.fb_height = vert_active_area;
  STARTUP_video_params.fb_color_bytes = bpp_mode_size;

  if(!cable_mode) // VGA
  {
    *(volatile uint32_t*)0xa05f80e8 = 0x00160008;
    *(volatile uint32_t*)0xa05f8044 = 0x00800000 | (fbuffer_color_mode << 2);

    *(volatile uint32_t*)0xa05f804c = (horiz_active_area * bpp_mode_size) / 8; // for PVR to know active area width
    *(volatile uint32_t*)0xa05f8040 = 0x00000000; // Border color in RGB0888 format (this mode has no border)
    *(volatile uint32_t*)0xa05f805c = (1 << 20) | ((vert_active_area - 1) << 10) | (((horiz_active_area * bpp_mode_size) / 4) - 1); // progressive scan has a 1 since no lines are skipped

    *(volatile uint32_t*)0xa05f80ec = 0x00000054; // ok: 84 ? 168, default: 0x000000a8 (horiz)
    *(volatile uint32_t*)0xa05f80f0 = 0x00280028; // ok: 40 & 40 ? 40 & 40, default: 0x00280028 (vert)
    *(volatile uint32_t*)0xa05f80c8 = 0x01a30000; // ok: 419 ? 837, default: 0x03450000 (horiz)
    *(volatile uint32_t*)0xa05f80cc = 0x00150208; // ok: 21, 520 ? 21, 520, default: 0x00150208 (vert)
    *(volatile uint32_t*)0xa05f80d0 = 0x00000100;
    *(volatile uint32_t*)0xa05f80d4 = 0x003f01a3; // ok: 63, 419 ? 126, 837, default: 0x007e0345 (horiz)
    *(volatile uint32_t*)0xa05f80d8 = 0x020c01ac; // ok: 428x524 (429x525) ? 857x524 (858x525), default: 0x020c0359
    *(volatile uint32_t*)0xa05f80dc = 0x00150208; // ok: 40, 520 ? 40, 520, default: 0x00280208 (vert)
    *(volatile uint32_t*)0xa05f80e0 = 0x01d8c41f; // ok: 7 (8,- 1), 396 (429 - 32,- 1), 4 (4:3), 31 (32,- 1) ? 15, 793, 3, 63, default: 0x03f1933f

    uint32_t scan_area_size = horiz_active_area * vert_active_area;
    uint32_t scan_area_size_bytes = scan_area_size * bpp_mode_size; // This will always be divisible by 4

    // Reset framebuffer address
    *(volatile uint32_t*)0xa05f8050 = 0x00000000; // BootROM sets this to 0x00200000 (framebuffer base is 0xa5000000 + this)
    *(volatile uint32_t*)0xa05f8054 = 0x00000000; // Same for progressive, resetting the offset gets us 2MB VRAM back after BootROM is done with it

    // zero out framebuffer area
    for(uint32_t pixel_or_two = 0; pixel_or_two < scan_area_size_bytes; pixel_or_two += 4)
    {
      *(uint32_t*)(0xa5000000 + pixel_or_two) = 0;
    }

    // re-enable video
    *(volatile uint32_t*)0xa05f80e8 &= ~8;
    *(volatile uint32_t*)0xa05f8044 |= 1;
  }
}

// 640x480 @ 120Hz (4:3, CVT, RB) - for monitors that need this instead of HDTV 480p120
// Framebuffer: 354x480
// Horizontal scale: 0.553125x (exact)
void STARTUP_640x480_VGA_120(uint8_t fbuffer_color_mode)
{
  // Set global scale factors
  STARTUP_video_params.video_scale = 640.0f / 354.0f;
  STARTUP_video_params.video_scale_multiplier = 354.0f / 640.0f;

  // Set global video output mode parameters
  STARTUP_video_params.video_width = 640;
  STARTUP_video_params.video_height = 480;
  STARTUP_video_params.video_color_type = fbuffer_color_mode;
  STARTUP_video_params.video_refresh_rate = 120;

  // 800 wide scaled to 443 wide (field)
  // 640 wide scaled to 354 wide (visible frame)
  uint32_t horiz_active_area = 354;
  uint32_t vert_active_area = 480;
  // {RGB0555, RGB565} = 2Bpp, {RGB888} = 3Bpp, {RGB0888} = 4Bpp
  uint32_t bpp_mode_size = fbuffer_color_mode + 1 + (0x1 ^ ((fbuffer_color_mode & 0x1) | (fbuffer_color_mode >> 1))); // Add another 1 only if 0b00

  // Set global framebuffer parameters
  STARTUP_video_params.fb_width = horiz_active_area;
  STARTUP_video_params.fb_height = vert_active_area;
  STARTUP_video_params.fb_color_bytes = bpp_mode_size;

  if(!cable_mode) // VGA
  {
    *(volatile uint32_t*)0xa05f80e8 = 0x00160008;
    *(volatile uint32_t*)0xa05f8044 = 0x00800000 | (fbuffer_color_mode << 2);

    *(volatile uint32_t*)0xa05f804c = (horiz_active_area * bpp_mode_size) / 8; // for PVR to know active area width
    *(volatile uint32_t*)0xa05f8040 = 0x00000000; // Border color in RGB0888 format (this mode has no border)
    *(volatile uint32_t*)0xa05f805c = (1 << 20) | ((vert_active_area - 1) << 10) | (((horiz_active_area * bpp_mode_size) / 4) - 1); // progressive scan has a 1 since no lines are skipped

    *(volatile uint32_t*)0xa05f80ec = 0x0000003e; // ok: 62 ? 168, default: 0x000000a8 (horiz)
    *(volatile uint32_t*)0xa05f80f0 = 0x001a001a; // ok: 26 & 26 ? 40 & 40, default: 0x00280028 (vert)
    *(volatile uint32_t*)0xa05f80c8 = 0x01a00000; // ok: 416 ? 837, default: 0x03450000 (horiz)
    *(volatile uint32_t*)0xa05f80cc = 0x001a01fa; // ok: 26, 506 ? 21, 520, default: 0x00150208 (vert)
    *(volatile uint32_t*)0xa05f80d0 = 0x00000100;
    *(volatile uint32_t*)0xa05f80d4 = 0x003e01a0; // ok: 62, 416 ? 126, 837, default: 0x007e0345 (horiz)
    *(volatile uint32_t*)0xa05f80d8 = 0x01fc01ba; // ok: 442x508 (443x509) ? 857x524 (858x525), default: 0x020c0359
    *(volatile uint32_t*)0xa05f80dc = 0x001a01fa; // ok: 26, 506 ? 40, 520, default: 0x00280208 (vert)
    *(volatile uint32_t*)0xa05f80e0 = 0x00da8411; // ok: 3 (4,- 1), 424 (443 - 18,- 1), 4 (4:3), 17 (18,- 1) ? 15, 793, 3, 63, default: 0x03f1933f

    uint32_t scan_area_size = horiz_active_area * vert_active_area;
    uint32_t scan_area_size_bytes = scan_area_size * bpp_mode_size; // This will always be divisible by 4

    // Reset framebuffer address
    *(volatile uint32_t*)0xa05f8050 = 0x00000000; // BootROM sets this to 0x00200000 (framebuffer base is 0xa5000000 + this)
    *(volatile uint32_t*)0xa05f8054 = 0x00000000; // Same for progressive, resetting the offset gets us 2MB VRAM back after BootROM is done with it

    // zero out framebuffer area
    for(uint32_t pixel_or_two = 0; pixel_or_two < scan_area_size_bytes; pixel_or_two += 4)
    {
      *(uint32_t*)(0xa5000000 + pixel_or_two) = 0;
    }

    // re-enable video
    *(volatile uint32_t*)0xa05f80e8 &= ~8;
    *(volatile uint32_t*)0xa05f8044 |= 1;
  }
}

// 640x480 @ 120Hz (4:3, CVT, RB) - for monitors that need this instead of HDTV 480p120 - PVR 32x32
// Framebuffer: 352x480
// Horizontal scale: 0.55x (exact)
// This mode has been shrunken by 2 columns for 32x32 framebuffer compatibility.
// As a result, there may be 2 columns of blank pixels (1 on either side or 2 on
// one side).
void STARTUP_640x480_VGA_120_PVR(uint8_t fbuffer_color_mode)
{
  // Set global scale factors
  STARTUP_video_params.video_scale = 640.0f / 352.0f;
  STARTUP_video_params.video_scale_multiplier = 352.0f / 640.0f;

  // Set global video output mode parameters
  STARTUP_video_params.video_width = 640;
  STARTUP_video_params.video_height = 480;
  STARTUP_video_params.video_color_type = fbuffer_color_mode;
  STARTUP_video_params.video_refresh_rate = 120;

  // 800 wide scaled to 443 wide (field)
  // 640 wide scaled to 354 wide (visible frame)
  uint32_t horiz_active_area = 352;
  uint32_t vert_active_area = 480;
  // {RGB0555, RGB565} = 2Bpp, {RGB888} = 3Bpp, {RGB0888} = 4Bpp
  uint32_t bpp_mode_size = fbuffer_color_mode + 1 + (0x1 ^ ((fbuffer_color_mode & 0x1) | (fbuffer_color_mode >> 1))); // Add another 1 only if 0b00

  // Set global framebuffer parameters
  STARTUP_video_params.fb_width = horiz_active_area;
  STARTUP_video_params.fb_height = vert_active_area;
  STARTUP_video_params.fb_color_bytes = bpp_mode_size;

  if(!cable_mode) // VGA
  {
    *(volatile uint32_t*)0xa05f80e8 = 0x00160008;
    *(volatile uint32_t*)0xa05f8044 = 0x00800000 | (fbuffer_color_mode << 2);

    *(volatile uint32_t*)0xa05f804c = (horiz_active_area * bpp_mode_size) / 8; // for PVR to know active area width
    *(volatile uint32_t*)0xa05f8040 = 0x00000000; // Border color in RGB0888 format
    *(volatile uint32_t*)0xa05f805c = (1 << 20) | ((vert_active_area - 1) << 10) | (((horiz_active_area * bpp_mode_size) / 4) - 1); // progressive scan has a 1 since no lines are skipped

    *(volatile uint32_t*)0xa05f80ec = 0x0000003f; // ok: 63 ? 168, default: 0x000000a8 (horiz)
    *(volatile uint32_t*)0xa05f80f0 = 0x001a001a; // ok: 26 & 26 ? 40 & 40, default: 0x00280028 (vert)
    *(volatile uint32_t*)0xa05f80c8 = 0x01a00000; // ok: 416 ? 837, default: 0x03450000 (horiz)
    *(volatile uint32_t*)0xa05f80cc = 0x001a01fa; // ok: 26, 506 ? 21, 520, default: 0x00150208 (vert)
    *(volatile uint32_t*)0xa05f80d0 = 0x00000100;
    *(volatile uint32_t*)0xa05f80d4 = 0x003e01a0; // ok: 62, 416 ? 126, 837, default: 0x007e0345 (horiz)
    *(volatile uint32_t*)0xa05f80d8 = 0x01fc01ba; // ok: 442x508 (443x509) ? 857x524 (858x525), default: 0x020c0359
    *(volatile uint32_t*)0xa05f80dc = 0x001a01fa; // ok: 26, 506 ? 40, 520, default: 0x00280208 (vert)
    *(volatile uint32_t*)0xa05f80e0 = 0x00da8411; // ok: 3 (4,- 1), 424 (443 - 18,- 1), 4 (4:3), 17 (18,- 1) ? 15, 793, 3, 63, default: 0x03f1933f

    uint32_t scan_area_size = horiz_active_area * vert_active_area;
    uint32_t scan_area_size_bytes = scan_area_size * bpp_mode_size; // This will always be divisible by 4

    // Reset framebuffer address
    *(volatile uint32_t*)0xa05f8050 = 0x00000000; // BootROM sets this to 0x00200000 (framebuffer base is 0xa5000000 + this)
    *(volatile uint32_t*)0xa05f8054 = 0x00000000; // Same for progressive, resetting the offset gets us 2MB VRAM back after BootROM is done with it

    // zero out framebuffer area
    for(uint32_t pixel_or_two = 0; pixel_or_two < scan_area_size_bytes; pixel_or_two += 4)
    {
      *(uint32_t*)(0xa5000000 + pixel_or_two) = 0;
    }

    // re-enable video
    *(volatile uint32_t*)0xa05f80e8 &= ~8;
    *(volatile uint32_t*)0xa05f8044 |= 1;
  }
}

// 800x600 @ 120Hz (4:3, DMT & CVT, RB)
// Framebuffer: 295x600
// Horizontal scale: 0.36875x (exact)
void STARTUP_800x600_VGA_120(uint8_t fbuffer_color_mode)
{
  // Set global scale factors
  STARTUP_video_params.video_scale = 800.0f / 295.0f;
  STARTUP_video_params.video_scale_multiplier = 295.0f / 800.0f;

  // Set global video output mode parameters
  STARTUP_video_params.video_width = 800;
  STARTUP_video_params.video_height = 600;
  STARTUP_video_params.video_color_type = fbuffer_color_mode;
  STARTUP_video_params.video_refresh_rate = 120;

  // 960 wide scaled to 354 wide (field)
  // 800 wide scaled to 295 wide (visible frame)
  uint32_t horiz_active_area = 295;
  uint32_t vert_active_area = 600;
  // {RGB0555, RGB565} = 2Bpp, {RGB888} = 3Bpp, {RGB0888} = 4Bpp
  uint32_t bpp_mode_size = fbuffer_color_mode + 1 + (0x1 ^ ((fbuffer_color_mode & 0x1) | (fbuffer_color_mode >> 1))); // Add another 1 only if 0b00

  // Set global framebuffer parameters
  STARTUP_video_params.fb_width = horiz_active_area;
  STARTUP_video_params.fb_height = vert_active_area;
  STARTUP_video_params.fb_color_bytes = bpp_mode_size;

  if(!cable_mode) // VGA
  {
    *(volatile uint32_t*)0xa05f80e8 = 0x00160008;
    *(volatile uint32_t*)0xa05f8044 = 0x00800000 | (fbuffer_color_mode << 2);

    *(volatile uint32_t*)0xa05f804c = (horiz_active_area * bpp_mode_size) / 8; // for PVR to know active area width
    *(volatile uint32_t*)0xa05f8040 = 0x00000000; // Border color in RGB0888 format (this mode has no border)
    *(volatile uint32_t*)0xa05f805c = (1 << 20) | ((vert_active_area - 1) << 10) | (((horiz_active_area * bpp_mode_size) / 4) - 1); // progressive scan has a 1 since no lines are skipped

    *(volatile uint32_t*)0xa05f80ec = 0x00000029; // ok: 41 ? 168, default: 0x000000a8 (horiz)
    *(volatile uint32_t*)0xa05f80f0 = 0x00210021; // ok: 33 & 33 ? 40 & 40, default: 0x00280028 (vert)
    *(volatile uint32_t*)0xa05f80c8 = 0x01500000; // ok: 336 ? 837, default: 0x03450000 (horiz)
    *(volatile uint32_t*)0xa05f80cc = 0x00210279; // ok: 33, 633 ? 21, 520, default: 0x00150208 (vert)
    *(volatile uint32_t*)0xa05f80d0 = 0x00000100;
    *(volatile uint32_t*)0xa05f80d4 = 0x00290150; // ok: 41, 336 ? 126, 837, default: 0x007e0345 (horiz)
    *(volatile uint32_t*)0xa05f80d8 = 0x027b0161; // ok: 353x635 (354x636) ? 857x524 (858x525), default: 0x020c0359
    *(volatile uint32_t*)0xa05f80dc = 0x00210279; // ok: 33, 633 ? 40, 520, default: 0x00280208 (vert)
    *(volatile uint32_t*)0xa05f80e0 = 0x0095540b; // ok: 2 (3,- 1), 341 (354 - 12,- 1), 4 (4:3), 11 (12,- 1) ? 15, 793, 3, 63, default: 0x03f1933f

    uint32_t scan_area_size = horiz_active_area * vert_active_area;
    uint32_t scan_area_size_bytes = scan_area_size * bpp_mode_size; // This will always be divisible by 4

    // Reset framebuffer address
    *(volatile uint32_t*)0xa05f8050 = 0x00000000; // BootROM sets this to 0x00200000 (framebuffer base is 0xa5000000 + this)
    *(volatile uint32_t*)0xa05f8054 = 0x00000000; // Same for progressive, resetting the offset gets us 2MB VRAM back after BootROM is done with it

    // zero out framebuffer area
    for(uint32_t pixel_or_two = 0; pixel_or_two < scan_area_size_bytes; pixel_or_two += 4)
    {
      *(uint32_t*)(0xa5000000 + pixel_or_two) = 0;
    }

    // re-enable video
    *(volatile uint32_t*)0xa05f80e8 &= ~8;
    *(volatile uint32_t*)0xa05f8044 |= 1;
  }
}

// 1024x768 @ 120Hz (4:3, DMT & CVT, RB)
// Framebuffer: 239x768
// Horizontal scale: 0.233398438x
void STARTUP_1024x768_VGA_120(uint8_t fbuffer_color_mode)
{
  // Set global scale factors
  STARTUP_video_params.video_scale = 1024.0f / 239.0f;
  STARTUP_video_params.video_scale_multiplier = 239.0f / 1024.0f;

  // Set global video output mode parameters
  STARTUP_video_params.video_width = 1024;
  STARTUP_video_params.video_height = 768;
  STARTUP_video_params.video_color_type = fbuffer_color_mode;
  STARTUP_video_params.video_refresh_rate = 120;

  // 1184 wide scaled to 277 wide (field)
  // 1024 wide scaled to 239 wide (visible frame)
  uint32_t horiz_active_area = 239;
  uint32_t vert_active_area = 768;
  // {RGB0555, RGB565} = 2Bpp, {RGB888} = 3Bpp, {RGB0888} = 4Bpp
  uint32_t bpp_mode_size = fbuffer_color_mode + 1 + (0x1 ^ ((fbuffer_color_mode & 0x1) | (fbuffer_color_mode >> 1))); // Add another 1 only if 0b00

  // Set global framebuffer parameters
  STARTUP_video_params.fb_width = horiz_active_area;
  STARTUP_video_params.fb_height = vert_active_area;
  STARTUP_video_params.fb_color_bytes = bpp_mode_size;

  if(!cable_mode) // VGA
  {
    *(volatile uint32_t*)0xa05f80e8 = 0x00160008;
    *(volatile uint32_t*)0xa05f8044 = 0x00800000 | (fbuffer_color_mode << 2);

    *(volatile uint32_t*)0xa05f804c = (horiz_active_area * bpp_mode_size) / 8; // for PVR to know active area width
    *(volatile uint32_t*)0xa05f8040 = 0x00000000; // Border color in RGB0888 format (this mode has no border)
    *(volatile uint32_t*)0xa05f805c = (1 << 20) | ((vert_active_area - 1) << 10) | (((horiz_active_area * bpp_mode_size) / 4) - 1); // progressive scan has a 1 since no lines are skipped

    *(volatile uint32_t*)0xa05f80ec = 0x00000011; // ok: 17 ? 168, default: 0x000000a8 (horiz)
    *(volatile uint32_t*)0xa05f80f0 = 0x002a002a; // ok: 42 & 42 ? 40 & 40, default: 0x00280028 (vert)
    *(volatile uint32_t*)0xa05f80c8 = 0x010a0000; // ok: 266 ? 837, default: 0x03450000 (horiz)
    *(volatile uint32_t*)0xa05f80cc = 0x002a032a; // ok: 42, 810 ? 21, 520, default: 0x00150208 (vert)
    *(volatile uint32_t*)0xa05f80d0 = 0x00000100;
    *(volatile uint32_t*)0xa05f80d4 = 0x0011010a; // ok: 17, 266 ? 126, 837, default: 0x007e0345 (horiz)
    *(volatile uint32_t*)0xa05f80d8 = 0x032c0114; // ok: 276x812 (277x813) ? 857x524 (858x525), default: 0x020c0359
    *(volatile uint32_t*)0xa05f80dc = 0x002a032a; // ok: 42, 810 ? 40, 520, default: 0x00280208 (vert)
    *(volatile uint32_t*)0xa05f80e0 = 0x0050c407; // ok: 1 (2,- 1), 268 (277 - 8,- 1), 4 (4:3), 7 (8,- 1) ? 15, 793, 3, 63, default: 0x03f1933f

    uint32_t scan_area_size = horiz_active_area * vert_active_area;
    uint32_t scan_area_size_bytes = scan_area_size * bpp_mode_size; // This will always be divisible by 4

    // Reset framebuffer address
    *(volatile uint32_t*)0xa05f8050 = 0x00000000; // BootROM sets this to 0x00200000 (framebuffer base is 0xa5000000 + this)
    *(volatile uint32_t*)0xa05f8054 = 0x00000000; // Same for progressive, resetting the offset gets us 2MB VRAM back after BootROM is done with it

    // zero out framebuffer area
    for(uint32_t pixel_or_two = 0; pixel_or_two < scan_area_size_bytes; pixel_or_two += 4)
    {
      *(uint32_t*)(0xa5000000 + pixel_or_two) = 0;
    }

    // re-enable video
    *(volatile uint32_t*)0xa05f80e8 &= ~8;
    *(volatile uint32_t*)0xa05f8044 |= 1;
  }
}

//------------------------------------------------------------------------------
// 240Hz Modes - LCD DISPLAYS ONLY!
//------------------------------------------------------------------------------
//
// Scaled 240Hz video modes
//
// Untested. I don't own a 240Hz monitor with VGA input nor a 240Hz TV. The
// timings appear to be correct, though.
//
// ...Why not? I already did everything else :P
//
// There are two modes: one is for 240Hz and the other is for 239.76Hz. It's the
// same deal as 60Hz vs 59.94Hz, although at such high rates this difference may
// determine whether the video trick works or not with some monitors. No idea.
//

// 480p @ 240Hz (4:3, CTA-861, 720x480) - PVR 32x32
// Framebuffer: 160x480
// Horizontal scale: 0.25x (exact)
void STARTUP_480p_VGA_240(uint8_t fbuffer_color_mode)
{
  // Set global scale factors
  STARTUP_video_params.video_scale = 4.0f;
  STARTUP_video_params.video_scale_multiplier = 0.25f;

  // Set global video output mode parameters
  STARTUP_video_params.video_width = 640;
  STARTUP_video_params.video_height = 480;
  STARTUP_video_params.video_color_type = fbuffer_color_mode;
  STARTUP_video_params.video_refresh_rate = 240;

  // 858 wide scaled to 214 wide (field)
  // 640 wide scaled to 160 wide (visible frame)
  uint32_t horiz_active_area = 160;
  uint32_t vert_active_area = 480;
  // {RGB0555, RGB565} = 2Bpp, {RGB888} = 3Bpp, {RGB0888} = 4Bpp
  uint32_t bpp_mode_size = fbuffer_color_mode + 1 + (0x1 ^ ((fbuffer_color_mode & 0x1) | (fbuffer_color_mode >> 1))); // Add another 1 only if 0b00

  // Set global framebuffer parameters
  STARTUP_video_params.fb_width = horiz_active_area;
  STARTUP_video_params.fb_height = vert_active_area;
  STARTUP_video_params.fb_color_bytes = bpp_mode_size;

  if(!cable_mode) // VGA
  {
    *(volatile uint32_t*)0xa05f80e8 = 0x00160008;
    *(volatile uint32_t*)0xa05f8044 = 0x00800000 | (fbuffer_color_mode << 2);

    *(volatile uint32_t*)0xa05f804c = (horiz_active_area * bpp_mode_size) / 8; // for PVR to know active area width
    *(volatile uint32_t*)0xa05f8040 = 0x00000000; // Border color in RGB0888 format (this mode has no border)
    *(volatile uint32_t*)0xa05f805c = (1 << 20) | ((vert_active_area - 1) << 10) | (((horiz_active_area * bpp_mode_size) / 4) - 1); // progressive scan has a 1 since no lines are skipped

    *(volatile uint32_t*)0xa05f80ec = 0x0000002a; // ok: 42 ? 168, default: 0x000000a8 (horiz)
    *(volatile uint32_t*)0xa05f80f0 = 0x00280028; // ok: 40 & 40 ? 40 & 40, default: 0x00280028 (vert)
    *(volatile uint32_t*)0xa05f80c8 = 0x00d10000; // ok: 209 ? 837, default: 0x03450000 (horiz)
    *(volatile uint32_t*)0xa05f80cc = 0x00150208; // ok: 21, 520 ? 21, 520, default: 0x00150208 (vert)
    *(volatile uint32_t*)0xa05f80d0 = 0x00000100;
    *(volatile uint32_t*)0xa05f80d4 = 0x001f00d1; // ok: 31, 209 ? 126, 837, default: 0x007e0345 (horiz)
    *(volatile uint32_t*)0xa05f80d8 = 0x020c00d5; // ok: 213x524 (214x525) ? 857x524 (858x525), default: 0x020c0359
    *(volatile uint32_t*)0xa05f80dc = 0x00150208; // ok: 40, 520 ? 40, 520, default: 0x00280208 (vert)
    *(volatile uint32_t*)0xa05f80e0 = 0x00cc540f; // ok: 3 (4,- 1), 197 (214 - 16,- 1), 4 (4:3), 15 (16,- 1) ? 15, 793, 3, 63, default: 0x03f1933f

    uint32_t scan_area_size = horiz_active_area * vert_active_area;
    uint32_t scan_area_size_bytes = scan_area_size * bpp_mode_size; // This will always be divisible by 4

    // Reset framebuffer address
    *(volatile uint32_t*)0xa05f8050 = 0x00000000; // BootROM sets this to 0x00200000 (framebuffer base is 0xa5000000 + this)
    *(volatile uint32_t*)0xa05f8054 = 0x00000000; // Same for progressive, resetting the offset gets us 2MB VRAM back after BootROM is done with it

    // zero out framebuffer area
    for(uint32_t pixel_or_two = 0; pixel_or_two < scan_area_size_bytes; pixel_or_two += 4)
    {
      *(uint32_t*)(0xa5000000 + pixel_or_two) = 0;
    }

    // re-enable video
    *(volatile uint32_t*)0xa05f80e8 &= ~8;
    *(volatile uint32_t*)0xa05f8044 |= 1;
  }
}

// 480p @ 239.76Hz (4:3, CTA-861, 720x480) - PVR 32x32
// Framebuffer: 160x480
// Horizontal scale: 0.25x (exact)
void STARTUP_480p_VGA_239(uint8_t fbuffer_color_mode)
{
  // Set global scale factors
  STARTUP_video_params.video_scale = 4.0f;
  STARTUP_video_params.video_scale_multiplier = 0.25f;

  // Set global video output mode parameters
  STARTUP_video_params.video_width = 640;
  STARTUP_video_params.video_height = 480;
  STARTUP_video_params.video_color_type = fbuffer_color_mode;
  STARTUP_video_params.video_refresh_rate = 240;

  // 858 wide scaled to 215 wide (field)
  // 640 wide scaled to 160 wide (visible frame)
  uint32_t horiz_active_area = 160;
  uint32_t vert_active_area = 480;
  // {RGB0555, RGB565} = 2Bpp, {RGB888} = 3Bpp, {RGB0888} = 4Bpp
  uint32_t bpp_mode_size = fbuffer_color_mode + 1 + (0x1 ^ ((fbuffer_color_mode & 0x1) | (fbuffer_color_mode >> 1))); // Add another 1 only if 0b00

  // Set global framebuffer parameters
  STARTUP_video_params.fb_width = horiz_active_area;
  STARTUP_video_params.fb_height = vert_active_area;
  STARTUP_video_params.fb_color_bytes = bpp_mode_size;

  if(!cable_mode) // VGA
  {
    *(volatile uint32_t*)0xa05f80e8 = 0x00160008;
    *(volatile uint32_t*)0xa05f8044 = 0x00800000 | (fbuffer_color_mode << 2);

    *(volatile uint32_t*)0xa05f804c = (horiz_active_area * bpp_mode_size) / 8; // for PVR to know active area width
    *(volatile uint32_t*)0xa05f8040 = 0x00000000; // Border color in RGB0888 format (this mode has no border)
    *(volatile uint32_t*)0xa05f805c = (1 << 20) | ((vert_active_area - 1) << 10) | (((horiz_active_area * bpp_mode_size) / 4) - 1); // progressive scan has a 1 since no lines are skipped

    *(volatile uint32_t*)0xa05f80ec = 0x0000002a; // ok: 42 ? 168, default: 0x000000a8 (horiz)
    *(volatile uint32_t*)0xa05f80f0 = 0x00280028; // ok: 40 & 40 ? 40 & 40, default: 0x00280028 (vert)
    *(volatile uint32_t*)0xa05f80c8 = 0x00d10000; // ok: 209 ? 837, default: 0x03450000 (horiz)
    *(volatile uint32_t*)0xa05f80cc = 0x00150208; // ok: 21, 520 ? 21, 520, default: 0x00150208 (vert)
    *(volatile uint32_t*)0xa05f80d0 = 0x00000100;
    *(volatile uint32_t*)0xa05f80d4 = 0x002000d1; // ok: 32, 209 ? 126, 837, default: 0x007e0345 (horiz)
    *(volatile uint32_t*)0xa05f80d8 = 0x020c00d6; // ok: 214x524 (215x525) ? 857x524 (858x525), default: 0x020c0359
    *(volatile uint32_t*)0xa05f80dc = 0x00150208; // ok: 40, 520 ? 40, 520, default: 0x00280208 (vert)
    *(volatile uint32_t*)0xa05f80e0 = 0x00cc540f; // ok: 3 (4,- 1), 197 (214 - 16,- 1), 4 (4:3), 15 (16,- 1) ? 15, 793, 3, 63, default: 0x03f1933f

    uint32_t scan_area_size = horiz_active_area * vert_active_area;
    uint32_t scan_area_size_bytes = scan_area_size * bpp_mode_size; // This will always be divisible by 4

    // Reset framebuffer address
    *(volatile uint32_t*)0xa05f8050 = 0x00000000; // BootROM sets this to 0x00200000 (framebuffer base is 0xa5000000 + this)
    *(volatile uint32_t*)0xa05f8054 = 0x00000000; // Same for progressive, resetting the offset gets us 2MB VRAM back after BootROM is done with it

    // zero out framebuffer area
    for(uint32_t pixel_or_two = 0; pixel_or_two < scan_area_size_bytes; pixel_or_two += 4)
    {
      *(uint32_t*)(0xa5000000 + pixel_or_two) = 0;
    }

    // re-enable video
    *(volatile uint32_t*)0xa05f80e8 &= ~8;
    *(volatile uint32_t*)0xa05f8044 |= 1;
  }
}

//------------------------------------------------------------------------------
// CVT RBv2 Modes - LCD DISPLAYS ONLY!
//------------------------------------------------------------------------------
//
// Extra native video modes
//
// CVT Reduced Blanking v2 is a newer standard introduced in 2013.
// Monitors made before 2014 may not display them correctly.
//
// Thanks to CVT Reduced Blanking v2, a couple modes' video timings were slimmed
// down just enough that the Dreamcast can do them pretty much natively now.
//

// 640x480 @ 75Hz (4:3, CVT, RBv2) - PVR 32x32
// Framebuffer: 640x480
// Horizontal scale: 1.0x (exact)
void STARTUP_640x480_VGA_75_CVT_RBv2(uint8_t fbuffer_color_mode)
{
  // Set global scale factors
  STARTUP_video_params.video_scale = 1.0f;
  STARTUP_video_params.video_scale_multiplier = 1.0f;

  // Set global video output mode parameters
  STARTUP_video_params.video_width = 640;
  STARTUP_video_params.video_height = 480;
  STARTUP_video_params.video_color_type = fbuffer_color_mode;
  STARTUP_video_params.video_refresh_rate = 75;

  // 720 wide scaled to 723 wide (field) for 75.00 Hz
  // visible frame unchanged
  uint32_t horiz_active_area = 640;
  uint32_t vert_active_area = 480;
  // {RGB0555, RGB565} = 2Bpp, {RGB888} = 3Bpp, {RGB0888} = 4Bpp
  uint32_t bpp_mode_size = fbuffer_color_mode + 1 + (0x1 ^ ((fbuffer_color_mode & 0x1) | (fbuffer_color_mode >> 1))); // Add another 1 only if 0b00

  // Set global framebuffer parameters
  STARTUP_video_params.fb_width = horiz_active_area;
  STARTUP_video_params.fb_height = vert_active_area;
  STARTUP_video_params.fb_color_bytes = bpp_mode_size;

  if(!cable_mode) // VGA 60Hz
  {
    *(volatile uint32_t*)0xa05f80e8 = 0x00160008;
    *(volatile uint32_t*)0xa05f8044 = 0x00800000 | (fbuffer_color_mode << 2);

    *(volatile uint32_t*)0xa05f804c = (horiz_active_area * bpp_mode_size) / 8; // for PVR to know active area width
    *(volatile uint32_t*)0xa05f8040 = 0x00000000; // Border color in RGB0888 format (this mode has no border)
    *(volatile uint32_t*)0xa05f805c = (1 << 20) | ((vert_active_area - 1) << 10) | (((horiz_active_area * bpp_mode_size) / 4) - 1); // progressive scan has a 1 since no lines are skipped

    *(volatile uint32_t*)0xa05f80ec = 0x0000004a; // ok: 74 ? 168, default: 0x000000a8 (horiz)
    *(volatile uint32_t*)0xa05f80f0 = 0x000e000e; // ok: 14 & 14 ? 40 & 40, default: 0x00280028 (vert)
    *(volatile uint32_t*)0xa05f80c8 = 0x02ca0000; // ok: 714 ? 837, default: 0x03450000 (horiz)
    *(volatile uint32_t*)0xa05f80cc = 0x000e01ee; // ok: 14, 494 ? 21, 520, default: 0x00150208 (vert)
    *(volatile uint32_t*)0xa05f80d0 = 0x00000100;
    *(volatile uint32_t*)0xa05f80d4 = 0x004a02ca; // ok: 74, 714 ? 126, 837, default: 0x007e0345 (horiz)
    *(volatile uint32_t*)0xa05f80d8 = 0x01f102d2; // ok: 722x497 (723x498) ? 857x524 (858x525), default: 0x020c0359
    *(volatile uint32_t*)0xa05f80dc = 0x000e01ee; // ok: 14, 494 ? 40, 520, default: 0x00280208 (vert)
    *(volatile uint32_t*)0xa05f80e0 = 0x01eb281f; // ok: 7 (8,- 1), 690 (723 - 32,- 1), 8 (CVT), 31 (32,- 1) ? 15, 793, 3, 63, default: 0x03f1933f

    uint32_t scan_area_size = horiz_active_area * vert_active_area;
    uint32_t scan_area_size_bytes = scan_area_size * bpp_mode_size; // This will always be divisible by 4

    // Reset framebuffer address
    *(volatile uint32_t*)0xa05f8050 = 0x00000000; // BootROM sets this to 0x00200000 (framebuffer base is 0xa5000000 + this)
    *(volatile uint32_t*)0xa05f8054 = 0x00000000; // Same for progressive, resetting the offset gets us 2MB VRAM back after BootROM is done with it

    // zero out framebuffer area
    for(uint32_t pixel_or_two = 0; pixel_or_two < scan_area_size_bytes; pixel_or_two += 4)
    {
      *(uint32_t*)(0xa5000000 + pixel_or_two) = 0;
    }

    // re-enable video
    *(volatile uint32_t*)0xa05f80e8 &= ~8;
    *(volatile uint32_t*)0xa05f8044 |= 1;
  }
}

// 848x480 @ 60Hz (16:9, CVT, RBv2) - PVR 32x32
// Horizontal scale: 1.0x (well, ok, 0.981132075x, but that's 16/832 pixels of stretch, or 2% per line)
// Framebuffer: 832x480
// This is basically a native widescreen mode.
// Only took 15 years for the right standard to come along!
void STARTUP_848x480_VGA_CVT_RBv2(uint8_t fbuffer_color_mode)
{
  // Set global scale factors
#ifdef WIDESCREEN_SCALE_1X
  STARTUP_video_params.video_scale = 1.0f;
  STARTUP_video_params.video_scale_multiplier = 1.0f;
#else
  STARTUP_video_params.video_scale = 848.0f / 832.0f;
  STARTUP_video_params.video_scale_multiplier = 832.0f / 848.0f;
#endif

  // Set global video output mode parameters
  STARTUP_video_params.video_width = 848;
  STARTUP_video_params.video_height = 480;
  STARTUP_video_params.video_color_type = fbuffer_color_mode;
  STARTUP_video_params.video_refresh_rate = 60;

  // 928 wide scaled to 909 wide (field)
  // 848 wide scaled to 832 wide (visible frame)
  // Perfect for the DC because the DC does 32x32 tiles.
  uint32_t horiz_active_area = 832;
  uint32_t vert_active_area = 480;
  // {RGB0555, RGB565} = 2Bpp, {RGB888} = 3Bpp, {RGB0888} = 4Bpp
  uint32_t bpp_mode_size = fbuffer_color_mode + 1 + (0x1 ^ ((fbuffer_color_mode & 0x1) | (fbuffer_color_mode >> 1))); // Add another 1 only if 0b00

  // Set global framebuffer parameters
  STARTUP_video_params.fb_width = horiz_active_area;
  STARTUP_video_params.fb_height = vert_active_area;
  STARTUP_video_params.fb_color_bytes = bpp_mode_size;

  if(!cable_mode) // VGA 848x480 @ 60Hz
  {
    *(volatile uint32_t*)0xa05f80e8 = 0x00160008;
    *(volatile uint32_t*)0xa05f8044 = 0x00800000 | (fbuffer_color_mode << 2);

    *(volatile uint32_t*)0xa05f804c = (horiz_active_area * bpp_mode_size) / 8; // for PVR to know active area width
    *(volatile uint32_t*)0xa05f8040 = 0x00000000; // Border color in RGB0888 format (this mode has no border)
    *(volatile uint32_t*)0xa05f805c = (1 << 20) | ((vert_active_area - 1) << 10) | (((horiz_active_area * bpp_mode_size) / 4) - 1); // progressive scan has a 1 since no lines are skipped

    *(volatile uint32_t*)0xa05f80ec = 0x00000045; // ok: 69 ? mod: 68. CVT-RBv2: 72. Using 10 (no, 20).. Using 7 (no, 23). 168, default: 0x000000a8 (horiz)
    *(volatile uint32_t*)0xa05f80f0 = 0x000e000e; // ok: 14 & 14 ? new: 11 & 11.. CVT-RBv2: 14 & 14. Using 31 & 31.. 40 & 40, default: 0x00280028 (vert)
    *(volatile uint32_t*)0xa05f80c8 = 0x03850000; // ok: 901 ? mod: 900. CVT-RBv2: 920. Using 858 (no, 868; nah, 871).. Using 855. 837, default: 0x03450000 (horiz)
    *(volatile uint32_t*)0xa05f80cc = 0x000e01ee; // ok: 14, 494 ? new: 11, 491.. CVT-RBv2: 14, 494. Using 21, 511.. Using 21, 512. 21, 520, default: 0x00150208 (vert)
    *(volatile uint32_t*)0xa05f80d0 = 0x00000100;
    *(volatile uint32_t*)0xa05f80d4 = 0x00450385; // ok: 69, 901 ? mod: 68, 900. CVT-RBv2: 72, 920. Using 10, 858 (no, 20, 868; nah, 23, 871).. Using 7, 855. 126, 837, default: 0x007e0345 (horiz)
    *(volatile uint32_t*)0xa05f80d8 = 0x01ee038c; // ok: 908x494 (909x495) ? new: 906x493 (907x494).. mod: 906x494 (907x495) CVT-RBv2: 927x494 (928x495). May need to fudge with horiz scale for 27MHz. VESA: 1088x517, our max is 870x516 (871x517). 857x524 (858x525), default: 0x020c0359
    *(volatile uint32_t*)0xa05f80dc = 0x000e01ee; // ok: 14, 494 ? new: 11, 491.. CVT-RBv2: 14, 494. Using 31, 511.. Using 40, 512. 40, 520, default: 0x00280208 (vert)
    *(volatile uint32_t*)0xa05f80e0 = 0x01f6d81e; // ok: 7 (8,- 1), 877 (909 - 31,- 1), 8 (CVT), 30 (31,- 1) ? new: 7, 875 (907 - 31 - 1), 5 (for 16:9), 30.. mod: 7, 875 (907 - 31 - 1), 8, 30. CVT-RBv2: 15, 895 (928 - 32 - 1), 8, 31. Using 15, 854, 8, 15.. Using 15, 854, 3, 15. 15, 793, 3, 63, default: 0x03f1933f

    uint32_t scan_area_size = horiz_active_area * vert_active_area;
    uint32_t scan_area_size_bytes = scan_area_size * bpp_mode_size; // This will always be divisible by 4

    // Reset framebuffer address
    *(volatile uint32_t*)0xa05f8050 = 0x00000000; // BootROM sets this to 0x00200000 (framebuffer base is 0xa5000000 + this)
    *(volatile uint32_t*)0xa05f8054 = 0x00000000; // Same for progressive, resetting the offset gets us 2MB VRAM back after BootROM is done with it

    // zero out framebuffer area
    for(uint32_t pixel_or_two = 0; pixel_or_two < scan_area_size_bytes; pixel_or_two += 4)
    {
      *(uint32_t*)(0xa5000000 + pixel_or_two) = 0;
    }

    // re-enable video
    *(volatile uint32_t*)0xa05f80e8 &= ~8;
    *(volatile uint32_t*)0xa05f8044 |= 1;
  }
}
