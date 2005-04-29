/*
 * MovieQTVRUtils.c --
 *
 *		Utilities for QTVR movies, part of QuickTimeTcl.
 *      Several code snippets from Apples AddVRActions example.
 *
 * $Id: MovieQTVRUtils.c,v 1.1.1.1 2003/04/04 16:24:24 matben Exp $
 */

#ifdef _WIN32
#   include "QuickTimeTclWin.h"
#endif  

#include "QuickTimeTcl.h"

#define kFloat 1
#define kBoolean 2

/*
 * For dispatching timecode commands.
 */

static char *allHotSpotCmds[] = {
	"configure", "setid", "setwiredactions",
    (char *) NULL
};

enum {
	kHotSpotCmdConfigure					= 0L,
    kHotSpotCmdSetID,
    kHotSpotCmdSetWiredActions
};    

/* -enabled treated separately. */
static char *allHotSpotSetWiredOptions[] = {
	"-fov", "-pan", "-tilt",
    (char *) NULL
};

enum {
    kHotSpotSetWiredFov						= 0L,
    kHotSpotSetWiredPan,
    kHotSpotSetWiredTilt
};    

static OSErr        ReplaceQTVRMedia( Movie movie, QTVRInstance qtvrInst,
                            QTAtomContainer nodeInfoContainer );
static int          AddWiredActionsToHotSpot( Tcl_Interp *interp, Movie movie,
	                        QTVRInstance qtvrInst, int hotspotID, int objc, Tcl_Obj *CONST objv[] );
static int          CreateHotSpotActionContainer( Tcl_Interp *interp, 
                            QTAtomContainer *actionContainer, int objc, Tcl_Obj *CONST objv[] );
static OSErr        SetWiredActionsToHotSpot ( Handle theSample, long theHotSpotID, 
                            QTAtomContainer actionContainer );
static OSErr        WriteTheMediaPropertyAtom ( Media theMedia, long propertyID, 
                            long thePropertySize, void *theProperty );
static OSErr        CreateFrameLoadedHotSpotEnabledActionContainer( 
                            QTAtomContainer *actionContainer, 
                            long hotSpotID, Boolean enabled );
static OSErr        SetWiredActionsToNode ( Handle theSample, 
                            QTAtomContainer actionContainer, UInt32 theActionType );

                

/*
 *----------------------------------------------------------------------
 *
 * ProcessHotspotSubCmd --
 *
 *		Process the "hotspot" subcommand.
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
ProcessHotspotSubCmd( Tcl_Interp *interp, 
		Movie movie,
	    QTVRInstance qtvrInst, 
	    int objc, 
	    Tcl_Obj *CONST objv[] ) 
{
	int 			    result = TCL_OK;
	int                 nodeID;
	int                 hotspotID;
	int                 newHotspotID;
	int                 iarg;
	int                 len;
	int                 rebuild = false;
	int					cmdIndex;
	int					intValue;
	short               index;
    OSErr               err = noErr;
    QTAtomContainer     nodeInfoContainer = NULL;
    QTAtomContainer     vrWorld = NULL;
    QTVRHotSpotInfoAtomPtr  hotspotInfoAtomPtr;
    QTAtom              hotspotParentAtom = 0;
    QTAtom              hotspotAtom = 0;
    QTAtom              hotspotInfoAtom = 0;
    QTAtom              nameAtom = 0;
    QTAtomID            atomID;
    Boolean             enabled;
	Tcl_DString         ds;

    if (objc < 3) {
		Tcl_WrongNumArgs( interp, 0, objv, 
				"pathName hotspot command nodeId hotspotId ?args?" );
        return TCL_ERROR;
    }
    if (GetNodeCount( qtvrInst ) != 1) {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( 
				"Didn't recognize this as a single node QTVR movie", -1 ) );
        return TCL_ERROR;
    }
	if (Tcl_GetIntFromObj( interp, objv[1], &nodeID ) != TCL_OK) {
		Tcl_AddErrorInfo( interp, "\n	(processing nodeID value)" );
        return TCL_ERROR;
	}
    nodeID = kQTVRCurrentNode;
    nodeID = QTVRGetCurrentNodeID( qtvrInst );
	if (Tcl_GetIntFromObj( interp, objv[2], &hotspotID ) != TCL_OK) {
		Tcl_AddErrorInfo( interp, "\n	(processing hotspotID value)" );
        return TCL_ERROR;
	}
    
    /*
     * This gets only a *copy* of the atom container maintained internally.
     * If we make any changes, we need to add it back, see below.
     */
     
    err = QTVRGetNodeInfo( qtvrInst, nodeID, &nodeInfoContainer );
    if (err != noErr) {
        CheckAndSetErrorResult( interp, err );
        return TCL_ERROR;
    }
    QTLockContainer( nodeInfoContainer );

    /* 
     * Get hotspot parent atom which is the parent of all hot spot atoms of the node. 
     */

    hotspotParentAtom = QTFindChildByID( nodeInfoContainer, kParentAtomIsContainer,
            kQTVRHotSpotParentAtomType, 1, NULL );
    if (hotspotParentAtom != 0) {
        hotspotAtom = QTFindChildByID( nodeInfoContainer, hotspotParentAtom, 
               kQTVRHotSpotAtomType, hotspotID, &index );
        if (hotspotAtom != 0) {

            /*
             * Get the hotspot info atom here.
             */

            hotspotInfoAtom = QTFindChildByIndex( nodeInfoContainer, hotspotAtom, 
                    kQTVRHotSpotInfoAtomType, 1, &atomID );
            if (hotspotInfoAtom != 0) {
                err = QTGetAtomDataPtr( nodeInfoContainer, hotspotInfoAtom, NULL,
                        (Ptr *) &hotspotInfoAtomPtr );
                if (err == noErr) {
                    if (EndianS32_BtoN(hotspotInfoAtomPtr->nameAtomID) != 0) {
                        nameAtom = QTFindChildByID( nodeInfoContainer, hotspotAtom,
                                kQTVRStringAtomType, 
                			    EndianS32_BtoN(hotspotInfoAtomPtr->nameAtomID), NULL );
                    }
                }
            }
        }
    }
    
    /*
     * Must be unlocked if we want to change something.
     */
    
    QTUnlockContainer( nodeInfoContainer );

	if (Tcl_GetIndexFromObj( interp, objv[0], allHotSpotCmds, "hotspot command", 
			TCL_EXACT, &cmdIndex ) != TCL_OK ) {
	    return TCL_ERROR;
	}
    
    switch (cmdIndex) {

        case kHotSpotCmdConfigure: {

	        /*
	         * Parse configure command options. Starting with iarg 3.
	         */

	        if (objc % 2 == 0) {
				Tcl_WrongNumArgs( interp, 0, objv, 
						"pathName hotspot configure nodeId hotspotId -key value ?-key value?" );
	      	    result = TCL_ERROR;
	      	    goto error;
	        }
	        for (iarg = 3; iarg < objc; iarg = iarg + 2) {
	        
	            if (strcmp(Tcl_GetString( objv[iarg] ), "-enabled") == 0) {
					if (Tcl_GetBooleanFromObj( interp, objv[iarg+1], &intValue ) 
							!= TCL_OK) {
						Tcl_AddErrorInfo( interp, 
								"\n	(processing -enabled option)" );
						return TCL_ERROR;
					}
	                enabled = (Boolean) intValue; 
	                err = QTVREnableHotSpot( qtvrInst, kQTVRHotSpotID, hotspotID, enabled );
	                if (err != noErr) {
	                    CheckAndSetErrorResult( interp, err );
	                    result = TCL_ERROR;
	               	    goto error;
	                }
	            } else if (strcmp(Tcl_GetString( objv[iarg] ), "-name") == 0) {
	                if (nameAtom != 0) {
	                    QTVRStringAtomPtr   strAtomPtr = NULL;
	                    UInt16              size;
	               
	                    Tcl_UtfToExternalDString( gQTTclTranslationEncoding, 
	                    		Tcl_GetString( objv[iarg+1] ), -1, &ds );
	                    len = Tcl_DStringLength(&ds);    
					    len = (len > 250) ? 250 : len;

	                    size = sizeof(QTVRStringAtom) - 4 + len;
	                    strAtomPtr = (QTVRStringAtomPtr) NewPtrClear(size);
	                    
	                    if (strAtomPtr != NULL) {
	                        strAtomPtr->stringUsage = EndianU16_NtoB(1);
	                        strAtomPtr->stringLength = EndianU16_NtoB(len);
	                        BlockMove( Tcl_DStringValue(&ds), strAtomPtr->theString, len );
	                        err = QTSetAtomData( nodeInfoContainer, nameAtom, size, 
	                                (Ptr) strAtomPtr );
	                        DisposePtr( (Ptr) strAtomPtr );
	                        Tcl_DStringFree( &ds );
	                        if (err != noErr) {
	                            CheckAndSetErrorResult( interp, err );
	                            result = TCL_ERROR;
	                       	    goto error;
	                        }
	                        rebuild = true;
	                    }
	                }
	        	} else {
					Tcl_SetObjResult( interp, Tcl_NewStringObj( 
							"Unrecognized option for hotspot configure", -1 ) );
	           	    result = TCL_ERROR;
	           	    goto error;
	            }
	        }        
        	break;
        }

        case kHotSpotCmdSetID: {
	    
	        /*
	         * The hotspot id should be changed to a new id.
	         */
	         
			if (Tcl_GetIntFromObj( interp, objv[3], &newHotspotID ) 
					!= TCL_OK) {
				Tcl_AddErrorInfo( interp, 
						"\n		(processing newHotspotID option)" );
				return TCL_ERROR;
			}
	        err = QTSetAtomID( nodeInfoContainer, hotspotAtom, newHotspotID );
	        if (err != noErr) {
	            CheckAndSetErrorResult( interp, err );
	       	    result = TCL_ERROR;
	       	    goto error;
	        }
	        rebuild = true;        
        	break;
        }

        case kHotSpotCmdSetWiredActions: {
	    
	        /*
	         * Set wired actions for the specified hotspot. We rebuild inside.
	         */
	         
	        result = AddWiredActionsToHotSpot( interp, movie, qtvrInst, hotspotID, 
	                objc - 3, objv + 3 );
	        if (result != TCL_OK) {
	            goto error;
	        }        
        	break;
        }
	}
	 
	if (rebuild) {
        err = ReplaceQTVRMedia( movie, qtvrInst, nodeInfoContainer );
        if (err != noErr) {
            CheckAndSetErrorResult( interp, err );
       	    result = TCL_ERROR;
            goto error;
        }
	}
    QTVRUpdate( qtvrInst, kQTVRStatic );
	
error:	
    if (nodeInfoContainer != NULL) {
        QTDisposeAtomContainer( nodeInfoContainer );
    }
    if (vrWorld != NULL) {
        QTDisposeAtomContainer( vrWorld );
    }
    return result;	
}   

/*
 *----------------------------------------------------------------------
 *
 * AddWiredActionsToHotSpot --
 *
 *	
 *
 * Results:
 *  	Tcl result
 *
 * Side effects:
 *		None.
 *
 *----------------------------------------------------------------------
 */

static int 
AddWiredActionsToHotSpot( Tcl_Interp *interp, 
		Movie movie,
	    QTVRInstance qtvrInst, 
	    int hotspotID, 
	    int objc, 
	    Tcl_Obj *CONST objv[] )
{	
	Track							track = NULL;
	Media							media = NULL;
	TimeValue						trackOffset;
	TimeValue						mediaTime;
	TimeValue						sampleDuration;
	QTVRSampleDescriptionHandle		theQTVRDesc = NULL;
	Handle							sample = NULL;
	short							sampleFlags;
	QTAtomContainer					actionContainer = NULL;	
	Boolean							hasActions;
	Boolean                         enabled;
	OSErr							err = noErr;
	int                             result = TCL_OK;
	int                             iarg;
	int                             hsObjc = 0;
	int                             frameObjc = 0;
	int								booleanInt;
	Tcl_Obj                         *hsObjv[10];
	Tcl_Obj                         *frameObjv[10];
	
    track = QTVRGetQTVRTrack( movie, 1 );
    if (track == NULL) {
        CheckAndSetErrorResult( interp, noErr );
        goto bail;
    }
	
	/* Get the first media sample in the QTVR track.
	 *
	 * The QTVR track contains one media sample for each node in the movie;
	 * that sample contains a node information atom container, which contains 
	 * general information about the node (such as its type, its ID, its name, 
	 * and a list of its hot spots)
	 */
	
	media = GetTrackMedia(track);
	if (media == NULL) {
        CheckAndSetErrorResult( interp, noErr );
		goto bail;
	}
	trackOffset = GetTrackOffset( track );
	mediaTime = TrackTimeToMediaTime( trackOffset, track );

	theQTVRDesc = (QTVRSampleDescriptionHandle) NewHandle(4);
	if (theQTVRDesc == NULL) {
        CheckAndSetErrorResult( interp, noErr );
		goto bail;
    }
	sample = NewHandle(0);
	if (sample == NULL) {
        CheckAndSetErrorResult( interp, noErr );
		goto bail;
    }
	err = GetMediaSample( media, sample, 0, NULL, mediaTime, NULL, 
	        &sampleDuration, (SampleDescriptionHandle) theQTVRDesc, NULL, 1, 
	        NULL, &sampleFlags );
	if (err != noErr) {
        CheckAndSetErrorResult( interp, noErr );
		goto bail;
	}
	
	/*
	 * Some of the arguments must be stored in a 'frame loaded container',
	 * while the other shall be put in the 'hotspot action container'.
	 */

    for (iarg = 0; iarg < objc; iarg = iarg + 2) {        		
        if ((strcmp(Tcl_GetString( objv[iarg] ), "-enabled") == 0)) {
            frameObjv[frameObjc] = objv[iarg];
            frameObjv[frameObjc+1] = objv[iarg+1];
            frameObjc += 2;
			if (Tcl_GetBooleanFromObj( interp, frameObjv[iarg+1], 
					&booleanInt ) != TCL_OK) {
				Tcl_AddErrorInfo( interp, 
						"\n	(processing -enabled option)" );
				result = TCL_ERROR;
				goto bail;
			}
    	    enabled = (Boolean) booleanInt;
        } else {
            hsObjv[hsObjc] = objv[iarg];
            hsObjv[hsObjc+1] = objv[iarg+1];
            hsObjc += 2;
		}
	}
	
	/*
	 * The enabled action is put in a frame loaded container.
	 */
	 
	if (frameObjc > 0) {	    
        err = CreateFrameLoadedHotSpotEnabledActionContainer( &actionContainer,
                hotspotID, enabled );
    	if (err != noErr) {
	        CheckAndSetErrorResult( interp, err );
            goto bail;
    	}
        err = SetWiredActionsToNode ( sample, actionContainer, kQTEventFrameLoaded );
    	if (err != noErr) {
	        CheckAndSetErrorResult( interp, err );
    		goto bail;
    	}	
    	if (actionContainer != NULL) {
    		QTDisposeAtomContainer(actionContainer);
    		actionContainer = NULL;
    	}	
	}
	
	/*
	 * Add hot-spot actions.
	 */
	
	if (hsObjc > 0) {
        err = CreateHotSpotActionContainer( interp, &actionContainer, hsObjc, hsObjv );
    	if (err != noErr) {
	        CheckAndSetErrorResult( interp, err );
            goto bail;
    	}
    	err = SetWiredActionsToHotSpot( sample, hotspotID, actionContainer );
    	if (err != noErr) {
	        CheckAndSetErrorResult( interp, err );
    		goto bail;
    	}
    	if (actionContainer != NULL) {
    		QTDisposeAtomContainer(actionContainer);
    		actionContainer = NULL;
    	}
    }
	
	/*
	 * Replace sample in media.
	 */
	 
    err = ReplaceQTVRMedia( movie, qtvrInst, (QTAtomContainer) sample );
	if (err != noErr) {
	    CheckAndSetErrorResult( interp, err );
		goto bail;
	}
        
	/*
	 * Set the actions property atom, to enable wired action processing.
	 */
			
	hasActions = true;	
	// since sizeof(Boolean) == 1, there is no need to swap bytes here
	err = WriteTheMediaPropertyAtom( media, kSpriteTrackPropertyHasActions, 
	        sizeof(Boolean), &hasActions );
	if (err != noErr) {
	    CheckAndSetErrorResult( interp, err );
		goto bail;
	}
	
bail:
	if (actionContainer != NULL) {
		QTDisposeAtomContainer(actionContainer);
	}
	if (sample != NULL) {
		DisposeHandle(sample);		
	}
	if (theQTVRDesc != NULL) {
		DisposeHandle((Handle)theQTVRDesc);		
	}
	if (result != TCL_OK) {
        CheckAndSetErrorResult( interp, err );
	}
	return result;
}

/*
 *----------------------------------------------------------------------
 *
 * ReplaceQTVRMedia --
 *
 *	
 *
 * Results:
 *  	OSErr result
 *
 * Side effects:
 *		None.
 *
 *----------------------------------------------------------------------
 */

static OSErr
ReplaceQTVRMedia( Movie movie, 
		QTVRInstance qtvrInst,
        QTAtomContainer nodeInfoContainer )     // the actual sample (Handle)
{
    OSErr               err = noErr;
    Track               track = NULL;
    Media               media = NULL;
	Handle				sample = NULL;
	short				sampleFlags;
	TimeValue			sampleDuration;
    TimeValue           trackOffset;
    TimeValue           mediaTime;
    TimeValue           newMediaTime;
	TimeValue			selectionDuration;
	Fixed 				trackEditRate;
    QTVRSampleDescriptionHandle theQTVRDesc = NULL;        

    /*
     * We need to replace the QTVR track with the newly edited information.
     * A single node QTVR movie contains only a single media sample, with:
     *  1) The VR World Atom Container as sample description, and
     *  2) The Node Info Atom Container as the media sample.
     * So replace the old with the new. Fails for multi node movies!
     */

    track = QTVRGetQTVRTrack( movie, 1 );
    if (track == NULL) {
        goto error;
    }
    media = GetTrackMedia( track );
    if (media == NULL) {
        goto error;
    }
	trackOffset = GetTrackOffset( track );
	mediaTime = TrackTimeToMediaTime( trackOffset, track );
	theQTVRDesc = (QTVRSampleDescriptionHandle) NewHandle(4);
	if (theQTVRDesc == NULL) {
		goto error;
    }
	err = GetMediaSample( media, sample, 0, NULL, mediaTime, NULL, 
	        &sampleDuration, (SampleDescriptionHandle) theQTVRDesc, NULL, 1, 
	        NULL, &sampleFlags );
	if (err != noErr) {
		goto error;
	}
	trackEditRate = GetTrackEditRate( track, trackOffset );
	if (GetMoviesError() != noErr)
		goto error;

	GetTrackNextInterestingTime( track, nextTimeMediaSample | nextTimeEdgeOK, 
	        trackOffset, fixed1, NULL, &selectionDuration );
	if (GetMoviesError() != noErr)
		goto error;
              
    /* Get rid of the old QTVR sample first. */

	err = DeleteTrackSegment( track, trackOffset, selectionDuration );
	if (err != noErr) {
		goto error;
    }    
    err = BeginMediaEdits( media );
    if (err != noErr) {
        goto error;
    }
    err = AddMediaSample( media, 
          (Handle) nodeInfoContainer,             // the actual data
          0,                                      // no offset in data
          GetHandleSize((Handle) nodeInfoContainer),  // the data size
    	  sampleDuration,                          // duration
          (SampleDescriptionHandle) theQTVRDesc, 
          1,                                      // one sample
          sampleFlags,                            // key frame or not
          &newMediaTime );
    if (err != noErr) {
        goto error;
    }   
    err = EndMediaEdits( media );
    if (err != noErr) {
        goto error;
    }
    err = InsertMediaIntoTrack( track,          // the track
         trackOffset,                           // track time, 0 for single node
         newMediaTime,                          // media start time in media's time
         selectionDuration,                     // media's duration in media's time
         trackEditRate );                       // the media's rate
    if (err != noErr) {
        goto error;
    }
    
error:
    if (sample != NULL) {
        DisposeHandle( sample );
    }
    return(err);
}

/*
 *----------------------------------------------------------------------
 *
 * CreateFrameLoadedHotSpotEnabledActionContainer --
 *
 *	
 *
 * Results:
 *		Standard OSErr result
 *
 * Side effects:
 *
 *----------------------------------------------------------------------
 */

static OSErr 
CreateFrameLoadedHotSpotEnabledActionContainer( QTAtomContainer *actionContainer, 
        long hotSpotID, Boolean enabled )
{
	QTAtom			actionAtom = 0;
	QTAtom          eventAtom = 0;
	long			action;
	OSErr			err = noErr;

	err = QTNewAtomContainer( actionContainer );
	if (err != noErr) {
		goto bail;
	}
	err = QTInsertChild( *actionContainer, kParentAtomIsContainer, kQTEventFrameLoaded, 
	        1, 1, 0, NULL, &eventAtom );
	if (err != noErr) {
		goto bail;
    }
	err = QTInsertChild( *actionContainer, eventAtom, kAction, 1, 1, 0, NULL, 
	        &actionAtom );
	if (err != noErr) {
		goto bail;
    }
	action = EndianS32_NtoB( kActionQTVREnableHotSpot );
	hotSpotID = EndianS32_NtoB( hotSpotID );
	err = QTInsertChild( *actionContainer, actionAtom, kWhichAction, 1, 1, 
	        sizeof(long), &action, NULL);
	if (err != noErr) {
		goto bail;
    }
	err = QTInsertChild( *actionContainer, actionAtom, kActionParameter, 0, 0, 
	        sizeof(long), &hotSpotID, NULL );
	if (err != noErr) {
		goto bail;
	}
	err = QTInsertChild( *actionContainer, actionAtom, kActionParameter, 0, 0, 
	        sizeof(Boolean), &enabled, NULL );
	if (err != noErr) {
		goto bail;
    }
    
bail:
	return(err);
}	

/*
 *----------------------------------------------------------------------
 *
 * CreateHotSpotActionContainer --
 *
 *		Return, through the actionContainer parameter, an atom container 
 *		that contains one or many hot spot actions.
 *
 * Results:
 *		Standard TCL result
 *
 * Side effects:
 *
 *----------------------------------------------------------------------
 */

static int 
CreateHotSpotActionContainer( Tcl_Interp *interp, 
		QTAtomContainer *actionContainer,
        int objc, 
        Tcl_Obj *CONST objv[] )
{
	QTAtom			eventAtom = 0;
	QTAtom			actionAtom = 0;
	long			action;
	float			aFloat;
	double			aDouble;
	OSErr			err = noErr;
	Tcl_Obj			*resultObjPtr;
	int             iarg;
	int				optIndex;
	
	err = QTNewAtomContainer( actionContainer );
	if (err != noErr) {
		goto error;
	}
	err = QTInsertChild( *actionContainer, kParentAtomIsContainer, kQTEventType, 
	        kQTEventMouseClick, 1, 0, NULL, &eventAtom );
	if (err != noErr) {
		goto error;
	}

    /*
     * Parse configure command options. Starting with iarg 0.
     */

    for (iarg = 0; iarg < objc; iarg = iarg + 2) {
    
    	if (Tcl_GetIndexFromObj( interp, objv[iarg], allHotSpotSetWiredOptions, 
    	        "hotspot setwiredactions option", TCL_EXACT, &optIndex ) != TCL_OK ) {
    	    goto error;
    	}
    	if (iarg + 1 == objc) {
    		resultObjPtr = Tcl_GetObjResult( interp );
    		Tcl_AppendStringsToObj( resultObjPtr, "value for \"",
    				Tcl_GetString(objv[iarg]), "\"missing", (char *) NULL );
    	    goto error;
    	}

        switch (optIndex) {

            case kHotSpotSetWiredFov: {
	    	    action = EndianS32_NtoB( kActionQTVRSetFieldOfView );
		    	if (Tcl_GetDoubleFromObj( interp, objv[iarg+1], &aDouble ) 
		    				!= TCL_OK) {
					Tcl_AddErrorInfo( interp, 
							"\n		(processing -fov option)" );
		    	    goto error;
				}
	    	    aFloat = (float) aDouble;
		        ConvertFloatToBigEndian( &aFloat );
				break;
			}    

            case kHotSpotSetWiredPan: {
	    	    action = EndianS32_NtoB( kActionQTVRSetPanAngle );
		    	if (Tcl_GetDoubleFromObj( interp, objv[iarg+1], &aDouble ) 
		    				!= TCL_OK) {
					Tcl_AddErrorInfo( interp, 
							"\n		(processing -pan option)" );
		    	    goto error;
				}
	    	    aFloat = (float) aDouble;
		        ConvertFloatToBigEndian( &aFloat );
				break;
			}    

            case kHotSpotSetWiredTilt: {
	    	    action = EndianS32_NtoB( kActionQTVRSetTiltAngle );
		    	if (Tcl_GetDoubleFromObj( interp, objv[iarg+1], &aDouble ) 
		    				!= TCL_OK) {
					Tcl_AddErrorInfo( interp, 
							"\n		(processing -tilt option)" );
		    	    goto error;
				}
	    	    aFloat = (float) aDouble;
		        ConvertFloatToBigEndian( &aFloat );
				break;
			}    
    	}
    	
    	/*
    	 * For each action we need an Action atom to hold the WhichAction,
    	 * and ActionParameter atoms.
    	 */
    	 
    	err = QTInsertChild( *actionContainer, eventAtom, kAction, 0, 
    	        0, 0, NULL, &actionAtom );
    	if (err != noErr) {
    		goto error;
        }
        err = QTInsertChild( *actionContainer, actionAtom, kWhichAction, 1, 
                1, sizeof(long), &action, NULL );
        if (err != noErr) {
            goto error;    	
        }
        err = QTInsertChild( *actionContainer, actionAtom, kActionParameter, 1, 
                1, sizeof(float), &aFloat, NULL );
        if (err != noErr) {
            goto error;
        }
    }
    return TCL_OK;
		
error:
    CheckAndSetErrorResult( interp, err );
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * CreateFrameLoadedActionContainer --
 *
 *		Return, through the actionContainer parameter, an atom container 
 *		that contains a frame-loaded event action.
 *
 * Results:
 *		Standard OSErr result
 *
 * Side effects:
 *
 *----------------------------------------------------------------------
 */

static OSErr 
CreateFrameLoadedActionContainer( QTAtomContainer *actionContainer )
{
	QTAtom			eventAtom = 0;
	QTAtom			actionAtom = 0;
	long			action;
	float			panAngle;
	OSErr			err = noErr;
	
	err = QTNewAtomContainer(actionContainer);
	if (err != noErr) {
		goto bail;
	}
	err = QTInsertChild(*actionContainer, kParentAtomIsContainer, kQTEventFrameLoaded, 
	        1, 1, 0, NULL, &eventAtom);
	if (err != noErr) {
		goto bail;
	}
	err = QTInsertChild(*actionContainer, eventAtom, kAction, 1, 1, 
	        0, NULL, &actionAtom);
	if (err != noErr) {
		goto bail;
	}
	action = EndianS32_NtoB(kActionQTVRSetPanAngle);
	err = QTInsertChild(*actionContainer, actionAtom, kWhichAction, 1, 
	        1, sizeof(long), &action, NULL);
	if (err != noErr) {
		goto bail;
	}
	panAngle = 180.0;
	ConvertFloatToBigEndian(&panAngle);
	err = QTInsertChild(*actionContainer, actionAtom, kActionParameter, 1, 
	        1, sizeof(float), &panAngle, NULL);
	if (err != noErr) {
		goto bail;	
    }
    
bail:
	return(err);
}	

/*
 *----------------------------------------------------------------------
 *
 * CreateIdleActionContainer --
 *
 *		Return, through the actionContainer parameter, an atom container 
 *		that contains an idle event action.
 *
 * Results:
 *		Standard OSErr result
 *
 * Side effects:
 *
 *----------------------------------------------------------------------
 */

static OSErr 
CreateIdleActionContainer (QTAtomContainer *actionContainer)
{
	QTAtom			eventAtom = 0;
	QTAtom			actionAtom = 0;
	long			action;
	float			panAngle;
	UInt32			flags;
	OSErr			err = noErr;
	
	err = QTNewAtomContainer(actionContainer);
	if (err != noErr)
		goto bail;
	
	err = QTInsertChild(*actionContainer, kParentAtomIsContainer, kQTEventIdle, 
	        1, 1, 0, NULL, &eventAtom);
	if (err != noErr)
		goto bail;
	
	err = QTInsertChild(*actionContainer, eventAtom, kAction, 1, 
	        1, 0, NULL, &actionAtom);
	if (err != noErr)
		goto bail;
	
	action = EndianS32_NtoB(kActionQTVRSetPanAngle);
	err = QTInsertChild(*actionContainer, actionAtom, kWhichAction, 1, 
	        1, sizeof(long), &action, NULL);
	if (err != noErr)
		goto bail;

	panAngle = 10.0;
	ConvertFloatToBigEndian(&panAngle);
	err = QTInsertChild(*actionContainer, actionAtom, kActionParameter, 
	        1, 1, sizeof(float), &panAngle, NULL);
	if (err != noErr)
		goto bail;	
		
	flags = EndianU32_NtoB(kActionFlagActionIsDelta | kActionFlagParameterWrapsAround);
	err = QTInsertChild(*actionContainer, actionAtom, kActionFlags, 1, 
	        1, sizeof(UInt32), &flags, NULL);
	if (err != noErr)
		goto bail;	
				
bail:
	return(err);
}	

/*
 *----------------------------------------------------------------------
 *
 * SetWiredActionsToNode --
 *
 *		Set the specified actions to be a node action of the specified 
 *		type. If actionContainer is NULL, remove any existing action of 
 *		that type from theSample. The theSample parameter is assumed to 
 *		be a node information atom container; any actions that are 
 *		global to the node should be inserted at the root level of this 
 *		atom container; in addition, the container type should be the 
 *		same as the event type and should have an atom ID of 1.
 *
 * Results:
 *		Standard OSErr result
 *
 * Side effects:
 *
 *----------------------------------------------------------------------
 */

static OSErr 
SetWiredActionsToNode ( Handle theSample, QTAtomContainer actionContainer, 
        UInt32 theActionType )
{
	QTAtom			eventAtom = 0;
	QTAtom			targetAtom = 0;
	OSErr			err = noErr;
	
	/* Look for a frame-loaded action atom in the specified actions atom container. */
	if (actionContainer != NULL) {
		eventAtom = QTFindChildByID(actionContainer, kParentAtomIsContainer, 
		        theActionType, 1, NULL);
    }
    			
	/* Look for a frame-loaded action atom in the node information atom container. */
	targetAtom = QTFindChildByID(theSample, kParentAtomIsContainer, 
	        theActionType, 1, NULL);
	if (targetAtom != 0) {
	
		/*
		 * If there is already a frame-loaded event atom in the node information 
		 * atom container, then either replace it with the one we were passed or 
		 * remove it.
		 */
		 
		if (actionContainer != NULL) {
			err = QTReplaceAtom(theSample, targetAtom, actionContainer, eventAtom);	
		} else {
			err = QTRemoveAtom(theSample, targetAtom);
		}	
	} else {
	
		/*
		 * There is no frame-loaded event atom in the node information atom container,
		 * so add in the one we were passed.
		 */
		 
		if (actionContainer != NULL) {
			err = QTInsertChildren(theSample, kParentAtomIsContainer, actionContainer);
		}
	}
		
	return(err);
}

/*
 *----------------------------------------------------------------------
 *
 * SetWiredActionsToHotSpot --
 *
 *		Set the specified actions to be a hot-spot action. If 
 *		actionContainer is NULL, remove any existing hot-spot actions 
 *		for the specified hot spot from theSample.
 *
 * Results:
 *		Standard OSErr result
 *
 * Side effects:
 *
 *----------------------------------------------------------------------
 */

static OSErr 
SetWiredActionsToHotSpot ( Handle theSample, long theHotSpotID, 
        QTAtomContainer actionContainer )
{
	QTAtom			hotSpotParentAtom = 0;
	QTAtom			hotSpotAtom = 0;
	short			count,
					index;
	OSErr			err = noErr;
	
	hotSpotParentAtom = QTFindChildByIndex(theSample, kParentAtomIsContainer, 
	        kQTVRHotSpotParentAtomType, 1, NULL);
	if (hotSpotParentAtom == 0) {
		goto bail;
	}
	hotSpotAtom = QTFindChildByID(theSample, hotSpotParentAtom, 
	        kQTVRHotSpotAtomType, theHotSpotID, NULL);
	if (hotSpotAtom == 0) {
		goto bail;
	}
	
	/* See how many events are already associated with the specified hot spot. */
	count = QTCountChildrenOfType(theSample, hotSpotAtom, kQTEventType);
	
	for (index = count; index > 0; index--) {
		QTAtom		targetAtom = 0;
		
		/* Remove all the existing events. */
		targetAtom = QTFindChildByIndex(theSample, hotSpotAtom, kQTEventType, 
		        index, NULL);
		if (targetAtom != 0) {
			err = QTRemoveAtom(theSample, targetAtom);
			if (err != noErr) {
				goto bail;
			}
		}
	}
	
	if (actionContainer) {
		err = QTInsertChildren(theSample, hotSpotAtom, actionContainer);
		if (err != noErr) {
			goto bail;
		}
	}
	
bail:
	return(err);
}

/*
 *----------------------------------------------------------------------
 *
 * WriteTheMediaPropertyAtom --
 *
 *		Add a media property action to the specified media. 
 *		We assume that the data passed to us through the theProperty 
 *		parameter is big-endian.
 *
 * Results:
 *		Standard OSErr result
 *
 * Side effects:
 *
 *----------------------------------------------------------------------
 */

static OSErr 
WriteTheMediaPropertyAtom ( Media theMedia, long propertyID, 
        long thePropertySize, void *theProperty )
{
	QTAtomContainer		propertyAtom = NULL;
	QTAtom				atom = 0;
	OSErr				err = noErr;

	/* Get the current media property atom. */
	err = GetMediaPropertyAtom(theMedia, &propertyAtom);
	if (err != noErr)
		goto bail;
	
	/* If there isn't one yet, then create one. */
	if (propertyAtom == NULL) {
		err = QTNewAtomContainer(&propertyAtom);
		if (err != noErr)
			goto bail;
	}

	/*
	 * See if there is an existing atom of the specified type; if not, 
	 * then create one.
	 */
	 
	atom = QTFindChildByID(propertyAtom, kParentAtomIsContainer, 
	        propertyID, 1, NULL);
	if (atom == 0) {
		err = QTInsertChild(propertyAtom, kParentAtomIsContainer, 
		        propertyID, 1, 0, 0, NULL, &atom);
		if ((err != noErr) || (atom == 0))
			goto bail;
	}
	
	/* Set the data of the specified atom to the data passed in. */
	err = QTSetAtomData(propertyAtom, atom, thePropertySize, (Ptr)theProperty);
	if (err != noErr)
		goto bail;

	/* Write the new atom data out to the media property atom. */
	err = SetMediaPropertyAtom(theMedia, propertyAtom);

bail:
	if (propertyAtom != NULL) {
		err = QTDisposeAtomContainer(propertyAtom);
	}
	return(err);
}

/*
 *----------------------------------------------------------------------
 *
 * PanoramaGetInfoNode --
 *
 *  	Gets all info from this panorama node, and fills the result 
 *		object.
 *
 * Results:
 *  	Tcl result.
 *
 * Side effects:
 *		Fills the result object with data.
 *
 *----------------------------------------------------------------------
 */

int
PanoramaGetInfoNode( Tcl_Interp *interp, 
		Movie movie, 
		QTVRInstance qtvrInst, 
        UInt32 nodeID, 
        Tcl_Obj **resObj )
{
    OSErr               err = noErr;
    QTAtomContainer     nodeInfoContainer = NULL;
    QTVRNodeHeaderAtomPtr nodeHeaderPtr;
    QTVRStringAtomPtr   stringAtomPtr;
    QTAtom              headerAtom = 0;
    QTAtom              hotspotParentAtom = 0;
    QTAtom              hotspotAtom = 0;
    QTAtom              hotspotInfoAtom = 0;
    QTAtom              nameAtom = 0;
    QTAtomID            atomHotSpotID;
    QTAtomID            atomID;
    OSType              nodeType;
    Track               panoTrack = NULL;
	Tcl_Obj             *listObjPtr;
    Tcl_Obj             *hotspotObjPtr;
    Tcl_Obj             *rectListObjPtr;
	Tcl_DString         ds;
    char                tmpstr[255];
    short               index;
    int                 len;
    UInt32              currentNodeID;

    *resObj = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );
    listObjPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );

    currentNodeID = QTVRGetCurrentNodeID( qtvrInst );
    Tcl_ListObjAppendElement( interp, *resObj, Tcl_NewStringObj("-nodeid", -1) );		    	
    Tcl_ListObjAppendElement( interp, *resObj, Tcl_NewIntObj(currentNodeID) );		    	
    nodeType = QTVRGetNodeType( qtvrInst, nodeID );
    Tcl_ListObjAppendElement( interp, *resObj, Tcl_NewStringObj("-nodetype", -1) );	
    if (nodeType == kQTVRPanoramaType) {	    	
        Tcl_ListObjAppendElement( interp, *resObj, Tcl_NewStringObj("panorama", -1) );		    	
    } else if (nodeType == kQTVRObjectType) {	    	
        Tcl_ListObjAppendElement( interp, *resObj, Tcl_NewStringObj("object", -1) );		    	
    } else {	    	
        Tcl_ListObjAppendElement( interp, *resObj, Tcl_NewStringObj("unknown", -1) );		    	
    }

    /*
     * Start by getting the panorama sample structure.
     */

    panoTrack = GetMovieIndTrackType( movie, 1, FOUR_CHAR_CODE('pano'), 
    	    movieTrackMediaType );
    if (panoTrack != NULL) {
        Media                   media = NULL;
        Handle                  dataOut = NULL;
        Ptr                     dataPtr = NULL;
        long                    size;
        TimeValue               sampleTime;
        TimeValue               durationPerSample;
        SampleDescriptionHandle descHandle = NULL;
        long                    descIndex;
        long                    numOfSamples;
        QTAtomContainer         containerHandle = NULL;
        QTAtom                  sampleAtom = 0;
        QTVRPanoSampleAtom      *samplePtr;

        media = GetTrackMedia( panoTrack );
        if (media != NULL) {
            dataOut = NewHandle(4);
            err = GetMediaSample( media, 
                    dataOut, 
                    0, 
                    &size, 
                    0,                  // media time
                    &sampleTime,
                    &durationPerSample,
                    descHandle,
                    &descIndex,
                    0,
                    &numOfSamples, 
                    NULL );
    		if (err != noErr) {
                CheckAndSetErrorResult( interp, err );
    			return TCL_ERROR;
    		}
            HLock( dataOut );
            containerHandle = (QTAtomContainer) dataOut;
            QTLockContainer( containerHandle );

            sampleAtom = QTFindChildByID( containerHandle, kParentAtomIsContainer,
                    kQTVRPanoSampleDataAtomType, 1, NULL );
            if (sampleAtom != 0) {
                err = QTGetAtomDataPtr( containerHandle, sampleAtom, NULL, (Ptr *) &samplePtr );
                if (err == noErr) {
					ConvertBigEndianFloatToNative( &(samplePtr->minPan) );
					ConvertBigEndianFloatToNative( &(samplePtr->maxPan) );
					ConvertBigEndianFloatToNative( &(samplePtr->minTilt) );
					ConvertBigEndianFloatToNative( &(samplePtr->maxTilt) );
					ConvertBigEndianFloatToNative( &(samplePtr->minFieldOfView) );
					ConvertBigEndianFloatToNative( &(samplePtr->maxFieldOfView) );
					ConvertBigEndianFloatToNative( &(samplePtr->defaultPan) );
					ConvertBigEndianFloatToNative( &(samplePtr->defaultTilt) );
					ConvertBigEndianFloatToNative( &(samplePtr->defaultFieldOfView) );

       		    	Tcl_ListObjAppendElement( interp, *resObj, 
                               Tcl_NewStringObj("-majorversion", -1) );		    	
       		    	Tcl_ListObjAppendElement( interp, *resObj, 
                               Tcl_NewIntObj(EndianU16_BtoN(samplePtr->majorVersion)) );		    	
       		    	Tcl_ListObjAppendElement( interp, *resObj, 
                               Tcl_NewStringObj("-minorversion", -1) );		    	
       		    	Tcl_ListObjAppendElement( interp, *resObj, 
                               Tcl_NewIntObj(EndianU16_BtoN(samplePtr->minorVersion)) );		    	
       		    	Tcl_ListObjAppendElement( interp, *resObj, 
                               Tcl_NewStringObj("-imagereftrackindex", -1) );		    	
       		    	Tcl_ListObjAppendElement( interp, *resObj, 
                               Tcl_NewIntObj(EndianU32_BtoN(samplePtr->imageRefTrackIndex)) );		    	
       		    	Tcl_ListObjAppendElement( interp, *resObj, 
                               Tcl_NewStringObj("-hotspotreftrackindex", -1) );		    	
       		    	Tcl_ListObjAppendElement( interp, *resObj, 
                               Tcl_NewIntObj(EndianU32_BtoN(samplePtr->hotSpotRefTrackIndex)) );		    	
       		    	Tcl_ListObjAppendElement( interp, *resObj, 
                               Tcl_NewStringObj("-minpan", -1) );		    	
       		    	Tcl_ListObjAppendElement( interp, *resObj, 
                               Tcl_NewDoubleObj(samplePtr->minPan) );		    	
       		    	Tcl_ListObjAppendElement( interp, *resObj, 
                               Tcl_NewStringObj("-maxpan", -1) );		    	
       		    	Tcl_ListObjAppendElement( interp, *resObj, 
                               Tcl_NewDoubleObj(samplePtr->maxPan) );		    	
       		    	Tcl_ListObjAppendElement( interp, *resObj, 
                               Tcl_NewStringObj("-mintilt", -1) );		    	
       		    	Tcl_ListObjAppendElement( interp, *resObj, 
                               Tcl_NewDoubleObj(samplePtr->minTilt) );		    	
       		    	Tcl_ListObjAppendElement( interp, *resObj, 
                               Tcl_NewStringObj("-maxtilt", -1) );		    	
       		    	Tcl_ListObjAppendElement( interp, *resObj, 
                               Tcl_NewDoubleObj(samplePtr->maxTilt) );		    	
       		    	Tcl_ListObjAppendElement( interp, *resObj, 
                               Tcl_NewStringObj("-minfov", -1) );		    	
       		    	Tcl_ListObjAppendElement( interp, *resObj, 
                               Tcl_NewDoubleObj(samplePtr->minFieldOfView) );		    	
       		    	Tcl_ListObjAppendElement( interp, *resObj, 
                               Tcl_NewStringObj("-maxfov", -1) );		    	
       		    	Tcl_ListObjAppendElement( interp, *resObj, 
                               Tcl_NewDoubleObj(samplePtr->maxFieldOfView) );		    	
       		    	Tcl_ListObjAppendElement( interp, *resObj, 
                               Tcl_NewStringObj("-defaultpan", -1) );		    	
       		    	Tcl_ListObjAppendElement( interp, *resObj, 
                               Tcl_NewDoubleObj(samplePtr->defaultPan) );		    	
       		    	Tcl_ListObjAppendElement( interp, *resObj, 
                               Tcl_NewStringObj("-defaulttilt", -1) );		    	
       		    	Tcl_ListObjAppendElement( interp, *resObj, 
                               Tcl_NewDoubleObj(samplePtr->defaultTilt) );		    	
       		    	Tcl_ListObjAppendElement( interp, *resObj, 
                               Tcl_NewStringObj("-defaultfov", -1) );		    	
       		    	Tcl_ListObjAppendElement( interp, *resObj, 
                               Tcl_NewDoubleObj(samplePtr->defaultFieldOfView) );		    	
       		    	Tcl_ListObjAppendElement( interp, *resObj, 
                               Tcl_NewStringObj("-imagesizex", -1) );		    	
       		    	Tcl_ListObjAppendElement( interp, *resObj, 
                               Tcl_NewIntObj(EndianU32_BtoN(samplePtr->imageSizeX)) );		    	
       		    	Tcl_ListObjAppendElement( interp, *resObj, 
                               Tcl_NewStringObj("-imagesizey", -1) );		    	
       		    	Tcl_ListObjAppendElement( interp, *resObj, 
                               Tcl_NewIntObj(EndianU32_BtoN(samplePtr->imageSizeY)) );		    	
       		    	Tcl_ListObjAppendElement( interp, *resObj, 
                               Tcl_NewStringObj("-imagenumframesx", -1) );		    	
       		    	Tcl_ListObjAppendElement( interp, *resObj, 
                               Tcl_NewIntObj(EndianU16_BtoN(samplePtr->imageNumFramesX)) );		    	
       		    	Tcl_ListObjAppendElement( interp, *resObj, 
                               Tcl_NewStringObj("-imagenumframesy", -1) );		    	
       		    	Tcl_ListObjAppendElement( interp, *resObj, 
                               Tcl_NewIntObj(EndianU16_BtoN(samplePtr->imageNumFramesY)) );		    	
       		    	Tcl_ListObjAppendElement( interp, *resObj, 
                               Tcl_NewStringObj("-hotspotsizex", -1) );		    	
       		    	Tcl_ListObjAppendElement( interp, *resObj, 
                               Tcl_NewIntObj(EndianU32_BtoN(samplePtr->hotSpotSizeX)) );		    	
       		    	Tcl_ListObjAppendElement( interp, *resObj, 
                               Tcl_NewStringObj("-hotspotsizey", -1) );		    	
       		    	Tcl_ListObjAppendElement( interp, *resObj, 
                               Tcl_NewIntObj(EndianU32_BtoN(samplePtr->hotSpotSizeY)) );		    	
       		    	Tcl_ListObjAppendElement( interp, *resObj, 
                               Tcl_NewStringObj("-hotspotnumframesx", -1) );		    	
       		    	Tcl_ListObjAppendElement( interp, *resObj, 
                               Tcl_NewIntObj(EndianU16_BtoN(samplePtr->hotSpotNumFramesX)) );		    	
       		    	Tcl_ListObjAppendElement( interp, *resObj, 
                               Tcl_NewStringObj("-hotspotnumframesy", -1) );		    	
       		    	Tcl_ListObjAppendElement( interp, *resObj, 
                               Tcl_NewIntObj(EndianU16_BtoN(samplePtr->hotSpotNumFramesY)) );		    	
                }
            }
            DisposeHandle( dataOut );
            DisposeHandle( (Handle) descHandle );
        }
    }

    /*
     * Investigate what's in the node information atom container.
     */
     
    err = QTVRGetNodeInfo( qtvrInst, nodeID, &nodeInfoContainer );
    if (err != noErr) {
        CheckAndSetErrorResult( interp, err );
        return TCL_ERROR;
    }
    headerAtom = QTFindChildByID( nodeInfoContainer, kParentAtomIsContainer,
            kQTVRNodeHeaderAtomType, 1, NULL );
    if (headerAtom == 0) {
        CheckAndSetErrorResult( interp, err );
        return TCL_ERROR;
    }
    QTLockContainer( nodeInfoContainer );
    err = QTGetAtomDataPtr( nodeInfoContainer, headerAtom, NULL, (Ptr *) &nodeHeaderPtr );

    /* 
     * Find any name atom. 
     */

    if ((err == noErr) && (EndianS32_BtoN(nodeHeaderPtr->nameAtomID) != 0)) {
        nameAtom = QTFindChildByID( nodeInfoContainer, kParentAtomIsContainer,
                kQTVRStringAtomType, EndianS32_BtoN(nodeHeaderPtr->nameAtomID), NULL );
        if (nameAtom != 0) {
            err = QTGetAtomDataPtr( nodeInfoContainer, nameAtom, NULL,
                    (Ptr *) &stringAtomPtr );
            if (err == noErr) {
                len = EndianU16_BtoN(stringAtomPtr->stringLength);
				len = (len > 250) ? 250 : len;
				memset( tmpstr, 0, 255 );
				memcpy( tmpstr, &stringAtomPtr->theString, len );
    		    Tcl_ListObjAppendElement( interp, listObjPtr, 
                        Tcl_NewStringObj("-name", -1) );		    	
                Tcl_ExternalToUtfDString( gQTTclTranslationEncoding, tmpstr, -1, &ds );                                                                         	
		    	Tcl_ListObjAppendElement( interp, listObjPtr, 
                        Tcl_NewStringObj(Tcl_DStringValue(&ds), -1) );		    	
                Tcl_DStringFree(&ds);		    	
            }
        }
    }

    /* 
     * Get hotspot parent atom which is the parent of all hot spot atoms of the node. 
     */

    hotspotParentAtom = QTFindChildByID( nodeInfoContainer, kParentAtomIsContainer,
            kQTVRHotSpotParentAtomType, 1, NULL );
    if (hotspotParentAtom != 0) {
        short                   numHotspots = 0;
        QTVRHotSpotInfoAtomPtr  hotspotAtomPtr;

        numHotspots = QTCountChildrenOfType( nodeInfoContainer, hotspotParentAtom, 
                kQTVRHotSpotAtomType );
    	Tcl_ListObjAppendElement( interp, *resObj, 
                Tcl_NewStringObj("-hotspots", -1) );		    	

        /*
         * Loop over all hotspots.
         */

        for (index = 1; index <= numHotspots; index++) {
            hotspotAtom = 0;
            hotspotAtom = QTFindChildByIndex( nodeInfoContainer, hotspotParentAtom, 
                    kQTVRHotSpotAtomType, index, &atomHotSpotID );
            if (hotspotAtom != 0) {
    		    hotspotObjPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );
   		    	Tcl_ListObjAppendElement( interp, hotspotObjPtr, 
                           Tcl_NewStringObj("-hotspotid", -1) );		    	
   		    	Tcl_ListObjAppendElement( interp, hotspotObjPtr, 
                           Tcl_NewIntObj(atomHotSpotID) );		    	

                /*
                 * Get the hotspot info atom here.
                 */

                hotspotInfoAtom = 0;
                hotspotInfoAtom = QTFindChildByIndex( nodeInfoContainer, hotspotAtom, 
                        kQTVRHotSpotInfoAtomType, 1, &atomID );
                if (hotspotInfoAtom != 0) {
                    err = QTGetAtomDataPtr( nodeInfoContainer, hotspotInfoAtom, NULL,
                            (Ptr *) &hotspotAtomPtr );
                    if (err == noErr) {
                        if (EndianS32_BtoN(hotspotAtomPtr->nameAtomID) != 0) {

                            nameAtom = QTFindChildByID( nodeInfoContainer, hotspotAtom,
                                    kQTVRStringAtomType, 
									EndianS32_BtoN(hotspotAtomPtr->nameAtomID), NULL );
                            if (nameAtom != 0) {                            
                                err = QTGetAtomDataPtr( nodeInfoContainer, nameAtom, NULL,
                                        (Ptr *) &stringAtomPtr );
                                if (err == noErr) {
					                len = EndianU16_BtoN(stringAtomPtr->stringLength);
									len = (len > 250) ? 250 : len;
                    				memset( tmpstr, 0, 255 );
                    				memcpy( tmpstr, &stringAtomPtr->theString, len );
                        		    Tcl_ListObjAppendElement( interp, hotspotObjPtr, 
                                            Tcl_NewStringObj("-name", -1) );		  
            	                    Tcl_ExternalToUtfDString( gQTTclTranslationEncoding, tmpstr, -1, &ds );                                      	
                    		    	Tcl_ListObjAppendElement( interp, hotspotObjPtr, 
                                            Tcl_NewStringObj(Tcl_DStringValue(&ds), -1) );		    	
                            	    Tcl_DStringFree(&ds);		    
                                }
                            }
                        }
                        if (EndianS32_BtoN(hotspotAtomPtr->commentAtomID) != 0) {

                            nameAtom = QTFindChildByID( nodeInfoContainer, hotspotAtom,
                                    kQTVRStringAtomType, 
									EndianS32_BtoN(hotspotAtomPtr->commentAtomID), NULL );
                            if (nameAtom != 0) {                            
                                err = QTGetAtomDataPtr( nodeInfoContainer, nameAtom, NULL,
                                        (Ptr *) &stringAtomPtr );
                                if (err == noErr) {
					                len = EndianU16_BtoN(stringAtomPtr->stringLength);
									len = (len > 250) ? 250 : len;
                    				memset( tmpstr, 0, 255 );
                    				memcpy( tmpstr, &stringAtomPtr->theString, len );
                        		    Tcl_ListObjAppendElement( interp, hotspotObjPtr, 
                                            Tcl_NewStringObj("-comment", -1) );		    	
            	                    Tcl_ExternalToUtfDString( gQTTclTranslationEncoding, tmpstr, -1, &ds );                                                                         	
                    		    	Tcl_ListObjAppendElement( interp, hotspotObjPtr, 
                                            Tcl_NewStringObj(Tcl_DStringValue(&ds), -1) );		    	
                            	    Tcl_DStringFree(&ds);		    
                                }
                            }
                        }
						ConvertBigEndianFloatToNative( &(hotspotAtomPtr->bestPan) );
						ConvertBigEndianFloatToNative( &(hotspotAtomPtr->bestTilt) );
						ConvertBigEndianFloatToNative( &(hotspotAtomPtr->bestFOV) );

        		    	Tcl_ListObjAppendElement( interp, hotspotObjPtr, 
                                Tcl_NewStringObj("-hotspottype", -1) );
                        if (EndianU32_BtoN(hotspotAtomPtr->hotSpotType) == kQTVRHotSpotLinkType) {
            		    	Tcl_ListObjAppendElement( interp, hotspotObjPtr, 
                                    Tcl_NewStringObj("link", -1) );
                        } else if (EndianU32_BtoN(hotspotAtomPtr->hotSpotType) == kQTVRHotSpotURLType) {
            		    	Tcl_ListObjAppendElement( interp, hotspotObjPtr, 
                                    Tcl_NewStringObj("url", -1) );
                        } else if (EndianU32_BtoN(hotspotAtomPtr->hotSpotType) == kQTVRHotSpotUndefinedType) {
            		    	Tcl_ListObjAppendElement( interp, hotspotObjPtr, 
                                    Tcl_NewStringObj("undefined", -1) );
                        } else {
            		    	Tcl_ListObjAppendElement( interp, hotspotObjPtr, 
                                    Tcl_NewStringObj("undefined", -1) );
                        }
        		    	Tcl_ListObjAppendElement( interp, hotspotObjPtr, 
                                Tcl_NewStringObj("-bestpan", -1) );		    	
        		    	Tcl_ListObjAppendElement( interp, hotspotObjPtr, 
                                Tcl_NewDoubleObj(hotspotAtomPtr->bestPan) );		    	
        		    	Tcl_ListObjAppendElement( interp, hotspotObjPtr, 
                                Tcl_NewStringObj("-besttilt", -1) );		    	
        		    	Tcl_ListObjAppendElement( interp, hotspotObjPtr, 
                                Tcl_NewDoubleObj(hotspotAtomPtr->bestTilt) );		    	
        		    	Tcl_ListObjAppendElement( interp, hotspotObjPtr, 
                                Tcl_NewStringObj("-bestfov", -1) );		    	
        		    	Tcl_ListObjAppendElement( interp, hotspotObjPtr, 
                                Tcl_NewDoubleObj(hotspotAtomPtr->bestFOV) );
        		    	Tcl_ListObjAppendElement( interp, hotspotObjPtr, 
                                Tcl_NewStringObj("-bounds", -1) );		    	
            		    rectListObjPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );
        		    	Tcl_ListObjAppendElement( interp, rectListObjPtr, 
                                Tcl_NewIntObj(EndianS16_BtoN(hotspotAtomPtr->hotSpotRect.left)) );		    	
        		    	Tcl_ListObjAppendElement( interp, rectListObjPtr, 
                                Tcl_NewIntObj(EndianS16_BtoN(hotspotAtomPtr->hotSpotRect.top )) );		    	
        		    	Tcl_ListObjAppendElement( interp, rectListObjPtr, 
                                Tcl_NewIntObj(EndianS16_BtoN(hotspotAtomPtr->hotSpotRect.right)) );		    	
        		    	Tcl_ListObjAppendElement( interp, rectListObjPtr, 
                                Tcl_NewIntObj(EndianS16_BtoN(hotspotAtomPtr->hotSpotRect.bottom)) );		    	

        		    	Tcl_ListObjAppendElement( interp, hotspotObjPtr, rectListObjPtr );
                    }
    		    	Tcl_ListObjAppendElement( interp, listObjPtr, hotspotObjPtr );
                }
            }
        }
    }
    if (nodeInfoContainer) {
        QTUnlockContainer( nodeInfoContainer );
        QTDisposeAtomContainer( nodeInfoContainer );
    }
	Tcl_ListObjAppendElement( interp, *resObj, listObjPtr );
	
	return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * GetNodeCount, SetPanoramaByDegrees, ZoomInOrOutPanorama --
 *
 *  	A collection of utilities for pano movies.
 *
 * Results:
 *  	Tcl result, Boolean, None.
 *
 * Side effects:
 *		None.
 *
 *----------------------------------------------------------------------
 */

int
GetNodeCount( QTVRInstance qtvrInst )
{
    QTAtomContainer     vrWorld = NULL;
    QTAtom              nodeParentAtom;
    int                 numNodes = 0;
    OSErr               err = noErr;
    
    err = QTVRGetVRWorld( qtvrInst, &vrWorld );
    if (err != noErr) {
        return numNodes;
    }
    nodeParentAtom = QTFindChildByIndex( vrWorld, kParentAtomIsContainer, 
            kQTVRNodeParentAtomType, 1, NULL );
    if (nodeParentAtom != 0) {
        numNodes = QTCountChildrenOfType( vrWorld, nodeParentAtom, kQTVRNodeIDAtomType );
    }
    QTDisposeAtomContainer( vrWorld );
    return numNodes;
}

Boolean
SetPanoramaByDegrees( QTVRInstance qtvrInst, long direction, float angle )
{
    Boolean     moved;
    
    switch (direction) {
        case kDirUp:
            QTVRSetTiltAngle( qtvrInst, angle );
            break;
        case kDirDown:
            QTVRSetTiltAngle( qtvrInst, -angle );
            break;
        case kDirLeft:
            QTVRSetPanAngle( qtvrInst, angle );
            break;
        case kDirRight:
            QTVRSetPanAngle( qtvrInst, -angle );
            break;
        default:
            break;
    }
    
    /* Update the image on the screen. */
    QTVRUpdate( qtvrInst, kQTVRStatic );

    switch (direction) {
        case kDirUp:
        case kDirDown:
            moved = (angle != QTVRGetTiltAngle( qtvrInst ));
            break;
        case kDirLeft:
        case kDirRight:
            moved = (angle != QTVRGetPanAngle( qtvrInst ));
            break;
        default:
            break;
    }
    return moved;    
}

void
ZoomInOrOutPanorama( QTVRInstance qtvrInst, long direction, float fov )
{
    float   oldFov;
    
    // we just set it directly, later could use a factor instead.
    oldFov = QTVRGetFieldOfView( qtvrInst );
    switch (direction) {
        case kDirIn:

            break;
        case kDirOut:

            break;
        default:
            break;
    }
    QTVRSetFieldOfView( qtvrInst, fov );
    QTVRUpdate( qtvrInst, kQTVRStatic );
}

/*---------------------------------------------------------------------------*/