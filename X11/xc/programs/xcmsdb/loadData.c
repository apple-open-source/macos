/* $Xorg: loadData.c,v 1.4 2000/08/17 19:54:13 cpqbld Exp $ */

/*
 * (c) Copyright 1990 Tektronix Inc.
 * 	All Rights Reserved
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of Tektronix not be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.
 *
 * Tektronix disclaims all warranties with regard to this software, including
 * all implied warranties of merchantability and fitness, in no event shall
 * Tektronix be liable for any special, indirect or consequential damages or
 * any damages whatsoever resulting from loss of use, data or profits,
 * whether in an action of contract, negligence or other tortious action,
 * arising out of or in connection with the use or performance of this
 * software.
 *
 *
 *	NAME
 *		LoadSCCData.c
 *
 *	DESCRIPTION
 *		TekCMS API routine that reads screen data from a file
 *	        and then loads the data on the root window of the screen.
 *		
 *
 *
 */
/* $XFree86: xc/programs/xcmsdb/loadData.c,v 3.3 2001/07/25 15:05:18 dawes Exp $ */

/*
 *      INCLUDES
 */

#include <X11/Xos.h>
#include <sys/stat.h>
#include <stdio.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include "SCCDFile.h"


/*
 *      EXTERNS
 *              External declarations required locally to this package
 *              that are not already declared in any of the included header
 *		files (external includes or internal includes).
 */

#include <stdlib.h>

/*
 *      LOCAL TYPEDEFS
 *              typedefs local to this package (for use with local vars).
 *
 */

typedef struct _DefineEntry {
    char	*pString;
    int		define;
} DefineEntry;


/*
 *      LOCAL VARIABLES
 */
static int linenum = 0;

static DefineEntry KeyTbl[] = {
    { SC_BEGIN_KEYWORD,			SC_BEGIN },
    { SC_END_KEYWORD,			SC_END },
    { COMMENT_KEYWORD,			COMMENT },
    { NAME_KEYWORD,			NAME },
    { MODEL_KEYWORD,			MODEL },
    { PART_NUMBER_KEYWORD,		PART_NUMBER },
    { SERIAL_NUMBER_KEYWORD,		SERIAL_NUMBER },
    { REVISION_KEYWORD,			REVISION },
    { SCREEN_CLASS_KEYWORD,		SCREEN_CLASS },
    { COLORIMETRIC_BEGIN_KEYWORD,	COLORIMETRIC_BEGIN },
    { COLORIMETRIC_END_KEYWORD,		COLORIMETRIC_END },
    { XYZTORGBMAT_BEGIN_KEYWORD,	XYZTORGBMAT_BEGIN },
    { XYZTORGBMAT_END_KEYWORD,		XYZTORGBMAT_END },
    { WHITEPT_XYZ_BEGIN_KEYWORD,	WHITEPT_XYZ_BEGIN },
    { WHITEPT_XYZ_END_KEYWORD,		WHITEPT_XYZ_END },
    { RGBTOXYZMAT_BEGIN_KEYWORD,	RGBTOXYZMAT_BEGIN },
    { RGBTOXYZMAT_END_KEYWORD,		RGBTOXYZMAT_END },
    { IPROFILE_BEGIN_KEYWORD,		IPROFILE_BEGIN },
    { IPROFILE_END_KEYWORD,		IPROFILE_END },
    { ITBL_BEGIN_KEYWORD,		ITBL_BEGIN },
    { ITBL_END_KEYWORD,			ITBL_END },
    { "",				-1 }
};

static DefineEntry ScrnClassTbl[] = {
    { VIDEO_RGB_KEYWORD,		VIDEO_RGB },
#ifdef GRAY
    { VIDEO_GRAY_KEYWORD,		VIDEO_GRAY },
#endif /* GRAY */
    { "",				-1 }
};

#define KEY_VISUALID		1
#define KEY_DEPTH		2
#define KEY_CLASS		3
#define KEY_RED_MASK		4
#define KEY_GREEN_MASK		5
#define KEY_BLUE_MASK		6
#define KEY_COLORMAP_SIZE	7
#define KEY_BITS_PER_RGB	8

static DefineEntry VisualOptKeyTbl[] = {
    { "visualid",		KEY_VISUALID },
    { "depth",			KEY_DEPTH },
    { "class",			KEY_CLASS },
    { "red_mask",		KEY_RED_MASK },
    { "green_mask",		KEY_GREEN_MASK },
    { "blue_mask",		KEY_BLUE_MASK },
    { "colormap_size",		KEY_COLORMAP_SIZE },
    { "bits_per_rgb",		KEY_BITS_PER_RGB },
    { "",				-1 }
};
static DefineEntry VisualClassTbl[] = {
    { "StaticGray",		StaticGray },
    { "GrayScale",		GrayScale },
    { "StaticColor",		StaticColor },
    { "PseudoColor",		PseudoColor },
    { "TrueColor",		TrueColor },
    { "DirectColor",		DirectColor },
    { "",				-1 }
};


/************************************************************************
 *									*
 *			 PRIVATE ROUTINES				*
 *									*
 ************************************************************************/

/*
 *	NAME
 *		StrToDefine - convert a string to a define
 *
 *	SYNOPSIS
 */
static int
StrToDefine(DefineEntry pde[],	/* IN: table of X string-define pairs     */
				/*     last entry must contain pair "", 0 */
	    char *pstring)	/* IN: string to be looked up in that table */
/*
 *	DESCRIPTION
 *		Converts a string to an integer define.
 *
 *		Looks up the string in the table and returns the integer
 *		associated with the string.
 *
 *		Later may need similar function for unsigned long define.
 *
 *
 *
 *	RETURNS
 *		The int equivalent of the defined string.
 *		-1 if the string is not found in table
 *
 */
{
    while( strcmp(pde->pString,"") != 0 ){
	if( strcmp(pde->pString,pstring) == 0){
	    return(pde->define);
	}
	pde++;
    }
    return(-1);
}

/*
 *	NAME
 *		DefineToStr
 *
 *	SYNOPSIS
 */
static char *
DefineToStr(DefineEntry pde[],	/* IN: table of X string-define pairs */
				/*     last entry must contain pair "", 0 */
	    int id)		/* IN: id to be looked up in that table	*/
/*
 *	DESCRIPTION
 *		Converts a string to an integer define.
 *
 *		Looks up the string in the table and returns the integer
 *		associated with the string.
 *
 *		Later may need similar function for unsigned long define.
 *
 *
 *
 *	RETURNS
 *		The int equivalent of the defined string.
 *		-1 if the string is not found in table
 *
 */
{
    while(pde->define != -1) {
	if (pde->define == id) {
	    return(pde->pString);
	}
	pde++;
    }
    return(NULL);
}

/*
 *	NAME
 *		SCKeyOf - convert keyword into key ID
 *
 *	SYNOPSIS
 */
static int
SCKeyOf(char *string)
/*
 *	DESCRIPTION
 *		Converts a string to an integer define.
 *
 *		Looks up the string in the table and returns the integer
 *		associated with the string.
 *
 *		Later may need similar function for unsigned long define.
 *
 *
 *
 *	RETURNS
 *		The int equivalent of the defined string.
 *		-1 if the string is not found in table
 *
 */
{
    return(StrToDefine(KeyTbl, string));
}


/*
 *	NAME
 *		SCScrnClassOf - convert screen class string into class ID
 *
 *	SYNOPSIS
 */
static int
SCScrnClassOf(char *string)
/*
 *	DESCRIPTION
 *		Converts a string to an integer define.
 *
 *		Looks up the string in the table and returns the integer
 *		associated with the string.
 *
 *		Later may need similar function for unsigned long define.
 *
 *
 *
 *	RETURNS
 *		The int equivalent of the defined string.
 *		-1 if the string is not found in table
 *
 */
{
    return(StrToDefine(ScrnClassTbl, string));
}


/*
 *	NAME
 *		SCScrnClassStringOf - convert screen class id into class string
 *
 *	SYNOPSIS
 */
static char *
SCScrnClassStringOf(int id)
/*
 *	DESCRIPTION
 *		Converts a id to astring
 *
 *	RETURNS
 *		Pointer to string if found; otherwise NULL.
 *
 */
{
    return(DefineToStr(ScrnClassTbl, id));
}

/* close the stream and return any memory allocated. */
/*ARGSUSED*/
static void 
closeS(FILE *stream, XDCCC_Correction *pCorrection) 
{
    XDCCC_Correction* pNext;
    if (stream) {
        fclose (stream);
    }
    while (pCorrection) {
	pNext = pCorrection->next;
	free(pCorrection);
	pCorrection = pNext;
    }
}

/*
 *  Get a line of text from the stream.
 */
static char *
nextline(char *buf, int maxch, FILE *stream)
{
    linenum++;
    return (fgets(buf, maxch, stream));
}


static int
ProcessColorimetric(FILE *stream, XDCCC_Matrix *pMatrix, int VisualFlag)
{
    char buf[BUFSIZ];
    char keyword[BUFSIZ];
    char token[BUFSIZ], *ptoken;
    int  ntok;
    unsigned int matrices_processed = 0;
		/* bit 0 for XYZtoRGB matrix */
		/* bit 1 for RGBtoXYZ matrix */
    int	 state = 0;
		 /* 0 -- looking for matrix */
		 /* 1 -- processing data from matrix */
		 /* 2 -- both matrices processed */
		 /* Note: the order of the matrices is not important. */
    int	 count = -1;
    XcmsFloat *pElement = NULL;

    while ((nextline(buf, BUFSIZ, stream)) != NULL) {
	if ((ntok = sscanf(buf, "%s %s", keyword, token)) > 0) {
	    switch (SCKeyOf(keyword)) {
	      case XYZTORGBMAT_BEGIN :
		if (VisualFlag != VIDEO_RGB) {
		  fprintf(stderr, 
			 "Line %d: Keyword XYZTORGBMAT_BEGIN mismatch for visual %s.\n",
			  linenum, SCScrnClassStringOf(VisualFlag));
		  return (0);
		}
		if (state != 0) {
		  fprintf(stderr,
			  "Line %d: Extraneous keyword %s.\n", 
			  linenum, keyword);
		  return (0);
		}
		state = 1;
		count = 0;
		pElement = (XcmsFloat *) pMatrix->XYZtoRGBmatrix;
		break;
	      case XYZTORGBMAT_END :
		if (VisualFlag != VIDEO_RGB) {
		  fprintf(stderr, 
			  "Line %d: Keyword XYZTORGBMAT_END mismatch for visual %s.\n",
			  linenum, SCScrnClassStringOf(VisualFlag));
		  return (0);
		}
		if ((state != 1) || (count != 9)) {
		  fprintf(stderr,
			  "Line %d: Incomplete XYZtoRGB matrix -- Premature %s\n", 
			  linenum, keyword);
		  return (0);
		}
		matrices_processed |= 0x1;
		if (matrices_processed == 3) {
		    state = 2;
		} else {
		    state = 0;
		}
		break;
	      case RGBTOXYZMAT_BEGIN :
		if (VisualFlag != VIDEO_RGB) {
		  fprintf(stderr,
			 "Line %d: Keyword RGBTOXYZMAT_BEGIN mismatch for visual %s.\n",
			  linenum, SCScrnClassStringOf(VisualFlag));
		  return (0);
		}
		if (state != 0) {
		    fprintf(stderr, "Line %d: Extraneous keyword %s.\n", 
			    linenum, keyword);
		    return (0);
		}
		state = 1;
		count = 0;
		pElement = (XcmsFloat *) pMatrix->RGBtoXYZmatrix;
		break;
	      case RGBTOXYZMAT_END :
		if (VisualFlag != VIDEO_RGB) {
		    fprintf(stderr, 
			   "Line %d: Keyword RGBTOXYZMAT_END mismatch for visual %s.\n",
			    linenum, SCScrnClassStringOf(VisualFlag));
		    return (0);
		}
		if ((state != 1) || (count != 9)) {
		    fprintf(stderr, 
			   "Line %d: Incomplete RGBtoXYZ matrix -- Premature %s\n", 
			    linenum, keyword);
		    return (0);
		}
		matrices_processed |= 0x2;
		if (matrices_processed == 3) {
		    state = 2;
		} else {
		    state = 0;
		}
		break;
#ifdef GRAY
	      case WHITEPT_XYZ_BEGIN :
		if (VisualFlag != VIDEO_GRAY) {
		    fprintf(stderr,
			 "Line %d: Keyword WHITEPT_XYZ_BEGIN mismatch for visual %s.\n",
			    linenum, SCScrnClassStringOf(VisualFlag));
		    return (0);
		}
		if (state != 0) {
		  fprintf(stderr,
			  "Line %d: Extraneous keyword %s.\n", 
			  linenum, keyword);
		  return (0);
		}
		state = 1;
		count = 0;
		pElement = (XcmsFloat *) pMatrix->XYZtoRGBmatrix;
		break;
	      case WHITEPT_XYZ_END :
		if (VisualFlag != VIDEO_GRAY) {
		    fprintf(stderr,
			   "Line %d: Keyword WHITEPT_XYZ_END mismatch for visual %s.\n",
			    linenum, SCScrnClassStringOf(VisualFlag));
		    return (0);
		}
		if ((state != 1) || (count != 3)) {
		    fprintf(stderr,
			"Line %d: Incomplete white point -- Premature %s\n", 
			    linenum, keyword);
		    return (0);
		}
		state = 2;
		break;
#endif /* GRAY */
	      case DATA :
		for (ptoken = strtok(buf, DATA_DELIMS); ptoken != NULL;
			ptoken = strtok(NULL, DATA_DELIMS)) {
		    if (sscanf(ptoken, "%lf", pElement) != 1) {
			if (VisualFlag == VIDEO_RGB) {
			    fprintf(stderr,
				    "Line %d: Invalid matrix value %s.", 
				    linenum, ptoken);
			} else {
			    fprintf(stderr,
				    "Line %d: Invalid CIEXYZ value %s.\n",
				    linenum, ptoken);
			}
			return (0);
		    }
		    pElement++;
		    if (VisualFlag == VIDEO_RGB) {
			if (++count > 9) {
			    fprintf(stderr,
				   "Line %d: Extra matrix value %s\n", 
				    linenum, ptoken);
			    return (0);
			}
		    } else {
			if (++count > 3) {
			    fprintf(stderr,
				    "Line %d: Extra CIEXYZ value %s.\n",
				    linenum, ptoken);
			    return (0);
			  }
		    }
		}
		break;
	      case COLORIMETRIC_BEGIN :
		fprintf(stderr, 
			"Line %d: Extraneous keyword %s.\n", 
			linenum, keyword);
		return (0);
/* NOTREACHED */break;	
	      case COLORIMETRIC_END :
		if (state != 2) {
		    fprintf(stderr,
		   "Line %d: Incomplete Colorimetric data -- Premature %s\n",
			    linenum, keyword);
		    return (0);
		}
		return (1);
	      case COMMENT :
		/* Currently, do nothing. */
		break;
	      default :
		fprintf(stderr,
			"Line %d: Unexpected keyword %s\n",
			linenum, keyword);
		return (0);
/* NOTREACHED */break;		
	    }
	} else if (ntok < 0) {
	    /* mismatch */
	    fprintf(stderr, "Line %d: Unrecognized keyword\n", linenum);
	    return (0);
/* NOTREACHED */break;		
	}
    }
    return (0);
}

static int
ProcessIProfile(FILE *stream, XDCCC_Correction *pCorrection)
{
    char buf[BUFSIZ];
    char *keyword;
    char *tableStr, *sizeStr, *ptoken;
    int  size;
    int	 state = 0;
	 /************************************************
	  * 0 -- Looking for Intensity Table(s)          *
	  * 1 -- Processing Intensity Table(s)           *
          ************************************************/
    int	 nTbl = 0;
    int	 count = 0;
    IntensityRec *pIRec = NULL;

    while ((nextline(buf, BUFSIZ, stream)) != NULL) {
	ptoken = keyword = strtok(buf, DATA_DELIMS);
	if (keyword != (char*)NULL) {
	    switch (SCKeyOf(keyword)) {
	      case ITBL_BEGIN :
		if (state != 0) {
		    fprintf(stderr,"Line %d: unexpected keyword %s\n", 
			   linenum, keyword);
		    return (0);
		}
		tableStr = strtok((char*)NULL, DATA_DELIMS);
		sizeStr = strtok((char*)NULL, DATA_DELIMS);
		if ((sizeStr == (char*)NULL) ||
			sscanf(sizeStr, "%d", &size) != 1) {
		    fprintf(stderr,
			    "Line %d: invalid Intensity Table size, %s.\n",
			      linenum, sizeStr);
		    return (0);
		}
		if (size < 0) {
		    fprintf(stderr,
			    "Line %d: count %d < 0 for Intensity Table.\n",
			      linenum, size);
		    return (0);
		}
		if (strcmp(tableStr, "GREEN") == 0) {
		    if (pCorrection->nTables != 3) {
			fprintf(stderr,"Line %d: incorrect number of tables\n", 
			linenum);
			return (0);
		    }
		    if (pCorrection->pGreenTbl->pBase != NULL) {
			fprintf(stderr,
			     "Line %d: multiple GREEN Intensity Profiles\n",
			           linenum);
			return (0);
		    }
		    pCorrection->pGreenTbl->nEntries = size;
		    pCorrection->pGreenTbl->pBase =
			 (IntensityRec *) calloc (size, sizeof(IntensityRec));
		    if (!pCorrection->pGreenTbl->pBase) {
			fprintf(stderr,
		     "Line %d: Unable to allocate space for GREEN Intensity Profile\n", linenum);
			return (0);
		    }
		    pIRec = pCorrection->pGreenTbl->pBase;
		} else if (strcmp(tableStr, "BLUE") == 0) {
		    if (pCorrection->nTables != 3) {
			fprintf(stderr,
				"Line %d: incorrect number of tables\n",
				linenum);
			return (0);
		    }
		    if (pCorrection->pBlueTbl->pBase != NULL) {
			fprintf(stderr,
			      "Line %d: multiple BLUE Intensity Profiles\n",
			           linenum);
			return (0);
		    }
		    pCorrection->pBlueTbl->nEntries = size;
		    pCorrection->pBlueTbl->pBase =
			 (IntensityRec *) calloc (size, sizeof(IntensityRec));
		    if (!pCorrection->pBlueTbl->pBase) {
			fprintf(stderr,
		      "Line %d: Unable to allocate space for BLUE Intensity Profile\n", linenum);
			return (0);
		    }
		    pIRec = pCorrection->pBlueTbl->pBase;
		} else {
		    if (!strcmp(tableStr, "RGB") && pCorrection->nTables != 1) {
			fprintf(stderr,"Line %d: multiple RGB Intensity Tables",
			      linenum);
			return (0);
		    }
		    if (pCorrection->pRedTbl->pBase != NULL) {
			fprintf(stderr,
		  "Line %d: multiple RED or GREEN or BLUE Intensity Tables\n",
				   linenum);
			return (0);
		    }
		    pCorrection->pRedTbl->nEntries = size;
		    pCorrection->pRedTbl->pBase = 
			 (IntensityRec *) calloc (size, sizeof(IntensityRec));
		    if (!pCorrection->pRedTbl->pBase) {
			fprintf(stderr,
			     "Line %d: Unable to allocate space for intensity table\n", linenum);
			return (0);
		    }
		    pIRec = pCorrection->pRedTbl->pBase;
		}
		state = 1;
		count = 0;
		break;
	      case ITBL_END :
		if ((state != 1) || (count != size)) {
		    fprintf(stderr,
		    "Line %d: incomplete Intensity Table -- Premature %s\n",
			  linenum, keyword);
		    return (0);
		}
		nTbl++;
		state = 0;
		break;
	      case DATA :
		do {
		    /********************************************************
		     * Note: tableType should only be 0 or 1 at this point. 
		     *       0 indicates value and intensity stored.
		     *       1 indicates only intensity stored. 
		     ********************************************************/
		    if (pCorrection->tableType) {
			if (sscanf(ptoken, "%lf", &pIRec->intensity) != 1) {
			    fprintf(stderr,
			   "Line %d: invalid Intensity Profile value %s\n", 
				  linenum, ptoken);
			    return (0);
			}
			/* With tableType 1 only store the intensity. */
			pIRec++;
		    } else {
			/* Note ansi C can handle 0x preceeding hex number */
			if (sscanf(ptoken, "%hi", &pIRec->value) != 1) {
			    fprintf(stderr,
			    "Line %d: invalid Intensity Profile value %s\n",
				  linenum, ptoken);
			    return (0);
			}
			if ((ptoken = strtok(NULL, DATA_DELIMS)) == NULL) {
			    fprintf(stderr,
				  "Line %d: missing Intensity Profile value\n",
				  linenum);
			    return (0);
			}
			if (sscanf(ptoken, "%lf", &pIRec->intensity) != 1) {
			    fprintf(stderr,
			"Line %d: invalid Intensity Profile intensity %s\n",
				  linenum, ptoken);
			    return (0);
			}
			/* With tableType 0 only store both value & intensity*/
			pIRec++;
		    }
		    if (++count > size) {
			fprintf(stderr,
				"Line %d: extra Intensity value %s\n",
				linenum, ptoken);
			return (0);
		    }
		    ptoken = strtok(NULL, DATA_DELIMS);
		} while(ptoken != NULL);
		break;
	      case IPROFILE_BEGIN :
		fprintf(stderr,"Line %d: extraneous keyword %s\n", 
			  linenum, keyword);
		return (0);
/* NOTREACHED */break;
	      case IPROFILE_END :
		if ((state != 0) || (nTbl != pCorrection->nTables)) {
		    fprintf(stderr,
	     "Line %d: incomplete Intensity Profile data -- Premature %s\n",
			   linenum, keyword);
		    return (0);
		}
		return (1);
	      case COMMENT :
		/* ignore line */
		break;
	      default :
		fprintf(stderr,"Line %d: unexpected keyword %s\n",
		      linenum, keyword);
		return (0);
/* NOTREACHED */break;
	    }
	} /* else its was just a blank line */
    }
    return (0);
}

static void
PutTableType0Card8(IntensityTbl *pTbl, unsigned char **pCard8)
{
    unsigned int count;
    IntensityRec *pIRec;

    pIRec = pTbl->pBase;
    count = pTbl->nEntries;
    **pCard8 = count - 1;
    *pCard8 += 1;
    for (; count; count--, pIRec++) {
	**pCard8 = pIRec->value >> 8;
	*pCard8 += 1;
	**pCard8 = pIRec->intensity * 255.0;
	*pCard8 += 1;
    }
}

static void
PutTableType1Card8(IntensityTbl *pTbl, unsigned char **pCard8)
{
    unsigned int count;
    IntensityRec *pIRec;

    pIRec = pTbl->pBase;
    count = pTbl->nEntries;
    **pCard8 = count - 1;
    *pCard8 += 1;
    for (; count; count--, pIRec++) {
	**pCard8 = pIRec->intensity * 255.0;
	*pCard8 += 1;
    }
}

static void
PutTableType0Card16(IntensityTbl *pTbl, unsigned short **pCard16)
{
    unsigned int count;
    IntensityRec *pIRec;

    pIRec = pTbl->pBase;
    count = pTbl->nEntries;
    **pCard16 = count - 1;
    *pCard16 += 1;
    for (; count; count--, pIRec++) {
	**pCard16 = pIRec->value;
	*pCard16 += 1;
	**pCard16 = pIRec->intensity * 65535.0;
	*pCard16 += 1;
    }
}

static void
PutTableType1Card16(IntensityTbl *pTbl, unsigned short **pCard16)
{
    unsigned int count;
    IntensityRec *pIRec;

    pIRec = pTbl->pBase;
    count = pTbl->nEntries;
    **pCard16 = count - 1;
    *pCard16 += 1;
    for (; count; count--, pIRec++) {
	**pCard16 = pIRec->intensity * 65535.0;
	*pCard16 += 1;
    }
}

static void
PutTableType0Card32(IntensityTbl *pTbl, unsigned long **pCard32)
{
    unsigned int count;
    IntensityRec *pIRec;

    pIRec = pTbl->pBase;
    count = pTbl->nEntries;
    **pCard32 = count - 1;
    *pCard32 += 1;
    for (; count; count--, pIRec++) {
	**pCard32 = pIRec->value;
	*pCard32 += 1;
	**pCard32 = pIRec->intensity * 4294967295.0;
	*pCard32 += 1;
    }
}

static void
PutTableType1Card32(IntensityTbl *pTbl, unsigned long **pCard32)
{
    unsigned int count;
    IntensityRec *pIRec;

    pIRec = pTbl->pBase;
    count = pTbl->nEntries;
    **pCard32 = count - 1;
    *pCard32 += 1;
    for (; count; count--, pIRec++) {
	**pCard32 = pIRec->intensity * 4294967295.0;
	*pCard32 += 1;
    }
}


static void
LoadMatrix(Display *pDpy, Window root, XDCCC_Matrix *pMatrix)
{
    int  count;
    unsigned long  *pCard32;
    unsigned long  Card32Array[18];
    Atom MatricesAtom;
    XcmsFloat *pValue;

    /*
     * Store the XDCCC_LINEAR_RGB_MATRICES
     */
    pCard32 = Card32Array;
    pValue = (XcmsFloat *)pMatrix->XYZtoRGBmatrix;
    for (count = 0; count < 9; count++) {
	*pCard32++ = (unsigned long) (*pValue++ * (XcmsFloat) XDCCC_NUMBER);
    }
    pValue = (XcmsFloat *)pMatrix->RGBtoXYZmatrix;
    for (count = 0; count < 9; count++) {
	*pCard32++ = (unsigned long) (*pValue++ * (XcmsFloat) XDCCC_NUMBER);
    }
    MatricesAtom = XInternAtom (pDpy, XDCCC_MATRIX_ATOM_NAME, False);
    XChangeProperty (pDpy, root, MatricesAtom, XA_INTEGER, 32, 
		     PropModeReplace, (unsigned char *)Card32Array, 18);
}


static int
LoadCorrections(Display *pDpy, Window root, XDCCC_Correction *pCorrection, 
		int targetFormat)
{
    unsigned char  *pCard8;
    unsigned char  *pCard8Array = (unsigned char *)NULL;
    unsigned short  *pCard16;
    unsigned short  *pCard16Array = (unsigned short *)NULL;
    unsigned long  *pCard32;
    unsigned long  *pCard32Array = (unsigned long *)NULL;
    Atom CorrectAtom;
    int	total;
    int i;

    /*
     * Store each XDCCC_CORRECTION into XDCCC_LINEAR_RGB_CORRECTION property
     */
    CorrectAtom = XInternAtom (pDpy, XDCCC_CORRECT_ATOM_NAME, False);

    for (i = 0; pCorrection; i++, pCorrection = pCorrection->next) {
	if ((pCorrection->tableType != 0) && (pCorrection->tableType != 1)) {
	    if (pCorrection->visual_info.visualid) {
		fprintf(stderr,"RGB Correction for visualid %ld: Invalid intensity table type %d.\n",
			pCorrection->visual_info.visualid,
			pCorrection->tableType);
	    } else {
		fprintf(stderr,"Global RGB Correction: Invalid intensity table type %d.\n",
			pCorrection->tableType);
	    }
	    return(0);
	}

	if (pCorrection->nTables != 1 && pCorrection->nTables != 3) {
	    if (pCorrection->visual_info.visualid) {
		fprintf(stderr,"RGB Correction for visualid %ld: %d invalid number of tables.\n",
			pCorrection->visual_info.visualid,
			pCorrection->nTables);
	    } else {
		fprintf(stderr,"Global RGB Correction: %d invalid number of tables.\n",
			pCorrection->nTables);
	    }
	    return(0);
	}

	if (pCorrection->nTables == 1) {
	    if (pCorrection->pRedTbl->nEntries < 2) {
		if (pCorrection->visual_info.visualid) {
		    fprintf(stderr,"RGB Correction for visualid %ld: Illegal number of entries in table\n",
			    pCorrection->visual_info.visualid);
		} else {
		    fprintf(stderr,"Global RGB Correction: Illegal number of entries in table\n");
		}
		return (0);
	    }
	    switch (targetFormat) {
	      case 8:
		total = 7 + (pCorrection->pRedTbl->nEntries *
			(pCorrection->tableType == 0 ? 2 : 1));
		if ((pCard8 = pCard8Array = (unsigned char *) calloc (total,
			sizeof (unsigned char))) == NULL) {
		    fprintf(stderr,"Unable allocate array of ints\n");
		    return (0);
		}
		*pCard8++ = (pCorrection->visual_info.visualid >> 24) & 0xFF;
		*pCard8++ = (pCorrection->visual_info.visualid >> 16) & 0xFF;
		*pCard8++ = (pCorrection->visual_info.visualid >> 8) & 0xFF;
		*pCard8++ = (pCorrection->visual_info.visualid) & 0xFF;
		*pCard8++ = pCorrection->tableType;	/* type */
		*pCard8++ = 1;		/* number of tables = 1 */
		if (pCorrection->tableType == 0) {
		    PutTableType0Card8(pCorrection->pRedTbl, &pCard8);
		} else {
		    PutTableType1Card8(pCorrection->pRedTbl, &pCard8);
		}
		XChangeProperty (pDpy, root, CorrectAtom, XA_INTEGER, 8, 
			i ? PropModeAppend : PropModeReplace,
			(unsigned char *)pCard8Array, total);
		free(pCard8Array);
		break;
	      case 16:
		total = 5 + (pCorrection->pRedTbl->nEntries * 
			(pCorrection->tableType == 0 ? 2 : 1));
		if ((pCard16 = pCard16Array = (unsigned short *) calloc (total,
			sizeof (unsigned short))) == NULL) {
		    fprintf(stderr,"Unable allocate array of ints\n");
		    return (0);
		}
		*pCard16++ = (pCorrection->visual_info.visualid >> 16) & 0xFFFF;
		*pCard16++ = (pCorrection->visual_info.visualid) & 0xFFFF;
		*pCard16++ = pCorrection->tableType;	/* type */
		*pCard16++ = 1;		/* number of tables = 1 */
		if (pCorrection->tableType == 0) {
		    PutTableType0Card16(pCorrection->pRedTbl, &pCard16);
		} else {
		    PutTableType1Card16(pCorrection->pRedTbl, &pCard16);
		}
		XChangeProperty (pDpy, root, CorrectAtom, XA_INTEGER, 16, 
			i ? PropModeAppend : PropModeReplace,
			(unsigned char *)pCard16Array, total);
		free(pCard16Array);
		break;
	      case 32:
		total = 4 + (pCorrection->pRedTbl->nEntries * 
			(pCorrection->tableType == 0 ? 2 : 1));
		if ((pCard32 = pCard32Array =
			(unsigned long *) calloc (total,
			sizeof (unsigned long))) == NULL) {
		    fprintf(stderr,"Unable allocate array of ints\n");
		    return (0);
		}
		*pCard32++ = pCorrection->visual_info.visualid;
		*pCard32++ = pCorrection->tableType;	/* type */
		*pCard32++ = 1;		/* number of tables = 1 */
		if (pCorrection->tableType == 0) {
		    PutTableType0Card32(pCorrection->pRedTbl, &pCard32);
		} else {
		    PutTableType1Card32(pCorrection->pRedTbl, &pCard32);
		}
		XChangeProperty (pDpy, root, CorrectAtom, XA_INTEGER, 32, 
			i ? PropModeAppend : PropModeReplace,
			(unsigned char *)pCard32Array, total);
		free(pCard32Array);
		break;
	      default:
		if (pCorrection->visual_info.visualid) {
		    fprintf(stderr,"RGB Correction for visualid %ld: Invalid property format\n",
			    pCorrection->visual_info.visualid);
		} else {
		    fprintf(stderr,"Global RGB Correction: Invalid property format\n");
		}
		return (0);
	    }
	  } else { /* pCorrection->nTables == 3 */
	    if ((pCorrection->pRedTbl->nEntries < 2) ||
		    (pCorrection->pGreenTbl->nEntries < 2) ||
		    (pCorrection->pBlueTbl->nEntries < 2)) {
		if (pCorrection->visual_info.visualid) {
		    fprintf(stderr,"RGB Correction for visualid %ld: Illegal number of entries in table\n",
			    pCorrection->visual_info.visualid);
		} else {
		    fprintf(stderr,"Global RGB Correction: Illegal number of entries in table\n");
		}
		return (0);
	    }
	    switch (targetFormat) {
	      case 8:
		total = 9 +	/* visualID, type, and 3 lengths */
		    (pCorrection->pRedTbl->nEntries * (pCorrection->tableType == 0 ? 2 : 1)) +
		    (pCorrection->pGreenTbl->nEntries * (pCorrection->tableType == 0 ? 2 : 1)) +
		    (pCorrection->pBlueTbl->nEntries * (pCorrection->tableType == 0 ? 2 : 1));
		if ((pCard8 = pCard8Array =
			(unsigned char *) calloc (total,
			sizeof (unsigned char))) == NULL) {
		    fprintf(stderr,"Unable allocate array of ints\n");
		    return (0);
		}
		*pCard8++ = (pCorrection->visual_info.visualid >> 24) & 0xFF;
		*pCard8++ = (pCorrection->visual_info.visualid >> 16) & 0xFF;
		*pCard8++ = (pCorrection->visual_info.visualid >> 8) & 0xFF;
		*pCard8++ = (pCorrection->visual_info.visualid) & 0xFF;
		*pCard8++ = pCorrection->tableType;	/* type */
		*pCard8++ = 3;		/* number of tables = 3 */
		if (pCorrection->tableType == 0) {
		    PutTableType0Card8(pCorrection->pRedTbl, &pCard8);
		    PutTableType0Card8(pCorrection->pGreenTbl, &pCard8);
		    PutTableType0Card8(pCorrection->pBlueTbl, &pCard8);
		} else {
		    PutTableType1Card8(pCorrection->pRedTbl, &pCard8);
		    PutTableType1Card8(pCorrection->pGreenTbl, &pCard8);
		    PutTableType1Card8(pCorrection->pBlueTbl, &pCard8);
		}
		XChangeProperty (pDpy, root, CorrectAtom, XA_INTEGER, 8, 
			i ? PropModeAppend : PropModeReplace,
			(unsigned char *)pCard8Array, total);
		free(pCard8Array);
		break;
	      case 16:
		total = 7 +	/* visualID, type, and 3 lengths */
		    (pCorrection->pRedTbl->nEntries * (pCorrection->tableType == 0 ? 2 : 1)) +
		    (pCorrection->pGreenTbl->nEntries * (pCorrection->tableType == 0 ? 2 : 1)) +
		    (pCorrection->pBlueTbl->nEntries * (pCorrection->tableType == 0 ? 2 : 1));
		if ((pCard16 = pCard16Array =
			(unsigned short *) calloc (total,
			sizeof (unsigned short))) == NULL) {
		    fprintf(stderr,"Unable allocate array of ints\n");
		    return (0);
		}
		*pCard16++ = (pCorrection->visual_info.visualid >> 16) & 0xFFFF;
		*pCard16++ = (pCorrection->visual_info.visualid) & 0xFFFF;
		*pCard16++ = pCorrection->tableType;	/* type = 0 */
		*pCard16++ = 3;		/* number of tables = 3 */
		if (pCorrection->tableType == 0) {
		    PutTableType0Card16(pCorrection->pRedTbl, &pCard16);
		    PutTableType0Card16(pCorrection->pGreenTbl, &pCard16);
		    PutTableType0Card16(pCorrection->pBlueTbl, &pCard16);
		} else {
		    PutTableType1Card16(pCorrection->pRedTbl, &pCard16);
		    PutTableType1Card16(pCorrection->pGreenTbl, &pCard16);
		    PutTableType1Card16(pCorrection->pBlueTbl, &pCard16);
		}
		XChangeProperty (pDpy, root, CorrectAtom, XA_INTEGER, 16, 
			i ? PropModeAppend : PropModeReplace,
			(unsigned char *)pCard16Array, total);
		free(pCard16Array);
		break;
	      case 32:
		total = 6 +	/* visualID, type, and 3 lengths */
		    (pCorrection->pRedTbl->nEntries * (pCorrection->tableType == 0 ? 2 : 1)) +
		    (pCorrection->pGreenTbl->nEntries * (pCorrection->tableType == 0 ? 2 : 1)) +
		    (pCorrection->pBlueTbl->nEntries * (pCorrection->tableType == 0 ? 2 : 1));
		if ((pCard32 = pCard32Array =
			(unsigned long *) calloc (total,
			sizeof (unsigned long))) == NULL) {
		    fprintf(stderr,"Unable allocate array of ints\n");
		    return (0);
		}
		*pCard32++ = pCorrection->visual_info.visualid;
		*pCard32++ = pCorrection->tableType;	/* type */
		*pCard32++ = 3;		/* number of tables = 3 */
		if (pCorrection->tableType == 0) {
		    PutTableType0Card32(pCorrection->pRedTbl, &pCard32);
		    PutTableType0Card32(pCorrection->pGreenTbl, &pCard32);
		    PutTableType0Card32(pCorrection->pBlueTbl, &pCard32);
		} else {
		    PutTableType1Card32(pCorrection->pRedTbl, &pCard32);
		    PutTableType1Card32(pCorrection->pGreenTbl, &pCard32);
		    PutTableType1Card32(pCorrection->pBlueTbl, &pCard32);
		}
		XChangeProperty (pDpy, root, CorrectAtom, XA_INTEGER, 32, 
			i ? PropModeAppend : PropModeReplace,
			(unsigned char *)pCard32Array, total);
		free(pCard32Array);
		break;
	      default:
		if (pCorrection->visual_info.visualid) {
		    fprintf(stderr,"RGB Correction for visualid %ld: Invalid property format\n",
			    pCorrection->visual_info.visualid);
		} else {
		    fprintf(stderr,"Global RGB Correction: Invalid property format\n");
		}
		return (0);
	    }
	}
    }

    return (1);
}

#ifdef GRAY

static int
LoadDataGray(Display *pDpy, window root, int tableType, 
	     LINEAR_RGB_SCCData *pScreenData, int targetFormat)
{
    unsigned char *ret_prop;
    int  count;
    int  nLevels;
    unsigned char  *pCard8;
    unsigned char  *pCard8Array = (unsigned char *)NULL;
    unsigned short  *pCard16;
    unsigned short  *pCard16Array = (unsigned short *)NULL;
    unsigned long  *pCard32;
    unsigned long  *pCard32Array = (unsigned long *)NULL;
    unsigned long  Card32Array[18];
    int  ret_format;
    unsigned long ret_len, ret_after;
    Atom MatricesAtom, CorrectAtom, ret_atom;
    XcmsFloat *pValue;
    int total;

    /* Now store the XDCCC_SCREENWHITEPT */
    pCard32 = Card32Array;
    pValue = (XcmsFloat *)pScreenData->XYZtoRGBmatrix;
    for (count = 0; count < 3; count++) {
	*pCard32++ = (unsigned long) (*pValue++ * (XcmsFloat) XDCCC_NUMBER);
    }
    MatricesAtom = XInternAtom (pDpy,XDCCC_SCREENWHITEPT_ATOM_NAME,False);
    XChangeProperty (pDpy, root, MatricesAtom, XA_INTEGER, 32, 
		     PropModeReplace, (unsigned char *)Card32Array, 3);

    /* Now store the XDCCC_GRAY_CORRECTION */
    CorrectAtom = XInternAtom (pDpy, XDCCC_GRAY_CORRECT_ATOM_NAME, False);

    if (tableType == CORR_TYPE_NONE) {
	XGetWindowProperty (pDpy, root, CorrectAtom, 
			    0, 5, False, XA_INTEGER, 
			    &ret_atom, &ret_format, &ret_len, &ret_after,
			    &ret_prop);
	if (ret_format != 0) {
	    XDeleteProperty (pDpy, root, CorrectAtom);
	    XFree ((char *)ret_prop);
	}
	return (1);
    }
    nLevels = pScreenData->pRedTbl->nEntries;
    if (nLevels < 2) {
	fprintf(stderr,"Illegal number of entries in table\n");
	return (0);
    }
    switch (targetFormat) {
      case 8:
	total = 6 /* visualID, type, length */
		+ (nLevels * (tableType == 0 ? 2 : 1));
	if ((pCard8 = pCard8Array = (unsigned char *)
		calloc (total, sizeof (unsigned char))) == NULL) {
	    fprintf(stderr,"Unable allocate array of Card8\n");
	    return (0);
	}
	*pCard8++ = 0;		/* VisualID = 0 */
	*pCard8++ = 0;		/* VisualID = 0 */
	*pCard8++ = 0;		/* VisualID = 0 */
	*pCard8++ = 0;		/* VisualID = 0 */
	*pCard8++ = tableType;	/* type */
	if (tableType == 0) {
	    PutTableType0Card8(pScreenData->pRedTbl, &pCard8);
	} else { /* tableType == 1 */
	    PutTableType1Card8(pScreenData->pRedTbl, &pCard8);
	}
	XChangeProperty (pDpy, root, CorrectAtom, XA_INTEGER, 8, 
			 PropModeReplace, (unsigned char *)pCard8Array,
			 total);
	free (pCard8Array);
	break;
      case 16:
	total = 4 /* visualID, type, length */
		+ (nLevels * (tableType == 0 ? 2 : 1));
	if ((pCard16 = pCard16Array = (unsigned short *)
		calloc (total, sizeof (unsigned short))) == NULL) {
	    fprintf(stderr,"Unable allocate array of Card16\n");
	    return (0);
	}
	*pCard16++ = 0;		/* VisualID = 0 */
	*pCard16++ = 0;		/* VisualID = 0 */
	*pCard16++ = tableType;	/* type */
	if (tableType == 0) {
	    PutTableType0Card16(pScreenData->pRedTbl, &pCard16);
	} else { /* tableType == 1 */
	    PutTableType1Card16(pScreenData->pRedTbl, &pCard16);
	}
	XChangeProperty (pDpy, root, CorrectAtom, XA_INTEGER, 16, 
			 PropModeReplace, (unsigned char *)pCard16Array,
			 total);
	free (pCard16Array);
	break;
      case 32:
	total = 3 /* visualID, type, length */
		+ (nLevels * (tableType == 0 ? 2 : 1));
	if ((pCard32 = pCard32Array = (unsigned long *)
		calloc (total, sizeof (unsigned long))) == NULL) {
	    fprintf(stderr,"Unable allocate array of Card32\n");
	    return (0);
	}
	*pCard32++ = 0;		/* VisualID = 0 */
	*pCard32++ = tableType;	/* type */
	if (tableType == 0) {
	    PutTableType0Card32(pScreenData->pRedTbl, &pCard32);
	} else { /* tableType == 1 */
	    PutTableType1Card32(pScreenData->pRedTbl, &pCard32);
	}
	XChangeProperty (pDpy, root, CorrectAtom, XA_INTEGER, 32, 
			 PropModeReplace, (unsigned char *)pCard32Array,
			 total);
	free (pCard32Array);
	break;
      default:
	fprintf(stderr,"Invalid property format\n");
	return (0);
    }
    return (1);
}
#endif /* GRAY */


static void
PrintVisualOptions(XDCCC_Correction *pCorrection)
{
    if (pCorrection->visual_info_mask & VisualIDMask) {
	fprintf(stderr, "\t%s:0x%lx\n",
		DefineToStr(VisualOptKeyTbl, KEY_VISUALID),
		(unsigned long)pCorrection->visual_info.visualid);
    }
    if (pCorrection->visual_info_mask & VisualDepthMask) {
	fprintf(stderr, "\t%s:%d\n",
		DefineToStr(VisualOptKeyTbl, KEY_DEPTH),
		pCorrection->visual_info.depth);
    }
    if (pCorrection->visual_info_mask & VisualClassMask) {
	fprintf(stderr, "\t%s:%s\n",
		DefineToStr(VisualOptKeyTbl, KEY_CLASS),
		DefineToStr(VisualClassTbl, pCorrection->visual_info.class));
    }
    if (pCorrection->visual_info_mask & VisualRedMaskMask) {
	fprintf(stderr, "\t%s:0x%lx\n",
		DefineToStr(VisualOptKeyTbl, KEY_RED_MASK),
		pCorrection->visual_info.red_mask);
    }
    if (pCorrection->visual_info_mask & VisualGreenMaskMask) {
	fprintf(stderr, "\t%s:0x%lx\n",
		DefineToStr(VisualOptKeyTbl, KEY_GREEN_MASK),
		pCorrection->visual_info.green_mask);
    }
    if (pCorrection->visual_info_mask & VisualBlueMaskMask) {
	fprintf(stderr, "\t%s:0x%lx\n",
		DefineToStr(VisualOptKeyTbl, KEY_BLUE_MASK),
		pCorrection->visual_info.blue_mask);
    }
    if (pCorrection->visual_info_mask & VisualColormapSizeMask) {
	fprintf(stderr, "\t%s:0x%x\n",
		DefineToStr(VisualOptKeyTbl, KEY_COLORMAP_SIZE),
		pCorrection->visual_info.colormap_size);
    }
    if (pCorrection->visual_info_mask & VisualBitsPerRGBMask) {
	fprintf(stderr, "\t%s:%d\n",
		DefineToStr(VisualOptKeyTbl, KEY_BITS_PER_RGB),
		pCorrection->visual_info.bits_per_rgb);
    }
}


static int
ParseVisualOptions(Display *pDpy, XDCCC_Correction *pCorrection, char *pbuf)
{
    char *key;
    char *value;
    XVisualInfo *vinfo;
    int n_matches;
    char delims[8];

    strcpy(delims, DATA_DELIMS);
    strcat(delims, ":");
    pCorrection->visual_info_mask = VisualNoMask;
    key = strtok(pbuf, delims);
    do {
	value = strtok((char*)NULL, delims);
	if ((key == (char*)NULL) || (value == (char*)NULL)) {
	    return (0);
	}
	switch (StrToDefine(VisualOptKeyTbl, key)) {
	  case  KEY_VISUALID:
	    if (sscanf(value, "%li", &pCorrection->visual_info.visualid) != 1) {
		fprintf(stderr,
			"Line %d: invalid VisualID specified, %s\n",
			linenum, value);
		return (0);
	    } 
	    pCorrection->visual_info_mask |= VisualIDMask;
	    break;
	  case  KEY_DEPTH:
	    if (sscanf(value, "%i", &pCorrection->visual_info.depth) != 1) {
		fprintf(stderr,
			"Line %d: invalid depth specified, %s\n",
			linenum, value);
		return (0);
	    } 
	    pCorrection->visual_info_mask |= VisualDepthMask;
	    break;
	  case  KEY_CLASS:
	    switch (pCorrection->visual_info.class =
		    StrToDefine(VisualClassTbl, value)) {
	      case  StaticColor:
		break;
	      case  PseudoColor:
		break;
	      case  TrueColor:
		break;
	      case  DirectColor:
		break;
	      case  StaticGray:
		/* invalid, fall through */
	      case  GrayScale:
		/* invalid, fall through */
	      default:
		fprintf(stderr,
			"Line %d: invalid Visual Class -- %s\n",
			linenum, value);
		return (0);
	    }
	    pCorrection->visual_info_mask |= VisualClassMask;
	    break;
	  case  KEY_RED_MASK:
	    if (sscanf(value, "%li", &pCorrection->visual_info.red_mask) != 1) {
		fprintf(stderr,
			"Line %d: invalid red_mask specified -- %s\n",
			linenum, value);
		return (0);
	    } 
	    pCorrection->visual_info_mask |= VisualRedMaskMask;
	    break;
	  case  KEY_GREEN_MASK:
	    if (sscanf(value, "%li", &pCorrection->visual_info.green_mask) != 1) {
		fprintf(stderr,
			"Line %d: invalid green_mask specified -- %s\n",
			linenum, value);
		return (0);
	    } 
	    pCorrection->visual_info_mask |= VisualGreenMaskMask;
	    break;
	  case  KEY_BLUE_MASK:
	    if (sscanf(value, "%li", &pCorrection->visual_info.blue_mask) != 1) {
		fprintf(stderr,
			"Line %d: invalid blue_mask specified -- %s\n",
			linenum, value);
		return (0);
	    } 
	    pCorrection->visual_info_mask |= VisualBlueMaskMask;
	    break;
	  case  KEY_COLORMAP_SIZE:
	    if (sscanf(value, "%i", &pCorrection->visual_info.colormap_size) != 1) {
		fprintf(stderr,
			"Line %d: invalid colormap_size specified -- %s\n",
			linenum, value);
		return (0);
	    } 
	    pCorrection->visual_info_mask |= VisualColormapSizeMask;
	    break;
	  case  KEY_BITS_PER_RGB:
	    if (sscanf(value, "%i", &pCorrection->visual_info.bits_per_rgb) != 1) {
		fprintf(stderr,
			"Line %d: invalid bits_per_rgb specified -- %s\n",
			linenum, value);
		return (0);
	    } 
	    pCorrection->visual_info_mask |= VisualBitsPerRGBMask;
	    break;
	  default:
	    fprintf(stderr,
		    "Line %d: invalid keyword %s\n", linenum, key);
	    return (0);
	}
	key = strtok((char*)NULL, delims);
    } while (key != (char *)NULL);

    vinfo = XGetVisualInfo(pDpy,
	    pCorrection->visual_info_mask,
	    &pCorrection->visual_info,
	    &n_matches);

    if (!n_matches) {
	fprintf(stderr, "Line %d: Cannot find visual matching ...\n", linenum);
	PrintVisualOptions(pCorrection);
	fprintf(stderr, "\n");
	return(0);
    }
    if (n_matches > 1) {
	fprintf(stderr, "Line %d: Found more than one visual matching ...\n", linenum);
	PrintVisualOptions(pCorrection);
	fprintf(stderr, "    Using VisualId 0x%lx\n", (unsigned long)vinfo->visualid);
    }
    memcpy((char*)&pCorrection->visual_info, (char*)vinfo,
	    sizeof(XVisualInfo));
    return (1);
} 


/************************************************************************
 *									*
 *			 PUBLIC ROUTINES				*
 *									*
 ************************************************************************/

/*
 *	NAME
 *		LoadSCCData - Read and store the screen data
 *
 *	SYNOPSIS
 */
int
LoadSCCData(Display *pDpy, int screenNumber, char *filename, int targetFormat)

/*
 *	DESCRIPTION
 *		Using the X Device Color Characterization Convention (XDCCC)
 *		read the screen data and store it on the root window of the
 *		screen.
 *
 *	RETURNS
 *		Returns 0 if failed; otherwise 1.
 *
 */
{    
    FILE *stream;
    char *pStr;
    char buf[BUFSIZ];
    char *keyword, *token1, *token2, *token3;
    int  state = 0;
    int VisualFlag = -2;
    Window root;
    XDCCC_Matrix matrix;
    XDCCC_Correction* CorrectionTail = (XDCCC_Correction*)NULL;
    XDCCC_Correction* CorrectionHead = (XDCCC_Correction*)NULL;
    XDCCC_Correction* pCurrent;

    if (screenNumber < 0) {
	fprintf(stderr,"Invalid Screen Number %d\n", screenNumber);
	return(0);
    }
    root = RootWindow(pDpy, screenNumber);

    if (!root) {
	/* if no root window is available then return an error */
	fprintf(stderr,"Could not open root window supplied.\n ");
	return (0);
    }
    /*
     * Open the file, determine its size, then read it into memory.
     */
    if (filename == NULL) {
	stream = stdin;
	filename = "stdin";
    } else if ((stream = fopen(filename, "r")) == NULL) {
	fprintf(stderr,"Could not open file %s.\n", filename);
	return (0);
    }

    /*
     * Advance to starting keyword 
     * Anything before this keyword is just treated as comments.
     */

    while((pStr = nextline(buf, BUFSIZ, stream)) != NULL) {
	keyword = strtok(buf, DATA_DELIMS);
	if (keyword != (char *)NULL &&
		(strcmp(keyword, SC_BEGIN_KEYWORD) == 0)) {
	    break;
	}  /* else ignore the line */
    }

    if (pStr == NULL) {
	fprintf(stderr,"File %s is missing %s\n", filename, SC_BEGIN_KEYWORD);
	closeS (stream, CorrectionHead);
	return (0);
    }

    token1 = strtok((char*)NULL, DATA_DELIMS);
    if ((strcmp(token1, TXT_FORMAT_VERSION) != 0) &&
	    (strcmp(token1, "0.3") != 0)) {
	fprintf(stderr,
    "Screen data format version mismatch in file %s-- expected %s, found %s\n",
		filename, TXT_FORMAT_VERSION, token1);
	closeS (stream, CorrectionHead);
	return (0);
    }

    while ((pStr = nextline(buf, BUFSIZ, stream)) != NULL) {
	token1 = token2 = token3 = (char*)NULL;
	keyword = strtok(buf, DATA_DELIMS);
	if (keyword != (char*)NULL) {
	    switch (SCKeyOf(keyword)) {
	      case COMMENT :
	      case NAME :
	      case PART_NUMBER :
	      case MODEL :
	      case SERIAL_NUMBER :
	      case REVISION :
		/* Do nothing */
		break;
	      case SCREEN_CLASS :
		token1 = strtok((char*)NULL, DATA_DELIMS);
		token2 = strtok((char*)NULL, DATA_DELIMS);
		if ((token1 == (char*)NULL)
			|| ((VisualFlag = SCScrnClassOf(token1)) == -1)) {
		    closeS (stream, CorrectionHead);
		    return (0);
		}
		/*include code to handle screen number input*/
		if (token2 != (char*)NULL) {
		    screenNumber = atoi(token2);

		    if (screenNumber < 0) {
			fprintf(stderr,"Invalid Screen Number %d\n", 
				screenNumber);
		    }
		    else {
			root = RootWindow(pDpy, screenNumber);
			if (!root) {
	/* if no root window is available then return an error */
			    fprintf(stderr,
				    "Could not open root window supplied.\n ");
			    return (0);
			}
		    }
		}
		break;
	      case COLORIMETRIC_BEGIN :
		if (VisualFlag == -2) {
		    closeS (stream, CorrectionHead);
		    return (0);
		}
		if (!ProcessColorimetric(stream, 
					 &matrix, VisualFlag)) {
		    closeS (stream, CorrectionHead);
		    return (0);
		}
		state |= 0x02;
		break;
	      case IPROFILE_BEGIN :
		if (VisualFlag == -2) {
		    closeS (stream, CorrectionHead);
		    return (0);
		}
		token1 = strtok((char*)NULL, DATA_DELIMS);
		token2 = strtok((char*)NULL, DATA_DELIMS);
		if ((token1 == (char*)NULL) || (token2 == (char*)NULL)) {
		    fprintf(stderr,
			"Line %d: Intensity profile missing TableType and/or nTables.",
			linenum);
		    closeS (stream, CorrectionHead);
		    return (0);
		}

		if ((pCurrent = (XDCCC_Correction *)
			calloc(1, sizeof(XDCCC_Correction))) ==NULL) {
		    fprintf(stderr,
			"Line %d: Could not allocate memory for intensity profile.",
			linenum);
		    closeS (stream, CorrectionHead);
		    return (0);
		}

		if (sscanf(token1, "%d", &pCurrent->tableType) != 1 ||
		    (pCurrent->tableType < 0 || pCurrent->tableType > 1)) {
		    fprintf(stderr,
			    "Line %d: invalid table type specified -- %s\n",
			    linenum, buf);
		    closeS (stream, CorrectionHead);
		    return (0);
		} 

		if ((VisualFlag == VIDEO_RGB) && (token2 == (char *)NULL)) {
		    fprintf(stderr,
			"Line %d: invalid number of tables specified -- %s\n",
			    linenum, buf);
		    closeS (stream, CorrectionHead);
		    return (0);
		}

		if (VisualFlag == VIDEO_RGB) {
		    if (sscanf(token2, "%d", &pCurrent->nTables) != 1 ||
			    (pCurrent->nTables != 0 && pCurrent->nTables != 1
			    && pCurrent->nTables != 3)) {
			fprintf(stderr,
				"Line %d: invalid number of tables (must be 0, 1, or 3)\n",
				linenum);
			closeS (stream, CorrectionHead);
			return (0);
		    }
		} else {
		    pCurrent->nTables = 0;
		}

		token3 = strtok((char*)NULL, "\n");
		if (token3 != (char*)NULL) {
		    if (!ParseVisualOptions(pDpy, pCurrent, token3)) {
			goto ByPassThisIProfile;
		    } 
		}

		switch (pCurrent->nTables) {
		  case 3 :
		    if (!(pCurrent->pRedTbl = (IntensityTbl *)
			calloc (1, sizeof (IntensityTbl)))) {
			fprintf(stderr,
			       "Line %d: Could not allocate Red Intensity Table\n",
			       linenum);
			closeS (stream, CorrectionHead);
			return (0);
		    }
		    if (!(pCurrent->pGreenTbl = (IntensityTbl *)
			calloc (1, sizeof (IntensityTbl)))) {
			fprintf(stderr,
			     "Line %d: Could not allocate Green Intensity Table\n",
			     linenum);
			closeS (stream, CorrectionHead);
			return (0);
		    }
		    if (!(pCurrent->pBlueTbl = (IntensityTbl *)
			calloc (1, sizeof (IntensityTbl)))) {
			fprintf(stderr,
				"Line %d: Could not allocate Blue Intensity Table",
				linenum);
			closeS (stream, CorrectionHead);
			return (0);
		    }
		    if (!ProcessIProfile(stream, pCurrent)) {
			goto ByPassThisIProfile;
		    }
		    break;
		  case 1 :
		    if (!(pCurrent->pRedTbl = (IntensityTbl *)
			  calloc (1, sizeof (IntensityTbl)))) {
			fprintf(stderr,
				"Line %d: Could not allocate Red Intensity Table",
				linenum);
			closeS (stream, CorrectionHead);
			return (0);
		    }
		    pCurrent->pGreenTbl = pCurrent->pRedTbl;
		    pCurrent->pBlueTbl = pCurrent->pRedTbl;
		    if (!ProcessIProfile(stream, pCurrent)) {
			goto ByPassThisIProfile;
		    }
		    break;
		  default :
		    /* do nothing */
		    break;
		}

		if (CorrectionHead == NULL) {
		    CorrectionHead = CorrectionTail = pCurrent;
		} else {
		    CorrectionTail->next = pCurrent;
		    CorrectionTail = pCurrent;
		}
		state |= 0x04;
		break;
ByPassThisIProfile:
		/* read till INTENSITY_PROFILE_END */
		while ((pStr = nextline(buf, BUFSIZ, stream)) != NULL) {
		    keyword = strtok(buf, DATA_DELIMS);
		    if (keyword != (char*)NULL) {
			switch (SCKeyOf(keyword)) {
			  case ITBL_BEGIN:
			  case ITBL_END:
			  case COMMENT:
			  case DATA:
			    break;
			  case IPROFILE_END:
			    goto IProfileProcessed;
			  default:
			    closeS (stream, CorrectionHead);
			    return (0);
			}
		    }
		}
		free(pCurrent);
IProfileProcessed:
		state |= 0x04;
		break;
	      case SC_END :
		if (!(state & 0x02)) {
		    fprintf(stderr,
			    "File %s is missing Colorimetric data.\n", 
			    filename);
		    closeS (stream, CorrectionHead);
		    return (0);
		}
		if (!(state & 0x04)) {
		    fprintf(stderr,
			    "File %s is missing Intensity Profile Data.\n",
			    filename);
		}
		if (VisualFlag == VIDEO_RGB) {
		    LoadMatrix(pDpy, root, &matrix);
		    if (!LoadCorrections(pDpy, root, CorrectionHead,
			    targetFormat)) {
			closeS (stream, CorrectionHead);
			return (0);
		    }
#ifdef GRAY
		} else if (VisualFlag == VIDEO_GRAY) {
		    if (!LoadDataGray(pDpy, root,
				      pCurrent->tableType, pScreenData, targetFormat)) {
			closeS (stream, CorrectionHead);
			return (0);
		    }
#endif /* GRAY */
		} else {
		    fprintf(stderr,"File %s Visual missing.", filename);
		}
		closeS (stream, CorrectionHead);
		return (1);
/* NOTREACHED */    break;
	      default :
		fprintf(stderr,"Line %d: extraneous keyword %s\n", 
			linenum, keyword);
		closeS (stream, CorrectionHead);
		return (0);

	    }
	}   /* else it was just a blank line */
    }
    closeS (stream, CorrectionHead);
    return (1);
}
