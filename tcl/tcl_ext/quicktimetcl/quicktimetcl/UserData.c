/*
 * UserData.c --
 *
 *		Implements the 'userdata' command.
 *		It is part of the QuickTimeTcl package which provides Tcl/Tk bindings for QuickTime.
 *
 * Copyright (c) 2003  Mats Bengtsson
 *
 * $Id: UserData.c,v 1.2 2003/11/28 13:38:42 matben Exp $
 */

#include "MoviePlayer.h"

const UInt8 			kUserDataIsText = 0xA9; // the copyright symbol

/*
 * Use hash table to map between human readable option names and long types
 * for user data.
 */
 
static Tcl_HashTable 	*gOptionToUserDataHashPtr = NULL;
static Tcl_HashTable 	*gUserDataToOptionHashPtr = NULL;

typedef struct UserDataLookupEntry {
    char       	*option;
  	OSType 		udType;		/* long */
} UserDataLookupEntry;

UserDataLookupEntry gLookupTable[] = {
	{"-album",				kUserDataTextAlbum},
	{"-artist",				kUserDataTextArtist},
	{"-author",				kUserDataTextAuthor},
	{"-chapter",			kUserDataTextChapter},
	{"-comment",			kUserDataTextComment},
	{"-composer",			kUserDataTextComposer},
	{"-copyright",			kUserDataTextCopyright},
	{"-creationdate",		kUserDataTextCreationDate},
	{"-description",		kUserDataTextDescription},
	{"-director",			kUserDataTextDirector},
	{"-disclaimer",			kUserDataTextDisclaimer},
	{"-encodedby",			kUserDataTextEncodedBy},
	{"-fullname",			kUserDataTextFullName},
	{"-genre",				kUserDataTextGenre},
	{"-hostcomputer",		kUserDataTextHostComputer},
	{"-information",		kUserDataTextInformation},
	{"-keywords",			kUserDataTextKeywords},
	{"-make",				kUserDataTextMake},
	{"-model",				kUserDataTextModel},
	{"-name",				kUserDataName},
	{"-originalartist",		kUserDataTextOriginalArtist},
	{"-originalformat",		kUserDataTextOriginalFormat},
	{"-originalsource",		kUserDataTextOriginalSource},
	{"-performers",			kUserDataTextPerformers},
	{"-producer",			kUserDataTextProducer},
	{"-product",			kUserDataTextProduct},
	{"-software",			kUserDataTextSoftware},
	{"-specialplaybackrequirements",	kUserDataTextSpecialPlaybackRequirements},
	{"-track",				kUserDataTextTrack},
	{"-warning",			kUserDataTextWarning},
	{"-writer",				kUserDataTextWriter},
	{"-urllink",			kUserDataTextURLLink},
	{"-editdate1",			kUserDataTextEditDate1},
	{NULL, 0}
};

static int			SetupUserDataOptionHashTables( Tcl_Interp *interp );


/*
 *----------------------------------------------------------------------
 *
 * GetUserDataListCmd --
 *
 *		Process the "userdata" subcommand.
 *
 * Results:
 *  	Normal TCL results
 *
 * Side effects:
 *		None.
 *
 *----------------------------------------------------------------------
 */

int
GetUserDataListCmd( UserData userData, 
		Tcl_Interp *interp ) 
{	
	Tcl_Obj         *listObjPtr;
  	OSType 			udType;
  	short 			count, i;
  	char 			nul = 0;
  	char			str8[8];
  	char			*charPtr;
	unsigned long	lType;
  	Handle 			hData = NULL;
	Tcl_HashEntry   *hashPtr = NULL;
  	Ptr 			p;
	Tcl_DString     ds;
  	OSErr 			err = noErr;
    int 			result = TCL_OK;

  	if (gOptionToUserDataHashPtr == NULL) {
  		if (TCL_OK != SetupUserDataOptionHashTables( interp )) {
  			result = TCL_ERROR;
  			goto bail;
  		}
  	}
	listObjPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );
  	hData = NewHandle(0);
  	udType = GetNextUserDataType( userData, 0 );
  	
  	if(0 != udType) {
    	do {
      		count = CountUserDataType( userData, udType );
      		for(i = 1; i <= count; i++) {
	        	if(((udType>>24) == kUserDataIsText) || (udType == kUserDataName)) {

	          		/* 
	          		 * If the first letter of udType is 0xA9 
	          		 * (the copyright symbol), then use GetUserDataText instead
	          		 * of GetUserData. See Movies.h for a list of interesting 
	          		 * user data types.
	          		 */
	          		 
	          		err = GetUserDataText( userData, hData, udType, i, langEnglish );
	          		if (err != noErr) {
				 		CheckAndSetErrorResult( interp, err );
	          			goto bail;
	          		}

	          		/* null-terminate the string in the handle */
	          		PtrAndHand( &nul, hData, 1 );

	          		/* turn any CRs into spaces */
	          		p = *hData;
	          		while (*p) {
	            		if (*p == kReturnCharCode) {
	            			*p = ' ';
	            		}
	            		p++;
	          		};
	          		
	          		/* 
	          		 * Find any option string in hash table for this ud type. 
	          		 */

				    hashPtr = Tcl_FindHashEntry( gUserDataToOptionHashPtr, 
				    		(char *) udType );
				    if (hashPtr != NULL) {
				        charPtr = (char *) Tcl_GetHashValue( hashPtr );
						Tcl_ListObjAppendElement( interp, listObjPtr, 
								Tcl_NewStringObj(charPtr, -1) );
				    } else {
						lType = EndianU32_BtoN( udType );
						memset( str8, 0, 8 );
						strcat( str8, "-" );
						memcpy( str8 + 1, &lType, 4 );
						Tcl_ListObjAppendElement( interp, listObjPtr, 
								Tcl_NewStringObj(str8, -1) );
				    }
	          		
	         		HLock( hData );
                    charPtr = Tcl_ExternalToUtfDString( gQTTclTranslationEncoding, *hData, -1, &ds );
    				Tcl_ListObjAppendElement( interp, listObjPtr, 
        					Tcl_NewStringObj(charPtr, Tcl_DStringLength(&ds)) );
	          		HUnlock( hData );
	        	} else {
						
					/* Non text user data. */
	          		err = GetUserData( userData, hData, udType, i );
	          		if (err != noErr) {
				 		CheckAndSetErrorResult( interp, err );
				 		result = TCL_ERROR;
	          			goto bail;
	          		}

					lType = EndianU32_BtoN( udType );
					memset( str8, 0, 8 );
					strcat( str8, "-" );
					memcpy( str8 + 1, &lType, 4 );
					Tcl_ListObjAppendElement( interp, listObjPtr, 
							Tcl_NewStringObj(str8, -1) );
	         		HLock( hData );
    				Tcl_ListObjAppendElement( interp, listObjPtr, 
        					Tcl_NewStringObj( *hData, GetHandleSize(hData) ) );
	          		HUnlock( hData );
	        	}
	   		}
	      	udType = GetNextUserDataType( userData, udType );

 		} while (0 != udType);
  	}

    Tcl_SetObjResult( interp, listObjPtr );

bail:
	if (hData != NULL) {
		DisposeHandle( hData );
	}
	return result;
}

/*
 *----------------------------------------------------------------------
 *
 * SetUserDataListCmd --
 *
 *		Process the "userdata" subcommand.
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
SetUserDataListCmd( UserData userData, 
		Tcl_Interp *interp, 
		int objc, 
		Tcl_Obj *CONST objv[] ) 
{	
	int				iarg;
  	OSType 			udType;
  	Handle 			hData = NULL;
  	char			*charPtr;
  	int				isTextUserData;
  	int				len;
	unsigned long	lType;
	Tcl_DString     ds;
	Tcl_HashEntry   *hashPtr = NULL;
  	OSErr 			err = noErr;
    int 			result = TCL_OK;

  	if (gOptionToUserDataHashPtr == NULL) {
  		if (SetupUserDataOptionHashTables( interp ) != TCL_OK) {
  			result = TCL_ERROR;
  			goto bail;
  		}
  	}

    for (iarg = 0; iarg < objc; iarg += 2) {
	    hashPtr = Tcl_FindHashEntry( gOptionToUserDataHashPtr, 
	    		Tcl_GetString( objv[iarg] ) );
	    if (hashPtr != NULL) {
			udType = (OSType) Tcl_GetHashValue( hashPtr );
			isTextUserData = true;
		} else {
				
			/* 
			 * The argument did not match any of the predefined options.
			 * UTF translation first!
			 */
			
	        charPtr = Tcl_UtfToExternalDString( gQTTclTranslationEncoding, 
	        		Tcl_GetString( objv[iarg] ), -1, &ds );
        	if ((charPtr[0] != '-') || (Tcl_DStringLength(&ds) != 5)) {
        		Tcl_SetObjResult( interp, Tcl_NewStringObj(
        				"userdata type option must be -FOUR if not predefined types", -1 ) );
	  			result = TCL_ERROR;
	  			goto bail;
			}
			memcpy( &lType, charPtr+1, 4 );
			udType = EndianU32_NtoB( lType );
	        if((udType>>24) == kUserDataIsText) {
				isTextUserData = true;			
			} else {
				isTextUserData = false;
			}
        }
        
        /* Get rid of any old before setting the new one. */
        if (isTextUserData) {
        	while (RemoveUserDataText( userData, udType, 1, langEnglish ) == noErr)
            	;
	        charPtr = Tcl_UtfToExternalDString( gQTTclTranslationEncoding, 
	        		Tcl_GetString( objv[iarg+1] ), -1, &ds );
	        len = Tcl_DStringLength(&ds);
	        PtrToHand( charPtr, &hData, len + 1 );
		    HLock( hData );
	        err = AddUserDataText( userData, hData, udType, 1, langEnglish );
		 	HUnlock( hData );
		 	Tcl_DStringFree( &ds );
        } else {
        	while (RemoveUserData( userData, udType, 1 ) == noErr)
            	;
            charPtr = Tcl_GetStringFromObj( objv[iarg+1], &len );
       		err = SetUserDataItem( userData, charPtr, len, udType, 1 ); 
        }
   	 	if (err != noErr) {
	 		CheckAndSetErrorResult( interp, err );
			result = TCL_ERROR;
	  		goto bail;
		}
	}

bail:

	return result;
}

/*
 *----------------------------------------------------------------------
 *
 * SetupUserDataOptionHashTables --
 *
 *		Sets up two hash tables from User Data types to option strings.
 *
 * Results:
 *  	Normal TCL results
 *
 * Side effects:
 *		hash tables made.
 *
 *----------------------------------------------------------------------
 */

static int
SetupUserDataOptionHashTables( Tcl_Interp *interp )
{
	Tcl_HashEntry 		*hPtr;
	UserDataLookupEntry	*entryPtr;
	int					isNew;
    int 				result = TCL_OK;
    
    /*
     * Map from option string to User Data Type (long).
     */
	
    gOptionToUserDataHashPtr = (Tcl_HashTable *) ckalloc( sizeof(Tcl_HashTable) );
	Tcl_InitHashTable( gOptionToUserDataHashPtr, TCL_STRING_KEYS );
	for (entryPtr = &gLookupTable[0]; entryPtr->option != NULL; entryPtr++) {
		hPtr = Tcl_CreateHashEntry( gOptionToUserDataHashPtr, 
				entryPtr->option, &isNew );
		Tcl_SetHashValue( hPtr, entryPtr->udType );
	}	
	
    /*
     * Map from User Data Type (long) to option string.
     */

    gUserDataToOptionHashPtr = (Tcl_HashTable *) ckalloc( sizeof(Tcl_HashTable) );
	Tcl_InitHashTable( gUserDataToOptionHashPtr, TCL_ONE_WORD_KEYS );
	for (entryPtr = &gLookupTable[0]; entryPtr->option != NULL; entryPtr++) {
		hPtr = Tcl_CreateHashEntry( gUserDataToOptionHashPtr, 
				(char *) entryPtr->udType, &isNew );
		Tcl_SetHashValue( hPtr, entryPtr->option );
	}	
	return result;
}

void 
UserDataHashTablesFree( void )
{
	if (gOptionToUserDataHashPtr != NULL) {
		Tcl_DeleteHashTable( gOptionToUserDataHashPtr );
		ckfree( (char *) gOptionToUserDataHashPtr );
	}
	if (gUserDataToOptionHashPtr != NULL) {
		Tcl_DeleteHashTable( gUserDataToOptionHashPtr );
		ckfree( (char *) gUserDataToOptionHashPtr );
	}
}

/*---------------------------------------------------------------------------*/