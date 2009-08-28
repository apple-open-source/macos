/*
 * MoviePlayer.c --
 *
 *	QuickTime Movie widget for Tcl/Tk, part of QuickTimeTcl.
 *
 * Copyright (c) 1998       Bruce O'Neel
 * Copyright (c) 2000-2008  Mats Bengtsson
 *
 * $Id: MoviePlayer.c,v 1.31 2008/04/29 13:11:31 matben Exp $
 */

#include "MoviePlayer.h"

/*
 * Used for controller timer callback. Presently unused.
 */

typedef struct ControllerTimerRecord {
    MoviePlayer     *movPtr;
    char            *tclCmd;
} ControllerTimerRecord;

TrackScrap   		gTrackScrap;

/*
 * Record to handle callbacks to scripy level when async movie loading (-url).
 */

typedef struct AsyncLoadHandlerEntry {
    MoviePlayer				*movPtr;
    Tcl_TimerToken			timerToken;     /* Tcl's token for the timer handler. */ 
    Tcl_Obj 				*commandObjPtr;	/* Command to invoke when the
     * handler is invoked. */
    Tcl_HashEntry			*hashPtr;		/* Self referencing; useful when freeing */
} AsyncLoadHandlerEntry;

static GWorldPtr    gOffscreenGWorldPtr = NULL;


/*
 * We need a global doubly linked list for our movies since we cant get them 
 * to the Mac native event handler in any other way.
 */

static MoviePlayerList  *gMoviePlayerListPtr = NULL;

/*
 * Need to keep a ref count on how many movies actually opened.
 * Different from 'MoviePlayerList' since movie "eventually" disposed.
 */

static int gMovieRefCount = 0;

/*
 * Time interval for Carbon event timer (in secs). Apple recommendation.
 */

#if TARGET_API_MAC_CARBON

EventLoopTimerRef   gCarbonMovieTimerRef = NULL;
const EventTime 	kCarbonMovieTimerSecs = kEventDurationSecond / 10;
long				kMinimumIdleDurationInMillis = 20;

/*
 * Hash table to keep track of Carbon window event handlers.
 * It maps a WindowRef (WindowPtr) to its corresponding event handler.
 */    

static Tcl_HashTable 	*gCarbonWindowHandlerHashPtr = NULL;

typedef struct CarbonWindowHandlerEntry {
    EventHandlerRef		handlerRef;
    long				refCount;		/* Number of movies in this window. */
} CarbonWindowHandlerEntry;    

#endif

/*
 * We need a global variable to set if either or both of MoviePlayer or 
 * SeqGrabber is running. This is used by our common Mac event procedure to 
 * figure out if we should call MoviePlayerMacEvent and/or SeqGrabberMacEvent.
 * It is declared external in SeqGrabber.c.
 */

long    gIsRunningFlags = 0L;

/*
 * Keeps track of freeing things in correct order when closing down.
 */
static enum QTTclFreeState gExitState = 0;

static Boolean gHasQTVRManager = false;

/*
 * For dispatching movie commands. Order!!!
 */

static char *allMovieCmds[] = {
    "add", 
    "callback", 
    "cget", 
    "clear", 
    "compress", 
    "configure", 
    "controllerinfo",
    "copy", 
    "cut", 
    "effect", 
    "export", 
    "fieldofview", 
    "flatten", 
    "getpreferredrate", 
    "getrate", 
    "getselection", 
    "gettime", 
    "haschanged", 
    "hotspot", 
    "isdone",
    "ismovie", 
    "ispanoramic", 
    "isscrapmovie", 
    "isvisual", 
    "new", 
    "nextinterestingtime", 
    "pan", 
    "panoinfo", 
    "paste", 
    "picture",
    "play",
    "playhints",
    "rate", 
    "save", 
    "saveas", 
    "select", 
    "setpreferredrate", 
    "setrate", 
    "settime",
    "size", 
    "step", 
    "stop", 
    "tilt", 
    "time", 
    "timecode",
    "tracks", 
    "undo", 
    "userdata",
    (char *) NULL
};

enum {
    kMovieCmdAdd                = 0L, 
    kMovieCmdCallBack, 
    kMovieCmdCget, 
    kMovieCmdClear, 
    kMovieCmdCompress, 
    kMovieCmdConfigure,
    kMovieCmdControllerinfo,
    kMovieCmdCopy,
    kMovieCmdCut,
    kMovieCmdEffect,
    kMovieCmdExport,
    kMovieCmdFieldofview,
    kMovieCmdFlatten,
    kMovieCmdGetpreferredrate,
    kMovieCmdGetrate,
    kMovieCmdGetselection,
    kMovieCmdGettime,
    kMovieCmdHasChanged,
    kMovieCmdHotspot,
    kMovieCmdIsdone,
    kMovieCmdIsmovie,
    kMovieCmdIspanoramic,
    kMovieCmdIsscrapmovie,
    kMovieCmdIsvisual,
    kMovieCmdNew,
    kMovieCmdNextInterestingTime,
    kMovieCmdPan,
    kMovieCmdPanoInfo,
    kMovieCmdPaste,
    kMovieCmdPicture,
    kMovieCmdPlay,
    kMovieCmdPlayHints,
    kMovieCmdRate,
    kMovieCmdSave,
    kMovieCmdSaveas,
    kMovieCmdSelect,
    kMovieCmdSetpreferredrate,
    kMovieCmdSetrate,
    kMovieCmdSettime,
    kMovieCmdSize,
    kMovieCmdStep,
    kMovieCmdStop,
    kMovieCmdTilt,
    kMovieCmdTime,
    kMovieCmdTimeCode,
    kMovieCmdTracks,
    kMovieCmdUndo,
    kMovieCmdUserData,
    kMovieCmdUndocumeted1,
    kMovieCmdUndocumeted2
};

static char *allMovieFlattenOpts[] = {
    "-dialog", "-dontinterleave", "-forceresourcebeforedata",
    (char *) NULL
};

enum {
    kMovieFlattenOptDialog           		= 0L,
    kMovieFlattenOptDontInterleave,
    kMovieFlattenOptForceResourceBeforeData
};

static char *allMoviePlayHintsOpts[] = {
    "-scrubmode",
    "-usesoundinterp",
    "-allowinterlace",
    "-allowblacklining",
    "-dontpurge",
    "-inactive",
    "-highquality",
    (char *) NULL
};

enum {
    kMoviePlayHintsOptScrubMode				= 0L,
    kMoviePlayHintsOptUseSoundInterp,
    kMoviePlayHintsOptAllowInterlace,
    kMoviePlayHintsOptAllowBlacklining,
    kMoviePlayHintsOptDontPurge,
    kMoviePlayHintsOptInactive,
    kMoviePlayHintsOptHighQuality
};

static Tk_ClassProcs MoviePlayerProcs = {
    sizeof(Tk_ClassProcs),
    MoviePlayerWorldChanged,	/* geometryProc */
    NULL,						/* createProc */
    NULL						/* modalproc */
};

/* 
 * Information used for parsing configuration options.
 * Mask bits for options changed.
 */

enum {
    MOV_CONF_NEWGWORLD              	= (1L << 0),
    MOV_CONF_FILE                     	= (1L << 1),
    MOV_CONF_URL                     	= (1L << 2),
    MOV_CONF_MCEDIT                 	= (1L << 3),
    MOV_CONF_LOOPSTATE               	= (1L << 4),
    MOV_CONF_PALINDROME_LOOPSTATE      	= (1L << 5),
    MOV_CONF_VOLUME                 	= (1L << 6),
    MOV_CONF_PREFERRED_RATE           	= (1L << 7),
    MOV_CONF_QTVR_QUALITY_STATIC        = (1L << 8),
    MOV_CONF_QTVR_QUALITY_MOTION        = (1L << 9),
    MOV_CONF_LOAD_INTO_RAM        		= (1L << 10),
    MOV_CONF_PROGRESS_PROC        		= (1L << 11),
    MOV_CONF_CUSTOM_BUTTON        		= (1L << 12),
    MOV_CONF_RESIZEABLE	        		= (1L << 13),
    MOV_CONF_HIGHLIGHTS	        		= (1L << 14)
};

/*
 * The following table defines the legal values for the -qtvrqualitystatic option.
 */

static CONST char *kQTVRQualityArr[] = { 
    "min", "low", "normal", "high", "max", (char *) NULL 
};
#define	MOV_QTVR_QUALITY_MIN			0
#define	MOV_QTVR_QUALITY_LOW			1
#define	MOV_QTVR_QUALITY_NORMAL			2
#define	MOV_QTVR_QUALITY_HIGH			3
#define	MOV_QTVR_QUALITY_MAX			4

#define BLACK		"Black"
#define WHITE		"White"

#if TARGET_API_MAC_CARBON
#define NORMAL_BG	"systemWindowBody"
#endif
#ifdef _WIN32
#define NORMAL_BG	"SystemButtonFace"
#endif


static Tk_OptionSpec MoviePlayerConfigSpecs[] = {
    {TK_OPTION_BOOLEAN, "-controller", "controller", "Controller",
	"1", -1, Tk_Offset(MoviePlayer, wantController), 0, 
    (ClientData) NULL, 0},
    {TK_OPTION_BOOLEAN, "-custombutton", "customButton", "CustomButton",
	"0", -1, Tk_Offset(MoviePlayer, custombutton), 0, 
    (ClientData) NULL, MOV_CONF_CUSTOM_BUTTON},
    {TK_OPTION_STRING, "-file", "file", "File",
	NULL, Tk_Offset(MoviePlayer, fileNamePtr), -1, TK_OPTION_NULL_OK,
    (ClientData) NULL, MOV_CONF_FILE},
    {TK_OPTION_COLOR, "-highlightbackground", "highlightBackground", "HighlightBackground", 
    NORMAL_BG, -1, Tk_Offset(MoviePlayer, highlightBgColorPtr), 0, 0, MOV_CONF_HIGHLIGHTS},
    {TK_OPTION_COLOR, "-highlightcolor", "highlightColor", "HighlightColor",
    BLACK, -1, Tk_Offset(MoviePlayer, highlightColorPtr), 0, 0, MOV_CONF_HIGHLIGHTS},
    {TK_OPTION_PIXELS, "-highlightthickness", "highlightThickness", "HighlightThickness", 
    "0", -1, Tk_Offset(MoviePlayer, highlightWidth), 0, 0, MOV_CONF_HIGHLIGHTS},
    {TK_OPTION_PIXELS, "-height", "height", "Height",
	"0", -1, Tk_Offset(MoviePlayer, height), 0,
    (ClientData) NULL, MOV_CONF_NEWGWORLD},
    {TK_OPTION_STRING, "-loadcommand", "loadCommand", "LoadCommand",
	NULL, -1, Tk_Offset(MoviePlayer, loadCommand), TK_OPTION_NULL_OK,
    (ClientData) NULL, 0},
    {TK_OPTION_BOOLEAN, "-loadintoram", "loadIntoRam", "LoadIntoRam",
	"0", -1, Tk_Offset(MoviePlayer, loadIntoRam), 0,
    (ClientData) NULL, MOV_CONF_LOAD_INTO_RAM},
    {TK_OPTION_BOOLEAN, "-loopstate", "loopstate", "Loopstate",
	"0", -1, Tk_Offset(MoviePlayer, loopstate), 0,
    (ClientData) NULL, MOV_CONF_LOOPSTATE},
    {TK_OPTION_STRING, "-mccommand", "mcCommand", "MCCommand",
	NULL, -1, Tk_Offset(MoviePlayer, mcCallbackProc), TK_OPTION_NULL_OK,
    (ClientData) NULL, 0},
    {TK_OPTION_BOOLEAN, "-mcedit", "mcEdit", "MCEdit",
	"0", -1, Tk_Offset(MoviePlayer, mcEdit), 0,
    (ClientData) NULL, MOV_CONF_MCEDIT},
    {TK_OPTION_BOOLEAN, "-palindromeloopstate", "palindromeLoopstate", "PalindromeLoopstate",
	"0", -1, Tk_Offset(MoviePlayer, palindromeloopstate), 0,
    (ClientData) NULL, MOV_CONF_PALINDROME_LOOPSTATE},
    {TK_OPTION_DOUBLE, "-preferredrate", "preferredRate", "PreferredRate",
	"1.0", -1, Tk_Offset(MoviePlayer, preferredRate), 0,
    (ClientData) NULL, MOV_CONF_PREFERRED_RATE},
    {TK_OPTION_STRING, "-progressproc", "progressProc", "ProgressProc",
	NULL, -1, Tk_Offset(MoviePlayer, progressProc), TK_OPTION_NULL_OK,
    (ClientData) NULL, MOV_CONF_PROGRESS_PROC},
    {TK_OPTION_BOOLEAN, "-qtprogress", "qtProgress", "QTProgress",
	"1", -1, Tk_Offset(MoviePlayer, qtprogress), 0,		
    (ClientData) NULL, MOV_CONF_PROGRESS_PROC},
    {TK_OPTION_STRING_TABLE, "-qtvrqualitystatic", "qtvrQualityStatic", "QTVRQualityStatic",
	"normal", -1, Tk_Offset(MoviePlayer, indQTVRQualityStatic), 0,
    (ClientData) kQTVRQualityArr, MOV_CONF_QTVR_QUALITY_STATIC},
    {TK_OPTION_STRING_TABLE, "-qtvrqualitymotion", "qtvrQualityMotion", "QTVRQualityMotion",
	"normal", -1, Tk_Offset(MoviePlayer, indQTVRQualityMotion), 0,
    (ClientData) kQTVRQualityArr, MOV_CONF_QTVR_QUALITY_MOTION},
    {TK_OPTION_BOOLEAN, "-resizable", "resizable", "Resizable",
	"0", -1, Tk_Offset(MoviePlayer, resizable), 0,
    (ClientData) NULL, MOV_CONF_RESIZEABLE},
    {TK_OPTION_BOOLEAN, "-swing", "swing", "Swing",
	"0", -1, Tk_Offset(MoviePlayer, swing), 0,
    (ClientData) NULL, 0},
    {TK_OPTION_INT, "-swingspeed", "swingSpeed", "SwingSpeed",
	"5", -1, Tk_Offset(MoviePlayer,swingSpeed), 0,
    (ClientData) NULL, 0},
    {TK_OPTION_STRING, "-url", "url", "Url",
	NULL, -1, Tk_Offset(MoviePlayer, url), TK_OPTION_NULL_OK,
    (ClientData) NULL, MOV_CONF_URL},
    {TK_OPTION_INT, "-volume", "volume", "Volume",
	"255", -1, Tk_Offset(MoviePlayer, volume), 0,
    (ClientData) NULL, MOV_CONF_VOLUME},
    {TK_OPTION_PIXELS, "-width", "width", "Width",
	"0", -1, Tk_Offset(MoviePlayer, width), 0,
    (ClientData) NULL, MOV_CONF_NEWGWORLD},
    {TK_OPTION_END, (char *) NULL, (char *) NULL, (char *) NULL,
    (char *) NULL, 0, 0, 0, 0}
};

/*
 * Declarations for functions in this source file only.
 */

static int 		MoviePlayerWidgetCmd( ClientData clientData, 
			    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[] );	
static MoviePlayer	*CreateMoviePlayer( Tk_Window tkwin );
static int              ConfigureMoviePlayer( Tcl_Interp *interp, 
			     MoviePlayer *movPtr, int objc, Tcl_Obj *CONST objv[] );							    
static int              ConfigureQTVRMovie( Tcl_Interp *interp, 
			    MoviePlayer * movPtr, int setQTVRQualityStatic, 
			    int setQTVRQualityMotion );
static int 		GetMovie( MoviePlayer *movPtr );
static int 		GetMovieFromUrl( MoviePlayer *movPtr );
static void 		MoviePlayerDeletedProc( ClientData clientData );
static void 		MoviePlayerEventProc( ClientData clientData, 
			    XEvent *eventPtr );
static void             AddOrRemoveMovieController( MoviePlayer *movPtr );
static void		AddMovieToOpenMovies( MoviePlayer *movPtr );
static void             RemoveMovieFromOpenMovies( MoviePlayer *movPtr );
static void 		DisplayMovie( ClientData clientData );
static void 		DestroyMovie( MoviePlayer *movPtr );
static void		MoviePlayerFree( char *clientData );
static void             DisposeMovieAtIdle( ClientData clientData );
static void		MovieExitProc( ClientData clientData );
static void		ExitMoviePlayer( ClientData clientData );
static void             CheckMovieLoadState( MoviePlayer *movPtr );
static int 		MoviePlayerMacEvent( EventRecord *eventPtr, 
			    WindowRef serveWindowRef );
#ifdef _WIN32
static int		MoviePlayerMacEventWin( EventRecord *eventPtr );
#endif
static int 		ProcessVectorSubcmd( ClientData clientData, 
			    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[] );
static int 		ProcessSpriteSubcmd( ClientData clientData, 
			    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[] );
static pascal Boolean   MovieControllerCallbackFunction( MovieController mc, 
			    short action, void *params, long refCon );
static void             InstallExtendedSCCallbackProcedures( 
			    ComponentInstance ci, long refCon );
static pascal Boolean   MySCFilterDialogProc( DialogPtr dialogPtr, 
			    EventRecord * event, short *itemHit, long refCon );
static pascal OSErr 	MovieProgressFunction( Movie theMovie, short message, 
			    short whatOperation, Fixed percentDone, long refcon );									
static void             ControllerCallbackTimer( ClientData clientData );

static int		ComressMovieCmd(Tcl_Interp *interp, 
			    int objc, Tcl_Obj *CONST objv[], 
			    MoviePlayer *movPtr);
static int		NewMovieCmd(Tcl_Interp *interp, 
			    int objc, Tcl_Obj *CONST objv[], 
			    MoviePlayer *movPtr);
static int		FlattenMovieCmd(Tcl_Interp *interp, 
			    int objc, Tcl_Obj *CONST objv[], 
			    MoviePlayer *movPtr);
		
static void		AsyncLoadHandlerProc( ClientData clientData );
static void		AsyncLoadFree( MoviePlayer *movPtr );

#if TARGET_OS_MAC
static void		SetMoviePlayerRectBox( MoviePlayer *movPtr );
#endif

#if TARGET_API_MAC_CARBON
static pascal void      MoviePlayerCarbonTimer( EventLoopTimerRef theTimer, 
			    void *userData );
static OSStatus		InstallMovieIdlingEventLoopTimer( void );
static void 		TaskNeededSoonerCallback( TimeValue duration,
			    unsigned long flags, void *refcon );
static void		CarbonTimerNextTime( void );
static void		InstallCarbonWindowEventHandler( MoviePlayer *movPtr );
static OSStatus		MovieCarbonWindowEventHandler( EventHandlerCallRef callRef, 
			    EventRef eventRef, void *userData );
static void		MovieDestroyCarbonWindowHandlerCleanup( MoviePlayer *movPtr );
#endif

#ifdef _WIN32
LRESULT CALLBACK	QTWinProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam );
#endif

/*
 *----------------------------------------------------------------------
 *
 * MoviePlayerObjCmd --
 *
 *		Create a MoviePlayer widget.  
 *
 * Results:
 *		Standard TCL result
 *
 * Side effects:
 *		Creates a command and allocates memory.  Possibly allocates 
 *		mac structures INITS the Movie toolbox, this could conflict 
 *		with someone else
 *
 *----------------------------------------------------------------------
 */

int
MoviePlayerObjCmd( 
		  ClientData clientData, 		/* NULL */
		  Tcl_Interp *interp, 
		  int objc, 
		  Tcl_Obj *CONST objv[] )
{
    MoviePlayer 	*movPtr;
    Tk_Window   	tkwin;
    Tk_OptionTable 	optionTable;
    long        	gestalt;
    long		eventFlags = 0;
    static int  	enterMoviesCalled = 0;
    OSErr       	err = noErr;
    Rect        	aRect = { 0, 0, 10, 10 };   /* Just a dummy rect. */
    
    if (noErr != Gestalt( gestaltQuickTime, &gestalt )) {
	Tcl_SetObjResult( interp, Tcl_NewStringObj( "QuickTime not installed", -1 ) );
	return TCL_ERROR;
    }
    QTTclDebugPrintf( interp, 2, "MoviePlayerObjCmd" );
    
    /* 
     * Init QuickTime movies. 
     */
    
    if (!enterMoviesCalled) {
	long    attr;
	
        err = EnterMovies();
	if (err != noErr) {
            CheckAndSetErrorResult( interp, err );
	    return TCL_ERROR;
	}
	enterMoviesCalled++;
	gIsRunningFlags |= MOVIE_PLAYER_RUNS;
	gTrackScrap.movPtr = NULL;
	Tcl_CreateExitHandler( MovieExitProc, (ClientData) NULL );
	
        /* 
         * We use one and the same offscreen GWorld for all movies. 
         */
	
	err = MySafeNewGWorld( &gOffscreenGWorldPtr, 32, &aRect, NULL, NULL, 0 );
	if (err != noErr) {
	    CheckAndSetErrorResult( interp, err );
	    return TCL_ERROR;
	}
	
	/*
	 * Check the availablity of the QTVR manager.
	 */
	
	gHasQTVRManager = false; 
	if (noErr == Gestalt( gestaltQTVRMgrAttr, &attr )) {
	    if (attr & (1 << gestaltQTVRMgrPresent)) {
		gHasQTVRManager = true; 
	    }
	} 
		
	/*
	 * The video effects record needs to be inited.
	 */
	
	EffectsInit();
        
        QTTclDebugPrintf( interp, 2, "KeyPress=%2d KeyRelease=%2d ButtonPress=%2d ButtonRelease=%2d",
			 KeyPress, KeyRelease, ButtonPress, ButtonRelease );
        QTTclDebugPrintf( interp, 2, "FocusIn=%2d FocusOut=%2d Expose=%2d DestroyNotify=%2d UnmapNotify=%2d MapNotify=%2d ConfigureNotify=%2d",
			 FocusIn, FocusOut, Expose, DestroyNotify, UnmapNotify, MapNotify, ConfigureNotify );
        QTTclDebugPrintf( interp, 3, "\tgOffscreenGWorldPtr=0x%.8x", gOffscreenGWorldPtr );
        QTTclDebugPrintf( interp, 3, 
			 "\tREDRAW_PENDING=%d, NEWGWORLD=%d, UPDATEMOVIE=%d", 
			 REDRAW_PENDING, NEWGWORLD, UPDATEMOVIE );
    }
    
    if (objc < 2) {
	Tcl_WrongNumArgs( interp, 1, objv, "pathName ?options?" );
	return TCL_ERROR;
    }	
    tkwin = Tk_CreateWindowFromPath( interp, Tk_MainWindow(interp), 
				    Tcl_GetString(objv[1]), (char *) NULL );
    if (tkwin == NULL) {
	return TCL_ERROR;
    }
    
    /*
     * Create the option table for this widget class.  If it has already
     * been created, the cached pointer will be returned.
     */
    
    optionTable = Tk_CreateOptionTable( interp, MoviePlayerConfigSpecs );
    
    Tk_SetClass( tkwin, "Movie" );
    movPtr = CreateMoviePlayer( tkwin );
    
    Tk_SetClassProcs( tkwin, &MoviePlayerProcs, (ClientData) movPtr );
    movPtr->tkwin = tkwin;
    movPtr->display = Tk_Display( tkwin );
    movPtr->interp = interp;
    movPtr->widgetCmd = Tcl_CreateObjCommand( interp, Tk_PathName(movPtr->tkwin),
					     MoviePlayerWidgetCmd, (ClientData) movPtr, MoviePlayerDeletedProc );
    movPtr->optionTable = optionTable;
    
    /*
     * Movie Player specific attributes. 
     */
    
    movPtr->aMovie = NULL;
    movPtr->filename = NULL;
    movPtr->fileNamePtr	= NULL;
    movPtr->url = NULL;	
    movPtr->flags = 0;
    movPtr->state = 0;
    movPtr->width = 0;
    movPtr->height = 0;
    movPtr->mheight = 0;
    movPtr->mwidth = 0;
    movPtr->padx = 0;
    movPtr->pady = 0;
    movPtr->highlightWidth = 0;
    movPtr->inset = movPtr->padx + movPtr->highlightWidth;
    movPtr->volume = 255;
    movPtr->isVisual = 1;
    movPtr->undoCount = 0;
    movPtr->editStates = NULL;
    movPtr->editStatesSize = 0;
    movPtr->loadIntoRam = 0;
    movPtr->qtvrInstance = NULL;
    movPtr->indQTVRQualityStatic = -1;
    movPtr->indQTVRQualityMotion = -1;
    movPtr->funcQTVRIntercept = NULL;
    movPtr->swing = 0;
    movPtr->swingSpeed = 5;
    movPtr->trackSelect->trackID = 0;
    
    /* A resource number -1 means movie in data fork. */
    movPtr->resourceNumber = -1;
    movPtr->progressProc = NULL;
    movPtr->grafPtr = NULL;
    movPtr->qtprogress = 1;
    movPtr->mcCallbackProc = NULL;
    movPtr->loadCommand = NULL;
    movPtr->loadState = 0;
    movPtr->aController = NULL;
    movPtr->wantController = 1;
    movPtr->controllerHeight = 0;
    movPtr->mcEdit = 0;
    movPtr->resizable = 0;
    movPtr->custombutton = 0;
    movPtr->loopstate = 0;
    movPtr->palindromeloopstate = 0;
    movPtr->rate = (Fixed) 0;
    movPtr->preferredRate = 1.0;	
    movPtr->insideMCCommand = 0;
    movPtr->asyncLoadHashTablePtr = NULL;
    movPtr->callBackHashTablePtr = NULL;
    movPtr->tmpMovieRecordPtr = NULL;
    
    QTTclDebugPrintf( interp, 2, "\tmovPtr=%d", movPtr );
    
    /*
     * This is an extremly important choice of which events shall be handled
     * by tk. Using the ExposureMask seems to cause infinite loops, 
     * in particular when the controller is obscured.
     */
    
#if TARGET_OS_MAC
    eventFlags = StructureNotifyMask | FocusChangeMask | 
    VisibilityChangeMask | KeyReleaseMask | ButtonPressMask;
#else
    eventFlags = ExposureMask | StructureNotifyMask | FocusChangeMask | 
    VisibilityChangeMask | KeyReleaseMask | ButtonPressMask;
#endif
    Tk_CreateEventHandler( movPtr->tkwin, eventFlags,
			  MoviePlayerEventProc, (ClientData) movPtr );
    
    if (Tk_InitOptions( interp, (char *) movPtr, optionTable, movPtr->tkwin )
	!= TCL_OK) {
        Tk_DestroyWindow( movPtr->tkwin );
        return TCL_ERROR;
    }
    
    /* 
     * Configure the widget; parse the command line arguments and look for defaults
     * in the resource database. The last 0 means to ...
     */
    
    if (ConfigureMoviePlayer( interp, movPtr, objc - 2, objv + 2 ) 
	!= TCL_OK) {
	Tk_DestroyWindow( movPtr->tkwin );
	return TCL_ERROR;
    }
    
    /*
     * Windows specific code to create a Mac style graphics port, and associate
     * it with a Windows HWND. Get the winproc given by tk, save it to be called
     * later, and set our own winproc.
     */
    
#ifdef _WIN32
    {
        HWND        tempHwnd = NULL;
	
        if (Tk_WindowId(movPtr->tkwin) == None) {
            Tk_MakeWindowExist( movPtr->tkwin );
        }	
        tempHwnd = TkWinGetHWND( Tk_WindowId(movPtr->tkwin) );
        movPtr->hwnd = tempHwnd;
    	CreatePortAssociation( tempHwnd, NULL, 0L );
        movPtr->winEventProc = GetWindowLong( tempHwnd, GWL_WNDPROC );
        SetWindowLong( tempHwnd, GWL_WNDPROC, (LONG) QTWinProc );
    }
#endif  // _WIN32

    /*
     * We should add our movie to our global list of open movies already here.
     * This is mainly to get a handle on the original WndProc on Windows, so it
     * can get called even if not widget on display.
     */
    
    AddMovieToOpenMovies( movPtr );
    
    /*
     * Set the NEWGWORLD in 'flags' to require an update and to find the geometry.
     * If a remote movie is async loading, a 1x1 size is given in 'MoviePlayerWorldChanged'.
     */
    
    movPtr->flags |= NEWGWORLD;
    movPtr->flags |= NEED_PREROLL;
    MoviePlayerWorldChanged( (ClientData) movPtr );
    Tcl_SetObjResult( interp, Tcl_NewStringObj( Tk_PathName(movPtr->tkwin), -1 ) );
    return TCL_OK;
}
/*
 *----------------------------------------------------------------------
 *
 * MoviePlayerWidgetCmd --
 *
 *		Command to run for each widget.
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
MoviePlayerWidgetCmd( ClientData clientData, 
		     Tcl_Interp *interp,
		     int objc, 
		     Tcl_Obj *CONST objv[] ) 
{	
    MoviePlayer     *movPtr = (MoviePlayer *) clientData;
    Movie	    aMovie = movPtr->aMovie;
    short           resId;		
    short           movieResFile;
    unsigned short  vol;
    int             i;
    int             undoLevel;
    int		    sizeMayChange;
    int             iarg;
    int             cmdIndex;
    int             optIndex;
    int		    boolValue;
    int		    intValue;
    long            flags;
    long	    mask;
    long	    longValue;
    double          theRate;
    Tcl_Obj	    *resultObjPtr;
    Tcl_Obj	    *listObjPtr;
    Fixed           rate;
    TimeValue       movTime;
    TimeValue       movDuration;
    FSSpec          fss;
    PicHandle       thePic = NULL;	
    Movie           tmpMovie = NULL;
    OSErr           err = noErr;
    ComponentResult compErr = badComponentType;
    int             result = TCL_OK;
    
    if (objc < 2) {
	Tcl_WrongNumArgs( interp, 1, objv, "command ?arg arg ...?" );
	return TCL_ERROR;
    }
    QTTclDebugPrintf( interp, 2, "MoviePlayerWidgetCmd" );
    
    Tcl_Preserve((ClientData) movPtr);
    if (Tcl_GetIndexFromObj( interp, objv[1], allMovieCmds, "command", TCL_EXACT, &cmdIndex )
	!= TCL_OK ) {
	return TCL_ERROR;
    }
    
    /* Get the volume since the user may have changed it. */
    
    vol = GetMovieVolume( aMovie );
    movPtr->volume = vol;
    
    /*
     * Dispatch the movie command to the right branch.
     */
    
    switch (cmdIndex) {
	    
        case kMovieCmdAdd: {
	    
    	    /* 
    	     * Add tracks in parallel to the movie. 
    	     */
	    
    	    if (objc >= 3) {
		Tcl_WrongNumArgs( interp, 2, objv, NULL );
		result = TCL_ERROR;
		goto error;
    	    }
	    if (aMovie != NULL) {
		tmpMovie = NewMovieFromScrap(0);
		if (tmpMovie != NULL) {
		    result = LogUndoState( movPtr, interp );
		    AddMovieSelection( aMovie, tmpMovie );
		    /* // scales to fit...
		     GetMovieBox( tmpMovie, &aRect );
		     MacOffsetRect( &aRect, -aRect.left, -aRect.top );
		     if (aRect.right > movPtr->mwidth) {
		     movPtr->mwidth = aRect.right;
		     }
		     if (aRect.bottom > movPtr->mheight) {
		     movPtr->mheight = aRect.bottom;
		     }
		     */
		    DisposeMovie( tmpMovie );
		} else {
		    CheckAndSetErrorResult( interp, noErr );
		    result = TCL_ERROR;
		}
		if (movPtr->aController != NULL) {
		    MCDoAction( movPtr->aController,  mcActionMovieEdited, NULL );
		    MCMovieChanged( movPtr->aController, aMovie );
		} else {
		    MoviePlayerWorldChanged( clientData );
		}
	    } else {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
		result = TCL_ERROR;
	    }
	    break;
        }
	    
        case kMovieCmdCallBack: {
	    if (objc < 3) {
		Tcl_WrongNumArgs( interp, 2, objv, "time|cmd ?args?" );
		goto error;
	    }
	    if (aMovie != NULL) {
		if (ProcessCallBackCmd( movPtr, objc-2, objv+2 ) != TCL_OK) {
		    result = TCL_ERROR;
		}
	    } else {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
		result = TCL_ERROR;
	    }
	    break;
        }
	    
        case kMovieCmdCget: {
	    
    	    /*
    	     * Just return an option.
    	     */
	    
	    if (objc != 3) {
		Tcl_WrongNumArgs( interp, 2, objv, "option" );
		result = TCL_ERROR;
		goto error;
	    }
	    resultObjPtr = Tk_GetOptionValue( interp, (char *) movPtr,
					     movPtr->optionTable, objv[2], movPtr->tkwin );
	    if (resultObjPtr == NULL) {
		result = TCL_ERROR;
	    } else {
		Tcl_SetObjResult( interp, resultObjPtr );
	    }
	    break;
        }
	    
        case kMovieCmdClear: {
	    
    	    /*
    	     * Clear any movie selection.
    	     */
	    
	    if (objc != 2) {
		Tcl_WrongNumArgs( interp, 2, objv, NULL );
		goto error;
	    }
	    if (aMovie != NULL) {
		result = LogUndoState( movPtr, interp );
		if (movPtr->aController && MCIsEditingEnabled( movPtr->aController )) {
		    if (noErr != MCClear( movPtr->aController )) {
			CheckAndSetErrorResult( interp, noErr );
			result = TCL_ERROR;
			goto error;
		    }
		} else {
		    ClearMovieSelection( aMovie );
		}
		if (movPtr->aController != NULL) {
		    MCDoAction( movPtr->aController,  mcActionMovieEdited, NULL );
		    MCMovieChanged( movPtr->aController, aMovie );
		} else {
		    MoviePlayerWorldChanged( clientData );
		}
	    } else {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
		result = TCL_ERROR;
	    }
	    break;
        }
	    
        case kMovieCmdConfigure: {
	    
    	    /*
    	     * Configure an option.
    	     */
	    
	    resultObjPtr = NULL;
	    if (objc <= 3) {
		resultObjPtr = Tk_GetOptionInfo( interp, (char *) movPtr,
						movPtr->optionTable,
						(objc == 2) ? (Tcl_Obj *) NULL : objv[2],
						movPtr->tkwin );
		if (resultObjPtr == NULL) {
		    result = TCL_ERROR;
		} else {
		    Tcl_SetObjResult( interp, resultObjPtr );
		}
	    } else {
    		
            	/* 
            	 * Configure the widget; parse the command line arguments and look for defaults
            	 * in the resource database. 
            	 */
		
            	result = ConfigureMoviePlayer( interp, movPtr, objc - 2, objv + 2 );
		
                /* 
                 * Only if we made a configuration that needs a redisplay. 
                 */
		
		if ((result == TCL_OK) && (movPtr->flags & NEWGWORLD)) {		
		    MoviePlayerWorldChanged( (ClientData) movPtr );
		}
	    }
	    break;
        }
	    
        case kMovieCmdCompress: {
	    
	    /* 
	     * Compress the movie. 
	     */
	    
	    if (!Tk_IsMapped(movPtr->tkwin)) {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( "Movie must be displayed", -1 ) );
		result = TCL_ERROR;
		goto error;
	    }
	    if (objc < 3) {
		Tcl_WrongNumArgs( interp, 2, objv, "filename" );
		result = TCL_ERROR;
		goto error;
	    }
	    result = ComressMovieCmd(interp, objc, objv, movPtr);
	    if (result != TCL_OK) {
		goto error;
	    }
	    break;
        }
	    
        case kMovieCmdControllerinfo: {
	    
	    /* 
	     * Gets info about the state of the movie controller. 
	     */
	    
	    if (objc != 2) {
		Tcl_WrongNumArgs( interp, 2, objv, NULL );
		result = TCL_ERROR;
		goto error;
	    }
	    if (aMovie != NULL) {
		if (movPtr->aController != NULL) {
		    
		    MCGetControllerInfo( movPtr->aController, &flags );
		    listObjPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );
		    Tcl_ListObjAppendElement( interp, listObjPtr, 
					     Tcl_NewStringObj("-undoavailable", -1) );		    	
		    Tcl_ListObjAppendElement( interp, listObjPtr, 
					     Tcl_NewBooleanObj(flags & mcInfoUndoAvailable) );		    	
		    Tcl_ListObjAppendElement( interp, listObjPtr, 
					     Tcl_NewStringObj("-cutavailable", -1) );		    	
		    Tcl_ListObjAppendElement( interp, listObjPtr, 
					     Tcl_NewBooleanObj(flags & mcInfoCutAvailable) );		    	
		    Tcl_ListObjAppendElement( interp, listObjPtr, 
					     Tcl_NewStringObj("-copyavailable", -1) );		    	
		    Tcl_ListObjAppendElement( interp, listObjPtr, 
					     Tcl_NewBooleanObj(flags & mcInfoCopyAvailable) );		    	
		    Tcl_ListObjAppendElement( interp, listObjPtr, 
					     Tcl_NewStringObj("-pasteavailable", -1) );		    	
		    Tcl_ListObjAppendElement( interp, listObjPtr, 
					     Tcl_NewBooleanObj(flags & mcInfoPasteAvailable) );		    	
		    Tcl_ListObjAppendElement( interp, listObjPtr, 
					     Tcl_NewStringObj("-clearavailable", -1) );		    	
		    Tcl_ListObjAppendElement( interp, listObjPtr, 
					     Tcl_NewBooleanObj(flags & mcInfoClearAvailable) );		    	
		    Tcl_ListObjAppendElement( interp, listObjPtr, 
					     Tcl_NewStringObj("-hassound", -1) );		    	
		    Tcl_ListObjAppendElement( interp, listObjPtr, 
					     Tcl_NewBooleanObj(flags & mcInfoHasSound) );		    	
		    Tcl_ListObjAppendElement( interp, listObjPtr, 
					     Tcl_NewStringObj("-isplaying", -1) );		    	
		    Tcl_ListObjAppendElement( interp, listObjPtr, 
					     Tcl_NewBooleanObj(flags & mcInfoIsPlaying) );		    	
		    Tcl_ListObjAppendElement( interp, listObjPtr, 
					     Tcl_NewStringObj("-islooping", -1) );		    	
		    Tcl_ListObjAppendElement( interp, listObjPtr, 
					     Tcl_NewBooleanObj(flags & mcInfoIsLooping) );		    	
		    Tcl_ListObjAppendElement( interp, listObjPtr, 
					     Tcl_NewStringObj("-isinpalindrome", -1) );		    	
		    Tcl_ListObjAppendElement( interp, listObjPtr, 
					     Tcl_NewBooleanObj(flags & mcInfoIsInPalindrome) );		    	
		    Tcl_ListObjAppendElement( interp, listObjPtr, 
					     Tcl_NewStringObj("-editingenebled", -1) );		    	
		    Tcl_ListObjAppendElement( interp, listObjPtr, 
					     Tcl_NewBooleanObj(flags & mcInfoEditingEnabled) );		    	
		    
		    Tcl_SetObjResult( interp, listObjPtr );
		} else {
		    Tcl_SetObjResult( interp, 
				     Tcl_NewStringObj( "No movie controller", -1 ) );
		    result = TCL_ERROR;
		}
	    } else {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
		result = TCL_ERROR;
	    }
	    break;
        }
	    
        case kMovieCmdCopy: {
	    
    	    /* 
    	     * Copy the Movie selection. Any track scrap must be invalidated.
    	     */
	    
	    if (aMovie != NULL) {
		if (movPtr->aController && MCIsEditingEnabled( movPtr->aController )) {
		    tmpMovie = MCCopy( movPtr->aController );
		} else {
		    tmpMovie = CopyMovieSelection( aMovie );
		}
		if (tmpMovie != NULL) {
		    PutMovieOnScrap( tmpMovie, 0 );
		    DisposeMovie( tmpMovie );
		    gTrackScrap.movPtr = NULL;
		} else {
		    CheckAndSetErrorResult( interp, noErr );
		}
	    } else {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
		result = TCL_ERROR;
	    }
	    break;
        }
	    
        case kMovieCmdCut: {
	    
    	    /* 
    	     * Cut the Movie selection. Any track scrap must be invalidated.
    	     */
	    
	    if (aMovie != NULL) {
		result = LogUndoState( movPtr, interp );
		if (movPtr->aController && MCIsEditingEnabled( movPtr->aController )) {
		    tmpMovie = MCCut( movPtr->aController );
		} else {
		    tmpMovie = CutMovieSelection( aMovie );
		}
		if (tmpMovie != NULL) {
		    PutMovieOnScrap( tmpMovie, 0 );
		    DisposeMovie( tmpMovie );
		    gTrackScrap.movPtr = NULL;
		} else {
		    CheckAndSetErrorResult( interp, noErr );
		}
		if (movPtr->aController != NULL) {
		    MCDoAction( movPtr->aController,  mcActionMovieEdited, NULL );
		    MCMovieChanged( movPtr->aController, aMovie );
		}
	    } else {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
		result = TCL_ERROR;
	    }
	    break;
        }
	    
        case kMovieCmdEffect: {
	    
    	    /* 
    	     * Make an effect, from 0, 1, or 2 sources. Displays an effect dialog.
    	     */
	    
#if TARGET_API_MAC_CARBON
            Tcl_SetObjResult( interp, Tcl_NewStringObj( "No effects on Mac OS X yet", -1 ) );
            result = TCL_ERROR;
            break;
#endif
	    if (aMovie != NULL) {
		
                /* Continues in 'EffectsRespondToDialogSelection'... */
                if (ProcessEffectCmd( interp, movPtr, objc - 2, objv + 2 ) 
		    != TCL_OK) {
		    result = TCL_ERROR;
		    goto error;
                }
	    } else {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
		result = TCL_ERROR;
	    }
	    break;
        }
	    
        case kMovieCmdExport: {
	    
    	    /* 
    	     * Export the Movie from a dialog. Return file path or empty.
    	     */
	    
    	    if (Tcl_IsSafe( interp )) {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( 
							   "\"export\" not allowed in a safe interpreter", -1 ) );
		result = TCL_ERROR;
            	goto error;
    	    } 
	    if (!Tk_IsMapped(movPtr->tkwin)) {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( "Movie must be displayed", -1 ) );
		result = TCL_ERROR;
		goto error;
	    }
	    if (aMovie != NULL) {
                if (ProcessExportCmd( interp, aMovie, objc - 2, objv + 2 ) != TCL_OK) {
		    result = TCL_ERROR;
		    goto error;
                }
	    } else {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
		result = TCL_ERROR;
	    }
	    break;
        }
	    
        case kMovieCmdFieldofview: {
	    
    	    /* 
    	     * Set or get fieldofview of QTVR pano. 
    	     */
	    
	    if (objc > 3) {
		Tcl_WrongNumArgs( interp, 2, objv, "?fov?" );
		result = TCL_ERROR;
		goto error;
	    }
	    if (!gHasQTVRManager) {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( 
							   "Couldn't find the QTVR manager on this system", -1 ) );
		result = TCL_ERROR;
		goto error;
	    }
	    if (aMovie != NULL) {
		double   fov;
		
		if (movPtr->qtvrInstance == NULL) {
		    Tcl_SetObjResult( interp, Tcl_NewStringObj( 
							       "Couldn't identify this movie as a VR movie", -1 ) );
		    result = TCL_ERROR;
		    goto error;
		}
		if (objc == 3) {
		    if (Tcl_GetDoubleFromObj( interp, objv[2], &fov ) != TCL_OK) {
			Tcl_AddErrorInfo( interp, 
					 "\n	(processing fieldofview command)" );
		    } else {
			ZoomInOrOutPanorama( movPtr->qtvrInstance, 0, (float) fov );
		    }
                } else {
                    fov = QTVRGetFieldOfView( movPtr->qtvrInstance );                    
                    Tcl_SetObjResult( interp, Tcl_NewDoubleObj(fov) );
                }
	    } else {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
		result = TCL_ERROR;
            }
	    break;
        }
	    
        case kMovieCmdFlatten: {
		    
    	    /* 
    	     * Flatten the movie into the datafork of a new file.
    	     * A separate mechanism is used for QTVR panos.
    	     */
	    
    	    if (Tcl_IsSafe( interp )) {
    	    	Tcl_SetObjResult( interp, 
				 Tcl_NewStringObj("\"flatten\" not allowed in a safe interpreter", -1) );
		result = TCL_ERROR;
            	goto error;
    	    } 
	    if (objc < 3) {
		Tcl_WrongNumArgs( interp, 2, objv, "fileName ?args?" );
		result = TCL_ERROR;
		goto error;
	    }
	    if (aMovie == NULL) {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
		result = TCL_ERROR;
		goto error;
	    }
	    result = FlattenMovieCmd(interp, objc, objv, movPtr);
	    if (result != TCL_OK) {
		goto error;
	    }
	    break;
        }
	    
        case kMovieCmdGetrate: {
	    
    	    /* 
    	     * Get the movie rate. Must be playing. Now obsolete! Use "rate" instead.
    	     */
	    
	    if (aMovie != NULL) {
		Tcl_SetObjResult( interp, Tcl_NewDoubleObj( 
							   Fix2X( GetMovieRate(aMovie) ) ) );
	    } else {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
		result = TCL_ERROR;
	    }
	    break;
        }
	    
        case kMovieCmdGetpreferredrate: {
	    
    	    /* 
    	     * Get the movie preferred rate. Need not be playing. Now obsolete!
    	     */
	    
	    if (aMovie != NULL) {
		Tcl_SetObjResult( interp, Tcl_NewDoubleObj( 
							   Fix2X( GetMoviePreferredRate(aMovie) ) ) );
	    } else {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
		result = TCL_ERROR;
	    }
	    break;
        }
	    
        case kMovieCmdGetselection: {
	    
    	    /*
    	     * Get the movie selection.  Now obsolete, use "select".
    	     */
	    
	    if (aMovie != NULL) {
		GetMovieSelection(aMovie, &movTime, &movDuration);
		if (noErr != CheckAndSetErrorResult( interp, noErr )) {
                    result = TCL_ERROR;
                } else {    
		    listObjPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );
		    Tcl_ListObjAppendElement( interp, listObjPtr, 
					     Tcl_NewStringObj("-selectionTime", -1) );
		    Tcl_ListObjAppendElement( interp, listObjPtr, 
					     Tcl_NewLongObj(movTime) );
		    Tcl_ListObjAppendElement( interp, listObjPtr, 
					     Tcl_NewStringObj("-selectionDuration", -1) );
		    Tcl_ListObjAppendElement( interp, listObjPtr, 
					     Tcl_NewLongObj(movDuration) );
		    Tcl_SetObjResult( interp, listObjPtr );
		}		
	    } else {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
		result = TCL_ERROR;
	    }
	    break;
        }
	    
        case kMovieCmdGettime: {
	    
    	    /* 
    	     * Return the time elapsed for the movie. 
    	     */
	    
	    if (aMovie != NULL) {
		listObjPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );
		Tcl_ListObjAppendElement( interp, listObjPtr, 
					 Tcl_NewStringObj("-movietime", -1) );
		Tcl_ListObjAppendElement( interp, listObjPtr, 
					 Tcl_NewLongObj(GetMovieTime( aMovie, NULL )) );
		Tcl_ListObjAppendElement( interp, listObjPtr, 
					 Tcl_NewStringObj("-movieduration", -1) );
		Tcl_ListObjAppendElement( interp, listObjPtr, 
					 Tcl_NewLongObj(GetMovieDuration( aMovie )) );
		Tcl_ListObjAppendElement( interp, listObjPtr, 
					 Tcl_NewStringObj("-movietimescale", -1) );
		Tcl_ListObjAppendElement( interp, listObjPtr, 
					 Tcl_NewLongObj(GetMovieTimeScale( aMovie )) );
		Tcl_ListObjAppendElement( interp, listObjPtr, 
					 Tcl_NewStringObj("-postertime", -1) );
		Tcl_ListObjAppendElement( interp, listObjPtr, 
					 Tcl_NewLongObj(GetMoviePosterTime( aMovie )) );
		Tcl_SetObjResult( interp, listObjPtr );
	    } else {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
		result = TCL_ERROR;
	    }
	    break;
        }
	    
        case kMovieCmdHasChanged: {
	    
    	    /* 
    	     * Return true if we edited the movie after it has been saved. 
    	     */
	    
	    if (aMovie != NULL) {
		Tcl_SetObjResult( interp, Tcl_NewBooleanObj(
							    HasMovieChanged( aMovie ) ? true : false) );
    	    	if (noErr != CheckAndSetErrorResult( interp, noErr )) {
    	            result = TCL_ERROR;
    	            goto error;
    	        }
	    } else {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
		result = TCL_ERROR;
	    }
	    break;
        }
	    
        case kMovieCmdHotspot: {
	    
    	    /* 
    	     * The hotspot sub command. 
    	     */
	    
	    if (aMovie != NULL) {
		if (IsQTVRMovie( aMovie ) && (movPtr->qtvrInstance != NULL)) {
		    if (TCL_OK != ProcessHotspotSubCmd( interp, aMovie,
						       movPtr->qtvrInstance, objc - 2, objv + 2 )) {
			result = TCL_ERROR;
                    }    
                } else {
		    Tcl_SetObjResult( interp, Tcl_NewStringObj( "Not a QTVR movie", -1 ) );
		    result = TCL_ERROR;
                }
	    } else {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
		result = TCL_ERROR;
	    }
	    break;
        }
	    
        case kMovieCmdIsdone: {
	    
    	    /* 
    	     * Is the movie done? 
    	     */
	    
	    if (aMovie != NULL) {
		Tcl_SetObjResult( interp, Tcl_NewBooleanObj(
							    IsMovieDone( aMovie ) ? true : false) );
	    } else {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
		result = TCL_ERROR;
	    }
	    break;
        }
	    
        case kMovieCmdIsmovie: {
	    
    	    /* 
    	     * Is the movie an ordinary movie and not a VR panoramic movie? 
    	     */
	    
	    if (aMovie != NULL) {
		Tcl_SetObjResult( interp, Tcl_NewBooleanObj(
							    IsQTVRMovie( aMovie ) ? false : true) );
	    } else {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
		result = TCL_ERROR;
	    }
	    break;
        }
	    
        case kMovieCmdIspanoramic: {
	    
    	    /* 
    	     * Is the movie a VR panoramic movie? 
    	     */
	    
	    if (aMovie != NULL) {
		Tcl_SetObjResult( interp, Tcl_NewBooleanObj(
							    IsQTVRMovie( aMovie ) ? true : false) );
	    } else {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
		result = TCL_ERROR;
	    }
	    break;
        }
	    
        case kMovieCmdIsscrapmovie: {
	    
    	    /* 
    	     * Is there a movie on the clipboard (scrap)? 
    	     */
	    
    	    { 
    	        Track       targetTrack = NULL;
    	        
		if (aMovie != NULL) {
		    Tcl_SetObjResult( interp, Tcl_NewBooleanObj(
								IsScrapMovie( targetTrack ) ? true : false) );
		} else {
		    Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
		    result = TCL_ERROR;
		}
	    }
	    break;
        }
	    
        case kMovieCmdIsvisual: {
	    
    	    /* 
    	     * Is there there something for our eyes? 
    	     */
	    
	    if (aMovie != NULL) {
		Tcl_SetObjResult( interp, Tcl_NewBooleanObj(
							    (GetMovieIndTrackType( aMovie, 1, 
										  VisualMediaCharacteristic, 
										  movieTrackCharacteristic ) == NULL) ? false : true) );    			        	
	    } else {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
		result = TCL_ERROR;
	    }
	    break;
        }
	    
        case kMovieCmdNew: {

    	    /* 
    	     * Create a new movie. 
    	     */
	    
	    if (objc != 3) {
		Tcl_WrongNumArgs( interp, 2, objv, "fileName" );
		result = TCL_ERROR;
		goto error;
	    }
	    if (movPtr->fileNamePtr != NULL) {
		Tcl_SetObjResult( interp, 
			Tcl_NewStringObj( "There is already a file name associated with this movie", -1 ) );
		result = TCL_ERROR;
		goto error;
	    }
	    result = NewMovieCmd(interp, objc, objv, movPtr);
	    if (result != TCL_OK) {
		goto error;
	    }
	    /* get rid of the old one */
	    break;
	}
	    
        case kMovieCmdNextInterestingTime: {
	    TimeValue	interestingTime;
	    TimeValue	interestingDuration;
	    OSType	*mediaTypesPtr;
	    long	lType;
	    int		num;
	    short	numMediaTypes = 0;
	    Tcl_Obj 	*objPtr;
	    
	    if (objc == 3) {
		movTime = GetMovieTime( aMovie, NULL );
	    } else if (objc == 4) {
		if (Tcl_GetIntFromObj( interp, objv[3], &intValue ) != TCL_OK) {
		    Tcl_AddErrorInfo( interp, "\n	(processing time value)" );
		    result = TCL_ERROR;
		    goto error;
		}
		movTime = intValue;
	    } else {
		Tcl_WrongNumArgs( interp, 2, objv, "mediaTypeList ?movieTime?" );
		return TCL_ERROR;
	    }
	    
	    /* 
	     * Get media types from list. 
	     */
	    
	    if (Tcl_ListObjLength( interp, objv[2], &num ) != TCL_OK) {
		return TCL_ERROR;
	    }
	    numMediaTypes = (short) num;
	    mediaTypesPtr = (OSType *) ckalloc( numMediaTypes * sizeof(OSType) );
	    for (i = 0; i < numMediaTypes; i++) {			
		if (Tcl_ListObjIndex( interp, objv[2], i, &objPtr ) != TCL_OK) {
		    return TCL_ERROR;
		}
		memcpy( &lType, Tcl_GetString( objPtr ), 4 );
		mediaTypesPtr[i] = EndianU32_NtoB( lType );
	    }
	    
	    listObjPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );
	    
	    GetMovieNextInterestingTime( aMovie, 
					nextTimeMediaSample | nextTimeEdgeOK, 
					numMediaTypes, mediaTypesPtr, movTime, fixed1, 
					&interestingTime, &interestingDuration );
	    Tcl_ListObjAppendElement( interp, listObjPtr, 
				     Tcl_NewStringObj("-sampletime", -1) );				
	    Tcl_ListObjAppendElement( interp, listObjPtr, 
				     Tcl_NewLongObj(interestingTime) );				
	    Tcl_ListObjAppendElement( interp, listObjPtr, 
				     Tcl_NewStringObj("-sampleduration", -1) );				
	    Tcl_ListObjAppendElement( interp, listObjPtr, 
				     Tcl_NewLongObj(interestingDuration) );				
	    
	    GetMovieNextInterestingTime( aMovie, 
					nextTimeMediaEdit | nextTimeEdgeOK,
					numMediaTypes, mediaTypesPtr, movTime, fixed1, 
					&interestingTime, &interestingDuration );
	    Tcl_ListObjAppendElement( interp, listObjPtr, 
				     Tcl_NewStringObj("-edittime", -1) );				
	    Tcl_ListObjAppendElement( interp, listObjPtr, 
				     Tcl_NewLongObj(interestingTime) );				
	    Tcl_ListObjAppendElement( interp, listObjPtr, 
				     Tcl_NewStringObj("-editduration", -1) );				
	    Tcl_ListObjAppendElement( interp, listObjPtr, 
				     Tcl_NewLongObj(interestingDuration) );				
	    
	    GetMovieNextInterestingTime( aMovie, 
					nextTimeSyncSample | nextTimeEdgeOK,
					numMediaTypes, mediaTypesPtr, movTime, fixed1, 
					&interestingTime, &interestingDuration );
	    Tcl_ListObjAppendElement( interp, listObjPtr, 
				     Tcl_NewStringObj("-syncsampletime", -1) );				
	    Tcl_ListObjAppendElement( interp, listObjPtr, 
				     Tcl_NewLongObj(interestingTime) );				
	    Tcl_ListObjAppendElement( interp, listObjPtr, 
				     Tcl_NewStringObj("-syncsampleduration", -1) );				
	    Tcl_ListObjAppendElement( interp, listObjPtr, 
				     Tcl_NewLongObj(interestingDuration) );				
	    
	    err = GetMoviesError();
	    if (err!= noErr) {
		Tcl_DecrRefCount( listObjPtr );
		CheckAndSetErrorResult( interp, err );
		return TCL_ERROR;
	    }
	    Tcl_SetObjResult( interp, listObjPtr );			     
	    break;
        }
	    
        case kMovieCmdPan: {
	    
    	    /* 
    	     * Set or get pan angle of QTVR pano. 
    	     */
	    
	    if (objc > 3) {
		Tcl_WrongNumArgs( interp, 2, objv, "?angle?" );
		result = TCL_ERROR;
		goto error;
	    }
	    if (!gHasQTVRManager) {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( 
							   "Couldn't find the QTVR manager on this system", -1 ) );
		result = TCL_ERROR;
		goto error;
	    }
	    if (aMovie != NULL) {
		double   angle;
		
		if (movPtr->qtvrInstance == NULL) {
		    Tcl_SetObjResult( interp, Tcl_NewStringObj( 
							       "Couldn't identify this movie as a VR movie", -1 ) );
		    result = TCL_ERROR;
		    goto error;
		}
		if (objc == 3) {
		    if (Tcl_GetDoubleFromObj( interp, objv[2], &angle ) != TCL_OK) {
			Tcl_AddErrorInfo( interp, 
					 "\n	(processing pan command)" );
		    } else {
			SetPanoramaByDegrees( movPtr->qtvrInstance, kDirLeft, (float) angle );
		    }
                } else {
		    Tcl_SetObjResult( interp, Tcl_NewDoubleObj( 
							       QTVRGetPanAngle( movPtr->qtvrInstance ) ) );
                }
	    } else {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
		result = TCL_ERROR;
            }
	    break;
        }
	    
        case kMovieCmdPanoInfo: {
	    
    	    /* 
    	     * Return info about pano movie. 
    	     */
	    
	    if (objc != 2) {
		Tcl_WrongNumArgs( interp, 2, objv, NULL );
		result = TCL_ERROR;
		goto error;
	    }
	    if (!gHasQTVRManager) {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( 
							   "Couldn't find the QTVR manager on this system", -1 ) );
		result = TCL_ERROR;
		goto error;
	    }
	    if (aMovie != NULL) {
                Tcl_Obj     *resObjPtr;
                UInt32      nodeID = kQTVRCurrentNode;
                //UInt32      nodeID = kQTVRDefaultNode;
		
		if (movPtr->qtvrInstance == NULL) {
		    Tcl_SetObjResult( interp, Tcl_NewStringObj( 
							       "Couldn't identify this movie as a VR movie", -1 ) );
		    result = TCL_ERROR;
		    goto error;
		}
		
                if ( TCL_OK != PanoramaGetInfoNode( interp, aMovie, movPtr->qtvrInstance, 
						   nodeID, &resObjPtr )) {
		    result = TCL_ERROR;
		    goto error;
		}
		Tcl_SetObjResult( interp, resObjPtr );
	    } else {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
		result = TCL_ERROR;
            }
	    break;
        }
	    
        case kMovieCmdPaste: {
	    
    	    /* 
    	     * Paste the movie or whatever that can be imported. Works differently
    	     * depending on if '-mcedit 1' (MCIsEditingEnabled) or not.
    	     */
	    
	    if (objc > 3) {
		Tcl_WrongNumArgs( interp, 2, objv, "?option?" );
		result = TCL_ERROR;
		goto error;
	    }
	    if (aMovie != NULL) {
		sizeMayChange = false;
		if ((movPtr->aController != NULL) && 
		    MCIsEditingEnabled( movPtr->aController )) {
		    long	modifiers = 0L;
		    char	*charPtr;
		    
		    if (objc == 3) {
			
			/*
			 * Set modifier flags for the controller.
			 */
			
			charPtr = Tcl_GetString( objv[2] );
			if (strcmp(charPtr, "scaled") == 0) {
			    modifiers |= shiftKey;
			    modifiers |= optionKey;
			    sizeMayChange = true;
			} else if (strcmp(charPtr, "replace") == 0) {
			    modifiers |= shiftKey;
			} else if (strcmp(charPtr, "parallel") == 0) {
			    modifiers |= optionKey;
			    sizeMayChange = true;
			    
#if TARGET_OS_MAC
			} else if (strcmp(charPtr, "dialog") == 0) {
			    modifiers |= controlKey;
			    modifiers |= optionKey;
			    sizeMayChange = true;
#endif			     		
			    
			} else {
			    Tcl_SetObjResult( interp, Tcl_NewStringObj( 
								       "unrecognized paste option", -1 ) );
			    result = TCL_ERROR;
			    goto error;
			}
		    }
		    result = LogUndoState( movPtr, interp );
		    compErr = MCSetUpEditMenu( movPtr->aController, modifiers, NULL );
		    if (compErr != noErr) {
			CheckAndSetErrorResult( interp, noErr );
			result = TCL_ERROR;
			goto error;
		    }
		    compErr = MCPaste( movPtr->aController, NULL );
		    if (compErr != noErr) {
			CheckAndSetErrorResult( interp, noErr );
			result = TCL_ERROR;
			goto error;
		    }
		} else {
		    if (objc == 3) {
			Tcl_SetObjResult( interp, Tcl_NewStringObj( 
								   "Need a controller with enabled editing to accept paste options", -1 ) );
			result = TCL_ERROR;
			goto error;
		    }
		    tmpMovie = NewMovieFromScrap(0);
		    if (tmpMovie != NULL) {
			result = LogUndoState( movPtr, interp );
			PasteMovieSelection( aMovie, tmpMovie );
			if (noErr != CheckAndSetErrorResult( interp, noErr )) {
    	                    result = TCL_ERROR;
			    goto error;
    	                } else {    
			    DisposeMovie(tmpMovie);
			}
		    } else { 
			CheckAndSetErrorResult( interp, noErr );
			result = TCL_ERROR;
			goto error;
		    }
		}	
		
                /*
                 * Option here: we choose to only change the size if there
                 * was nothing visual before (mwidth/mheight = 0).
                 * Else it gets scaled.
                 */
		
                if ((movPtr->mwidth == 0) || (movPtr->mheight == 0)) {
                    Rect            aRect;
		    
                    GetMovieBox( aMovie, &aRect );
                    movPtr->mwidth = aRect.right - aRect.left;
                    movPtr->mheight = aRect.bottom - aRect.top;
                    sizeMayChange = true;
                }
		if (movPtr->aController != NULL) {
		    MCDoAction( movPtr->aController,  mcActionMovieEdited, NULL );
		    MCMovieChanged( movPtr->aController, aMovie );
		    if (sizeMayChange) {
			MoviePlayerWorldChanged( clientData );
		    }
		} else {
		    MoviePlayerWorldChanged( clientData );
		}
	    } else {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
		result = TCL_ERROR;
	    }
	    break;
        }
	    
        case kMovieCmdPicture: {
	    
    	    /* 
    	     * Make a tk image from a time in a movie. 
    	     */
	    
	    if (aMovie ) {
		int 		dstWidth = 0;
		int 		dstHeight = 0;
		char		*charPtr;
		
		if (objc < 4) {
		    Tcl_WrongNumArgs( interp, 2, objv, 
				     "time imageName ?-width width -height height?" );
		    result = TCL_ERROR;
		    goto error;
		}
		
                /*
                 * If not visual then error.
                 */
		
		if (GetMovieIndTrackType( aMovie, 1, VisualMediaCharacteristic, 
					 movieTrackCharacteristic ) == NULL) {
		    Tcl_SetObjResult( interp, Tcl_NewStringObj( 
							       "No visual media characteristics in movie", -1 ) );
		    result = TCL_ERROR;
		    goto error;
		}
		if (Tcl_GetIntFromObj( interp, objv[2], &intValue ) != TCL_OK) {
		    Tcl_AddErrorInfo( interp, "\n	(processing time value)" );
		    result = TCL_ERROR;
		    goto error;
		}
		movTime = intValue;
		thePic = GetMoviePict( aMovie, movTime );
		if (thePic == NULL) {
                    CheckAndSetErrorResult( interp, noErr );
		    result = TCL_ERROR;
		    goto error;
		}
		
		/* Process any -width and -height options. */
		for (iarg = 4; iarg < objc; iarg = iarg + 2) {
		    charPtr = Tcl_GetString( objv[iarg] );  		
		    if (strcmp(charPtr, "-width") == 0) {
			if (Tcl_GetIntFromObj( interp, objv[iarg+1], &dstWidth ) 
			    != TCL_OK) {
			    Tcl_AddErrorInfo( interp, "\n	(processing -width value)" );
			    result = TCL_ERROR;
			    goto error;
			}
		    } else if (strcmp(charPtr, "-height") == 0) {
			if (Tcl_GetIntFromObj( interp, objv[iarg+1], &dstHeight ) 
			    != TCL_OK) {
			    Tcl_AddErrorInfo( interp, "\n	(processing -height value)" );
			    result = TCL_ERROR;
			    goto error;
			}
		    } else {
			Tcl_SetObjResult( interp, Tcl_NewStringObj( 
								   "unrecognized option", -1 ) );
			result = TCL_ERROR;
			goto error;
		    }
		}
		result = ConvertPictureToTkPhoto( interp, thePic, 
						 dstWidth, dstHeight, Tcl_GetString(objv[3]) );
		KillPicture( thePic );
	    } else {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
		result = TCL_ERROR;
	    }
	    break;
        }
	    
        case kMovieCmdPlay: {
	    
    	    /* 
    	     * Play the movie from its present position. 
    	     */
	    
	    if (aMovie ) {
		if (!Tk_IsMapped(movPtr->tkwin) && 
		    (GetMovieIndTrackType( aMovie, 1, VisualMediaCharacteristic, 
					  movieTrackCharacteristic ) != NULL)) {
		    Tcl_SetObjResult( interp, 
				     Tcl_NewStringObj( "Movie not displayed", -1 ) );
		    result = TCL_ERROR;
		} else {
		    if (movPtr->aController != NULL) {
			rate = X2Fix(movPtr->preferredRate);
			MCDoAction( movPtr->aController, mcActionPlay, (void *) rate );
		    } else {
			
			/*
			 * If at end, set movie time to beginning before playing. 
			 */
			
			if (IsMovieDone( aMovie )) {
			    if (GetMoviePreferredRate(aMovie) > 0) {
				SetMovieTimeValue( aMovie, 0 );
			    } else {
				SetMovieTimeValue( aMovie, GetMovieDuration(aMovie) );
			    }
			}
			StartMovie( aMovie );
		    }
		}
	    } else {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
		result = TCL_ERROR;
	    }
	    break;
        }
	    
        case kMovieCmdPlayHints: {
	    if (aMovie != NULL) {
                int 	hintFlag = 0L;
                
                flags = 0L;
                mask = 0L;
		
                for (iarg = 2; iarg < objc; iarg += 2) {                      
		    if (Tcl_GetIndexFromObj( interp, objv[iarg], allMoviePlayHintsOpts, 
					    "playhints option", TCL_EXACT, &optIndex ) != TCL_OK ) {
			result = TCL_ERROR;
			goto error;
		    }			    	
		    if (iarg + 1 == objc) {
			resultObjPtr = Tcl_GetObjResult( interp );
			Tcl_AppendStringsToObj( resultObjPtr, "value for \"",
					       Tcl_GetString(objv[iarg]), "\"missing", 
					       (char *) NULL );
			result = TCL_ERROR;
			goto error;
		    }
                    
                    /*
                     * All values are boolean.
                     */
                    if (Tcl_GetBooleanFromObj( interp, objv[iarg+1], 
					      &boolValue ) != TCL_OK) {
                        Tcl_AddErrorInfo( interp, 
					 "\n	(processing -playhints option)" );
                        result = TCL_ERROR;
                        goto error;
                        break;
                    }
		    
		    switch (optIndex) {
    	    	    	case kMoviePlayHintsOptScrubMode: {
                            hintFlag = hintsScrubMode;
                        }
    	    	    	case kMoviePlayHintsOptUseSoundInterp: {
                            hintFlag = hintsUseSoundInterp;
                        }
    	    	    	case kMoviePlayHintsOptAllowInterlace: {
                            hintFlag = hintsAllowInterlace;
                        }
    	    	    	case kMoviePlayHintsOptAllowBlacklining: {
                            hintFlag = hintsAllowBlacklining;
                        }
    	    	    	case kMoviePlayHintsOptDontPurge: {
                            hintFlag = hintsDontPurge;
                        }
    	    	    	case kMoviePlayHintsOptInactive: {
                            hintFlag = hintsInactive;
                        }
    	    	    	case kMoviePlayHintsOptHighQuality: {
                            hintFlag = hintsHighQuality;
                        }
                    }
                    if (boolValue) {
                        flags |= hintFlag;
                    } else {
                        flags &= ~hintFlag;
                    }
                    mask |= hintFlag;
                }
                SetMoviePlayHints( aMovie, flags, mask );
	    } else {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
		result = TCL_ERROR;
	    }
	    break;
        }
	    
        case kMovieCmdRate: {
	    
    	    /* 
    	     * Set or get the movie play rate.
    	     */
	    
	    if (aMovie != NULL) {
		if (objc == 2) {
		    Tcl_SetObjResult( interp, Tcl_NewDoubleObj( 
							       Fix2X( GetMovieRate(aMovie) ) ) );
		} else if (objc == 3) {
		    if (Tcl_GetDoubleFromObj( interp, objv[2], &theRate ) != TCL_OK) {
			Tcl_AddErrorInfo( interp, 
					 "\n	(processing rate command)" );
		    } else {
			rate = X2Fix(theRate);
			SetMovieRate( aMovie, rate );
		    }
		} else {
		    Tcl_WrongNumArgs( interp, 2, objv, "?rate?" );
		    result = TCL_ERROR;
		    goto error;
		}
		if (movPtr->aController != NULL) {
		    MCMovieChanged( movPtr->aController, aMovie );
		}
	    } else {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
		result = TCL_ERROR;
	    }
	    break;
        }
	    
        case kMovieCmdSave: {
	    
    	    /* 
    	     * Save the movie. If there was a movie resource in the orig movie, save in
    	     * resource fork, else save in the data fork. 
    	     */
	    
	    if (aMovie != NULL) {
		if (objc != 2) {
		    Tcl_WrongNumArgs( interp, 2, objv, NULL );
		    result = TCL_ERROR;
		    goto error;
		}
		if (movPtr->fileNamePtr == NULL) {
		    Tcl_SetObjResult( interp, Tcl_NewStringObj( 
							       "Have no file name. Use \"saveas\" or \"flatten\" instead", -1 ) );	
		    result = TCL_ERROR;
		    goto error;
    	        } 
		
		/*
		 * Translate file name to FSSpec.
		 */
		
		err = QTTclNativePathNameToFSSpec( movPtr->interp, 
						  Tcl_GetString( movPtr->fileNamePtr ), &fss );
		
		if (err == fnfErr) {
		    Tcl_AppendStringsToObj( Tcl_GetObjResult( interp ), 
					   "File not found: ", 
					   Tcl_GetString( movPtr->fileNamePtr ), 
					   (char *) NULL );
		    result = TCL_ERROR;
		    goto error;
		}
		if (err != noErr) {
		    Tcl_SetObjResult( interp, Tcl_NewStringObj( 
							       "Unable to make a FSSpec from file name", -1 ) );
		    result = TCL_ERROR;
		    goto error;
		}
#if 0
extern OSErr 
OpenMovieStorage(
  Handle         dataRef,
  OSType         dataRefType,
  long           flags,
  DataHandler *  outDataHandler)
#endif
		if (noErr != OpenMovieFile( &fss, &movieResFile, fsRdWrPerm )) {
                    CheckAndSetErrorResult( interp, noErr );
		    result = TCL_ERROR;
		    goto error;
		}
		if (movPtr->resourceNumber == -1) {
		    
    	            /* We have probably the movie in the data fork. */
    	            resId = movieInDataForkResID;
		} else {
		    
		    /* Save in the resource fork. */
		    resId = movPtr->resourceNumber;
		}
		
		if (noErr != UpdateMovieResource( aMovie, movieResFile, resId, NULL )) {
                    CheckAndSetErrorResult( interp, noErr );
		    result = TCL_ERROR;
		}
#if 0
extern OSErr 
CloseMovieStorage(DataHandler dh)
#endif
		CloseMovieFile(movieResFile);
		
	    } else {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
		result = TCL_ERROR;
	    }
	    break;
        }
	    
        case kMovieCmdSaveas: {
	    
    	    /* 
    	     * Save the movie in a new file. 
    	     */
	    
	    if (aMovie != NULL) {
		Handle	    dataRef = NULL;
		OSType	    dataRefType;
		DataHandler dataHandler = 0;

		if (objc != 3) {
		    Tcl_WrongNumArgs( interp, 2, objv, "fileName" );
		    result = TCL_ERROR;
		    goto error;
		}

		result = QTTclNewDataRefFromUTF8Obj(interp, objv[2], &dataRef, &dataRefType);
		if (result != TCL_OK) {
		    goto error;
		}
		flags = createMovieFileDeleteCurFile | createMovieFileDontCreateResFile |
			createMovieFileDontCreateMovie;
		err = CreateMovieStorage(dataRef, dataRefType, ksigMoviePlayer,
			smCurrentScript, flags, &dataHandler, NULL);		   
		if (noErr != err) {
                    CheckAndSetErrorResult( interp, noErr );
		    result = TCL_ERROR;
		    goto error;
		}
		resId = movieInDataForkResID;
		if (noErr != AddMovieToStorage(aMovie, dataHandler)) {
		    if (dataHandler) {
			err = CloseMovieStorage(dataHandler);
		    }
		    if (dataRef) {
			DisposeHandle(dataRef);
		    }
		    CheckAndSetErrorResult( interp, noErr );
		    result = TCL_ERROR;
		    goto error;
		}
		movPtr->resourceNumber = resId;
		
		if (movPtr->fileNamePtr != NULL) {
		    Tcl_DecrRefCount( movPtr->fileNamePtr );
		}
		movPtr->fileNamePtr = objv[2];
		Tcl_IncrRefCount( movPtr->fileNamePtr );
		movPtr->filename = Tcl_GetString( objv[2] );
		if (dataHandler) {
		    err = CloseMovieStorage(dataHandler);
		}
		ClearMovieChanged( aMovie );
		if (dataRef) {
		    DisposeHandle(dataRef);
		}
	    } else {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
		result = TCL_ERROR;
	    }
	    break;
        }
	    
        case kMovieCmdSelect: {
	    
    	    /* 
    	     * Select a section of movie. A movie selection shall invalidate any track selection.
    	     */
	    
	    if (aMovie != NULL) {
		if (objc == 2) {
		    
            	    /*
            	     * Get the movie selection.
            	     */
		    
		    if (aMovie != NULL) {
			GetMovieSelection( aMovie, &movTime, &movDuration );
			if (noErr != CheckAndSetErrorResult( interp, noErr )) {
                            result = TCL_ERROR;
                            goto error;
                        }     
			listObjPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );
			Tcl_ListObjAppendElement( interp, listObjPtr, 
						 Tcl_NewLongObj(movTime) );				
			Tcl_ListObjAppendElement( interp, listObjPtr, 
						 Tcl_NewLongObj(movDuration) );				
                        Tcl_SetObjResult( interp, listObjPtr );
		    } else {
			Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
			result = TCL_ERROR;
		    }
		    
		} else if (objc == 3) {
                    char *strObjv2 = Tcl_GetString(objv[2]);
                    
		    if (strcmp(strObjv2, "clear") == 0) {
			result = LogUndoState( movPtr, interp );
			ClearMovieSelection( aMovie );
			if (movPtr->aController != NULL) {
			    MCDoAction( movPtr->aController,  mcActionMovieEdited, NULL );
			    MCMovieChanged( movPtr->aController, aMovie );
			} else {
			    MoviePlayerWorldChanged( clientData );
			}
		    } else if (strcmp(strObjv2, "all") == 0) {
			SetMovieSelection( aMovie, 0, GetMovieDuration( aMovie ) );
			if (noErr != CheckAndSetErrorResult( interp, noErr )) {
                            result = TCL_ERROR;
                            goto error;
                        }     
    	                movPtr->trackSelect->trackID = 0;
		    } else if (strcmp(strObjv2, "end") == 0) {
			SetMovieSelection( aMovie, GetMovieTime( aMovie, NULL ), 0 );
			if (noErr != CheckAndSetErrorResult( interp, noErr )) {
                            result = TCL_ERROR;
                            goto error;
                        }     
    	                movPtr->trackSelect->trackID = 0;
		    } else if (strcmp(strObjv2, "none") == 0) {
			SetMovieSelection( aMovie, GetMovieTime( aMovie, NULL ), 0 );
			if (noErr != CheckAndSetErrorResult( interp, noErr )) {
                            result = TCL_ERROR;
                            goto error;
                        }     
    	                movPtr->trackSelect->trackID = 0;
		    } else {    
                        if (GetMovieStartTimeFromObj( interp, aMovie, objv[2], &longValue ) != TCL_OK) {
                            goto error;
                        } 
                        movTime = longValue;
			movDuration = 0;
			SetMovieSelection( aMovie, movTime, movDuration );
			if (noErr != CheckAndSetErrorResult( interp, noErr )) {
                            result = TCL_ERROR;
                            goto error;
                        }     
    	                movPtr->trackSelect->trackID = 0;
                    }
		} else if (objc == 4) {
                    if (GetMovieStartTimeFromObj( interp, aMovie, objv[2], &longValue ) != TCL_OK) {
                        goto error;
                    } 
                    movTime = longValue;
                    if (GetMovieDurationFromObj( interp, aMovie, objv[3], movTime, &longValue ) != TCL_OK) {
                        goto error;
                    } 
                    movDuration = longValue;
		    SetMovieSelection( aMovie, movTime, movDuration );
		    if (noErr != CheckAndSetErrorResult( interp, noErr )) {
                        result = TCL_ERROR;
                        goto error;
                    }     
    	            movPtr->trackSelect->trackID = 0;
		} else {
		    Tcl_WrongNumArgs( interp, 2, objv, "?startTime? ?duration?" );
		    result = TCL_ERROR;
                    goto error;
		}
		if (movPtr->aController != NULL) {
		    MCMovieChanged( movPtr->aController, aMovie );
		}
	    } else {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
		result = TCL_ERROR;
	    }
	    break;
        }
	    
        case kMovieCmdSetpreferredrate: {
	    
    	    /* 
    	     * Set the movie's preferred play rate. Now obsolete!
    	     */
	    
	    if (aMovie != NULL) {
		if (objc == 3) {
		    if (Tcl_GetDoubleFromObj( interp, objv[2], &theRate ) != TCL_OK) {
			Tcl_AddErrorInfo( interp, 
					 "\n	(processing rate command)" );
		    } else {
			rate = X2Fix(theRate);
			movPtr->preferredRate = theRate;
			if (movPtr->aController != NULL) {
			    
			    /* For a controller, the movie's play rate is set from the play action. */
			    
			    SetMoviePreferredRate( aMovie, rate );                    
			} else {
			    SetMoviePreferredRate( aMovie, rate );
			}
		    }
		} else {
		    Tcl_WrongNumArgs( interp, 2, objv, "rate");
		    result = TCL_ERROR;
                    goto error;
		}
		if (movPtr->aController != NULL) {
		    MCMovieChanged( movPtr->aController, aMovie );
		}
	    } else {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
		result = TCL_ERROR;
	    }
	    break;
        }
	    
        case kMovieCmdSetrate: {
	    
    	    /* 
    	     * Set the movie play rate. Now obsolete!
    	     */
	    
	    if (aMovie != NULL) {
		if (objc == 3) {
		    if (Tcl_GetDoubleFromObj( interp, objv[2], &theRate ) != TCL_OK) {
			Tcl_AddErrorInfo( interp, 
					 "\n	(processing rate command)" );
		    } else {
			rate = X2Fix(theRate);
			SetMovieRate( aMovie, rate );
		    }
		} else {
		    Tcl_WrongNumArgs( interp, 2, objv, "rate");
		    result = TCL_ERROR;
                    goto error;
		}
		if (movPtr->aController != NULL) {
		    MCMovieChanged( movPtr->aController, aMovie );
		}
	    } else {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
		result = TCL_ERROR;
	    }
	    break;
        }
	    
        case kMovieCmdSettime: {
	    
    	    /* 
    	     * Set the movie time. Now obsolete, use "time".
    	     */
	    
	    if (objc != 3) {
		Tcl_WrongNumArgs( interp, 2, objv, "time" );
		result = TCL_ERROR;
	    } else {
		if (aMovie != NULL) {
		    if (Tcl_GetIntFromObj( interp, objv[2], &intValue ) != TCL_OK) {
			Tcl_AddErrorInfo( interp, "\n	(processing time value)" );
			result = TCL_ERROR;
			goto error;
		    }
		    movTime = intValue;
		    SetMovieTimeValue( aMovie, movTime );
		    if (noErr != CheckAndSetErrorResult( interp, noErr )) {
                        result = TCL_ERROR;
                        goto error;
                    }     
		    if (movPtr->aController != NULL) {
			MCMovieChanged(movPtr->aController, aMovie);
		    }
		} else {
		    Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
		    result = TCL_ERROR;
		}
	    }
	    break;
        }
	    
        case kMovieCmdSize: {
	    
    	    /* 
    	     * Return the movies "natural" size excluding any movie controller. 
    	     */
	    
	    if (aMovie != NULL) {
		listObjPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );
		Tcl_ListObjAppendElement( interp, listObjPtr, 
					 Tcl_NewLongObj(movPtr->mwidth) );				
		Tcl_ListObjAppendElement( interp, listObjPtr, 
					 Tcl_NewLongObj(movPtr->mheight) );				
		Tcl_SetObjResult( interp, listObjPtr );
	    } else {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
		result = TCL_ERROR;
	    }
	    break;
        }
	    
        case kMovieCmdUndocumeted1: {
	    
    	    /* 
    	     * Sprite Subcommand (not implemented).
    	     */
	    
	    if (aMovie != NULL) {
		ProcessSpriteSubcmd(clientData, interp, objc-2, objv+2);
	    } else {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
		result = TCL_ERROR;
	    }
	    break;
        }
	    
        case kMovieCmdStep: {
	    
	    /* 
	     * Step through the movie. 
	     */
	    
            long 	numSteps = 1;
            if (objc > 2) {
		if (Tcl_GetIntFromObj( interp, objv[2], &intValue ) != TCL_OK) {
		    Tcl_AddErrorInfo( interp, "\n	(processing numSteps value)" );
		    result = TCL_ERROR;
		    goto error;
		}
		numSteps = intValue;
            }
            if (aMovie != NULL) {
                if (movPtr->aController != NULL) {
                    MCDoAction( movPtr->aController, mcActionStep, 
			       (void*) numSteps );
		    MCMovieChanged( movPtr->aController, aMovie );
                } else {
                    long 		i;
                    TimeValue 	intTime;
                    TimeValue 	intDur;
                    TimeValue 	curTime;
                    long 		num = numSteps;
                    
                    if (num < 0) {
                        num = 0 - num;
                    }
                    for (i = 0; i < num; i++) {
                        curTime = GetMovieTime( aMovie, NULL ); 
                        GetMovieNextInterestingTime( aMovie, nextTimeStep,
						    0, 0, curTime, numSteps, &intTime, &intDur );
                        SetMovieTimeValue( aMovie, intTime );
                    }
                }
            } else {
                Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
                result = TCL_ERROR;
            }
	    break;
        }
	    
        case kMovieCmdStop: {
	    
    	    /* 
    	     * Stop the movie from playing. 
    	     */
	    
	    if (aMovie != NULL) {
		if (movPtr->aController != NULL) {
		    MCDoAction( movPtr->aController, mcActionPlay, 0 );
		    MCDoAction( movPtr->aController, mcActionSetLooping, (void *) false );
		} else {
		    StopMovie(aMovie);
		}
	    } else {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
		result = TCL_ERROR;
	    }
	    break;
        }
	    
        case kMovieCmdTilt: {
	    
    	    /* 
    	     * Set or get tilt angle of QTVR pano. 
    	     */
	    
	    if (objc > 3) {
		Tcl_WrongNumArgs( interp, 2, objv, "?angle?" );
		result = TCL_ERROR;
		goto error;
	    }
	    if (!gHasQTVRManager) {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( 
							   "Couldn't find the QTVR manager on this system", -1 ) );
		result = TCL_ERROR;
		goto error;
	    }
	    if (aMovie != NULL) {
		double   angle;
		
		if (movPtr->qtvrInstance == NULL) {
		    Tcl_SetObjResult( interp, Tcl_NewStringObj( 
							       "Couldn't identify this movie as a VR movie", -1 ) );
		    result = TCL_ERROR;
		    goto error;
		}
		if (objc == 3) {
		    if (Tcl_GetDoubleFromObj( interp, objv[2], &angle ) != TCL_OK) {
			Tcl_AddErrorInfo( interp, 
					 "\n	(processing tilt command)" );
		    } else {
			SetPanoramaByDegrees( movPtr->qtvrInstance, kDirUp, (float) angle );
		    }
                } else {
                    angle = QTVRGetTiltAngle( movPtr->qtvrInstance );
		    Tcl_SetObjResult( interp, Tcl_NewDoubleObj( angle ) );
                }
	    } else {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
		result = TCL_ERROR;
            }
	    break;
        }
	    
        case kMovieCmdTime: {
	    
    	    /* 
    	     * Set or get the movie time. 
    	     */
	    
            if (objc == 2) {
		if (aMovie != NULL) {
		    Tcl_SetObjResult( interp, Tcl_NewLongObj( 
							     GetMovieTime( aMovie, NULL ) ) );
		} else {
		    Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
		    result = TCL_ERROR;
		}
            } else if (objc == 3) {       
		if (aMovie != NULL) {
		    if (Tcl_GetIntFromObj( interp, objv[2], &intValue ) != TCL_OK) {
			Tcl_AddErrorInfo( interp, "\n	(processing time value)" );
			result = TCL_ERROR;
			goto error;
		    }
		    movTime = intValue;
		    SetMovieTimeValue( aMovie, movTime );
		    if (noErr != CheckAndSetErrorResult( interp, noErr )) {
                        result = TCL_ERROR;
                        goto error;
                    }     
		    if (movPtr->aController != NULL) {
			MCMovieChanged( movPtr->aController, aMovie );
                    }
		} else {
		    Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
		    result = TCL_ERROR;
		}
	    } else {
		Tcl_WrongNumArgs( interp, 2, objv, "?time?" );
		result = TCL_ERROR;
	    }
	    break;
        }
	    
        case kMovieCmdTimeCode: {
	    
    	    /* 
    	     * The tracks sub command. 
    	     */
	    
	    if (aMovie != NULL) {
		if (TCL_OK != TimeCodeCmd( interp, movPtr, objc - 2, objv + 2 )) {
		    result = TCL_ERROR;
                }    
	    } else {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
		result = TCL_ERROR;
	    }
	    break;
        }
	    
        case kMovieCmdTracks: {
	    
    	    /* 
    	     * The tracks sub command. 
    	     */
	    
	    if (aMovie != NULL) {
		if (TCL_OK != ProcessTracksObjCmd( clientData, interp, objc - 2, objv + 2 )) {
		    result = TCL_ERROR;
                }    
	    } else {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
		result = TCL_ERROR;
	    }
	    break;
        }
	    
        case kMovieCmdUndo: {
	    
    	    /* 
    	     * Either checkpoint or undo a change. 
    	     */
	    
	    if (aMovie != NULL) {
		if (objc == 3) {
		    if (strcmp(Tcl_GetString(objv[2]), "set") == 0) {
			result = LogUndoState( movPtr, interp );
		    } else {
			if (Tcl_GetIntFromObj( interp, objv[2], &intValue ) 
			    != TCL_OK) {
			    Tcl_AddErrorInfo( interp, 
					     "\n	(processing undoLevel value)" );
			    result = TCL_ERROR;
			    goto error;
			}
			undoLevel = intValue;
			if (undoLevel < 0 || undoLevel >= movPtr->undoCount) {
			    Tcl_SetObjResult( interp, Tcl_NewStringObj( 
								       "Invalid Undo Level", -1 ) );
			    result = TCL_ERROR;
			    goto error;
			}
			err = UseMovieEditState( aMovie, movPtr->editStates[undoLevel] );
			if (err != noErr) {
                            CheckAndSetErrorResult( interp, err );
			    result = TCL_ERROR;
			    goto error;
			}
			for (i = undoLevel; i < movPtr->undoCount; i++) {
			    DisposeMovieEditState( movPtr->editStates[i] );
			}
			movPtr->undoCount = undoLevel;
			if (movPtr->aController != NULL) {
			    MCDoAction( movPtr->aController,  mcActionMovieEdited, NULL );
			    MCMovieChanged( movPtr->aController, aMovie );
			}	
		    }
                } else if (objc == 2) {
                    Tcl_SetObjResult( interp, Tcl_NewIntObj( movPtr->undoCount ) );
		} else {
		    Tcl_WrongNumArgs( interp, 2, objv, "set | level" );
		    result = TCL_ERROR;
		}
	    } else {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
		result = TCL_ERROR;
	    }
	    break;
        }
	    
        case kMovieCmdUserData: {
	    
    	    /* 
    	     * The userdata command. 
    	     */
	    
	    if (aMovie != NULL) {
		UserData 		userData = 0;
		
		userData = GetMovieUserData( aMovie );
		if (userData == NULL) {
		    CheckAndSetErrorResult( interp, noErr );
		    result = TCL_ERROR;
		    goto error;
		}
		if (objc == 2) {
		    if (TCL_OK != GetUserDataListCmd( userData, interp )) {
			result = TCL_ERROR;
		    }
		} else if ((objc > 2) && (objc % 2 == 0)) { 
		    if (TCL_OK != SetUserDataListCmd( userData, interp, objc - 2, objv + 2 )) {
			result = TCL_ERROR;
		    }
                } else {
		    Tcl_WrongNumArgs( interp, 2, objv, "?options?" );
		    result = TCL_ERROR;
                }
	    } else {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
		result = TCL_ERROR;
	    }        
	    break;
        }
	    
        case kMovieCmdUndocumeted2: {
	    
    	    /* 
    	     * Vector Subcommand. 
    	     */
	    
	    if (aMovie != NULL) {
		ProcessVectorSubcmd(clientData, interp, objc-2, objv+2);
	    } else {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
		result = TCL_ERROR;
	    }
        }
    }
    
error:
    Tcl_Release((ClientData) movPtr);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * ComressMovieCmd --
 *
 *	Compress the movie.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then the interp's result contains an error message.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
ComressMovieCmd(Tcl_Interp *interp, 
	int objc, Tcl_Obj *CONST objv[], 
	MoviePlayer *movPtr)
{
    Handle		dataRef = NULL;
    OSType		dataRefType;
    ComponentInstance	videoCodec;	
    Boolean		canceled;
    OSErr		err = noErr;
    int			result = TCL_OK;
        
    /*
     * Translate file name to Data Reference.
     */
    result = QTTclNewDataRefFromUTF8Obj(interp, objv[2],
	    &dataRef, &dataRefType);
    if (result != TCL_OK) {
	goto error;
    }
    videoCodec = NULL;
    if (objc == 4) {
	
	/* Have a codec to work with. We do not need 'EndianU32_NtoB'! */
	
	videoCodec = OpenDefaultComponent( MovieExportType,
					  MovieFileType );
	if (videoCodec == NULL) {
	    Tcl_SetObjResult( interp, 
			     Tcl_NewStringObj( "Codec not found", -1 ) );
	    result = TCL_ERROR;
	    goto error;
	} 
	MovieExportDoUserDialog( videoCodec, movPtr->aMovie, NULL, 
				0, GetMovieDuration(movPtr->aMovie), &canceled );
	if (canceled) {
	    
	    /* This is actually not an error, just to clean up things. */
	    goto error;
	}
    }
    err = ConvertMovieToDataRef(movPtr->aMovie, 0, dataRef, dataRefType,
				MovieFileType, ksigMoviePlayer, 0, videoCodec);
    if (noErr != err) {
	CheckAndSetErrorResult( interp, err );
	result = TCL_ERROR;
	goto error;
    }
    
error:
    if (dataRef) {
	DisposeHandle(dataRef);
    }
    if (videoCodec != NULL) {
	CloseComponent( videoCodec );
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * FlattenMovieCmd --
 *
 *	Flatten the movie into the datafork of a new file.
 *	A separate mechanism is used for QTVR panos.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then the interp's result contains an error message.
 *
 * Side effects:
 *	New movie may be assiciated with movie widget.
 *
 *----------------------------------------------------------------------
 */

static int
FlattenMovieCmd(Tcl_Interp *interp, 
	int objc, Tcl_Obj *CONST objv[], 
	MoviePlayer *movPtr)
{
    int			iarg;
    int			optIndex;
    int			boolValue;
    int			showDialog;
    long		flags;
    Tcl_Obj		*obj;
    Handle		dataRef = NULL;
    OSType		dataRefType;
    ComponentDescription    desc;
    MovieExportComponent    exporter = NULL;
    OSErr		err = noErr;
    int			result = TCL_OK;
        
    /* 
     * Set default sub options for flatten. 
     */
    
    flags = flattenAddMovieToDataFork | flattenForceMovieResourceBeforeMovieData;
    showDialog = false;
    
    for (iarg = 3; iarg < objc; iarg += 2) {                      
	if (Tcl_GetIndexFromObj( interp, objv[iarg], allMovieFlattenOpts, 
				"flatten option", TCL_EXACT, &optIndex ) != TCL_OK ) {
	    result = TCL_ERROR;
	    goto error;
	}			    	
	if (iarg + 1 == objc) {
	    Tcl_AppendStringsToObj( Tcl_GetObjResult(interp), 
		    "value for \"", Tcl_GetString(objv[iarg]), "\"missing", 
		    (char *) NULL );
	    result = TCL_ERROR;
	    goto error;
	}
	
	switch (optIndex) {
		
	    case kMovieFlattenOptDialog: {
		if (Tcl_GetBooleanFromObj( interp, objv[iarg+1], 
					  &showDialog ) != TCL_OK) {
		    Tcl_AddErrorInfo( interp, 
				     "\n	(processing -dialog option)" );
		    result = TCL_ERROR;
		    goto error;
		}
		if (showDialog) {
		    flags |= showUserSettingsDialog;
		}
		break;
	    }
		
	    case kMovieFlattenOptDontInterleave: {
		if (Tcl_GetBooleanFromObj( interp, objv[iarg+1], 
					  &boolValue ) != TCL_OK) {
		    Tcl_AddErrorInfo( interp, 
				     "\n	(processing -dontinterleave option)" );
		    result = TCL_ERROR;
		    goto error;
		}
		if (boolValue) {
		    flags |= flattenDontInterleaveFlatten;
		}
		break;
	    }
		
	    case kMovieFlattenOptForceResourceBeforeData: {
		if (Tcl_GetBooleanFromObj( interp, objv[iarg+1], 
					  &boolValue ) != TCL_OK) {
		    Tcl_AddErrorInfo( interp, 
				     "\n	(processing -forceresourcebeforedata option)" );
		    result = TCL_ERROR;
		    goto error;
		}
		if (!boolValue) {
		    flags &= ~flattenForceMovieResourceBeforeMovieData;
		}
		break;
	    }		
	}
    }

    /*
     * Translate file name to Data Reference.
     */
    result = QTTclNewDataRefFromUTF8Obj(interp, objv[2],
	    &dataRef, &dataRefType);
    if (result != TCL_OK) {
	goto error;
    }
    if (showDialog || movPtr->qtvrInstance != NULL) {	
	flags = createMovieFileDeleteCurFile | movieToFileOnlyExport | movieFileSpecValid;
	if (showDialog) {
	    flags |= showUserSettingsDialog;
	}
	
	if (movPtr->qtvrInstance != NULL) {
	    desc.componentType = MovieExportType;
	    desc.componentSubType = MovieFileType;
	    desc.componentFlags = 0;
	    desc.componentFlagsMask = 0;
	    /* 'vrwe' */
	    desc.componentManufacturer = kQTVRFlattenerManufacturer; 
	    exporter = OpenComponent( FindNextComponent( NULL, &desc ) );
	    if (exporter == NULL) {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( 
			"Failed finding a QTVR flattener component", -1 ) );
		result = TCL_ERROR;
		goto error;
	    }
	}
		
	// export the movie into a file
	// @@@ This used to return the new FSSpec but doesn't!
	err = ConvertMovieToDataRef(movPtr->aMovie, /* the movie */
				NULL,		    /* all tracks */
				dataRef,	    /* the output data reference */
				dataRefType,	    /* the data ref type */
				MovieFileType,	    /* output file type */
				ksigMoviePlayer,    /* output file creator */
				flags,
				exporter);          /* export component */
	if (err == userCanceledErr) {
	    goto error;
	} else if (err != noErr) {
	    CheckAndSetErrorResult( interp, err );
	    result = TCL_ERROR;
	    goto error;
	}
	obj = Tcl_NewObj();
	result = QTTclNewUTF8ObjFromDataRef(interp, dataRef, dataRefType, &obj);
	if (result != TCL_OK) {
	    goto error;
	}
	Tcl_SetObjResult(interp, obj);	
    } else {
        Movie newMovie = FlattenMovieDataToDataRef(movPtr->aMovie, flags,
		dataRef, dataRefType, ksigMoviePlayer,
		smSystemScript, createMovieFileDeleteCurFile);
        if (NULL != newMovie) {
            // we choose not to do anything with the returned movie
            DisposeMovie(newMovie);
        } else {
	    Tcl_SetObjResult( interp, 
		    Tcl_NewStringObj("Failed FlattenMovieDataToDataRef", -1 ) );
	    result = TCL_ERROR;
	    goto error;
        }
	obj = Tcl_NewObj();
	result = QTTclNewUTF8ObjFromDataRef(interp, dataRef, dataRefType, &obj);
	if (result != TCL_OK) {
	    goto error;
	}
	Tcl_SetObjResult(interp, obj);
    }
    
error:
    if (exporter) {
	CloseComponent(exporter);
    }
    if (dataRef) {
	DisposeHandle(dataRef);
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * NewMovieCmd --
 *
 *	Make new movie.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then the interp's result contains an error message.
 *
 * Side effects:
 *	New movie may be assiciated with movie widget.
 *
 *----------------------------------------------------------------------
 */

static int
NewMovieCmd(Tcl_Interp *interp, 
	int objc, Tcl_Obj *CONST objv[], 
	MoviePlayer *movPtr)
{
    int		i;
    long	flags;
    OSType	dataRefType;
    Handle	dataRef = NULL;
    DataHandler dataHandler = 0;
    Movie	aMovie = movPtr->aMovie;
    OSErr	err = noErr;
    int		result = TCL_OK;
    
    /* get rid of the old one */
    RemoveMovieFromOpenMovies( movPtr );
    
    if (aMovie != NULL) {
	for (i = 0; i < movPtr->undoCount; i++) {
	    DisposeMovieEditState(movPtr->editStates[i]);
	}
	movPtr->undoCount = 0;
	DisposeMovie( aMovie );
	aMovie = NULL;
	gMovieRefCount--;
    }
    
    /*
     * Translate file name to Data Reference.
     */
    result = QTTclNewDataRefFromUTF8Obj(movPtr->interp, objv[2], &dataRef, &dataRefType);
    if (result != TCL_OK) {
	goto error;
    }
    flags = createMovieFileDeleteCurFile | createMovieFileDontCreateResFile;
    
    /* Create a file. */
    err = CreateMovieStorage(dataRef, dataRefType, ksigMoviePlayer, 
			     smCurrentScript, flags, &dataHandler, &(movPtr->aMovie));	
    if (err != noErr) {
	CheckAndSetErrorResult( interp, err );
	movPtr->aMovie = NULL;
	result = TCL_ERROR;
	goto error;
    }
    aMovie = movPtr->aMovie;
    gMovieRefCount++;
        
    /* Add the movie. */
    err = AddMovieToStorage(aMovie, dataHandler);
    if (err != noErr) {
	DataHDeleteFile(dataHandler);
	CheckAndSetErrorResult( interp, err );
	movPtr->aMovie = NULL;
	result = TCL_ERROR;
	goto error;
    }
    movPtr->resourceNumber = -1;
    err = CloseMovieStorage(dataHandler);
    
    if (movPtr->fileNamePtr != NULL) {
	Tcl_DecrRefCount( movPtr->fileNamePtr );
    }
    movPtr->fileNamePtr = objv[2];
    Tcl_IncrRefCount( movPtr->fileNamePtr );
    movPtr->filename = Tcl_GetString( objv[2] );
    
    Tcl_SetObjResult( interp, Tcl_NewIntObj( movPtr->resourceNumber ) );
    movPtr->flags |= NEWGWORLD;
    
error:
    /* Close the storage. */
    if (dataHandler) {
	CloseMovieStorage(dataHandler);
    }
    if (dataRef) {
	DisposeHandle(dataRef);	    
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureMoviePlayer --
 *
 *		This procedure is called to process an objv/objc list, plus
 *		the Tk option database, in order to configure (or
 *		reconfigure) a movie player widget. Many options are set via a
 *      movie controller so therefore it must be created just after finsished
 *      reading the movie.
 *
 * Results:
 *		The return value is a standard Tcl result.  If TCL_ERROR is
 *		returned, then the interp's result contains an error message.
 *
 * Side effects:
 *		Configuration information, such as text string, colors, font,
 *		etc. get set for movPtr;  old resources get freed, if there
 *		were any. 
 *      Only if the configuration means that a (re)display is necessary,
 *      the NEWGWORLD flag is set. This triggers a call to 'MoviePlayerWorldChanged'.
 *
 *----------------------------------------------------------------------
 */

static int 
ConfigureMoviePlayer( Tcl_Interp *interp, 
		     MoviePlayer *movPtr, 
		     int objc, 
		     Tcl_Obj *CONST objv[] )
{
    int         		ierror;
    int         		i;
    int         		result = TCL_OK;
    int 				mask = 0L;
    long        		optFlags;
    Tcl_Obj 			*errorResult = NULL;
    Tk_SavedOptions 	savedOptions;
    OSErr       		err = noErr;
    
    QTTclDebugPrintf( interp, 2, "ConfigureMoviePlayer" );
    
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
	    
	    if (Tk_SetOptions( interp, (char *) movPtr, movPtr->optionTable, objc,
			      objv, movPtr->tkwin, &savedOptions, &mask) != TCL_OK ) {
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
	 * Check possible inconsistencies of the options. Return error if found any.
	 * 
	 * Don't allow both file and url, at most one of them. 
	 * Tk_DestroyWindow and DestroyMovie is made in calling procedure. 
	 */
	
	if (movPtr->fileNamePtr != NULL && movPtr->url != NULL) {
	    Tcl_SetObjResult( interp, Tcl_NewStringObj( 
						       "Does not accept both -file and -url", -1 ) );
	    Tk_RestoreSavedOptions( &savedOptions );
	    return TCL_ERROR;
	}
	
	/* Url only with a controller. */
	
	if (movPtr->url != NULL && !movPtr->wantController) {
	    Tcl_SetObjResult( interp, Tcl_NewStringObj( 
						       "Need a controller for -url", -1 ) );
	    Tk_RestoreSavedOptions( &savedOptions );
	    return TCL_ERROR;
	}
	
	/*
	 * We cannot configure a movie that is async loading and that is not yet playable.
	 * It is configured when playable.
	 */
	
	if ((movPtr->url != NULL) && (movPtr->state & kQTTclMovieStateAsyncLoading) && 
	    (movPtr->loadState < kMovieLoadStatePlayable)) {
	    Tk_RestoreSavedOptions( &savedOptions );
	    return TCL_ERROR;
	} 
        
        Tk_SetInternalBorder( movPtr->tkwin, movPtr->highlightWidth );
        if (movPtr->highlightWidth <= 0) {
            movPtr->highlightWidth = 0;
        }
        movPtr->inset = movPtr->padx + movPtr->highlightWidth;
	
	/*
	 * Handle the -file option. Was there a new filename? If -file {}  ???
	 * Handle the -url option. Was there a new url?
	 */
	
	if ((mask & MOV_CONF_FILE) || (mask & MOV_CONF_URL)) {
	    
	    /* Get rid of the old one in the global display list. */
	    RemoveMovieFromOpenMovies( movPtr );
	    
	    /* 
	     * If there is a controller, remove since it may need a different one. 
	     */
	    
	    /* Old edit states not valid anymore. */
	    if (movPtr->aMovie != NULL) {
		for (i = 0; i < movPtr->undoCount; i++) {
		    DisposeMovieEditState( movPtr->editStates[i] );
		}
		movPtr->undoCount = 0;
	    }
	    
	    /*
	     * We may have end up here during a 'configure -file newFile' call from the
	     * '-mccommand' tcl proc, which is called from inside 'MCIsPlayerEvent'.
	     * When we return to 'MCIsPlayerEvent' suddenly the movie doesn't exist
	     * anymore, and the thing crashes. Wait therefore to dispose off the movie.
	     */
	    
	    if (movPtr->insideMCCommand) {
		movPtr->tmpMovieRecordPtr = (TmpMovieRecord *) ckalloc( sizeof(TmpMovieRecord) );
		movPtr->tmpMovieRecordPtr->movie = movPtr->aMovie;
		movPtr->tmpMovieRecordPtr->movieController = movPtr->aController;
		SetMovieGWorld( movPtr->aMovie, gOffscreenGWorldPtr, NULL );
		movPtr->aMovie = NULL;
		movPtr->aController = NULL;
	    } else {
		if (movPtr->aController != NULL) {
		    DisposeMovieController( movPtr->aController );
		    movPtr->aController = NULL;
		}
		if (movPtr->aMovie != NULL) {
		    DisposeMovie( movPtr->aMovie ); 
		    movPtr->aMovie = NULL;
                    gMovieRefCount--;
		}
	    }
	    
	    /*
	     * In case we ended up here due to a 'configure -file/-url' command, 
	     * the new movie's GWorld is not valid anymore, and the movie 
	     * should NOT be put on display until we called 'SetMovieGWorld'.
	     */
	    
	    movPtr->grafPtr = NULL;
	    movPtr->state = 0;
	    
	    /*
	     * Retrieve the actual movie from the specified file or url. 
	     */
	    
	    if (movPtr->fileNamePtr != NULL) {
		if ((result = GetMovie( movPtr )) != TCL_OK) {
		    continue;
		}
	    } else if (movPtr->url != NULL) {
		if ((result = GetMovieFromUrl( movPtr )) != TCL_OK) {
		    continue;
		}
	    }
	    
	    /*
	     * Whenever we open a new movie put it offscreen unitil we 
	     * display it.
	     */
	    
	    SetMovieGWorld( movPtr->aMovie, gOffscreenGWorldPtr, NULL );
	    movPtr->flags |= NEWGWORLD;
	}		    
	
	/*
	 * Set no progress procedure, QuickTime default, or a tcl procedure.
	 */
	
     	if (mask & MOV_CONF_PROGRESS_PROC) {
	    if (movPtr->progressProc != NULL) {
#if TARGET_API_MAC_CARBON
		SetMovieProgressProc( movPtr->aMovie, NewMovieProgressUPP(MovieProgressFunction), 
				     (long) movPtr );
#else
		SetMovieProgressProc( movPtr->aMovie, NewMovieProgressProc(MovieProgressFunction), 
				     (long) movPtr );
#endif			
	    } else if (movPtr->qtprogress) {
		SetMovieProgressProc( movPtr->aMovie, (MovieProgressUPP) -1, 0 ); 
	    } else {
		SetMovieProgressProc( movPtr->aMovie, NULL, 0 ); 
	    }   
	}
	
	/* 
	 * Attach any controller. Many of the configure options need a controller to work,
	 * and therefore we cannot wait to make the controller until it is displayed.
	 * If we write 'movie .m', the controller is not added here, but later in 
	 * 'MoviePlayerWorldChanged,.
	 */
	
	AddOrRemoveMovieController( movPtr );
	
	/*
	 * Do we need to set the controllers edit state? Be sure to also set
	 * the controller callback function to make movie and track selection
	 * mutually exclusive.
	 */
	
	if ((movPtr->aController != NULL) && (mask & MOV_CONF_MCEDIT)) {
	    if (movPtr->mcEdit) {
		MCEnableEditing( movPtr->aController, true );
#if TARGET_API_MAC_CARBON
		MCSetActionFilterWithRefCon( movPtr->aController, 
					    NewMCActionFilterWithRefConUPP( MovieControllerCallbackFunction ),
					    (long) movPtr );
#else
		MCSetActionFilterWithRefCon( movPtr->aController, 
					    NewMCActionFilterWithRefConProc( MovieControllerCallbackFunction ),
					    (long) movPtr );
#endif 
	    } else {
		MCEnableEditing( movPtr->aController, false );
	    }
	    MCMovieChanged( movPtr->aController, movPtr->aMovie );
	}
	
	/* 
	 * Set the loopstates if any. Set only if different from old state.
	 */
	
	if (mask & MOV_CONF_LOOPSTATE) {
	    if (movPtr->aMovie != NULL) {
		if (movPtr->aController != NULL) {
		    MCDoAction( movPtr->aController, mcActionSetLooping, 
			       (void *) movPtr->loopstate );
		    MCDoAction( movPtr->aController, mcActionSetLoopIsPalindrome, 
			       (void *) false );
		} else {
		    if (movPtr->loopstate) {
			SetMovieLoopState( movPtr->aMovie, kQTTclNormalLooping );
		    } else {
			SetMovieLoopState( movPtr->aMovie, kQTTclNoLooping );
		    }
		}
	    } else {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
		continue;
	    }
	}
	if (mask & MOV_CONF_PALINDROME_LOOPSTATE) {
	    if (movPtr->aMovie != NULL) {
		if (movPtr->aController != NULL) {
		    if (movPtr->palindromeloopstate) {
			SetLoopingState( movPtr->aController, kQTTclPalindromeLooping );
		    } else {
			SetLoopingState( movPtr->aController, kQTTclNoLooping );
		    }
		} else {
		    if (movPtr->palindromeloopstate) {
			SetMovieLoopState( movPtr->aMovie, kQTTclPalindromeLooping );
		    } else {
			SetMovieLoopState( movPtr->aMovie, kQTTclNoLooping );
		    }
		}
	    } else {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( "No movie", -1 ) );
		continue;
	    }
	}
	
	/* 
	 * Set the movie's volume if changed. If user interactively changes the volume,
	 * we cannot detect this. 
	 * The 'MCMovieChanged' updates the volume button in the controller.
	 */
	
	if ((movPtr->aMovie != NULL) && (mask & MOV_CONF_VOLUME)) {
	    SetMovieVolume( movPtr->aMovie, (short) movPtr->volume );
	    if (movPtr->aController != NULL) {
		MCMovieChanged( movPtr->aController, movPtr->aMovie );
	    }
	}
	
	/*
	 * Set movie's preferred play rate.
	 */
	
	if ((movPtr->aMovie != NULL) && (mask & MOV_CONF_PREFERRED_RATE)) {
	    SetMoviePreferredRate( movPtr->aMovie, X2Fix( movPtr->preferredRate ) );
	}		
	
	/*
	 * Here we set any callback function for the movie controller so that 
	 * a tcl procedure can monitor user actions. 
	 * It is also necessary if we allow resizing via the controller.
	 */
	
	if ((movPtr->aMovie != NULL) && (movPtr->aController != NULL) && 
	    ((movPtr->mcCallbackProc != NULL) || movPtr->resizable)) {
#if TARGET_API_MAC_CARBON
	    MCSetActionFilterWithRefCon( movPtr->aController, 
					NewMCActionFilterWithRefConUPP( MovieControllerCallbackFunction ),
					(long) movPtr );
#else
	    MCSetActionFilterWithRefCon( movPtr->aController, 
					NewMCActionFilterWithRefConProc( MovieControllerCallbackFunction ),
					(long) movPtr );
#endif				
	}
	
	/*
	 * Get the QTVR instance for QTVR panos. Returns NULL for non QTVR movies.
	 * Special configuration for QTVR movies.
	 */
	
	if ((movPtr->aMovie != NULL) && (movPtr->aController != NULL) && 
	    IsQTVRMovie( movPtr->aMovie )) {
	    movPtr->qtvrInstance = GetQTVRInstanceFromController( movPtr->aController );
	    if (movPtr->qtvrInstance != NULL) {
		ConfigureQTVRMovie( interp, movPtr,
				   mask & MOV_CONF_QTVR_QUALITY_STATIC,
				   mask & MOV_CONF_QTVR_QUALITY_MOTION );
	    }
	}
	
	/*
	 * Is there a custom button in the movie controller bar?
	 */
	
	if ((movPtr->aController != NULL) && (mask & MOV_CONF_CUSTOM_BUTTON)) {
	    if (movPtr->custombutton) {
		ShowControllerButton( movPtr->aController, mcFlagsUseCustomButton );
	    } else {
		HideControllerButton( movPtr->aController, mcFlagsUseCustomButton );
	    }
	    MCMovieChanged( movPtr->aController, movPtr->aMovie );
	}
	
	/*
	 * Do we want the movie to be resizable via the controller.
	 */
	
	if ((movPtr->aController != NULL) && (mask & MOV_CONF_RESIZEABLE)) {
	    Rect    r;
	    
#if TARGET_API_MAC_CARBON
	    MacSetRect( &r, 0, 0, 2000, 1800 );
#else
	    {
		CGrafPtr    wmPortPtr = NULL;
		
		GetCWMgrPort( &wmPortPtr );
		r = wmPortPtr->portRect;
	    }
#endif
	    if (movPtr->resizable) {
		MCDoAction( movPtr->aController, mcActionSetGrowBoxBounds, &r );
	    } else {
		MacSetRect( &r, 0, 0, 0, 0 );
		MCDoAction( movPtr->aController, mcActionSetGrowBoxBounds, &r );
	    }
	    MCMovieChanged( movPtr->aController, movPtr->aMovie );
	}
	
	/* 
	 * Loading a remote movie into ram is unpredictable. 
	 */
	
	if ((movPtr->aMovie != NULL) && (movPtr->url == NULL) &&
	    (mask & MOV_CONF_LOAD_INTO_RAM)) {
	    if (movPtr->loadIntoRam) {
		optFlags = keepInRam;
	    } else {
		optFlags = unkeepInRam;
	    }
	    err = LoadMovieIntoRam( movPtr->aMovie, 0, 
				   GetMovieDuration(movPtr->aMovie), optFlags );
	    if (err != noErr) {
		CheckAndSetErrorResult( movPtr->interp, noErr );
		return TCL_ERROR;
	    }
	}
        
        /*
         * Changing any of the -highlight* needs a redisplay.
         */
	
	if (mask & MOV_CONF_HIGHLIGHTS) {
	    movPtr->flags |= NEWGWORLD;
        }	
	
	/*
	 * If we came so far break out of the ierror loop.
	 */
	
	break;
    }
    
    if (ierror) {
	Tcl_SetObjResult(interp, errorResult);
	Tcl_DecrRefCount(errorResult);
	return TCL_ERROR;
    } else {
	/* 
	 * If width or height changed, reschedule a new display. 
	 */
	
	if (mask & MOV_CONF_NEWGWORLD) {
	    movPtr->flags |= NEWGWORLD;
    	}
	Tk_FreeSavedOptions( &savedOptions );
	return TCL_OK;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureQTVRMovie --
 *
 *		Processes the QTVR specific options.
 *
 * Results:
 *		The return value is a standard Tcl result.  If TCL_ERROR is
 *		returned, then the interp's result contains an error message.
 *
 * Side effects:
 *		Movie settings may be changed.
 *
 *----------------------------------------------------------------------
 */

static int
ConfigureQTVRMovie( Tcl_Interp *interp, 
		   MoviePlayer * movPtr, 
		   int setQTVRQualityStatic, 
		   int setQTVRQualityMotion )
{
    int         result = TCL_OK;
    SInt32      propertyValue;
    
    if (!IsQTVRMovie( movPtr->aMovie )) {
        return result;
    }
    if (movPtr->qtvrInstance == NULL) {
        return result;
    }
    
    /* 
     * In QTVR panos we doesn't want tk to set our cursor. Doesn't work!
     * Handle quality settings.
     */     
    
    Tk_UndefineCursor( movPtr->tkwin );
    
    if (movPtr->indQTVRQualityStatic == -1) {
	
        /* First time: */
	
        QTVRGetImagingProperty( movPtr->qtvrInstance, kQTVRStatic, kQTVRImagingQuality, 
			       &propertyValue ); 
        if (propertyValue == codecMinQuality) {
            movPtr->indQTVRQualityStatic = MOV_QTVR_QUALITY_MIN;
        } else if (propertyValue == codecLowQuality) {
            movPtr->indQTVRQualityStatic = MOV_QTVR_QUALITY_LOW;
        } else if (propertyValue == codecNormalQuality) {
            movPtr->indQTVRQualityStatic = MOV_QTVR_QUALITY_NORMAL;
        } else if (propertyValue == codecHighQuality) {
            movPtr->indQTVRQualityStatic = MOV_QTVR_QUALITY_HIGH;
        } else if (propertyValue == codecMaxQuality) {
            movPtr->indQTVRQualityStatic = MOV_QTVR_QUALITY_MAX;
        }             
    } else if (setQTVRQualityStatic) {
	
        /* User configured this one: */
	
       	switch (movPtr->indQTVRQualityStatic) {
	    case MOV_QTVR_QUALITY_MIN: {
		propertyValue = codecMinQuality;
		break;
	    }
	    case MOV_QTVR_QUALITY_LOW: {
		propertyValue = codecLowQuality;
		break;
	    }
	    case MOV_QTVR_QUALITY_NORMAL: {
		propertyValue = codecNormalQuality;
		break;
	    }
	    case MOV_QTVR_QUALITY_HIGH: {
		propertyValue = codecHighQuality;
		break;
	    }
	    case MOV_QTVR_QUALITY_MAX: {
		propertyValue = codecMaxQuality;
		break;
	    }
       	}
        QTVRSetImagingProperty( movPtr->qtvrInstance, kQTVRStatic, kQTVRImagingQuality, 
			       propertyValue ); 
    }
    
    if (movPtr->indQTVRQualityMotion == -1) {
	
	/* First time: */
	
	QTVRGetImagingProperty( movPtr->qtvrInstance, kQTVRMotion, kQTVRImagingQuality, 
			       &propertyValue ); 
        if (propertyValue == codecMinQuality) {
            movPtr->indQTVRQualityMotion = MOV_QTVR_QUALITY_MIN;
        } else if (propertyValue == codecLowQuality) {
            movPtr->indQTVRQualityMotion = MOV_QTVR_QUALITY_LOW;
        } else if (propertyValue == codecNormalQuality) {
            movPtr->indQTVRQualityMotion = MOV_QTVR_QUALITY_NORMAL;
        } else if (propertyValue == codecHighQuality) {
            movPtr->indQTVRQualityMotion = MOV_QTVR_QUALITY_HIGH;
        } else if (propertyValue == codecMaxQuality) {
            movPtr->indQTVRQualityMotion = MOV_QTVR_QUALITY_MAX;
        }             
    } else if (setQTVRQualityMotion) {
	
        /* User configured this one: */
	
       	switch (movPtr->indQTVRQualityMotion) {
	    case MOV_QTVR_QUALITY_MIN: {
		propertyValue = codecMinQuality;
		break;
	    }
	    case MOV_QTVR_QUALITY_LOW: {
		propertyValue = codecLowQuality;
		break;
	    }
	    case MOV_QTVR_QUALITY_NORMAL: {
		propertyValue = codecNormalQuality;
		break;
	    }
	    case MOV_QTVR_QUALITY_HIGH: {
		propertyValue = codecHighQuality;
		break;
	    }
	    case MOV_QTVR_QUALITY_MAX: {
		propertyValue = codecMaxQuality;
		break;
	    }
       	}
        QTVRSetImagingProperty( movPtr->qtvrInstance, kQTVRMotion, kQTVRImagingQuality, 
			       propertyValue ); 
    }        
    
    /*
     * Callback function for QTVR movies.
     */
    
    if ((movPtr->aController != NULL) && (movPtr->mcCallbackProc != NULL)) {
	
#if TARGET_API_MAC_CARBON
	movPtr->funcQTVRIntercept = NewQTVRInterceptUPP( MyQTVRInterceptProc );
#else
	movPtr->funcQTVRIntercept = NewQTVRInterceptProc( MyQTVRInterceptProc );
#endif
	QTVRInstallInterceptProc( movPtr->qtvrInstance, kQTVRSetPanAngleSelector, 
				 movPtr->funcQTVRIntercept, (long) movPtr, 0 );
	QTVRInstallInterceptProc( movPtr->qtvrInstance, kQTVRSetTiltAngleSelector, 
				 movPtr->funcQTVRIntercept, (long) movPtr, 0 );
	QTVRInstallInterceptProc( movPtr->qtvrInstance, kQTVRSetFieldOfViewSelector, 
				 movPtr->funcQTVRIntercept, (long) movPtr, 0 );
	QTVRInstallInterceptProc( movPtr->qtvrInstance, kQTVRTriggerHotSpotSelector, 
				 movPtr->funcQTVRIntercept, (long) movPtr, 0 );
	QTVRInstallInterceptProc( movPtr->qtvrInstance, kQTVRMouseDownSelector, 
				 movPtr->funcQTVRIntercept, (long) movPtr, 0 );
        /*
	 QTVRInstallInterceptProc( movPtr->qtvrInstance, kQTVRMouseEnterSelector, 
	 movPtr->funcQTVRIntercept, (long) movPtr, 0 );
	 QTVRInstallInterceptProc( movPtr->qtvrInstance, kQTVRMouseLeaveSelector, 
	 movPtr->funcQTVRIntercept, (long) movPtr, 0 );
	 QTVRInstallInterceptProc( movPtr->qtvrInstance, kQTVRGetHotSpotTypeSelector, 
	 movPtr->funcQTVRIntercept, (long) movPtr, 0 );
	 */
    } else {   
	QTVRInstallInterceptProc( movPtr->qtvrInstance, kQTVRSetPanAngleSelector, NULL, 0, 0 );
	QTVRInstallInterceptProc( movPtr->qtvrInstance, kQTVRSetTiltAngleSelector, NULL, 0, 0 );
	QTVRInstallInterceptProc( movPtr->qtvrInstance, kQTVRSetFieldOfViewSelector, NULL, 0, 0 );
	QTVRInstallInterceptProc( movPtr->qtvrInstance, kQTVRMouseEnterSelector, NULL, 0, 0 );
	QTVRInstallInterceptProc( movPtr->qtvrInstance, kQTVRMouseLeaveSelector, NULL, 0, 0 );
	QTVRInstallInterceptProc( movPtr->qtvrInstance, kQTVRTriggerHotSpotSelector, NULL, 0, 0 );
	QTVRInstallInterceptProc( movPtr->qtvrInstance, kQTVRGetHotSpotTypeSelector, NULL, 0, 0 );
	QTVRInstallInterceptProc( movPtr->qtvrInstance, kQTVRMouseDownSelector, NULL, 0, 0 );
#if TARGET_API_MAC_CARBON
	DisposeQTVRInterceptUPP( movPtr->funcQTVRIntercept );
#else
	DisposeRoutineDescriptor( movPtr->funcQTVRIntercept );
#endif
	movPtr->funcQTVRIntercept = NULL;
    }
    if (movPtr->swing) {
        QTVREnableTransition( movPtr->qtvrInstance, kQTVRTransitionSwing, true );
    } else {
        QTVREnableTransition( movPtr->qtvrInstance, kQTVRTransitionSwing, false );
    }
    
    /* Verify that the speed is an integer between 1 and 10. */
    if (movPtr->swingSpeed < 1) {
        movPtr->swingSpeed = 1;
    } else if (movPtr->swingSpeed > 10) {
        movPtr->swingSpeed = 10;
    }
    QTVRSetTransitionProperty( movPtr->qtvrInstance, kQTVRTransitionSwing, 
			      kQTVRTransitionSpeed, movPtr->swingSpeed );
    QTVRSetTransitionProperty( movPtr->qtvrInstance, kQTVRTransitionSwing, 
			      kQTVRTransitionDirection, -1 );
    
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * GetMovie --
 *
 *	Get a Movie given a filename, all info is stored in movPtr.
 *
 * Results:
 *	Normal TCL.
 *
 * Side effects:
 *	Movie Allocated
 *
 *----------------------------------------------------------------------
 */

static int 
GetMovie( MoviePlayer *movPtr ) 
{
    OSErr       err = noErr;
    Rect 	aRect;
    Fixed	thePlayRate;
    short       resId = 0;
    int		result = TCL_OK;
    OSType	dataRefType;
    Handle	dataRef = NULL;
    
    QTTclDebugPrintf( movPtr->interp, 2, "GetMovie" );
    
    /*
     * Translate file name to Data Reference.
     */
    result = QTTclNewDataRefFromUTF8Obj(movPtr->interp, movPtr->fileNamePtr,
	    &dataRef, &dataRefType);
    if (result != TCL_OK) {
	result = TCL_ERROR;
	goto bail;
    }
    err = NewMovieFromDataRef(&(movPtr->aMovie), newMovieActive,
                                &resId, dataRef, dataRefType);
    if (err != noErr) {
        CheckAndSetErrorResult( movPtr->interp, err );
	result = TCL_ERROR;
	goto bail;
    }
    QTTclDebugPrintf( movPtr->interp, 2, "\taMovie=%d", movPtr->aMovie );
    
    movPtr->resourceNumber = resId;
    gMovieRefCount++;
    
    /* This disables the movie so it wont play and wont show up. */
    //SetMovieActive( movPtr->aMovie, false );	// gives no sound sometimes
    SetMovieActive( movPtr->aMovie, true );
    
    /* The movies natural size without any controller, and a few other options */
    GetMovieBox( movPtr->aMovie, &aRect );
    movPtr->mwidth = aRect.right - aRect.left;
    movPtr->mheight = aRect.bottom - aRect.top;
    thePlayRate = GetMoviePreferredRate( movPtr->aMovie );
    movPtr->preferredRate = Fix2X( thePlayRate );
    if (movPtr->volume == 255) {
	movPtr->volume = GetMoviePreferredVolume( movPtr->aMovie );
    }
    
    /*
     * -1 sets the default progress, NULL removes any progress dialog.
     */
    SetMovieProgressProc( movPtr->aMovie, (MovieProgressUPP) -1L, 0 ); 
    
bail:
    if (dataRef) {
	DisposeHandle(dataRef);
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * GetMovieFromUrl --
 *
 *		Get a Movie from the url (stored in movPtr).
 *
 * Results:
 *		Normal TCL.
 *
 * Side effects:
 *		Movie allocated.
 *
 *----------------------------------------------------------------------
 */

static int 
GetMovieFromUrl( MoviePlayer *movPtr ) 
{
    char			*charPtr;
    int				len = 0;
    short			flags = 0;
    Tcl_DString     ds;
    Rect			aRect;
    Handle			urlDataRef = NULL;
    CGrafPtr 		oldPort = NULL;
    GDHandle 		oldGDeviceH = NULL;
    QDErr       	err = noErr;	
    int				result = TCL_OK;
    
    QTTclDebugPrintf( movPtr->interp, 2, "GetMovieFromUrl" );
    
    if (strlen(movPtr->url) == 0) {
	Tcl_SetObjResult( movPtr->interp, Tcl_NewStringObj( 
							   "URL of zero length not valid", -1 ) );
	result = TCL_ERROR;
	goto bail;
    }
    
    charPtr = Tcl_UtfToExternalDString( gQTTclTranslationEncoding, movPtr->url, -1, &ds);
    len = Tcl_DStringLength( &ds );
    urlDataRef = MySafeNewHandle( len + 1, 1 );	
    BlockMoveData( charPtr, *urlDataRef, len );
    
    /* 
     * Fetch the movie. In blocking mode if no 'loadCommand',
     * else async. Need to use 'gMoviePlayerListPtr' and the state member to serve
     * it while loading. Note: async movie must? be active to be served? yes.
     */
    
    movPtr->resourceNumber = -1;
    GetGWorld( &oldPort, &oldGDeviceH );
    SetGWorld( gOffscreenGWorldPtr, NULL );
    
    if (movPtr->loadCommand != NULL) {
	flags = newMovieActive | newMovieAsyncOK;
    } else {
        flags = newMovieActive;
    }
    
    err = NewMovieFromDataRef( &(movPtr->aMovie), flags, NULL, urlDataRef, 
			      URLDataHandlerSubType );
    if (err != noErr) {
	CheckAndSetErrorResult( movPtr->interp, err );
	result = TCL_ERROR;
	goto bail;
    }
    gMovieRefCount++;
    if (movPtr->loadCommand != NULL) {
	
	/* The movie should be served, with 'MoviesTask', but not displayed. */
	movPtr->state |= kQTTclMovieStateAsyncLoading;
    } else {
	
	/* Set the rect to the movies natural size. */
	GetMovieBox( movPtr->aMovie, &aRect );
	movPtr->mwidth = aRect.right - aRect.left;
	movPtr->mheight = aRect.bottom - aRect.top;
    }
    
bail:
    
    if (urlDataRef != NULL) {
	DisposeHandle( urlDataRef );
    }
    Tcl_DStringFree( &ds );		    
    SetGWorld( oldPort, oldGDeviceH );
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * MovieProgressFunction  --
 *
 *		Called by the Movie Toolbox when there is a long operation.
 *
 * Results:
 *		Mac Error Codes
 *
 * Side effects:
 *		Tcl command may be run.
 *
 *----------------------------------------------------------------------
 */

static pascal OSErr 
MovieProgressFunction( Movie theMovie, short message, short whatOperation,
		      Fixed percentDone, long refcon )
{
    MoviePlayer     *movPtr = (MoviePlayer *) refcon;
    int             result = TCL_OK;
    char            cmd[255];
    char            percent[255];
    
    if (movPtr->progressProc != NULL) {
	strcpy( cmd, movPtr->progressProc );
	strcat( cmd, " " );
	strcat( cmd, Tcl_GetCommandName( movPtr->interp, movPtr->widgetCmd ) );
	
	switch (message) {
	    case movieProgressOpen: {
		strcat(cmd, " open");
		break;
	    }
	    case movieProgressUpdatePercent: {
		strcat(cmd, " percent");
		break;
	    }
	    case movieProgressClose: {
		strcat(cmd, " close");
		break;
	    }
	    default:
		strcat(cmd, " other");
	}
	
	switch (whatOperation) {
	    case progressOpFlatten: {
		strcat(cmd, " flatten");
		break;
	    }
	    case progressOpInsertTrackSegment: {
		strcat(cmd, " insertTrackSegment");
		break;
	    }
	    case progressOpInsertMovieSegment: {
		strcat(cmd, " insertMovieSegment");
		break;
	    }
	    case progressOpPaste: {
		strcat(cmd, " paste");
		break;
	    }
	    case progressOpAddMovieSelection: {
		strcat(cmd, " addMovieSelection");
		break;
	    }
	    case progressOpCopy: {
		strcat(cmd, " copy");
		break;
	    }
	    case progressOpCut: {
		strcat(cmd, " cut");
		break;
	    }
	    case progressOpLoadMovieIntoRam: {
		strcat(cmd, " loadMovieIntoRam");
		break;
	    }
	    case progressOpLoadTrackIntoRam: {
		strcat(cmd, " loadTrackIntoRam");
		break;
	    }
	    case progressOpLoadMediaIntoRam: {
		strcat(cmd, " loadMediaIntoRam");
		break;
	    }
	    case progressOpImportMovie: {
		strcat(cmd, " importMovie");
		break;
	    }
	    case progressOpExportMovie: {
		strcat(cmd, " exportMovie");
		break;
	    }
	    default: {
		strcat(cmd, " other");
	    }
	}
	
	sprintf( percent, " %ld", Fix2Long( FixMul( Long2Fix(100), percentDone ) ) );
	strcat( cmd, percent );
	result = Tcl_Eval( movPtr->interp, cmd );
    }
    // This seems to create infinite loop in some circumstances!!!
    Tcl_DoOneEvent( TCL_DONT_WAIT | TCL_ALL_EVENTS );
    
    if (result != TCL_OK) {
	
        /*
         * Whenever the tcl procedure returns anything else than TCL_OK, this should
         * trigger a cancellation of the job. A press to a cancel button should return,
         * for instance, a 'break' code (return -code 3).
         */
	
	return userCanceledErr;
    } else {
	return noErr;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * MovieControllerCallbackFunction  --
 *
 *		Called by the Movie Controller when there is an action.
 *
 * Results:
 *		Mac Error Codes
 *
 * Side effects:
 *		Tcl command may be run, or 
 *
 *----------------------------------------------------------------------
 */

static pascal Boolean 
MovieControllerCallbackFunction( MovieController mc, 
				short action, 
				void *params,
				long refCon )
{
    MoviePlayer 	*movPtr = (MoviePlayer *) refCon;
    Tcl_Interp 		*interp = movPtr->interp;
    Movie           saveMovie = NULL;
    MovieController saveController = NULL;
    Rect			myMovieBounds;
    int 			result = TCL_OK;
    int             width, height;
    int				prevWidth, prevHeight;
    Boolean			isHandled = false;
    Boolean			isResized;
    Point			aPoint;
    Rect            aRect;
    TimeRecord		*timePtr;
    UInt32			aMessage;
    char 			cmd[255];
    char			strPara[255];
    
    if ((action == mcActionIdle) || (movPtr->aMovie == NULL)) {
	return isHandled;
    }
    
    /*
     * Signals that we does not exist anymore.
     */
    
    if (movPtr->flags & MOVIE_DELETED) {
	return isHandled;
    }
    
    /*
     * We shall not call the tcl proc recursively since this screws up any idle calls
     * on exiting this function.
     */
    
    if (movPtr->insideMCCommand) {
	return isHandled;
    }
    QTTclDebugPrintf( movPtr->interp, 4, "MovieControllerCallbackFunction enter" );
    
    Tcl_Preserve( (ClientData) movPtr );
    saveMovie = movPtr->aMovie;
    saveController = movPtr->aController;
    
    /*
     * We can have either a tcl procedure or resizing via the controller or both.
     */
    
    if (movPtr->resizable && (action == mcActionControllerSizeChanged) && 
	!(movPtr->flags & REDRAW_PENDING)) {
	MCGetControllerBoundsRect( movPtr->aController, &myMovieBounds );
	width = myMovieBounds.right - myMovieBounds.left;
	height = myMovieBounds.bottom - myMovieBounds.top;
	
	/*
	 * This procedure gets called, for instance, on the initial display, and we
	 * must figure out if the size actually changed before requesting a new size.
	 */
	
	if (movPtr->width == 0) {
	    prevWidth = movPtr->mwidth;
	} else {
	    prevWidth = movPtr->width;
	}
	if (movPtr->height == 0) {
	    prevHeight = movPtr->mheight + movPtr->controllerHeight;
	} else {
	    prevHeight = movPtr->height;
	}
	if ((prevWidth != width) || (prevHeight != height)) {
	    isResized = true;
	} else {
	    isResized = false;
	}
	
	QTTclDebugPrintf( interp, 2, "\taction=%d, width=%d, height=%d, isResized=%d", 
			 action, width, height, isResized );
	
	/*
	 * We try to make an immediate resize here. The size is changed before this
	 * procedure returns. Schedule for a redisplay to tell tk as well.
	 */
	
	Tk_ResizeWindow( movPtr->tkwin, width, height );
	
	/* 
	 * Need to set the width and height elements if resized interactively, but not
	 * if the user has 'configure'ed us to any width = 0, or height = 0.
	 */
	
	if (width != movPtr->mwidth) {
	    movPtr->width = width;
	}
	if (height != movPtr->mheight + movPtr->controllerHeight) {
	    movPtr->height = height;
	}
	
	if (isResized) {
	    movPtr->flags |= NEWGWORLD;
	    movPtr->flags |= UPDATEMOVIE;
	    MoviePlayerWorldChanged( (ClientData) movPtr );
        }
    }
    
    /*
     * If we have '-mcedit' enabled, any selection must invalidate any track selection.
     */
    
    if (movPtr->mcEdit && ( (action == mcActionSetSelectionBegin) || 
			   (action == mcActionSetSelectionDuration) )) {
	movPtr->trackSelect->trackID = 0;
    }
    
    /*
     * Catch the event, get any parameters, and call the registered tcl callback procedure.
     * 'ProcName widget action {parameter}'
     */
    
    if (movPtr->mcCallbackProc != NULL) {
       	CGrafPtr 	saveWorld = NULL;
       	GDHandle 	saveDevice = NULL;
       	GWorldPtr   destPort = NULL;
       	//Tcl_Obj		*listObjPtr;
	
	/*
	 listObjPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );
	 Tcl_ListObjAppendElement( interp, listObjPtr, 
	 Tcl_NewStringObj(movPtr->mcCallbackProc, -1) );
	 */				
	strcpy( cmd, movPtr->mcCallbackProc );
	strcat( cmd, " " );
	strcat( cmd, Tcl_GetCommandName(interp, movPtr->widgetCmd) );
	
	switch (action) {
	    case mcActionActivate: {
		strcat(cmd, " activate");
		break;
	    }
	    case mcActionCustomButtonClick: {
		strcat(cmd, " customButtonClick");
		aPoint = ((EventRecord *) params)->where;
		LocalToGlobal( &aPoint );
		sprintf( strPara, " {%d %d}", aPoint.h, aPoint.v );
		strcat(cmd, strPara);
		isHandled = true;
		break;
	    }
	    case mcActionDeactivate: {
		strcat(cmd, " deactivate");
		break;
	    }
	    case mcActionGoToTime: {
		timePtr = (TimeRecord *) params;
		strcat(cmd, " goToTime" );
		sprintf( strPara, " {%d %d %d}", 
			(int) timePtr->value.hi, (int) timePtr->value.lo, (int) timePtr->scale );
		strcat( cmd, strPara );
		break;
	    }
	    case mcActionKey: {
		strcat(cmd, " key");
		aMessage = ((EventRecord *) params)->message;
		sprintf( strPara, " %c", (int) aMessage & charCodeMask );
		strcat(cmd, strPara);
		break;
	    }
	    case mcActionMouseDown: {
		strcat(cmd, " mouseDown");
		aPoint = ((EventRecord *) params)->where;
		LocalToGlobal( &aPoint );
		sprintf( strPara, " {%d %d}", aPoint.h, aPoint.v );
		strcat(cmd, strPara);
		break;
	    }
	    case mcActionPlay: {
		strcat(cmd, " play");
		sprintf( strPara, " %f", Fix2X((Fixed) params) );
		strcat( cmd, strPara );
		break;
	    }
	    case mcActionSetSelectionBegin: {
		timePtr = (TimeRecord *) params;
		strcat(cmd, " setSelectionBegin");
		sprintf( strPara, " %li", timePtr->value.lo );	
		strcat( cmd, strPara );
		break;
	    }
	    case mcActionSetSelectionDuration: {
		timePtr = (TimeRecord *) params;
		strcat(cmd, " setSelectionDuration");
		sprintf( strPara, " %li", timePtr->value.lo );	
		strcat( cmd, strPara );
		break;
	    }
	    case mcActionSetVolume: {
		strcat(cmd, " setVolume");
		sprintf( strPara, " %li", ((EventRecord *) params)->message );	
		strcat( cmd, strPara );
		break;
	    }
	    default:
                goto bail;
	}
	
	/*
	 * Perform the actual action. Any 'update' command in the tcl procedure
	 * ruins update events sent to redraw the controller, and we must therefore
	 * invalidate, make dirty, the controller rectangle!
         * Beware, we may have been destroyed in the callback!
	 */
	
    	movPtr->insideMCCommand++;
	result = Tcl_Eval( interp, cmd );
    	movPtr->insideMCCommand--;
        
        if (movPtr->flags & MOVIE_DELETED) {
            isHandled = true;
            goto bail;
        }
	
       	GetGWorld( &saveWorld, &saveDevice );	
    	GetMovieGWorld( saveMovie, &destPort, NULL );
    	SetGWorld( destPort, NULL );
        MCGetControllerBoundsRect( saveController, &aRect );
        aRect.top = aRect.bottom - movPtr->controllerHeight;    	
#if TARGET_API_MAC_CARBON
        // This seems to create infinite loop in some circumstances!!!
        //Tcl_DoOneEvent( TCL_DONT_WAIT | TCL_ALL_EVENTS );
        InvalWindowRect( GetWindowFromPort(destPort), &aRect );
#else
        InvalRect( &aRect );
#endif        
       	SetGWorld( saveWorld, saveDevice );
	
       	/*
       	 * If the original movie was destroyed or configure -file, then we put the
       	 * destruction as an idle call so that we don't crash.
       	 */
	
       	if (movPtr->tmpMovieRecordPtr != NULL) {
            Tcl_DoWhenIdle( DisposeMovieAtIdle, (ClientData) movPtr );
       	}
    }	
    
    /*
     * We should return false to indicate that we only monitor the events here,
     * except if the callback script changed our movie (with configure -file),
     * or if script returned TCL_BREAK (break).
     */
    
    if ((saveMovie != movPtr->aMovie) || (result == TCL_BREAK)) {
        isHandled = true;
        
        /*
         * We are still processing the old movie, and therefore need to stop any updates of it.
         * Seems not to work, schedule redisplay of new.
         */
	
      	if (movPtr->tmpMovieRecordPtr != NULL) {
	    MCGetControllerBoundsRect( movPtr->tmpMovieRecordPtr->movieController, &aRect );
#if TARGET_API_MAC_CARBON
            {
                GWorldPtr   destPort = NULL;
		
		GetMovieGWorld( movPtr->tmpMovieRecordPtr->movie, &destPort, NULL );
                //ValidWindowRect( GetWindowFromPort(destPort), &aRect );
            }
#else
            ValidRect( &aRect );
#endif        
        }
    	if (!(movPtr->flags & MOVIE_DELETED) && !(movPtr->flags & REDRAW_PENDING)) {
	    Tcl_DoWhenIdle( DisplayMovie, (ClientData) movPtr );
	    movPtr->flags |= REDRAW_PENDING;
    	}
	
	QTTclDebugPrintf( interp, 2, 
			 "\tMovieControllerCallbackFunction:: isHandled = true" );
    }   
    
bail:
    QTTclDebugPrintf( movPtr->interp, 4, "\tMovieControllerCallbackFunction exit" );
    Tcl_Release( (ClientData) movPtr );
    
    return isHandled;
}

/*
 *----------------------------------------------------------------------
 *
 * MyQTVRInterceptProc --
 *
 *		Used to monitor user action in a QTVR movie.
 *
 * Results:
 *		boolean
 *
 * Side effects:
 *		the intercepted QTVR procedure called.
 *
 *----------------------------------------------------------------------
 */

pascal void
MyQTVRInterceptProc( QTVRInstance qtvrInst, 
		    QTVRInterceptPtr msg, 
		    SInt32 refCon, 
		    Boolean *cancel )
{
    MoviePlayer 	*movPtr = (MoviePlayer *) refCon;
    Boolean         hasCancelled = false;
    CGrafPtr 	    saveWorld = NULL;
    GDHandle 	    saveDevice = NULL;
    GWorldPtr       destPort = NULL;
    Movie           saveMovie = NULL;
    Rect            aRect;
    int 			result = TCL_OK;
    char 			cmd[255];
    char			strPara[255];
    float           fValue;
    UInt32          uint32Value;
    OSType          typeValue;
    
    *cancel = hasCancelled;
    if (movPtr->mcCallbackProc == NULL) {
        return;
    }
    
    /*
     * Signals that we does not exist anymore.
     */
    
    if (movPtr->flags & MOVIE_DELETED) {
	return;
    }
    
    /*
     * We shall not call the tcl proc recursively since this screws up any idle calls
     * on exiting this function.
     */
    
    if (movPtr->insideMCCommand) {
	return;
    }
    Tcl_Preserve((ClientData) movPtr);
    
    saveMovie = movPtr->aMovie;
    strcpy( cmd, movPtr->mcCallbackProc );
    strcat( cmd, " " );
    strcat( cmd, Tcl_GetCommandName(movPtr->interp, movPtr->widgetCmd) );
    
    switch (msg->selector) {
        case kQTVRSetPanAngleSelector:
            fValue = *((float *) msg->parameter[0]);
	    strcat(cmd, " pan");
	    sprintf( strPara, " %f", RadiansToDegrees( fValue ) );
	    strcat( cmd, strPara );
            break;
        case kQTVRSetTiltAngleSelector:
            fValue = *((float *) msg->parameter[0]);
	    strcat(cmd, " tilt");
	    sprintf( strPara, " %f", RadiansToDegrees( fValue ) );
	    strcat( cmd, strPara );
            break;
        case kQTVRSetFieldOfViewSelector:
            fValue = *((float *) msg->parameter[0]);
	    strcat(cmd, " fieldofview");
	    sprintf( strPara, " %f", RadiansToDegrees( fValue ) );
	    strcat( cmd, strPara );
            break;
        case kQTVRSetViewCenterSelector:
	    
            break;
        case kQTVRMouseEnterSelector:
	    
            break;
        case kQTVRMouseWithinSelector:
	    
            break;
        case kQTVRMouseLeaveSelector:
	    
            break;
        case kQTVRMouseDownSelector:
            /*
	     strcat(cmd, " mouseDown");
	     sprintf( strPara, " {%f %f %d %d %d}", *((float *) msg->parameter[0]),
	     *((float *) msg->parameter[1]), ((UInt32) msg->parameter[2]),
	     ((UInt32) msg->parameter[3]), ((UInt32) msg->parameter[4]) );
	     strcat(cmd, strPara); */
            break;
        case kQTVRMouseStillDownSelector:
	    
            break;
        case kQTVRMouseUpSelector:
	    
            break;
        case kQTVRTriggerHotSpotSelector:
            uint32Value = ((UInt32) msg->parameter[0]);
	    strcat(cmd, " triggerhotspot");
	    sprintf( strPara, " %li", uint32Value );
	    strcat( cmd, strPara );
            break;
        case kQTVRGetHotSpotTypeSelector:
            uint32Value = ((UInt32) msg->parameter[0]);
            typeValue = *((OSType *) msg->parameter[1]);
	    strcat(cmd, " hotspottype");
	    sprintf( strPara, " %li %li", uint32Value, typeValue );
	    strcat( cmd, strPara );
	    
            break;
        default:
            goto exit;
    }
    
    movPtr->insideMCCommand++;
    result = Tcl_Eval( movPtr->interp, cmd );
    movPtr->insideMCCommand--;
    
    /*
     * Any 'update' command in the tcl procedure
     * ruins update events sent to redraw the controller, and we must therefore
     * invalidate, make dirty, the controller rectangle!
     * Beware, we may have been destroyed in the callback! Note Preserve/Release.
     */
    
    if (!(movPtr->flags & MOVIE_DELETED) && (movPtr->aController != NULL)) {
      	GetGWorld( &saveWorld, &saveDevice );	
        GetMovieGWorld( movPtr->aMovie, &destPort, NULL );
        SetGWorld( destPort, NULL );
        MCGetControllerBoundsRect( movPtr->aController, &aRect );
        aRect.top = aRect.bottom - movPtr->controllerHeight;    	
#if TARGET_API_MAC_CARBON
        InvalWindowRect( GetWindowFromPort(destPort), &aRect );
#else
        InvalRect( &aRect );
#endif        
      	SetGWorld( saveWorld, saveDevice );
    }
    
    /*
     * If the original movie was destroyed or configure -file, then we put the
     * destruction as an idle call so that we don't crash.
     */
    
    if (movPtr->tmpMovieRecordPtr) {
        Tcl_DoWhenIdle( DisposeMovieAtIdle, (ClientData) movPtr );
    }
    
    /*
     * We should return false to indicate that we only monitor the events here,
     * except if the callback script changed our movie (with configure -file),
     * or if script returned TCL_BREAK (break).
     */
    
    if ((saveMovie != movPtr->aMovie) || (result == TCL_BREAK)) {
        hasCancelled = true;
        
        /*
         * We are still processing the old movie, and therefore need to stop any updates of it.
         * Seems not to work, schedule redisplay of new.
         */
	
      	if (movPtr->tmpMovieRecordPtr) {
	    MCGetControllerBoundsRect( movPtr->tmpMovieRecordPtr->movieController, &aRect );
#if TARGET_API_MAC_CARBON
	    GetMovieGWorld( movPtr->tmpMovieRecordPtr->movie, &destPort, NULL );
            ValidWindowRect( GetWindowFromPort(destPort), &aRect );
#else
            ValidRect( &aRect );
#endif        
        }
    	if (!(movPtr->flags & MOVIE_DELETED) && !(movPtr->flags & REDRAW_PENDING)) {
	    Tcl_DoWhenIdle( DisplayMovie, (ClientData) movPtr );
	    movPtr->flags |= REDRAW_PENDING;
    	}
        
	QTTclDebugPrintf( movPtr->interp, 2, "\tsaveMovie=%d, movPtr->aMovie=%d",
			 saveMovie, movPtr->aMovie );
    }   
    
exit:
    Tcl_Release((ClientData) movPtr);
    *cancel = hasCancelled;
}

/*
 *----------------------------------------------------------------------
 *
 * ControllerCallbackTimer --
 *
 *		Timer function for tcl callbacks from the controller.
 *  	(Presently unused; does not fix update problem!)
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
ControllerCallbackTimer ( ClientData clientData )
{
    int         result;
    ControllerTimerRecord     *timerPtr = (ControllerTimerRecord *) clientData;
    
    /*
     * Perform the actual call to the registered tcl procedure.
     */
    
    result = Tcl_Eval( timerPtr->movPtr->interp, timerPtr->tclCmd );
    
    /* Release memory! */
    ckfree( timerPtr->tclCmd );
    ckfree( (char *) timerPtr );
}

/*
 *----------------------------------------------------------------------
 *
 * LogUndoState --
 *
 *		Capture an UnDo state.
 *
 * Results:
 *  	Normal TCL results
 *
 * Side effects:
 *		Remembers where we are in a movie, memory is likely allocated.
 *		Result in interpreter as a list '-undostate number'.
 *
 *----------------------------------------------------------------------
 */

int
LogUndoState( MoviePlayer *movPtr, Tcl_Interp *interp) 
{	
    MovieEditState  *tmparr = NULL;
    int             result = TCL_OK;
    int             i;
    Tcl_Obj			*listObjPtr;
    
    /* Do we need to make the array bigger? */
    
    if (movPtr->undoCount >= movPtr->editStatesSize) {
	tmparr = (MovieEditState *) 
	ckalloc( sizeof(MovieEditState)*(UNDO_SIZE + movPtr->editStatesSize) );
	if (tmparr == NULL) {
	    Tcl_SetObjResult( interp, Tcl_NewStringObj( "Out of Memory", -1 ) );
	    return TCL_ERROR;
	}
	if (movPtr->editStates) {
	    for (i = 0; i < movPtr->editStatesSize; i++) {
		tmparr[i] = movPtr->editStates[i];
	    }
	    ckfree((char  *) movPtr->editStates);
	}
	movPtr->editStates = tmparr;
	movPtr->editStatesSize += UNDO_SIZE;
    }
    
    movPtr->editStates[movPtr->undoCount] = NewMovieEditState( movPtr->aMovie );
    listObjPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );
    Tcl_ListObjAppendElement( interp, listObjPtr, 
			     Tcl_NewStringObj("-undostate", -1) );				
    Tcl_ListObjAppendElement( interp, listObjPtr, 
			     Tcl_NewIntObj(movPtr->undoCount) );				
    Tcl_AppendObjToObj( Tcl_GetObjResult( interp ), listObjPtr );
    movPtr->undoCount++;
    
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * AddImagesToMovie --
 *
 *		Add images to a media with a duration of duration.
 *
 * Results:
 *		Normal Tcl
 *
 * Side effects:
 *		Memory allocated, any error messages in the interpreter.
 *
 *----------------------------------------------------------------------
 */

int 
AddImagesToMovie( Tcl_Interp *interp, 
		 Media myMedia,              /* the media (in) */
		 TimeValue durationPerFrame, /* the duration of one frame in media's time scale (in) */
		 int nPhotos,                /* number of photos in array (in) */
		 Tk_PhotoHandle *photoList,  /* the photo (in) */
		 int showDialog,             /* show dialog ? (in) */
		 CodecType codecType,        /* the compressor type to use (in) */
		 CodecQ spatialQuality,
		 CodecQ temporalQuality,
		 short colorDepth,
		 long keyFrameRate,
		 TimeValue *sampleTimePtr )  /* returns the position where the first sample was inserted
 * in the media, in media's time (out) */
{   
    GWorldPtr 		        theGWorld = NULL;
    CGrafPtr 		        oldPort = NULL;
    GDHandle 		        oldGDeviceH = NULL;
    ImageDescriptionHandle  imageDesc = NULL;
    ImageSequence           sequenceID = 0;
    ComponentInstance       ci = NULL;
    ComponentResult         err = noErr;
    OSErr                   osErr = noErr;
    long 			        maxCompressedSize;
    long                    dataSize;
    Handle 			        compressedData = NULL;
    Ptr 			        compressedDataPtr = NULL;
    Ptr 			        nextDataPtr = NULL;
    TimeValue               thisSampleTime;
    short                   syncFlag = 0;
    PixMapHandle 	        pixels = NULL;
    int                     pixelDepth;
    Tk_PhotoImageBlock      photoBlock;
    unsigned char 	        *pixelPtr;
    unsigned char 	        *photoPixels;
    Rect 			        myRect;
    Tcl_Obj					*resultObjPtr;
    int 			        right, bottom;	
    int                     iphoto;
    int                     nPhotosInBatch;
    int 			        i, j;
    int 			        result = TCL_OK;
    int                     useStandardCompress = true;  /* mainly for testing */
    
    if (nPhotos <= 0) {
        return TCL_ERROR;
    }
    if ((nPhotos > 1) || showDialog) {
        useStandardCompress = true;
    }
    
    GetGWorld( &oldPort, &oldGDeviceH );
    Tk_PhotoGetSize( photoList[0], &right, &bottom );
    myRect.top = 0;
    myRect.left = 0;
    myRect.right = right;
    myRect.bottom = bottom;
    err = QTNewGWorld( &theGWorld, 32, &myRect, NULL, NULL, kICMTempThenAppMemory );
    if (err != noErr) {
    	CheckAndSetErrorResult( interp, err );
	result = TCL_ERROR;
	goto done;
    }
    
    SetGWorld( theGWorld, NULL );
    pixels = GetGWorldPixMap( theGWorld );
    
    /*
     * Lock down the pixels so they don't move out from under us.
     */
    
    LockPixels(pixels);
    
    /*
     * If we are using CompressImage and has a specific colorDepth, it is necessary
     * to make a GWorld of that colorDepth.
     */
    
    if (!showDialog && ((colorDepth != 0) && (colorDepth != 32))) {
        int     ans;
	
        if (colorDepth <= 32) {
            pixelDepth = colorDepth;
        } else {
            pixelDepth = colorDepth - 32;
        }
        ans = DoesCodecSupport( codecType, kDoesCodecPixmapDepth, pixelDepth );
        if (ans == false) {
	    Tcl_SetObjResult( interp, Tcl_NewStringObj( 
						       "Compressor does not support this colordepth", -1 ) );
	    result = TCL_ERROR;
	    goto done;
        }
        ans = DoesCodecSupport( codecType, kDoesCodecCompressDepth, colorDepth );
        if (ans == false) {
	    Tcl_SetObjResult( interp, Tcl_NewStringObj( 
						       "Compressor does not support this colordepth", -1 ) );
	    result = TCL_ERROR;
	    goto done;
        }
    }
    
    imageDesc = (ImageDescriptionHandle) NewHandle(4);
    
    if (!useStandardCompress && !showDialog && (nPhotos > 1)) {
	
        /*
         * Initialize the dialogless sequence compressor.
         */
	
        err = CompressSequenceBegin( 
				    &sequenceID,      	/* unique identifier for this sequence */
				    pixels,
				    NULL,             	/* tell ICM to allocate previous buffer */
				    &myRect,
				    &myRect,
				    colorDepth,       
				    codecType,
				    (CompressorComponent) anyCodec,
				    spatialQuality,  	/* spatial quality */
				    temporalQuality,    /* temporal quality */
				    keyFrameRate,       /* key frame rate */
				    NULL,               /* default color table */
				    codecFlagUpdatePrevious,
				    imageDesc );
    	if (err != noErr) {
	    UnlockPixels(pixels);
	    CheckAndSetErrorResult( interp, err );
	    result = TCL_ERROR;
	    goto done;
    	}
        nPhotosInBatch = nPhotos;
    } else {
        nPhotosInBatch = 1;
    }
    if (!useStandardCompress) {
        err = GetMaxCompressionSize( pixels, 
				    &myRect, 
				    colorDepth,
				    spatialQuality, 
				    codecType, 
				    (CompressorComponent) anyCodec, 
				    &maxCompressedSize );
        maxCompressedSize *= nPhotosInBatch;
        if (err != noErr) {
	    UnlockPixels( pixels );
	    CheckAndSetErrorResult( interp, err );
	    result = TCL_ERROR;
	    goto done;
        }
    	compressedData = MySafeNewHandle( maxCompressedSize, 0 );
      	if (osErr != noErr) {
    	    panic( "Out of memory in AddImagesToMovie" );
      	}     
	
        MoveHHi( compressedData );
        HLock( compressedData );
#if TARGET_API_MAC_CARBON
        compressedDataPtr = *compressedData;
#else
        compressedDataPtr = StripAddress( *compressedData );
#endif
        nextDataPtr = compressedDataPtr;
    }
    if (useStandardCompress) {
        err = OpenADefaultComponent( StandardCompressionType,
				    StandardCompressionSubType, &ci );
       	if (err != noErr) {
	    result = TCL_ERROR;
	    CheckAndSetErrorResult( interp, err );
	    goto done;
       	}     
	
        /*
         * If setting defaults of the compressor cmponent like this, we
         * can avoid having the dialog displayed.
         * Thanks to Kevin Marks at Apple for this one.
         */
	
        if (!showDialog) {      /* not sure about this... */
            SCSpatialSettings       spatSet;
            SCTemporalSettings      tempSet;
            SCDataRateSettings      rateSet;
	    
            SCGetInfo( ci, scSpatialSettingsType, &spatSet );
            SCGetInfo( ci, scTemporalSettingsType, &tempSet );
            SCGetInfo( ci, scDataRateSettingsType, &rateSet );
	    
            spatSet.codecType = codecType;
            spatSet.codec = anyCodec;
            spatSet.depth = colorDepth;
            spatSet.spatialQuality = spatialQuality;
            tempSet.temporalQuality = temporalQuality;
            tempSet.keyFrameRate = keyFrameRate;
            rateSet.minSpatialQuality = spatialQuality;
            rateSet.minTemporalQuality = temporalQuality;
	    
            SCSetInfo( ci, scSpatialSettingsType, &spatSet );
            SCSetInfo( ci, scTemporalSettingsType, &tempSet );
            SCSetInfo( ci, scDataRateSettingsType, &rateSet );
        }
    }   
    
    /*
     * Loop over all photos we have got.
     */
    
    for (iphoto = 0; iphoto < nPhotos; iphoto++) {
	
        Tk_PhotoGetSize( photoList[iphoto], &right, &bottom );
        if ((right != myRect.right) && (bottom != myRect.bottom)) {
	    resultObjPtr = Tcl_GetObjResult( interp );
	    Tcl_AppendObjToObj( resultObjPtr, 
			       Tcl_NewStringObj( "image number ", -1 ) );
	    Tcl_AppendObjToObj( resultObjPtr, Tcl_NewIntObj( iphoto + 1 ) );
	    Tcl_AppendObjToObj( resultObjPtr, 
			       Tcl_NewStringObj( " has a different size than previous", -1 ) );
	    Tcl_SetObjResult( interp, resultObjPtr );
	    result = TCL_ERROR;
	    goto done;
        }
    	Tk_PhotoGetImage( photoList[iphoto], &photoBlock );
    	
        /* 
         * Copy the pixels to the gworld.
         * The Mac pixmap stores them as "undefined, red, gree, blue", but tk 8.3 stores them
         * as "red, green, blue, alpha (transparency)".
         */
	
    	for (i = 0; i < photoBlock.height; i++) {
	    pixelPtr = (unsigned char *) 
	    (GetPixBaseAddr(pixels) + i * (0x3FFF & ((*pixels)->rowBytes)));
	    photoPixels = photoBlock.pixelPtr + i * photoBlock.pitch;
	    for (j  =0; j < photoBlock.width; j++) {
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
         * There are four possibilities here: with or without dialog, 
         * and single image or sequence of images.
         */
	
    	if (useStandardCompress || showDialog) {
            if (nPhotos == 1) {
            	if (showDialog) {
                    SCSetTestImagePixMap( ci, pixels, NULL, scPreferCropping );
                    InstallExtendedSCCallbackProcedures( ci, (long) interp );
                    SCDefaultPixMapSettings( ci, pixels, false );
                    err = SCRequestImageSettings( ci );
                    if (err == scUserCancelled) {
			
                        /* Perhaps we should return something here and set the error code */ 	
			goto done;
                    } else if (err != noErr) {
			result = TCL_ERROR;
			CheckAndSetErrorResult( interp, err );
			goto done;
		    }     
                }
            	err = SCCompressImage( ci,
				      pixels, 
				      &myRect, 
				      &imageDesc,
				      &compressedData );
            } else {
		
                /*
                 * Sequence.
                 */
		
                if (iphoto == 0) {
		    if (showDialog) {
                        SCSetTestImagePixMap( ci, pixels, NULL, scPreferCropping );
                        InstallExtendedSCCallbackProcedures( ci, (long) interp );
                        SCDefaultPixMapSettings( ci, pixels, true );
                        err = SCRequestSequenceSettings( ci );
                        if (err == scUserCancelled) {
			    
                            /* Perhaps we should return something here and set the error code */  	
			    goto done;
                        } else if (err != noErr) {
			    result = TCL_ERROR;
			    CheckAndSetErrorResult( interp, err );
			    goto done;
                       	}     
                    }
                    err = SCCompressSequenceBegin( ci, pixels, &myRect, &imageDesc );
		    if (err != noErr) {
			result = TCL_ERROR;
			CheckAndSetErrorResult( interp, err );
			goto done;
		    }     
                }
                err = SCCompressSequenceFrame( ci, pixels, &myRect, 
					      &compressedData, 
					      &dataSize, 
					      &syncFlag );
                (**imageDesc).dataSize = dataSize;
            }
    	} else {                    /* dialogless, old compress */
	    
            if (nPhotos == 1) {
		err = CompressImage( pixels,  
				    &myRect, 
				    spatialQuality, 
				    codecType, 
				    imageDesc, 
				    compressedDataPtr );
            } else {
                err = CompressSequenceFrame( sequenceID,
					    pixels,
					    &myRect,
					    codecFlagUpdatePrevious,
					    nextDataPtr,
					    &dataSize,
					    NULL,
					    NULL );
                nextDataPtr += dataSize;
            }
        }  	
        if (err != noErr) {      
	    result = TCL_ERROR;
	    CheckAndSetErrorResult( interp, err );
	    goto done;
       	}
	if ((nPhotosInBatch == 1) || (iphoto = nPhotosInBatch - 1)) {
	    err = AddMediaSample( myMedia, 
				 compressedData, 
				 0,                   		/* no offset in data */
				 (**imageDesc).dataSize,
				 durationPerFrame,         	/* frame duration */
				 (SampleDescriptionHandle) imageDesc, 
				 nPhotosInBatch,          	/* one sample or the complete sequence */
				 syncFlag,                	/* key frame or not */
				 &thisSampleTime );
        }
	
        /*
         * Important that we return the media position where the first frame was stored!
         */
	
        if (iphoto == 0) {
            *sampleTimePtr = thisSampleTime;
        }
    	if (err != noErr) {
	    result = TCL_ERROR;
	    CheckAndSetErrorResult( interp, err );
	    goto done;
    	}
    } /* end loop over photos */
    
done:
    
    UnlockPixels(pixels);
    SetGWorld(oldPort, oldGDeviceH);
    if (nPhotos > 1) {
        if (useStandardCompress || showDialog) {
	    
            /* disposes imageDesc and compressedData handles */
            SCCompressSequenceEnd(ci);
        } else {
	    CDSequenceEnd(sequenceID);
        }
    }
    if (imageDesc) {
	DisposeHandle((Handle) imageDesc);
    }
    if (compressedData) {
	DisposeHandle(compressedData);
    }
    if (ci) {
        CloseComponent(ci);
    }
    if (theGWorld) {
	DisposeGWorld(theGWorld);
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * AddImageFileToMovie --
 *
 *		Add images to a media with a duration of duration.
 *
 * Results:
 *		Normal Tcl
 *
 * Side effects:
 *		Memory allocated, any error messages in the interpreter.
 *
 *----------------------------------------------------------------------
 */

int 
AddImageFileToMovie( Tcl_Interp *interp, 
		    Media myMedia,              /* the media (in) */
		    TimeValue duration,         /* the duration of one frame in media's time scale (in) */
		    char *fileName,             /* file name (in) */
		    long optFlags,              /* flags specifying options set (in) */
		    int showDialog,             /* show dialog ? (in) */
		    CodecType codecType,        /* the compressor type to use (in) */
		    CodecQ spatialQuality,
		    CodecQ temporalQuality,
		    short colorDepth,
		    long keyFrameRate,
		    TimeValue *sampleTimePtr )  /* returns the position where the first sample was inserted
 * in the media, in media's time (out) */
{
    GWorldPtr 		        theGWorld = NULL;
    PixMapHandle 	        pixels = NULL;
    ComponentResult         err = noErr;
    ComponentInstance       gi = NULL;
    ComponentInstance       ci = NULL;
    ImageDescriptionHandle  srcImageDesc = NULL;
    ImageDescriptionHandle  dstImageDesc = NULL;
    Handle 			        compressedData = NULL;
    Rect                    bounds;
    SCSpatialSettings       spatSet;
    SCTemporalSettings      tempSet;
    SCDataRateSettings      rateSet;
    FSSpec 			        fss;
    CTabHandle              colTabHandle = NULL;
    short                   pixelDepth;
    int 			        result = TCL_OK;
    
    err = QTTclNativePathNameToFSSpec( interp, fileName, &fss );
    if (err != noErr) {
	Tcl_SetObjResult( interp, Tcl_NewStringObj( 
						   "Failed making FSSpec from filename", -1 ) );
	result = TCL_ERROR;
	goto done;
    } 
    
    /* This can be slow :-( */
    err = GetGraphicsImporterForFile( &fss, &gi );
    if (err != noErr) {
    	result = TCL_ERROR;
       	CheckAndSetErrorResult( interp, err );
    	goto done;
    }
    err = GraphicsImportGetImageDescription( gi, &srcImageDesc );
    if (err != noErr) {
    	result = TCL_ERROR;
       	CheckAndSetErrorResult( interp, err );
    	goto done;
    }
    err = GraphicsImportGetNaturalBounds( gi, &bounds );
    if (err != noErr) {
    	result = TCL_ERROR;
       	CheckAndSetErrorResult( interp, err );
    	goto done;
    }
    
    /*
     * Try to be as faithful as possible to the original image. Depth and color table.
     */
    
    if ((**srcImageDesc).depth > 32) {
        pixelDepth = (**srcImageDesc).depth - 32;
    } else {
        pixelDepth = (**srcImageDesc).depth;
    }
    if ((**srcImageDesc).clutID == 0) {
        err = GetImageDescriptionCTable( srcImageDesc, &colTabHandle );
        if (err != noErr) {
	    result = TCL_ERROR;
	    CheckAndSetErrorResult( interp, err );
	    goto done;
        }
        if (((**colTabHandle).ctSize == 0) || ((**colTabHandle).ctSeed < 0)) {
	    if (colTabHandle) {
		DisposeCTable(colTabHandle);
	    }
	    colTabHandle = NULL;
        }
    }
    err = QTNewGWorld( &theGWorld, pixelDepth, &bounds, colTabHandle, NULL, 
		      kICMTempThenAppMemory );
    if (err != noErr) {
    	CheckAndSetErrorResult( interp, err );
	result = TCL_ERROR;
	goto done;
    }
    GraphicsImportSetGWorld( gi, theGWorld, NULL );
    err = GraphicsImportDraw( gi );
    if (err != noErr) {
    	CheckAndSetErrorResult( interp, err );
	result = TCL_ERROR;
	goto done;
    }
    
    /*
     * Time to compress the thing. Set defaults from pixmap and image description.
     */
    
    err = OpenADefaultComponent( StandardCompressionType, StandardCompressionSubType, &ci );
    if (err != noErr) {
    	CheckAndSetErrorResult( interp, err );
	result = TCL_ERROR;
	goto done;
    }
    pixels = GetGWorldPixMap( theGWorld );
    LockPixels( pixels );
    SCDefaultPixMapSettings( ci, pixels, false );
    
    SCGetInfo( ci, scSpatialSettingsType, &spatSet );
    SCGetInfo( ci, scTemporalSettingsType, &tempSet );
    SCGetInfo( ci, scDataRateSettingsType, &rateSet );
    
    spatSet.codecType = (**srcImageDesc).cType;
    spatSet.spatialQuality = (**srcImageDesc).spatialQuality;
    
    /*
     * Override options from file with any command line options.
     */
    
    if (optFlags & kCompressFlagColorDepth) {
        spatSet.depth = colorDepth;
    }
    if (optFlags & kCompressFlagCompressor) {
        spatSet.codecType = codecType;
    }
    if (optFlags & kCompressFlagKeyFrameRate) {
        tempSet.keyFrameRate = keyFrameRate;
    }
    if (optFlags & kCompressFlagSpatialQuality) {
        spatSet.spatialQuality = spatialQuality;
    }
    if (optFlags & kCompressFlagTemporalQuality) {
        tempSet.temporalQuality = temporalQuality;
    }
    SCSetInfo( ci, scSpatialSettingsType, &spatSet );
    SCSetInfo( ci, scTemporalSettingsType, &tempSet );
    SCSetInfo( ci, scDataRateSettingsType, &rateSet );
    
    if (showDialog) {
        SCSetTestImagePixMap( ci, pixels, NULL, scPreferCropping );
        InstallExtendedSCCallbackProcedures( ci, (long) interp );
        SCDefaultPixMapSettings( ci, pixels, true );
        err = SCRequestSequenceSettings( ci );
        if (err == scUserCancelled) {
	    
            /* Perhaps we should return something here and set the error code??? */  	
            goto done;
        } else if (err != noErr) {
	    result = TCL_ERROR;
	    CheckAndSetErrorResult( interp, err );
	    goto done;
        }
    }     
    err = SCCompressImage( ci,
			  pixels, 
			  &bounds, 
			  &dstImageDesc,
			  &compressedData );
    if (err != noErr) {
    	result = TCL_ERROR;
       	CheckAndSetErrorResult( interp, err );
    	goto done;
    }
    err = AddMediaSample( myMedia, 
			 compressedData, 
			 0,                            	/* no offset in data */
			 (**dstImageDesc).dataSize,
			 duration,                      	/* frame duration */
			 (SampleDescriptionHandle) dstImageDesc, 
			 1,                           	/* one sample or the complete sequence */
			 0,                            	/* key frame or not */
			 sampleTimePtr );            
    if (err != noErr) {
    	result = TCL_ERROR;
       	CheckAndSetErrorResult( interp, err );
    	goto done;
    }
    
    
done:
    
    if (gi) {
        CloseComponent(gi);
    }
    if (ci) {
        CloseComponent(ci);
    }
    if (srcImageDesc) {
	DisposeHandle((Handle) srcImageDesc);
    }
    if (dstImageDesc) {
	DisposeHandle((Handle) dstImageDesc);
    }
    if (colTabHandle) {
	DisposeCTable(colTabHandle);
    }
    if (compressedData) {
	DisposeHandle(compressedData);
    }
    if (theGWorld) {
	DisposeGWorld(theGWorld);
    }
    return result;
}   

/*
 *----------------------------------------------------------------------
 *
 * MoviePlayerEventProc --
 *
 *		Deal with Movie Events, these events are TCL events. 
 *  	Update (Expose) events for a movie with controller are dealt 
 *		with in MCIsPlayerEvent in 'MoviePlayerMacEvent'.
 *
 * Results:
 *		None.
 *
 * Side effects:
 *		Depends on event.
 *
 *----------------------------------------------------------------------
 */

static void
MoviePlayerEventProc(ClientData clientData, XEvent *eventPtr) 
{
    MoviePlayer *movPtr = (MoviePlayer *) clientData;
    
    QTTclDebugPrintf( movPtr->interp, 2, 
		     "MoviePlayerEventProc: type=%d, flags=%1d%1d%1d%1d%1d%1d",
		     eventPtr->type, (movPtr->flags >> 5) & 0x1, (movPtr->flags >> 4) & 0x1, 
		     (movPtr->flags >> 3) & 0x1, (movPtr->flags >> 2) & 0x1,
		     (movPtr->flags >> 1) & 0x1, (movPtr->flags >> 0) & 0x1 );
    
    if ((eventPtr->type == Expose) && (eventPtr->xexpose.count == 0)) {
	movPtr->flags |= UPDATEMOVIE;
	goto redraw;
    } else if (eventPtr->type == ConfigureNotify) {
	goto redraw;
    } else if (eventPtr->type == MapNotify) {
	
#if TARGET_API_MAC_CARBON
        /*
         * Is this the right place to install the Carbon window event handler?
         */
	
        InstallCarbonWindowEventHandler( movPtr );
#endif
	movPtr->flags |= NEWGWORLD;
	goto redraw;
    } else if (eventPtr->type == UnmapNotify) {
	
	/* 
	 * We do not want to serve it anymore; if async loaded it still gets served,
	 * but what about the GWorld?
	 */
	
	movPtr->state &= ~kQTTclMovieStateOnDisplay;
	movPtr->grafPtr = NULL;
	return;
    } else if (eventPtr->type == FocusIn) {
        movPtr->flags |= GOT_FOCUS;
	goto redraw;
    } else if (eventPtr->type == FocusOut) {
        movPtr->flags &= ~GOT_FOCUS;
	goto redraw;
    } else if (eventPtr->type == DestroyNotify) {
        DestroyMovie( movPtr );
    } 
    return;
    
redraw:
    
    if (!(movPtr->flags & MOVIE_DELETED) && !(movPtr->flags & REDRAW_PENDING)) {
	Tcl_DoWhenIdle( DisplayMovie, (ClientData) movPtr );
	movPtr->flags |= REDRAW_PENDING;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * MoviePlayerDeletedProc --
 *
 *		Deletes a MoviePlayer widget.
 *
 * Results:
 *		None.
 *
 * Side effects:
 *		DestroyMovie implicitly called.
 *
 *----------------------------------------------------------------------
 */

static void
MoviePlayerDeletedProc( ClientData clientData ) 
{
    MoviePlayer 	*movPtr = (MoviePlayer *) clientData;	
    
    /*
     * This procedure could be invoked either because the window was
     * destroyed and the command was then deleted or because the command
     * was deleted, and then this procedure destroys the widget.  The
     * MOVIE_DELETED flag distinguishes these cases.
     */
    
    if (!(movPtr->flags & MOVIE_DELETED)) {
	Tk_DestroyWindow( movPtr->tkwin );
    }
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyMovie --
 *
 *		Deletes a MoviePlayer Widget.  Most things cleaned up with 
 *		Tk_FreeOptions but some things are freed up by me.
 *
 * Results:
 *		None.
 *
 * Side effects:
 *		Schedules or calls directly MoviePlayerFree to free all resources.
 *
 *----------------------------------------------------------------------
 */

static void
DestroyMovie( MoviePlayer *movPtr ) 
{
    QTTclDebugPrintf( movPtr->interp, 2, "DestroyMovie: movPtr=%d", movPtr );
    movPtr->flags |= MOVIE_DELETED;
    
#if TARGET_API_MAC_CARBON
    MovieDestroyCarbonWindowHandlerCleanup( movPtr );
#endif
    
    if (movPtr->flags & REDRAW_PENDING) {
        Tcl_CancelIdleCall( DisplayMovie, (ClientData) movPtr );
    }
    Tcl_DeleteCommandFromToken( movPtr->interp, movPtr->widgetCmd );
    
    /*
     * Remove from the display list.
     */
    
    RemoveMovieFromOpenMovies( movPtr );
    
    /*
     * This is the "protected" free procedure. Anything Tcl_Preserve'ed
     * is not freed until Tcl_Release'ed.
     */
    
    Tcl_EventuallyFree( (ClientData) movPtr, MoviePlayerFree );
}

/*
 *----------------------------------------------------------------------
 *
 * MoviePlayerFree --
 *
 *		Does the real job of freeing memory, Movie, and MovieController.
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
MoviePlayerFree( char *clientData )
{
    MoviePlayer 	*movPtr = (MoviePlayer *) clientData;
    int     		i;
    
    QTTclDebugPrintf( movPtr->interp, 2, "MoviePlayerFree: movPtr=%d", movPtr );
    
    if (movPtr->trackSelect != NULL) {
	ckfree( (char *) movPtr->trackSelect );
	movPtr->trackSelect = NULL;
    }
    
    /* Delete and free any async load async callbacks. */
    AsyncLoadFree( movPtr );
    
    if (movPtr->aMovie != NULL) {
	for (i = 0; i < movPtr->undoCount; i++) {
	    DisposeMovieEditState( movPtr->editStates[i] );
	}
	movPtr->undoCount = 0;
	if (movPtr->editStates) {
	    ckfree( (char *) movPtr->editStates );
	}
	CallBackFree( movPtr );
	
	/*
	 * If called from inside the -mccommand proc we destroy the actual movie at idle.
	 */
	
        if (movPtr->insideMCCommand) {
            movPtr->tmpMovieRecordPtr = (TmpMovieRecord *) ckalloc( sizeof(TmpMovieRecord) );
            movPtr->tmpMovieRecordPtr->movie = movPtr->aMovie;
            movPtr->tmpMovieRecordPtr->movieController = movPtr->aController;
    	    SetMovieGWorld( movPtr->aMovie, gOffscreenGWorldPtr, NULL );
            movPtr->aMovie = NULL;
            movPtr->aController = NULL;
        } else {
            if (movPtr->aController != NULL) {
                DisposeMovieController( movPtr->aController );
		movPtr->aController = NULL;
	    }
	    if (movPtr->aMovie != NULL) {
            	QTTclDebugPrintf( movPtr->interp, 2, "\taMovie=%d", movPtr->aMovie );
		DisposeMovie( movPtr->aMovie ); 
		movPtr->aMovie = NULL;
                gMovieRefCount--;
	    }
        }
    }
    
#ifdef _WIN32
    if (movPtr->hwnd != NULL) {
        /* There have been a previous crash problem from DestroyPortAssociation
         * but that was likely due to ExitMovies could be called before this. */
	/* IMPORTANT: Must be after movie is disposed!!! */
    	DestroyPortAssociation( (CGrafPtr) GetHWNDPort(movPtr->hwnd) );
        movPtr->hwnd = NULL;
    }
#endif
    
    Tk_FreeConfigOptions( (char *) movPtr, movPtr->optionTable, movPtr->tkwin );
    ckfree( (char *) movPtr );
    
    /*
     * If this is the last movie destroyed and the exit handler
     * has been called and not yet freed it must be done now.
     */
    if ((gExitState == kQTTclExitStateExitHandlerCalled) && (gMovieRefCount <= 0)) {
        ExitMoviePlayer( NULL );
    }
}

/*
 *----------------------------------------------------------------------
 *
 * DisposeMovieAtIdle --
 *
 * 		When doing 'configure -file' from the '-mccommand' tcl proc it 
 *		crashes for QTVR movies if not disposing movie at idle.
 *
 * Results:
 *		None.
 *
 * Side effects:
 *		Movie and controller disposed.
 *
 *----------------------------------------------------------------------
 */

static void
DisposeMovieAtIdle( ClientData clientData ) 
{
    MoviePlayer 	*movPtr = (MoviePlayer *) clientData;
    
    QTTclDebugPrintf( movPtr->interp, 2, "DisposeMovieAtIdle" );
    
    if (gExitState > 0) {
        return;
    }
    if (movPtr->tmpMovieRecordPtr == NULL) {
        return;
    }
    if (movPtr->tmpMovieRecordPtr->movieController != NULL) {
        DisposeMovieController( movPtr->tmpMovieRecordPtr->movieController );
    }
    if (movPtr->tmpMovieRecordPtr->movie != NULL) {
    	QTTclDebugPrintf( movPtr->interp, 2, "\tmovPtr->tmpMovieRecordPtr->movie=%d", 
			 movPtr->tmpMovieRecordPtr->movie );
	DisposeMovie( movPtr->tmpMovieRecordPtr->movie );
        gMovieRefCount--;
    }
    ckfree((char *) movPtr->tmpMovieRecordPtr);
    movPtr->tmpMovieRecordPtr = NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * MoviePlayerWorldChanged --
 *
 *		Something changed, arange for the movie to be redisplayed. 
 *  	Find its geometry including any movie controller, and request 
 *		this size from Tk.
 *
 * Results:
 *		None.
 *
 * Side effects:
 *		Movie widget displayed sometime in the future.
 *
 *----------------------------------------------------------------------
 */

void
MoviePlayerWorldChanged (ClientData instanceData) 
{
    MoviePlayer     *movPtr = (MoviePlayer *) instanceData;
    Rect            aRect;
    
    QTTclDebugPrintf( movPtr->interp, 2, "MoviePlayerWorldChanged" );
    {
        GWorldPtr		    destPort = NULL;
	
        GetMovieGWorld( movPtr->aMovie, &destPort, NULL );
        QTTclDebugPrintf( movPtr->interp, 4, "\tGetMovieGWorld=0x%.8x", destPort );
    }
    if (movPtr->tkwin && Tk_IsMapped(movPtr->tkwin) && !(movPtr->flags & REDRAW_PENDING)) {
	Tcl_DoWhenIdle( DisplayMovie, (ClientData) movPtr );
	movPtr->flags |= REDRAW_PENDING;
    }
    
    if (movPtr->aMovie == NULL) { 
	return;
    }
    
    /*
     * Cache info about visual or not; Windows need to serve unmapped audio only movies.
     */
    
    if (GetMovieIndTrackType( movPtr->aMovie, 1, VisualMediaCharacteristic, 
			     movieTrackCharacteristic ) == NULL) {
	movPtr->isVisual = 0;
    } else {
	movPtr->isVisual = 1;
    }
    
    if ((movPtr->state & kQTTclMovieStateAsyncLoading) && 
	(movPtr->loadState < kMovieLoadStatePlayable)) {
	
	/*
	 * During the first stage of async loading we give it a default size (compare frame).
	 */
	
	Tk_GeometryRequest( movPtr->tkwin, 1, 1 );
	Tk_SetInternalBorder( movPtr->tkwin, 0 );
	
    } else {
	
	/*
	 * It may happen that a movie controller is not added in 'ConfigureMoviePlayer',
	 * for instance if we start creating an empty movie with 'Movie .m', and then
	 * pastes data into it.
	 */
	
	AddOrRemoveMovieController( movPtr );
	
	/* 
	 * Get the total size requested, with or without a controller.
	 */
	
	aRect.left = 0;
	aRect.top = 0;
	aRect.right = movPtr->mwidth;
	aRect.bottom = movPtr->mheight;
	if (movPtr->wantController && movPtr->aController != NULL) {
	    MCGetControllerBoundsRect( movPtr->aController, &aRect );
	} else if (movPtr->aMovie != NULL) {		// testing...
	    GetMovieBox( movPtr->aMovie, &aRect );
	}
	MacOffsetRect( &aRect, (short) -aRect.left, (short) -aRect.top );
	
	QTTclDebugPrintf( movPtr->interp, 2, "\taRect.right=%d, aRect.bottom=%d", 
			 aRect.right, aRect.bottom );
	
	/*
	 * If any requested particular size (different from the default 0), 
	 * adjust the rectangle size here.
	 */
	
	if (movPtr->width) {
	    aRect.right = movPtr->width;
	} 
	if (movPtr->height) {
	    aRect.bottom = movPtr->height;
	} 
	
	/*
	 * What if any ->width or ->height equal to zero? Get natural size instead!
	 */
	
	if (movPtr->width == 0) {
	    if (movPtr->wantController && movPtr->aController != NULL) {
		if (movPtr->mwidth == 0) {
		    aRect.right = CONTROLLER_WIDTH;		/* bad!!! */
		} else {
		    aRect.right = movPtr->mwidth;
		}
	    } else {
		aRect.right = movPtr->mwidth;
	    }
	} 
	if (movPtr->height == 0) {
	    if (movPtr->wantController && movPtr->aController != NULL) {
		aRect.bottom = movPtr->mheight + movPtr->controllerHeight;
	    } else {
		aRect.bottom = movPtr->mheight;
	    }
	} 
	
	/* Padding not supported for the moment. */
	aRect.right += 2 * movPtr->padx + 2 * movPtr->highlightWidth;
	aRect.bottom += 2 * movPtr->pady + 2 * movPtr->highlightWidth;
	
	Tk_GeometryRequest( movPtr->tkwin, aRect.right, aRect.bottom );
	//Tk_SetInternalBorder( movPtr->tkwin, movPtr->highlightWidth );	
	movPtr->flags |= NEWGWORLD;
	
	QTTclDebugPrintf( movPtr->interp, 2, "\treq size width=%d, height=%d", 
			 aRect.right, aRect.bottom );
    }   // end if not kQTTclMovieStateAsyncLoading
}

/*
 *----------------------------------------------------------------------
 *
 * CreateMoviePlayer --
 *
 *		Create a MoviePlayer Object.
 *
 * Results:
 *		Return pointer to movieplayer object
 *
 * Side effects:
 *		Memory allocated
 *
 *----------------------------------------------------------------------
 */

static MoviePlayer * 
CreateMoviePlayer( Tk_Window tkwin ) 
{
    MoviePlayer *movPtr;
    
    movPtr = (MoviePlayer *) ckalloc( sizeof(MoviePlayer) );
    memset( (void *) movPtr, 0, sizeof(MoviePlayer) );
    movPtr->trackSelect = (TrackSelection *) ckalloc( sizeof(TrackSelection) );
    return movPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * AddOrRemoveMovieController --
 *
 *  	Attach any controller. Many of the configure options need a 
 *		controller to work, and therefore we cannot wait to make the 
 *		controller until it is displayed.
 *
 * Results:
 *		None
 *
 * Side effects:
 *		Controller created and attached, or disposed for the 
 *		corresponding movie.
 *
 *----------------------------------------------------------------------
 */

static void
AddOrRemoveMovieController( MoviePlayer *movPtr )
{
    Rect    aRect;
    
    if (movPtr->aMovie != NULL) {
    	if (!movPtr->wantController && (movPtr->aController != NULL)) {
    	    DisposeMovieController( movPtr->aController );
	    movPtr->aController = NULL;
	    movPtr->flags |= NEWGWORLD;
	    movPtr->controllerHeight = 0;
        }
    	if (movPtr->wantController && (movPtr->aController == NULL)) {
            GWorldPtr		    theGWorldPtr = NULL;
            
            /*
             * It appears to be a bug on Mac OS X that creating the controller
             * resets the movies GWorld to current port. Added workaround.
             */
    	    
            GetMovieBox( movPtr->aMovie, &aRect );
            GetMovieGWorld( movPtr->aMovie, &theGWorldPtr, NULL );
	    movPtr->aController  = NewMovieController( movPtr->aMovie, &aRect, 
						      mcTopLeftMovie );
            MCSetControllerPort( movPtr->aController, theGWorldPtr );	
	    if (movPtr->mcEdit) {
		MCEnableEditing( movPtr->aController, true );
	    }
	    movPtr->controllerHeight = GetControllerBarHeight( movPtr->aController );
	    movPtr->flags |= NEWGWORLD;
        }
    }
}

/*
 *----------------------------------------------------------------------
 *
 * AddMovieToOpenMovies --
 *
 *  	Add this movie to the global list of open movies.
 *
 * Results:
 *		None
 *
 * Side effects:
 *		Global movies list updated.
 *
 *----------------------------------------------------------------------
 */

static void
AddMovieToOpenMovies( MoviePlayer *movPtr )
{
    MoviePlayerList     *movListPtr = NULL;
    
#if TARGET_API_MAC_CARBON
    /* 
     *  If this is the first movie we open on Carbon, be sure to set a timer
     *  to serve it and all that may follow.
     */
    
    if (gCarbonMovieTimerRef == NULL) {
    	InstallMovieIdlingEventLoopTimer();
    }
#endif	 
    
    QTTclDebugPrintf( movPtr->interp, 2, "AddMovieToOpenMovies: movPtr=%d", movPtr );
    
    /*
     * Search to see if already in our list. If not, then add it.
     * gMoviePlayerListPtr always points to the first node, or NULL if none.
     */
    
    movListPtr = gMoviePlayerListPtr;
    while ((movListPtr != NULL) && (movPtr != movListPtr->movPtr)) {
        movListPtr = movListPtr->next;
    }
    if (movListPtr == NULL) {
    	MoviePlayerList     *newMovListPtr = NULL;
	
        /* Add as first in list (simplest!). */        
        newMovListPtr = (MoviePlayerList *) ckalloc( sizeof(MoviePlayerList) );
        newMovListPtr->movPtr = movPtr;
        newMovListPtr->prev = NULL;
        newMovListPtr->next = gMoviePlayerListPtr;
        if (gMoviePlayerListPtr != NULL) {
            gMoviePlayerListPtr->prev = newMovListPtr;
        }
        gMoviePlayerListPtr = newMovListPtr;
    }	
}

static void
RemoveMovieFromOpenMovies( MoviePlayer *movPtr )
{
    MoviePlayerList     *movListPtr = NULL;
    
    QTTclDebugPrintf( movPtr->interp, 2, "RemoveMovieFromOpenMovies: movPtr=%d", movPtr );
    
    /*
     * Search to see if in our list. If then remove it.
     */
    
    movListPtr = gMoviePlayerListPtr;
    while ((movListPtr != NULL) && (movPtr != movListPtr->movPtr)) {
        movListPtr = movListPtr->next;
    }
    if (movListPtr != NULL) {
	if (movListPtr->prev != NULL) {
	    movListPtr->prev->next = movListPtr->next;
	} else {
	    gMoviePlayerListPtr = movListPtr->next;
	}
	if (movListPtr->next != NULL) {
	    movListPtr->next->prev = movListPtr->prev;
	}
	ckfree( (char *) movListPtr );
    }
    
#if TARGET_API_MAC_CARBON
    /* 
     *  If this is the last movie we have on Carbon, be sure to remove the timer.
     */
    
    if (gMoviePlayerListPtr == NULL) {
    	QTUninstallNextTaskNeededSoonerCallback( 
						NewQTNextTaskNeededSoonerCallbackUPP( TaskNeededSoonerCallback ),
						(void *) gCarbonMovieTimerRef );
        RemoveEventLoopTimer( gCarbonMovieTimerRef );
        gCarbonMovieTimerRef = NULL;
    }
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * DisplayMovie --
 *
 *		Display a movie.
 *
 * Results:
 *		None.
 *
 * Side effects:
 *		With luck, the movie is displayed
 *
 *----------------------------------------------------------------------
 */

static void
DisplayMovie( ClientData clientData ) 
{
    MoviePlayer         *movPtr = (MoviePlayer *) clientData;
    Tk_Window		tkwin = movPtr->tkwin;
    CGrafPtr 	        saveWorld = NULL;
    GDHandle 	        saveDevice = NULL;
    GWorldPtr           theGWorldPtr = NULL;
    Rect 		aRect;
    static RgnHandle    region = NULL;
    static RgnHandle    saveRegion = NULL;
    RgnHandle           regionHand = NULL;
    
    /* The first time we allocate a region to help us with update events. */	
    if (region == NULL) {
	region = NewRgn();
    }
    if (saveRegion == NULL) {
	saveRegion = NewRgn();
    }
    QTTclDebugPrintf( movPtr->interp, 2, 
		     "DisplayMovie: flags=%1d%1d%1d%1d%d%d",
		     (movPtr->flags >> 5) & 0x1, (movPtr->flags >> 4) & 0x1,
		     (movPtr->flags >> 3) & 0x1, (movPtr->flags >> 2) & 0x1,
		     (movPtr->flags >> 1) & 0x1, (movPtr->flags >> 0) & 0x1 );
    
    /* 
     * Are we ready to display. Clear the redraw pending state. 
     */
    
    movPtr->flags &= ~REDRAW_PENDING;
    if ((movPtr->flags & MOVIE_DELETED) || !Tk_IsMapped(tkwin)) {
	return;
    }
    if ((movPtr->state & kQTTclMovieStateAsyncLoading) &&
	(movPtr->loadState < kMovieLoadStatePlayable)) {
	
	/* If we are async loading and not yet playable. */
	return;
    }    
    GetGWorld( &saveWorld, &saveDevice );		
    
    /*
     * Tk drawing: Draw the focus highlight if any.
     */
    
    if (movPtr->highlightWidth > 0) {
        GC 		fgGC, bgGC;
        Pixmap	pixmap;
	
        pixmap = (Pixmap) Tk_WindowId(tkwin);
        bgGC = Tk_GCForColor( movPtr->highlightBgColorPtr, pixmap );
        if (movPtr->flags & GOT_FOCUS) {
            fgGC = Tk_GCForColor( movPtr->highlightColorPtr, pixmap );
            TkpDrawHighlightBorder( tkwin, fgGC, bgGC, movPtr->highlightWidth, pixmap );
        } else {
            TkpDrawHighlightBorder( tkwin, bgGC, bgGC, movPtr->highlightWidth, pixmap );
        }
    }
    
    if (movPtr->aMovie != NULL) {
	movPtr->display->request++;
	
	theGWorldPtr = QTTclMacGetDrawablePort( Tk_WindowId( tkwin ) );
	SetGWorld( theGWorldPtr, NULL );
	GetClip( saveRegion );
	QTTclMacWinBounds( (TkWindow *) tkwin, &aRect );
	
	aRect.top += movPtr->inset;
	aRect.bottom -= movPtr->inset;
	aRect.left += movPtr->inset;
	aRect.right -= movPtr->inset;
	
	if (movPtr->flags & NEWGWORLD) {
	    
	    /*
	     * It is first when the movie is to be displayed we know which graphics
	     * port it should be associated with.
	     * The 'SetMovieGWorld' is costy so set it only if the graphics port
	     * actually changed. Bug: even if changed the address may be the same?
	     */
	    
	    if ((movPtr->grafPtr == NULL) || (movPtr->grafPtr != theGWorldPtr)) {
		SetMovieGWorld( movPtr->aMovie, theGWorldPtr, NULL );
		movPtr->grafPtr = theGWorldPtr;		
            	QTTclDebugPrintf( movPtr->interp, 3, "\tSetMovieGWorld()" );
	    }    
	    if (movPtr->aController != NULL) {
		MCSetControllerPort( movPtr->aController, theGWorldPtr );	
	    }
	    
	    /* 
	     * Be sure to set the movie active here, only to be really sure.
	     */
	    
	    SetMovieActive( movPtr->aMovie, true );
	    
	    /* 
	     * We should determine if the toplevel is an actice window or not.
	     * If not, deactivate the controller. Once this initial state is set correctly,
	     * the controller seems to get these events and set its correct state automatically.
	     */
	    
#if TARGET_OS_MAC
            QTTclDebugPrintf( movPtr->interp, 3, "\tMyIsToplevelInFront %d",
			     MyIsToplevelInFront( tkwin ) );
	    
	    if (movPtr->aController != NULL) {
                if (MyIsToplevelInFront( tkwin )) {
                    MCDoAction( movPtr->aController, mcActionActivate, NULL );
                } else {
                    MCDoAction( movPtr->aController, mcActionDeactivate, NULL );
                }
		MCMovieChanged( movPtr->aController, movPtr->aMovie );
	    }
#endif      // TARGET_OS_MAC

#if TARGET_OS_MAC	// Windows should manage this automatically through its subwindows.

	    /*
	     * Setup clipping region. Need to check in the event loop if something happened that
	     * changed the clipping region. Once a clipping region has been set it is valid
	     * for the lifetime of the movie.
	     * The clipping regions for the window and its children are
	     * marked invalid.  (Make sure they are valid before drawing.)
	     */
	    
#if TARGET_API_MAC_CARBON
            TkMacOSXInvalClipRgns( ((TkWindow *) (movPtr->tkwin))->privatePtr->winPtr->parentPtr );
#else
	    TkMacInvalClipRgns( ((TkWindow *) (movPtr->tkwin))->privatePtr->winPtr->parentPtr );
#endif
	    regionHand = QTTclMacVisableClipRgn( ((TkWindow *) (movPtr->tkwin)) );
	    if (movPtr->aController != NULL) {
		MCSetClip( movPtr->aController, regionHand, NULL );
	    } else {
		SetMovieDisplayClipRgn( movPtr->aMovie, regionHand );
	    }
#endif      // TARGET_OS_MAC

	    /*
	     * Add movie to our list of displayed movies only if it's not there already.
	     */
	    
	    AddMovieToOpenMovies( movPtr );
	    movPtr->state |= kQTTclMovieStateOnDisplay;			
	    movPtr->flags &= ~NEWGWORLD;
	}	// end NEWGWORLD
        
	if ((movPtr->flags & UPDATEMOVIE) && (movPtr->aController == NULL)) {
	    
	    /*
	     * 'MCIsPlayerEvent' takes care of update (Expose) events 
	     * via the controller.
	     * InvalidateMovieRegion is the preferred way to treat update events
	     * when there is no controller. Perhaps we could get the exact region that
	     * needs to be updated and not the complete movie rectangle.
	     */
	    
#if !TARGET_API_MAC_CARBON
            /* The update event this generates is cleared in the mac event loop. */
	    RectRgn( region, &aRect );
	    InvalidateMovieRegion( movPtr->aMovie, region );
#endif
            
#if TARGET_API_MAC_CARBON
            /* 
             * This triggers a new update event which calls Display() again in an
             * infinite loop!!! Never do this! Does not seems to be neeed anyway.
             */
            //InvalWindowRect( GetWindowFromPort(theGWorldPtr), &aRect );
#endif
	}
	movPtr->flags &= ~UPDATEMOVIE;
	
	/* 
	 * Important, set the size and position of the movie in the actual window as 
	 * indicated by TkMacWinBounds above.
	 */
	
	if (movPtr->aController != NULL) {
	    MCSetControllerBoundsRect( movPtr->aController, &aRect );			
	} else {
	    SetMovieBox( movPtr->aMovie, &aRect) ;
	}
	
	if (movPtr->flags & NEED_PREROLL) {
	    // testing... PreRoll'ing should be made as late as possible. First time here.
	    /* this crashes on Windows for QT 5
	     PrerollMovie( movPtr->aMovie, GetMovieTime( movPtr->aMovie, NULL ), 
	     GetMoviePreferredRate( movPtr->aMovie ) ); */
	    movPtr->flags &= ~NEED_PREROLL;
	}
	SetClip( saveRegion );
    }
    SetGWorld( saveWorld, saveDevice );
}

/*
 *----------------------------------------------------------------------
 *
 * SetMoviePlayerRectBox --
 *
 *		Sets the movies display rectangle in mac window.
 *
 * Results:
 *		None.
 *
 * Side effects:
 *		None.
 *
 *----------------------------------------------------------------------
 */

#if TARGET_OS_MAC
static void
SetMoviePlayerRectBox( MoviePlayer *movPtr )
{
    Rect	aRect;
    Rect	tkRect;
    
    QTTclMacWinBounds( (TkWindow *) movPtr->tkwin, &tkRect );
    tkRect.left += movPtr->inset;
    tkRect.top += movPtr->inset;
    tkRect.right -= movPtr->inset;
    tkRect.bottom -= movPtr->inset;
    if (movPtr->aController != NULL) {
        MCGetControllerBoundsRect( movPtr->aController, &aRect );
        if (!MacEqualRect( &tkRect, &aRect )) {		
            MCSetControllerBoundsRect( movPtr->aController, &tkRect );
        }
    } else {
        GetMovieBox( movPtr->aMovie, &aRect);
        if (!MacEqualRect( &tkRect, &aRect )) {		
            SetMovieBox( movPtr->aMovie, &tkRect);
        }
    }
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * InstallMovieIdlingEventLoopTimer, TaskNeededSoonerCallback,
 * 		CarbonTimerNextTime --
 *
 *		Sets up Carbon timers to serve movies.
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
InstallMovieIdlingEventLoopTimer( void )
{
    OSStatus 		err;
    
    err = InstallEventLoopTimer( GetMainEventLoop(),
				0, 											/* firedelay */
				kEventDurationMillisecond * kMinimumIdleDurationInMillis, /* interval */
				NewEventLoopTimerUPP( MoviePlayerCarbonTimer ),
				NULL, 		
				&gCarbonMovieTimerRef );
    if (err != noErr) {
	/* 
	 * Install a callback that the Idle Manager will use when
	 * QuickTime needs to wake me up immediately.
	 */
	
	err = QTInstallNextTaskNeededSoonerCallback(
						    NewQTNextTaskNeededSoonerCallbackUPP( TaskNeededSoonerCallback ),
						    1000, 			/* Millisecond timescale */
						    0, 				/* No flags */
						    (void *) gCarbonMovieTimerRef );
    }
    return err;
}

static void 
TaskNeededSoonerCallback( TimeValue duration,
			 unsigned long flags,
			 void *refcon )
{
    SetEventLoopTimerNextFireTime( (EventLoopTimerRef) refcon,
				  duration * kEventDurationMillisecond );
}

static void	
CarbonTimerNextTime( void )
{
    OSStatus		err = noErr;
    long			durationMillis;
    
    err = QTGetTimeUntilNextTask( &durationMillis, 1000 );
    if (durationMillis == 0) {
	durationMillis = kMinimumIdleDurationInMillis;
    }
    // testing...
    /* This is only a temporary workaround for missing events
     * when movie is without a controller.
     */
    durationMillis = 50;
    // end testing...
    
    SetEventLoopTimerNextFireTime( gCarbonMovieTimerRef,
				  durationMillis * kEventDurationMillisecond );
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
MoviePlayerCarbonTimer( EventLoopTimerRef theTimer, 
		       void *userData )
{
    /* 
     * The first NULL (EventRecord *) just says idle movie. 
     * The second NULL says idle all movies.
     */
    
    MoviePlayerMacEvent( NULL, NULL );
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * InstallCarbonWindowEventHandler, 
 *		MovieDestroyCarbonWindowHandlerCleanup --
 *
 *		Makes sure that there is always a Carbon window event handler.
 *		Removed when not useful anymore.
 *
 * Results:
 *		None.
 *
 * Side effects:
 *		May install or remove Carbon window event handler.	
 *
 *----------------------------------------------------------------------
 */

#if TARGET_API_MAC_CARBON
static void
InstallCarbonWindowEventHandler( MoviePlayer *movPtr )
{
    WindowRef			windowRef;		/* identical to WindowPtr */
    EventHandlerRef		handlerRef;
    OSStatus			err;
    Tcl_HashEntry   	*hashPtr = NULL;
    CarbonWindowHandlerEntry	*entryPtr;
    int					isNew;
    const EventTypeSpec kEvents[] = {
	{kEventClassWindow,		kEventWindowUpdate},
	{kEventClassWindow,		kEventWindowActivated},
	{kEventClassWindow,		kEventWindowDeactivated},
	{kEventClassMouse,		kEventMouseDown},
	{kEventClassMouse,		kEventMouseUp},
	{kEventClassMouse,		kEventMouseDragged}		/* Needed in controller,
	 * volume, and panos. */
    };
    
    /*
     * We shall keep a single carbon window event handler for each toplevel
     * window that contains a displayed movie.
     */
    
    if (gCarbonWindowHandlerHashPtr == NULL) {
        gCarbonWindowHandlerHashPtr = (Tcl_HashTable *) ckalloc( sizeof(Tcl_HashTable) );
	Tcl_InitHashTable( gCarbonWindowHandlerHashPtr, TCL_ONE_WORD_KEYS );
    }
    windowRef = GetWindowFromPort( TkMacOSXGetDrawablePort( Tk_WindowId(movPtr->tkwin)) );
    
    /*
     * If already in our hash table, just add a refCount, else create it.
     */
    
    hashPtr = Tcl_FindHashEntry( gCarbonWindowHandlerHashPtr, (char *) windowRef );
    if (hashPtr != NULL) {
	entryPtr = (CarbonWindowHandlerEntry *) Tcl_GetHashValue( hashPtr );
	entryPtr->refCount++;
	
        QTTclDebugPrintf( movPtr->interp, 2, 
			 "InstallCarbonWindowEventHandler: (exists) refCount=%d, windowRef=0x%.8x",
			 entryPtr->refCount, windowRef );
    } else {
	err = InstallWindowEventHandler( windowRef, 
					NewEventHandlerUPP( MovieCarbonWindowEventHandler ), 
					GetEventTypeCount(kEvents), 
					&kEvents, 						/* const EventTypeSpec*  */
					(void *) movPtr, &handlerRef );
	if (err == noErr) {       
            entryPtr = (CarbonWindowHandlerEntry *) ckalloc( sizeof(CarbonWindowHandlerEntry) );
            entryPtr->handlerRef = handlerRef;
            entryPtr->refCount = 1;
            hashPtr = Tcl_CreateHashEntry( gCarbonWindowHandlerHashPtr, 
					  (char *) windowRef, &isNew );
            Tcl_SetHashValue( hashPtr, entryPtr );	
        }           
        QTTclDebugPrintf( movPtr->interp, 2, 
			 "InstallCarbonWindowEventHandler: (new) refCount=%d, windowRef=0x%.8x",
			 entryPtr->refCount, windowRef );
    }
}
#endif

#if TARGET_API_MAC_CARBON
static void
MovieDestroyCarbonWindowHandlerCleanup( MoviePlayer *movPtr )
{
    WindowRef			windowRef = NULL;		/* identical to WindowPtr */
    Tcl_HashEntry   	*hashPtr = NULL;
    CarbonWindowHandlerEntry	*entryPtr;
    OSStatus			err = noErr;
    
    QTTclDebugPrintf( movPtr->interp, 2, 
		     "MovieDestroyCarbonWindowHandlerCleanup: gCarbonWindowHandlerHashPtr=%d",
		     gCarbonWindowHandlerHashPtr );
    if (gCarbonWindowHandlerHashPtr == NULL) {
        return;
    }
    if (movPtr->tkwin == NULL) {
        return;
    }
    windowRef = GetWindowFromPort( TkMacOSXGetDrawablePort( Tk_WindowId(movPtr->tkwin)) );
    if (windowRef == NULL) {
	return;
    }
    hashPtr = Tcl_FindHashEntry( gCarbonWindowHandlerHashPtr, (char *) windowRef );
    if (hashPtr == NULL) {
	return;
    }
    entryPtr = Tcl_GetHashValue( hashPtr );
    entryPtr->refCount--;
    
    QTTclDebugPrintf( movPtr->interp, 2, "\trefCount=%d, windowRef=0x%.8x",
		     entryPtr->refCount, windowRef );
    
    if (entryPtr->refCount <= 0) {
	
	/* 
	 * No more movies in this mac window; remove event handler. 
	 */
	
	err = RemoveEventHandler( entryPtr->handlerRef );		
	ckfree( (char *) entryPtr );
	Tcl_DeleteHashEntry( hashPtr );
    }
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * MovieCarbonWindowEventHandler --
 *
 *		Carbon window event handler for the Movie Player. Not related to
 *		the Carbon timers.
 *
 * Results:
 *		OSStatus if event handled or not.
 *
 * Side effects:
 *		Depending on the event.
 *
 *----------------------------------------------------------------------
 */

#if TARGET_API_MAC_CARBON
static OSStatus
MovieCarbonWindowEventHandler( EventHandlerCallRef callRef, 
			      EventRef eventRef, 
			      void *userData )
{
    MoviePlayer 		*movPtr = (MoviePlayer *) userData;
    OSStatus			returnOSStatus = eventNotHandledErr;
    EventRecord			eventRec;
    WindowRef			windowRef = NULL;
    OSStatus			err = noErr;
    
    /* Lots of temporary debug stuff. */
    UInt32				eventClass;
    UInt32				eventKind;
    char				tmp1[64];
    char				tmp2[64];
    
    QTTclDebugPrintf( movPtr->interp, 3, 
		     "MovieCarbonWindowEventHandler: movPtr->tkwin=%d", movPtr->tkwin );
    
    /* We are being destroyed. */
    if (movPtr->flags & MOVIE_DELETED) {
        return returnOSStatus;
    }
    eventClass = GetEventClass( eventRef );
    eventKind = GetEventKind( eventRef );
    memset( tmp1, 0, 64 );
    memcpy( tmp1, &eventClass, 4 );
    
    switch (eventClass) {
        case kEventClassWindow: {
	    
            err = GetEventParameter( eventRef, kEventParamDirectObject, typeWindowRef,
				    NULL, sizeof(windowRef), NULL, &windowRef ); 
	    
            switch (eventKind) {
                case kEventWindowDrawContent: strcpy( tmp2, "kEventWindowDrawContent" ); break;
                case kEventWindowUpdate: strcpy( tmp2, "kEventWindowUpdate" ); break;
                case kEventWindowActivated: strcpy( tmp2, "kEventWindowActivated" ); break;
                case kEventWindowDeactivated: strcpy( tmp2, "kEventWindowDeactivated" ); break;
            }
            break;
        }
        case kEventClassMouse: {
	    
            err = GetEventParameter( eventRef, kEventParamWindowRef, typeWindowRef,
				    NULL, sizeof(windowRef), NULL, &windowRef ); 
	    
            switch (eventKind) {
                case kEventMouseDown: strcpy( tmp2, "kEventMouseDown" ); break;
                case kEventMouseUp: strcpy( tmp2, "kEventMouseUp" ); break;
                case kEventMouseDragged: strcpy( tmp2, "kEventMouseDragged" ); break;
            }
            break;
        }
    }
    
    QTTclDebugPrintf( movPtr->interp, 3, "\twindowRef=0x%.8x  %s  %s",
		     windowRef, tmp1, tmp2 );
    
    if (err == noErr) {
	ConvertEventRefToEventRecord( eventRef, &eventRec );
    	MoviePlayerMacEvent( &eventRec, windowRef );
    }
    return returnOSStatus;   
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * MoviePlayerMacEvent --
 *
 *		Processes mac events. Gets events until none of them are used by
 *		any movies or dialogs (Mac). 
 *  	Update (Expose) events for a movie without a controller are 
 *		dealt with here.
 *
 * Results:
 *		0 if the event was not handled, 1 if it was.
 *
 * Side effects:
 *		Events processed.
 *
 *----------------------------------------------------------------------
 */

#if TARGET_OS_MAC

static int 
MoviePlayerMacEvent( 
		    EventRecord *eventPtr,			/* If NULL just idle movie. */
		    WindowRef serveWindowRef )		/* If NULL serve all movies in our list,
 * else serve only movies in this mac
 * window. */
{
    MoviePlayer 	    *movPtr;
    Movie			    theMovie = NULL;
    MoviePlayerList     *movListPtr = NULL;
    MoviePlayerList     *nextMovListPtr = NULL;
    GWorldPtr 		    saveWorld = NULL;
    GDHandle 		    saveDevice = NULL;
    GWorldPtr		    destPort = NULL;
    Rect        	    aRect;
    int         	    eventDone = false;
    int         	    clipInvalid = false;
    long    			maxMilliSecsToUse = 0;
    static long 	    counter = 0;
    static RgnHandle    saveRegion = NULL;
    static RgnHandle    region = NULL;
    RgnHandle           rgnHandle = NULL;
#if TARGET_API_MAC_CARBON
    int                 shortcutLoop = false;
#else
    /* if true we get mac events in a shortcut loop; never on Mac OS X */
    int                 shortcutLoop = true;   
#endif
    
    if (saveRegion == NULL) {
	saveRegion = NewRgn();
    }
    if (region == NULL) {
	region = NewRgn();
    }
    
    /* 
     * We keep a counter here to enable regular calls only at a fraction rate. 
     * This is used for queries to the status when a movie is loaded remotely.
     */
    
    counter++;
    
    /* 
     * Mac: process the incoming event, if movie player event, poll for new
     * events, handle them, until we get an event not aimed for us. See 'shortcutLoop'.
     * See "Inside Macintosh: QuickTime Components", p. 2-46.
     * Potentially dangerous???
     */
    
    do {
	eventDone = false;
	
	/*
	 * See if the event is meant for the effects parameter dialog box.
	 */
        
        if (eventPtr != NULL) {
            eventDone = EffectsHandleDialogEvents( eventPtr, 0 );
	}
        
	if (!eventDone) {		
	    GetGWorld( &saveWorld, &saveDevice );
	    
            movListPtr = gMoviePlayerListPtr;
            while (movListPtr != NULL) {
                movPtr = movListPtr->movPtr;
                nextMovListPtr = movListPtr->next;
		theMovie = movPtr->aMovie;
		
		if (movPtr->state & kQTTclMovieStateOnDisplay) {
                    if (movPtr->flags & MOVIE_DELETED) {
                        goto nextMovie;
                    }
		    if (serveWindowRef != NULL) {
			
			/*
			 * Serve only movies in specified mac window.
			 */
                        
			if (GetWindowFromPort( movPtr->grafPtr ) != serveWindowRef) {
			    goto nextMovie;
			}			
		    }
		    GetMovieGWorld( theMovie, &destPort, NULL );
		    SetGWorld( destPort, NULL );
		    GetClip( saveRegion );
		    
		    /* 
		     * The 'ClipRect' affects the clipping of the controller, and must
		     * encompass it to get it drawn correctly. 
		     * 'MCSetClip/SetMovieDisplayClipRgn' then sets the clipping of the
		     * actual movie.
		     * Setting clip here is absolutely necessary to get the controller drawn!
		     * BUG: if another widget 'place'ed on top of controller => inf loop;
		     * has something to do with clipping, test 'clipInvalid = false;'.
		     */						 
		    
		    QTTclMacWinBounds( (TkWindow *) movPtr->tkwin, &aRect );
		    RectRgn( region, &aRect );
		    SetClip( region );
                    clipInvalid = false;
		    
		    /* 
		     * We must find out if the present clip region for the movie is invalid,
		     * and set a new one below if this is the case. 
		     */
		    
		    if (MyIsClipRegionInvalid( movPtr->tkwin )) {
                        clipInvalid = true;
                        QTTclDebugPrintf( movPtr->interp, 3, "\tMyIsClipRegionInvalid" );
                    }
                    TkMacOSXSetUpClippingRgn( (Drawable) Tk_WindowId( movPtr->tkwin ) );
                    if (clipInvalid) {
                        rgnHandle = QTTclMacVisableClipRgn( (TkWindow *) movPtr->tkwin );
                        SetMoviePlayerRectBox( movPtr );
                        if (rgnHandle == NULL) {
                            clipInvalid = false;
                        }
                    }
		    
		    if (movPtr->aController != NULL) {
			if (clipInvalid) {		
			    MCSetClip( movPtr->aController, rgnHandle, NULL );
			}	
                        if (eventPtr == NULL) {
			    
                            /* Says we shall only idle movie. */
                            MCIdle( movPtr->aController );
                        } else {
                            if (MCIsPlayerEvent( movPtr->aController, eventPtr )) {
                                eventDone = 1;
                                break;
                            }
                        }
		    } else {		/* without a controller */
			if (theMovie != NULL) {
			    
			    /*
			     * For a movie without a controller, the rate is not reset to 0
			     * when the movie ends. Needs to do that provided it is not looping.
			     */
			    
                            /* Not working on MacOSX's event model!!! */
			    
			    if (!IsMovieLooping( theMovie )) {
				if (IsMovieDone( theMovie )) {
				    if (GetMovieRate( theMovie ) > 0) {
					SetMovieRate( theMovie, 0 );
                                        QTTclDebugPrintf( movPtr->interp, 3, 
							 "\tMovieDone, SetMovieRate 0" );
				    }
				}
			    }						 
			    if (clipInvalid) {				
				SetMovieDisplayClipRgn( theMovie, rgnHandle );
			    }
			    MoviesTask( theMovie, maxMilliSecsToUse );
			}
		    }
		    if (movPtr->loadCommand && 
			(movPtr->loadState < kMovieLoadStateComplete)) {
			if (counter % 10 == 0) {
			    CheckMovieLoadState(movPtr);
			}
		    }
		    SetClip( saveRegion );
		    
		} else if (movPtr->url != NULL) {
		    
		    /*
		     * If loading a remote movie (-url) it needs to be served while loading,
		     * but not displayed.
		     */
		    
		    MoviesTask( theMovie, maxMilliSecsToUse );
		    if (movPtr->loadCommand) {
			if (counter % 10 == 0) {
			    CheckMovieLoadState( movPtr );
			}
		    }
		}
                
	    nextMovie:
                
                movListPtr = nextMovListPtr;
	    }	/* end loop over movie player list */
	    
	    SetGWorld( saveWorld, saveDevice );
	}	/* end if !eventDone */
	
	/*
	 * We should get all events that are "movie player" events before returning.
	 * The first event that is not handled by us is passed on. 
         * We don't do this on Carbon.
	 */
	
#if !TARGET_API_MAC_CARBON
	if (shortcutLoop && eventDone) {
	    WaitNextEvent( everyEvent, eventPtr, 0, NULL );
	}
#endif
	
    } while (shortcutLoop && eventDone);
    
#if TARGET_API_MAC_CARBON
    CarbonTimerNextTime();	
#endif
    
    return eventDone;
}

#endif 	// TARGET_OS_MAC

#ifdef _WIN32
/*
 *----------------------------------------------------------------------
 *
 * QTWinProc --
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

LRESULT CALLBACK 
QTWinProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
    LONG                origProc = 0L;
    HWND                tempH = NULL;
    MoviePlayerList     *movListPtr = NULL;
    MoviePlayer			*movPtr;
    
    if (GetNativeWindowPort(hWnd)) {
	MSG	        msg;
	EventRecord	macEvent;
	LONG        thePoint = GetMessagePos();
	int         retVal;
	
	msg.hwnd = hWnd;
	msg.message = message;
	msg.wParam = wParam;
	msg.lParam = lParam;
	msg.time = GetMessageTime();
	msg.pt.x = LOWORD(thePoint);
	msg.pt.y = HIWORD(thePoint);
	
	/* Convert the message to a QTML event */
	WinEventToMacEvent( &msg, &macEvent );  
	retVal = MoviePlayerMacEventWin( &macEvent );
    }
    
    /*
     * Dispatch the event to the correct original winproc from tk.
     */
    
    movListPtr = gMoviePlayerListPtr;
    while (movListPtr != NULL) {
        movPtr = movListPtr->movPtr;
        if (movPtr->hwnd != NULL) {
	    if (hWnd == movPtr->hwnd) {
		origProc = movPtr->winEventProc;
		break;
	    }
        }
        movListPtr = movListPtr->next;
    }
    
    if (origProc) {
	return CallWindowProc( (WNDPROC) origProc, hWnd, message, wParam, lParam );
    } else {
	return 0;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * MoviePlayerMacEventWin --
 *
 *		Processes mac events. 
 *		Incoming event just filtered through all active movies and 
 *		dialogs (Windows). Just a stripped down version of 
 *		'MoviePlayerMacEvent'.
 *
 * Results:
 *		0 if the event was not handled, 1 if it was.
 *
 * Side effects:
 *		Events processed.
 *
 *----------------------------------------------------------------------
 */

static int 
MoviePlayerMacEventWin( EventRecord *eventPtr )
{
    MoviePlayer         *movPtr;
    Movie		        theMovie = NULL;
    MoviePlayerList     *movListPtr = NULL;
    MoviePlayerList     *nextMovListPtr = NULL;
    int                 eventDone = 0;
    static long         counter = 0;
    long    			maxMilliSecsToUse = 0;
    
    /* 
     * We keep a counter here to enable regular calls only at a fraction rate. 
     * This is used for queries to the status when a movie is loaded remotely.
     */
    
    counter++;	
    movListPtr = gMoviePlayerListPtr;
    
    while (movListPtr != NULL) {
        nextMovListPtr = movListPtr->next;
        movPtr = movListPtr->movPtr;
	theMovie = movPtr->aMovie;
	
	/*
	 * Unmapped audio only movies need to be served on Windows, in contrast to Mac. 
	 * The 'isVisual' member variable is cached with info if audio only or not.
	 */
	
	if ((movPtr->state & kQTTclMovieStateOnDisplay) || !(movPtr->isVisual)) {
	    
	    if (movPtr->aController != NULL) {
		if (MCIsPlayerEvent( movPtr->aController, eventPtr )) {
		    eventDone = 1;
		}
	    } else {
		if (theMovie != NULL) {
		    
		    /*
		     * For a movie without a controller, the rate is not reset to 0
		     * when the movie ends. Needs to do that provided it is not looping.
		     */
		    
		    if (!IsMovieLooping( theMovie )) {
			if (IsMovieDone( theMovie )) {
			    if (GetMovieRate( theMovie ) > 0) {
				SetMovieRate( theMovie, 0 );
			    }
			}
		    }						 
		    MoviesTask( theMovie, maxMilliSecsToUse );
		}
	    }
	    if (movPtr->loadCommand && (movPtr->loadState < kMovieLoadStateComplete)) {
		if (counter % 10 == 0) {
		    CheckMovieLoadState(movPtr);
		}
	    }
	    
	} else if (movPtr->url != NULL) {
	    
	    /*
	     * If loading a remote movie (-url) it needs to be served while loading,
	     * but not displayed. 
	     */
	    
	    MoviesTask( theMovie, maxMilliSecsToUse );
	    
	    /* does this increase the download speed? */
	    if (movPtr->loadCommand != NULL) {
		if (counter % 10 == 0) {
		    CheckMovieLoadState( movPtr );
		}
	    }
	}
	
	/* Take next movie in our linked list. */
	movListPtr = nextMovListPtr;
    } 
    return 0;
}
#endif      // _WIN32

/*
 *----------------------------------------------------------------------
 *
 * CheckMovieLoadState --
 *
 *		During an async opening of a remote movie, we need to 
 *		regularly query the movie's load state. Calls to 'MoviesTask' 
 *		is necessary to keep the network process alive.
 *
 * Results:
 *		None.
 *
 * Side effects:
 *		Any registered Tcl procedure gets called if the load state changes.
 *
 *----------------------------------------------------------------------
 */

static void
CheckMovieLoadState( MoviePlayer *movPtr )
{
    Tcl_Interp 		*interp = movPtr->interp;
    long            loadState = 0;
    ComponentResult errStatus;
    int				resTcl = TCL_OK;
    Tcl_Obj			*listObjPtr = NULL;
    int				useDirectCallback = false;
    
    loadState = GetMovieLoadState( movPtr->aMovie );
    
    if (loadState == movPtr->loadState) {
	
	/* Movies state did not change compared to previous call. */
        return;
    }
    QTTclDebugPrintf( interp, 2, "CheckMovieLoadState loadState=%d", 
		     loadState );
    
    /*
     * Start designing the Tcl callback. 
     * Note that movPtr->loadCommand may already be a list.
     */
    
    if (movPtr->loadCommand) {
        listObjPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );
        Tcl_ListObjAppendList( interp, listObjPtr, 
			      Tcl_NewStringObj(movPtr->loadCommand, -1) );
        Tcl_ListObjAppendElement( interp, listObjPtr, 
				 Tcl_NewStringObj( Tcl_GetCommandName( interp, movPtr->widgetCmd ), -1 ) );
    }   
    
    if ((loadState < 0) || (loadState == kMovieLoadStateError)) {
	
        /* 
         * We've got an error. Find a way to add both an error code and the message
         * to the tcl callback procedure. 'GetMovieStatus' returns errors
         * occuring in 'MoviesTask', which is where we are when establishing
         * network connections etc.
         */
        
	/* Reset the load state. */
	movPtr->loadState = loadState;
	
        errStatus = GetMovieStatus( movPtr->aMovie, NULL );
        QTTclDebugPrintf( interp, 2, "\t GetMovieStatus errStatus=%d", 
			 errStatus );
	
        if (movPtr->loadCommand) {
            Tcl_ListObjAppendElement( interp, listObjPtr, Tcl_NewStringObj("error", -1) );
	    if (errStatus != noErr) {
                Tcl_Obj		*errObjPtr;
                
                errObjPtr = GetErrorObj( errStatus );
                Tcl_ListObjAppendElement( interp, listObjPtr, errObjPtr );
	    } else {
                Tcl_ListObjAppendElement( interp, listObjPtr, Tcl_NewLongObj( errStatus ) );
	    }
            if (useDirectCallback) {
                resTcl = Tcl_Eval( interp, Tcl_GetString( listObjPtr ) );
                Tcl_DoOneEvent( TCL_DONT_WAIT | TCL_ALL_EVENTS );
            }
	}        
	
    } else if (loadState != movPtr->loadState) {
	
	/* Reset the load state. */
	movPtr->loadState = loadState;
	
	if ((loadState >= kMovieLoadStatePlayable) && 
	    !(movPtr->state & kQTTclMovieStateAsyncHasWorld)) {
	    
	    /* 
	     * Now its time to query the movies size, and add a controller.
	     * Many of the configure options need a controller to work,
	     * and therefore we cannot wait to make the controller until it is displayed.
	     */
	    
            if (movPtr->aMovie != NULL) {
		Rect	    aRect;
		
		GetMovieBox( movPtr->aMovie, &aRect );
		MacOffsetRect( &aRect, (short) -aRect.left, (short) -aRect.top );
		movPtr->mwidth = aRect.right;
		movPtr->mheight = aRect.bottom;
		
                /*
                 * All configuration options are taken from the struct now,
                 * and we therefore need to force all settings (except -file -url etc).
                 */
		
            	resTcl = ConfigureMoviePlayer( interp, movPtr, 0, NULL );
                if (resTcl != TCL_OK) {
                    Tcl_ListObjAppendElement( interp, listObjPtr, 
					     Tcl_NewStringObj("error", -1) );
                    Tcl_ListObjAppendElement( interp, listObjPtr, 
					     Tcl_GetObjResult( interp ) );
		    
                    if (useDirectCallback) {
                        resTcl = Tcl_Eval( interp, Tcl_GetString( listObjPtr ) );
                        Tcl_DoOneEvent( TCL_DONT_WAIT | TCL_ALL_EVENTS );
                    }
                    return;
                } else {
                    MoviePlayerWorldChanged( (ClientData) movPtr );
                    movPtr->state |= kQTTclMovieStateAsyncHasWorld;
                }
            }
	}    
	if (loadState >= kMovieLoadStateComplete) {
	    
	    /*
	     * We are finished with async loading. Clear flag.
	     */
	    
	    movPtr->state &= ~kQTTclMovieStateAsyncLoading;
	}    
	
	/* 
	 * Load state changed, call Tcl proc. 
	 * Must not be called before 'MoviePlayerWorldChanged'!
	 */
        
        if (movPtr->loadCommand) {
            if (loadState < kMovieLoadStatePlayable) {
                Tcl_ListObjAppendElement( interp, listObjPtr, 
					 Tcl_NewStringObj("loading", -1) );
            } else if (loadState < kMovieLoadStatePlaythroughOK) {
                Tcl_ListObjAppendElement( interp, listObjPtr, 
					 Tcl_NewStringObj("playthroughok", -1) );
            } else if (loadState < kMovieLoadStateComplete) {
                Tcl_ListObjAppendElement( interp, listObjPtr, 
					 Tcl_NewStringObj("playable", -1) );
            } else if (loadState >= kMovieLoadStateComplete) {
                Tcl_ListObjAppendElement( interp, listObjPtr, 
					 Tcl_NewStringObj("complete", -1) );
            }
            if (useDirectCallback) {
                resTcl = Tcl_Eval( interp, Tcl_GetString( listObjPtr ) );
                Tcl_DoOneEvent( TCL_DONT_WAIT | TCL_ALL_EVENTS );
            }
	}
    }
    if (useDirectCallback) {
        Tcl_DecrRefCount( listObjPtr );
    }
    
    /*
     * Design an async callback to be executed as soon as possible.
     */
    
    if (!useDirectCallback && listObjPtr != NULL) {
        Tcl_HashTable 		*asyncLoadHashTablePtr = movPtr->asyncLoadHashTablePtr;
        Tcl_HashEntry   	*hashPtr = NULL;
        AsyncLoadHandlerEntry 	*asyncHandlerEntryPtr;
        int					isNew;
        
        /* Init hash table. */
        if (asyncLoadHashTablePtr == NULL) {
            movPtr->asyncLoadHashTablePtr = (Tcl_HashTable *) 
	    ckalloc( sizeof(Tcl_HashTable) );
            asyncLoadHashTablePtr = movPtr->asyncLoadHashTablePtr;
            Tcl_InitHashTable( asyncLoadHashTablePtr, TCL_ONE_WORD_KEYS );
        }
	
        /* Create the entry in hash table. */
        asyncHandlerEntryPtr = (AsyncLoadHandlerEntry *) 
	ckalloc( sizeof(AsyncLoadHandlerEntry) );   
        hashPtr = Tcl_CreateHashEntry( asyncLoadHashTablePtr, 
				      (char *) asyncHandlerEntryPtr, &isNew );
        Tcl_SetHashValue( hashPtr, (char *) asyncHandlerEntryPtr );	
        asyncHandlerEntryPtr->movPtr = movPtr;
        asyncHandlerEntryPtr->commandObjPtr = listObjPtr;
        asyncHandlerEntryPtr->hashPtr = hashPtr;
        
        asyncHandlerEntryPtr->timerToken = Tcl_CreateTimerHandler( 0, 
								  AsyncLoadHandlerProc, (ClientData) asyncHandlerEntryPtr );
    }
}

/*
 *----------------------------------------------------------------------
 *
 * AsyncLoadHandlerProc --
 *
 *		Callback when async loading movies.
 *
 * Results:
 *		.
 *
 * Side effects:
 *		.
 *
 *----------------------------------------------------------------------
 */

static void
AsyncLoadHandlerProc(
		     ClientData clientData )		/* Pointer to AsyncLoadAsyncHandlerRecord structure. */
{
    AsyncLoadHandlerEntry 	*asyncHandlerEntryPtr = (AsyncLoadHandlerEntry *) clientData;
    Tcl_Interp 			*interp = asyncHandlerEntryPtr->movPtr->interp;
    int					cmdCode;
    
    /* Since we may be destroyed in the callback keep storage alive. */
    Tcl_Preserve( (ClientData) asyncHandlerEntryPtr->movPtr );
    
    if (interp != NULL) {
        QTTclDebugPrintf( interp, 2, "AsyncLoadHandlerProc" );
        Tcl_IncrRefCount( asyncHandlerEntryPtr->commandObjPtr );  
        cmdCode = Tcl_EvalObjEx( interp, asyncHandlerEntryPtr->commandObjPtr, TCL_EVAL_GLOBAL );
    } else {
        /*
         * This should not happen, but by definition of how async
         * handlers are invoked, it's possible.  Better error
         * checking is needed here.
         */
    }
    
    /* Remove it since it is a single shot call. */
    Tcl_DecrRefCount( asyncHandlerEntryPtr->commandObjPtr );  
    Tcl_DeleteHashEntry( asyncHandlerEntryPtr->hashPtr );
    
    Tcl_Release( (ClientData) asyncHandlerEntryPtr->movPtr );
    
    ckfree( (char *) asyncHandlerEntryPtr );
}

static void		
AsyncLoadFree( MoviePlayer *movPtr )
{
    Tcl_HashTable 				*asyncLoadHashTablePtr = movPtr->asyncLoadHashTablePtr;
    Tcl_HashEntry 				*hashPtr;
    Tcl_HashSearch 				search;
    AsyncLoadHandlerEntry 		*entryPtr;
    
    /* Loop through the list of async handler hash entries and kill. */
    if (asyncLoadHashTablePtr != NULL) {
	hashPtr = Tcl_FirstHashEntry( asyncLoadHashTablePtr, &search );
	while (hashPtr != NULL) {
	    entryPtr = (AsyncLoadHandlerEntry *) Tcl_GetHashValue( hashPtr );
            Tcl_DeleteTimerHandler( entryPtr->timerToken );
            Tcl_DecrRefCount( entryPtr->commandObjPtr );  
	    Tcl_DeleteHashEntry( hashPtr );
	    ckfree( (char *) entryPtr );
	    hashPtr = Tcl_NextHashEntry( &search );
	}
        Tcl_DeleteHashTable( asyncLoadHashTablePtr );
        Tcl_Free( (char *) asyncLoadHashTablePtr );
        movPtr->asyncLoadHashTablePtr = NULL;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * MovieExitproc --
 *
 *		Registered exit handler. If no movies left call ExitMoviePlayer
 *		else set flag for later call to ExitMoviePlayer.
 *
 * Results:
 *		None.
 *
 * Side effects:
 *		Invokes ExitMoviePlayer or sets flag to schedule this.
 *
 *----------------------------------------------------------------------
 */

static void
MovieExitProc( ClientData clientData )
{
    QTTclDebugPrintf( NULL, 2, "MovieExitProc" );
    
    gExitState = kQTTclExitStateExitHandlerCalled;
    if (gMovieRefCount <= 0) {
        ExitMoviePlayer( clientData );
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ExitMoviePlayer --
 *
 *		Last bit of cleanup.
 *
 * Results:
 *		None.
 *
 * Side effects:
 *		Exits movie toolbox, probably frees memory.
 *
 *----------------------------------------------------------------------
 */

static void
ExitMoviePlayer( ClientData clientData )
{
    QTTclDebugPrintf( NULL, 2, "ExitMoviePlayer" );
    
    if (gExitState == kQTTclExitStateExitHandlerCalled) {
        gExitState = kQTTclExitStateAllFreed;
        ExitMovies();
	
#ifdef _WIN32
	TerminateQTML();		/* This used to crash Windows when exit
	 * when this call came before DestroyNotify */
	TerminateQTVR();
#endif	
	
        ExportComponentSettingsFree();
        UserDataHashTablesFree();
        EffectsFree();
        TracksCommandFree();
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ProcessSpriteSubcmd --
 *
 *		Process the "sprite" subcommand.
 *
 * Results:
 *  	Normal TCL results
 *
 * Side effects:
 *		Depends on event.
 *
 *----------------------------------------------------------------------
 */

static int
ProcessSpriteSubcmd( ClientData clientData, 
		    Tcl_Interp *interp,
		    int objc, 
		    Tcl_Obj *CONST objv[]) 
{	
    int result = TCL_ERROR;
    
    Tcl_SetObjResult( interp, Tcl_NewStringObj( 
					       "sprite subcommand not implimented, yet", -1 ) );
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * ProcessVectorSubcmd --
 *
 *		Process the "vector" subcommand.
 *
 * Results:
 *  	Normal TCL results
 *
 * Side effects:
 *		Depends on event.
 *
 *----------------------------------------------------------------------
 */

static int
ProcessVectorSubcmd( ClientData clientData, 
		    Tcl_Interp *interp,
		    int objc, 
		    Tcl_Obj *CONST objv[]) 
{	
    int result = TCL_ERROR;
    
    Tcl_SetObjResult( interp, Tcl_NewStringObj( 
					       "vector subcommand not implimented, yet", -1 ) );
    return result;
    
}

static void
InstallExtendedSCCallbackProcedures( ComponentInstance ci, long refCon )
{
    SCExtendedProcs     procStruct;
    
    procStruct.filterProc = NewSCModalFilterUPP( MySCFilterDialogProc );
    procStruct.hookProc = NULL;
    procStruct.customName[0] = 0;
    procStruct.refcon = refCon;
    SCSetInfo( ci, scExtendedProcsType, &procStruct );
}

#if TARGET_OS_MAC && !TARGET_API_MAC_CARBON

static pascal Boolean
MySCFilterDialogProc( DialogPtr dialogPtr, EventRecord * event, short *itemHit,
		     long refCon )
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
MySCFilterDialogProc( DialogPtr dialogPtr, EventRecord * event, short *itemHit,
		     long refCon )
{
    Boolean         handled = false;
    
    return handled;
}
#endif // TARGET_API_MAC_CARBON

#ifdef _WIN32
Boolean
MySCFilterDialogProc( DialogPtr dialogPtr, EventRecord * event, short *itemHit,
		     long refCon )
{
    Boolean         handled = false;
    
    return handled;
}
#endif // _WIN32

/*----------------------------------------------------------------------*/
