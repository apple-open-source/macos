/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sis/sis_regs.h,v 1.17 2003/01/29 15:42:17 eich Exp $ */
/*
 * Copyright 1998,1999 by Alan Hourihane, Wigan, England.
 * Parts Copyright 2001, 2002 by Thomas Winischhofer, Vienna, Austria.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holder not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  The copyright holder makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * THE PROVIDER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE PROVIDER BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors:  Alan Hourihane, alanh@fairlite.demon.co.uk
 *           Mike Chapman <mike@paranoia.com>,
 *           Juanjo Santamarta <santamarta@ctv.es>,
 *           Mitani Hiroshi <hmitani@drl.mei.co.jp>
 *           David Thomas <davtom@dream.org.uk>
 *
 *	     Thomas Winischhofer <thomas@winischhofer.net>
 *              - 310/325 series (315/550/650/651/740/M650) support
 *		- (possibly incomplete) Xabre (SiS330) support
 *              - new mode switching code for 300, 310/325 and 330 series
 *              - many fixes for 300/540/630/730 chipsets,
 *              - many fixes for 5597/5598, 6326 and 530/620 chipsets,
 *              - VESA mode switching (deprecated),
 *              - extended CRT2/video bridge handling support,
 *              - dual head support on 300, 310/325 and 330 series
 *              - 650/LVDS (up to 1400x1050), 650/Chrontel 701x support
 *              - 30xB/30xLV/30xLVX video bridge support (300, 310/325, 330 series)
 *              - Xv support for 5597/5598, 6326, 530/620 and 310/325 series
 *              - video overlay enhancements for 300 series
 *              - TV and hi-res support for the 6326
 *              - etc.
 */

/* For general use --------------------------------------------------------------- */

#define inSISREG(base)          inb(base)
#define outSISREG(base,val)     outb(base,val)
#define orSISREG(base,val)      do { \
                      unsigned char __Temp = inb(base); \
                      outSISREG(base, __Temp | (val)); \
                    } while (0)
#define andSISREG(base,val)     do { \
                      unsigned char __Temp = inb(base); \
                      outSISREG(base, __Temp & (val)); \
                    } while (0)

#define inSISIDXREG(base,idx,var)   do { \
                      outb(base,idx); var=inb((base)+1); \
                    } while (0)
#define outSISIDXREG(base,idx,val)  do { \
                      outb(base,idx); outb((base)+1,val); \
                    } while (0)
#define orSISIDXREG(base,idx,val)   do { \
                      unsigned char __Temp; \
                      outb(base,idx);   \
                      __Temp = inb((base)+1)|(val); \
                      outSISIDXREG(base,idx,__Temp); \
                    } while (0)
#define andSISIDXREG(base,idx,and)  do { \
                      unsigned char __Temp; \
                      outb(base,idx);   \
                      __Temp = inb((base)+1)&(and); \
                      outSISIDXREG(base,idx,__Temp); \
                    } while (0)
#define setSISIDXREG(base,idx,and,or)   do { \
                      unsigned char __Temp; \
                      outb(base,idx);   \
                      __Temp = (inb((base)+1)&(and))|(or); \
                      outSISIDXREG(base,idx,__Temp); \
                    } while (0)

#define BITMASK(h,l)    (((unsigned)(1U << ((h)-(l)+1))-1)<<(l))
#define GENMASK(mask)   BITMASK(1?mask,0?mask)

#define GETBITS(var,mask)   	(((var) & GENMASK(mask)) >> (0?mask))
#define SETBITS(val,mask)   	((val) << (0?mask))
#define SETBIT(n)       	(1<<(n))

#define GETBITSTR(val,from,to)  ((GETBITS(val,from)) << (0?to))
#define SETVARBITS(var,val,from,to) (((var)&(~(GENMASK(to)))) | \
                                    GETBITSTR(val,from,to))
#define GETVAR8(var)        ((var)&0xFF)
#define SETVAR8(var,val)    (var) =  GETVAR8(val)

/* #define VGA_RELIO_BASE  0x380 */

#define AROFFSET        0x40   /* VGA_ATTR_INDEX - VGA_RELIO_BASE */
#define ARROFFSET       0x41   /* VGA_ATTR_DATA_R - VGA_RELIO_BASE  */
#define GROFFSET        0x4e   /* VGA_GRAPH_INDEX - VGA_RELIO_BASE */
#define SROFFSET        0x44   /* VGA_SEQ_INDEX - VGA_RELIO_BASE */
#define CROFFSET        0x54   /* VGA_CRTC_INDEX_OFFSET + VGA_IOBASE_COLOR - VGA_RELIO_BASE */
#define MISCROFFSET     0x4c   /* VGA_MISC_OUT_R - VGA_RELIO_BASE */
#define MISCWOFFSET     0x42   /* VGA_MISC_OUT_W - VGA_RELIO_BASE */
#define INPUTSTATOFFSET 0x5A
#define PART1OFFSET     0x04
#define PART2OFFSET     0x10
#define PART3OFFSET     0x12
#define PART4OFFSET     0x14
#define PART5OFFSET     0x16
#define VIDEOOFFSET     0x02
#define COLREGOFFSET    0x48

#define SISAR       pSiS->RelIO + AROFFSET
#define SISARR      pSiS->RelIO + ARROFFSET
#define SISGR       pSiS->RelIO + GROFFSET
#define SISSR       pSiS->RelIO + SROFFSET
#define SISCR       pSiS->RelIO + CROFFSET
#define SISMISCR    pSiS->RelIO + MISCROFFSET
#define SISMISCW    pSiS->RelIO + MISCWOFFSET
#define SISINPSTAT  pSiS->RelIO + INPUTSTATOFFSET
#define SISPART1    pSiS->RelIO + PART1OFFSET
#define SISPART2    pSiS->RelIO + PART2OFFSET
#define SISPART3    pSiS->RelIO + PART3OFFSET
#define SISPART4    pSiS->RelIO + PART4OFFSET
#define SISPART5    pSiS->RelIO + PART5OFFSET
#define SISVID      pSiS->RelIO + VIDEOOFFSET
#define SISCOLIDX   pSiS->RelIO + COLREGOFFSET
#define SISCOLDATA  pSiS->RelIO + COLREGOFFSET + 1
#define SISCOL2IDX  SISPART5
#define SISCOL2DATA SISPART5 + 1


#define vc_index_offset    0x00  /* Video capture - unused */
#define vc_data_offset     0x01
#define vi_index_offset    VIDEOOFFSET
#define vi_data_offset     (VIDEOOFFSET + 1)
#define crt2_index_offset  PART1OFFSET
#define crt2_port_offset   (PART1OFFSET + 1)
#define sr_index_offset    SROFFSET
#define sr_data_offset     (SROFFSET + 1)
#define cr_index_offset    CROFFSET
#define cr_data_offset     (CROFFSET + 1)
#define input_stat         INPUTSTATOFFSET

/* For old chipsets (5597/5598, 6326, 530/620) ------------ */
/* SR (3C4) */
#define BankReg 0x06
#define ClockReg 0x07
#define CPUThreshold 0x08
#define CRTThreshold 0x09
#define CRTCOff 0x0A
#define DualBanks 0x0B
#define MMIOEnable 0x0B
#define RAMSize 0x0C
#define Mode64 0x0C
#define ExtConfStatus1 0x0E
#define ClockBase 0x13
#define LinearAdd0 0x20
#define LinearAdd1 0x21
#define GraphEng 0x27
#define MemClock0 0x28
#define MemClock1 0x29
#define XR2A 0x2A
#define XR2B 0x2B
#define TurboQueueBase 0x2C
#define FBSize 0x2F
#define ExtMiscCont5 0x34
#define ExtMiscCont9 0x3C

/* 3x4 */
#define Offset 0x13

/* SiS Registers for 300, 540, 630, 730, 315, 550, 650, 740 ---------------------- */

/* VGA standard register */
#define  Index_SR_Graphic_Mode                  0x06
#define  Index_SR_RAMDAC_Ctrl                   0x07
#define  Index_SR_Threshold_Ctrl1               0x08
#define  Index_SR_Threshold_Ctrl2               0x09
#define  Index_SR_Misc_Ctrl                     0x0F
#define  Index_SR_DDC                           0x11
#define  Index_SR_Feature_Connector_Ctrl        0x12
#define  Index_SR_DRAM_Sizing                   0x14
#define  Index_SR_DRAM_State_Machine_Ctrl       0x15
#define  Index_SR_AGP_PCI_State_Machine         0x21
#define  Index_SR_Internal_MCLK0                0x28
#define  Index_SR_Internal_MCLK1                0x29
#define  Index_SR_Internal_DCLK1                0x2B
#define  Index_SR_Internal_DCLK2                0x2C
#define  Index_SR_Internal_DCLK3                0x2D
#define  Index_SR_Ext_Clock_Sel                 0x32
#define  Index_SR_Int_Status                    0x34
#define  Index_SR_Int_Enable                    0x35
#define  Index_SR_Int_Reset                     0x36
#define  Index_SR_Power_On_Trap                 0x38
#define  Index_SR_Power_On_Trap2                0x39
#define  Index_SR_Power_On_Trap3                0x3A

/* video registers (300/630/730/315/550/650/740 only) */
#define  Index_VI_Passwd                        0x00

/* Video overlay horizontal start/end, unit=screen pixels */
#define  Index_VI_Win_Hor_Disp_Start_Low        0x01
#define  Index_VI_Win_Hor_Disp_End_Low          0x02
#define  Index_VI_Win_Hor_Over                  0x03 /* Overflow */

/* Video overlay vertical start/end, unit=screen pixels */
#define  Index_VI_Win_Ver_Disp_Start_Low        0x04
#define  Index_VI_Win_Ver_Disp_End_Low          0x05
#define  Index_VI_Win_Ver_Over                  0x06 /* Overflow */

/* Y Plane (4:2:0) or YUV (4:2:2) buffer start address, unit=word */
#define  Index_VI_Disp_Y_Buf_Start_Low          0x07
#define  Index_VI_Disp_Y_Buf_Start_Middle       0x08
#define  Index_VI_Disp_Y_Buf_Start_High         0x09

/* U Plane (4:2:0) buffer start address, unit=word */
#define  Index_VI_U_Buf_Start_Low               0x0A
#define  Index_VI_U_Buf_Start_Middle            0x0B
#define  Index_VI_U_Buf_Start_High              0x0C

/* V Plane (4:2:0) buffer start address, unit=word */
#define  Index_VI_V_Buf_Start_Low               0x0D
#define  Index_VI_V_Buf_Start_Middle            0x0E
#define  Index_VI_V_Buf_Start_High              0x0F

/* Pitch for Y, UV Planes, unit=word */
#define  Index_VI_Disp_Y_Buf_Pitch_Low          0x10
#define  Index_VI_Disp_UV_Buf_Pitch_Low         0x11
#define  Index_VI_Disp_Y_UV_Buf_Pitch_Middle    0x12

/* What is this ? */
#define  Index_VI_Disp_Y_Buf_Preset_Low         0x13
#define  Index_VI_Disp_Y_Buf_Preset_Middle      0x14

#define  Index_VI_UV_Buf_Preset_Low             0x15
#define  Index_VI_UV_Buf_Preset_Middle          0x16
#define  Index_VI_Disp_Y_UV_Buf_Preset_High     0x17

/* Scaling control registers */
#define  Index_VI_Hor_Post_Up_Scale_Low         0x18
#define  Index_VI_Hor_Post_Up_Scale_High        0x19
#define  Index_VI_Ver_Up_Scale_Low              0x1A
#define  Index_VI_Ver_Up_Scale_High             0x1B
#define  Index_VI_Scale_Control                 0x1C

/* Playback line buffer control */
#define  Index_VI_Play_Threshold_Low            0x1D
#define  Index_VI_Play_Threshold_High           0x1E
#define  Index_VI_Line_Buffer_Size              0x1F

/* Destination color key */
#define  Index_VI_Overlay_ColorKey_Red_Min      0x20
#define  Index_VI_Overlay_ColorKey_Green_Min    0x21
#define  Index_VI_Overlay_ColorKey_Blue_Min     0x22
#define  Index_VI_Overlay_ColorKey_Red_Max      0x23
#define  Index_VI_Overlay_ColorKey_Green_Max    0x24
#define  Index_VI_Overlay_ColorKey_Blue_Max     0x25

/* Source color key, YUV color space */
#define  Index_VI_Overlay_ChromaKey_Red_Y_Min   0x26
#define  Index_VI_Overlay_ChromaKey_Green_U_Min 0x27
#define  Index_VI_Overlay_ChromaKey_Blue_V_Min  0x28
#define  Index_VI_Overlay_ChromaKey_Red_Y_Max   0x29
#define  Index_VI_Overlay_ChromaKey_Green_U_Max 0x2A
#define  Index_VI_Overlay_ChromaKey_Blue_V_Max  0x2B

/* Contrast enhancement and brightness control */
#define  Index_VI_Contrast_Factor               0x2C	/* obviously unused/undefined */
#define  Index_VI_Brightness                    0x2D
#define  Index_VI_Contrast_Enh_Ctrl             0x2E

#define  Index_VI_Key_Overlay_OP                0x2F

#define  Index_VI_Control_Misc0                 0x30
#define  Index_VI_Control_Misc1                 0x31
#define  Index_VI_Control_Misc2                 0x32

/* TW: Subpicture registers */
#define  Index_VI_SubPict_Buf_Start_Low		0x33
#define  Index_VI_SubPict_Buf_Start_Middle	0x34
#define  Index_VI_SubPict_Buf_Start_High	0x35

/* TW: What is this ? */
#define  Index_VI_SubPict_Buf_Preset_Low	0x36
#define  Index_VI_SubPict_Buf_Preset_Middle	0x37

/* TW: Subpicture pitch, unit=16 bytes */
#define  Index_VI_SubPict_Buf_Pitch		0x38

/* TW: Subpicture scaling control */
#define  Index_VI_SubPict_Hor_Scale_Low		0x39
#define  Index_VI_SubPict_Hor_Scale_High	0x3A
#define  Index_VI_SubPict_Vert_Scale_Low	0x3B
#define  Index_VI_SubPict_Vert_Scale_High	0x3C

#define  Index_VI_SubPict_Scale_Control		0x3D
/* (0x40 = enable/disable subpicture) */

/* TW: Subpicture line buffer control */
#define  Index_VI_SubPict_Threshold		0x3E

/* TW: What is this? */
#define  Index_VI_FIFO_Max			0x3F

/* TW: Subpicture palette; 16 colors, total 32 bytes address space */
#define  Index_VI_SubPict_Pal_Base_Low		0x40
#define  Index_VI_SubPict_Pal_Base_High		0x41

/* I wish I knew how to use these ... */
#define  Index_MPEG_Read_Ctrl0                  0x60	/* MPEG auto flip */
#define  Index_MPEG_Read_Ctrl1                  0x61	/* MPEG auto flip */
#define  Index_MPEG_Read_Ctrl2                  0x62	/* MPEG auto flip */
#define  Index_MPEG_Read_Ctrl3                  0x63	/* MPEG auto flip */

/* TW: MPEG AutoFlip scale */
#define  Index_MPEG_Ver_Up_Scale_Low            0x64
#define  Index_MPEG_Ver_Up_Scale_High           0x65

#define  Index_MPEG_Y_Buf_Preset_Low		0x66
#define  Index_MPEG_Y_Buf_Preset_Middle		0x67
#define  Index_MPEG_UV_Buf_Preset_Low		0x68
#define  Index_MPEG_UV_Buf_Preset_Middle	0x69
#define  Index_MPEG_Y_UV_Buf_Preset_High	0x6A

/* TW: The following registers only exist on the 310/325 series */

/* TW: Bit 16:24 of Y_U_V buf start address (?) */
#define  Index_VI_Y_Buf_Start_Over		0x6B
#define  Index_VI_U_Buf_Start_Over		0x6C
#define  Index_VI_V_Buf_Start_Over		0x6D

#define  Index_VI_Disp_Y_Buf_Pitch_High		0x6E
#define  Index_VI_Disp_UV_Buf_Pitch_High	0x6F

/* Hue and saturation */
#define	 Index_VI_Hue				0x70
#define  Index_VI_Saturation			0x71

#define  Index_VI_SubPict_Start_Over		0x72
#define  Index_VI_SubPict_Buf_Pitch_High	0x73

#define  Index_VI_Control_Misc3			0x74


/* TW: Bits (and helpers) for Index_VI_Control_Misc0 */
#define  VI_Misc0_Enable_Overlay		0x02
#define  VI_Misc0_420_Plane_Enable		0x04 /* Select Plane or Packed mode */
#define  VI_Misc0_422_Enable			0x20 /* Select 422 or 411 mode */
#define  VI_Misc0_Fmt_YVU420P			0x0C /* YUV420 Planar (I420, YV12) */
#define  VI_Misc0_Fmt_YUYV			0x28 /* YUYV Packed (YUY2) */
#define  VI_Misc0_Fmt_UYVY			0x08 /* (UYVY) */

/* TW: Bits for Index_VI_Control_Misc1 */
/* #define  VI_Misc1_?                          0x01  */
#define  VI_Misc1_BOB_Enable			0x02
#define	 VI_Misc1_Line_Merge			0x04
#define  VI_Misc1_Field_Mode			0x08
/* #define  VI_Misc1_?                          0x10  */
#define  VI_Misc1_Non_Interleave                0x20 /* 300 series only? */
#define  VI_Misc1_Buf_Addr_Lock			0x20 /* 310 series only? */
/* #define  VI_Misc1_?                          0x40  */
/* #define  VI_Misc1_?                          0x80  */

/* TW: Bits for Index_VI_Control_Misc2 */
#define  VI_Misc2_Select_Video2			0x01
#define  VI_Misc2_Video2_On_Top			0x02
/* #define  VI_Misc2_?                          0x04  */
#define  VI_Misc2_Vertical_Interpol		0x08
#define  VI_Misc2_Dual_Line_Merge               0x10
#define  VI_Misc2_All_Line_Merge                0x20  /* 310 series only? */
#define  VI_Misc2_Auto_Flip_Enable		0x40  /* 300 series only? */
#define  VI_Misc2_Video_Reg_Write_Enable        0x80  /* 310 series only? */

/* TW: Bits for Index_VI_Control_Misc3 */
#define  VI_Misc3_Submit_Video_1		0x01  /* AKA "address ready" */
#define  VI_Misc3_Submit_Video_2		0x02  /* AKA "address ready" */
#define  VI_Misc3_Submit_SubPict		0x04  /* AKA "address ready" */

/* TW: Values for Index_VI_Key_Overlay_OP (0x2F) */
#define  VI_ROP_Never				0x00
#define  VI_ROP_DestKey				0x03
#define  VI_ROP_Always				0x0F


/* video registers (6326 and 530/620) --------------- */
#define  Index_VI6326_Passwd                        0x80

/* Video overlay horizontal start/end, unit=screen pixels */
#define  Index_VI6326_Win_Hor_Disp_Start_Low        0x81
#define  Index_VI6326_Win_Hor_Disp_End_Low          0x82
#define  Index_VI6326_Win_Hor_Over                  0x83 /* Overflow */

/* Video overlay vertical start/end, unit=screen pixels */
#define  Index_VI6326_Win_Ver_Disp_Start_Low        0x84
#define  Index_VI6326_Win_Ver_Disp_End_Low          0x85
#define  Index_VI6326_Win_Ver_Over                  0x86 /* Overflow */

/* Y Plane (4:2:0) or YUV (4:2:2) buffer start address, unit=dword */
#define  Index_VI6326_Disp_Y_Buf_Start_Low          0x8A
#define  Index_VI6326_Disp_Y_Buf_Start_Middle       0x8B
#define  Index_VI6326_Disp_Capt_Y_Buf_Start_High    0x89 /* 6326: 7:4 display, 3:0 capture */
							 /* 530/620: 7:3 display. 2:0 reserved */
/* End address of Y plane (in 16k unit) - 6326 ONLY */
#define  Index_VI6326_Disp_Y_End                    0x8D

/* U Plane (4:2:0) buffer start address, unit=dword */
#define  Index_VI6326_U_Buf_Start_Low               0xB7
#define  Index_VI6326_U_Buf_Start_Middle            0xB8

/* V Plane (4:2:0) buffer start address, unit=dword */
#define  Index_VI6326_V_Buf_Start_Low               0xBA
#define  Index_VI6326_V_Buf_Start_Middle            0xBB

/* U/V plane start address overflow bits 19:16 */
#define  Index_VI6326_UV_Buf_Start_High             0xB9

/* Pitch for Y, UV Planes, unit=dword(6326 & 530/620) */
#define  Index_VI6326_Disp_Y_Buf_Pitch_Low          0x8C  /* 7:0 */
#define  Index_VI6326_Disp_Y_Buf_Pitch_High         0x8E  /* 11:8 (3:0 here) */

#define  Index_VI6326_Disp_UV_Buf_Pitch_Low         0xBC  /* 7:0 */
#define  Index_VI6326_Disp_UV_Buf_Pitch_High        0xBD  /* 11:8 (3:0 here) */

/* Scaling control registers */
#define  Index_VI6326_Hor_Scale         	    0x92
#define  Index_VI6326_Hor_Scale_Integer		    0x94
#define  Index_VI6326_Ver_Scale	            	    0x93

/* Playback line buffer control */
#define  Index_VI6326_Play_Threshold_Low            0x9E
#define  Index_VI6326_Play_Threshold_High           0x9F
#define  Index_VI6326_Line_Buffer_Size              0xA0 /* 530 & 6326: quad-word */

/* Destination color key */
#define  Index_VI6326_Overlay_ColorKey_Red_Min      0x97
#define  Index_VI6326_Overlay_ColorKey_Green_Min    0x96
#define  Index_VI6326_Overlay_ColorKey_Blue_Min     0x95
#define  Index_VI6326_Overlay_ColorKey_Red_Max      0xA3
#define  Index_VI6326_Overlay_ColorKey_Green_Max    0xA2
#define  Index_VI6326_Overlay_ColorKey_Blue_Max     0xA1

/* Source color key */
#define  Index_VI6326_Overlay_ChromaKey_Red_Y_Min   0x9C
#define  Index_VI6326_Overlay_ChromaKey_Green_U_Min 0x9B
#define  Index_VI6326_Overlay_ChromaKey_Blue_V_Min  0x9A
#define  Index_VI6326_Overlay_ChromaKey_Red_Y_Max   0xA6
#define  Index_VI6326_Overlay_ChromaKey_Green_U_Max 0xA5
#define  Index_VI6326_Overlay_ChromaKey_Blue_V_Max  0xA4

/* Contrast enhancement and brightness control */
#define  Index_VI6326_Contrast_Factor               0xB3
#define  Index_VI6326_Brightness                    0xB4
#define  Index_VI6326_Contrast_Enh_Ctrl             0xB5

/* Alpha (ALL 6326 only?) */
#define  Index_VI6326_AlphaGraph                    0xA7
#define  Index_VI6326_AlphaVideo                    0xA8

#define  Index_VI6326_Key_Overlay_OP                0xA9

#define  Index_VI6326_Control_Misc0                 0x98
#define  Index_VI6326_Control_Misc1                 0x99  /* (Datasheet: 6326 ONLY - not correct?) */
#define  Index_VI6326_Control_Misc3                 0x9D
#define  Index_VI6326_Control_Misc4                 0xB6
#define  Index_VI6326_VideoFormatSelect             Index_VI6326_Ver_Scale
#define  Index_VI6326_Control_Misc5                 0xBE  /* (Datasheet: 530/620 ONLY - not correct) */
#define  Index_VI6326_Control_Misc6                 0xB2  /* 5597 and 6326 only! */

/* TW: What is this?  not a register, obviously */
#define  Index_VI6326_FIFO_Max			    0x3F

/* TW: Bits (and helpers) for Index_VI6326_Control_Misc0 */
#define  VI6326_Misc0_EnableCapture		0x01  /* 1 = on, 0 = off (6326 only) */
#define  VI6326_Misc0_EnableOverlay		0x02  /* 1 = on, 0 = off */
#define  VI6326_Misc0_VideoOnly			0x10  /* 1 = video only, 0 = gfx + video */
#define  VI6326_Misc0_CaptureInterlace		0x20  /* 1 = capture data is interlace, 0 = not (6326 only) */
#define  VI6326_Misc0_VideoFormat		0x40  /* 1 = YUV, 0 = RGB */
#define  VI6326_Misc0_FieldPolarity		0x80  /* 1 = *Odd / Even, 0 = Odd / *Even (6326 only) */

/* TW: Bits for Index_VI6326_Control_Misc1 (ALL 6326 ONLY) */
#define  VI6326_Misc1_EnableYUVCapture		0x01  /* 0 = RGB, 1 = YUV */
#define  VI6326_Misc1_EnableCaptureDithering    0x02  /* 0 = disable, 1 = enable */
#define  VI6326_Misc1_CaptureFormat555		0x04  /* 1 = 555, 0 = 565 */
#define  VI6326_Misc1_FilterModeMask		0x38
#define  VI6326_Misc1_FilterMode0		0x00  /* 1 */
#define  VI6326_Misc1_FilterMode1		0x08  /* 1/8(1+3z^-1+3z^-2+z^-3)*/
#define  VI6326_Misc1_FilterMode2		0x10  /* 1/4(1+2z^-1+z^-2) */
#define  VI6326_Misc1_FilterMode3		0x18  /* 1/2(1+z^-1) */
#define  VI6326_Misc1_FilterMode4		0x20  /* 1/8(1+2z^-1+2z^-2+2z^-3+z^-4) */
#define  VI6326_Misc1_EnableVBSyncIRQ		0x40  /* 1 = Enable IRQ on vertical blank */
#define  VI6326_Misc1_ClearVBSyncIRQ		0x80  /* Clear pending irq */

/* TW: Bits for Index_VI6326_Control_Misc3 */
#define  VI6326_Misc3_UVCaptureFormat		0x01  /* 1 = 2's complement, 0 = CCIR 601 (6326 only) */
#define  VI6326_Misc3_UVOverlayFormat		0x02  /* 1 = 2's complement, 0 = CCIR 601 */
#define  VI6326_Misc3_ChromaKeyFormat		0x04  /* 1 = YUV, 0 = RGB */
#define  VI6326_Misc3_VMIAccess			0x08  /* 1 = enable, 0 = disable (6326 only) */
#define  VI6326_Misc3_VMIEnable			0x10  /* 1 = enable, 0 = disable (6326 only) */
#define  VI6326_Misc3_VMIIRQ			0x20  /* 1 = enable, 0 = disable (6326 only) */
#define  VI6326_Misc3_BT819A			0x40  /* 1 = enable, 0 = disable (6326 only) */
#define  VI6326_Misc3_SystemMemFB		0x80  /* 1 = enable, 0 = disable (6326 only) */

/* TW: Bits for Index_VI6326_Control_Misc4 */
#define  VI6326_Misc4_CPUVideoFormatMask	0x03
#define  VI6326_Misc4_CPUVideoFormatRGB555      0x00
#define  VI6326_Misc4_CPUVideoFormatYUV422      0x01
#define  VI6326_Misc4_CPUVideoFormatRGB565      0x02
#define  VI6326_Misc4_EnableYUV420		0x04  /* 1 = enable, 0 = disable */
/** #define WHATISTHIS                          0x40  */

/* TW: Bits for Index_VI6326_Control_Misc5 (all 530/620 only) */
#define  VI6326_Misc5_LineBufferMerge           0x10  /* 0 = disable, 1=enable */
#define  VI6326_Misc5_VPlaneBit20               0x04
#define  VI6326_Misc5_UPlaneBit20               0x02

/* TW: Bits for Index_VI6326_Control_Misc6  (5597 and 6326 only) */
#define  VI6326_Misc6_Decimation                0x80  /* 0=disable 1=enable video decimation */

/* Video format selection */
#define  VI_6326_VideoUYVY422			0x00
#define  VI_6326_VideoVYUY422                   0x40
#define  VI_6326_VideoYUYV422                   0x80
#define  VI_6326_VideoYVYU422                   0xC0
#define  VI_6326_VideoRGB555                    0x00
#define  VI_6326_VideoRGB565                    0x40

/* TW: Values for Index_VI6326_Key_Overlay_OP */
#define  VI6326_ROP_Never			0x00
#define  VI6326_ROP_DestKey			0x03
#define  VI6326_ROP_Always			0x0F

/* --- end of 6326 video registers ---------------------------------- */

/* TW register base (6326 only) */
#define  Index_TV6326_TVOutIndex		    0xE0
#define  Index_TV6326_TVOutData		    	    0xE1

/*
 *  CRT_2 function control register ---------------------------------
 */
#define  Index_CRT2_FC_CONTROL                  0x00
#define  Index_CRT2_FC_SCREEN_HIGH              0x04
#define  Index_CRT2_FC_SCREEN_MID               0x05
#define  Index_CRT2_FC_SCREEN_LOW               0x06
#define  Index_CRT2_FC_ENABLE_WRITE             0x24
#define  Index_CRT2_FC_VR                       0x25
#define  Index_CRT2_FC_VCount                   0x27
#define  Index_CRT2_FC_VCount1                  0x28

#define  Index_310_CRT2_FC_VR                   0x30  /* d[1] = vertical retrace */
#define  Index_310_CRT2_FC_RT			0x33  /* d[7] = retrace in progress */

/* video attributes - these should probably be configurable on the fly
 *                    so users with different desktop sizes can keep
 *                    captured data off the desktop
 */
#define _VINWID                                  704
#define _VINHGT                         _VINHGT_NTSC
#define _VINHGT_NTSC                             240
#define _VINHGT_PAL                              290
#define _VIN_WINDOW                  (704 * 291 * 2)
#define _VBI_WINDOW                   (704 * 64 * 2)

#define _VIN_FIELD_EVEN                            1
#define _VIN_FIELD_ODD                             2
#define _VIN_FIELD_BOTH                            4


/* i2c registers (TW; not on 300/310/325 series) */
#define X_INDEXREG      0x14
#define X_PORTREG       0x15
#define X_DATA          0x0f
#define I2C_SCL         0x00
#define I2C_SDA         0x01
#define I2C_DELAY       10

/* mmio registers for video */
#define REG_PRIM_CRT_COUNTER    0x8514

/* TW: MPEG MMIO registers (630 and later) ----------------------------------------- */

/* Not public (yet?) */

