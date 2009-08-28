// -------------------------------------------------------
// File: "tclResource.c"
//                        Created: 2003-09-20 10:13:07
//              Last modification: 2006-01-05 09:23:29
// Author: Bernard Desgraupes
// e-mail: <bdesgraupes@users.sourceforge.net>
// (c) Copyright : Bernard Desgraupes, 2003-2006
// All rights reserved.
// This software is free software with BSD licence.
// Versions history: see the Changes.Log file.
// 
// $Date: 2007/08/23 11:04:53 $
// -------------------------------------------------------

#include "tclResource_version.h"

#include <CoreServices/CoreServices.h>
#ifndef TCLRESOURCE_DONT_USE_CARBON
#include <Carbon/Carbon.h>
#endif

#ifdef TCLRESOURCE_USE_FRAMEWORK_INCLUDES
#include <Tcl/tcl.h>
#include <Tcl/tclInt.h>
#else
#include <tcl.h>
#include <tclInt.h>
#endif

#include <fcntl.h>

#define TCLRESOURCE_PATH_SEP '/'

// Hash table to track open resource files.
typedef struct OpenResourceFork {
	short fileRef;
	int   fileFork;
	int   flags;
} OpenResourceFork;

// Flags used by the TclRes_RegisterResourceFork() function. 
// See comments with this function.
enum {	
	fork_InsertTail = 1,
	fork_DontClose = 2,
	fork_CheckIfOpen = 4
};

// Enumerated values to designate the resource fork
enum {	
	from_unspecified = -1,
	from_anyfork = 0,
	from_rezfork,
	from_datafork
};


//  Prototypes for static functions
static int	TclResCmd_Attributes(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[], Tcl_Obj *resultPtr);
static int	TclResCmd_Close(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[], Tcl_Obj *resultPtr);
static int	TclResCmd_Delete(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[], Tcl_Obj *resultPtr);
static int	TclResCmd_Files(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[], Tcl_Obj *resultPtr);
static int	TclResCmd_Fork(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[], Tcl_Obj *resultPtr);
static int	TclResCmd_Id(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[], Tcl_Obj *resultPtr);
static int	TclResCmd_List(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[], Tcl_Obj *resultPtr);
static int	TclResCmd_Name(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[], Tcl_Obj *resultPtr);
static int	TclResCmd_Open(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[], Tcl_Obj *resultPtr);
static int	TclResCmd_Read(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[], Tcl_Obj *resultPtr);
static int	TclResCmd_Types(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[], Tcl_Obj *resultPtr);
static int	TclResCmd_Update(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[], Tcl_Obj *resultPtr);
static int	TclResCmd_Write(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[], Tcl_Obj *resultPtr);

static void	TclRes_BuildResourceForkList(void);
static void	TclRes_UpdateStringOfOSType(Tcl_Obj *objPtr);
static void	TclRes_DupOSTypeInternalRep(Tcl_Obj *srcPtr, Tcl_Obj *copyPtr);
static int	TclRes_SetOSTypeFromAny(Tcl_Interp *interp, Tcl_Obj *objPtr);

// Prototypes moved from /tcl/generic/tclPlatDecls.h
static Handle	 	TclRes_FindResource(Tcl_Interp * interp, 
									long resourceType, 
									CONST char * resourceName, 
									int resourceNumber, 
									CONST char * resFileRef, 
									int * releaseIt);
static OpenResourceFork * TclRes_GetResourceRefFromObj(Tcl_Obj *objPtr, 
									int okayOnReadOnly, 
									const char *operation, 
									Tcl_Obj *resultPtr);
static void			TclRes_InitializeTables(void);
static int 			TclRes_GetOSTypeFromObj(Tcl_Interp * interp, Tcl_Obj * objPtr, OSType * osTypePtr);
static void 		TclRes_SetOSTypeObj(Tcl_Obj * objPtr, OSType osType);
static Tcl_Obj * 	TclRes_NewOSTypeObj(OSType osType);
static int 			TclRes_RegisterResourceFork(short fileRef, Tcl_Obj * tokenPtr, int whichFork, int insert);
static short 		TclRes_UnRegisterResourceFork(char * tokenPtr, Tcl_Obj * resultPtr);


static int			Tcl_ResourceCommand(ClientData clientData,  Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);

// The init function called when the package is loaded in the Tcl interpreter. 
#pragma export on
int Resource_Init(Tcl_Interp *interp);
#pragma export off


// The structure below defines the Tcl object type defined in this file by
// means of procedures that can be invoked by generic object code.
static Tcl_ObjType osType = {
	"ostype",							// name 
	(Tcl_FreeInternalRepProc *) NULL,   // freeIntRepProc 
	TclRes_DupOSTypeInternalRep,		// dupIntRepProc 
	TclRes_UpdateStringOfOSType,		// updateStringProc 
	TclRes_SetOSTypeFromAny				// setFromAnyProc 
};


static Tcl_HashTable nameTable;			// Id to process number mapping. 
static Tcl_HashTable resourceTable;		// Process number to id mapping. 

Tcl_Obj *resourceForkList = NULL;		// Ordered list of resource forks 
int newId = 0;							// Id source. 
int osTypeInit = 0;						// 0 means Tcl object of osType hasn't 
										//     been initialized yet. 
int initialized = 0;					// 0 means static structures haven't 
										//     been initialized yet. 




// ----------------------------------------------------------------------
//
// Resource_Init --
//
//	This procedure is invoked when the package is loaded.
//
// Results:
//	A standard Tcl result.
//
// Side effects:
//	None.
//
// ----------------------------------------------------------------------

int Resource_Init(Tcl_Interp *interp) {
	char vstr[64];
	
#ifdef USE_TCL_STUBS
	if (Tcl_InitStubs(interp, "8.1", 0) == NULL) {
		return TCL_ERROR;
	}
#endif
	
	// Register resource command 
	Tcl_CreateObjCommand(interp, "resource", Tcl_ResourceCommand, (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
	
	// Version numbering
	if (TCLRESOURCE_STAGE=='f') {
		if (TCLRESOURCE_SUBMINOR) {
			sprintf(vstr,"%d.%d.%d", TCLRESOURCE_MAJOR, TCLRESOURCE_MINOR, TCLRESOURCE_SUBMINOR);
		} else {
			sprintf(vstr,"%d.%d", TCLRESOURCE_MAJOR, TCLRESOURCE_MINOR);
		}
	} else {
		sprintf(vstr,"%d.%d%c%d", TCLRESOURCE_MAJOR, TCLRESOURCE_MINOR, 
				TCLRESOURCE_STAGE, TCLRESOURCE_SUBMINOR);
	}
	
	// Declare the TclResource package. 
	if (Tcl_PkgProvide(interp, "resource", vstr) != TCL_OK) {
		return TCL_ERROR;
	}       
	return TCL_OK;
}



// ----------------------------------------------------------------------
//
// Tcl_ResourceCommand --
//
//	This procedure is invoked to process the "resource" Tcl command.
//	See the user documentation for details on what it does.
//
// Results:
//	A standard Tcl result.
//
// Side effects:
//	See the user documentation.
//
// ----------------------------------------------------------------------

int
Tcl_ResourceCommand(
    ClientData clientData,		// Not used. 
    Tcl_Interp *interp,			// Current interpreter. 
    int objc,					// Number of arguments. 
    Tcl_Obj *CONST objv[])		// Argument values. 
{
	Tcl_Obj *resultPtr;
	int index, result;
	
	static CONST char *switches[] = {
		"attributes", "close", "delete", "files", 
		"fork", "id", "list", "name", "open", 
		"read", "types", "update", "write", (char *) NULL
	};
	
	enum {
		RESOURCE_ATTRIBUTES, RESOURCE_CLOSE, RESOURCE_DELETE, RESOURCE_FILES, 
		RESOURCE_FORK, RESOURCE_ID, RESOURCE_LIST, RESOURCE_NAME, RESOURCE_OPEN, 
		RESOURCE_READ, RESOURCE_TYPES, RESOURCE_UPDATE, RESOURCE_WRITE
	};
	
 	resultPtr = Tcl_NewObj();
	
	if (objc < 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "subcommand ?arg ...?");
		return TCL_ERROR;
	}
	
	if (Tcl_GetIndexFromObj(interp, objv[1], switches, "subcommand", 0, &index) != TCL_OK) {
		return TCL_ERROR;
	}
	if (!initialized) {
		TclRes_InitializeTables();
	}
	
	switch (index) {
		case RESOURCE_ATTRIBUTES:
		result = TclResCmd_Attributes(clientData, interp, objc, objv, resultPtr);
		break;
		
		case RESOURCE_CLOSE:			
		result = TclResCmd_Close(clientData, interp, objc, objv, resultPtr);
		break;
		
		case RESOURCE_DELETE:
		result = TclResCmd_Delete(clientData, interp, objc, objv, resultPtr);
		break;
		
		case RESOURCE_FILES:
		result = TclResCmd_Files(clientData, interp, objc, objv, resultPtr);
		break;
		
		case RESOURCE_FORK:			
		result = TclResCmd_Fork(clientData, interp, objc, objv, resultPtr);
		break;
		
		case RESOURCE_ID:			
		result = TclResCmd_Id(clientData, interp, objc, objv, resultPtr);
		break;
		
		case RESOURCE_LIST:			
		result = TclResCmd_List(clientData, interp, objc, objv, resultPtr);
		break;
		
		case RESOURCE_NAME:
		result = TclResCmd_Name(clientData, interp, objc, objv, resultPtr);
		break;
		
		case RESOURCE_OPEN:
		result = TclResCmd_Open(clientData, interp, objc, objv, resultPtr);
		break;
		
		case RESOURCE_READ:			
		result = TclResCmd_Read(clientData, interp, objc, objv, resultPtr);
		break;
		
		case RESOURCE_TYPES:			
		result = TclResCmd_Types(clientData, interp, objc, objv, resultPtr);
		break;
		
		case RESOURCE_UPDATE:			
		result = TclResCmd_Update(clientData, interp, objc, objv, resultPtr);
		break;
		
		case RESOURCE_WRITE:			
		result = TclResCmd_Write(clientData, interp, objc, objv, resultPtr);
		break;
		
		default:
		panic("Tcl_GetIndexFromObj returned unrecognized option");
		return TCL_ERROR;	// Should never be reached. 
	}
	
	Tcl_ResetResult(interp);
	Tcl_SetObjResult(interp, resultPtr);
	
	return result;
}


// ----------------------------------------------------------------------
//
// TclResCmd_Attributes --
//
//	This procedure is invoked to process the [resource attributes] Tcl command.
//	See the user documentation for details on what it does.
//
// Syntax:
// 		resource attributes resourceRef
// 		resource attributes resourceRef value
// 		resource attributes resourceRef option resourceType
// 		resource attributes resourceRef option resourceType value
// 		
// Results:
//	A standard Tcl result.
//
// Side effects:
//	See the user documentation.
//
// ----------------------------------------------------------------------

int
TclResCmd_Attributes(
    ClientData clientData,		// Not used. 
    Tcl_Interp *interp,			// Current interpreter. 
    int objc,					// Number of arguments. 
    Tcl_Obj *CONST objv[],		// Argument values. 
    Tcl_Obj *resultPtr)			// Pointer to store the result. 
{
	OpenResourceFork * resourceRef;
    int		index, result, gotResID, gotValue, length, newValue;
	short	rsrcId = 0;
	long	theLong;
    short	saveRef, theMapAttrs, theRezAttrs;
    char *	resourceName = NULL;
    char	buffer[128];
    Handle	resourceH = NULL;
    OSErr	err = noErr;
    Str255	theName;
    OSType	rezType;

	static CONST char *attributesSwitches[] = {
		"-id", "-name", (char *) NULL
	};
	
	enum {
		RESOURCE_ATTRIBUTES_ID, RESOURCE_ATTRIBUTES_NAME
	};
	
	result = TCL_OK;

	if (!(objc == 3 || objc == 4 || objc == 6 || objc == 7)) {
		Tcl_WrongNumArgs(interp, 2, objv, 
						 "resourceRef ?(-id resourceID|-name resourceName) resourceType? ?value?");
		return TCL_ERROR;
	}
	
	resourceRef = TclRes_GetResourceRefFromObj(objv[2], true, 
											   "get attributes from", resultPtr);
	if (resourceRef == NULL) {
		return TCL_ERROR;
	}	
	
	gotValue = false; 
	
	if (objc == 4 || objc == 7) {
		if (Tcl_GetIntFromObj(interp, objv[objc-1], &newValue) != TCL_OK) {
			return TCL_ERROR;
		}
		gotValue = true;
	} 
	
	if (objc == 3) {
		// Getting the resource map attributes
		theMapAttrs = GetResFileAttrs(resourceRef->fileRef);
		err = ResError();
		if (err != noErr) {
			Tcl_AppendStringsToObj(resultPtr, "error getting resource map attributes", (char *) NULL);
			return TCL_ERROR;
		} else {
			Tcl_SetIntObj(resultPtr, theMapAttrs);
			return TCL_OK;
		}
	} 
	
	if (objc == 4) {
		// Setting the resource map attributes
		SetResFileAttrs(resourceRef->fileRef, newValue);
		err = ResError();
		if (err != noErr) {
			Tcl_AppendStringsToObj(resultPtr, "error setting resource map attributes", (char *) NULL);
			return TCL_ERROR;
		} 
		return TCL_OK;
	} 
	
	gotResID = false;
	resourceName = NULL;
	
	if (Tcl_GetIndexFromObj(interp, objv[3], attributesSwitches, "switch", 0, &index) != TCL_OK) {
		return TCL_ERROR;
	}
		
	switch (index) {
		
		case RESOURCE_ATTRIBUTES_ID:		
		if (Tcl_GetLongFromObj(interp, objv[4], &theLong) != TCL_OK) {
			return TCL_ERROR;
		}
		rsrcId = (short) theLong;
		gotResID = true;
		break;
		
		case RESOURCE_ATTRIBUTES_NAME:		
		resourceName = Tcl_GetStringFromObj(objv[4], &length);
		resourceName = strcpy((char *) theName, resourceName);
		c2pstr(resourceName);
		break;
	}
	
	if (TclRes_GetOSTypeFromObj(interp, objv[5], &rezType) != TCL_OK) {
		return TCL_ERROR;
	}
	
	if ((resourceName == NULL) && !gotResID) {
		Tcl_AppendStringsToObj(resultPtr,"you must specify either ",
				"-id or -name",
				(char *) NULL);
		return TCL_ERROR;
	}
	
	saveRef = CurResFile();
	UseResFile(resourceRef->fileRef);
	
	// Don't load the resource in memory 
	SetResLoad(false);
	if (gotResID == true) {
		resourceH = Get1Resource(rezType, rsrcId);
		err = ResError();
	} else if (resourceName != NULL) {
		resourceH = Get1NamedResource(rezType, (StringPtr) resourceName);
		err = ResError();
	}

	SetResLoad(true);
	    
	if (err != noErr) {                
		sprintf(buffer, "resource error %d while trying to find resource", err);
		Tcl_AppendStringsToObj(resultPtr, buffer, (char *) NULL);
		result = TCL_ERROR;
		goto attributesDone;               
	}
	
	// Getting/setting the value
	if (resourceH != NULL) {
		if (gotValue) {
			// Setting the resource attributes
			theMapAttrs = GetResFileAttrs(resourceRef->fileRef);
			if (theMapAttrs & mapReadOnly) {
				Tcl_AppendStringsToObj(resultPtr, "cannot set the attributes, resource map is read only", (char *) NULL);
				result = TCL_ERROR;
				goto attributesDone;
			}
			theRezAttrs = GetResAttrs(resourceH);
			if (theRezAttrs != newValue) {
				// If the user is setting the resChanged flag on, load the
				// resource in memory if it is not already there (i-e if its
				// master pointer contains NULL) otherwise, upon updating, null
				// data would be written to the disk. NB: no need to bother about
				// releasing the resource because anyway ReleaseResource() wonÕt
				// release a resource whose resChanged attribute has been set.
				if (newValue & resChanged) {
					if (*resourceH == NULL) {
						LoadResource(resourceH);
					} 
				} 
				SetResAttrs(resourceH, newValue);
				err = ResError();
				if (err != noErr) {
					sprintf(buffer, "error %d setting resource attributes", err);
					Tcl_AppendStringsToObj(resultPtr, buffer, (char *) NULL);
					result = TCL_ERROR;
					goto attributesDone;
				} 
			} 
			result = TCL_OK;
		} else {
			// Getting the resource attributes
			theRezAttrs = GetResAttrs(resourceH);
			err = ResError();
			if (err != noErr) {
				Tcl_AppendStringsToObj(resultPtr, "error getting resource attributes", (char *) NULL);
				result = TCL_ERROR;
				goto attributesDone;
			} else {
				Tcl_SetIntObj(resultPtr, theRezAttrs);
				result = TCL_OK;
			}
		}
	} else {
		Tcl_AppendStringsToObj(resultPtr, "resource not found", (char *) NULL);
		result = TCL_ERROR;
		goto attributesDone;
	}
	
attributesDone:
	UseResFile(saveRef);                        
	return result;	
}


// ----------------------------------------------------------------------
//
// TclResCmd_Close --
//
//	This procedure is invoked to process the [resource close] Tcl command.
//	See the user documentation for details on what it does.
//
// Syntax:
// 		resource close resourceRef
// 		
// Results:
//	A standard Tcl result.
//
// Side effects:
//	See the user documentation.
//
// ----------------------------------------------------------------------

int
TclResCmd_Close(
	ClientData clientData,		// Not used. 
	Tcl_Interp *interp,			// Current interpreter. 
	int objc,					// Number of arguments. 
	Tcl_Obj *CONST objv[],		// Argument values. 
	Tcl_Obj *resultPtr)			// Pointer to store the result. 
{
	int		length;
	short	fileRef;
	char *	stringPtr;
	OSErr	err;
	int		result = TCL_OK;
	
	if (objc != 3) {
		Tcl_WrongNumArgs(interp, 2, objv, "resourceRef");
		return TCL_ERROR;
	}
	stringPtr = Tcl_GetStringFromObj(objv[2], &length);
	fileRef = TclRes_UnRegisterResourceFork(stringPtr, resultPtr);
	
	// If fileRef is not a reference number for a file whose resource fork is
	// open, CloseResFile() does nothing, and ResError() returns the
	// result code resFNotFound. If fileRef is 0, it represents the System
	// file and is ignored. You cannot close the System fileÕs resource fork.
	if (fileRef > 0) {
		CloseResFile(fileRef);
		err = ResError();
		if (err != noErr) {
			Tcl_AppendStringsToObj(resultPtr, "couldn't close the resource fork", (char *) NULL);
			result = TCL_ERROR;
		}
	} else {
		result = TCL_ERROR;
	}
	return result;
}


// ----------------------------------------------------------------------
//
// TclResCmd_Delete --
//
//	This procedure is invoked to process the [resource delete] Tcl command.
//	See the user documentation for details on what it does.
//
// Syntax:
// 		resource delete ?options? resourceType
// 		
// Results:
//	A standard Tcl result.
//
// Side effects:
//	See the user documentation.
//
// ----------------------------------------------------------------------

int
TclResCmd_Delete(
    ClientData clientData,		// Not used. 
    Tcl_Interp *interp,			// Current interpreter. 
    int objc,					// Number of arguments. 
    Tcl_Obj *CONST objv[],		// Argument values. 
    Tcl_Obj *resultPtr)			// Pointer to store the result. 
{
	OpenResourceFork *resourceRef = NULL;
    int		index, result, gotResID;
    int		i, limitSearch, length;
    short	saveRef = 0, resInfo;
	short	fileRef, rsrcId = 0;
	long	theLong;
    char *	resourceName = NULL;	
	char	buffer[128];
    Handle	resourceH = NULL;
    OSErr	err;
    Str255	theName;
    OSType	rezType;

    static CONST char *deleteSwitches[] = {"-id", "-name", "-file", (char *) NULL};
             
    enum {RESOURCE_DELETE_ID, RESOURCE_DELETE_NAME, RESOURCE_DELETE_FILE};

    result = TCL_OK;

	if (!((objc >= 3) && (objc <= 9) && ((objc % 2) == 1))) {
		Tcl_WrongNumArgs(interp, 2, objv, 
						 "?-id resourceID? ?-name resourceName? ?-file resourceRef? resourceType");
		return TCL_ERROR;
	}
	
	i = 2;
	fileRef = kResFileNotOpened;
	gotResID = false;
	resourceName = NULL;
	limitSearch = false;
	
	while (i < (objc - 2)) {
		if (Tcl_GetIndexFromObj(interp, objv[i], deleteSwitches, "option", 0, &index) != TCL_OK) {
			return TCL_ERROR;
		}
		
		switch (index) {
			
			case RESOURCE_DELETE_ID:		
			if (Tcl_GetLongFromObj(interp, objv[i+1], &theLong) != TCL_OK) {
				return TCL_ERROR;
			}
			rsrcId = (short) theLong;
			gotResID = true;
			break;
			
			case RESOURCE_DELETE_NAME:		
			resourceName = Tcl_GetStringFromObj(objv[i+1], &length);
			if (length > 255) {
				Tcl_AppendStringsToObj(resultPtr, 
					   "-name argument too long, must be < 255 characters", (char *) NULL);
				return TCL_ERROR;
			}
			resourceName = strcpy((char *) theName, resourceName);
			c2pstr(resourceName);
			break;
			
			case RESOURCE_DELETE_FILE:
			resourceRef = TclRes_GetResourceRefFromObj(objv[i+1], 0, "delete from", resultPtr);
			if (resourceRef == NULL) {
				return TCL_ERROR;
			}	
			limitSearch = true;
			break;
		}
		i += 2;
	}
	
	if ((resourceName == NULL) && !gotResID) {
		Tcl_AppendStringsToObj(resultPtr,"you must specify either ",
		        "-id or -name or both", (char *) NULL);
		return TCL_ERROR;
	}

	if (TclRes_GetOSTypeFromObj(interp, objv[i], &rezType) != TCL_OK) {
		return TCL_ERROR;
	}
	
	if (limitSearch) {
		saveRef = CurResFile();
		UseResFile(resourceRef->fileRef);
	}
	
	SetResLoad(false);
	
	if (gotResID == true) {
		if (limitSearch) {
			resourceH = Get1Resource(rezType, rsrcId);
		} else {
			resourceH = GetResource(rezType, rsrcId);
		}
		err = ResError();
		
		if (err == resNotFound || resourceH == NULL) {
			Tcl_AppendStringsToObj(resultPtr, "resource not found", (char *) NULL);
			result = TCL_ERROR;
			goto deleteDone;               
		} else if (err != noErr) {
			sprintf(buffer, "error %d while trying to find resource", err);
			Tcl_AppendStringsToObj(resultPtr, buffer, (char *) NULL);
			result = TCL_ERROR;
			goto deleteDone;               
		}
	} 
	
	if (resourceName != NULL) {
		Handle tmpResource;
		if (limitSearch) {
			tmpResource = Get1NamedResource(rezType, (StringPtr) resourceName);
		} else {
			tmpResource = GetNamedResource(rezType, (StringPtr) resourceName);
		}
		err = ResError();
		
		if (err == resNotFound || tmpResource == NULL) {
			Tcl_AppendStringsToObj(resultPtr, "resource not found", (char *) NULL);
			result = TCL_ERROR;
			goto deleteDone;               
		} else if (err != noErr) {
			sprintf(buffer, "error %d while trying to find resource", err);
			Tcl_AppendStringsToObj(resultPtr, buffer, (char *) NULL);
			result = TCL_ERROR;
			goto deleteDone;               
		}
		
		if (gotResID) { 
			if (resourceH != tmpResource) {
				Tcl_AppendStringsToObj(resultPtr, "-id and -name ", 
									   "values do not point to the same resource", (char *) NULL);
				result = TCL_ERROR;
				goto deleteDone;
			}
		} else {
			resourceH = tmpResource;
		}
	}

	resInfo = GetResAttrs(resourceH);
																																		
	if ((resInfo & resProtected) == resProtected) {
		Tcl_AppendStringsToObj(resultPtr, 
							   "resource cannot be deleted: it is protected.", (char *) NULL);
		result = TCL_ERROR;
		goto deleteDone;               
	} else if ((resInfo & resSysHeap) == resSysHeap) {   
		Tcl_AppendStringsToObj(resultPtr, 
							   "resource cannot be deleted: it is in the system heap.", (char *) NULL);
		result = TCL_ERROR;
		goto deleteDone;               
	}
	    
	// Find the resource file, if it was not specified,
	// so we can flush the changes now. Perhaps this is
	// a little paranoid, but better safe than sorry.
	RemoveResource(resourceH);
	
	if (!limitSearch) {
		UpdateResFile(HomeResFile(resourceH));
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


// ----------------------------------------------------------------------
//
// TclResCmd_Files --
//
//	This procedure is invoked to process the [resource files] Tcl command.
//	See the user documentation for details on what it does.
//
// Syntax:
// 		resource files ?resourceRef?
// 		
// Results:
//	A standard Tcl result.
//
// Side effects:
//	See the user documentation.
//
// ----------------------------------------------------------------------

int
TclResCmd_Files(
    ClientData clientData,		// Not used. 
    Tcl_Interp *interp,			// Current interpreter. 
    int objc,					// Number of arguments. 
    Tcl_Obj *CONST objv[],		// Argument values. 
    Tcl_Obj *resultPtr)			// Pointer to store the result. 
{
	OpenResourceFork * resourceRef;
	int		length;
	char *	stringPtr;
	OSErr	err;
	
	if ((objc < 2) || (objc > 3)) {
		Tcl_WrongNumArgs(interp, 2, objv, "?resourceID?");
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
		Tcl_DString	ds;
		
		resourceRef = TclRes_GetResourceRefFromObj(objv[2], 1, "files", resultPtr);
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
		
		// Get an FSRef and build the path 
		fileFSSpec.vRefNum = fileRec.ioFCBVRefNum;
		fileFSSpec.parID = fileRec.ioFCBParID;
		strncpy( (char *) fileFSSpec.name, (char *) fileRec.ioNamePtr, fileRec.ioNamePtr[0]+1);
		err = FSpMakeFSRef(&fileFSSpec, &fileFSRef);
		err = FSRefMakePath(&fileFSRef, pathPtr, 256);
		if ( err != noErr) {
			Tcl_SetStringObj(resultPtr,
							 "could not get file path from token", -1);
			return TCL_ERROR;
		}
		
		Tcl_ExternalToUtfDString(NULL, pathPtr, strlen(pathPtr), &ds);
		Tcl_SetStringObj(resultPtr, Tcl_DStringValue(&ds), Tcl_DStringLength(&ds));
		Tcl_DStringFree(&ds);
	}                    	    
	return TCL_OK;
}


// ----------------------------------------------------------------------
//
// TclResCmd_Fork --
//
//	This procedure is invoked to process the [resource fork] Tcl command.
//	See the user documentation for details on what it does.
//
// Syntax:
// 		resource fork resourceRef
// 		
// Results:
//	A standard Tcl result.
//
// Side effects:
//	See the user documentation.
//
// ----------------------------------------------------------------------

int
TclResCmd_Fork(
    ClientData clientData,		// Not used. 
    Tcl_Interp *interp,			// Current interpreter. 
    int objc,					// Number of arguments. 
    Tcl_Obj *CONST objv[],		// Argument values. 
    Tcl_Obj *resultPtr)			// Pointer to store the result. 
{
	OpenResourceFork * resourceRef;
	
	if (objc != 3) {
		Tcl_WrongNumArgs(interp, 2, objv, "resourceRef");
		return TCL_ERROR;
	}
	resourceRef = TclRes_GetResourceRefFromObj(objv[2], true, 
										"get fork from", resultPtr);
	
	if (resourceRef != NULL) {
		Tcl_ResetResult(interp);
		switch (resourceRef->fileFork) {
			
			case from_rezfork:
			Tcl_AppendStringsToObj(resultPtr, "resourcefork", (char *) NULL);
			return TCL_OK;
			break;
			
			case from_datafork:
			Tcl_AppendStringsToObj(resultPtr, "datafork", (char *) NULL);
			return TCL_OK;
			break;
			
			default:
			Tcl_AppendStringsToObj(resultPtr, "unknown", (char *) NULL);
			return TCL_OK;
		}
	} else {
		return TCL_ERROR;
	}
}


// ----------------------------------------------------------------------
//
// TclResCmd_Id --
//
//	This procedure is invoked to process the [resource id] Tcl command.
//	See the user documentation for details on what it does.
//
// Syntax:
// 		resource id resourceType resourceName resourceRef
// 		
// Results:
//	A standard Tcl result.
//
// Side effects:
//	See the user documentation.
//
// ----------------------------------------------------------------------

int
TclResCmd_Id(
    ClientData clientData,		// Not used. 
    Tcl_Interp *interp,			// Current interpreter. 
    int objc,					// Number of arguments. 
    Tcl_Obj *CONST objv[],		// Argument values. 
    Tcl_Obj *resultPtr)			// Pointer to store the result. 
{
	short	rsrcId = 0;
	int		length, releaseIt = 0;
	char *	resmapRef;
	char *	resourceName = NULL;	
	Handle	resourceH = NULL;
	OSErr	err;
	Str255	theName;
	OSType	rezType;
	
	Tcl_ResetResult(interp);
	if (objc != 5) {
		Tcl_WrongNumArgs(interp, 2, objv,
						 "resourceType resourceName resourceRef");
		return TCL_ERROR;
	}
	
	if (TclRes_GetOSTypeFromObj(interp, objv[2], &rezType) != TCL_OK) {
		return TCL_ERROR;
	}
	
	resourceName = Tcl_GetStringFromObj(objv[3], &length);
	if (resourceName == NULL) {
		Tcl_AppendStringsToObj(resultPtr, "wrong third argument", (char *) NULL);
		return TCL_ERROR;
	} 
	
	resmapRef = Tcl_GetStringFromObj(objv[4], &length);
	resourceH = TclRes_FindResource(interp, rezType, resourceName, 
									rsrcId, resmapRef, &releaseIt);
	
	if (resourceH != NULL) {
		GetResInfo(resourceH, &rsrcId, (ResType *) &rezType, theName);
		err = ResError();
		if (err == noErr) {
			Tcl_SetIntObj(resultPtr, rsrcId);
			return TCL_OK;
		} else {
			Tcl_AppendStringsToObj(resultPtr, "could not get resource info", (char *) NULL);
			return TCL_ERROR;
		}
		if (releaseIt) {
			ReleaseResource(resourceH);
		}
	} else {
		Tcl_AppendStringsToObj(resultPtr, "could not find resource", (char *) NULL);
		return TCL_ERROR;
	}
}


// ----------------------------------------------------------------------
//
// TclResCmd_List --
//
//	This procedure is invoked to process the [resource list] Tcl command.
//	See the user documentation for details on what it does.
//
// Syntax:
// 		resource list ?-ids? resourceType ?resourceRef?
// 		
// Results:
//	A standard Tcl result.
//
// Side effects:
//	See the user documentation.
//
// ----------------------------------------------------------------------
			
int
TclResCmd_List(
    ClientData clientData,		// Not used. 
    Tcl_Interp *interp,			// Current interpreter. 
    int objc,					// Number of arguments. 
    Tcl_Obj *CONST objv[],		// Argument values. 
    Tcl_Obj *resultPtr)			// Pointer to store the result. 
{
	OpenResourceFork * resourceRef;
	Tcl_Obj *	objPtr;
	int			i, count, result, limitSearch, onlyID, length;
	short		id, saveRef = 0;
	char *		string;
	Handle		resourceH = NULL;
	Str255		theName;
	OSType		rezType;
	
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
	if (TclRes_GetOSTypeFromObj(interp, objv[i], &rezType) != TCL_OK) {
		return TCL_ERROR;
	}
	i++;
	if (objc == i + 1) {
		resourceRef = TclRes_GetResourceRefFromObj(objv[i], 1, "list", resultPtr);
		if (resourceRef == NULL) {
			return TCL_ERROR;
		}	
		
		saveRef = CurResFile();
		UseResFile(resourceRef->fileRef);
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
			resourceH = Get1IndResource(rezType, i);
		} else {
			resourceH = GetIndResource(rezType, i);
		}
		if (resourceH != NULL) {
			GetResInfo(resourceH, &id, (ResType *) &rezType, theName);
			if (theName[0] != 0 && !onlyID) {
				objPtr = Tcl_NewStringObj((char *) theName + 1, theName[0]);
			} else {
				objPtr = Tcl_NewIntObj(id);
			}
			// Bug in the original code: the resource was released in all cases 
			// This could cause a crash when calling the command without a 
			// recourceRef, like for instance:
			//     resource list CURS
			// because this would release system CURS resources. 
			// Fix: if the Master Pointer of the returned handle is
			// null, then the resource was not in memory, and it is
			// safe to release it. Otherwise, it is not.
			if (*resourceH == NULL) {
				ReleaseResource(resourceH);
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
	
	return result;
}


// ----------------------------------------------------------------------
//
// TclResCmd_Name --
//
//	This procedure is invoked to process the [resource name] Tcl command.
//	See the user documentation for details on what it does.
//
// Syntax:
// 		resource name resourceType resourceId resourceRef
// 		
// Results:
//	A standard Tcl result.
//
// Side effects:
//	See the user documentation.
//
// ----------------------------------------------------------------------

int
TclResCmd_Name(
    ClientData clientData,		// Not used. 
    Tcl_Interp *interp,			// Current interpreter. 
    int objc,					// Number of arguments. 
    Tcl_Obj *CONST objv[],		// Argument values. 
    Tcl_Obj *resultPtr)			// Pointer to store the result. 
{
	short	rsrcId;
	long	theLong;
	int		length, releaseIt = 0;
	char *	resmapRef;
	Handle	resourceH = NULL;
	OSErr	err;
	Str255	theName;
	OSType	rezType;
	
	Tcl_ResetResult(interp);
	if (objc != 5) {
		Tcl_WrongNumArgs(interp, 2, objv, "resourceType resourceID resourceRef");
		return TCL_ERROR;
	}
	
	if (TclRes_GetOSTypeFromObj(interp, objv[2], &rezType) != TCL_OK) {
		return TCL_ERROR;
	}
	
	if (Tcl_GetLongFromObj(interp, objv[3], &theLong) != TCL_OK) {
		Tcl_AppendStringsToObj(resultPtr, "wrong third argument: expected integer", (char *) NULL);
		return TCL_ERROR;
	}
	rsrcId = (short) theLong;
	resmapRef = Tcl_GetStringFromObj(objv[4], &length);
	resourceH = TclRes_FindResource(interp, rezType, NULL,
									rsrcId, resmapRef, &releaseIt);
	
	if (resourceH != NULL) {
		GetResInfo(resourceH, &rsrcId, (ResType *) &rezType, theName);
		err = ResError();
		if (err == noErr) {
			p2cstr(theName);
			Tcl_AppendStringsToObj(resultPtr, theName, (char *) NULL);
			return TCL_OK;
		} else {
			Tcl_AppendStringsToObj(resultPtr, "could not get resource info", (char *) NULL);
			return TCL_ERROR;
		}
		if (releaseIt) {
			ReleaseResource(resourceH);
		}
	} else {
		Tcl_AppendStringsToObj(resultPtr, "could not find resource", (char *) NULL);
		return TCL_ERROR;
	}
}


// ----------------------------------------------------------------------
//
// TclResCmd_Open --
//
//	This procedure is invoked to process the [resource open] Tcl command.
//	See the user documentation for details on what it does.
//
// Syntax:
// 		resource open ?(-datafork|-resourcefork)? fileName ?access?
// 		
// Results:
//	A standard Tcl result.
//
// Side effects:
//	See the user documentation.
//
// ----------------------------------------------------------------------

int
TclResCmd_Open(
    ClientData clientData,		// Not used. 
    Tcl_Interp *interp,			// Current interpreter. 
    int objc,					// Number of arguments. 
    Tcl_Obj *CONST objv[],		// Argument values. 
    Tcl_Obj *resultPtr)			// Pointer to store the result. 
{
    int		index, length, mode;
    int		fromFork = from_anyfork, foundFork = from_unspecified, filenameIdx = 2;
	Boolean permSpecified = false, isDir = false, gotParentRef = false;
	short	refnum;
    char *	stringPtr;
	char *	native;
	char	resultStr[256];
    SInt8	macPermision = 0;
    FSSpec	fileSpec;
    FSRef	fileFSRef, parentFSRef;
    OSErr	err;
	CONST char * str;
	Tcl_DString dss, ds;

    static CONST char *openSwitches[] = {
		"-datafork", "-resourcefork", (char *) NULL
    };
            
    enum {
		RESOURCE_OPEN_DATAFORK, RESOURCE_OPEN_RESOURCEFORK
    };

    if (!((objc == 3) || (objc == 4) || (objc == 5))) {
		Tcl_WrongNumArgs(interp, 2, objv, "?(-datafork|-resourcefork)? fileName ?permission?");
		return TCL_ERROR;
    }
	
	// Parse the arguments
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
    if (Tcl_TranslateFileName(interp, str, &ds) == NULL) {
		Tcl_AppendStringsToObj(resultPtr, "couldn't translate file name", (char *) NULL);
		return TCL_ERROR;
    }
    native = Tcl_UtfToExternalDString(NULL, Tcl_DStringValue(&ds), Tcl_DStringLength(&ds), &dss);

	// Get an FSRef
	err = FSPathMakeRef(native, &fileFSRef, &isDir);
    Tcl_DStringFree(&ds);
    if (err != noErr && err != fnfErr) {
		Tcl_AppendStringsToObj(resultPtr, "couldn't get file ref from path", (char *) NULL);
		return TCL_ERROR;
    }
    if (isDir) {
		Tcl_AppendStringsToObj(resultPtr, "specified path is a directory", (char *) NULL);
		return TCL_ERROR;
    }

	if (err == fnfErr) {
		// Build an FSSpec manually with the parent folder (which must exist) and the name
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
			CopyCStringToPascal(separatorPtr+1, fileSpec.name);
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
    Tcl_DStringFree(&dss);
	
	// Get permissions for the file. We really only understand read-only and
	// shared-read-write. If no permissions are given, we default to read only.
    if (permSpecified) {
		stringPtr = Tcl_GetStringFromObj(objv[objc-1], &length);
 		mode = TclGetOpenMode(interp, stringPtr, &index);
		if (mode == -1) {
			// TODO: TclGetOpenMode doesn't work with Obj commands. 
			Tcl_AppendStringsToObj(resultPtr, "invalid access mode '", stringPtr, "'", (char *) NULL);
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
			panic("TclResCmd_Open: invalid permission value");
			break;
		}
    } else {
		macPermision = fsRdPerm;
    }
    
	// If path was invalid, don't try to open the resource map. If file 
	// permission is fsRdWrShPerm we will try to create a new file.
	if (err == fnfErr) {
		refnum = kResFileNotOpened;
		goto openforkDone;
	}

	// The opening functions below are enclosed between SetResLoad(false) and
	// SetResLoad(true) statements in order not to load in any of the
	// resources in the file: this could cause problems if you open a file
	// that has CODE resources...
	// 
	// The following heuristic is applied:
	// - if we have from_rezfork or from_datafork, then only the
	//   corresponding fork is searched
	// - if it is from_anyfork, then we first look for resources in the data
	//   fork and, if this fails, we look for resources in the resource fork
	
	if (fromFork != from_rezfork) {
		// Try to open the file as a datafork resource file
		SetResLoad(false); 
		err = FSOpenResourceFile( &fileFSRef, 0, nil, macPermision, &refnum );
		SetResLoad(true); 
		if (err == noErr) {
			foundFork = from_datafork;
			goto openforkDone;
		} 
    } 
    if (fromFork != from_datafork) {
		// Now try to open as a resourcefork resource file
		SetResLoad(false); 
		refnum = FSpOpenResFile( &fileSpec, macPermision);
		SetResLoad(true); 
		err = ResError();
		if (err == noErr) {
			foundFork = from_rezfork;
		} 
    } 

    openforkDone:
	// If the functions opening the resource map failed and if the permission is
	// fsRdWrShPerm, try to create a new resource file.
    if (refnum == kResFileNotOpened) {
		if (((err == fnfErr) || (err == eofErr)) && (macPermision == fsRdWrShPerm)) {
			// Create the resource fork now.
			switch (fromFork) {
				
				case from_rezfork:
				HCreateResFile(fileSpec.vRefNum, fileSpec.parID, fileSpec.name);
				refnum = FSpOpenResFile(&fileSpec, macPermision);
				break;
				
				default: {
					CONST Tcl_UniChar *	uniString;
					FSSpec 				parentSpec;
					int 				numChars;
					
					if (!gotParentRef) {
						// Get FSRef of parent 
						CInfoPBRec	pb;
						Str255		dirName;
						
						pb.dirInfo.ioNamePtr = dirName;
						pb.dirInfo.ioVRefNum = fileSpec.vRefNum;
						pb.dirInfo.ioDrParID = fileSpec.parID;
						pb.dirInfo.ioFDirIndex = -1;	// Info about directory 
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
					// Get Unicode name 
					Tcl_DStringInit(&ds);
					Tcl_ExternalToUtfDString(NULL, (CONST char *) fileSpec.name + 1, fileSpec.name[0], &ds);

					Tcl_DStringInit(&dss);
					uniString = Tcl_UtfToUniCharDString(Tcl_DStringValue(&ds), Tcl_DStringLength(&ds), &dss);
					numChars = Tcl_DStringLength(&dss) / sizeof(Tcl_UniChar);
					Tcl_DStringFree(&ds);
					Tcl_DStringFree(&dss);

					err = FSCreateResourceFile(&parentFSRef, numChars, uniString, kFSCatInfoNone, 
									 NULL, 0, NULL, &fileFSRef, &fileSpec);
					if (err == noErr) {
						err = FSOpenResourceFile( &fileFSRef, 0, NULL, macPermision, &refnum );
					} 
					break;
				}
			}
			if (refnum == kResFileNotOpened) {
				goto openError;
			} else {
				foundFork = fromFork;
			}
		} else if (err == fnfErr) {
			Tcl_AppendStringsToObj(resultPtr,
			"file does not exist", (char *) NULL);
			return TCL_ERROR;
		} else if (err == eofErr || err == mapReadErr) {
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
			sprintf(resultStr, "error %d opening resource file", err);
			Tcl_AppendStringsToObj(resultPtr, resultStr, (char *) NULL);
			return TCL_ERROR;
		}
    }

	// The FspOpenResFile function does not set the ResFileAttrs.
	// Even if you open the file read only, the mapReadOnly attribute is not
	// set. This means we can't detect writes to a read only resource fork
	// until the write fails, which is bogus. So set it here...
    if (macPermision == fsRdPerm) {
		SetResFileAttrs(refnum, mapReadOnly);
    }
	
	Tcl_SetStringObj(resultPtr, "", 0);   
    if (TclRes_RegisterResourceFork(refnum, resultPtr, foundFork, fork_CheckIfOpen) != TCL_OK) {
		CloseResFile(refnum);
		return TCL_ERROR;
    }
	
    return TCL_OK;
}


// ----------------------------------------------------------------------
//
// TclResCmd_Read --
//
//	This procedure is invoked to process the [resource read] Tcl command.
//	See the user documentation for details on what it does.
//
// Syntax:
// 		resource read resourceType resourceId ?resourceRef?
// 		
// Results:
//	A standard Tcl result.
//
// Side effects:
//	See the user documentation.
//
// ----------------------------------------------------------------------

int
TclResCmd_Read(
    ClientData clientData,		// Not used. 
    Tcl_Interp *interp,			// Current interpreter. 
    int objc,					// Number of arguments. 
    Tcl_Obj *CONST objv[],		// Argument values. 
    Tcl_Obj *resultPtr)			// Pointer to store the result. 
{
	short	rsrcId = 0;
	long	theLong, size;
    int		length, releaseIt = 0;
    char *	resmapRef;
    char *	resourceName = NULL;	
    Handle	resourceH = NULL;
    OSType	rezType;

	if (!((objc == 4) || (objc == 5))) {
		Tcl_WrongNumArgs(interp, 2, objv, "resourceType resourceID ?resourceRef?");
		return TCL_ERROR;
	}
	
	if (TclRes_GetOSTypeFromObj(interp, objv[2], &rezType) != TCL_OK) {
		return TCL_ERROR;
	}
	
	if (Tcl_GetLongFromObj((Tcl_Interp *) NULL, objv[3], &theLong) != TCL_OK) {
		resourceName = Tcl_GetStringFromObj(objv[3], &length);
	} else {
		rsrcId = (short) theLong;
	}

	if (objc == 5) {
		resmapRef = Tcl_GetStringFromObj(objv[4], &length);
	} else {
		resmapRef = NULL;
	}
	
	resourceH = TclRes_FindResource(interp, rezType, resourceName,
									rsrcId, resmapRef, &releaseIt);
	
	if (resourceH != NULL) {
		size = GetResourceSizeOnDisk(resourceH);
		Tcl_SetByteArrayObj(resultPtr, (unsigned char *) *resourceH, size);
		
		// Don't release the resource unless WE loaded it...
		if (releaseIt) {
			ReleaseResource(resourceH);
		}
		return TCL_OK;
	} else {
		Tcl_AppendStringsToObj(resultPtr, "could not load resource", (char *) NULL);
		return TCL_ERROR;
	}
}


// ----------------------------------------------------------------------
//
// TclResCmd_Types --
//
//	This procedure is invoked to process the [resource types] Tcl command.
//	See the user documentation for details on what it does.
//
// Syntax:
// 		resource types ?resourceRef?
// 		
// Results:
//	A standard Tcl result.
//
// Side effects:
//	See the user documentation.
//
// ----------------------------------------------------------------------

int
TclResCmd_Types(
    ClientData clientData,		// Not used. 
    Tcl_Interp *interp,			// Current interpreter. 
    int objc,					// Number of arguments. 
    Tcl_Obj *CONST objv[],		// Argument values. 
    Tcl_Obj *resultPtr)			// Pointer to store the result. 
{
	OpenResourceFork * resourceRef;
	Tcl_Obj *	objPtr;
	int			i, count, result, limitSearch;
	short		saveRef = 0;
	OSType		rezType;
	
	result = TCL_OK;
	limitSearch = false;
	
	if (!((objc == 2) || (objc == 3))) {
		Tcl_WrongNumArgs(interp, 2, objv, "?resourceRef?");
		return TCL_ERROR;
	}
	
	if (objc == 3) {
		resourceRef = TclRes_GetResourceRefFromObj(objv[2], 1, "get types of", resultPtr);
		if (resourceRef == NULL) {
			return TCL_ERROR;
		}
		saveRef = CurResFile();
		UseResFile(resourceRef->fileRef);
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
		objPtr = TclRes_NewOSTypeObj(rezType);
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


// ----------------------------------------------------------------------
//
// TclResCmd_Update --
//
//	This procedure is invoked to process the [resource update] Tcl command.
//	See the user documentation for details on what it does.
//
// Syntax:
// 		resource update resourceRef
// 		
// Results:
//	A standard Tcl result.
//
// Side effects:
//	See the user documentation.
//
// ----------------------------------------------------------------------

int
TclResCmd_Update(
    ClientData clientData,		// Not used. 
    Tcl_Interp *interp,			// Current interpreter. 
    int objc,					// Number of arguments. 
    Tcl_Obj *CONST objv[],		// Argument values. 
    Tcl_Obj *resultPtr)			// Pointer to store the result. 
{
	OpenResourceFork * resourceRef;
	char	buffer[128];
	OSErr	err;
	
	if (objc != 3) {
		Tcl_WrongNumArgs(interp, 2, objv, "resourceRef");
		return TCL_ERROR;
	}
	
	resourceRef = TclRes_GetResourceRefFromObj(objv[2], true, "update", resultPtr);
	if (resourceRef == NULL) {
		return TCL_ERROR;
	}	
	
	if (resourceRef->fileRef >= 0) {
		UpdateResFile(resourceRef->fileRef);
		err = ResError();
		if (err != noErr) {
			sprintf(buffer, "error %d updating resource map", err);
			Tcl_AppendStringsToObj(resultPtr, buffer, (char *) NULL);
			return TCL_ERROR;
		} 
		return TCL_OK;
	} else {
		Tcl_AppendStringsToObj(resultPtr, "invalid file ref", (char *) NULL);
		return TCL_ERROR;
	}
}


// ----------------------------------------------------------------------
//
// TclResCmd_Write --
//
//	This procedure is invoked to process the [resource write] Tcl command.
//	See the user documentation for details on what it does.
//
// Syntax:
// 		resource write ?options? resourceType data
// 		
// Results:
//	A standard Tcl result.
//
// Side effects:
//	See the user documentation.
//
// ----------------------------------------------------------------------

int
TclResCmd_Write(
    ClientData clientData,		// Not used. 
    Tcl_Interp *interp,			// Current interpreter. 
    int objc,					// Number of arguments. 
    Tcl_Obj *CONST objv[],		// Argument values. 
    Tcl_Obj *resultPtr)			// Pointer to store the result. 
{
    int		index, result, gotResID, releaseIt = 0, force;
    int		i, limitSearch, length;
	short	rsrcId = 0;
	long	theLong;
    short	saveRef = 0;
    char *	bytesPtr;
    char *	resourceName = NULL;	
    char	errbuf[16];
    OpenResourceFork * resourceRef = NULL;
    Handle	resourceH = NULL;
    OSErr	err;
    Str255	theName;
    OSType	rezType;

	static CONST char *writeSwitches[] = {
		"-id", "-name", "-file", "-force", "-datafork", (char *) NULL
	};
	
	enum {
		RESOURCE_WRITE_ID, RESOURCE_WRITE_NAME, RESOURCE_WRITE_FILE, 
		RESOURCE_WRITE_FORCE, RESOURCE_WRITE_DATAFORK
	};
	
	result = TCL_OK;
	limitSearch = false;
	
	if ((objc < 4) || (objc > 11)) {
		Tcl_WrongNumArgs(interp, 2, objv, 
						 "?-id resourceID? ?-name resourceName? ?-file resourceRef? ?-force? resourceType data");
		return TCL_ERROR;
	}
	
	i = 2;
	gotResID = false;
	theName[0] = 0;
	limitSearch = false;
	force = 0;

	while (i < (objc - 2)) {
		if (Tcl_GetIndexFromObj(interp, objv[i], writeSwitches, "switch", 0, &index) != TCL_OK) {
			return TCL_ERROR;
		}
		
		switch (index) {
			
			case RESOURCE_WRITE_ID:		
			if (Tcl_GetLongFromObj(interp, objv[i+1], &theLong) != TCL_OK) {
				return TCL_ERROR;
			}
			rsrcId = (short) theLong;
			gotResID = true;
			i += 2;
			break;
			
			case RESOURCE_WRITE_NAME: {
			resourceName = Tcl_GetStringFromObj(objv[i+1], &length);
			strcpy((char *) theName, resourceName);
			i += 2;
			break;
			}
			
			
			case RESOURCE_WRITE_FILE:		
			resourceRef = TclRes_GetResourceRefFromObj(objv[i+1], 0, "write to", resultPtr);
			if (resourceRef == NULL) {
				return TCL_ERROR;
			}	
			limitSearch = true;
			i += 2;
			break;
			
			case RESOURCE_WRITE_FORCE:
			force = 1;
			i += 1;
			break;
		}
	}
	if (TclRes_GetOSTypeFromObj(interp, objv[i], &rezType) != TCL_OK) {
		return TCL_ERROR;
	}
	bytesPtr = (char *) Tcl_GetByteArrayFromObj(objv[i+1], &length);
	
	resourceName = (char *) theName;
	c2pstr(resourceName);

	if (limitSearch) {
		saveRef = CurResFile();
		UseResFile(resourceRef->fileRef);
	}
	if (gotResID == false) {
		if (limitSearch) {
			rsrcId = Unique1ID(rezType);
		} else {
			rsrcId = UniqueID(rezType);
		}
	}
	
	// If we are adding the resource by number, then we must make sure
	// there is not already a resource of that number. We are not going
	// load it here, since we want to detect whether we loaded it or
	// not. Remember that releasing some resources, in particular menu
	// related ones, can be fatal.
	if (gotResID == true) {
		SetResLoad(false);
		resourceH = Get1Resource(rezType,rsrcId);
		SetResLoad(true);
	}     
			
	if (resourceH == NULL) {
		// We get into this branch either if there was not already a
		// resource of this type and ID, or the ID was not specified.
		resourceH = NewHandle(length);
		if (resourceH == NULL) {
			resourceH = NewHandle(length);
			if (resourceH == NULL) {
				panic("could not allocate memory to write resource");
			}
		}
		HLock(resourceH);
		memcpy(*resourceH, bytesPtr, length);
		HUnlock(resourceH);
		AddResource(resourceH, rezType, rsrcId, (StringPtr) resourceName);
		releaseIt = 1;
	} else {
		// We got here because there was a resource of this type and ID in the file. 
		if (*resourceH == NULL) {
			releaseIt = 1;
		} else {
			releaseIt = 0;
		}
		   
		if (!force) {
			// We only overwrite existant resources when the -force flag has been set.
			sprintf(errbuf,"%d", rsrcId);
		  
			Tcl_AppendStringsToObj(resultPtr, "the resource ", errbuf, 
				" already exists, use the \"-force\" option to overwrite it.", (char *) NULL);
			result = TCL_ERROR;
			goto writeDone;
		} else if (GetResAttrs(resourceH) & resProtected) {
			// If it is protected
			sprintf(errbuf,"%d", rsrcId);
			Tcl_AppendStringsToObj(resultPtr,
								   "could not write resource id ",
								   errbuf, " of type ",
								   Tcl_GetStringFromObj(objv[i],&length),
								   ", it was protected.",(char *) NULL);
		   result = TCL_ERROR;
		   goto writeDone;
	   } else {
			// Be careful, the resource might already be in memory if something else loaded it.
			if (*resourceH == 0) {
				LoadResource(resourceH);
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
 
			SetHandleSize(resourceH, length);
			if ( MemError() != noErr ) {
				panic("could not allocate memory to write resource");
			}

			HLock(resourceH);
			memcpy(*resourceH, bytesPtr, length);
			HUnlock(resourceH);
	   
			ChangedResource(resourceH);
		
			// We also may have changed the name...
			SetResInfo(resourceH, rsrcId, (StringPtr) resourceName);
		}
	}
	
	err = ResError();
	if (err != noErr) {
		Tcl_AppendStringsToObj(resultPtr, "error adding resource to resource map", (char *) NULL);
		result = TCL_ERROR;
		goto writeDone;
	}
	
	WriteResource(resourceH);
	err = ResError();
	if (err != noErr) {
		Tcl_AppendStringsToObj(resultPtr, "error writing resource to disk", (char *) NULL);
		result = TCL_ERROR;
	}
	
writeDone:
	if (releaseIt) {
		ReleaseResource(resourceH);
		err = ResError();
		if (err != noErr) {
			Tcl_GetStringFromObj(resultPtr, &length);
			if (length == 0) {
				Tcl_AppendStringsToObj(resultPtr, "error releasing resource", (char *) NULL);
			} 
			result = TCL_ERROR;
		}
	}
	
	if (limitSearch) {
		UseResFile(saveRef);
	}

	return result;
}


/****************
*               *
*   Utilities   *
*               *
****************/


// ----------------------------------------------------------------------
//
// TclRes_InitializeTables --
//
//	Initialize the structures used for resource management.
//
// Results:
//	None.
//
// Side effects:
//	Read the code.
//
// ----------------------------------------------------------------------

void
TclRes_InitializeTables()
{
	initialized = 1;
	Tcl_InitHashTable(&nameTable, TCL_STRING_KEYS);
	Tcl_InitHashTable(&resourceTable, TCL_ONE_WORD_KEYS);
	resourceForkList = Tcl_NewObj();
	Tcl_IncrRefCount(resourceForkList);

	TclRes_BuildResourceForkList();
}


// -----------------------------------------------------------------------------
//
// TclRes_FindResource --
//
//	Higher level interface for loading resources.
//
// Side Effects:
//	Attempts to load a resource.
//
// Results:
//  A handle on success.
//
// -----------------------------------------------------------------------------

Handle
TclRes_FindResource(
	Tcl_Interp *interp,			// Interpreter in which to process file.
	long resourceType,			// Type of resource to load.
	CONST char *resourceName,	// Name of resource to find,
								//   NULL if number should be used.
	int resourceNumber,			// Resource id of source.
	CONST char *resFileRef,		// Registered resource file reference,
								//   NULL if searching all open resource files.
	int *releaseIt)				// Should we release this resource when done.
{
	OpenResourceFork *	resourceRef;
	Tcl_HashEntry *		nameHashPtr;
	int					limitSearch = false;
	short				saveRef = 0;
	Handle				resourceH;
	
	if (resFileRef != NULL) {
		nameHashPtr = Tcl_FindHashEntry(&nameTable, resFileRef);
		if (nameHashPtr == NULL) {
			Tcl_AppendResult(interp, "invalid resource file reference \"", resFileRef, "\"", (char *) NULL);
			return NULL;
		}
		resourceRef = (OpenResourceFork *) Tcl_GetHashValue(nameHashPtr);
		saveRef = CurResFile();
		UseResFile(resourceRef->fileRef);
		limitSearch = true;
	}
	
	// Some system resources (for example system resources) should not 
	// be released.  So we set autoload to false, and try to get the resource.
	// If the Master Pointer of the returned handle is null, then resource was 
	// not in memory, and it is safe to release it.  Otherwise, it is not.
	SetResLoad(false);
	
	if (resourceName == NULL) {
		if (limitSearch) {
			resourceH = Get1Resource(resourceType, resourceNumber);
		} else {
			resourceH = GetResource(resourceType, resourceNumber);
		}
	} else {
		Str255 rezName;
		Tcl_DString ds;
		Tcl_UtfToExternalDString(NULL, resourceName, -1, &ds);
		strcpy((char *) rezName + 1, Tcl_DStringValue(&ds));
		rezName[0] = (unsigned) Tcl_DStringLength(&ds);
		if (limitSearch) {
			resourceH = Get1NamedResource(resourceType, rezName);
		} else {
			resourceH = GetNamedResource(resourceType, rezName);
		}
		Tcl_DStringFree(&ds);
	}
	
	if (resourceH != NULL && *resourceH == NULL) {
		*releaseIt = 1;
		LoadResource(resourceH);
	} else {
		*releaseIt = 0;
	}
	
	SetResLoad(true);
	
	if (limitSearch) {
		UseResFile(saveRef);
	}
	
	return resourceH;
}


// ----------------------------------------------------------------------
//
// TclRes_GetResourceRefFromObj --
//
//	Given a String object containing a resource file token, return
//	the OpenResourceFork structure that it represents, or NULL if 
//	the token cannot be found.  If okayOnReadOnly is false, it will 
//      also check whether the token corresponds to a read-only file, 
//      and return NULL if it is.
//
// Results:
//	A pointer to an OpenResourceFork structure, or NULL.
//
// Side effects:
//	An error message may be left in resultPtr.
//
// ----------------------------------------------------------------------

OpenResourceFork *
TclRes_GetResourceRefFromObj(
	register Tcl_Obj *objPtr,	// String obj containing file token
	int okayOnReadOnly,         // Whether this operation is okay for a 
								//   read only file.
	const char *operation,      // String containing the operation we were 
								//   trying to perform, used for errors
	Tcl_Obj *resultPtr)         // Tcl_Obj to contain error message
{
	OpenResourceFork *	resourceRef;
	char *				stringPtr;
	Tcl_HashEntry *		nameHashPtr;
	int					length;
	OSErr				err;
	
	stringPtr = Tcl_GetStringFromObj(objPtr, &length);
	nameHashPtr = Tcl_FindHashEntry(&nameTable, stringPtr);
	if (nameHashPtr == NULL) {
		Tcl_AppendStringsToObj(resultPtr,
			"invalid resource file reference \"", stringPtr, "\"", (char *) NULL);
		return NULL;
	}

	resourceRef = (OpenResourceFork *) Tcl_GetHashValue(nameHashPtr);
	
	if (!okayOnReadOnly) {
		err = GetResFileAttrs(resourceRef->fileRef);
		if (err & mapReadOnly) {
			Tcl_AppendStringsToObj(resultPtr, "cannot ", operation, " resource file \"", 
								   stringPtr, "\", it was opened read only", (char *) NULL);
			return NULL;
		}
	}
	return resourceRef;
}


// ----------------------------------------------------------------------
//
// TclRes_RegisterResourceFork --
//
//	Register an open resource fork in the table of open resources 
//	managed by the procedures in this file.  If the resource file
//  is already registered with the table, then no new token is made.
//
//  The behavior is controlled by the value of tokenPtr, and of the 
//	flags variable.  
//	
//	For tokenPtr, the possibilities are:
//	  * NULL: the new token is auto-generated, but not returned.
//    * The string value of tokenPtr is the empty string: then
//		the new token is auto-generated, and returned in tokenPtr.
//	  * tokenPtr has a value: the string value will be used for the token,
//		unless it is already in use, in which case a new token will
//		be generated, and returned in tokenPtr.
//
//  For the flags variable, it can be one of:
//	  * fork_InsertTail: the element is inserted at the
//              end of the list of open resources.  Used only in Resource_Init.
//	  * fork_dontclose: the [resource close] command will not close
//	        this resource.
//	  * fork_CheckIfOpen: this will check to see if this file's
//	        resource fork is already opened by this Tcl shell, and return 
//	        an error without registering the resource fork.
//
// Results:
//	Standard Tcl Result
//
// Side effects:
//	An entry may be added to the resource name table.
//
// ----------------------------------------------------------------------

int
TclRes_RegisterResourceFork(
	short fileRef,        	// File ref for an open resource fork.
	Tcl_Obj * tokenPtr,		// A Tcl Object to which to write the new token
	int whichFork, 			// The fork in which the resource map has been found
	int flags)	     		// 1 means insert at the head of the resource
							//    fork list, 0 means at the tail
{
	OpenResourceFork *	resourceRef;
	Tcl_HashEntry *		resourceHashPtr = NULL;
	Tcl_HashEntry *		nameHashPtr;
	char *				resourceId = NULL;
	int 				new;
	
	if (!initialized) {
		TclRes_InitializeTables();
	}
	
	// If we were asked to, check that this file has not been opened
	// already with a different permission. If it has, then return an error.
	new = 1;
	if (flags & fork_CheckIfOpen) {
		Tcl_HashSearch	search;
		short			oldFileRef, filePermissionFlag;
		FCBPBRec		newFileRec, oldFileRec;
		OSErr			err;
		
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
			oldFileRef = (short) Tcl_GetHashKey(&resourceTable, resourceHashPtr);
			if (oldFileRef == fileRef) {
				new = 0;
				break;
			}
			oldFileRec.ioVRefNum = 0;
			oldFileRec.ioRefNum = oldFileRef;
			err = PBGetFCBInfo(&oldFileRec, false);
			
			// err might not be noErr either because the file has closed 
			// out from under us somehow, which is bad but we're not going
			// to fix it here, OR because it is the ROM MAP, which has a 
			// fileRef, but can't be gotten to by PBGetFCBInfo.
			if ((err == noErr) 
				&& (newFileRec.ioFCBVRefNum == oldFileRec.ioFCBVRefNum)
				&& (newFileRec.ioFCBFlNm == oldFileRec.ioFCBFlNm)) {
				// In MacOS 8.1 it seems like we get different file refs even
				// though we pass the same file & permissions.  This is not
				// what Inside Mac says should happen, but it does, so if it
				// does, then close the new res file and return the original one...
				if (filePermissionFlag == ((oldFileRec.ioFCBFlags >> 12) & 0x1)) {
					CloseResFile(fileRef);
					new = 0;
					break;
				} else {
					if (tokenPtr != NULL) {
						Tcl_SetStringObj(tokenPtr, "resource already opened with different permission", -1);
					}   	
					return TCL_ERROR;
				}
			}
			resourceHashPtr = Tcl_NextHashEntry(&search);
		}
	}
	
	// If the file has already been opened with these same permissions, then
	// it will be in our list and we will have set new to 0 above. So we will
	// just return the token (if tokenPtr is non-null).
	if (new) {
		resourceHashPtr = Tcl_CreateHashEntry(&resourceTable, (char *) fileRef, &new);
	} else {
		if (tokenPtr != NULL) {   
			resourceId = (char *) Tcl_GetHashValue(resourceHashPtr);
			Tcl_SetStringObj(tokenPtr, resourceId, -1);
		}
		return TCL_OK;
	}        
		
	// If we were passed in a result pointer which is not an empty string,
	// attempt to use that as the key. If the key already exists, silently
	// fall back on "resource%d"...
	if (tokenPtr != NULL) {
		char *	tokenVal;
		int		length;
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
	
	if (flags & fork_InsertTail) {
		Tcl_ListObjAppendElement(NULL, resourceForkList, tokenPtr);
	} else {
		Tcl_ListObjReplace(NULL, resourceForkList, 0, 0, 1, &tokenPtr);	
	}
	return TCL_OK;
}
	
	
// ----------------------------------------------------------------------
//
// TclRes_UnRegisterResourceFork --
//
//	Removes the entry for an open resource fork from the table of 
//	open resources managed by the procedures in this file.
//      If resultPtr is not NULL, it will be used for error reporting.
//
// Results:
//	The fileRef for this token, or -1 if an error occured.
//
// Side effects:
//	An entry is removed from the resource name table.
//
// ----------------------------------------------------------------------

short
TclRes_UnRegisterResourceFork(
	char * tokenPtr,
	Tcl_Obj * resultPtr)

{
	OpenResourceFork *	resourceRef;
	Tcl_HashEntry *		resourceHashPtr;
	Tcl_HashEntry *		nameHashPtr;
	Tcl_Obj **			elemPtrs;
	short				fileRef;
	char *				bytes;
	int 				i, match = 0, index, listLen, length, elemLen;
	
	nameHashPtr = Tcl_FindHashEntry(&nameTable, tokenPtr);
	if (nameHashPtr == NULL) {
		if (resultPtr != NULL) {
			Tcl_AppendStringsToObj(resultPtr, "invalid resource file reference \"", 
								   tokenPtr, "\"", (char *) NULL);
		}
		return -1;
	}
	
	resourceRef = (OpenResourceFork *) Tcl_GetHashValue(nameHashPtr);
	fileRef = resourceRef->fileRef;
	
	if ( resourceRef->flags & fork_DontClose ) {
		if (resultPtr != NULL) {
			Tcl_AppendStringsToObj(resultPtr, "not allowed to close \"", 
								   tokenPtr, "\" resource file", (char *) NULL);
		}
		return -1;
	}            
	
	Tcl_DeleteHashEntry(nameHashPtr);
	ckfree((char *) resourceRef);
	
	// Now remove the resource from the resourceForkList object
	Tcl_ListObjGetElements(NULL, resourceForkList, &listLen, &elemPtrs);
	index = -1;
	length = strlen(tokenPtr);
	
	for (i = 0; i < listLen; i++) {
		match = 0;
		bytes = Tcl_GetStringFromObj(elemPtrs[i], &elemLen);
		if (length == elemLen) {
			match = (memcmp(bytes, tokenPtr, (size_t) length) == 0);
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


// ----------------------------------------------------------------------
//
// TclRes_BuildResourceForkList --
//
//	Traverses the list of open resource forks, and builds the 
//	list of resources forks.  Also creates a resource token for any that 
//      are opened but not registered with our resource system.
//      This is based on code from Apple DTS.
//      
//	This code had to be redefined on OSX because some low-memory 
//	accessor functions it used in its OS8/9 incarnation are
//	now obsolete (LMGetTopMapHndl and LMGetSysMapHndl). Using
//	GetTopResourceFile() and GetNextResourceFile() instead.
//
// Results:
//	None.
//
// Side effects:
//      The list of resource forks is updated.
//	The resource name table may be augmented.
//
// ----------------------------------------------------------------------

void
TclRes_BuildResourceForkList()
{
	FCBPBRec	fileRec;
	char		fileName[256];
	char *		s;
	Tcl_Obj *	nameObj;
	OSErr		err;
	FSSpec				fileSpec;
	SInt16				curRefNum, nextRefNum;
#ifndef TCLRESOURCE_DONT_USE_CARBON
	char		appName[256];
	ProcessSerialNumber psn;
	ProcessInfoRec		info;
	
	// Get the application name, so we can substitute
	// the token "application" for the application's resource. 
	GetCurrentProcess(&psn);
	info.processInfoLength = sizeof(ProcessInfoRec);
	info.processName = (StringPtr) &appName;
	info.processAppSpec = &fileSpec;
	GetProcessInformation(&psn, &info);
	p2cstr((StringPtr) appName);
#endif
	
	fileRec.ioCompletion = NULL;
	fileRec.ioVRefNum = 0;
	fileRec.ioFCBIndx = 0;
	fileRec.ioNamePtr = (StringPtr) &fileName;
	
	err = GetTopResourceFile(&nextRefNum);
	
	if (err==noErr) {
		while (nextRefNum != 0) {
			curRefNum = nextRefNum;
			
			// Now do the ones opened after the application
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
#ifndef TCLRESOURCE_DONT_USE_CARBON
				if (strcmp(fileName,appName) == 0) {
					Tcl_SetStringObj(nameObj, "application", -1);
				} else
#endif
				{
					Tcl_SetStringObj(nameObj, fileName, -1);
				}
				c2pstr(fileName);
			}
			
			TclRes_RegisterResourceFork(fileRec.ioRefNum, nameObj, 
									   from_unspecified, fork_DontClose | fork_InsertTail);
			
			GetNextResourceFile(curRefNum, &nextRefNum);
		}
	} 
}


// ----------------------------------------------------------------------
//
// TclRes_NewOSTypeObj --
//
//	This procedure is used to create a new resource name type object.
//
// Results:
//	The newly created object is returned. This object will have a NULL
//	string representation. The returned object has ref count 0.
//
// Side effects:
//	None.
//
// ----------------------------------------------------------------------

Tcl_Obj *
TclRes_NewOSTypeObj(
	OSType newOSType)		// Int used to initialize the new object
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


// ----------------------------------------------------------------------
//
// TclRes_SetOSTypeObj --
//
//	Modify an object to be a resource type and to have the 
//	specified long value.
//
// Results:
//	None.
//
// Side effects:
//	The object's old string rep, if any, is freed. Also, any old
//	internal rep is freed. 
//
// ----------------------------------------------------------------------

void
TclRes_SetOSTypeObj(
	Tcl_Obj *objPtr,		// Object whose internal rep to init.
	OSType newOSType)		// Integer used to set object's value.
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


// ----------------------------------------------------------------------
//
// TclRes_GetOSTypeFromObj --
//
//	Attempt to return an int from the Tcl object "objPtr". If the object
//	is not already an int, an attempt will be made to convert it to one.
//
// Results:
//	The return value is a standard Tcl object result. If an error occurs
//	during conversion, an error message is left in interp->objResult
//	unless "interp" is NULL.
//
// Side effects:
//	If the object is not already an int, the conversion will free
//	any old internal representation.
//
// ----------------------------------------------------------------------

int
TclRes_GetOSTypeFromObj(
	Tcl_Interp *interp, 	// Used for error reporting if not NULL
	Tcl_Obj *objPtr,		// The object from which to get a int
	OSType *osTypePtr)		// Place to store resulting int
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
	
	result = TclRes_SetOSTypeFromAny(interp, objPtr);
	if (result == TCL_OK) {
		*osTypePtr = objPtr->internalRep.longValue;
	}
	return result;
}


// ----------------------------------------------------------------------
//
// TclRes_DupOSTypeInternalRep --
//
//	Initialize the internal representation of an int Tcl_Obj to a
//	copy of the internal representation of an existing int object. 
//
// Results:
//	None.
//
// Side effects:
//	"copyPtr"s internal rep is set to the integer corresponding to
//	"srcPtr"s internal rep.
//
// ----------------------------------------------------------------------

static void
TclRes_DupOSTypeInternalRep(
	Tcl_Obj *srcPtr,	// Object with internal rep to copy
	Tcl_Obj *copyPtr)	// Object with internal rep to set
{
	copyPtr->internalRep.longValue = srcPtr->internalRep.longValue;
	copyPtr->typePtr = &osType;
}


// ----------------------------------------------------------------------
//
// TclRes_SetOSTypeFromAny --
//
//	Attempt to generate an integer internal form for the Tcl object
//	"objPtr".
//
// Results:
//	The return value is a standard object Tcl result. If an error occurs
//	during conversion, an error message is left in interp->objResult
//	unless "interp" is NULL.
//
// Side effects:
//	If no error occurs, an int is stored as "objPtr"s internal
//	representation. 
//
// ----------------------------------------------------------------------

static int
TclRes_SetOSTypeFromAny(
	Tcl_Interp *interp,		// Used for error reporting if not NULL
	Tcl_Obj *objPtr)		// The object to convert
{
	Tcl_ObjType *oldTypePtr = objPtr->typePtr;
	char *string;
	int length;
	OSType newOSType = 0UL;
	Tcl_DString ds;
	
	// Get the string representation. Make it up-to-date if necessary.
	string = Tcl_GetStringFromObj(objPtr, &length);
	Tcl_UtfToExternalDString(NULL, string, length, &ds);
	
	if (Tcl_DStringLength(&ds) > sizeof(OSType)) {
		if (interp != NULL) {
			Tcl_ResetResult(interp);
			Tcl_AppendStringsToObj(Tcl_GetObjResult(interp), "expected Macintosh OS type but got \"", string, "\"", (char *) NULL);
		}
		Tcl_DStringFree(&ds);
		return TCL_ERROR;
	}
	memcpy(&newOSType, Tcl_DStringValue(&ds), (size_t) Tcl_DStringLength(&ds));
	Tcl_DStringFree(&ds);
	
	// The conversion to resource type succeeded. Free the old internalRep 
	// before setting the new one.
	if ((oldTypePtr != NULL) &&	(oldTypePtr->freeIntRepProc != NULL)) {
		oldTypePtr->freeIntRepProc(objPtr);
	}
	
	objPtr->internalRep.longValue = newOSType;
	objPtr->typePtr = &osType;
	return TCL_OK;
}


// ----------------------------------------------------------------------
//
// TclRes_UpdateStringOfOSType --
//
//	Update the string representation for an resource type object.
//	Note: This procedure does not free an existing old string rep
//	so storage will be lost if this has not already been done. 
//
// Results:
//	None.
//
// Side effects:
//	The object's string is set to a valid string that results from
//	the int-to-string conversion.
//
// ----------------------------------------------------------------------

static void
TclRes_UpdateStringOfOSType(
	register Tcl_Obj *objPtr)	// Int object whose string rep to update.
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


