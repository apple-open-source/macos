/*********************************************************************\
* Module Name: 32IFIMET.H
*
* OS/2 Intelligent Font Interface
*
* Copyright (c) 1989,1994  IBM Corporation
* Copyright (c) 1989  Microsoft Corporation
*
* Definition and description of IFIMETRICS structure
* This file is included by FDSTRUCS.H
*
\*********************************************************************/
/* $XFree86: xc/extras/FreeType/contrib/ftos2/ifi/32ifimet.h,v 1.2 2003/01/12 03:55:43 tsi Exp $ */
#ifndef     __32IFIMET_H__
#define     __32IFIMET_H__

#define FACESIZE 32
#define GLYPHNAMESIZE 16

/* #defines for fsType in IFIMETRICS  */

#define IFIMETRICS_FIXED       0x0001   /*Fixed pitch */
#define IFIMETRICS_LICENSED    0x0002   /*Font subject of licensing agreement */
#define IFIMETRICS_KERNING     0x0004   /*Font has kerning data */
#define IFIMETRICS_DBCS        0x0010   /*DBCS font */
#define IFIMETRICS_MBCS        0x0018   /*MBCS (DBCS + SBCS) font */
/* Reserved                    0x8000 */
#define IFIMETRICS_ATOMS       0x4000   /*The atom name fields are valid */
#define IFIMETRICS_FAMTRUNC    0x2000   /*Familyname field is truncated */
#define IFIMETRICS_FACETRUNC   0x1000   /*Facename field is truncated */
#define IFIMETRICS_ANTIALIASED 0x0020
#define IFIMETRICS_UNICODE     0x0040
#define IFIMETRICS_NO_CACHE    0x0080

/* #defines for fsDefn in IFIMETRICS   */

#define IFIMETRICS_OUTLINE     0x0001   /*1 - Outline. 0 - Raster */
/* Reserved                    0x0002 */
/* Reserved                    0x0004 */
/* Reserved                    0x8000 */
#define IFIMETRICS_UDC_FONT    0x0010   /*User defined font */
/* Reserved                           */

/* #defines for fsSelection in IFIMETRICS valid for bitmap or outline fonts  */

#define IFIMETRICS_ITALIC      0x8000  /*Italic */
#define IFIMETRICS_UNDERSCORE  0x4000  /*Underscored */
#define IFIMETRICS_OVERSTRUCK  0x2000  /*Overstruck */

/* #defines for fsSelection in IFIMETRICS valid for bitmap fonts */

#define IFIMETRICS_NEGATIVE    0x1000   /*Negative image */
#define IFIMETRICS_HOLLOW      0x0800   /*Outline (hollow) */

#if defined(__IBMCPP__) || defined(__IBMC__)
#    pragma pack(1)
#else
#    pragma Align_members(1)
#endif

typedef struct _IFIMETRICS    /* ifim */
{                                                                    /* UNITS */
  UCHAR   szFamilyname[FACESIZE];   /*Font Family Name, e.g. Roman */
  UCHAR   szFacename[FACESIZE];     /*Face name, e.g. Tms Rmn Bold Italic */
  UCHAR   szGlyphlistName[GLYPHNAMESIZE]; /*e.g. PM316, Latin-2, Greek */
  USHORT  idRegistry;          /*IBM registration number (or zero).        I */
  LONG    lCapEmHeight;        /*Height of uppercase M                     N */
  LONG    lXHeight;            /*Nominal height of lowercase               N */
  LONG    lMaxAscender;        /*Maximum height above baseline of any char N */
  LONG    lMaxDescender;       /*Maximum depth below baseline of any char  N */
  LONG    lLowerCaseAscent;    /*Maximum height above baseline of any a-z  N */
  LONG    lLowerCaseDescent;   /*Maximum depth below basiline of any a-z   N */
  LONG    lInternalLeading;    /*White space within character              N */
  LONG    lExternalLeading;    /*White space between lines                 N */
  LONG    lAveCharWidth;       /*Weighted average character width          N */
  LONG    lMaxCharInc;         /*Maximum character increment               N */
  LONG    lEmInc;              /*Increment for Capitals (typically 'M')    N */
  LONG    lMaxBaselineExt;     /*Height of character cell                  N */
  FIXED   fxCharSlope;         /*Slope angle, degrees, clockwise           D */
  FIXED   fxInlineDir;         /*Drawing direction, degrees clockwise      D */
  FIXED   fxCharRot;           /*Glyph rotation in cell, degrees clockwise D */
  USHORT  usWeightClass;       /*Character weight, 1-9 (1=ultra-light)     I */
  USHORT  usWidthClass;        /*Character width, 1-9 (1=ultra condensed)  I */
  LONG    lEmSquareSizeX;      /*Em Square size, x-direction               N */
  LONG    lEmSquareSizeY;      /*Em Square size, y-direction               N */
  GLYPH   giFirstChar;         /*Number of first glyph in font             I */
  GLYPH   giLastChar;          /*Number of last glyph in font              I */
  GLYPH   giDefaultChar;       /*Glyph used if requested glyph invalid     I */
  GLYPH   giBreakChar;         /*Space glyph                               I */
  USHORT  usNominalPointSize;  /*Point size for which font was designed    N */
  USHORT  usMinimumPointSize;  /*Minimum point size scaling for font       N */
  USHORT  usMaximumPointSize;  /*Maximum point size scaling for font       N */
  USHORT  fsType;              /*Type indicators  (see #defines)           B */
  USHORT  fsDefn;              /*Font definition data (see #defines)       B */
  USHORT  fsSelection;         /*Font selection flags (see #defines)       B */
  USHORT  fsCapabilities;      /*Font capabilities must be 0               B */
  LONG    lSubscriptXSize;     /*Size in x-direction of subscript          N */
  LONG    lSubscriptYSize;     /*Size in y-direction of subscript          N */
  LONG    lSubscriptXOffset;   /*Offset in x-direction of subscript        N */
  LONG    lSubscriptYOffset;   /*Offset in y-direction of subscript        N */
  LONG    lSuperscriptXSize;   /*Size in x-direction of superscript        N */
  LONG    lSuperscriptYSize;   /*Size in y-direction of superscript        N */
  LONG    lSuperscriptXOffset; /*Offset in x-direction of superscript      N */
  LONG    lSuperscriptYOffset; /*Offset in y-direction of superscript      N */
  LONG    lUnderscoreSize;     /*Underscore size                           N */
  LONG    lUnderscorePosition; /*Underscore position                       N */
  LONG    lStrikeoutSize;      /*Strikeout size                            N */
  LONG    lStrikeoutPosition;  /*Strikeout position                        N */
  SHORT   cKerningPairs;       /*Number of kerning pairs in pair table     I */
  ULONG   ulFontClass;         /*IBM font classification                   B */
} IFIMETRICS;
typedef IFIMETRICS FAR *PIFIMETRICS;
#if defined(__IBMCPP__) || defined(__IBMC__)
#    pragma pack()
#else
#    pragma Align_members()
#endif

#endif
