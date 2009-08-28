/*
 * EffectCommand.c --
 *
 *		Process the "effect" subcommand.
 *
 * Copyright (c) 2000-2003  Mats Bengtsson
 *
 * $Id: EffectCommand.c,v 1.5 2005/04/10 14:57:48 matben Exp $
 */

#include "MoviePlayer.h"

/*
 * Keep a global record of the effect list, and other useful stuff, 
 * for an n'th source effect. We need this since the dialog is handled in the
 * event loop, and the parameters must be transported to a callback procedure.
 */
 
typedef struct StandardEffectsRecord {
    MoviePlayer         *movPtr;
    QTParameterDialog   dialog;			/* Dialog token. 
    									 * Nonzero => serve in event loop */
    QTAtomContainer     effectList[3]; 	/* Cache for n effect list. Keep
    									 * Between calls! */
    QTAtomContainer     theSampleAtom;
    int                 nTrackEffect;
    long                trackIDs[2];
    PicHandle			posterPict[2];
    TimeValue           movTime;
    TimeValue           movDuration;
    Boolean				doneWithDialog;	/* Are we done using the effects 
    									 * parameters dialog box? */
} StandardEffectsRecord;

/*
 * The effect dialog callback needs the interpreter, and this is the only way.
 */

static Tcl_Interp   *gInterp;

/*
 * Keep a global record of the effect list, and other useful stuff, 
 * for an n'th source effect. We need this since the dialog is handled in the
 * event loop, and the parameters must be transported to a callback procedure.
 */
 
StandardEffectsRecord   *gEffectsPtr = NULL;

static void      	EffectsRespondToDialogSelection( OSErr dialogSelectType );
static void			EffectsReset( void );

#if TARGET_OS_WIN32
static void 		EffectsDialogCallback( EventRecord *theEvent, 
							DialogRef theDialog, DialogItemIndex theItemHit );
LRESULT CALLBACK 	CustomDialogWndProc( HWND theWnd, UINT theMessage, 
                         	UINT wParam, LONG lParam );
#endif


/*
 *----------------------------------------------------------------------
 *
 * EffectsInit, EffectsFree --
 *
 *	   	Initializes and frees the StandardEffectsRecord 'gEffectsPtr'.
 *
 * Results:
 *     	None.
 *
 * Side effects:
 *	   	Memory allocated, released.
 *
 *----------------------------------------------------------------------
 */

void
EffectsInit( void ) 
{
	gEffectsPtr = (StandardEffectsRecord *) ckalloc(sizeof(StandardEffectsRecord));
	memset( gEffectsPtr, '\0', sizeof(StandardEffectsRecord) );		
    gEffectsPtr->dialog = 0;
}

void
EffectsFree( void ) 
{
	int		i;
	
	EffectsReset();
	
	/* Free cached effect lists. */
	for (i = 0; i < 3; i++) {
		if (gEffectsPtr->effectList[i] != NULL) {
			QTDisposeAtomContainer( gEffectsPtr->effectList[i] );
		}
	}
	if (gEffectsPtr != NULL) {
		ckfree( (char *) gEffectsPtr);
        gEffectsPtr = NULL;
	}
}

/*
 *----------------------------------------------------------------------
 *
 * ProcessEffectCmd --
 *
 *	   	Process the "effect" command
 *
 * Results:
 *     	Normal TCL results
 *
 * Side effects:
 *	   	Effects, with dialog.
 *
 *----------------------------------------------------------------------
 */

int
ProcessEffectCmd( Tcl_Interp *interp, 
		MoviePlayer *movPtr, 
		int objc, 
		Tcl_Obj *CONST objv[] ) 
{
    int             nTrackEffect;
    long			longValue;
	TimeValue       movTime;
	TimeValue       movDuration;
	Track			aTrack = NULL;
	OSErr           err = noErr;
	int             result = TCL_OK;

    if (objc == 2) {
    
        /* Zero source effect. */
    
	    nTrackEffect = 0;
    } else if (objc == 3) {
    
        /* Single source effect. */
    
	    nTrackEffect = 1;
		if (Tcl_GetLongFromObj( interp, objv[2], &longValue ) != TCL_OK) {
			Tcl_AddErrorInfo( interp, "\n	(processing trackOneID value)" );
			result = TCL_ERROR;
			goto bail;
		}
	    gEffectsPtr->trackIDs[0] = longValue;
    } else if (objc == 4) {
    
        /* Two source effect. */
    
	    nTrackEffect = 2;
		if (Tcl_GetLongFromObj( interp, objv[2], &longValue ) != TCL_OK) {
			Tcl_AddErrorInfo( interp, "\n	(processing trackOneID value)" );
			result = TCL_ERROR;
			goto bail;
		}
	    gEffectsPtr->trackIDs[0] = longValue;
		if (Tcl_GetLongFromObj( interp, objv[3], &longValue ) != TCL_OK) {
			Tcl_AddErrorInfo( interp, "\n	(processing trackTwoID value)" );
			result = TCL_ERROR;
			goto bail;
		}
	    gEffectsPtr->trackIDs[1] = longValue;
  	} else {
		Tcl_WrongNumArgs( interp, 0, objv, 
				"pathName effect startTime duration ?trackID? ?trackID?" );
		result = TCL_ERROR;
		goto bail;
  	}
	gEffectsPtr->nTrackEffect = nTrackEffect;

    if (GetMovieStartTimeFromObj( interp, movPtr->aMovie, objv[0], &longValue ) != TCL_OK) {
        goto bail;
    } 
    movTime = longValue;
    if (GetMovieDurationFromObj( interp, movPtr->aMovie, objv[1], movTime, &longValue ) != TCL_OK) {
        goto bail;
    } 
    movDuration = longValue;

	gEffectsPtr->movTime = movTime;
	gEffectsPtr->movDuration = movDuration;
	gEffectsPtr->movPtr = movPtr;
	result = LogUndoState( movPtr, interp );

    /*
  	 * The effect is going on async; need variable to wait for.
  	 */

  	Tcl_SetVar( interp, "quicktimetcl::effectfinished", "-1", TCL_GLOBAL_ONLY );
   	gInterp = interp;

  	err = QTNewAtomContainer( &gEffectsPtr->theSampleAtom );
  	if (err != noErr) {
        CheckAndSetErrorResult( interp, err );
		result = TCL_ERROR;
		goto bail;
	}

    /*
     * If we haven't made and cached the effect list for this n'th source effect,
     * do it now.
     */

  	if (gEffectsPtr->effectList[nTrackEffect] == NULL) {
       	err = QTGetEffectsList( 
                  &(gEffectsPtr->effectList[nTrackEffect]), 
                  nTrackEffect,
                  nTrackEffect, 0 );
       	if (err != noErr) {
          	CheckAndSetErrorResult( interp, err );
  			result = TCL_ERROR;
  			goto bail;
      	}
  	}

   	/*
   	 * Make the standard parameter effects dialog. 
   	 * Needs to be served in the event loop.
     * From the event loop the actual procedure 'EffectsRespondToDialogSelection' 
     * is called to make the effect.
     */
      
   	err = QTCreateStandardParameterDialog( 
              gEffectsPtr->effectList[nTrackEffect],/* the effects list */
              gEffectsPtr->theSampleAtom,          	/* empty container */
              0,
              &(gEffectsPtr->dialog) );          	/* identifier for the 
              										 * standard dialog */
   	if (noErr != err) {
       	CheckAndSetErrorResult( interp, err );
		result = TCL_ERROR;
		goto bail;
  	}
      
	/*
	 *	Insert poster frames into dialog.
	 */
	 
	if (nTrackEffect >= 1) {
		aTrack = GetMovieTrack( movPtr->aMovie, gEffectsPtr->trackIDs[0] );
		if (aTrack != NULL) {
			gEffectsPtr->posterPict[0] = GetTrackPict( aTrack, 
					GetMoviePosterTime( movPtr->aMovie ));
			if (gEffectsPtr->posterPict[0] != NULL) {
				QTParamPreviewRecord			myPreviewRecord;

				myPreviewRecord.sourcePicture = gEffectsPtr->posterPict[0];
				myPreviewRecord.sourceID = 1;
				err = QTStandardParameterDialogDoAction( gEffectsPtr->dialog, 
						pdActionSetPreviewPicture, &myPreviewRecord );
			}
		}
	}
	/* not very clever since it is the same movie...
	if (nTrackEffect >= 2) {
		aTrack = GetMovieTrack( movPtr->aMovie, gEffectsPtr->trackIDs[1] );
		if (aTrack != NULL) {
			gEffectsPtr->posterPict[1] = GetTrackPict( aTrack, 
					GetMoviePosterTime( movPtr->aMovie ));
			if (gEffectsPtr->posterPict[1] != NULL) {
				QTParamPreviewRecord			myPreviewRecord;

				myPreviewRecord.sourcePicture = gEffectsPtr->posterPict[1];
				myPreviewRecord.sourceID = 2;
				err = QTStandardParameterDialogDoAction( gEffectsPtr->dialog, 
						pdActionSetPreviewPicture, &myPreviewRecord );
			}
		}
	}
	*/
	
	/*
	 *	Now, the frontmost window is the standard effects parameter dialog box;
	 * 	on the Mac, we call EffectsHandleDialogEvents in our main event loop
	 *  to find and process events targeted at the effects parameter dialog box; 
	 *	On Windows, we need to use a different strategy: we install a modeless 
	 *	dialog callback procedure that is called internally by QTML.
	 */

#if TARGET_OS_WIN32
	gEffectsPtr->doneWithDialog = false;
	
	/* Force the dialog box to be drawn. */
	{
		EventRecord			myEvent = {0};
		
		EffectsDialogCallback( &myEvent, FrontWindow(), 0 );
	}
	SetModelessDialogCallbackProc( FrontWindow(), 
			(QTModelessCallbackProcPtr) EffectsDialogCallback );
	QTMLSetWindowWndProc( FrontWindow(), (void *) CustomDialogWndProc );
#endif
      
      /* Continues in 'EffectsRespondToDialogSelection'... */

bail:
	if (result != TCL_OK) {
	}
	return result;
}

/*
 *----------------------------------------------------------------------
 *
 * EffectsHandleDialogEvents --
 *
 *      Process events that might be targeted at the standard effects 
 *		parameter dialog box. Return true if the event was completely 
 *		handled.
 *
 * Results:
 *		Boolean telling if event was handled or not.
 *
 * Side effects:
 *  	Dialog procedures called.
 *
 *----------------------------------------------------------------------
 */

Boolean
EffectsHandleDialogEvents( EventRecord *eventPtr, DialogItemIndex itemIndex )
{
    Boolean     isHandled;
    OSErr       err = noErr;
    
    if (eventPtr == NULL) {
        return false;
    }
    if (gEffectsPtr == NULL) {
        return false;
    }
    if (gEffectsPtr->dialog == 0) {
    	return false;
    }
    err = QTIsStandardParameterDialogEvent( eventPtr, gEffectsPtr->dialog );

    switch (err) {
        
        case codecParameterDialogConfirm:
        case userCanceledErr:
            
            /* The user clicked the OK or Cancel button.*/
            
			QTStandardParameterDialogDoAction( gEffectsPtr->dialog, pdActionConfirmDialog, NULL );
			QTDismissStandardParameterDialog( gEffectsPtr->dialog );
			gEffectsPtr->dialog = 0L;
			EffectsRespondToDialogSelection( err );
			isHandled = true;

            /*
             * The effect is going on async; set variable to wait for.
             */

            if (err == userCanceledErr) {
                Tcl_SetVar( gInterp, "quicktimetcl::effectfinished", "0", TCL_GLOBAL_ONLY );
            } else {
                Tcl_SetVar( gInterp, "quicktimetcl::effectfinished", "1", TCL_GLOBAL_ONLY );
            }
			break;
			
		case noErr:
		
			/* 
			 * The event was completely handled by 
			 * QTIsStandardParameterDialogEvent
			 */
			
			isHandled = true;
			break;
			
		case featureUnsupported:
		
			/*
			 * The event was not handled by QTIsStandardParameterDialogEvent;
			 * let the event be processed normally.
			 */
			
			isHandled = false;
			break;
			
		default:
		
			/*
			 * The event was not handled by QTIsStandardParameterDialogEvent;
			 * do not let the event be processed normally.
			 */
			
			isHandled = true;
			break;
	}
	return( isHandled );           
}

/*
 *----------------------------------------------------------------------
 *
 * EffectsRespondToDialogSelection --
 *
 *     	If dialogSelectType is codecParameterDialogConfirm, make an 
 *		effects movie.
 *     	If dialogSelectType is userCanceledErr, do any necessary clean up.
 *
 * Results:
 *	   	None.
 *
 * Side effects:
 *	   	New source tracks created, effect track created.
 *
 *----------------------------------------------------------------------
 */

static void 
EffectsRespondToDialogSelection( OSErr dialogSelectType ) 
{
	MoviePlayer     *movPtr;
	int             nTrackEffect = gEffectsPtr->nTrackEffect;
	TimeValue       movTime = gEffectsPtr->movTime;
	TimeValue       movDuration = gEffectsPtr->movDuration;
	TimeValue       sampleTime;
	Track           origTrack[2] = {NULL, NULL};
	Track           srcTrack[2] = {NULL, NULL};
	Track           videoTrackFX = NULL;
	Track           aTrack = NULL;
	Media           myMedia = NULL;
	Media           videoMediaFX = NULL;
	Fixed           fixWidth;
	Fixed           fixHeight;
	Boolean		    dialogWasCancelled = false;
	QTAtomContainer	theInputMapContainer = NULL;
	QTAtom			inputAtom;
	QTAtom			effectAtom;
	QTAtomID		effectAtomID;
	OSType          theEffectCode;
	OSType			theInputType;
	OSErr			err = noErr;
	long			effectCodeSize;
	Ptr				effectCodePtr = NULL;
	ImageDescriptionHandle	theDesc = NULL;
	MediaHandler	mediaHandler;
	RGBColor        transparentColor = {0x0000, 0x0000, 0x0000};
	int             copyMovieMedia = false;
	int 		    timeScale;
	int             i;
	long	        aLong;
	long			refIndex1, refIndex2;
	int				result = TCL_OK;
	
    movPtr = gEffectsPtr->movPtr;
    if (movPtr->aMovie == NULL) {
    	result = TCL_ERROR;
     	goto bail;
    }
	dialogWasCancelled = (dialogSelectType == userCanceledErr);
	if (dialogWasCancelled) {
    	result = TCL_ERROR;
     	goto bail;
    }
        
    /*
     * First, create any source tracks that is needed by the effect, 
     * and copy the media from the original track.
     */
     
    for (i = 0; i <  nTrackEffect; i++) {
		origTrack[i] = GetMovieTrack( movPtr->aMovie, gEffectsPtr->trackIDs[i] );
		if (origTrack[i] == NULL) {
			CheckAndSetErrorResult( gInterp, noErr );
	    	result = TCL_ERROR;
            goto bail;
		}
        AddEmptyTrackToMovie( origTrack[i], movPtr->aMovie, NULL, 0, 
        		&srcTrack[i] );
        if (srcTrack[i] == NULL) {
			CheckAndSetErrorResult( gInterp, noErr );
	    	result = TCL_ERROR;
            goto bail;
        }
		myMedia = GetTrackMedia( srcTrack[i] );
		if (myMedia == NULL) {
			CheckAndSetErrorResult( gInterp, noErr );
	    	result = TCL_ERROR;
            goto bail;
		}
        if (copyMovieMedia) {
			if (noErr != BeginMediaEdits( myMedia )) {
				CheckAndSetErrorResult( gInterp, noErr );
		    	result = TCL_ERROR;
            	goto bail;
			}
        }         
	    if (noErr != (err = InsertTrackSegment( 
	    		origTrack[i],  			/* source track */
	            srcTrack[i],           	/* destination track */
	            movTime,                /* start time */
	            movDuration,            /* duration */
	            movTime ))) {        	/* insert time */
			CheckAndSetErrorResult( gInterp, err );
	    	result = TCL_ERROR;
            goto bail;
	    }          
        if (copyMovieMedia) {
			if (noErr != EndMediaEdits( myMedia )) {
				CheckAndSetErrorResult( gInterp, noErr );
		    	result = TCL_ERROR;
	            goto bail;
			}
        }         
    }

	/* 
	 * Extract the 'what' atom to find out what kind of effect it is. 
	 */

	effectAtom = QTFindChildByIndex( gEffectsPtr->theSampleAtom, 
			kParentAtomIsContainer, kParameterWhatName, kParameterWhatID, 
			&effectAtomID );
	
	if (noErr != QTLockContainer( gEffectsPtr->theSampleAtom )) {
		CheckAndSetErrorResult( gInterp, noErr );
	    result = TCL_ERROR;
     	goto bail;
    }
	if (noErr != QTGetAtomDataPtr( gEffectsPtr->theSampleAtom, effectAtom, 
			&effectCodeSize, &effectCodePtr )) {
		CheckAndSetErrorResult( gInterp, noErr );
	    result = TCL_ERROR;
     	goto bail;
    }
	if (effectCodeSize != sizeof(OSType)) {
	    result = TCL_ERROR;
     	goto bail;
	}
	theEffectCode = *(OSType *) effectCodePtr;		/* "tsk" */
	
	/* Because the data is read from an atom container. */
	theEffectCode = EndianU32_BtoN(theEffectCode);	
	if (noErr != QTUnlockContainer( gEffectsPtr->theSampleAtom )) {
		CheckAndSetErrorResult( gInterp, noErr );
	    result = TCL_ERROR;
     	goto bail;
	}

    /*
     * Make an empty effect track.
     */
    
    if (nTrackEffect > 0) {
        GetTrackDimensions( origTrack[0], &fixWidth, &fixHeight );
    } else {
    
        /* 
         * Here we search for a visual track and pick its dimensions. 
         */
        
        if (GetFirstVideoTrackInMovie( movPtr->aMovie, &aTrack ) == invalidTrack) {
            fixWidth = Long2Fix(160);
            fixHeight = Long2Fix(120);
        } else {
            GetTrackDimensions( aTrack, &fixWidth, &fixHeight );
        }    
    }
    videoTrackFX = NewMovieTrack( movPtr->aMovie, fixWidth, fixHeight, kNoVolume );
	if (videoTrackFX == NULL) {
		CheckAndSetErrorResult( gInterp, noErr );
	    result = TCL_ERROR;
     	goto bail;
    }	
    timeScale = GetMovieTimeScale( movPtr->aMovie );
	videoMediaFX = NewTrackMedia( videoTrackFX, VideoMediaType, timeScale, 
			NULL, 0 );
	if (videoMediaFX == NULL) {
		CheckAndSetErrorResult( gInterp, noErr );
	    result = TCL_ERROR;
     	goto bail;
	}
    
    /*
     * An effect track has two important attributes: the effect description 
     * and the input map.
     * The effect description requires two things: 
     *  1)  The actual description, which's name and parameters are already 
     *		inserted in the atom container 'gEffectsPtr->theSampleAtom' 
     *		using 'QTCreateStandardParameterDialog'.
     *  2)  The second is atoms naming the sources.
     *
     * Add atoms naming the sources.
     */

	if (nTrackEffect >= 1) {
		aLong = EndianU32_NtoB( kSourceOneName );
		QTInsertChild( gEffectsPtr->theSampleAtom, kParentAtomIsContainer, 
				kEffectSourceName, 1, 0, sizeof(aLong), &aLong, NULL );
	}		
	if (nTrackEffect >= 2) {
		aLong = EndianU32_NtoB( kSourceTwoName );
		QTInsertChild( gEffectsPtr->theSampleAtom, kParentAtomIsContainer, 
				kEffectSourceName, 2, 0, sizeof(aLong), &aLong, NULL );
	}

	theDesc = (ImageDescriptionHandle) NewHandleClear(sizeof(ImageDescription));
	if (theDesc == NULL) {
		CheckAndSetErrorResult( gInterp, noErr );
	    result = TCL_ERROR;
     	goto bail;
	}
	
	/*
	 * Fill in the fields of the sample description.
	 */
	 
	(**theDesc).idSize = sizeof(ImageDescription);
	(**theDesc).cType = theEffectCode;
	(**theDesc).vendor = kAppleManufacturer;
	(**theDesc).temporalQuality = codecNormalQuality;
	(**theDesc).spatialQuality = codecNormalQuality;
	(**theDesc).width = (short) Fix2Long( fixWidth );
	(**theDesc).height = (short) Fix2Long( fixHeight );
	(**theDesc).hRes = 72L << 16;
	(**theDesc).vRes = 72L << 16;
	(**theDesc).dataSize = 0L;
	(**theDesc).frameCount = 1;
	(**theDesc).depth = 0;
	(**theDesc).clutID = -1;
	
	/*
	 *	Add effects sample to movie.
	 */
	 
	if (noErr != BeginMediaEdits( videoMediaFX )) {
		CheckAndSetErrorResult( gInterp, noErr );
	    result = TCL_ERROR;
     	goto bail;
    }	
	if (noErr != AddMediaSample( videoMediaFX, gEffectsPtr->theSampleAtom, 0,
	        GetHandleSize( gEffectsPtr->theSampleAtom ), movDuration, 
	        (SampleDescriptionHandle) theDesc, 1, 0, &sampleTime )) {
		CheckAndSetErrorResult( gInterp, noErr );
	    result = TCL_ERROR;
     	goto bail;
    }	
	if (noErr != EndMediaEdits( videoMediaFX )) {
		CheckAndSetErrorResult( gInterp, noErr );
	    result = TCL_ERROR;
     	goto bail;
    }	
	if (noErr != InsertMediaIntoTrack( videoTrackFX,/* the track */
			movTime, 								/* insert time in movie's 
													 * time */
			sampleTime, 							/* media start time in 
													 * media's time */
			movDuration, 							/* media's duration in 
													 * media's time */
			fixed1 )) {								/* the media's rate */
		CheckAndSetErrorResult( gInterp, noErr );
	    result = TCL_ERROR;
     	goto bail;
	}
	if (gEffectsPtr->theSampleAtom != NULL) {
		QTDisposeAtomContainer( gEffectsPtr->theSampleAtom );
		gEffectsPtr->theSampleAtom = NULL;
	}
	DisposeHandle( (Handle) theDesc );
     
    /*
     * Create and add the input map.
     */
     
	QTNewAtomContainer( &theInputMapContainer );

	/* The first input. */
	
	if (nTrackEffect > 0) {
		AddTrackReference( videoTrackFX, srcTrack[0], kTrackModifierReference, 
		        &refIndex1 );
		QTInsertChild( theInputMapContainer, kParentAtomIsContainer, 
				kTrackModifierInput, refIndex1, 0, 0, NULL, &inputAtom );

		theInputType = EndianU32_NtoB( kTrackModifierTypeImage );
		QTInsertChild( theInputMapContainer, inputAtom, kTrackModifierType, 
				1, 0, sizeof(theInputType), &theInputType, NULL );

		aLong = EndianU32_NtoB( kSourceOneName );
		QTInsertChild( theInputMapContainer, inputAtom, kEffectDataSourceType, 
				1, 0, sizeof(aLong), &aLong, NULL );
	}

	/* The second input. */
	
	if (nTrackEffect > 1) {
		AddTrackReference( videoTrackFX, srcTrack[1], kTrackModifierReference, 
		        &refIndex2 );
		QTInsertChild( theInputMapContainer, kParentAtomIsContainer, 
				kTrackModifierInput, refIndex2, 0, 0, NULL, &inputAtom );

		theInputType = EndianU32_NtoB( kTrackModifierTypeImage );
		QTInsertChild( theInputMapContainer, inputAtom, kTrackModifierType, 
				1, 0, sizeof(theInputType), &theInputType, NULL );

		aLong = EndianU32_NtoB( kSourceTwoName );
		QTInsertChild( theInputMapContainer, inputAtom, kEffectDataSourceType, 
				1, 0, sizeof(aLong), &aLong, NULL );
	}

	/* 
	 * Set that mapping. 
	 */
	
	if (nTrackEffect > 0) {
		SetMediaInputMap( GetTrackMedia(videoTrackFX), theInputMapContainer );
	}
	
	/* 
	 * Set transparent graphics mode for the zero source effect track. 
	 */
	
	if (nTrackEffect == 0) {
		mediaHandler = GetMediaHandler( videoMediaFX );
        MediaSetGraphicsMode( mediaHandler, transparent, &transparentColor );
	}

bail:

	if (result != TCL_OK) {
	  	if (srcTrack[0] != NULL) {
	  		DisposeMovieTrack( srcTrack[0] );
	  	}
	  	if (srcTrack[1] != NULL) {
	  		DisposeMovieTrack( srcTrack[1] );
	  	}
	  	if (videoTrackFX != NULL) {
	  		DisposeMovieTrack( videoTrackFX );
	  	}	
	}	   	
	if (theInputMapContainer != NULL) {
		QTDisposeAtomContainer( theInputMapContainer );
	}
  	if (theDesc != NULL) {
		DisposeHandle( (Handle) theDesc );
	}
	EffectsReset();
  	
  	return;
}

/*
 *----------------------------------------------------------------------
 *
 * EffectsReset --
 *
 *		Resets the StandardEffectsRecord 'gEffectsPtr'. Kills any
 *		poster picts. Keeps any effect lists untouched!
 *
 * Results:
 *		None.
 *
 * Side effects:
 *		Picts may be killed. Not served in event loop.
 *
 *----------------------------------------------------------------------
 */

static void
EffectsReset( void )
{
    if (gEffectsPtr == NULL) {
        return;
    }
    gEffectsPtr->movPtr = NULL;
	gEffectsPtr->dialog = 0L;
	gEffectsPtr->trackIDs[0] = 0;
	gEffectsPtr->trackIDs[1] = 0;
	gEffectsPtr->nTrackEffect = 0;
	gEffectsPtr->doneWithDialog = false;
	if (gEffectsPtr->posterPict[0] != NULL) {
		KillPicture( gEffectsPtr->posterPict[0] );
		gEffectsPtr->posterPict[0] = NULL;
	}
	if (gEffectsPtr->posterPict[1] != NULL) {
		KillPicture( gEffectsPtr->posterPict[1] );
		gEffectsPtr->posterPict[1] = NULL;
	}
	if (gEffectsPtr->theSampleAtom != NULL) {
		QTDisposeAtomContainer( gEffectsPtr->theSampleAtom );
		gEffectsPtr->theSampleAtom = NULL;
	}
}

/*
 *----------------------------------------------------------------------
 *
 * EffectsDialogCallback --
 *
 *		This function is called by QTML when it processes events for 
 *		the standard or custom effects parameter dialog box.
 *
 * Results:
 *		None.
 *
 * Side effects:
 *		EffectsHandleDialogEvents called.
 *
 *----------------------------------------------------------------------
 */
#if TARGET_OS_WIN32 || TARGET_API_MAC_CARBON

static void 
EffectsDialogCallback( EventRecord *theEvent, DialogRef theDialog, 
		DialogItemIndex theItemHit )
{
	QTParamDialogEventRecord	myRecord;

	myRecord.theEvent = theEvent;
	myRecord.whichDialog = theDialog;
	myRecord.itemHit = theItemHit;

	if (gEffectsPtr->dialog != 0L) {
		QTStandardParameterDialogDoAction( gEffectsPtr->dialog, 
				pdActionModelessCallback, &myRecord );
	
		/*
		 * See if the event is meant for the effects parameter dialog box.
		 */
		 
		EffectsHandleDialogEvents( theEvent, theItemHit );
	}
}
#endif

#if TARGET_OS_WIN32
/*
 *----------------------------------------------------------------------
 *
 * CustomDialogWndProc --
 *
 *		Handle messages for the custom effects parameters dialog box.
 *
 * Results:
 *		DefWindowProc.
 *
 * Side effects:
 *		DefWindowProc called.
 *
 *----------------------------------------------------------------------
 */

LRESULT CALLBACK 
CustomDialogWndProc( HWND theWnd, UINT theMessage, UINT wParam, LONG lParam )
{
	EventRecord			myEvent = {0};

	if (!gEffectsPtr->doneWithDialog && (theMessage == 0x7FFF))
		EffectsDialogCallback( &myEvent, GetNativeWindowPort(theWnd), 0 );

	return(DefWindowProc( theWnd, theMessage, wParam, lParam ));
}
#endif

/*---------------------------------------------------------------------------*/
