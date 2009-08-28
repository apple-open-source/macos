/*
 * MoviePlayer.h --
 *
 *	Part of QuickTimeTcl.
 *
 * Copyright (c) 2000-2008  Mats Bengtsson
 *
 * $Id: MoviePlayer.h,v 1.8 2008/02/15 15:23:06 matben Exp $
 */

#ifndef INCLUDED_MOVIEPLAYER_H
#define INCLUDED_MOVIEPLAYER_H

#ifdef _WIN32
#   include "QuickTimeTclWin.h"
#endif  

#include "QuickTimeTcl.h"
#include "Tfp_Arrays.h"

#define PiNumber                (3.1415926535898)
#define RadiansToDegrees(x)     ((x) * 180.0 / PiNumber)
#ifndef Fixed2Int
#   define Fixed2Int(f)	    ((f) >> 16)
#   define Int2Fixed(i)	    ((i) << 16)
#endif

/*
 * Some notes about movie editing versus track editing. Movie editing has 
 * high level API's, such as select, cut, copy, paste, using the real scrap 
 * (clipboard). Track editing lacks high level support, and is implemented 
 * using the fake track scrap 'TrackScrap' defined below, which only contains 
 * references, never any actual content. 
 *
 * We make a track selection record for each movie, see below. 
 * Thus, there can only 
 * be one track selection in each movie.
 * Notes:
 *      -   A movie selection should invalidate any track selection for that 
 *          movie.
 *      -   When something is placed on the real scrap, the fake track scrap 
 *	    should be invalidated.
 *      -   A track selection sets the movie selection to 0 0.
 *      -   When something is placed on the fake track scrap, the real scrap 
 *	    should be emptied.
 */

typedef struct TrackSelection {
    long        trackID;        /* ID of selected track, 0 if no selection. */
    TimeValue   startTime;      /* Start of track selection, in movie's time 
				 * coordinates. */
    TimeValue   duration;       /* And the selections duration, in movie's 
				 * time coordinates. */
} TrackSelection;

/*
 * Used for disposing of movie at idle time.
 */

typedef struct TmpMovieRecord {
    Movie               movie;
    MovieController     movieController;
} TmpMovieRecord;


/*
 * A data structure of the following type is kept for each
 * movie player that currently exists for this process:
 */

typedef struct MoviePlayer {
    Tk_Window	    tkwin;	/* Window for MoviePlayer */
    Display	    *display;	/* Display containting widget */
    Tcl_Interp	    *interp;	/* Interpreter for widget */
    Tcl_Command	    widgetCmd;	/* token for button's widget */
    Tk_OptionTable  optionTable;
    
    int		    flags;	/* Various status flags. */
    int		    state;	/* Tells how the movie should be served, 
				 * on display or loading async remotely. */
    int		    mheight;	/* The natural movie height and width 
				 * without controller. */
    int		    mwidth;
    int		    width;	/* Width of widget; defaults to 0, then 
				 * use natural size. */
    int		    height;	/* Height of widget; only if specifically 
				 * requested. */
    int		    padx;	/* X padding */
    int		    pady;	/* Y padding */
    XColor 		*highlightBgColorPtr;/* Color for drawing traversal highlight
     * area when highlight is off. */
    XColor 		*highlightColorPtr;	/* Color for drawing traversal highlight. */
    int 		highlightWidth;		/* Width in pixels of highlight to draw
     * around widget when it has the focus.
     * <= 0 means don't draw a highlight. */
    int 		inset;				/* Number of pixels on the left and right
     * sides that are taken up by xpad, 
     * and highlightWidth (if any). */
    int 		volume;				/* volume control */
    Movie 		aMovie;				/* Movie pointer */
    MovieController aController; 	/* The movie controller */
    int 		wantController;     /* 1 if we want a controller, 0 otherwise */
    int			controllerHeight;	/* Height of controller bar, if any. */
    int			resizable;			/* If we want the movie to be resizable via 
     * the controller. */
    int			custombutton;		/* Custom button in the mc bar? */
    int         loopstate;          /* Movie loopstate. */
    int         palindromeloopstate;/* Movie palindrome loopstate. Goes back 
     * and forth. */
    int         mcEdit;             /* Boolean if selection vie the controller. */
    Fixed       rate;               /* The play rate. */
    double      preferredRate;      /* The preferred play rate. */
    int			isVisual;			/* If something for eyes; needed on Windows 
     * to serve audio only. */
    char 		*filename;			/* File name to read movie from. 
     * Only pointer to string in 'fileNamePtr' */
    Tcl_Obj		*fileNamePtr;		/* Corresponding object representation. */
    char 		*url;				/* Url to fetch movie from. */
    long        loadState;          /* The load state 'kMovieLoadState...' when 
     * async url. 
     * This is an Apple constant, different 
     * from 'state' above. */
    char		*loadCommand;		/* Tcl callback procedure that tells the 
     * -url status. */	
    int         loadIntoRam;        /* Boolean if load into ram. */
    int 		undoCount;			/* undo counter, is the array index of the 
     * next editStates[] to use. */
    QTVRInstance    qtvrInstance;   /* For QTVR movies we operate on this thing, 
     * and therefore cache it here. 
     * For ordinary movies it should be NULL. */	
    int        	indQTVRQualityStatic; /* Render quality for QTVR, static. */                                 
    int         indQTVRQualityMotion; /* Render quality for QTVR, motion. */                                 
    int         swing;              /* For QTVR movies: boolean, makes soft 
     * camera movements. */   
    int         swingSpeed;         /* For QTVR movies: number from 1 to 10 
     * giving the camera speed. */                  							 
    QTVRInterceptUPP funcQTVRIntercept; 
    /* The controller callback intercept 
     * procedure for QTVR. */
    MovieEditState  *editStates; 	/* Array of movie edit states */
    int 		editStatesSize;     /* size of above array */
    TrackSelection  *trackSelect;   /* The track selection record, see above. */
    short 		resourceNumber;		/* resource number, should be -1 when the 
     * movie is in the data fork. */
    GWorldPtr   grafPtr;            /* We save the actual graf port so we don't 
     * need to call SetMovieGWorld if not 
     * changed, it costs! */
    char 		*progressProc;		/* Called by the movie toolbox. 
     * Text string for tcl proc. */
    int         qtprogress;         /* Boolean; want standard QuickTime 
     * progress or not. */
    char 		*mcCallbackProc;	/* Called by the movie controller when 
     * processing events. */
    int         insideMCCommand;    /* Ref count set when enter/exit the 
     * -mccommand tcl proc. */	
    Tcl_HashTable *asyncLoadHashTablePtr; /* Keep a hash table for async 
     * callbacks during async loading. */
    Tcl_HashTable *callBackHashTablePtr; /* Keep a hash table for registered
     * callbacks. */
    TmpMovieRecord  *tmpMovieRecordPtr;/* Used to hold movie and controller for 
     * destruction at idle time. */
#ifdef _WIN32
    HWND        hwnd;               /* Cache the Windows win handle. 
     * NULL signals port association */
    LONG	    winEventProc;       /* Original event procedure (Windows). */
#endif
} MoviePlayer;

/*
 * We keep a global fake clipboard for tracks. Only one global copy! 
 * If empty scrap, the gTrackScrap.movPtr is NULL.
 */ 

typedef struct TrackScrap {
    TrackSelection  trackSelect;
    MoviePlayer     *movPtr;
} TrackScrap;

/*
 * We need a global doubly linked list for our movies since we cant get them 
 * to the Mac native event handler in any other way.
 */

typedef struct MoviePlayerList {
    MoviePlayer                 *movPtr;
    struct MoviePlayerList      *prev;
    struct MoviePlayerList      *next;
} MoviePlayerList;

/*
 * Flag bits for grabber used in 'flags' in MoviePlayer struct:
 *
 * REDRAW_PENDING:		Non-zero means a DoWhenIdle handler has already been 
 *						queued to redraw this window.
 * NEWGWORLD:			Non-zero means this widget has a new GWorld, and that we need
 *						to associate the movie with this GWorld. Need to
 *						know when displaying.
 * UPDATEMOVIE:			This flag bit is set when received an Expose (update) 
 *						event in the tcl event procedure. Only applicable for 
 *						movies without a movie.
 * MOVIE_DELETED:		Non-zero needs that this button has been
 *						deleted, or is in the process of being deleted.
 */

#define REDRAW_PENDING 		(1L << 0)
#define NEWGWORLD 			(1L << 1)
#define UPDATEMOVIE 		(1L << 2)
#define NEED_PREROLL 		(1L << 3)
#define GOT_FOCUS			(1L << 4)
#define MOVIE_DELETED 		(1L << 5)

#define CONTROLLER_WIDTH	160
#define UNDO_SIZE 			100		/* Allocate increment for undo stack */

/*
 * We need some constants for the state member in the MoviePlayer struct.
 * It is used to determine how a movie should be served.
 */

enum {
    kQTTclMovieStateOnDisplay		= 0x0001,
    kQTTclMovieStateAsyncLoading	= 0x0002,
    kQTTclMovieStateAsyncHasWorld	= 0x0004      /* For knowing if 'MoviePlayerWorldChanged' 
     * has been called when async loading. */
};

/*
 * The ExitHandlerProc is in certain situations called BEFORE the
 * widgets receive the DestroyNotify event and we must therefore
 * NEVER do final cleanup before all movies have been properly freed.
 * We keep a global state variable to keep track of this.
 */

enum QTTclFreeState {
    kQTTclExitStateExitHandlerCalled		= 1,
    kQTTclExitStateAllFreed					= 2
}; 

enum {
    kDoesCodecPixmapDepth                       = 0L,
    kDoesCodecCompressDepth
};

enum {
    kCompressFlagColorDepth                     = (1L << 0),
    kCompressFlagCompressor                     = (1L << 1),
    kCompressFlagDialog                         = (1L << 2),
    kCompressFlagKeyFrameRate                   = (1L << 3),
    kCompressFlagSpatialQuality                 = (1L << 4),
    kCompressFlagTemporalQuality                = (1L << 5)
};

enum {
    kQTTclNormalLooping		= 0,
    kQTTclPalindromeLooping	= 1,
    kQTTclNoLooping			= 2
};

int 			ProcessTracksObjCmd(ClientData clientData, 
					    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[] );
void 			TracksCommandFree( void );
int 			LogUndoState( MoviePlayer *movPtr, Tcl_Interp *interp );
int 			AddImagesToMovie( Tcl_Interp *interp, Media myMedia, 
					 TimeValue duration, int nPhotos, Tk_PhotoHandle *photoList, 
					 int showDialog, CodecType codecType,
					 CodecQ spatialQuality, CodecQ temporalQuality,
					 short colorDepth, long keyFrameRate, 
					 TimeValue *sampleTime );	
int             AddImageFileToMovie( Tcl_Interp *interp, Media myMedia,
				    TimeValue duration, char *fileName, long optFlags,
				    int showDialog, CodecType codecType, CodecQ spatialQuality, 
				    CodecQ temporalQuality, short colorDepth, 
				    long keyFrameRate, TimeValue *sampleTimePtr );
void 			MoviePlayerWorldChanged( ClientData clientData );

int				GetMovieStartTimeFromObj( Tcl_Interp *interp, Movie movie, 
							 Tcl_Obj *obj, long *timeValuePtr );
int				GetMovieDurationFromObj( Tcl_Interp *interp, Movie movie, 
							Tcl_Obj *obj, long movTime, long *durValuePtr );

/*
 * TimeCode, UserData:
 */

int	    TimeCodeCmd( Tcl_Interp *interp, MoviePlayer *movPtr,
		int objc, Tcl_Obj *CONST objv[] );
int	    GetUserDataListCmd( UserData userData, Tcl_Interp *interp );
int	    SetUserDataListCmd( UserData userData, Tcl_Interp *interp, 
		int objc, Tcl_Obj *CONST objv[] );
void	    UserDataHashTablesFree( void );


int	    ProcessEffectCmd( Tcl_Interp *interp, MoviePlayer *movPtr,
		int objc, Tcl_Obj *CONST objv[] );
int	    ProcessExportCmd( Tcl_Interp *interp, Movie theMovie,
		int objc, Tcl_Obj *CONST objv[] );
Boolean	    EffectsHandleDialogEvents( EventRecord *eventPtr, 
		DialogItemIndex itemIndex );
void	    ExportComponentSettingsFree( void );
void	    EffectsInit( void );
void	    EffectsFree( void );

int	    ProcessCallBackCmd( MoviePlayer *movPtr, int objc, 
		Tcl_Obj *CONST objv[] );
void	    CallBackFree( MoviePlayer *movPtr );


/* Non-tk specific routines. */

OSErr	    GetFirstVideoTrackInMovie( Movie theMovie, Track *theTrack );
void	    ShowControllerButton( MovieController theMC, long theButton );
void	    HideControllerButton( MovieController theMC, long theButton );
void	    SetMovieLoopState( Movie theMovie, UInt8 theLoopState );
void	    SetLoopingState( MovieController theMC, 
		UInt8 theLoopState );
Boolean	    IsMovieLooping( Movie theMovie );
void	    MoviePrePrerollCompleteProc( Movie theMovie, 
		OSErr err, void *refConst );
OSType          GetControllerType( Movie theMovie );
Boolean         IsQTVRMovie( Movie theMovie );
short			GetControllerBarHeight( MovieController mc ); 
QTVRInstance    GetQTVRInstanceFromController( MovieController mc );
pascal void     MyQTVRInterceptProc( QTVRInstance qtvrInst, 
				    QTVRInterceptPtr msg, SInt32 refCon, Boolean *cancel );
int           	TranslateTclListToMatrixRecord( Tcl_Interp *interp, 
					       Tcl_Obj *listObjPtr, MatrixRecord *mr );
int             TranslateMatrixRecordToTclList( Tcl_Interp *interp, 
					       Tcl_Obj **listObj, MatrixRecord *mr );
int             DoesCodecSupport( CodecType cType, long what, int value );
int             IsTrackForEyes( Track myTrack );


#endif	// INCLUDED_MOVIEPLAYER_H
