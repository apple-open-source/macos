/*********************************************************************\
* Module Name: 32FDDEF.H
*
* OS/2 Intelligent Font Interface
*
* Copyright (c) 1989,1994  IBM Corporation
* Copyright (c) 1989  Microsoft Corporation
*
\*********************************************************************/
#ifndef     __32FDDEF_H__
#define     __32FDDEF_H__

/* Typedef the Font Driver 32 Bit entry points  */

/* FdLoadFontFile */
typedef HFF  (* _syscall PFDLFF)(PSZ pszFileName);

/* FdQueryFaces */
typedef LONG (* _syscall PFDQF)(HFF hff, PIFIMETRICS pifim,
                                 ULONG cMetricLen, ULONG cFontCount,
                                 ULONG cStart);
/* FdConvertFontFile */
typedef LONG (* _syscall PFDCFF)(PSZ pszSrc, PSZ pszDestDir,
                                  PSZ pszName);

/* FdClaimFontFile */
typedef LONG (* _syscall PFDCLF)(PSZ pszFileName);

/* FdUnloadFontFile  */
typedef LONG (* _syscall PFDUFF)(HFF hff);

/* FdOpenFontContext */
typedef HFC  (* _syscall PFDOFC)(HFF hff, ULONG ulFont);

/* FdSetFontContext  */
typedef LONG (* _syscall PFDSFC)(HFC hfc, PCONTEXTINFO pci);

/* FdCloseFontContext */
typedef LONG (* _syscall PFDCFC)(HFC hfc);

/* FdQueryFaceAttr */
typedef LONG (* _syscall PFDQFA)(HFC hfc, ULONG iQuery, PBYTE pBuffer,
                                  ULONG cb, PGLYPH pagi, GLYPH gistart);

/* FdQueryCharAttr */
typedef LONG (* _syscall PFDQCA)(HFC hfc, PCHARATTR pCharAttr,
                                  PBITMAPMETRICS pbmm);

/* FdQueryFullFaces */
typedef LONG (* _syscall PFDQFF)(HFF hff, PVOID pBuf, PULONG cBufLen,
                                  PULONG cFontCount, ULONG cStart);
#endif

