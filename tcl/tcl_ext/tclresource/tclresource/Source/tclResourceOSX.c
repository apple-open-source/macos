/* 
 * File : "tclResourceOSX.c"
 *                        Created : 2003-09-20 10:13:07
 *              Last modification : 2003-10-24 19:37:30
 * Author: Bernard Desgraupes
 * e-mail: <bdesgraupes@easyconnect.fr>
 * Note:
 * =====
 * (bd 2003/09/20): this is a development of the tclMacResource.c file from
 * the Tcl sources (/tcl/mac/tclMacResource.c) to define on MacOSX the 
 * [resource] command as a loadable extension (dylib). 
 * OSX versions of Tcl do not have the [resource] command built-in. 
 * With this extension, it is available with the following instruction:
 *         package require resource
 *         
 * (c) Copyright : Bernard Desgraupes, 2003
 *         All rights reserved.
 * This software is free software with BSD licence.
 * Versions history: see the Changes.Log file.
 */

/* Original header from /tcl/mac/tclMacResource.c
 * 
 * tclMacResource.c --
 *
 *	This file contains several commands that manipulate or use
 *	Macintosh resources.  Included are extensions to the "source"
 *	command, the mac specific "beep" and "resource" commands, and
 *	administration for open resource file references.
 *
 * Copyright (c) 1996-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 */

#include <fcntl.h>

/*
 * This flag tells the RegisterResource function to insert the
 * resource into the tail of the resource fork list.  Needed only
 * Resource_Init.
 */
 
#define TCL_RESOURCE_INSERT_TAIL 1
/*
 * 2 is taken by TCL_RESOURCE_DONT_CLOSE
 * which is the only public flag to TclMacRegisterResourceFork.
 * 
 * We moved it here from tclMacInt.h.
 *
 * This flag is passed to TclMacRegisterResourceFork
 * by a file (usually a library) whose resource fork
 * should not be closed by the resource command.
 */
 
#define TCL_RESOURCE_DONT_CLOSE  2

 
#define TCL_RESOURCE_CHECK_IF_OPEN 4

	    
/*
 * Hash table to track open resource files.
 */

typedef struct OpenResourceFork {
    short fileRef;
    int   fileFork;
    int   flags;
} OpenResourceFork;


/* 
 * Enumerated values to designate the fork containing resources
 */
static enum {	
	from_unspecified = -1,
	from_anyfork = 0,
	from_rezfork,
	from_datafork
} forkEnum;


static Tcl_HashTable nameTable;		/* Id to process number mapping. */
static Tcl_HashTable resourceTable;	/* Process number to id mapping. */
static Tcl_Obj *resourceForkList;       /* Ordered list of resource forks */
static int appResourceIndex;            /* This is the index of the application*
					 * in the list of resource forks */
static int newId = 0;			/* Id source. */
static int initialized = 0;		/* 0 means static structures haven't 
					 * been initialized yet. */
static int osTypeInit = 0;		/* 0 means Tcl object of osType hasn't 
					 * been initialized yet. */
/*
 * Prototypes for static procedures defined later in this file:
 */

static void	DupOSTypeInternalRep(Tcl_Obj *srcPtr, Tcl_Obj *copyPtr);
static void	ResourceInitialize(void);
static void	BuildResourceForkList(void);
static int	SetOSTypeFromAny(Tcl_Interp *interp, Tcl_Obj *objPtr);
static void	UpdateStringOfOSType(Tcl_Obj *objPtr);
static int	Tcl_ResourceAttributesObjCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[], Tcl_Obj *resultPtr);
static int	Tcl_ResourceCloseObjCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[], Tcl_Obj *resultPtr);
static int	Tcl_ResourceDeleteObjCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[], Tcl_Obj *resultPtr);
static int	Tcl_ResourceFilesObjCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[], Tcl_Obj *resultPtr);
static int	Tcl_ResourceForkObjCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[], Tcl_Obj *resultPtr);
static int	Tcl_ResourceIdObjCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[], Tcl_Obj *resultPtr);
static int	Tcl_ResourceListObjCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[], Tcl_Obj *resultPtr);
static int	Tcl_ResourceNameObjCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[], Tcl_Obj *resultPtr);
static int	Tcl_ResourceOpenObjCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[], Tcl_Obj *resultPtr);
static int	Tcl_ResourceReadObjCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[], Tcl_Obj *resultPtr);
static int	Tcl_ResourceTypesObjCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[], Tcl_Obj *resultPtr);
static int	Tcl_ResourceUpdateObjCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[], Tcl_Obj *resultPtr);
static int	Tcl_ResourceWriteObjCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[], Tcl_Obj *resultPtr);
static OpenResourceFork* GetResourceRefFromObj(Tcl_Obj *objPtr, int okayOnReadOnly, 
					   const char *operation, Tcl_Obj *resultPtr);

/* 
 * Prototypes moved from /tcl/generic/tclPlatDecls.h
 */
Handle	 	Tcl_MacFindResource(Tcl_Interp * interp, 
			    long resourceType, CONST char * resourceName, 
			    int resourceNumber, CONST char * resFileRef, 
			    int * releaseIt);
int 		Tcl_GetOSTypeFromObj(Tcl_Interp * interp, Tcl_Obj * objPtr, OSType * osTypePtr);
void 		Tcl_SetOSTypeObj(Tcl_Obj * objPtr, OSType osType);
Tcl_Obj * 	Tcl_NewOSTypeObj(OSType osType);
int 		TclMacRegisterResourceFork(short fileRef, Tcl_Obj * tokenPtr, int whichFork, int insert);
short 		TclMacUnRegisterResourceFork(char * tokenPtr, Tcl_Obj * resultPtr);

int			Tcl_ResourceObjCmd(ClientData clientData,  Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);

/* 
 * The init function called when the package is loaded 
 * in the Tcl interpreter. 
 */
#pragma export on
int Resource_Init(Tcl_Interp *interp);
#pragma export off


/*
 * The structures below defines the Tcl object type defined in this file by
 * means of procedures that can be invoked by generic object code.
 */

static Tcl_ObjType osType = {
    "ostype",				/* name */
    (Tcl_FreeInternalRepProc *) NULL,   /* freeIntRepProc */
    DupOSTypeInternalRep,	        /* dupIntRepProc */
    UpdateStringOfOSType,		/* updateStringProc */
    SetOSTypeFromAny			/* setFromAnyProc */
};


/*
 *----------------------------------------------------------------------
 *
 * Resource_Init --
 *
 *	This procedure is invoked when the package is loaded.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int Resource_Init(Tcl_Interp *interp) {
	char vstr[64];
	
#ifdef USE_TCL_STUBS
    if (Tcl_InitStubs(interp, "8.1", 0) == NULL) {
		return TCL_ERROR;
    }
#endif

	/* Register resource command */
    Tcl_CreateObjCommand(interp, "resource", Tcl_ResourceObjCmd,
			     (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
	
    sprintf(vstr,"%d.%d%c%d\0", TCLRESOURCE_MAJOR, TCLRESOURCE_MINOR, 
			TCLRESOURCE_STAGE, TCLRESOURCE_SUBMINOR);
				 
    /* Declare the tclResource package. */
    if (Tcl_PkgProvide(interp, "resource", vstr) != TCL_OK) {
		return TCL_ERROR;
    }       
    return TCL_OK;
}



/*
 *----------------------------------------------------------------------
 *
 * Tcl_ResourceObjCmd --
 *
 *	This procedure is invoked to process the "resource" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_ResourceObjCmd(
    ClientData clientData,		/* Not used. */
    Tcl_Interp *interp,			/* Current interpreter. */
    int objc,				/* Number of arguments. */
    Tcl_Obj *CONST objv[])		/* Argument values. */
{
    Tcl_Obj *resultPtr;
    int index, result;

    static CONST char *switches[] = {"attributes", "close", "delete", "files", "fork", 
	"id", "list", "name", "open", "read", "types", "update", "write", (char *) NULL
    };
	        
    enum {
            RESOURCE_ATTRIBUTES, RESOURCE_CLOSE, RESOURCE_DELETE, RESOURCE_FILES, 
	    RESOURCE_FORK, RESOURCE_ID, RESOURCE_LIST, RESOURCE_NAME, RESOURCE_OPEN, 
	    RESOURCE_READ, RESOURCE_TYPES, RESOURCE_UPDATE, RESOURCE_WRITE
    };
              
    resultPtr = Tcl_GetObjResult(interp);
    
    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "option ?arg ...?");
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[1], switches, "option", 0, &index)
	    != TCL_OK) {
	return TCL_ERROR;
    }
    if (!initialized) {
	ResourceInitialize();
    }

    switch (index) {
	case RESOURCE_ATTRIBUTES:
	result = Tcl_ResourceAttributesObjCmd(clientData, interp, objc, objv, resultPtr);
	break;
	
	case RESOURCE_CLOSE:			
	result = Tcl_ResourceCloseObjCmd(clientData, interp, objc, objv, resultPtr);
	break;
	
	case RESOURCE_DELETE:
	result = Tcl_ResourceDeleteObjCmd(clientData, interp, objc, objv, resultPtr);
	break;
	
	case RESOURCE_FILES:
	result = Tcl_ResourceFilesObjCmd(clientData, interp, objc, objv, resultPtr);
	break;
	
	case RESOURCE_FORK:			
	result = Tcl_ResourceForkObjCmd(clientData, interp, objc, objv, resultPtr);
	break;
	
	case RESOURCE_ID:			
	result = Tcl_ResourceIdObjCmd(clientData, interp, objc, objv, resultPtr);
	break;
	
	case RESOURCE_LIST:			
	result = Tcl_ResourceListObjCmd(clientData, interp, objc, objv, resultPtr);
	break;
	
	case RESOURCE_NAME:
	result = Tcl_ResourceNameObjCmd(clientData, interp, objc, objv, resultPtr);
	break;
	
	case RESOURCE_OPEN:
	result = Tcl_ResourceOpenObjCmd(clientData, interp, objc, objv, resultPtr);
	break;
	
	case RESOURCE_READ:			
	result = Tcl_ResourceReadObjCmd(clientData, interp, objc, objv, resultPtr);
	break;
	
	case RESOURCE_TYPES:			
	result = Tcl_ResourceTypesObjCmd(clientData, interp, objc, objv, resultPtr);
	break;
	
	case RESOURCE_UPDATE:			
	result = Tcl_ResourceUpdateObjCmd(clientData, interp, objc, objv, resultPtr);
	break;
	
	case RESOURCE_WRITE:			
	result = Tcl_ResourceWriteObjCmd(clientData, interp, objc, objv, resultPtr);
	break;
	
	default:
	    panic("Tcl_GetIndexFromObj returned unrecognized option");
	    return TCL_ERROR;	/* Should never be reached. */
    }
    return result;
}


/*
 *----------------------------------------------------------------------
 *
 * Tcl_ResourceAttributesObjCmd --
 *
 *	This procedure is invoked to process the "resource attributes" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_ResourceAttributesObjCmd(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,			/* Current interpreter. */
    int objc,				/* Number of arguments. */
    Tcl_Obj *CONST objv[],		/* Argument values. */
    Tcl_Obj *resultPtr)			/* Pointer to store the result. */
{
    int index, result, gotInt, gotValue, length, newValue;
    long rsrcId;
    short saveRef, theAttrs;
    char *resourceId = NULL;
    char buffer[16];
    OpenResourceFork *resourceRef;
    Handle resource = NULL;
    OSErr err;
    Str255 theName;
    OSType rezType;

    static CONST char *attributesSwitches[] = {
            "-id", "-name", (char *) NULL
    };
            
    enum {
            RESOURCE_ATTRIBUTES_ID, RESOURCE_ATTRIBUTES_NAME
    };

    result = TCL_OK;

	    
	    if (!(objc == 3 || objc == 4 || objc == 6 || objc == 7)) {
		Tcl_WrongNumArgs(interp, 2, objv, 
		"resourceRef ?(-id resourceId|-name resourceName) resourceType? ?value?");
		return TCL_ERROR;
	    }
	    
	    resourceRef = GetResourceRefFromObj(objv[2], true, 
			    "get attributes from", resultPtr);
	    if (resourceRef == NULL) {
		return TCL_ERROR;
	    }	
	    
	    gotValue = false; 
	   
	    if (objc == 4 || objc == 7) {
		if (Tcl_GetIntFromObj(interp, objv[objc-1], &newValue)
			!= TCL_OK) {
		    return TCL_ERROR;
		}
	       gotValue = true;
	    } 
	    
	    if (objc == 3) {
		/* 
		 * Getting the resource map attributes
		 */
	        theAttrs = GetResFileAttrs(resourceRef->fileRef);
		err = ResError();
		if (err != noErr) {
		    Tcl_AppendStringsToObj(resultPtr,
			    "error getting resource map attributes",
		            (char *) NULL);
		    return TCL_ERROR;
		} else {
		    Tcl_SetObjResult(interp, Tcl_NewIntObj(theAttrs) );
		    return TCL_OK;
		}
	    } 
	    
	    if (objc == 4) {
		/* 
		 * Setting the resource map attributes
		 */
	        SetResFileAttrs(resourceRef->fileRef, newValue);
		err = ResError();
		if (err != noErr) {
		    Tcl_AppendStringsToObj(resultPtr,
			    "error setting resource map attributes",
		            (char *) NULL);
		    return TCL_ERROR;
		} 
		return TCL_OK;
	    } 
	    
	    gotInt = false;
	    resourceId = NULL;

	    if (Tcl_GetIndexFromObj(interp, objv[3], attributesSwitches,
		    "switch", 0, &index) != TCL_OK) {
		return TCL_ERROR;
	    }

	    switch (index) {
		case RESOURCE_ATTRIBUTES_ID:		
		    if (Tcl_GetLongFromObj(interp, objv[4], &rsrcId)
			    != TCL_OK) {
			return TCL_ERROR;
		    }
		    gotInt = true;
		    break;
		case RESOURCE_ATTRIBUTES_NAME:		
		    resourceId = Tcl_GetStringFromObj(objv[4], &length);
		    strcpy((char *) theName, resourceId);
		    resourceId = (char *) theName;
		    c2pstr(resourceId);
		    break;
	    }
	    if (Tcl_GetOSTypeFromObj(interp, objv[5], &rezType) != TCL_OK) {
		return TCL_ERROR;
	    }
	    
	    if ((resourceId == NULL) && !gotInt) {
		Tcl_AppendStringsToObj(resultPtr,"you must specify either ",
		        "\"-id\" or \"-name\" to get the ",
		        "attributes of a specified resource",
		        (char *) NULL);
	        return TCL_ERROR;
            }
	    
	    saveRef = CurResFile();
	    UseResFile((short) resourceRef->fileRef);
	    
	    if (gotInt == true) {
		// Don't load the resource in memory 
		SetResLoad(false);
		resource = Get1Resource(rezType, rsrcId);
		SetResLoad(true);
                err = ResError();
            
                if (err == resNotFound || resource == NULL) {
	            Tcl_AppendStringsToObj(resultPtr, "resource not found",
	                (char *) NULL);
	            result = TCL_ERROR;
	            goto attributesDone;               
                } else if (err != noErr) {
                    sprintf(buffer, "%12d", err);
	            Tcl_AppendStringsToObj(resultPtr, "resource error #",
	                    buffer, "occured while trying to find resource",
	                    (char *) NULL);
	            result = TCL_ERROR;
	            goto attributesDone;               
	        }
	    } 
	    
	    if (resourceId != NULL) {
	        Handle tmpResource;
		// Don't load the resource in memory 
		SetResLoad(false);
		tmpResource = Get1NamedResource(rezType, (StringPtr) resourceId);
		SetResLoad(true);
                err = ResError();
            
                if (err == resNotFound || tmpResource == NULL) {
	            Tcl_AppendStringsToObj(resultPtr, "resource not found",
	                (char *) NULL);
	            result = TCL_ERROR;
	            goto attributesDone;               
                } else if (err != noErr) {                
                    sprintf(buffer, "%12d", err);
	            Tcl_AppendStringsToObj(resultPtr, "resource error #",
	                    buffer, "occured while trying to find resource",
	                    (char *) NULL);
	            result = TCL_ERROR;
	            goto attributesDone;               
	        }
	        
	        if (gotInt) { 
	            if (resource != tmpResource) {
	                Tcl_AppendStringsToObj(resultPtr,
				"\"-id\" and \"-name\" ",
	                        "values do not point to the same resource",
	                        (char *) NULL);
	                result = TCL_ERROR;
	                goto attributesDone;
	            }
	        } else {
	            resource = tmpResource;
	        }
	    }
	    
	    if (gotValue) {
		/* 
		 * Setting the resource attributes
		 */
		theAttrs = GetResFileAttrs((short) resourceRef->fileRef);
		if (theAttrs & mapReadOnly) {
		    Tcl_AppendStringsToObj(resultPtr, 
					   "cannot set the attributes, resource map is read only",
					 (char *) NULL);
		    result = TCL_ERROR;
		    goto attributesDone;
		}
		theAttrs = GetResAttrs(resource);
		if (theAttrs != newValue) {
		    // Set the resChanged flag manually because, if the resProtected 
		    // flag has to be set, it will be immediately active and 
		    // ChangedResource(resource) would fail.
		    newValue |= resChanged;
		    SetResAttrs(resource, newValue);
		    err = ResError();
		    if (err != noErr) {
			sprintf(buffer, "%12d", err);
			Tcl_AppendStringsToObj(resultPtr,
				"error ", buffer, " setting resource attributes",
				(char *) NULL);
			result = TCL_ERROR;
	                goto attributesDone;
		    } 
		} 
		result = TCL_OK;
	    } else {
		/* 
		 * Getting the resource attributes
		 */
		theAttrs = GetResAttrs(resource);
		err = ResError();
		if (err != noErr) {
		    Tcl_AppendStringsToObj(resultPtr,
			    "error getting resource attributes",
			    (char *) NULL);
		    result = TCL_ERROR;
	            goto attributesDone;
		} else {
		    Tcl_SetObjResult(interp, Tcl_NewIntObj(theAttrs) );
		    result = TCL_OK;
		}
	    }
	    
	attributesDone:
	   UseResFile(saveRef);                        
	   return result;	
}


/*
 *----------------------------------------------------------------------
 *
 * Tcl_ResourceCloseObjCmd --
 *
 *	This procedure is invoked to process the "resource close" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_ResourceCloseObjCmd(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,			/* Current interpreter. */
    int objc,				/* Number of arguments. */
    Tcl_Obj *CONST objv[],		/* Argument values. */
    Tcl_Obj *resultPtr)			/* Pointer to store the result. */
{
    int length;
    long fileRef;
    char *stringPtr;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "resourceRef");
	return TCL_ERROR;
    }
    stringPtr = Tcl_GetStringFromObj(objv[2], &length);
    fileRef = TclMacUnRegisterResourceFork(stringPtr, resultPtr);
    
    if (fileRef >= 0) {
	CloseResFile((short) fileRef);
	return TCL_OK;
    } else {
	return TCL_ERROR;
    }
}


/*
 *----------------------------------------------------------------------
 *
 * Tcl_ResourceDeleteObjCmd --
 *
 *	This procedure is invoked to process the "resource delete" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_ResourceDeleteObjCmd(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,			/* Current interpreter. */
    int objc,				/* Number of arguments. */
    Tcl_Obj *CONST objv[],		/* Argument values. */
    Tcl_Obj *resultPtr)			/* Pointer to store the result. */
{
    int index, result, gotInt;
    int i, limitSearch, length;
    short saveRef, resInfo;
    long fileRef, rsrcId;
    char *resourceId = NULL;	
    OpenResourceFork *resourceRef;
    Handle resource = NULL;
    OSErr err;
    Str255 theName;
    OSType rezType;

    static CONST char *deleteSwitches[] = {"-id", "-name", "-file", (char *) NULL};
             
    enum {RESOURCE_DELETE_ID, RESOURCE_DELETE_NAME, RESOURCE_DELETE_FILE};

    result = TCL_OK;

	    if (!((objc >= 3) && (objc <= 9) && ((objc % 2) == 1))) {
		Tcl_WrongNumArgs(interp, 2, objv, 
		    "?-id resourceId? ?-name resourceName? ?-file resourceRef? resourceType");
		return TCL_ERROR;
	    }
	    
	    i = 2;
	    fileRef = -1;
	    gotInt = false;
	    resourceId = NULL;
	    limitSearch = false;

	    while (i < (objc - 2)) {
		if (Tcl_GetIndexFromObj(interp, objv[i], deleteSwitches,
			"option", 0, &index) != TCL_OK) {
		    return TCL_ERROR;
		}

		switch (index) {
		    case RESOURCE_DELETE_ID:		
			if (Tcl_GetLongFromObj(interp, objv[i+1], &rsrcId)
				!= TCL_OK) {
			    return TCL_ERROR;
			}
			gotInt = true;
			break;
		    case RESOURCE_DELETE_NAME:		
			resourceId = Tcl_GetStringFromObj(objv[i+1], &length);
			if (length > 255) {
			    Tcl_AppendStringsToObj(resultPtr,"-name argument ",
			            "too long, must be < 255 characters",
			            (char *) NULL);
			    return TCL_ERROR;
			}
			strcpy((char *) theName, resourceId);
			resourceId = (char *) theName;
			c2pstr(resourceId);
			break;
		    case RESOURCE_DELETE_FILE:
		        resourceRef = GetResourceRefFromObj(objv[i+1], 0, 
		                "delete from", resultPtr);
		        if (resourceRef == NULL) {
		            return TCL_ERROR;
		        }	
			limitSearch = true;
			break;
		}
		i += 2;
	    }
	    
	    if ((resourceId == NULL) && !gotInt) {
		Tcl_AppendStringsToObj(resultPtr,"you must specify either ",
		        "\"-id\" or \"-name\" or both ",
		        "to \"resource delete\"",
		        (char *) NULL);
	        return TCL_ERROR;
            }

	    if (Tcl_GetOSTypeFromObj(interp, objv[i], &rezType) != TCL_OK) {
		return TCL_ERROR;
	    }

	    if (limitSearch) {
		saveRef = CurResFile();
		UseResFile((short) resourceRef->fileRef);
	    }
	    
	    SetResLoad(false);
	    
	    if (gotInt == true) {
	        if (limitSearch) {
		    resource = Get1Resource(rezType, rsrcId);
		} else {
		    resource = GetResource(rezType, rsrcId);
		}
                err = ResError();
            
                if (err == resNotFound || resource == NULL) {
	            Tcl_AppendStringsToObj(resultPtr, "resource not found",
	                (char *) NULL);
	            result = TCL_ERROR;
	            goto deleteDone;               
                } else if (err != noErr) {
                    char buffer[16];
                
                    sprintf(buffer, "%12d", err);
	            Tcl_AppendStringsToObj(resultPtr, "resource error #",
	                    buffer, "occured while trying to find resource",
	                    (char *) NULL);
	            result = TCL_ERROR;
	            goto deleteDone;               
	        }
	    } 
	    
	    if (resourceId != NULL) {
	        Handle tmpResource;
	        if (limitSearch) {
	            tmpResource = Get1NamedResource(rezType,
			    (StringPtr) resourceId);
	        } else {
	            tmpResource = GetNamedResource(rezType,
			    (StringPtr) resourceId);
	        }
                err = ResError();
            
                if (err == resNotFound || tmpResource == NULL) {
	            Tcl_AppendStringsToObj(resultPtr, "resource not found",
	                (char *) NULL);
	            result = TCL_ERROR;
	            goto deleteDone;               
                } else if (err != noErr) {
                    char buffer[16];
                
                    sprintf(buffer, "%12d", err);
	            Tcl_AppendStringsToObj(resultPtr, "resource error #",
	                    buffer, "occured while trying to find resource",
	                    (char *) NULL);
	            result = TCL_ERROR;
	            goto deleteDone;               
	        }
	        
	        if (gotInt) { 
	            if (resource != tmpResource) {
	                Tcl_AppendStringsToObj(resultPtr,
				"\"-id\" and \"-name\" ",
	                        "values do not point to the same resource",
	                        (char *) NULL);
	                result = TCL_ERROR;
	                goto deleteDone;
	            }
	        } else {
	            resource = tmpResource;
	        }
	    }
	        
       	    resInfo = GetResAttrs(resource);
	    
	    if ((resInfo & resProtected) == resProtected) {
	        Tcl_AppendStringsToObj(resultPtr, "resource ",
	                "cannot be deleted: it is protected.",
	                (char *) NULL);
	        result = TCL_ERROR;
	        goto deleteDone;               
	    } else if ((resInfo & resSysHeap) == resSysHeap) {   
	        Tcl_AppendStringsToObj(resultPtr, "resource",
	                "cannot be deleted: it is in the system heap.",
	                (char *) NULL);
	        result = TCL_ERROR;
	        goto deleteDone;               
	    }
	    
	    /*
	     * Find the resource file, if it was not specified,
	     * so we can flush the changes now.  Perhaps this is
	     * a little paranoid, but better safe than sorry.
	     */
	     
	    RemoveResource(resource);
	    
	    if (!limitSearch) {
	        UpdateResFile(HomeResFile(resource));
	    } else {
	        UpdateResFile(resourceRef->fileRef);
	    }
	    
	    
	    deleteDone:
            SetResLoad(true);
	    if (limitSearch) {
                 UseResFile(saveRef);                        
	    }
	    return result;
}


/*
 *----------------------------------------------------------------------
 *
 * Tcl_ResourceFilesObjCmd --
 *
 *	This procedure is invoked to process the "resource files" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_ResourceFilesObjCmd(
    ClientData clientData,	/* Not used. */
    Tcl_Interp * interp,		/* Current interpreter. */
    int objc,				/* Number of arguments. */
    Tcl_Obj *CONST objv[],		/* Argument values. */
    Tcl_Obj *resultPtr)			/* Pointer to store the result. */
{
    int length;
    char *stringPtr;
    OpenResourceFork *resourceRef;
    OSErr err;

	if ((objc < 2) || (objc > 3)) {
	Tcl_SetStringObj(resultPtr,
			"wrong # args: should be \"resource files \?resourceId?\"", -1);
	return TCL_ERROR;
	}
	
	if (objc == 2) {
		stringPtr = Tcl_GetStringFromObj(resourceForkList, &length);
		Tcl_SetStringObj(resultPtr, stringPtr, length);
	} else {
		FCBPBRec	fileRec;
		Str255		fileName;
		UInt8		pathPtr[256];
		FSSpec		fileFSSpec;
		FSRef		fileFSRef;
		Tcl_DString	dstr;
		
		resourceRef = GetResourceRefFromObj(objv[2], 1, "files", resultPtr);
		if (resourceRef == NULL) {
			return TCL_ERROR;
		}

		fileRec.ioCompletion = NULL;
		fileRec.ioFCBIndx = 0;
		fileRec.ioNamePtr = fileName;
		fileRec.ioVRefNum = 0;
		fileRec.ioRefNum = resourceRef->fileRef;
		err = PBGetFCBInfo(&fileRec, false);
		if (err != noErr) {
			Tcl_SetStringObj(resultPtr,
					"could not get FCB for resource file", -1);
			return TCL_ERROR;
		}

		/* Get an FSRef and build the path */
		fileFSSpec.vRefNum = fileRec.ioFCBVRefNum;
		fileFSSpec.parID = fileRec.ioFCBParID;
		strncpy( (char *) fileFSSpec.name, fileRec.ioNamePtr, fileRec.ioNamePtr[0]+1);
		err = FSpMakeFSRef(&fileFSSpec, &fileFSRef);
		err = FSRefMakePath(&fileFSRef, pathPtr, 256);
		if ( err != noErr) {
			Tcl_SetStringObj(resultPtr,
					"could not get file path from token", -1);
			return TCL_ERROR;
		}
			
		Tcl_ExternalToUtfDString(NULL, pathPtr, strlen(pathPtr), &dstr);
		Tcl_SetStringObj(resultPtr, Tcl_DStringValue(&dstr), Tcl_DStringLength(&dstr));
		Tcl_DStringFree(&dstr);
	}                    	    
	return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * Tcl_ResourceForkObjCmd --
 *
 *	This procedure is invoked to process the "resource fork" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_ResourceForkObjCmd(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,			/* Current interpreter. */
    int objc,				/* Number of arguments. */
    Tcl_Obj *CONST objv[],		/* Argument values. */
    Tcl_Obj *resultPtr)			/* Pointer to store the result. */
{
    OpenResourceFork *resourceRef;

	    if (objc != 3) {
		Tcl_WrongNumArgs(interp, 2, objv, "resourceRef");
		return TCL_ERROR;
	    }
	    resourceRef = GetResourceRefFromObj(objv[2], true, 
		                "get fork from", resultPtr);
	    
	    if (resourceRef != NULL) {
		Tcl_ResetResult(interp);
		switch (resourceRef->fileFork) {
		  case from_rezfork:
		    Tcl_AppendStringsToObj(Tcl_GetObjResult(interp), "resourcefork", (char *) NULL);
		    return TCL_OK;
		    break;
		
		  case from_datafork:
		    Tcl_AppendStringsToObj(Tcl_GetObjResult(interp), "datafork", (char *) NULL);
		    return TCL_OK;
		    break;
		
		  default:
		    Tcl_AppendStringsToObj(Tcl_GetObjResult(interp), "unknown", (char *) NULL);
		    return TCL_OK;
		}
	    } else {
	        return TCL_ERROR;
	    }
}


/*
 *----------------------------------------------------------------------
 *
 * Tcl_ResourceIdObjCmd --
 *
 *	This procedure is invoked to process the "resource id" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_ResourceIdObjCmd(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,			/* Current interpreter. */
    int objc,				/* Number of arguments. */
    Tcl_Obj *CONST objv[],		/* Argument values. */
    Tcl_Obj *resultPtr)			/* Pointer to store the result. */
{
    short rsrcId = 0;  /* A short, not a long. Reverse from ResourceNameObjCmd */
    int length, releaseIt = 0;
    char *stringPtr;
    char *resourceId = NULL;	
    Handle resource = NULL;
    OSErr err;
    Str255 theName;
    OSType rezType;

	    Tcl_ResetResult(interp);
	    if (objc != 5) {
		Tcl_WrongNumArgs(interp, 2, objv,
			"resourceType resourceId resourceRef");
		return TCL_ERROR;
	    }

	    if (Tcl_GetOSTypeFromObj(interp, objv[2], &rezType) != TCL_OK) {
		return TCL_ERROR;
	    }
	    
	    resourceId = Tcl_GetStringFromObj(objv[3], &length);

	    if (resourceId == NULL) {
		Tcl_AppendStringsToObj(resultPtr,
			"wrong third argument",
			(char *) NULL);
		return TCL_ERROR;
	    } 
	    
	    stringPtr = Tcl_GetStringFromObj(objv[4], &length);
	    resource = Tcl_MacFindResource(interp, rezType, resourceId,
		rsrcId, stringPtr, &releaseIt);
			    
	    if (resource != NULL) {
		GetResInfo(resource, &rsrcId, (ResType *) &rezType, theName);
		err = ResError();
		if (err == noErr) {
		    Tcl_SetIntObj( resultPtr, rsrcId);
		    return TCL_OK;
		} else {
		    Tcl_AppendStringsToObj(resultPtr,
			    "could not get resource info",
			    (char *) NULL);
		    return TCL_ERROR;
		}
		if (releaseIt) {
		    ReleaseResource(resource);
		}
	    } else {
		Tcl_AppendStringsToObj(resultPtr, "could not find resource",
		    (char *) NULL);
		return TCL_ERROR;
	    }
}


/*
 *----------------------------------------------------------------------
 *
 * Tcl_ResourceListObjCmd --
 *
 *	This procedure is invoked to process the "resource list" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */
			
int
Tcl_ResourceListObjCmd(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,			/* Current interpreter. */
    int objc,				/* Number of arguments. */
    Tcl_Obj *CONST objv[],		/* Argument values. */
    Tcl_Obj *resultPtr)			/* Pointer to store the result. */
{
    Tcl_Obj *objPtr;
    int result, count, i, limitSearch, onlyID, length;
    short id, saveRef;
    char *string;
    OpenResourceFork *resourceRef;
    Handle resource = NULL;
    Str255 theName;
    OSType rezType;

    result = TCL_OK;
    limitSearch = false;
    onlyID = false;
    i = 2;
    
	    if (!((objc >= 3) && (objc <= 5))) {
		Tcl_WrongNumArgs(interp, 2, objv, "?-ids? resourceType ?resourceRef?");
		return TCL_ERROR;
	    }
	    string = Tcl_GetStringFromObj(objv[i], &length);
	    if (!strcmp(string, "-ids")) {
		onlyID = true;
		i++;
	    } 
	    if (Tcl_GetOSTypeFromObj(interp, objv[i], &rezType) != TCL_OK) {
		return TCL_ERROR;
	    }
	    i++;
	    if (objc == i + 1) {
	        resourceRef = GetResourceRefFromObj(objv[i], 1, 
		                "list", resultPtr);
		if (resourceRef == NULL) {
		    return TCL_ERROR;
		}	

		saveRef = CurResFile();
		UseResFile((short) resourceRef->fileRef);
		limitSearch = true;
	    }

	    Tcl_ResetResult(interp);
	    if (limitSearch) {
		count = Count1Resources(rezType);
	    } else {
		count = CountResources(rezType);
	    }
	    SetResLoad(false);
	    for (i = 1; i <= count; i++) {
		if (limitSearch) {
		    resource = Get1IndResource(rezType, i);
		} else {
		    resource = GetIndResource(rezType, i);
		}
		if (resource != NULL) {
		    GetResInfo(resource, &id, (ResType *) &rezType, theName);
		    if (theName[0] != 0 && !onlyID) {
			objPtr = Tcl_NewStringObj((char *) theName + 1, theName[0]);
		    } else {
			objPtr = Tcl_NewIntObj(id);
		    }
		    /* Bug in the original code: the resource was released in any case 
		     * which could cause a crash when calling the command without a 
		     * recourceRef, like for instance:
		     *     resource list CURS
		     * which would release system CURS resources. 
		     * Fix: if the Master Pointer of the returned handle is
		     * null, then resource was not in memory, and it is
		     * safe to release it. Otherwise, it is not.
		     */
		    if (*resource == NULL) {
			ReleaseResource(resource);
		    }
		    result = Tcl_ListObjAppendElement(interp, resultPtr, objPtr);
		    if (result != TCL_OK) {
			Tcl_DecrRefCount(objPtr);
			break;
		    }
		}
	    }
	    SetResLoad(true);
	
	    if (limitSearch) {
		UseResFile(saveRef);
	    }
	
	    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * Tcl_ResourceNameObjCmd --
 *
 *	This procedure is invoked to process the "resource name" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_ResourceNameObjCmd(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,			/* Current interpreter. */
    int objc,				/* Number of arguments. */
    Tcl_Obj *CONST objv[],		/* Argument values. */
    Tcl_Obj *resultPtr)			/* Pointer to store the result. */
{
    long rsrcId;  /* A long, not a short. Reverse from ResourceIdObjCmd */
    int length, releaseIt = 0;
    char *stringPtr;
    Handle resource = NULL;
    OSErr err;
    Str255 theName;
    OSType rezType;

	    Tcl_ResetResult(interp);
	    if (objc != 5) {
		Tcl_WrongNumArgs(interp, 2, objv,
			"resourceType resourceId resourceRef");
		return TCL_ERROR;
	    }

	    if (Tcl_GetOSTypeFromObj(interp, objv[2], &rezType) != TCL_OK) {
		return TCL_ERROR;
	    }
	    
	    if (Tcl_GetLongFromObj(interp, objv[3], &rsrcId)
		    != TCL_OK) {
		Tcl_AppendStringsToObj(resultPtr,
			"wrong third argument: expected integer",
			(char *) NULL);
		return TCL_ERROR;
            }

	    stringPtr = Tcl_GetStringFromObj(objv[4], &length);
	    resource = Tcl_MacFindResource(interp, rezType, NULL,
		rsrcId, stringPtr, &releaseIt);
			    
	    if (resource != NULL) {
		GetResInfo(resource, &rsrcId, (ResType *) &rezType, theName);
		err = ResError();
		if (err == noErr) {
		    p2cstr(theName);
		    Tcl_AppendStringsToObj(resultPtr, theName, (char *) NULL);
		    return TCL_OK;
		} else {
		    Tcl_AppendStringsToObj(resultPtr,
			    "could not get resource info",
			    (char *) NULL);
		    return TCL_ERROR;
		}
		if (releaseIt) {
		    ReleaseResource(resource);
		}
	    } else {
		Tcl_AppendStringsToObj(resultPtr, "could not find resource",
		    (char *) NULL);
		return TCL_ERROR;
	    }
}


/*
 *----------------------------------------------------------------------
 *
 * Tcl_ResourceOpenObjCmd --
 *
 *	This procedure is invoked to process the "resource open" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_ResourceOpenObjCmd(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,			/* Current interpreter. */
    int objc,				/* Number of arguments. */
    Tcl_Obj *CONST objv[],		/* Argument values. */
    Tcl_Obj *resultPtr)			/* Pointer to store the result. */
{
    int index, length, mode;
    int fromFork, foundFork = from_unspecified, filenameIdx = 2;
    long fileRef;
    char *stringPtr;
    char macPermision;
    CONST char *str;
	char *native;
    FSSpec fileSpec;
    FSRef fileFSRef, parentFSRef;
    OSErr err;
    Tcl_DString ds, buffer;
    Boolean permSpecified = false, isDir = false, gotParentRef = false;

    static CONST char *openSwitches[] = {
		"-datafork", "-resourcefork", (char *) NULL
    };
            
    enum {
		RESOURCE_OPEN_DATAFORK, RESOURCE_OPEN_RESOURCEFORK
    };

    if (!((objc == 3) || (objc == 4) || (objc == 5))) {
	Tcl_WrongNumArgs(interp, 2, objv, "?(-datafork|-resourcefork)? fileName ?permissions?");
		return TCL_ERROR;
    }
	
    fromFork = from_anyfork;
    if (objc != 3) {
		if (Tcl_GetIndexFromObj(interp, objv[2], openSwitches,
					"switch", 0, &index) == TCL_OK) {
			switch (index) {
				case RESOURCE_OPEN_DATAFORK:		
					fromFork = from_datafork;
					break;
			
				case RESOURCE_OPEN_RESOURCEFORK:		
					fromFork = from_rezfork;
					break;
			}
			filenameIdx = 3;
			if (objc == 5) {
				permSpecified = true;
			}
		} else {
			if (objc == 5) {
			return TCL_ERROR;
			} else {
				filenameIdx = 2;
				permSpecified = true;
			}
		}
    }
	
    str = Tcl_GetStringFromObj(objv[filenameIdx], &length);
    if (Tcl_TranslateFileName(interp, str, &buffer) == NULL) {
		return TCL_ERROR;
    }
    native = Tcl_UtfToExternalDString(NULL, Tcl_DStringValue(&buffer),
									Tcl_DStringLength(&buffer), &ds);

	/* 
	 * FSPathMakeRef: the format of the pathname you must supply can be determined with the
	 * Gestalt selector gestaltFSAttr's gestaltFSUsesPOSIXPathsForConversion bit.
	 */
    err = FSPathMakeRef(native, &fileFSRef, &isDir);
    Tcl_DStringFree(&buffer);

    if (!((err == noErr) || (err == fnfErr))) {
		Tcl_AppendStringsToObj(resultPtr, "invalid path", (char *) NULL);
		return TCL_ERROR;
    }
    if (isDir) {
		Tcl_AppendStringsToObj(resultPtr, "specified path is a directory", (char *) NULL);
		return TCL_ERROR;
    }

	if (err == fnfErr) {
		// Build a FSSpec manually from the parent folder and the name
		char * 			separatorPtr;
		FSCatalogInfo 	catalogInfo;
		
		separatorPtr = strrchr(native, TCLRESOURCE_PATH_SEP);
		if (separatorPtr) {
			native[separatorPtr-native] = 0;
			err = FSPathMakeRef(native, &parentFSRef, &isDir);
			err = FSGetCatalogInfo(&parentFSRef, kFSCatInfoNodeID | kFSCatInfoVolume, &catalogInfo, NULL, NULL, NULL);
			if (err != noErr) {
				Tcl_AppendStringsToObj(resultPtr, "invalid parent folder", (char *) NULL);
				return TCL_ERROR;
			} else {
				gotParentRef = true;
			}
			fileSpec.vRefNum = catalogInfo.volume;
			fileSpec.parID = catalogInfo.nodeID;
			separatorPtr++;
			strncpy( (char *) fileSpec.name+1, separatorPtr, strlen(separatorPtr));
			fileSpec.name[0] = strlen(separatorPtr);
			err = fnfErr;
		} 
	} else {
		// Get the FSSpec from the FSRef
		err = FSGetCatalogInfo(&fileFSRef, kFSCatInfoNone, NULL, NULL, &fileSpec, NULL);
		if (err != noErr) {
			Tcl_AppendStringsToObj(resultPtr, "couldn't get file spec", (char *) NULL);
			return TCL_ERROR;
		}
	}
    Tcl_DStringFree(&ds);
	
    /*
     * Get permissions for the file. We really only understand
     * read-only and shared-read-write. If no permissions are 
     * given, we default to read only.
     */
    
    if (permSpecified) {
		stringPtr = Tcl_GetStringFromObj(objv[objc-1], &length);
		mode = TclGetOpenMode(interp, stringPtr, &index);
		if (mode == -1) {
			/* TODO: TclGetOpenMode doesn't work with Obj commands. */
			return TCL_ERROR;
		}
		switch (mode & (O_RDONLY | O_WRONLY | O_RDWR)) {
			case O_RDONLY:
			macPermision = fsRdPerm;
			break;
			
			case O_WRONLY:
			case O_RDWR:
			macPermision = fsRdWrShPerm;
			break;
			
			default:
			panic("Tcl_ResourceObjCmd: invalid permission value");
			break;
		}
    } else {
		macPermision = fsRdPerm;
    }
    
    /* 
     * If path was invalid, don't even bother trying to open a resource map
     */
    if (err == fnfErr) {
		fileRef = -1;
		goto openforkDone;
    }

    /*
     * Don't load in any of the resources in the file, this could 
     * cause problems if you open a file that has CODE resources...
     * So, the opening functions below are enclosed in 
     * SetResLoad(false)/SetResLoad(true) statements
     * 
     * The algorithm as to which fork to look into is like this:
     * - if we have from_rezfork or from_datafork, then only the 
     *   corresponding fork is searched
     * - if it is from_anyfork, then we first look for resources in the 
     *   data fork and, if this fails, we look for resources in the 
     *   resource fork
     *   
     * If the functions opening the resource map fail, we handle it later 
     * depending on the value given to the "permissions" option.
     */
    if (fromFork != from_rezfork) {
		/* 
		 * Try to open the file as a datafork resource file
		 */
		short refnum;
		SetResLoad(false); 
		err = FSOpenResourceFile( &fileFSRef, 0, nil, macPermision, &refnum );
		SetResLoad(true); 
		fileRef = (long) refnum;
		if (err == noErr) {
			foundFork = from_datafork;
			goto openforkDone;
		} 
    } 
    if (fromFork != from_datafork) {
		/* 
		 * Now try to open as a resourcefork resource file
		 */
		SetResLoad(false); 
		fileRef = (long) FSpOpenResFile( &fileSpec, macPermision);
		SetResLoad(true); 
		err = ResError();
		if (err == noErr) {
			foundFork = from_rezfork;
		} 
    } 

    openforkDone:
    if (fileRef == -1) {
		if (((err == fnfErr) || (err == eofErr)) &&
			(macPermision == fsRdWrShPerm)) {
			/*
			 * No resources existed for this file in the specified fork. Since we are
			 * opening it for writing, we will create the resource fork now.
			 */
			switch (fromFork) {
				case from_rezfork:
				HCreateResFile(fileSpec.vRefNum, fileSpec.parID, fileSpec.name);
				fileRef = (long) FSpOpenResFile(&fileSpec, macPermision);
				break;
				
				default: {
					FSSpec 				parentSpec;
					CONST Tcl_UniChar *	uniString;
					int 				numChars;
					
					if (!gotParentRef) {
						/* Get FSRef of parent */
						CInfoPBRec	pb;
						Str255		dirName;
						
						pb.dirInfo.ioNamePtr = dirName;
						pb.dirInfo.ioVRefNum = fileSpec.vRefNum;
						pb.dirInfo.ioDrParID = fileSpec.parID;
						pb.dirInfo.ioFDirIndex = -1;	/* Info about directory */
						if ( pb.dirInfo.ioDrDirID != fsRtDirID ) {
							pb.dirInfo.ioDrDirID = pb.dirInfo.ioDrParID;
							err = PBGetCatInfo( &pb, false);
							if ( err == noErr ) {
								BlockMoveData(dirName, parentSpec.name, dirName[0]+1);
								parentSpec.vRefNum = fileSpec.vRefNum;
								parentSpec.parID = pb.dirInfo.ioDrParID;
							}
						}
						err = FSpMakeFSRef( &parentSpec, &parentFSRef );
						if (err != noErr) {
							Tcl_AppendStringsToObj(resultPtr, 
										   "couldn't get parent's ref", (char *) NULL);
							return TCL_ERROR;
						} 
					} 
					/* Get Unicode name */
					Tcl_DStringInit(&buffer);
					Tcl_ExternalToUtfDString(NULL, (CONST char *) fileSpec.name + 1, fileSpec.name[0], &buffer);

					Tcl_DStringInit(&ds);
					uniString = Tcl_UtfToUniCharDString(Tcl_DStringValue(&buffer), Tcl_DStringLength(&buffer), &ds);
					numChars = Tcl_DStringLength(&ds) / sizeof(Tcl_UniChar);
					Tcl_DStringFree(&buffer);
					Tcl_DStringFree(&ds);

					err = FSCreateResourceFile(&parentFSRef, numChars, uniString, kFSCatInfoNone, 
									 NULL, 0, NULL, &fileFSRef, &fileSpec);
					err = FSOpenResourceFile( &fileFSRef, 0, NULL, macPermision, &fileRef );
					break;
				}
			}
			if (fileRef == -1) {
				goto openError;
			} else {
				foundFork = fromFork;
			}
		} else if (err == fnfErr) {
			Tcl_AppendStringsToObj(resultPtr,
			"file does not exist", (char *) NULL);
			return TCL_ERROR;
		} else if (err == eofErr) {
			switch (fromFork) {
				case from_rezfork:
				Tcl_AppendStringsToObj(resultPtr,
					"file does not contain resources in the resource fork", (char *) NULL);
				break;
				
				case from_datafork:
				Tcl_AppendStringsToObj(resultPtr,
					"file does not contain resources in the data fork", (char *) NULL);
				break;
				
				default: {
				Tcl_AppendStringsToObj(resultPtr,
					"file does not contain resources in any fork", (char *) NULL);
				break;
				}
			}
			return TCL_ERROR;
		} else {
			openError:
			Tcl_AppendStringsToObj(resultPtr,
			"error opening resource file", (char *) NULL);
			return TCL_ERROR;
		}
    }

    /*
     * The FspOpenResFile function does not set the ResFileAttrs.
     * Even if you open the file read only, the mapReadOnly
     * attribute is not set.  This means we can't detect writes to a 
     * read only resource fork until the write fails, which is bogus.  
     * So set it here...
     */
    
    if (macPermision == fsRdPerm) {
		SetResFileAttrs(fileRef, mapReadOnly);
    }
    
    Tcl_SetStringObj(resultPtr, "", 0);
    if (TclMacRegisterResourceFork(fileRef, resultPtr, foundFork,
	    TCL_RESOURCE_CHECK_IF_OPEN) != TCL_OK) {
		CloseResFile(fileRef);
		return TCL_ERROR;
    }
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * Tcl_ResourceReadObjCmd --
 *
 *	This procedure is invoked to process the "resource read" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_ResourceReadObjCmd(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,			/* Current interpreter. */
    int objc,				/* Number of arguments. */
    Tcl_Obj *CONST objv[],		/* Argument values. */
    Tcl_Obj *resultPtr)			/* Pointer to store the result. */
{
    long rsrcId, size;
    int length, releaseIt = 0;
    char *stringPtr;
    char *resourceId = NULL;	
    Handle resource = NULL;
    OSType rezType;

	    if (!((objc == 4) || (objc == 5))) {
		Tcl_WrongNumArgs(interp, 2, objv,
			"resourceType resourceId ?resourceRef?");
		return TCL_ERROR;
	    }

	    if (Tcl_GetOSTypeFromObj(interp, objv[2], &rezType) != TCL_OK) {
		return TCL_ERROR;
	    }
	    
	    if (Tcl_GetLongFromObj((Tcl_Interp *) NULL, objv[3], &rsrcId)
		    != TCL_OK) {
		resourceId = Tcl_GetStringFromObj(objv[3], &length);
            }

	    if (objc == 5) {
		stringPtr = Tcl_GetStringFromObj(objv[4], &length);
	    } else {
		stringPtr = NULL;
	    }
	
	    resource = Tcl_MacFindResource(interp, rezType, resourceId,
		rsrcId, stringPtr, &releaseIt);
			    
	    if (resource != NULL) {
		size = GetResourceSizeOnDisk(resource);
		Tcl_SetByteArrayObj(resultPtr, (unsigned char *) *resource, size);

		/*
		 * Don't release the resource unless WE loaded it...
		 */
		 
		if (releaseIt) {
		    ReleaseResource(resource);
		}
		return TCL_OK;
	    } else {
		Tcl_AppendStringsToObj(resultPtr, "could not load resource",
		    (char *) NULL);
		return TCL_ERROR;
	    }
}


/*
 *----------------------------------------------------------------------
 *
 * Tcl_ResourceTypesObjCmd --
 *
 *	This procedure is invoked to process the "resource types" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_ResourceTypesObjCmd(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,			/* Current interpreter. */
    int objc,				/* Number of arguments. */
    Tcl_Obj *CONST objv[],		/* Argument values. */
    Tcl_Obj *resultPtr)			/* Pointer to store the result. */
{
    Tcl_Obj *objPtr;
    int result, count, i, limitSearch;
    short saveRef;
    OpenResourceFork *resourceRef;
    OSType rezType;

    result = TCL_OK;
    limitSearch = false;

	    if (!((objc == 2) || (objc == 3))) {
		Tcl_WrongNumArgs(interp, 2, objv, "?resourceRef?");
		return TCL_ERROR;
	    }

	    if (objc == 3) {
	        resourceRef = GetResourceRefFromObj(objv[2], 1, 
		                "get types of", resultPtr);
		if (resourceRef == NULL) {
		    return TCL_ERROR;
		}
			
		saveRef = CurResFile();
		UseResFile((short) resourceRef->fileRef);
		limitSearch = true;
	    }

	    if (limitSearch) {
		count = Count1Types();
	    } else {
		count = CountTypes();
	    }
	    for (i = 1; i <= count; i++) {
		if (limitSearch) {
		    Get1IndType((ResType *) &rezType, i);
		} else {
		    GetIndType((ResType *) &rezType, i);
		}
		objPtr = Tcl_NewOSTypeObj(rezType);
		result = Tcl_ListObjAppendElement(interp, resultPtr, objPtr);
		if (result != TCL_OK) {
		    Tcl_DecrRefCount(objPtr);
		    break;
		}
	    }
		
	    if (limitSearch) {
		UseResFile(saveRef);
	    }
		
	    return result;
}


/*
 *----------------------------------------------------------------------
 *
 * Tcl_ResourceUpdateObjCmd --
 *
 *	This procedure is invoked to process the "resource update" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_ResourceUpdateObjCmd(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,			/* Current interpreter. */
    int objc,				/* Number of arguments. */
    Tcl_Obj *CONST objv[],		/* Argument values. */
    Tcl_Obj *resultPtr)			/* Pointer to store the result. */
{
    OpenResourceFork *resourceRef;
    char buffer[16];
    OSErr err;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "resourceRef");
	return TCL_ERROR;
    }

    resourceRef = GetResourceRefFromObj(objv[2], true, 
		    "update", resultPtr);
    if (resourceRef == NULL) {
	return TCL_ERROR;
    }	
    
    if (resourceRef->fileRef >= 0) {
	UpdateResFile(resourceRef->fileRef);
	err = ResError();
	if (err != noErr) {
	    sprintf(buffer, "%12d", err);
	    Tcl_AppendStringsToObj(resultPtr,
		    "error ", buffer, " updating resource map",
		    (char *) NULL);
	    return TCL_ERROR;
	} 
	return TCL_OK;
    } else {
	    Tcl_AppendStringsToObj(resultPtr, "invalid file ref", (char *) NULL);
	return TCL_ERROR;
    }
}


/*
 *----------------------------------------------------------------------
 *
 * Tcl_ResourceWriteObjCmd --
 *
 *	This procedure is invoked to process the "resource write" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_ResourceWriteObjCmd(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,			/* Current interpreter. */
    int objc,				/* Number of arguments. */
    Tcl_Obj *CONST objv[],		/* Argument values. */
    Tcl_Obj *resultPtr)			/* Pointer to store the result. */
{
    int index, result, gotInt, releaseIt = 0, force;
    int i, limitSearch, length;
    long rsrcId;
    short saveRef;
    char *stringPtr;
    char *resourceId = NULL;	
    char errbuf[16];
    OpenResourceFork *resourceRef;
    Handle resource = NULL;
    OSErr err;
    Str255 theName;
    OSType rezType;

	static CONST char *writeSwitches[] = {
		"-id", "-name", "-file", "-force", "-datafork", (char *) NULL
	};
	
	enum {
		RESOURCE_WRITE_ID, RESOURCE_WRITE_NAME, 
		RESOURCE_WRITE_FILE, RESOURCE_FORCE, RESOURCE_WRITE_DATAFORK
	};
	
	result = TCL_OK;
	limitSearch = false;
	
	if ((objc < 4) || (objc > 11)) {
		Tcl_WrongNumArgs(interp, 2, objv, 
						 "?-id resourceId? ?-name resourceName? ?-file resourceRef? ?-force? resourceType data");
		return TCL_ERROR;
	}
	
	i = 2;
	gotInt = false;
	resourceId = NULL;
	limitSearch = false;
	force = 0;

	while (i < (objc - 2)) {
		if (Tcl_GetIndexFromObj(interp, objv[i], writeSwitches,
								"switch", 0, &index) != TCL_OK) {
			return TCL_ERROR;
		}
		
		switch (index) {
			case RESOURCE_WRITE_ID:		
			if (Tcl_GetLongFromObj(interp, objv[i+1], &rsrcId)
				!= TCL_OK) {
				return TCL_ERROR;
			}
			gotInt = true;
			i += 2;
			break;
			
			case RESOURCE_WRITE_NAME:		
			resourceId = Tcl_GetStringFromObj(objv[i+1], &length);
			strcpy((char *) theName, resourceId);
			resourceId = (char *) theName;
			c2pstr(resourceId);
			i += 2;
			break;
			
			case RESOURCE_WRITE_FILE:		
			resourceRef = GetResourceRefFromObj(objv[i+1], 0, 
												"write to", resultPtr);
			if (resourceRef == NULL) {
				return TCL_ERROR;
			}	
			limitSearch = true;
			i += 2;
			break;
			
			case RESOURCE_FORCE:
			force = 1;
			i += 1;
			break;
		}
	}
	if (Tcl_GetOSTypeFromObj(interp, objv[i], &rezType) != TCL_OK) {
		return TCL_ERROR;
	}
	stringPtr = (char *) Tcl_GetByteArrayFromObj(objv[i+1], &length);
	
	if (resourceId == NULL) {
		resourceId = (char *) "\p";
	}
	if (limitSearch) {
		saveRef = CurResFile();
		UseResFile((short) resourceRef->fileRef);
	}
	if (gotInt == false) {
		if (limitSearch) {
			rsrcId = Unique1ID(rezType);
		} else {
			rsrcId = UniqueID(rezType);
		}
	}
	
	/*
	 * If we are adding the resource by number, then we must make sure
	 * there is not already a resource of that number.  We are not going
	 * load it here, since we want to detect whether we loaded it or
	 * not.  Remember that releasing some resources in particular menu
	 * related ones, can be fatal.
	 */
	 
	if (gotInt == true) {
		SetResLoad(false);
		resource = Get1Resource(rezType,rsrcId);
		SetResLoad(true);
	}     
			
	if (resource == NULL) {
		/*
		 * We get into this branch either if there was not already a
		 * resource of this type & id, or the id was not specified.
		 */
		
		resource = NewHandle(length);
		if (resource == NULL) {
			resource = NewHandle(length);
			if (resource == NULL) {
				panic("could not allocate memory to write resource");
			}
		}
		HLock(resource);
		memcpy(*resource, stringPtr, length);
		HUnlock(resource);
		AddResource(resource, rezType, (short) rsrcId, (StringPtr) resourceId);
		releaseIt = 1;
	} else {
		/* 
		 * We got here because there was a resource of this type 
		 * & ID in the file. 
		 */ 
		
		if (*resource == NULL) {
			releaseIt = 1;
		} else {
			releaseIt = 0;
		}
		   
		if (!force) {
			/*
			 * We only overwrite existant resources
			 * when the -force flag has been set.
			 */
			 
			sprintf(errbuf,"%d", rsrcId);
		  
			Tcl_AppendStringsToObj(resultPtr, "the resource ",
				  errbuf, " already exists, use \"-force\"",
				  " to overwrite it.", (char *) NULL);
			
			result = TCL_ERROR;
			goto writeDone;
		} else if (GetResAttrs(resource) & resProtected) {
			/*  
			 * Next, check to see if it is protected...
			 */
			
			sprintf(errbuf,"%d", rsrcId);
			Tcl_AppendStringsToObj(resultPtr,
								   "could not write resource id ",
								   errbuf, " of type ",
								   Tcl_GetStringFromObj(objv[i],&length),
								   ", it was protected.",(char *) NULL);
								   result = TCL_ERROR;
								   goto writeDone;
	   } else {
			/*
			 * Be careful, the resource might already be in memory
			 * if something else loaded it.
			 */
			 
			if (*resource == 0) {
				LoadResource(resource);
				err = ResError();
				if (err != noErr) {
					sprintf(errbuf,"%d", rsrcId);
					Tcl_AppendStringsToObj(resultPtr,
										   "error loading resource ",
										   errbuf, " of type ",
										   Tcl_GetStringFromObj(objv[i],&length),
										   " to overwrite it", (char *) NULL);
				   goto writeDone;
			   }
		   }
 
			SetHandleSize(resource, length);
			if ( MemError() != noErr ) {
				panic("could not allocate memory to write resource");
			}

			HLock(resource);
			memcpy(*resource, stringPtr, length);
			HUnlock(resource);
	   
			ChangedResource(resource);
		
			/*
			 * We also may have changed the name...
			 */ 
		 
			SetResInfo(resource, rsrcId, (StringPtr) resourceId);
		}
	}
	
	err = ResError();
	if (err != noErr) {
		Tcl_AppendStringsToObj(resultPtr,
			"error adding resource to resource map",
				(char *) NULL);
		result = TCL_ERROR;
		goto writeDone;
	}
	
	WriteResource(resource);
	err = ResError();
	if (err != noErr) {
		Tcl_AppendStringsToObj(resultPtr,
			"error writing resource to disk",
				(char *) NULL);
		result = TCL_ERROR;
	}
	
	writeDone:
	if (releaseIt) {
		ReleaseResource(resource);
		err = ResError();
		if (err != noErr) {
			Tcl_AppendStringsToObj(resultPtr,
				"error releasing resource",
					(char *) NULL);
			result = TCL_ERROR;
		}
	}
	
	if (limitSearch) {
		UseResFile(saveRef);
	}

	return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Tcl_MacFindResource --
 *
 *	Higher level interface for loading resources.
 *
 * Side Effects:
 *	Attempts to load a resource.
 *
 * Results:
 *      A handle on success.
 *
 *-----------------------------------------------------------------------------
 */

Handle
Tcl_MacFindResource(
    Tcl_Interp *interp,		/* Interpreter in which to process file. */
    long resourceType,		/* Type of resource to load. */
    CONST char *resourceName,	/* Name of resource to find,
				 * NULL if number should be used. */
    int resourceNumber,		/* Resource id of source. */
    CONST char *resFileRef,	/* Registered resource file reference,
				 * NULL if searching all open resource files. */
    int *releaseIt)	        /* Should we release this resource when done. */
{
    Tcl_HashEntry *nameHashPtr;
    OpenResourceFork *resourceRef;
    int limitSearch = false;
    short saveRef;
    Handle resource;

    if (resFileRef != NULL) {
	nameHashPtr = Tcl_FindHashEntry(&nameTable, resFileRef);
	if (nameHashPtr == NULL) {
	    Tcl_AppendResult(interp, "invalid resource file reference \"",
			     resFileRef, "\"", (char *) NULL);
	    return NULL;
	}
	resourceRef = (OpenResourceFork *) Tcl_GetHashValue(nameHashPtr);
	saveRef = CurResFile();
	UseResFile((short) resourceRef->fileRef);
	limitSearch = true;
    }

    /* 
     * Some system resources (for example system resources) should not 
     * be released.  So we set autoload to false, and try to get the resource.
     * If the Master Pointer of the returned handle is null, then resource was 
     * not in memory, and it is safe to release it.  Otherwise, it is not.
     */
    
    SetResLoad(false);
	 
    if (resourceName == NULL) {
	if (limitSearch) {
	    resource = Get1Resource(resourceType, resourceNumber);
	} else {
	    resource = GetResource(resourceType, resourceNumber);
	}
    } else {
    	Str255 rezName;
	Tcl_DString ds;
	Tcl_UtfToExternalDString(NULL, resourceName, -1, &ds);
	strcpy((char *) rezName + 1, Tcl_DStringValue(&ds));
	rezName[0] = (unsigned) Tcl_DStringLength(&ds);
	if (limitSearch) {
	    resource = Get1NamedResource(resourceType,
		    rezName);
	} else {
	    resource = GetNamedResource(resourceType,
		    rezName);
	}
	Tcl_DStringFree(&ds);
    }
    
    if (resource != NULL && *resource == NULL) {
    	*releaseIt = 1;
    	LoadResource(resource);
    } else {
    	*releaseIt = 0;
    }
    
    SetResLoad(true);
    	

    if (limitSearch) {
	UseResFile(saveRef);
    }

    return resource;
}

/*
 *----------------------------------------------------------------------
 *
 * ResourceInitialize --
 *
 *	Initialize the structures used for resource management.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Read the code.
 *
 *----------------------------------------------------------------------
 */

static void
ResourceInitialize()
{
    initialized = 1;
    Tcl_InitHashTable(&nameTable, TCL_STRING_KEYS);
    Tcl_InitHashTable(&resourceTable, TCL_ONE_WORD_KEYS);
    resourceForkList = Tcl_NewObj();
    Tcl_IncrRefCount(resourceForkList);

    BuildResourceForkList();
}


/*
 *----------------------------------------------------------------------
 *
 * Tcl_NewOSTypeObj --
 *
 *	This procedure is used to create a new resource name type object.
 *
 * Results:
 *	The newly created object is returned. This object will have a NULL
 *	string representation. The returned object has ref count 0.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
Tcl_NewOSTypeObj(
    OSType newOSType)		/* Int used to initialize the new object. */
{
    register Tcl_Obj *objPtr;

    if (!osTypeInit) {
	osTypeInit = 1;
	Tcl_RegisterObjType(&osType);
    }

    objPtr = Tcl_NewObj();
    objPtr->bytes = NULL;
    objPtr->internalRep.longValue = newOSType;
    objPtr->typePtr = &osType;
    return objPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetOSTypeObj --
 *
 *	Modify an object to be a resource type and to have the 
 *	specified long value.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The object's old string rep, if any, is freed. Also, any old
 *	internal rep is freed. 
 *
 *----------------------------------------------------------------------
 */

void
Tcl_SetOSTypeObj(
    Tcl_Obj *objPtr,		/* Object whose internal rep to init. */
    OSType newOSType)		/* Integer used to set object's value. */
{
    register Tcl_ObjType *oldTypePtr = objPtr->typePtr;

    if (!osTypeInit) {
	osTypeInit = 1;
	Tcl_RegisterObjType(&osType);
    }

    if ((oldTypePtr != NULL) && (oldTypePtr->freeIntRepProc != NULL)) {
	oldTypePtr->freeIntRepProc(objPtr);
    }
    
    objPtr->internalRep.longValue = newOSType;
    objPtr->typePtr = &osType;

    Tcl_InvalidateStringRep(objPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetOSTypeFromObj --
 *
 *	Attempt to return an int from the Tcl object "objPtr". If the object
 *	is not already an int, an attempt will be made to convert it to one.
 *
 * Results:
 *	The return value is a standard Tcl object result. If an error occurs
 *	during conversion, an error message is left in interp->objResult
 *	unless "interp" is NULL.
 *
 * Side effects:
 *	If the object is not already an int, the conversion will free
 *	any old internal representation.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_GetOSTypeFromObj(
    Tcl_Interp *interp, 	/* Used for error reporting if not NULL. */
    Tcl_Obj *objPtr,		/* The object from which to get a int. */
    OSType *osTypePtr)		/* Place to store resulting int. */
{
    register int result;
    
    if (!osTypeInit) {
	osTypeInit = 1;
	Tcl_RegisterObjType(&osType);
    }

    if (objPtr->typePtr == &osType) {
	*osTypePtr = objPtr->internalRep.longValue;
	return TCL_OK;
    }

    result = SetOSTypeFromAny(interp, objPtr);
    if (result == TCL_OK) {
	*osTypePtr = objPtr->internalRep.longValue;
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * DupOSTypeInternalRep --
 *
 *	Initialize the internal representation of an int Tcl_Obj to a
 *	copy of the internal representation of an existing int object. 
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	"copyPtr"s internal rep is set to the integer corresponding to
 *	"srcPtr"s internal rep.
 *
 *----------------------------------------------------------------------
 */

static void
DupOSTypeInternalRep(
    Tcl_Obj *srcPtr,	/* Object with internal rep to copy. */
    Tcl_Obj *copyPtr)	/* Object with internal rep to set. */
{
    copyPtr->internalRep.longValue = srcPtr->internalRep.longValue;
    copyPtr->typePtr = &osType;
}

/*
 *----------------------------------------------------------------------
 *
 * SetOSTypeFromAny --
 *
 *	Attempt to generate an integer internal form for the Tcl object
 *	"objPtr".
 *
 * Results:
 *	The return value is a standard object Tcl result. If an error occurs
 *	during conversion, an error message is left in interp->objResult
 *	unless "interp" is NULL.
 *
 * Side effects:
 *	If no error occurs, an int is stored as "objPtr"s internal
 *	representation. 
 *
 *----------------------------------------------------------------------
 */

static int
SetOSTypeFromAny(
    Tcl_Interp *interp,		/* Used for error reporting if not NULL. */
    Tcl_Obj *objPtr)		/* The object to convert. */
{
    Tcl_ObjType *oldTypePtr = objPtr->typePtr;
    char *string;
    int length;
    OSType newOSType = 0UL;
    Tcl_DString ds;

    /*
     * Get the string representation. Make it up-to-date if necessary.
     */

    string = Tcl_GetStringFromObj(objPtr, &length);
    Tcl_UtfToExternalDString(NULL, string, length, &ds);

    if (Tcl_DStringLength(&ds) > sizeof(OSType)) {
	if (interp != NULL) {
	    Tcl_ResetResult(interp);
	    Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
		    "expected Macintosh OS type but got \"", string, "\"",
		    (char *) NULL);
	}
	Tcl_DStringFree(&ds);
	return TCL_ERROR;
    }
    memcpy(&newOSType, Tcl_DStringValue(&ds),
            (size_t) Tcl_DStringLength(&ds));
    Tcl_DStringFree(&ds);
    
    /*
     * The conversion to resource type succeeded. Free the old internalRep 
     * before setting the new one.
     */

    if ((oldTypePtr != NULL) &&	(oldTypePtr->freeIntRepProc != NULL)) {
	oldTypePtr->freeIntRepProc(objPtr);
    }
    
    objPtr->internalRep.longValue = newOSType;
    objPtr->typePtr = &osType;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * UpdateStringOfOSType --
 *
 *	Update the string representation for an resource type object.
 *	Note: This procedure does not free an existing old string rep
 *	so storage will be lost if this has not already been done. 
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The object's string is set to a valid string that results from
 *	the int-to-string conversion.
 *
 *----------------------------------------------------------------------
 */

static void
UpdateStringOfOSType(
    register Tcl_Obj *objPtr)	/* Int object whose string rep to update. */
{
    char string[sizeof(OSType)+1];
    Tcl_DString ds;
    
    memcpy(string, &(objPtr->internalRep.longValue), sizeof(OSType));
    string[sizeof(OSType)] = '\0';
    Tcl_ExternalToUtfDString(NULL, string, -1, &ds);
    objPtr->bytes = ckalloc(Tcl_DStringLength(&ds) + 1);
    memcpy(objPtr->bytes, Tcl_DStringValue(&ds), Tcl_DStringLength(&ds) + 1);
    objPtr->length = Tcl_DStringLength(&ds);
    Tcl_DStringFree(&ds);
}

/*
 *----------------------------------------------------------------------
 *
 * GetResourceRefFromObj --
 *
 *	Given a String object containing a resource file token, return
 *	the OpenResourceFork structure that it represents, or NULL if 
 *	the token cannot be found.  If okayOnReadOnly is false, it will 
 *      also check whether the token corresponds to a read-only file, 
 *      and return NULL if it is.
 *
 * Results:
 *	A pointer to an OpenResourceFork structure, or NULL.
 *
 * Side effects:
 *	An error message may be left in resultPtr.
 *
 *----------------------------------------------------------------------
 */

static OpenResourceFork *
GetResourceRefFromObj(
    register Tcl_Obj *objPtr,	/* String obj containing file token     */
    int okayOnReadOnly,         /* Whether this operation is okay for a *
                                 * read only file.                      */
    const char *operation,      /* String containing the operation we   *
                                 * were trying to perform, used for errors */
    Tcl_Obj *resultPtr)         /* Tcl_Obj to contain error message     */
{
    char *stringPtr;
    Tcl_HashEntry *nameHashPtr;
    OpenResourceFork *resourceRef;
    int length;
    OSErr err;
    
    stringPtr = Tcl_GetStringFromObj(objPtr, &length);
    nameHashPtr = Tcl_FindHashEntry(&nameTable, stringPtr);
    if (nameHashPtr == NULL) {
        Tcl_AppendStringsToObj(resultPtr,
	        "invalid resource file reference \"",
	        stringPtr, "\"", (char *) NULL);
        return NULL;
    }

    resourceRef = (OpenResourceFork *) Tcl_GetHashValue(nameHashPtr);
    
    if (!okayOnReadOnly) {
        err = GetResFileAttrs((short) resourceRef->fileRef);
        if (err & mapReadOnly) {
            Tcl_AppendStringsToObj(resultPtr, "cannot ", operation, 
                    " resource file \"",
                    stringPtr, "\", it was opened read only",
                    (char *) NULL);
            return NULL;
        }
    }
    return resourceRef;
}

/*
 *----------------------------------------------------------------------
 *
 * TclMacRegisterResourceFork --
 *
 *	Register an open resource fork in the table of open resources 
 *	managed by the procedures in this file.  If the resource file
 *      is already registered with the table, then no new token is made.
 *
 *      The behavior is controlled by the value of tokenPtr, and of the 
 *	flags variable.  For tokenPtr, the possibilities are:
 *	  - NULL: The new token is auto-generated, but not returned.
 *        - The string value of tokenPtr is the empty string: Then
 *		the new token is auto-generated, and returned in tokenPtr
 *	  - tokenPtr has a value: The string value will be used for the token,
 *		unless it is already in use, in which case a new token will
 *		be generated, and returned in tokenPtr.
 *
 *      For the flags variable:  it can be one of:
 *	  - TCL_RESOURCE__INSERT_TAIL: The element is inserted at the
 *              end of the list of open resources.  Used only in Resource_Init.
 *	  - TCL_RESOURCE_DONT_CLOSE: The resource close command will not close
 *	        this resource.
 *	  - TCL_RESOURCE_CHECK_IF_OPEN: This will check to see if this file's
 *	        resource fork is already opened by this Tcl shell, and return 
 *	        an error without registering the resource fork.
 *
 * Results:
 *	Standard Tcl Result
 *
 * Side effects:
 *	An entry may be added to the resource name table.
 *
 *----------------------------------------------------------------------
 */

int
TclMacRegisterResourceFork(
    short fileRef,        	/* File ref for an open resource fork. */
    Tcl_Obj *tokenPtr,		/* A Tcl Object to which to write the  *
				 * new token */
    int whichFork, /* The fork in which the resource map has been found */
    int flags)	     		/* 1 means insert at the head of the resource
                                 * fork list, 0 means at the tail */

{
    Tcl_HashEntry *resourceHashPtr;
    Tcl_HashEntry *nameHashPtr;
    OpenResourceFork *resourceRef;
    int new;
    char *resourceId = NULL;
   
    if (!initialized) {
        ResourceInitialize();
    }
    
    /*
     * If we were asked to, check that this file has not been opened
     * already with a different permission.  It it has, then return an error.
     */
     
    new = 1;
    
    if (flags & TCL_RESOURCE_CHECK_IF_OPEN) {
        Tcl_HashSearch search;
        short oldFileRef, filePermissionFlag;
        FCBPBRec newFileRec, oldFileRec;
        OSErr err;
        
        oldFileRec.ioCompletion = NULL;
        oldFileRec.ioFCBIndx = 0;
        oldFileRec.ioNamePtr = NULL;
        
        newFileRec.ioCompletion = NULL;
        newFileRec.ioFCBIndx = 0;
        newFileRec.ioNamePtr = NULL;
        newFileRec.ioVRefNum = 0;
        newFileRec.ioRefNum = fileRef;
        err = PBGetFCBInfo(&newFileRec, false);
        filePermissionFlag = ( newFileRec.ioFCBFlags >> 12 ) & 0x1;
            
        
        resourceHashPtr = Tcl_FirstHashEntry(&resourceTable, &search);
        while (resourceHashPtr != NULL) {
            oldFileRef = (short) Tcl_GetHashKey(&resourceTable,
                    resourceHashPtr);
            if (oldFileRef == fileRef) {
                new = 0;
                break;
            }
            oldFileRec.ioVRefNum = 0;
            oldFileRec.ioRefNum = oldFileRef;
            err = PBGetFCBInfo(&oldFileRec, false);
            
            /*
             * err might not be noErr either because the file has closed 
             * out from under us somehow, which is bad but we're not going
             * to fix it here, OR because it is the ROM MAP, which has a 
             * fileRef, but can't be gotten to by PBGetFCBInfo.
             */
            if ((err == noErr) 
                    && (newFileRec.ioFCBVRefNum == oldFileRec.ioFCBVRefNum)
                    && (newFileRec.ioFCBFlNm == oldFileRec.ioFCBFlNm)) {
                /*
				 * In MacOS 8.1 it seems like we get different file refs even
                 * though we pass the same file & permissions.  This is not
                 * what Inside Mac says should happen, but it does, so if it
                 * does, then close the new res file and return the original
                 * one...
				 */
                 
                if (filePermissionFlag == ((oldFileRec.ioFCBFlags >> 12) & 0x1)) {
                    CloseResFile(fileRef);
                    new = 0;
                    break;
                } else {
                    if (tokenPtr != NULL) {
                        Tcl_SetStringObj(tokenPtr, "Resource already open with different permissions.", -1);
                    }   	
                    return TCL_ERROR;
                }
            }
            resourceHashPtr = Tcl_NextHashEntry(&search);
        }
    }
       
    
    /*
     * If the file has already been opened with these same permissions, then it
     * will be in our list and we will have set new to 0 above.
     * So we will just return the token (if tokenPtr is non-null)
     */
     
    if (new) {
        resourceHashPtr = Tcl_CreateHashEntry(&resourceTable,
		(char *) fileRef, &new);
    }
    
    if (!new) {
        if (tokenPtr != NULL) {   
            resourceId = (char *) Tcl_GetHashValue(resourceHashPtr);
	    Tcl_SetStringObj(tokenPtr, resourceId, -1);
        }
        return TCL_OK;
    }        

    /*
     * If we were passed in a result pointer which is not an empty
     * string, attempt to use that as the key.  If the key already
     * exists, silently fall back on resource%d...
     */
     
    if (tokenPtr != NULL) {
        char *tokenVal;
        int length;
        tokenVal = Tcl_GetStringFromObj(tokenPtr, &length);
        if (length > 0) {
            nameHashPtr = Tcl_FindHashEntry(&nameTable, tokenVal);
            if (nameHashPtr == NULL) {
                resourceId = ckalloc(length + 1);
                memcpy(resourceId, tokenVal, length);
                resourceId[length] = '\0';
            }
        }
    }
    
    if (resourceId == NULL) {	
        resourceId = (char *) ckalloc(15);
        sprintf(resourceId, "resource%d", newId);
    }
    
    Tcl_SetHashValue(resourceHashPtr, resourceId);
    newId++;

    nameHashPtr = Tcl_CreateHashEntry(&nameTable, resourceId, &new);
    if (!new) {
	panic("resource id has repeated itself");
    }
    
    resourceRef = (OpenResourceFork *) ckalloc(sizeof(OpenResourceFork));
    resourceRef->fileRef = fileRef;
    resourceRef->fileFork = whichFork;
    resourceRef->flags = flags;
    
    Tcl_SetHashValue(nameHashPtr, (ClientData) resourceRef);
    if (tokenPtr != NULL) {
        Tcl_SetStringObj(tokenPtr, resourceId, -1);
    }
    
    if (flags & TCL_RESOURCE_INSERT_TAIL) {
        Tcl_ListObjAppendElement(NULL, resourceForkList, tokenPtr);
    } else {
        Tcl_ListObjReplace(NULL, resourceForkList, 0, 0, 1, &tokenPtr);	
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclMacUnRegisterResourceFork --
 *
 *	Removes the entry for an open resource fork from the table of 
 *	open resources managed by the procedures in this file.
 *      If resultPtr is not NULL, it will be used for error reporting.
 *
 * Results:
 *	The fileRef for this token, or -1 if an error occured.
 *
 * Side effects:
 *	An entry is removed from the resource name table.
 *
 *----------------------------------------------------------------------
 */

short
TclMacUnRegisterResourceFork(
    char *tokenPtr,
    Tcl_Obj *resultPtr)

{
    Tcl_HashEntry *resourceHashPtr;
    Tcl_HashEntry *nameHashPtr;
    OpenResourceFork *resourceRef;
    short fileRef;
    char *bytes;
    int i, match, index, listLen, length, elemLen;
    Tcl_Obj **elemPtrs;
    
     
    nameHashPtr = Tcl_FindHashEntry(&nameTable, tokenPtr);
    if (nameHashPtr == NULL) {
        if (resultPtr != NULL) {
	    Tcl_AppendStringsToObj(resultPtr,
		    "invalid resource file reference \"",
		    tokenPtr, "\"", (char *) NULL);
        }
	return -1;
    }
    
    resourceRef = (OpenResourceFork *) Tcl_GetHashValue(nameHashPtr);
    fileRef = resourceRef->fileRef;
        
    if ( resourceRef->flags & TCL_RESOURCE_DONT_CLOSE ) {
        if (resultPtr != NULL) {
	    Tcl_AppendStringsToObj(resultPtr,
		    "can't close \"", tokenPtr, "\" resource file", 
		    (char *) NULL);
	}
	return -1;
    }            

    Tcl_DeleteHashEntry(nameHashPtr);
    ckfree((char *) resourceRef);
    
    
    /* 
     * Now remove the resource from the resourceForkList object 
     */
     
    Tcl_ListObjGetElements(NULL, resourceForkList, &listLen, &elemPtrs);
    
 
    index = -1;
    length = strlen(tokenPtr);
    
    for (i = 0; i < listLen; i++) {
	match = 0;
	bytes = Tcl_GetStringFromObj(elemPtrs[i], &elemLen);
	if (length == elemLen) {
		match = (memcmp(bytes, tokenPtr,
			(size_t) length) == 0);
	}
	if (match) {
	    index = i;
	    break;
	}
    }
    if (!match) {
        panic("the resource Fork List is out of synch!");
    }
    
    Tcl_ListObjReplace(NULL, resourceForkList, index, 1, 0, NULL);
    
    resourceHashPtr = Tcl_FindHashEntry(&resourceTable, (char *) fileRef);
    
    if (resourceHashPtr == NULL) {
	panic("Resource & Name tables are out of synch in resource command.");
    }
    ckfree(Tcl_GetHashValue(resourceHashPtr));
    Tcl_DeleteHashEntry(resourceHashPtr);
    
    return fileRef;

}


/*
 *----------------------------------------------------------------------
 *
 * BuildResourceForkList --
 *
 *	Traverses the list of open resource forks, and builds the 
 *	list of resources forks.  Also creates a resource token for any that 
 *      are opened but not registered with our resource system.
 *      This is based on code from Apple DTS.
 *      
 *	This code had to be redefined on OSX because some low-memory 
 *	accessor functions it used in its OS8/9 incarnation are
 *	now obsolete (LMGetTopMapHndl and LMGetSysMapHndl). Using
 *	GetTopResourceFile() and GetNextResourceFile() instead.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *      The list of resource forks is updated.
 *	The resource name table may be augmented.
 *
 *----------------------------------------------------------------------
 */

void
BuildResourceForkList()
{
    FCBPBRec fileRec;
    char fileName[256];
    char appName[256];
	    char * s;
	    
    Tcl_Obj *nameObj;
    OSErr err;
    ProcessSerialNumber psn;
    ProcessInfoRec info;
    FSSpec fileSpec;
    SInt16 curRefNum, nextRefNum;
    
    /* 
     * Get the application name, so we can substitute
     * the token "application" for the application's resource.
     */ 
     
    GetCurrentProcess(&psn);
    info.processInfoLength = sizeof(ProcessInfoRec);
    info.processName = (StringPtr) &appName;
    info.processAppSpec = &fileSpec;
    GetProcessInformation(&psn, &info);
    p2cstr((StringPtr) appName);

    
    fileRec.ioCompletion = NULL;
    fileRec.ioVRefNum = 0;
    fileRec.ioFCBIndx = 0;
    fileRec.ioNamePtr = (StringPtr) &fileName;
    
    err = GetTopResourceFile(&nextRefNum);

    if (err==noErr) {
	while (nextRefNum != NULL) {
	    curRefNum = nextRefNum;
	
	    /* 
	     * Now do the ones opened after the application.
	     */
	   
	    nameObj = Tcl_NewObj();
	    
	    fileRec.ioRefNum = curRefNum;
	    err = PBGetFCBInfo(&fileRec, false);
	    
	    if (err == noErr) {
		p2cstr((StringPtr) fileName);
		// Strip rsrc extension: for bundled applications, the main resource 
		// fork is named after the name of the app followed by this extension.
		s = strrchr(fileName,'.');
		if (s != NULL && strcmp(s+1,"rsrc") == 0) {
		    *s = 0;
		} 
		if (strcmp(fileName,appName) == 0) {
		    Tcl_SetStringObj(nameObj, "application", -1);
		} else {
		    Tcl_SetStringObj(nameObj, fileName, -1);
		}
		c2pstr(fileName);
	    }
	    
	    TclMacRegisterResourceFork(fileRec.ioRefNum, nameObj, 
		from_unspecified, TCL_RESOURCE_DONT_CLOSE | TCL_RESOURCE_INSERT_TAIL);
	   	    
	    GetNextResourceFile(curRefNum, &nextRefNum);
	}
    } 
}

