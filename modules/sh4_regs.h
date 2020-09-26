// ---- sh4_regs.h - SH4 System Registers Module ----
//
// Version 1.0.1
//
// This file is part of the DreamHAL project, a hardware abstraction library
// primarily intended for use on the SH7091 found in hardware such as the SEGA
// Dreamcast game console.
//
// This module provides a complete map of all SH4 hardware registers based on the
// Address List appendix of the Renesas SH7750 group hardware manual. The module
// is hereby released into the public domain in the hope that it may prove useful.
//
// NOTE: This module may conflict with other modules in DreamHAL and cause
// "multiple definition" compiler errors, as each module in DreamHAL is designed
// for standalone use and already contain their respective register definitions.
//
// --Moopthehedgehog, April 2020
//
// Notes:
// - Each register is provided with its name, address, and access size (in bits).
// - All registers are assumed Read-Write (R/W) unless otherwise noted.
// - All registers defined here can only be accessed by the CPU in privileged mode.
// - Special memory areas, for example the Store Queue area and On-Chip RAM, are
//  also included. Note that most of these memory areas have access size
//  restrictions, as well.
//

#ifndef __SH4_REGISTERS_H_
#define __SH4_REGISTERS_H_

//==============================================================================
// Misc System Registers
//==============================================================================

// Processor Version Register
#define SYS_PVR 0xFF000030  // 32 (R Only)
// Cache Version Register (undocumented)
// Note: Good luck finding much information on this register. All I ever found was
// its name and that it exists.
#define SYS_CVR 0xFF000040  // 32 (R Only)
// Product Register
#define SYS_PRR 0xFF000044  // 32 (R Only)

//==============================================================================
// Memory Management Unit (MMU) Registers
//==============================================================================

// Page Table Configuration
#define MMU_PTEH 0xFF000000 // 32
#define MMU_PTEL 0xFF000004 // 32
#define MMU_TTB 0xFF000008  // 32
#define MMU_TEA 0xFF00000C  // 32
#define MMU_PTEA 0xFF000034 // 32

// MMU Configuration
#define MMU_MMUCR 0xFF000010  // 32

//
// MMU-Related Memory Areas
//

// ITLB Address Array
#define MMU_ITLB_ADDRESS_AREA_BASE 0xF2000000 // 32
#define MMU_ITLB_ADDRESS_AREA_SIZE 0x01000000 // 16MB region

// ITLB Data Arrays
#define MMU_ITLB_DATA_AREA_1_BASE 0xF3000000  // 32
#define MMU_ITLB_DATA_AREA_1_SIZE 0x00800000  // 8MB region
#define MMU_ITLB_DATA_AREA_2_BASE 0xF3800000  // 32
#define MMU_ITLB_DATA_AREA_2_SIZE 0x00800000  // 8MB region

// UTLB Address Array
#define MMU_UTLB_ADDRESS_AREA_BASE 0xF6000000 // 32
#define MMU_UTLB_ADDRESS_AREA_SIZE 0x01000000 // 16MB region

// UTLB Data Arrays
#define MMU_UTLB_DATA_AREA_1_BASE 0xF7000000  // 32
#define MMU_UTLB_DATA_AREA_1_SIZE 0x00800000  // 8MB region
#define MMU_UTLB_DATA_AREA_2_BASE 0xF7800000  // 32
#define MMU_UTLB_DATA_AREA_2_SIZE 0x00800000  // 8MB region

//==============================================================================
// Cache Controller (CCN) Registers and Special Memory Areas
//==============================================================================

// Cache Controller Configuration
#define CCN_CCR 0xFF00001C    // 32

// Store Queues
#define CCN_QACR0 0xFF000038  // 32
#define CCN_QACR1 0xFF00003C  // 32

// Store Queue Memory Area
#define CCN_SQ_AREA_BASE 0xE0000000 // 32 and 64 (via fmov.d)
#define CCN_SQ_AREA_SIZE 0x04000000 // 64MB region

// On-Chip RAM Memory Area
#define CCN_OCRAM_AREA_BASE 0x7C000000  // 8, 16, 32, and 64 (via fmov.d)
// Actual OCRAM size depends on CPU's Operand Cache size (it's half the cache),
// however the entire 64MB region is reserved for OCRAM and OCRAM image areas
#define CCN_OCRAM_AREA_SIZE 0x04000000 // 64MB region
// NOTE: OCRAM area is NOT contiguous!
// See section 4.3.6 "RAM Mode" in the SH7750 hardware manual
#define CCN_OCRAM_SIZE_8kB 8192
//#define CCN_OCRAM_SIZE_16kB 16384 // SH7750R Only

//
// Cache-Related Memory Areas
//

// Instruction Cache Address Array
#define CCN_ICACHE_ADDRESS_AREA_BASE 0xF0000000 // 32
#define CCN_ICACHE_ADDRESS_AREA_SIZE 0x01000000 // 16MB region

// Instruction Cache Data Array
#define CCN_ICACHE_DATA_AREA_BASE 0xF1000000  // 32
#define CCN_ICACHE_DATA_AREA_SIZE 0x01000000  // 16MB region

// Operand Cache Address Array
#define CCN_OCACHE_ADDRESS_AREA_BASE 0xF4000000 // 32
#define CCN_OCACHE_ADDRESS_AREA_SIZE 0x01000000 // 16MB region

// Operand Cache Data Array
#define CCN_OCACHE_DATA_AREA_BASE 0xF5000000  // 32
#define CCN_OCACHE_DATA_AREA_SIZE 0x01000000  // 16MB region

//==============================================================================
// Processor Exception (EXPT) Registers
//==============================================================================

// TRAPA Exception Data
#define EXPT_TRA 0xFF000020    // 32
// Processor Exception Event Codes
#define EXPT_EXPEVT 0xFF000024 // 32
// Interrupt Event Codes
#define EXPT_INTEVT 0xFF000028 // 32

//==============================================================================
// User Break Controller (UBC) Registers
//==============================================================================

// Channel A
#define UBC_BASRA 0xFF000014  // 8

#define UBC_BARA 0xFF200000   // 32
#define UBC_BAMRA 0xFF200004  // 8
#define UBC_BBRA 0xFF200008   // 16

// Channel B
#define UBC_BASRB 0xFF000018  // 8

#define UBC_BARB 0xFF20000C   // 32
#define UBC_BAMRB 0xFF200010  // 8
#define UBC_BBRB 0xFF200014   // 16

// Channel B Data (Channel A doesn't have data regs)
#define UBC_BDRB 0xFF200018   // 32
#define UBC_BDMRB 0xFF20001C  // 32
#define UBC_BRCR 0xFF200020   // 16

//==============================================================================
// Bus State Controller (BSC) Registers
//==============================================================================

// Bus Control
#define BSC_BCR1 0xFF800000 // 32
#define BSC_BCR2 0xFF800004 // 16
//#define BSC_BCR3 0xFF800050 // 16 - SH7750R Only
//#define BSC_BCR4 0xFE0A00F0 // 32 - SH7750R Only

// Wait Control
#define BSC_WCR1 0xFF800008 // 32
#define BSC_WCR2 0xFF80000C // 32
#define BSC_WCR3 0xFF800010 // 32

// Memory Control
#define BSC_MCR 0xFF800014  // 32

// PCMCIA Control
#define BSC_PCR 0xFF800018  // 16

// Refresh Timer Control
#define BSC_RTCSR 0xFF80001C  // 16
#define BSC_RTCNT 0xFF800020  // 16
#define BSC_RTCOR 0xFF800024  // 16
#define BSC_RFCR 0xFF800028   // 16

// Port A GPIO
#define BSC_PCTRA 0xFF80002C  // 32
#define BSC_PDTRA 0xFF800030  // 16

// Port B GPIO
#define BSC_PCTRB 0xFF800040  // 32
#define BSC_PDTRB 0xFF800044  // 16

// GPIO Interrupt Control
#define BSC_GPIOIC 0xFF800048  // 16

// Synchronous DRAM Mode Registers
// NOTE: These registers are unusual in that they use the address bus for writes!
// See section 13.2.10 "Synchronous DRAM Mode Register" in the SH7750 hardware manual
#define BSC_SDMR2 0xFF900000  // 8 (W Only)
#define BSC_SDMR3 0xFF940000  // 8 (W Only)

//==============================================================================
// Direct Memory Access Controller (DMAC) Registers
//==============================================================================

// Channel 0
#define DMAC_SAR0 0xFFA00000    // 32
#define DMAC_DAR0 0xFFA00004    // 32
#define DMAC_DMATCR0 0xFFA00008 // 32
#define DMAC_CHCR0 0xFFA0000C   // 32

// Channel 1
#define DMAC_SAR1 0xFFA00010    // 32
#define DMAC_DAR1 0xFFA00014    // 32
#define DMAC_DMATCR1 0xFFA00018 // 32
#define DMAC_CHCR1 0xFFA0001C   // 32

// Channel 2
#define DMAC_SAR2 0xFFA00020    // 32
#define DMAC_DAR2 0xFFA00024    // 32
#define DMAC_DMATCR2 0xFFA00028 // 32
#define DMAC_CHCR2 0xFFA0002C   // 32

// Channel 3
#define DMAC_SAR3 0xFFA00030    // 32
#define DMAC_DAR3 0xFFA00034    // 32
#define DMAC_DMATCR3 0xFFA00038 // 32
#define DMAC_CHCR3 0xFFA0003C   // 32

// DMA Operation Register
#define DMAC_DMAOR 0xFFA00040   // 32

//
// SH7750R Only
//

// Channel 4
//#define DMAC_SAR4 0xFFA00050    // 32
//#define DMAC_DAR4 0xFFA00054    // 32
//#define DMAC_DMATCR4 0xFFA00058 // 32
//#define DMAC_CHCR4 0xFFA0005C   // 32

// Channel 5
//#define DMAC_SAR5 0xFFA00060    // 32
//#define DMAC_DAR5 0xFFA00064    // 32
//#define DMAC_DMATCR5 0xFFA00068 // 32
//#define DMAC_CHCR5 0xFFA0006C   // 32

// Channel 6
//#define DMAC_SAR6 0xFFA00070    // 32
//#define DMAC_DAR6 0xFFA00074    // 32
//#define DMAC_DMATCR6 0xFFA00078 // 32
//#define DMAC_CHCR6 0xFFA0007C   // 32

// Channel 7
//#define DMAC_SAR7 0xFFA00080    // 32
//#define DMAC_DAR7 0xFFA00084    // 32
//#define DMAC_DMATCR7 0xFFA00088 // 32
//#define DMAC_CHCR7 0xFFA0008C   // 32

//==============================================================================
// Clock Pulse Generator (CPG) Registers
//==============================================================================

// Frequency Control Register
#define CPG_FRQCR 0xFFC00000  // 16

// Power-Down/Standby Control Registers
#define CPG_STBCR 0xFFC00004  // 8
#define CPG_STBCR2 0xFFC00010 // 8
// These SH7750R registers only apply to timer channels 3 and 4 in power-down states
//#define CPG_CLKSTP00 0xFE0A0000    // 32 - SH7750R Only
//#define CPG_CLKSTPCLR00 0xFE0A0008 // 32 (W Only) - SH7750R Only

// Watchdog Timer
// NOTE: These register require special handling when being written to.
// See section 10.8 "WDT Registers" in the SH7750 hardware manual
#define CPG_WTCNT 0xFFC00008  // 8 (R) / 16 (W)
#define CPG_WTCSR 0xFFC0000C  // 8 (R) / 16 (W)

//==============================================================================
// Realtime Clock (RTC) Registers
//==============================================================================

// 64Hz Counter
#define RTC_R64CNT 0xFFC80000   // 8 (R Only)

// Counters
#define RTC_RSECCNT 0xFFC80004  // 8
#define RTC_RMINCNT 0xFFC80008  // 8
#define RTC_RHRCNT 0xFFC8000C   // 8
#define RTC_RWKCNT 0xFFC80010   // 8
#define RTC_RDAYCNT 0xFFC80014  // 8
#define RTC_RMONCNT 0xFFC80018  // 8
#define RTC_RYRCNT 0xFFC8001C   // 16

// Alarms
#define RTC_RSECAR 0xFFC80020   // 8
#define RTC_RMINAR 0xFFC80024   // 8
#define RTC_RHRAR 0xFFC80028    // 8
#define RTC_RWKAR 0xFFC8002C    // 8
#define RTC_RDAYAR 0xFFC80030   // 8
#define RTC_RMONAR 0xFFC80034   // 8
//#define RTC_RYRAR 0xFFC80054    // 16 - SH7750R Only

// RTC Control Registers
#define RTC_RCR1 0xFFC80038   // 8
#define RTC_RCR2 0xFFC8003C   // 8
//#define RTC_RCR3 0xFFC80050   // 8 - SH7750R Only

//==============================================================================
// Interrupt Controller (INTC) Registers
//==============================================================================

// Interrupt Control Register
#define INTC_ICR 0xFFD00000   // 16

// Interrupt Priority Control
#define INTC_IPRA 0xFFD00004  // 16
#define INTC_IPRB 0xFFD00008  // 16
#define INTC_IPRC 0xFFD0000C  // 16
//#define INTC_IPRD 0xFFD00010  // 16 - SH7750S and SH770R Only

//
// SH7750R Only
//

// Extra Interrupt Control Registers
// These are only defined for timer channels 3 and 4
//#define INTC_INTPRI00 0xFE080000    // 32
//#define INTC_INTREQ00 0xFE080020    // 32 (R Only)
//#define INTC_INTMSK00 0xFE080040    // 32
//#define INTC_INTMSKCLR00 0xFE080060 // 32 (W Only)

//==============================================================================
// Timer Unit (TMU) Registers
//==============================================================================

// Timer Output Control Register
#define TMU_TOCR 0xFFD80000 // 8

// Timer Start Register (Channels 0, 1, and 2)
#define TMU_TSTR 0xFFD80004 // 8

// Channel 0
#define TMU_TCOR0 0xFFD80008 // 32
#define TMU_TCNT0 0xFFD8000C // 32
#define TMU_TCR0 0xFFD80010  // 16

// Channel 1
#define TMU_TCOR1 0xFFD80014 // 32
#define TMU_TCNT1 0xFFD80018 // 32
#define TMU_TCR1 0xFFD8001C  // 16

// Channel 2
#define TMU_TCOR2 0xFFD80020  // 32
#define TMU_TCNT2 0xFFD80024  // 32
#define TMU_TCR2 0xFFD80028   // 16
// Input Capture (Channel 2 Only)
#define TMU_TCPR2 0xFFD8002C  // 32 (R Only)

//
// SH7750R Only
//

// Timer Start Register (Channels 3 and 4)
//#define TMU_TSTR2 0xFE100004 // 8

// Channel 3
//#define TMU_TCOR3 0xFE100008 // 32
//#define TMU_TCNT3 0xFE10000C // 32
//#define TMU_TCR3 0xFE100010  // 16

// Channel 4
//#define TMU_TCOR4 0xFE100014 // 32
//#define TMU_TCNT4 0xFE100018 // 32
//#define TMU_TCR4 0xFE10001C  // 16

//==============================================================================
// Serial Communication Interface (SCI) Registers
//==============================================================================
// NOTE: Also used for Smart Card Interface

// SCI Configuration
#define SCI_SCSMR1 0xFFE00000  // 8
#define SCI_SCBRR1 0xFFE00004  // 8
#define SCI_SCSCR1 0xFFE00008  // 8

// Transmit Registers
#define SCI_SCTDR1 0xFFE0000C  // 8
// Not accessible by the CPU
//#define SCI_SCTSR1

// SCI Status
#define SCI_SCSSR1 0xFFE00010  // 8

// Receive Registers
#define SCI_SCRDR1 0xFFE00014  // 8 (R Only)
// Not accessible by the CPU
//#define SCI_SCRSR1

// Smart Card Mode
#define SCI_SCSCMR1 0xFFE00018 // 8

// SCI Port Configuration
#define SCI_SCSPTR1 0xFFE0001C // 8

//==============================================================================
// Serial Communication Interface with FIFO (SCIF) Registers
//==============================================================================

// SCIF Configuration
#define SCIF_SCSMR2 0xFFE80000  // 16
#define SCIF_SCBRR2 0xFFE80004  // 8
#define SCIF_SCSCR2 0xFFE80008  // 16

// Transmit Registers
#define SCIF_SCFTDR2 0xFFE8000C  // 8 (W Only)
// Not accessible by the CPU
//#define SCIF_SCTSR2

// SCIF Status
#define SCIF_SCFSR2 0xFFE80010  // 16

// Receive Registers
#define SCIF_SCFRDR2 0xFFE80014  // 8 (R Only)
// Not accessible by the CPU
//#define SCIF_SCRSR2

// FIFO Configuration
#define SCIF_SCFCR2 0xFFE80018 // 16
#define SCIF_SCFDR2 0xFFE8001C // 16 (R Only)

// SCIF Port Configuration
#define SCIF_SCSPTR2 0xFFE80020 // 16

// SCIF Line Status
#define SCIF_SCLSR2 0xFFE80024 // 16

//==============================================================================
// High-Performance User Debug Interface (H-UDI) Registers
//==============================================================================

// Instruction Register
#define HUDI_SDIR 0xFFF00000  // 16 (R Only)

// Data Registers (Single or dual access possible)
// Single, combined register
#define HUDI_SDDR 0xFFF00008  // 32
// Dual, separated data registers
#define HUDI_SDDRH 0xFFF00008  // 16
#define HUDI_SDDRL 0xFFF0000A  // 16

// Interrupt Source Register
//#define HUDI_SDINT 0xFFF00014 // 16 - SH7750R Only

// These are not accessible by the CPU
// Bypass Register
//#define HUDI_SDBPR
// Boundary Scan Register
//#define HUDI_SDBSR // SH7750R Only

//==============================================================================
// Performance Counter Registers
//==============================================================================
// Undocumented, see DreamHAL Performance Counter Module (perfctr.h/.c) for info

// Performance Counter Control
#define PMCR_PMCR1_CTRL 0xFF000084 // 16
#define PMCR_PMCR2_CTRL 0xFF000088 // 16

// Channel 1 (48-bit, mask the upper 16 bits to 0 according to endianness)
#define PMCR_PMCTR1H 0xFF100004  // 32
#define PMCR_PMCTR1L 0xFF100008  // 32

// Channel 2 (48-bit, mask the upper 16 bits to 0 according to endianness)
#define PMCR_PMCTR2H 0xFF10000C  // 32
#define PMCR_PMCTR2L 0xFF100010  // 32

//==============================================================================
// Utility Functions
//==============================================================================

//------------------------------------------------------------------------------
// Register Reads
//------------------------------------------------------------------------------

// 8-bit
static inline unsigned char REG_Read_8bit(unsigned int reg)
{
  return *(volatile unsigned char*)reg;
}

// 16-bit
static inline unsigned short REG_Read_16bit(unsigned int reg)
{
  return *(volatile unsigned short*)reg;
}

// 32-bit
static inline unsigned int REG_Read_32bit(unsigned int reg)
{
  return *(volatile unsigned int*)reg;
}

//------------------------------------------------------------------------------
// Register Writes
//------------------------------------------------------------------------------

// 8-bit
static inline void REG_Write_8bit(unsigned int reg, unsigned char value)
{
  *(volatile unsigned char*)reg = value;
}

// 16-bit
static inline void REG_Write_16bit(unsigned int reg, unsigned short value)
{
  *(volatile unsigned short*)reg = value;
}

// 32-bit
static inline void REG_Write_32bit(unsigned int reg, unsigned int value)
{
  *(volatile unsigned int*)reg = value;
}

#endif /* __SH4_REGISTERS_H_ */
