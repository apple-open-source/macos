/*
 * MovieUtils.c --
 *
 *		Non-Tk Utility functions, part of QuickTimeTcl.
 *
 * Copyright (c) 2000-2003  Mats Bengtsson
 *
 * $Id: MovieUtils.c,v 1.2 2005/04/02 13:41:26 matben Exp $
 */

#include "MoviePlayer.h"


/*
 *----------------------------------------------------------------------
 *
 * GetFirstVideoTrackInMovie --
 *
 *	 	Return, through the theTrack parameter, the first video track in 
 *		the specified movie. Actually, we look for the first track that 
 *		has the kCharacteristicCanSendVideo characteristic, so we can 
 *		apply effects to MPEG or QD3D tracks as well.
 *
 * Results:
 *  	OSErr: noErr if found track, returned in '*theTrack', 
 *		or 'invalidTrack' if none.
 *
 * Side effects:
 *		None.
 *
 *----------------------------------------------------------------------
 */

OSErr 
GetFirstVideoTrackInMovie( Movie theMovie, Track *theTrack )
{
	*theTrack = GetMovieIndTrackType( theMovie, 1, kCharacteristicCanSendVideo, 
	        movieTrackCharacteristic | movieTrackEnabledOnly );
	
	if (*theTrack == NULL) {
		return(invalidTrack);
	}	
	return(noErr);
}

/*
 *----------------------------------------------------------------------
 *
 * ShowControllerButton, HideControllerButton --
 *
 *		Shows or hides controller buttons.
 *
 * Results:
 *  	None.
 *
 * Side effects:
 *		None.
 *
 *----------------------------------------------------------------------
 */

void 
ShowControllerButton( MovieController theMC, long theButton ) 
{
	long	flags;
	
	/* Handle the custom button separately. */
	if (theButton == mcFlagsUseCustomButton) {
		MCDoAction( theMC, mcActionGetFlags, &flags );
		MCDoAction( theMC, mcActionSetFlags, 
				(void *)((flags | theButton)));
	} else {
	
		/*
		 * Get the current explicit flags and set the explicit flag for the 
		 * specified button.
		 */
		 
		flags = mcFlagQTVRExplicitFlagSet;
		MCDoAction(theMC, mcActionGetFlags, &flags);
		MCDoAction(theMC, mcActionSetFlags, 
				(void *)((flags | theButton) | mcFlagQTVRExplicitFlagSet));
		
		/*
		 * Get the current control flags and clear the suppress flag for the 
		 * specified button.
		 */
		 
		flags = 0;
		MCDoAction(theMC, mcActionGetFlags, &flags);
		MCDoAction(theMC, mcActionSetFlags, 
				(void *)(flags & ~theButton & ~mcFlagQTVRExplicitFlagSet));
	}
}

void 
HideControllerButton( MovieController theMC, long theButton ) 
{
	long	flags;
	
	/* Handle the custom button separately. */
	if (theButton == mcFlagsUseCustomButton) {
		MCDoAction(theMC, mcActionGetFlags, &flags);
		MCDoAction(theMC, mcActionSetFlags, (void *)(flags & ~theButton));
	} else {
		flags = mcFlagQTVRExplicitFlagSet;
		MCDoAction(theMC, mcActionGetFlags, &flags);
		MCDoAction(theMC, mcActionSetFlags, 
				(void *)((flags | theButton) | mcFlagQTVRExplicitFlagSet));
		
		flags = 0;
		MCDoAction(theMC, mcActionGetFlags, &flags);
		MCDoAction(theMC, mcActionSetFlags, 
				(void *)((flags | theButton) & ~mcFlagQTVRExplicitFlagSet));
	}
}

/*
 *----------------------------------------------------------------------
 *
 * SetLoopingState --
 *
 *		Sets the movies looping state via its controller.
 *
 * Results:
 *  	None.
 *
 * Side effects:
 *		None.
 *
 *----------------------------------------------------------------------
 */
 
void
SetLoopingState( MovieController theMC, UInt8 theLoopState )
{

	switch (theLoopState) {

		case kQTTclNormalLooping:
			MCDoAction( theMC, mcActionSetLooping, (void *) true );
			MCDoAction( theMC, mcActionSetLoopIsPalindrome, (void *) false );
			break;			

		case kQTTclPalindromeLooping:
			MCDoAction( theMC, mcActionSetLooping, (void *) true );
			MCDoAction( theMC, mcActionSetLoopIsPalindrome, (void *) true );
			break;
			
		case kQTTclNoLooping:
			MCDoAction( theMC, mcActionSetLooping, (void *) false );
			MCDoAction( theMC, mcActionSetLoopIsPalindrome, (void *) false );

		default:
			break;
	}
}	

/*
 *----------------------------------------------------------------------
 *
 * SetMovieLoopState --
 *
 *		Sets the movies looping state if it doesn't have a controller.
 *
 * Results:
 *  	None.
 *
 * Side effects:
 *		None.
 *
 *----------------------------------------------------------------------
 */
 
void
SetMovieLoopState( Movie theMovie, UInt8 theLoopState )
{
	TimeBase		myTimeBase;
	long			myFlags = 0L;
	
	if (theMovie == NULL)
		return;
		
	myTimeBase = GetMovieTimeBase( theMovie );
	myFlags = GetTimeBaseFlags( myTimeBase );
	
	switch (theLoopState) {
	
		case kQTTclNormalLooping:
			SetMoviePlayHints( theMovie, hintsLoop, hintsLoop );
			SetMoviePlayHints( theMovie, 0L, hintsPalindrome );
			myFlags |= loopTimeBase;
			myFlags &= ~palindromeLoopTimeBase;
			break;
			
		case kQTTclPalindromeLooping:
			SetMoviePlayHints( theMovie, hintsLoop, hintsLoop );
			SetMoviePlayHints( theMovie, hintsPalindrome, hintsPalindrome );
			myFlags |= loopTimeBase;
			myFlags |= palindromeLoopTimeBase;
			break;
			
		case kQTTclNoLooping:
			myFlags &= ~loopTimeBase;
			myFlags &= ~palindromeLoopTimeBase;
			SetMoviePlayHints( theMovie, 0L, hintsLoop | hintsPalindrome );
			break;
		}
	SetTimeBaseFlags( myTimeBase, myFlags );
}

/*
 *----------------------------------------------------------------------
 *
 * IsMovieLooping --
 *
 *		Returns a boolean of 'true' if the movie is in a looping state, 
 *  	and 'false' else.
 *
 * Results:
 *  	Boolean.
 *
 * Side effects:
 *		None.
 *
 *----------------------------------------------------------------------
 */

Boolean
IsMovieLooping( Movie theMovie )
{
	TimeBase	myTimeBase;
	long		myFlags = 0L;
	
	if (theMovie == NULL)
		return(false);
	
	myTimeBase = GetMovieTimeBase( theMovie );
	myFlags = GetTimeBaseFlags( myTimeBase );
	return((Boolean) (myFlags & loopTimeBase));
}			
/*
 *----------------------------------------------------------------------
 *
 * MoviePrePrerollCompleteProc --
 *
 *	.
 *
 * Results:
 *  None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
MoviePrePrerollCompleteProc( Movie theMovie, OSErr err, void *refConst )
{
	TimeValue		myTimeValue;
	Fixed			myPlayRate;
	
	if (theMovie == NULL) {
		return;
	}
	// Preroll the movie so that it's ready to play.
	myTimeValue = GetMovieTime( theMovie, NULL );
	myPlayRate = GetMoviePreferredRate( theMovie );
}

/*
 *----------------------------------------------------------------------
 *
 * GetControllerType --
 *
 *	Gets a movie's type of controller by examining the movie's user data.
 *
 * Results:
 *  Returns the movie's controller type which is a four-character code.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

OSType
GetControllerType( Movie theMovie )
{
    UserData    theUserData;
    OSType      theType = kUnknownType;
    OSErr       err = noErr;
    
    if (theMovie == NULL) {
        return(theType);
    }    
    theUserData = GetMovieUserData( theMovie );
    if (theUserData != NULL) {
        err = GetUserDataItem( theUserData, &theType, sizeof(theType), 
                kUserDataMovieControllerType, 0 );
        if (err == noErr) {
            theType = EndianU32_BtoN( theType );
        }    
    }          
    return(theType);
}

/*
 *----------------------------------------------------------------------
 *
 * IsQTVRMovie --
 *
 *  Returns a Boolean value of 'true' if we have got a VR panoramic movie,
 *  and 'false' otherwise.
 *
 * Results:
 *  Returns a Boolean value of 'true' if we have got a VR panoramic movie,
 *  and 'false' otherwise.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Boolean
IsQTVRMovie( Movie theMovie )
{
    Boolean     isQTVRMovie = false;
    OSType      theType = kUnknownType;

    /* 
     * QTVR panos have a special piece of user data identifying the movie 
     * controller type. 
     */
    
    theType = GetControllerType( theMovie );
    if ((theType == kQTVRQTVRType) || (theType == kQTVROldPanoType) || 
            (theType == kQTVROldObjectType)) {
        isQTVRMovie = true;
    }
    return(isQTVRMovie);
}

/*
 *----------------------------------------------------------------------
 *
 * GetControllerBarHeight --
 *
 *  Return the height of the controller bar displayed by the movie controller.
 *	Note that MCGetControllerBoundsRect returns rectangle of bar and movie, 
 *	if attached, so we need to unattach the controller bar first.
 *
 * Results:
 *  the height
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

short GetControllerBarHeight( MovieController mc ) 
{
	Boolean		wasAttached = false;
	Rect		myRect;
	short		myHeight = 0;
	
	/* If the controller bar is attached, detach it (and remember we did so) */
	if (MCIsControllerAttached(mc) == 1) {
		wasAttached = true;
		MCSetControllerAttached( mc, false );
	}
	
	/* Get the rectangle of the controller */
	MCGetControllerBoundsRect( mc, &myRect );
	myHeight = myRect.bottom - myRect.top;
	
	/* Now reattach the controller bar, if it was originally attached */
	if (wasAttached) {
		MCSetControllerAttached( mc, true );
	}
	return(myHeight);
}

/*
 *----------------------------------------------------------------------
 *
 * GetQTVRInstanceFromController --
 *
 *  Returns either the QTVR instance or NULL for non QTVR movies.
 *
 * Results:
 *  QTVR instance or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

QTVRInstance
GetQTVRInstanceFromController( MovieController mc )
{
    Track           track = NULL;
    QTVRInstance    qtvrInstance = NULL;
    Movie           aMovie = NULL;
    
    aMovie = MCGetMovie( mc );
    if (aMovie != NULL) {
        track = QTVRGetQTVRTrack( aMovie, 1 );
        if (track != NULL) {
            QTVRGetQTVRInstance( &qtvrInstance, track, mc );
            
            /* Set units to degrees */
            if (qtvrInstance != NULL) {
                QTVRSetAngularUnits( qtvrInstance, kQTVRDegrees );
            }
        }
    }       
    return qtvrInstance;
}

/*
 *----------------------------------------------------------------------
 *
 * TranslateTclListToMatrixRecord --
 *
 *		Takes a tcl list, and fills in the matrix record.
 *
 * Results:
 *		The return value is a standard Tcl result.  If TCL_ERROR is
 *		returned, then the interp's result contains an error message.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
TranslateTclListToMatrixRecord( Tcl_Interp *interp, 
		Tcl_Obj *listObjPtr,
		MatrixRecord *mr )
{
    int         i;
    int         j;
    int			len;
    double      doubleElement;
	Tcl_Obj 	*subListObjPtr;
	Tcl_Obj		*objPtr;
    
	if (Tcl_ListObjLength( interp, listObjPtr, &len ) != TCL_OK) {
		Tcl_AddErrorInfo( interp, 
				"\n	(processing matrix object)" );
		return TCL_ERROR;
	}
    if (len != 3) {
        Tcl_SetObjResult( interp, Tcl_NewStringObj( 
        		"Wrong number of items in matrix list", -1 ) );
		return TCL_ERROR;
    }
	for (i = 0; i < 3; i++) {			
		if (Tcl_ListObjIndex( interp, listObjPtr, i, &subListObjPtr ) != TCL_OK) {
			Tcl_AddErrorInfo( interp, 
					"\n	(processing matrix object)" );
			return TCL_ERROR;
		}
		if (Tcl_ListObjLength( interp, subListObjPtr, &len ) != TCL_OK) {
			Tcl_AddErrorInfo( interp, 
					"\n	(processing matrix object)" );
			return TCL_ERROR;
		}
	    if (len != 3) {
	        Tcl_SetObjResult( interp, Tcl_NewStringObj( 
	        		"Wrong number of items in matrix list", -1 ) );
			return TCL_ERROR;
	    }
        
        /*
         * Loop over all columns for this row.
         */
         
        for (j = 0; j < 2; j++) {
			if (Tcl_ListObjIndex( interp, subListObjPtr, j, &objPtr ) 
					!= TCL_OK) {
				Tcl_AddErrorInfo( interp, 
						"\n	(processing matrix object)" );
				return TCL_ERROR;
			}
        	if (Tcl_GetDoubleFromObj( interp, objPtr, &doubleElement ) 
        			!= TCL_OK) {
				Tcl_AddErrorInfo( interp, 
						"\n	(processing matrix object)" );
				return TCL_ERROR;
			}	
            mr->matrix[i][j] = X2Fix( doubleElement );
        }
    }

    mr->matrix[0][2] = X2Frac(0.0);
    mr->matrix[1][2] = X2Frac(0.0);
    mr->matrix[2][2] = X2Frac(1.0);
    
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TranslateMatrixRecordToTclList --
 *
 *		Takes a matrix record and fills in the tcl object as a 3x3 tcl 
 *      list-sublist.
 *
 * Results:
 *		The return value is a standard Tcl result. 
 *		3x3 matrix of Tcl_Obj* put in listObjPtrPtr.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
TranslateMatrixRecordToTclList( Tcl_Interp *interp, 
		Tcl_Obj **listObjPtrPtr, 
        MatrixRecord *mr )
{
    int         i;
    int         j;
    double      doubleElement;
    Tcl_Obj     *rowObj;
    
    *listObjPtrPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );

    for (i = 0; i < 3; i++) {
        rowObj = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );
        for (j = 0; j < 3; j++) {
            if (j == 2) {
                doubleElement = Frac2X( mr->matrix[i][j] );
            } else {
                doubleElement = Fix2X( mr->matrix[i][j] );
            }
            Tcl_ListObjAppendElement( interp, rowObj, 
                    Tcl_NewDoubleObj(doubleElement) );
        }
        Tcl_ListObjAppendElement( interp, *listObjPtrPtr, rowObj );
    }
    return TCL_OK;
}

int
DoesCodecSupport( CodecType cType, long what, int value )
{
    int         answer = 0;
    CodecInfo   cInfo;
    OSErr       err = noErr;
    
    err = GetCodecInfo( &cInfo, cType, 0 );
    
    switch (what) {

        case kDoesCodecPixmapDepth: {
            long    compressFlags = cInfo.compressFlags;

            switch (value) {
                case 1: {
                    answer = compressFlags | codecInfoDoes1;
                    break;
                }
                case 2: {
                    answer = compressFlags | codecInfoDoes2;
                    break;
                }
                case 4: {
                    answer = compressFlags | codecInfoDoes4;
                    break;
                }
                case 8: {
                    answer = compressFlags | codecInfoDoes8;
                    break;
                }
                case 16: {
                    answer = compressFlags | codecInfoDoes16;
                    break;
                }
                case 32: {
                    answer = compressFlags | codecInfoDoes32;
                    break;
                }
                default: {

                }
            }
            break;
        }
        case kDoesCodecCompressDepth: {
            long    formatFlags = cInfo.formatFlags;

            switch (value) {
                case 1: {
                    answer = formatFlags | codecInfoDepth1;
                    break;
                }
                case 2: {
                    answer = formatFlags | codecInfoDepth2;
                    break;
                }
                case 4: {
                    answer = formatFlags | codecInfoDepth4;
                    break;
                }
                case 8: {
                    answer = formatFlags | codecInfoDepth8;
                    break;
                }
                case 16: {
                    answer = formatFlags | codecInfoDepth16;
                    break;
                }
                case 32: {
                    answer = formatFlags | codecInfoDepth32;
                    break;
                }
                case 24: {
                    answer = formatFlags | codecInfoDepth24;
                    break;
                }
                case 33: {
                    answer = formatFlags | codecInfoDepth33;
                    break;
                }
                case 34: {
                    answer = formatFlags | codecInfoDepth34;
                    break;
                }
                case 36: {
                    answer = formatFlags | codecInfoDepth36;
                    break;
                }
                case 40: {
                    answer = formatFlags | codecInfoDepth40;
                    break;
                }
                default: {

                }
            }
        }
    }
    return answer;
}

/*
 *----------------------------------------------------------------------
 *
 * IsTrackForEyes --
 *
 *		Returns false if sound track, and true if text or video track.
 *
 * Results:
 *		Boolean.
 *
 * Side effects:
 *		None.
 *
 *----------------------------------------------------------------------
 */
 
int
IsTrackForEyes( Track myTrack ) 
{
    int     isForEyes = true;
    Media   myMedia = NULL;
    OSType  mediaType;
	
	myMedia = GetTrackMedia( myTrack );
	if (myMedia != NULL) {
		GetMediaHandlerDescription( myMedia, &mediaType, NULL, NULL );
		if (mediaType == SoundMediaType) {
		    isForEyes = false;
		}    	    
	}	
	return isForEyes;
}

/*----------------------------------------------------------------------*/
