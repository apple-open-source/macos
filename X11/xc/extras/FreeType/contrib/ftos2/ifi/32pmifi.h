/*********************************************************************\
* Module Name: 32PMIFI.H
*
* OS/2 Intelligent Font Interface
*
* Copyright (c) 1989,1994  IBM Corporation
* Copyright (c) 1989  Microsoft Corporation
*
\*********************************************************************/
#ifndef __32PMIFI_H__
#define __32PMIFI_H__

#define INCL_IFD

typedef PVOID HFF;      /* Font file handle */
typedef PVOID HFC;      /* Font context handle */

#ifndef INCL_GRE_FONTS
typedef ULONG GLYPH;    /* gi */
typedef ULONG *PGLYPH;  /* pgi */
#endif

#include "32fdstrc.h"   /* Font Driver structures */
#include "32ifimet.h"   /* Pifi Metrics */
#include "32fddef.h"    /* Font Driver entry definitions */


typedef struct _FDDISPATCH16 { /* fdisp */
        PVOID  FdLoadFontFile;
        PVOID  FdQueryFaces;
        PVOID  FdUnloadFontFile;
        PVOID  FdOpenFontContext;
        PVOID  FdSetFontContext;
        PVOID  FdCloseFontContext;
        PVOID  FdQueryFaceAttr;
        PVOID  FdQueryCharAttr;
        PVOID  FdClaimFontFile;
        PVOID  FdConvertFontFile;
} FDDISPATCH16;
typedef FDDISPATCH16 FAR *PFDDISPATCH16;


typedef struct _FDDISPATCH { /* fdisp */
        PFDLFF  FdLoadFontFile;
        PFDQF   FdQueryFaces;
        PFDUFF  FdUnloadFontFile;
        PFDOFC  FdOpenFontContext;
        PFDSFC  FdSetFontContext;
        PFDCFC  FdCloseFontContext;
        PFDQFA  FdQueryFaceAttr;
        PFDQCA  FdQueryCharAttr;
        PFDCLF  FdClaimFontFile;
        PFDCFF  FdConvertFontFile;
        PFDQFF  FdQueryFullFaces;
} FDDISPATCH;
typedef FDDISPATCH *PFDDISPATCH;

typedef struct _FDHEADER { /* fdhdr */
        ULONG   cbLength;               /* Length of FDHEADER */
        UCHAR   strId[16];              /* String 'OS/2 FONT DRIVER' */
        UCHAR   szTechnology[40];       /* Identifier of Font Driver technology */
        ULONG   ulVersion;              /* IFI version number (0x0100) */
        ULONG   ufDeviceCaps;           /* Capabilities of device */
        PFDDISPATCH pfddisp;
} FDHEADER;

typedef FDHEADER FAR *PFDHEADER;

#define OK     0
#define ERROR  -1

#define IFI_VERSION        10
#define IFI_VERSION20      20
#define IFI_VERSION21      21

#define FD_DISPATCH_COUNT  11
#define DISPATCHTABLE "FONT_DRIVER_DISPATCH_TABLE"

#endif
