/* 
 * vfs.c --
 *
 *	This file contains the implementation of the Vfs extension
 *	to Tcl.  It provides a script level interface to Tcl's 
 *	virtual file system support, and therefore allows 
 *	vfs's to be implemented in Tcl.
 *	
 *	Some of this file could be used as a basis for a hard-coded
 *	vfs implemented in C (e.g. a zipvfs).
 *	
 *	The code is thread-safe.  Although under normal use only
 *	one interpreter will be used to add/remove mounts and volumes,
 *	it does cope with multiple interpreters in multiple threads.
 *	
 * Copyright (c) 2001-2004 Vince Darley.
 * Copyright (c) 2006 ActiveState Software Inc.
 * 
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include <tcl.h>
/* Required to access the 'stat' structure fields, and TclInExit() */
#include "tclInt.h"
#include "tclPort.h"

/*
 * Windows needs to know which symbols to export.  Unix does not.
 * BUILD_vfs should be undefined for Unix.
 */

#ifdef BUILD_vfs
#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLEXPORT
#endif /* BUILD_vfs */

#ifndef TCL_GLOB_TYPE_MOUNT
#define TCL_GLOB_TYPE_MOUNT		(1<<7)
#endif

/*
 * tclvfs will return this code instead of TCL_OK/ERROR/etc. to propagate
 * through the Tcl_Eval* calls to indicate a posix error has been raised by
 * some vfs implementation.  -1 is what Tcl expects, adopts from posix's
 * standard error value.
 */
#define TCLVFS_POSIXERROR (-1)

#ifndef CONST86
#define CONST86
#endif

/*
 * Only the _Init function is exported.
 */

EXTERN int Vfs_Init _ANSI_ARGS_((Tcl_Interp*));

/* 
 * Functions to add and remove a volume from the list of volumes.
 * These aren't currently exported, but could be in the future.
 */
static void Vfs_AddVolume    _ANSI_ARGS_((Tcl_Obj*));
static int  Vfs_RemoveVolume _ANSI_ARGS_((Tcl_Obj*));

/*
 * struct Vfs_InterpCmd --
 * 
 * Any vfs action which is exposed to Tcl requires both an interpreter
 * and a command prefix for evaluation.  To carry out any filesystem
 * action inside a vfs, this extension will lappend various additional
 * parameters to the command string, evaluate it in the interpreter and
 * then extract the result (the way the result is handled is documented
 * in each individual vfs callback below).
 * 
 * We retain a refCount on the 'mountCmd' object, but there is no need
 * for us to register our interpreter reference, since we will be
 * made invalid when the interpreter disappears.  Also, Tcl_Objs of
 * "path" type which use one of these structures as part of their
 * internal representation also do not need to add to any refCounts,
 * because if this object disappears, all internal representations will
 * be made invalid.
 */

typedef struct Vfs_InterpCmd {
    Tcl_Obj *mountCmd;    /* The Tcl command prefix which will be used
                           * to perform all filesystem actions on this
                           * file. */
    Tcl_Interp *interp;   /* The Tcl interpreter in which the above
                           * command will be evaluated. */
} Vfs_InterpCmd;

/*
 * struct VfsNativeRep --
 * 
 * Structure used for the native representation of a path in a Tcl vfs.
 * To fully specify a file, the string representation is also required.
 * 
 * When a Tcl interpreter is deleted, all mounts whose callbacks
 * are in it are removed and freed.  This also means that the
 * global filesystem epoch that Tcl retains is modified, and all
 * path internal representations are therefore discarded.  Therefore we
 * don't have to worry about vfs files containing stale VfsNativeRep
 * structures (but it also means we mustn't touch the fsCmd field
 * of one of these structures if the interpreter has gone).  This
 * means when we free one of these structures, we just free the
 * memory allocated, and ignore the fsCmd pointer (which may or may
 * not point to valid memory).
 */

typedef struct VfsNativeRep {
    int splitPosition;    /* The index into the string representation
                           * of the file which indicates where the 
                           * vfs filesystem is mounted. */
    Vfs_InterpCmd* fsCmd; /* The Tcl interpreter and command pair
                           * which will be used to perform all filesystem 
                           * actions on this file. */
} VfsNativeRep;

/*
 * struct VfsChannelCleanupInfo --
 * 
 * Structure we use to retain sufficient information about
 * a channel that we can properly clean up all resources
 * when the channel is closed.  This is required when using
 * 'open' on things inside the vfs.
 * 
 * When the channel in question is begin closed, we will
 * temporarily register the channel with the given interpreter,
 * evaluate the closeCallBack, and then detach the channel
 * from the interpreter and return (allowing Tcl to continue
 * closing the channel as normal).
 * 
 * Nothing in the callback can prevent the channel from
 * being closed.
 */

typedef struct VfsChannelCleanupInfo {
    Tcl_Channel channel;    /* The channel which needs cleaning up */
    Tcl_Obj* closeCallback; /* The Tcl command string to evaluate
                             * when the channel is closing, which will
                             * carry out any cleanup that is necessary. */
    Tcl_Interp* interp;     /* The interpreter in which to evaluate the
                             * cleanup operation. */
} VfsChannelCleanupInfo;


/*
 * Forward declarations for procedures defined later in this file:
 */

static int		 VfsFilesystemObjCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int objc, 
			    Tcl_Obj *CONST objv[]));

/* 
 * Now we define the virtual filesystem callbacks.  Note that some
 * of these callbacks are passed a Tcl_Interp for error messages.
 * We will copy over the error messages from the vfs interp to the
 * calling interp.  Currently this is done directly, but we
 * could investigate using 'TclTransferResult' which would allow
 * error traces to be copied over as well.
 */

static Tcl_FSStatProc VfsStat;
static Tcl_FSAccessProc VfsAccess;
static Tcl_FSOpenFileChannelProc VfsOpenFileChannel;
static Tcl_FSMatchInDirectoryProc VfsMatchInDirectory;
static Tcl_FSDeleteFileProc VfsDeleteFile;
static Tcl_FSCreateDirectoryProc VfsCreateDirectory;
static Tcl_FSRemoveDirectoryProc VfsRemoveDirectory; 
static Tcl_FSFileAttrStringsProc VfsFileAttrStrings;
static Tcl_FSFileAttrsGetProc VfsFileAttrsGet;
static Tcl_FSFileAttrsSetProc VfsFileAttrsSet;
static Tcl_FSUtimeProc VfsUtime;
static Tcl_FSPathInFilesystemProc VfsPathInFilesystem;
static Tcl_FSFilesystemPathTypeProc VfsFilesystemPathType;
static Tcl_FSFilesystemSeparatorProc VfsFilesystemSeparator;
static Tcl_FSFreeInternalRepProc VfsFreeInternalRep;
static Tcl_FSDupInternalRepProc VfsDupInternalRep;
static Tcl_FSListVolumesProc VfsListVolumes;

static Tcl_Filesystem vfsFilesystem = {
    "tclvfs",
    sizeof(Tcl_Filesystem),
    TCL_FILESYSTEM_VERSION_1,
    &VfsPathInFilesystem,
    &VfsDupInternalRep,
    &VfsFreeInternalRep,
    /* No internal to normalized, since we don't create any
     * pure 'internal' Tcl_Obj path representations */
    NULL,
    /* No create native rep function, since we don't use it
     * or 'Tcl_FSNewNativePath' */
    NULL,
    /* Normalize path isn't needed - we assume paths only have
     * one representation */
    NULL,
    &VfsFilesystemPathType,
    &VfsFilesystemSeparator,
    &VfsStat,
    &VfsAccess,
    &VfsOpenFileChannel,
    &VfsMatchInDirectory,
    &VfsUtime,
    /* We choose not to support symbolic links inside our vfs's */
    NULL,
    &VfsListVolumes,
    &VfsFileAttrStrings,
    &VfsFileAttrsGet,
    &VfsFileAttrsSet,
    &VfsCreateDirectory,
    &VfsRemoveDirectory, 
    &VfsDeleteFile,
    /* No copy file - fallback will occur at Tcl level */
    NULL,
    /* No rename file - fallback will occur at Tcl level */
    NULL,
    /* No copy directory - fallback will occur at Tcl level */
    NULL, 
    /* Use stat for lstat */
    NULL,
    /* No load - fallback on core implementation */
    NULL,
    /* We don't need a getcwd or chdir - fallback on Tcl's versions */
    NULL,
    NULL
};

/*
 * struct VfsMount --
 * 
 * Each filesystem mount point which is registered will result in
 * the allocation of one of these structures.  They are stored
 * in a linked list whose head is 'listOfMounts'.
 */

typedef struct VfsMount {
    CONST char* mountPoint;
    int mountLen;
    int isVolume;
    Vfs_InterpCmd interpCmd;
    struct VfsMount* nextMount;
} VfsMount;

#define TCL_TSD_INIT(keyPtr)	(ThreadSpecificData *)Tcl_GetThreadData((keyPtr), sizeof(ThreadSpecificData))

/*
 * Declare a thread-specific list of vfs mounts and volumes.
 *
 * Stores the list of volumes registered with the vfs (and therefore
 * also registered with Tcl).  It is maintained as a valid Tcl list at
 * all times, or NULL if there are none (we don't keep it as an empty
 * list just as a slight optimisation to improve Tcl's efficiency in
 * determining whether paths are absolute or relative).
 *
 * We keep a refCount on this object whenever it is non-NULL.
 *
 * internalErrorScript is evaluated when an internal error is detected in
 * a tclvfs implementation.  This is most useful for debugging.
 *
 * When it is not NULL we keep a refCount on it.
 */

typedef struct ThreadSpecificData {
    VfsMount *listOfMounts;
    Tcl_Obj *vfsVolumes;
    Tcl_Obj *internalErrorScript;
} ThreadSpecificData;
static Tcl_ThreadDataKey dataKey;

/* We might wish to consider exporting these in the future */

static int             Vfs_AddMount(Tcl_Obj* mountPoint, int isVolume, 
				    Tcl_Interp *interp, Tcl_Obj* mountCmd);
static int             Vfs_RemoveMount(Tcl_Obj* mountPoint, Tcl_Interp* interp);
static Vfs_InterpCmd*  Vfs_FindMount(Tcl_Obj *pathMount, int mountLen);
static Tcl_Obj*        Vfs_ListMounts(void);
static void            Vfs_UnregisterWithInterp _ANSI_ARGS_((ClientData, 
							     Tcl_Interp*));
static void            Vfs_RegisterWithInterp _ANSI_ARGS_((Tcl_Interp*));

/* Some private helper procedures */

static VfsNativeRep*   VfsGetNativePath(Tcl_Obj* pathPtr);
static Tcl_CloseProc   VfsCloseProc;
static void            VfsExitProc(ClientData clientData);
static void            VfsThreadExitProc(ClientData clientData);
static Tcl_Obj*	       VfsFullyNormalizePath(Tcl_Interp *interp, 
				             Tcl_Obj *pathPtr);
static Tcl_Obj*        VfsBuildCommandForPath(Tcl_Interp **iRef, 
			          CONST char* cmd, Tcl_Obj * pathPtr);
static void            VfsInternalError(Tcl_Interp* interp);

/* 
 * Hard-code platform dependencies.  We do not need to worry 
 * about backslash-separators on windows, because a normalized
 * path will never contain them.
 */
#ifdef MAC_TCL
    #define VFS_SEPARATOR ':'
#else
    #define VFS_SEPARATOR '/'
#endif


/*
 *----------------------------------------------------------------------
 *
 * Vfs_Init --
 *
 *	This procedure is the main initialisation point of the Vfs
 *	extension.
 *
 * Results:
 *	Returns a standard Tcl completion code, and leaves an error
 *	message in the interp's result if an error occurs.
 *
 * Side effects:
 *	Adds a command to the Tcl interpreter.
 *
 *----------------------------------------------------------------------
 */

int
Vfs_Init(interp)
    Tcl_Interp *interp;		/* Interpreter for application. */
{
    if (Tcl_InitStubs(interp, "8.4", 0) == NULL) {
	return TCL_ERROR;
    }
    if (Tcl_PkgRequire(interp, "Tcl", "8.4", 0) == NULL) {
	return TCL_ERROR;
    }
    
    /* 
     * Safe interpreters are not allowed to modify the filesystem!
     * (Since those modifications will affect other interpreters).
     */
    if (Tcl_IsSafe(interp)) {
        return TCL_ERROR;
    }

#ifndef PACKAGE_VERSION
    /* keep in sync with actual version */
#define PACKAGE_VERSION "1.4"
#endif
    if (Tcl_PkgProvide(interp, "vfs", PACKAGE_VERSION) == TCL_ERROR) {
        return TCL_ERROR;
    }

    /*
     * Create 'vfs::filesystem' command, and interpreter-specific
     * initialisation.
     */

    Tcl_CreateObjCommand(interp, "vfs::filesystem", VfsFilesystemObjCmd, 
	    (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
    Vfs_RegisterWithInterp(interp);
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * Vfs_RegisterWithInterp --
 *
 *	Allow the given interpreter to be used to handle vfs callbacks.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May register the entire vfs code (if not previously registered).
 *	Registers some cleanup action for when this interpreter is
 *	deleted.
 *
 *----------------------------------------------------------------------
 */
static void 
Vfs_RegisterWithInterp(interp)
    Tcl_Interp *interp;
{
    ClientData vfsAlreadyRegistered;
    /* 
     * We need to know if the interpreter is deleted, so we can
     * remove all interp-specific mounts.
     */
    Tcl_SetAssocData(interp, "vfs::inUse", (Tcl_InterpDeleteProc*) 
		     Vfs_UnregisterWithInterp, (ClientData) 1);
    /* 
     * Perform one-off registering of our filesystem if that
     * has not happened before.
     */
    vfsAlreadyRegistered = Tcl_FSData(&vfsFilesystem);
    if (vfsAlreadyRegistered == NULL) {
	Tcl_FSRegister((ClientData)1, &vfsFilesystem);
	Tcl_CreateExitHandler(VfsExitProc, (ClientData)NULL);
	Tcl_CreateThreadExitHandler(VfsThreadExitProc, NULL);
    }
}
   

/*
 *----------------------------------------------------------------------
 *
 * Vfs_UnregisterWithInterp --
 *
 *	Remove all of the mount points that this interpreter handles.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static void 
Vfs_UnregisterWithInterp(dummy, interp)
    ClientData dummy;
    Tcl_Interp *interp;
{
    int res = TCL_OK;
    /* Remove all of this interpreters mount points */
    while (res == TCL_OK) {
        res = Vfs_RemoveMount(NULL, interp);
    }
    /* Make sure our assoc data has been deleted */
    Tcl_DeleteAssocData(interp, "vfs::inUse");
}


/*
 *----------------------------------------------------------------------
 *
 * Vfs_AddMount --
 *
 *	Adds a new vfs mount point.  After this call all filesystem
 *	access within that mount point will be redirected to the
 *	interpreter/mountCmd pair.
 *	
 *	This command must not be called unless 'interp' has already
 *	been registered with 'Vfs_RegisterWithInterp' above.  This 
 *	usually happens automatically with a 'package require vfs'.
 *
 * Results:
 *	TCL_OK unless the inputs are bad or a memory allocation
 *	error occurred, or the interpreter is not vfs-registered.
 *
 * Side effects:
 *	A new volume may be added to the list of available volumes.
 *	Future filesystem access inside the mountPoint will be 
 *	redirected.  Tcl is informed that a new mount has been added
 *	and this will make all cached path representations invalid.
 *
 *----------------------------------------------------------------------
 */
static int 
Vfs_AddMount(mountPoint, isVolume, interp, mountCmd)
    Tcl_Obj* mountPoint;
    int isVolume;
    Tcl_Interp* interp;
    Tcl_Obj* mountCmd;
{
    char *strRep;
    int len;
    VfsMount *newMount;
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);
    
    if (mountPoint == NULL || interp == NULL || mountCmd == NULL) {
	return TCL_ERROR;
    }
    /* 
     * Check whether this intepreter can properly clean up
     * mounts on exit.  If not, throw an error.
     */
    if (Tcl_GetAssocData(interp, "vfs::inUse", NULL) == NULL) {
        return TCL_ERROR;
    }
    
    newMount = (VfsMount*) ckalloc(sizeof(VfsMount));
    
    if (newMount == NULL) {
	return TCL_ERROR;
    }
    strRep = Tcl_GetStringFromObj(mountPoint, &len);
    newMount->mountPoint = (char*) ckalloc(1+(unsigned)len);
    newMount->mountLen = len;
    
    if (newMount->mountPoint == NULL) {
	ckfree((char*)newMount);
	return TCL_ERROR;
    }
    
    strcpy((char*)newMount->mountPoint, strRep);
    newMount->interpCmd.mountCmd = mountCmd;
    newMount->interpCmd.interp = interp;
    newMount->isVolume = isVolume;
    Tcl_IncrRefCount(mountCmd);
    
    newMount->nextMount = tsdPtr->listOfMounts;
    tsdPtr->listOfMounts = newMount;

    if (isVolume) {
	Vfs_AddVolume(mountPoint);
    }
    Tcl_FSMountsChanged(&vfsFilesystem);
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * Vfs_RemoveMount --
 *
 *	This procedure searches for a matching mount point and removes
 *	it if one is found.  If 'mountPoint' is given, then both it and
 *	the interpreter must match for a mount point to be removed.
 *	
 *	If 'mountPoint' is NULL, then the first mount point for the
 *	given interpreter is removed (if any).
 *
 * Results:
 *	TCL_OK if a mount was removed, TCL_ERROR otherwise.
 *
 * Side effects:
 *	A volume may be removed from the current list of volumes
 *	(as returned by 'file volumes').  A vfs may be removed from
 *	the filesystem.  If successful, Tcl will be informed that
 *	the list of current mounts has changed, and all cached file
 *	representations will be made invalid.
 *
 *----------------------------------------------------------------------
 */
static int 
Vfs_RemoveMount(mountPoint, interp)
    Tcl_Obj* mountPoint;
    Tcl_Interp *interp;
{
    /* These two are only used if mountPoint is non-NULL */
    char *strRep = NULL;
    int len = 0;
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);
    
    VfsMount *mountIter;
    /* Set to NULL just to avoid warnings */
    VfsMount *lastMount = NULL;
    
    if (mountPoint != NULL) {
	strRep = Tcl_GetStringFromObj(mountPoint, &len);
    }

    mountIter = tsdPtr->listOfMounts;
    
    while (mountIter != NULL) {
	if ((interp == mountIter->interpCmd.interp) 
	    && ((mountPoint == NULL) ||
		(mountIter->mountLen == len && 
		 !strcmp(mountIter->mountPoint, strRep)))) {
	    /* We've found the mount. */
	    if (mountIter == tsdPtr->listOfMounts) {
		tsdPtr->listOfMounts = mountIter->nextMount;
	    } else {
		lastMount->nextMount = mountIter->nextMount;
	    }
	    /* Free the allocated memory */
	    if (mountIter->isVolume) {
		if (mountPoint == NULL) {
		    Tcl_Obj *volObj = Tcl_NewStringObj(mountIter->mountPoint, 
						       mountIter->mountLen);
		    Tcl_IncrRefCount(volObj);
		    Vfs_RemoveVolume(volObj);
		    Tcl_DecrRefCount(volObj);
		} else {
		    Vfs_RemoveVolume(mountPoint);
		}
	    }
	    ckfree((char*)mountIter->mountPoint);
	    Tcl_DecrRefCount(mountIter->interpCmd.mountCmd);
	    ckfree((char*)mountIter);
	    Tcl_FSMountsChanged(&vfsFilesystem);
	    return TCL_OK;
	}
	lastMount = mountIter;
	mountIter = mountIter->nextMount;
    }
    return TCL_ERROR;
}


/*
 *----------------------------------------------------------------------
 *
 * Vfs_FindMount --
 *
 *	This procedure searches all currently mounted paths for one
 *	which matches the given path.  The given path must be the
 *	absolute, normalized, unique representation for the given path.
 *	If 'len' is -1, we use the entire string representation of the
 *	mountPoint, otherwise we treat 'len' as the length of the mount
 *	we are comparing.
 *
 * Results:
 *	Returns the interpreter, command-prefix pair for the given
 *	mount point, if one is found, otherwise NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static Vfs_InterpCmd* 
Vfs_FindMount(pathMount, mountLen)
    Tcl_Obj *pathMount;
    int mountLen;
{
    VfsMount *mountIter;
    char *mountStr;
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);
    
    if (pathMount == NULL) {
	return NULL;
    }
    
    if (mountLen == -1) {
        mountStr = Tcl_GetStringFromObj(pathMount, &mountLen);
    } else {
	mountStr = Tcl_GetString(pathMount);
    }

    mountIter = tsdPtr->listOfMounts;
    while (mountIter != NULL) {
	if (mountIter->mountLen == mountLen && 
	  !strncmp(mountIter->mountPoint, mountStr, (size_t)mountLen)) {
	    Vfs_InterpCmd *ret = &mountIter->interpCmd;
	    return ret;
	}
	mountIter = mountIter->nextMount;
    }
    return NULL;
}


/*
 *----------------------------------------------------------------------
 *
 * Vfs_ListMounts --
 *
 *	Returns a valid Tcl list, with refCount of zero, containing
 *	all currently mounted paths.
 *	
 *----------------------------------------------------------------------
 */
static Tcl_Obj* 
Vfs_ListMounts(void) 
{
    VfsMount *mountIter;
    Tcl_Obj *res = Tcl_NewObj();
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    /* Build list of mounts */
    mountIter = tsdPtr->listOfMounts;
    while (mountIter != NULL) {
	Tcl_Obj* mount = Tcl_NewStringObj(mountIter->mountPoint, 
					  mountIter->mountLen);
	Tcl_ListObjAppendElement(NULL, res, mount);
	mountIter = mountIter->nextMount;
    }
    return res;
}

/*
 *----------------------------------------------------------------------
 *
 * VfsFilesystemObjCmd --
 *
 *	This procedure implements the "vfs::filesystem" command.  It is
 *	used to mount/unmount particular interfaces to new filesystems,
 *	or to query for what is mounted where.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Inserts or removes a filesystem from Tcl's stack.
 *
 *----------------------------------------------------------------------
 */

static int
VfsFilesystemObjCmd(dummy, interp, objc, objv)
    ClientData dummy;
    Tcl_Interp *interp;
    int		objc;
    Tcl_Obj	*CONST objv[];
{
    int index;
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    static CONST char *optionStrings[] = {
	"info", "internalerror", "mount", "unmount", 
	"fullynormalize", "posixerror", 
	NULL
    };
    
    enum options {
	VFS_INFO, VFS_INTERNAL_ERROR, VFS_MOUNT, VFS_UNMOUNT, 
	VFS_NORMALIZE, VFS_POSIXERROR
    };

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "option ?arg ...?");
	return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(interp, objv[1], optionStrings, "option", 0,
	    &index) != TCL_OK) {
	return TCL_ERROR;
    }

    switch ((enum options) index) {
	case VFS_INTERNAL_ERROR: {
	    if (objc > 3) {
		Tcl_WrongNumArgs(interp, 2, objv, "?script?");
		return TCL_ERROR;
	    }
	    if (objc == 2) {
	        /* Return the current script */
		if (tsdPtr->internalErrorScript != NULL) {
		    Tcl_SetObjResult(interp, tsdPtr->internalErrorScript);
		}
	    } else {
		/* Set the script */
		int len;
		if (tsdPtr->internalErrorScript != NULL) {
		    Tcl_DecrRefCount(tsdPtr->internalErrorScript);
		}
		Tcl_GetStringFromObj(objv[2], &len);
		if (len == 0) {
		    /* Clear our script */
		    tsdPtr->internalErrorScript = NULL;
		} else {
		    /* Set it */
		    tsdPtr->internalErrorScript = objv[2];
		    Tcl_IncrRefCount(tsdPtr->internalErrorScript);
		}
	    }
	    return TCL_OK;
	}
	case VFS_POSIXERROR: {
	    int posixError = -1;
	    if (objc != 3) {
		Tcl_WrongNumArgs(interp, 2, objv, "errorcode");
		return TCL_ERROR;
	    }
	    if (Tcl_GetIntFromObj(NULL, objv[2], &posixError) != TCL_OK) {
		return TCL_ERROR;
	    }
	    Tcl_SetErrno(posixError);
	    /*
	     * This special error code propagate to the Tcl_Eval* calls in
	     * other parts of the vfs C code to indicate a posix error
	     * being raised by some vfs implementation.
	     */
	    return TCLVFS_POSIXERROR;
	}
	case VFS_NORMALIZE: {
	    Tcl_Obj *path;
	    if (objc != 3) {
		Tcl_WrongNumArgs(interp, 2, objv, "path");
		return TCL_ERROR;
	    }
	    path = VfsFullyNormalizePath(interp, objv[2]);
	    if (path == NULL) {
		Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
			"not a valid path \"", Tcl_GetString(objv[2]), 
			"\"", (char *) NULL);
	    } else {
		Tcl_SetObjResult(interp, path);
		Tcl_DecrRefCount(path);
		return TCL_OK;
	    }
	}
        case VFS_MOUNT: {
	    if (objc < 4 || objc > 5) {
		Tcl_WrongNumArgs(interp, 1, objv, "mount ?-volume? path cmd");
		return TCL_ERROR;
	    }
	    if (objc == 5) {
		char *option = Tcl_GetString(objv[2]);
		if (strcmp("-volume", option)) {
		    Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
			    "bad option \"", option,
			    "\": must be -volume", (char *) NULL);
		    return TCL_ERROR;
		}
		return Vfs_AddMount(objv[3], 1, interp, objv[4]);
	    } else {
		Tcl_Obj *path;
		int retVal;
		path = VfsFullyNormalizePath(interp, objv[2]);
		retVal = Vfs_AddMount(path, 0, interp, objv[3]);
		if (path != NULL) { Tcl_DecrRefCount(path); }
		return retVal;
	    }
	    break;
	}
	case VFS_INFO: {
	    if (objc > 3) {
		Tcl_WrongNumArgs(interp, 2, objv, "path");
		return TCL_ERROR;
	    }
	    if (objc == 2) {
		Tcl_SetObjResult(interp, Vfs_ListMounts());
	    } else {
		Vfs_InterpCmd *val;
		
		val = Vfs_FindMount(objv[2], -1);
		if (val == NULL) {
		    Tcl_Obj *path;
		    path = VfsFullyNormalizePath(interp, objv[2]);
		    val = Vfs_FindMount(path, -1);
		    Tcl_DecrRefCount(path);
		    if (val == NULL) {
			Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
				"no such mount \"", Tcl_GetString(objv[2]), 
				"\"", (char *) NULL);
			return TCL_ERROR;
		    }
		}
		Tcl_SetObjResult(interp, val->mountCmd);
	    }
	    break;
	}
	case VFS_UNMOUNT: {
	    if (objc != 3) {
		Tcl_WrongNumArgs(interp, 2, objv, "path");
		return TCL_ERROR;
	    }
	    if (Vfs_RemoveMount(objv[2], interp) == TCL_ERROR) {
		Tcl_Obj *path;
		int retVal;
		path = VfsFullyNormalizePath(interp, objv[2]);
		retVal = Vfs_RemoveMount(path, interp);
		Tcl_DecrRefCount(path);
		if (retVal == TCL_ERROR) {
		    Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
			    "no such mount \"", Tcl_GetString(objv[2]), 
			    "\"", (char *) NULL);
		    return TCL_ERROR;
		}
	    }
	    return TCL_OK;
	}
    }
    return TCL_OK;
}

/* Handle an error thrown by a tcl vfs implementation */
static void
VfsInternalError(Tcl_Interp* interp)
{
    if (interp != NULL) {
	ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);
	if (tsdPtr->internalErrorScript != NULL) {
	    Tcl_EvalObjEx(interp, tsdPtr->internalErrorScript,
			  TCL_EVAL_GLOBAL | TCL_EVAL_DIRECT);
	}
    }
}

/* Return fully normalized path owned by the caller */
static Tcl_Obj*
VfsFullyNormalizePath(Tcl_Interp *interp, Tcl_Obj *pathPtr) {
    Tcl_Obj *path;
    int counter = 0;

    Tcl_IncrRefCount(pathPtr);
    while (1) {
	path = Tcl_FSLink(pathPtr,NULL,0);
	if (path == NULL) {
	    break;
	}
	if (Tcl_FSGetPathType(path) != TCL_PATH_ABSOLUTE) {
	    /* 
	     * This is more complex, we need to find the path
	     * relative to the original file, effectively:
	     * 
	     *  file join [file dirname $pathPtr] $path
	     *  
	     * or 
	     * 
	     *  file join $pathPtr .. $path
	     *  
	     * So...
	     */
	    Tcl_Obj *dotdotPtr, *joinedPtr;
	    Tcl_Obj *joinElements[2];
	    
	    dotdotPtr = Tcl_NewStringObj("..",2);
	    Tcl_IncrRefCount(dotdotPtr);
	    
	    joinElements[0] = dotdotPtr;
	    joinElements[1] = path;

	    joinedPtr = Tcl_FSJoinToPath(pathPtr, 2, joinElements);
	    
	    if (joinedPtr != NULL) {
		Tcl_IncrRefCount(joinedPtr);
		Tcl_DecrRefCount(path);
		path = joinedPtr;
	    } else {
		/* We failed, and our action is undefined */
	    }
	    Tcl_DecrRefCount(dotdotPtr);
	}
	Tcl_DecrRefCount(pathPtr);
	pathPtr = path;
	counter++;
	if (counter > 10) {
	    /* Too many links */
	    Tcl_DecrRefCount(pathPtr);
	    return NULL;
	}
    }
    path = Tcl_FSGetNormalizedPath(interp, pathPtr);
    Tcl_IncrRefCount(path);
    Tcl_DecrRefCount(pathPtr);
    return path;
}

/*
 *----------------------------------------------------------------------
 *
 * VfsPathInFilesystem --
 *
 *	Check whether a path is in any of the mounted points in this
 *	vfs.
 *	
 *	If it is in the vfs, set the clientData given to our private
 *	internal representation for a vfs path.
 *	
 * Results:
 *	Returns TCL_OK on success, or 'TCLVFS_POSIXERROR' on failure.
 *	If Tcl is exiting, we always return a failure code.
 *
 * Side effects:
 *	On success, we allocate some memory for our internal
 *	representation structure.  Tcl will call us to free this
 *	when necessary.
 *
 *----------------------------------------------------------------------
 */
static int 
VfsPathInFilesystem(Tcl_Obj *pathPtr, ClientData *clientDataPtr) {
    Tcl_Obj *normedObj;
    int len, splitPosition;
    char *normed;
    VfsNativeRep *nativeRep;
    Vfs_InterpCmd *interpCmd = NULL;
    
    if (TclInExit()) {
	/* 
	 * Even Tcl_FSGetNormalizedPath may fail due to lack of system
	 * encodings, so we just say we can't handle anything if we are
	 * in the middle of the exit sequence.  We could perhaps be
	 * more subtle than this!
	 */
	return TCLVFS_POSIXERROR;
    }

    normedObj = Tcl_FSGetNormalizedPath(NULL, pathPtr);
    if (normedObj == NULL) {
        return TCLVFS_POSIXERROR;
    }
    normed = Tcl_GetStringFromObj(normedObj, &len);
    splitPosition = len;

    /* 
     * Find the most specific mount point for this path.
     * Mount points are specified by unique strings, so
     * we have to use a unique normalised path for the
     * checks here.
     * 
     * Given mount points are paths, 'most specific' means
     * longest path, so we scan from end to beginning
     * checking for valid mount points at each separator.
     */
    while (1) {
	/* 
	 * We need this test here both for an empty string being
	 * passed in above, and so that if we are testing a unix
	 * absolute path /foo/bar we will come around the loop
	 * with splitPosition at 0 for the last iteration, and we
	 * must return then.
	 */
	if (splitPosition == 0) {
	    return TCLVFS_POSIXERROR;
	}
	
	/* Is the path up to 'splitPosition' a valid moint point? */
	interpCmd = Vfs_FindMount(normedObj, splitPosition);
	if (interpCmd != NULL) break;

	while (normed[--splitPosition] != VFS_SEPARATOR) {
	    if (splitPosition == 0) {
		/* 
		 * We've reached the beginning of the string without
		 * finding a mount, so we've failed.
		 */
		return TCLVFS_POSIXERROR;
	    }
	}
	
	/* 
	 * We now know that normed[splitPosition] is a separator.
	 * However, we might have mounted a root filesystem with a
	 * name (for example 'ftp://') which actually includes a
	 * separator.  Therefore we test whether the path with
	 * a separator is a mount point.
	 * 
	 * Since we must have decremented splitPosition at least once
	 * already (above) 'splitPosition+1 <= len' so this won't
	 * access invalid memory.
	 */
	interpCmd = Vfs_FindMount(normedObj, splitPosition+1);
	if (interpCmd != NULL) {
	    splitPosition++;
	    break;
	}
    }
    
    /* 
     * If we reach here we have a valid mount point, since the
     * only way to escape the above loop is through a 'break' when
     * an interpCmd is non-NULL.
     */
    nativeRep = (VfsNativeRep*) ckalloc(sizeof(VfsNativeRep));
    nativeRep->splitPosition = splitPosition;
    nativeRep->fsCmd = interpCmd;
    *clientDataPtr = (ClientData)nativeRep;
    return TCL_OK;
}

/* 
 * Simple helper function to extract the native vfs representation of a
 * path object, or NULL if no such representation exists.
 */
static VfsNativeRep* 
VfsGetNativePath(Tcl_Obj* pathPtr) {
    return (VfsNativeRep*) Tcl_FSGetInternalRep(pathPtr, &vfsFilesystem);
}

static void 
VfsFreeInternalRep(ClientData clientData) {
    VfsNativeRep *nativeRep = (VfsNativeRep*)clientData;
    if (nativeRep != NULL) {
	/* Free the native memory allocation */
	ckfree((char*)nativeRep);
    }
}

static ClientData 
VfsDupInternalRep(ClientData clientData) {
    VfsNativeRep *original = (VfsNativeRep*)clientData;

    VfsNativeRep *nativeRep = (VfsNativeRep*) ckalloc(sizeof(VfsNativeRep));
    nativeRep->splitPosition = original->splitPosition;
    nativeRep->fsCmd = original->fsCmd;
    
    return (ClientData)nativeRep;
}

static Tcl_Obj* 
VfsFilesystemPathType(Tcl_Obj *pathPtr) {
    VfsNativeRep* nativeRep = VfsGetNativePath(pathPtr);
    if (nativeRep == NULL) {
	return NULL;
    } else {
	return nativeRep->fsCmd->mountCmd;
    }
}

static Tcl_Obj*
VfsFilesystemSeparator(Tcl_Obj* pathPtr) {
    char sep=VFS_SEPARATOR;
    return Tcl_NewStringObj(&sep,1);
}

static int
VfsStat(pathPtr, bufPtr)
    Tcl_Obj *pathPtr;		/* Path of file to stat (in current CP). */
    Tcl_StatBuf *bufPtr;	/* Filled with results of stat call. */
{
    Tcl_Obj *mountCmd = NULL;
    Tcl_SavedResult savedResult;
    int returnVal;
    Tcl_Interp* interp;
    
    mountCmd = VfsBuildCommandForPath(&interp, "stat", pathPtr);
    if (mountCmd == NULL) {
	return TCLVFS_POSIXERROR;
    }

    Tcl_SaveResult(interp, &savedResult);
    /* Now we execute this mount point's callback. */
    returnVal = Tcl_EvalObjEx(interp, mountCmd, 
			      TCL_EVAL_GLOBAL | TCL_EVAL_DIRECT);
    if (returnVal == TCL_OK) {
	int statListLength;
	Tcl_Obj* resPtr = Tcl_GetObjResult(interp);
	if (Tcl_ListObjLength(interp, resPtr, &statListLength) == TCL_ERROR) {
	    returnVal = TCL_ERROR;
	} else if (statListLength & 1) {
	    /* It is odd! */
	    returnVal = TCL_ERROR;
	} else {
	    /* 
	     * The st_mode field is set part by the 'mode'
	     * and part by the 'type' stat fields.
	     */
	    bufPtr->st_mode = 0;
	    while (statListLength > 0) {
		Tcl_Obj *field, *val;
		char *fieldName;
		statListLength -= 2;
		Tcl_ListObjIndex(interp, resPtr, statListLength, &field);
		Tcl_ListObjIndex(interp, resPtr, statListLength+1, &val);
		fieldName = Tcl_GetString(field);
		if (!strcmp(fieldName,"dev")) {
		    long v;
		    if (Tcl_GetLongFromObj(interp, val, &v) != TCL_OK) {
			returnVal = TCL_ERROR;
			break;
		    }
		    bufPtr->st_dev = v;
		} else if (!strcmp(fieldName,"ino")) {
		    long v;
		    if (Tcl_GetLongFromObj(interp, val, &v) != TCL_OK) {
			returnVal = TCL_ERROR;
			break;
		    }
		    bufPtr->st_ino = (unsigned short)v;
		} else if (!strcmp(fieldName,"mode")) {
		    int v;
		    if (Tcl_GetIntFromObj(interp, val, &v) != TCL_OK) {
			returnVal = TCL_ERROR;
			break;
		    }
		    bufPtr->st_mode |= v;
		} else if (!strcmp(fieldName,"nlink")) {
		    long v;
		    if (Tcl_GetLongFromObj(interp, val, &v) != TCL_OK) {
			returnVal = TCL_ERROR;
			break;
		    }
		    bufPtr->st_nlink = (short)v;
		} else if (!strcmp(fieldName,"uid")) {
		    long v;
		    if (Tcl_GetLongFromObj(interp, val, &v) != TCL_OK) {
			returnVal = TCL_ERROR;
			break;
		    }
		    bufPtr->st_uid = (short)v;
		} else if (!strcmp(fieldName,"gid")) {
		    long v;
		    if (Tcl_GetLongFromObj(interp, val, &v) != TCL_OK) {
			returnVal = TCL_ERROR;
			break;
		    }
		    bufPtr->st_gid = (short)v;
		} else if (!strcmp(fieldName,"size")) {
		    Tcl_WideInt v;
		    if (Tcl_GetWideIntFromObj(interp, val, &v) != TCL_OK) {
			returnVal = TCL_ERROR;
			break;
		    }
		    bufPtr->st_size = v;
		} else if (!strcmp(fieldName,"atime")) {
		    long v;
		    if (Tcl_GetLongFromObj(interp, val, &v) != TCL_OK) {
			returnVal = TCL_ERROR;
			break;
		    }
		    bufPtr->st_atime = v;
		} else if (!strcmp(fieldName,"mtime")) {
		    long v;
		    if (Tcl_GetLongFromObj(interp, val, &v) != TCL_OK) {
			returnVal = TCL_ERROR;
			break;
		    }
		    bufPtr->st_mtime = v;
		} else if (!strcmp(fieldName,"ctime")) {
		    long v;
		    if (Tcl_GetLongFromObj(interp, val, &v) != TCL_OK) {
			returnVal = TCL_ERROR;
			break;
		    }
		    bufPtr->st_ctime = v;
		} else if (!strcmp(fieldName,"type")) {
		    char *str;
		    str = Tcl_GetString(val);
		    if (!strcmp(str,"directory")) {
			bufPtr->st_mode |= S_IFDIR;
		    } else if (!strcmp(str,"file")) {
			bufPtr->st_mode |= S_IFREG;
#ifdef S_ISLNK
		    } else if (!strcmp(str,"link")) {
			bufPtr->st_mode |= S_IFLNK;
#endif
		    } else {
			/* 
			 * Do nothing.  This means we do not currently
			 * support anything except files and directories
			 */
		    }
		} else {
		    /* Ignore additional stat arguments */
		}
	    }
	}
    }
    
    if (returnVal != TCL_OK && returnVal != TCLVFS_POSIXERROR) {
	VfsInternalError(interp);
    }

    Tcl_RestoreResult(interp, &savedResult);
    Tcl_DecrRefCount(mountCmd);
    
    if (returnVal != TCL_OK && returnVal != TCLVFS_POSIXERROR) {
	Tcl_SetErrno(ENOENT);
        return TCLVFS_POSIXERROR;
    } else {
	return returnVal;
    }
}

static int
VfsAccess(pathPtr, mode)
    Tcl_Obj *pathPtr;		/* Path of file to access (in current CP). */
    int mode;                   /* Permission setting. */
{
    Tcl_Obj *mountCmd = NULL;
    Tcl_SavedResult savedResult;
    int returnVal;
    Tcl_Interp* interp;
    
    mountCmd = VfsBuildCommandForPath(&interp, "access", pathPtr);
    if (mountCmd == NULL) {
	return TCLVFS_POSIXERROR;
    }

    Tcl_ListObjAppendElement(interp, mountCmd, Tcl_NewIntObj(mode));
    /* Now we execute this mount point's callback. */
    Tcl_SaveResult(interp, &savedResult);
    returnVal = Tcl_EvalObjEx(interp, mountCmd, 
			      TCL_EVAL_GLOBAL | TCL_EVAL_DIRECT);
    if (returnVal != TCL_OK && returnVal != TCLVFS_POSIXERROR) {
	VfsInternalError(interp);
    }
    Tcl_RestoreResult(interp, &savedResult);
    Tcl_DecrRefCount(mountCmd);

    if (returnVal != 0) {
	Tcl_SetErrno(ENOENT);
	return TCLVFS_POSIXERROR;
    } else {
	return returnVal;
    }
}

static Tcl_Obj*
VfsGetMode(int mode) {
    Tcl_Obj *ret = Tcl_NewObj();
    if (mode & O_RDONLY) {
        Tcl_AppendToObj(ret, "r", 1);
    } else if (mode & O_WRONLY || mode & O_RDWR) {
	if (mode & O_TRUNC) {
	    Tcl_AppendToObj(ret, "w", 1);
	} else {
	    Tcl_AppendToObj(ret, "a", 1);
	}
	if (mode & O_RDWR) {
	    Tcl_AppendToObj(ret, "+", 1);
	}
    }
    return ret;
}

static Tcl_Channel
VfsOpenFileChannel(cmdInterp, pathPtr, mode, permissions)
    Tcl_Interp *cmdInterp;              /* Interpreter for error reporting;
					 * can be NULL. */
    Tcl_Obj *pathPtr;                   /* Name of file to open. */
    int mode;             		/* POSIX open mode. */
    int permissions;                    /* If the open involves creating a
					 * file, with what modes to create
					 * it? */
{
    Tcl_Channel chan = NULL;
    Tcl_Obj *mountCmd = NULL;
    Tcl_Obj *closeCallback = NULL;
    Tcl_SavedResult savedResult;
    int returnVal;
    Tcl_Interp* interp;
    
    mountCmd = VfsBuildCommandForPath(&interp, "open", pathPtr);
    if (mountCmd == NULL) {
	return NULL;
    }

    Tcl_ListObjAppendElement(interp, mountCmd, VfsGetMode(mode));
    Tcl_ListObjAppendElement(interp, mountCmd, Tcl_NewIntObj(permissions));
    Tcl_SaveResult(interp, &savedResult);
    /* Now we execute this mount point's callback. */
    returnVal = Tcl_EvalObjEx(interp, mountCmd, 
			      TCL_EVAL_GLOBAL | TCL_EVAL_DIRECT);
    if (returnVal == TCL_OK) {
	int reslen;
	Tcl_Obj *resultObj;
	/* 
	 * There may be file channel leaks on these two 
	 * error conditions, if the open command actually
	 * created a channel, but then passed us a bogus list.
	 */
	resultObj =  Tcl_GetObjResult(interp);
	if ((Tcl_ListObjLength(interp, resultObj, &reslen) == TCL_ERROR) 
	  || (reslen > 2) || (reslen == 0)) {
	    returnVal = TCL_ERROR;
	} else {
	    Tcl_Obj *element;
	    Tcl_ListObjIndex(interp, resultObj, 0, &element);
	    chan = Tcl_GetChannel(interp, Tcl_GetString(element), 0);
	    
	    if (chan == NULL) {
	        returnVal = TCL_ERROR;
	    } else {
		if (reslen == 2) {
		    Tcl_ListObjIndex(interp, resultObj, 1, &element);
		    closeCallback = element;
		    Tcl_IncrRefCount(closeCallback);
		}
	    }
	}
	Tcl_RestoreResult(interp, &savedResult);
    } else {
	/* Leave an error message if the cmdInterp is non NULL */
	if (cmdInterp != NULL) {
	    if (returnVal == TCLVFS_POSIXERROR) {
		Tcl_ResetResult(cmdInterp);
		Tcl_AppendResult(cmdInterp, "couldn't open \"", 
				 Tcl_GetString(pathPtr), "\": ",
				 Tcl_PosixError(cmdInterp), (char *) NULL);
	    } else {
		Tcl_Obj* error = Tcl_GetObjResult(interp);
		/* 
		 * Copy over the error message to cmdInterp,
		 * duplicating it in case of threading issues.
		 */
		Tcl_SetObjResult(cmdInterp, Tcl_DuplicateObj(error));
	    }
	} else {
	    /* Report any error, since otherwise it is lost */
	    if (returnVal != TCLVFS_POSIXERROR) {
		VfsInternalError(interp);
	    }
	}
	if (interp == cmdInterp) {
	    /* 
	     * We want our error message to propagate up,
	     * so we want to forget this result
	     */
	    Tcl_DiscardResult(&savedResult);
	} else {
	    Tcl_RestoreResult(interp, &savedResult);
	}
    }

    Tcl_DecrRefCount(mountCmd);

    if (chan != NULL) {
	/*
	 * We got the Channel from some Tcl code.  This means it was
	 * registered with the interpreter.  But we want a pristine
	 * channel which hasn't been registered with anyone.  We use
	 * Tcl_DetachChannel to do this for us.  We must use the
	 * correct interpreter.
	 */
	if (Tcl_IsStandardChannel(chan)) {
	    /*
	     * If we have somehow ended up with a VFS channel being a std
	     * channel, it is likely auto-inherited, which we need to reverse.
	     * [Bug 1468291]
	     */
	    if (chan == Tcl_GetStdChannel(TCL_STDIN)) {
		Tcl_SetStdChannel(NULL, TCL_STDIN);
	    } else if (chan == Tcl_GetStdChannel(TCL_STDOUT)) {
		Tcl_SetStdChannel(NULL, TCL_STDOUT);
	    } else if (chan == Tcl_GetStdChannel(TCL_STDERR)) {
		Tcl_SetStdChannel(NULL, TCL_STDERR);
	    }
	    Tcl_UnregisterChannel(NULL, chan);
	}
	Tcl_DetachChannel(interp, chan);

	if (closeCallback != NULL) {
	    VfsChannelCleanupInfo *channelRet = NULL;
	    channelRet = (VfsChannelCleanupInfo*) 
			    ckalloc(sizeof(VfsChannelCleanupInfo));
	    channelRet->channel = chan;
	    channelRet->interp = interp;
	    channelRet->closeCallback = closeCallback;
	    /* The channelRet structure will be freed in the callback */
	    Tcl_CreateCloseHandler(chan, &VfsCloseProc, 
				   (ClientData)channelRet);
	}
    }
    return chan;
}

/* 
 * IMPORTANT: This procedure must *not* modify the interpreter's result
 * this leads to the objResultPtr being corrupted (somehow), and curious
 * crashes in the future (which are very hard to debug ;-).
 * 
 * This is particularly important since we are evaluating arbitrary
 * Tcl code in the callback.
 * 
 * Also note we are relying on the close-callback to occur just before
 * the channel is about to be properly closed, but after all output
 * has been flushed.  That way we can, in the callback, read in the
 * entire contents of the channel and, say, compress it for storage
 * into a tclkit or zip archive.
 */
static void 
VfsCloseProc(ClientData clientData) {
    VfsChannelCleanupInfo * channelRet = (VfsChannelCleanupInfo*) clientData;
    int returnVal;
    Tcl_SavedResult savedResult;
    Tcl_Channel chan = channelRet->channel;
    Tcl_Interp * interp = channelRet->interp;

    Tcl_SaveResult(interp, &savedResult);

    /* 
     * The interpreter needs to know about the channel, else the Tcl
     * callback will fail, so we register the channel (this allows
     * the Tcl code to use the channel's string-name).
     */
    if (!Tcl_IsStandardChannel(chan)) {
	Tcl_RegisterChannel(interp, chan);
    }

    if (!(Tcl_GetChannelMode(chan) & TCL_READABLE)) {
	/* 
	 * We need to make this channel readable, since tclvfs
	 * documents that close callbacks are allowed to read
	 * from the channels we create.
	 */
	
	/* Currently if we reach here we have a bug */
    }
    
    returnVal = Tcl_EvalObjEx(interp, channelRet->closeCallback, 
		  TCL_EVAL_GLOBAL | TCL_EVAL_DIRECT);
    if (returnVal != TCL_OK) {
	VfsInternalError(interp);
    }
    Tcl_DecrRefCount(channelRet->closeCallback);

    /* 
     * More complications; we can't just unregister the channel,
     * because it is in the middle of being cleaned up, and the cleanup
     * code doesn't like a channel to be closed again while it is
     * already being closed.  So, we do the same trick as above to
     * unregister it without cleanup.
     */
    if (!Tcl_IsStandardChannel(chan)) {
	Tcl_DetachChannel(interp, chan);
    }

    Tcl_RestoreResult(interp, &savedResult);
    ckfree((char*)channelRet);
}

static int
VfsMatchInDirectory(
    Tcl_Interp *cmdInterp,	/* Interpreter to receive error msgs. */
    Tcl_Obj *returnPtr,		/* Object to receive results. */
    Tcl_Obj *dirPtr,	        /* Contains path to directory to search. */
    CONST char *pattern,	/* Pattern to match against. */
    Tcl_GlobTypeData *types)	/* Object containing list of acceptable types.
				 * May be NULL. */
{
    if ((types != NULL) && (types->type & TCL_GLOB_TYPE_MOUNT)) {
	VfsMount *mountIter;
	int len;
	CONST char *prefix;
	ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

	prefix = Tcl_GetStringFromObj(Tcl_FSGetNormalizedPath(NULL, dirPtr), 
				      &len);
	if (prefix[len-1] == '/') {
	    /* 
	     * It's a root directory; we must subtract one for
	     * our comparisons below
	     */
	    len--;
	}

	/* Build list of mounts */
	mountIter = tsdPtr->listOfMounts;
	while (mountIter != NULL) {
	    if (mountIter->mountLen > (len+1) 
		&& !strncmp(mountIter->mountPoint, prefix, (size_t)len) 
		&& mountIter->mountPoint[len] == '/'
		&& strchr(mountIter->mountPoint+len+1, '/') == NULL
		&& Tcl_StringCaseMatch(mountIter->mountPoint+len+1, 
				       pattern, 0)) {
		Tcl_Obj* mount = Tcl_NewStringObj(mountIter->mountPoint, 
						  mountIter->mountLen);
		Tcl_ListObjAppendElement(NULL, returnPtr, mount);
	    }
	    mountIter = mountIter->nextMount;
	}
	return TCL_OK;
    } else {
	Tcl_Obj *mountCmd = NULL;
	Tcl_SavedResult savedResult;
	int returnVal;
	Tcl_Interp* interp;
	int type = 0;
	Tcl_Obj *vfsResultPtr = NULL;
	
	mountCmd = VfsBuildCommandForPath(&interp, "matchindirectory", dirPtr);
	if (mountCmd == NULL) {
	    return TCLVFS_POSIXERROR;
	}

	if (types != NULL) {
	    type = types->type;
	}

	if (pattern == NULL) {
	    Tcl_ListObjAppendElement(interp, mountCmd, Tcl_NewObj());
	} else {
	    Tcl_ListObjAppendElement(interp, mountCmd, 
				     Tcl_NewStringObj(pattern,-1));
	}
	Tcl_ListObjAppendElement(interp, mountCmd, Tcl_NewIntObj(type));
	Tcl_SaveResult(interp, &savedResult);
	/* Now we execute this mount point's callback. */
	returnVal = Tcl_EvalObjEx(interp, mountCmd, 
				  TCL_EVAL_GLOBAL | TCL_EVAL_DIRECT);
	if (returnVal != TCLVFS_POSIXERROR) {
	    vfsResultPtr = Tcl_DuplicateObj(Tcl_GetObjResult(interp));
	}
	Tcl_RestoreResult(interp, &savedResult);
	Tcl_DecrRefCount(mountCmd);

	if (vfsResultPtr != NULL) {
	    if (returnVal == TCL_OK) {
		Tcl_IncrRefCount(vfsResultPtr);
		Tcl_ListObjAppendList(cmdInterp, returnPtr, vfsResultPtr);
		Tcl_DecrRefCount(vfsResultPtr);
	    } else {
		if (cmdInterp != NULL) {
		    Tcl_SetObjResult(cmdInterp, vfsResultPtr);
		} else {
		    Tcl_DecrRefCount(vfsResultPtr);
		}
	    }
	}
	return returnVal;
    }
}

static int
VfsDeleteFile(
    Tcl_Obj *pathPtr)		/* Pathname of file to be removed */
{
    Tcl_Obj *mountCmd = NULL;
    Tcl_SavedResult savedResult;
    int returnVal;
    Tcl_Interp* interp;
    
    mountCmd = VfsBuildCommandForPath(&interp, "deletefile", pathPtr);
    if (mountCmd == NULL) {
	return TCLVFS_POSIXERROR;
    }

    /* Now we execute this mount point's callback. */
    Tcl_SaveResult(interp, &savedResult);
    returnVal = Tcl_EvalObjEx(interp, mountCmd, 
			      TCL_EVAL_GLOBAL | TCL_EVAL_DIRECT);
    if (returnVal != TCL_OK && returnVal != TCLVFS_POSIXERROR) {
	VfsInternalError(interp);
    }
    Tcl_RestoreResult(interp, &savedResult);
    Tcl_DecrRefCount(mountCmd);
    return returnVal;
}

static int
VfsCreateDirectory(
    Tcl_Obj *pathPtr)		/* Pathname of directory to create */
{
    Tcl_Obj *mountCmd = NULL;
    Tcl_SavedResult savedResult;
    int returnVal;
    Tcl_Interp* interp;
    
    mountCmd = VfsBuildCommandForPath(&interp, "createdirectory", pathPtr);
    if (mountCmd == NULL) {
	return TCLVFS_POSIXERROR;
    }

    /* Now we execute this mount point's callback. */
    Tcl_SaveResult(interp, &savedResult);
    returnVal = Tcl_EvalObjEx(interp, mountCmd, 
			      TCL_EVAL_GLOBAL | TCL_EVAL_DIRECT);
    if (returnVal != TCL_OK && returnVal != TCLVFS_POSIXERROR) {
	VfsInternalError(interp);
    }
    Tcl_RestoreResult(interp, &savedResult);
    Tcl_DecrRefCount(mountCmd);
    return returnVal;
}

static int
VfsRemoveDirectory(
    Tcl_Obj *pathPtr,		/* Pathname of directory to be removed
				 * (UTF-8). */
    int recursive,		/* If non-zero, removes directories that
				 * are nonempty.  Otherwise, will only remove
				 * empty directories. */
    Tcl_Obj **errorPtr)	        /* Location to store name of file
				 * causing error. */
{
    Tcl_Obj *mountCmd = NULL;
    Tcl_SavedResult savedResult;
    int returnVal;
    Tcl_Interp* interp;
    
    mountCmd = VfsBuildCommandForPath(&interp, "removedirectory", pathPtr);
    if (mountCmd == NULL) {
	return TCLVFS_POSIXERROR;
    }

    Tcl_ListObjAppendElement(interp, mountCmd, Tcl_NewIntObj(recursive));
    /* Now we execute this mount point's callback. */
    Tcl_SaveResult(interp, &savedResult);
    returnVal = Tcl_EvalObjEx(interp, mountCmd, 
			      TCL_EVAL_GLOBAL | TCL_EVAL_DIRECT);
    if (returnVal != TCL_OK && returnVal != TCLVFS_POSIXERROR) {
	VfsInternalError(interp);
    }
    Tcl_RestoreResult(interp, &savedResult);
    Tcl_DecrRefCount(mountCmd);

    if (returnVal == TCL_ERROR) {
	/* Assume there was a problem with the directory being non-empty */
        if (errorPtr != NULL) {
            *errorPtr = pathPtr;
	    Tcl_IncrRefCount(*errorPtr);
        }
	Tcl_SetErrno(EEXIST);
    }
    return returnVal;
}

static CONST char * CONST86 *
VfsFileAttrStrings(pathPtr, objPtrRef)
    Tcl_Obj* pathPtr;
    Tcl_Obj** objPtrRef;
{
    Tcl_Obj *mountCmd = NULL;
    Tcl_SavedResult savedResult;
    int returnVal;
    Tcl_Interp* interp;
    
    mountCmd = VfsBuildCommandForPath(&interp, "fileattributes", pathPtr);
    if (mountCmd == NULL) {
	*objPtrRef = NULL;
	return NULL;
    }

    Tcl_SaveResult(interp, &savedResult);
    /* Now we execute this mount point's callback. */
    returnVal = Tcl_EvalObjEx(interp, mountCmd, 
			      TCL_EVAL_GLOBAL | TCL_EVAL_DIRECT);
    if (returnVal != TCL_OK && returnVal != TCLVFS_POSIXERROR) {
	VfsInternalError(interp);
    }
    if (returnVal == TCL_OK) {
	*objPtrRef = Tcl_DuplicateObj(Tcl_GetObjResult(interp));
    } else {
	*objPtrRef = NULL;
    }
    Tcl_RestoreResult(interp, &savedResult);
    Tcl_DecrRefCount(mountCmd);
    return NULL;
}

static int
VfsFileAttrsGet(cmdInterp, index, pathPtr, objPtrRef)
    Tcl_Interp *cmdInterp;	/* The interpreter for error reporting. */
    int index;			/* index of the attribute command. */
    Tcl_Obj *pathPtr;		/* filename we are operating on. */
    Tcl_Obj **objPtrRef;	/* for output. */
{
    Tcl_Obj *mountCmd = NULL;
    Tcl_SavedResult savedResult;
    int returnVal;
    Tcl_Interp* interp;
    
    mountCmd = VfsBuildCommandForPath(&interp, "fileattributes", pathPtr);
    if (mountCmd == NULL) {
	return TCLVFS_POSIXERROR;
    }

    Tcl_ListObjAppendElement(interp, mountCmd, Tcl_NewIntObj(index));
    Tcl_SaveResult(interp, &savedResult);
    /* Now we execute this mount point's callback. */
    returnVal = Tcl_EvalObjEx(interp, mountCmd, 
			      TCL_EVAL_GLOBAL | TCL_EVAL_DIRECT);
    if (returnVal != TCLVFS_POSIXERROR) {
	*objPtrRef = Tcl_DuplicateObj(Tcl_GetObjResult(interp));
    }
    Tcl_RestoreResult(interp, &savedResult);
    Tcl_DecrRefCount(mountCmd);
    
    if (returnVal != TCLVFS_POSIXERROR) {
	if (returnVal == TCL_OK) {
	    /* 
	     * Our caller expects a ref count of zero in
	     * the returned object pointer.
	     */
	} else {
	    /* Leave error message in correct interp */
	    if (cmdInterp != NULL) {
		Tcl_SetObjResult(cmdInterp, *objPtrRef);
	    } else {
		Tcl_DecrRefCount(*objPtrRef);
	    }
	    *objPtrRef = NULL;
	}
    } else {
	if (cmdInterp != NULL) {
	    Tcl_ResetResult(cmdInterp);
	    Tcl_AppendResult(cmdInterp, "couldn't read attributes for \"", 
			     Tcl_GetString(pathPtr), "\": ",
			     Tcl_PosixError(cmdInterp), (char *) NULL);
	}
    }
    
    return returnVal;
}

static int
VfsFileAttrsSet(cmdInterp, index, pathPtr, objPtr)
    Tcl_Interp *cmdInterp;	/* The interpreter for error reporting. */
    int index;			/* index of the attribute command. */
    Tcl_Obj *pathPtr;		/* filename we are operating on. */
    Tcl_Obj *objPtr;		/* for input. */
{
    Tcl_Obj *mountCmd = NULL;
    Tcl_SavedResult savedResult;
    int returnVal;
    Tcl_Interp* interp;
    Tcl_Obj *errorPtr = NULL;
    
    mountCmd = VfsBuildCommandForPath(&interp, "fileattributes", pathPtr);
    if (mountCmd == NULL) {
	return TCLVFS_POSIXERROR;
    }

    Tcl_ListObjAppendElement(interp, mountCmd, Tcl_NewIntObj(index));
    Tcl_ListObjAppendElement(interp, mountCmd, objPtr);
    Tcl_SaveResult(interp, &savedResult);
    /* Now we execute this mount point's callback. */
    returnVal = Tcl_EvalObjEx(interp, mountCmd, 
			      TCL_EVAL_GLOBAL | TCL_EVAL_DIRECT);
    if (returnVal != TCLVFS_POSIXERROR && returnVal != TCL_OK) {
	errorPtr = Tcl_DuplicateObj(Tcl_GetObjResult(interp));
    }

    Tcl_RestoreResult(interp, &savedResult);
    Tcl_DecrRefCount(mountCmd);
    
    if (cmdInterp != NULL) {
	if (returnVal == TCLVFS_POSIXERROR) {
	    Tcl_ResetResult(cmdInterp);
	    Tcl_AppendResult(cmdInterp, "couldn't set attributes for \"", 
			     Tcl_GetString(pathPtr), "\": ",
			     Tcl_PosixError(cmdInterp), (char *) NULL);
	} else if (errorPtr != NULL) {
	    /* 
	     * Leave error message in correct interp, errorPtr was
	     * duplicated above, in case of threading issues.
	     */
	    Tcl_SetObjResult(cmdInterp, errorPtr);
	}
    } else if (errorPtr != NULL) {
	Tcl_DecrRefCount(errorPtr);
    }
    return returnVal;
}

static int 
VfsUtime(pathPtr, tval)
    Tcl_Obj* pathPtr;
    struct utimbuf *tval;
{
    Tcl_Obj *mountCmd = NULL;
    Tcl_SavedResult savedResult;
    int returnVal;
    Tcl_Interp* interp;
    
    mountCmd = VfsBuildCommandForPath(&interp, "utime", pathPtr);
    if (mountCmd == NULL) {
	return TCLVFS_POSIXERROR;
    }

    Tcl_ListObjAppendElement(interp, mountCmd, Tcl_NewLongObj(tval->actime));
    Tcl_ListObjAppendElement(interp, mountCmd, Tcl_NewLongObj(tval->modtime));
    /* Now we execute this mount point's callback. */
    Tcl_SaveResult(interp, &savedResult);
    returnVal = Tcl_EvalObjEx(interp, mountCmd, 
			      TCL_EVAL_GLOBAL | TCL_EVAL_DIRECT);
    if (returnVal != TCL_OK && returnVal != TCLVFS_POSIXERROR) {
	VfsInternalError(interp);
    }
    Tcl_RestoreResult(interp, &savedResult);
    Tcl_DecrRefCount(mountCmd);

    return returnVal;
}

static Tcl_Obj*
VfsListVolumes(void)
{
    Tcl_Obj *retVal;
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    if (tsdPtr->vfsVolumes != NULL) {
	Tcl_IncrRefCount(tsdPtr->vfsVolumes);
	retVal = tsdPtr->vfsVolumes;
    } else {
	retVal = NULL;
    }

    return retVal;
}

static void
Vfs_AddVolume(volume)
    Tcl_Obj *volume;
{
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    if (tsdPtr->vfsVolumes == NULL) {
        tsdPtr->vfsVolumes = Tcl_NewObj();
	Tcl_IncrRefCount(tsdPtr->vfsVolumes);
    } else {
#if 0
	if (Tcl_IsShared(tsdPtr->vfsVolumes)) {
	    /* 
	     * Another thread is using this object, so we duplicate the
	     * object and reduce the refCount on the shared one.
	     */
	    Tcl_Obj *oldVols = tsdPtr->vfsVolumes;
	    tsdPtr->vfsVolumes = Tcl_DuplicateObj(oldVols);
	    Tcl_IncrRefCount(tsdPtr->vfsVolumes);
	    Tcl_DecrRefCount(oldVols);
	}
#endif
    }
    Tcl_ListObjAppendElement(NULL, tsdPtr->vfsVolumes, volume);
}

static int
Vfs_RemoveVolume(volume)
    Tcl_Obj *volume;
{
    int i, len;
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    Tcl_ListObjLength(NULL, tsdPtr->vfsVolumes, &len);
    for (i = 0;i < len; i++) {
	Tcl_Obj *vol;
        Tcl_ListObjIndex(NULL, tsdPtr->vfsVolumes, i, &vol);
	if (!strcmp(Tcl_GetString(vol),Tcl_GetString(volume))) {
	    /* It's in the list, at index i */
	    if (len == 1) {
		/* An optimization here */
		Tcl_DecrRefCount(tsdPtr->vfsVolumes);
		tsdPtr->vfsVolumes = NULL;
	    } else {
		/*
		 * Make ourselves the unique owner
		 * XXX: May be unnecessary now that it is tsd
		 */
		if (Tcl_IsShared(tsdPtr->vfsVolumes)) {
		    Tcl_Obj *oldVols = tsdPtr->vfsVolumes;
		    tsdPtr->vfsVolumes = Tcl_DuplicateObj(oldVols);
		    Tcl_IncrRefCount(tsdPtr->vfsVolumes);
		    Tcl_DecrRefCount(oldVols);
		}
		/* Remove the element */
		Tcl_ListObjReplace(NULL, tsdPtr->vfsVolumes, i, 1, 0, NULL);
		return TCL_OK;
	    }
	}
    }

    return TCL_ERROR;
}


/*
 *----------------------------------------------------------------------
 *
 * VfsBuildCommandForPath --
 *
 *	Given a path object which we know belongs to the vfs, and a 
 *	command string (one of the standard filesystem operations
 *	"stat", "matchindirectory" etc), build the standard vfs
 *	Tcl command and arguments to carry out that operation.
 *	
 *	If the command is successfully built, it is returned to the
 *	caller with a refCount of 1.  The caller also needs to know
 *	which Tcl interpreter to evaluate the command in; this
 *	is returned in the 'iRef' provided.
 *	
 *	Each mount-point dictates a command prefix to use for a 
 *	particular file.  We start with that and then add 4 parameters,
 *	as follows:
 *	
 *	(1) the 'cmd' to use
 *	(2) the mount point of this path (which is the portion of the
 *	path string which lies outside the vfs).
 *	(3) the remainder of the path which lies inside the vfs
 *	(4) the original (possibly unnormalized) path string used
 *	in the command.
 *	
 *	Example (i):
 *	
 *	If 'C:/Apps/data.zip' is mounted on top of
 *	itself, then if we do:
 *	
 *	cd C:/Apps
 *	file exists data.zip/foo/bar.txt
 *	
 *	this will lead to:
 *	
 *	<mountcmd> "access" C:/Apps/data.zip foo/bar.txt data.zip/foo/bar.txt
 *	
 *	Example (ii)
 *	
 *	If 'ftp://' is mounted as a new volume,
 *	then 'glob -dir ftp://ftp.scriptics.com *' will lead to:
 *	
 *	<mountcmd> "matchindirectory" ftp:// ftp.scriptics.com \
 *	  ftp://ftp.scriptics.com
 *	  
 *	
 * Results:
 *	Returns a list containing the command, or NULL if an
 *	error occurred.  If the interpreter for this vfs command
 *	is in the process of being deleted, we always return NULL.
 *
 * Side effects:
 *	None except memory allocation.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj* 
VfsBuildCommandForPath(Tcl_Interp **iRef, CONST char* cmd, Tcl_Obj *pathPtr) {
    Tcl_Obj *normed;
    Tcl_Obj *mountCmd;
    int len;
    int splitPosition;
    int dummyLen;
    VfsNativeRep *nativeRep;
    Tcl_Interp *interp;
    
    char *normedString;

    nativeRep = VfsGetNativePath(pathPtr);
    if (nativeRep == NULL) {
	return NULL;
    }
    
    interp = nativeRep->fsCmd->interp;
    
    if (Tcl_InterpDeleted(interp)) {
        return NULL;
    }
    
    splitPosition = nativeRep->splitPosition;
    normed = Tcl_FSGetNormalizedPath(NULL, pathPtr);
    normedString = Tcl_GetStringFromObj(normed, &len);
    
    mountCmd = Tcl_DuplicateObj(nativeRep->fsCmd->mountCmd);
    Tcl_IncrRefCount(mountCmd);
    if (Tcl_ListObjLength(NULL, mountCmd, &dummyLen) == TCL_ERROR) {
	Tcl_DecrRefCount(mountCmd);
	return NULL;
    }
    Tcl_ListObjAppendElement(NULL, mountCmd, Tcl_NewStringObj(cmd,-1));
    if (splitPosition == len) {
	Tcl_ListObjAppendElement(NULL, mountCmd, normed);
	Tcl_ListObjAppendElement(NULL, mountCmd, Tcl_NewStringObj("",0));
    } else {
	Tcl_ListObjAppendElement(NULL, mountCmd, 
		Tcl_NewStringObj(normedString,splitPosition));
	if ((normedString[splitPosition] != VFS_SEPARATOR) 
	    || (VFS_SEPARATOR ==':')) {
	    /* This will occur if we mount 'ftp://' */
	    splitPosition--;
	}
	Tcl_ListObjAppendElement(NULL, mountCmd, 
		Tcl_NewStringObj(normedString+splitPosition+1,
				 len-splitPosition-1));
    }
    Tcl_ListObjAppendElement(NULL, mountCmd, pathPtr);

    if (iRef != NULL) {
        *iRef = interp;
    }
    
    return mountCmd;
}

static void 
VfsExitProc(ClientData clientData)
{
    Tcl_FSUnregister(&vfsFilesystem);
}

static void
VfsThreadExitProc(ClientData clientData)
{
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);
    /*
     * This is probably no longer needed, because each individual
     * interp's cleanup will trigger removal of all volumes which
     * belong to it.
     */
    if (tsdPtr->vfsVolumes != NULL) {
	Tcl_DecrRefCount(tsdPtr->vfsVolumes);
	tsdPtr->vfsVolumes = NULL;
    }
    if (tsdPtr->internalErrorScript != NULL) {
	Tcl_DecrRefCount(tsdPtr->internalErrorScript);
	tsdPtr->internalErrorScript = NULL;
    }
}
