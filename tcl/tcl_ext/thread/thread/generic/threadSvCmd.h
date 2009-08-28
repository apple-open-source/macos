/* 
 * This is the header file for the module that implements shared variables.
 * for protected multithreaded access.
 *
 * Copyright (c) 2002 by Zoran Vasiljevic.
 *
 * See the file "license.txt" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Rcsid: @(#)$Id: threadSvCmd.h,v 1.14 2005/01/03 09:00:06 vasiljevic Exp $
 * ---------------------------------------------------------------------------
 */

#ifndef _SV_H_
#define _SV_H_

#include <tcl.h>
#include <ctype.h>
#include <string.h>

#include "threadSpCmd.h" /* For recursive locks */

/*
 * Uncomment following line to get command-line
 * compatibility with AOLserver nsv_* commands
 */

/* #define NSV_COMPAT 1 */

/*
 * Uncomment following line to force command-line
 * compatibility with older thread::sv_ commands
 * If you leave it commented-out, the older style
 * command is going to be included in addition to
 * the new tsv::* style.
 */

/* #define OLD_COMPAT 1 */

#ifdef NS_AOLSERVER
# ifdef NSV_COMPAT
#  define N "nsv_"  /* Compatiblity prefix for AOLserver */
# else
#  define N "sv_"   /* Regular command prefix for AOLserver */
# endif
#else
# ifdef OLD_COMPAT
#  define N  "thread::sv_" /* Old command prefix for Tcl */
# else
#  define N  "tsv::" /* Regular command prefix for Tcl */
# endif
#endif

/*
 * Used when creating arrays/variables
 */

#define FLAGS_CREATEARRAY  1   /* Create the array in bucket if none found */
#define FLAGS_NOERRMSG     2   /* Do not format error message */
#define FLAGS_CREATEVAR    4   /* Create the array variable if none found */

/*
 * Macros for handling locking and unlocking
 */
#define LOCK_BUCKET(a)      Sp_RecursiveMutexLock(&(a)->lock)
#define UNLOCK_BUCKET(a)    Sp_RecursiveMutexUnlock(&(a)->lock)

#define LOCK_CONTAINER(a)   Sp_RecursiveMutexLock(&(a)->bucketPtr->lock)
#define UNLOCK_CONTAINER(a) Sp_RecursiveMutexUnlock(&(a)->bucketPtr->lock)

/*
 * This is named synetrically to LockArray as function
 * rather than as a macro just to improve readability.
 */

#define UnlockArray(a) UNLOCK_CONTAINER(a)

/*
 * Mode for Sv_PutContainer, so it knows what
 * happened with the embedded shared object.
 */

#define SV_UNCHANGED       0   /* Object has not been modified */
#define SV_CHANGED         1   /* Object has been modified */
#define SV_ERROR          -1   /* Object may be in incosistent state */

/*
 * Definitions of functions implementing simple key/value 
 * persistent storage for shared variable arrays.
 */

typedef ClientData (ps_open_proc)(const char*);

typedef int (ps_get_proc)   (ClientData, const char*, char**, int*);
typedef int (ps_put_proc)   (ClientData, const char*, char*, int);
typedef int (ps_first_proc) (ClientData, char**, char**, int*);
typedef int (ps_next_proc)  (ClientData, char**, char**, int*);
typedef int (ps_delete_proc)(ClientData, const char*);
typedef int (ps_close_proc) (ClientData);
typedef void(ps_free_proc)  (char*);

typedef char* (ps_geterr_proc)(ClientData);

/*
 * This structure maintains a bunch of pointers to functions implementing
 * the simple persistence layer for the shared variable arrays. 
 */

typedef struct PsStore {
    char *type;                /* Type identifier of the persistent storage */
    ClientData psHandle;       /* Handle to the opened storage */
    ps_open_proc   *psOpen;    /* Function to open the persistent key store */
    ps_get_proc    *psGet;     /* Function to retrieve value bound to key */
    ps_put_proc    *psPut;     /* Function to store user key and value */
    ps_first_proc  *psFirst;   /* Function to retrieve the first key/value */
    ps_next_proc   *psNext;    /* Function to retrieve the next key/value */
    ps_delete_proc *psDelete;  /* Function to delete user key and value */
    ps_close_proc  *psClose;   /* Function to close the persistent store */
    ps_free_proc   *psFree;    /* Fuction to free allocated memory */
    ps_geterr_proc *psError;   /* Function to return last store error */
    struct PsStore *nextPtr;   /* For linking into linked lists */
} PsStore;

/*
 * The following structure defines a collection of arrays.
 * Only the arrays within a given bucket share a lock, 
 * allowing for more concurency.
 */

typedef struct Bucket {
    Sp_RecursiveMutex lock;    /* */
    Tcl_ThreadId lockt;        /* Thread holding the lock */
    Tcl_HashTable arrays;      /* Hash table of all arrays in bucket */
    Tcl_HashTable handles;     /* Hash table of given-out handles in bucket */
    struct Container *freeCt;  /* List of free Tcl-object containers */
} Bucket;

/*
 * The following structure maintains the context for each variable array.
 */

typedef struct Array {
    char *bindAddr;            /* Array is bound to this address */
    PsStore *psPtr;            /* Persistent storage functions */
    Bucket *bucketPtr;         /* Array bucket. */
    Tcl_HashEntry *entryPtr;   /* Entry in bucket array table. */
    Tcl_HashEntry *handlePtr;  /* Entry in handles table */
    Tcl_HashTable vars;        /* Table of variables. */
} Array;

/*
 * The object container for Tcl-objects stored within shared arrays.
 */

typedef struct Container {
    Bucket *bucketPtr;         /* Bucket holding the array below */
    Array *arrayPtr;           /* Array with the object container*/
    Tcl_HashEntry *entryPtr;   /* Entry in array table. */
    Tcl_HashEntry *handlePtr;  /* Entry in handles table */
    Tcl_Obj *tclObj;           /* Tcl object to hold shared values */
    int epoch;                 /* Track object changes */
    char *chunkAddr;           /* Address of one chunk of object containers */
    struct Container *nextPtr; /* Next object container in the free list */
} Container;

/*
 * Structure for generating command names in Tcl
 */

typedef struct SvCmdInfo {
    char *name;                 /* The short name of the command */
    char *cmdName;              /* Real (rewritten) name of the command */
    Tcl_ObjCmdProc *objProcPtr; /* The object-based command procedure */
    Tcl_CmdDeleteProc *delProcPtr; /* Pointer to command delete function */
    ClientData *clientData;     /* Pointer passed to above command */
    struct SvCmdInfo *nextPtr;  /* Next in chain of registered commands */
} SvCmdInfo;

/*
 * Structure for registering special object duplicator functions.
 * Reason for this is that even some regular Tcl duplicators
 * produce shallow instead of proper deep copies of the object.
 * While this is considered to be ok in single-threaded apps,
 * a multithreaded app could have problems when accessing objects
 * which live in (i.e. are accessed from) different interpreters.
 * So, for each object type which should be stored in shared object
 * pools, we must assure that the object is copied properly.
 */

typedef struct RegType {
    Tcl_ObjType *typePtr;       /* Type of the registered object */
    Tcl_DupInternalRepProc *dupIntRepProc; /* Special deep-copy duper */
    struct RegType *nextPtr;    /* Next in chain of registered types */
} RegType;

/*
 * Limited API functions
 */

void 
Sv_RegisterCommand(char*,Tcl_ObjCmdProc*,Tcl_CmdDeleteProc*,ClientData);

void 
Sv_RegisterObjType(Tcl_ObjType*, Tcl_DupInternalRepProc*);

void 
Sv_RegisterPsStore(PsStore*);

int
Sv_GetContainer(Tcl_Interp*,int,Tcl_Obj*CONST objv[],Container**,int*,int);

int
Sv_PutContainer(Tcl_Interp*, Container*, int);

/*
 * Private version of Tcl_DuplicateObj which takes care about
 * copying objects when loaded to and retrieved from shared array.
 */

Tcl_Obj* Sv_DuplicateObj(Tcl_Obj*);

#endif /* _SV_H_ */

/* EOF $RCSfile: threadSvCmd.h,v $ */

/* Emacs Setup Variables */
/* Local Variables:      */
/* mode: C               */
/* indent-tabs-mode: nil */
/* c-basic-offset: 4     */
/* End:                  */

