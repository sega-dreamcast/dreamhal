// ---- startup_support.h - Dreamcast Startup Support Module Header ----
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
// This file contains some definitions and prototypes for startup_support.c.
// See that file for more details.
//

#ifndef __STARTUP_SUPPORT_H_
#define __STARTUP_SUPPORT_H_

#include <stdint.h>

//==============================================================================
// System Support
//==============================================================================

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

#define CABLE_TYPE_VGA 0
#define CABLE_TYPE_UNDEFINED 1
#define CABLE_TYPE_RGB 2
#define CABLE_TYPE_CVBS 3

// Set by STARTUP_Init_Video().
// This is a global cable type value for user reference (0 = VGA, 2 = RGB, 3 = Composite/S-Video)
extern uint32_t STARTUP_cable_type;

#define CONSOLE_REGION_JP 0
#define CONSOLE_REGION_NA 1
#define CONSOLE_REGION_PAL 2

// Set by STARTUP_Init_Video().
// This is a global console region type for user reference (0 = JP, 1 = NA, 2 = PAL)
extern uint32_t STARTUP_console_region;

//==============================================================================
// FPSCR Support
//==============================================================================
//
// GCC deprecated __set_fpscr and __get_fpscr and replaced them with builtins.
// Built-in functions can't be called directly by ASM, so these little wrappers
// can be called instead to do the job.
//
// void __builtin_sh_set_fpscr(uint32_t val) doesn't affect SZ, PR, and FR,
// unlike the old void __set_fpscr(uint32_t val) macro.
//
// uint32_t __builtin_sh_get_fpscr(void), on the other hand, behaves the same as
// the old uint32_t __get_fpscr(void) macro.
//
// Also: the old macros were in libgcc, and the new ones are not (yay!).
// These new macros can't be called by ASM because of that, however, hence the
// need for these simple wrappers, which can be called by ASM without issue.

void __call_builtin_sh_set_fpscr(uint32_t value);

uint32_t __call_builtin_sh_get_fpscr(void);

//==============================================================================
// Video Support
//==============================================================================
//
// Various parameters for setting up and working with video output
//

//------------------------------------------------------------------------------
// Video Utility Definitions
//------------------------------------------------------------------------------
//
// Global definitions for video parameters and color conversions
//

// The PAL (576i) standard allows for a larger video frame than NTSC (480i).
// Normally it's 96 more lines, but the Dreamcast can only do half that (48)
// when using BootROM-style video timing. This flag enables those extra lines:
// 48 in 576i mode (aka PAL 640x480) for a 528 line frame, and 24 in 288p mode
// (aka PAL 320x240) for a 264 line frame. The result is a 640x528 or 320x264
// active frame, which a developer may want to use to hide secret messages or
// to put subtitles for PAL users, for example.
//
// Since the framebuffer only extends out to cover those extra lines in PAL mode
// with this flag enabled, after video mode initialization occurs a static image
// could also be placed right at the edge of the NTSC/VGA framebuffer. Such an
// image would only be displayed on PAL region machines.
//#define PAL_EXTRA_LINES

// These definitions convert values to 16-bit RGB formats

// Make a uint16_t from RGB565 values
#define RGB565_TO_16(r, g, b) ( (r << 11) | (g << 5) | (b) )
// Convert 8 Bpp RGB to RGB565 format
#define RGB565_TO_16_SCALED(r, g, b) RGB565_TO_16(r >> 3, g >> 2, b >> 3)

// Make a uint16_t from RGB0555 values
#define RGB0555_TO_16(r, g, b) (0x0 + ( (r << 10) | (g << 5) | (b) ))
// Convert 8 Bpp RGB to RGB0555 format
#define RGB0555_TO_16_SCALED(r, g, b) RGB0555_TO_16(r >> 3, g >> 3, b >> 3)

// The structs are useful for addressing the framebuffer
// Using uint16_t types instead of uint8_t because GCC complains about bitfields
// crossing type boundaries

typedef struct __attribute__((packed)) {
  uint16_t blue : 5;
  uint16_t green : 5;
  uint16_t red : 5;
  uint16_t zero : 1;
} RGB0555_PIXEL;

typedef struct __attribute__((packed)) {
  uint16_t blue : 5;
  uint16_t green : 6;
  uint16_t red : 5;
} RGB565_PIXEL;

typedef struct __attribute__((packed)) {
  uint8_t blue;
  uint8_t green;
  uint8_t red;
} RGB888_PIXEL;

typedef struct __attribute__((packed)) {
  uint8_t blue;
  uint8_t green;
  uint8_t red;
  uint8_t zero;
} RGB0888_PIXEL;

// Global structure that keeps track of current video mode parameters
// Consider this read-only; it gets overwritten every time the video mode changes.
typedef struct {
  // Keeps track of the horizontal framebuffer scale factor for reference
  // Since division is slow, multiply by this number to divide by the video scale
  float video_scale_multiplier;
  // This is the inverse of video_scale_multiplier and tracks by how much the output
  // image will be stretched from the framebuffer.
  float video_scale;

  // Current resolution (in pixels) and color depth
  uint32_t video_width;
  uint32_t video_height;
  // In Hz
  uint32_t video_refresh_rate;
  // RGB0555 = 0, RGB565 = 1, RGB888 = 2, RGB0888 = 3
  uint32_t video_color_type;

  // Current framebuffer resolution (in pixels) and color depth
  uint32_t fb_width;
  uint32_t fb_height;
  // {RGB0555, RGB565} = 2Bpp, {RGB888} = 3Bpp, {RGB0888} = 4Bpp
  uint32_t fb_color_bytes;
} VIDEO_PARAMS_STRUCT;

extern VIDEO_PARAMS_STRUCT STARTUP_video_params;

//------------------------------------------------------------------------------
// Video Mode Setup
//------------------------------------------------------------------------------
//
// Given a desired framebuffer color mode, set up everything related to Dreamcast
// video modes according to the cable type and console region.
//
// Specify "use_320x240" to use a 320x240 mode appropriate for the cable type
// and region, otherwise the video mode will be the appropriate variant of
// 640x480 (it's really 720x480, by the way, using standard CTA-861 480p timings).
//
// Supported output formats are VGA, NTSC (480i), and PAL (576i). 320x240 gets
// linedoubled + pixeldoubled to 640x480 for compatibility, in particular to
// allow software to look the same size and use the same assets between VGA and
// interlaced modes.
//
// STARTUP_Init_Video() MUST be called by a program once at startup to get
// everything set up the first time. Use STARTUP_Set_Video() for any runtime
// changes. STARTUP_Init_Video() will also set the cable_type and region variables.
//
// The framebuffer address will always be 0xa5000000 after either runs.
//

// These definitions correspond to 'fbuffer_color_mode' in the video setup and
// mode setting functions.
// 16-bit
#define FB_RGB0555 0
// 16-bit
#define FB_RGB565 1
// 24-bit
#define FB_RGB888 2
// 32-bit
#define FB_RGB0888 3

// For "use_320x240" in STARTUP_Init_Video() and STARTUP_Set_Video()
#define USE_640x480 0
#define USE_320x240 1

void STARTUP_Init_Video(uint8_t fbuffer_color_mode, uint8_t use_320x240);
void STARTUP_Set_Video(uint8_t fbuffer_color_mode, uint8_t use_320x240);

//------------------------------------------------------------------------------
// Extra Video Modes
//------------------------------------------------------------------------------
//
// These can be used to set up extra video modes after running STARTUP_Init_Video().
//
// Notes:
// - The framebuffer address will always be set to 0xa5000000 after any of these run.
// - Modes marked "PVR 32x32" have a framebuffer that is an integer multiple of 32x32.
// - Some of the raw modes are naturally 32x32-aligned, but some are not. A function
// with "_PVR" appended to its name means it is a modified version of a non-multiple
// raw timing meant to work with the Tile Accelerator's 32x32 size constraint.
//
//
// -- About These Modes --
//
// The Dreamcast is actually able to output higher resolutions over VGA by taking
// advantage of the properties of analog display signalling. Namely, analog
// signals are measured in terms of time a signal spends high or low, and this
// can be used to make a slow pixel clock appear to output a higher rate than it
// should otherwise be capable of doing.
//
// This works because digital displays like LCDs use an analog-to-digital
// converter (ADC) to convert analog signals into something they can use. ADCs
// work by sampling the RGB signals at specific rates, and each sample then
// becomes a pixel. By ensuring the signal timing is correct, and that the LCD
// is expecting a higher resolution, we can trick the ADC into sampling the same
// spot multiple times. What this does is stretch something like 320 pixels (as
// the Dreamcast sees it) into 1280 (as the monitor sees it).
//
// An LCD is made to expect a higher resolution by using the right number of
// total vertical lines (so, including blanking), since in analog display
// signalling vertical is the "master" signal and horizontal is just whatever
// fits between the vertical line signal pulses. That's the core of how this
// trick works, and also why the resolutions have an exact vertical line count
// despite using weird horizontal pixel counts.
//
// A CRT, by contrast, just keeps moving the electron gun and "smears" the screen
// for a bit longer per pixel, since the Dreamcast is feeding the expected timings
// for a higher resolution. A CRT may need its picture to be manually widened if
// the image comes out looking squished on it, however.
//
// The way to calculate the horizontal pixel count is simple: in VGA mode the
// Dreamcast has a 27MHz clock, so you just do
//
//   (horizontal parameter) * 27MHz / (DMT pixel clock for desired resolution)
//
// And the vertical line parameters stay the same as each resolution standard
// asks for. This does mean that, because the Dreamcast video size registers max
// at 10 bits in size, resolutions with vertical lines over 1024 (including
// blanking) are not possible to output. But we can do 60Hz 1280x960 and 720p!
//
// So, in simple terms, this ultimately all just means that the signal coming
// out over the VGA cable is a valid signal for the higher resolution, but the
// Dreamcast thinks it's outputting something else. For example, a 1024x768
// signal coming out of the Dreamcast looks like 425x768 internally, since that's
// just what it takes to make the signals line up in the right places.
//
//
// -- Overview of Available Modes --
//
// "PVR 32x32" means the framebuffer is a multiple of 32x32. Modes without that
// label are "raw modes" and have no blank pixels or extra pixels, which may be
// useful for compatibility testing or software renderers. Modes that only have
// a PVR 32x32 version are actually raw modes with framebuffers already multiples
// of 32x32, and no extra modification was needed for them.
//
// 60Hz:
//
// 848x480 @ 60Hz (16:9, DMT, but using a slightly-too-short hsync)
// 848x480 @ 60Hz (16:9, DMT, but using a slightly-too-short hsync) - PVR 32x32
// 800x600 @ 60Hz (4:3, DMT, but using a slightly-too-short hsync)
// 800x600 @ 60Hz (4:3, DMT, but using a slightly-too-short hsync) - PVR 32x32
// 800x600 @ 60Hz (4:3, CVT)
// 800x600 @ 60Hz (4:3, CVT) - PVR 32x32
// 1024x768 @ 60Hz (4:3, DMT)
// 1024x768 @ 60Hz (4:3, DMT) - PVR 32x32
// 1152x864 @ 60Hz (4:3, CVT)
// 1152x864 @ 60Hz (4:3, CVT) - PVR 32x32
// 720p60 (16:9, DMT & CTA-861) - for HDTVs
// 720p60 (16:9, DMT & CTA-861) - for HDTVs - PVR 32x32
// 1280x720 (16:9, CVT) - for monitors that need this instead of HDTV 720p60
// 1280x720 (16:9, CVT) - for monitors that need this instead of HDTV 720p60 - PVR 32x32
// 1280x800 @ 60Hz (16:10, DMT & CVT)
// 1280x800 @ 60Hz (16:10, DMT & CVT) - PVR 32x32
// 1280x960 @ 60Hz (4:3, DMT) - PVR 32x32
// 1440x900 @ 60Hz (16:10, DMT & CVT)
// 1440x900 @ 60Hz (16:10, DMT & CVT) - PVR 32x32
//
// 75Hz:
//
// 640x480 @ 75Hz (4:3, DMT)
// 640x480 @ 75Hz (4:3, DMT) - PVR 32x32
// 800x600 @ 75Hz (4:3, DMT)
// 800x600 @ 75Hz (4:3, DMT) - PVR 32x32
// 1024x768 @ 75Hz (4:3, DMT) - PVR 32x32
// 1152x864 @ 75Hz (4:3, DMT) - PVR 32x32
//
// 120Hz (LCD Only - Untested):
//
// 480p @ 120Hz (4:3, CTA-861, 720x480) - for HDTVs - PVR 32x32
// 640x480 @ 120Hz (4:3, CVT, RB) - for monitors that need this instead of HDTV 480p120
// 640x480 @ 120Hz (4:3, CVT, RB) - PVR 32x32
// 800x600 @ 120Hz (4:3, DMT & CVT, RB)
// 1024x768 @ 120Hz (4:3, DMT & CVT, RB)
//
// 240Hz (LCD Only - Untested):
//
// 480p @ 240Hz (4:3, CTA-861, 720x480) - PVR 32x32
// 480p @ 239.76Hz (4:3, CTA-861, 720x480) - PVR 32x32
//
// CVT RBv2 Native Modes (LCD Only, probably need one newer than 2013):
//
// 640x480 @ 75Hz (4:3, CVT, RBv2) - PVR 32x32
// 848x480 @ 60Hz (16:9, CVT, RBv2) - PVR 32x32**
//
// ** Well, ok, this one's actually 832x480 internally, but the horizontal
// "stretch" is just 2%. It's the closest we can get to a real native 16:9 mode,
// anyways, because the next step up is 864 wide... Which is too big for 848x480
// to work. So if you're looking for a true native widescreen, this is about as
// good as it gets.
//
//
// -- Scaling Formula --
//
// Simple! Just use this formula to correctly squish the image horizontally:
//
//  (float or int) scaled_hsize = (float or int) round_to_nearest_int((float)actual_hsize * STARTUP_video_params.video_scale_multiplier)
//
// where "round_to_nearest_int()" might be floor(), ceil(), floorf(), or ceilf()
// depending on whether you need an int or a float and depending on the mode's
// scale. The easy way to check is to multiply the mode's horizontal output
// resolution (e.g. 1280 in 1280x960) by the horizontal scale (0.25x) and seeing
// whether you need to use floor or ceil (or neither) to get the framebuffer's
// horizontal dimension. Modes with a horizontal scale labeled "(exact)" may not
// need to be rounded. The vertical dimension does not need to be scaled.
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
void STARTUP_848x480_VGA(uint8_t fbuffer_color_mode);

// 848x480 @ 60Hz (16:9, DMT, but using a slightly-too-short hsync) - PVR 32x32
// Framebuffer: 672x480
// Horizontal scale: 0.79245283x
// Same caveat as 848x480 VGA, but this mode has been shrunken by 6 columns for
// 32x32 framebuffer compatibility. As a result, there may be 6 total columns of
// blank pixels on the horizontal sides.
void STARTUP_848x480_VGA_PVR(uint8_t fbuffer_color_mode);

// 800x600 @ 60Hz (4:3, DMT, but using a slightly-too-short hsync)
// Framebuffer: 540x600
// Horizontal scale: 0.675x (exact)
// DMT specifies a 3.20usec hsync width, which is 86 Dreamcast pixels when scaled
// for 800x600. The Dreamcast maxes at an hsync width of 64 pixels, which is
// 2.37usec. This may not really cause a problem (no issues with any of the 5
// different LCDs I tried), but it is worth pointing out.
void STARTUP_800x600_VGA(uint8_t fbuffer_color_mode);

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
void STARTUP_800x600_VGA_PVR(uint8_t fbuffer_color_mode);

// 800x600 @ 60Hz (4:3, CVT)
// Framebuffer: 565x600
// Horizontal scale: 0.70625x (exact)
// This won't work with all monitors due to Dreamcast sync polarity: Dreamcast
// sync polarity has to be -h -v or the video DAC won't work, while CVT asks for
// -h +v polarity. Some monitors don't care, some do; those that do may think
// this signal is 1440x576. This is why most of these modes are following DMT
// standards, since DMT modes can be either -h -v or +h +v and it doesn't really
// matter which.
void STARTUP_800x600_VGA_CVT(uint8_t fbuffer_color_mode);

// 800x600 @ 60Hz (4:3, CVT) - PVR 32x32
// Framebuffer: 544x608
// Horizontal scale: 0.68x (exact)
// Same caveat as 800x600 VGA CVT.
// This mode has been shrunken by 11 columns and expanded by 8 rows for 32x32
// framebuffer compatibility. As a result, there may be 11 total columns of blank
// pixels on the horizontal sides, and 8 total rows of pixels cut off on the
// vertical sides.
void STARTUP_800x600_VGA_CVT_PVR(uint8_t fbuffer_color_mode);

// 1024x768 @ 60Hz (4:3, DMT)
// Framebuffer: 425x768
// Horizontal scale: 0.415039063x
// This one actually uses negative/negative polarity, too!
void STARTUP_1024x768_VGA(uint8_t fbuffer_color_mode);

// 1024x768 @ 60Hz (4:3, DMT) - PVR 32x32
// Framebuffer: 416x768
// Horizontal scale: 0.40625x (exact)
// This mode has been shrunken by 9 columns for 32x32 framebuffer compatibility.
// As a result, there may be 9 total columns of blank pixels on the horizontal
// sides.
void STARTUP_1024x768_VGA_PVR(uint8_t fbuffer_color_mode);

// 1152x864 @ 60Hz (4:3, CVT)
// Framebuffer: 380x864
// Horizontal scale: 0.329861111x
void STARTUP_1152x864_VGA(uint8_t fbuffer_color_mode);

// 1152x864 @ 60Hz (4:3, CVT) - PVR 32x32
// Framebuffer: 384x864
// Horizontal scale: 0.333333333x (1/3x)
// This mode has been expanded by 4 columns for 32x32 framebuffer compatibility.
// As a result, there may be 4 columns of pixels cut off (2 on either side or 4
// on one side).
void STARTUP_1152x864_VGA_PVR(uint8_t fbuffer_color_mode);

// 720p60 (16:9, DMT & CTA-861) - for HDTVs
// Framebuffer: 465x720
// Horizontal scale: 0.36328125x (exact)
void STARTUP_720p_VGA(uint8_t fbuffer_color_mode);

// 720p60 (16:9, DMT & CTA-861) - for HDTVs - PVR 32x32
// Framebuffer: 448x704
// Horizontal scale: 0.35x (exact)
// This mode has been shrunken by 17 columns and 16 rows for 32x32 framebuffer
// compatibility. As a result, there may be 17 total columns of blank pixels on
// the horizontal sides and 16 rows of blank pixels on the vertical sides.
// Fun fact: The Sega Saturn's max resolution is 704x448, which is the reverse
// of this mode's framebuffer.
void STARTUP_720p_VGA_PVR(uint8_t fbuffer_color_mode);

// 1280x720 (16:9, CVT) - for monitors that need this instead of HDTV 720p60
// Framebuffer: 464x720
// Horizontal scale: 0.3625x (exact)
void STARTUP_1280x720_VGA(uint8_t fbuffer_color_mode);

// 1280x720 (16:9, CVT) - for monitors that need this instead of HDTV 720p60 - PVR 32x32
// Framebuffer: 448x704
// Horizontal scale: 0.35x (exact)
// This mode has been shrunken by 16 columns and 16 rows for 32x32 framebuffer
// compatibility. As a result, there may be 16 total columns of blank pixels on
// the horizontal sides and 16 rows of blank pixels on the vertical sides.
void STARTUP_1280x720_VGA_PVR(uint8_t fbuffer_color_mode);

// 1280x800 @ 60Hz (16:10, DMT & CVT)
// Framebuffer: 414x800
// Horizontal scale: 0.3234375x (exact)
void STARTUP_1280x800_VGA(uint8_t fbuffer_color_mode);

// 1280x800 @ 60Hz (16:10, DMT & CVT) - PVR 32x32
// Framebuffer: 416x800
// Horizontal scale: 0.325x (exact)
// Here, the framebuffer is slightly expanded to 416x800, which is an exact
// multiple of 32x32 for Dreamcast Tile Accelerator compatibility. Note that it
// is possible some monitors may cut off 2 columns of pixels. No idea which side,
// it might even be 1 on each side depending on the monitor.
void STARTUP_1280x800_VGA_PVR(uint8_t fbuffer_color_mode);

// 1280x960 @ 60Hz (4:3, DMT) - PVR 32x32
// Framebuffer: 320x960
// Horizontal scale: 0.25x (exact)
void STARTUP_1280x960_VGA(uint8_t fbuffer_color_mode);

// 1440x900 @ 60Hz (16:10, DMT & CVT)
// Framebuffer: 365x900
// Horizontal scale: 0.253472222x
void STARTUP_1440x900_VGA(uint8_t fbuffer_color_mode);

// 1440x900 @ 60Hz (16:10, DMT & CVT) - PVR 32x32
// Framebuffer: 352x896
// Horizontal scale: 0.244444444x
// This mode has been shrunken by 13 columns and 4 rows for 32x32 framebuffer
// compatibility. As a result, there may be 13 total columns of blank pixels on
// the horizontal sides and 4 rows of blank pixels on the vertical sides.
void STARTUP_1440x900_VGA_PVR(uint8_t fbuffer_color_mode);

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
void STARTUP_640x480_VGA_75(uint8_t fbuffer_color_mode);

// 640x480 @ 75Hz (4:3, DMT) - PVR 32x32
// Framebuffer: 544x480
// Horizontal scale: 0.85x (exact)
// This mode has been shrunken by 4 columns for 32x32 framebuffer compatibility.
// As a result, there may be 4 columns of blank pixels (2 on either side or 4 on
// one side).
void STARTUP_640x480_VGA_75_PVR(uint8_t fbuffer_color_mode);

// 800x600 @ 75Hz (4:3, DMT)
// Framebuffer: 436x600
// Horizontal scale: 0.545x (exact)
void STARTUP_800x600_VGA_75(uint8_t fbuffer_color_mode);

// 800x600 @ 75Hz (4:3, DMT) - PVR 32x32
// Framebuffer: 416x608
// Horizontal scale: 0.52x (exact)
// This mode has been shrunken by 20 columns and expanded by 8 rows for 32x32
// framebuffer compatibility. As a result, there may be 20 total columns of blank
// pixels on the horizontal sides, and 8 total rows of pixels cut off on the
// vertical sides.
void STARTUP_800x600_VGA_75_PVR(uint8_t fbuffer_color_mode);

// 1024x768 @ 75Hz (4:3, DMT) - PVR 32x32
// Framebuffer: 352x768
// Horizontal scale: 0.34375x (exact)
void STARTUP_1024x768_VGA_75(uint8_t fbuffer_color_mode);

// 1152x864 @ 75Hz (4:3, DMT) - PVR 32x32
// Framebuffer 288x864
// Horizontal scale: 0.25x (exact)
// This is actually a very widely supported mode!
void STARTUP_1152x864_VGA_75(uint8_t fbuffer_color_mode);

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
void STARTUP_480p_VGA_120(uint8_t fbuffer_color_mode);

// 640x480 @ 120Hz (4:3, CVT, RB) - for monitors that need this instead of HDTV 480p120
// Framebuffer: 354x480
// Horizontal scale: 0.553125x (exact)
void STARTUP_640x480_VGA_120(uint8_t fbuffer_color_mode);

// 640x480 @ 120Hz (4:3, CVT, RB) - for monitors that need this instead of HDTV 480p120 - PVR 32x32
// Framebuffer: 352x480
// Horizontal scale: 0.55x (exact)
// This mode has been shrunken by 2 columns for 32x32 framebuffer compatibility.
// As a result, there may be 2 columns of blank pixels (1 on either side or 2 on
// one side).
void STARTUP_640x480_VGA_120_PVR(uint8_t fbuffer_color_mode);

// 800x600 @ 120Hz (4:3, DMT & CVT, RB)
// Framebuffer: 295x600
// Horizontal scale: 0.36875x (exact)
void STARTUP_800x600_VGA_120(uint8_t fbuffer_color_mode);

// 1024x768 @ 120Hz (4:3, DMT & CVT, RB)
// Framebuffer: 239x768
// Horizontal scale: 0.233398438x
void STARTUP_1024x768_VGA_120(uint8_t fbuffer_color_mode);

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
void STARTUP_480p_VGA_240(uint8_t fbuffer_color_mode);

// 480p @ 239.76Hz (4:3, CTA-861, 720x480) - PVR 32x32
// Framebuffer: 160x480
// Horizontal scale: 0.25x (exact)
void STARTUP_480p_VGA_239(uint8_t fbuffer_color_mode);

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
void STARTUP_640x480_VGA_75_CVT_RBv2(uint8_t fbuffer_color_mode);

// For 848x480 VGA CVT-RBv2:
// Use this flag to set video_scale_multiplier to either 1.0x or 0.981132075x.
// video_scale will also change accordingly.
// Enable for 1.0x, comment out for 0.981132075x
#define WIDESCREEN_SCALE_1X

// 848x480 @ 60Hz (16:9, CVT, RBv2) - PVR 32x32
// Horizontal scale: 1.0x (well, ok, 0.981132075x, but that's 16/832 pixels of stretch, or 2% per line)
// Framebuffer: 832x480
// This is basically a native widescreen mode.
// Only took 15 years for the right standard to come along!
void STARTUP_848x480_VGA_CVT_RBv2(uint8_t fbuffer_color_mode);

#endif
