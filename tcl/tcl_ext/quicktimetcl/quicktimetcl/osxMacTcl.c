#include "QuickTimeTcl.h"
#include "osxMacTcl.h"

//#if TARGET_RT_MAC_MACHO

#ifndef __CARBON__
#include <Carbon/Carbon.h>
#endif

// ------------------------------------------------------------------------


static Tcl_Encoding gFSpPathMacRomanEncoding=NULL;

#define SetupFSpPathEncoding() { \
	if(!gFSpPathMacRomanEncoding) \
		gFSpPathMacRomanEncoding = Tcl_GetEncoding(NULL,"macRoman"); \
}


/*============================== CFStrings ==============================*/

/* 
 * CFStringGetCString[Path]() with kCFStringEncodingUTF8 do not work. 
 * They return fully decomposed Utf8 characters which Tcl does not
 * understand.  See Bug 587 and associated discussion on
 * AlphaTcl-developers
 *
 * !!!  conversion through macRoman is grotesque and should not be
 * necessary, but Tcl appears not to properly handle accented character
 * encodings.  See Bug 587 and the associated discussion on
 * AlphaTcl-developers. !!!
 */

#ifndef MAC_OS_X_VERSION_10_2
/* define constants from 10.2 CFString.h to allow compilation in 10.1 */
typedef enum {
	kCFStringNormalizationFormD = 0, // Canonical Decomposition
	kCFStringNormalizationFormKD, // Compatibility Decomposition
	kCFStringNormalizationFormC, // Canonical Decomposition followed by Canonical Composition
	kCFStringNormalizationFormKC // Compatibility Decomposition followed by Canonical Composition
} CFStringNormalizationForm;
#endif

/*
 *----------------------------------------------------------------------
 *
 * TryCFStringNormalize --
 *
 * call the 10.2 only CFStringNormalize() in a backwards compatible way. 
 * 
 * Results:
 *	normalized mutable copy of string (retained, needs to be released!)
 *  or NULL if CFStringNormalize not available.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
#include <mach-o/dyld.h>

static CFMutableStringRef TryCFStringNormalize(CFStringRef theString, CFStringNormalizationForm theForm)
{
	static Boolean initialized = FALSE;
	static void (*cfstringnormalize)(CFMutableStringRef, CFStringNormalizationForm) = NULL;
	
	if(!initialized) {
		if(NSIsSymbolNameDefinedWithHint("_CFStringNormalize", "CoreFoundation")) {
			NSSymbol nsSymbol = NSLookupAndBindSymbolWithHint("_CFStringNormalize", "CoreFoundation");
			if(nsSymbol)  cfstringnormalize = NSAddressOfSymbol(nsSymbol);
		}
		initialized = TRUE;
	}
	if(cfstringnormalize) {
		CFMutableStringRef theMutableString = CFStringCreateMutableCopy(NULL, 0, theString);
		if (theMutableString) {
			cfstringnormalize(theMutableString, theForm);
			return(theMutableString);
		}
	}
	return(NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * CFStringToDString --
 *
 *	This helper function converts a CFString into a DString, 
 *  first transforming to canonical composed or decomposed unicode
 *  depending on the 'compose' flag then transforming to external
 *  (macRoman) encoding if 'external' is set.
 * 
 *  Uses the most direct transformation possible on the current
 *  system, e.g. CFStringNormalize if available, or by transcoding 
 *  to/from macRoman otherwise (potentially lossy!).
 *
 * Results:
 *	Tcl error code.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int CFStringToDString(Tcl_Interp * interp, CFStringRef strRef, Tcl_DString * dsPtr, 
			Boolean compose, Boolean external)
{
	CONST84 char *	str;
	Boolean			success;
	int				len;
	int				result = TCL_ERROR;
	
	CFStringRef 		theStrRef = NULL;
	CFStringEncoding 	theEncoding;
	Tcl_DString			ds, *theDsPtr = dsPtr;
	Boolean				usedNormalize = FALSE, needConvert;
	
	if (compose)
		theStrRef = TryCFStringNormalize(strRef, kCFStringNormalizationFormC);
	else
		theStrRef = TryCFStringNormalize(strRef, kCFStringNormalizationFormD);
	
	if(theStrRef) {
		usedNormalize = TRUE;
			
	} else {
		theStrRef = strRef;
		theEncoding = kCFStringEncodingMacRoman;
	}
	
	if (usedNormalize && !external)
		theEncoding = kCFStringEncodingUTF8;
	else
		theEncoding = kCFStringEncodingMacRoman;
	
	if(!usedNormalize && !external)
		theDsPtr = &ds; // will need ExternalToUtf conversion
					
	len = CFStringGetMaximumSizeForEncoding(CFStringGetLength(theStrRef), theEncoding);
	Tcl_DStringInit(theDsPtr);
	Tcl_DStringSetLength(theDsPtr, len);
	
	success = CFStringGetCString(theStrRef, Tcl_DStringValue(theDsPtr), len+1, theEncoding);
	
	if (success) {
		/* len was only a guess */
		Tcl_DStringSetLength(theDsPtr, strlen(Tcl_DStringValue(theDsPtr)));
		result = TCL_OK;
	} else
		if (interp)  Tcl_SetResult(interp, "Can't extract string from CFString", TCL_STATIC);

	if (!usedNormalize && !external) {
		// need ExternalToUtf conversion
		if(success) {
			SetupFSpPathEncoding();
			Tcl_ExternalToUtfDString(gFSpPathMacRomanEncoding,
					Tcl_DStringValue(theDsPtr), Tcl_DStringLength(theDsPtr), dsPtr);
		}
		Tcl_DStringFree(theDsPtr);
	}
	if(usedNormalize)
		CFRelease(theStrRef);
	
	return result;
}

/*
 *----------------------------------------------------------------------
 *
 * BufferToDString --
 *
 *	This helper function converts a text buffer of given lenth into 
 *  a DString (if length == -1, buffer is assumed to be a C string), 
 *  first transforming from external (macRoman) encoding if
 *  'fromExternal' is set, then transforming to canonical composed
 *  or decomposed unicode depending on the 'compose' flag and finally
 *  transforming to external (macRoman) encoding if 'external' is set.
 * 
 *  Tries to use the most efficient transformations possible on the
 *  current  system, e.g. CFStringNormalize if available, and 
 *  CFStringCreateWithCStringNoCopy if given a C string.
 *
 * Results:
 *	Tcl error code.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int BufferToDString(Tcl_Interp * interp, CONST84 char *buffer, int length,
		Tcl_DString * dsPtr, Boolean compose, Boolean toExternal, Boolean fromExternal)
{
	int result;
	CFStringRef			theString;
	CFStringEncoding 	theEncoding;
	
	theEncoding = (fromExternal ? kCFStringEncodingMacRoman : kCFStringEncodingUTF8);
	
	if(length < 0) //assume buffer is a C string
	    theString = CFStringCreateWithCStringNoCopy(NULL, buffer, theEncoding, kCFAllocatorNull);
	else
	    theString = CFStringCreateWithBytes(NULL, buffer, length, theEncoding, FALSE);

	if(theString) {
		result = CFStringToDString(interp, theString, dsPtr, compose, toExternal);
		CFRelease(theString); // bug 671
	} else {
		if (interp)  Tcl_SetResult(interp, "Can't create CFString from buffer", TCL_STATIC);
	}
	
	return result;
}

/* CFString to external DString */
int CFStringToExternalDString(Tcl_Interp * interp, CFStringRef strRef, Tcl_DString * dsPtr)
{
	return CFStringToDString(interp, strRef, dsPtr, TRUE, TRUE);
}

/* CFString to DString */
int CFStringToUtfDString(Tcl_Interp * interp, CFStringRef strRef, Tcl_DString * dsPtr)
{
	return CFStringToDString(interp, strRef, dsPtr, TRUE, FALSE);
}

/* decomposed utf8 buffer to external DString */
int DUtfToExternalDString(Tcl_Interp * interp, CONST84 char * src, int length, Tcl_DString * dsPtr)
{
	return BufferToDString(interp, src, length, dsPtr, TRUE, TRUE, FALSE);
}

/* decomposed utf8 buffer to DString */
int DUtfToUtfDString(Tcl_Interp * interp, CONST84 char * src, int length, Tcl_DString * dsPtr)
{
	return BufferToDString(interp, src, length, dsPtr, TRUE, FALSE, FALSE);
}

/* external buffer to decomposed utf8 DString */
int ExternalToDUtfDString(Tcl_Interp * interp, CONST84 char * src, int length, Tcl_DString * dsPtr)
{
	return BufferToDString(interp, src, length, dsPtr, FALSE, FALSE, TRUE);
}

/* utf8 buffer to decomposed utf8 DString */
int UtfToDUtfDString(Tcl_Interp * interp, CONST84 char * src, int length, Tcl_DString * dsPtr)
{
	return BufferToDString(interp, src, length, dsPtr, FALSE, FALSE, FALSE);
}

/*==============================        ==============================*/

//  das 091200 reimplemented the following routines from scratch 
//             for Tcl on OSX using modern FileMgr APIs and FSRefs
//             they are analogous to the MacTcl routines in tclMacUtil.c 
//             
//             on OSX these routines are used in oldEndre.c instead
//             of the crufty old Alpha versions


#define MAXPATHLEN 1024

/*
 *----------------------------------------------------------------------
 *
 * FSpLocationFromPath --
 *
 *	This function obtains an FSSpec for a given macintosh path.
 *	Unlike the More Files function FSpLocationFromFullPath, this
 *	function will also accept partial paths and resolve any aliases
 *	along the path. It will also create an FSSpec for a path that
 *	does not yet exist. 
 *
 * Results:
 *	OSErr code.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */


OSErr
FSpLocationFromPath(
    int length,			/* Length of path. */
    CONST84 char *path,		/* The path to convert. */
    FSSpecPtr fileSpecPtr)	/* On return the spec for the path. */
{
    UInt8 fileName[MAXPATHLEN];
	unsigned int fileNameLen;
    OSErr err;
    unsigned int pos, cur;
    Boolean isDirectory=TRUE, filenotexist=FALSE, wasAlias, done;
    FSRef fsref;
	Tcl_DString ds;
	
	// FSRefMakePath et al use deomposed UTF8 on OSX
	if(ExternalToDUtfDString(NULL, path, length, &ds) == TCL_ERROR) {
		err = coreFoundationUnknownErr;
		goto done;
	}
	
	path = Tcl_DStringValue(&ds);
	length = Tcl_DStringLength(&ds);
	
    pos = 0;
    fileName[0] = 0;
	fileNameLen = 0;

    /*
     * Check to see if this is a full path.  If partial
     * we assume that path starts with the current working
     * directory.  (Ie. volume & dir = 0)
     */
    if ((done=(length == 0)) || (path[0] != '/')) {
	     // start at current directory
		{
		CFBundleRef appBundle=CFBundleGetMainBundle();
		CFURLRef	appURL=NULL, parentURL=NULL;
		CFURLPathStyle pathStyle=kCFURLPOSIXPathStyle;
		err = coreFoundationUnknownErr;
		if(appBundle)
		{
			appURL=CFBundleCopyBundleURL(appBundle);
			if(appURL)
			{
				parentURL=CFURLCreateCopyDeletingLastPathComponent(kCFAllocatorDefault,appURL);
				CFRelease(appURL);
				if(parentURL)
				{
					if(CFURLGetFSRef(parentURL, &fsref))
						err=noErr;
					CFRelease(parentURL);
				}
			}
		}
		}
		if (err != noErr) goto done;
		if(!done){
			err = FSRefMakePath(&fsref,fileName,MAXPATHLEN);
			if (err != noErr) goto done;
			fileNameLen=strlen(fileName);
		}
    } else { 
    if(path[0] == '/') {
        if((done=(length == 1)))
        {
	    /*
	     * If path = "/", just get root directory.
	     */
            err = FSPathMakeRef((UInt8 *) path, &fsref, &isDirectory);
            if (err != noErr) goto done;
        } else {
            pos++;
        }
        
    }
    }
    if(!done) {
        fileName[fileNameLen++] = '/';
        fileName[fileNameLen] = 0;
        
    while (pos < length) {
	if (!isDirectory || filenotexist) {err=dirNFErr; goto done;}
	cur = pos;
	while (path[pos] != '/' && pos < length) {
	    pos++;
	}
	if (fileNameLen+pos-cur > MAXPATHLEN) {
	    err=bdNamErr; goto done;
	} else {
	    strncpy(fileName+fileNameLen, &path[cur], pos - cur);
	    fileNameLen += pos - cur;
	}
        fileName[fileNameLen] = 0;
        err = FSPathMakeRef(fileName, &fsref, &isDirectory);
        if ((err != noErr) && !(filenotexist=(err == fnfErr))) goto done;
        if (!filenotexist) {
			err = FSResolveAliasFile(&fsref, true, &isDirectory, &wasAlias);
			if (err != noErr) goto done;
			if(wasAlias){
				err = FSRefMakePath(&fsref,fileName,MAXPATHLEN);
            if (err != noErr) goto done;
			fileNameLen=strlen(fileName);
			}
        }

	if (path[pos] == '/') {
            if (!isDirectory || filenotexist) {err=dirNFErr; goto done;}
	    pos++;
	    fileName[fileNameLen++] = '/';
		fileName[fileNameLen] = 0;
	}
    }
    }
    if(!filenotexist) {
        err = FSGetCatalogInfo(&fsref,kFSCatInfoNone,NULL,NULL,fileSpecPtr,NULL);
    } else {
        FSCatalogInfo catalogInfo;
		Tcl_DString ds;
        if(pos - cur>sizeof(StrFileName)) {err=bdNamErr; goto done;}
        err = FSGetCatalogInfo(&fsref, kFSCatInfoNodeID | kFSCatInfoVolume, &catalogInfo, NULL, NULL, NULL);
        if (err != noErr) goto done;
		fileSpecPtr->vRefNum = catalogInfo.volume;
        fileSpecPtr->parID	 = catalogInfo.nodeID;
		if(DUtfToExternalDString(NULL, &path[cur], pos - cur, &ds) == TCL_ERROR) {
			err = coreFoundationUnknownErr;
			goto done;
		}
        strncpy(fileSpecPtr->name+1, Tcl_DStringValue(&ds), Tcl_DStringLength(&ds));
        fileSpecPtr->name[0] = Tcl_DStringLength(&ds);
		Tcl_DStringFree(&ds);
        err = fnfErr;
    }
	
done:
	Tcl_DStringFree(&ds);
    return err;
}

/*
 *----------------------------------------------------------------------
 *
 * FSpPathFromLocation --
 *
 *	This function obtains a full path name for a given macintosh
 *	FSSpec.  Unlike the More Files function FSpGetFullPath, this
 *	function will return a C string in the Handle.  It also will
 *	create paths for FSSpec that do not yet exist.
 *
 * Results:
 *	OSErr code.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

OSErr
FSpPathFromLocation(
    FSSpecPtr spec,		/* The location we want a path for. */
    int *length,		/* Length of the resulting path. */
    Handle *fullPath)		/* Handle to path. */
{
    OSErr err;
    FSSpec tempSpec;
    UInt8 fileName[MAXPATHLEN];
	unsigned int fileNameLen;
    Boolean filenotexist=FALSE;
    FSRef fsref;
    
    *fullPath = NULL;
		
    err=FSpMakeFSRef(spec,&fsref);

    if ((err == noErr) || (filenotexist=(err == fnfErr))) {
        if (filenotexist) {
            // file does not exist, find parent dir
            memmove(&tempSpec, spec, sizeof(FSSpec));
            tempSpec.name[0]=0;
            err=FSpMakeFSRef(&tempSpec,&fsref);
        } 
        if (err == noErr) {            
            err = FSRefMakePath(&fsref,fileName,MAXPATHLEN);
            if (err == noErr) {
                fileNameLen=strlen(fileName);
                if(filenotexist) { // add file name manually
                    // need to convert spec name from macroman to utf8d before adding to fileName
                    Tcl_DString ds;
                    if (ExternalToDUtfDString(NULL,&spec->name[1], spec->name[0], &ds) == TCL_OK) { // bug 671
                        if(fileNameLen+Tcl_DStringLength(&ds)<MAXPATHLEN) {
                            fileName[fileNameLen++] = '/';
                            strncpy(fileName+fileNameLen, Tcl_DStringValue(&ds), Tcl_DStringLength(&ds));
                            fileNameLen += Tcl_DStringLength(&ds);
                        } else {
                            err=bdNamErr;
                        }
                        Tcl_DStringFree(&ds);
                    } else {
                        err = coreFoundationUnknownErr;
                    }
                } else {
                    FSCatalogInfo catalogInfo;
                    err = FSGetCatalogInfo(&fsref,kFSCatInfoNodeFlags,&catalogInfo,NULL,NULL,NULL);
                    if (err == noErr && (catalogInfo.nodeFlags & kFSNodeIsDirectoryMask)) {
                        // if we have a directory, end path with /
                        if(fileNameLen < MAXPATHLEN) {
                            fileName[fileNameLen++] = '/';
                        } else {
                            err=bdNamErr;
                        }
                    }
                }
                if (err == noErr) {
                    // FSRefMakePath et al use decomposed UTF8 on OSX
                    Tcl_DString ds;
                    fileName[fileNameLen] = 0; // add 0 cstr terminator
                    if (DUtfToExternalDString(NULL, fileName, -1, &ds) == TCL_OK) {
                        err = PtrToHand(Tcl_DStringValue(&ds), fullPath, Tcl_DStringLength(&ds)+1);
                        *length = Tcl_DStringLength(&ds);
                        Tcl_DStringFree(&ds); // bug 671
                    } else {
                        err = coreFoundationUnknownErr;
                    }
                }
            }
        }
    }
    
    /*
     * On error Dispose the handle, set it to NULL & return the err.
     * Otherwise, set the length & return.
     */
    if (err != noErr) {
        if ( *fullPath != NULL ) {
            DisposeHandle(*fullPath);
        }
        *fullPath = NULL;
        *length = 0;
    }

    return err;
}

// ------------------------------------------------------------------------


//  das 271100 modified and adapted from MacTcl for Tcl on OSX


/*
 * tclMacResource.c --
 *
 *	This file contains several commands that manipulate or use
 *	Macintosh resources.  Included are extensions to the "source"
 *	command, the mac specific "beep" and "resource" commands, and
 *	administration for open resource file references.
 *
 * Copyright (c) 1996-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: osxMacTcl.c,v 1.1.1.1 2003/04/04 16:24:35 matben Exp $
 */

/*
 * Pass this in the mode parameter of SetSoundVolume to determine
 * which volume to set.
 */

enum WhichVolume {
    SYS_BEEP_VOLUME,    /* This sets the volume for SysBeep calls */ 
    DEFAULT_SND_VOLUME, /* This one for SndPlay calls */
    RESET_VOLUME        /* And this undoes the last call to SetSoundVolume */
};
 
/*
 * Prototypes for procedures defined later in this file:
 */


static void 		SetSoundVolume(int volume, enum WhichVolume mode);

/*
 *----------------------------------------------------------------------
 *
 * Tcl_BeepObjCmd --
 *
 *	This procedure makes the beep sound.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Makes a beep.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_BeepObjCmd(
    ClientData dummy,			/* Not used. */
    Tcl_Interp *interp,			/* Current interpreter. */
    int objc,				/* Number of arguments. */
    Tcl_Obj *CONST84 objv[])		/* Argument values. */
{
    Tcl_Obj *resultPtr, *objPtr;
    Handle sound;
    Str255 sndName;
    int volume = -1, length;
    char * sndArg = NULL;

    resultPtr = Tcl_GetObjResult(interp);
    if (objc == 1) {
	SysBeep(1);
	return TCL_OK;
    } else if (objc == 2) {
	if (!strcmp(Tcl_GetStringFromObj(objv[1], &length), "-list")) {
	    int count, i;
	    short id;
	    Str255 theName;
	    ResType rezType;
			
	    count = CountResources('snd ');
	    for (i = 1; i <= count; i++) {
		sound = GetIndResource('snd ', i);
		if (sound != NULL) {
		    GetResInfo(sound, &id, &rezType, theName);
		    if (theName[0] == 0) {
			continue;
		    }
		    objPtr = Tcl_NewStringObj((char *) theName + 1,
			    theName[0]);
		    Tcl_ListObjAppendElement(interp, resultPtr, objPtr);
		}
	    }
	    return TCL_OK;
	} else {
	    sndArg = Tcl_GetStringFromObj(objv[1], &length);
	}
    } else if (objc == 3) {
	if (!strcmp(Tcl_GetStringFromObj(objv[1], &length), "-volume")) {
	    Tcl_GetIntFromObj(interp, objv[2], &volume);
	} else {
	    goto beepUsage;
	}
    } else if (objc == 4) {
	if (!strcmp(Tcl_GetStringFromObj(objv[1], &length), "-volume")) {
	    Tcl_GetIntFromObj(interp, objv[2], &volume);
	    sndArg = Tcl_GetStringFromObj(objv[3], &length);
	} else {
	    goto beepUsage;
	}
    } else {
	goto beepUsage;
    }
	
    /*
     * Play the sound
     */
    if (sndArg == NULL) {
	/*
         * Set Volume for SysBeep
         */

	if (volume >= 0) {
	    SetSoundVolume(volume, SYS_BEEP_VOLUME);
	}
	SysBeep(1);

	/*
         * Reset Volume
         */

	if (volume >= 0) {
	    SetSoundVolume(0, RESET_VOLUME);
	}
    } else {
	strcpy((char *) sndName + 1, sndArg);
	sndName[0] = length;
	sound = GetNamedResource('snd ', sndName);
	if (sound != NULL) {
	    /*
             * Set Volume for Default Output device
             */

	    if (volume >= 0) {
		SetSoundVolume(volume, DEFAULT_SND_VOLUME);
	    }

	    SndPlay(NULL, (SndListHandle) sound, false);

	    /*
             * Reset Volume
             */

	    if (volume >= 0) {
		SetSoundVolume(0, RESET_VOLUME);
	    }
	} else {
	    Tcl_AppendStringsToObj(resultPtr, " \"", sndArg, 
		    "\" is not a valid sound.  (Try ",
		    Tcl_GetString(objv[0]), " -list)", NULL);
	    return TCL_ERROR;
	}
    }

    return TCL_OK;

    beepUsage:
    Tcl_WrongNumArgs(interp, 1, objv, "[-volume num] [-list | sndName]?");
    return TCL_ERROR;
}

/*
 *-----------------------------------------------------------------------------
 *
 * SetSoundVolume --
 *
 *	Set the volume for either the SysBeep or the SndPlay call depending
 *	on the value of mode (SYS_BEEP_VOLUME or DEFAULT_SND_VOLUME
 *      respectively.
 *
 *      It also stores the last channel set, and the old value of its 
 *	VOLUME.  If you call SetSoundVolume with a mode of RESET_VOLUME, 
 *	it will undo the last setting.  The volume parameter is
 *      ignored in this case.
 *
 * Side Effects:
 *	Sets the System Volume
 *
 * Results:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
SetSoundVolume(
    int volume,              /* This is the new volume */
    enum WhichVolume mode)   /* This flag says which volume to
			      * set: SysBeep, SndPlay, or instructs us
			      * to reset the volume */
{
    static int hasSM3 = 1;
    static enum WhichVolume oldMode;
    static long oldVolume = -1;

    
    /*
     * If we don't have Sound Manager 3.0, we can't set the sound volume.
     * We will just ignore the request rather than raising an error.
     */
    
    if (!hasSM3) {
    	return;
    }
    
    switch (mode) {
    	case SYS_BEEP_VOLUME:
	    GetSysBeepVolume(&oldVolume);
	    SetSysBeepVolume(volume);
	    oldMode = SYS_BEEP_VOLUME;
	    break;
	case DEFAULT_SND_VOLUME:
	    GetDefaultOutputVolume(&oldVolume);
	    SetDefaultOutputVolume(volume);
	    oldMode = DEFAULT_SND_VOLUME;
	    break;
	case RESET_VOLUME:
	    /*
	     * If oldVolume is -1 someone has made a programming error
	     * and called reset before setting the volume.  This is benign
	     * however, so we will just exit.
	     */
	  
	    if (oldVolume != -1) {	
	        if (oldMode == SYS_BEEP_VOLUME) {
	    	    SetSysBeepVolume(oldVolume);
	        } else if (oldMode == DEFAULT_SND_VOLUME) {
		    SetDefaultOutputVolume(oldVolume);
	        }
	    }
	    oldVolume = -1;
    }
}


//#endif
