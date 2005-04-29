/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/via/ddmpeg.h,v 1.4 2003/08/27 15:16:06 tsi Exp $ */
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

#ifndef _VIA_DDMPEG_H_
#define _VIA_DDMPEG_H_ 1

#include "via_capture.h"

/*
 *  Overlay surfaces ( frame buffers ) we use
 */
#define NUM_FRAME_BUFFERS           4

/*
 * Alignment macro functions
 */
#define ALIGN_TO_32_BYTES(f)         (((f) + 31) & ~31)
#define ALIGN_TO_16_BYTES(f)         (((f) + 15) & ~15)
#define ALIGN_TO_256_BITS(f)         (((f) + 255) & ~255)
#define ALIGN_TO_8_BYTES(f)          (((f) + 7) & ~7)
#define ALIGN_TO_64_BITS(f)          (((f) + 63) & ~63)
#define ENG_ALIGN_BYTE              ALIGN_TO_32_BYTES
#define ENG_ALIGN_BIT               ALIGN_TO_256_BITS

/*
 * FOURCC definitions
 */

#define FOURCC_VIA     0x4E4B4C57  /*VIA*/
#define FOURCC_SUBP    0x50425553  /*SUBP*/
#define FOURCC_TV0     0x00325654  /*TV0*/
#define FOURCC_TV1     0x00315654  /*TV1*/
#define FOURCC_ALPHA   0x48504C41  /*ALPH*/
#define FOURCC_YUY2    0x32595559  /*YUY2*/
#define FOURCC_YV12    0x32315659  /*YV12*/
#define FOURCC_Y211    0x31313259
#define FOURCC_UYVY    0x59565955
#define FOURCC_YVYU    0x55595659
#define FOURCC_IYUV    0x56555949  /* #define FOURCC_IYUV   'VUYI'*/

#define FOURCC_HQVMPEG 0x31565148  /*HQV1*/
#define FOURCC_HQVTV0  0x33565148  /*HQV3*/
#define FOURCC_HQVTV1  0x32565148  /*HQV2*/
#define FOURCC_HQVSW   0x34565148  /*HQV4*/



/*
 * Actions for MPEG functions
 */

#define CREATEDRIVER               0x00
#define DESTROYDRIVER              CREATEDRIVER +1
#define CREATESURFACE              CREATEDRIVER +2
#define DESTROYSURFACE             CREATEDRIVER +3
#define LOCKSURFACE                CREATEDRIVER +4
#define UNLOCKSURFACE              CREATEDRIVER +5
#define UPDATEOVERLAY              CREATEDRIVER +6
#define FLIP                       CREATEDRIVER +7  
#define SETALPHAWIN                CREATEDRIVER +8
#define BEGINPICTRE                CREATEDRIVER +9
#define BEGINPICTURE               CREATEDRIVER +9
#define ENDPICTURE                 CREATEDRIVER +10
#define SLICERECEIVEDATA           CREATEDRIVER +11
#define DRIVERPROC                 CREATEDRIVER +12
#define DISPLAYCONTROL             CREATEDRIVER +13
#define SUBPICTURE                 CREATEDRIVER +14
#define SETDEINTERLACEMODE         CREATEDRIVER +15
#define MPEGENABLE                 CREATEDRIVER +16  
#define MPEGGETPICTYPE             CREATEDRIVER +17  

/*
 * Actions for Capture functions  / XV standard functions
 */

#define PUTVIDEO                CREATEDRIVER +100
#define PUTSTILL                CREATEDRIVER +101
#define GETVIDEO                CREATEDRIVER +102
#define GETSTILL                CREATEDRIVER +103
#define STOPVIDEO               CREATEDRIVER +104
#define SETPORTATTRIBUTE        CREATEDRIVER +105
#define GETPORTATTRIBUTE        CREATEDRIVER +106
#define SELECTCAPTUREPORT       CREATEDRIVER +107
#define VIDEOCONTROL            CREATEDRIVER +108
#define HQVCONTROL              CREATEDRIVER +109
#define PUTIMAGE                CREATEDRIVER +110

#define VDEC_DETECT             CREATEDRIVER +150
#define VDEC_INIT               CREATEDRIVER +151
#define TUNER_DETECT            CREATEDRIVER +152
#define TUNER_INIT              CREATEDRIVER +153
#define TUNER_SETTVBAND_CABLE   CREATEDRIVER +154
#define TUNER_SETCHANNEL        CREATEDRIVER +155

#define AUDIOCONTROL            CREATEDRIVER +170

/*
 * Define for Utility functions using XvPutImage.
 */
/* Primary ID */
#define	UT_XV_FUNC_BIOS			0x11   /* Primary ID */
#define	UT_XV_FUNC_DRIVER		0x12
#define	UT_XV_FUNC_DEVICE		0x13
#define	UT_XV_FUNC_PANEL		0x14
#define	UT_XV_FUNC_TV			0x15
#define	UT_XV_FUNC_GAMMA		0x16
/* Secondary ID */
#define	UT_XV_FUNC_BIOS_GetChipID			0x01    /* Get Chip ID */
#define UT_XV_FUNC_BIOS_GetVersion			0x02    /* Get the version of the BIOS */
#define	UT_XV_FUNC_BIOS_GetDate				0x03    /* Get the date (year, month, day) of the BIOS. */
#define	UT_XV_FUNC_BIOS_GetVideoMemSizeMB		0x04    /* Get the video memory size, in MB */

#define	UT_XV_FUNC_DRIVER_GetFileName			0x01
#define	UT_XV_FUNC_DRIVER_GetFileVersion		0x02
/* Return value */
#define	UT_RESULT_OK			1    /* Return value */
#define	UT_RESULT_FAIL			0
#define	UT_RESULT_NO_FUNC		-1


/*
 * Structures for create surface
 */
typedef struct _DDSURFACEDESC
{
    unsigned long     dwSize;      /* size of the DDSURFACEDESC structure*/
    unsigned long     dwFlags;     /* determines what fields are valid*/
    unsigned long     dwHeight;    /* height of surface to be created*/
    unsigned long     dwWidth;     /* width of input surface*/
    unsigned long      lPitch;      /* distance to start of next line(return value)*/
    unsigned long     dwBackBufferCount;     /* number of back buffers requested*/
    void *    lpSurface;             /* pointer to the surface memory*/
    unsigned long     dwColorSpaceLowValue;  /* low boundary of color space that is to*/
                                     /* be treated as Color Key, inclusive*/
    unsigned long     dwColorSpaceHighValue; /* high boundary of color space that is*/
                                     /* to be treated as Color Key, inclusive*/
    unsigned long     dwFourCC;              /* (FOURCC code)*/
} DDSURFACEDESC;
typedef DDSURFACEDESC * LPDDSURFACEDESC;


/*
 * Structures for SubPicture
 */
typedef struct _SUBDEVICE
{
 unsigned char * lpSUBOverlaySurface[2];   /*Max 2 Pointers to SUB Overlay Surface*/
 unsigned long  dwSUBPhysicalAddr[2];     /*Max 2 Physical address to SUB Overlay Surface*/
 unsigned long  dwPitch;                  /*SUB frame buffer pitch*/
 unsigned long  gdwSUBSrcWidth;           /*SUB Source Width*/
 unsigned long  gdwSUBSrcHeight;          /*SUB Source Height*/
 unsigned long  gdwSUBDstWidth;           /*SUB Destination Width*/
 unsigned long  gdwSUBDstHeight;          /*SUB Destination Height*/
 unsigned long  gdwSUBDstLeft;            /*SUB Position : Left*/
 unsigned long  gdwSUBDstTop;             /*SUB Position : Top*/
}SUBDEVICE;
typedef SUBDEVICE * LPSUBDEVICE;


/*
 * Structures for H/W mpeg decode
 */
typedef struct _MPGDEVICE
{
 unsigned char * lpVideoMemIO;             /* Pointer to Video Memory MAP IO */
 unsigned char * lpMPEGOverlaySurface[NUM_FRAME_BUFFERS];/* Max 4 Pointers to MPEG Overlay Surface */
 unsigned long  dwMPEGPhysicalAddr[NUM_FRAME_BUFFERS];  /* Max 4 Physical address to MPEG Overlay Surface */
 unsigned long  dwWidth;                  /* MPEG coded_picture_width                                */
 unsigned long  dwHeight;                 /* MPEG coded_picture_height                               */
 unsigned long  dwPitch;                  /* MPEG frame buffer pitch                                 */
 unsigned long  dwPageNum;                /* Frame buffer Number                                     */
 unsigned char   byDeviceType;             /* Device type. Such as DEV_MPEG and DEV_SUBP              */
 unsigned long  gdwSetBufferIndex;        /* Used to assigned buffer pointer in SetOverlayBuffer()   */
 unsigned long  gdwMPGState;              /* MPG states                                              */
 unsigned long  gdwSUBPState;             /* Sub Picture states                                      */
 unsigned long  dwSubpPageNum;            /* SubPicture Frame buffer Number                          */
 unsigned long  dwSUBPPitch;              /* SubPicture Pitch                                        */
 unsigned long  gdwSUBPSrcLeft;           /* SubPicture Position : Left                              */
 unsigned long  gdwSUBPSrcTop;            /* SubPicture Position : Top                               */
 unsigned long  gdwSUBPSrcWidth;          /* SubPicture Source Width                                 */
 unsigned long  gdwSUBPSrcHeight;         /* SubPicture Source Height                                */
 unsigned long  gdwSUBPDisplayIndex;      /* Subpicture Display Index                                */
 unsigned long  gdwMPGSrcWidth;           /* MPEG Source Width                                       */
 unsigned long  gdwMPGSrcHeight;          /* MPEG Source Height                                      */
 unsigned long  gdwMPGDstWidth;           /* MPEG Destination Width                                  */
 unsigned long  gdwMPGDstHeight;          /* MPEG Destination Height                                 */
 unsigned long  gdwMPGDstLeft;            /* MPEG Position : Left                                    */
 unsigned long  gdwMPGDstTop;             /* MPEG Position : Top                                     */
 unsigned long  dwDeinterlaceMode;        /* BOB / WEAVE                                             */
 unsigned long  gdwSUBP_NotVisible;
 unsigned long  dwMPEGYPhysicalAddr[NUM_FRAME_BUFFERS];   /* Physical address to MPEG Y Overlay Surface  */
 unsigned long  dwMPEGCbPhysicalAddr[NUM_FRAME_BUFFERS];  /* Physical address to MPEG Cb Overlay Surface */
 unsigned long  dwMPEGCrPhysicalAddr[NUM_FRAME_BUFFERS];  /* Physical address to MPEG Cr Overlay Surface */
 unsigned long  dwMPEGDisplayIndex ;      /* Currently display index                     */
 unsigned long  dwHQVAddr[3];             /* Physical address to HQV surface -- CLE_C0   */
 /*unsigned long  dwHQVAddr[2];*/             /* Physical address to HQV surface             */
 unsigned long  dwEnableErrorConcealment; /* For MPEG ErrorConcealment                   */
 /* Chip Info */
 unsigned long  dwVendorID;
 unsigned long  dwDeviceID;
 unsigned long  dwRevisionID;
 unsigned long  dwSubVendorID;
 unsigned long  dwSubDeviceID;
}MPGDEVICE, * LPMPGDEVICE;


/*
 * Structures for S/W mpeg decode
 */
typedef struct _SWDEVICE
{
 unsigned char * lpSWOverlaySurface[2];   /* Max 2 Pointers to SW Overlay Surface*/
 unsigned long  dwSWPhysicalAddr[2];     /*Max 2 Physical address to SW Overlay Surface */
 unsigned long  dwSWCbPhysicalAddr[2];  /* Physical address to SW Cb Overlay Surface, for YV12 format use */
 unsigned long  dwSWCrPhysicalAddr[2];  /* Physical address to SW Cr Overlay Surface, for YV12 format use */
 unsigned long  dwHQVAddr[3];             /* Physical address to HQV surface -- CLE_C0   */
 /*unsigned long  dwHQVAddr[2];*/			  /*Max 2 Physical address to SW HQV Overlay Surface*/
 unsigned long  dwWidth;                  /*SW Source Width, not changed*/
 unsigned long  dwHeight;                 /*SW Source Height, not changed*/
 unsigned long  dwPitch;                  /*SW frame buffer pitch*/
 unsigned long  gdwSWSrcWidth;           /*SW Source Width, changed if window is out of screen*/
 unsigned long  gdwSWSrcHeight;          /*SW Source Height, changed if window is out of screen*/
 unsigned long  gdwSWDstWidth;           /*SW Destination Width*/
 unsigned long  gdwSWDstHeight;          /*SW Destination Height*/
 unsigned long  gdwSWDstLeft;            /*SW Position : Left*/
 unsigned long  gdwSWDstTop;             /*SW Position : Top*/
 unsigned long  dwDeinterlaceMode;        /*BOB / WEAVE*/
}SWDEVICE;
typedef SWDEVICE * LPSWDEVICE;


/* Device Type of MPGDEVICE */
#define DEV_MPEG    0
#define DEV_SUBP    1

/* dwDecodePictStruct and dwDisplayPictStruct of VIAMPGSURFACE */
#define VIA_PICT_STRUCT_TOP                 0x00000001
#define VIA_PICT_STRUCT_BOTTOM              0x00000002
#define VIA_PICT_STRUCT_FRAME               0x00000003

/*
 * Structures for LOCK surface
 */

typedef struct _DDLOCK
{
    unsigned long     dwVersion;             
    unsigned long     dwFourCC;
    unsigned long     dwPhysicalBase;
    SUBDEVICE SubDev;
    CAPDEVICE Capdev_TV0;
    CAPDEVICE Capdev_TV1;
    MPGDEVICE MPGDev;
    SWDEVICE SWDevice;
} DDLOCK;
typedef DDLOCK * LPDDLOCK;


/*
 * structure for passing information to DDHAL UpdateOverlay fn
 */
typedef struct _RECTL
{
    unsigned long     left;
    unsigned long     top;
    unsigned long     right;
    unsigned long     bottom;
} RECTL;

typedef struct _DDUPDATEOVERLAY
{
    RECTL     rDest;          /* dest rect */
    RECTL     rSrc;           /* src rect */
    unsigned long     dwFlags;        /* flags */
    unsigned long     dwColorSpaceLowValue;
    unsigned long     dwColorSpaceHighValue;
    unsigned long     dwFourcc;
} DDUPDATEOVERLAY;
typedef DDUPDATEOVERLAY * LPDDUPDATEOVERLAY;

typedef struct _ADJUSTFRAME
{
    int     x;
    int     y;
} ADJUSTFRAME;
typedef ADJUSTFRAME * LPADJUSTFRAME;

/* Definition for dwFlags */
#define DDOVER_HIDE       0x00000001
#define DDOVER_SHOW       0x00000002
#define DDOVER_KEYDEST    0x00000004
#define DDOVER_ENABLE     0x00000008
#define DDOVER_CLIP       0x00000010
/*
 * Display each field of video port data individually without
 * causing any jittery artifacts
 */
#define DDOVER_BOB                       	0x00200000l

/*
 * Indicates that bob/weave decisions should not be overridden by other
 * interfaces.
 */
#define DDOVER_OVERRIDEBOBWEAVE			0x00400000l

/*
 * Indicates that the surface memory is composed of interleaved fields.
 */
#define DDOVER_INTERLEAVED			0x00800000l

/*
 * Indicates that bob will be performed using hardware rather than
 * software or emulated.
 */
#define DDOVER_BOBHARDWARE			0x01000000l

typedef struct _DDFLIP
{
   unsigned long dwBuffIndex;
   unsigned long dwFlags;
} DDFLIP;
typedef DDFLIP * LPDDFLIP;

typedef struct
{
    CARD32         dwWidth;
    CARD32         dwHeight;
    CARD32         dwOffset;
    CARD32         dwUVoffset;
    CARD32         dwFlipTime;
    CARD32         dwFlipTag;
    CARD32         dwStartAddr;
    CARD32         dwV1OriWidth;
    CARD32         dwV1OriHeight;
    CARD32         dwV1OriPitch;
    CARD32         dwV1SrcWidth;
    CARD32         dwV1SrcHeight;
    CARD32         dwV1SrcLeft;
    CARD32         dwV1SrcRight;
    CARD32         dwV1SrcTop;
    CARD32         dwV1SrcBot;
    CARD32         dwSPWidth;
    CARD32         dwSPHeight;
    CARD32         dwSPLeft;
    CARD32         dwSPRight;
    CARD32         dwSPTop;
    CARD32         dwSPBot;
    CARD32         dwSPOffset;
    CARD32         dwSPstartAddr;
    CARD32         dwDisplayPictStruct;
    CARD32         dwDisplayBuffIndex;        /* Display buffer Index. 0 to ( dwBufferNumber -1) */
    CARD32         dwFetchAlignment;
    CARD32         dwSPPitch;
    unsigned long  dwHQVAddr[3];          /* CLE_C0 */
    /*unsigned long   dwHQVAddr[2];*/
    CARD32         dwMPEGDeinterlaceMode; /* default value : VIA_DEINTERLACE_WEAVE */
    CARD32         dwMPEGProgressiveMode; /* default value : VIA_PROGRESSIVE */
    CARD32         dwHQVheapInfo;         /* video memory heap of the HQV buffer */
    CARD32         dwVideoControl;        /* video control flag */
    CARD32         dwminifyH; 			   /* Horizontal minify factor */
    CARD32         dwminifyV;			   /* Vertical minify factor */
    CARD32         dwMpegDecoded;
} OVERLAYRECORD;

/* DeinterLace Mode */
#define VIA_DEINTERLACE_WEAVE               0x00000000
#define VIA_DEINTERLACE_BOB                 0x00000001
#define VIA_NON_PROGRESSIVE                 0x00000000
#define VIA_PROGRESSIVE                     0x00000010

/*
 * DDPIXELFORMAT
 */
typedef struct _DDPIXELFORMAT
{
	unsigned long	dwSize;			/* size of structure */
	unsigned long	dwFlags;		/* pixel format flags */
	unsigned long	dwFourCC;		/* (FOURCC code) */

	unsigned long	dwRGBBitCount;		/* how many bits per pixel */
	unsigned long	dwYUVBitCount;		/* how many bits per pixel */
	unsigned long	dwZBufferBitDepth;	/* how many bits for z buffers */
	unsigned long	dwAlphaBitDepth;	/* how many bits for alpha channels */

	unsigned long	dwRBitMask;		/* mask for red bit */
	unsigned long	dwYBitMask;		/* mask for Y bits */

	unsigned long	dwGBitMask;		/* mask for green bits */
	unsigned long	dwUBitMask;		/* mask for U bits */

	unsigned long	dwBBitMask;		/* mask for blue bits */
	unsigned long	dwVBitMask;		/* mask for V bits */

	unsigned long	dwRGBAlphaBitMask;	/* mask for alpha channel */
	unsigned long	dwYUVAlphaBitMask;	/* mask for alpha channel */
	unsigned long	dwRGBZBitMask;		/* mask for Z channel */
	unsigned long	dwYUVZBitMask;		/* mask for Z channel */
} DDPIXELFORMAT;
typedef DDPIXELFORMAT * LPDDPIXELFORMAT;

/****************************************************************************
 *
 * PIXELFORMAT FLAGS
 *
 ****************************************************************************/

/*
 * The FourCC code is valid.
 */
#define DDPF_FOURCC				0x00000004l

/*
 * The RGB data in the pixel format structure is valid.
 */
#define DDPF_RGB				0x00000040l



/*
 * Return value of Proprietary Interface
 */

#define PI_OK                              0x00
#define PI_ERR                             0x01
#define PI_ERR_NO_X_WINDOW                 PI_ERR +1
#define PI_ERR_CANNOT_OPEN_VIDEO_DEVICE    PI_ERR +2
#define PI_ERR_CANNOT_USE_IOCTL            PI_ERR +3
#define PI_ERR_CANNOT_CREATE_SURFACE       PI_ERR +4


/*
 * Exported Driver functions
 */

unsigned long VIADriverProc(unsigned long wAction, void * lpParam);
unsigned long VIABeginPicture(void * lpMPGSurface);
unsigned long VIAEndPicture(void * lpMPGSurface);
unsigned long VIASliceReceiveData(unsigned long dwByteCount, unsigned char * lpData);
unsigned long VIADisplayControl(unsigned long devType, void * lpData);
unsigned long VIASUBPicture(void * lpSubp);
unsigned long VIASetDeInterlaceMode(unsigned long dwMode);
int PrivPutImage(unsigned char* buf);

#endif /* _VIA_DDMPEG_H_ */
