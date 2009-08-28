/*
 * SeqGrabber.c --
 *
 *		This file implements a sequence grabber for audio and video using QuickTime
 *		on the Macintosh. It is part of the QuickTimeTcl package which provides
 *		Tcl/Tk bindings for QuickTime.
 *
 * Copyright (c) 2000-2005  Mats Bengtsson
 *
 * $Id: SeqGrabber.c,v 1.17 2008/02/15 15:23:06 matben Exp $
 */

#include "SeqGrabber.h"
 
/* 
 * I store the seqence grabber widget struct in a global variable here because I couldn't 
 * find a way to get it into 'SeqGrabberMacEvent'; perhaps through EventRecord?
 */
 
static SeqGrabber *gSGrabPtr = NULL;

/*
 * Time interval for Carbon event timer (in secs) from SonOfMungGrab.c.
 * Q: How often should I call SGIdle in my application?
 * A: Generally, you should call SGIdle frequently enough for the type of content 
 * you are trying to capture. There's no single answer which will work optimally 
 * for every format. For example, if you are capturing DV at 29.97 fps, at a minimum 
 * you should be calling SGIdle at least as frequently as the frame rate. 
 * If you are capturing audio simultaneously, you should take this into account and 
 * increase the frequency as well.
 */
 
#if TARGET_API_MAC_CARBON
EventLoopTimerRef   gCarbonGrabberTimerRef = NULL;
const EventTime kCarbonGrabberTimerInterval = kEventDurationSecond / 60;
#endif

/*
 * Record to handle async image command.
 */
 
typedef struct AsyncImageHandlerRecord {
    SeqGrabber			*sgrabPtr;
    Tcl_TimerToken		timerToken;     /* Tcl's token for the timer handler. */ 
    Tcl_Obj 			*commandObjPtr;	/* Command to invoke. */
} AsyncImageHandlerRecord;

/*
 * For dispatching grabber commands.
 */

static char *allGrabberCmds[] = {
    "audiosettings",
	"cget", "configure", "image",
	"ispaused", "isrunning", "isstopped",
	"pause", "picture",
	"record", "start", "stop",
	"videosettings",
    (char *) NULL
};

enum {
    kGrabberCmdAudioSettings        = 0L,
    kGrabberCmdCget, 
    kGrabberCmdConfigure,
    kGrabberCmdImage,
    kGrabberCmdIsPaused,
    kGrabberCmdIsRunning,
    kGrabberCmdIsStopped,
    kGrabberCmdPause,
    kGrabberCmdPicture,
    kGrabberCmdRecord,
    kGrabberCmdStart,
    kGrabberCmdStop,
    kGrabberCmdVideoSettings
};

/*
 * We need a global variable to set if either or both of MoviePlayer or SeqGrabber is running.
 * This is used by our common Mac event procedure to figure out if we should call
 * MoviePlayerMacEvent and/or SeqGrabberMacEvent.
 * It is defined in MoviePlayer.c.
 */

extern long    gIsRunningFlags;

/* 
 * Information used for parsing configuration options.
 * Mask bits for options changed.
 */
 
enum {
    SEQGRAB_CONF_NEWGWORLD              	= (1L << 0),
    SEQGRAB_CONF_FILE                     	= (1L << 1),
    SEQGRAB_CONF_QUALITY                	= (1L << 2),
    SEQGRAB_CONF_OVERLAYIMAGE            	= (1L << 3),
    SEQGRAB_CONF_ZOOM                 		= (1L << 4),
    SEQGRAB_CONF_PLAYDURINGRECORD        	= (1L << 5),
    SEQGRAB_CONF_VIDEOCOMPRESSOR        	= (1L << 6),
    SEQGRAB_CONF_VOLUME                     = (1L << 7),
    SEQGRAB_CONF_FRAMERATE                  = (1L << 8)
};

/*
 * The following table defines the legal values for the -quality option.
 */

static CONST char *playbackQualityST[] = {
 	"fast", "normal", "high", (char *) NULL 
};
#define	SG_QUALITY_MODE_FAST 			0
#define	SG_QUALITY_MODE_NORMAL			1
#define	SG_QUALITY_MODE_HIGH			2

/*
 * The following table defines the legal values for the -size option.
 */

static CONST char *widgetSizeST[] = { 
	"quarter", "half", "full", (char *) NULL 
};
#define	SG_WIDGET_SIZE_QUARTER 		0
#define	SG_WIDGET_SIZE_HALF			1
#define	SG_WIDGET_SIZE_FULL			2

static Tk_OptionSpec SeqGrabberConfigSpecs[] = {
	{TK_OPTION_BOOLEAN, "-audio", "audio", "Audio",
		"1", -1, Tk_Offset(SeqGrabber, audio), 0, 
		(ClientData) NULL, 0},
	{TK_OPTION_BOOLEAN, "-audioonly", "audioOnly", "AudioOnly",
		"0", -1, Tk_Offset(SeqGrabber, audioOnly), 0, 
		(ClientData) NULL, 0},
	{TK_OPTION_STRING, "-file", "file", "File",
		NULL, -1, Tk_Offset(SeqGrabber, filename), TK_OPTION_NULL_OK, 
		(ClientData) NULL, SEQGRAB_CONF_FILE},
	{TK_OPTION_DOUBLE, "-framerate", "frameRate", "FrameRate",
		"0.0", -1, Tk_Offset(SeqGrabber, frameRate), 0, 
		(ClientData) NULL, SEQGRAB_CONF_FRAMERATE},
	{TK_OPTION_PIXELS, "-height", "height", "Height",
		"0", -1, Tk_Offset(SeqGrabber, height), 0, 
		(ClientData) NULL, SEQGRAB_CONF_NEWGWORLD},
#if TARGET_OS_MAC
	{TK_OPTION_STRING, "-overlayimage", "overlayImage", "OverlayImage",
		NULL, -1, Tk_Offset(SeqGrabber, overlayimage), TK_OPTION_NULL_OK, 
		(ClientData) NULL, SEQGRAB_CONF_OVERLAYIMAGE},
#endif
	{TK_OPTION_STRING_TABLE, "-quality", "quality", "Quality",
		"normal", -1, Tk_Offset(SeqGrabber, indQuality), 0,
		(ClientData) playbackQualityST, SEQGRAB_CONF_QUALITY},
	{TK_OPTION_BOOLEAN, "-playduringrecord", "playDuringRecord", "PlayDuringRecord",
		"1", -1, Tk_Offset(SeqGrabber, playDuringRecord), 0, 
		(ClientData) NULL, SEQGRAB_CONF_PLAYDURINGRECORD},
	{TK_OPTION_BOOLEAN, "-showfps", "showFPS", "ShowFPS",
		"0", -1, Tk_Offset(SeqGrabber, showFPS), 0, 
		(ClientData) NULL, 0},
	{TK_OPTION_STRING_TABLE, "-size", "size", "Size",
		"half", -1, Tk_Offset(SeqGrabber, indSize), 0, 
		(ClientData) widgetSizeST, SEQGRAB_CONF_NEWGWORLD},
	{TK_OPTION_STRING, "-videocompressor", "videoCompressor", "VideoCompressor",
		NULL, -1, Tk_Offset(SeqGrabber, videoCompressor), TK_OPTION_NULL_OK, 
		(ClientData) NULL, SEQGRAB_CONF_VIDEOCOMPRESSOR},
	{TK_OPTION_DOUBLE, "-volume", "volume", "Volume",
		"1.0", -1, Tk_Offset(SeqGrabber, volume), 0, 
		(ClientData) NULL, SEQGRAB_CONF_VOLUME},
	{TK_OPTION_PIXELS, "-width", "width", "Width",
		"0", -1, Tk_Offset(SeqGrabber, width), 0, 
		(ClientData) NULL, SEQGRAB_CONF_NEWGWORLD},
	{TK_OPTION_DOUBLE, "-zoom", "zoom", "Zoom",
		"1.0", -1, Tk_Offset(SeqGrabber, zoom), 0, 
		(ClientData) NULL, SEQGRAB_CONF_ZOOM},
    {TK_OPTION_END, (char *) NULL, (char *) NULL, (char *) NULL,
		(char *) NULL, 0, 0, 0, 0}
};  

/*
 * Prototypes for procedures referenced only in this file:
 */

static int 		SeqGrabberWidgetCmd( ClientData clientData, Tcl_Interp *interp, 
						int objc, Tcl_Obj *CONST objv[] );
static int 		ConfigureSeqGrabber( Tcl_Interp *interp, SeqGrabber *sgrabPtr, 
						int objc, Tcl_Obj *CONST objv[] );
static int 	    SeqGrabberComputeGeometry( SeqGrabber *sgrabPtr, int *width, 
						int *height, int setZoom );
static void 	SeqGrabberEventProc( ClientData clientData, XEvent *eventPtr );
static void 	SeqGrabberDeletedProc( ClientData clientData );
static void 	DestroySeqGrabber( SeqGrabber *sgrabPtr );
static void		SeqGrabberWorldChanged( ClientData instanceData );
static SeqGrabber *CreateSeqGrabber( Tk_Window tkwin );
static void 	DisplaySeqGrabber( ClientData clientData );
static void		SeqGrabberExitProc( ClientData clientData );
static int		SetInternalVideoOptions( SeqGrabber *sgrabPtr );
static int		SetInternalAudioOptions( SeqGrabber *sgrabPtr );
static void		AsyncImageHandlerProc( ClientData clientData );
						

/* ...and the non tk specific ones. */
static SeqGrabComponent 		MakeMySequenceGrabber( void );
static pascal ComponentResult 	MakeVideoChannel( SeqGrabComponent seqGrab, 
                                        SGChannel *sgchanVideo, const Rect *rect, 
                                        int playDuringRecord, Boolean willRecord );
static pascal ComponentResult 	MakeAudioChannel( SeqGrabComponent seqGrab, 
                                        SGChannel *sgchanSound, int playDuringRecord,
                                        Boolean willRecord );
static void						FreeVideoChannel(SeqGrabber *sgrabPtr);
static void						FreeAudioChannel(SeqGrabber *sgrabPtr);
static pascal ComponentResult 	SetupMyVideoBottlenecks( SGChannel sgchanVideo, 
										WindowPtr macWndPtr, SeqGrabber *sgrabPtr );
static pascal ComponentResult  	MyGrabFrameComplete( SGChannel sgChan, 
										short nBufferNum, Boolean *pbDone, long refConst );
static pascal ComponentResult   TakePicture( SeqGrabber *sgrabPtr, 
                                		PicHandle *thePic );
pascal Boolean          		MovableDialogModalFilter( DialogPtr theDialog, 
                                	    const EventRecord *event, short *itemHit, long refCon );
                                		                                
#if TARGET_API_MAC_CARBON
pascal void                     SeqGrabberCarbonTimer( EventLoopTimerRef theTimer, void *userData );
#endif

#ifdef _WIN32
LRESULT CALLBACK 		QTGrabberWinProc( HWND hWnd, UINT message, 
								WPARAM wParam, LPARAM lParam );
#endif


Tk_ClassProcs SeqGrabberProcs = {
        sizeof(Tk_ClassProcs),
		SeqGrabberWorldChanged,		/* geometryProc */
		NULL,						/* createProc */
		NULL						/* modalproc */
};

/*
 *----------------------------------------------------------------------
 *
 * SeqGrabberObjCmd --
 *
 *		Create a Sequence Grabber widget in QuickTime. 
 *
 * Results:
 *		Standard TCL result
 *
 * Side effects:
 *		Creates a command and allocates memory. 
 *
 *----------------------------------------------------------------------
 */

int
SeqGrabberObjCmd( 
		ClientData clientData, 		/* NULL */
		Tcl_Interp *interp, 
		int objc, 
		Tcl_Obj *CONST objv[] )
{ 
	Tk_Window 	        tkwin;
	SeqGrabber 	        *sgrabPtr;
	Tk_OptionTable 		optionTable;
	ComponentResult		err;
    GWorldPtr	        sgGWorldPtr = NULL;
#ifdef _WIN32
    HWND        		tempHwnd = NULL;
#endif
	
	if (objc < 2) {
		Tcl_WrongNumArgs( interp, 1, objv, "command ?arg arg...?" );
		return TCL_ERROR;
	}
	if (gSGrabPtr != NULL) {
		Tcl_SetObjResult( interp, 
				Tcl_NewStringObj("Can only have one simultaneous grabber", -1) );
		return TCL_ERROR;
	}
	
	/* Create new window */
	
	tkwin = Tk_CreateWindowFromPath( interp, Tk_MainWindow(interp), 
			Tcl_GetStringFromObj(objv[1], NULL), (char *) NULL);
	if (tkwin == NULL) {
		return TCL_ERROR;
	}

    /*
     * Create the option table for this widget class.  If it has already
     * been created, the cached pointer will be returned.
     */

	optionTable = Tk_CreateOptionTable( interp, SeqGrabberConfigSpecs );
	
	/* Set resource class */
	
	Tk_SetClass( tkwin, "SeqGrabber" );
	
	/* allocate memory */
	sgrabPtr = CreateSeqGrabber( tkwin );
	
	Tk_SetClassProcs( tkwin, &SeqGrabberProcs, (ClientData) sgrabPtr );

	sgrabPtr->tkwin = tkwin;
	sgrabPtr->display = Tk_Display(tkwin);
	sgrabPtr->interp = interp;
	sgrabPtr->widgetCmd = Tcl_CreateObjCommand( interp,
			Tk_PathName(sgrabPtr->tkwin), SeqGrabberWidgetCmd,
			(ClientData) sgrabPtr, SeqGrabberDeletedProc );
	sgrabPtr->optionTable = optionTable;
			
	/*
	 * Sequence Grabber specific attributes. 
	 */
	 
	sgrabPtr->seqGrab = NULL;
	sgrabPtr->sgChanVideo = NULL;
	sgrabPtr->sgChanAudio = NULL;
	sgrabPtr->filename = NULL;	
	sgrabPtr->willRecord = true;	/* Seems not to cause any harm even of not recording. */
	sgrabPtr->sgWidth = 0;
	sgrabPtr->sgHeight = 0;
	sgrabPtr->srcWidth = 0;
	sgrabPtr->srcHeight = 0;
	sgrabPtr->videoWidth = 0;
	sgrabPtr->videoHeight = 0;
	sgrabPtr->width = 0;
	sgrabPtr->height = 0;
	sgrabPtr->zoom = 1.0;
	sgrabPtr->audio = 1;
	sgrabPtr->audioOnly = 0;
	sgrabPtr->playDuringRecord = 1;
	sgrabPtr->frameCount = 0;
	sgrabPtr->showFPS = 0;
	sgrabPtr->latestTick = TickCount();
	sgrabPtr->padx = 0;
	sgrabPtr->pady = 0;
	sgrabPtr->background = NULL;
	sgrabPtr->videoBottle = 1;
#ifdef _WIN32
	sgrabPtr->videoBottle = 0;
#endif  
    sgrabPtr->imageAsyncProcObj = NULL;
    sgrabPtr->imageNameObj = NULL;
    sgrabPtr->asyncImageHandlerPtr = NULL;
	sgrabPtr->overlayimage = NULL;
	sgrabPtr->overlayPictHand = NULL;
	sgrabPtr->updatePictHand = NULL;
	sgrabPtr->flags = 0;
	 
	Tcl_CreateExitHandler( SeqGrabberExitProc, (ClientData) NULL );
		
	/* We want all the events */
	Tk_CreateEventHandler( sgrabPtr->tkwin,
		ExposureMask|StructureNotifyMask|FocusChangeMask|VisibilityChangeMask|KeyReleaseMask,
		SeqGrabberEventProc, (ClientData) sgrabPtr );

    /*
     * Windows need to have a window already to set the new winproc, and in either case,
     * we need to set the GWorld of the grabber before creating the video channel!
     */

    if (Tk_WindowId(sgrabPtr->tkwin) == None) {
        Tk_MakeWindowExist(sgrabPtr->tkwin);
    }	
	
	/*
	 * Windows specific code to create a Mac style graphics port, and associate
	 * it with a Windows HWND. Get the winproc given by tk, save it to be called
	 * later, and set our own winproc.
	 */
	 
#ifdef _WIN32
    tempHwnd = TkWinGetHWND( Tk_WindowId(sgrabPtr->tkwin) );
    CreatePortAssociation( tempHwnd, NULL, 0 );
    sgrabPtr->winEventProc = GetWindowLong( tempHwnd, GWL_WNDPROC );
    SetWindowLong( tempHwnd, GWL_WNDPROC, (LONG) QTGrabberWinProc );
#endif

	if (Tk_InitOptions( interp, (char *) sgrabPtr, optionTable, tkwin ) != TCL_OK) {
		Tk_DestroyWindow( sgrabPtr->tkwin );
		return TCL_ERROR;
	}
		
	/*
	 * Init the sequence grabber component.
	 */

	sgrabPtr->seqGrab = MakeMySequenceGrabber();
	if (sgrabPtr->seqGrab == NULL) {
		Tk_DestroyWindow( sgrabPtr->tkwin );
		Tcl_SetObjResult( interp, 
				Tcl_NewStringObj("Failed making sequence grabber", -1) );
		return TCL_ERROR;
	}
     
    /*
     * Windows (and Mac?) needs to have the SGSetGWorld before creating the video channel. 
     */
     
    sgGWorldPtr = QTTclMacGetDrawablePort( Tk_WindowId(sgrabPtr->tkwin) );
    if (sgGWorldPtr) {
	    SGSetGWorld( sgrabPtr->seqGrab, sgGWorldPtr, NULL );
    }
		
	/* 
	 * Configure the widget; parse the command line arguments and look for defaults
	 * in the resource database.
	 * IMPORTANT: need to have the channels prepared before configuring them!
	 * Channels created in 'ConfigureSeqGrabber'.
	 */

	if (ConfigureSeqGrabber( interp, sgrabPtr, objc - 2, objv + 2 ) != TCL_OK) {
		Tk_DestroyWindow(sgrabPtr->tkwin);
		return TCL_ERROR;
	}

	/* 
	 * Check if channels succesfully created. Perhaps accept audio channel only?
	 */
	
	if ((sgrabPtr->audioOnly == false) && (sgrabPtr->sgChanVideo == NULL)) {
		Tk_DestroyWindow( sgrabPtr->tkwin );
		Tcl_SetObjResult( interp, 
				Tcl_NewStringObj("Failed making video channel. Component not found", -1) );
		return TCL_ERROR;
	}
	
	/* Do we want a video bottleneck for... */
	
	if (sgrabPtr->videoBottle && (sgrabPtr->sgChanVideo != NULL)) {
#if TARGET_API_MAC_CARBON
		err = SetupMyVideoBottlenecks( sgrabPtr->sgChanVideo, 
				GetWindowFromPort( QTTclMacGetDrawablePort( Tk_WindowId(sgrabPtr->tkwin) ) ), 
				sgrabPtr );
#else
		err = SetupMyVideoBottlenecks( sgrabPtr->sgChanVideo, 
				(GrafPtr) QTTclMacGetDrawablePort( Tk_WindowId(sgrabPtr->tkwin) ), 
				sgrabPtr );
#endif
		if (err != noErr) {
			Tk_DestroyWindow( sgrabPtr->tkwin );
			Tcl_SetObjResult( interp, 
					Tcl_NewStringObj("Failed making video bottleneck", -1) );
			return TCL_ERROR;
		}
	}

	/* Store in a global variable, used in mac/win event procedure. */
	gSGrabPtr = sgrabPtr;

    if (sgrabPtr->audioOnly) {
        gIsRunningFlags |= SEQ_GRABBER_RUNS;
    }
    
#if TARGET_API_MAC_CARBON
        if (gCarbonGrabberTimerRef == NULL) {
            InstallEventLoopTimer( GetMainEventLoop(), 
                    0,									/* firedelay */
                    kCarbonGrabberTimerInterval,  		/* interval */
                    NewEventLoopTimerUPP( SeqGrabberCarbonTimer ), 
                    NULL, &gCarbonGrabberTimerRef );
        }
#endif
		
	/*
	 * Set the NEWGWORLD in 'flags' to require an update and to find the geometry.
	 */
	 
	sgrabPtr->flags |= NEWGWORLD;
	SeqGrabberWorldChanged( (ClientData) sgrabPtr );
	Tcl_SetObjResult( interp, Tcl_NewStringObj(Tk_PathName(sgrabPtr->tkwin), -1) );
	return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SeqGrabberWidgetCmd --
 *
 *		Command to run for  each widget
 *
 * Results:
 *		Normal TCL results
 *
 * Side effects:
 *		Memory allocated and/or freed, Mac Movie structures modified
 *
 *----------------------------------------------------------------------
 */
	
static int 
SeqGrabberWidgetCmd( ClientData clientData, Tcl_Interp *interp,
		int objc, Tcl_Obj *CONST objv[] ) 
{
	SeqGrabber 		*sgrabPtr = (SeqGrabber *) clientData;
    int             cmdIndex;
	int             usedUpdatePict;
	int				saveIsRunningFlag;
	int				boolean;
	PicHandle		thePic = NULL;
	ComponentResult res;
	Rect			aRect;
	Byte			isPaused;
	Tcl_Obj 		*resultObjPtr;
	int 			result = TCL_OK;

	if (objc < 2) {
		Tcl_WrongNumArgs( interp, 1, objv, "command ?arg arg...?" );
		return TCL_ERROR;
	}
	
	Tcl_Preserve( (ClientData) sgrabPtr );
	if (Tcl_GetIndexFromObj( interp, objv[1], allGrabberCmds, "command", 
			TCL_EXACT, &cmdIndex ) != TCL_OK ) {
	    result = TCL_ERROR;
	    goto error;
	}

    /*
     * Dispatch the movie command to the right branch.
     */
    
    switch(cmdIndex) {

        case kGrabberCmdCget: {
			if (objc != 3) {
				Tcl_WrongNumArgs(interp, 2, objv, "option");
			    result = TCL_ERROR;
				goto error;
			}
			resultObjPtr = Tk_GetOptionValue( interp, (char *) sgrabPtr,
				sgrabPtr->optionTable, objv[2], sgrabPtr->tkwin );
			if (resultObjPtr == NULL) {
				result = TCL_ERROR;
			} else {
				Tcl_SetObjResult( interp, resultObjPtr );
			}
			break;
        }

        case kGrabberCmdConfigure: {
			resultObjPtr = NULL;
			if (objc <= 3) {
				resultObjPtr = Tk_GetOptionInfo( interp, (char *) sgrabPtr,
					sgrabPtr->optionTable,
					(objc == 2) ? (Tcl_Obj *) NULL : objv[2],
					sgrabPtr->tkwin );
				if (resultObjPtr == NULL) {
					result = TCL_ERROR;
				} else {
					Tcl_SetObjResult( interp, resultObjPtr );
				}
			} else {
    		
    			/*
    			 * Change one or more attributes. Reschedule a new display via 
    			 * 'SeqGrabberWorldChanged'. Be sure to only set the argv values by the flag.
    			 * The NEWGWORLD bit in flags is set if we need a redisplay.
    			 */

    			result = ConfigureSeqGrabber( interp, sgrabPtr, objc - 2, objv + 2 );
    					
                /* 
                 * Only if we made a configuration that needs a redisplay. 
                 */
                 
    			if ((result == TCL_OK) && (sgrabPtr->flags & NEWGWORLD)) {		
    				SeqGrabberWorldChanged( (ClientData) sgrabPtr );
    			}
			}
			break;
        }
        
        case kGrabberCmdImage: {
            Tcl_CmdInfo		info;
            Tk_PhotoHandle 	photo;
            Tcl_Obj			*resultObjPtr;
            
    		if (objc != 4) {
                Tcl_WrongNumArgs( interp, 2, objv, "procName imageName" );
    			result = TCL_ERROR;
    			goto error;
    		}
            if (!Tk_IsMapped(sgrabPtr->tkwin)) {
                Tcl_SetObjResult( interp, Tcl_NewStringObj("seqgrabber must be displayed", -1) );
                result = TCL_ERROR;
                goto error;
            }
            if (Tcl_GetCommandInfo( interp, Tcl_GetString(objv[2]), &info ) == 0) {
                resultObjPtr = Tcl_NewStringObj("proc name not found: \"", -1);
                Tcl_AppendObjToObj( resultObjPtr, objv[2] );
                Tcl_SetObjResult( interp, resultObjPtr );
                result = TCL_ERROR;
                goto error;
            }

            photo = Tk_FindPhoto( interp, Tcl_GetString(objv[3]) );
            if (photo == NULL) {
                resultObjPtr = Tcl_NewStringObj("Image not found \"", -1);
                Tcl_AppendObjToObj( resultObjPtr, objv[3] );
                Tcl_SetObjResult( interp, resultObjPtr );
                result = TCL_ERROR;
                goto error;
            }
            if (sgrabPtr->imageAsyncProcObj != NULL) {
                Tcl_DecrRefCount( sgrabPtr->imageAsyncProcObj );
            }
            sgrabPtr->imageAsyncProcObj = (void *) objv[2];
            Tcl_IncrRefCount( objv[2] );

            sgrabPtr->imageNameObj = objv[3];
            Tcl_IncrRefCount( objv[3] );
            
            break;
        }

        case kGrabberCmdIsPaused: {
    		if (objc > 2) {
				Tcl_WrongNumArgs( interp, 2, objv, NULL );
    			result = TCL_ERROR;
    			goto error;
    		}
    		if (sgrabPtr->seqGrab) {
                SGGetPause( sgrabPtr->seqGrab, &isPaused );
                if (isPaused == seqGrabPause) {
					Tcl_SetObjResult( interp, Tcl_NewBooleanObj(1) );
                } else {
					Tcl_SetObjResult( interp, Tcl_NewBooleanObj(0) );
                }
    		} else {
				Tcl_SetObjResult( interp, 
						Tcl_NewStringObj("No sequence grabber", -1) );
    			result = TCL_ERROR;
    		}
		    break;
        }

        case kGrabberCmdIsRunning: {
    		if (objc > 2) {
				Tcl_WrongNumArgs( interp, 2, objv, NULL );
    			result = TCL_ERROR;
    			goto error;
    		}
    		if (sgrabPtr->seqGrab) {
    		    if (sgrabPtr->flags & ISRUNNING) {
					Tcl_SetObjResult( interp, Tcl_NewBooleanObj(1) );
                } else {
					Tcl_SetObjResult( interp, Tcl_NewBooleanObj(0) );
                }
    		} else {
				Tcl_SetObjResult( interp, 
						Tcl_NewStringObj("No sequence grabber", -1) );
    			result = TCL_ERROR;
    		}
		    break;
        }

        case kGrabberCmdIsStopped: {
    		if (objc > 2) {
				Tcl_WrongNumArgs( interp, 2, objv, NULL );
    			result = TCL_ERROR;
    			goto error;
    		}
    		if (sgrabPtr->seqGrab) {

                /* If not paused and not running. */

                SGGetPause( sgrabPtr->seqGrab, &isPaused );
                if (!(isPaused == seqGrabPause) && !(sgrabPtr->flags & ISRUNNING)) {
					Tcl_SetObjResult( interp, Tcl_NewBooleanObj(1) );
                } else {
					Tcl_SetObjResult( interp, Tcl_NewBooleanObj(0) );
                }
    		} else {
				Tcl_SetObjResult( interp, 
						Tcl_NewStringObj("No sequence grabber", -1) );
    			result = TCL_ERROR;
    		}
		    break;
        }

        case kGrabberCmdPause: {
    		if (objc != 3) {
				Tcl_WrongNumArgs( interp, 2, objv, "boolean" );
    			result = TCL_ERROR;
    			goto error;
    		}
    		if (sgrabPtr->seqGrab) {
                SGGetPause( sgrabPtr->seqGrab, &isPaused );
				if (Tcl_GetBooleanFromObj( interp, objv[2], &boolean ) != TCL_OK) {
					Tcl_AddErrorInfo( interp, 
							"\n	(processing pause command)" );
					result = TCL_ERROR;
					goto error;
				}
    			if (boolean) {
    								
        		    /* Take pict for updating and then pause if we are not already paused. */
        				
    				if (isPaused != seqGrabPause) {
    				    if (sgrabPtr->sgChanVideo) {
            			    res = TakePicture( sgrabPtr, &thePic );
            			    sgrabPtr->updatePictHand = thePic;
        				}
        				SGPause( sgrabPtr->seqGrab, seqGrabPause );
        				sgrabPtr->flags &= ~ISRUNNING;
                    	sgrabPtr->frameCount = 0;
    				}
    			} else {
    			    SGPause( sgrabPtr->seqGrab, seqGrabUnpause );
    				sgrabPtr->flags |= ISRUNNING;
    				
    				/*
    				 * Annulate update pict since invalid now.
    				 */
    				 
    				if (sgrabPtr->updatePictHand) {
    				    KillPicture( sgrabPtr->updatePictHand );
    				}
    				sgrabPtr->updatePictHand = NULL;
    				
    			}
    		} else {
				Tcl_SetObjResult( interp, 
						Tcl_NewStringObj("No sequence grabber", -1) );
    			result = TCL_ERROR;
    		}
		    break;
        }

        case kGrabberCmdPicture: {
		
    		/* 
    		 * Make a tk image from the sequence grabber display.
    		 * If we are paused there should already be a pict for us to use.
    		 */

    		if (sgrabPtr->seqGrab && sgrabPtr->sgChanVideo) {
    			if (objc != 3) {
					Tcl_WrongNumArgs( interp, 2, objv, "imageName" );
    				result = TCL_ERROR;
    				goto error;
    			}
                SGGetPause( sgrabPtr->seqGrab, &isPaused );
    			if ((isPaused == seqGrabPause) && sgrabPtr->updatePictHand) {
    			    thePic = sgrabPtr->updatePictHand;
    			    
    			    // fails on windows
    			    aRect = (**thePic).picFrame;
    			    usedUpdatePict = true;
    			} else {
    		        res = TakePicture( sgrabPtr, &thePic );
    			    usedUpdatePict = false;
    		    }
			
    			if (thePic == NULL) {
					Tcl_SetObjResult( interp, 
							Tcl_NewStringObj("Error getting pict from sequence grabber", -1) );
    				result = TCL_ERROR;
    				goto error;
    			}
    			result = ConvertPictureToTkPhoto( interp, thePic, 0, 0, 
    					Tcl_GetStringFromObj(objv[2], NULL) );
    			
    			/* Do not kill the pict if we used the update pict (updatePictHand)! */
    			if (!usedUpdatePict) {
    			    KillPicture(thePic);
    			}
    		} else {
				Tcl_SetObjResult( interp, 
						Tcl_NewStringObj("No sequence grabber", -1) );
    			result = TCL_ERROR;
    		}
		    break;
        }

        case kGrabberCmdRecord: {
		
    		/* 
    		 * Start recording to file specified with -file option.
    		 */

    	    if (Tcl_IsSafe( interp )) {
				Tcl_SetObjResult( interp, 
						Tcl_NewStringObj("\"record\" not allowed in a safe interpreter", -1) );
    			result = TCL_ERROR;
    		    goto error;
    	    } 
    		if (sgrabPtr->seqGrab) {
    			if (objc != 2) {
					Tcl_WrongNumArgs( interp, 2, objv, NULL );
    				result = TCL_ERROR;
    				goto error;
    			}
    			if (sgrabPtr->filename == NULL){
					Tcl_SetObjResult( interp, 
							Tcl_NewStringObj("Need a file specified with -file option", -1) );
    				result = TCL_ERROR;
    				goto error;
    			}
    			if (noErr != SGStartRecord( sgrabPtr->seqGrab )) {
					Tcl_SetObjResult( interp, 
							Tcl_NewStringObj("Failed starting recording", -1) );
    				result = TCL_ERROR;
    				goto error;
    			}
    			sgrabPtr->frameCount = 0;
    		} else {
				Tcl_SetObjResult( interp, 
						Tcl_NewStringObj("No sequence grabber", -1) );
    			result = TCL_ERROR;
    		}
		    break;
        }

        case kGrabberCmdStart: {
    		if (objc > 2) {
				Tcl_WrongNumArgs( interp, 2, objv, NULL );
    			result = TCL_ERROR;
    			goto error;
    		}
    		if (sgrabPtr->seqGrab) {
    			SGStartPreview( sgrabPtr->seqGrab );
    			sgrabPtr->flags |= ISRUNNING;
    			sgrabPtr->frameCount = 0;
    			
    			/*
    			 * Annulate update pict since invalid now.
    			 */
    			 
    			if (sgrabPtr->updatePictHand) {
    			    KillPicture(sgrabPtr->updatePictHand);
    			}
    			sgrabPtr->updatePictHand = NULL;
    		} else {
				Tcl_SetObjResult( interp, 
						Tcl_NewStringObj("No sequence grabber", -1) );
    			result = TCL_ERROR;
    		}
		    break;
        }

        case kGrabberCmdStop: {
    		if (objc > 2) {
				Tcl_WrongNumArgs( interp, 2, objv, NULL );
    			result = TCL_ERROR;
    			goto error;
    		}
    		if (sgrabPtr->seqGrab) {
    			
    			/*
    			 * Take a Pict to have to show when getting an update event. Same when pausing.
    			 */

                if (sgrabPtr->sgChanVideo) {
        			res = TakePicture( sgrabPtr, &thePic );
        			sgrabPtr->updatePictHand = thePic;
    			}
                SGPause( sgrabPtr->seqGrab, seqGrabPause );
    			SGStop( sgrabPtr->seqGrab );
    			sgrabPtr->flags &= ~ISRUNNING;
                sgrabPtr->frameCount = 0;
    		} else {
				Tcl_SetObjResult( interp, 
						Tcl_NewStringObj("No sequence grabber", -1) );
    			result = TCL_ERROR;
    		}
		    break;
        }

        case kGrabberCmdAudioSettings:
        case kGrabberCmdVideoSettings: {
    		if (objc > 2) {
				Tcl_WrongNumArgs( interp, 2, objv, NULL );
    			result = TCL_ERROR;
    			goto error;
    		}
    		if (sgrabPtr->seqGrab) {
    		
    			/*
    			 * Standard dialog boxes for video or audio grabber settings.
    			 * Pause grabbing during settings. Made automatically.
    			 */

    			if ((sgrabPtr->updatePictHand == NULL) && (sgrabPtr->sgChanVideo)) {
        			res = TakePicture( sgrabPtr, &thePic );
        			sgrabPtr->updatePictHand = thePic;
        		}
    			res = SGGetPause( sgrabPtr->seqGrab, &isPaused );
    			saveIsRunningFlag = sgrabPtr->flags & ISRUNNING;
        		sgrabPtr->flags &= ~ISRUNNING;
			 
    			if (cmdIndex == kGrabberCmdVideoSettings) {
    				if (sgrabPtr->sgChanVideo != NULL) {
    			        /* pausing seems not necessary */
    			        //res = SGPause( sgrabPtr->seqGrab, seqGrabPause );
#if TARGET_API_MAC_CARBON
    					res = SGSettingsDialog( sgrabPtr->seqGrab, sgrabPtr->sgChanVideo, 
    					        0, NULL, 0, NewSGModalFilterUPP( MovableDialogModalFilter ), 
    					        (long) sgrabPtr );
#else
    					res = SGSettingsDialog( sgrabPtr->seqGrab, sgrabPtr->sgChanVideo, 
    					        0, NULL, 0, NewSGModalFilterProc( MovableDialogModalFilter ), 
    					        (long) sgrabPtr );
#endif

    					/* Could also use this one to hide compressor settings. */
    					/*
    					res = SGSettingsDialog( sgrabPtr->seqGrab, sgrabPtr->sgChanVideo, 
    					        0, NULL, seqGrabSettingsPreviewOnly, NULL, 0 );
    					*/
    			        if (res == noErr) {
							Tcl_SetObjResult( interp, Tcl_NewBooleanObj(1) );
    			        } else if (res == userCanceledErr) {
							Tcl_SetObjResult( interp, Tcl_NewBooleanObj(0) );
    			        } else {
    				        CheckAndSetErrorResult( interp, res );
    						result = TCL_ERROR;
    			        }
    				} else {
						Tcl_SetObjResult( interp, 
								Tcl_NewStringObj("No video channel", -1) );
    					result = TCL_ERROR;
    				}
                    if (TCL_OK != SetInternalVideoOptions( sgrabPtr )) {
                        /* ??? */
                    }                    
    			} else if (cmdIndex == kGrabberCmdAudioSettings) {
    				if (sgrabPtr->sgChanAudio != NULL) {
#if TARGET_API_MAC_CARBON
    					res = SGSettingsDialog( sgrabPtr->seqGrab, sgrabPtr->sgChanAudio, 
    					        0, NULL, seqGrabSettingsPreviewOnly, 
    							NewSGModalFilterUPP( MovableDialogModalFilter ), 
    							(long) sgrabPtr );
#else
    					res = SGSettingsDialog( sgrabPtr->seqGrab, sgrabPtr->sgChanAudio, 
    					        0, NULL, seqGrabSettingsPreviewOnly, 
    							NewSGModalFilterProc( MovableDialogModalFilter ), 
    							(long) sgrabPtr );
#endif
    			        if (res == noErr) {
							Tcl_SetObjResult( interp, Tcl_NewBooleanObj(1) );
    			        } else if (res == userCanceledErr) {
							Tcl_SetObjResult( interp, Tcl_NewBooleanObj(0) );
    			        } else {
    				        CheckAndSetErrorResult( interp, res );
    						result = TCL_ERROR;
    			        }
    				} else {
						Tcl_SetObjResult( interp, 
								Tcl_NewStringObj("No audio channel", -1) );
    					result = TCL_ERROR;
    				}
                    if (TCL_OK != SetInternalAudioOptions( sgrabPtr )) {
                        /* ??? */
                    }
    			}
    	        res = SGPause( sgrabPtr->seqGrab, isPaused );
    	        
    	        /* set running flag to what it was */
    	        if (saveIsRunningFlag & ISRUNNING) {
    				sgrabPtr->flags |= ISRUNNING;
    	        } else {
    	    		sgrabPtr->flags &= ~ISRUNNING;
    	        }
    	        sgrabPtr->frameCount = 0;
    		} else {
				Tcl_SetObjResult( interp, 
						Tcl_NewStringObj("No sequence grabber", -1) );
    			result = TCL_ERROR;
    		}
        }
	}
	
error:
	
	Tcl_Release( (ClientData) sgrabPtr );
	return result;
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureSeqGrabber --
 *
 *		This procedure is called to process an objv/objc list, plus
 *		the Tk option database, in order to configure (or
 *		reconfigure) a sequence grabber widget.
 *
 * Results:
 *		The return value is a standard Tcl result.  If TCL_ERROR is
 *		returned, then the interp's result contains an error message.
 *
 * Side effects:
 *		Configuration information, such as text string, colors, font,
 *		etc. get set for sgrabPtr;  old resources get freed, if there
 *		were any.
 *      Only if the configuration means that a (re)display is necessary,
 *      the NEWGWORLD flag is set. This triggers a call to 'SeqGrabberWorldChanged'.
 *
 *----------------------------------------------------------------------
 */

static int 
ConfigureSeqGrabber( Tcl_Interp *interp, SeqGrabber *sgrabPtr, 
		int objc, Tcl_Obj *CONST objv[] ) 
{
    int             	width, height;
	int 				mask = 0L;
    int 				ierror;
	long				qflag = 0L;
    Boolean         	wantVideo, wantAudio;
	Tcl_Obj 			*resultObjPtr = NULL;
    Tcl_Obj 			*errorResult = NULL;
	Tk_PhotoHandle 		tkPhoto = NULL;
	Tk_SavedOptions 	savedOptions;
	Rect            	winRect;
	FSSpec				fss;
	PicHandle 			thePic = NULL;
	ComponentResult		err;
	OSErr				osErr;
	
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

			if (Tk_SetOptions( interp, (char *) sgrabPtr, sgrabPtr->optionTable, objc,
					objv, sgrabPtr->tkwin, &savedOptions, &mask) != TCL_OK ) {
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
	    
		/* 
		 * Init the video and audio channel components.
		 * Size of widget. Need to find the geometry first??? (SeqGrabberComputeGeometry).
		 * Just take a standard size for now. 
		 */
		 
		if (sgrabPtr->audioOnly == false) {
		    wantVideo = true;
	    } else {
		    wantVideo = false;
	    }
        if (sgrabPtr->audio) {
		    wantAudio = true;
        } else {
		    wantAudio = false;
        }
		if (wantVideo && sgrabPtr->sgChanVideo == NULL) {
	    	MacSetRect( &winRect, 0, 0, 160, 120 );	
	     	err = MakeVideoChannel( sgrabPtr->seqGrab,  /* This is the actual 
	     											 * sequence grabber component. */
	    			&(sgrabPtr->sgChanVideo),       /* The video channel component. */
	    			&winRect,                       /* A temporary rectangle 
	    											 * for the video bounds. */
	    			sgrabPtr->playDuringRecord,
	    			sgrabPtr->willRecord );         /* Prepare for recording? */
            if (err != noErr) {
    			CheckAndSetErrorResult( interp, err );
                continue;
            }
            if (TCL_OK != SetInternalVideoOptions( sgrabPtr )) {
                continue;
            }
        }
        if (wantAudio && sgrabPtr->sgChanAudio == NULL) {
	     	err = MakeAudioChannel( sgrabPtr->seqGrab,  /* This is the actual 
	     											 * sequence grabber component. */
	    			&(sgrabPtr->sgChanAudio),       /* The audio channel component. */
	    			sgrabPtr->playDuringRecord,
	    			sgrabPtr->willRecord );         /* Prepare for recording? */
            if (err != noErr) {
    			CheckAndSetErrorResult( interp, err );
                continue;
            }
            if (TCL_OK != SetInternalAudioOptions( sgrabPtr )) {
                continue;
            }
        } else if (!wantAudio && sgrabPtr->sgChanAudio != NULL) {
            FreeAudioChannel( sgrabPtr );
        }
		
		/*
		 * Check possible inconsistencies of the options. Return error if found any.
		 */
		
		if (sgrabPtr->zoom < 1.0) {
			sgrabPtr->zoom = 1.0;
		}
		if ((sgrabPtr->width > 0) && (sgrabPtr->height > 0)) {
			Tcl_SetObjResult( interp, 
					Tcl_NewStringObj("Cannot set both width and height. Set one to 0", -1) );
			continue;
		}
		
		/* 
		 * Geometry and zoom. If any of width, height, size, or zoom changed.
		 * We only need to set the flag bit NEWGWORLD for the new geometry to be
		 * found and requested in .
		 */
	
		if ((mask & SEQGRAB_CONF_NEWGWORLD) || (mask & SEQGRAB_CONF_ZOOM)) {
		    if (SeqGrabberComputeGeometry( sgrabPtr, &width, &height, 1 ) != TCL_OK) {
				continue;
	        }
		    sgrabPtr->flags |= NEWGWORLD;
		}

		if ((mask & SEQGRAB_CONF_PLAYDURINGRECORD) && 
				(sgrabPtr->sgChanVideo != NULL)) {
	        long    lUsage = 0;

	        SGGetChannelUsage( sgrabPtr->sgChanVideo, &lUsage );
	        if (sgrabPtr->playDuringRecord) {
	            lUsage |= seqGrabPlayDuringRecord;
	        } else {
	            lUsage &= ~seqGrabPlayDuringRecord;
	        }
	        err = SGSetChannelUsage( sgrabPtr->sgChanVideo, lUsage );
			if (err != noErr) {
    			CheckAndSetErrorResult( interp, err );
				continue;
			}
	    }
	    	
		/* 
		 * Set quality. 
		 */

		if ((mask & SEQGRAB_CONF_QUALITY) && (sgrabPtr->sgChanVideo != NULL)) {
			switch (sgrabPtr->indQuality) {
				case SG_QUALITY_MODE_FAST: 
					qflag = channelPlayFast;
					break;
				case SG_QUALITY_MODE_NORMAL: 
					qflag = channelPlayNormal;
					break;
				case SG_QUALITY_MODE_HIGH: 
					qflag = channelPlayHighQuality;
					break;
			}
			qflag |= channelPlayAllData;
			err = SGSetChannelPlayFlags( sgrabPtr->sgChanVideo, qflag );
			if (err != noErr) {
    			CheckAndSetErrorResult( interp, err );
				continue;
			}
		}
	
		/*
		 * Specify an output file when recording.
		 */
		 
		if (mask & SEQGRAB_CONF_FILE) {
			osErr = QTTclNativePathNameToFSSpec( interp, sgrabPtr->filename, &fss );
			if ((osErr != fnfErr) && (osErr != noErr)) {
				resultObjPtr = Tcl_NewStringObj("Failed making FSSpec from filename \"", -1);
				Tcl_AppendStringsToObj( resultObjPtr, sgrabPtr->filename, "\"", 
						(char *) NULL);
				Tcl_SetObjResult( interp, resultObjPtr );
				continue;
			}
			if (noErr != SGSetDataOutput( sgrabPtr->seqGrab, &fss, seqGrabToDisk )) {
				resultObjPtr = Tcl_NewStringObj("Failed setting data output file \"", -1);
				Tcl_AppendStringsToObj( resultObjPtr, sgrabPtr->filename, "\"", 
						(char *) NULL);
				Tcl_SetObjResult( interp, resultObjPtr );
				continue;
			}
		}
        
		if (mask & SEQGRAB_CONF_VIDEOCOMPRESSOR) {
            if (sgrabPtr->sgChanVideo == NULL) {
				resultObjPtr = Tcl_NewStringObj(
                        "Can't configure video compressor without video channel", -1);
				Tcl_SetObjResult( interp, resultObjPtr );
				continue;
            } else {
                OSType				compressorType;
                unsigned long		lType;
        
                memcpy( &lType, sgrabPtr->videoCompressor, 4 );
                compressorType = EndianU32_NtoB( lType );
                err = SGSetVideoCompressorType( sgrabPtr->sgChanVideo, compressorType );
                if (err != noErr) {
                    CheckAndSetErrorResult( interp, err );
                    continue;
                }
            }
        }
        
		if (mask & SEQGRAB_CONF_FRAMERATE) {
            if (sgrabPtr->sgChanVideo == NULL) {
				resultObjPtr = Tcl_NewStringObj(
                        "Can't configure video compressor without video channel", -1);
				Tcl_SetObjResult( interp, resultObjPtr );
				continue;
            } else {
                Fixed		frameRate;
                
                frameRate = X2Fix( sgrabPtr->frameRate );
                err = SGSetFrameRate( sgrabPtr->sgChanVideo, frameRate );
                if (err != noErr) {
                    CheckAndSetErrorResult( interp, err );
                    continue;
                }
            }
        }
        
		if (mask & SEQGRAB_CONF_VOLUME) {
            short        		volume;

            volume = (short) ( 256.0 * sgrabPtr->volume );
            err = SGSetChannelVolume( sgrabPtr->sgChanAudio, volume );
            if (err != noErr) {
                CheckAndSetErrorResult( interp, err );
                continue;
            }
        }
        
		/*
		 * If we want to overlay a tk image in one of the corners, translate it to
		 * a mac picture.
		 */
		 
		/* Beware! must not be done during previewing, only before started previewing. */
		
		if (Tk_IsMapped(sgrabPtr->tkwin) && sgrabPtr->overlayimage && 
				(sgrabPtr->overlayPictHand == NULL)) {
			Tcl_SetObjResult( interp, 
					Tcl_NewStringObj("Overlayimage must be made before window is mapped", -1) );
			continue;
		}
		 
		if (sgrabPtr->overlayimage && (sgrabPtr->overlayPictHand == NULL)) {
			tkPhoto = Tk_FindPhoto( interp, sgrabPtr->overlayimage );
			if (!tkPhoto) {
				Tcl_SetObjResult( interp, 
						Tcl_NewStringObj("Image not found", -1) );
				continue;
			}
			if (ConvertTkPhotoToPicture( interp, tkPhoto, &thePic ) != TCL_OK) {
				Tcl_SetObjResult( interp, 
						Tcl_NewStringObj("Error converting image to Picture", -1) );
				continue;
			}
			sgrabPtr->overlayPictHand = thePic;
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

static int
SetInternalVideoOptions( SeqGrabber *sgrabPtr )
{
    OSType				compressorType;
    Fixed        		frameRate;
	ComponentResult		err;
    
    if (sgrabPtr->sgChanVideo == NULL) {
        return TCL_OK;
    }
    
    err = SGGetVideoCompressorType( sgrabPtr->sgChanVideo, &compressorType );
    if (err == noErr) {
        unsigned long			lType;
            
        lType = EndianU32_BtoN( compressorType );
        sgrabPtr->videoCompressor = (char *) ckalloc(5);
        memset( (void *) sgrabPtr->videoCompressor, 0, 5 );
        memcpy( sgrabPtr->videoCompressor, &lType, 4 );
    } else {
        return TCL_ERROR;
    }
    
    err = SGGetFrameRate( sgrabPtr->sgChanVideo, &frameRate );
    if (err == noErr) {
        sgrabPtr->frameRate = Fix2X( frameRate );
    } else {
        return TCL_ERROR;
    }
    return TCL_OK;
}

static int
SetInternalAudioOptions( SeqGrabber *sgrabPtr )
{
    short        		volume;
	ComponentResult		err;
    
    if (sgrabPtr->sgChanAudio == NULL) {
        return TCL_OK;
    }

    /*
     *  The volume setting of the channel represented as a 16-bit, fixed-point number. The high-			 
     *  order 8 bits contain the integer part of the value; the low-order 8 bits 
     *  contain the fractional part. Volume values range from -1.0 to 1.0. 
     *  Negative values play no sound but preserve the absolute value of the volume setting.
     */
        
    err = SGGetChannelVolume( sgrabPtr->sgChanAudio, &volume );
    if (err == noErr) {
        sgrabPtr->volume = volume/256.0;
    } else {
        return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SeqGrabberComputeGeometry --
 *
 *		Finds the widget size to request at tk. It manages the zooming 
 *		also.
 *
 * Results:
 *		The return value is a standard Tcl result. If TCL_ERROR is
 *		returned, then the interp's result contains an error message.
 *
 * Side effects:
 *		Returns width and height in function arguments. Note that these 
 *		are the actual width and height to request from tk, and not the 
 *		options. Set the video rectangle in source coordinates (zoom) 
 *		if setZoom is true.
 *
 *----------------------------------------------------------------------
 */
	
static int 
SeqGrabberComputeGeometry( SeqGrabber *sgrabPtr, int *width, int *height, int setZoom )
{
    Tcl_Interp 	*interp;
	Rect		srcVideoRect, newVideoRect, origVideoRect;
	Rect		winRect;
	int			divisor = 1;
	short	    newVideoWidth, newVideoHeight;
	double		goldenRatio;
	ComponentResult	err = noErr;

    interp = sgrabPtr->interp;
	*width = 0;
	*height = 0;

	/* 
	 * First, make sure we have got a video source at all. 
	 */

    if (sgrabPtr->sgChanVideo == NULL) {		
		*width = 1;
		*height = 1;
	} else if (sgrabPtr->seqGrab && sgrabPtr->sgChanVideo) { 
		winRect.left = 0;
		winRect.top = 0;
			
		/* 
		 * Get and store the maximal video source size. Here?
		 */
		 
		SGGetSrcVideoBounds( sgrabPtr->sgChanVideo, &srcVideoRect );
		sgrabPtr->srcWidth = srcVideoRect.right - srcVideoRect.left;
		sgrabPtr->srcHeight = srcVideoRect.bottom - srcVideoRect.top;
		
		/*
		 * There are two possibilities here: either we have got one of 
		 * '-width' or '-height' > 0; use these in this case. Or use the
		 * '-size' option (quarter, half, or full).
		 */
		 
		if (sgrabPtr->width > 0 || sgrabPtr->height > 0) {
			goldenRatio = (double) sgrabPtr->srcWidth/(double) sgrabPtr->srcHeight;
			if (sgrabPtr->width > 0 && sgrabPtr->height == 0) {
				*width = sgrabPtr->width;
				*height = (int) ((double) sgrabPtr->width/goldenRatio);
			} else if (sgrabPtr->width == 0 && sgrabPtr->height > 0) {
				*height = sgrabPtr->height;
				*width = (int) (goldenRatio * sgrabPtr->height);
			} else {
			
				/* This code should never be executed; my QuickCam becomes weired! */
				*width = sgrabPtr->width;
				*height = sgrabPtr->height;
			}			
			
			/*  
			 * Check max source size.
			 */

			if ((sgrabPtr->width > sgrabPtr->srcWidth) || 
					(sgrabPtr->height > sgrabPtr->srcHeight)) {
				sgrabPtr->width = sgrabPtr->srcWidth;
				*width =  sgrabPtr->srcWidth;
				sgrabPtr->height = sgrabPtr->srcHeight;
				*height =  sgrabPtr->srcHeight;
			}
		} else {
			switch (sgrabPtr->indSize) {
				case SG_WIDGET_SIZE_QUARTER: 
					divisor = 4;
					break;
				case SG_WIDGET_SIZE_HALF: 
					divisor = 2;
					break;
				case SG_WIDGET_SIZE_FULL: 
					divisor = 1;
					break;
			}
			*width = sgrabPtr->srcWidth/divisor;
			*height = sgrabPtr->srcHeight/divisor;
		}
		
		if (setZoom) {			
						
			/* 
			 * If we have got a zoomed in video rectangle, do that. 
			 * In video source coordinates!
			 */
			 
			newVideoWidth = (short) ((double) sgrabPtr->srcWidth/sgrabPtr->zoom);
			newVideoHeight = (short) ((double) sgrabPtr->srcHeight/sgrabPtr->zoom);
			MacSetRect( &newVideoRect, (short) ((sgrabPtr->srcWidth - newVideoWidth)/2),
					 (short) ((sgrabPtr->srcHeight - newVideoHeight)/2),
					 (short) ((sgrabPtr->srcWidth + newVideoWidth)/2),
					 (short) ((sgrabPtr->srcHeight + newVideoHeight)/2) );
			err = SGGetVideoRect( sgrabPtr->sgChanVideo, &origVideoRect );
			if (!MacEqualRect( &origVideoRect, &newVideoRect )) {
				err = SGSetVideoRect( sgrabPtr->sgChanVideo, &newVideoRect );
			}
			if (err != noErr) {
				CheckAndSetErrorResult( interp, err );
			}
		}
	}
		
	/* Add the padding. Presently unused. */
	*width += 2 * sgrabPtr->padx;
	*height += 2 * sgrabPtr->pady;

	return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SeqGrabberEventProc --
 *
 *		Deal with Sequence Grabber Events; these events are TCL events
 *
 * Results:
 *		None.
 *
 * Side effects:
 *		Depends on event. Schedules a redisplay.
 *
 *----------------------------------------------------------------------
 */

static void
SeqGrabberEventProc( ClientData clientData, XEvent *eventPtr ) 
{
	SeqGrabber 	*sgrabPtr = (SeqGrabber *) clientData;

	/* 
	 * Depending on the event type, do different things. Set the relevant flag
	 * bits for use in 'DisplaySeqGrabber'. When first mapped it should be running.
	 */
	 
	if ((eventPtr->type == Expose) && (eventPtr->xexpose.count == 0)) {
		sgrabPtr->flags |= UPDATEGRABBER;
		goto redraw;
	} else if (eventPtr->type == ConfigureNotify) {
		goto redraw;
	} else if (eventPtr->type == MapNotify) {
		sgrabPtr->flags |= NEWGWORLD;
        sgrabPtr->flags |= ISRUNNING;
		goto redraw;
	} else if (eventPtr->type == UnmapNotify) {
		if (sgrabPtr->seqGrab) {
			
			/* 
			 * Do some cleaning up:
			 * Set flag in global 'gIsRunningFlags' to stop serving the grabber. 
			 */
			 
			SGStop( sgrabPtr->seqGrab );
			/*
			if (sgrabPtr->updatePictHand) {
			    KillPicture( sgrabPtr->updatePictHand );
			}
			*/
            sgrabPtr->flags &= ~ISRUNNING;
			gIsRunningFlags &= ~SEQ_GRABBER_RUNS;
        }
	} else if (eventPtr->type == DestroyNotify) {
	
		/*
		 * We are being destroyed.
		 */
		 
		if (sgrabPtr->tkwin != NULL) {
			sgrabPtr->tkwin = NULL;
			Tcl_DeleteCommandFromToken( sgrabPtr->interp, sgrabPtr->widgetCmd );
		}
		if (sgrabPtr->flags & REDRAW_PENDING) {
			Tcl_CancelIdleCall( DisplaySeqGrabber, (ClientData) sgrabPtr );
		}
		DestroySeqGrabber( sgrabPtr );
	} 
	return;
	
	redraw:
	
	/*
	 * Now we know that the event type was such that the widget needs to be redrawn.
	 * Schedule the redrawing procedure.
	 */
	 
	if ((sgrabPtr->tkwin != NULL) && Tk_IsMapped(sgrabPtr->tkwin) &&
			!(sgrabPtr->flags & REDRAW_PENDING)) {
		Tcl_DoWhenIdle( DisplaySeqGrabber, (ClientData) sgrabPtr );
		sgrabPtr->flags |= REDRAW_PENDING;
	}
}

/*
 *----------------------------------------------------------------------
 *
 * SeqGrabberDeleted Proc --
 *
 *		Deletes a Sequence Grabber
 *
 * Results:
 *		None.
 *
 * Side effects:
 *		Uh, deletes that sequence grabber. Hopefully frees memory.
 *
 *----------------------------------------------------------------------
 */
 
static void
SeqGrabberDeletedProc( ClientData clientData ) 
{

	SeqGrabber *sgrabPtr = (SeqGrabber *) clientData;
	Tk_Window tkwin = sgrabPtr->tkwin;
	
	QTTclDebugPrintf( sgrabPtr->interp, 2, "SeqGrabberDeletedProc" );

	if (tkwin != NULL) {
		sgrabPtr->tkwin = NULL;
		Tk_DestroyWindow( tkwin );
	}
}

/*
 *----------------------------------------------------------------------
 *
 * DestroySeqGrabber --
 *
 *		Deletes a SeqGrabber Widget. Most things cleaned up with Tk_FreeOptions 
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
DestroySeqGrabber( SeqGrabber *sgrabPtr ) 
{

#ifdef _WIN32
	HWND    tempHwnd;
	if (sgrabPtr->tkwin) {
		tempHwnd = TkWinGetHWND( Tk_WindowId(sgrabPtr->tkwin) );
		/* should we use 'GetHWNDPort()' instead ? */
		//DestroyPortAssociation( (CGrafPtr) GetNativeWindowPort(tempHwnd) );
		DestroyPortAssociation( (CGrafPtr) GetHWNDPort(tempHwnd) );
	}
#endif

	QTTclDebugPrintf( sgrabPtr->interp, 2, "DestroySeqGrabber" );

	gIsRunningFlags &= ~SEQ_GRABBER_RUNS;

    FreeVideoChannel( sgrabPtr );
    FreeAudioChannel( sgrabPtr );
	/*
	if (sgrabPtr->videoBottleTempPort) {
		CloseCPort( &sgrabPtr->videoBottleTempPort ); causes crash if no port created yet
	}*/
	if (sgrabPtr->seqGrab != NULL) {
		SGStop( sgrabPtr->seqGrab );
		CloseComponent( sgrabPtr->seqGrab );
		gSGrabPtr = NULL;
		sgrabPtr->seqGrab = NULL;
	}
	if (sgrabPtr->overlayPictHand != NULL) {
		KillPicture( sgrabPtr->overlayPictHand );
	}
	if (sgrabPtr->updatePictHand != NULL) {
	    KillPicture( sgrabPtr->updatePictHand );
	}
	if (sgrabPtr->asyncImageHandlerPtr != NULL) {
        AsyncImageHandlerRecord	*asyncPtr;
        
        asyncPtr = (AsyncImageHandlerRecord *) sgrabPtr->asyncImageHandlerPtr;
        Tcl_DeleteTimerHandler( asyncPtr->timerToken );
        Tcl_DecrRefCount( asyncPtr->commandObjPtr );  
        ckfree( (char *) asyncPtr );
	}
	
#if TARGET_API_MAC_CARBON
	/* 
	 *  On Carbon, be sure to remove the timer.
	 */

    if (gCarbonGrabberTimerRef != NULL) {
        RemoveEventLoopTimer( gCarbonGrabberTimerRef );
        gCarbonGrabberTimerRef = NULL;
    }
#endif
	
	Tk_FreeConfigOptions( (char *) sgrabPtr, sgrabPtr->optionTable, sgrabPtr->tkwin );
	Tcl_EventuallyFree( (ClientData) sgrabPtr, TCL_DYNAMIC );
}

/*
 *----------------------------------------------------------------------
 *
 * SeqGrabberWorldChanged --
 *
 *		Something changed, arrange for the movie to be redisplayed. 
 *      Compute geometry.
 *
 * Results:
 *		None.
 *
 * Side effects:
 *		SeqGrabber Widget displayed: if already on display it is scheduled for 
 *		a renewed display, else, the size is requested by the tk geometry manager,
 *		and displayed upon a MapNotify event.
 *
 *----------------------------------------------------------------------
 */

static void
SeqGrabberWorldChanged( ClientData clientData ) 
{
	SeqGrabber 		*sgrabPtr = (SeqGrabber *) clientData;
	int				width, height;

	/*
	 * If not already scheduled for (re)display, it should be if it's mapped,
	 * else upon a MapNotify event.
	 */
	 
	if (Tk_IsMapped(sgrabPtr->tkwin) && !(sgrabPtr->flags & REDRAW_PENDING)) {
		Tcl_DoWhenIdle( DisplaySeqGrabber, (ClientData) sgrabPtr );
		sgrabPtr->flags |= REDRAW_PENDING;
	}
	
	/*
	 * Get the desired width and height to request. We always finds zoom which is uneconomical.
	 */
	 
	SeqGrabberComputeGeometry( sgrabPtr, &width, &height, 1 );

	if (sgrabPtr->seqGrab && sgrabPtr->sgChanVideo) { 
		
		/*
		 * After getting our geometry above, let tk also know.
		 */
		 
		Tk_GeometryRequest( sgrabPtr->tkwin, width, height );
		Tk_SetInternalBorder( sgrabPtr->tkwin, 0 );
		sgrabPtr->flags |= NEWGWORLD;
	}
	sgrabPtr->frameCount = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * CreateSeqGrabber --
 *
 *		Create a Sequence Grabber Object
 *
 * Results:
 *		Return pointer to sequence grabber object
 *
 * Side effects:
 *		Memory allocated
 *
 *----------------------------------------------------------------------
 */

static SeqGrabber * 
CreateSeqGrabber( Tk_Window tkwin ) 
{
    SeqGrabber *sgPtr = (SeqGrabber *) ckalloc(sizeof(SeqGrabber));
    memset((void *) sgPtr, 0, (sizeof(SeqGrabber)));
	return sgPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * DisplaySeqGrabber --
 *
 *		Display a sequence grabber window.
 *
 * Results:
 *		None.
 *
 * Side effects:
 *		With luck, the grabber is displayed
 *
 *----------------------------------------------------------------------
 */

static void
DisplaySeqGrabber( ClientData clientData ) 
{
	SeqGrabber *sgrabPtr = (SeqGrabber *) clientData;	
	CGrafPtr	 		saveWorld = NULL;
	GDHandle 			saveDevice = NULL;
	GWorldPtr			sgGWorldPtr = NULL;
	ComponentResult		res;
	Rect 				tkRect;
	static RgnHandle	region = NULL;
	static RgnHandle	saveRegion = NULL;
	
	if (sgrabPtr->sgChanVideo == NULL) {
	    return;
	}

	/* The first time we allocate a region to help us with update events. */	
	if (region == NULL) {
		region = NewRgn();
	}
	if (saveRegion == NULL) {
		saveRegion = NewRgn();
	}

	/* Save current graphics world, reset at end */
	GetGWorld( &saveWorld, &saveDevice );
	
	/* Are we ready to display. Clear the redraw pending state. */
	sgrabPtr->flags &= ~REDRAW_PENDING;
	
	/* 
	 * Make sure that the sequence grabber widget still exists and is mapped to the 
	 * display.
	 */
	 
	if ((sgrabPtr->tkwin == NULL) || !Tk_IsMapped(sgrabPtr->tkwin)) {
		return;
	}
	
	if (sgrabPtr->seqGrab) {
	
		/*
		 * Get our graph port, set it, and get the local coordinates available to us.
		 */
		 
		sgGWorldPtr = QTTclMacGetDrawablePort( Tk_WindowId(sgrabPtr->tkwin) );
		MacSetPort( (GrafPtr) sgGWorldPtr );
		GetClip( saveRegion );
		QTTclMacWinBounds( (TkWindow *) sgrabPtr->tkwin, &tkRect );
		
		/*
		 * This gives the total size of the widget including the padding of the widget
		 * itself (which is not used for the moment). Therefore, subtract padding.
		 */
		 
		tkRect.left += sgrabPtr->padx;
		tkRect.right -= sgrabPtr->padx;
		tkRect.top += sgrabPtr->pady;
		tkRect.bottom -= sgrabPtr->pady;
							
		if (sgrabPtr->flags & NEWGWORLD) {
		
			/*
			 * This associates the sequence grabber with the actual graphics world.
			 * It should have been done already, before the video channel creation!
			 */
			 
			SGSetGWorld( sgrabPtr->seqGrab, sgGWorldPtr, NULL );
			
			/*
			 * We start the previewing here only if not started before. 
			 * Set the global flag 'gIsRunningFlags' that is used to process Mac events.
			 */

            if (!(gIsRunningFlags & SEQ_GRABBER_RUNS)) {
    			SGStartPreview( sgrabPtr->seqGrab );
			    //SGPause( sgrabPtr->seqGrab, seqGrabUnpause );
    	        gIsRunningFlags |= SEQ_GRABBER_RUNS;
	        }
			sgrabPtr->flags &= ~NEWGWORLD;
		}

		/*
		 * Treat the actual update event.
		 */
		
		if (sgrabPtr->flags & UPDATEGRABBER) {
			SGUpdate( sgrabPtr->seqGrab, NULL );
			if (!(sgrabPtr->flags & ISRUNNING)) {
			
				/* 
				 * Here we copy the Pict that should have been taken when stopped or paused.
				 * Should fix old clip region!!!
				 */
				 
                if (sgrabPtr->updatePictHand) {
#if TARGET_OS_MAC
                    region = QTTclMacVisableClipRgn( ((TkWindow *) (sgrabPtr->tkwin)) );
			        SetClip( region );
#endif		        
                    DrawPicture( sgrabPtr->updatePictHand, &tkRect );
                }
 			}
			sgrabPtr->flags &= ~UPDATEGRABBER;
		}
		
		/*
		 * Here we should set the clipping region equal to the visible region of the
		 * tk widget. My Grayscale QuickCam seems not to support clipping :-(
		 */

#if TARGET_OS_MAC
        // A lot of guessing here...
        region = QTTclMacVisableClipRgn( ((TkWindow *) (sgrabPtr->tkwin)) );
		SGSetChannelClip( sgrabPtr->seqGrab, region );
#endif		        
				
		/*
		 * If we decreased our size, it is possible that other parts need to be given
		 * an update event. Seems to work! Crash reason sometimes?
		 */
		 /*
			QTTclMacWinBounds( (TkWindow *) sgrabPtr->tkwin, &dirtyRect );
			InvalRect( &dirtyRect );
			*/
		/* 
		 * Set the the video channel bounds. This is the actual size on screen without padding. 
		 * Local coordinates with respect to the complete window. 
		 */
		 
		res = SGSetChannelBounds( sgrabPtr->sgChanVideo, &tkRect );
		sgrabPtr->sgWidth = tkRect.right - tkRect.left;
		sgrabPtr->sgHeight = tkRect.bottom - tkRect.top;
		
		SetClip( saveRegion );
	}
	
	/* Reset the graphics world to the previous one. */
	SetGWorld( saveWorld, saveDevice );
}

/*
 *----------------------------------------------------------------------
 *
 * SeqGrabberMacEvent --
 *
 *		Processes events
 *
 * Results:
 *		Always 0 since we only respond to null events.
 *
 * Side effects:
 *		Gives time to the sequence grabber through a 'SGIdle' call.
 *
 *----------------------------------------------------------------------
 */

int 
SeqGrabberMacEvent( EventRecord *eventPtr )
{
	GWorldPtr           saveWorld = NULL;
	GWorldPtr           sgGWorldPtr = NULL;
	GDHandle            saveDevice = NULL;
	ComponentResult		err;
	static RgnHandle	region = NULL;
	static RgnHandle	saveRegion = NULL;
	
	/* The first time we allocate a region to help us with update events. */	
	if (region == NULL) {
		region = NewRgn();
	}
	if (saveRegion == NULL) {
		saveRegion = NewRgn();
	}
	if (gSGrabPtr == NULL) {
		return 0;
	}
	
	/*
	 * We catch only 'nullEvent'«s here. 
	 */

	if (eventPtr->what == nullEvent) {		
		if (Tk_IsMapped(gSGrabPtr->tkwin) && gSGrabPtr->seqGrab) {

    		sgGWorldPtr = QTTclMacGetDrawablePort( Tk_WindowId(gSGrabPtr->tkwin) );
			GetGWorld( &saveWorld, &saveDevice );
			SetGWorld( sgGWorldPtr, NULL );
			GetClip( saveRegion );
		    //MacSetPort( (GrafPort *) sgGWorldPtr );	//	shoudn't need this
			
			/* We must find out if the present clip region for the movie is invalid. */
			
#if TARGET_OS_MAC
			if (MyIsClipRegionInvalid( gSGrabPtr->tkwin )) {
    			region = QTTclMacVisableClipRgn( (TkWindow *) gSGrabPtr->tkwin );
        		SGSetChannelClip( gSGrabPtr->seqGrab, region );
				SetClip( region );
			}
#endif
			/*
			 * Here the sequence grabber is served.
			 */
			 
			err = SGIdle( gSGrabPtr->seqGrab );
			if (err != noErr) {
				SGStop( gSGrabPtr->seqGrab );
			}	
			SetClip( saveRegion );
			SetGWorld( saveWorld, saveDevice );
		} else if (gSGrabPtr->audioOnly && gSGrabPtr->sgChanAudio) {
		
			err = SGIdle( gSGrabPtr->seqGrab );
			if (err != noErr) {
				SGStop( gSGrabPtr->seqGrab );
			}	
		}
	}
	return 0;
}


#if TARGET_API_MAC_CARBON
pascal void
SeqGrabberCarbonTimer( EventLoopTimerRef theTimer, void *userData )
{
    EventRecord     eventRec;
    
    /* The grabber doesn't care what events it gets. */
    eventRec.what = nullEvent;
    SeqGrabberMacEvent( &eventRec );
}
#endif


/*
 *----------------------------------------------------------------------
 *
 * QTGrabberWinProc --
 *
 *		This is the window callback procedure for Windows only.
 *
 * Results:
 *		Same as the original WinProc.
 *
 * Side effects:
 *		First the Mac event procedure called, then the original WinProc.
 *
 *----------------------------------------------------------------------
 */

#ifdef _WIN32
LRESULT CALLBACK 
QTGrabberWinProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	
	if(GetNativeWindowPort(hWnd)) {
		MSG	        msg;
		EventRecord	macEvent;
		LONG        thePoint = GetMessagePos();
		
		msg.hwnd = hWnd;
		msg.message = message;
		msg.wParam = wParam;
		msg.lParam = lParam;
		msg.time = GetMessageTime();
		msg.pt.x = LOWORD(thePoint);
		msg.pt.y = HIWORD(thePoint);
		
		/* Convert the message to a QTML event   Keep it for the moment. */
		WinEventToMacEvent( &msg, &macEvent );  
		if (gSGrabPtr && gSGrabPtr->seqGrab) {
			if (noErr != SGIdle( gSGrabPtr->seqGrab )) {
				SGStop( gSGrabPtr->seqGrab );
			}	
		}
	}	
	
	/*
	 * Call the original winproc given by tk.
	 */
	 
	if (gSGrabPtr && gSGrabPtr->winEventProc) {
		return CallWindowProc( (WNDPROC) (gSGrabPtr->winEventProc), hWnd, message, 
				wParam, lParam );
	} else {
		return 0;
	}
}
#endif      // _WIN32

/*
 *----------------------------------------------------------------------
 *
 * SeqGrabberExitProc --
 *
 *		Last bit of Mac cleanup
 *
 * Results:
 *		None.
 *
 * Side effects:
 *		
 *
 *----------------------------------------------------------------------
 */
 
static void
SeqGrabberExitProc( ClientData clientData )
{
	SeqGrabber *sgrabPtr = (SeqGrabber *) clientData;
	
	/* Only if not already destroyed the sequence grabber. */
	if (sgrabPtr != NULL) {
		DestroySeqGrabber( sgrabPtr );
	}
}

/*
 *----------------------------------------------------------------------
 *
 * TakePicture --
 *
 *		Takes a Pict from the video.
 *
 * Results:
 *
 * Side effects:
 *      Returns the Pict. A Picture record allocated.
 *      Free it with KillPicture when finished.
 *
 *----------------------------------------------------------------------
 */

static pascal ComponentResult 
TakePicture( SeqGrabber *sgrabPtr, PicHandle *thePic )
{
    ComponentResult res;
	Byte			isPaused;
	Rect            aRect;
    
	/* 
	 * Critical to choose the flag 'grabPictOffScreen' to make it work.
	 * If we failed, try taking the picture without 'grabPictCurrentImage'.
	 */

	res = SGGetChannelBounds( sgrabPtr->sgChanVideo, &aRect );
	res = SGGetPause( sgrabPtr->seqGrab, &isPaused );

	res = SGGrabPict( sgrabPtr->seqGrab, thePic, &aRect, 0, 
			grabPictIgnoreClip | grabPictCurrentImage | grabPictOffScreen );
	if (!thePic || (res != noErr)) {
		res = SGGrabPict( sgrabPtr->seqGrab, thePic, &aRect, 0, 
				grabPictIgnoreClip | grabPictOffScreen );
	}

    return res;
}


/*--- Non Tcl specific code (one exception) ----------------------------------*/

/*
 *----------------------------------------------------------------------
 *
 * MakeMySequenceGrabber --
 *
 *		Creates the actual sequence grabber component.
 *
 * Results:
 *		The actual sequence grabber component.
 *
 * Side effects:
 *		None.
 *
 *----------------------------------------------------------------------
 */

static SeqGrabComponent 
MakeMySequenceGrabber( void )
{
	SeqGrabComponent		seqGrab = NULL;
	ComponentResult		    err = noErr;

	/* open up the default sequence grabber */
	seqGrab = OpenDefaultComponent( SeqGrabComponentType, 0 );
	if (seqGrab != NULL) { 
	
		/* initialize the default sequence grabber component */
		err = SGInitialize(seqGrab);
	}

	/* clean up on failure */
	if (err && (seqGrab != NULL)) {								
		CloseComponent(seqGrab);
		seqGrab = NULL;
	}
	return seqGrab;
}

/*
 *----------------------------------------------------------------------
 *
 * MakeVideoChannel, MakeAudioChannel --
 *
 *		Creates the actual sequence grabber *channel* components; Audio
 *		and video as respectively.
 *
 * Results:
 *		None.
 *
 * Side effects:
 *		None.
 *
 *----------------------------------------------------------------------
 */

static pascal ComponentResult 
MakeVideoChannel( 
        SeqGrabComponent seqGrab, 
		SGChannel *sgchanVideo, 
		const Rect *rect, 
		int playDuringRecord,
		Boolean willRecord )
{
	long				lUsage = 0;
	ComponentResult		err = noErr;

	/* Figure out the usage. Always previewing. */
	lUsage = seqGrabPreview;
	
	/* Sometimes recording. */
	if (willRecord) {
		lUsage |= seqGrabRecord;									
    }
    if (playDuringRecord) {
		lUsage |= seqGrabPlayDuringRecord;									
    }
    err = SGNewChannel( seqGrab, VideoMediaType, sgchanVideo );
    if (err == noErr) {
    
        /* Set boundaries for new video channel. We don't know at this stage! */
        err = SGSetChannelBounds( *sgchanVideo, rect );
        
        /* Set usage for new video channel. */
        if (err == noErr) {
            err = SGSetChannelUsage( *sgchanVideo, lUsage );
                    
            /* 
             * We want an offscreen buffer. This eliminates cursor flicker, gives better
             * and flicker free display, works better together with tk.
             */
                
            SGSetUseScreenBuffer( *sgchanVideo, false );
            
            /*
             * Set compession parameters if prepared to record. There are more alternatives.
             * Could also use 'SGSetVideoCompressor(...)'. Use panels instead?
             */
                
            if (0 && willRecord) {
                SGSetVideoCompressorType( *sgchanVideo, 
                        FOUR_CHAR_CODE('cvid') );
            }
        } else {
        
            /* Clean up on failure. */
            SGDisposeChannel( seqGrab, *sgchanVideo );
            *sgchanVideo = NULL;
        }
    }
    return err;
}

static pascal ComponentResult 
MakeAudioChannel( 
        SeqGrabComponent seqGrab, 
		SGChannel *sgchanSound,
		int playDuringRecord,
		Boolean willRecord )
{
	ComponentResult		err = noErr;
	long				lUsage = 0;

	/* Figure out the usage. Always previewing. */
	lUsage = seqGrabPreview;
	
	/* Sometimes recording. */
	if (willRecord) {
		lUsage |= seqGrabRecord;									
    }
    if (playDuringRecord) {
		lUsage |= seqGrabPlayDuringRecord;									
    }
    err = SGNewChannel( seqGrab, SoundMediaType, sgchanSound );
    if (err == noErr) {
    
        /* Set usage of new sound channel. */
        err = SGSetChannelUsage( *sgchanSound, lUsage );
        if (err != noErr) {
        
            /* Clean up on failure. */
            SGDisposeChannel( seqGrab, *sgchanSound );
            *sgchanSound = NULL;
        }
    }
    return err;
}

static void
FreeVideoChannel(SeqGrabber *sgrabPtr)
{
	if (sgrabPtr->sgChanVideo != NULL) {
		SGDisposeChannel( sgrabPtr->seqGrab, sgrabPtr->sgChanVideo );
		sgrabPtr->sgChanVideo = NULL;
	}
}

static void
FreeAudioChannel(SeqGrabber *sgrabPtr)
{
	if (sgrabPtr->sgChanAudio != NULL) {
		SGDisposeChannel( sgrabPtr->seqGrab, sgrabPtr->sgChanAudio );
		sgrabPtr->sgChanAudio = NULL;
	}
}

/*
 *----------------------------------------------------------------------
 *
 * MyGrabFrameComplete --
 *
 *      This routine is the frame completion routine for video in the sequence
 *      grabber. It gets called just before the frame is displayed, and may
 *      be used to draw things in it, like a pict (via overlayPictHand).
 *
 * Results:
 *		ComponentResult.
 *
 * Side effects:
 *		Draws in graphics port.
 *
 *----------------------------------------------------------------------
 */

static pascal ComponentResult  
MyGrabFrameComplete( SGChannel sgChan, short nBufferNum, Boolean *pbDone, long refConst )
{
	ComponentResult		err;
	
	/* First, call the default grab-complete function. */
	
 	err = SGGrabFrameComplete( sgChan, nBufferNum, pbDone );

	if (*pbDone) {								
	
		/* Frame is done. */
		
		SeqGrabber 			*sgrabPtr = (SeqGrabber *) refConst;		
		CGrafPtr			tmpPortPtr = sgrabPtr->videoBottlePortPtr;
		CGrafPtr			oldPortPtr;
		GDHandle			hghOldDevice;
		PixMapHandle		hpmBuffer, hpmOld;
		Rect				rectBuffer, destRect;
		char				str[16];
		static unsigned short	counter = 0;
		double              fps;
		double              averageFps;

		/* Set to our temporary port. */
		GetGWorld( &oldPortPtr, &hghOldDevice );
		SetGWorld( tmpPortPtr, NULL );

		/* Find out about this buffer. */
		err = SGGetBufferInfo( sgChan, nBufferNum, &hpmBuffer,
                &rectBuffer, NULL, NULL );

		if (err == noErr) {
		
			/*
			 * The temporary port passed in in 'refConst' via sgrabPtr is used to draw in with
			 * its pixmap replaced by the buffers pixmap. Must be sure that the temporary
			 * port is still valid.
			 */
			 
#if TARGET_API_MAC_CARBON
			hpmOld = GetPortPixMap( tmpPortPtr );
#else
			hpmOld = tmpPortPtr->portPixMap;
#endif			
			SetPortPix( hpmBuffer );
#if TARGET_API_MAC_CARBON
            //LockPortBits( hpmBuffer );
#endif
			if (sgrabPtr->frameCount == 0) {
			    sgrabPtr->startTick = TickCount();
			}
		    sgrabPtr->frameCount++;
            
            if ((sgrabPtr->imageAsyncProcObj != NULL) &&
                    (sgrabPtr->asyncImageHandlerPtr == NULL)) {
                Tcl_Obj		*listObjPtr = NULL;

                if (MakeTkPhotoFromPixMap( sgrabPtr->interp, hpmBuffer, 
                        Tcl_GetString(sgrabPtr->imageNameObj) ) == TCL_OK) {
                    AsyncImageHandlerRecord	*asyncImageHandlerPtr;
                    
                    listObjPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );
                    Tcl_ListObjAppendElement( sgrabPtr->interp, listObjPtr, sgrabPtr->imageAsyncProcObj );
                    Tcl_ListObjAppendElement( sgrabPtr->interp, listObjPtr, 
                           Tcl_NewStringObj( Tk_PathName(sgrabPtr->tkwin), -1) );
                    Tcl_ListObjAppendElement( sgrabPtr->interp, listObjPtr, sgrabPtr->imageNameObj );
                    
                    /*
                     * Do the callback as soon as possible but from a safe place,
                     * like the Tcl event loop instead of here.
                     */
                    asyncImageHandlerPtr = (AsyncImageHandlerRecord *) 
                            ckalloc( sizeof(AsyncImageHandlerRecord) );
                    asyncImageHandlerPtr->sgrabPtr = sgrabPtr;
                    asyncImageHandlerPtr->commandObjPtr = listObjPtr;
                    asyncImageHandlerPtr->timerToken = Tcl_CreateTimerHandler( 0, 
                            AsyncImageHandlerProc, (ClientData) asyncImageHandlerPtr );
                    Tcl_IncrRefCount( asyncImageHandlerPtr->commandObjPtr );  
                    sgrabPtr->asyncImageHandlerPtr = (void *) asyncImageHandlerPtr;
                }
                sgrabPtr->imageAsyncProcObj = NULL;
            }

			/* 
			 * Draw some text into the buffer.
			 */

            if (sgrabPtr->showFPS) {
    			TextMode( srcXor );
    			MoveTo( (short) (rectBuffer.right - 50), (short) (rectBuffer.bottom - 22) );

#if TARGET_API_MAC_CARBON
    			sprintf( StrBody(str), "%d", counter++ );			
                str[0] = strlen(str) + 1;
    			DrawString( (ConstStr255Param) str );
#else
    			sprintf( str, "%d", counter++ );			
    			DrawString( c2pstr( str ) );		
#endif			
    			MoveTo( (short) (rectBuffer.right - 120), (short) (rectBuffer.bottom - 8) );
    	       	fps = 60.0/(TickCount() - sgrabPtr->latestTick);
    	       	averageFps = 60.0 * sgrabPtr->frameCount/(TickCount() - sgrabPtr->startTick);

#if TARGET_API_MAC_CARBON
    	       	sprintf( StrBody(str), "<fps>:%4.1f fps:%4.1f", averageFps, fps );
        		str[0] = strlen(str) + 1;
        		DrawString( (ConstStr255Param) str );
#else
    	       	sprintf( str, "<fps>:%4.1f fps:%4.1f", averageFps, fps );
        		DrawString( c2pstr( str ) );		
#endif			
    	       	sgrabPtr->latestTick = TickCount();
    			TextMode( srcOr );
			}
			
			/* Have we got a pict to overlay? Set the 'destRect' to the NE corner. */
			if (sgrabPtr->overlayPictHand) {
				destRect = (**(sgrabPtr->overlayPictHand)).picFrame;
				if (!EmptyRect( &destRect )) {
					MacOffsetRect( &destRect, (short) (rectBuffer.right - destRect.right), 
							(short) (rectBuffer.top - destRect.top) );
					DrawPicture( sgrabPtr->overlayPictHand, &destRect );
				}
			}

#if TARGET_API_MAC_CARBON
            //UnlockPortBits( hpmBuffer );
#endif
			/* Restore temporary port's pixmap. */
			SetPortPix( hpmOld );
		}
		SetGWorld( oldPortPtr, hghOldDevice );
	}
	return err;
}

static void
AsyncImageHandlerProc(
    ClientData clientData )
{
    AsyncImageHandlerRecord *asyncImageHandlerPtr = (AsyncImageHandlerRecord *) clientData;
    int				code;
    SeqGrabber 		*sgrabPtr = asyncImageHandlerPtr->sgrabPtr;		

    /* Since we may be destroyed in the callback keep storage alive. */
    Tcl_Preserve( (ClientData) sgrabPtr );
    
    Tcl_IncrRefCount( asyncImageHandlerPtr->commandObjPtr );  
    code = Tcl_EvalObjEx( sgrabPtr->interp, asyncImageHandlerPtr->commandObjPtr, 
            TCL_EVAL_GLOBAL );
    Tcl_DecrRefCount( asyncImageHandlerPtr->commandObjPtr );  
    
    sgrabPtr->asyncImageHandlerPtr = NULL;
    Tcl_Release( (ClientData) sgrabPtr );
    ckfree( (char *) asyncImageHandlerPtr );
}

/*
 *----------------------------------------------------------------------
 *
 * SetupMyVideoBottlenecks --
 *
 *      This is used to register the MyGrabFrameComplete procedure before
 *      starting playback.
 *
 * Results:
 *		OSErr.
 *
 * Side effects:
 *		Registers the MyGrabFrameComplete procedure.
 *
 *----------------------------------------------------------------------
 */

static pascal ComponentResult 	 
SetupMyVideoBottlenecks( SGChannel sgchanVideo, 
		WindowPtr macWndPtr, 
		SeqGrabber *sgrabPtr )
{
	ComponentResult		err = noErr;
	CGrafPtr            tempPortPtr;
	Rect                bounds = {0, 0, 10, 10};

	/*
	 * We set the reference constant to our sequence grabber struct (SeqGrabber) to make things
	 * available to our bottleneck callback procedure.
	 */
	 
	err = SGSetChannelRefCon( sgchanVideo, (long) sgrabPtr );
	if (err == noErr) {
		VideoBottles	vb;

		/* get the current bottlenecks */
		vb.procCount = 9;
		err = SGGetVideoBottlenecks(sgchanVideo, &vb);
		if (err == noErr) {
		
			/* 
			 * Add our GrabFrameComplete function. 
			 * Searched in <QuickTimeComponents.h> for UPP stuff.
			 */
#if TARGET_API_MAC_CARBON
			vb.grabCompleteProc = NewSGGrabCompleteBottleUPP( MyGrabFrameComplete );
#else
			vb.grabCompleteProc = NewSGGrabCompleteBottleProc( MyGrabFrameComplete );
#endif
			err = SGSetVideoBottlenecks( sgchanVideo, &vb );

			/*
			 * Create a temporary port for drawing...
			 * ...with a wide open visible and clip region.
			 * We just borrow the graphics port from the GWorld.
			 */
			 
            err = MySafeNewGWorld( &tempPortPtr, 32, &bounds, NULL, NULL, 0 );
            if (err != noErr) {
                panic( "Out of memory: NewGWorld failed in SetupMyVideoBottlenecks" );
            }
            sgrabPtr->videoBottlePortPtr = tempPortPtr;

#if TARGET_API_MAC_CARBON
            {
                RgnHandle   rgn;
                
                rgn = NewRgn();
    			tempPortPtr = CreateNewPort();
                MacSetRectRgn( rgn, -32000, -32000, 32000, 32000 );
                SetPortVisibleRegion( tempPortPtr, rgn );
                SetPortClipRegion( tempPortPtr, rgn );
    			PortChanged( (GrafPtr) tempPortPtr );
                DisposeRgn( rgn );
            }
#else
			//OpenCPort( tempPortPtr );
			MacSetRectRgn( tempPortPtr->visRgn, -32000, -32000, 32000, 32000 );
			/* so that you can use it in any video buffer */
			MacCopyRgn( tempPortPtr->visRgn, tempPortPtr->clipRgn );
			/* tell QuickDraw about the changes */
			PortChanged( (GrafPtr) tempPortPtr );
#endif
		}
	}

	return err;
}

/*
 *----------------------------------------------------------------------
 *
 * MovableDialogModalFilter --
 *
 *      Used to handle update events when the dialog is moved.
 *
 * Results:
 *		Boolean describing if the event was handled or not.
 *
 * Side effects:
 *		Handles update events.
 *
 *----------------------------------------------------------------------
 */

pascal Boolean
MovableDialogModalFilter( DialogPtr dialogPtr, 
		const EventRecord *eventPtr, 
		short *itemHit, 
		long refCon )
{

#if TARGET_OS_MAC	// Windows works different
    SeqGrabber 	*sgrabPtr = (SeqGrabber *) refCon;
    Boolean     eventDone = false;
    GrafPtr		savePort;
        
#if TARGET_API_MAC_CARBON
    if ((eventPtr->what == updateEvt) && 
    		((WindowPtr) eventPtr->message != GetDialogWindow( dialogPtr ))) {
#else
    if ((eventPtr->what == updateEvt) && 
    		((WindowPtr) eventPtr->message != dialogPtr)) {
        Rect		saveBounds;
        
    	SGGetChannelBounds( sgrabPtr->sgChanVideo, &saveBounds );

		/* 
		 * Handle update events to background windows here. 
		 * First, translate mac event to a number of tcl events.
		 * If any tcl events generated, execute them until empty, and don't wait.
		 */

		if (TkMacConvertEvent( (EventRecord *) eventPtr )) {
			while ( Tcl_DoOneEvent( TCL_IDLE_EVENTS | TCL_DONT_WAIT | TCL_WINDOW_EVENTS ) )
				/* empty */
				;
		}
		SGSetChannelBounds( sgrabPtr->sgChanVideo, &saveBounds );
#endif

    } else {
    	GetPort( &savePort );
#if TARGET_API_MAC_CARBON
    	MacSetPort( GetDialogPort( dialogPtr ) );
#else
    	MacSetPort( dialogPtr );
#endif    	
    	eventDone = StdFilterProc( dialogPtr, (EventRecord *) eventPtr, itemHit );
    	
    	MacSetPort( savePort );
    }
    return eventDone;
#endif	// TARGET_OS_MAC

#ifdef _WIN32
    Boolean     eventDone = false;
    SeqGrabber 	*sgrabPtr = (SeqGrabber *) refCon;
    GrafPtr		savePort;
        
    if ((eventPtr->what == updateEvt) && 
    		((WindowPtr) eventPtr->message != dialogPtr)) {

    // this is Windows equivalent of TkMacConvertEvent above
    //GenerateXEvent(hwnd, message, wParam, lParam);
    
    } else {
    	GetPort( &savePort );
    	MacSetPort( dialogPtr );
    	
    	eventDone = StdFilterProc( dialogPtr, (EventRecord *) eventPtr, itemHit );
    	
    	MacSetPort( savePort );
    }
    
    return eventDone;
#endif  // WIN32
}

/*------------------------------------------------------------------------------------*/