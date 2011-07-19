/*
 * tclLoadMac.c --
 *
 *	This procedure provides a version of the dlopen() function for use
 *	on the Macintosh.  This procedure will only work with systems
 *	that use the Code Fragment Manager.
 *
 *	Adapted from tclMacLoad.c in the Tcl 8.0p2 distribution.
 */

#include <CodeFragments.h>
#include <Errors.h>
#include <Resources.h>
#include <Strings.h>
#include <FSpCompat.h>

/*
 * Seems that the 3.0.1 Universal headers leave this define out.  So we
 * define it here...
 */

#ifndef fragNoErr
#   define fragNoErr noErr
#endif

#include "tcl.h"
#include "compat:dlfcn.h"

#if GENERATINGPOWERPC
#   define OUR_ARCH_TYPE kPowerPCCFragArch
#else
#   define OUR_ARCH_TYPE kMotorola68KCFragArch
#endif

/*
 * The following data structure defines the structure of a code fragment
 * resource.  We can cast the resource to be of this type to access
 * any fields we need to see.
 */
struct CfrgHeader {
    long 	res1;
    long 	res2;
    long 	version;
    long 	res3;
    long 	res4;
    long 	filler1;
    long 	filler2;
    long 	itemCount;
    char	arrayStart;	/* Array of externalItems begins here. */
};
typedef struct CfrgHeader CfrgHeader, *CfrgHeaderPtr, **CfrgHeaderPtrHand;

/*
 * The below structure defines a cfrag item within the cfrag resource.
 */
struct CfrgItem {
    OSType 	archType;
    long 	updateLevel;
    long	currVersion;
    long	oldDefVersion;
    long	appStackSize;
    short	appSubFolder;
    char	usage;
    char	location;
    long	codeOffset;
    long	codeLength;
    long	res1;
    long	res2;
    short	itemSize;
    Str255	name;		/* This is actually variable sized. */
};
typedef struct CfrgItem CfrgItem;

/*
 *----------------------------------------------------------------------
 *
 * dlopen --
 *
 *	This function is an implementation of dlopen() for the Mac.
 *
 * Results:
 *	Returns the handle of the newly loaded library, or NULL on
 *	failure.
 *
 * Side effects:
 *	Loads the specified library into the process.
 *
 *----------------------------------------------------------------------
 */

static Str255 errName;

void *
dlopen(path, mode)
    const char *path;
    int mode;
{
    CFragConnectionID connID;
    Ptr dummy;
    OSErr err;
    FSSpec fileSpec;
    short fragFileRef, saveFileRef;
    Handle fragResource;
    UInt32 offset = 0;
    UInt32 length = kCFragGoesToEOF;
    char packageName[255];
    const char* pkgGuess;
    char* p;

    /*
     * First thing we must do is infer the package name from the file
     * name.  This is kind of dumb since the caller actually knows
     * this value, it just doesn't give it to us.
     */

    if ((pkgGuess = strrchr(path,':')) != NULL) {
      pkgGuess++;
    } else {
      pkgGuess = path;
    }
    if (!strncmp(pkgGuess,"lib",3)) {
      pkgGuess+=3;
    }
    strcpy(packageName,pkgGuess);
    p = packageName;
    if ((*p) && islower(UCHAR(*p++))) {
      packageName[0] = (char) toupper(UCHAR(packageName[0]));
    }
    while (isalpha(UCHAR(*p)) || (*p == '_')) {
      if (isupper(UCHAR(*p))) {
        *p = (char) tolower(UCHAR(*p));
      }
      p++;
    }
    *p = 0;

    err = FSpLocationFromPath(strlen(path), (char *) path, &fileSpec);
    if (err != noErr) {
	strcpy((char *) errName, "file not found");
	return (void *) NULL;
    }

    /*
     * See if this fragment has a 'cfrg' resource.  It will tell us were
     * to look for the fragment in the file.  If it doesn't exist we will
     * assume we have a ppc frag using the whole data fork.  If it does
     * exist we find the frag that matches the one we are looking for and
     * get the offset and size from the resource.
     */
    saveFileRef = CurResFile();
    SetResLoad(false);
    fragFileRef = FSpOpenResFile(&fileSpec, fsRdPerm);
    SetResLoad(true);
    if (fragFileRef != -1) {
	UseResFile(fragFileRef);
	fragResource = Get1Resource(kCFragResourceType, kCFragResourceID);
	HLock(fragResource);
	if (ResError() == noErr) {
	    CfrgItem* srcItem;
	    long itemCount, index;
	    Ptr itemStart;

	    itemCount = (*(CfrgHeaderPtrHand)fragResource)->itemCount;
	    itemStart = &(*(CfrgHeaderPtrHand)fragResource)->arrayStart;
	    for (index = 0; index < itemCount;
		 index++, itemStart += srcItem->itemSize) {
		srcItem = (CfrgItem*)itemStart;
		if (srcItem->archType != OUR_ARCH_TYPE) continue;
		if (!strncasecmp(packageName, (char *) srcItem->name + 1,
			srcItem->name[0])) {
		    offset = srcItem->codeOffset;
		    length = srcItem->codeLength;
		}
	    }
	}
	/*
	 * Close the resource file.  If the extension wants to reopen the
	 * resource fork it should use the tclMacLibrary.c file during it's
	 * construction.
	 */
	HUnlock(fragResource);
	ReleaseResource(fragResource);
	CloseResFile(fragFileRef);
	UseResFile(saveFileRef);
    }

    /*
     * Now we can attempt to load the fragement using the offset & length
     * obtained from the resource.  We don't worry about the main entry point
     * as we are going to search for specific entry points passed to us.
     */

    c2pstr(packageName);
    err = GetDiskFragment(&fileSpec, offset, length, (StringPtr) packageName,
	    kLoadCFrag, &connID, &dummy, errName);
    if (err != fragNoErr) {
	p2cstr(errName);
	return (void *) NULL;
    }
    return (void *) connID;
}

/*
 *----------------------------------------------------------------------
 *
 * dlsym --
 *
 *	This function is an alternative for the system function
 *	GetProcAddress. It returns the address of a
 *	symbol, give the handle returned by dlopen().
 *
 * Results:
 *	Returns the address of the symbol in the dll.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void *
dlsym(handle, symbol)
    void *handle;
    const char *symbol;
{
    void *procPtr;
    char sym1[255];
    OSErr err;
    SymClass symClass;

    strcpy(sym1, symbol);
    c2pstr(sym1);

    err = FindSymbol((ConnectionID) handle, (StringPtr) sym1,
	    (Ptr *) &procPtr, &symClass);
    if (err != fragNoErr || symClass == kDataCFragSymbol) {
	procPtr = (void *) NULL;
    }
    return procPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * dlerror --
 *
 *	This function returns a string describing the error which
 *	occurred in dlopen().
 *
 * Results:
 *	Returns an error message.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
dlerror()
{
    return (char *) errName;
}


/*
 *----------------------------------------------------------------------
 *
 * dlclose --
 *
 *	Not implemented yet.
 *
 * Results:
 *	Always returns 0 (= O.K.)
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

int
dlclose(handle)
    void *handle;
{
    return 0;
}

