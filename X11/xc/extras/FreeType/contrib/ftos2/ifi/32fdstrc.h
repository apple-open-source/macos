/*********************************************************************\
* Module Name: 32FDSTRC.H
*
* OS/2 Intelligent Font Interface
*
* Copyright (c) 1989,1994  IBM Corporation
* Copyright (c) 1989  Microsoft Corporation
*
\*********************************************************************/
#ifndef     __32FDSTRC_H__
#define     __32FDSTRC_H__

#define FACESIZE 32
#define GLYPHNAMESIZE 16

/* Error codes defined to be returned by IFI */
/* NOTE:  The actual values are subject to change */

/*#define PMERR_BUFFER_TOO_SMALL          23003L*/
#define PMERR_FACENAME_NOT_FOUND        23004L
#define PMERR_FD_ALREADY_INSTALLED      23005L
#define PMERR_INVALID_CONTEXTINFO       23006L
#define PMERR_NOT_A_FONT_FILE           23007L
#define PMERR_INVALID_FONT_SELECTION    23008L
#define PMERR_INVALID_FORMAT            23009L
#define PMERR_BUSY_HFC                  230010L
#define PMERR_INVALID_HFC               230011L
#define PMERR_INVALID_INDEX             230012L
#define PMERR_INVALID_QUERY_TYPE        230013L
#define PMERR_CONTEXT_NOT_SET           230014L

/* Query faces subfunction */
#define FD_QUERY_CONTEXTMETRICS 1L
#define FD_QUERY_ABC_WIDTHS     2L
#define FD_QUERY_KERNINGPAIRS   3L

/* Query char subfunction */
#define FD_QUERY_CHARIMAGE      1L
#define FD_QUERY_OUTLINE        2L
#define FD_QUERY_BITMAPMETRICS  4L

#define FD_CHARATTR_ALIGNED_8           0x00000001
#define FD_CHARATTR_ALIGNED_16          0x00000002
#define FD_CHARATTR_ALIGNED_32          0x00000004
#define FD_CHARATTR_NO_CACHE            0x00000010

typedef struct _ABC_TRIPLETS /*abc*/
{
   LONG  lA;
   ULONG ulB;
   LONG  lC;
} ABC_TRIPLETS;
typedef ABC_TRIPLETS *PABC_TRIPLETS;

// THIS STRUCTURE NOW RESIDES IN PMDDI.H FOR CRUISER WORLD
// BUT IFI FONT DRIVER DOES NOT INCLUDE PMDDI.H

#ifndef INCL_IFI
typedef struct _POINTFX { /* ptfx */
    FIXED x;
    FIXED y;
} POINTFX;
typedef POINTFX *PPOINTFX;
#endif

typedef struct _BITMAPMETRICS /* bmm */
{
     SIZEL     sizlExtent;
     ULONG     cyAscent;
#ifdef OLD_DRIVER
     POINTFX *ppfxOrigin;     /* Return character origin. */
#else
     POINTFX    pfxOrigin;    /* Return character origin. */
#endif
} BITMAPMETRICS;
typedef BITMAPMETRICS *PBITMAPMETRICS;

typedef struct _MAT2 /* mat */
{
     FIXED eM11;
     FIXED eM12;
     FIXED eM21;
     FIXED eM22;
} MAT2;

typedef struct _FD_KERNINGPAIRS  /* krnpr */
{
     GLYPH     giFirst;
     GLYPH     giSecond;
     LONG      eKerningAmount;
} FD_KERNINGPAIRS;

typedef struct _CONTEXTINFO /* ci */
{
     ULONG     cb;        /* Length in bytes of this structure. */
     ULONG     fl;        /* Flags. */
     SIZEL     sizlPPM;   /* Device resolution in pels/meter. */
     POINTFX   pfxSpot;   /* Spot size in pels. */
     MAT2      matXform;  /* Notional to Device transform. */
} CONTEXTINFO;
typedef CONTEXTINFO *PCONTEXTINFO;

typedef struct _CHARATTR  /* ca */
{
    ULONG     cb;
    ULONG     iQuery;   /* Query type. */
    GLYPH     gi;       /* Glyph index in font. */
    PBYTE     pBuffer;  /* Bitmap buffer. */
    ULONG     cbLen;    /* Size of buffer in bytes. */
} CHARATTR;
typedef CHARATTR *PCHARATTR;

typedef struct _CHARATTR2  /* ca2 */
{
    ULONG     cb;
    ULONG     iQuery;   /* Query type. */
    GLYPH     gi;       /* Glyph index in font. */
    PBYTE     pBuffer;  /* Bitmap buffer. */
    ULONG     cbLen;    /* Size of buffer in bytes. */
    ULONG     fl;       /* Flags */
} CHARATTR2;
typedef CHARATTR2 *PCHARATTR2;

typedef struct _CONTEXTMETRICS
{
    SIZEL   sizlMax;
    ULONG   cyMaxAscent;
    ULONG   cyMaxDescent;
    ULONG   cxTotal;
    ULONG   cGlyphs;
} CONTEXTMETRICS;
typedef CONTEXTMETRICS * PCONTEXTMETRICS;

typedef struct _POLYGONHEADER {
  ULONG cb;
  ULONG iType;  /*  Must be FD_POLYGON_TYPE */
} POLYGONHEADER;
typedef POLYGONHEADER *PPOLYGONHEADER;

typedef struct _PRIMLINE {
  ULONG iType;  /* Must be FD_PRIM_LINE */
  POINTFX pte;
} PRIMLINE;
typedef PRIMLINE *PPRIMLINE;

typedef struct _PRIMSPLINE {
  ULONG iType;  /* Must be FD_PRIM_SPLINE */
  POINTFX pte[3];
} PRIMSPLINE;
typedef PRIMSPLINE *PPRIMSPLINE;

/*
 * The names of these were changed to avoid conflict with PRIM_LINE
 * which is defined ion some other header file.
 */
#define FD_POLYGON_TYPE 24
#define FD_PRIM_LINE    1
#define FD_PRIM_SPLINE  3

#endif

