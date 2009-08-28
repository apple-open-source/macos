/* -*- mode: C; coding: macintosh; -*- */
#ifdef __APPLE_CC__ //  das 111200 compiling with gcc on OS X
#include <Carbon/Carbon.h>
#else
#include <ConditionalMacros.h>
#include <MacTypes.h>
#include <CodeFragments.h>
#endif

#include "tclMacOSError.h"

#ifndef _TCL
#include <tcl.h>
#endif
#include <string.h>
#include <stdio.h>
//  das 130700
//  FindErrorLib only on PPC non-carbon
#if __POWERPC__ && !TARGET_API_MAC_CARBON
#define USE_FIND_ERROR_LIB 1
#include <FindError.h>
#else
#define USE_FIND_ERROR_LIB 0
#endif

/*
 *----------------------------------------------------------------------
 *
 * Tcl_MacOSError --
 *
 *	This procedure is typically called after MacOS ToolBox calls return
 *	errors.  It stores machine-readable information about the error in
 *	$errorCode and returns an information string for the caller's use.
 *	It's a bit like Tcl_PosixError().
 *
 *  To get the most bang for your buck, install FindErrorLib.
 *    
 * Results:
 *	The return value is a human-readable string describing the error.
 *
 * Side effects:
 *	The global variable $errorCode is reset to:
 *	
 *		 {theErrorName theErrorNumber theErrorDescription}
 *		 
 *  or at least as much as is available.
 *
 *----------------------------------------------------------------------
 */
char *
Tcl_MacOSError(Tcl_Interp  *interp,			/* to assign global error code */
			   OSStatus    err)				/* error code to interpret */
{
	char        theErrorNumber[132];        /* text representation of 'err' */
	char		theErrorName[132];          /* from FindErrorLib */
	char 		theErrorDescription[132];   /* from FindErrorLib */
	static char	theErrorString[132];		/* static for return */
	
	theErrorDescription[0] = 0;  // (bd 2003-10-10)
	
//  FindErrorLib exists only for PPC
#if USE_FIND_ERROR_LIB
	/* Try to use FindErrorLib to interpret result */
	if ((((long) GetFindErrorLibDataInfo) != kUnresolvedCFragSymbolAddress)
	&&  (LookupError(err, theErrorName, theErrorDescription))) {
		/* error was found by FindErrorLib */
		if (strlen(theErrorDescription) > 0) {
			strcpy(theErrorString, theErrorDescription);
		} else {
			/* 
			 * No description was found in database.
			 * Make as much of an error string as we can.
			 */
			snprintf(theErrorString, 132, "OSErr %d, %s", err, theErrorName);
		} 
	} else 
#endif
	{
	
		/* 
		 * FindErrorLib is not installed or error wasn't found in database.
		 * Make a generic error string.
		 */
		strcpy(theErrorName, "OSErr");
		snprintf(theErrorString, 132, "OSErr %ld", (long)err);
	}
		
	/* string representation of the number */
	snprintf(theErrorNumber, 132, "%ld", (long)err);

	if (interp) {
		/* Set up Tcl error code with all info */
		Tcl_SetErrorCode(interp, 
						 theErrorName, 
						 theErrorNumber, 
						 theErrorDescription, 
						 (char *) NULL);		
	}
	
	/* Return the error description for output in the Tcl result */
    return theErrorString;
}

