/*
 * TracksCommand.c --
 *
 *		Process the "tracks" command.
 *
 * Copyright (c) 1998       Bruce O'Neel
 * Copyright (c) 2000-2005  Mats Bengtsson
 *
 * $Id: TracksCommand.c,v 1.7 2006/10/10 12:17:23 matben Exp $
 */

#include "MoviePlayer.h"

/*
 * For dispatching tracks commands. Order!!!
 */

static char *allTracksCmds[] = {
	"add", "configure", "copy", 
	"delete", "disable", "duplicate", 
	"enable", "full",
	"list", "media", "new",
	"nextinterestingtime",
	"paste", "picture", "scale", 
	"select", "size", "userdata",
    (char *) NULL
};

enum {
    kTracksCmdAdd                   = 0L,
    kTracksCmdConfigure,
    kTracksCmdCopy,
    kTracksCmdDelete,
    kTracksCmdDisable,
    kTracksCmdDuplicate,
    kTracksCmdEnable,
    kTracksCmdFull,
    kTracksCmdList,
    kTracksCmdMedia,
    kTracksCmdNew,
    kTracksCmdNextInterestingTime,
    kTracksCmdPaste,
    kTracksCmdPicture,
    kTracksCmdScale,
    kTracksCmdSelect,
    kTracksCmdSize,
    kTracksCmdUserData
};

static char *allTracksAddCmds[] = {
	"chapters", "picture", "picturefile",
	"space", "text",
    (char *) NULL
};

enum {
    kTracksCmdAddChapters            	= 0L,
    kTracksCmdAddPicture,
    kTracksCmdAddPictureFile,
    kTracksCmdAddSpace,
    kTracksCmdAddText
};

static char *allTracksAddPictureOptions[] = {
	"-colordepth", "-compressor", "-dialog",
	"-keyframerate", "-spatialquality", "-temporalquality",
    (char *) NULL
};

enum {
    kTracksAddPictureColorDepth             	= 0L,
    kTracksAddPictureCompressor,
    kTracksAddDialog,
    kTracksAddKeyFrameRate,
    kTracksAddSpatialQuality,
    kTracksAddTemporalQuality
};

static char *allTracksAddTextOptions[] = {
	"-background", "-cliptobox", "-font", "-foreground", 
	"-justification", "-scrolldelay", "-scrollhorizontal", 
	"-scrollin", "-scrollout", "-scrollreverse", "-textbox",
    (char *) NULL
};

enum {
    kTracksAddTextOptionBackground              = 0L,
    kTracksAddTextOptionClipToBox,
    kTracksAddTextOptionFont,
    kTracksAddTextOptionForeground,
    kTracksAddTextOptionJustification,
    kTracksAddTextOptionScrollDelay,
    kTracksAddTextOptionScrollHorizontal,
    kTracksAddTextOptionScrollIn,
    kTracksAddTextOptionScrollOut,
    kTracksAddTextOptionScrollReverse,
    kTracksAddTextOptionTextBox
};

static char *allTracksMediaCmd[] = {
	"nextinterestingtime", "samplecount", 
	"sampledescriptioncount", "samplenum",
	"syncsamplecount", "time", "timefromnum",
	"userdata",
    (char *) NULL
};

enum {
    kTracksMediaCmdNextInterestingTime			= 0L,
    kTracksMediaCmdSampleCount,
    kTracksMediaCmdSampleDescriptionCount,
    kTracksMediaCmdSampleNum,
    kTracksMediaCmdSyncSampleCount,
    kTracksMediaCmdTime,
    kTracksMediaCmdTimeFromNum,
    kTracksMediaCmdUserData
};

static char *allMediaTypes[] = {
	"video", "sound", "text", "music", "sprite", "flash",
    (char *) NULL
};

enum {
    kMediaTypeVideoMediaType			= 0L,
    kMediaTypeSoundMediaType,
    kMediaTypeTextMediaType,
    kMediaTypeMusicMediaType,
    kMediaTypeSpriteMediaType,
    kMediaTypeFlashMediaType
};

/*
 * For dispatching tracks options. Order!!!
 */

static char *allTracksOptions[] = {
	"-balance", "-enabled", "-graphicsmode", "-graphicsmodecolor",
	"-layer", "-matrix", "-offset", "-volume",
    (char *) NULL
};

enum {
    kTracksOptionBalance                    = 0L,
    kTracksOptionEnabled,
    kTracksOptionGraphicsMode,
    kTracksOptionGraphicsModeColor,
    kTracksOptionLayer,
    kTracksOptionMatrix,
    kTracksOptionOffset,
    kTracksOptionVolume
};

/*
 * Use hash table to map between human readable option names and long types
 * for codec qualities.
 */
 
static Tcl_HashTable 	*gCodecQualityHashPtr = NULL;

typedef struct CodecQualityEntry {
    char       	*option;
  	CodecQ 		quality;		/* long */
} CodecQualityEntry;

CodecQualityEntry CodecQualityLookupTable[] = {
	{"min",					codecMinQuality},
	{"low",					codecLowQuality},
	{"normal",				codecNormalQuality},
	{"high",				codecHighQuality},
	{"max",					codecMaxQuality},
	{"lossless",			codecLosslessQuality},
	{NULL, 0}
};

EXTERN TrackScrap   gTrackScrap;


static void     InvalidateTrackSelectionAndScrap( MoviePlayer *movPtr, 
                        long trackID );
static int      ConfigureTracks( Tcl_Interp *interp, MoviePlayer *movPtr, 
                        long trackID, int objc, Tcl_Obj *CONST objv[] );
static int		AddTracks( Tcl_Interp *interp, MoviePlayer *movPtr, 
                        int objc, Tcl_Obj *CONST objv[] ) ;
static int		SetupOptionHashTables( Tcl_Interp *interp );


/*
 *----------------------------------------------------------------------
 *
 * ProcessTracksObjCmd --
 *
 *		Process the "tracks" command.
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
ProcessTracksObjCmd( ClientData clientData, 
		Tcl_Interp *interp,
	    int objc, 				/* starts with first argument after 'tracks' */
	    Tcl_Obj *CONST objv[] ) 
{	
	MoviePlayer 	*movPtr = (MoviePlayer *) clientData;
	Movie           aMovie = movPtr->aMovie;
    MovieController aController = movPtr->aController;	
    int             cmdIndex;
    int             optIndex;
    int				booleanInt;
	short           volume;
	long 			timeScale;
	long 			width;
	long 			height;
	Fixed           fixWidth;
	Fixed           fixHeight;
	Track 			myTrack = NULL;
	Track           myNewTrack = NULL;
	Media 			myMedia = NULL;
	Media           myNewMedia = NULL;
	OSType 			mediaType;
	Str255 			creatorName;
	Handle          h = NULL;
	Rect            rect;
	RGBColor        transparentColor = {0x0000, 0x0000, 0x0000};
	long			lType;
	long			longValue;
	int 			i;
	char 			tmpstr[STR255LEN];
    char			type8Char[8];
	long            trackID;
	TimeValue       movTime;
	TimeValue       movTimeIn;
	TimeValue       movDuration;
	TimeValue       newDuration;
	PicHandle       thePic = NULL;	
	UserData        ud;
	Tcl_Obj			*listObjPtr;
	Tcl_Obj			*subListObjPtr;
	Tcl_Obj			*resultObjPtr;
	OSErr           err;
    int 			result = TCL_OK;
		
  	if (gCodecQualityHashPtr == NULL) {
  		if (TCL_OK != SetupOptionHashTables( interp )) {
		    return TCL_ERROR;
  		}
  	}

	if (objc == 0) {
	
	    /*
	     * Just return the media types for all the enabled tracks.
	     */
	      
		listObjPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );

		for (i = 1; i <= GetMovieTrackCount( aMovie ); i++) {
			myTrack = GetMovieIndTrack( aMovie, i );
			if (myTrack != NULL) {
				myMedia = GetTrackMedia( myTrack );
				if (myMedia != NULL) {
					GetMediaHandlerDescription( myMedia,
						    &mediaType,
						    creatorName,
						    NULL );

    				/* Be nice to the poor Windows people! Translate endians. */				
					lType = EndianU32_BtoN( mediaType );
					memcpy( tmpstr, &lType, 4 );					
					tmpstr[4] = '\0';
					if (GetTrackEnabled( myTrack )) {
						Tcl_ListObjAppendElement( interp, listObjPtr, 
								Tcl_NewStringObj(tmpstr, -1) );				
					}
				}
			}
		}
		Tcl_SetObjResult( interp, listObjPtr );
	    return result;
    }

	if (Tcl_GetIndexFromObj( interp, objv[0], allTracksCmds, "tracks command", 
			TCL_EXACT, &cmdIndex ) != TCL_OK ) {
	    return TCL_ERROR;
	}
    
    /*
     * Dispatch the tracks command to the right branch.
     */
    
    switch (cmdIndex) {

        case kTracksCmdAdd: {
	    		if (objc < 2) {
				Tcl_WrongNumArgs( interp, 0, objv, 
						"pathName tracks add type ?args?" );
    			return TCL_ERROR;
    		}
            result = AddTracks( interp, movPtr, objc - 1, objv + 1 );               
    	    break;
       	}

        case kTracksCmdConfigure: {
            
            /*
             * Limited 'configure' tracks command.
             */
        
    		if (objc < 2) {
				Tcl_WrongNumArgs( interp, 0, objv, 
						"pathName tracks configure trackID ?options ...?" );
    			return TCL_ERROR;
    		}
			if (Tcl_GetLongFromObj( interp, objv[1], &trackID ) != TCL_OK) {
				Tcl_AddErrorInfo( interp, "\n	(processing trackID value)" );
	    		return TCL_ERROR;
			}
            result = ConfigureTracks( interp, movPtr, trackID, objc - 2, objv + 2 );        
            break;
        }

        case kTracksCmdCopy: {

    	    /* 
    	     * Copies one track selection to the fake track clipboard. Should we invalidate 
    	     * the real scrap????
    	     */
    	     
    		if (objc != 1) {
				Tcl_WrongNumArgs( interp, 0, objv, 
						"pathName tracks copy" );
    			return TCL_ERROR;
    		}
    		if (movPtr->trackSelect->trackID != 0) {
    		
    		    /*
    		     * We've got a track selection, copy it to the global and fake track clipboard.
    		     * Remove any ordinary scrap.
    		     */
    		
    		    gTrackScrap.movPtr = movPtr;
    		    gTrackScrap.trackSelect = *movPtr->trackSelect;
#if TARGET_API_MAC_CARBON
                ClearCurrentScrap();
#else
    		    ZeroScrap();
#endif    		  
    		} else {
				Tcl_SetObjResult( interp, Tcl_NewStringObj( 
						"No valid track selection", -1 ) );
    		    result = TCL_ERROR;
    		}
    	    break;
        }

        case kTracksCmdDelete: {
    	
            if ((objc == 1) || (objc == 2) || (objc == 4)) {
                if (objc == 1) {
                
    	            /*
    	             * If any track selection and no arguments to this command, delete the
    	             * selection for the specified duration.
    	             */
    	            
    	            if (movPtr->trackSelect->trackID) {
        				trackID = movPtr->trackSelect->trackID;
    	                movTime = movPtr->trackSelect->startTime;
    	                movDuration = movPtr->trackSelect->duration;
    	            } else {
						Tcl_SetObjResult( interp, Tcl_NewStringObj( 
								"No valid track selection", -1 ) );
            			return TCL_ERROR;
    	            } 
    	        } else if (objc == 2) {

            	    /*
            	     * Delete the specified track ID completely.
            	     */

					if (Tcl_GetLongFromObj( interp, objv[1], &trackID ) 
							!= TCL_OK) {
						Tcl_AddErrorInfo( interp, 
								"\n	(processing trackID value)" );
			    		return TCL_ERROR;
					}
    	        } else if (objc == 4) {
    	
            	    /*
            	     * Delete the specified segment in the track.
            	     */

					if (Tcl_GetLongFromObj( interp, objv[1], &trackID ) 
							!= TCL_OK) {
						Tcl_AddErrorInfo( interp, 
								"\n	(processing trackID value)" );
			    		return TCL_ERROR;
					}
                    if (GetMovieStartTimeFromObj( interp, aMovie, objv[2], &longValue ) != TCL_OK) {
                        return TCL_ERROR;
                    } 
                    movTime = longValue;
                    if (GetMovieDurationFromObj( interp, aMovie, objv[3], movTime, &longValue ) != TCL_OK) {
                        return TCL_ERROR;
                    } 
                    movDuration = longValue;
        		}
    			myTrack = GetMovieTrack( aMovie, trackID );
    			if (myTrack == NULL) {
                    CheckAndSetErrorResult( interp, noErr );
    				return TCL_ERROR;
    			}
    			result = LogUndoState( movPtr, interp );
    			if (objc == 2) {
        			DisposeMovieTrack( myTrack );
                    if (noErr != CheckAndSetErrorResult( interp, noErr )) {
        			    return TCL_ERROR;
        			}    
    	        } else {
                    if (noErr != DeleteTrackSegment( myTrack, movTime, movDuration )) {
        			    CheckAndSetErrorResult( interp, noErr );
        			    return TCL_ERROR;
        	        }
    	        }
    			if (aController) {
    			    MCDoAction( aController,  mcActionMovieEdited, NULL );
    				MCMovieChanged( aController, aMovie );
    			} else if (IsTrackForEyes( myTrack )) {
    			    MoviePlayerWorldChanged( (ClientData) movPtr );
    			}    
    			
    	        /*
    	         * If we have this track in this movie on our fake track clipboard, invalidate it.
    	         * Invalidate the selection as well.
    	         */

    			InvalidateTrackSelectionAndScrap( movPtr, trackID );
    		} else {
    			Tcl_WrongNumArgs( interp, 0, objv, 
    					"pathName tracks delete ?trackID? ?startTime duration?");
    			result = TCL_ERROR;
    		}
    	    break;
        }

        case kTracksCmdDisable:
        case kTracksCmdEnable: {
    	
    	    /*
    	     * Disable or enable the specified track.
    	     */
    	     
    		if (objc != 2) {
    			Tcl_WrongNumArgs( interp, 0, objv, 
    					"pathName tracks disable|enable trackID");
    			result = TCL_ERROR;
    		} else {
    			trackID = 0;
				if (Tcl_GetLongFromObj( interp, objv[1], &trackID ) != TCL_OK) {
					Tcl_AddErrorInfo( interp, 
							"\n	(processing trackID value)" );
		    		return TCL_ERROR;
				}
    			myTrack = GetMovieTrack( aMovie, trackID );
    			if (myTrack != NULL) {
    				if (!strcmp(Tcl_GetString( objv[0] ), "enable")) {
    					SetTrackEnabled( myTrack, true );
    				} else {
    					SetTrackEnabled( myTrack, false );
    				}
    				if (aController) {
        			    MCDoAction( aController,  mcActionMovieEdited, NULL );
    					MCMovieChanged( aController, aMovie );
    				}
    			} else {
    				result = TCL_ERROR;
                    CheckAndSetErrorResult( interp, noErr );
    			}
    		}
    	    break;
        }

        case kTracksCmdDuplicate: {
    	
    	    /*
    	     * Make a copy (duplicate) of the specified track. 
    	     * The fake track clipboard not involved.
    	     */
    	
    		if (objc != 2) {
    			Tcl_WrongNumArgs( interp, 0, objv, 
    					"pathName tracks duplicate trackID" );
    			result = TCL_ERROR;
    		} else {
    		
    			trackID = 0;
				if (Tcl_GetLongFromObj( interp, objv[1], &trackID ) != TCL_OK) {
					Tcl_AddErrorInfo( interp, 
							"\n	(processing trackID value)" );
		    		return TCL_ERROR;
				}
    			myTrack = GetMovieTrack( aMovie, trackID );
    			if (myTrack == NULL) {
                    CheckAndSetErrorResult( interp, noErr );
    				return TCL_ERROR;
                }	
                GetTrackDimensions( myTrack, &fixWidth, &fixHeight );
                if (noErr != CheckAndSetErrorResult( interp, noErr )) {
    				return TCL_ERROR;
                }
                volume = GetTrackVolume( myTrack );
                myMedia = GetTrackMedia( myTrack );
    			if (myMedia == NULL) {
                    CheckAndSetErrorResult( interp, noErr );
    				return TCL_ERROR;
                }	
    			GetMediaHandlerDescription( myMedia, &mediaType, creatorName, NULL);
                timeScale = GetMediaTimeScale( myMedia );
                if (noErr != CheckAndSetErrorResult( interp, noErr )) {
    				return TCL_ERROR;
                }            
    			result = LogUndoState( movPtr, interp );

                /* and the new one... */

                myNewTrack = NewMovieTrack( aMovie, fixWidth, fixHeight, volume );
    			if (myNewTrack == NULL) {
                    CheckAndSetErrorResult( interp, noErr );
    				return TCL_ERROR;
                }	
    			myNewMedia = NewTrackMedia( myNewTrack, mediaType, timeScale, NULL, 0 );
    			if (myNewMedia == NULL) {
                    CheckAndSetErrorResult( interp, noErr );
    				return TCL_ERROR;
    			}
    			if (noErr != CopyTrackSettings( myTrack, myNewTrack )) {
                    CheckAndSetErrorResult( interp, noErr );
    				return TCL_ERROR;
    			}
                
                /* Now we are ready to actually copy the track. */
                
    			err = BeginMediaEdits( myNewMedia );
    			if (noErr != err) {
                    CheckAndSetErrorResult( interp, err );
    				return TCL_ERROR;
    			}
    			/* Need to check this one more time... */
                if (noErr != InsertTrackSegment( 
                        myTrack,                    	/* source track */
                        myNewTrack,                    	/* destination track */
                        0,                            	/* source start */
                        GetTrackDuration(myTrack),   	/* source duration */
                        0 )) {                       	/* destination start */
                    CheckAndSetErrorResult( interp, noErr );
    				return TCL_ERROR;
                }
    			if (noErr != EndMediaEdits( myNewMedia )) {
                    CheckAndSetErrorResult( interp, noErr );
    				return TCL_ERROR;
    			}
    			
    			/*
    			 * The result is to be returned as a list: 
    			 * '{-undostate number} trackID'. Bruce's choice :-(
    			 */
    			
    			resultObjPtr = Tcl_GetObjResult( interp );
    			Tcl_IncrRefCount( resultObjPtr );
				listObjPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );
		    	subListObjPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );
    			Tcl_ListObjAppendList( interp, subListObjPtr, resultObjPtr );
    			Tcl_ListObjAppendElement( interp, listObjPtr, subListObjPtr );
				Tcl_ListObjAppendElement( interp, listObjPtr, 
						Tcl_NewLongObj(GetTrackID( myNewTrack )));						
    			Tcl_SetObjResult( interp, listObjPtr );
    	    }
    	    break;
        }

        case kTracksCmdFull: {
        	char	type8Char[8];
    	
    	    /*
    	     * Long list of info for one track or for all.
    	     * trackID = 0 implies that all tracks to be returned.
    	     */
    	     
    		trackID = 0;
    		if (objc == 2) {
				if (Tcl_GetLongFromObj( interp, objv[1], &trackID ) != TCL_OK) {
					Tcl_AddErrorInfo( interp, 
							"\n		(processing trackID value)" );
		    		return TCL_ERROR;
				}
    		}
		    listObjPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );
    
    		for (i = 1; i <= GetMovieTrackCount(aMovie); i++) {
    			myTrack = GetMovieIndTrack( aMovie, i );
    			if ((myTrack == NULL) || 
    					(noErr != CheckAndSetErrorResult( interp, noErr ))) {
    				return TCL_ERROR;
    			}
    			if (trackID && (GetTrackID(myTrack) != trackID)) {
					continue;
				}
    			myMedia = GetTrackMedia( myTrack );
    			if ((myMedia == NULL) ||
    					(noErr != CheckAndSetErrorResult( interp, noErr ))) {
    				return TCL_ERROR;
    			}    
				GetMediaHandlerDescription( myMedia, &mediaType, creatorName, NULL );
    			if (noErr != CheckAndSetErrorResult( interp, noErr )) {
    				return TCL_ERROR;
    			}    
		    	subListObjPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );
				
				Tcl_ListObjAppendElement( interp, subListObjPtr, 
				        Tcl_NewStringObj("-mediatype", -1) );
				lType = EndianU32_BtoN( mediaType );
				memset( type8Char, '\0', 8 );		
				memcpy( type8Char, &lType, 4 );
				Tcl_ListObjAppendElement( interp, subListObjPtr, 
						Tcl_NewStringObj(type8Char, -1) );
				Tcl_ListObjAppendElement( interp, subListObjPtr, 
				        Tcl_NewStringObj("-trackid", -1) );
				Tcl_ListObjAppendElement( interp, subListObjPtr, 
						Tcl_NewLongObj(GetTrackID( myTrack )) );
				Tcl_ListObjAppendElement( interp, subListObjPtr, 
				        Tcl_NewStringObj("-tracklayer", -1) );
				Tcl_ListObjAppendElement( interp, subListObjPtr, 
						Tcl_NewLongObj(GetTrackLayer( myTrack )) );
				Tcl_ListObjAppendElement( interp, subListObjPtr, 
				        Tcl_NewStringObj("-state", -1) );
				if (GetTrackEnabled( myTrack )) {
					Tcl_ListObjAppendElement( interp, subListObjPtr, 
					        Tcl_NewStringObj("enabled", -1) );
				} else {
					Tcl_ListObjAppendElement( interp, subListObjPtr, 
					        Tcl_NewStringObj("disabled", -1) );
				}
				Tcl_ListObjAppendElement( interp, subListObjPtr, 
				        Tcl_NewStringObj("-trackoffset", -1) );
				Tcl_ListObjAppendElement( interp, subListObjPtr, 
						Tcl_NewLongObj(GetTrackOffset( myTrack )) );
				Tcl_ListObjAppendElement( interp, subListObjPtr, 
				        Tcl_NewStringObj("-trackduration", -1) );
				Tcl_ListObjAppendElement( interp, subListObjPtr, 
						Tcl_NewLongObj(GetTrackDuration( myTrack )) );
				Tcl_ListObjAppendElement( interp, subListObjPtr, 
				        Tcl_NewStringObj("-mediaduration", -1) );
				Tcl_ListObjAppendElement( interp, subListObjPtr, 
						Tcl_NewLongObj(GetMediaDuration( myMedia )) );
				Tcl_ListObjAppendElement( interp, subListObjPtr, 
				        Tcl_NewStringObj("-mediatimescale", -1) );
				Tcl_ListObjAppendElement( interp, subListObjPtr, 
						Tcl_NewLongObj(GetMediaTimeScale( myMedia )) );
				
				if (trackID == 0) {
		    		Tcl_ListObjAppendElement( interp, listObjPtr, subListObjPtr );
				} else {
					Tcl_DecrRefCount( listObjPtr );
					listObjPtr = subListObjPtr;
				}
    		}
		    Tcl_SetObjResult( interp, listObjPtr );
    
    	    break;
        }

        case kTracksCmdList: {
    	
    	    /*
    	     * List only track ID, for specific media or track state.
    	     */
    	     
		    listObjPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );

    		if (objc == 1) {
    			for (i = 1; i <= GetMovieTrackCount(aMovie); i++) {
    				myTrack = GetMovieIndTrack( aMovie, i );
    				if (myTrack != NULL) {
						Tcl_ListObjAppendElement( interp, listObjPtr, 
								Tcl_NewLongObj(GetTrackID( myTrack )) );
    				}
    			}
			    Tcl_SetObjResult( interp, listObjPtr );
    		} else if (objc == 3) {
    			if (strcmp(Tcl_GetString( objv[1] ), "-mediatype") == 0) {
    				for (i = 1; i <= GetMovieTrackCount(aMovie); i++) {
    					myTrack = GetMovieIndTrack( aMovie, i );
    					if (myTrack != NULL) {
    						myMedia = GetTrackMedia( myTrack );
    						if (myMedia != NULL) {
    							GetMediaHandlerDescription( myMedia, &mediaType,
    								    creatorName, NULL );
    							memset( type8Char, 0, 8 );
    							lType = EndianU32_BtoN( mediaType );
    							memcpy( type8Char, &lType, 4 );					
    							if (strcmp( type8Char, Tcl_GetString( objv[2] )) == 0) {
			 						Tcl_ListObjAppendElement( interp, listObjPtr, 
											Tcl_NewLongObj(GetTrackID( myTrack )) );
    							}
    						}
    					}
    				}
				    Tcl_SetObjResult( interp, listObjPtr );
    			} else if (strcmp(Tcl_GetString( objv[1]), "-enabled") == 0) {
					if (Tcl_GetBooleanFromObj( interp, objv[2], &booleanInt ) 
							!= TCL_OK) {
						Tcl_AddErrorInfo( interp, 
								"\n	(processing -enabled option)" );
		    			return TCL_ERROR;
					}
    				for (i = 1; i <= GetMovieTrackCount( aMovie ); i++) {
    					myTrack = GetMovieIndTrack( aMovie, i );
    					if (myTrack != NULL) {
    						if (GetTrackEnabled(myTrack) == booleanInt) {
		 						Tcl_ListObjAppendElement( interp, listObjPtr, 
										Tcl_NewLongObj(GetTrackID( myTrack )) );
    						}
    					}
    				}
				    Tcl_SetObjResult( interp, listObjPtr );
    			} else {
					Tcl_DecrRefCount( listObjPtr );
					Tcl_SetObjResult( interp, Tcl_NewStringObj( 
							"option must be -mediatype or -enabled", -1 ) );
    				result = TCL_ERROR;
    			}
    		} else {
				Tcl_DecrRefCount( listObjPtr );
    			Tcl_WrongNumArgs( interp, 0, objv, 
    					"pathName tracks list ?args?" );
    			result = TCL_ERROR;
    		}
    	    break;
        }

        case kTracksCmdMedia: {
        	TimeValue       	mediaTime;

    		if (objc <= 2) {
				Tcl_WrongNumArgs( interp, 0, objv, 
						"pathName tracks media subCommand trackID ?args ...?" );
    			return TCL_ERROR;
    		}
          	if (Tcl_GetIndexFromObj( interp, objv[1], allTracksMediaCmd, 
          	        "tracks media command", TCL_EXACT, &optIndex )
          	        != TCL_OK ) {
          	    return TCL_ERROR;
          	}
			if (Tcl_GetLongFromObj( interp, objv[2], &trackID ) != TCL_OK) {
				Tcl_AddErrorInfo( interp, 
						"\n	(processing trackID value)" );
	    		return TCL_ERROR;
			}
	   		myTrack = GetMovieTrack( aMovie, trackID );
	   		if (myTrack == NULL) {
	           	CheckAndSetErrorResult( interp, noErr );
	   			return TCL_ERROR;
	   		}
			myMedia = GetTrackMedia( myTrack );
			if (myMedia == NULL) {
	            CheckAndSetErrorResult( interp, noErr );
	            return TCL_ERROR;
			}

		    /*
		     * Dispatch the tracks media command to the right branch.
		     */

		    switch (optIndex) {

        		case kTracksMediaCmdNextInterestingTime: {
	    			TimeValue		interestingTime;
	    			TimeValue		interestingDuration;

	        		if (objc == 3) {
	        			movTime = GetMovieTime( movPtr->aMovie, NULL );
		        		mediaTime = TrackTimeToMediaTime( movTime, myTrack );
	        		} else if (objc == 4) {
						if (Tcl_GetLongFromObj( interp, objv[3], &longValue )
								!= TCL_OK) {
							Tcl_AddErrorInfo( interp, 
									"\n	(processing mediaTime value)" );
				    		return TCL_ERROR;
						}
	        			mediaTime = longValue;
	        		} else {
						Tcl_WrongNumArgs( interp, 0, objv, 
								"pathName tracks media nextinterestingtime trackID ?mediaTime?" );
		    			return TCL_ERROR;
	        		}
			    	listObjPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );
			    	
	    			GetMediaNextInterestingTime( myMedia, 
	    					nextTimeMediaSample | nextTimeEdgeOK,
	    					mediaTime, fixed1, &interestingTime, 
	    					&interestingDuration );
					Tcl_ListObjAppendElement( interp, listObjPtr, 
							Tcl_NewStringObj("-sampletime", -1) );				
					Tcl_ListObjAppendElement( interp, listObjPtr, 
							Tcl_NewLongObj(interestingTime) );				
					Tcl_ListObjAppendElement( interp, listObjPtr, 
							Tcl_NewStringObj("-sampleduration", -1) );				
					Tcl_ListObjAppendElement( interp, listObjPtr, 
							Tcl_NewLongObj(interestingDuration) );				

	    			GetMediaNextInterestingTime( myMedia, 
	    					nextTimeMediaEdit | nextTimeEdgeOK,
	    					mediaTime, fixed1, &interestingTime, 
	    					&interestingDuration );
					Tcl_ListObjAppendElement( interp, listObjPtr, 
							Tcl_NewStringObj("-edittime", -1) );				
					Tcl_ListObjAppendElement( interp, listObjPtr, 
							Tcl_NewLongObj(interestingTime) );				
					Tcl_ListObjAppendElement( interp, listObjPtr, 
							Tcl_NewStringObj("-editduration", -1) );				
					Tcl_ListObjAppendElement( interp, listObjPtr, 
							Tcl_NewLongObj(interestingDuration) );				

	    			GetMediaNextInterestingTime( myMedia, 
	    					nextTimeSyncSample | nextTimeEdgeOK,
	    					mediaTime, fixed1, &interestingTime, 
	    					&interestingDuration );
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

        		case kTracksMediaCmdSampleCount: {
		    		if (objc != 3) {
						Tcl_WrongNumArgs( interp, 0, objv, 
								"pathName tracks media samplecount trackID" );
		    			return TCL_ERROR;
		    		}
				    Tcl_SetObjResult( interp, 
				    		Tcl_NewLongObj( GetMediaSampleCount( myMedia ) ) );			             		
        			break;
        		}

        		case kTracksMediaCmdSampleDescriptionCount: {
		    		if (objc != 3) {
						Tcl_WrongNumArgs( interp, 0, objv, 
								"pathName tracks media sampledescriptioncount trackID" );
		    			return TCL_ERROR;
		    		}
				    Tcl_SetObjResult( interp, Tcl_NewLongObj( 
				    		GetMediaSampleDescriptionCount( myMedia ) ) );			             		
        			break;
        		}

        		case kTracksMediaCmdSampleNum: {
	    			long		sampleNum;
	    			TimeValue	sampleTime;
	    			TimeValue	sampleDuration;
	    		
	        		if (objc == 3) {
	        			movTime = GetMovieTime( movPtr->aMovie, NULL );
		        		mediaTime = TrackTimeToMediaTime( movTime, myTrack );
	        		} else if (objc == 4) {
						if (Tcl_GetLongFromObj( interp, objv[3], &longValue )
								!= TCL_OK) {
							Tcl_AddErrorInfo( interp, 
									"\n	(processing mediaTime value)" );
				    		return TCL_ERROR;
						}
	        			mediaTime = longValue;
	        		} else {
						Tcl_WrongNumArgs( interp, 0, objv, 
								"pathName tracks media samplenum trackID ?mediaTime?" );
		    			return TCL_ERROR;
	        		}
					MediaTimeToSampleNum( myMedia, mediaTime, &sampleNum, 
							&sampleTime, &sampleDuration );
		    		err = GetMoviesError();
		    		if (err!= noErr) {
				      	CheckAndSetErrorResult( interp, err );
			         	return TCL_ERROR;
			       	}
			    	listObjPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );
					Tcl_ListObjAppendElement( interp, listObjPtr, 
							Tcl_NewStringObj("-samplenum", -1) );				
					Tcl_ListObjAppendElement( interp, listObjPtr, 
							Tcl_NewLongObj(sampleNum) );				
					Tcl_ListObjAppendElement( interp, listObjPtr, 
							Tcl_NewStringObj("-sampletime", -1) );				
					Tcl_ListObjAppendElement( interp, listObjPtr, 
							Tcl_NewLongObj(sampleTime) );				
					Tcl_ListObjAppendElement( interp, listObjPtr, 
							Tcl_NewStringObj("-sampleduration", -1) );				
					Tcl_ListObjAppendElement( interp, listObjPtr, 
							Tcl_NewLongObj(sampleDuration) );				
				    Tcl_SetObjResult( interp, listObjPtr );			     
        			break;
        		}

        		case kTracksMediaCmdSyncSampleCount: {
		    		if (objc != 3) {
						Tcl_WrongNumArgs( interp, 0, objv, 
								"pathName tracks media syncsamplecount trackID" );
		    			return TCL_ERROR;
		    		}
				    Tcl_SetObjResult( interp, Tcl_NewLongObj( 
				    		GetMediaSyncSampleCount( myMedia ) ) );			             		
        			break;
        		}

        		case kTracksMediaCmdTime: {
	        		if (objc == 3) {
	        			movTime = GetMovieTime( aMovie, NULL );
	        		} else if (objc == 4) {
                        if (GetMovieStartTimeFromObj( interp, aMovie, objv[3], &longValue ) != TCL_OK) {
                            return TCL_ERROR;
                        } 
	        			movTime = longValue;
	        		} else {
						Tcl_WrongNumArgs( interp, 0, objv, 
								"pathName tracks media time trackID ?movieTime?" );
		    			return TCL_ERROR;
	        		}
				    Tcl_SetObjResult( interp, Tcl_NewLongObj( 
				    		TrackTimeToMediaTime( movTime, myTrack ) ) );			     	    			    					
        			break;
        		}

        		case kTracksMediaCmdTimeFromNum: {
	    			long		sampleNum;
	    			TimeValue	sampleTime;
	    			TimeValue	sampleDuration;
	    		
	        		if (objc != 4) {
						Tcl_WrongNumArgs( interp, 0, objv, 
								"pathName tracks media timefromnum trackID sampleNum" );
		    			return TCL_ERROR;
	        		}
					if (Tcl_GetLongFromObj( interp, objv[3], &sampleNum )
							!= TCL_OK) {
						Tcl_AddErrorInfo( interp, 
								"\n	(processing sampleNum value)" );
			    		return TCL_ERROR;
					}
					SampleNumToMediaTime( myMedia, sampleNum, &sampleTime, &sampleDuration );
		    		err = GetMoviesError();
		    		if (err!= noErr) {
				      	CheckAndSetErrorResult( interp, err );
			         	return TCL_ERROR;
			       	}
			    	listObjPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );
					Tcl_ListObjAppendElement( interp, listObjPtr, 
							Tcl_NewStringObj("-sampletime", -1) );				
					Tcl_ListObjAppendElement( interp, listObjPtr, 
							Tcl_NewLongObj(sampleTime) );				
					Tcl_ListObjAppendElement( interp, listObjPtr, 
							Tcl_NewStringObj("-sampleduration", -1) );				
					Tcl_ListObjAppendElement( interp, listObjPtr, 
							Tcl_NewLongObj(sampleDuration) );				
				    Tcl_SetObjResult( interp, listObjPtr );			             		
        			break;
        		}

        		case kTracksMediaCmdUserData: {
	        		ud = GetMediaUserData( myMedia );
	        		if (ud == NULL) {
	                    CheckAndSetErrorResult( interp, noErr );
	    				return TCL_ERROR;
	        		}
	    	        if (objc == 3) {
		        		result = GetUserDataListCmd( ud, interp );
	        	    } else if ((objc >= 5) && (objc % 2 == 1)) {  
	        	        result = SetUserDataListCmd( ud, interp, objc - 3, objv + 3 );
	                } else {
						Tcl_WrongNumArgs( interp, 0, objv, 
								"pathName tracks media userdata trackID ?-key value ...?" );
		    			result = TCL_ERROR;
	                }        		
        			break;
        		}
			}
        	break;
        }
        
        case kTracksCmdNew: {

    	    /* 
    	     * Make a a new track. 
    	     */
    	     
    		if (objc == 1) {
				Tcl_WrongNumArgs( interp, 0, objv, 
						"pathName tracks new mediaType ?args?" );
    			result = TCL_ERROR;
    		} else {
    		
    		    /* Check media type. */
    	    	if (Tcl_GetIndexFromObj( interp, objv[1], allMediaTypes, 
		    	        "media type", TCL_EXACT, &optIndex ) != TCL_OK ) {
	    			return TCL_ERROR;
		    	}
		        
		        switch (optIndex) {
		            case kMediaTypeVideoMediaType: {
	    		        mediaType = VideoMediaType;
	    		        timeScale = kVideoTimeScale;
		            	break;
		            }
		            case kMediaTypeSoundMediaType: {
	    		        mediaType = SoundMediaType;		    
	    		        timeScale = kSoundTimeScale;
		            	break;
		            }
		            case kMediaTypeTextMediaType: {
	    		        mediaType = TextMediaType;		    
	    		        timeScale = kTextTimeScale;
		            	break;
		            }
		            case kMediaTypeMusicMediaType: {
	    		        mediaType = MusicMediaType;		    
	    		        timeScale = kMusicTimeScale;
		            	break;
		            }
		            case kMediaTypeSpriteMediaType: {
	    		        mediaType = SpriteMediaType;		    
	    		        timeScale = kSpriteTimeScale;		    
		            	break;
		            }
		            case kMediaTypeFlashMediaType: {
	    		        mediaType = FlashMediaType;		    
	    		        timeScale = kFlashTimeScale;
		            	break;
		            }
		      	}

    		    if ((mediaType == VideoMediaType) || (mediaType == TextMediaType) ||
    		            (mediaType == SpriteMediaType)) {
        		    GetMovieBox( aMovie, &rect );
        		    volume = kNoVolume;
    		        if (objc == 2) {
                		width = rect.right - rect.left;
                		height = rect.bottom - rect.top;
        		    } else if ((objc == 4) && 
        		    		!strcmp(Tcl_GetString( objv[2]), "-timescale")) {
						if (Tcl_GetLongFromObj( interp, objv[3], &timeScale ) 
								!= TCL_OK) {
							Tcl_AddErrorInfo( interp, 
									"\n	(processing -timescale option)" );
				    		return TCL_ERROR;
						}
                		width = rect.right - rect.left;
                		height = rect.bottom - rect.top;        		        
    		        } else if (objc == 4) {
						if (Tcl_GetLongFromObj( interp, objv[2], &width ) 
								!= TCL_OK) {
							Tcl_AddErrorInfo( interp, 
									"\n	(processing width option)" );
				    		return TCL_ERROR;
						}
						if (Tcl_GetLongFromObj( interp, objv[3], &height ) 
								!= TCL_OK) {
							Tcl_AddErrorInfo( interp, 
									"\n	(processing height option)" );
				    		return TCL_ERROR;
						}
        		    } else if ((objc == 6) && 
        		    		!strcmp(Tcl_GetString( objv[4] ), "-timescale")) {
						if (Tcl_GetLongFromObj( interp, objv[2], &width ) 
								!= TCL_OK) {
							Tcl_AddErrorInfo( interp, 
									"\n	(processing width option)" );
				    		return TCL_ERROR;
						}
						if (Tcl_GetLongFromObj( interp, objv[3], &height ) 
								!= TCL_OK) {
							Tcl_AddErrorInfo( interp, 
									"\n	(processing height option)" );
				    		return TCL_ERROR;
						}
						if (Tcl_GetLongFromObj( interp, objv[5], &timeScale ) 
								!= TCL_OK) {
							Tcl_AddErrorInfo( interp, 
									"\n	(processing -timescale option)" );
				    		return TCL_ERROR;
						}
            		} else {
						Tcl_WrongNumArgs( interp, 0, objv, 
								"pathName tracks new type ?width height? ?-timescale scale?" );
        				return TCL_ERROR;
    		        }
    		    } else {
    		        if ((objc == 4) && 
    		        		!strcmp(Tcl_GetString( objv[2] ), "-timescale")) {
						if (Tcl_GetLongFromObj( interp, objv[3], &timeScale ) 
								!= TCL_OK) {
							Tcl_AddErrorInfo( interp, 
									"\n	(processing -timescale option)" );
				    		return TCL_ERROR;
						}
    		        } else if (objc != 2) {
						Tcl_WrongNumArgs( interp, 0, objv, 
								"pathName tracks new type ?-timescale scale?" );
        				return TCL_ERROR;
    		        }
    	            width = 0;
    	            height = 0;
       		        volume = kFullVolume;
    		    }
    		    
    			result = LogUndoState( movPtr, interp );
    			myTrack = NewMovieTrack( aMovie, Long2Fix(width), Long2Fix(height), 
    			        volume );
    			if (myTrack == NULL) {
    			    CheckAndSetErrorResult( interp, noErr );
    				return TCL_ERROR;
    			}
    			myMedia = NewTrackMedia( myTrack, mediaType, timeScale, NULL, 0 );
    			if (myMedia == NULL) {
                    CheckAndSetErrorResult( interp, noErr );
    				return TCL_ERROR;
    			}
    			
    			/*
    			 * The result is to be returned as a list: 
    			 * '{-undostate number} trackID'. Bruce's choice :-(
    			 */
    			
    			resultObjPtr = Tcl_GetObjResult( interp );
    			Tcl_IncrRefCount( resultObjPtr );
				listObjPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );
		    	subListObjPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );
    			Tcl_ListObjAppendList( interp, subListObjPtr, resultObjPtr );
    			Tcl_ListObjAppendElement( interp, listObjPtr, subListObjPtr );
				Tcl_ListObjAppendElement( interp, listObjPtr, 
						Tcl_NewLongObj(GetTrackID( myTrack )));						
    			Tcl_SetObjResult( interp, listObjPtr );
    			 
    			if (width > movPtr->mwidth) {
    				movPtr->mwidth = width;
    			}
    			if (height > movPtr->mheight) {
    				movPtr->mheight = height;
    			}
    			
    			/*
    			 * If this happens to be the first track in the movie, a call to 
    			 * 'MoviePlayerWorldChanged' adds any controller if wants one.
    			 */

    		    MoviePlayerWorldChanged( (ClientData) movPtr );
    			if (aController != NULL) {
    			    MCDoAction( aController,  mcActionMovieEdited, NULL );
    				MCMovieChanged( aController, aMovie );
    			}
    		}
    	    break;
        }        

        case kTracksCmdNextInterestingTime: {
			TimeValue		interestingTime;
			TimeValue		interestingDuration;
		
	   		if (objc == 2) {
	   			movTime = GetMovieTime( aMovie, NULL );
	   		} else if (objc == 3) {
                if (GetMovieStartTimeFromObj( interp, aMovie, objv[2], &longValue ) != TCL_OK) {
                    return TCL_ERROR;
                } 
	   			movTime = longValue;
	   		} else {
				Tcl_WrongNumArgs( interp, 0, objv, 
						"pathName tracks nextinterestingtime trackID ?movieTime?" );
	   			return TCL_ERROR;
	   		}
			if (Tcl_GetLongFromObj( interp, objv[1], &trackID ) != TCL_OK) {
				Tcl_AddErrorInfo( interp, 
						"\n	(processing trackID value)" );
	    		return TCL_ERROR;
			}
        	myTrack = GetMovieTrack( aMovie, trackID );
       		if (myTrack == NULL) {
           		CheckAndSetErrorResult( interp, noErr );
        		return TCL_ERROR;
       		}
	    	listObjPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );
	    	
			GetTrackNextInterestingTime( myTrack, 
					nextTimeMediaSample | nextTimeEdgeOK,
					movTime, fixed1, &interestingTime, &interestingDuration );
			Tcl_ListObjAppendElement( interp, listObjPtr, 
					Tcl_NewStringObj("-sampletime", -1) );				
			Tcl_ListObjAppendElement( interp, listObjPtr, 
					Tcl_NewLongObj(interestingTime) );				
			Tcl_ListObjAppendElement( interp, listObjPtr, 
					Tcl_NewStringObj("-sampleduration", -1) );				
			Tcl_ListObjAppendElement( interp, listObjPtr, 
					Tcl_NewLongObj(interestingDuration) );				

			GetTrackNextInterestingTime( myTrack, 
					nextTimeMediaEdit | nextTimeEdgeOK,
					movTime, fixed1, &interestingTime, 
					&interestingDuration );
			Tcl_ListObjAppendElement( interp, listObjPtr, 
					Tcl_NewStringObj("-edittime", -1) );				
			Tcl_ListObjAppendElement( interp, listObjPtr, 
					Tcl_NewLongObj(interestingTime) );				
			Tcl_ListObjAppendElement( interp, listObjPtr, 
					Tcl_NewStringObj("-editduration", -1) );				
			Tcl_ListObjAppendElement( interp, listObjPtr, 
					Tcl_NewLongObj(interestingDuration) );				

			GetTrackNextInterestingTime( myTrack, 
					nextTimeSyncSample | nextTimeEdgeOK,
					movTime, fixed1, &interestingTime, 
					&interestingDuration );
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

        case kTracksCmdPaste: {
            Track       srcTrack = NULL;
            Track       dstTrack = NULL;

    	    /* 
    	     * Pastes the stuff we've got on the fake track clipboard to the current
    	     * track selection.
    	     * Delete any nonzero selection, just as any typical word processing program.
    	     */
    	     
    		if (objc != 1) {
				Tcl_WrongNumArgs( interp, 0, objv, "pathName tracks paste" );
    			return TCL_ERROR;
    		}
    		if (movPtr->trackSelect->trackID != 0) {
    		
    		    /*
    		     * We've got a track selection, check the fake track clipboard.
    		     */
    		
                if (gTrackScrap.movPtr == NULL) {
					Tcl_SetObjResult( interp, Tcl_NewStringObj( 
							"No valid track clipboard", -1 ) );
    				return TCL_ERROR;
    		    }          
    		    
    		    /* Find the source track... */
    		    
    		    srcTrack = GetMovieTrack( gTrackScrap.movPtr->aMovie, 
    		            gTrackScrap.trackSelect.trackID );
    		    if (srcTrack == NULL) {        
                    CheckAndSetErrorResult( interp, noErr );
    				return TCL_ERROR;
                }		  
                		    
    		    /* ...and the destination track. */
    		    
    		    if (movPtr->trackSelect->trackID == 0) {
					Tcl_SetObjResult( interp, Tcl_NewStringObj( 
							"No valid track selection", -1 ) );
    				return TCL_ERROR;
    		    }
    		    dstTrack = GetMovieTrack( aMovie, movPtr->trackSelect->trackID );
    		    if (dstTrack == NULL) {        
                    CheckAndSetErrorResult( interp, noErr );
    				return TCL_ERROR;
                }		              
                movTime = gTrackScrap.trackSelect.startTime;
                movDuration = gTrackScrap.trackSelect.duration;
                movTimeIn = movPtr->trackSelect->startTime;
    			result = LogUndoState( movPtr, interp );
    						
    		    if (noErr != InsertTrackSegment( srcTrack,  /* source track */
    		            dstTrack,                    	/* destination track */
    		            movTime,                      	/* start selection time */
    		            movDuration,                  	/* duration selection */
    		            movTimeIn )) {                	/* insert time */
                    CheckAndSetErrorResult( interp, noErr );
    				result = TCL_ERROR;
    		    }          
    		    
    		    /*
    		     * The selected segment is deleted *after* the clipboard has been inserted.
    		     */
    		     
    		    movTime = movPtr->trackSelect->startTime + gTrackScrap.trackSelect.duration;
    		    movDuration =  movPtr->trackSelect->duration;
    		    if (movDuration > 0) {
                    if (noErr != DeleteTrackSegment( dstTrack, movTime, movDuration )) {
        			    CheckAndSetErrorResult( interp, noErr );
        			    return TCL_ERROR;
        	        }
    	        }
    	        
    	        /*
    	         * Leave the pasted segment to be the current track selection.
    	         * Start time unchanged.
    	         */
    	         
    	        movPtr->trackSelect->duration = gTrackScrap.trackSelect.duration;
    	        
    	        /*
    	         * The track clipboard need to be updated if the content came from the 
    	         * destination track in the destination movie because it may be invalidated
    	         * by the deleted segment.
    	         * Note that trackID and duration are identical.
    	         */
    	         
    	        if ((gTrackScrap.movPtr->aMovie == aMovie) && 
    	                (gTrackScrap.trackSelect.trackID == movPtr->trackSelect->trackID)) {
                    gTrackScrap.trackSelect.startTime = movPtr->trackSelect->startTime;
    	        }
    	         
    			if (aController) {
    			    MCDoAction( aController,  mcActionMovieEdited, NULL );
    				MCMovieChanged( aController, aMovie );
    			} else if (IsTrackForEyes( srcTrack )) {
    			    MoviePlayerWorldChanged( (ClientData) movPtr );
    			}    

    		} else {
				Tcl_SetObjResult( interp, Tcl_NewStringObj( 
						"No valid track selection", -1 ) );
    		    result = TCL_ERROR;
    		}
    	    break;
        }

        case kTracksCmdPicture: {

    	    /* 
    	     * Make a tk image from a time in the track. 
    	     */
    	     
    		if (objc < 4) {
				Tcl_WrongNumArgs( interp, 0, objv, 
						"pathName tracks picture trackID time imageName ?args?" );
    			return TCL_ERROR;
    		}
    		trackID = 0;
			if (Tcl_GetLongFromObj( interp, objv[1], &trackID ) != TCL_OK) {
				Tcl_AddErrorInfo( interp, 
						"\n	(processing trackID value)" );
	    		return TCL_ERROR;
			}
    		myTrack = GetMovieTrack( aMovie, trackID );
    		if (myTrack != NULL) {

               /*
                * If not visual then error.
                */

                if (!IsTrackForEyes( myTrack )) {
					Tcl_SetObjResult( interp, Tcl_NewStringObj( 
							"No visual media characteristics in track", -1 ) );
    			    return TCL_ERROR;
    		    }
                if (GetMovieStartTimeFromObj( interp, aMovie, objv[2], &longValue ) != TCL_OK) {
                    return TCL_ERROR;
                } 
	   			movTime = longValue;
        		thePic = GetTrackPict( myTrack, movTime );
        		if (thePic != NULL) {
    				long 		dstWidth = 0;
	    			long 		dstHeight = 0;
	    			int			iarg;
    			
	    			/* Process any -width and -height options. */
				    for (iarg = 4; iarg < objc; iarg = iarg + 2) {        		
				        if (strcmp(Tcl_GetString( objv[iarg] ), "-width") == 0) {
							if (Tcl_GetLongFromObj( interp, objv[iarg+1], 
									&dstWidth ) != TCL_OK) {
								Tcl_AddErrorInfo( interp, 
										"\n	(processing -width option)" );
					    		return TCL_ERROR;
							}
				        } else if (strcmp(Tcl_GetString( objv[iarg] ), "-height") == 0) {
							if (Tcl_GetLongFromObj( interp, objv[iarg+1], 
									&dstHeight ) != TCL_OK) {
								Tcl_AddErrorInfo( interp, 
										"\n	(processing -height option)" );
					    		return TCL_ERROR;
							}
				        } else {
							Tcl_SetObjResult( interp, Tcl_NewStringObj( 
									"option must be -width or -height", -1 ) );
			    			return TCL_ERROR;
				        }
				    }
	    			result = ConvertPictureToTkPhoto( interp, thePic, 
	    					dstWidth, dstHeight, Tcl_GetString( objv[3] ) );
            		KillPicture( thePic );
        		} else {   
        		    result = TCL_ERROR;
        		    CheckAndSetErrorResult( interp, noErr );
    	        }			
            } else {
    			result = TCL_ERROR;
    			CheckAndSetErrorResult( interp, noErr );
            }
    	    break;
        }

        case kTracksCmdScale: {
    	
            if ((objc == 2) || (objc == 5)) {
                if (objc == 2) {
                
    	            /*
    	             * If any track selection and no arguments to this command, scale the
    	             * selection for the specified duration.
    	             */
    	            
    	            if (movPtr->trackSelect->trackID) {
        				trackID = movPtr->trackSelect->trackID;
    	                movTime = movPtr->trackSelect->startTime;
    	                movDuration = movPtr->trackSelect->duration;
    	            } else {
						Tcl_SetObjResult( interp, Tcl_NewStringObj( 
								"No valid track selection", -1 ) );
            			return TCL_ERROR;
    	            } 
					if (Tcl_GetLongFromObj( interp, objv[1], &longValue ) != TCL_OK) {
						Tcl_AddErrorInfo( interp, 
								"\n	(processing duration value)" );
			    		return TCL_ERROR;
					}
            		newDuration = longValue;
    	        } else if (objc == 5) {
					if (Tcl_GetLongFromObj( interp, objv[1], &trackID ) != TCL_OK) {
						Tcl_AddErrorInfo( interp, 
								"\n	(processing trackID value)" );
			    		return TCL_ERROR;
					}                    
                    if (GetMovieStartTimeFromObj( interp, aMovie, objv[2], &longValue ) != TCL_OK) {
                        return TCL_ERROR;
                    } 
                    movTime = longValue;
                    if (GetMovieDurationFromObj( interp, aMovie, objv[3], movTime, &longValue ) != TCL_OK) {
                        return TCL_ERROR;
                    } 
                    movDuration = longValue;
					if (Tcl_GetLongFromObj( interp, objv[4], &longValue ) != TCL_OK) {
						Tcl_AddErrorInfo( interp, 
								"\n	(processing duration value)" );
			    		return TCL_ERROR;
					}
            		newDuration = longValue;
        		}
    			myTrack = GetMovieTrack( aMovie, trackID );
    			if (myTrack == NULL) {
                    CheckAndSetErrorResult( interp, noErr );
    				return TCL_ERROR;
    			}
    			result = LogUndoState( movPtr, interp );
    			if (noErr != ScaleTrackSegment( myTrack, movTime, movDuration, newDuration )) {
                    CheckAndSetErrorResult( interp, noErr );
    				return TCL_ERROR;
                }
    			if (aController) {
    			    MCDoAction( aController,  mcActionMovieEdited, NULL );
    				MCMovieChanged( aController, aMovie );
    			}
                
                /* Update the selection to embrace the scaled segment. */
                
                if (objc == 2) {
    	            movPtr->trackSelect->duration = newDuration;
                }
            } else {
				Tcl_WrongNumArgs( interp, 0, objv, "pathName tracks scale args" );
        	    return TCL_ERROR;
            }
    	    break;
        }

        case kTracksCmdSelect: {

    	    /* 
    	     * Makes a selection in one track. What shall we do with the movie selection????
    	     * If no arguments, return the current track selection if any.
    	     */
    	     
    		if (objc == 1) {
        		if (movPtr->trackSelect->trackID != 0) {
        		
        		    /*
        		     * We've got a track selection. Return a description as a list.
        		     */

			    	listObjPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );
					Tcl_ListObjAppendElement( interp, listObjPtr, 
							Tcl_NewLongObj(movPtr->trackSelect->trackID) );				
					Tcl_ListObjAppendElement( interp, listObjPtr, 
							Tcl_NewLongObj(movPtr->trackSelect->startTime) );				
					Tcl_ListObjAppendElement( interp, listObjPtr, 
							Tcl_NewLongObj(movPtr->trackSelect->duration) );				
				    Tcl_SetObjResult( interp, listObjPtr );			     
                }    
            } else if (objc == 2) {
    			if (!(strcmp(Tcl_GetString( objv[1] ), "clear"))) {
                
                    /*
                     * Delete the selected segment.
                     */

    				trackID = movPtr->trackSelect->trackID;
                    movTime = movPtr->trackSelect->startTime;
                    movDuration = movPtr->trackSelect->duration;
        			myTrack = GetMovieTrack( aMovie, trackID );
        			if (myTrack == NULL) {
                        CheckAndSetErrorResult( interp, noErr );
        				return TCL_ERROR;
        			}
        			result = LogUndoState( movPtr, interp );
                    if (noErr != DeleteTrackSegment( myTrack, movTime, movDuration )) {
        			    CheckAndSetErrorResult( interp, noErr );
        			    return TCL_ERROR;
        	        }
        			if (aController) {
        			    MCDoAction( aController,  mcActionMovieEdited, NULL );
        				MCMovieChanged( aController, aMovie );
        			} else if (IsTrackForEyes( myTrack )) {
        			    MoviePlayerWorldChanged( (ClientData) movPtr );
        			}    
        			movPtr->trackSelect->duration = 0; 
                } else {     
            
                    /*
                     * Select the whole track.
                     */
                     
					if (Tcl_GetLongFromObj( interp, objv[1], &trackID ) != TCL_OK) {
						Tcl_AddErrorInfo( interp, 
								"\n	(processing trackID value)" );
			    		return TCL_ERROR;
					}
            		myTrack = GetMovieTrack( aMovie, trackID );
            		if (myTrack == NULL) {
                        CheckAndSetErrorResult( interp, noErr );
            			return TCL_ERROR;
            		}            
                    movPtr->trackSelect->trackID = trackID;
                    movPtr->trackSelect->startTime = 0;
                    movPtr->trackSelect->duration = GetTrackDuration( myTrack );
                }
            } else if ((objc == 3) || (objc == 4)) {
        		trackID = 0;
				if (Tcl_GetLongFromObj( interp, objv[1], &trackID ) != TCL_OK) {
					Tcl_AddErrorInfo( interp, 
							"\n	(processing trackID value)" );
		    		return TCL_ERROR;
				}
        		myTrack = GetMovieTrack( aMovie, trackID );
        		if (myTrack != NULL) {
                    if (GetMovieStartTimeFromObj( interp, aMovie, objv[2], &longValue ) != TCL_OK) {
                        return TCL_ERROR;
                    } 
                    movTime = longValue;
                	if (objc == 3) {
                	    movDuration = 0;
                	} else {                    
                        if (GetMovieDurationFromObj( interp, aMovie, objv[3], movTime, &longValue ) != TCL_OK) {
                            return TCL_ERROR;
                        } 
                        movDuration = longValue;
                	}    
                    movPtr->trackSelect->trackID = trackID;
                    movPtr->trackSelect->startTime = movTime;
                    movPtr->trackSelect->duration = movDuration;
                    
                    /*
                     * Invalidate any movie selection to avoid any interference between the two
                     * clipboards.
                     */
                     
                    SetMovieSelection( aMovie, 0, 0 );
        		} else {
        			CheckAndSetErrorResult( interp, noErr );
        			result = TCL_ERROR;
                }
            } else {
				Tcl_WrongNumArgs( interp, 0, objv, 
						"pathName tracks select ?args?" );
        	    return TCL_ERROR;
        	}
    	    break;
        }

        case kTracksCmdSize: {
    	    
    	    /*
    	     * Return the dimensions of the track.
    	     */
    		
    		if (objc == 2) {			
				if (Tcl_GetLongFromObj( interp, objv[1], &trackID ) != TCL_OK) {
					Tcl_AddErrorInfo( interp, 
							"\n	(processing trackID value)" );
		    		return TCL_ERROR;
				}
        		myTrack = GetMovieTrack( aMovie, trackID );
        		if (myTrack == NULL) {
                    CheckAndSetErrorResult( interp, noErr );
        			return TCL_ERROR;
        		}            
        	    GetTrackDimensions( myTrack, &fixWidth, &fixHeight );
		    	listObjPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );
				Tcl_ListObjAppendElement( interp, listObjPtr, 
						Tcl_NewLongObj(Fix2Long( fixWidth )) );				
				Tcl_ListObjAppendElement( interp, listObjPtr, 
						Tcl_NewLongObj(Fix2Long( fixHeight )) );				
			    Tcl_SetObjResult( interp, listObjPtr );			     
        	} else {
				Tcl_WrongNumArgs( interp, 0, objv, 
						"pathName tracks size trackID" );
        	    return TCL_ERROR;
    		}
    	    break;
        }

        case kTracksCmdUserData: {

    	    /* 
    	     * Set or get the user data name of the track. (Not working yet!)
    	     */
    	     
    	    if (objc >= 2) { 
        		trackID = 0;
				if (Tcl_GetLongFromObj( interp, objv[1], &trackID ) != TCL_OK) {
					Tcl_AddErrorInfo( interp, 
							"\n	(processing trackID value)" );
		    		return TCL_ERROR;
				}
        		myTrack = GetMovieTrack( aMovie, trackID );
        		if (myTrack == NULL) {
                    CheckAndSetErrorResult( interp, noErr );
        			return TCL_ERROR;
        		}            
        		ud = GetTrackUserData( myTrack );
        		if (ud == NULL) {
                    CheckAndSetErrorResult( interp, noErr );
    				return TCL_ERROR;
        		}
    	        if (objc == 2) {
	        		result = GetUserDataListCmd( ud, interp );
        	    } else if ((objc >= 4) && (objc % 2 == 0)) {  
        	        result = SetUserDataListCmd( ud, interp, objc - 2, objv + 2 );
                } else {
					Tcl_WrongNumArgs( interp, 0, objv, 
							"pathName tracks userdata trackID ?-key value ...?" );
	    			result = TCL_ERROR;
                }
    		} else {
				Tcl_WrongNumArgs( interp, 0, objv, 
						"pathName tracks userdata trackID ?-key value ...?" );
    			result = TCL_ERROR;
    		}
    	    break;
        }
    }
	
	return result;
}

/*
 *----------------------------------------------------------------------
 *
 * AddTracks --
 *
 *		Process the "tracks add" subcommand.
 *
 * Results:
 *  	Normal TCL results
 *
 * Side effects:
 *		Depends on the command.
 *
 *----------------------------------------------------------------------
 */

static int
AddTracks( Tcl_Interp *interp, MoviePlayer *movPtr, 
        int objc, Tcl_Obj *CONST objv[] ) 
{	
	Movie           aMovie = movPtr->aMovie;
    MovieController aController = movPtr->aController;	
	int 			i;
    int             cmdIndex;
    int             optIndex;
    int				booleanInt;
	long            displayFlags;
	long			lType;
	long            trackID;
	long            textTrackID;
	long			longValue;
	Track 			myTrack = NULL;
	Track           myNewTrack = NULL;
	Media 			myMedia = NULL;
	MediaHandler    mediaHandler;
	TimeValue       movTime;
	TimeValue       movDuration;
	TimeValue       medDuration;
	TimeValue       sampleTime;
	Fixed           fixWidth;
	Fixed           fixHeight;
	Rect			rect;
	Tcl_Obj			*resultObjPtr;
	OSErr           err;
    int 			result = TCL_OK;

	if (Tcl_GetIndexFromObj( interp, objv[0], allTracksAddCmds, 
			"tracks add command", TCL_EXACT, &cmdIndex ) != TCL_OK ) {
	    return TCL_ERROR;
	}
    
    /*
     * Dispatch the tracks add command to the right branch.
     */
    
    switch (cmdIndex) {

        case kTracksCmdAddChapters: {

      		/*
      		 * Makes a reference from the specified track to a text track for chapters.
      		 */
       
			if (aController == NULL) {
				Tcl_SetObjResult( interp, Tcl_NewStringObj( 
						"Chapters need a controller", -1 ) );
				result = TCL_ERROR;
			}
      		if (objc != 3) {
				Tcl_WrongNumArgs( interp, 0, objv, 
						"pathName tracks add chapters srcTrackID textTrackID" );
				return TCL_ERROR;
			}
			if (Tcl_GetLongFromObj( interp, objv[1], &trackID ) != TCL_OK) {
				Tcl_AddErrorInfo( interp, "\n	(processing trackID value)" );
	    		return TCL_ERROR;
			}
			if (Tcl_GetLongFromObj( interp, objv[2], &textTrackID ) != TCL_OK) {
				Tcl_AddErrorInfo( interp, "\n	(processing textTrackID value)" );
	    		return TCL_ERROR;
			}
			myTrack = GetMovieTrack( aMovie, trackID );
			if (myTrack == NULL) {
         		CheckAndSetErrorResult( interp, noErr );
				return TCL_ERROR;
			}
			myNewTrack = GetMovieTrack( aMovie, textTrackID );
			if (myNewTrack == NULL) {
         		CheckAndSetErrorResult( interp, noErr );
				return TCL_ERROR;
			}
			result = LogUndoState( movPtr, interp );
			
     		/* 
     		 * Perhaps we should check that 'myNewTrack' is a text track?
     		 * Should we remove some existing references? 
     		 */
     
     		if (noErr != AddTrackReference( myTrack, myNewTrack, 
             		kTrackReferenceChapterList, NULL )) {
         		CheckAndSetErrorResult( interp, noErr );
				return TCL_ERROR;
     		}                
			if (aController) {
				MCMovieChanged( aController, aMovie );
			}
			break;
		}

        case kTracksCmdAddPicture:
        case kTracksCmdAddPictureFile: {
      		int             iarg;
      		int             nPhotos = 1;
  			Tk_PhotoHandle 	*photoList = NULL;
      		CodecType       codecType = kAnimationCodecType;
      		long            keyFrameRate = 12;
      		CodecQ          spatialQuality = codecNormalQuality;
      		CodecQ          temporalQuality = codecNormalQuality;
      		int             colorDepth = 0;      /* let ICM choose color depth */
  			int             showDialog = false;
  			TimeValue       medDurationPerFrame;
  			long            optFlags = 0L;
			Tcl_HashEntry   *hashPtr = NULL;
			Tcl_Obj			*imageListObjPtr;	/* points to the imageList obj */
			Tcl_Obj			*objPtr;
	
			if (!((objc == 2) || (objc >= 5))) {
				Tcl_WrongNumArgs( interp, 0, objv, 
						"pathName tracks add picture|picturefile args" );
				result = TCL_ERROR;
				goto bail;
			}
  			if (objc == 2) {
  
	       		/*
	       		 * If any track selection and no arguments to this command, replace the
	       		 * selection with the picture for the specified duration.
	       		 */
		            
	            if (movPtr->trackSelect->trackID) {
	  				trackID = movPtr->trackSelect->trackID;
	                movTime = movPtr->trackSelect->startTime;
	                movDuration = movPtr->trackSelect->duration;
	            } else {
					Tcl_SetObjResult( interp, Tcl_NewStringObj( 
							"No valid track selection", -1 ) );
	      			return TCL_ERROR;
	       		} 
	       		imageListObjPtr = objv[1];
	        } else if (objc >= 5) {

	      	    /*
	      	     * Add a tcl image to the specified track ID, with a start time and duration.
	      	     */

				if (Tcl_GetLongFromObj( interp, objv[1], &trackID ) != TCL_OK) {
					Tcl_AddErrorInfo( interp, "\n	(processing trackID value)" );
		    		return TCL_ERROR;
				} 
				if (GetMovieStartTimeFromObj( interp, aMovie, objv[2], &longValue ) != TCL_OK) {
		    		return TCL_ERROR;
				} 
				movTime = longValue;
				if (GetMovieDurationFromObj( interp, aMovie, objv[3], movTime, &longValue ) != TCL_OK) {
		    		return TCL_ERROR;
				} 
				movDuration = longValue;
	       		imageListObjPtr = objv[4];
	  		}
			myTrack = GetMovieTrack( aMovie, trackID );
			if (myTrack == NULL) {
	           	CheckAndSetErrorResult( interp, noErr );
				return TCL_ERROR;
			}

          	/*
          	 * Do we have a list of photos or just a single one?
          	 */

          	if (cmdIndex == kTracksCmdAddPicture) {
          	
		   		if (Tcl_ListObjLength( interp, imageListObjPtr, &nPhotos ) 
		   				!= TCL_OK) {
					return TCL_ERROR;
				}
                photoList = (Tk_PhotoHandle *) ckalloc( nPhotos * sizeof(Tk_PhotoHandle) );
				for (i = 0; i < nPhotos; i++) {			
					if (Tcl_ListObjIndex( interp, imageListObjPtr, i, 
							&objPtr ) != TCL_OK) {
						return TCL_ERROR;
					}
  					if ((photoList[i] = Tk_FindPhoto( interp, Tcl_GetString(objPtr))) 
  							== NULL) {
  						resultObjPtr = Tcl_GetObjResult( interp );
  						Tcl_AppendStringsToObj( resultObjPtr, "Not an image: ", 
  								Tcl_GetString(objPtr), (char *) NULL);
  						return TCL_ERROR;
  					}
				}
          	}
			myMedia = GetTrackMedia( myTrack );
			if (myMedia == NULL) {
         		CheckAndSetErrorResult( interp, noErr );
				return TCL_ERROR;
			}

     		/*
     		 * Parse any command options.
     		 */

		    for (iarg = 5; iarg < objc; iarg += 2) {

		    	if (Tcl_GetIndexFromObj( interp, objv[iarg], allTracksAddPictureOptions, 
		    	        "tracks add picture/picturefile option", TCL_EXACT, &optIndex ) != TCL_OK ) {
		    	    return TCL_ERROR;
		    	}
		    	if (iarg + 1 == objc) {
		    		resultObjPtr = Tcl_GetObjResult( interp );
		    		Tcl_AppendStringsToObj( resultObjPtr, "value for \"",
		    				Tcl_GetString(objv[iarg]), "\"missing", (char *) NULL );
		    	    return TCL_ERROR;
		    	}
		                
		        switch(optIndex) {

		            case kTracksAddPictureColorDepth: {
						if (Tcl_GetLongFromObj( interp, objv[iarg+1], &longValue ) 
								!= TCL_OK) {
							Tcl_AddErrorInfo( interp, 
									"\n	(processing -colordepth option)" );
				    		return TCL_ERROR;
						}
	                  	colorDepth = longValue;
	                  	if (!( (colorDepth == 1) || (colorDepth == 2)  || 
	                  		(colorDepth == 4)    || (colorDepth == 8)  || 
	                  		(colorDepth == 16)   || (colorDepth == 24) ||
	                          (colorDepth == 32) || (colorDepth == 34) || 
	                          (colorDepth == 36) || (colorDepth == 40) || 
	                          (colorDepth == 0) )) {
							Tcl_SetObjResult( interp, Tcl_NewStringObj( 
									"invalid -colordepth value", -1 ) );
	  						return TCL_ERROR;
	              		}
				    	optFlags |= kCompressFlagColorDepth;
						break;
					}

		            case kTracksAddPictureCompressor: {
	             		memcpy( &lType, Tcl_GetString( objv[iarg+1] ), 4 );
	             		codecType = EndianU32_NtoB( lType );
			    		optFlags |= kCompressFlagCompressor;
						break;
					}

		            case kTracksAddDialog: {
						if (Tcl_GetBooleanFromObj( interp, objv[iarg+1], 
								&booleanInt ) != TCL_OK) {
							Tcl_AddErrorInfo( interp, 
									"\n	(processing -dialog option)" );
			    			return TCL_ERROR;
						}
				    	showDialog = booleanInt;
				    	optFlags |= kCompressFlagDialog;
						break;
					}

		            case kTracksAddKeyFrameRate: {
						if (Tcl_GetLongFromObj( interp, objv[iarg+1], &keyFrameRate ) 
								!= TCL_OK) {
							Tcl_AddErrorInfo( interp, 
									"\n	(processing -keyframerate option)" );
				    		return TCL_ERROR;
						}
				    	optFlags |= kCompressFlagKeyFrameRate;
						break;
					}

		            case kTracksAddSpatialQuality: {
		            	hashPtr = Tcl_FindHashEntry( gCodecQualityHashPtr,
	    						Tcl_GetString( objv[iarg+1] ) );
					    if (hashPtr == NULL) {
							Tcl_SetObjResult( interp, Tcl_NewStringObj( 
									"Not a valid quality option", -1 ) );
	  						return TCL_ERROR;
					    }
					    spatialQuality = (CodecQ) Tcl_GetHashValue( hashPtr );
				    	optFlags |= kCompressFlagSpatialQuality;
						break;
					}

		            case kTracksAddTemporalQuality: {
		            	hashPtr = Tcl_FindHashEntry( gCodecQualityHashPtr,
	    						Tcl_GetString( objv[iarg+1] ) );
					    if (hashPtr == NULL) {
							Tcl_SetObjResult( interp, Tcl_NewStringObj( 
									"Not a valid quality option", -1 ) );
	  						return TCL_ERROR;
					    }
					    temporalQuality = (CodecQ) Tcl_GetHashValue( hashPtr );
				    	optFlags |= kCompressFlagTemporalQuality;
						break;
					}
				}
			}    
			result = LogUndoState( movPtr, interp );
	
			/*
			 * If we have an initial selection to work with, start by removing this. 
			 */
		 
			if (objc == 2) {
              	if (noErr != DeleteTrackSegment( myTrack, movTime, movDuration )) {
  			    	CheckAndSetErrorResult( interp, noErr );
  			   	 	return TCL_ERROR;
  	        	}
			}
			if (noErr != BeginMediaEdits( myMedia )) {
              	CheckAndSetErrorResult( interp, noErr );
				return TCL_ERROR;
			}
		
			/*
			 * Need to translate the duration given in movie's time to media's time.
			 */
		 
			medDurationPerFrame = (movDuration * GetMediaTimeScale( myMedia ))/
		        	GetMovieTimeScale( aMovie );
          	medDuration = nPhotos * medDurationPerFrame;
          	if (cmdIndex == kTracksCmdAddPicture) {
          	
				if (TCL_OK != AddImagesToMovie( interp, 
	                   	myMedia, 
				        medDurationPerFrame,/* duration per frame in medias time scale */
	                    nPhotos,
				        photoList, 
				        showDialog, 
	                    codecType,
	                    spatialQuality,
	                    temporalQuality,
	                    (short) colorDepth,
	                    keyFrameRate,
				        &sampleTime )) {   	/* the media time where it was added */
					return TCL_ERROR;
				}
			} else {        
				
				/* picturefile */
				if (TCL_OK != AddImageFileToMovie( interp, 
                      	myMedia, 
			        	medDurationPerFrame,/* duration per frame in medias time scale */
                      	Tcl_GetString( objv[4] ),	/* file name */
                      	optFlags,           /* flags for options specified */
			        	showDialog, 
                      	codecType,
                      	spatialQuality,
                      	temporalQuality,
                      	(short) colorDepth,
                      	keyFrameRate,
			        	&sampleTime )) {    /* the media time where it was added */
					return TCL_ERROR;
				}
			}
            if (photoList != NULL) {
                ckfree( (char *) photoList);
            }
			if (noErr != EndMediaEdits( myMedia )) {
              	CheckAndSetErrorResult( interp, noErr );
				return TCL_ERROR;
			}
          	err = InsertMediaIntoTrack( myTrack,/* the track */
		        	movTime,                  	/* insert time in movie's time */
		        	sampleTime,              	/* media start time in media's time */
					medDuration,               	/* media's duration in media's time */
					fixed1 );                	/* the media's rate */
          	if (err != noErr) {
              	CheckAndSetErrorResult( interp, err );
				return TCL_ERROR;
			}
			if (aController) {
			    MCDoAction( aController,  mcActionMovieEdited, NULL );
				MCMovieChanged( aController, aMovie );
			} else if (IsTrackForEyes( myTrack )) {
			    MoviePlayerWorldChanged( (ClientData) movPtr );
			}    
			if (objc >= 5) {
			
			    /* Remove track selection if on this track, same with the fake scrap. */
			    InvalidateTrackSelectionAndScrap( movPtr, trackID );
			} else {
			
			    /* Keep selection, possibly invalidate the track scrap. */
	              if ((gTrackScrap.movPtr == movPtr) && 
	                          (gTrackScrap.trackSelect.trackID == trackID)) {
	                  gTrackScrap.movPtr = NULL;
	              }	        
			}
			break;
		}

        case kTracksCmdAddSpace: {
   			if (!((objc == 1) || (objc == 4))) {
				Tcl_WrongNumArgs( interp, 0, objv, 
						"pathName tracks add space ?args?" );
				result = TCL_ERROR;
				goto bail;
   			}
	       	if (objc == 1) {
	       
	            /*
	             * If any track selection and no arguments to this command, replace the
	             * selection with empty space for the specified duration.
	             */
	            
	            if (movPtr->trackSelect->trackID) {
	  				trackID = movPtr->trackSelect->trackID;
	                movTime = movPtr->trackSelect->startTime;
	                movDuration = movPtr->trackSelect->duration;
	            } else {
					Tcl_SetObjResult( interp, Tcl_NewStringObj( 
							"No valid track selection", -1 ) );
	      			return TCL_ERROR;
	            } 
	        
	        } else if (objc == 4) {

	      	    /*
	      	     * Add empty space to the specified track ID, with a start time and duration.
	      	     */
				
				if (Tcl_GetLongFromObj( interp, objv[1], &trackID ) != TCL_OK) {
					Tcl_AddErrorInfo( interp, 
							"\n	(processing trackID value)" );
		    		return TCL_ERROR;
				}
				if (GetMovieStartTimeFromObj( interp, aMovie, objv[2], &longValue ) != TCL_OK) {
		    		return TCL_ERROR;
				} 
				movTime = longValue;
				if (GetMovieDurationFromObj( interp, aMovie, objv[3], movTime, &longValue ) != TCL_OK) {
		    		return TCL_ERROR;
				} 
				movDuration = longValue;
	  		}
			myTrack = GetMovieTrack( aMovie, trackID );
			if (myTrack == NULL) {
	              CheckAndSetErrorResult( interp, noErr );
				return TCL_ERROR;
			}
			result = LogUndoState( movPtr, interp );
		
			/*
			 * If we have an initial selection to work with, start by removing this. 
			 */
			 
			if (objc == 1) {
	              if (noErr != DeleteTrackSegment( myTrack, movTime, movDuration )) {
	  			    CheckAndSetErrorResult( interp, noErr );
	  			    return TCL_ERROR;
	  	        }
			}
	          if (noErr != InsertEmptyTrackSegment( myTrack, movTime, movDuration )) {
	              CheckAndSetErrorResult( interp, noErr );
				return TCL_ERROR;
	          } else {
				if (aController) {
	  			    MCDoAction( aController,  mcActionMovieEdited, NULL );
					MCMovieChanged( aController, aMovie );
				} else if (IsTrackForEyes( myTrack )) {
			        MoviePlayerWorldChanged( (ClientData) movPtr );
			    }    
			}
			if (objc == 4) {
			
			    /* Remove track selection if on this track, same with the fake scrap. */
			    InvalidateTrackSelectionAndScrap( movPtr, trackID );
			} else {
			
			    /* Keep selection, possibly invalidate the track scrap. */
	              if ((gTrackScrap.movPtr == movPtr) && 
	                          (gTrackScrap.trackSelect.trackID == trackID)) {
	                  gTrackScrap.movPtr = NULL;
	              }	        
			}	
			break;
		}

        case kTracksCmdAddText: {
   			int             iarg;
			TimeValue       scrollDelay = 0;
			short           textJustification = teCenter;
			RGBColor        macFGColor = {0xFFFF, 0xFFFF, 0xFFFF};
			RGBColor        macBGColor = {0x0000, 0x0000, 0x0000};
			XColor          *xcolPtr;
			Tk_Font         tkFont;
			short           faceNum = 0;
			short           size= 0;
			Style           style = 0;
			Tcl_DString     textDString;
			char            *theText;
                      
		   	/*
		   	 * Add text to a text track. If not text track???
		   	 */
		
   			if (objc < 5) {
				Tcl_WrongNumArgs( interp, 0, objv, 
						"pathName tracks add text trackID startTime duration text ?args? " );
				result = TCL_ERROR;
				goto bail;
   			}
          	if (objc % 2 == 0) {
				Tcl_SetObjResult( interp, Tcl_NewStringObj( 
						"Must have an even number of \"-key value\" options", -1 ) );
				result = TCL_ERROR;
				goto bail;
          	}
			if (Tcl_GetLongFromObj( interp, objv[1], &trackID ) != TCL_OK) {
				Tcl_AddErrorInfo( interp, 
						"\n	(processing trackID value)" );
	    		return TCL_ERROR;
			}
            if (GetMovieStartTimeFromObj( interp, aMovie, objv[2], &longValue ) != TCL_OK) {
                return TCL_ERROR;
            } 
            movTime = longValue;
            if (GetMovieDurationFromObj( interp, aMovie, objv[3], movTime, &longValue ) != TCL_OK) {
                return TCL_ERROR;
            } 
            movDuration = longValue;

			myTrack = GetMovieTrack( aMovie, trackID );
			if (myTrack == NULL) {
	            CheckAndSetErrorResult( interp, noErr );
				return TCL_ERROR;
			}
			myMedia = GetTrackMedia( myTrack );
			if (myMedia == NULL) {
	            CheckAndSetErrorResult( interp, noErr );
				return TCL_ERROR;
			}
			mediaHandler = GetMediaHandler( myMedia );
			if (noErr != BeginMediaEdits( myMedia )) {
	            CheckAndSetErrorResult( interp, noErr );
				return TCL_ERROR;
			}
          	GetTrackDimensions( myTrack, &fixWidth, &fixHeight );
          	if (noErr != CheckAndSetErrorResult( interp, noErr )) {
				return TCL_ERROR;
          	}
          	rect.left = 0;
          	rect.top = 0;
          	rect.right = (short) Fix2Long( fixWidth );
          	rect.bottom = (short) Fix2Long( fixHeight );
          
          	/*
          	 * Process the configuration options if any.
          	 */

          	displayFlags = 0L;
          	for (iarg = 5; iarg < objc; iarg += 2) {
 
          		if (Tcl_GetIndexFromObj( interp, objv[iarg], allTracksAddTextOptions, 
          	    	    "tracks add text option", TCL_EXACT, &optIndex )
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
              	 * Dispatch the tracks add text option to the right branch.
              	 */

              	switch (optIndex) {

                  	case kTracksAddTextOptionBackground: {
                  
                      	/* Get mac color from string that can either by a X11 color name or hex value. */
                      	xcolPtr = Tk_AllocColorFromObj( interp, movPtr->tkwin, 
                      			objv[iarg+1] );
      	            	TkSetMacColor( xcolPtr->pixel, &macBGColor );
                      	break;
                  	}

                  	case kTracksAddTextOptionClipToBox: {
						if (Tcl_GetBooleanFromObj( interp, objv[iarg+1], 
								&booleanInt ) != TCL_OK) {
							Tcl_AddErrorInfo( interp, 
									"\n	(processing -cliptobox option)" );
			    			return TCL_ERROR;
						}
                      	if (booleanInt) {
                          	displayFlags |= dfClipToTextBox;
                      	}
                      	break;
                  	}

                  	case kTracksAddTextOptionFont: {
                      	tkFont = Tk_AllocFontFromObj( interp, movPtr->tkwin, 
                      			objv[iarg+1] );
                      	QTTclGetMacFontAttributes( interp, movPtr->tkwin, tkFont, 
                              	&faceNum, &size, &style );
                      	Tk_FreeFont( tkFont );
                      	break;
                  	}

                  	case kTracksAddTextOptionForeground: {
                  
                      	/* Get mac color from string that can either by a X11 color name or hex value. */
                      	xcolPtr = Tk_AllocColorFromObj( interp, movPtr->tkwin, 
                      			objv[iarg+1] );
      	            	TkSetMacColor( xcolPtr->pixel, &macFGColor );
                      	break;
                  	}

                  	case kTracksAddTextOptionJustification: {
                      	if (strcmp( Tcl_GetString( objv[iarg+1] ), "center" ) == 0) {
                          	textJustification = teCenter;
                      	} else if (strcmp( Tcl_GetString( objv[iarg+1] ), "right" ) == 0) {
                          	textJustification = teFlushRight;
                      	} else if (strcmp( Tcl_GetString( objv[iarg+1] ), "left" ) == 0) {
                          	textJustification = teFlushLeft;
                      	} else if (strcmp( Tcl_GetString( objv[iarg+1] ), "default" ) == 0) {
                          	textJustification = teFlushDefault;
                      	} else { 
							Tcl_SetObjResult( interp, Tcl_NewStringObj( 
									"Unknown option for -justification", -1 ) );
                          	return TCL_ERROR;
                      	}
                      	break;
	               	}

	               	case kTracksAddTextOptionScrollDelay: {
						if (Tcl_GetLongFromObj( interp, objv[iarg+1], 
								&longValue ) != TCL_OK) {
							Tcl_AddErrorInfo( interp, 
									"\n	(processing -scrolldelay option)" );
				    		return TCL_ERROR;
						}
	                  	scrollDelay = longValue;
	                  	break;
	               	}

	              	case kTracksAddTextOptionScrollHorizontal: {
						if (Tcl_GetBooleanFromObj( interp, objv[iarg+1], 
								&booleanInt ) != TCL_OK) {
							Tcl_AddErrorInfo( interp, 
									"\n	(processing -scrollhorizontal option)" );
			    			return TCL_ERROR;
						}
	                  	if (booleanInt) {
	                       	displayFlags |= dfHorizScroll;
	                    }
	                  	break;
	              	}

	               	case kTracksAddTextOptionScrollIn: {
						if (Tcl_GetBooleanFromObj( interp, objv[iarg+1], 
								&booleanInt ) != TCL_OK) {
							Tcl_AddErrorInfo( interp, 
									"\n	(processing -scrollin option)" );
			    			return TCL_ERROR;
						}
	                  	if (booleanInt) {
	                     	displayFlags |= dfScrollIn;
	                  	}
	                  	break;
	              	}

	             	case kTracksAddTextOptionScrollOut: {
						if (Tcl_GetBooleanFromObj( interp, objv[iarg+1], 
								&booleanInt ) != TCL_OK) {
							Tcl_AddErrorInfo( interp, 
									"\n	(processing -scrollout option)" );
			    			return TCL_ERROR;
						}
	                  	if (booleanInt) {
	                      	displayFlags |= dfScrollOut;
	                  	}
	                  	break;
	              	}

	            	case kTracksAddTextOptionScrollReverse: {
						if (Tcl_GetBooleanFromObj( interp, objv[iarg+1], 
								&booleanInt ) != TCL_OK) {
							Tcl_AddErrorInfo( interp, 
									"\n	(processing -scrollreverse option)" );
			    			return TCL_ERROR;
						}
	                  	if (booleanInt) {
	                     	displayFlags |= dfReverseScroll;
	                  	}
	                 	break;
	             	}

	             	case kTracksAddTextOptionTextBox: {
	                  	int         objcBox;
	                  	int			box[4];
	                	Tcl_Obj		*objPtr;

				   		if (Tcl_ListObjLength( interp, objv[iarg+1], &objcBox ) 
				   				!= TCL_OK) {
							return TCL_ERROR;
						}
	                	if (objcBox != 4) {
							Tcl_SetObjResult( interp, Tcl_NewStringObj( 
									"Must have four integers for -textbox", -1 ) );
							return TCL_ERROR;
	                  	}
						for (i = 0; i < objcBox; i++) {			
							if (Tcl_ListObjIndex( interp, objv[iarg+1], i, 
									&objPtr ) != TCL_OK) {
								return TCL_ERROR;
							}
							if (Tcl_GetIntFromObj( interp, objPtr, &box[i] ) 
									!= TCL_OK) {
								Tcl_AddErrorInfo( interp, 
										"\n	(processing -textbox option)" );
								return TCL_ERROR;
							}								
						}
	                  	rect.left = (short) box[0];
	                    rect.top = (short) box[1];
	                    rect.right = (short) (box[0] + box[2]);
	                    rect.bottom = (short) (box[1] + box[3]);
	                    break;
	            	}

	              	default: {
	                  	/* Empty */		
	             	}
	        	}
	   		}

          	/*
          	 * Text translation from UTF-8 to internal.
          	 */
           
          	theText = Tcl_UtfToExternalDString( gQTTclTranslationEncoding, Tcl_GetString( objv[4]) , 
          			-1, &textDString );
			result = LogUndoState( movPtr, interp );
						
			/*
			 * Need to translate the duration given in movie's time to media's time.
			 */
		 
			medDuration = (movDuration * GetMediaTimeScale( myMedia ))/
		    	    GetMovieTimeScale( aMovie );

			/*
			 * And add the text to the specified track.
			 */
		
			if (noErr != TextMediaAddTextSample( 
			        mediaHandler,         	/* the media handler */
			        theText,               	/* the actual text */
			        strlen(theText),       	/* and its length */
			        faceNum,              	/* font number */
			        size,                 	/* font size */
			        style,             		/* text style */
			        &macFGColor,           	/* text color */
			        &macBGColor,          	/* background color */
			        textJustification,    	/* text justification */
			        &rect,                	/* bounding box */
			        displayFlags,        	/* display flags */
			        scrollDelay,          	/* scroll delay */
			        0,                     	/* hilite start */
			        0,                     	/* hilite end */
			        NULL,                  	/* hilite color */
			        medDuration,          	/* length in media's time */
			        &sampleTime )) {     	/* the media time where it was added */
	              	CheckAndSetErrorResult( interp, noErr );
              	Tcl_DStringFree( &textDString );
				return TCL_ERROR;
			}
		Tcl_DStringFree( &textDString );
		if (noErr != EndMediaEdits( myMedia )) {
              CheckAndSetErrorResult( interp, noErr );
			return TCL_ERROR;
		}
		if (noErr != InsertMediaIntoTrack( myTrack, /* the track */
		        movTime,           		/* insert time in movie's time */
		        sampleTime,            	/* media start time in media's time */
				medDuration,           	/* media's duration in media's time */
				fixed1 )) {           	/* the media's rate */
              CheckAndSetErrorResult( interp, noErr );
			return TCL_ERROR;
		}
		if (aController) {
		    MCDoAction( aController,  mcActionMovieEdited, NULL );
			MCMovieChanged( aController, aMovie );
		} else if (IsTrackForEyes( myTrack )) {
		    MoviePlayerWorldChanged( (ClientData) movPtr );
		}    				
			break;
		}

	}

bail:

	return result;
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureTracks --
 *
 *		This procedure is called to process an objv/objc list given to 
 *      'tracks configure'.
 *
 * Results:
 *		The return value is a standard Tcl result.  If TCL_ERROR is
 *		returned, then the interp's result contains an error message.
 *
 * Side effects:
 *		Internal track state changed.
 *
 *----------------------------------------------------------------------
 */

static int 
ConfigureTracks( Tcl_Interp *interp, 
		MoviePlayer *movPtr, 
		long trackID, 
        int objc, 				/* starts with -option */
        Tcl_Obj *CONST objv[] ) 
{
	Movie           aMovie = movPtr->aMovie;
    MovieController aController = movPtr->aController;	
    char            tmpstr[255];
	short           balance;
	short           volume;
	short           layerNo;
    int             iarg;
    int             optIndex;
    int             optIndexIn;
    int             redraw = false;
    int             i;
	int				booleanInt;
    long			longValue;
    long            graphicsMode;
	Track 			myTrack = NULL;
	Media 			myMedia = NULL;
	MediaHandler    mh;
    TimeValue       offset;
	RGBColor        opColor = {0x0000, 0x0000, 0x0000};
    Boolean         hasGraphicsMode = false;
    Boolean         enabled;
    MatrixRecord    matrixRecord;		/* Fixed [3][3] */
	Tcl_Obj         *resObjPtr;
	Tcl_Obj         *modeObjPtr;
	Tcl_Obj         *listObjPtr;
	Tcl_Obj			*resultObjPtr;
	int             result = TCL_OK;

	myTrack = GetMovieTrack( aMovie, trackID );
	if (myTrack == NULL) {
          CheckAndSetErrorResult( interp, noErr );
		return TCL_ERROR;
	}            
    if (objc == 1) {
   	    if (Tcl_GetIndexFromObj( interp, objv[0], allTracksOptions, 
   	    		"tracks option", TCL_EXACT, &optIndexIn ) != TCL_OK ) {
   	        return TCL_ERROR;
   	    }
    }
    if ((objc == 0) || (objc == 1)) {
    
        /*
         * objc == 0: return all 'configure' options.
         * objc == 1: return the option given in objv[0].
         */
         
		myMedia = GetTrackMedia( myTrack );
		if (myMedia == NULL) {
            CheckAndSetErrorResult( interp, noErr );
            return TCL_ERROR;
		}
        mh = GetMediaHandler( myMedia );
		if (mh == NULL) {
            CheckAndSetErrorResult( interp, noErr );
            return TCL_ERROR;
		}
        MediaGetGraphicsMode( mh, &graphicsMode, &opColor );
    	resObjPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );

        for (i = 0; allTracksOptions[i] != NULL; i++) {
        	optIndex = i;
        	if (objc == 1) {
        	
        	    /*
        	     * If we don't have a matching option we get on with the next one.
        	     */
        	
        	    if (optIndexIn != optIndex) {
        	        continue;
        	    }
        	} else {
    	        Tcl_ListObjAppendElement( interp, resObjPtr, 
    	        		Tcl_NewStringObj( allTracksOptions[i], -1 ) );
    	    }
            switch (optIndex) {

                case kTracksOptionBalance: {
                    MediaGetSoundBalance( mh, &balance );
            	    Tcl_ListObjAppendElement( interp, resObjPtr, 
            	    		Tcl_NewIntObj(balance) );
                    break;
                }

                case kTracksOptionEnabled: {
                    enabled = GetTrackEnabled( myTrack );
                	Tcl_ListObjAppendElement( interp, resObjPtr, 
                			Tcl_NewBooleanObj( enabled ) );
                    break;
                }

                case kTracksOptionGraphicsMode: {
            	    modeObjPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );
                    if (graphicsMode >= 64) {
            		    Tcl_ListObjAppendElement( interp, modeObjPtr, 
            				    Tcl_NewStringObj("dithercopy", -1) );
                        graphicsMode -= 64;
                    }
                    switch(graphicsMode) {

                        case blend: {
            				Tcl_ListObjAppendElement( interp, modeObjPtr, 
            				        Tcl_NewStringObj("blend", -1) );
            				break;
                        }
                        case srcCopy: {
            				Tcl_ListObjAppendElement( interp, modeObjPtr, 
            				        Tcl_NewStringObj("copy", -1) );
            				break;
                        }
                        case transparent: {
            				Tcl_ListObjAppendElement( interp, modeObjPtr, 
            				        Tcl_NewStringObj("transparent", -1) );
            				break;
                        }
                        case addPin: {
            				Tcl_ListObjAppendElement( interp, modeObjPtr, 
            				        Tcl_NewStringObj("addpin", -1) );
            				break;
                        }
                        case addOver: {
            				Tcl_ListObjAppendElement( interp, modeObjPtr, 
            				        Tcl_NewStringObj("addover", -1) );
            				break;
                        }
                        case subPin: {
            				Tcl_ListObjAppendElement( interp, modeObjPtr, 
            				        Tcl_NewStringObj("subpin", -1) );
            				break;
                        }
                        case addMax: {
            				Tcl_ListObjAppendElement( interp, modeObjPtr, 
            				        Tcl_NewStringObj("addmax", -1) );
            				break;
                        }
                        case subOver: {
            				Tcl_ListObjAppendElement( interp, modeObjPtr, 
            				        Tcl_NewStringObj("subover", -1) );
            				break;
                        }
                        case adMin: {
            				Tcl_ListObjAppendElement( interp, modeObjPtr, 
            				        Tcl_NewStringObj("addmin", -1) );
            				break;
                        }            
                        case graphicsModeStraightAlpha: {
            				Tcl_ListObjAppendElement( interp, modeObjPtr, 
            				        Tcl_NewStringObj("straighalpha", -1) );
            				break;
                        }            
                        case graphicsModePreWhiteAlpha: {
            				Tcl_ListObjAppendElement( interp, modeObjPtr, 
            				        Tcl_NewStringObj("prewhitealpha", -1) );
            				break;
                        }            
                        case graphicsModePreBlackAlpha: {
            				Tcl_ListObjAppendElement( interp, modeObjPtr, 
            				        Tcl_NewStringObj("preblackalpha", -1) );
            				break;
                        }            
                        case graphicsModeStraightAlphaBlend: {
            				Tcl_ListObjAppendElement( interp, modeObjPtr, 
            				        Tcl_NewStringObj("straighalphablend", -1) );
            				break;
                        }            
            			default: {
            				Tcl_ListObjAppendElement( interp, modeObjPtr, 
            				        Tcl_NewStringObj("unknown", -1) );
            				break;
			            }
                    }            
            		Tcl_ListObjAppendElement( interp, resObjPtr, modeObjPtr );
                    break;
                }

                case kTracksOptionGraphicsModeColor: {
                    memset( tmpstr, 0, 255 );
                    sprintf( tmpstr, "#%04x%04x%04x", opColor.red, opColor.green, opColor.blue );
    				Tcl_ListObjAppendElement( interp, resObjPtr, Tcl_NewStringObj(tmpstr, -1) );
                    break;
                }

                case kTracksOptionLayer: {
                    layerNo = GetTrackLayer( myTrack );
                    if (noErr != CheckAndSetErrorResult( interp, noErr )) {
        				result = TCL_ERROR;
        			} else {
        				Tcl_ListObjAppendElement( interp, resObjPtr, 
        						Tcl_NewLongObj(layerNo) );
        			}        
                    break;
                }

                case kTracksOptionMatrix: {
                    GetTrackMatrix( myTrack, &matrixRecord );
                    if (noErr != CheckAndSetErrorResult( interp, noErr )) {
        				result = TCL_ERROR;
        			} else {
                        if (TranslateMatrixRecordToTclList( interp, &listObjPtr, 
                        		&matrixRecord ) != TCL_OK) {
            				result = TCL_ERROR;
            			} else {
            				Tcl_ListObjAppendElement( interp, resObjPtr, listObjPtr );
                        }
                    }
                    break;
                }

                case kTracksOptionOffset: {
                    offset = GetTrackOffset( myTrack );
                    if (noErr != CheckAndSetErrorResult( interp, noErr )) {
        				result = TCL_ERROR;
        			} else {
        				Tcl_ListObjAppendElement( interp, resObjPtr, 
        						Tcl_NewLongObj(offset) );
        			}        
                    break;
                }

                case kTracksOptionVolume: {
                    volume = GetTrackVolume( myTrack );
                    if (noErr != CheckAndSetErrorResult( interp, noErr )) {
        				result = TCL_ERROR;
        			} else {
            			Tcl_ListObjAppendElement( interp, resObjPtr, 
            					Tcl_NewIntObj(volume) );
        			}            
                    break;
                }
            }
            if ((objc == 1) && (optIndexIn == optIndex)) {
            
                /* 
                 * We've got a matching option and are done!
                 * Strip off one level of the list structure.
                 */
                 
                Tcl_ListObjIndex( interp, resObjPtr, 0, &resultObjPtr );
                Tcl_SetObjResult( interp, resultObjPtr );
                return result;
            }
        }
        Tcl_SetObjResult( interp, resObjPtr );
        return result;
    } else if (objc % 2 == 1) {
		Tcl_SetObjResult( interp, Tcl_NewStringObj( 
				"Must have an even number of -key value options", -1 ) );
		return TCL_ERROR;
    }
    
    /*
     * Set all configuration options for this track.
     */

	myMedia = GetTrackMedia( myTrack );
	if (myMedia == NULL) {
        CheckAndSetErrorResult( interp, noErr );
        return TCL_ERROR;
	}
    mh = GetMediaHandler( myMedia );
	if (mh == NULL) {
        CheckAndSetErrorResult( interp, noErr );
        return TCL_ERROR;
	}
     
    for (iarg = 0; iarg < objc; iarg += 2) {
    
    	if (Tcl_GetIndexFromObj( interp, objv[iarg], allTracksOptions, 
    	        "tracks option", TCL_EXACT, &optIndex ) != TCL_OK ) {
    	    return TCL_ERROR;
    	}
    	if (iarg + 1 == objc) {
    		resultObjPtr = Tcl_GetObjResult( interp );
    		Tcl_AppendStringsToObj( resultObjPtr, "value for \"",
    				Tcl_GetString(objv[iarg]), "\"missing", (char *) NULL );
    	    return TCL_ERROR;
    	}
        
        /*
         * Dispatch the tracks command to the right branch.
         */
        
        switch(optIndex) {

            case kTracksOptionBalance: {
				if (Tcl_GetLongFromObj( interp, objv[iarg+1], &longValue ) != TCL_OK) {
					Tcl_AddErrorInfo( interp, 
							"\n	(processing -balance option)" );
		    		return TCL_ERROR;
				}
                balance = (short) longValue;
                MediaSetSoundBalance( mh, balance );
                break;
            }

            case kTracksOptionEnabled: {
				if (Tcl_GetBooleanFromObj( interp, objv[iarg+1], &booleanInt ) 
						!= TCL_OK) {
					Tcl_AddErrorInfo( interp, 
							"\n	(processing -enabled option)" );
	    			return TCL_ERROR;
				}
   				SetTrackEnabled( myTrack, (Boolean) (booleanInt ? true : false) );
    			if (aController != NULL) {
       			    MCDoAction( aController,  mcActionMovieEdited, NULL );
    				MCMovieChanged( aController, aMovie );
    			}           
                break;
            }

            case kTracksOptionGraphicsMode: {
                int         argcGMode;
                char        **argvGMode;
                char        *argvOther;
        
                /*
                 * Figure out if we've got a list with one "dithercopy" element.
                 */
                 
                hasGraphicsMode = true;
                graphicsMode = 0;
                Tcl_SplitList( interp, Tcl_GetString( objv[iarg+1] ), 
                		&argcGMode, &argvGMode );
                if (argcGMode > 2) {
					Tcl_SetObjResult( interp, Tcl_NewStringObj( 
							"Max two elements acceptable in -graphicsmode", -1 ) );
		            ckfree( (char *) argvGMode );
		            return TCL_ERROR;
                } else if (argcGMode == 2) {
                    if (!strcmp(argvGMode[0], "dithercopy")) {
                        graphicsMode = ditherCopy;
                        argvOther = argvGMode[1];
                    } else if (!strcmp(argvGMode[1], "dithercopy")) {
                        graphicsMode = ditherCopy;
                        argvOther = argvGMode[0];
                    } else {
						Tcl_SetObjResult( interp, Tcl_NewStringObj( 
								"Unknown value for -graphicsmode", -1 ) );
		                ckfree( (char *) argvGMode );
		                return TCL_ERROR;
                    }
                } else {
                    argvOther = Tcl_GetString( objv[iarg+1] );
                }
                
                if (!strcmp(argvOther, "blend")) {
                    graphicsMode += blend;
                } else if (!strcmp(argvOther, "copy")) {
                    graphicsMode += srcCopy;
                } else if (!strcmp(argvOther, "transparent")) {
                    graphicsMode += transparent;
                } else if (!strcmp(argvOther, "addpin")) {
                    graphicsMode += addPin;
                } else if (!strcmp(argvOther, "addover")) {
                    graphicsMode += addOver;
                } else if (!strcmp(argvOther, "subpin")) {
                    graphicsMode += subPin;
                } else if (!strcmp(argvOther, "addmax")) {
                    graphicsMode += addMax;
                } else if (!strcmp(argvOther, "subover")) {
                    graphicsMode += subOver;
                } else if (!strcmp(argvOther, "addmin")) {
                    graphicsMode += adMin;
                } else if (!strcmp(argvOther, "straighalpha")) {
                    graphicsMode += graphicsModeStraightAlpha;
                } else if (!strcmp(argvOther, "prewhitealpha")) {
                    graphicsMode += graphicsModePreWhiteAlpha;
                } else if (!strcmp(argvOther, "preblackalpha")) {
                    graphicsMode += graphicsModePreBlackAlpha;
                } else if (!strcmp(argvOther, "straighalphablend")) {
                    graphicsMode += graphicsModeStraightAlphaBlend;
                } else {
					Tcl_SetObjResult( interp, Tcl_NewStringObj( 
							"Unrecognized graphics mode", -1 ) );
            		return TCL_ERROR;
                }
                
                ckfree( (char *) argvGMode );
   			    redraw = true;
                break;
            }

            case kTracksOptionGraphicsModeColor: {
                XColor      *xcolPtr;
            
                /* 
                 * Get mac color from string that can either by a X11 color name or a hex value. 
                 */
                 
                xcolPtr = Tk_AllocColorFromObj( interp, movPtr->tkwin, objv[iarg+1] );
	            TkSetMacColor( xcolPtr->pixel, &opColor );
   			    redraw = true;
                break;
            }

            case kTracksOptionLayer: {
				if (Tcl_GetLongFromObj( interp, objv[iarg+1], &longValue ) 
						!= TCL_OK) {
					Tcl_AddErrorInfo( interp, "\n	(processing -layer option)" );
		    		return TCL_ERROR;
				}
           		layerNo = (short) longValue;
                SetTrackLayer( myTrack, layerNo );
                if (noErr != CheckAndSetErrorResult( interp, noErr )) {
       				result = TCL_ERROR;
       			}            
                break;
            }

            case kTracksOptionMatrix: {
                if (TranslateTclListToMatrixRecord( interp, objv[iarg+1], 
                		&matrixRecord ) != TCL_OK) {
       			    result = TCL_ERROR;
       			} else {
                    SetTrackMatrix( myTrack, &matrixRecord );
                    if (noErr != CheckAndSetErrorResult( interp, noErr )) {
   		    		    result = TCL_ERROR;
   			    	}
   			    }
   			    redraw = true;
                break;
            }

            case kTracksOptionOffset: {
				if (Tcl_GetLongFromObj( interp, objv[iarg+1], &longValue ) 
						!= TCL_OK) {
					Tcl_AddErrorInfo( interp, "\n	(processing -offset option)" );
		    		return TCL_ERROR;
				}
           		offset = longValue;
       	        if (offset < 0) {
       	            offset = 0;
       	        }
                SetTrackOffset( myTrack, offset );
                if (noErr != CheckAndSetErrorResult( interp, noErr )) {
       			    result = TCL_ERROR;
       			} else if (aController) {
                    MCMovieChanged( aController, aMovie );
                }
                break;
            }

            case kTracksOptionVolume: {
				if (Tcl_GetLongFromObj( interp, objv[iarg+1], &longValue ) 
						!= TCL_OK) {
					Tcl_AddErrorInfo( interp, "\n	(processing -volume option)" );
		    		return TCL_ERROR;
				}
           		volume = (short) longValue;
                SetTrackVolume( myTrack, volume );
                if (noErr != CheckAndSetErrorResult( interp, noErr )) {
       				result = TCL_ERROR;
       			}            
                break;
            }
        }
    }
    
    if (hasGraphicsMode) {
        MediaSetGraphicsMode( mh, graphicsMode, &opColor );        
    }
    if (redraw) {
        MoviePlayerWorldChanged( (ClientData) movPtr );
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * InvalidateTrackSelectionAndScrap --
 *
 * 		If we have this track in this movie on our fake track clipboard, 
 *		invalidate it. Invalidate the selection as well.
 *
 * Results:
 *		None.
 *
 * Side effects:
 *		Possible invalidates track selection and track scrap.
 *
 *----------------------------------------------------------------------
 */

static void
InvalidateTrackSelectionAndScrap( MoviePlayer *movPtr, long trackID ) {

    /* can be refined...  */
    if ((gTrackScrap.movPtr == movPtr) && (gTrackScrap.trackSelect.trackID == trackID)) {
        gTrackScrap.movPtr = NULL;
    }	        
    if ((movPtr->trackSelect->trackID != 0) && (movPtr->trackSelect->trackID == trackID)) {
        movPtr->trackSelect->trackID = 0;
    }
}

static int
SetupOptionHashTables( Tcl_Interp *interp )
{
	Tcl_HashEntry 		*hPtr;
	CodecQualityEntry	*entryPtr;
	int					isNew;
    int 				result = TCL_OK;

    gCodecQualityHashPtr = (Tcl_HashTable *) ckalloc( sizeof(Tcl_HashTable) );
	Tcl_InitHashTable( gCodecQualityHashPtr, TCL_STRING_KEYS );
	for (entryPtr = &CodecQualityLookupTable[0]; entryPtr->option != NULL; entryPtr++) {
		hPtr = Tcl_CreateHashEntry( gCodecQualityHashPtr, 
				entryPtr->option, &isNew );
		Tcl_SetHashValue( hPtr, entryPtr->quality );
	}	
	return result;
}

void 
TracksCommandFree( void ) 
{
	if (gCodecQualityHashPtr != NULL) {
		Tcl_DeleteHashTable( gCodecQualityHashPtr );
		ckfree( (char *) gCodecQualityHashPtr );
	}
}

/*---------------------------------------------------------------------------*/
