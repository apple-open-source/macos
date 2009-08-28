/*
 * ExportCommand.c --
 *
 *		Process the "export" subcommand.
 *
 * Copyright (c) 2000-2003  Mats Bengtsson
 *
 * $Id: ExportCommand.c,v 1.2 2005/06/02 12:39:29 matben Exp $
 */

#include "MoviePlayer.h"


/*
 * For dispatching export options. Order!!!
 */

static char *allExportOptions[] = {
	"-codecsubtype", "-dialog", "-file",
	"-mediatype", "-onlytrack", "-readsettingsfromfile",
	"-restrictexport", "-savesettingstofile", "-uselatestsettings",
    (char *) NULL
};

enum {
    kExportOptionCodecSubType                   = 0L,
    kExportOptionDialog,
    kExportOptionFile,
    kExportOptionMediaType,
    kExportOptionOnlyTrack,
    kExportOptionReadSettingsFromFile,
    kExportOptionRestrictExport,
    kExportOptionSaveSettingsToFile,
    kExportOptionUseLatestSettings
};

/*
 * Hash table for mapping codec to QTAtomContainer that contains latest settings
 * for an export component.
 */
 
static Tfp_ArrayType    *exportComponentSettingsArr = NULL;
static OSType           exportComponentSettingsSubType = MovieFileType;
static OSType           exportComponentSettingsManufacturer = 0L;


static void     	DeleteExportComponentSettings( ClientData clientData );



/*
 *----------------------------------------------------------------------
 *
 * ProcessExportCmd --
 *
 *		Process the "export" command
 *
 * Results:
 *  	Normal TCL results
 *
 * Side effects:
 *		Exports, with or without dialog.
 *
 *----------------------------------------------------------------------
 */

int
ProcessExportCmd( Tcl_Interp *interp,  
		Movie theMovie, 
		int objc, 
		Tcl_Obj *CONST objv[] ) 
{
    int             iarg;
    int             optIndex;
    int             showDialog = true;
    int             useLatestSettings = false;
    int             haveFile = false;
    int             haveCodecSubType = false;
    int             haveMediaType = false;
    int				booleanInt;
    long            flags = 0L;
    long            trackID;
    char            codecKey[9];
    char            pathname[256];
    char            *charPtr;
    unsigned long   lType;
	Tcl_Obj 		*resultObjPtr;
    Tcl_Obj			*saveSettingsPathPtr = NULL;
    Tcl_Obj			*readSettingsPathPtr = NULL;
	Tcl_DString     ds;
    Boolean         cancelled = false;
    FSSpec          theFSSpec;
    Track           track = NULL;
	Component               exportComp;
    MovieExportComponent    exporter = NULL;
    ComponentDescription    compDesc;
    ComponentDescription    compInfo;
    QTAtomContainer settingsAtomContainer = NULL;
    int             result = TCL_OK;
    OSErr           err = noErr;
    ComponentResult compErr = noErr;
    
    /*
     * Initial settings for the exporter component description.
     * 0 entries mean default values.
     */
     
    compDesc.componentType = MovieExportType;
    compDesc.componentSubType = MovieFileType;
    compDesc.componentManufacturer = 0L;
    compDesc.componentFlags = 0L;
    compDesc.componentFlagsMask = 0L;
    
    flags = createMovieFileDeleteCurFile | movieToFileOnlyExport;

    strcpy( pathname, "0Untitled.mov" );
    BlockMove( pathname, theFSSpec.name, strlen(pathname) );
    theFSSpec.name[0] = strlen(pathname);
        
    for (iarg = 0; iarg < objc; iarg += 2) {
    
    	if (Tcl_GetIndexFromObj( interp, objv[iarg], allExportOptions, 
    	        "export option", TCL_EXACT, &optIndex ) != TCL_OK ) {
    	    result = TCL_ERROR;
    	    goto bail;
    	}
    	if (iarg + 1 == objc) {
    		resultObjPtr = Tcl_GetObjResult( interp );
    		Tcl_AppendStringsToObj( resultObjPtr, "value for \"",
    				Tcl_GetString(objv[iarg]), "\"missing", (char *) NULL );
    	    result = TCL_ERROR;
    	    goto bail;
    	}
        
        /*
         * Dispatch the export option to the right branch.
         */
        
        switch (optIndex) {

            case kExportOptionCodecSubType: {
           	    charPtr = Tcl_UtfToExternalDString( gQTTclTranslationEncoding, 
           	    		Tcl_GetString( objv[iarg+1] ), -1, &ds);
                if (Tcl_DStringLength( &ds ) != 4) {
          			Tcl_SetObjResult( interp, Tcl_NewStringObj( 
          					"-codecsubtype must be four characters", -1 ) );
            	    result = TCL_ERROR;
        	        goto bail;
                }
    			memcpy( &lType, charPtr, 4 );
                compDesc.componentSubType = EndianU32_NtoB( lType );
                haveCodecSubType = true;
                break;
            }

            case kExportOptionDialog: {
				if (Tcl_GetBooleanFromObj( interp, objv[iarg+1], 
						&showDialog ) != TCL_OK) {
					Tcl_AddErrorInfo( interp, 
							"\n	(processing -dialog option)" );
					result = TCL_ERROR;
					goto bail;
				}
                break;
            }

            case kExportOptionFile: {
        		err = QTTclNativePathNameToFSSpec( interp, 
        				Tcl_GetString( objv[iarg+1] ), &theFSSpec );
          		if ((err != fnfErr) && (err != noErr)) {
          			Tcl_SetObjResult( interp, Tcl_NewStringObj( 
          					"failed making FSSpec from filename", -1 ) );
        	        result = TCL_ERROR;
            	    goto bail;
          		} 
                flags |= movieFileSpecValid;
                haveFile = true;
                break;
            }

            case kExportOptionReadSettingsFromFile: {
                readSettingsPathPtr = objv[iarg+1];
                break;
            }

            case kExportOptionMediaType: {
           	    charPtr = Tcl_UtfToExternalDString( gQTTclTranslationEncoding, 
           	    		Tcl_GetString( objv[iarg+1] ), -1, &ds);
                if (Tcl_DStringLength( &ds ) != 4) {
          			Tcl_SetObjResult( interp, Tcl_NewStringObj( 
          					"-mediatype must be four characters", -1 ) );
            	    result = TCL_ERROR;
        	        goto bail;
                }
    			memcpy( &lType, charPtr, 4 );
                compDesc.componentManufacturer = EndianU32_NtoB( lType );
                haveMediaType = true;
           	    Tcl_DStringFree(&ds);		    
                break;
            }

            case kExportOptionOnlyTrack: {
				if (Tcl_GetLongFromObj( interp, objv[iarg+1], &trackID ) != TCL_OK) {
					Tcl_AddErrorInfo( interp, "\n	(processing -onlytrack option)" );
					result = TCL_ERROR;
					goto bail;
				}
        		track = GetMovieTrack( theMovie, trackID );
       			if (track == NULL) {
                    CheckAndSetErrorResult( interp, noErr );
            	    result = TCL_ERROR;
        	        goto bail;
    			}
                break;
            }

            case kExportOptionRestrictExport: {
				if (Tcl_GetBooleanFromObj( interp, objv[iarg+1], &booleanInt ) 
							!= TCL_OK) {
					Tcl_AddErrorInfo( interp, 
							"\n	(processing -restrictexport option)" );
					result = TCL_ERROR;
					goto bail;
				}
                if (booleanInt == 0) {
                    flags &= ~movieToFileOnlyExport;
                }
                break;
            }

            case kExportOptionSaveSettingsToFile: {
                saveSettingsPathPtr = objv[iarg+1];                
                break;
            }

            case kExportOptionUseLatestSettings: {
				if (Tcl_GetBooleanFromObj( interp, objv[iarg+1], &useLatestSettings ) 
							!= TCL_OK) {
					Tcl_AddErrorInfo( interp, 
							"\n	(processing -uselatestsettings option)" );
					result = TCL_ERROR;
					goto bail;
				}
                break;
            }
        }    
    }

    /*
     * Error checking.
     */
     
    if (!showDialog && !haveFile) {
        Tcl_SetObjResult( interp, Tcl_NewStringObj( 
        		"must have either -file or -dialog", -1 ) );
    	result = TCL_ERROR;
    	goto bail;
    }
    if (haveMediaType && !haveCodecSubType) {
        Tcl_SetObjResult( interp, Tcl_NewStringObj( 
        		"must have -codecsubtype together with -mediatype", -1 ) );
       	result = TCL_ERROR;
       	goto bail;
    }
    if (!haveFile && (haveCodecSubType || haveMediaType)) {   
        Tcl_SetObjResult( interp, Tcl_NewStringObj( 
        		"must have -file option here", -1 ) );
       	result = TCL_ERROR;
       	goto bail;
    } 
    if (readSettingsPathPtr && useLatestSettings) {
        Tcl_SetObjResult( interp, Tcl_NewStringObj( 
        		"can't combine -uselatestsettings and -readsettingsfromfile", -1 ) );
       	result = TCL_ERROR;
       	goto bail;
    }
    if (readSettingsPathPtr && !( haveCodecSubType && haveMediaType )) {
        Tcl_SetObjResult( interp, Tcl_NewStringObj( 
        		"must specify -codecsubtype and -mediatype to use -readsettingsfromfile", -1 ) );
       	result = TCL_ERROR;
       	goto bail;
    }
    if (saveSettingsPathPtr && !( haveCodecSubType && haveMediaType )) {
        Tcl_SetObjResult( interp, Tcl_NewStringObj( 
        		"must specify -codecsubtype and -mediatype to use -savesettingstofile", -1 ) );
       	result = TCL_ERROR;
       	goto bail;
    }
    
    if (useLatestSettings) {
        compDesc.componentSubType = exportComponentSettingsSubType;
        compDesc.componentManufacturer = exportComponentSettingsManufacturer;
    }
    if (exportComponentSettingsArr == NULL) {
        exportComponentSettingsArr = Tfp_ArrayInit( DeleteExportComponentSettings );
    }
    
    if (haveCodecSubType || haveMediaType) {
        exportComp = FindNextComponent( NULL, &compDesc );
        exporter = OpenComponent( exportComp );
        if (exporter == NULL) {
            Tcl_SetObjResult( interp, Tcl_NewStringObj( 
            		"didn't find an export component", -1 ) );
    	    result = TCL_ERROR;
    	    goto bail;
        }
       
       /*
        * Form codecKey which is the entry to the hash table mapping to any
        * previously stored settings for this component.
        * The codecKey is a string of length 8: SubType + Manufacturer ("MooVappl").
        */

        if (useLatestSettings) {
			err = GetComponentInfo( exportComp, &compInfo, NULL, NULL, NULL );			
			if (err == noErr) {            
                memset( codecKey, 0, 9 );
                memcpy( codecKey, &compInfo.componentSubType, 4 );
                memcpy( codecKey + 4, &compInfo.componentManufacturer, 4 );

                if (Tfp_ArrayGet( exportComponentSettingsArr, codecKey, 
                        (ClientData *) &settingsAtomContainer )) {
                    compErr = MovieExportSetSettingsFromAtomContainer( exporter, 
                            settingsAtomContainer );       
                    if (compErr != noErr) {
                        CheckAndSetErrorResult( interp, compErr );
            	        result = TCL_ERROR;
            	        goto bail;
                    }
                }    
            }
        } else if (readSettingsPathPtr) {
        	Tcl_Channel     readChannel = NULL;
        	Tcl_Obj         *readObj = Tcl_NewObj();
        	int             nread;
        	int             len;
            unsigned char   *contentPtr;
        
            readChannel = Tcl_FSOpenFileChannel( interp, readSettingsPathPtr, "r", 0666 );
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

            /* Copy the file content to a handle which is our AtomContainer. */
            
            err = PtrToHand( contentPtr, &settingsAtomContainer, len );
            if (err != noErr) {
                CheckAndSetErrorResult( interp, err );
      	        result = TCL_ERROR;
  	            goto bail;
            }
            compErr = MovieExportSetSettingsFromAtomContainer( exporter, 
            		settingsAtomContainer );       
            Tcl_Close( interp, readChannel );
            Tcl_DecrRefCount( readObj );
            if (compErr != noErr) {
                CheckAndSetErrorResult( interp, compErr );
                result = TCL_ERROR;
                goto bail;
            }
        }
    }
    if (showDialog) {
        if (haveCodecSubType || haveMediaType) {

            /* 
             * Check first that the export component has a dialog. Error?
             */
             
            if (ComponentFunctionImplemented( exporter, 
                    kMovieExportDoUserDialogSelect ) == false) {
                Tcl_SetObjResult( interp, Tcl_NewStringObj( 
                		"the selected export component has no dialog", -1 ) );
            	result = TCL_ERROR;
        	    goto bail;
            }
            compErr = MovieExportDoUserDialog( exporter, theMovie, NULL, 0, 
                    GetMovieDuration(theMovie), &cancelled );
            if (compErr != noErr) {
                CheckAndSetErrorResult( interp, compErr );
        	    result = TCL_ERROR;
        	    goto bail;
            }
            if (cancelled) {
            
                /* Shall return an empty string. */
        	    goto bail;
            }
             
			err = GetComponentInfo( exportComp, &compInfo, NULL, NULL, NULL );			
			if (err == noErr) {            
			
			    /*
			     * Store the just selected settings for this specific component
			     * for future use.
			     */
			     
                memset( codecKey, 0, 9 );
                memcpy( codecKey, &compInfo.componentSubType, 4 );
                memcpy( codecKey + 4, &compInfo.componentManufacturer, 4 );

                compErr = MovieExportGetSettingsAsAtomContainer( exporter, &settingsAtomContainer );
                if (compErr != noErr) {
                    CheckAndSetErrorResult( interp, compErr );
            	    result = TCL_ERROR;
            	    goto bail;
                }
                Tfp_ArraySet( exportComponentSettingsArr, codecKey, 
                        (ClientData) settingsAtomContainer );  
                        
                if (saveSettingsPathPtr) {
                    Tcl_Channel     saveChannel = NULL;
                    int             size;
                    int             written;
                
                    saveChannel = Tcl_FSOpenFileChannel( interp, saveSettingsPathPtr, "w", 0666 );
                    if (saveChannel == NULL) {
                	    result = TCL_ERROR;
                	    goto bail;
                    }
                    result = Tcl_SetChannelOption( interp, saveChannel, 
                            "-translation", "binary" );
                    if (result != TCL_OK) {
                	    goto bail;
                    }
                    size = GetHandleSize( settingsAtomContainer );
                    if (size == 0) {
                        Tcl_SetObjResult( interp, Tcl_NewStringObj( 
                        		"GetHandleSize failed", -1 ) );
                	    result = TCL_ERROR;
            	        goto bail;
                    }
                    HLock( settingsAtomContainer );
                    written = Tcl_Write( saveChannel, *settingsAtomContainer, size );
                    if (written == -1) {
                        Tcl_SetObjResult( interp, Tcl_NewStringObj( 
                        		Tcl_ErrnoMsg( Tcl_GetErrno() ), -1 ) );
                	    result = TCL_ERROR;
            	        goto bail;
                    }
                    Tcl_Close( interp, saveChannel );
                    HUnlock( settingsAtomContainer );
                }
            }          
        } else {
            flags |= showUserSettingsDialog;
        }
    }
                
    /* Export the movie into a file. */
    
    err = ConvertMovieToFile(
            theMovie,     	/* The movie to convert. */
            track,        	/* NULL is all tracks in the movie. */
            &theFSSpec,    	/* The output file. */
            0L,         	/* The output file type. */
            ksigMoviePlayer,/* The output file creator. */
            smSystemScript,	/* The script. */
            NULL,         	/* No resource ID to be returned. */
            flags,        	/* Export flags. */
            exporter );    	/* Specific export component. NULL means all. */
	if (err == userCanceledErr) {
        goto bail;
	}
    if (noErr != CheckAndSetErrorResult( interp, err )) {
        result = TCL_ERROR;
        goto bail;
    }            
    
    /*
     * When 'ConvertMovieToFile' exits the FSSpec contains the picked file.
     */
     
    result = QTTclFSSpecToNativePathName( interp, pathname, &theFSSpec );

bail:
    if (exporter != NULL) {
        CloseComponent( exporter );
    }
    return result;
}


static void
DeleteExportComponentSettings( ClientData clientData )
{
    Handle      atomContainer = (QTAtomContainer) clientData;
    
    if (atomContainer != NULL) {
        QTDisposeAtomContainer( atomContainer );
    }
}

void
ExportComponentSettingsFree( void )
{
    if (exportComponentSettingsArr != NULL) {
        Tfp_ArrayDestroy( exportComponentSettingsArr );
    }
}

/*---------------------------------------------------------------------------*/
