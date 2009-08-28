/*
 * TimeCode.c --
 *
 *		Implements the 'timecode' command.
 *		It is part of the QuickTimeTcl package which provides Tcl/Tk bindings for QuickTime.
 *
 * Copyright (c) 2003  Mats Bengtsson
 *
 * $Id: TimeCode.c,v 1.3 2005/06/19 10:44:16 matben Exp $
 */

#include "MoviePlayer.h"


/*
 * For dispatching timecode commands.
 */

static char *allTimeCodeCmds[] = {
	"delete", "display", "get", "new", "toggle",
    (char *) NULL
};

enum {
	kTimeCodeCmdDelete					= 0L,
    kTimeCodeCmdDisplay,
    kTimeCodeCmdGet,
    kTimeCodeCmdNew,
    kTimeCodeCmdToggle
};

static char *allTimeCodeNewOptions[] = {
	"-anchor", "-background", "-dropframes", "-font",
	"-foreground", "-frameduration", "-framespersecond",
	"-pady",
	"-sourcename", "-startframes",
	"-starthours", "-startminutes", "-startseconds", 	  
	"-timescale",  
    (char *) NULL
};

enum {
    kTimeCodeNewOptionAnchor		= 0L,
    kTimeCodeNewOptionBackground,
    kTimeCodeNewOptionDropFrames,
    kTimeCodeNewOptionFont,
    kTimeCodeNewOptionForeground,
    kTimeCodeNewOptionFrameDuration,
    kTimeCodeNewOptionFramesPerSecond,
    kTimeCodeNewOptionPadY,
    kTimeCodeNewOptionSourceName,
    kTimeCodeNewOptionStartFrames,
    kTimeCodeNewOptionStartHours,
    kTimeCodeNewOptionStartMinutes,
    kTimeCodeNewOptionStartSeconds,
    kTimeCodeNewOptionTimeScale
};

static void 			DeleteTimeCodeTracks( Movie aMovie );
static int 				NewTimeCodeTrack( Tcl_Interp *interp, 
                                MoviePlayer *movPtr, int objc, Tcl_Obj *CONST objv[] );
static MediaHandler 	GetTimeCodeMediaHandler( Movie aMovie );
static void 			ToggleTimeCodeDisplay( Movie aMovie );
static void 			SetTimeCodeDisplay( Movie aMovie, Boolean state );
static int				GetTimeCodeInfo( Movie aMovie, Tcl_Interp *interp, 
								int objc, Tcl_Obj *CONST objv[] );

/*
 *----------------------------------------------------------------------
 *
 * TimeCodeCmd --
 *
 *		Process the "timecode" subcommand.
 *
 * Results:
 *  	Normal TCL results
 *
 * Side effects:
 *		Depends on the command.
 *
 *----------------------------------------------------------------------
 */
 
int
TimeCodeCmd( Tcl_Interp *interp, 
        MoviePlayer *movPtr,
	    int objc, Tcl_Obj *CONST objv[] ) 
{	
    Movie           aMovie = movPtr->aMovie;
    Tk_Window       tkwin = movPtr->tkwin;
	Track			track = NULL;
    int             cmdIndex;
    int 			result = TCL_OK;

	if (Tcl_GetIndexFromObj( interp, objv[0], allTimeCodeCmds, "timecode command", 
			TCL_EXACT, &cmdIndex ) != TCL_OK ) {
	    return TCL_ERROR;
	}
    
    /*
     * Dispatch the tracks command to the right branch.
     */
    
    switch(cmdIndex) {

        case kTimeCodeCmdDelete: {
    		DeleteTimeCodeTracks( aMovie );
			break;
		}

        case kTimeCodeCmdDisplay: {
		    int				boolean;
        	
			if (Tcl_GetBooleanFromObj( interp, objv[1], &boolean ) != TCL_OK) {
				Tcl_AddErrorInfo( interp, 
						"\n	(processing timecode display command)" );
				return TCL_ERROR;
			}
			SetTimeCodeDisplay( aMovie, (Boolean) boolean );
			break;
		}

        case kTimeCodeCmdGet: {
			result = GetTimeCodeInfo( aMovie, interp, objc - 1, objv + 1 );
			break;
		}

        case kTimeCodeCmdNew: {
			result = NewTimeCodeTrack( interp, movPtr, objc - 1, objv + 1 );
			break;
		}

		case kTimeCodeCmdToggle: {
			ToggleTimeCodeDisplay( aMovie );
			break;
		}
	}
	return result;
}

/*
 *----------------------------------------------------------------------
 *
 * DeleteTimeCodeTracks --
 *
 *		Remove all existing timecode tracks from the specified movie.
 *
 * Results:
 *  	None.
 *
 * Side effects:
 *		track may be deleted
 *
 *----------------------------------------------------------------------
 */

static void 
DeleteTimeCodeTracks( Movie aMovie )
{
	Track				tcTrack = NULL;
	
	tcTrack = GetMovieIndTrackType( aMovie, 1, TimeCodeMediaType, 
			movieTrackMediaType );
	while (tcTrack != NULL) {
		DisposeMovieTrack( tcTrack );
		tcTrack = GetMovieIndTrackType( aMovie, 1, TimeCodeMediaType, 
				movieTrackMediaType );
	}
}

/*
 *----------------------------------------------------------------------
 *
 * NewTimeCodeTrack --
 *
 *		Add a timecode track to the specified movie.
 *
 * Results:
 *  	Standard Tcl return code
 *
 * Side effects:
 *		track may be deleted, new track created.
 *
 *----------------------------------------------------------------------
 */

static int 
NewTimeCodeTrack( Tcl_Interp *interp, 
        MoviePlayer *movPtr, 
		int objc, Tcl_Obj *CONST objv[] )
{
    Movie               aMovie = movPtr->aMovie;
    Tk_Window           tkwin = movPtr->tkwin;
	Track				videTrack = NULL;
	Track				tcTrack = NULL;
	Media				tcMedia = NULL;
	Media				videMedia = NULL;
	MediaHandler		tcMediaHandler = NULL;
	OSType 				mediaType;
	TimeCodeDef			tcDef;
	TimeCodeRecord		tcRec;
	Str255 				creatorName;
	Str63				myString;
	TimeValue			videDuration;
	TimeValue			mediaDuration;
	Rect				rect;
	MatrixRecord		trackMatrix;
	Fixed				widthFix;
	Fixed				heightFix;
	Fixed				tcHeightFix;
	Fixed				padYFix = 0;
	
	/* typical for NTSC video (29.97 frames/sec) */
	TimeScale			timeScale = 600;	
		
	/* duration of a single frame */
	TimeValue			frameDuration = 20;		
	long       			trackID;
	
	/* frames per second */
	long				framesPerSec = 30;		
	long				startHours = 0;
	long				startMinutes = 0;
	long				startSeconds = 0;
	long				startFrames = 0;
	long				flags = 0L;
	long				longValue;
	int					anchorSouth = true;
	int					fontSet = false;
	TCTextOptions		tcTextOptions;
	FontInfo			fontInfo;
    short      			fontFaceNum = 0;
    short           	fontSize= 0;
    Style          		fontStyle = 0;
    RGBColor   			macBGColor = {0x0000, 0x0000, 0x0000};
    RGBColor        	macFGColor = {0xFFFF, 0xFFFF, 0xFFFF};
	RGBColor       		opColor = {0x7700, 0x7700, 0x7700};
    long       			graphicsMode;
	TimeCodeDescriptionHandle	tcDescHand = NULL;
	long				**frameHand;
	OSErr				err = noErr;
	HandlerError		handerr = noErr;
	Tcl_Obj			    *listObjPtr;
	Tcl_Obj			    *subListObjPtr;
	Tcl_Obj			    *resultObjPtr;
	char				*sourceNamePtr = NULL;
	int					booleanInt;
	int					iarg;
	int					optIndex;
    int 			    result = TCL_OK;
    
	if (objc < 1) {
		Tcl_WrongNumArgs( interp, 0, objv, 
				"pathName timecode new trackID ?args?" );
		return TCL_ERROR;
	}
	if (Tcl_GetLongFromObj( interp, objv[0], &trackID ) != TCL_OK) {
		Tcl_AddErrorInfo( interp, "\n	(processing trackID option)" );
		return TCL_ERROR;
	}
    videTrack = GetMovieTrack( aMovie, trackID );
	if (videTrack == NULL) {
        CheckAndSetErrorResult( interp, noErr );
		return TCL_ERROR;
	}
    videMedia = GetTrackMedia( videTrack );
    if (videMedia == NULL) {
        CheckAndSetErrorResult( interp, noErr );
		return TCL_ERROR;
	}
    GetMediaHandlerDescription( videMedia, &mediaType, creatorName, NULL );
	if (mediaType !=VideoMediaType ) {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( 
				"Track id must be a 'vide' track", -1 ) );
		return TCL_ERROR;
	}
    result = LogUndoState( movPtr, interp );
	
	/* Cleanup any existing timecode track(s). */
	DeleteTimeCodeTracks( aMovie );

	/* The movies natural size without any controller. */
	GetMovieBox( aMovie, &rect );
	MacOffsetRect( &rect, (short) -rect.left, (short) -rect.top );
	widthFix = FixRatio( rect.right, 1 );
	heightFix = FixRatio( rect.bottom, 1 );

	/*
	 * Process the configuration options if any.
	 */

	for (iarg = 1; iarg < objc; iarg += 2) {

		if (Tcl_GetIndexFromObj( interp, objv[iarg], allTimeCodeNewOptions, 
				"timecode new option", TCL_EXACT, &optIndex )
				!= TCL_OK ) {
			return TCL_ERROR;
		}
    	if (iarg + 1 == objc) {
    		resultObjPtr = Tcl_GetObjResult( interp );
    		Tcl_AppendStringsToObj( resultObjPtr, "value for \"",
    				Tcl_GetString(objv[iarg]), "\"missing", (char *) NULL );
			return TCL_ERROR;
    	}

		/*
		 * Dispatch the timecode option to the right branch.
		 */

		switch(optIndex) {

			case kTimeCodeNewOptionAnchor: {
				if (!strncmp(Tcl_GetString( objv[iarg+1] ), "n", 1)) {
					anchorSouth = false;
				} else if (!strncmp(Tcl_GetString( objv[iarg+1] ), "s", 1)) {
					anchorSouth = true;
				} else {
					Tcl_SetObjResult( interp, Tcl_NewStringObj( 
							"Not a valid anchor option; \"n\" or \"s\" ", -1 ) );
    				result = TCL_ERROR;
					goto bail;
				}
				break;
			}

			case kTimeCodeNewOptionBackground: {
                XColor          *xcolPtr;

				/* Get mac color from string that can either by a X11 color name or hex value. */
				xcolPtr = Tk_AllocColorFromObj( interp, tkwin, objv[iarg+1] );
				TkSetMacColor( xcolPtr->pixel, &macBGColor );
				break;
			}

			case kTimeCodeNewOptionDropFrames: {
				if (Tcl_GetBooleanFromObj( interp, objv[iarg+1], &booleanInt ) 
							!= TCL_OK) {
					Tcl_AddErrorInfo( interp, 
							"\n	(processing -dropframes option)" );
					result = TCL_ERROR;
					goto bail;
				}
				if (booleanInt) {
					flags |= tcDropFrame;
				}
				break;
			}

			case kTimeCodeNewOptionFont: {
                Tk_Font         tkFont;
			
	            tkFont = Tk_AllocFontFromObj( interp, tkwin, objv[iarg+1] );
	            QTTclGetMacFontAttributes( interp, tkwin, tkFont, 
	                    &fontFaceNum, &fontSize, &fontStyle );
	            Tk_FreeFont( tkFont );
	            fontSet = true;
				break;
			}

			case kTimeCodeNewOptionForeground: {
                XColor          *xcolPtr;

				/* Get mac color from string that can either by a X11 color name or hex value. */
				xcolPtr = Tk_AllocColorFromObj( interp, tkwin, objv[iarg+1] );
				TkSetMacColor( xcolPtr->pixel, &macFGColor );
				break;
			}

			case kTimeCodeNewOptionFrameDuration: {
				if (Tcl_GetLongFromObj( interp, objv[iarg+1], &longValue ) 
							!= TCL_OK) {
					Tcl_AddErrorInfo( interp, "\n	(processing -frameduration option)" );
					result = TCL_ERROR;
					goto bail;
				}
				frameDuration = longValue;
				break;
			}

			case kTimeCodeNewOptionFramesPerSecond: {
				if (Tcl_GetLongFromObj( interp, objv[iarg+1], &longValue ) 
							!= TCL_OK) {
					Tcl_AddErrorInfo( interp, 
							"\n	(processing -framespersecond option)" );
					result = TCL_ERROR;
					goto bail;
				}
				framesPerSec = longValue;
				break;
			}

			case kTimeCodeNewOptionPadY: {
				if (Tcl_GetLongFromObj( interp, objv[iarg+1], &longValue ) 
							!= TCL_OK) {
					Tcl_AddErrorInfo( interp, 
							"\n	(processing -pady option)" );
					result = TCL_ERROR;
					goto bail;
				}
				padYFix = FixRatio( (short) longValue, 1 );
				break;
			}

			case kTimeCodeNewOptionSourceName: {
				sourceNamePtr = Tcl_GetString( objv[iarg+1] );
				break;
			}

			case kTimeCodeNewOptionStartFrames: {
				if (Tcl_GetLongFromObj( interp, objv[iarg+1], &longValue ) 
							!= TCL_OK) {
					Tcl_AddErrorInfo( interp, 
							"\n	(processing -startframes option)" );
					result = TCL_ERROR;
					goto bail;
				}
				startFrames = longValue;
				break;
			}

			case kTimeCodeNewOptionStartHours: {
				if (Tcl_GetLongFromObj( interp, objv[iarg+1], &longValue ) 
							!= TCL_OK) {
					Tcl_AddErrorInfo( interp, 
							"\n	(processing -starthours option)" );
					result = TCL_ERROR;
					goto bail;
				}
				startHours = longValue;
				break;
			}

			case kTimeCodeNewOptionStartMinutes: {
				if (Tcl_GetLongFromObj( interp, objv[iarg+1], &longValue ) 
							!= TCL_OK) {
					Tcl_AddErrorInfo( interp, 
							"\n	(processing -startminutes option)" );
					result = TCL_ERROR;
					goto bail;
				}
				startMinutes = longValue;
				break;
			}

			case kTimeCodeNewOptionStartSeconds: {
				if (Tcl_GetLongFromObj( interp, objv[iarg+1], &longValue ) 
							!= TCL_OK) {
					Tcl_AddErrorInfo( interp, 
							"\n	(processing -startseconds option)" );
					result = TCL_ERROR;
					goto bail;
				}
				startSeconds = longValue;
				break;
			}

			case kTimeCodeNewOptionTimeScale: {
				if (Tcl_GetLongFromObj( interp, objv[iarg+1], &longValue ) 
							!= TCL_OK) {
					Tcl_AddErrorInfo( interp, 
							"\n	(processing -timescale option)" );
					result = TCL_ERROR;
					goto bail;
				}
				timeScale = longValue;
				break;
			}
		}
	}
	
	/*
	 * Create the timecode track and media
	 */

	tcTrack = NewMovieTrack( aMovie, widthFix, heightFix, kNoVolume );
	if (tcTrack == NULL) {
        CheckAndSetErrorResult( interp, noErr );
		result = TCL_ERROR;
		goto bail;
	}
	tcMedia = NewTrackMedia( tcTrack, TimeCodeMediaType, timeScale, NULL, 0 );
	if (tcMedia == NULL) {
        CheckAndSetErrorResult( interp, noErr );
		result = TCL_ERROR;
		goto bail;
	}
	tcMediaHandler = GetMediaHandler( tcMedia );
	if (tcMediaHandler == NULL) {
        CheckAndSetErrorResult( interp, noErr );
		result = TCL_ERROR;
		goto bail;
	}

	/*
	 * Fill in a timecode definition structure; 
	 * this becomes part of the timecode description.
	 */
	
	tcDef.flags = flags;
	tcDef.fTimeScale = timeScale;
	tcDef.frameDuration = frameDuration;
	tcDef.numFrames = (unsigned char) framesPerSec;

	/*
	 * Fill in a timecode record.
	 */
	
	tcRec.t.hours = (UInt8) startHours;
	tcRec.t.minutes = (UInt8) startMinutes;		/* negative flag is here */
	tcRec.t.seconds = (UInt8) startSeconds;
	tcRec.t.frames = (UInt8) startFrames;

	/*
	 * Figure out the timecode track geometry.
	 * Get display options to calculate box height.
	 */
	
	TCGetDisplayOptions( tcMediaHandler, &tcTextOptions );
	tcTextOptions.foreColor = macFGColor;
	tcTextOptions.backColor = macBGColor;
	if (fontSet) {
		tcTextOptions.txFont = fontFaceNum;
		tcTextOptions.txFace = fontStyle;
		tcTextOptions.txSize = fontSize;
	}
	TCSetDisplayOptions( tcMediaHandler, &tcTextOptions );
	
	/*
	 * Use the starting time to figure out the dimensions of track.
	 */
	 
	TCTimeCodeToString( tcMediaHandler, &tcDef, &tcRec, myString );
	TextFont( tcTextOptions.txFont );
	TextFace( tcTextOptions.txFace );
	TextSize( tcTextOptions.txSize );
	GetFontInfo( &fontInfo );
	
	/*
	 * Calculate track width and height based on text.
	 */
	 	
	tcHeightFix = FixRatio( (short) (fontInfo.ascent + fontInfo.descent), 1 );
	SetTrackDimensions( tcTrack, widthFix, tcHeightFix );

	GetTrackMatrix( tcTrack, &trackMatrix );
	if (anchorSouth) {
		TranslateMatrix( &trackMatrix, 0, heightFix - tcHeightFix - padYFix );
	} else {
		TranslateMatrix( &trackMatrix, 0, padYFix );
	}
	SetTrackMatrix( tcTrack, &trackMatrix );	
	SetTrackEnabled( tcTrack, true );
		
	TCSetTimeCodeFlags( tcMediaHandler, tcdfShowTimeCode, tcdfShowTimeCode );
	
	/*
	 * Edit the track media.
	 */

	err = BeginMediaEdits( tcMedia );	
	if (err == noErr) {
		long				size;
		UserData			userData;
		
		/*
		 * Create and configure a new timecode description handle.
		 */

		size = sizeof(TimeCodeDescription);
		tcDescHand = (TimeCodeDescriptionHandle) NewHandleClear( size );
		if (tcDescHand == NULL) {
	        CheckAndSetErrorResult( interp, noErr );
			result = TCL_ERROR;
			goto bail;
		}
		(**tcDescHand).descSize = size;
		(**tcDescHand).dataFormat = TimeCodeMediaType;
		(**tcDescHand).timeCodeDef = tcDef;
		
		if (sourceNamePtr != NULL) {
		
			/*
			 * Set the source identification information.
			 * The source identification information for a timecode track is stored
			 * in a user data item of type TCSourceRefNameType.
			 */

			err = NewUserData( &userData );
			if (err == noErr) {
                char			*charPtr;
				Handle 			nameH = NULL;
                Tcl_DString     ds;
				
                charPtr = Tcl_UtfToExternalDString( gQTTclTranslationEncoding, 
                        sourceNamePtr, -1, &ds );
				err = PtrToHand( charPtr, &nameH, Tcl_DStringLength(&ds) );
				if (err == noErr) {
                    HLock( nameH );
					err = AddUserDataText( userData, nameH, TCSourceRefNameType, 
							1, langEnglish );
                    HUnlock( nameH );
					if (err == noErr) {
						TCSetSourceRef( tcMediaHandler, tcDescHand, userData );
					}
				}
                Tcl_DStringFree( &ds );
				if (nameH != NULL) {
					DisposeHandle( nameH );
				}
				DisposeUserData( userData );
			}
		}

		/*
		 * Add a sample to the timecode track.
		 *
		 * Each sample in a timecode track provides timecode information for a 
		 * span of movie time; here, we add a single sample that spans the entire 
		 * movie duration.
		 */

		/*
		 * The sample data contains a frame number that identifies one or more 
		 * content frames that use the timecode; this value (a long integer) 
		 * identifies the first frame that uses the timecode.
		 */
		 
		frameHand = (long **) NewHandle( sizeof(long) );
		if (frameHand == NULL) {
	        CheckAndSetErrorResult( interp, noErr );
			result = TCL_ERROR;
			goto bail;
		}
		handerr = TCTimeCodeToFrameNumber( tcMediaHandler, &(**tcDescHand).timeCodeDef, 
				&tcRec, *frameHand );

		/* The data in the timecode track must be big-endian. */
		**frameHand = EndianS32_NtoB( **frameHand );
		
		/* Need to convert the duration. */
		videDuration = GetTrackDuration( videTrack );
    	mediaDuration = videDuration * timeScale/GetMovieTimeScale( aMovie );
				
		err = AddMediaSample( tcMedia, 
				(Handle) frameHand, 
				0, 						/* no offset in data */
				GetHandleSize((Handle) frameHand), 
				mediaDuration, 			/* duration in medias time scale */
				(SampleDescriptionHandle) tcDescHand, 
				1, 0, 0 );
		if (err != noErr) {
	        CheckAndSetErrorResult( interp, err );
			result = TCL_ERROR;
			goto bail;	
		}
	}
	
	err = EndMediaEdits( tcMedia );	
	if (err != noErr) {
	    CheckAndSetErrorResult( interp, err );
		result = TCL_ERROR;
		goto bail;
	}
	err = InsertMediaIntoTrack( tcTrack,/* the track */
			0, 							/* insert time in movie's time */
			0, 							/* media start time in media's time */
			mediaDuration, 				/* media's duration in media's time */
			fixed1 );					/* the media's rate */
	if (err != noErr) {
	    CheckAndSetErrorResult( interp, err );
		result = TCL_ERROR;
		goto bail;
	}
	
	/*
	 * Create a track reference from the target track to the timecode track.
	 */
	
	err = AddTrackReference( videTrack, tcTrack, TimeCodeMediaType, NULL );
	
	graphicsMode = addMax;
    MediaSetGraphicsMode( tcMediaHandler, graphicsMode, &opColor );        
	
	/*
	 * The result is to be returned as a list: '{-undostate number} trackID'.
	 */
	
	resultObjPtr = Tcl_GetObjResult( interp );
	Tcl_IncrRefCount( resultObjPtr );
		listObjPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );
    	subListObjPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );
	Tcl_ListObjAppendList( interp, subListObjPtr, resultObjPtr );
	Tcl_ListObjAppendElement( interp, listObjPtr, subListObjPtr );
	Tcl_ListObjAppendElement( interp, listObjPtr, 
			Tcl_NewLongObj(GetTrackID( tcTrack )));						
	Tcl_SetObjResult( interp, listObjPtr );
	
bail:
	if (result != TCL_OK && tcTrack != NULL) {
        DisposeMovieTrack( tcTrack );
	}
	if (tcDescHand != NULL) {
		DisposeHandle( (Handle) tcDescHand );
	}
	if (frameHand != NULL) {
		DisposeHandle( (Handle) frameHand );
	}
	
	return result;
}


static MediaHandler 
GetTimeCodeMediaHandler( Movie aMovie )
{
	Track				tcTrack = NULL;
	Media				tcMedia = NULL;
	MediaHandler		mh = NULL;
	
	/* Get the (first) timecode track in the specified movie. */
	tcTrack = GetMovieIndTrackType( aMovie, 1, TimeCodeMediaType, movieTrackMediaType );
	if (tcTrack != NULL) {
		/* Get the timecode track's media and media handler. */
		tcMedia = GetTrackMedia( tcTrack );
		if (tcMedia != NULL) {
			mh = GetMediaHandler( tcMedia );
		}
	}
	return(mh);
}

static void 
ToggleTimeCodeDisplay( Movie aMovie )
{
	Track				tcTrack = NULL;
	MediaHandler		mh = NULL;
	long				flags = 0L;
	
	/* Get the (first) timecode track in the specified movie. */
	tcTrack = GetMovieIndTrackType( aMovie, 1, TimeCodeMediaType, movieTrackMediaType );
	if (tcTrack != NULL) {
	
		/* Get the timecode track's media handler. */
		mh = GetTimeCodeMediaHandler( aMovie );
		if (mh != NULL) {
		
			/* Toggle the show-timecode flag. */
			TCGetTimeCodeFlags( mh, &flags );
			flags ^= tcdfShowTimeCode;
			TCSetTimeCodeFlags( mh, flags, tcdfShowTimeCode );
			
			/* Toggle the track enabled state. */
			if (GetTrackEnabled( tcTrack )) {
				SetTrackEnabled( tcTrack, false );
			} else {
				SetTrackEnabled( tcTrack, true );
			}
		}
	}
}

static void 
SetTimeCodeDisplay( Movie aMovie, Boolean state )
{
	Track				tcTrack = NULL;
	MediaHandler		mh = NULL;
	long				flags = 0L;
	
	/* Get the (first) timecode track in the specified movie. */
	tcTrack = GetMovieIndTrackType( aMovie, 1, TimeCodeMediaType, movieTrackMediaType );
	if (tcTrack != NULL) {
	
		/* Get the timecode track's media handler. */
		mh = GetTimeCodeMediaHandler( aMovie );
		if (mh != NULL) {
			if (state) {
				flags |= tcdfShowTimeCode;
			} else {
				flags &= ~tcdfShowTimeCode;
			}
			TCSetTimeCodeFlags( mh, flags, tcdfShowTimeCode );
			SetTrackEnabled( tcTrack, state );
		}
	}
}

/*
 *----------------------------------------------------------------------
 *
 * GetTimeCodeInfo --
 *
 *		Returns a number of timecode track info as a key-value list.
 *
 * Results:
 *  	list -key value ...
 *
 * Side effects:
 *		None.
 *
 *----------------------------------------------------------------------
 */

static int
GetTimeCodeInfo( Movie aMovie, 
		Tcl_Interp *interp, 
		int objc, 
		Tcl_Obj *CONST objv[] )
{
	Track				tcTrack = NULL;
	MediaHandler		mh = NULL;
	TimeCodeDef			tcDef;
	TimeCodeRecord		tcRec;
	TimeValue			trackTime;
	TimeValue			mediaTime;
	Str255				pasStr;
	char      			tmpstr[STR255LEN];
	long				frameNum;
	long				flags = 0L;
	long				longValue;
	Tcl_Obj             *listObjPtr;
	UserData			userData = NULL;
	HandlerError		err = noErr;
    int 				result = TCL_OK;
	
	tcTrack = GetMovieIndTrackType( aMovie, 1, TimeCodeMediaType, movieTrackMediaType );
	if (tcTrack == NULL) {
	    CheckAndSetErrorResult( interp, noErr );
		result = TCL_ERROR;
		goto bail;
	}
	mh = GetTimeCodeMediaHandler( aMovie );
	if (mh == NULL) {
	    CheckAndSetErrorResult( interp, noErr );
		result = TCL_ERROR;
		goto bail;
	}
	if (objc == 0) {
		err = TCGetCurrentTimeCode( mh, &frameNum, &tcDef, &tcRec, &userData );
		if (err != noErr) {
		    CheckAndSetErrorResult( interp, err );
			result = TCL_ERROR;
			goto bail;
		}
	} else if (objc == 1) {
		if (Tcl_GetLongFromObj( interp, objv[0], &longValue ) != TCL_OK) {
			Tcl_AddErrorInfo( interp, 
					"\n	(processing time value)" );
			result = TCL_ERROR;
			goto bail;
		}
    	trackTime = longValue;
		mediaTime = TrackTimeToMediaTime( trackTime, tcTrack );
		err = TCGetTimeCodeAtTime( mh, mediaTime, &frameNum, &tcDef, &tcRec, &userData );
		if (err != noErr) {
		    CheckAndSetErrorResult( interp, err );
			result = TCL_ERROR;
			goto bail;
		}
	} else {
		Tcl_WrongNumArgs( interp, 0, objv, "pathName timecode get ?movieTime?" );
		result = TCL_ERROR;
		goto bail;
	}

	TCTimeCodeToString( mh, &tcDef, &tcRec, pasStr );
	
#if TARGET_API_MAC_CARBON
	CopyPascalStringToC( pasStr, tmpstr );
#else				
	p2cstr( (unsigned char *) pasStr );
	strcpy( tmpstr, (const char *) pasStr );
#endif

	listObjPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );
	Tcl_ListObjAppendElement( interp, listObjPtr, 
			Tcl_NewStringObj("-timecode", -1) );
	Tcl_ListObjAppendElement( interp, listObjPtr, 
			Tcl_NewLongObj(frameNum) );
	Tcl_ListObjAppendElement( interp, listObjPtr, 
			Tcl_NewStringObj("-timescale", -1) );
	Tcl_ListObjAppendElement( interp, listObjPtr, 
			Tcl_NewLongObj(tcDef.fTimeScale) );
	Tcl_ListObjAppendElement( interp, listObjPtr, 
			Tcl_NewStringObj("-frameduration", -1) );
	Tcl_ListObjAppendElement( interp, listObjPtr, 
			Tcl_NewLongObj(tcDef.frameDuration) );
	Tcl_ListObjAppendElement( interp, listObjPtr, 
			Tcl_NewStringObj("-framespersecond", -1) );
	Tcl_ListObjAppendElement( interp, listObjPtr, 
			Tcl_NewLongObj(tcDef.numFrames) );

	if (userData != NULL) {
		Handle 		nameH = NewHandleClear(0);
	
		err = GetUserDataText( userData, nameH, TCSourceRefNameType, 1, langEnglish );
		if (err == noErr) {
			HLock( nameH );
            Tcl_ListObjAppendElement( interp, listObjPtr, 
                    Tcl_NewStringObj("-sourcename", -1) );
            Tcl_ListObjAppendElement( interp, listObjPtr, 
                    Tcl_NewStringObj(*nameH, GetHandleSize(nameH)) );
            HUnlock( nameH );
		}
		if (nameH != NULL) {
			DisposeHandle( nameH );
		}
  	}    
	Tcl_SetObjResult( interp, listObjPtr );	

bail:

	if (userData != NULL) {
		DisposeUserData( userData );
	}
	return result;
}

/*---------------------------------------------------------------------------*/

