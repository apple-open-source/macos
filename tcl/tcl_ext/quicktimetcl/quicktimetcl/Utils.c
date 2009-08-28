/*
 * Utils.c --
 *
 *		Various utilities.
 *		It is part of the QuickTimeTcl package which provides Tcl/Tk bindings for QuickTime.
 *
 * Copyright (c) 2003-2008  Mats Bengtsson
 *
 * $Id: Utils.c,v 1.20 2008/05/06 08:21:40 matben Exp $
 */

#ifdef _WIN32
#   include "QuickTimeTclWin.h"
#endif  

#include "QuickTimeTcl.h"
#include "tkFont.h"

extern int gQTTclDebugLevel;
extern int gQTTclDebugLog;
extern Tcl_Channel gQTTclDebugChannel;

#ifndef PATH_MAX
#	define PATH_MAX 1024
#endif

/*
 * Mapping from an Apple Movie Toolbox error code, where -2000 corresponds to an index 0,
 * and -2053 to an index 53 etc., to a text message. Perhaps this can be found
 * in some Apple procedure?
 */

char *MovieResultCodes[] = {
    "Cannot use this data reference",
    "Problem with this image description",
    "Movie file corrupted",
    "Cannot locate this handler",
    "Cannot open this handler",
    "Component cannot accomodate this data",
    "Media has no media handler",
    "Media has no data handler",
    "This media is corrupted or invalid",
    "This track is corrupted or invalid",
    "This movie is corrupted or invalid",           // -2010
    "This sample table is corrupted or invalid",
    "This data reference is invalid",
    "This handler is invalid",
    "This duration value is invalid",
    "This time value is invalid",
    "Cannot write to this movie file",
    "The track's edit list is corrupted",
    "These media don't match",
    "Your progress procedure returned an error",
    "You haven't initialized the movie toolbox",    // -2020
    "Cannot locate this file",
    "Error trying to create a single-fork file. This occurs when the file already exists",
    "This edit state is invalid",
    "This edit state is not valid for this movie",
    "Movie or track has been disposed",
    "Cannot locate this user data item",
    "Maximum size must be larger",
    "This track index value is not valid",
    "Cannot locate a track with this ID value",
    "This track is not in this movie",              // -2030
    "This time value is outside of this track",
    "This time value is outside of this media",
    "This edit index value is not valid",
    "Internal error",
    "Cannot enable this track",
    "Specified rectangle has invalid coordinates",
    "There is no sample with this sample number",
    "There is no chunk with this chunk number",
    "Sample description index value invalid",
    "The chunk cache is corrupted",                 // -2040
    "This sample description is invalid or corrupted",
    "Cannot read from this data source",
    "Cannot write to this data source",
    "Data source is already open for write",
    "You have already closed this data source",
    "End of data",
    "No data reference value found",
    "Toolbox cannot find a movie in the movie file",
    "Invalid data reference",
    "Data reference index value is invalid",         // -2050
    "Could not find a default data reference",
    "Movie toolbox could not use a sample",
    "Movie toolbox does not support this feature"
};

/*
 * Some network error codes. (-2129 - -2148)
 */

char *URLDataErrorCodes [] = {
    "urlDataHHTTPProtocolErr",   		// -2129
    "urlDataHHTTPNoNetDriverErr",    	// -2130
    "urlDataHHTTPURLErr",             	// -2131
    "urlDataHHTTPRedirectErr",        	// -2132
    "urlDataHFTPProtocolErr",         	// -2133
    "urlDataHFTPShutdownErr",        	// -2134
    "urlDataHFTPBadUserErr",           	// -2135
    "urlDataHFTPBadPasswordErr",       	// -2136
    "urlDataHFTPServerErr",            	// -2137
    "urlDataHFTPDataConnectionErr",    	// -2138
    "urlDataHFTPNoDirectoryErr",       	// -2139
    "urlDataHFTPQuotaErr",             	// -2140
    "urlDataHFTPPermissionsErr",      	// -2141
    "urlDataHFTPFilenameErr",          	// -2142
    "urlDataHFTPNoNetDriverErr",       	// -2143
    "urlDataHFTPBadNameListErr",     	// -2144
    "urlDataHFTPNeedPasswordErr",    	// -2145
    "urlDataHFTPNoPasswordErr",        	// -2146
    "urlDataHFTPServerDisconnectedErr",	// -2147
    "urlDataHFTPURLErr"					// -2148
};

/*
 * Mapping from an Apple Image Compressor Manager error code, where -8960 corresponds 
 * to an index 0, and -8973 to an index 13 etc., to a text message. 
 */

char *ICMResultCodes[] = {
    "General error condition",                                  // -8960
    "Image Compression Manager could not find the specified compressor",
    "Feature not implemented by this compressor",
    "Invalid buffer size specified",
    "Could not allocate the screen buffer",
    "Could not allocate the image buffer",
    "Error loading or unloading data",
    "Operation aborted by the progress function",
    "Compressor would use screen buffer if it could",
    "Compressor data contains inconsistences",
    "Compressor does not support the compression version used to compress the image",   // -8970
    "Requested extension is not in the image description",
    "Component cannot perform requested operation",
    "Could not open the compressor or decompressor"
};


/*
 *----------------------------------------------------------------------
 *
 * ConvertTkPhotoToPicture --
 *
 *		Convert a Tk image to a Mac Picture.
 *
 * Results:
 *  	Normal TCL results
 *
 * Side effects:
 *		Creates a mac picture. 
 *
 *----------------------------------------------------------------------
 */

int  	
ConvertTkPhotoToPicture( 
	Tcl_Interp 	*interp, 		/* tcl interpreter */
	Tk_PhotoHandle 	tkPhoto, 		/* (in) tk photo handle */
	PicHandle 	*thePic )		/* (out) an Apple Pict */
{
    GWorldPtr 		gw = NULL;
    GWorldPtr 		saveGW = NULL;
    GDHandle 		saveGD = NULL;
    PixMapHandle 	pixels = NULL;
    Tk_PhotoImageBlock 	photoBlock;
    unsigned char 	*pixelPtr;
    unsigned char 	*photoPixels;
    OSErr 		err;
    Rect 		r;
    int			i, j;
    int			width, height;
    
    /*
     * Retrieve image data from 'tkPhoto' and put it into 'photoBlock'.
     */
    
    Tk_PhotoGetImage( tkPhoto, &photoBlock );
    GetGWorld( &saveGW, &saveGD );
    
    Tk_PhotoGetSize( tkPhoto, &width, &height );
    r.top = 0;
    r.left = 0;
    r.right = width;
    r.bottom = height;
    
    /* Get a new GWorld to draw into */ 
    err = MySafeNewGWorld( &gw, 32, &r, NULL, NULL, 0 );
    SetGWorld( gw, NULL );	
    pixels = GetGWorldPixMap( gw );
    
    /*
     * Lock down the pixels so they don't move out from under us.
     */
    
    LockPixels(pixels);
    
    /* 
     * Copy the pixels to the gworld.
     * The Mac pixmap stores them as "alpha, red, gree, blue", but tk 8.3 stores them
     * as "red, green, blue, alpha (transparency)".
     */
    
    for (i = 0; i < photoBlock.height; i++) {
	pixelPtr = (unsigned char *) 
	(GetPixBaseAddr(pixels) + i * (0x3FFF & ((*pixels)->rowBytes)));
	photoPixels = photoBlock.pixelPtr + i * photoBlock.pitch;
	for (j = 0; j < photoBlock.width; j++) {
#if TK_MINOR_VERSION <= 2			    				
	    *pixelPtr = 0; pixelPtr++;
#else
	    *pixelPtr = *(photoPixels + photoBlock.offset[3]); pixelPtr++;
#endif			
	    *pixelPtr = *(photoPixels + photoBlock.offset[0]); pixelPtr++;
	    *pixelPtr = *(photoPixels + photoBlock.offset[1]); pixelPtr++;
	    *pixelPtr = *(photoPixels + photoBlock.offset[2]); pixelPtr++;
	    photoPixels += photoBlock.pixelSize;
	}
    }
    
    /* 
     * Capture the gworlds contents in a picture handle. 
     */
    
    *thePic = OpenPicture( &r );
#if TARGET_API_MAC_CARBON
    CopyBits( GetPortBitMapForCopyBits( gw ),
	     GetPortBitMapForCopyBits( gw ),
	     &r,
	     &r,
	     srcCopy,
	     nil );
#else
    CopyBits( &((GrafPtr) gw)->portBits,
	     &((GrafPtr) gw)->portBits,
	     &r,
	     &r,
	     srcCopy,
	     nil );
#endif
    ClosePicture();
    
    UnlockPixels( pixels );
    if (gw) {
	DisposeGWorld( gw );
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ConvertPictureToTkPhoto --
 *
 *		Convert a Pict to a Tk photo image.
 *
 * Results:
 *  	Normal TCL results
 *
 * Side effects:
 *		Writes a tk image. 
 *
 *----------------------------------------------------------------------
 */

int  	
ConvertPictureToTkPhoto( 
			Tcl_Interp 	*interp, 
			PicHandle 	thePic, 		/* (in) the Pict to be translated */
			int 		width, 			/* (in) if 0 use natural width */
			int 		height, 		/* (in) if 0 use natural height */
			char 		*tkImage )		/* (in) name of image */
{
    Rect 				bounds;
    PixMapHandle 		pixels = NULL;
    CGrafPtr 			saveWorld = NULL;
    GDHandle 			saveDevice = NULL;
    GWorldPtr 			gWorld = NULL;
    QDErr 				err = noErr;
    int					result = TCL_OK;	
    
    GetGWorld( &saveWorld, &saveDevice );
    
    /*
     * Find the desired width and height of image. 
     * Note that for QTVR movies the track picture's dimension is not the 
     * same as the movies dimension!
     * Endians: Thanks to Per Bergland and Tim Monroe Apple for this one!
     */
    
    bounds.left = 0;
    bounds.right = EndianS16_BtoN((**thePic).picFrame.right) - 
    EndianS16_BtoN((**thePic).picFrame.left);
    bounds.top = 0;
    bounds.bottom = EndianS16_BtoN((**thePic).picFrame.bottom) - 
    EndianS16_BtoN((**thePic).picFrame.top);
    if (width > 0) {
    	bounds.right = width;
    }
    if (height > 0) {
    	bounds.bottom = height;
    }
    
    /* Get a new GWorld to draw into */ 
    err = MySafeNewGWorld( &gWorld, 32, &bounds, NULL, NULL, 0 );
    if (err != noErr) {
        CheckAndSetErrorResult( interp, err );
	result = TCL_ERROR;
	goto bail;
    }
    SetGWorld( gWorld, saveDevice );			
    EraseRect( &bounds );
    DrawPicture( thePic, &bounds );
    pixels = GetGWorldPixMap( gWorld );
    if (MakeTkPhotoFromPixMap( interp, pixels, tkImage ) != TCL_OK) {
	result = TCL_ERROR;
	goto bail;
    }
    
bail:
    SetGWorld( saveWorld, saveDevice );
    if (gWorld != NULL) {
	DisposeGWorld( gWorld );
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * MakeTkPhotoFromPixMap --
 *
 *		Takes a PixMap handle and makes a Tk photo image.
 *
 * Results:
 *  	Normal Tcl results
 *
 * Side effects:
 *		Writes a tk image. 
 *
 *----------------------------------------------------------------------
 */

int  	
MakeTkPhotoFromPixMap( 
		      Tcl_Interp 	*interp, 
		      PixMapHandle pixels,		/* (in) pixmap handle */
		      char 		*tkImage )		/* (in) name of image */
{
    int					i, j;
    Rect 				bounds;
    unsigned char 		*photoPixels;
    Tk_PhotoHandle 		tkPhoto = NULL;
    Tk_PhotoImageBlock 	blockPtr;
    Tcl_Obj 			*resultObjPtr;
    int					result = TCL_OK;	
    
    tkPhoto = Tk_FindPhoto( interp, tkImage );	
    if (tkPhoto == NULL) {
	resultObjPtr = Tcl_NewStringObj("Image not found \"", -1);
	Tcl_AppendStringsToObj( resultObjPtr, tkImage, "\"", (char *) NULL);
	Tcl_SetObjResult( interp, resultObjPtr );
	result = TCL_ERROR;
	goto bail;
    }
    
    Tk_PhotoBlank( tkPhoto );
    
    /*
     * Lock down the pixels so they don't move out from under us.
     */
    
    LockPixels( pixels );
#if TARGET_API_MAC_CARBON
    GetPixBounds( pixels, &bounds );
#else
    bounds = (**pixels).bounds;
#endif
    /*
     * The Mac pixmap stores them as "undefined, red, gree, blue", but tk 8.3 stores them
     * as "red, green, blue, alpha (transparency)".
     */
    
    blockPtr.pixelPtr = (unsigned char *) GetPixBaseAddr( pixels );
    blockPtr.width = bounds.right;
    blockPtr.height = bounds.bottom;
    blockPtr.pitch = 0x3FFF & ((*pixels)->rowBytes);
    blockPtr.pixelSize = 4;
    blockPtr.offset[0] = 1;
    blockPtr.offset[1] = 2;
    blockPtr.offset[2] = 3;
#if TK_MINOR_VERSION >= 3			    				
    blockPtr.offset[3] = 0;
#endif
    
    /*
     * Problem with transparency: the first 8 bits in the 32 bit offscreen GWorld
     * doesn't correspond to an alpha channel, but is undefined. Since its content
     * seems to be 0, which by tk is interpreted as completely transparent, we need
     * to set it to 255, completely opaque.
     */
    
#if TK_MINOR_VERSION >= 3		
    for (i = 0; i < blockPtr.height; i++) {
	photoPixels = blockPtr.pixelPtr + i * blockPtr.pitch;
	for (j = 0; j < blockPtr.width; j++) {
	    photoPixels[0] = 255;
	    photoPixels += blockPtr.pixelSize;
	}
    }
#endif
    
    Tk_PhotoPutBlock(tkPhoto, &blockPtr, 
		     0, 0, bounds.right, bounds.bottom, TK_PHOTO_COMPOSITE_SET );
    
bail:
    UnlockPixels( pixels );
    return result;
}

/*
 *-----------------------------------------------------------------------------
 *
 * QTTclNewDataRefFromUTF8Obj --
 *
 * 	Convert the file name into a Data Reference.
 *      Take care of any path normalization, resolve relative paths, and make
 *      the necessary utf translations.
 *
 * Results:
 *	Standard Tcl result. 
 *
 * Side effects:
 *	None
 *
 *-----------------------------------------------------------------------------
 */
 
int
QTTclNewDataRefFromUTF8Obj(			   
    Tcl_Interp 	*interp, 
    Tcl_Obj 	*fileNameObj, 		/* (in) utf8 */
    Handle	*outDataRef,
    OSType	*outDataRefType) 
{
    char	*file;
    Tcl_Obj	*normObj;
    OSStatus    err;
    CFStringRef fileCF = NULL;
    QTPathStyle pathStyle;
    int		result = TCL_OK;
    
#if TARGET_API_MAC_CARBON
    pathStyle = kQTNativeDefaultPathStyle;
#endif
#ifdef _WIN32
    pathStyle = kQTPOSIXPathStyle;
#endif

	/* Get the file path with normal slashes etc. */
    normObj = Tcl_FSGetNormalizedPath(interp, fileNameObj);
    if (normObj == NULL) {
 	Tcl_AppendStringsToObj(Tcl_GetObjResult(interp), 
		"file \"", Tcl_GetString(fileNameObj),
		"\" doesn't exist", NULL);
	result = TCL_ERROR;
	goto error;
    }
    file = Tcl_GetString(normObj);
    fileCF = CFStringCreateWithCString(NULL, file, kCFStringEncodingUTF8);
    err = QTNewDataReferenceFromFullPathCFString(fileCF, pathStyle, 
	    0, outDataRef, outDataRefType);
    if (err != noErr) {
	*outDataRef = NULL;
	Tcl_AppendStringsToObj(Tcl_GetObjResult(interp), 
		"file \"", Tcl_GetString(fileNameObj),
		"\" doesn't exist", NULL);
	result = TCL_ERROR;
	goto error;
    }

error:
    if (fileCF) {
	CFRelease(fileCF);
    }
    return result;
}

/*
 *-----------------------------------------------------------------------------
 *
 * QTTclNewUTF8ObjFromDataRef --
 *
 * 	Convert a Data Reference into a Tcl_Obj with utf8 file path.
 *
 * Results:
 *	Standard Tcl result. 
 *
 * Side effects:
 *	None
 *
 *-----------------------------------------------------------------------------
 */

int
QTTclNewUTF8ObjFromDataRef(
    Tcl_Interp	*interp, 
    Handle	inDataRef,
    OSType	inDataRefType,
    Tcl_Obj 	**fileNameObjPtr) 	/* (out) utf8 */
{
    OSStatus    err;
    CFStringRef	fileCF;
    char	file[PATH_MAX + 1];
    QTPathStyle pathStyle;
    int		result = TCL_OK;
    
#if TARGET_API_MAC_CARBON
    pathStyle = kQTNativeDefaultPathStyle;
#endif
#ifdef _WIN32
    pathStyle = kQTWindowsPathStyle;
#endif

    err = QTGetDataReferenceFullPathCFString(inDataRef, inDataRefType, pathStyle, &fileCF);
    if (err != noErr) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj("QTGetDataReferenceFullPathCFString failed", -1));
	result = TCL_ERROR;
	goto error;
    }

    if (!CFStringGetCString(fileCF, file, PATH_MAX + 1, kCFStringEncodingUTF8)) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj("CFStringGetCString failed", -1));
	result = TCL_ERROR;
	goto error;
    }
    Tcl_SetStringObj(*fileNameObjPtr, file, -1);
					   
error:
    if (fileCF) {
	CFRelease(fileCF);
    }
    return result;
}

/*
 *-----------------------------------------------------------------------------
 *
 * QTTclNativePathNameToFSSpec --
 *
 * 	Convert the file name into a 'FSSpec'.
 *      Take care of any path normalization, resolve relative paths, and make
 *      the necessary utf translations.
 *
 * Results:
 *		An OSErr 
 *
 * Side effects:
 *		None
 *
 *-----------------------------------------------------------------------------
 */

OSErr
QTTclNativePathNameToFSSpec( 
			    Tcl_Interp 	*interp, 
			    const char 	*filename, 		/* (in) utf8 */
			    FSSpec 		*fssPtr )		/* (out) */
{
    Tcl_Obj			*translatedPathObjPtr = NULL;
    CONST char		*charPtr = NULL;        
    char			normalizedPath[512] = "";
    Tcl_DString 	ds;
    OSErr			err = noErr;
    
    /*
     * Normalize path:
     * 1) 'file nativename filename' 
     *    (Convert forward slashes to backslashes in Windows paths)
     * 2) make absolute path if relative
     */
    
    filename = Tcl_TranslateFileName(interp, filename, &ds);
    if (filename == NULL) {
        return fnfErr;
    }
    translatedPathObjPtr = Tcl_NewStringObj( filename, -1 );
    Tcl_IncrRefCount( translatedPathObjPtr );
    Tcl_DStringFree(&ds);
    
    if (TCL_PATH_RELATIVE == Tcl_FSGetPathType( translatedPathObjPtr )) {
        Tcl_Obj			*cwdPathObjPtr = NULL;
        Tcl_Obj  		*absPathObjPtr = NULL;
        Tcl_Obj			*listObjPtr = NULL;
	
        cwdPathObjPtr = Tcl_FSGetCwd( interp );
        if (cwdPathObjPtr == NULL) {
            return fnfErr;
        }
        listObjPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );
        Tcl_ListObjAppendElement( interp, listObjPtr, cwdPathObjPtr );
        Tcl_ListObjAppendElement( interp, listObjPtr, translatedPathObjPtr );
        
        /* Tcl_FSJoinPath returns object with ref count 0 */
        Tcl_IncrRefCount(listObjPtr);
        absPathObjPtr = Tcl_FSJoinPath( listObjPtr, -1 );
        Tcl_IncrRefCount( absPathObjPtr );
        Tcl_DecrRefCount( listObjPtr );
        Tcl_DecrRefCount( cwdPathObjPtr );
        Tcl_DecrRefCount( translatedPathObjPtr );
        translatedPathObjPtr = absPathObjPtr;
    }
    charPtr = Tcl_GetStringFromObj( translatedPathObjPtr, NULL );
    strncpy( normalizedPath, charPtr, 511 );
    Tcl_DecrRefCount( translatedPathObjPtr );
    
    /*
     * Platform specific parts.
     */
    

#if TARGET_API_MAC_CARBON	// Mac OS X
{
    CFStringRef 	cfString;
    char 			classicFilename[512] = "";
    
    /* 
     * We must handle both composed and decomposed utf format.
     * The Tcl encoding conversions and Tcl_UtfToExternalDString() don't
     * automatically handle Unicode composition.
     * Code snippet provided by Benjamin Riefenstahl. Many Thanks!
     */
    
    cfString = CFStringCreateWithCStringNoCopy(
					       NULL, normalizedPath, kCFStringEncodingUTF8, kCFAllocatorNull );
    CFStringGetCString(
		       cfString, classicFilename, sizeof(classicFilename) - 1,
		       GetScriptManagerVariable(smSysScript) );
    err = FSpLocationFromPath( strlen(classicFilename), classicFilename, fssPtr );
    CFRelease( cfString );
}
#endif      // TARGET_OS_MAC

#ifdef _WIN32
{
    int     srcRead, dstWrote;       
    char 	externalPath[512];
    
    Tcl_UtfToExternal( NULL, gQTTclTranslationEncoding, normalizedPath, 
		      strlen(normalizedPath), 0, NULL, externalPath, 511, &srcRead, &dstWrote, NULL );
    err = NativePathNameToFSSpec( externalPath, fssPtr, 0 );
}
#endif      // _WIN32

return err;
}

/*
 *-----------------------------------------------------------------------------
 *
 * QTTclFSSpecToNativePathName --
 *
 * 		Convert the 'FSSpec' into a file name.
 * 		There are two things to consider: UTF translation and Mac vs. Windows.
 *
 * Results:
 *		An standard Tcl error code. File path in 'pathname'. Be sure to
 *      allocate it before calling!
 *
 * Side effects:
 *		Leaves file path in interp.
 *
 *-----------------------------------------------------------------------------
 */

int
QTTclFSSpecToNativePathName( 
			    Tcl_Interp 	*interp, 
			    char 		*pathname, 
			    FSSpec 		*fssPtr )
{
    Tcl_DString     ds;
    
#if TARGET_OS_MAC && TARGET_API_MAC_CARBON		// Mac OS X
{
    Handle 	pathHandle = NULL;
    int		length;
    
    if (FSpPathFromLocation( fssPtr, &length, &pathHandle ) != noErr) {
	Tcl_SetObjResult( interp, Tcl_NewStringObj(
						   "Failed creating file pathname", -1 ) );
	return TCL_ERROR;
    }
    HLock(pathHandle);
    sprintf( pathname, "%s", (char *) *pathHandle );
    HUnlock( pathHandle );
    Tcl_SetObjResult( interp, Tcl_NewStringObj( pathname, -1 ) );
    DisposeHandle( pathHandle );
}
#endif

#if TARGET_OS_MAC && !TARGET_API_MAC_CARBON		// Mac Classic
{
Handle  pathHandle;
int     length;

pathHandle = NULL;
FSpPathFromLocation( fssPtr, &length, &pathHandle );
if (pathHandle == NULL) {
    Tcl_SetObjResult( interp, Tcl_NewStringObj(
					       "Failed creating file pathname", -1 ) );
    return TCL_ERROR; 		
}
HLock(pathHandle);
sprintf( pathname, "%s", (char *) *pathHandle );
HUnlock( pathHandle );
DisposeHandle( pathHandle );
}
#endif

#ifdef _WIN32
if (noErr != FSSpecToNativePathName( fssPtr, pathname, 255, kFullNativePath )) {
    Tcl_SetObjResult( interp, Tcl_NewStringObj(
					       "Failed creating file pathname", -1 ) );
    return TCL_ERROR; 		
}
#endif

Tcl_ExternalToUtfDString( gQTTclTranslationEncoding, pathname, -1, &ds );
Tcl_SetObjResult( interp, 
		 Tcl_NewStringObj( Tcl_DStringValue(&ds), -1 ) );
Tcl_DStringFree( &ds );		    

return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * QTTclMacWinBounds --
 *
 *		Given a Tk window this function determines the windows
 *		bounds in relation to the Macintosh window's coordinate
 *		system.  This is also the same coordinate system as the
 *		Tk toplevel window in which this window is contained.
 *
 * Results:
 *		None.
 *
 * Side effects:
 *		None.
 *
 *----------------------------------------------------------------------
 */

void
QTTclMacWinBounds(
		  TkWindow 	*winPtr,
		  Rect 		*bounds)
{
#if TARGET_API_MAC_CARBON
    TkMacOSXWinBounds( winPtr, bounds );
#endif
    
#ifdef _WIN32
    bounds->left = 0;
    bounds->top = 0;
    bounds->right = (short)  winPtr->changes.width;
    bounds->bottom = (short) winPtr->changes.height;
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * QTTclMacGetDrawablePort --
 *
 *		This function returns the Graphics Port for a given X drawable.
 *  	Beware, a very special routine to mimic Mac behaviour on Windows!
 *
 * Results:
 *		A GWorld pointer.  Either an off screen pixmap or a Window.
 *
 * Side effects:
 *		None.
 *
 *----------------------------------------------------------------------
 */

    GWorldPtr
    QTTclMacGetDrawablePort(
			    Drawable drawable)
    {
#if TARGET_API_MAC_CARBON
        return TkMacOSXGetDrawablePort( drawable );
#endif
	
#ifdef _WIN32
	TkWinDrawable   *winWin = (TkWinDrawable *) drawable;
  	CWindowPtr      qtmlPtr; /* Macintosh window pointer */
  	
	if (winWin == NULL) {
	    return NULL;
	}
	/* Convert to window pointer */
	qtmlPtr = (CGrafPtr) GetHWNDPort( TkWinGetHWND(winWin) );    
	if (qtmlPtr != NULL) {
	    return qtmlPtr;
	}
 	return NULL;
#endif      // _WIN32
}
	
	/*
	 *----------------------------------------------------------------------
	 *
	 * QTTclMacVisableClipRgn --
	 *
	 *		This function returnd the Macintosh cliping region for the 
	 *		given window.  A NULL Rgn means the window is not visable.
	 *
	 * Results:
	 *		The region.
	 *
	 * Side effects:
	 *		None.
	 *
	 *----------------------------------------------------------------------
	 */
#if TARGET_OS_MAC
	
	RgnHandle
	QTTclMacVisableClipRgn(
			       TkWindow *winPtr)
	{
#if TARGET_API_MAC_CARBON
	    return TkMacOSXVisableClipRgn( winPtr );
#else
	    return TkMacVisableClipRgn( winPtr );
#endif
	}
#endif      // TARGET_OS_MAC

	
	/*
	 *-----------------------------------------------------------------------------
	 *
	 * QTTclGetMacFontAttributes --
	 *
	 *		Takes a Tk_Font and gets the Mac font attributes faceNum, size, and style.
	 *      Note that the Mac font size is in pixels while the Tk_Font size is
	 *      in points.
	 *
	 * Results:
	 *		Sets tha Mac font attributes.
	 *
	 * Side effects:
	 *		None.
	 *
	 *-----------------------------------------------------------------------------
	 */
	
	void
	QTTclGetMacFontAttributes( 
				  Tcl_Interp 	*interp, 
				  Tk_Window 	tkwin, 
				  Tk_Font 	tkFont, 
				  short 		*faceNum, 
				  short 		*macSize, 
				  Style 		*style)
	{
	    const TkFontAttributes      *faPtr;
	    Str255                      pstr;
	    int                         srcRead, dstWrote;
	    int                         size;
	    
	    *faceNum = 0;
	    faPtr = GetFontAttributes(tkFont);
	    Tcl_UtfToExternal( interp, gQTTclTranslationEncoding, faPtr->family, strlen(faPtr->family),
			      0, NULL, StrBody(pstr), 255, &srcRead, &dstWrote, NULL );
	    pstr[0] = dstWrote;        
#if TARGET_API_MAC_CARBON
	    *faceNum = FMGetFontFamilyFromName( pstr );
#else
	    GetFNum( pstr, faceNum );
#endif    		  
	    if (faPtr->size == 0) {
#if TARGET_OS_MAC
		size = -GetDefFontSize();
#endif
#ifdef _WIN32
		
		/*
		 * Seems Apple didn't port this one. Make a reasonable guess.
		 */
		
		size = 10;
#endif  
	    } else {
		size = faPtr->size;
	    }
	    // seems to be a problem with exported symbols from 8.3.2
	    //*macSize = (short) TkFontGetPixels( tkwin, size );
	    *macSize = (size > 0) ? size : -size;
	    *style = 0;
	    if (faPtr->weight != TK_FW_NORMAL) {
		*style |= bold;
	    }
	    if (faPtr->slant != TK_FS_ROMAN) {
		*style |= italic;
	    }
	    if (faPtr->underline) {
		*style |= underline;
	    }
	}
	
	/*
	 *----------------------------------------------------------------------
	 *
	 * CheckAndSetErrorResult --
	 *
	 *		Is called when something unexpected happens a movie, and here 
	 *		we check if Apple can provide us with an error message. 
	 *		If found any we append this message to the Tcl interpreters 
	 *		result. If there is an error, we return an Apple OSErr error 
	 *		code. So far only Movie Toolbox errors are checked; add more 
	 *		later! If an nonzero myErr is given, then take this one since 
	 *		only Movie Toolbox errors are checked for in GetMoviesError.
	 *  	If have no error code put noErr in myErr.
	 *
	 * Results:
	 *		OSErr, Apple error code.
	 *
	 * Side effects:
	 *		May add message to the Tcl result.
	 *
	 *----------------------------------------------------------------------
	 */
	
	OSStatus
	CheckAndSetErrorResult( 
			       Tcl_Interp 	*interp, 
			       OSStatus 	myErr )
	{
	    OSStatus           	result = noErr;
	    
	    if (myErr == noErr) {
		if (noErr == (result = GetMoviesError())) {
		    return noErr;
		}
	    } else {
		result = myErr;
	    }    
	    Tcl_SetObjResult( interp, GetErrorObj( result ) );
	    return result;  
	}
	
	/*
	 *----------------------------------------------------------------------
	 *
	 * GetErrorObj --
	 *
	 *		Translates the Apple error code into a readable string object.
	 *
	 * Results:
	 *		Tcl_Obj describing error code.
	 *
	 * Side effects:
	 *		None.
	 *
	 *----------------------------------------------------------------------
	 */
	
	Tcl_Obj *
	GetErrorObj ( OSStatus 	err )
	{
	    long        ind;
	    Tcl_Obj      *errObj = NULL;
	    char        tmp[STR255LEN];
	    
	    /* Movie Toolbox codes are from -2000 to -2053 ; 
	     * translate to array index. */
	    ind = -(err + 2000);
	    if ((ind >= 0) && (ind <= 53)) {
		errObj = Tcl_NewStringObj(MovieResultCodes[ind], -1);
	    }  
	    
	    /* Image Compressor Manager codes are from -8960 to -8973 ; 
	     * translate to array index. */    
	    ind = -(err + 8960);
	    if ((ind >= 0) && (ind <= 13)) {
		errObj = Tcl_NewStringObj(ICMResultCodes[ind], -1);
	    }  
	    
	    /* Some url error codes. */
	    if ((err <= -2129) && (err >= -2148)) {
		ind = -(err + 2129);
		errObj = Tcl_NewStringObj(URLDataErrorCodes[ind], -1);
	    }  
	    
	    /* Miscellaneous error codes. */
	    if (errObj == NULL) {
		if (err == -43) {
		    errObj = Tcl_NewStringObj("File not found", -1);
		} else if (err == -50) {
		    errObj = Tcl_NewStringObj("Error in user parameter list", -1);
		} else if (err == -100) {
		    errObj = Tcl_NewStringObj("No scrap exists", -1);
		} else if (err == -102) {
		    errObj = Tcl_NewStringObj(
					      "Format not available [no object of that type in scrap]", -1);
		} else if (err == -108) {
		    errObj = Tcl_NewStringObj("Not enough memory available", -1);
		} else if (err == 10061) {
		    errObj = Tcl_NewStringObj("Could not connect to server", -1);
		}
	    }
	    if (errObj == NULL) {
		sprintf( tmp, "Apple error code %d", (int) err );
		errObj = Tcl_NewStringObj(tmp, -1);
	    }  
	    return errObj;
	}
	
	/*
	 *----------------------------------------------------------------------
	 *
	 * MySafeNewHandle --
	 *
	 *		Allocate a new block of memory free from the System. 
	 *		This is the safe way to use NewHandle; rip-off from tclMacAlloc.c 
	 *		after a suggestion of Daniel Steffen.
	 *
	 * Results:
	 *		Returns same as NewHandle.
	 *
	 * Side effects:
	 *		May obtain memory from app or sys space.
	 *
	 *----------------------------------------------------------------------
	 */
	
	Handle
	MySafeNewHandle( long size, int clear )
	{
	    Handle hand = NULL;
	    
	    hand = NewHandle( size );
	    if (clear) {
		HLock( hand );
		memset( (char *) *hand, 0, size );
		HUnlock( hand );
	    }
	    return hand;
	}
	
	/*
	 *----------------------------------------------------------------------
	 *
	 * MySafeNewGWorld --
	 *
	 *		Allocate a new GWorld. Keeps a toolbox space around; 
	 * 	 	rip-off from tclMacAlloc.c after a suggestion of Daniel Steffen.
	 *
	 * Results:
	 *		Returns same as NewGWorld.
	 *
	 * Side effects:
	 *		May obtain memory from app or temp space.
	 *
	 *----------------------------------------------------------------------
	 */
	
	OSErr
	MySafeNewGWorld( 
			GWorldPtr *offscreenGWorldHand, 
			short depth, 
			const Rect *bounds, 
			CTabHandle cTable, 
			GDHandle device, 
			GWorldFlags flags )
	{
	    OSErr       err = noErr;
	    
	    err = NewGWorld( offscreenGWorldHand, depth, bounds, cTable, device, 0 );
	    if (err != noErr) {
		err = NewGWorld( offscreenGWorldHand, depth, bounds, cTable, device, useTempMem );
	    }
	    if (err != noErr) {
		panic( "Out of memory: NewGWorld failed" );
	    }
	    return err;
	}
	
	/*
	 *-----------------------------------------------------------------------------
	 *
	 * SafeStrcmp --
	 *
	 *		Just a safe 'strcmp' that accepts NULL pointers.
	 *		A zero length string equals a NULL pointer.
	 *
	 * Results:
	 *		As 'strcmp' if no NULL pointers, 0 if both NULL, 0 if one NULL
	 *		and another zero length, else +1 or -1.
	 *
	 * Side effects:
	 *		None
	 *
	 *-----------------------------------------------------------------------------
	 */
	
	int 
	SafeStrcmp( const char *cs, const char *ct ) {
	    
	    if ((cs == NULL) && (ct == NULL)) {
		return 0;
	    } else if ((cs == NULL) && (ct[0] == '\0')) {
		return 0;
	    } else if ((cs[0] == '\0') && (ct == NULL)) {
		return 0;
	    } else if (cs == NULL) {
		return -1;		// could use 'strlen' here...
	    } else if (ct == NULL) {
		return 1;
	    } else {
		return strcmp( cs, ct );
	    }
	}
	
	/*
	 *-----------------------------------------------------------------------------
	 *
	 * SafeStrcpy --
	 *
	 *		Just a safe 'strcpy' that accepts NULL pointers.
	 *
	 * Results:
	 *		As 'strcpy' if no NULL pointers,
	 *
	 * Side effects:
	 *		None
	 *
	 *-----------------------------------------------------------------------------
	 */
	
	char * 
	SafeStrcpy( char *s, const char *ct ) {
	    
	    if ((s == NULL) && (ct == NULL)) {
		return NULL;
	    } else if (ct == NULL) {
		s[0] = '\0';
		return s;
	    } else {
		return strcpy( s, ct );
	    }
	}
	
	/*
	 *----------------------------------------------------------------------
	 *
	 * ConvertFloatToBigEndian --
	 *
	 *		Convert the specified floating-point number to big-endian format.
	 *
	 * Results:
	 *		None.
	 *
	 * Side effects:
	 *		None.
	 *
	 *----------------------------------------------------------------------
	 */
	
	void 
	ConvertFloatToBigEndian (float *theFloat)
	{
	    unsigned long		*longPtr;
	    
	    longPtr = (unsigned long *)theFloat;
	    *longPtr = EndianU32_NtoB(*longPtr);
	}
	
	void 
	ConvertBigEndianFloatToNative( float *theFloat )
	{
	    unsigned long		*myLongPtr;
	    
	    myLongPtr = (unsigned long *)theFloat;
	    *myLongPtr = EndianU32_BtoN(*myLongPtr);
	}
	
	int
	GetMovieStartTimeFromObj( Tcl_Interp *interp, Movie movie, Tcl_Obj *obj, long *timeValuePtr )
	{
	    int result = TCL_OK;
	    
	    if (strcmp(Tcl_GetString( obj ), "end") == 0) {
		*timeValuePtr = GetMovieDuration( movie );
	    } else if (Tcl_GetLongFromObj( interp, obj, timeValuePtr ) != TCL_OK) {
		Tcl_AddErrorInfo( interp, "\n	(processing time value)" );
		result = TCL_ERROR;
	    } else {
		result = TCL_OK;
	    }
	    return result;
	}
	
	int
	GetMovieDurationFromObj( Tcl_Interp *interp, Movie movie, Tcl_Obj *obj, long movTime, long *durValuePtr )
	{
	    int result = TCL_OK;
	    
	    if (strcmp(Tcl_GetString( obj ), "end") == 0) {
		*durValuePtr = GetMovieDuration( movie ) - movTime;
	    } else if (Tcl_GetLongFromObj( interp, obj, durValuePtr ) != TCL_OK) {
		Tcl_AddErrorInfo( interp, "\n	(processing duration value)" );
		result = TCL_ERROR;
	    } else {
		result = TCL_OK;
	    }
	    return result;
	}
	
	/*
	 *----------------------------------------------------------------------
	 *
	 * MyDebugStr, QTTclDebugPrintf --
	 *
	 *		Debugging aid.
	 *
	 * Results:
	 *		None.
	 *
	 * Side effects:
	 *		Printouts.
	 *
	 *----------------------------------------------------------------------
	 */
	
	void
	MyDebugStr( ConstStr255Param debuggerMsg )
	{
#if TARGET_OS_MAC
	    DebugStr( debuggerMsg );
#else
	    /* Do nothing */
#endif
	}
	
	void
	QTTclDebugPrintf( Tcl_Interp *interp, int level, char *fmt, ... )
	{
	    va_list		args;
	    char		tmpstr[256];
	    
	    if (level > gQTTclDebugLevel) {
		return;
	    }
	    va_start( args, fmt );
	    vsprintf( tmpstr, fmt, args );
	    if (interp != NULL) {
		Tcl_VarEval( interp, "puts \"", tmpstr, "\"", (char *) NULL );
	    }
	    va_end (args );
	    
	    if (gQTTclDebugLog) {
		if (gQTTclDebugChannel == NULL) {
		    gQTTclDebugChannel = Tcl_FSOpenFileChannel(interp, 
							       Tcl_NewStringObj("_QTTclDebug.txt", -1), "w", 420);
		}
		Tcl_Write(gQTTclDebugChannel, tmpstr, strlen(tmpstr));
		Tcl_Write(gQTTclDebugChannel, "\n", 1);
		Tcl_Flush(gQTTclDebugChannel);
	    }
	}
	
#if TARGET_OS_MAC
	/*
	 *----------------------------------------------------------------------
	 *
	 * GetMacSystemEncoding --
	 *
	 *		Gets the system encoding for Mac OS X/8/9. 
	 *		Copied from TkpInit in tkMacOSXInit.c
	 *
	 * Results:
	 *		Tcl_Encoding.
	 *
	 * Side effects:
	 *		None.
	 *
	 *----------------------------------------------------------------------
	 */
	
	Tcl_Encoding
	GetMacSystemEncoding( void )
	{
	    CFStringEncoding encoding;
	    char *encodingStr = NULL;
	    int  i;
	    Tcl_Encoding 	tclEncoding;
	    typedef struct Map {
		int numKey;
		char *strKey;
	    } Map;
	    
	    static Map scriptMap[] = {
		{smRoman,		"macRoman"},
		{smJapanese,	"macJapan"},
		{smTradChinese,	"macChinese"},
		{smKorean,		"macKorean"},
		{smArabic,		"macArabic"},
		{smHebrew,		"macHebrew"},
		{smGreek,		"macGreek"},
		{smCyrillic,	"macCyrillic"},
		{smRSymbol,		"macRSymbol"},
		{smDevanagari,	"macDevanagari"},
		{smGurmukhi,	"macGurmukhi"},
		{smGujarati,	"macGujarati"},
		{smOriya,		"macOriya"},
		{smBengali,		"macBengali"},
		{smTamil,		"macTamil"},
		{smTelugu,		"macTelugu"},
		{smKannada,		"macKannada"},
		{smMalayalam,	"macMalayalam"},
		{smSinhalese,	"macSinhalese"},
		{smBurmese,		"macBurmese"},
		{smKhmer,		"macKhmer"},
		{smThai,		"macThailand"},
		{smLaotian,		"macLaos"},
		{smGeorgian,	"macGeorgia"},
		{smArmenian,	"macArmenia"},
		{smSimpChinese,	"macSimpChinese"},
		{smTibetan,		"macTIbet"},
		{smMongolian,	"macMongolia"},
		{smGeez,		"macEthiopia"},
		{smEastEurRoman,	"macCentEuro"},
		{smVietnamese,	"macVietnam"},
		{smExtArabic,	"macSindhi"},
		{NULL,		NULL}
	    };
	    
	    encoding = CFStringGetSystemEncoding();
	    
	    for (i = 0; scriptMap[i].strKey != NULL; i++) {
		if (scriptMap[i].numKey == encoding) {
		    encodingStr = scriptMap[i].strKey;
		    break;
		}
	    }
	    if (encodingStr == NULL) {
		encodingStr = "macRoman";
	    }
	    
	    tclEncoding = Tcl_GetEncoding (NULL, encodingStr);
	    if (tclEncoding == NULL) {
		tclEncoding = Tcl_GetEncoding (NULL, NULL);
	    }
	    return tclEncoding;
	}
#endif
	
	/*---------------------------------------------------------------------------*/
