/* 
 * Broadcast.c --
 *
 *      This module implements a "broadcast" widget that is object
 *      based. It is part of the QuickTimeTcl package.
 *      It provides an interface to the (ill docoumented) QuickTime broadcaster
 *      API's. This includes the Presentation and Sourcer classes of APIs.
 *
 * Copyright (c) 2003 Mats Bengtsson
 *
 * $Id: Broadcast.c,v 1.8 2008/02/15 15:23:05 matben Exp $
 */

#include "Broadcast.h"

/*
 * We need a (single) linked list for our broadcasters.
 */

static MovieBroadcastList  *gMovieBroadcastListPtr = NULL;

static GWorldPtr    gOffscreenGWorldPtr = NULL;

#if TARGET_API_MAC_CARBON
EventLoopTimerRef   gCarbonPresentationTimerRef = NULL;
const EventTime 	kCarbonPresentationTimerSecs = kEventDurationSecond / 10;
//long				kMinimumIdleDurationInMillis = 20;
#endif

static char *broadcastCommands[] = {
    "audioconfigure",
    "cget", 
    "configure", 
    "export",
    "getsettings", 
    "getstatistics", 
    "pause", 
    "picture",
    "preview", 
    "settingsdialog", 
    "skipto",
    "sourcer",
    "start", 
    "stop", 
    "stream", 
    "videoconfigure", 
    (char *) NULL
};

enum {
    kBcastCmdAudioConfigure     = 0L,
    kBcastCmdCget,
    kBcastCmdConfigure,
    kBcastCmdExport,
    kBcastCmdGetSettings,
    kBcastCmdGetStatistics,
    kBcastCmdPause,
    kBcastCmdPicture,
    kBcastCmdPreview,
    kBcastCmdSettingsDialog,
    kBcastCmdSkipTo,
    kBcastCmdSourcer,
    kBcastCmdStart,
    kBcastCmdStop,
    kBcastCmdStream,
    kBcastCmdVideoConfigure
};

static char *sourcerCommands[] = {
    "add", "info", "remove",
    (char *) NULL
};

enum {
    kSourcerCmdAdd          = 0L,
    kSourcerCmdInfo,
    kSourcerCmdRemove
};

static char *videoConfigOptions[] = {
    "-brightness", "-contrast", "-hue",
    "-saturation", "-sharpness",
    (char *) NULL
};

enum {
    kVideoConfigOptionBrightness    = 0L,
    kVideoConfigOptionContrast,
    kVideoConfigOptionHue,
    kVideoConfigOptionSaturation,
    kVideoConfigOptionSharpness
};

static char *audioConfigOptions[] = {
    "-autogain", "-gain",
    (char *) NULL
};

enum {
    kAudioConfigOptionAutoGainOnOff    = 0L,
    kAudioConfigOptionGain
};

/*
 * Strings for the partial parsing of SDP data.
 */
 
#define kSDPAudioDefString      "m=audio"
#define kSDPVideoDefString      "m=video"

/* 
 * Information used for parsing configuration options.
 * Mask bits for options changed.
 */
 
enum {
    BCAST_CONF_NEWGWORLD              	= (1L << 0)
};

/*
 * The following table defines the legal values for the -size option.
 */

static CONST char *widgetSizeST[] = { 
	"quarter", "half", "full", (char *) NULL 
};
#define	BCAST_WIDGET_SIZE_QUARTER 		0
#define	BCAST_WIDGET_SIZE_HALF			1
#define	BCAST_WIDGET_SIZE_FULL			2

/*
 * Information used for objv parsing.
 */

static Tk_OptionSpec optionSpecs[] = {
	{TK_OPTION_STRING, "-command", "command", "Command",
		NULL, -1, Tk_Offset(MovieBroadcast, command), TK_OPTION_NULL_OK,
		(ClientData) NULL, 0},
	{TK_OPTION_PIXELS, "-height", "height", "Height",
		"0", -1, Tk_Offset(MovieBroadcast, height), 0, 
		(ClientData) NULL, BCAST_CONF_NEWGWORLD},
	{TK_OPTION_STRING_TABLE, "-size", "size", "Size",
		"half", -1, Tk_Offset(MovieBroadcast, indSize), 0, 
		(ClientData) widgetSizeST, BCAST_CONF_NEWGWORLD},
	{TK_OPTION_PIXELS, "-width", "width", "Width",
		"0", -1, Tk_Offset(MovieBroadcast, width), 0, 
		(ClientData) NULL, BCAST_CONF_NEWGWORLD},
    {TK_OPTION_END}
};

/*
 * Forward declarations for procedures defined later in this file:
 */

static void		BroadcastDeletedProc( ClientData clientData );
static int		BroadcastConfigure( Tcl_Interp *interp,
        			    MovieBroadcast *bcastPtr, int objc, 
                        Tcl_Obj *CONST objv[] );
static void     BroadcastWorldChanged( ClientData clientData );
static int      BroadcastComputeGeometry( MovieBroadcast *bcastPtr, 
                        int *width, int *height );
static void		DisplayBroadcast( ClientData clientData );
static void     DrawBluescreen( MovieBroadcast *bcastPtr );
static void		DestroyBroadcast( ClientData clientData );
static void		BroadcastEventProc( ClientData clientData,
        			    XEvent *eventPtr );
static int		BroadcastWidgetObjCmd( ClientData clientData,
        			    Tcl_Interp *, int objc, Tcl_Obj * CONST objv[] );
static ComponentResult PresentationNotification( ComponentResult inErr, 
                        OSType inNotificationType,
	                    void *inNotificationParams, void *inRefCon );
static ComponentResult HandleErrorNotification( MovieBroadcast *bcastPtr,
                	    const char *inNotificationTypeString,
                		ComponentResult inErr, 
                		QTSErrorParams *inErrorParams );
static int      GetSDPFromFile( MovieBroadcast *bcastPtr, 
                        Tcl_Obj *fileNamePtr );
static int      GetSDPFromURL( MovieBroadcast *bcastPtr, 
                        Tcl_Obj *urlPtr );
static int      GetSDPFromData( MovieBroadcast *bcastPtr, 
                        Tcl_Obj *sdpListPtr );
static void     AddToBroadcastList( MovieBroadcast *bcastPtr );
static void     RemoveFromBroadcastList( MovieBroadcast *bcastPtr );
static void     IdleAllPresentations( void );
static void     IdleSourcersForPresentation( MovieBroadcast *bcastPtr );
static int      PresentationExportToFile( MovieBroadcast *bcastPtr,
	                    int objc, Tcl_Obj *CONST objv[] );
static int      SourcerObjCmd( Tcl_Interp *interp, 
                        MovieBroadcast *movPtr,
	                    int objc, Tcl_Obj *CONST objv[] );
static int      StreamObjCmd( Tcl_Interp *interp, 
                        MovieBroadcast *bcastPtr,
	                    int objc, Tcl_Obj *CONST objv[] );
static void     GetAllStreamMediaTypes( QTSPresentation  presentation, 
                        int *haveAudio, int *haveVideo, int *haveText );
static int      SourcerGetInfo( Tcl_Interp *interp, 
                        MovieBroadcast *bcastPtr, QTSStream inStream,
	                    int objc, Tcl_Obj *CONST objv[] );
static OSErr    GetSourcerType( QTSSourcer sourcer, OSType *typePtr );
static OSErr    GetSeqGrabberFullInputRect( QTSPresentation	presentation,
                        int *fullWidth, int *fullHeight );
static int      PictureObjCmd( Tcl_Interp *interp, 
                        MovieBroadcast *bcastPtr,
	                    int objc, Tcl_Obj *CONST objv[] );
static int      SetupTrackSourcerForStream( MovieBroadcast *bcastPtr,
                        QTSStream inStream );
static void     PresentationFree( MovieBroadcast *bcastPtr );
static void     SourcerFree( BcastSourcerInfo *sourcerPtr );
static pascal ComponentResult MovieSourcingCallbackProc(
                        ComponentResult inErr, OSType inSelector, 
		                void  *ioParams, void *inRefCon );
void            QTSDisposeMediaParams( QTSMediaParams *inMediaParams );
static pascal Boolean   MyModalFilterDialogProc( DialogPtr dialogPtr, 
                        EventRecord * event, SInt16 *itemHit, long *refCon );

#if TARGET_API_MAC_CARBON
static OSStatus	InstallBroadcastIdlingEventLoopTimer( void );
static pascal void	PresentationCarbonTimer( EventLoopTimerRef theTimer, 
                        void *userData );
#endif

/*
 *--------------------------------------------------------------
 *
 * BroadcastCmd --
 *
 *	    This procedure is invoked to process the "qtbroadcast" Tcl
 *	    command.  It creates a new "qtbroadcast" widget.
 *
 * Results:
 *	    A standard Tcl result.
 *
 * Side effects:
 *	    A new widget is created and configured.
 *
 *--------------------------------------------------------------
 */

int
BroadcastObjCmd(
        ClientData  clientData,	    /* NULL. */
        Tcl_Interp  *interp,		/* Current interpreter. */
        int         objc,		    /* Number of arguments. */
        Tcl_Obj     *CONST objv[] )	/* Argument objects. */
{
    MovieBroadcast              *bcastPtr;
    long                        eventFlags = 0L;
    Tk_Window                   tkwin;
    Tk_OptionTable              optionTable;
	static int  	            isInitialized = false;
    OSErr                       err = noErr;

    if (objc < 3) {
    	Tcl_WrongNumArgs( interp, 1, objv, "pathName sdpFile ?options?" );
    	return TCL_ERROR;
    }

    /* 
     * We use one and the same offscreen GWorld for all movies. 
     */
        
    if (gOffscreenGWorldPtr == NULL) {
        Rect    aRect = { 0, 0, 10, 10 };   /* Just a dummy rect. */
        
	    err = MySafeNewGWorld( &gOffscreenGWorldPtr, 32, &aRect, NULL, NULL, 0 );
	    if (err != noErr) {
	        CheckAndSetErrorResult( interp, err );
			return TCL_ERROR;
	    }
    }
    
    if (!isInitialized) {        
        //err = InitializeQTS();
        if (err != noErr) {
        
            return TCL_ERROR;
        }
	    isInitialized = true;
	    
        QTTclDebugPrintf( interp, 2, 
                "FocusIn=%2d FocusOut=%2d Expose=%2d DestroyNotify=%2d UnmapNotify=%2d MapNotify=%2d ConfigureNotify=%2d",
                FocusIn, FocusOut, Expose, DestroyNotify, UnmapNotify, MapNotify, ConfigureNotify );
    }
    
    tkwin = Tk_CreateWindowFromPath( interp, Tk_MainWindow( interp ), 
    	    Tcl_GetStringFromObj( objv[1], NULL ), (char *) NULL );
    if (tkwin == NULL) {
    	return TCL_ERROR;
    }
    Tk_SetClass( tkwin, "QTBroadcast" );

    /*
     * Create the option table for this widget class.  If it has
     * already been created, the refcount will get bumped and just
     * the pointer will be returned.  The refcount getting bumped
     * does not concern us, because Tk will ensure the table is
     * deleted when the interpreter is destroyed.
     */

    optionTable = Tk_CreateOptionTable( interp, optionSpecs );

    /*
     * Allocate and initialize the widget record.  The memset allows
     * us to set just the non-NULL/0 items.
     */

    bcastPtr = (MovieBroadcast *) ckalloc( sizeof(MovieBroadcast) );
    memset( (void *) bcastPtr, 0, sizeof(MovieBroadcast) );

    bcastPtr->tkwin = tkwin;
    bcastPtr->display = Tk_Display( tkwin );
    bcastPtr->interp = interp;
    bcastPtr->widgetCmd	= Tcl_CreateObjCommand( interp,
	    Tk_PathName( bcastPtr->tkwin ), BroadcastWidgetObjCmd,
	    (ClientData) bcastPtr, BroadcastDeletedProc );
    bcastPtr->optionTable = optionTable;

    if (Tk_InitOptions( interp, (char *) bcastPtr, optionTable, tkwin)
    	    != TCL_OK) {
    	Tk_DestroyWindow( bcastPtr->tkwin );
    	ckfree( (char *) bcastPtr );
    	return TCL_ERROR;
    }
    bcastPtr->srcWidth = 320;
    bcastPtr->srcHeight = 240;
    bcastPtr->command = NULL;
    bcastPtr->grafPtr = NULL;
    bcastPtr->presentation = kQTSInvalidPresentation;

    if (GetSDPFromFile( bcastPtr, objv[2] ) != TCL_OK) {
    	goto error;
    }
    bcastPtr->state = kPresentationStateIdle;
    bcastPtr->targetState = kPresentationStateIdle;

    GetAllStreamMediaTypes( bcastPtr->presentation, 
            &bcastPtr->haveAudioStream, &bcastPtr->haveVideoStream,
            &bcastPtr->haveTextStream );

    eventFlags = ExposureMask | StructureNotifyMask;
    Tk_CreateEventHandler( bcastPtr->tkwin, eventFlags,
    	    BroadcastEventProc, (ClientData) bcastPtr );

    if (BroadcastConfigure( interp, bcastPtr, objc - 3, objv + 3) != TCL_OK) {
    	goto error;
    }

	bcastPtr->flags |= NEWGWORLD;
	BroadcastWorldChanged( (ClientData) bcastPtr );
	AddToBroadcastList( bcastPtr );
    
#if TARGET_API_MAC_CARBON
    /* 
        *  If this is the first presentation we open on Carbon, be sure to set a timer
        *  to serv it and all that may follow.
        */

    if (gMovieBroadcastListPtr == NULL) {
        InstallBroadcastIdlingEventLoopTimer();
    }
#endif	 

    Tcl_SetObjResult( interp,
    	    Tcl_NewStringObj( Tk_PathName( bcastPtr->tkwin ), -1) );
    return TCL_OK;

error:

    Tk_DestroyWindow( bcastPtr->tkwin );
    return TCL_ERROR;
}

/*
 *--------------------------------------------------------------
 *
 * BroadcastWidgetObjCmd --
 *
 *	    This procedure is invoked to process the Tcl command
 *	    that corresponds to a widget managed by this module.
 *	    See the user documentation for details on what it does.
 *
 * Results:
 *	    A standard Tcl result.
 *
 * Side effects:
 *	    See the user documentation.
 *
 *--------------------------------------------------------------
 */

static int
BroadcastWidgetObjCmd(
        ClientData  clientData,		/* Information about square widget. */
        Tcl_Interp  *interp,		/* Current interpreter. */
        int         objc,		    /* Number of arguments. */
        Tcl_Obj     *CONST objv[] ) /* Argument objects. */
{
    //MovieBroadcast  *bcastPtr = (MovieBroadcast *) clientData;
    MovieBroadcast  *bcastPtr;
    Tcl_Obj         *resultObjPtr;
    int             index;
    OSErr           err = noErr;
    int             result = TCL_OK;
    
    bcastPtr = (MovieBroadcast *) clientData;

	QTTclDebugPrintf( bcastPtr->interp, 2, 
	        "BroadcastWidgetObjCmd: targetState=%2d, state=%2d",
	        bcastPtr->targetState, bcastPtr->state );

    if (objc < 2) {
    	Tcl_WrongNumArgs( interp, 1, objv, "option ?arg arg...?" );
    	return TCL_ERROR;
    }
   	if (bcastPtr->presentation == kQTSInvalidPresentation)  {
        Tcl_SetObjResult( interp,
                Tcl_NewStringObj( "Invalid presentation", -1) );
        goto error;
   	}
    if (Tcl_GetIndexFromObj( interp, objv[1], broadcastCommands, "command",
    	    0, &index ) != TCL_OK) {
    	return TCL_ERROR;
    }

    Tcl_Preserve( (ClientData) bcastPtr );
    
    switch (index) {

        case kBcastCmdAudioConfigure: {


            break;
        }

    	case kBcastCmdCget: {
    	    if (objc != 3) {
        		Tcl_WrongNumArgs( interp, 2, objv, "option" );
        		goto error;
    	    }
    	    resultObjPtr = Tk_GetOptionValue( interp, (char *) bcastPtr,
    		        bcastPtr->optionTable, objv[2], bcastPtr->tkwin );
    	    if (resultObjPtr == NULL) {
        		result = TCL_ERROR;
    	    } else {
        		Tcl_SetObjResult( interp, resultObjPtr );
    	    }
    	    break;
    	}

    	case kBcastCmdConfigure: {
    	    resultObjPtr = NULL;
			if (objc <= 3) {
				resultObjPtr = Tk_GetOptionInfo( interp, (char *) bcastPtr,
					bcastPtr->optionTable,
					(objc == 2) ? (Tcl_Obj *) NULL : objv[2],
					bcastPtr->tkwin );
				if (resultObjPtr == NULL) {
					result = TCL_ERROR;
				} else {
					Tcl_SetObjResult( interp, resultObjPtr );
				}
			} else {
    		
    			/*
    			 * Change one or more attributes. Reschedule a new display via 
    			 * 'BroadcastWorldChanged'. Be sure to only set the objv values by the flag.
    			 * The NEWGWORLD bit in flags is set if we need a redisplay.
    			 */

    			result = BroadcastConfigure( interp, bcastPtr, objc - 2, objv + 2 );
    					
                /* 
                 * Only if we made a configuration that needs a redisplay. 
                 */
                 
    			if ((result == TCL_OK) && (bcastPtr->flags & NEWGWORLD)) {		
    				BroadcastWorldChanged( (ClientData) bcastPtr );
    			}
			}
			break;
        }
        
        case kBcastCmdExport: {
            if (PresentationExportToFile( bcastPtr, objc - 2, objv + 2 )
                    != TCL_OK) {
                goto error;
            }        
            break;
        }

        case kBcastCmdGetSettings: {
          	char 			nul = 0;
          	Ptr 			p = NULL;
            Handle          outTextHand = NULL;
        	Tcl_DString     ds;
            
            //  kQTSSettingsTextSummary
            //  kQTSSettingsTextDetails
            err = QTSPresGetSettingsAsText( bcastPtr->presentation, 
                    kQTSAllStreams, 0, kQTSSettingsTextDetails, 
                    &outTextHand, NULL, NULL );
            if (err != noErr) {

                goto error;
            }
            HLock( outTextHand );

	        /* null-terminate the string in the handle */
	   		PtrAndHand( &nul, outTextHand, 1 );
     		p = *outTextHand;
     		while (*p) {
           		if (*p == kReturnChar) {
           			*p = kNewlineChar;
           		}
           		p++;
     		};
            Tcl_ExternalToUtfDString( gQTTclTranslationEncoding, *outTextHand, 
                    -1, &ds );
            Tcl_SetObjResult( interp, Tcl_NewStringObj(
               		Tcl_DStringValue(&ds), -1 ) );
            HUnlock( outTextHand );
            Tcl_DStringFree( &ds );		    
            break;
        }

        case kBcastCmdGetStatistics: {



            break;
        }

        case kBcastCmdPause: {
        	err = QTSPresStop( bcastPtr->presentation, kQTSAllStreams, 0L );
        	if (err != noErr) {
            	err = QTSPresPreview( bcastPtr->presentation, kQTSAllStreams, 
                        NULL, kQTSNormalForwardRate, 0 );
            }
        	if (err != noErr) {

                result = TCL_ERROR;
            }
        	bcastPtr->targetState = kPresentationStateIdle;
            break;
        }
        case kBcastCmdPicture: {
            if (PictureObjCmd( interp, bcastPtr, objc - 2, objv + 2 ) 
                    != TCL_OK) {
                    
                goto error;
            }
            break;
        }

        case kBcastCmdPreview: {
            Fixed       rate;
            int         doStart = true;    
            
            if (objc == 2) {
                rate = kQTSNormalForwardRate;
            } else if (objc == 3) {
				if (Tcl_GetBooleanFromObj( interp, objv[2], &doStart ) 
				        != TCL_OK) {
					Tcl_AddErrorInfo( interp, 
							"\n	(processing preview command)" );
					result = TCL_ERROR;
					goto error;
				}
				if (doStart) {
                    rate = kQTSNormalForwardRate;
				} else {
                    rate = kQTSStoppedRate;
                }
            } else {
            	Tcl_WrongNumArgs( interp, 2, objv, "?boolean?" );
            	return TCL_ERROR;
            }
            if (doStart) {
            	if (bcastPtr->state >= kPresentationStateStartingPreview) {

            		/* we're already playing or trying to start */
                    goto error;
            	}
            	bcastPtr->targetState = kPresentationStatePreviewing;
            	bcastPtr->state = kPresentationStateStartingPreview;
        	} else {
            	bcastPtr->targetState = kPresentationStateIdle;
        	}
            err = QTSPresPreview( bcastPtr->presentation, kQTSAllStreams, NULL,
                    rate, 0 );
                    
            break;
        }

        case kBcastCmdSettingsDialog: {
            SInt32      flags = 0L;
            
            err = QTSPresSettingsDialog( bcastPtr->presentation,
                    kQTSAllStreams, flags, 
                    NewQTSModalFilterUPP(MyModalFilterDialogProc), bcastPtr );
                    
            break;
        }

        case kBcastCmdSkipTo: {
            TimeValue64     timeValue64;
            
            if (objc < 3) {
            	Tcl_WrongNumArgs( interp, 2, objv, "?time?" );
            	return TCL_ERROR;
            }
            // get TimeValue64 from string?????????
            
            err = QTSPresSkipTo64( bcastPtr->presentation, &timeValue64 );
            break;
        }
        
        case kBcastCmdSourcer: {
            if (objc < 3) {
            	Tcl_WrongNumArgs( interp, 2, objv, "command ?arg arg...?" );
            	return TCL_ERROR;
            }
            if (SourcerObjCmd( interp, bcastPtr, objc - 2, objv + 2 ) 
                    != TCL_OK) {

                goto error;
            }
            break;
        }

        case kBcastCmdStart: {
        	if (bcastPtr->state >= kPresentationStateStarting)  {

        		/* we're already playing or trying to start */
                goto error;
        	}
        	
        	/* Right now, we have to preroll before we start. */
        	bcastPtr->targetState = kPresentationStatePlaying;
        	bcastPtr->state = kPresentationStateStartingPreroll;
        	err = QTSPresPreroll( bcastPtr->presentation, kQTSAllStreams, 
                    0, kQTSNormalForwardRate, 0L );
            if (err != noErr) {

                goto error;
            }
            break;
        }

        case kBcastCmdStop: {
        	bcastPtr->targetState = kPresentationStateIdle;
        	if (bcastPtr->state != kPresentationStateIdle)  {
        		bcastPtr->state = kPresentationStateIdle;
        		err = QTSPresStop( bcastPtr->presentation, kQTSAllStreams, 0L );
                if (err != noErr) {

                    goto error;
                }
        	}


            break;
        }

        case kBcastCmdStream: {
            if (StreamObjCmd( interp, bcastPtr, objc - 2, objv + 2 ) 
                    != TCL_OK) {

                goto error;
            }
        
            break;
        }
        
        case kBcastCmdVideoConfigure: {


            break;
        }
    }
    Tcl_Release( (ClientData) bcastPtr );
    return result;

error:
    Tcl_Release( (ClientData) bcastPtr );
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * BroadcastConfigure --
 *
 *	    This procedure is called to process an objv/objc list in
 *	    conjunction with the Tk option database to configure (or
 *	    reconfigure) a square widget.
 *
 * Results:
 *	    The return value is a standard Tcl result.  If TCL_ERROR is
 *	    returned, then the interp's result contains an error message.
 *
 * Side effects:
 *	    Configuration information, such as colors, border width,
 *	    etc. get set for bcastPtr;  old resources get freed,
 *	    if there were any.
 *
 *----------------------------------------------------------------------
 */

static int
BroadcastConfigure(
        Tcl_Interp  *interp,	    /* Used for error reporting. */
        MovieBroadcast *bcastPtr, 	/* Information about widget. */
        int         objc, 
        Tcl_Obj     *CONST objv[] )
{
    int 				ierror;
	int 				mask = 0L;
    int                 width, height;
	Tk_SavedOptions 	savedOptions;
	Tcl_Obj 			*resultObjPtr = NULL;
    Tcl_Obj 			*errorResult = NULL;
	OSErr				err = noErr;
	
	QTTclDebugPrintf( bcastPtr->interp, 2, "BroadcastConfigure" );

    /*
     * The following loop is potentially executed twice.  During the
     * first pass configuration options get set to their new values.
     * If there is an error in this pass, we execute a second pass
     * to restore all the options to their previous values.
     *
     * A 'continue' within this loop signals an error condition;
     * 'break' when everything went OK.
     */

    for (ierror = 0; ierror <= 1; ierror++) {
		if (!ierror) {
		    /*
		     * First pass: set options to new values.
		     */

			if (Tk_SetOptions( interp, (char *) bcastPtr, bcastPtr->optionTable, 
                    objc, objv, bcastPtr->tkwin, &savedOptions, &mask) 
                        != TCL_OK ) {
				continue;
		    }
		} else {
		    /*
		     * Second pass: restore options to old values.
		     */

		    errorResult = Tcl_GetObjResult( interp );
		    Tcl_IncrRefCount( errorResult );
		    Tk_RestoreSavedOptions( &savedOptions );
		}

		if (mask & BCAST_CONF_NEWGWORLD) {
		    if (BroadcastComputeGeometry( bcastPtr, &width, &height ) != TCL_OK) {
				continue;
	        }
            err = QTSPresSetDimensions( bcastPtr->presentation,
                    kQTSAllStreams, Long2Fix( width ), Long2Fix( height ) );
		    bcastPtr->flags |= NEWGWORLD;
		}



	
		/*
		 * If we came so far break out of the ierror loop.
		 */
	
		break;
    }
    if (ierror) {
		Tcl_SetObjResult( interp, errorResult );
		Tcl_DecrRefCount( errorResult );
		return TCL_ERROR;
    } else {
		Tk_FreeSavedOptions( &savedOptions );
		return TCL_OK;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * BroadcastWorldChanged --
 *
 *		Something changed, arrange for the widget to be redisplayed. 
 *      Compute geometry.
 *
 * Results:
 *		None.
 *
 * Side effects:
 *		Broadcast Widget displayed: if already on display it is scheduled for 
 *		a renewed display, else, the size is requested by the tk geometry manager,
 *		and displayed upon a MapNotify event.
 *
 *----------------------------------------------------------------------
 */

static void
BroadcastWorldChanged( ClientData clientData ) 
{
    MovieBroadcast  *bcastPtr = (MovieBroadcast *) clientData;
	int 			width, height;

	QTTclDebugPrintf( bcastPtr->interp, 2, "BroadcastConfigure" );

	/*
	 * If not already scheduled for (re)display, it should be if it's mapped,
	 * else upon a MapNotify event.
	 */
	 
	if (Tk_IsMapped(bcastPtr->tkwin) && !(bcastPtr->flags & REDRAW_PENDING)) {
		Tcl_DoWhenIdle( DisplayBroadcast, (ClientData) bcastPtr );
		bcastPtr->flags |= REDRAW_PENDING;
	}
	
	/*
	 * Get the desired width and height to request.
	 */
	 
    BroadcastComputeGeometry( bcastPtr, &width, &height );

	if (bcastPtr->tkwin != NULL) { 
		
		/*
		 * After getting our geometry above, let tk also know.
		 */
		 
		Tk_GeometryRequest( bcastPtr->tkwin, width, height );
		Tk_SetInternalBorder( bcastPtr->tkwin, 0 );
		bcastPtr->flags |= NEWGWORLD;
	}
}

/*
 *----------------------------------------------------------------------
 *
 * BroadcastComputeGeometry --
 *
 *		Finds the widget size to request at tk.
 *
 * Results:
 *		The return value is a standard Tcl result. If TCL_ERROR is
 *		returned, then the interp's result contains an error message.
 *
 * Side effects:
 *		Returns width and height in function arguments. Note that these 
 *		are the actual width and height to request from tk, and not the 
 *		options.
 *
 *----------------------------------------------------------------------
 */
	
static int 
BroadcastComputeGeometry( 
        MovieBroadcast *bcastPtr, 
        int *width, int *height )
{
    Tcl_Interp 	    *interp = bcastPtr->interp;
    int             divisor;
    int             srcWidth = 0, srcHeight = 0;
	double		    goldenRatio;
	OSErr           err = noErr;

	QTTclDebugPrintf( bcastPtr->interp, 2, "BroadcastComputeGeometry" );

	*width = 0;
	*height = 0;
	
    err = GetSeqGrabberFullInputRect( bcastPtr->presentation,
            &srcWidth, &srcHeight );
    if (err == noErr) {
    	bcastPtr->srcWidth = srcWidth;
    	bcastPtr->srcHeight = srcHeight;
    	QTTclDebugPrintf( bcastPtr->interp, 2, 
    	        "\tsrcWidth=%d, srcHeight=%d", srcWidth, srcHeight );
    }
    	
	/*
	 * There are two possibilities here: either we have got one of 
	 * '-width' or '-height' > 0; use these in this case. Or use the
	 * '-size' option (quarter, half, or full).
	 */
	 
	if ((bcastPtr->width > 0) || (bcastPtr->height > 0)) {
		goldenRatio = (double) bcastPtr->srcWidth/(double) bcastPtr->srcHeight;
		if ((bcastPtr->width > 0) && (bcastPtr->height == 0)) {
			*width = bcastPtr->width;
			*height = (int) ((double) bcastPtr->width/goldenRatio);
		} else if ((bcastPtr->width == 0) && (bcastPtr->height > 0)) {
			*height = bcastPtr->height;
			*width = (int) (goldenRatio * bcastPtr->height);
		} else {
		
			/* This code should never be executed; my QuickCam becomes weired! */
			*width = bcastPtr->width;
			*height = bcastPtr->height;
		}			
		
		/*  
		 * Check max source size.
		 */

		if ((bcastPtr->width > bcastPtr->srcWidth) || 
				(bcastPtr->height > bcastPtr->srcHeight)) {
			bcastPtr->width = bcastPtr->srcWidth;
			*width =  bcastPtr->srcWidth;
			bcastPtr->height = bcastPtr->srcHeight;
			*height =  bcastPtr->srcHeight;
		}
	} else {
		switch (bcastPtr->indSize) {
			case BCAST_WIDGET_SIZE_QUARTER: 
				divisor = 4;
				break;
			case BCAST_WIDGET_SIZE_HALF: 
				divisor = 2;
				break;
			case BCAST_WIDGET_SIZE_FULL: 
				divisor = 1;
				break;
		}
		*width = bcastPtr->srcWidth/divisor;
		*height = bcastPtr->srcHeight/divisor;
	}
	QTTclDebugPrintf( bcastPtr->interp, 2, "\twidth=%d, height=%d",
	        *width, *height );

    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * GetSDPFromFile --
 *
 *      Fills in the necessary elements necessary for getting 
 *      settings from an SDP file.
 *
 * Results:
 *	    Standard TCL result.
 *
 * Side effects:
 *      None.
 *
 *--------------------------------------------------------------
 */

static int      
GetSDPFromFile( 
        MovieBroadcast *bcastPtr, 
        Tcl_Obj *fileNamePtr )
{
    Tcl_Interp      *interp = bcastPtr->interp;
   	Tcl_Channel     readChannel = NULL;
   	Tcl_Obj         *readObj = Tcl_NewObj();
    unsigned char   *contentPtr;
   	int             nread;
   	int             len;
	QTSPresentation presentation = kQTSInvalidPresentation;
	QTSPresParams	presParams; 
	QTSMediaParams	mediaParams;
    FSSpec          fsSpec;
    OSErr           err = noErr;
   	int             result = TCL_OK;

    err = QTTclNativePathNameToFSSpec( interp,
            Tcl_GetString( fileNamePtr ), &fsSpec );
    if (err != noErr) {
		Tcl_SetObjResult( bcastPtr->interp, Tcl_NewStringObj( 
				"File not found", -1 ) );
        return TCL_ERROR;
    }
	
	/*
	 * Make a partial parsing...
	 */

    readChannel = Tcl_FSOpenFileChannel( interp, Tcl_GetString(fileNamePtr), 
            "r", 0666 );
    if (readChannel == NULL) {
        result = TCL_ERROR;
        goto bail;
    }
    result = Tcl_SetChannelOption( interp, readChannel, "-translation", "binary" );
    if (result != TCL_OK) {
        goto bail;
    }
    nread = Tcl_ReadChars( readChannel, readObj, 99999, 0 );
    if (nread == -1) {
        Tcl_SetObjResult( interp, 
     		    Tcl_NewStringObj( Tcl_ErrnoMsg( Tcl_GetErrno() ), -1 ) );
        result = TCL_ERROR;
        goto bail;
    }
    contentPtr = Tcl_GetByteArrayFromObj( readObj, &len );


	memset( &presParams, 0, sizeof(presParams) );
	memset( &mediaParams, 0, sizeof(mediaParams) );
    QTSInitializeMediaParams( &mediaParams );

	mediaParams.v.width = Long2Fix(320);
	mediaParams.v.height = Long2Fix(240);
	//mediaParams.v.gWorld = gOffscreenGWorldPtr;
    mediaParams.v.gWorld = TkMacOSXGetDrawablePort(Tk_WindowId(bcastPtr->tkwin));
	mediaParams.v.gdHandle = GetMainDevice();

	presParams.version = kQTSPresParamsVersion1;
	presParams.flags = kQTSSendMediaFlag |      /* Send, not receive, 
	                                             * kQTSReceiveMediaFlag */
	        kQTSAutoModeFlag |                  /* Use seq grabber */
	        kQTSDontShowStatusFlag;
	presParams.timeScale = kDefaultPresTimeScale;
	presParams.mediaParams = &mediaParams;
	presParams.notificationProc = (QTSNotificationUPP) 
            NewQTSNotificationUPP( PresentationNotification );
	presParams.notificationRefCon = bcastPtr;

    /*
     * Create a new presentation.
     */

	err = QTSNewPresentationFromFile( &fsSpec, &presParams, &bcastPtr->presentation );
	if (err != noErr) {
        Tcl_SetObjResult( interp,
        	    Tcl_NewStringObj( "Failed creating new presentation", -1) );
        result = TCL_ERROR;
        goto bail;
    }
    //bcastPtr->presentation = presentation;
	 
bail:	 

    Tcl_Close( interp, readChannel );
    Tcl_DecrRefCount( readObj );
  	QTSDisposeMediaParams( &mediaParams );
    return result;
}

/*
 *--------------------------------------------------------------
 *
 * GetSDPFromData --
 *
 *      Fills in the necessary elements necessary for getting 
 *      settings from SDP Tcl list. Each item corresponding to a
 *      line in an SDP file.
 *
 * Results:
 *	    Standard TCL result.
 *
 * Side effects:
 *      None.
 *
 *--------------------------------------------------------------
 */

static int      
GetSDPFromData( 
        MovieBroadcast *bcastPtr, 
        Tcl_Obj *sdpListPtr )
{
    Tcl_Interp      *interp = bcastPtr->interp;
	char			*charPtr;
    int             nlines;
    int             i;
	Tcl_DString     ds;
	Tcl_DString     dsSDP;
    SInt64          dataLength;
    Tcl_Obj         *objPtr = NULL;
	QTSMediaParams	mediaParams;
	QTSPresParams	presParams; 
    OSErr           err = noErr;
    int             result = TCL_OK;

    if (Tcl_ListObjLength( interp, sdpListPtr, &nlines ) != TCL_OK) {
        result = TCL_ERROR;
        goto bail;
    }
    Tcl_DStringInit( &dsSDP );
    
    for (i = 0; i < nlines; i++) {			
        if (Tcl_ListObjIndex( interp, sdpListPtr, i, &objPtr ) != TCL_OK) {
            result = TCL_ERROR;
            goto bail;
        }
 	    charPtr = Tcl_UtfToExternalDString( gQTTclTranslationEncoding, 
 	            Tcl_GetString( objPtr ), -1 , &ds );
 	    Tcl_DStringAppendElement( &dsSDP, charPtr );
        Tcl_DStringFree( &ds );		    
        if (i < nlines - 1) {
            Tcl_DStringAppend( &dsSDP, "\r", 1 );
        }
    }    
	QTTclDebugPrintf( bcastPtr->interp, 2, 
	        "GetSDPFromData: SDP=%s", Tcl_DStringValue( &dsSDP ) );
    
	memset( &presParams, 0, sizeof(presParams) );
	memset( &mediaParams, 0, sizeof(mediaParams) );
    QTSInitializeMediaParams( &mediaParams );

	mediaParams.v.width = Long2Fix(320);
	mediaParams.v.height = Long2Fix(240);
	mediaParams.v.gWorld = gOffscreenGWorldPtr;
	mediaParams.v.gdHandle = GetMainDevice();
    
	presParams.version = kQTSPresParamsVersion1;
    presParams.flags = kQTSSendMediaFlag |  kQTSAutoModeFlag | 
            kQTSDontShowStatusFlag;
    presParams.timeScale = kDefaultPresTimeScale;
	presParams.mediaParams = &mediaParams;
    presParams.notificationProc = (QTSNotificationUPP) 
            NewQTSNotificationUPP( PresentationNotification );
	presParams.notificationRefCon = bcastPtr;

    dataLength = Tcl_DStringLength( &dsSDP );
	err = QTSNewPresentationFromData( kQTSSDPDataType, 
	        Tcl_DStringValue( &dsSDP ), &dataLength,
	        &presParams, &bcastPtr->presentation );    
	if (err != noErr) {
        Tcl_SetObjResult( interp,
        	    Tcl_NewStringObj( "Failed creating new presentation", -1) );
        goto bail;
    }
    
bail:

    Tcl_DStringFree( &dsSDP );		    
    return result;
}

static int      
GetSDPFromURL( 
        MovieBroadcast *bcastPtr, 
        Tcl_Obj *urlPtr )
{
	char			*charPtr;
    int             len;
	QTSMediaParams	mediaParams;
	QTSPresParams	presParams; 
    OSErr           err = noErr;
	Handle			urlDataRef = NULL;
	Tcl_DString     ds;
    int             result = TCL_OK;

	memset( &presParams, 0, sizeof(presParams) );
	memset( &mediaParams, 0, sizeof(mediaParams) );
    QTSInitializeMediaParams( &mediaParams );

	mediaParams.v.width = Long2Fix(320);
	mediaParams.v.height = Long2Fix(240);
	mediaParams.v.gWorld = gOffscreenGWorldPtr;
	mediaParams.v.gdHandle = GetMainDevice();
    
	presParams.version = kQTSPresParamsVersion1;
    presParams.flags = kQTSSendMediaFlag |  kQTSAutoModeFlag | 
            kQTSDontShowStatusFlag;
    presParams.timeScale = kDefaultPresTimeScale;
	presParams.mediaParams = &mediaParams;
    presParams.notificationProc = (QTSNotificationUPP) 
            NewQTSNotificationUPP( PresentationNotification );
	presParams.notificationRefCon = bcastPtr;

    charPtr = Tcl_UtfToExternalDString( gQTTclTranslationEncoding, 
            Tcl_GetString( urlPtr ), -1, &ds);
    len = Tcl_DStringLength( &ds );
	urlDataRef = MySafeNewHandle( len + 1, 1 );
	BlockMoveData( charPtr, *urlDataRef, len );
    err = QTSNewPresentationFromDataRef( urlDataRef,
            URLDataHandlerSubType, &presParams, &bcastPtr->presentation );
    Tcl_DStringFree( &ds );		    
	if (err != noErr) {
        Tcl_SetObjResult( bcastPtr->interp,
        	    Tcl_NewStringObj( "Failed creating new presentation", -1) );
        goto bail;
    }

bail:

	if (urlDataRef != NULL) {
		DisposeHandle( urlDataRef );
	}
    return result;
}

/*
 *--------------------------------------------------------------
 *
 * BroadcastEventProc --
 *
 *	    This procedure is invoked by the Tk dispatcher for various
 *	    events on squares.
 *
 * Results:
 *	    None.
 *
 * Side effects:
 *	    When the window gets deleted, internal structures get
 *	    cleaned up.  When it gets exposed, it is redisplayed.
 *
 *--------------------------------------------------------------
 */

static void
BroadcastEventProc(
    ClientData clientData,	/* Information about window. */
    XEvent *eventPtr )		/* Information about event. */
{
    MovieBroadcast *bcastPtr = (MovieBroadcast *) clientData;
    OSErr           err = noErr;

	QTTclDebugPrintf( bcastPtr->interp, 2, 
            "BroadcastEventProc: type=%d, flags=%1d%1d%1d%1d",
			eventPtr->type, (bcastPtr->flags >> 3) & 0x1, (bcastPtr->flags >> 2) & 0x1,
            (bcastPtr->flags >> 1) & 0x1, (bcastPtr->flags >> 0) & 0x1 );

	if ((eventPtr->type == Expose) && (eventPtr->xexpose.count == 0)) {

        goto redraw;
    } else if (eventPtr->type == ConfigureNotify) {

        goto redraw;
	} else if (eventPtr->type == UnmapNotify) {
        err = QTSPresSetGWorld( bcastPtr->presentation, kQTSAllStreams,
                gOffscreenGWorldPtr, NULL );
        bcastPtr->grafPtr = NULL;
	} else if (eventPtr->type == MapNotify) {
		bcastPtr->flags |= NEWGWORLD;
		goto redraw;
    } else if (eventPtr->type == DestroyNotify) {
	
		/*
		 * We are being destroyed.
		 */
		 
		if (bcastPtr->tkwin != NULL) {
			bcastPtr->tkwin = NULL;
			Tcl_DeleteCommandFromToken( bcastPtr->interp, bcastPtr->widgetCmd );
		}
		if (bcastPtr->flags & REDRAW_PENDING) {
			Tcl_CancelIdleCall( DisplayBroadcast, (ClientData) bcastPtr );
		}
		DestroyBroadcast( bcastPtr );
    }
	return;
	
redraw:
	
	/*
	 * Now we know that the event type was such that the widget needs to be redrawn.
	 * Schedule the redrawing procedure.
	 */
	 
	if ((bcastPtr->tkwin != NULL) && Tk_IsMapped(bcastPtr->tkwin) &&
			!(bcastPtr->flags & REDRAW_PENDING)) {
		Tcl_DoWhenIdle( DisplayBroadcast, (ClientData) bcastPtr );
		bcastPtr->flags |= REDRAW_PENDING;
	}
}

/*
 *--------------------------------------------------------------
 *
 * DisplayBroadcast --
 *
 *	This procedure redraws the contents of a square window.
 *	It is invoked as a do-when-idle handler, so it only runs
 *	when there's nothing else for the application to do.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Information appears on the screen.
 *
 *--------------------------------------------------------------
 */

static void
DisplayBroadcast(
        ClientData clientData )	/* Information about window. */
{
    MovieBroadcast      *bcastPtr = (MovieBroadcast *) clientData;
    Tk_Window           tkwin = bcastPtr->tkwin;
	CGrafPtr	 		saveWorld = NULL;
	GDHandle 			saveDevice = NULL;
	GWorldPtr			theGWorldPtr = NULL;
	Rect 				tkRect;
	MatrixRecord        matrix;
	OSErr               err = noErr;

	QTTclDebugPrintf( bcastPtr->interp, 2, 
	        "DisplayBroadcast: targetState=%2d, state=%2d",
	        bcastPtr->targetState, bcastPtr->state );

	bcastPtr->flags &= ~REDRAW_PENDING;
    if (!Tk_IsMapped( tkwin)) {
	    return;
    }
    if (bcastPtr->presentation == kQTSInvalidPresentation) {
        return;
    }
	GetGWorld( &saveWorld, &saveDevice );

	theGWorldPtr = QTTclMacGetDrawablePort( Tk_WindowId(tkwin) );
	MacSetPort( (GrafPtr) theGWorldPtr );
	//GetClip( saveRegion );
	QTTclMacWinBounds( (TkWindow *) tkwin, &tkRect );

    /*
     * Setting QTSPresSetGWorld stops presentation if rolling.
     */
     
    if (bcastPtr->grafPtr == NULL) {
        err = QTSPresSetGWorld( bcastPtr->presentation, kQTSAllStreams,
                theGWorldPtr, NULL );
        bcastPtr->grafPtr = theGWorldPtr;
    }            
    SetIdentityMatrix( &matrix );
    TranslateMatrix( &matrix, Long2Fix( tkRect.left ), Long2Fix( tkRect.top ) );
    err = QTSPresSetMatrix( bcastPtr->presentation, kQTSAllStreams, &matrix );

    err = QTSPresSetDimensions( bcastPtr->presentation, kQTSAllStreams,
            Long2Fix( tkRect.right - tkRect.left ), 
            Long2Fix( tkRect.bottom - tkRect.top ) );

    if (bcastPtr->targetState <= kPresentationStateIdle) {
        DrawBluescreen( bcastPtr );
    }

	SetGWorld( saveWorld, saveDevice );
}

/*
 *--------------------------------------------------------------
 *
 * DrawBluescreen --
 *
 *      Draws a "pause" screen when broadcaster idle.
 *
 * Results:
 *	    Standard TCL result.
 *
 * Side effects:
 *	    None.
 *
 *--------------------------------------------------------------
 */

static void
DrawBluescreen( MovieBroadcast *bcastPtr )
{
    short               x, y;
    short               dx;
    short               rad;
    short               faceNum;
    short               strWidth;
    short               width;
    int                 i;
    RGBColor            blueColor = {0x0000, 0x0000, 0xFFFF};
    RGBColor            blackColor = {0x0000, 0x0000, 0x0000};
    RGBColor            whiteColor = {0xFFFF, 0xFFFF, 0xFFFF};
    RGBColor            color;
	Rect 				tkRect;
	Rect                r;
	GWorldPtr			theGWorldPtr = NULL;
	CGrafPtr	 		saveWorld = NULL;
	GDHandle 			saveDevice = NULL;

	QTTclDebugPrintf( bcastPtr->interp, 2, "DrawBluescreen" );

	QTTclMacWinBounds( (TkWindow *) bcastPtr->tkwin, &tkRect );
	theGWorldPtr = QTTclMacGetDrawablePort( Tk_WindowId(bcastPtr->tkwin) );	
	GetGWorld( &saveWorld, &saveDevice );
	SetGWorld( theGWorldPtr, NULL );
#if TARGET_API_MAC_CARBON
    TkMacOSXSetUpClippingRgn( (Drawable) Tk_WindowId( bcastPtr->tkwin ) );
#else
    TkMacSetUpClippingRgn( (Drawable) Tk_WindowId( bcastPtr->tkwin ) );
#endif
    
	RGBForeColor( &blueColor );
	PenMode( patCopy );
	PaintRect( &tkRect );
	
	width = tkRect.right - tkRect.left;
	dx = width/15;
	x = (tkRect.right + tkRect.left)/2. - 5.5 * dx;
	r.top = tkRect.top + (1./3.) * (tkRect.bottom - tkRect.top);
	r.bottom = tkRect.top + (2./3.) * (tkRect.bottom - tkRect.top);
    for (i = 0; i <= 10; i++) {
        color.red = (10 - i)/10.0 * 0xFFFF;
        color.green = (10 - i)/10.0 * 0xFFFF;
        color.blue = (10 - i)/10.0 * 0xFFFF;
        RGBForeColor( &color );
        r.left = x + i * dx;
        r.right = r.left + dx;
    	PaintRect( &r );
    }
    rad = 0.4 * (tkRect.bottom - tkRect.top);
	x = tkRect.left + 0.5 * (tkRect.right - tkRect.left);
	y = tkRect.top + 0.5 * (tkRect.bottom - tkRect.top);
    r.left = x - rad;
    r.right = x + rad;
    r.top = y - rad;
    r.bottom = y + rad;
    RGBForeColor( &blackColor );
    FrameOval( &r );

    RGBForeColor( &whiteColor );
#if TARGET_API_MAC_CARBON
    faceNum = FMGetFontFamilyFromName( "\pHelvetica" );
#else
    GetFNum( "\pHelvetica", &faceNum );
#endif    		  
	TextFont( faceNum );
    TextSize( 24 );
	strWidth = StringWidth( "\pQuickTime TV" );
    if (strWidth > 0.8 * width) {
    	TextSize( 18 );
    	strWidth = StringWidth( "\pQuickTime TV" );
    }
    MoveTo( width/2 - strWidth/2, 32 );
    DrawString( "\pQuickTime TV" );
    
	SetGWorld( saveWorld, saveDevice );
}

/*
 *----------------------------------------------------------------------
 *
 * BroadcastDeletedProc --
 *
 *	    This procedure is invoked when a widget command is deleted.  If
 *	    the widget isn't already in the process of being destroyed,
 *	    this command destroys it.
 *
 * Results:
 *	    None.
 *
 * Side effects:
 *	    The widget is destroyed.
 *
 *----------------------------------------------------------------------
 */

static void
BroadcastDeletedProc(
    ClientData clientData )	/* Pointer to widget record for widget. */
{
    MovieBroadcast  *bcastPtr = (MovieBroadcast *) clientData;
    Tk_Window       tkwin = bcastPtr->tkwin;

    /*
     * This procedure could be invoked either because the window was
     * destroyed and the command was then deleted (in which case tkwin
     * is NULL) or because the command was deleted, and then this procedure
     * destroys the widget.
     */

    if (tkwin != NULL) {
        bcastPtr->tkwin = NULL;
	    Tk_DestroyWindow( tkwin );
    }
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyBroadcast --
 *
 *		Deletes a Broadcast Widget. Most things cleaned up with Tk_FreeOptions 
 *		but some things are freed up by me.
 *
 * Results:
 *		None.
 *
 * Side effects:
 *		Hopefully frees memory.
 *
 *----------------------------------------------------------------------
 */

static void 
DestroyBroadcast( ClientData clientData ) 
{
    MovieBroadcast  *bcastPtr = (MovieBroadcast *) clientData;


	QTTclDebugPrintf( bcastPtr->interp, 2, "DestroyBroadcast" );
	 
    RemoveFromBroadcastList( bcastPtr );
    PresentationFree( bcastPtr );



	Tk_FreeConfigOptions( (char *) bcastPtr, bcastPtr->optionTable, 
	        bcastPtr->tkwin );
	Tcl_EventuallyFree( (ClientData) bcastPtr, TCL_DYNAMIC );
}

static void
PresentationFree( MovieBroadcast *bcastPtr )
{
    BcastSourcerInfo    *sourcerPtr = NULL;
	OSErr               err = noErr;

    sourcerPtr = bcastPtr->sourcerInfoListPtr;
    while (sourcerPtr != NULL) {    
        SourcerFree( sourcerPtr );
        sourcerPtr = sourcerPtr->next;
    }

    err = QTSDisposePresentation( bcastPtr->presentation, 0 );
    bcastPtr->presentation = kQTSInvalidPresentation;
}

static void
SourcerFree( BcastSourcerInfo *sourcerPtr )
{
	OSErr               err = noErr;


    QTSDisposeStream( sourcerPtr->stream, 0 );
}

/*
 *----------------------------------------------------------------------
 *
 * PresentationNotification --
 *
 *      Notification callback function for the presentation.
 *
 * Results:
 *	    Apple error code.
 *
 * Side effects:
 *      Depending in notification type.
 *
 *----------------------------------------------------------------------
 */

static ComponentResult 
PresentationNotification(
        ComponentResult inErr, 
        OSType          inNotificationType,
	    void            *inNotificationParams, 
        void            *inRefCon )
{
    MovieBroadcast      *bcastPtr = (MovieBroadcast *) inRefCon;
	ComponentResult		err = noErr;
    char				tmp1[64];
	char 			    cmd[255];
    int                 callTclCommand = false;
	int                 result = TCL_OK;
	
	if (bcastPtr->tkwin == NULL) {
	    return noErr;
	}
	
    memset( tmp1, 0, 64 );
    
    switch (inNotificationType) {
        case kQTSPreviewAckNotification: strcpy( tmp1, "kQTSPreviewAckNotification" ); break;
        case kQTSPrerollAckNotification: strcpy( tmp1, "kQTSPrerollAckNotification" ); break;
        case kQTSStartAckNotification: strcpy( tmp1, "kQTSStartAckNotification" ); break;
        case kQTSStopAckNotification: strcpy( tmp1, "kQTSStopAckNotification" ); break;
        case kQTSStatusNotification: strcpy( tmp1, "kQTSStatusNotification" ); break;
        case kQTSErrorNotification: strcpy( tmp1, "kQTSErrorNotification" ); break;
        default: memcpy( tmp1, &inNotificationType, 4 );
    }

	QTTclDebugPrintf( bcastPtr->interp, 2, 
	        "PresentationNotification targetState=%2d, state=%2d, type=%s", 
	        bcastPtr->targetState, bcastPtr->state, tmp1 );

    if (bcastPtr->command != NULL) {
        memset( cmd, 0, 255 );
		strcpy( cmd, bcastPtr->command );
		strcat( cmd, " " );
		strcat( cmd, Tcl_GetCommandName(bcastPtr->interp, bcastPtr->widgetCmd) );
    }
    
	switch (inNotificationType)  {
	
		case kQTSPreviewAckNotification: {
        	if (bcastPtr->state != kPresentationStateStartingPreview) {
        		// we're in the wrong state to be getting this ack
        		// ignore it...

        	} else {
        		if (inErr != noErr) {
        			bcastPtr->targetState = kPresentationStateIdle;
        			bcastPtr->state = kPresentationStateIdle;
                    HandleErrorNotification( bcastPtr, "Preview", inErr, NULL );	
        		} else {
        			bcastPtr->state = kPresentationStatePreviewing;
            		strcat( cmd, " preview" );
            		callTclCommand = true;
        		}
        	}
			break;
		}	
		
		case kQTSPrerollAckNotification: {
        	if (bcastPtr->state != kPresentationStateStartingPreroll) {
        		// we're in the wrong state to be getting this ack
        		// ignore it...

        	} else {
        		if (inErr != noErr) {
        			bcastPtr->targetState = kPresentationStateIdle;
        			bcastPtr->state = kPresentationStateIdle;
                    HandleErrorNotification( bcastPtr, "Preroll", inErr, NULL );	
        		} else {
        			bcastPtr->state = kPresentationStateReadyToPlay;
        			if (bcastPtr->targetState > kPresentationStateReadyToPlay)  {
	                    bcastPtr->state = kPresentationStateStarting;
	                    err = QTSPresStart( bcastPtr->presentation, 
	                            kQTSAllStreams, 0L );
                		strcat( cmd, " preroll" );
                		callTclCommand = true;
	                }
	            }
			}
			break;
		}
			
		case kQTSStartAckNotification: {
        	if (bcastPtr->state != kPresentationStateStarting) {
        		// we're in the wrong state to be getting this ack
        		// ignore it...

        	} else {
        		if (inErr != noErr) {
        			bcastPtr->targetState = kPresentationStateIdle;
        			bcastPtr->state = kPresentationStateIdle;
                    HandleErrorNotification( bcastPtr, "Start", inErr, NULL );	
        		} else {
        			bcastPtr->state = kPresentationStatePlaying;
                		strcat( cmd, " start" );
                		callTclCommand = true;
        		}
        	}
			break;
		}
			
		case kQTSStopAckNotification: {
        	if (bcastPtr->state != kPresentationStateStarting) {
        		// we're in the wrong state to be getting this ack
        		// ignore it...

        	} else {
        		if (inErr != noErr) {
        			bcastPtr->targetState = kPresentationStateIdle;
        			bcastPtr->state = kPresentationStateIdle;
                    HandleErrorNotification( bcastPtr, "Preroll", inErr, NULL );
                } else {
            	    bcastPtr->state = kPresentationStateIdle;
               		strcat( cmd, " stop" );
               		callTclCommand = true;
            	}
            }
			break;
		}
			
		case kQTSStatusNotification: {
            if (inErr != noErr) {
                bcastPtr->targetState = kPresentationStateIdle;
                bcastPtr->state = kPresentationStateIdle;
                HandleErrorNotification( bcastPtr, "Status", inErr, NULL );
            }
			break;	
		}
	
		case kQTSErrorNotification: {
        	if (inErr != noErr) {
                HandleErrorNotification( bcastPtr, "Error", inErr, NULL );	
        	}
			break;
		}
		
		default: {
			// it's ok to get a notification you don't know about
			// just silently ignore it - this is not an error
			break;
		}

		// all you can do in a kQTSNewPresentationNotification
		// is store the presentation - do not make any presentation calls
		// when you that notification

	}
    if (Tk_IsMapped(bcastPtr->tkwin) && 
            (bcastPtr->state == kPresentationStateIdle)) {
	    DrawBluescreen( bcastPtr );
	}
	if ((bcastPtr->command != NULL) && callTclCommand) {
    	result = Tcl_Eval( bcastPtr->interp, cmd );
	}
	return err;
}

static ComponentResult 
HandleErrorNotification( MovieBroadcast *bcastPtr,
	    const char *inNotificationTypeString,
		ComponentResult inErr, 
		QTSErrorParams *inErrorParams)
{
	char 			cmd[255];
	int             result = TCL_OK;

	/*
	 * Tell the user about the error in some nice way.
	 */

    if (bcastPtr->command != NULL) {
        memset( cmd, 0, 255 );
		strcpy( cmd, bcastPtr->command );
		strcat( cmd, " " );
		strcat( cmd, Tcl_GetCommandName(bcastPtr->interp, bcastPtr->widgetCmd) );
		strcat( cmd, " error " );
		strcat( cmd, inNotificationTypeString );    
    }
	result = Tcl_Eval( bcastPtr->interp, cmd );

	return noErr;
}

/*
 *----------------------------------------------------------------------
 *
 * AddToBroadcastList, RemoveFromBroadcastList --
 *
 *      Adds/removes broadcast from our linked list.
 *
 * Results:
 *	    None.
 *
 * Side effects:
 *	    None.
 *
 *----------------------------------------------------------------------
 */

static void
AddToBroadcastList( MovieBroadcast *bcastPtr )
{
    MovieBroadcastList      *newEntryPtr = NULL;

    // check!!!!
    newEntryPtr = (MovieBroadcastList *) ckalloc( sizeof(MovieBroadcastList) );
    newEntryPtr->bcastPtr = bcastPtr;
	newEntryPtr->next = gMovieBroadcastListPtr;
	gMovieBroadcastListPtr = newEntryPtr;
}

static void
RemoveFromBroadcastList( MovieBroadcast *bcastPtr )
{
	MovieBroadcastList		*currentPtr = NULL;
	MovieBroadcastList		*prevPtr = NULL;

    /*
     * Find the entry in the linked list for a start.
     */

    // check!!!!
	currentPtr = gMovieBroadcastListPtr;
	while (currentPtr != NULL) {
		if (currentPtr->bcastPtr == bcastPtr) {
			if (prevPtr != NULL)  {
				prevPtr->next = currentPtr->next;
			}  else  {
				gMovieBroadcastListPtr = currentPtr->next;
			}
    	    ckfree( (char *) currentPtr );
			break;
		}
		prevPtr = currentPtr;
		currentPtr = currentPtr->next;
	}
}

void
BroadcastMacEvent( void )
{
    IdleAllPresentations();
}

/*
 *----------------------------------------------------------------------
 *
 * IdleAllPresentations, IdleSourcersForPresentation --
 *
 *      Idles presentations in our linked list.
 *      Also idles any sourcers for each presentation.
 *
 * Results:
 *	    None.
 *
 * Side effects:
 *	    Just gives cpu cycles.
 *
 *----------------------------------------------------------------------
 */

static void
IdleAllPresentations( void )
{
	int         	        clipInvalid = false;
    RgnHandle               rgnHandle = NULL;
	MovieBroadcastList		*currentPtr;
    MovieBroadcast          *bcastPtr;
    OSErr                   err = noErr;

	currentPtr = gMovieBroadcastListPtr;
	while (currentPtr != NULL) {
        bcastPtr = currentPtr->bcastPtr;
    	if ((bcastPtr->presentation != kQTSInvalidPresentation) && 
                (bcastPtr->presentation != NULL)) {

            clipInvalid = false;
            if (MyIsClipRegionInvalid( bcastPtr->tkwin )) {
                clipInvalid = true;
                QTTclDebugPrintf( bcastPtr->interp, 3, "\tMyIsClipRegionInvalid" );
            }
            if (clipInvalid) {
                rgnHandle = QTTclMacVisableClipRgn( (TkWindow *) bcastPtr->tkwin );
                if (rgnHandle == NULL) {
                    clipInvalid = false;
                }
            }
            if (clipInvalid) {
                err = QTSPresSetClip( bcastPtr->presentation, kQTSAllStreams,
                        rgnHandle );            
            }
    		QTSPresIdle( bcastPtr->presentation, NULL );
    		IdleSourcersForPresentation( bcastPtr );
    	}
		currentPtr = currentPtr->next;
    };
}

static void
IdleSourcersForPresentation( MovieBroadcast *bcastPtr )
{
    BcastSourcerInfo        *sourcerPtr = NULL;

    sourcerPtr = bcastPtr->sourcerInfoListPtr;
    while (sourcerPtr != NULL) {
        QTSSourcerIdle( sourcerPtr->sourcer, 0, 0, 0 );
        sourcerPtr = sourcerPtr->next;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * InstallBroadcastIdlingEventLoopTimer, TaskNeededSoonerCallback,
 * 		CarbonTimerNextTime --
 *
 *		Sets up Carbon timers to serve presentations.
 *
 * Results:
 *		.
 *
 * Side effects:
 *		Timers installed etc.
 *
 *----------------------------------------------------------------------
 */

#if TARGET_API_MAC_CARBON
static OSStatus
InstallBroadcastIdlingEventLoopTimer( void )
{
	OSStatus 		err;
    
    int kMinimumIdleDurationInMillis = 100;
	
	err = InstallEventLoopTimer( GetMainEventLoop(),
			0, 											/* firedelay */
			kEventDurationMillisecond * kMinimumIdleDurationInMillis, /* interval */
			NewEventLoopTimerUPP( PresentationCarbonTimer ),
			NULL, 		
			&gCarbonPresentationTimerRef );
	if (err != noErr) {
		/* 
		 * Install a callback that the Idle Manager will use when
		 * QuickTime needs to wake me up immediately.
		 */

//		err = QTInstallNextTaskNeededSoonerCallback(
//				NewQTNextTaskNeededSoonerCallbackUPP( TaskNeededSoonerCallback ),
//				1000, 			/* Millisecond timescale */
//				0, 				/* No flags */
//				(void *) gCarbonMovieTimerRef );
                
	}
	return err;
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * MoviePlayerCarbonTimer --
 *
 *		Timer callback to serve movies under Carbon.
 *
 * Results:
 *		None.
 *
 * Side effects:
 *		Calls 'MoviePlayerMacEvent'.
 *
 *----------------------------------------------------------------------
 */

#if TARGET_API_MAC_CARBON
static pascal void
PresentationCarbonTimer( EventLoopTimerRef theTimer, 
		void *userData )
{
    IdleAllPresentations();
}
#endif

static int 
PresentationExportToFile( MovieBroadcast *bcastPtr,
	    int objc, Tcl_Obj *CONST objv[] )
{
	OSErr				err = noErr;
	QTSExportParams		exportParams;
		
	if (bcastPtr->presentation != kQTSInvalidPresentation)  {
		memset( &exportParams, 0, sizeof(exportParams) );
		exportParams.version = kQTSExportParamsVersion1;
		exportParams.exportType = ksigMoviePlayer;
		exportParams.flagsIn = kQTSExportFlag_ShowDialog;
		exportParams.filterProc = NewQTSModalFilterUPP( MyModalFilterDialogProc );
        exportParams.filterProcRefCon = bcastPtr;
        
		err = QTSPresExport( bcastPtr->presentation, kQTSAllStreams, &exportParams );
		DisposeQTSNotificationUPP( exportParams.filterProc );
		if (err != noErr)  {

		}
	}  else  {

		err = qtsBadStateErr;
	}
	return err;
}

static int
SourcerObjCmd( Tcl_Interp *interp, 
        MovieBroadcast *bcastPtr,
	    int objc, Tcl_Obj *CONST objv[] ) 
{
    int             index;
	OSErr           err = noErr;
    int             result = TCL_OK;

    if (objc < 1) {
    	Tcl_WrongNumArgs( interp, 1, objv, "option ?arg arg...?" );
    	return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj( interp, objv[0], sourcerCommands, "command",
    	    0, &index) != TCL_OK) {
    	return TCL_ERROR;
    }

    Tcl_Preserve( (ClientData) bcastPtr );
    
    switch (index) {

        case kSourcerCmdAdd: {

            break;
        }
        case kSourcerCmdInfo: {
            SourcerGetInfo( interp, bcastPtr, kQTSAllStreams, objc - 1, objv + 1 );
            break;
        }
        case kSourcerCmdRemove: {

            break;
        }
	}
    Tcl_Release( (ClientData) bcastPtr );
    return result;

error:
    Tcl_Release( (ClientData) bcastPtr );
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * GetAllStreamMediaTypes --
 *
 *		Investigate the presentation for stream types.
 *
 * Results:
 *		None.
 *
 * Side effects:
 *		None.
 *
 *----------------------------------------------------------------------
 */

static void
GetAllStreamMediaTypes( QTSPresentation  presentation, int *haveAudio,
        int *haveVideo, int *haveText )
{
    int             i;
    int             numStreams;
    QTSStream       stream;
    OSType          mediaType;
    OSErr           err = noErr;

    *haveAudio = 0;
    *haveVideo = 0;
    *haveText = 0;
	numStreams = QTSPresGetNumStreams( presentation );
	
    for (i = 1; i <= numStreams; ++i) {
		stream = QTSPresGetIndStream( presentation, i );
        if (err == noErr) {
    	    err = QTSPresGetInfo( presentation, stream, 
    	            kQTSMediaTypeInfo, &mediaType );
            if (err == noErr) {
                switch (mediaType) {
                    case SoundMediaType: {
                        *haveAudio = 1;
                        break;
                    }
                    case VideoMediaType: {
                        *haveVideo = 1;
                        break;
                    }
                    case TextMediaType: {
                        *haveText = 1;
                        break;
                    }
                    case MPEGMediaType: {
                        /* empty */
                    }
                }
            }        
        }
    }
}

/*
 *----------------------------------------------------------------------
 *
 * SourcerGetInfo --
 *
 *		Returns a hierarchical list of various info for a particular stream,
 *      or for all if kQTSAllStreams.
 *
 * Results:
 *		Standard Tcl result.
 *
 * Side effects:
 *		None.
 *
 *----------------------------------------------------------------------
 */

static int
SourcerGetInfo( Tcl_Interp *interp, 
        MovieBroadcast *bcastPtr,
        QTSStream       inStream,   /* Investigate only this stream */
	    int             objc, 
	    Tcl_Obj         *CONST objv[] ) 
{
    int             i;
    int             numSourcers;
    long            lType;
    char            type8Char[8];
    QTSSourcer      sourcer = 0;
	Tcl_Obj			*listObjPtr;
	Tcl_Obj			*subListObjPtr;
	Tcl_Obj			*rectObjPtr;
	Rect            rect;
	QTSSourcerTimingParams  timingParams;
	UInt16          uint16;
	UInt32          uint32;
	Handle          handle = NULL;
	OSType          osType;
	Fixed           fixed;
	OSErr           err = noErr;
    int             result = TCL_OK;

    listObjPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );
    numSourcers = QTSPresGetNumSourcers( bcastPtr->presentation, 
            inStream );
    if (numSourcers == 0) {
        Tcl_SetObjResult( interp, Tcl_NewStringObj("", -1) );    
    }

    for (i = 1; i <= numSourcers; ++i) {
        subListObjPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );

        QTSPresGetIndSourcer( bcastPtr->presentation, inStream,
                i, &sourcer );

        err = GetSourcerType( sourcer, &osType );
        if (err == noErr) {
			Tcl_ListObjAppendElement( interp, subListObjPtr, 
			        Tcl_NewStringObj("-sourcertype", -1) );
    		lType = EndianU32_BtoN( osType );
    		memset( type8Char, '\0', 8 );		
    		memcpy( type8Char, &lType, 4 );
    		Tcl_ListObjAppendElement( interp, subListObjPtr, 
    				Tcl_NewStringObj(type8Char, -1) );
        }  
                      
        err = QTSSourcerGetInfo( sourcer, kQTSMediaTypeInfo, &osType );
        if (err == noErr) {
			Tcl_ListObjAppendElement( interp, subListObjPtr, 
			        Tcl_NewStringObj("-mediatype", -1) );
    		lType = EndianU32_BtoN( osType );
    		memset( type8Char, '\0', 8 );		
    		memcpy( type8Char, &lType, 4 );
    		Tcl_ListObjAppendElement( interp, subListObjPtr, 
    				Tcl_NewStringObj(type8Char, -1) );
        }                

        err = QTSSourcerGetInfo( sourcer, kQTSInfo_TargetFrameRate, &fixed );
        if (err == noErr) {
			Tcl_ListObjAppendElement( interp, subListObjPtr, 
			        Tcl_NewStringObj("-targetframerate", -1) );
    		Tcl_ListObjAppendElement( interp, subListObjPtr, 
    				Tcl_NewDoubleObj( Fix2X(fixed) ) );
        }                

        err = QTSSourcerGetInfo( sourcer, kQTSInfo_TargetDataRate, &uint32 );
        if (err == noErr) {
			Tcl_ListObjAppendElement( interp, subListObjPtr, 
			        Tcl_NewStringObj("-targetdatarate", -1) );
    		Tcl_ListObjAppendElement( interp, subListObjPtr, 
    				Tcl_NewLongObj( uint32 ) );
        }                

        err = QTSSourcerGetInfo( sourcer, kQTSInfo_InputDeviceName, &handle );
        if (err == noErr) {
			Tcl_ListObjAppendElement( interp, subListObjPtr, 
			        Tcl_NewStringObj("-inputdevicename", -1) );
    		Tcl_ListObjAppendElement( interp, subListObjPtr, 
    				Tcl_NewStringObj( *handle, -1) );
    		DisposeHandle( handle );
        }                

        err = QTSSourcerGetInfo( sourcer, kQTSInfo_InputSourceName, &handle );
        if (err == noErr) {
			Tcl_ListObjAppendElement( interp, subListObjPtr, 
			        Tcl_NewStringObj("-inputsourcename", -1) );
    		Tcl_ListObjAppendElement( interp, subListObjPtr, 
    				Tcl_NewStringObj( *handle, -1) );
    		DisposeHandle( handle );
        }                

        err = QTSSourcerGetInfo( sourcer, kQTSInfo_FullInputRect, &rect );
        if (err == noErr) {
			Tcl_ListObjAppendElement( interp, subListObjPtr, 
			        Tcl_NewStringObj("-fullinputrect", -1) );
            rectObjPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );
			Tcl_ListObjAppendElement( interp, rectObjPtr, 
			        Tcl_NewIntObj(rect.left) );
			Tcl_ListObjAppendElement( interp, rectObjPtr, 
			        Tcl_NewIntObj(rect.top) );
			Tcl_ListObjAppendElement( interp, rectObjPtr, 
			        Tcl_NewIntObj(rect.right) );
			Tcl_ListObjAppendElement( interp, rectObjPtr, 
			        Tcl_NewIntObj(rect.bottom) );
            Tcl_ListObjAppendElement( interp, subListObjPtr, rectObjPtr );        
        }
        
        /*
         * Video device settings.
         */

        err = QTSSourcerGetInfo( sourcer, kQTSInfo_VideoHue, &uint16 );
        if (err == noErr) {
			Tcl_ListObjAppendElement( interp, subListObjPtr, 
			        Tcl_NewStringObj("-videohue", -1) );
    		Tcl_ListObjAppendElement( interp, subListObjPtr, 
    				Tcl_NewIntObj( uint16 ) );
        }                
         
         
        /*
         * For tracks sourcers only.
         */
        
        err = QTSSourcerGetInfo( sourcer, kQTSInfo_SourcerTiming, &timingParams );
        if (err == noErr) {
			Tcl_ListObjAppendElement( interp, subListObjPtr, 
			        Tcl_NewStringObj("-timescale", -1) );
			Tcl_ListObjAppendElement( interp, subListObjPtr, 
			        Tcl_NewLongObj(timingParams.timeScale) );
			/* TimeValue64 !!!! wrong!!!!!! */
			Tcl_ListObjAppendElement( interp, subListObjPtr, 
			        Tcl_NewStringObj("-presentationstarttime", -1) );
			Tcl_ListObjAppendElement( interp, subListObjPtr, 
			        Tcl_NewLongObj(timingParams.presentationStartTime) );
			Tcl_ListObjAppendElement( interp, subListObjPtr, 
			        Tcl_NewStringObj("-presentationendtime", -1) );
			Tcl_ListObjAppendElement( interp, subListObjPtr, 
			        Tcl_NewLongObj(timingParams.presentationEndTime) );
			Tcl_ListObjAppendElement( interp, subListObjPtr, 
			        Tcl_NewStringObj("-presentationcurrenttime", -1) );
			Tcl_ListObjAppendElement( interp, subListObjPtr, 
			        Tcl_NewLongObj(timingParams.presentationCurrentTime) );
			Tcl_ListObjAppendElement( interp, subListObjPtr, 
			        Tcl_NewStringObj("-localstarttime", -1) );
			Tcl_ListObjAppendElement( interp, subListObjPtr, 
			        Tcl_NewLongObj(timingParams.localStartTime) );
			Tcl_ListObjAppendElement( interp, subListObjPtr, 
			        Tcl_NewStringObj("-localendtime", -1) );
			Tcl_ListObjAppendElement( interp, subListObjPtr, 
			        Tcl_NewLongObj(timingParams.localEndTime) );
			Tcl_ListObjAppendElement( interp, subListObjPtr, 
			        Tcl_NewStringObj("-localcurrenttime", -1) );
			Tcl_ListObjAppendElement( interp, subListObjPtr, 
			        Tcl_NewLongObj(timingParams.localCurrentTime) );
        }

        Tcl_ListObjAppendElement( interp, listObjPtr, subListObjPtr );        
    }
    Tcl_SetObjResult( interp, listObjPtr );

    return result;
}

static OSErr
GetSourcerType( QTSSourcer sourcer, OSType *typePtr )
{
    ComponentDescription    cd;
    OSErr                   err = noErr;
    
    err = GetComponentInfo( (Component) sourcer, &cd, NULL, NULL, NULL );
    if (err == noErr) {
        *typePtr = cd.componentSubType;
    }
    return err;
}

static OSErr
GetSeqGrabberFullInputRect( QTSPresentation	presentation,
        int *fullWidth, int *fullHeight )
{
    int             i;
    int             numSourcers;
    QTSSourcer      sourcer = 0;
	Rect            rect;
	OSErr           err = paramErr;

    numSourcers = QTSPresGetNumSourcers( presentation, 
            kQTSAllStreams );
    for (i = 1; i <= numSourcers; ++i) {
        QTSPresGetIndSourcer( presentation, kQTSAllStreams,
                i, &sourcer );                
        err = QTSSourcerGetInfo( sourcer, kQTSInfo_FullInputRect, &rect );
        if (err == noErr) {
    		MacOffsetRect( &rect, (short) -rect.left, (short) -rect.top );
            *fullWidth = rect.right;
            *fullHeight = rect.bottom;
            break;
        }
    }
    return err;
}

static int
StreamObjCmd( Tcl_Interp *interp, 
        MovieBroadcast *bcastPtr,
	    int objc, Tcl_Obj *CONST objv[] ) 
{
    int             numStreams;
    int             numSourcers;
    int             i;
    char	        type8Char[8];
    long            lType;
    QTSStream       stream;
	OSType		    trackType;
	OSErr           err = noErr;
	Tcl_Obj			*listObjPtr;
	Tcl_Obj			*subListObjPtr;
    int             result = TCL_OK;


	listObjPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );
	numStreams = QTSPresGetNumStreams( bcastPtr->presentation );
	
	for (i = 1; i <= numStreams; ++i) {
		stream = QTSPresGetIndStream( bcastPtr->presentation, i );
	    err = QTSPresGetInfo( bcastPtr->presentation, stream, 
	            kQTSMediaTypeInfo, &trackType );

		subListObjPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );
		Tcl_ListObjAppendElement( interp, subListObjPtr, 
		        Tcl_NewStringObj("-mediatype", -1) );
		lType = EndianU32_BtoN( trackType );
		memset( type8Char, '\0', 8 );		
		memcpy( type8Char, &lType, 4 );
		Tcl_ListObjAppendElement( interp, subListObjPtr, 
				Tcl_NewStringObj(type8Char, -1) );


		if (stream != kQTSInvalidStream) {
			numSourcers = QTSPresGetNumSourcers( bcastPtr->presentation, stream );
        }
		Tcl_ListObjAppendElement( interp, listObjPtr, subListObjPtr );
    }

	Tcl_SetObjResult( interp, listObjPtr );
    return result;
}

static int
PictureObjCmd( Tcl_Interp *interp, 
        MovieBroadcast *bcastPtr,
	    int objc, Tcl_Obj *CONST objv[] ) 
{
    int             dstWidth = 0;
    int             dstHeight = 0;
    PicHandle       picHand = NULL;
    OSErr           err = noErr;
    int             result = TCL_OK;

    err = QTSPresGetPicture( bcastPtr->presentation, kQTSAllStreams,
            &picHand );
    if ((err == noErr) && (picHand != NULL)) {
		result = ConvertPictureToTkPhoto( interp, picHand, 
				dstWidth, dstHeight, Tcl_GetString(objv[0]) );
    } else {
        Tcl_SetObjResult( interp,
        	    Tcl_NewStringObj( "Failed taking picture", -1) );
        result = TCL_ERROR;
    }
    if (picHand != NULL) {
		KillPicture( picHand );
    }
    return result;
}

static int
SetupTrackSourcerForStream(
        MovieBroadcast  *bcastPtr,
        QTSStream       inStream )
{
    QTSSourcer          sourcer = 0;
    ComponentDescription    cd;
    Component           component;
    OSType              trackType = SoundMediaType;
	//QTSSourcerCallbackProcParams	cParams;    
    OSErr               tmpErr = noErr;
    OSErr               err = noErr;
    BcastSourcerInfo	*sourcerInfoPtr = NULL;
    int                 result = TCL_OK;

	/*
	 * Ask the presentation if it already has a media type.
	 */
	 
	tmpErr = QTSPresGetInfo( bcastPtr->presentation, inStream, 
	        kQTSMediaTypeInfo, &trackType );
	if (tmpErr != noErr)  {
		trackType = SoundMediaType;
	}

	memset( &cd, 0, sizeof(cd) );
	cd.componentType = kQTSSourcerType;
	cd.componentSubType = kQTSMovieTrackSourcerType;
	cd.componentManufacturer = trackType;
	component = FindNextComponent( 0, &cd );
	if (component == 0)  {

        goto exit;
	}
	
	if ((err = OpenAComponent( component, &sourcer ) ) != noErr) {
	
	    goto exit;
	}
	if ((err = QTSSourcerInitialize( sourcer, NULL ) ) != noErr) {
	
	    goto exit;
	}
/*
	if (inHandler->loop) {
		memset(&loopParams, 0, sizeof(loopParams));
		loopParams.loopFlags = kQTSLoopFlag_Loop;
		loopParams.flagsMask = kQTSLoopFlag_Loop;
		QTSSourcerSetInfo( sourcer, kQTSInfo_Loop, &loopParams );
	}
*/
/*		
	memset( &cParams, 0, sizeof(cParams) );
	cParams.version = kQTSSourcerCallbackProcParamsVersion1;
	if (gMovieSourcingCallbackUPP == NULL)  {
		gMovieSourcingCallbackUPP = 
		        NewQTSNotificationUPP( MovieSourcingCallbackProc );
	}
	cParams.proc = gMovieSourcingCallbackUPP;
	cParams.refCon = inHandler;

	if ((err = QTSSourcerSetInfo( sourcer, kQTSInfo_SourcerCallbackProc, 
	        &cParams) ) != noErr) {
	        
	    goto error;
    }
*/    
    /*
     * Create and fill in a SourcerInfo object.
     */
     
    sourcerInfoPtr = (BcastSourcerInfo *) ckalloc(sizeof(BcastSourcerInfo) );
    memset( (void *) sourcerInfoPtr, 0, (sizeof(BcastSourcerInfo)) );
	sourcerInfoPtr->presentation = bcastPtr->presentation;
	sourcerInfoPtr->stream = inStream;
	sourcerInfoPtr->component = component;
	sourcerInfoPtr->sourcer = sourcer;
	sourcerInfoPtr->trackType = trackType;
	sourcerInfoPtr->track = NULL;
	sourcerInfoPtr->done = false;	

	/*
	 * The presentation now owns the sourcer and will dispose of it unless you 
	 * set the right flags.
	 */
	 
	if ((err = QTSPresAddSourcer( bcastPtr->presentation, inStream, 
	        sourcer, 0L) ) != noErr) {
	        
	    goto exit;
	}

exit:
	if (err != noErr)  {
		if (sourcer != 0)  {
			CloseComponent( sourcer );
		}
	}

    return result;
}


static pascal ComponentResult 
MovieSourcingCallbackProc(
        ComponentResult inErr,
		OSType          inSelector, 
		void            *ioParams, 
		void            *inRefCon )
{
    MovieBroadcast      *bcastPtr = (MovieBroadcast *) inRefCon;
	ComponentResult		err = noErr;
	
	if (inErr != noErr)  {

	}

	switch (inSelector)  {
		case 9999:

			break;	
	
		default:
			/* Ignore any selectors you don't know about. */
			break;
			
	}
	return err;
}

void 
QTSDisposeMediaParams( QTSMediaParams *inMediaParams )
{
	if (inMediaParams != NULL) {
	    if (inMediaParams->v.clip != NULL) {
	        DisposeRgn( inMediaParams->v.clip );
	    }
		DisposePtr( (Ptr) inMediaParams->a.frequencyBands );
	}
}

#if TARGET_OS_MAC && !TARGET_API_MAC_CARBON

static pascal Boolean
MyModalFilterDialogProc( DialogPtr dialogPtr, 
        EventRecord *event, 
        SInt16 *itemHit,
        long *refCon )
{
    Boolean         handled = false;
    WindowRef       eventWindow = NULL;
    WindowRef       dialogWindow = NULL;

#if TARGET_API_MAC_CARBON
    dialogWindow = GetDialogWindow( dialogPtr );
#else
    dialogWindow = dialogPtr;
#endif

    switch (event->what) {

        case updateEvt:
            eventWindow = (WindowRef) event->message;
            if ((eventWindow != NULL) && (eventWindow != dialogWindow)) {

        		/* 
        		 * Handle update events to background windows here. 
        		 * First, translate mac event to a number of tcl events.
        		 * If any tcl events generated, execute them until empty, and don't wait.
        		 */

        		if (TkMacConvertEvent( event )) {
        			while ( Tcl_DoOneEvent( TCL_IDLE_EVENTS | TCL_DONT_WAIT | TCL_WINDOW_EVENTS ) )
        				/* empty */
        				;
        		}
            }
            break;
    }
    return handled;
}

#endif // TARGET_OS_MAC && !TARGET_API_MAC_CARBON

#if TARGET_API_MAC_CARBON
static Boolean
MyModalFilterDialogProc( DialogPtr dialogPtr, 
        EventRecord *event, 
        SInt16 *itemHit,
        long *refCon )
{
    Boolean         handled = false;

    return handled;
}
#endif // TARGET_API_MAC_CARBON

/*---------------------------------------------------------------------------*/

