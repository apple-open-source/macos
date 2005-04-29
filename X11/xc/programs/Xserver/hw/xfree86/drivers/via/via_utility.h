/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/via/via_utility.h,v 1.3 2003/08/27 15:16:14 tsi Exp $ */
/*
 * Copyright 1998-2003 VIA Technologies, Inc. All Rights Reserved.
 * Copyright 2001-2003 S3 Graphics, Inc. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * VIA, S3 GRAPHICS, AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef _VIA_UTILITY_H_
#define _VIA_UTILITY_H_ 1

/*
 * Define for Utility functions using XvPutImage.
 */

/* Primary ID */
#define	UT_XV_FUNC_BIOS				0x11   /* Primary ID */
#define	UT_XV_FUNC_DRIVER			0x12
#define	UT_XV_FUNC_DEVICE			0x13
#define	UT_XV_FUNC_PANEL			0x14
#define	UT_XV_FUNC_TV				0x15
#define	UT_XV_FUNC_GAMMA			0x16

/* Secondary ID */
#define	UT_XV_FUNC_BIOS_GetChipID		0x01    /* Get Chip ID */
#define UT_XV_FUNC_BIOS_GetVersion		0x02    /* Get the version of the BIOS */
#define	UT_XV_FUNC_BIOS_GetDate			0x03    /* Get the date (year, month, day) of the BIOS. */
#define	UT_XV_FUNC_BIOS_GetVideoMemSizeMB	0x04    /* Get the video memory size, in MB */

#define	UT_XV_FUNC_DRIVER_GetFileName		0x01
#define	UT_XV_FUNC_DRIVER_GetFileVersion	0x02

#define	UT_XV_FUNC_DEVICE_GetSupportState	1
#define	UT_XV_FUNC_DEVICE_GetConnectState	2
#define	UT_XV_FUNC_DEVICE_GetActiveState	3
#define	UT_XV_FUNC_DEVICE_SetActiveState	4
#define	UT_XV_FUNC_DEVICE_GetSAMMState		5
#define	UT_XV_FUNC_DEVICE_GetRoateState		6
#define	UT_XV_FUNC_DEVICE_SetRoateState		7

#define	UT_XV_FUNC_DEVICE_SetTargetPanel	1
#define	UT_XV_FUNC_DEVICE_GetPanelInfo		2
#define	UT_XV_FUNC_DEVICE_GetExpandState	3
#define	UT_XV_FUNC_DEVICE_SetExpandState	4
#define	UT_XV_FUNC_DEVICE_GetSupportExpand	5

#define	UT_XV_FUNC_TV_GetSupportStandardState	1
#define	UT_XV_FUNC_TV_GetStandard		2
#define	UT_XV_FUNC_TV_SetStandard		3
#define	UT_XV_FUNC_TV_GetSupportSignalTypeState	4
#define	UT_XV_FUNC_TV_GetSignalType             5
#define	UT_XV_FUNC_TV_SetSignalType             6
#define	UT_XV_FUNC_TV_GetMaxViewSizeValue       7
#define	UT_XV_FUNC_TV_GetViewSizeValue          8
#define	UT_XV_FUNC_TV_SetViewSizeValue          9
#define	UT_XV_FUNC_TV_GetMaxViewPositionValue	10
#define	UT_XV_FUNC_TV_GetViewPositionValue	11
#define	UT_XV_FUNC_TV_SetViewPositionValue	12
#define	UT_XV_FUNC_TV_GetSupportTuningState     13
#define	UT_XV_FUNC_TV_GetTuningItemMaxValue     14
#define	UT_XV_FUNC_TV_GetTuningItemValue	15
#define	UT_XV_FUNC_TV_SetTuningItemValue	16
#define	UT_XV_FUNC_TV_GetSupportSettingState	17
#define	UT_XV_FUNC_TV_GetSettingItemState	18
#define	UT_XV_FUNC_TV_SetSettingItemState	19

#define	UT_ESC_FUNC_GAMMA_GetDeviceSupportState	1
#define	UT_ESC_FUNC_GAMMA_GetLookUpTable	2
#define	UT_ESC_FUNC_GAMMA_SetLookUpTable	3
#define	UT_ESC_FUNC_GAMMA_GetDefaultLookUpTable	4

/* Input & Output Data */
#define	DISPLAY_DRIVER  			1
#define	VIDEO_CAPTURE_DRIVER			2
#define	HWOVERLAY_DRIVER			3
#define UT_DEVICE_NONE				0x00
#define	UT_DEVICE_CRT1				0x01
#define	UT_DEVICE_LCD				0x02
#define	UT_DEVICE_TV				0x04
#define	UT_DEVICE_DFP				0x08
#define	UT_DEVICE_CRT2				0x16
#define	UT_STATE_SAMM_OFF   			0       /* Not in SAMM mode. */
#define	UT_STATE_SAMM_ON    			1       /* In SAMM mode. */
#define	UT_ROTATE_NONE      			0       /* Normal. */
#define	UT_ROTATE_90_DEG    			1       /* Rotate 90 deg. clockwise. */
#define	UT_ROTATE_180_DEG   			2       /* Rotate 180 deg. clockwise. */
#define	UT_ROTATE_270_DEG   			3       /* Rotate 270 deg. clockwise. */
#define UT_PANEL_TYPE_STN   			0       /* STN. */
#define UT_PANEL_TYPE_TFT   			1       /* TFT. */
#define UT_STATE_EXPAND_OFF			0       /* Not expanded; centered. */
#define UT_STATE_EXPAND_ON 			1       /* Expanded. */
#define UT_PANEL_SUPPORT_EXPAND_OFF		0
#define UT_PANEL_SUPPORT_EXPAND_ON		1
#define	UT_TV_STD_NTSC 				0x0001  /* NTSC. */
#define	UT_TV_STD_PAL				0x0002  /* PAL. */
#define	UT_TV_STD_PAL_M 			0x0004  /* PAL M. */
#define	UT_TV_STD_PAL_N 			0x0008  /* PAL N. */
#define	UT_TV_STD_PAL_NC			0x0010  /* PAL N combination. */
#define	UT_TV_SGNL_COMPOSITE			0x0001  /* Composite. */
#define	UT_TV_SGNL_S_VIDEO			0x0002  /* S-Video. */
#define	UT_TV_SGNL_SCART			0x0004  /* SCART. */
#define	UT_TV_SGNL_COMP_OUTPUT      		0x0008  /* Component Output. */
#define	UT_TV_TUNING_FFILTER			0x0001  /* Flicker Filter. */
#define	UT_TV_TUNING_ADAPTIVE_FFILTER		0x0002  /* Adaptive Flicker Filter. */
#define	UT_TV_TUNING_BRIGHTNESS         	0x0004  /* Brightness. */
#define	UT_TV_TUNING_CONTRAST   		0x0008  /* Contrast. */
#define	UT_TV_TUNING_SATURATION      		0x0010  /* Saturation. */
#define	UT_TV_TUNING_TINT       		0x0020  /* Tint. */
#define	UT_TV_SETTING_FFILTER			0x0001
#define	UT_TV_SETTING_ADAPTIVE_FFILTER		0x0002
#define	UT_TV_SETTING_DOT_CRAWL			0x0004
#define	UT_TV_SETTING_LOCK_ASPECT_RATIO		0x0008
#define	UT_STATE_OFF                		0
#define	UT_STATE_ON                 		1
#define	UT_STATE_DEFAULT            		0xFFFF


typedef struct _UTBIOSVERSION
{
    CARD32 dwVersion;    /* The version. */
    CARD32 dwRevision;   /* The revision. */
} UTBIOSVERSION;

typedef struct _UTBIOSDATE
{
    CARD32 dwYear;   /* Year, like 2001. */
    CARD32 dwMonth;  /* Month, like 5, 12. */
    CARD32 dwDay;    /* Day, like 7, 30. */
} UTBIOSDATE;

typedef struct _UTDriverVersion
{
	CARD32 dwMajorNum;
	CARD32 dwMinorNum;
	CARD32 dwReversionNum;
} UTDriverVersion;

typedef struct _UTXYVALUE
{
    CARD32 dwX;
    CARD32 dwY;
} UTXYVALUE;

typedef struct _UTDEVICEINFO
{
    CARD32 dwDeviceType;
    CARD32 dwResX;
    CARD32 dwResY;
    CARD32 dwColorDepth;
} UTDEVICEINFO;

typedef struct _UTPANELINFO
{
    CARD32 dwType;  /* Panel type */
    CARD32 dwResX;  /* X Resolution of the panel. */
    CARD32 dwResY;  /* Y Resolution of the panel. */
} UTPANELINFO;

typedef struct _UTSETTVTUNINGINFO
{
    CARD32 dwItemID;
    CARD32 dwValue;
} UTSETTVTUNINGINFO;

typedef struct _UTSETTVITEMSTATE
{
    CARD32 dwItemID;
    CARD32 dwState;  /* Value defined in TV Setting state. */
} UTSETTVITEMSTATE;

typedef struct _UTGAMMAINFO
{
    UTDEVICEINFO    DeviceInfo;         /* E.g., if we want the table of CRT1, we should set */
    CARD32          LookUpTable[256];   /* the dwDeviceType as UT_DEVICE_CRT1.               */
} UTGAMMAINFO;


/* Functions protype */
Bool VIASaveUserSetting(VIABIOSInfoPtr  pBIOSInfo);
Bool VIASaveGammaSetting(VIABIOSInfoPtr pBIOSInfo);

/* Philips SAA7108 TV Position Table Rec. */
unsigned char VIASAA7108PostionOffset[3] = { 0x6D, 0x92, 0x93 };
unsigned char VIASAA7108PostionNormalTabRec[2][4][11][3] = {
	/* NTSC */
	{
		/* 640x480 */
		{
	    	{ 0x30, 0x1E, 0x1E },
	    	{ 0x2E, 0x20, 0x20 },
	    	{ 0x2C, 0x22, 0x22 },
	    	{ 0x2A, 0x24, 0x24 },
	    	{ 0x28, 0x26, 0x26 },
	    	/* default BIOS setting */
	    	{ 0x26, 0x28, 0x28 },
	    	{ 0x24, 0x2A, 0x2A },
	    	{ 0x22, 0x2C, 0x2C },
	    	{ 0x22, 0x30, 0x30 },
	    	{ 0x20, 0x32, 0x32 },
	    	{ 0x20, 0x33, 0x33 }
	    },
	    /* 800x600 */
		{
	    	{ 0x32, 0x2E, 0x2E },
	    	{ 0x30, 0x31, 0x31 },
	    	{ 0x2E, 0x34, 0x34 },
	    	{ 0x2C, 0x37, 0x37 },
	    	{ 0x2A, 0x3A, 0x3A },
	    	/* default BIOS setting */
	    	{ 0x28, 0x3D, 0x3D },
	    	{ 0x26, 0x40, 0x40 },
	    	{ 0x24, 0x43, 0x43 },
	    	{ 0x22, 0x46, 0x46 },
	    	{ 0x20, 0x48, 0x48 },
	    	{ 0x20, 0x49, 0x49 }
	    },
	    /* 1024x768 */
		{
	    	{ 0x2E, 0x4F, 0x4F },
	    	{ 0x2C, 0x55, 0x55 },
	    	{ 0x2A, 0x5A, 0x5A },
	    	{ 0x28, 0x60, 0x60 },
	    	{ 0x26, 0x65, 0x65 },
	    	/* default BIOS setting */
	    	{ 0x24, 0x69, 0x69 },
	    	{ 0x22, 0x6D, 0x6D },
	    	{ 0x20, 0x71, 0x71 },
	    	{ 0x20, 0x73, 0x73 },
	    	{ 0x20, 0x74, 0x74 },
	    	{ 0x20, 0x76, 0x76 }
	    },
	    /* 848x480 */
		{
	    	{ 0x30, 0x1E, 0x1E },
	    	{ 0x2E, 0x20, 0x20 },
	    	{ 0x2C, 0x22, 0x22 },
	    	{ 0x2A, 0x24, 0x24 },
	    	{ 0x28, 0x26, 0x26 },
	    	/* default BIOS setting */
	    	{ 0x26, 0x28, 0x28 },
	    	{ 0x24, 0x2A, 0x2A },
	    	{ 0x22, 0x2C, 0x2C },
	    	{ 0x22, 0x30, 0x30 },
	    	{ 0x20, 0x32, 0x32 },
	    	{ 0x20, 0x33, 0x33 }
	    }
	},
	/* PAL */
	{
		/* 640x480 */
		{
	    	{ 0x21, 0x42, 0x42 },
	    	{ 0x23, 0x44, 0x44 },
	    	{ 0x23, 0x45, 0x45 },
	    	{ 0x23, 0x46, 0x46 },
	    	{ 0x25, 0x47, 0x47 },
	    	/* default BIOS setting */
	    	{ 0x25, 0x48, 0x48 },
	    	{ 0x25, 0x49, 0x49 },
	    	{ 0x27, 0x4B, 0x4B },
	    	{ 0x2B, 0x4E, 0x4E },
	    	{ 0x2D, 0x50, 0x50 },
	    	{ 0x2F, 0x52, 0x52 }
	    },
	    /* 800x600 */
		{
	    	{ 0x30, 0x3F, 0x3F },
	    	{ 0x2E, 0x41, 0x41 },
	    	{ 0x2C, 0x43, 0x43 },
	    	{ 0x2A, 0x45, 0x45 },
	    	{ 0x28, 0x47, 0x47 },
	    	/* default BIOS setting */
	    	{ 0x26, 0x49, 0x49 },
	    	{ 0x24, 0x4B, 0x4B },
	    	{ 0x22, 0x4D, 0x4D },
	    	{ 0x20, 0x51, 0x51 },
	    	{ 0x20, 0x52, 0x52 },
	    	{ 0x20, 0x53, 0x53 }
	    },
	    /* 1024x768 */
		{
	    	{ 0x26, 0x5F, 0x5F },
	    	{ 0x24, 0x61, 0x61 },
	    	{ 0x24, 0x62, 0x62 },
	    	{ 0x22, 0x64, 0x64 },
	    	{ 0x22, 0x65, 0x65 },
	    	/* default BIOS setting */
	    	{ 0x22, 0x66, 0x66 },
	    	{ 0x22, 0x67, 0x67 },
	    	{ 0x22, 0x68, 0x68 },
	    	{ 0x22, 0x69, 0x69 },
	    	{ 0x22, 0x6A, 0x6A },
	    	{ 0x20, 0x6D, 0x6D }
	    },
	    /* 848x480 */
			{
	    	{ 0x21, 0x42, 0x42 },
	    	{ 0x23, 0x44, 0x44 },
	    	{ 0x23, 0x45, 0x45 },
	    	{ 0x23, 0x46, 0x46 },
	    	{ 0x25, 0x47, 0x47 },
	    	/* default BIOS setting */
	    	{ 0x25, 0x48, 0x48 },
	    	{ 0x25, 0x49, 0x49 },
	    	{ 0x27, 0x4B, 0x4B },
	    	{ 0x2B, 0x4E, 0x4E },
	    	{ 0x2D, 0x50, 0x50 },
	    	{ 0x2F, 0x52, 0x52 }
	    }
	}
};

unsigned char VIASAA7108PostionOverTabRec[2][4][11][3] = {
	/* NTSC */
	{
		/* 640x480 */
		{
	    	{ 0x34, 0x0C, 0x0C },
	    	{ 0x32, 0x0E, 0x0E },
	    	{ 0x30, 0x10, 0x10 },
	    	{ 0x2E, 0x12, 0x12 },
	    	{ 0x2C, 0x14, 0x14 },
	    	/* default BIOS setting */
	    	{ 0x2A, 0x16, 0x16 },
	    	{ 0x28, 0x18, 0x18 },
	    	{ 0x26, 0x1A, 0x1A },
	    	{ 0x24, 0x1C, 0x1C },
	    	{ 0x24, 0x1D, 0x1D },
	    	{ 0x22, 0x1F, 0x1F }
	    },
	    /* 800x600 */
		{
	    	{ 0x2C, 0x1A, 0x1A },
	    	{ 0x2A, 0x1C, 0x1C },
	    	{ 0x28, 0x1E, 0x1E },
	    	{ 0x28, 0x1F, 0x1F },
	    	{ 0x26, 0x22, 0x22 },
	    	/* default BIOS setting */
	    	{ 0x24, 0x24, 0x24 },
	    	{ 0x24, 0x25, 0x25 },
	    	{ 0x22, 0x27, 0x27 },
	    	{ 0x22, 0x28, 0x28 },
	    	{ 0x20, 0x2A, 0x2A },
	    	{ 0x20, 0x2B, 0x2B }
	    },
	    /* 1024x768 */
		{
	    	{ 0x2A, 0x24, 0x24 },
	    	{ 0x28, 0x27, 0x27 },
	    	{ 0x26, 0x2A, 0x2A },
	    	{ 0x26, 0x2B, 0x2B },
	    	{ 0x24, 0x2E, 0x2E },
	    	/* default BIOS setting */
	    	{ 0x22, 0x31, 0x31 },
	    	{ 0x22, 0x32, 0x32 },
	    	{ 0x22, 0x33, 0x33 },
	    	{ 0x22, 0x34, 0x34 },
	    	{ 0x20, 0x37, 0x37 },
	    	{ 0x20, 0x39, 0x39 }
	    },
	    /* 848x480 */
		{
	    	{ 0x34, 0x0C, 0x0C },
	    	{ 0x32, 0x0E, 0x0E },
	    	{ 0x30, 0x10, 0x10 },
	    	{ 0x2E, 0x12, 0x12 },
	    	{ 0x2C, 0x14, 0x14 },
	    	/* default BIOS setting */
	    	{ 0x2A, 0x16, 0x16 },
	    	{ 0x28, 0x18, 0x18 },
	    	{ 0x26, 0x1A, 0x1A },
	    	{ 0x24, 0x1C, 0x1C },
	    	{ 0x24, 0x1D, 0x1D },
	    	{ 0x22, 0x1F, 0x1F }
	    }
	},
	/* PAL */
	{
		/* 640x480 */
		{
	    	{ 0x3C, 0x0B, 0x0B },
	    	{ 0x3A, 0x0C, 0x0C },
	    	{ 0x38, 0x0E, 0x0E },
	    	{ 0x34, 0x11, 0x11 },
	    	{ 0x32, 0x13, 0x13 },
	    	/* default BIOS setting */
	    	{ 0x2E, 0x16, 0x16 },
	    	{ 0x2C, 0x18, 0x18 },
	    	{ 0x28, 0x1B, 0x1B },
	    	{ 0x26, 0x1D, 0x1D },
	    	{ 0x22, 0x20, 0x20 },
	    	{ 0x22, 0x21, 0x21 }
	    },
	    /* 800x600 */
		{
	    	{ 0x34, 0x17, 0x17 },
	    	{ 0x32, 0x19, 0x19 },
	    	{ 0x30, 0x1B, 0x1B },
	    	{ 0x2E, 0x1D, 0x1D },
	    	{ 0x2E, 0x1E, 0x1E },
	    	/* default BIOS setting */
	    	{ 0x2C, 0x20, 0x20 },
	    	{ 0x2C, 0x21, 0x21 },
	    	{ 0x2C, 0x22, 0x22 },
	    	{ 0x2A, 0x24, 0x24 },
	    	{ 0x2A, 0x25, 0x25 },
	    	{ 0x28, 0x27, 0x27 }
	    },
	    /* 1024x768 */
		{
	    	{ 0x2C, 0x27, 0x27 },
	    	{ 0x2C, 0x28, 0x28 },
	    	{ 0x2A, 0x2A, 0x2A },
	    	{ 0x28, 0x2C, 0x2C },
	    	{ 0x28, 0x2D, 0x2D },
	    	/* default BIOS setting */
	    	{ 0x26, 0x2F, 0x2F },
	    	{ 0x26, 0x30, 0x30 },
	    	{ 0x26, 0x31, 0x31 },
	    	{ 0x26, 0x32, 0x32 },
	    	{ 0x24, 0x35, 0x35 },
	    	{ 0x22, 0x37, 0x37 }
	    },
	    /* 848x480 */
		{
	    	{ 0x3C, 0x0B, 0x0B },
	    	{ 0x3A, 0x0C, 0x0C },
	    	{ 0x38, 0x0E, 0x0E },
	    	{ 0x34, 0x11, 0x11 },
	    	{ 0x32, 0x13, 0x13 },
	    	/* default BIOS setting */
	    	{ 0x2E, 0x16, 0x16 },
	    	{ 0x2C, 0x18, 0x18 },
	    	{ 0x28, 0x1B, 0x1B },
	    	{ 0x26, 0x1D, 0x1D },
	    	{ 0x22, 0x20, 0x20 },
	    	{ 0x22, 0x21, 0x21 }
	    }
	}
};

#endif /* _VIA_UTILITY_H_ */
