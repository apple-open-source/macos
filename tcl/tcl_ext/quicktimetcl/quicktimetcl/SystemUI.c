/*
 *  SystemUI.c
 *  QuickTimeTcl
 *
 *  Created by Mats Bengtsson on Wed May 12 2004.
 *  Copyright (c) 2004 Mats Bengtsson. All rights reserved.
 *
 */

#include "QuickTimeTcl.h"


/*
 * For dispatching commands.
 */

static char *allUIModeCmds[] = {
    "normal", "contentsuppressed", "contenthidden", "allhidden",
    (char *) NULL
};

enum {
	kQTTclUIModeCmdNormal						= 0L,
    kQTTclUIModeCmdContentSuppressed,
    kQTTclUIModeCmdContentHidden,
    kQTTclUIModeCmdAllHidden
};

static char *allUIModeOptions[] = {
	"-autoshowmenubar", "-disableapplemenu", "-disableprocessswitch", 
    "-disableforcequit", "-disablesessionterminate",
    (char *) NULL
};

enum {
    kQTTclUIModeOptionAutoShowMenuBar			= 0L,
    kQTTclUIModeOptionDisableAppleMenu,
    kQTTclUIModeOptionDisableProcessSwitch,
    kQTTclUIModeOptionDisableForceQuit,
    kQTTclUIModeOptionDisableSessionTerminate
};

/*
 *----------------------------------------------------------------------
 *
 * MacControlUICmd --
 *
 *		Gets or sets the system UI mode.  
 *
 * Results:
 *		Standard Tcl result
 *
 * Side effects:
 *		None
 *
 *----------------------------------------------------------------------
 */

int MacControlUICmd( ClientData     clientData,
        Tcl_Interp*   interp,
        int           objc,
        Tcl_Obj *CONST objv[])
{
    SystemUIMode      	mode;
    SystemUIOptions   	options = 0L;
    Tcl_Obj				*listObjPtr;
    int             	cmdIndex;
	int					iarg;
	int					optIndex;
    char              	errorCodeStr[20];
    OSStatus          	osErr = noErr;

    if (objc == 1) {

        /* 
         * Get current mode & options. 
         */

        GetSystemUIMode( &mode, &options );
        listObjPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );

        switch (mode) {
            case kUIModeNormal:
            Tcl_ListObjAppendElement( interp, listObjPtr, Tcl_NewStringObj("normal", -1) );		    	
            break;

            case kUIModeContentSuppressed:
            Tcl_ListObjAppendElement( interp, listObjPtr, Tcl_NewStringObj("contentsuppressed", -1) );		    	
            break;

            case kUIModeContentHidden:
            Tcl_ListObjAppendElement( interp, listObjPtr, Tcl_NewStringObj("contenthidden", -1) );		    	
            break;

            case kUIModeAllHidden:
            Tcl_ListObjAppendElement( interp, listObjPtr, Tcl_NewStringObj("allhidden", -1) );		    	
            break;

            default:
            Tcl_ListObjAppendElement( interp, listObjPtr, Tcl_NewStringObj("unknown", -1) );		    	
            break;
        }

        if (options & kUIOptionAutoShowMenuBar) {
            Tcl_ListObjAppendElement( interp, listObjPtr, Tcl_NewStringObj("-autoshowmenubar", -1) );		    	
        }
        if (options & kUIOptionDisableAppleMenu) {
            Tcl_ListObjAppendElement( interp, listObjPtr, Tcl_NewStringObj("-disableapplemenu", -1) );		    	
        }
        if (options & kUIOptionDisableProcessSwitch) {
            Tcl_ListObjAppendElement( interp, listObjPtr, Tcl_NewStringObj("-disableprocessswitch", -1) );		    	
        }
        if (options & kUIOptionDisableForceQuit) {
            Tcl_ListObjAppendElement( interp, listObjPtr, Tcl_NewStringObj("-disableforcequit", -1) );		    	
        }
        if (options & kUIOptionDisableSessionTerminate) {
            Tcl_ListObjAppendElement( interp, listObjPtr, Tcl_NewStringObj("-disablesessionterminate", -1) );		    	
        }
        Tcl_SetObjResult( interp, listObjPtr );
        return TCL_OK;
    }

    /* 
     * Set current mode & options. 
     */

	if (Tcl_GetIndexFromObj( interp, objv[1], allUIModeCmds, "command", 
			TCL_EXACT, &cmdIndex ) != TCL_OK ) {
	    return TCL_ERROR;
	}
    switch(cmdIndex) {

        case kQTTclUIModeCmdNormal: {
    		mode = kUIModeNormal;
			break;
		}
        case kQTTclUIModeCmdContentSuppressed: {
    		mode = kUIModeContentSuppressed;
			break;
		}
        case kQTTclUIModeCmdContentHidden: {
    		mode = kUIModeContentHidden;
			break;
		}
        case kQTTclUIModeCmdAllHidden: {
    		mode = kUIModeAllHidden;
			break;
		}
    }    
    
	/*
	 * Process the configuration options if any.
	 */

	for (iarg = 2; iarg < objc; iarg++) {
		if (Tcl_GetIndexFromObj( interp, objv[iarg], allUIModeOptions, 
				"option", TCL_EXACT, &optIndex ) != TCL_OK ) {
			return TCL_ERROR;
		}
        
        switch(optIndex) {

            case kQTTclUIModeOptionAutoShowMenuBar: {
                options |= kUIOptionAutoShowMenuBar;
                break;
            }
            case kQTTclUIModeOptionDisableAppleMenu: {
                options |= kUIOptionDisableAppleMenu;
                break;
            }
            case kQTTclUIModeOptionDisableProcessSwitch: {
                options |= kUIOptionDisableProcessSwitch;
                break;
            }
            case kQTTclUIModeOptionDisableForceQuit: {
                options |= kUIOptionDisableForceQuit;
                break;
            }
            case kQTTclUIModeOptionDisableSessionTerminate: {
                options |= kUIOptionDisableSessionTerminate;
                break;
            }
        }
    }

    osErr = SetSystemUIMode( mode, options );
    
    if (osErr != noErr) {
        sprintf( errorCodeStr, "%i", (int) osErr );
        Tcl_AppendResult( interp, "system call \"SetSystemUIMode\" returned error code: ",
                errorCodeStr, (char*) NULL );
        return TCL_ERROR;
    }

    return TCL_OK;
}


