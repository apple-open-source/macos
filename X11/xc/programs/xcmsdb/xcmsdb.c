/* $Xorg: xcmsdb.c,v 1.3 2000/08/17 19:54:13 cpqbld Exp $ */

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
 *		xcmsdb.c
 *
 *	DESCRIPTION
 *		Program to load, query or remove the Screen Color 
 *		Characterization Data from the root window of the screen.
 *
 */
/* $XFree86: xc/programs/xcmsdb/xcmsdb.c,v 1.6 2001/04/01 14:00:19 tsi Exp $ */

/*
 *      INCLUDES
 */

#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xos.h>
#include <ctype.h>

#include "SCCDFile.h"

static void QuerySCCDataRGB(Display *dpy, Window root);
static void RemoveSCCData(Display *dpy, Window root, int colorFlag);
static unsigned long _XcmsGetElement(int format, char **pValue, 
				     unsigned long *pCount);
static int _XcmsGetProperty(Display *pDpy, Window w, Atom property, 
			    int *pFormat, unsigned long *pNItems, 
			    unsigned long *pNBytes, char **pValue);


char *ProgramName;

static void
Syntax (void)
{
    fprintf (stderr, 
	     "usage:  %s [-options ...] [filename]\n\n",
	     ProgramName);
    fprintf (stderr, 
	     "where options include:\n");
    fprintf (stderr, 
	     "    -display host:dpy[.scrn]     display to use\n");
    fprintf (stderr, 
	     "    -format [ 32 | 16 | 8 ]      property format\n");
    fprintf (stderr, 
	     "    -query                       query Screen Color Characterization Data\n");
    fprintf (stderr, 
	     "    -remove                      remove Screen Color Characterization Data\n");
#ifdef GRAY
    fprintf (stderr, 
	     "    -color                       use color as default\n");
    fprintf (stderr, 
	     "    -gray                        use gray-scale as default\n");
#endif /* GRAY */
    fprintf (stderr, 
	     "\n");
    exit (1);
}

static Bool 
optionmatch(char *opt, char *arg, int minlen)
{
    int arglen;

    if (strcmp(opt, arg) == 0) {
	return(True);
    }

    if ((arglen = strlen(arg)) >= (int)strlen(opt) || arglen < minlen) {
	return(False);
    }

    if (strncmp (opt, arg, arglen) == 0) {
	return(True);
    }
    
    return(False);
}

int
main(int argc, char *argv[])
{
    Display *dpy;
    int i;
    char *displayname = NULL;
    char *filename = NULL;
    int query = 0;
    int remove = 0;
    int load = 0;
    int color = -1;
    int targetFormat = 32;

    ProgramName = argv[0];

    for (i = 1; i < argc; i++) {
	char *arg = argv[i];

	if (arg[0] == '-') {
	    if (arg[1] == '\0') {
		filename = NULL;
		continue;
	    } else if (optionmatch ("-help", arg, 1)) {
		Syntax ();
		/* doesn't return */
	    } else if (optionmatch ("-display", arg, 1)) {
		if (++i >= argc) Syntax ();
		displayname = argv[i];
		continue;
	    } else if (optionmatch ("-format", arg, 1)) {
		if (++i >= argc) Syntax ();
		targetFormat = atoi(argv[i]);
		if (targetFormat != 32 && targetFormat != 16 &&
			targetFormat != 8) Syntax();
		continue;
	    } else if (optionmatch ("-query", arg, 1)) {
		query = 1;
		continue;
	    } else if (optionmatch ("-remove", arg, 1)) {
		remove = 1;
		continue;
#ifdef GRAY
	    } else if (optionmatch ("-color", arg, 1)) {
		color = 1;
		continue;
	    } else if (optionmatch ("-gray", arg, 1)) {
		color = 0;
		continue;
#endif /* GRAY */
	    }
	    Syntax ();
	} else {
	    load = 1;
	    filename = arg;
	}
    }

    /* Open display  */ 
    if (!(dpy = XOpenDisplay (displayname))) {
      fprintf (stderr, "%s:  Can't open display '%s'\n",
	       ProgramName, XDisplayName(displayname));
      exit (1);
    }

    if (load && query) {
	load = 0;
    }
    if (load && remove) {
	load = 0;
    }

    if (!query && !remove) {
	LoadSCCData(dpy, DefaultScreen(dpy), filename, targetFormat);
    }

    if (query) {
	if (color != 0)
	    QuerySCCDataRGB(dpy, RootWindow(dpy, DefaultScreen(dpy)));
#ifdef GRAY
	if (color != 1)
	    QuerySCCDataGray(dpy, RootWindow(dpy, DefaultScreen(dpy)));
#endif /* GRAY */
    }

    if (remove) {
	RemoveSCCData(dpy, RootWindow(dpy, DefaultScreen(dpy)), color);
    }

    XCloseDisplay(dpy);
    exit (0);
    /*NOTREACHED*/
}


static Atom
ParseAtom(Display *dpy, char *name, int only_flag)
{
    return(XInternAtom(dpy, name, only_flag));
}


/*
 *	NAME
 *		PrintTableType0
 *
 *	SYNOPSIS
 */
static void
PrintTableType0(int format, char **pChar, unsigned long *pCount)
/*
 *	DESCRIPTION
 *
 *	RETURNS
 *		XcmsFailure if failed.
 *		XcmsSuccess if succeeded.
 *
 */
{
    unsigned int nElements;
    unsigned short hValue;
    XcmsFloat fValue;

    nElements = _XcmsGetElement(format, pChar, pCount) + 1;
    printf ("\t    length:%d\n", nElements);

    switch (format) {
      case 8:
	while (nElements--) {
	    /* 0xFFFF/0xFF = 0x101 */
	    hValue = _XcmsGetElement (format, pChar, pCount) * 0x101;
	    fValue = _XcmsGetElement (format, pChar, pCount)
		    / (XcmsFloat)255.0;
	    printf ("\t\t0x%x\t%8.5f\n", hValue, fValue);
	}
	break;
      case 16:
	while (nElements--) {
	    hValue = _XcmsGetElement (format, pChar, pCount);
	    fValue = _XcmsGetElement (format, pChar, pCount)
		    / (XcmsFloat)65535.0;
	    printf ("\t\t0x%x\t%8.5f\n", hValue, fValue);
	}
	break;
      case 32:
	while (nElements--) {
	    hValue = _XcmsGetElement (format, pChar, pCount);
	    fValue = _XcmsGetElement (format, pChar, pCount)
		    / (XcmsFloat)4294967295.0;
	    printf ("\t\t0x%x\t%8.5f\n", hValue, fValue);
	}
	break;
      default:
	return;
    }
}


/*
 *	NAME
 *		PrintTableType1
 *
 *	SYNOPSIS
 */
static void
PrintTableType1(int format, char **pChar, unsigned long *pCount)
/*
 *	DESCRIPTION
 *
 *	RETURNS
 *		XcmsFailure if failed.
 *		XcmsSuccess if succeeded.
 *
 */
{
    int count;
    unsigned int max_index;
    unsigned short hValue;
    XcmsFloat fValue;

    max_index = _XcmsGetElement(format, pChar, pCount);
    printf ("\t    length:%d\n", max_index + 1);

    switch (format) {
      case 8:
	for (count = 0; count < max_index+1; count++) {
	    hValue = (count * 65535) / max_index;
	    fValue = _XcmsGetElement (format, pChar, pCount)
		    / (XcmsFloat)255.0;
	    printf ("\t\t0x%x\t%8.5f\n", hValue, fValue);
	}
	break;
      case 16:
	for (count = 0; count < max_index+1; count++) {
	    hValue = (count * 65535) / max_index;
	    fValue = _XcmsGetElement (format, pChar, pCount)
		    / (XcmsFloat)65535.0;
	    printf ("\t\t0x%x\t%8.5f\n", hValue, fValue);
	}
	break;
      case 32:
	for (count = 0; count < max_index+1; count++) {
	    hValue = (count * 65535) / max_index;
	    fValue = _XcmsGetElement (format, pChar, pCount)
		    / (XcmsFloat)4294967295.0;
	    printf ("\t\t0x%x\t%8.5f\n", hValue, fValue);
	}
	break;
      default:
	return;
    }
}


/*
 *      NAME
 *		QuerySCCData - Query for the SCC data on the root window
 *
 *      SYNOPSIS
 */
static void
QuerySCCDataRGB(Display *dpy, Window root)
/*
 *      DESCRIPTION
 *
 *      RETURNS
 *		None
 */
{
    char *property_return, *pChar;
    int  i, j;
    int  count, format, cType, nTables;
    unsigned long nitems, nbytes_return;
    Atom MatricesAtom, CorrectAtom;
    VisualID visualID;
    XVisualInfo vinfo_template, *vinfo_ret;
    int nvis;
    static char *visual_strings[] = {
	"StaticGray",
	"GrayScale",
	"StaticColor",
	"PseudoColor",
	"TrueColor",
	"DirectColor"
	};

    /*
     * Get Matrices
     */
    MatricesAtom = ParseAtom (dpy, XDCCC_MATRIX_ATOM_NAME, True);
    if (MatricesAtom != None) {
	if (_XcmsGetProperty (dpy, root, MatricesAtom, &format, &nitems,
			  &nbytes_return, &property_return) == XcmsFailure) {
	    format = 0;
	} else if (nitems != 18) {
	    printf ("Property %s had invalid length of %ld\n",
		    XDCCC_MATRIX_ATOM_NAME, nitems);
	    if (property_return) {
		XFree (property_return);
	    }
	    return;
	}
    } 
    if (MatricesAtom == None || !format) {
	printf ("Could not find property %s\n", XDCCC_MATRIX_ATOM_NAME);
    } else if (format != 32) {
	printf ("Data in property %s not in 32 bit format\n",
		XDCCC_MATRIX_ATOM_NAME);
    } else {
	pChar = property_return;
	printf ("Screen: %d\n", DefaultScreen(dpy));
	printf ("Querying property %s\n", XDCCC_MATRIX_ATOM_NAME);
	printf ("\tXYZtoRGB matrix :\n");
	for (i = 0; i < 3; i++) {
	    printf ("\t");
	    for (j = 0; j < 3; j++) {
		printf ("\t%8.5f", 
			(long)_XcmsGetElement(format, &pChar, &nitems)
			/ (XcmsFloat) XDCCC_NUMBER);
	    }
	    printf ("\n");
	}
	printf ("\tRGBtoXYZ matrix :\n");
	for (i = 0; i < 3; i++) {
	    printf ("\t");
	    for (j = 0; j < 3; j++) {
		printf ("\t%8.5f", 
			(long) _XcmsGetElement(format, &pChar, &nitems)
			/ (XcmsFloat) XDCCC_NUMBER);
	    }
	    printf ("\n");
	}
	XFree (property_return);
    }


    /*
     * Get Intensity Tables
     */
    CorrectAtom = XInternAtom (dpy, XDCCC_CORRECT_ATOM_NAME, True);
    if (CorrectAtom != None) {
	if (_XcmsGetProperty (dpy, root, CorrectAtom, &format, &nitems,
			  &nbytes_return, &property_return) == XcmsFailure) {
	    format = 0;
	} else if (nitems <= 0) {
            printf ("Property %s had invalid length of %ld\n",
		    XDCCC_CORRECT_ATOM_NAME, nitems);
	    if (property_return) {
		XFree (property_return);
	    }
	    return;
	}
    }
    if (CorrectAtom == None || !format) {
	printf ("Could not find property %s\n", XDCCC_CORRECT_ATOM_NAME);
    } else {
	printf ("\nQuerying property %s\n", XDCCC_CORRECT_ATOM_NAME);
	pChar = property_return;

	while (nitems) {
	    switch (format) {
	      case 8:
		/*
		 * Must have at least:
		 *		VisualID0
		 *		VisualID1
		 *		VisualID2
		 *		VisualID3
		 *		type
		 *		count
		 *		length
		 *		intensity1
		 *		intensity2
		 */
		if (nitems < 9) {
		    goto IntensityTblError;
		}
		count = 3;
		break;
	      case 16:
		/*
		 * Must have at least:
		 *		VisualID0
		 *		VisualID3
		 *		type
		 *		count
		 *		length
		 *		intensity1
		 *		intensity2
		 */
		if (nitems < 7) {
		    goto IntensityTblError;
		}
		count = 1;
		break;
	      case 32:
		/*
		 * Must have at least:
		 *		VisualID0
		 *		type
		 *		count
		 *		length
		 *		intensity1
		 *		intensity2
		 */
		if (nitems < 6) {
		    goto IntensityTblError;
		}
		count = 0;
		break;
	      default:
		goto IntensityTblError;
	    }

	    /*
	     * Get VisualID
	     */
	    visualID = _XcmsGetElement(format, &pChar, &nitems);
	    /* add the depth, class, and bits info in output */
	    vinfo_template.visualid = visualID;
	    vinfo_ret = XGetVisualInfo(dpy, VisualIDMask, &vinfo_template, 
				       &nvis);
	    while (count--) {
		visualID = visualID << format;
		visualID |= _XcmsGetElement(format, &pChar, &nitems);
	    }

	    if (vinfo_ret != NULL) {
		printf 
		 ("\n\tVisualID: 0x%lx class: %s depth: %d bits_per_rgb: %d\n",
		  visualID, visual_strings[vinfo_ret->class],
		  vinfo_ret->depth, vinfo_ret->bits_per_rgb);
	    }
	    else
	    printf ("\n\tVisualID: 0x%lx\n", visualID);
	    XFree(vinfo_ret);
	    cType = _XcmsGetElement(format, &pChar, &nitems);
	    printf ("\ttype: %d\n", cType);
	    nTables = _XcmsGetElement(format, &pChar, &nitems);
	    printf ("\tcount: %d\n", nTables);

	    switch (cType) {
	      case 0:
		/* Red Table should always exist */
		printf ("\tRed Conversion Table:\n");
		PrintTableType0(format, &pChar, &nitems);
		if (nTables > 1) {
		    printf ("\tGreen Conversion Table:\n");
		    PrintTableType0(format, &pChar, &nitems);
		    printf ("\tBlue Conversion Table:\n");
		    PrintTableType0(format, &pChar, &nitems);
		}
		break;
	      case 1:
		/* Red Table should always exist */
		printf ("\tRed Conversion Table:\n");
		PrintTableType1(format, &pChar, &nitems);
		if (nTables > 1) {
		    printf ("\tGreen Conversion Table:\n");
		    PrintTableType1(format, &pChar, &nitems);
		    printf ("\tBlue Conversion Table:\n");
		    PrintTableType1(format, &pChar, &nitems);
		}
	        break;
	      default:
		goto IntensityTblError;
	    }
	}    
	XFree (property_return);
    }    
    return;

IntensityTblError:
    XFree (property_return);
    printf("Fatal error in %s property\n", XDCCC_CORRECT_ATOM_NAME);
}


#ifdef GRAY

/*
 *      NAME
 *		QuerySCCDataGray - Query for the SCC data on the root window
 *
 *      SYNOPSIS
 */
int
QuerySCCDataGray(Display *dpy, Window root)
/*
 *      DESCRIPTION
 *
 *      RETURNS
 *		None
 */
{
    char *property_return, *pChar;
    int  j;
    int  count, format, cType;
    unsigned long  nitems, nbytes_return;
    Atom MatricesAtom, CorrectAtom;
    VisualID visualID;

    MatricesAtom = ParseAtom (dpy, XDCCC_SCREENWHITEPT_ATOM_NAME, True);
    if (MatricesAtom != None) {
	if (_XcmsGetProperty (dpy, root, MatricesAtom, &format, &nitems,
			  &nbytes_return, &property_return)  == XcmsFailure) {
	    format = 0;
	} else if (nitems != 3) {
	    printf ("Property %s had invalid length of %d\n",
		    XDCCC_SCREENWHITEPT_ATOM_NAME, nitems);
	    if (property_return) {
		XFree (property_return);
	    }
	    return;
	}
    } 
    if (MatricesAtom == None || !format) {
	printf ("Could not find property %s\n", XDCCC_SCREENWHITEPT_ATOM_NAME);
    } else {
	pChar = property_return;
	printf ("\nQuerying property %s\n", XDCCC_SCREENWHITEPT_ATOM_NAME);
	printf ("\tWhite Point XYZ :\n");
	printf ("\t");
	for (j = 0; j < 3; j++) {
	    printf ("\t%8.5lf", 
			(long) _XcmsGetElement(format, &pChar, &nitems) / 
			(XcmsFloat) XDCCC_NUMBER);
	}
	printf ("\n");
	XFree (property_return);
    }

    CorrectAtom = XInternAtom (dpy, XDCCC_GRAY_CORRECT_ATOM_NAME, True);
    if (CorrectAtom != None) {
	if (_XcmsGetProperty (dpy, root, CorrectAtom, &format, &nitems,
			  &nbytes_return, &property_return) == XcmsFailure) {
	    format = 0;
	} else if (nitems <= 0) {
            printf ("Property %s had invalid length of %d\n",
		    XDCCC_GRAY_CORRECT_ATOM_NAME, nitems);
	    if (property_return) {
		XFree (property_return);
	    }
	    return;
	}
    }
    if (CorrectAtom == None || !format) {
	printf ("Could not find property %s\n", XDCCC_GRAY_CORRECT_ATOM_NAME);
    } else {
	printf ("\nQuerying property %s\n\n", XDCCC_GRAY_CORRECT_ATOM_NAME);
	pChar = property_return;

	while (nitems) {
	    switch (format) {
	      case 8:
		/*
		 * Must have at least:
		 *		VisualID0
		 *		VisualID1
		 *		VisualID2
		 *		VisualID3
		 *		type
		 *		count
		 *		length
		 *		intensity1
		 *		intensity2
		 */
		if (nitems < 9) {
		    goto IntensityTblError;
		}
		count = 3;
		break;
	      case 16:
		/*
		 * Must have at least:
		 *		VisualID0
		 *		VisualID3
		 *		type
		 *		count
		 *		length
		 *		intensity1
		 *		intensity2
		 */
		if (nitems < 7) {
		    goto IntensityTblError;
		}
		count = 1;
		break;
	      case 32:
		/*
		 * Must have at least:
		 *		VisualID0
		 *		type
		 *		count
		 *		length
		 *		intensity1
		 *		intensity2
		 */
		if (nitems < 6) {
		    goto IntensityTblError;
		}
		count = 0;
		break;
	      default:
		goto IntensityTblError;
		break;
	    }

	    /*
	     * Get VisualID
	     */
	    visualID = _XcmsGetElement(format, &pChar, &nitems);
	    while (count--) {
		visualID = visualID << format;
		visualID |= _XcmsGetElement(format, &pChar, &nitems);
	    }

	    printf ("\n\tVisualID: 0x%lx\n", visualID);
	    cType = _XcmsGetElement(format, &pChar, &nitems);
	    printf ("\ttype: %d\n", cType);
	    printf ("\tGray Conversion Table:\n");
	    switch (cType) {
	      case 0:
		PrintTableType0(format, &pChar, &nitems);
		break;
	      case 1:
		PrintTableType1(format, &pChar, &nitems);
		break;
	      default:
		goto IntensityTblError;
	    }
	}    
	XFree (property_return);
    }    
    return;
IntensityTblError:
    XFree (property_return);
    printf("Fatal error in %s property\n", XDCCC_CORRECT_ATOM_NAME);
}
#endif /* GRAY */


/*
 *      NAME
 *		RemoveSCCData - Remove for the SCC data on the root window
 *
 *      SYNOPSIS
 */
static void
RemoveSCCData(Display *dpy, Window root, int colorFlag)
/*
 *      DESCRIPTION
 *
 *      RETURNS
 *		None
 */
{
    unsigned char *ret_prop;
    unsigned long ret_len, ret_after;
    int  ret_format;
    Atom MatricesAtom, CorrectAtom, ret_atom;

    if (colorFlag != 0) {
	MatricesAtom = ParseAtom (dpy, XDCCC_MATRIX_ATOM_NAME, True);
	if (MatricesAtom != None) {
	    XGetWindowProperty (dpy, root, MatricesAtom, 0, 8192, False,
			XA_INTEGER, &ret_atom, &ret_format, &ret_len, 
			&ret_after, &ret_prop);
	} 
	if (MatricesAtom == None || !ret_format) {
	    printf ("Could not find property %s\n", XDCCC_MATRIX_ATOM_NAME);
	} else {
	    printf ("Deleting property %s\n", XDCCC_MATRIX_ATOM_NAME);
	    XDeleteProperty (dpy, root, MatricesAtom);
	    XFree ((char *)ret_prop);
	}

	CorrectAtom = XInternAtom (dpy, XDCCC_CORRECT_ATOM_NAME, True);
	if (CorrectAtom != None) {
	    XGetWindowProperty (dpy, root, CorrectAtom, 0, 8192, False, 
			XA_INTEGER, &ret_atom, &ret_format, &ret_len, 
			&ret_after, &ret_prop);
	}
	if (CorrectAtom == None || !ret_format) {
	    printf ("Could not find property %s\n", XDCCC_CORRECT_ATOM_NAME);
	} else {
	    printf ("Deleting property %s\n", XDCCC_CORRECT_ATOM_NAME);
	    XDeleteProperty (dpy, root, CorrectAtom);
	    XFree ((char *)ret_prop);
	}
    }
#ifdef GRAY
    if (colorFlag != 1) {
	MatricesAtom = ParseAtom (dpy, XDCCC_SCREENWHITEPT_ATOM_NAME, True);
	if (MatricesAtom != None) {
	    XGetWindowProperty (dpy, root, MatricesAtom, 0, 8192, False,
			XA_INTEGER, &ret_atom, &ret_format, &ret_len, 
			&ret_after, &ret_prop);
	} 
	if (MatricesAtom == None || !ret_format) {
	    printf ("Could not find property %s\n", XDCCC_SCREENWHITEPT_ATOM_NAME);
	} else {
	    printf ("Deleting property %s\n", XDCCC_SCREENWHITEPT_ATOM_NAME);
	    XDeleteProperty (dpy, root, MatricesAtom);
	    XFree ((char *)ret_prop);
	}

	CorrectAtom = XInternAtom (dpy, XDCCC_GRAY_CORRECT_ATOM_NAME, True);
	if (CorrectAtom != None) {
	    XGetWindowProperty (dpy, root, CorrectAtom, 0, 8192, False, 
			XA_INTEGER, &ret_atom, &ret_format, &ret_len, 
			&ret_after, &ret_prop);
	}
	if (CorrectAtom == None || !ret_format) {
	    printf ("Could not find property %s\n", XDCCC_GRAY_CORRECT_ATOM_NAME);
	} else {
	    printf ("Deleting property %s\n", XDCCC_GRAY_CORRECT_ATOM_NAME);
	    XDeleteProperty (dpy, root, CorrectAtom);
	    XFree ((char *)ret_prop);
	}
    }
#endif /* GRAY */
}

static unsigned long
_XcmsGetElement(int format, char **pValue, unsigned long *pCount) 
/*
 *	DESCRIPTION
 *	    Get the next element from the property and return it.
 *	    Also increment the pointer the amount needed.
 *
 *	Returns
 *	    unsigned long
 */
{
    unsigned long value;

    switch (format) {
      case 32:
	value = *((unsigned long *)(*pValue)) & 0xFFFFFFFF;
	*pValue += sizeof(unsigned long);
	*pCount -= 1;
	break;
      case 16:
	value = *((unsigned short *)(*pValue));
	*pValue += sizeof(unsigned short);
	*pCount -= 1;
	break;
      case 8:
	value = *((unsigned char *) (*pValue));
	*pValue += 1;
	*pCount -= 1;
	break;
      default:
	value = 0;
	break;
    }
    return(value);
}


/*
 *	NAME
 *		_XcmsGetProperty -- Determine the existance of a property
 *
 *	SYNOPSIS
 */
static int
_XcmsGetProperty(Display *pDpy, Window w, Atom property, int *pFormat, 
		 unsigned long *pNItems, unsigned long *pNBytes, 
		 char **pValue) 
/*
 *	DESCRIPTION
 *
 *	Returns
 *	    0 if property does not exist.
 *	    1 if property exists.
 */
{
    char *prop_ret;
    int format_ret;
    long len = 6516;
    unsigned long nitems_ret, after_ret;
    Atom atom_ret;
    
    while (XGetWindowProperty (pDpy, w, property, 0, len, False, 
			       XA_INTEGER, &atom_ret, &format_ret, 
			       &nitems_ret, &after_ret, 
			       (unsigned char **)&prop_ret)) {
	if (after_ret > 0) {
	    len += nitems_ret * (format_ret >> 3);
	    XFree (prop_ret);
	} else {
	    break;
	}
    }
    if (format_ret == 0 || nitems_ret == 0) { 
	/* the property does not exist or is of an unexpected type */
	return(XcmsFailure);
    }

    *pFormat = format_ret;
    *pNItems = nitems_ret;
    *pNBytes = nitems_ret * (format_ret >> 3);
    *pValue = prop_ret;
    return(XcmsSuccess);
}
