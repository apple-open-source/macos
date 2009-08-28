/*
 * QuickTimeTcl.c --
 *
 *		Main routine for the QuickTimeTcl package.
 *		It is part of the QuickTimeTcl package which provides Tcl/Tk bindings for QuickTime.
 *      Some parts from the Tkimg package.
 *
 * Copyright (c) 1998       Bruce O'Neel
 * Copyright (c) 2000-2005  Mats Bengtsson
 *
 * version: 3.1.0
 *
 * $Id: QuickTimeTcl.c,v 1.21 2008/02/26 13:40:47 matben Exp $
 */

#ifdef _WIN32
#   include "QuickTimeTclWin.h"
#endif  

#include "QuickTimeTcl.h"

Tcl_Encoding 	gQTTclTranslationEncoding;

/*
 * For dispatching canopen options.
 */

static char *allCanOpenOptions[] = {
	"-allowall", "-allownewfile", "-type",
    (char *) NULL
};

enum {
    kCanOpenOptionAllowAll                  = 0L,
    kCanOpenOptionAllowNewFile,
    kCanOpenOptionType
};

/*
 * Sets the debug level for printouts via QTTclDebugPrintf().
 * 0 : no printouts, > 0 depends in level in call.
 */
 
int gQTTclDebugLevel = 0;
int gQTTclDebugLog   = 0;

Tcl_Channel gQTTclDebugChannel = NULL;

/*
 * Various code from Tkimg used for base64 reading.
 */

typedef struct {
    Tcl_DString *buffer;/* pointer to dynamical string */
    char *data;			/* mmencoded source string */
    int c;				/* bits left over from previous char */
    int state;			/* decoder state (0-4 or IMG_DONE) */
    int length;			/* length of physical line already written */
} MFile;

static char base64_table[64] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
    'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
    'w', 'x', 'y', 'z', '0', '1', '2', '3',
    '4', '5', '6', '7', '8', '9', '+', '/'
};

#define IMG_SPECIAL	 (1<<8)
#define IMG_PAD		(IMG_SPECIAL+1)
#define IMG_SPACE	(IMG_SPECIAL+2)
#define IMG_BAD		(IMG_SPECIAL+3)
#define IMG_DONE	(IMG_SPECIAL+4)
#define IMG_CHAN    (IMG_SPECIAL+5)
#define IMG_STRING	(IMG_SPECIAL+6)


static int      FileMatchQuickTime( Tcl_Channel chan, const char *fileName,
		                Tcl_Obj *format, int *widthPtr, int *heightPtr, Tcl_Interp *interp );
static int 		StringMatchQuickTime( Tcl_Obj *data, Tcl_Obj *format, int *widthPtr,
                        int *heightPtr, Tcl_Interp *interp );
static int      FileReadQuickTime( Tcl_Interp *interp,
		                Tcl_Channel chan, const char *fileName, Tcl_Obj *format,
		                Tk_PhotoHandle imageHandle, int destX, int destY,
		                int width, int height, int srcX, int srcY );
static int 		StringReadQuickTime( Tcl_Interp *interp, Tcl_Obj *dataObj, Tcl_Obj *format,
                        Tk_PhotoHandle imageHandle, int destX, int destY,
                        int width, int height, int srcX, int srcY );
static int      FileWriteQuickTime( Tcl_Interp *interp,
		                const char *fileName, Tcl_Obj *format,
		                Tk_PhotoImageBlock *blockPtr );

static int      GetOpenFilePreviewObjCmd( ClientData clientData, 
                        Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[] );
static pascal Boolean   EventFilter( DialogPtr dialogPtr, 
                                EventRecord *eventStrucPtr, SInt16 *itemHit, void *doNotKnow );
#if TARGET_OS_MAC
extern OSErr    GetOneFileWithPreview( AEDesc *defaultLocation, short theNumTypes, 
                        OSTypePtr theTypeList, StringPtr title,
                        FSSpecPtr theFSSpecPtr, void *theFilterProc );
static int      HandleInitialDirectory( Tcl_Interp *interp, char *initialDir, 
                        FSSpec *dirSpec, AEDesc *dirDescPtr );
#endif

static int      CanOpenObjCmd( ClientData clientData, Tcl_Interp *interp, 
                        int objc, Tcl_Obj *CONST objv[] );
static int      DebugLevelObjCmd( ClientData clientData, Tcl_Interp *interp, 
                        int objc, Tcl_Obj *CONST objv[] );
static int		ImgReadInit( Tcl_Obj *data, int	c,  MFile *handle );
static int		ImgRead( MFile *handle, char *dst, int count );
static int		ImgGetc( MFile *handle );
static int 		char64( int c );


Tk_PhotoImageFormat tkImgFmtQuickTime = {
	"quicktime",			    						/* name of handler  */
	(Tk_ImageFileMatchProc *) FileMatchQuickTime,    	/* fileMatchProc    */
	(Tk_ImageStringMatchProc *) StringMatchQuickTime,	/* stringMatchProc  */
	(Tk_ImageFileReadProc *) FileReadQuickTime,        	/* fileReadProc     */
	/*(Tk_ImageStringReadProc *) StringReadQuickTime,*/		/* stringReadProc   */
	(Tk_ImageStringReadProc *) NULL,		/* stringReadProc   */
	(Tk_ImageFileWriteProc *) FileWriteQuickTime,      	/* fileWriteProc    */
	(Tk_ImageStringWriteProc *) NULL,                 	/* stringWriteProc  */
};

/*
 * "export" is a MetroWerks specific pragma.  It flags the linker that  
 * any symbols that are defined when this pragma is on will be exported 
 * to shared libraries that link with this library.
 */
 

#if TARGET_OS_MAC
#   pragma export on
    int Quicktimetcl_Init( Tcl_Interp *interp );
    int Quicktimetcl_SafeInit( Tcl_Interp *interp );
#   pragma export reset
#endif

#ifdef _WIN32
    BOOL APIENTRY
    DllMain( hInst, reason, reserved )
        HINSTANCE   hInst;		/* Library instance handle. */
        DWORD       reason;		/* Reason this function is being called. */
        LPVOID      reserved;	/* Not used. */
    {
        return TRUE;
    }
#endif


#if (TCL_MAJOR_VERSION <= 8) && (TCL_MINOR_VERSION <= 3)			    				
#   error "Sorry, no support for 8.3 or earlier anymore"
#endif

/*
 *----------------------------------------------------------------------
 *
 * Quicktimetcl_Init --
 *
 *		Initializer for the QuickTimeTcl package.
 *
 * Results:
 *		A standard Tcl result.
 *
 * Side Effects:
 *   	Tcl commands created
 *
 *----------------------------------------------------------------------
 */
#ifdef _WIN32
    __declspec(dllexport)
#endif

int 
Quicktimetcl_Init(
    Tcl_Interp *interp )		/* Tcl interpreter. */
{
 	long    version;
 	char	*tclRunVersion;
 	double	dtclRunVersion;
 	double	dtclBuildVersion;
        
#ifdef USE_TCL_STUBS
    if (Tcl_InitStubs( interp, "8.4", 0 ) == NULL) {
	    return TCL_ERROR;
    }
#endif
#ifdef USE_TK_STUBS
    if (Tk_InitStubs( interp, "8.4", 0 ) == NULL) {
		return TCL_ERROR;
    }
#endif

	/*
     * We now require version 8.4 since we use some Tcl_FS* functions.
	 */
	 
	tclRunVersion = Tcl_GetVar( interp, "tcl_version", 
			(TCL_GLOBAL_ONLY | TCL_LEAVE_ERR_MSG) );
	dtclRunVersion = atof( tclRunVersion );	
	dtclBuildVersion = atof( TCL_VERSION );
    if (dtclRunVersion < 8.4) {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( 
				"QuickTimeTcl requires tcl version 8.4 or later", -1 ));
		return TCL_ERROR;
	}

	/* 
	 * QuickTime Installed? Version?
	 */

#ifdef _WIN32

	/*
	 * An issue: the problem with movie region not following window if moved seems
	 * to be specific for 'InitializeQTML(0)'.
	 * If problems with this use 'InitializeQTML( kInitializeQTMLUseGDIFlag )' instead.
	 */

    if (noErr != InitializeQTML( 0 )) {
		Tcl_SetObjResult( interp,  
			    Tcl_NewStringObj( "Failed initialize the QuickTime Media Layer", -1 ));
		return TCL_ERROR;
	}
    if (noErr != InitializeQTVR()) {
		Tcl_SetObjResult( interp,  
			    Tcl_NewStringObj( "Failed initialize the QuickTime VR manager", -1 ));
		return TCL_ERROR;
	}
#endif
	if (noErr != Gestalt( gestaltQuickTimeVersion, &version )) {
		Tcl_SetObjResult( interp,  
			    Tcl_NewStringObj( "QuickTime is not installed", -1 ));
		return TCL_ERROR;
	}
	if (((version >> 16) & 0xffff) < MIN_QUICKTIME_VERSION) {
		char         	cvers[30];
	
		/*
		 * We are running QuickTime prior to MIN_QUICKTIME_VERSION. (0x0500)
		 */

	    sprintf(cvers, "%5.2f", (double) MIN_QUICKTIME_VERSION/ (double) 0x0100);
		Tcl_AppendStringsToObj( Tcl_GetObjResult( interp ), 
				"We require at least version ", cvers, " of QuickTime", (char *) NULL);
		return TCL_ERROR;
	}		 

#if TARGET_OS_MAC
#	if TARGET_API_MAC_CARBON
        gQTTclTranslationEncoding = GetMacSystemEncoding();
#   else
        gQTTclTranslationEncoding = NULL;
#   endif
#else
    gQTTclTranslationEncoding = NULL;
#endif

	/*
	 * Create namespace and add variables.
	 */

    Tcl_Eval( interp, "namespace eval ::quicktimetcl:: {}" );
    Tcl_SetVar( interp, "quicktimetcl::patchlevel", QTTCL_PATCH_LEVEL, TCL_GLOBAL_ONLY );
    Tcl_SetVar( interp, "quicktimetcl::version", QTTCL_VERSION, TCL_GLOBAL_ONLY );
    Tcl_CreateObjCommand( interp, "quicktimetcl::info", QuickTimeStat, 
    		(ClientData) NULL, (Tcl_CmdDeleteProc *) NULL );
    Tcl_CreateObjCommand( interp, "quicktimetcl::canopen", CanOpenObjCmd, 
    		(ClientData) NULL, (Tcl_CmdDeleteProc *) NULL );
    Tcl_CreateObjCommand( interp, "quicktimetcl::debuglevel", DebugLevelObjCmd, 
    		(ClientData) NULL, (Tcl_CmdDeleteProc *) NULL );

#if TARGET_API_MAC_CARBON
	Tcl_CreateObjCommand( interp, "quicktimetcl::systemui", MacControlUICmd, 
			(ClientData) NULL, NULL );
#endif

    Tcl_CreateObjCommand( interp, "QuickTimeStat", QuickTimeStat, (ClientData) NULL,
	        (Tcl_CmdDeleteProc *) NULL );
    Tcl_CreateObjCommand( interp, "Movie", MoviePlayerObjCmd, (ClientData) NULL,
	    	(Tcl_CmdDeleteProc *) NULL );
    Tcl_CreateObjCommand( interp, "movie", MoviePlayerObjCmd, (ClientData) NULL,
	    	(Tcl_CmdDeleteProc *) NULL );

	/*
	 * Preview open dialog.
	 */
	 
    Tcl_CreateObjCommand( interp, "tk_getOpenFilePreview", GetOpenFilePreviewObjCmd, 
    	    (ClientData) Tk_MainWindow(interp), (Tcl_CmdDeleteProc *) NULL );
    
	/*
	 * Sequence grabber widget.
	 */
	 
    Tcl_CreateObjCommand( interp, "seqgrabber", SeqGrabberObjCmd, 
	    (ClientData) NULL, NULL );

#if TARGET_OS_MAC
    Tcl_CreateObjCommand( interp, "qtbroadcast", BroadcastObjCmd, 
	    (ClientData) NULL, NULL );
#endif
	
    Tk_CreatePhotoImageFormat( &tkImgFmtQuickTime );

    /*
     * Link the ::quicktimetcl::debuglog variable to control debug log file. 
     */
    Tcl_EvalEx( interp, "namespace eval ::quicktimetcl {}", -1, TCL_EVAL_GLOBAL );
    if (Tcl_LinkVar( interp, "::quicktimetcl::debuglog",
            (char *) &gQTTclDebugLog, TCL_LINK_BOOLEAN ) != TCL_OK) {
        Tcl_ResetResult(interp);
    }
	
    return Tcl_PkgProvide( interp, "QuickTimeTcl", QTTCL_VERSION );
}

/*
 *----------------------------------------------------------------------
 *
 * Quicktimetcl_SafeInit --
 *
 *		This is just to provide a "safe" entry point (that is not safe!).
 *
 * Results:
 *		A standard Tcl result.
 *
 * Side Effects:
 *   	Tcl commands created
 *
 *----------------------------------------------------------------------
 */
#ifdef _WIN32
    __declspec(dllexport)
#endif

int 
Quicktimetcl_SafeInit(
    Tcl_Interp *interp )		/* Tcl interpreter. */
{
    return Quicktimetcl_Init( interp );
}

/*
 *---------------------------------------------------------------------- 
 *
 * QuickTimeStat
 *
 *		Implements the 'QuickTimeStat' command.
 * Results:
 *  	A standard Tcl result.
 *
 * Side effects:
 *  	Depends on the subcommand, see the user documentation
 *		for more details.
 *
 *----------------------------------------------------------------------
 */
 
int 
QuickTimeStat(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST objv[])
{
	OSErr                   err;
	long                    response;
	int                     iresponse;
	int                     i;
	char                    cvers[32];
	CodecNameSpecListPtr    codecs = NULL;
	Tcl_Obj                 *listObjPtr;
	Tcl_Obj                 *codecObjPtr;
	char      				tmpstr[STR255LEN];
	Component               videoCodec;
	ComponentDescription    videoCodecDesc;
	Handle                  compName = NULL;
	ComponentDescription    videoCodecInfo;
    QTAtomContainer         prefs = NULL;
    QTAtom                  prefsAtom;
    Ptr                     atomData = NULL;
    long                    dataSize;
    long                    connectSpeed;
	unsigned long			lType;
	Tcl_DString             ds;

	if ((objc <= 1) || (objc >= 4)) {
		Tcl_WrongNumArgs( interp, 1, objv, 
				"qtversion | icversion | iccodecs | components ?type? | connectspeed" );
	    return TCL_ERROR; 
	}
    if ((strcmp(Tcl_GetStringFromObj( objv[1], (int *) NULL), "QTversion" ) == 0) ||
            (strcmp(Tcl_GetStringFromObj( objv[1], (int *) NULL), "qtversion" ) == 0)) {
    
		if (objc >= 3) {
			Tcl_WrongNumArgs( interp, 2, objv, NULL );
		    return TCL_ERROR; 
		}
		err = Gestalt( gestaltQuickTimeVersion, &response );
		if (err == noErr) {
			iresponse = response;
	    	sprintf(cvers, "%x", iresponse);
	    	Tcl_SetObjResult( interp, Tcl_NewStringObj(cvers, -1) );
	    } else {
			Tcl_SetObjResult( interp, Tcl_NewStringObj("QuickTime is not installed", -1) );
			return TCL_ERROR;
		}
	} else if ((strcmp(Tcl_GetStringFromObj( objv[1], (int *) NULL), "ICversion") == 0) ||
            (strcmp(Tcl_GetStringFromObj( objv[1], (int *) NULL), "icversion") == 0)) {
    
		if (objc >= 3) {
			Tcl_WrongNumArgs( interp, 2, objv, NULL );
		    return TCL_ERROR; 
		}
		err = Gestalt( gestaltCompressionMgr, &response );
		if (err == noErr) {
			iresponse = response;
	    	sprintf(cvers, "%x",iresponse);
	    	Tcl_SetObjResult( interp, Tcl_NewStringObj(cvers, -1) );
	    } else {
			Tcl_SetObjResult( interp, Tcl_NewStringObj("Image Compressor is not installed", -1) );
			return TCL_ERROR;
		}
	} else if ((strcmp(Tcl_GetStringFromObj( objv[1], (int *) NULL), "ICcodecs" ) == 0) ||
            (strcmp(Tcl_GetStringFromObj( objv[1], (int *) NULL), "iccodecs" ) == 0)) {
	
		if (objc >= 3) {
			Tcl_WrongNumArgs( interp, 2, objv, NULL );
		    return TCL_ERROR; 
		}
		err = Gestalt( gestaltCompressionMgr, &response );
		if (err == noErr) {
			err = GetCodecNameList( &codecs, 1 );
			if (err != noErr) {
				Tcl_SetObjResult(interp, Tcl_NewStringObj("Can't get list of codecs", -1) );
				return TCL_ERROR;
			}
	    	listObjPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );
			for (i = 0; i < codecs->count; i++) {
		    	codecObjPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );
				memset( tmpstr, 0, STR255LEN );
		    	Tcl_ListObjAppendElement( interp, codecObjPtr, Tcl_NewStringObj("-subtype", -1) );		    	
				lType = EndianU32_BtoN( codecs->list[i].cType );
				memcpy( tmpstr, &lType, 4 );
		    	Tcl_ListObjAppendElement( interp, codecObjPtr, Tcl_NewStringObj(tmpstr, -1) );
				memcpy( tmpstr, &codecs->list[i].typeName, 4 );
#if TARGET_API_MAC_CARBON
				CopyPascalStringToC( (ConstStr255Param) tmpstr, tmpstr );
#else				
				p2cstr( (unsigned char *) tmpstr );
#endif			
				Tcl_ListObjAppendElement( interp, codecObjPtr, Tcl_NewStringObj("-name", -1) );
				Tcl_ListObjAppendElement( interp, codecObjPtr, Tcl_NewStringObj(tmpstr, -1) );				
		    	Tcl_ListObjAppendElement( interp, listObjPtr, codecObjPtr );
			}
		    Tcl_SetObjResult( interp, listObjPtr );
			DisposeCodecNameList(codecs);
	    } else {
			Tcl_SetObjResult(interp,  
				Tcl_NewStringObj("Image Compressor is not installed", -1) );
			return TCL_ERROR;
		}
	} else if ((strcmp(Tcl_GetStringFromObj( objv[1], (int *) NULL ), "Components") == 0) ||
            (strcmp(Tcl_GetStringFromObj( objv[1], (int *) NULL ), "components") == 0)) {
	
	    listObjPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );
		videoCodecDesc.componentType = 0;
	    if (objc == 3) {
			memcpy( &lType, Tcl_GetString( objv[2] ), 4 );
			videoCodecDesc.componentType = EndianU32_NtoB( lType );
		}
		videoCodecDesc.componentSubType = 0;
		videoCodecDesc.componentManufacturer = 0;
		videoCodecDesc.componentFlags = 0;
		videoCodecDesc.componentFlagsMask = 0;
		videoCodec = FindNextComponent(NULL, &videoCodecDesc);
		compName = NewHandle(255);
		
		while (videoCodec != NULL) {
			err = GetComponentInfo( videoCodec, &videoCodecInfo, compName, NULL, NULL );
			
			if (err == noErr) {
		    	codecObjPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );
				memset( tmpstr, 0, STR255LEN );
				
				Tcl_ListObjAppendElement( interp, codecObjPtr, 
				        Tcl_NewStringObj("-type", -1) );
				lType = EndianU32_BtoN( videoCodecInfo.componentType );
				memcpy( tmpstr, &lType, 4 );
				Tcl_ListObjAppendElement( interp, codecObjPtr, Tcl_NewStringObj(tmpstr, -1) );

				Tcl_ListObjAppendElement( interp, codecObjPtr, 
				        Tcl_NewStringObj("-subtype", -1) );
				lType = EndianU32_BtoN( videoCodecInfo.componentSubType );
				memcpy( tmpstr, &lType, 4 );
				Tcl_ListObjAppendElement( interp, codecObjPtr, Tcl_NewStringObj(tmpstr, -1) );

				Tcl_ListObjAppendElement( interp, codecObjPtr, 
				        Tcl_NewStringObj("-manufacture", -1) );
				lType = EndianU32_BtoN( videoCodecInfo.componentManufacturer );
				memcpy(tmpstr, &lType, 4);
				Tcl_ListObjAppendElement( interp, codecObjPtr, Tcl_NewStringObj(tmpstr, -1) );

				if (*compName) {
				
					/* If pointer NULL then there is no name for this thing. */
					
					HLock(compName);
					memset( tmpstr, 0, STR255LEN );
	    			Tcl_ListObjAppendElement( interp, codecObjPtr, 
	    			        Tcl_NewStringObj("-name", -1) );
					memcpy( tmpstr, *compName, *compName[0] + 1 );
#if TARGET_API_MAC_CARBON
    				CopyPascalStringToC( (ConstStr255Param) tmpstr, tmpstr );
#else				
    				p2cstr( (unsigned char *) tmpstr );
#endif			
            	    Tcl_ExternalToUtfDString( gQTTclTranslationEncoding, tmpstr, -1, &ds );
					Tcl_ListObjAppendElement( interp, codecObjPtr, 
							Tcl_NewStringObj(Tcl_DStringValue(&ds), -1) );
            	    Tcl_DStringFree(&ds);		    
					HUnlock(compName);
				}
		    	Tcl_ListObjAppendElement( interp, listObjPtr, codecObjPtr );
			}
			videoCodec = FindNextComponent( videoCodec, &videoCodecDesc );
		}
	    Tcl_SetObjResult( interp, listObjPtr );
	    DisposeHandle(compName);
    	
	} else if ((strcmp(Tcl_GetStringFromObj( objv[1], (int *) NULL ), "ConnectSpeed") == 0) ||
            (strcmp(Tcl_GetStringFromObj( objv[1], (int *) NULL ), "connectspeed") == 0)) {
	    
	    /*
	     * Get the preferred connection speed. Note no Endian swapping needed!
	     */
	    
		if (objc >= 3) {
			Tcl_WrongNumArgs( interp, 2, objv, NULL );
		    return TCL_ERROR; 
		}
	    err = GetQuickTimePreference( 'cspd', &prefs ); 
	    if (err == noErr) {
	        prefsAtom = QTFindChildByID( prefs, kParentAtomIsContainer,
	                'cspd', 1, NULL );
	        if (!prefsAtom) {
	            // Set default to 28.8 
	            connectSpeed = kDataRate288ModemRate;
	        } else {
	            err = QTGetAtomDataPtr( prefs, prefsAtom, &dataSize, &atomData );
	            if (dataSize != 4) {
	                // Wrong size; corrupt?
	                connectSpeed = kDataRate288ModemRate;
	            } else {
	                connectSpeed = *(long *) atomData;
	            }            
	        }
	    	sprintf( tmpstr, "%ld", connectSpeed );
	    	Tcl_SetObjResult( interp, Tcl_NewStringObj(tmpstr, -1) );
	        QTDisposeAtomContainer( prefs );
	    } else {
			Tcl_SetObjResult( interp,  
				    Tcl_NewStringObj( "Failed retrieving the connection speed", -1 ) );
			return TCL_ERROR;
	    }     
	} else {
    	Tcl_AppendStringsToObj( Tcl_GetObjResult(interp),
    		    "Unrecognized option: ",
    		    Tcl_GetStringFromObj(objv[1], (int *) NULL), (char *) NULL );
        return TCL_ERROR;
    }

    return TCL_OK;
}

/*
 *---------------------------------------------------------------------- 
 *
 * DebugLevelObjCmd
 *
 *		Gets or sets the debug level.
 *
 * Results:
 *  	A standard Tcl result.
 *
 * Side effects:
 *  	Switches print outs on/off.
 *
 *----------------------------------------------------------------------
 */
 
static int 
DebugLevelObjCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST objv[])
{
	int		result = TCL_OK;

	if (objc == 1) {
		Tcl_SetObjResult( interp, Tcl_NewIntObj( gQTTclDebugLevel ));
	} else if (objc == 2) {
		if (Tcl_GetIntFromObj( interp, objv[1], &gQTTclDebugLevel ) != TCL_OK) {
			result = TCL_ERROR;
		}
	} else {
		Tcl_WrongNumArgs( interp, 1, objv, "?debugLevel?" );
		result = TCL_ERROR;
	}
	return result;
}
/*
 *---------------------------------------------------------------------- 
 *
 * FileMatchQuickTime
 *
 * Results:
 *  	0 if the filename isn't going to be readable by QuickTime
 * 		1 if it is, in which case widhtPtr and heightPtr are set to
 *		the image width and height
 *
 * Side effects:
 *		Image is opened
 *
 *----------------------------------------------------------------------
 */

int      
FileMatchQuickTime( 
        Tcl_Channel 	chan, 
        const char 		*fileName,
        Tcl_Obj 		*format, 
        int 			*widthPtr, 
        int 			*heightPtr, 
        Tcl_Interp 		*interp )
{
	GraphicsImportComponent     gi;
	Rect                        bounds;
	FSSpec                      fss;
		
	/*
	 * Translate file name to FSSpec.
	 */
	 	
	if (noErr != QTTclNativePathNameToFSSpec( interp, fileName, &fss )) {
		return 0;
	}
	
	/* See if QuickTime can import the file */
	if (noErr != GetGraphicsImporterForFile( &fss, &gi)) {
		return 0;
	}
	
	/* Now get it's bounds */
	if (noErr != GraphicsImportGetNaturalBounds( gi, &bounds )) {
		CloseComponent( gi );
		return 0;
	}
	*widthPtr = bounds.right - bounds.left;
	*heightPtr = bounds.bottom - bounds.top;
	CloseComponent(gi);
	return 1;
}

/*
 *---------------------------------------------------------------------- 
 *
 * StringMatchQuickTime
 *
 * Results:
 *  	0 if the data isn't going to be readable by QuickTime
 * 		1 if it is, in which case widhtPtr and heightPtr are set to
 *		the image width and height
 *
 * Side effects:
 *		None
 *
 *----------------------------------------------------------------------
 */

int 
StringMatchQuickTime(
        Tcl_Obj 	*data,
        Tcl_Obj 	*format,
        int 		*widthPtr,
        int 		*heightPtr,
        Tcl_Interp 	*interp ) 
{
    MFile handle;
    
    /* unfinished! */
    return 0;
    if (!ImgReadInit( data, '\211', &handle )) {
        return 0;
    }

    return 0;
}	
        
/*
 *---------------------------------------------------------------------- 
 *
 * FileReadQuickTime
 *
 * Results:
 *  	A standard Tcl result.  If TCL_OK then image was sucessfuly read in
 *		and put into imageHandle
 *
 * Side effects:
 *		Image read in
 *
 *----------------------------------------------------------------------
 */

int      
FileReadQuickTime( Tcl_Interp *interp,
	    Tcl_Channel chan, const char *fileName, Tcl_Obj *format,
	    Tk_PhotoHandle imageHandle, int destX, int destY,
	    int width, int height, int srcX, int srcY )
{
	GraphicsImportComponent     gi = NULL;
	Rect                        bounds;
    Bool						hasAlpha = false;
	FSSpec                      fss;
    GWorldPtr                   gWorld = NULL;
	CGrafPtr 					saveWorld = NULL;
	GDHandle 					saveDevice = NULL;
    QDErr                       err = noErr;
    PixMapHandle                pm = NULL;
    ComponentResult				compRes = noErr;
    RGBColor					rgbOpColor;
    ImageDescriptionHandle		imageDesc = NULL;
    Tk_PhotoImageBlock          imageBlock;
	unsigned char 				*pixelPtr = NULL;
	unsigned char 				*photoPixelsPtr = NULL;
	short						drawsAllPixels = graphicsImporterDrawsAllPixels;
    long						graphicsMode;
    int							i, j;
    int                         result = TCL_OK;

	/*
	 * Translate file name to FSSpec.
	 */

	if (noErr != QTTclNativePathNameToFSSpec( interp, fileName, &fss )) {
		Tcl_SetObjResult( interp,  
			Tcl_NewStringObj( "Can't make a FSSpec from filename", -1 ) );
		return TCL_ERROR;
	}
	
	/* 
	 * Get the proper importer.
	 */
	 
	if (noErr != GetGraphicsImporterForFile( &fss, &gi )) {
		Tcl_SetObjResult( interp,  
			    Tcl_NewStringObj( "No image importer found", -1 ) );
		result = TCL_ERROR;	
		goto bail;
	}

	/* Set the bounds. */
	bounds.top = srcY;
	bounds.bottom = srcY + height;
	bounds.left = srcX;
	bounds.right = srcX + width;
	
	/* Defines the rectangle in which to draw an image, the dest rect. */
	
	if (noErr != GraphicsImportSetBoundsRect( gi, &bounds )) {
		Tcl_SetObjResult( interp,  
		    	Tcl_NewStringObj( "Can't set image bounds", -1 ) );
		result = TCL_ERROR;	
		goto bail;
	}
	
	/* Defines the source rectangle of the image identical to dest rect. */
	
	if (noErr != GraphicsImportSetSourceRect( gi, &bounds )) {
		Tcl_SetObjResult( interp,  
		    	Tcl_NewStringObj( "Can't set image bounds", -1 ) );
		result = TCL_ERROR;	
		goto bail;
	}
	
	/* 
	 * Get a new GWorld to draw into.
	 */
	
    err = MySafeNewGWorld( &gWorld, 32, &bounds, NULL, NULL, 0 );
    if (err != noErr) {
        CheckAndSetErrorResult( interp, err );
		result = TCL_ERROR;
		goto bail;
    }
	GetGWorld( &saveWorld, &saveDevice );	
	SetGWorld( gWorld, NULL );

	if (noErr != GraphicsImportSetGWorld( gi, gWorld, nil )) {
		Tcl_SetObjResult( interp,  
    			Tcl_NewStringObj("Can't set GWorld", -1) );
		result = TCL_ERROR;	
		goto bail;
	}
	
    /*
     * Lock down the pixels so they don't move out from under us.
     */
     
    pm = GetGWorldPixMap( gWorld );
    LockPixels( pm );

    imageBlock.pixelPtr = (unsigned char *) GetPixBaseAddr( pm );
    if (imageBlock.pixelPtr == NULL) {
		Tcl_SetObjResult( interp,  
    			Tcl_NewStringObj( "GetPixBaseAddr failed. Likely out of memory.", -1 ) );
  		result = TCL_ERROR;	
		goto bail;
    }
    imageBlock.width = width;
    imageBlock.height = height;
#if TARGET_API_MAC_CARBON
    imageBlock.pitch = GetPixRowBytes( pm );
#else
    imageBlock.pitch = 0x3FFF & ((*pm)->rowBytes);
#endif
    imageBlock.pixelSize = 4;
    
    /*
     * Erase should fill each pixel with 00FFFFFF, which has the wrong 1st byte since
     * 00 means completely transparent (FF is opaque).
     */
    
#if TARGET_API_MAC_CARBON
    EraseRect( &bounds );
#else
    EraseRect( &gWorld->portRect );
#endif
    if (noErr != GraphicsImportGetGraphicsMode( gi, &graphicsMode, &rgbOpColor )) {
		result = TCL_ERROR;	
    	goto bail;
    }
    
    /*
     * Try to figure out if there is an original alpha channel.
     */
     
    if (noErr != GraphicsImportGetImageDescription( gi, &imageDesc )) {
		result = TCL_ERROR;	
    	goto bail;
    }
    // We need something else for Carbon here...
    if ((**imageDesc).depth == 32) {
    	hasAlpha = true;
    } else {
        compRes = GraphicsImportDoesDrawAllPixels( gi, &drawsAllPixels );
        if ((noErr == compRes) && (drawsAllPixels == graphicsImporterDoesntDrawAllPixels)) {
        	hasAlpha = true;
        }
    }
    
    /* 
     * The Mac pixmap stores them as "undefined (0), red, gree, blue", 
     * but tk 8.3 stores them as "red, green, blue, alpha (transparency)".
     * If we have an alpha channel in the original image, this is written
     * in the first byte.
     */

    imageBlock.offset[0] = 1;
    imageBlock.offset[1] = 2;
    imageBlock.offset[2] = 3;
  	imageBlock.offset[3] = 0;		
	
	/* Import the file. */
	
	if (noErr != GraphicsImportDraw( gi )) {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( "Can't import image", -1 ) );
		result = TCL_ERROR;	
        goto bail;
	}
    
    if (!hasAlpha) {
    
	    /*
	     * Problem with transparency: the upper 8 bits in the 32 bit offscreen GWorld
	     * doesn't correspond to an alpha channel, but is undefined. Since its content
	     * seems to be 0, which by tk is interpreted as completely transparent, we need
	     * to set it to 255, completely opaque.
	     */
     
	    for (i = 0; i < height; i++) {
			photoPixelsPtr = imageBlock.pixelPtr + i * imageBlock.pitch;
			pixelPtr = photoPixelsPtr;
			for (j = 0; j < width; j++) {
				photoPixelsPtr[0] = 0xFF;
				photoPixelsPtr += imageBlock.pixelSize;
	    	}
	    }
	}

    /* The image is constructed from the photo block. */
    Tk_PhotoPutBlock(imageHandle, &imageBlock, 
	    destX, destY, width, height, TK_PHOTO_COMPOSITE_SET );
	
bail:

	SetGWorld( saveWorld, saveDevice );
    UnlockPixels( pm );
	if (gWorld != NULL) {
		DisposeGWorld( gWorld );
	}
	if (gi != NULL) {
		CloseComponent( gi );
	}
	return result;
}

/*
 *---------------------------------------------------------------------- 
 *
 * StringReadQuickTime
 *
 * Results:
 *  	A standard Tcl result.  If TCL_OK then image was sucessfuly read in
 *		and put into imageHandle
 *
 * Side effects:
 *		Image read in
 *
 *----------------------------------------------------------------------
 */

int 
StringReadQuickTime(
        Tcl_Interp 		*interp,
        Tcl_Obj 		*dataObj,
        Tcl_Obj 		*format,
        Tk_PhotoHandle 	imageHandle,
        int destX, int destY,
        int width, int height,
        int srcX, int srcY)
{
    MFile 					handle;
    DataReferenceRecord     dataRef;
    Handle					myHandle = NULL;
    Handle					myDataRef = NULL;
    ComponentInstance 		gi;
    OSErr					err = noErr;
    int						result = TCL_ERROR;

    return TCL_ERROR;
    
    /* Prepare reading */ /* We could use the code from Img to identify handlers */
    ImgReadInit( dataObj, '\211', &handle );

    /* Read base64 data and decode into binary */
    

    /* Create a data handle reference. */
    myHandle = NewHandleClear(0);
    PtrToHand( &myHandle, &myDataRef, sizeof(Handle) );
    dataRef.dataRefType = HandleDataHandlerSubType;
    dataRef.dataRef = myDataRef;
    
    err = GetGraphicsImporterForDataRef( myDataRef, HandleDataHandlerSubType, &gi );
    if (err == noErr) {
        result = TCL_OK;
    }
    return result;
}

/*
 *---------------------------------------------------------------------- 
 *
 * FileWriteQuickTime
 *
 * 		Uses QuickTime graphics exporter to write image to file.
 *		In case no explicit format specified uses a graphics importer
 *		to export vis dialog.
 *
 * Results:
 *  	A standard Tcl result.  If TCL_OK then image was sucessfuly written
 *
 * Side effects:
 *		Image written
 *
 *----------------------------------------------------------------------
 */

int      
FileWriteQuickTime( Tcl_Interp *interp,
		    const char *fileName, 			/* File name where to store image. */
            Tcl_Obj *formatObj,				/* Any -format option, or NULL! */
	        Tk_PhotoImageBlock *blockPtr )
{
    int					numSubFormats = 0;
	int                 showDialog = 0;
    int					useGImporterWithDialog = 0;
    int 				argc;
	int         	    i;
	int         	    j;
	int                 pitch;
    Handle      	    h = NULL;
    PicHandle   	    thePicture = NULL;
	GWorldPtr   	    gw = NULL;
	Rect        	    r;
	OSType      	    fileType = 0;
	FSSpec      	    fss;
	CGrafPtr    	    saveGW = NULL;
	GDHandle    	    saveGD = NULL;
    GraphicsExportComponent ge = 0;
	GraphicsImportComponent gi = 0;
	PixMapHandle 	    pm = NULL;
	ModalFilterYDUPP    eventFilterProcUPP = NULL;
	const char          unrecognizedFormat[] = "Unrecognized format: try \
quicktimepict, quicktimequicktimeimage, quicktimebmp, quicktimejpeg, \
quicktimephotoshop, quicktimepng, quicktimetiff, quicktimesgiimage \
quicktimejfif, quicktimemacpaint, quicktimetargaimage ?-dialog?, or {quicktime -dialog}";
    typedef struct { 
        char     	*subFormatName;
        OSType      osType;
    } MapperNameToOSType;
    /* Not sure that all of these actually have exporters. */
    MapperNameToOSType nameToOSType[] = {
        {"pict",               kQTFileTypePicture},
        {"quicktimeimage",     kQTFileTypeQuickTimeImage},
        {"bmp",                kQTFileTypeBMP},
        {"jpeg",               kQTFileTypeJPEG},
        {"photoshop",          kQTFileTypePhotoShop},
        {"dvc",                kQTFileTypeDVC},
        {"movie",              kQTFileTypeMovie},
        {"pics",               kQTFileTypePICS},
        {"png",                kQTFileTypePNG},
        {"tiff",               kQTFileTypeTIFF},
        {"sgiimage",           kQTFileTypeSGIImage},
        {"jfif",               kQTFileTypeJFIF},
        {"macpaint",           kQTFileTypeMacPaint},
        {"targaimage",         kQTFileTypeTargaImage},
        {"quickdrawgxpicture", kQTFileTypeQuickDrawGXPicture},
        {"3dmf",               kQTFileType3DMF},
        {"flc",                kQTFileTypeFLC},
        {"flash",              kQTFileTypeFlash},
        {"flashpix",           kQTFileTypeFlashPix},
        {NULL, 0}};
	unsigned char       *pixBaseAddr;
	unsigned char 	    *pixelPtr;
	unsigned char 	    *photoPixelsPtr;
	char        	    *formatPtr;
    char 				**argv = NULL;
	OSErr       	    err = noErr;
	ComponentResult     compErr = noErr;
	int                 result = TCL_OK;
	
    if (Tcl_IsSafe( interp )) {
		Tcl_SetObjResult( interp, Tcl_NewStringObj(
				"imageName \"write\" not allowed in a safe interpreter", -1 ) );
		return TCL_ERROR;
    }
    
    if (formatObj == NULL) {
        return TCL_ERROR;
    } else {
        formatPtr = Tcl_GetStringFromObj( formatObj, (int *) NULL );
    }
    if (strncmp("quicktime", formatPtr, strlen("quicktime")) != 0) {
		Tcl_SetObjResult( interp,  
			Tcl_NewStringObj( unrecognizedFormat, -1 ) );
		return TCL_ERROR;
	}    
    if (TCL_OK != Tcl_SplitList( interp, formatPtr, &argc, &argv )) {
		return TCL_ERROR;
	}
    if (argc > 2) {
		Tcl_SetObjResult( interp,  
			Tcl_NewStringObj( unrecognizedFormat, -1 ) );
		return TCL_ERROR;
	}
    
    /* 
     * The first format argument must match the format specifier,
     * or "quicktime" which implies that we must have -dialog as well.
     */    
    
    if (strcmp("quicktime", argv[0]) == 0) {
        if (strcmp("-dialog", argv[1]) == 0) {
            useGImporterWithDialog = 1;
        } else {
            Tcl_SetObjResult( interp,  
                Tcl_NewStringObj( unrecognizedFormat, -1 ) );
            result = TCL_ERROR;
            goto bail;
        }
    } else {
        formatPtr = argv[0];
        formatPtr += strlen("quicktime");        
        numSubFormats = sizeof(nameToOSType) / sizeof(MapperNameToOSType);
        i = 0;
        while (nameToOSType[i].subFormatName != NULL) {
            if (strcmp(nameToOSType[i].subFormatName, formatPtr) == 0) {
                fileType = nameToOSType[i].osType;
                break;
            }
            i++;
        }	
        if (i >= numSubFormats - 1) {
            Tcl_SetObjResult( interp,  
                    Tcl_NewStringObj( unrecognizedFormat, -1 ) );
            result = TCL_ERROR;
            goto bail;
        }
        if (argc == 2) {
            if (strcmp("-dialog", argv[1]) == 0) {
                showDialog = 1;
            } else {
                Tcl_SetObjResult( interp,  
                    Tcl_NewStringObj( unrecognizedFormat, -1 ) );
                result = TCL_ERROR;
                goto bail;
            }
        }
    }
	
	/*
	 * Translate file name to FSSpec.
	 */

    err = QTTclNativePathNameToFSSpec( interp, fileName, &fss );
    if ((err != fnfErr) && (err != noErr)) {
		Tcl_SetObjResult( interp,  
				Tcl_NewStringObj( "Can't make a FSSpec from filename", -1 ) );
        result = TCL_ERROR;
        goto bail;
	}
	GetGWorld( &saveGW, &saveGD );

	r.top = 0;
	r.left = 0;
	r.right = blockPtr->width;
	r.bottom = blockPtr->height;
	
	/* Get a new GWorld to draw into. */
    err = MySafeNewGWorld( &gw, 32, &r, NULL, NULL, 0 );
	if (err != noErr) {
        CheckAndSetErrorResult( interp, err );
    	result = TCL_ERROR;
   		goto bail;
    }
	SetGWorld( gw, nil );
	    
    /*
     * Lock down the pixels so they don't move out from under us.
     */
     
    pm = GetGWorldPixMap(gw);
    LockPixels( pm );
    pixBaseAddr = (unsigned char *) GetPixBaseAddr( pm );
#if TARGET_API_MAC_CARBON
    pitch = GetPixRowBytes( pm );
#else
    pitch = 0x3FFF & ((*pm)->rowBytes);
#endif

    /* 
     * Copy the pixels to the GWorld.
     * The Mac pixmap stores them as "dummy, red, gree, blue", but tk 8.3 stores them
     * as "red, green, blue, alpha (transparency)". Alpha not working.
     */
     
	for (i = 0; i < blockPtr->height; i++) {
		pixelPtr = pixBaseAddr + i * pitch;
		photoPixelsPtr = blockPtr->pixelPtr + i * blockPtr->pitch;
		for (j = 0; j < blockPtr->width; j++) {
			*pixelPtr = *(photoPixelsPtr + blockPtr->offset[3]); pixelPtr++;
			*pixelPtr = *(photoPixelsPtr + blockPtr->offset[0]); pixelPtr++;
			*pixelPtr = *(photoPixelsPtr + blockPtr->offset[1]); pixelPtr++;
			*pixelPtr = *(photoPixelsPtr + blockPtr->offset[2]); pixelPtr++;
			photoPixelsPtr += blockPtr->pixelSize;
		}
	}
    
    /*
     * Now is the question, a direct graphics exporter or using an
     * importer with dialog if no explicit format given to us.
     */
     
    if (useGImporterWithDialog) {
        Tcl_Obj				*listObjPtr;
        ScriptCode  	    filescriptcode = smSystemScript;
        FSSpec      	    fssOut;
    
        /* Capture the gworlds contents in a picture handle. Alpha not handled. */
	
        thePicture = OpenPicture( &r );
#if TARGET_API_MAC_CARBON
        CopyBits( GetPortBitMapForCopyBits( gw ),
                GetPortBitMapForCopyBits( gw ),
                &r, &r, srcCopy, nil );
#else
        CopyBits( &((GrafPtr)gw)->portBits,
                &((GrafPtr)gw)->portBits,
                &r, &r, srcCopy, nil );
#endif	
        ClosePicture();
            
        /* 
         * Convert the picture handle into a PICT file (still in a handle ) 
         * by adding a 512-byte header to the start.
         */
        
        h = NewHandleClear(512);
        err = MemError();
        if (err) {
            result = TCL_ERROR;
            goto bail;
        }
        err = HandAndHand( (Handle) thePicture, h );        
        err = OpenADefaultComponent( GraphicsImporterComponentType, 
                kQTFileTypePicture, &gi );
        if (err) {
            Tcl_SetObjResult( interp,  
                    Tcl_NewStringObj( "No image importer found for PICT files", -1 ) );
            result = TCL_ERROR;
            goto bail;
        }        
        compErr = GraphicsImportSetDataHandle( gi, h );
        if (compErr) {
            Tcl_SetObjResult( interp,  
                    Tcl_NewStringObj( "Error setting import handler", -1 ) );
            result = TCL_ERROR;
            goto bail;
        }
        
        /* Important! */
        SetGWorld( saveGW, saveGD );

#if TARGET_API_MAC_CARBON
        eventFilterProcUPP = NewModalFilterYDUPP( EventFilter );
#else
        eventFilterProcUPP = NewModalFilterYDProc( EventFilter );
#endif
        compErr = GraphicsImportDoExportImageFileDialog(
                gi,                     // component instance
                &fss,                   // suggesting name of file
                NULL,                   // use default prompt "Save As"
                eventFilterProcUPP,     // event filter function; not working; 2nd dialog?
                &fileType,              // exported file type
                &fssOut,                // user selected file specifier
                &filescriptcode );      // script system
#if TARGET_API_MAC_CARBON
        DisposeModalFilterYDUPP( eventFilterProcUPP );
#else
        DisposeRoutineDescriptor( eventFilterProcUPP );
#endif
        if (compErr == userCanceledErr) {
        
            /* User canceled. */
            listObjPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );
            Tcl_ListObjAppendElement( interp, listObjPtr, 
                    Tcl_NewStringObj("0", -1) );
            Tcl_ListObjAppendElement( interp, listObjPtr, 
                    Tcl_NewStringObj("User canceled", -1) );
            Tcl_SetObjResult( interp, listObjPtr );
        } else if (compErr != noErr) {
            CheckAndSetErrorResult( interp, compErr );
    	    result = TCL_ERROR;
       		goto bail;
        } else { 
            char		pathName[255];
            
            result = QTTclFSSpecToNativePathName( interp, pathName, &fssOut );

            /* User picked another file. Should we signal this by throwing an error? */
            Tcl_SetObjResult( interp, Tcl_NewStringObj( pathName, -1 ) );
        }
    } else {

        /*
        * Find appropriate graphics export component.
        */
        
        err = OpenADefaultComponent( GraphicsExporterComponentType, fileType, &ge );
        if (err != noErr) {
            CheckAndSetErrorResult( interp, err );
            result = TCL_ERROR;
            goto bail;
        }
        if (0 && showDialog) {
            /* Seems not to work... */
            compErr = CallComponentCanDo( ge, kGraphicsExportRequestSettingsSelect );
            if (compErr != noErr) {
                Tcl_SetObjResult( interp, Tcl_NewStringObj( 
                        "The chosen export format does not support dialogs", -1 ) );
                result = TCL_ERROR;
                goto bail;
            }
        }
        
        /* Export options. Ignore errors. */
        GraphicsExportSetCompressionQuality( ge, codecMaxQuality );
        
        compErr = GraphicsExportSetInputPixmap( ge, pm );
        if (compErr != noErr) {
            CheckAndSetErrorResult( interp, compErr );
            result = TCL_ERROR;
            goto bail;
        }
            
        /* Defines the output file for a graphics export operation. */
        compErr = GraphicsExportSetOutputFile( ge, &fss );
        if (compErr != noErr) {
            CheckAndSetErrorResult( interp, compErr );
            result = TCL_ERROR;
            goto bail;
        }
	
        /* 
         * Be very careful to reset the GWorld before calling the dialog,
         * else it will be completely blank!
         * Thanks to Tom Dowdy at Apple for this one!
         */
        
        SetGWorld( saveGW, saveGD );
        
        if (showDialog) { 
#if TARGET_API_MAC_CARBON
            eventFilterProcUPP = NewModalFilterYDUPP( EventFilter );
#else
            eventFilterProcUPP = NewModalFilterYDProc( EventFilter );
#endif
            compErr = GraphicsExportRequestSettings( ge, eventFilterProcUPP, NULL );
#if TARGET_API_MAC_CARBON
            DisposeModalFilterYDUPP( eventFilterProcUPP );
#else
            DisposeRoutineDescriptor( eventFilterProcUPP );
#endif
            if (compErr != noErr) {
                CheckAndSetErrorResult( interp, compErr );
                result = TCL_ERROR;
                goto bail;
            }
        }
        compErr = GraphicsExportDoExport( ge, nil );
        if (compErr != noErr) {
            CheckAndSetErrorResult( interp, compErr );
            result = TCL_ERROR;
            goto bail;
        }
    }
    
bail:
    UnlockPixels( pm );
	SetGWorld( saveGW, saveGD );
    if (argv != NULL) {
        Tcl_Free( (char *) argv );
    }
	if (ge != NULL) {
		CloseComponent( ge );
	}
	if (gi != NULL) {
		CloseComponent( gi );
	}
	if (thePicture != NULL) {
		KillPicture( thePicture );
	}
	if (h != NULL) {
		DisposeHandle( h );
	}
	if (gw != NULL) {
		DisposeGWorld( gw );
	}
	return result;
}

/*
 *----------------------------------------------------------------------
 *
 * GetOpenFilePreviewObjCmd --
 *
 *		Calls the QuickTime file open dialog for the user to choose a
 *		movie file to open.
 *
 * Results:
 *		A standard Tcl result.
 *
 * Side effects:
 *		If the user selects a file, the native pathname of the file
 *		is returned in the interp's result. Otherwise an empty string
 *		is returned in the interp's result.
 *
 *----------------------------------------------------------------------
 */

int
GetOpenFilePreviewObjCmd(
    ClientData clientData,	/* Main window associated with interpreter. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,				/* Number of arguments. */
    Tcl_Obj *CONST objv[])	/* Argument objects. */
{
    OSType                      typeList = kQTFileTypeMovie;
    FSSpec                      theFSSpec;
    Boolean						sfGood = false;
	OSErr                       err = noErr;
#if TARGET_OS_MAC	 
    AEDesc                      initialDesc = {typeNull, NULL};
    Str255 						title = "\p";
#endif
	char						pathname[256];
	Tcl_Obj						*resultObjPtr = NULL;
	int                         result = TCL_OK;
	
	/* A few of the file types QuickTime can open. */
	OSType          typeListPtr[] = {FOUR_CHAR_CODE('MooV'), FOUR_CHAR_CODE('TEXT'),
	                                FOUR_CHAR_CODE('PICT'), FOUR_CHAR_CODE('JPEG'),
	                                FOUR_CHAR_CODE('PNGf'), FOUR_CHAR_CODE('PNG '),
	                                FOUR_CHAR_CODE('TIFF'), FOUR_CHAR_CODE('GIFf'),
	                                FOUR_CHAR_CODE('PLAY'), FOUR_CHAR_CODE('WAVE'),
	                                FOUR_CHAR_CODE('SWFL'), FOUR_CHAR_CODE('SWF '),
	                                FOUR_CHAR_CODE('MPEG'), FOUR_CHAR_CODE('MP3 '),
	                                FOUR_CHAR_CODE('ULAW'), FOUR_CHAR_CODE('WAV '),
	                                FOUR_CHAR_CODE('AIFF'), FOUR_CHAR_CODE('AIFC'),
	                                FOUR_CHAR_CODE('Midi'), FOUR_CHAR_CODE('BMP ')
	                                };
	
	/* 
	 * Just adds the usual options for a possible future implementation.
	 */
	 
    static char *openOptionStrings[] = {
	    "-defaultextension", "-filetypes", 
	    "-initialdir", "-initialfile", "-title", NULL
    };
    enum openOptions {
	    OPEN_DEFAULT, OPEN_TYPES,	
	    OPEN_INITDIR, OPEN_INITFILE, OPEN_TITLE
    };

#if TARGET_OS_MAC
    {
        int     i;

        for (i = 1; i < objc; i += 2) {
            char    *choice;
        	int     index, choiceLen;
            int 	srcRead, dstWrote;
            FSSpec  dirSpec;

        	if (Tcl_GetIndexFromObj( interp, objv[i], openOptionStrings, "option",
        		    TCL_EXACT, &index ) != TCL_OK) {
        	    result = TCL_ERROR;
        	    goto end;
        	}
        	if (i + 1 == objc) {
        		resultObjPtr = Tcl_GetObjResult( interp );
        		Tcl_AppendStringsToObj( resultObjPtr, "value for \"",
        				Tcl_GetString(objv[i]), "\"missing", (char *) NULL );
        	    result = TCL_ERROR;
        	    goto end;
        	}

        	switch (index) {
#if !TARGET_API_MAC_CARBON		// Classic
        	    case OPEN_INITDIR:
                    choice = Tcl_GetStringFromObj(objv[i + 1], NULL);
                    if (HandleInitialDirectory( interp, choice, &dirSpec, &initialDesc ) 
                            != TCL_OK) {
                        result = TCL_ERROR;
                        goto end;
                    }
        	        break;
#endif 
                case OPEN_TITLE:
                    choice = Tcl_GetStringFromObj(objv[i + 1], &choiceLen);
                    Tcl_UtfToExternal(NULL, gQTTclTranslationEncoding, choice, choiceLen, 
                            0, NULL, StrBody(title), 255, 
                            &srcRead, &dstWrote, NULL);
                    title[0] = dstWrote;
                    break;
            }
        }
    }
#endif
    
	/* 
	 * Open standard preview dialog for movies and other QT files. -1 lists all!
	 */
	 
#if TARGET_OS_MAC	 
#if TARGET_API_MAC_CARBON	// Mac OS X
    err = GetOneFileWithPreview( &initialDesc, 20, typeListPtr, title, &theFSSpec, NULL );
    if (err == noErr) {
        sfGood = true;
    }
#else	// Classic
	if (TkMacHaveAppearance() && NavServicesAvailable()) {
    	err = GetOneFileWithPreview( &initialDesc, 20, typeListPtr, title, &theFSSpec, NULL );
    	if (err == noErr) {
            sfGood = true;
    	}
    } else {
        SFTypeList 					types = {MovieFileType, 0, 0, 0};
        StandardFileReply			reply;
        
	    StandardGetFilePreview( NULL, -1, types, &reply );
	    theFSSpec = reply.sfFile;
	}
#endif
#endif  // TARGET_OS_MAC

#ifdef _WIN32
    {
        SFTypeList 					types = {MovieFileType, 0, 0, 0};
        StandardFileReply			reply;

        StandardGetFilePreview( NULL, -1, types, &reply );
        theFSSpec = reply.sfFile;
        sfGood = reply.sfGood;
    }
#endif  // _WIN32

    if ((err == noErr) && sfGood) {
    
		/* 
		 * Translate mac file system specification to path name.
		 */
		 
		result = QTTclFSSpecToNativePathName( interp, pathname, &theFSSpec );
	} else {
	
	    /* Cancel button pressed. */
	    Tcl_SetObjResult( interp, Tcl_NewStringObj("", -1) );
	}

#if TARGET_OS_MAC

end:
    AEDisposeDesc( &initialDesc );
#endif
	return result;
}

/*
 *-----------------------------------------------------------------------------
 *
 * EventFilter --
 *
 *		Callback for movable alert dialog.
 *
 * Results:
 *		A standard Tcl result.
 *
 * Side effects:
 *		Update events to background windows handled.
 *
 *-----------------------------------------------------------------------------
 */

pascal Boolean 
EventFilter( 
        DialogPtr       dialogPtr,
        EventRecord     *eventStructPtr,
        SInt16          *itemHit,
        void            *doNotKnow )
{
#if TARGET_OS_MAC
    Boolean         handledEvent = false;
    GrafPtr         oldPort;
    
#if TARGET_API_MAC_CARBON
    if((eventStructPtr->what == updateEvt) &&
            ((WindowPtr) eventStructPtr->message != NULL) &&
            ((WindowPtr) eventStructPtr->message != GetDialogWindow( dialogPtr ))) {
#else        
    if((eventStructPtr->what == updateEvt) &&
            ((WindowPtr) eventStructPtr->message != NULL) &&
            ((WindowPtr) eventStructPtr->message != dialogPtr)) {

		/* 
		 * Handle update events to background windows here. 
		 * First, translate mac event to a number of tcl events.
		 * If any tcl events generated, execute them until empty, and don't wait.
		 */

		if (TkMacConvertEvent( eventStructPtr )) {
			while ( Tcl_DoOneEvent( TCL_IDLE_EVENTS | TCL_DONT_WAIT | TCL_WINDOW_EVENTS ) )
				/* empty */
				;
		}
#endif

    } else {
        GetPort( &oldPort );
#if TARGET_API_MAC_CARBON
        SetPortDialogPort( dialogPtr );
#else        
        SetPort( dialogPtr );
#endif
        handledEvent = StdFilterProc( dialogPtr, eventStructPtr, itemHit );
        SetPort( oldPort );
    }
    return( handledEvent );
#endif  // TARGET_OS_MAC

#ifdef _WIN32
    return false;
#endif  // _WIN32
}

#if TARGET_OS_MAC && !TARGET_API_MAC_CARBON	// Classic

int
HandleInitialDirectory(
    Tcl_Interp *interp,
    char *initialDir, 
    FSSpec *dirSpec, 
    AEDesc *dirDescPtr)
{
	Tcl_DString     ds;
	long            dirID;
	OSErr           err;
	Boolean         isDirectory;
	Str255          dir;
	int             srcRead, dstWrote;
	
	if (Tcl_TranslateFileName( interp, initialDir, &ds ) == NULL) {
	    return TCL_ERROR;
	}
	Tcl_UtfToExternal( NULL, gQTTclTranslationEncoding, Tcl_DStringValue(&ds), 
    		Tcl_DStringLength(&ds), 0, NULL, StrBody(dir), 255, 
    		&srcRead, &dstWrote, NULL );
        StrLength(dir) = (unsigned char) dstWrote;
	Tcl_DStringFree(&ds);
          
	err = FSpLocationFromPath( StrLength(dir), StrBody(dir), dirSpec );
	if (err != noErr) {
	    Tcl_AppendResult( interp, "bad directory \"", initialDir, "\"", NULL );
	    return TCL_ERROR;
	}
	err = FSpGetDirectoryIDTcl( dirSpec, &dirID, &isDirectory );
	if ((err != noErr) || !isDirectory) {
	    Tcl_AppendResult( interp, "bad directory \"", initialDir, "\"", NULL );
	    return TCL_ERROR;
	}
    AECreateDesc( typeFSS, dirSpec, sizeof(*dirSpec), dirDescPtr );        
    return TCL_OK;
}
#endif  // Classic

/*
 *----------------------------------------------------------------------
 *
 * CanOpenObjCmd --
 *
 *		Investigates if file may be opened by QuickTime.
 *      '::quicktimetcl::canopen fileName ?-type graphics|movie -allownewfile 0|1
 *              -allowall 0|1?'
 *
 * Results:
 *		A standard Tcl result.
 *
 * Side effects:
 *		None.
 *
 *----------------------------------------------------------------------
 */

int
CanOpenObjCmd(
    ClientData  clientData,
    Tcl_Interp  *interp,		/* Current interpreter. */
    int         objc,		    /* Number of arguments. */
    Tcl_Obj     *CONST objv[])	/* Argument objects. */
{
    int             result = TCL_OK;
	OSStatus	    err;
	FSSpec 			fss;
	Boolean         withGrahicsImporter = false;
	Boolean         *withGrahicsImporterPtr;
	Boolean         asMovie = false;
	Boolean         *asMoviePtr;
	Boolean         preferGraphicsImporter;
	UInt32          flags = 0;
	int             canOpen = 0;
	int             iarg;
	int             optIndex;
	int             oneInt;
	char            *type;
	Tcl_Obj			*resultObjPtr;
	char            usage[] = "fileName ?-type graphics|movie -allownewfile 0|1 -allowall 0|1?";
	
    if (objc < 2) {
		Tcl_WrongNumArgs( interp, 1, objv, usage );
	    return TCL_ERROR; 
    }
	err = QTTclNativePathNameToFSSpec( interp, Tcl_GetString(objv[1]), &fss );
	if (err == fnfErr) {
        Tcl_SetObjResult( interp, Tcl_NewStringObj("File not found ", -1) );
		return TCL_ERROR;
	} else if (err != noErr) {
        Tcl_SetObjResult( interp, Tcl_NewStringObj("Unable to make a FSSpec from file", -1) );
		return TCL_ERROR;
	}
	withGrahicsImporterPtr = &withGrahicsImporter;
	asMoviePtr = &asMovie;
	
    for (iarg = 2; iarg < objc; iarg += 2) {
    
    	if (Tcl_GetIndexFromObj( interp, objv[iarg], allCanOpenOptions, 
    	        "canopen option", TCL_EXACT, &optIndex ) != TCL_OK ) {
    	    result = TCL_ERROR;
    	    goto done;
    	}    	
    	if (iarg + 1 == objc) {
    		resultObjPtr = Tcl_GetObjResult( interp );
    		Tcl_AppendStringsToObj( resultObjPtr, "value for \"",
    				Tcl_GetString(objv[iarg]), "\"missing", (char *) NULL );
    	    result = TCL_ERROR;
    	    goto done;
    	}
        
        /*
         * Dispatch the option to the right branch.
         */

        switch(optIndex) {

            case kCanOpenOptionAllowAll: {
                if (TCL_OK != Tcl_GetBooleanFromObj( interp, objv[iarg+1], &oneInt )) {
					Tcl_AddErrorInfo( interp, 
							"\n	(processing -allowall option)" );
            	    result = TCL_ERROR;
            	    goto done;
                }
                if (oneInt) {
                    flags |= kQTAllowAggressiveImporters;
                }
                break;
            }

            case kCanOpenOptionAllowNewFile: {
                if (TCL_OK != Tcl_GetBooleanFromObj( interp, objv[iarg+1], &oneInt )) {
					Tcl_AddErrorInfo( interp, 
							"\n	(processing -allownewfile option)" );
            	    result = TCL_ERROR;
            	    goto done;
                }
                if (oneInt) {
                    flags |= kQTAllowImportersThatWouldCreateNewFile;
                }
                break;
            }

            case kCanOpenOptionType: {
                type = Tcl_GetStringFromObj( objv[iarg+1], (int *) NULL);
            	if (strcmp(type, "graphics" ) == 0) {
            	    asMoviePtr = NULL;
                } else if (strcmp( type, "movie" ) == 0) {
                    withGrahicsImporterPtr = NULL;
                } else {
                    Tcl_SetObjResult( interp, 
                            Tcl_NewStringObj("Error: use -type graphics|movie", -1) );
            	    result = TCL_ERROR;
            	    goto done;
                }
                break;
            }
        }
    }

    err = CanQuickTimeOpenFile( &fss, 0, 0, withGrahicsImporterPtr, asMoviePtr, 
            &preferGraphicsImporter, flags );
    if (err != noErr) {
        Tcl_SetObjResult( interp, Tcl_NewStringObj("CanQuickTimeOpenFile failed", -1) );
		return TCL_ERROR;
	}
	
	if (withGrahicsImporter || asMovie) {
	    canOpen = 1;
	}
    Tcl_SetObjResult( interp, Tcl_NewIntObj(canOpen) );
    
done:

	return result;
}

/*
 *-------------------------------------------------------------------------
 * ImgReadInit --
 *  This procedure initializes a base64 decoder handle for reading.
 *
 * Results:
 *  none
 *
 * Side effects:
 *  the base64 handle is initialized
 *
 *-------------------------------------------------------------------------
 */

int
ImgReadInit( Tcl_Obj *data,		/* string containing initial mmencoded data */
        int	c, 
        MFile *handle )			/* mmdecode "file" handle */
{
    handle->data = Tcl_GetByteArrayFromObj( data, &handle->length );
    if (*handle->data == c) {
        handle->state = IMG_STRING;
        return 1;
    }
    c = base64_table[(c>>2)&63];

    while( (handle->length) && (char64(*handle->data) == IMG_SPACE) ) {
        handle->data++;
        handle->length--;
    }
    if (c != *handle->data) {
        handle->state = IMG_DONE;
        return 0;
    }
    handle->state = 0;
    return 1;
}

/*
 *--------------------------------------------------------------------------
 * ImgRead --
 *
 *  This procedure returns a buffer from the stream input. This stream
 *  could be anything from a base-64 encoded string to a Channel.
 *
 * Results:
 *  The number of characters successfully read from the input
 *
 * Side effects:
 *  The MFile state could change.
 *--------------------------------------------------------------------------
 */

int
ImgRead(handle, dst, count)
    MFile *handle;	/* mmdecode "file" handle */
    char *dst;		/* where to put the result */
    int count;		/* number of bytes */
{
    register int i, c;
    
    switch (handle->state) {
        case IMG_STRING:
            if (count > handle->length) {
                count = handle->length;
            }
            if (count) {
                memcpy(dst, handle->data, count);
                handle->length -= count;
                handle->data += count;
            }
            return count;
        case IMG_CHAN:
            return Tcl_Read((Tcl_Channel) handle->data, dst, count);
    }

    for (i = 0; i < count && (c = ImgGetc(handle)) != IMG_DONE; i++) {
        *dst++ = c;
    }
    return i;
}

/*
 *--------------------------------------------------------------------------
 *
 * ImgGetc --
 *
 *  This procedure returns the next input byte from a stream. This stream
 *  could be anything from a base-64 encoded string to a Channel.
 *
 * Results:
 *  The next byte (or IMG_DONE) is returned.
 *
 * Side effects:
 *  The MFile state could change.
 *
 *--------------------------------------------------------------------------
 */

int
ImgGetc( MFile *handle )	/* Input stream handle */
{
    int c;
    int result = 0;			/* Initialization needed only to prevent
                             * gcc compiler warning */
    if (handle->state == IMG_DONE) {
        return IMG_DONE;
    }

    if (handle->state == IMG_STRING) {
        if (!handle->length--) {
            handle->state = IMG_DONE;
            return IMG_DONE;
        }
        return *handle->data++;
    }

    do {
        if (!handle->length--) {
            handle->state = IMG_DONE;
            return IMG_DONE;
        }
        c = char64(*handle->data++);
    } while (c == IMG_SPACE);

    if (c > IMG_SPECIAL) {
        handle->state = IMG_DONE;
        return IMG_DONE;
    }

    switch (handle->state++) {
        case 0:
            handle->c = c<<2;
            result = ImgGetc(handle);
            break;
        case 1:
            result = handle->c | (c>>4);
            handle->c = (c&0xF)<<4;
            break;
        case 2:
            result = handle->c | (c>>2);
            handle->c = (c&0x3)<<6;
            break;
        case 3:
            result = handle->c | c;
            handle->state = 0;
            break;
    }
    return result;
}

/*
 *--------------------------------------------------------------------------
 * char64 --
 *
 *	This procedure converts a base64 ascii character into its binary
 *	equivalent. This code is a slightly modified version of the
 *	char64 proc in N. Borenstein's metamail decoder.
 *
 * Results:
 *	The binary value, or an error code.
 *
 * Side effects:
 *	None.
 *--------------------------------------------------------------------------
 */

static int
char64(c)
    int c;
{
    switch(c) {
	case 'A': return 0;	case 'B': return 1;	case 'C': return 2;
	case 'D': return 3;	case 'E': return 4;	case 'F': return 5;
	case 'G': return 6;	case 'H': return 7;	case 'I': return 8;
	case 'J': return 9;	case 'K': return 10;	case 'L': return 11;
	case 'M': return 12;	case 'N': return 13;	case 'O': return 14;
	case 'P': return 15;	case 'Q': return 16;	case 'R': return 17;
	case 'S': return 18;	case 'T': return 19;	case 'U': return 20;
	case 'V': return 21;	case 'W': return 22;	case 'X': return 23;
	case 'Y': return 24;	case 'Z': return 25;	case 'a': return 26;
	case 'b': return 27;	case 'c': return 28;	case 'd': return 29;
	case 'e': return 30;	case 'f': return 31;	case 'g': return 32;
	case 'h': return 33;	case 'i': return 34;	case 'j': return 35;
	case 'k': return 36;	case 'l': return 37;	case 'm': return 38;
	case 'n': return 39;	case 'o': return 40;	case 'p': return 41;
	case 'q': return 42;	case 'r': return 43;	case 's': return 44;
	case 't': return 45;	case 'u': return 46;	case 'v': return 47;
	case 'w': return 48;	case 'x': return 49;	case 'y': return 50;
	case 'z': return 51;	case '0': return 52;	case '1': return 53;
	case '2': return 54;	case '3': return 55;	case '4': return 56;
	case '5': return 57;	case '6': return 58;	case '7': return 59;
	case '8': return 60;	case '9': return 61;	case '+': return 62;
	case '/': return 63;

	case ' ': case '\t': case '\n': case '\r': case '\f': return IMG_SPACE;
	case '=': return IMG_PAD;
	case '\0': return IMG_DONE;
	default: return IMG_BAD;
    }
}

/*--------------------------------------------------------------------------------*/
