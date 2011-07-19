/* -*- mode: C; coding: macintosh; -*- */

#include "osxMacTcl.h"

#if TARGET_RT_MAC_MACHO

#ifndef __CARBON__
#include <Carbon/Carbon.h>
#endif

#include <dlfcn.h>

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
            void* handle = dlopen("CoreFoundation", RTLD_LAZY);
            if (dlerror() != NULL) {
                cfstringnormalize = dlsym(handle, "_CFStringNormalize");
                dlclose(handle);
                if (cfstringnormalize) {
                    initialized = TRUE;
                }
            }
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
	Boolean			success;
	int				len;
	int				result = TCL_ERROR;
	
	CFStringRef 		theStrRef = NULL;
	CFStringEncoding 	theEncoding;
	Tcl_DString			ds, *theDsPtr = dsPtr;
	Boolean				usedNormalize = FALSE;
	
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
	int result = TCL_ERROR;
	CFStringRef			theString;
	CFStringEncoding 	theEncoding;
	
	theEncoding = (fromExternal ? kCFStringEncodingMacRoman : kCFStringEncodingUTF8);
	
	if(length < 0) //assume buffer is a C string
	    theString = CFStringCreateWithCStringNoCopy(NULL, buffer, theEncoding, kCFAllocatorNull);
	else
	    theString = CFStringCreateWithBytes(NULL, (const unsigned char *) buffer, length, theEncoding, FALSE);

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

static Tcl_Obj * _CFStringToTclObj(CFStringRef strRef)
{
	Tcl_Obj *		outObj;
	CFIndex 		len = CFStringGetLength(strRef);
	const UniChar *	chars = CFStringGetCharactersPtr(strRef);
	
	if (chars != NULL) {
		outObj = Tcl_NewUnicodeObj(chars, len);
	} else {
		UniChar * buffer = (UniChar*) ckalloc(len * sizeof(UniChar));
		CFStringGetCharacters(strRef, CFRangeMake(0, len), buffer);
		outObj = Tcl_NewUnicodeObj(buffer, len);
		ckfree((char*) buffer);
	}
	
	return outObj;
}


/* CFStringRef to decomposed Unicode Tcl_Obj */
Tcl_Obj * CFStringToTclObj(CFStringRef strRef)
{
	Tcl_Obj *			outObj;
	CFStringRef 		theStrRef = NULL;
	
	theStrRef = TryCFStringNormalize(strRef, kCFStringNormalizationFormC);
	
/* 
 *     if (theStrRef != NULL) {
 *         const UniChar *    chars = CFStringGetCharactersPtr(theStrRef);
 *         CFIndex        len = CFStringGetLength(theStrRef);
 *         outObj = Tcl_NewUnicodeObj(chars, len);
 *         outObj = Tcl_NewUnicodeObj(CFStringGetCharactersPtr(theStrRef), CFStringGetLength(theStrRef));
 *         CFRelease(theStrRef);
 *     } else {
 *         outObj = Tcl_NewUnicodeObj(CFStringGetCharactersPtr(strRef), CFStringGetLength(strRef));
 *     }
 */

	if (theStrRef != NULL) {
		outObj = _CFStringToTclObj(theStrRef);
		CFRelease(theStrRef);
	} else {
		outObj = _CFStringToTclObj(strRef);
	}

	return outObj;
}

/* Unicode Tcl_Obj * to CFStringRef */
CFStringRef TclObjToCFString(Tcl_Obj * inObj)
{
	if (inObj == NULL) {
		return CFSTR("");
	} else {
		return CFStringCreateWithCharacters(NULL, Tcl_GetUnicode(inObj), Tcl_GetCharLength(inObj));
	}
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
 *	This function obtains an FSRef for a given macintosh path.
 *	Unlike the More Files function FSpLocationFromFullPath, this
 *	function will also accept partial paths and resolve any aliases
 *	along the path. It will also create an FSRef for a path that
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
    FSRefPtr fileRefPtr)	/* On return the reference for the path. */
{
    UInt8 fileName[MAXPATHLEN];
	unsigned int fileNameLen;
    OSErr err;
    unsigned int pos, cur = 0;
    Boolean isDirectory=TRUE, filenotexist=FALSE, wasAlias, done;
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
					if(CFURLGetFSRef(parentURL, fileRefPtr))
						err=noErr;
					CFRelease(parentURL);
				}
			}
		}
		}
		if (err != noErr) goto done;
		if(!done){
			err = FSRefMakePath(fileRefPtr,fileName,MAXPATHLEN);
			if (err != noErr) goto done;
			fileNameLen=strlen((const char*) fileName);
		}
    } else { 
    if(path[0] == '/') {
        if((done=(length == 1)))
        {
	    /*
	     * If path = "/", just get root directory.
	     */
            err = FSPathMakeRef((UInt8 *) path, fileRefPtr, &isDirectory);
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
	    strncpy((char*) fileName+fileNameLen, &path[cur], pos - cur);
	    fileNameLen += pos - cur;
	}
        fileName[fileNameLen] = 0;
        err = FSPathMakeRef(fileName, fileRefPtr, &isDirectory);
        if ((err != noErr) && !(filenotexist=(err == fnfErr))) goto done;
        if (!filenotexist) {
			err = FSResolveAliasFile(fileRefPtr, true, &isDirectory, &wasAlias);
			if (err != noErr) goto done;
			if(wasAlias){
				err = FSRefMakePath(fileRefPtr,fileName,MAXPATHLEN);
            if (err != noErr) goto done;
			fileNameLen=strlen((const char*) fileName);
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
 *	FSRef.  Unlike the More Files function FSpGetFullPath, this
 *	function will return a C string in the Handle.  It also will
 *	create paths for FSRef that do not yet exist.
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
    FSRefPtr fsrefP,		/* The location we want a path for. */
    int *length,		/* Length of the resulting path. */
    Handle *fullPath)		/* Handle to path. */
{
    OSErr err;
    UInt8 fileName[MAXPATHLEN];
        unsigned int fileNameLen;
    
    *fullPath = NULL;
                
    err = FSRefMakePath(fsrefP,fileName,MAXPATHLEN);
    if (err == noErr) {
        fileNameLen=strlen((const char*) fileName);
        FSCatalogInfo catalogInfo;
        err = FSGetCatalogInfo(fsrefP,kFSCatInfoNodeFlags,&catalogInfo,NULL,NULL,NULL);
        if (err == noErr && (catalogInfo.nodeFlags & kFSNodeIsDirectoryMask)) {
            // if we have a directory, end path with /
            if(fileNameLen < MAXPATHLEN) {
                fileName[fileNameLen++] = '/';
            } else {
                err=bdNamErr;
            }
        }
        if (err == noErr) {
            // FSRefMakePath et al use decomposed UTF8 on OSX
            Tcl_DString ds;
            fileName[fileNameLen] = 0; // add 0 cstr terminator
            if (DUtfToExternalDString(NULL, (const char*) fileName, -1, &ds) == TCL_OK) {
                err = PtrToHand(Tcl_DStringValue(&ds), fullPath, Tcl_DStringLength(&ds)+1);
                *length = Tcl_DStringLength(&ds);
                Tcl_DStringFree(&ds); // bug 671
            } else {
                err = coreFoundationUnknownErr;
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

#endif
