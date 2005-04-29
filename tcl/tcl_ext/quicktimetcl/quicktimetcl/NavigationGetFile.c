/*
 * NavigationGetFile.c --
 *
 *		This file implements the Navigation dialog for opening movies with
 *		preview. It is part of the QuickTimeTcl package which provides
 *		Tcl/Tk bindings for QuickTime.
 *
 * Copyright (c) 2000-2003  Mats Bengtsson
 *
 * $Id: NavigationGetFile.c,v 1.2 2004/05/28 14:47:46 matben Exp $
 */

#include "QuickTimeTcl.h"

#if !TARGET_API_MAC_CARBON
    #include <Navigation.h>
#endif

typedef const OSTypePtr TypeListPtr;

OSErr               GetOneFileWithPreview( AEDesc *defaultLocation, short numTypes, TypeListPtr typeListPtr, 
                            StringPtr title, FSSpecPtr theFSSpecPtr, void *filterProc );
static Handle       CreateOpenHandle( OSType theApplicationSignature, short numTypes, 
                            TypeListPtr typeListPtr );
static pascal void  HandleNavEvent( NavEventCallbackMessage callbackSelector, 
                            NavCBRecPtr theCallBackParms, void *callbackUD );
static pascal void  OpenEventProc( NavEventCallbackMessage callBackSelector,
                            NavCBRecPtr callBackParams, NavCallBackUserData callBackUD );


/*
 *-----------------------------------------------------------------------------
 *
 * GetOneFileWithPreview --
 *
 * Display the appropriate file-opening dialog box, with an optional QuickTime 
 * preview pane. If the user selects a file, return information about it using 
 * the theFSSpecPtr parameter.
 *
 * Note that both StandardGetFilePreview and NavGetFile use the function 
 * specified by filterProc as a file filter. This framework always passes NULL 
 * in the filterProc parameter. If you use this function in your own code, 
 * keep in mind that on Windows the function specifier must be of type FileFilterUPP 
 * and on Macintosh it must be of type NavObjectFilterUPP. 
 * (You can use the QTFrame_GetFileFilterUPP to create a function specifier of 
 * the appropriate type.) 
 * Also keep in mind that Navigation Services expects a file filter function to
 * return true if a file is to be displayed, while the Standard File Package 
 * expects the filter to return false if a file is to be displayed.
 *
 * Results:
 *	OSErr
 *
 * Side effects:
 *	Dialog displayed
 *
 *-----------------------------------------------------------------------------
 */

OSErr 
GetOneFileWithPreview( 
        AEDesc          *defaultLocation,
        short           numTypes, 
        TypeListPtr     typeListPtr, 
        StringPtr 		title,
        FSSpecPtr       theFSSpecPtr, 
        void            *filterProc )
{
	NavReplyRecord		    reply;
	NavDialogOptions	    dialogOptions;
	NavTypeListHandle	    openList = NULL;
	NavEventUPP			    eventUPP = NewNavEventUPP( HandleNavEvent );
    TextEncoding 			encoding;
	OSErr				    err = noErr;
	
	if (theFSSpecPtr == NULL) {
		return( paramErr );
    }
    encoding = GetApplicationTextEncoding();
    
	/* Specify the options for the dialog box. */
	
	NavGetDefaultDialogOptions( &dialogOptions );
	dialogOptions.dialogOptionFlags -= kNavNoTypePopup;
	dialogOptions.dialogOptionFlags -= kNavAllowMultipleFiles;
	dialogOptions.dialogOptionFlags |= kNavDontAddTranslateItems;
    if (title != NULL) {
        BlockMoveData( title, dialogOptions.clientName, title[0] + 1 );
    }
	
	openList = (NavTypeListHandle) CreateOpenHandle( kNavGenericSignature, 
	        numTypes, typeListPtr );
	if (openList != NULL) {
		HLock( (Handle) openList );
	}
	
	/* Prompt the user for a file. */
	
	err = NavGetFile( defaultLocation, &reply, &dialogOptions, eventUPP, NULL, 
	        (NavObjectFilterUPP) filterProc, openList, NULL );
	if ((err == noErr) && reply.validRecord) {
		AEKeyword		keyword;
		DescType		actualType;
		Size			actualSize = 0;
		
		/* Get the FSSpec for the selected file. */
		
		if (theFSSpecPtr != NULL) {
			err = AEGetNthPtr( &(reply.selection), 1, typeFSS, &keyword, &actualType, 
			        theFSSpecPtr, sizeof(FSSpec), &actualSize );
        }
		NavDisposeReply( &reply );
	} else if (err == noErr) {
	    err = userCanceledErr;
	}
	if (openList != NULL) {
		HUnlock( (Handle) openList );
		DisposeHandle( (Handle) openList );
	}
	
	DisposeNavEventUPP( eventUPP );
 
	return( err );
}

/*
 *-----------------------------------------------------------------------------
 *
 * CreateOpenHandle --
 *
 *	Get the 'open' resource or dynamically create a NavTypeListHandle.
 *
 * Results:
 *	Handle
 *
 * Side effects:
 *	Memory allocated
 *
 *-----------------------------------------------------------------------------
 */

Handle 
CreateOpenHandle( OSType theApplicationSignature, short numTypes, TypeListPtr typeListPtr )
{
	Handle			handle = NULL;
		
	if (typeListPtr == NULL) {
		return handle;
	}
	if (numTypes > 0) {
		handle = NewHandle( sizeof(NavTypeList) + (numTypes * sizeof(OSType)) );
		if (handle != NULL) {
			NavTypeListHandle 	typeListHand = (NavTypeListHandle) handle;
			
			(*typeListHand)->componentSignature = theApplicationSignature;
			(*typeListHand)->osTypeCount = numTypes;
			BlockMoveData( typeListPtr, (*typeListHand)->osType, numTypes * sizeof(OSType) );
		}
	}
	
	return handle;
}

/*
 *-----------------------------------------------------------------------------
 *
 * HandleNavEvent --
 *
 *
 * Results:
 *		None.
 *
 * Side effects:
 *		Events processed.
 *
 *-----------------------------------------------------------------------------
 */

pascal void 
HandleNavEvent( NavEventCallbackMessage callbackSelector, NavCBRecPtr callbackParams, void *callbackUD )
{
#pragma unused(callbackUD)

    static SInt32 otherEvent = ~(kNavCBCustomize|kNavCBStart|kNavCBTerminate
	    |kNavCBNewLocation|kNavCBShowDesktop|kNavCBSelectEntry|kNavCBAccept
	    |kNavCBCancel|kNavCBAdjustPreview);
	
	if (callbackSelector == kNavCBEvent) {

#if !TARGET_API_MAC_CARBON
		switch (callbackParams->eventData.eventDataParms.event->what) {

			case updateEvt:
        		if (TkMacConvertEvent( callbackParams->eventData.eventDataParms.event)) {
            		while (Tcl_DoOneEvent(TCL_IDLE_EVENTS|TCL_DONT_WAIT|TCL_WINDOW_EVENTS)) {
               			/* Empty Body */
            		}
            	}
				break;

			case nullEvent:
				// Handle Null Event
				break;
		}
#endif

    } else if ( callbackSelector & otherEvent != 0) { 
        while (Tcl_DoOneEvent(TCL_IDLE_EVENTS|TCL_DONT_WAIT|TCL_WINDOW_EVENTS)) {
            /* Empty Body */
        }
	}
}

/*---------------------------------------------------------------------------*/