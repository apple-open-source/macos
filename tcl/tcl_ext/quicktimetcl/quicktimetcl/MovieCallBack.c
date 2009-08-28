/*
 * MovieCallBack.c --
 *
 *		Process the "callback" subcommand.
 *
 * Copyright (c) 2003  Mats Bengtsson
 *
 * $Id: MovieCallBack.c,v 1.3 2003/12/02 10:17:03 matben Exp $
 */

#include "MoviePlayer.h"


/*
 * For dispatching 'callback' sub commands.
 */

static char *allCallBackCmds[] = {
	"cancel", "info",
    (char *) NULL
};

enum {
    kCallBackCmdCancel              	= 0L,
	kCallBackCmdInfo
};

/*
 * Use hash table to map between token names and the corresponding
 * callback struct entry.
 */
 
typedef struct CallBackEntry {
	MoviePlayer 	*movPtr;
	TimeValue		movTime;
	long			uid;
	Tcl_Obj			*cmdObjPtr;
	QTCallBack		callback;
	Tcl_HashEntry	*hashPtr;		/* Self referencing; useful when freeing */
} CallBackEntry;

static char		gTokenCallbackStr[] = "moviecallback";
static long		gTokenUID = 0;

pascal void		CallBackProc( QTCallBack cb, long refCon );

/*
 *----------------------------------------------------------------------
 *
 * ProcessCallBackCmd --
 *
 *		Process the "callback" command
 *
 * Results:
 *  	Normal TCL results
 *
 * Side effects:
 *		Sets up or cancels callbacks, or just returns info.
 *
 *----------------------------------------------------------------------
 */

int
ProcessCallBackCmd( 
        MoviePlayer *movPtr,  
		int objc, 				/* Starts with the thing after 'callback' */
		Tcl_Obj *CONST objv[] ) 
{
	Tcl_Interp 		*interp = movPtr->interp;
	Movie 			theMovie = movPtr->aMovie;
	int				cmdIndex;
	int				isNew;
	long			movTime;
	Tcl_HashTable 	*callBackHashTablePtr = movPtr->callBackHashTablePtr;
	Tcl_HashSearch 	search;
	Tcl_HashEntry   *hashPtr = NULL;
	Tcl_Obj			*objPtr;
	Tcl_Obj			*listObjPtr;
	CallBackEntry	*entryPtr;
	OSErr			err = noErr;
	int				result = TCL_OK;

  	if (callBackHashTablePtr == NULL) {
	    movPtr->callBackHashTablePtr = (Tcl_HashTable *) 
	    		ckalloc( sizeof(Tcl_HashTable) );
	    callBackHashTablePtr = movPtr->callBackHashTablePtr;
		Tcl_InitHashTable( callBackHashTablePtr, TCL_STRING_KEYS );
  	}
	if (objc < 1) {
		Tcl_WrongNumArgs( interp, 0, objv, 
				"pathName callback time|cmd ?args?" );
		return TCL_ERROR;
	}
	
	/*
	 * Either a command (cancel or info) or an integer number.
	 */

	if (Tcl_GetIndexFromObj( interp, objv[0], allCallBackCmds, "callback command",
			TCL_EXACT, &cmdIndex ) != TCL_OK ) {
		if (Tcl_GetLongFromObj( interp, objv[0], &movTime ) == TCL_OK) {
			cmdIndex = -1;
		} else {
    		Tcl_SetObjResult( interp, Tcl_NewStringObj( 
    				"Usage: \"pathName callback time|cmd ?args?\"", -1 ) );
		    return TCL_ERROR;
		}
	}
    
    /*
     * Dispatch the callback command to the right branch.
     */
    
    switch (cmdIndex) {

        case -1: {
        	char	tmpstr[32];
        	
        	if (objc != 2) {
				Tcl_WrongNumArgs( interp, 0, objv, 
						"pathName callback time procName" );
				return TCL_ERROR;
			}
			if (Tcl_GetLongFromObj( interp, objv[0], &movTime ) != TCL_OK) {
				Tcl_AddErrorInfo( interp, "\n	(processing time value)" );
	    		return TCL_ERROR;
			}
        	entryPtr = (CallBackEntry *) ckalloc( sizeof(CallBackEntry) );
        	entryPtr->callback = NewCallBack( GetMovieTimeBase( theMovie ), 
        			callBackAtTime );
        	entryPtr->uid = gTokenUID;
        	entryPtr->movPtr = movPtr;
        	entryPtr->movTime = movTime;
        	err = CallMeWhen( entryPtr->callback, 
        			NewQTCallBackUPP( CallBackProc), (long) entryPtr,
        			triggerTimeEither, movTime, GetMovieTimeScale( theMovie ) );
        	
         	entryPtr->cmdObjPtr = objv[1];
            Tcl_IncrRefCount( objv[1] );
        	sprintf( tmpstr, "%s%ld", gTokenCallbackStr, gTokenUID );
			gTokenUID++;
			hashPtr = Tcl_CreateHashEntry( callBackHashTablePtr, 
					tmpstr, &isNew );
			Tcl_SetHashValue( hashPtr, entryPtr );
			entryPtr->hashPtr = hashPtr;
			Tcl_SetObjResult( interp, Tcl_NewStringObj( tmpstr, -1 ) );
    	    break;
       	}

        case kCallBackCmdCancel: {
       		hashPtr = Tcl_FindHashEntry( callBackHashTablePtr,
					Tcl_GetString( objv[1] ) );
		    if (hashPtr == NULL) {
				Tcl_SetObjResult( interp, Tcl_NewStringObj( 
						"Unrecognized callback token", -1 ) );
				return TCL_ERROR;
		    }
		    entryPtr = (CallBackEntry *) Tcl_GetHashValue( hashPtr );
		    CancelCallBack( entryPtr->callback );
		    DisposeCallBack( entryPtr->callback );
		    Tcl_DeleteHashEntry( hashPtr );
		    ckfree( (char *) entryPtr );
    	    break;
       	}

        case kCallBackCmdInfo: {
        	if (callBackHashTablePtr == NULL) {
				return TCL_OK;
			}
			if (objc == 1) {
				
				/*
				 * Return a list of all tokens.
				 */
				 
		    	listObjPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );
				hashPtr = Tcl_FirstHashEntry( callBackHashTablePtr, &search );
				while (hashPtr != NULL) {
					entryPtr = (CallBackEntry *) Tcl_GetHashValue( hashPtr );
				    objPtr = Tcl_NewStringObj( gTokenCallbackStr, -1 );
				    Tcl_AppendObjToObj( objPtr, Tcl_NewIntObj( entryPtr->uid ) );
			    	Tcl_ListObjAppendElement( interp, listObjPtr, objPtr );
					hashPtr = Tcl_NextHashEntry( &search );
				}
				Tcl_SetObjResult( interp, listObjPtr );
			
			} else if (objc == 2) {
			
				/*
				 * Return detailed info on this particular callback token.
				 */
				 
	       		hashPtr = Tcl_FindHashEntry( callBackHashTablePtr,
						Tcl_GetString( objv[1] ) );
			    if (hashPtr == NULL) {
					Tcl_SetObjResult( interp, Tcl_NewStringObj( 
							"Unrecognized callback token", -1 ) );
					return TCL_ERROR;
			    }
			    entryPtr = (CallBackEntry *) Tcl_GetHashValue( hashPtr );
		    	listObjPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );
		    	Tcl_ListObjAppendElement( interp, listObjPtr, entryPtr->cmdObjPtr );
		    	Tcl_ListObjAppendElement( interp, listObjPtr, 
		    			Tcl_NewLongObj( entryPtr->movTime ) );
				Tcl_SetObjResult( interp, listObjPtr );
			
			} else {
				Tcl_WrongNumArgs( interp, 0, objv, 
						"pathName callback info ?callbacktoken?" );
				return TCL_ERROR;
			}
    	    break;
       	}
	}

	return result;
}

/*
 *----------------------------------------------------------------------
 *
 * CallBackProc --
 *
 *		Called from QuickTime when time event to invoke the registered
 *		Tcl procedure.
 *
 * Results:
 *  	None.
 *
 * Side effects:
 *		Invokes Tcl procedure.
 *
 *----------------------------------------------------------------------
 */

pascal void
CallBackProc( QTCallBack cb, long refCon )
{
	CallBackEntry 	*entryPtr = (CallBackEntry *) refCon;
	Tcl_Interp 		*interp = entryPtr->movPtr->interp;
	int				i;
	int				result = TCL_OK;
    Tcl_Obj			*objv[4];

    objv[0] = entryPtr->cmdObjPtr;
    objv[1] = Tcl_NewStringObj( gTokenCallbackStr, -1 );
    Tcl_AppendObjToObj( objv[1], Tcl_NewIntObj( entryPtr->uid ) );
    objv[2] = Tcl_NewStringObj( Tk_PathName(entryPtr->movPtr->tkwin), -1 );
    objv[3] = Tcl_NewLongObj( entryPtr->movTime );
    for (i = 0; i < 4; i++) {
        Tcl_IncrRefCount( objv[i] );
    }
    Tcl_Preserve( (ClientData) entryPtr );
    result = Tcl_EvalObjv( interp, 4, objv, 0 );
    Tcl_Release( (ClientData) entryPtr );
    for (i = 0; i < 4; i++) {
        Tcl_DecrRefCount( objv[i] );
    }
	Tcl_DecrRefCount( entryPtr->cmdObjPtr );
	
	/*
	 * One shot callback; remove from our hash table since used.
	 */
	 
    if (entryPtr->hashPtr != NULL) {
		Tcl_DeleteHashEntry( entryPtr->hashPtr );
    }
	ckfree( (char *) entryPtr );
}

/*
 *----------------------------------------------------------------------
 *
 * CallBackFree --
 *
 *		Called when destroying movie to free up hash table for this specific
 *		instance. Assume the QT callbacks are destroyed with the movie itself.
 *
 * Results:
 *  	None.
 *
 * Side effects:
 *		Frees memory.
 *
 *----------------------------------------------------------------------
 */

void		
CallBackFree( MoviePlayer *movPtr )
{
	Tcl_HashTable 		*callBackHashTablePtr = movPtr->callBackHashTablePtr;
	Tcl_HashEntry 		*hPtr;
	Tcl_HashSearch 		search;
	CallBackEntry 		*entryPtr;

	if (callBackHashTablePtr != NULL) {
		hPtr = Tcl_FirstHashEntry( callBackHashTablePtr, &search );
		while (hPtr != NULL) {
			entryPtr = (CallBackEntry *) Tcl_GetHashValue( hPtr );
			Tcl_DecrRefCount( entryPtr->cmdObjPtr );
			Tcl_DeleteHashEntry( hPtr );
        	ckfree( (char *) entryPtr );
			hPtr = Tcl_NextHashEntry( &search );
		}
	}
}

/*---------------------------------------------------------------------------*/
