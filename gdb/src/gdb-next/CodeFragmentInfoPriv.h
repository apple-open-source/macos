/*
     File:       CarbonCore/CodeFragmentInfoPriv.h
 
     Contains:   Informational routines of CFM.
 
     Version:    Technology: Forte CFM
                 Release:    Cheetah4I
 
     Copyright:  (c) 1994-2000 by Apple Computer, Inc., all rights reserved.
 
     Bugs?:      For bug reports, consult the following page on
                 the World Wide Web:
 
                     http://developer.apple.com/bugreporter/
 
*/
#ifndef __CODEFRAGMENTINFOPRIV__
#define __CODEFRAGMENTINFOPRIV__

#ifndef __MACTYPES__
#include <MacTypes.h>
#endif

#ifndef __CODEFRAGMENTS__
#include <CodeFragments.h>
#endif

#ifndef __CODEFRAGMENTCONTAINERPRIV__
#include <CodeFragmentContainerPriv.h>
#endif





#if PRAGMA_ONCE
#pragma once
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if PRAGMA_STRUCT_ALIGN
    #pragma options align=mac68k
#elif PRAGMA_STRUCT_PACKPUSH
    #pragma pack(push, 2)
#elif PRAGMA_STRUCT_PACK
    #pragma pack(2)
#endif

/*
   .
   ===========================================================================================
   Notification types and constants
   ================================
*/


/*
   ------------------------------------------------------------------------------------------
   Notification clients get told about the CFM events they are interested in.  There are four
   classes of notifications relating to kernel processes, closures, init/term routines, and
   other stuff.  In each case relevant informational routines are callable, e.g. the process
   information can be obtained from CFragGetProcessInfo at both the process start and finish
   notifications.  The specific notifications are:
    kCFragProcessStartNotify    (Under kCFragProcessNotifyMask)
        A kernel process is being used by CFM for the first time.
    kCFragProcessFinishNotify   (Under kCFragProcessNotifyMask)
        A kernel process is being torn down, CFM will not use it any longer.
    kCFragLoadClosureNotify     (Under kCFragClosureNotifyMask)
        A closure is being prepared, initialization functions (if any) are about to be run.
    kCFragUnloadClosureNotify   (Under kCFragClosureNotifyMask)
        A closure is being released, termination routines (if any) have already been run.
    kCFragInitRoutineNotify     (Under kCFragRoutineNotifyMask)
        A specific initialization function is about to be run.
    kCFragTermRoutineNotify     (Under kCFragRoutineNotifyMask)
        A specific termination routine is about to be run.
    kCFragInitFinishNotify      (Under kCFragRoutineNotifyMask)
        All initialization functions have been run.  If the connection ID is kInvalidID then
        all of them succeeded.  Otherwise the connection ID tells which one failed, and the
        extraInfo field contains the status from it.
    kCFragTermStartNotify       (Under kCFragRoutineNotifyMask)
        A closure is starting to be released, termination routines are about to be run.  The
        connection ID field is kInvalidID.  (This is a routine notification instead of a
        closure notification since it is probably not of general interest.  I.e. only those
        clients willing to get lots of notifications will get this.)
    kCFragForgetAccRsrcNotify   (Under kCFragAccRsrcNotifyMask)
        An accelerated resource has been prepared and is about to be "forgotten".  The old
        and new addresses are the same.
    kCFragRelocateAccRsrcNotify (Under kCFragAccRsrcNotifyMask)
        An accelerated resource has been moved and is being relocated at the new address.  The
        closure ID is kInvalidID, it has already been forgotten.
*/

/*
   *** Is there really enough stuff here for debuggers, the primary client?
   *** Add notifications of other things, such as file registrations.
*/

/* ??? Think about adding a search/load failure notification.*/


typedef UInt8                           CFragNotifyKind;
enum {
                                        /* Values for CFragNotifyKind.*/
                                        /* Events at which notifications are sent.*/
  kCFragProcessStartNotify      = 0x00,
  kCFragProcessFinishNotify     = 0x01,
  kCFragPrepareClosureNotify    = 0x10,
  kCFragReleaseClosureNotify    = 0x11,
  kCFragInitRoutineNotify       = 0x20,
  kCFragTermRoutineNotify       = 0x21,
  kCFragInitFinishNotify        = 0x22,
  kCFragTermStartNotify         = 0x23,
  kCFragForgetAccRsrcNotify     = 0x30,
  kCFragRelocateAccRsrcNotify   = 0x31
};


enum {
                                        /* Options for notification requests.*/
                                        /* Flags for groups of related notification events.*/
  kCFragProcessNotifyMask       = 0x00000001,
  kCFragClosureNotifyMask       = 0x00000002,
  kCFragRoutineNotifyMask       = 0x00000004,
  kCFragAccRsrcNotifyMask       = 0x00000008,
  kCFragAllNotifyGroups         = 0x0000FFFF,
  kCFragBroadcastNotifyMask     = 0x00010000
};


/*
   -----------------------------------------------------------------------------------------
   Different kinds of notification provide different (specific) information about the event.
*/


struct CFragClosureNotifyInfo {
  CFragClosureID      closureID;
};
typedef struct CFragClosureNotifyInfo   CFragClosureNotifyInfo;

struct CFragRoutineNotifyInfo {
  CFragClosureID      closureID;
  CFragConnectionID   connectionID;
};
typedef struct CFragRoutineNotifyInfo   CFragRoutineNotifyInfo;

struct CFragAccRsrcNotifyInfo {
  CFragClosureID      closureID;
  LogicalAddress      oldAddress;
  LogicalAddress      newAddress;
};
typedef struct CFragAccRsrcNotifyInfo   CFragAccRsrcNotifyInfo;

struct CFragNotifyInfo {
  CFragNotifyKind     notifyKind;
  UInt8               reservedA;
  UInt16              reservedB;
  MPProcessID         processID;
  MPTaskID            taskID;
  UInt32              sequence;
  UInt32              extraInfo;
  UInt32              reservedC;
  void *              extension;
  union {
    CFragClosureNotifyInfo  closureInfo;
    CFragRoutineNotifyInfo  routineInfo;
    CFragAccRsrcNotifyInfo  accRsrcInfo;
  }                       u;
};
typedef struct CFragNotifyInfo          CFragNotifyInfo;
enum {
  kCFragNotifyInfoVersion       = 0x00010001
};



typedef UInt8                           CFragNotifyMethod;
enum {
                                        /* Values for CFragNotifyMethod.*/
  kCFragNotifyByMessage         = 0,
  kCFragNotifyByCallback        = 1
};


typedef CALLBACK_API_C( OSStatus , CFragNotifyProc )(const CFragNotifyInfo *notifyInfo, void *refCon);

struct CFragRequestByMessageInfo {
  MPQueueID           notifyQueue;
  void *              refCon;
};
typedef struct CFragRequestByMessageInfo CFragRequestByMessageInfo;

struct CFragRequestByCallbackInfo {
  CFragNotifyProc     notifyProc;
  void *              refCon;
};
typedef struct CFragRequestByCallbackInfo CFragRequestByCallbackInfo;

struct CFragNotifyRequestInfo {
  CFragNotifyMethod   notifyMethod;
  UInt8               reservedA;
  UInt16              reservedB;
  OptionBits          options;
  PBVersion           infoVersion;
  union {
    CFragRequestByMessageInfo  messageNotify;
    CFragRequestByCallbackInfo  callbackNotify;
  }                       u;
};
typedef struct CFragNotifyRequestInfo   CFragNotifyRequestInfo;
enum {
  kCFragNotifyRequestInfoVersion = 0x00010001
};


/*
   .
   ===========================================================================================
   Notification routines
   =====================
*/


/* *** This is preliminary and subject to change!*/


extern OSStatus 
CFragRequestNotifyByMessage(
  MPProcessID   processID,
  MPQueueID     queueID,
  void *        refCon,
  OptionBits    options,
  PBVersion     infoVersion);


extern OSStatus 
CFragCancelNotifyByMessage(
  MPProcessID   processID,
  MPQueueID     queueID,
  void *        refCon);



extern OSStatus 
CFragRequestNotifyByCallback(
  MPProcessID       processID,
  CFragNotifyProc   notifyProc,
  void *            refCon,
  OptionBits        options,
  PBVersion         infoVersion);


extern OSStatus 
CFragCancelNotifyByCallback(
  MPProcessID       processID,
  CFragNotifyProc   notifyProc,
  void *            refCon);



extern OSStatus 
CFragGetNotifyRequestInfo(
  MPProcessID               processID,
  ItemCount                 requestedCount,
  ItemCount                 skipCount,
  ItemCount *               totalCount_o,
  PBVersion                 infoVersion,
  CFragNotifyRequestInfo *  notifyRequests_o);


/*
   .
   ===========================================================================================
   Informational types and constants
   =================================
*/



struct CFragMemoryLocator {
  MPAddressSpaceID    spaceID;
  LogicalAddress      address;
  ByteCount           length;
};
typedef struct CFragMemoryLocator       CFragMemoryLocator;

struct CFragFileBasedLocator {
  FSRef *             fileRef;                /* ! Must match file based forms.*/
};
typedef struct CFragFileBasedLocator    CFragFileBasedLocator;

struct CFragDataForkLocator {
  FSRef *             fileRef;                /* ! Must match file based forms.*/
  ByteCount           offset;
  ByteCount           length;
};
typedef struct CFragDataForkLocator     CFragDataForkLocator;

struct CFragResourceLocator {
  FSRef *             fileRef;                /* ! Must match file based forms.*/
  OSType              rsrcType;
  UInt16              reservedA;
  SInt16              rsrcID;
};
typedef struct CFragResourceLocator     CFragResourceLocator;

struct CFragNamedFragmentLocator {
  CFragUsage          usage;
  UInt8               reservedA;              /* ! Must be zero!*/
  UInt16              reservedB;              /* ! Must be zero!*/
  CFragArchitecture   architecture;
  CFragVersionNumber  preferredVersion;
  CFragVersionNumber  oldestVersion;
  StringPtr           libraryName;
};
typedef struct CFragNamedFragmentLocator CFragNamedFragmentLocator;
enum {
  kCFragLocatorVersion          = 0x00010001
};


struct CFragLocator {
  UInt16              reservedA;
  UInt8               reservedB;
  CFragLocatorKind    where;
  UInt32              reservedC;
  union {
    CFragMemoryLocator  inMemory;
    CFragFileBasedLocator  inFile;
    CFragDataForkLocator  inDataFork;
    CFragResourceLocator  inResource;
    CFragNamedFragmentLocator  asNamed;
  }                       u;
};
typedef struct CFragLocator             CFragLocator;
enum {
  kCFragContainerInfoVersion    = 0x00010001
};


struct CFragContainerInfo {
  CFragContainerID    containerID;
  MPAddressSpaceID    spaceID;
  LogicalAddress      address;
  ByteCount           length;
  CFContHandlerProcs * handlerProcs;
  CFContHandlerRef    handlerRef;
  ItemCount           connectionRefCount;
  ItemCount           sectionCount;
  ItemCount           exportCount;
  UInt8               updateLevel;
  UInt8               reservedA;              /* ! Must be zero!*/
  UInt16              reservedB;              /* ! Must be zero!*/
  CFContLogicalLocation  mainSymbol;
  CFContLogicalLocation  initRoutine;
  CFContLogicalLocation  termRoutine;
  UInt32              reservedC;              /* ! Must be zero!*/
  UInt32              reservedD;              /* ! Must be zero!*/
  Str63               name;
  CFragLocator        locator;
  FSRef               fileRef;                /* ! The locator might point here.*/
};
typedef struct CFragContainerInfo       CFragContainerInfo;
enum {
  kCFragProcessInfoVersion      = 0x00010001
};


struct CFragProcessInfo {
  MPProcessID         processID;
  ItemCount           closureCount;
  ItemCount           connectionCount;
  CFragClosureID      mainClosure;
};
typedef struct CFragProcessInfo         CFragProcessInfo;

typedef SInt8                           CFragConnectionState;
enum {
                                        /* Values for CFragConnectionState.*/
                                        /* *** Think about renumbering these from zero.*/
  kInitializedCFragState        = 3,    /* Fully initialized.*/
  kInInitFuncCFragState         = 2,    /* Running initialization functions.*/
  kResolvedCFragState           = 1,    /* Code & data sections are resolved and relocated.*/
  kNewbornCFragState            = 0,    /* Newly created.*/
  kTermPendingCFragState        = -1,   /* Termination in progress, don't share anymore.*/
  kInTermFuncCFragState         = -2,   /* Running termination functions.*/
  kTerminatedCFragState         = -3    /* Terminated, tearing down internal representation.*/
};


enum {
  kCFragClosureInfoVersion      = 0x00010001
};


struct CFragClosureInfo {
  MPProcessID         processID;
  CFragClosureID      closureID;
  CFragConnectionID   rootConnectionID;
  CFragLoadOptions    options;
  ItemCount           connectionCount;
  CFragConnectionState  state;
  UInt8               reservedA[3];
};
typedef struct CFragClosureInfo         CFragClosureInfo;
enum {
  kCFragHasUnresolvedImportsMask = 0x01,
  kCFragConnectionInfoVersion   = 0x00010001
};


struct CFragConnectionInfo {
  MPProcessID         processID;
  CFragConnectionID   connectionID;
  CFragContainerID    containerID;
  CFContHandlerProcs * handlerProcs;
  CFContHandlerRef    handlerRef;
  ItemCount           closureRefCount;
  ItemCount           rootedClosureCount;
  CFragConnectionID   updateID;
  CFragConnectionState  state;
  UInt8               flags;
  UInt16              reservedA;
};
typedef struct CFragConnectionInfo      CFragConnectionInfo;
enum {
  kCFragSectionInfoVersion      = 0x00010001
};


struct CFragSectionInfo {
  LogicalAddress      address;
  ByteCount           length;
  CFContMemoryAccess  access;
  CFContSectionSharing  sharing;
  CFragConnectionState  state;
  UInt8               reservedA;
  ItemCount           connectionRefCount;
};
typedef struct CFragSectionInfo         CFragSectionInfo;
/*
   .
   ===========================================================================================
   Informational routines
   ======================
*/


/*
   -----------------------------------------------------------------------
   These are routines that return arrays of IDs for various sets of items.
*/


extern OSStatus 
CFragGetContainersInSystem(
  ItemCount           requestedCount,
  ItemCount           skipCount,
  ItemCount *         totalCount_o,         /* can be NULL */
  CFragContainerID *  containerIDs_o);      /* can be NULL */


extern OSStatus 
CFragGetClosuresInProcess(
  MPProcessID       processID,
  ItemCount         requestedCount,
  ItemCount         skipCount,
  ItemCount *       totalCount_o,         /* can be NULL */
  CFragClosureID *  closureIDs_o);        /* can be NULL */


extern OSStatus 
CFragGetConnectionsInProcess(
  MPProcessID          processID,
  ItemCount            requestedCount,
  ItemCount            skipCount,
  ItemCount *          totalCount_o,          /* can be NULL */
  CFragConnectionID *  connectionIDs_o);      /* can be NULL */


extern OSStatus 
CFragGetConnectionsInClosure(
  CFragClosureID       closureID,
  ItemCount            requestedCount,
  ItemCount            skipCount,
  ItemCount *          totalCount_o,          /* can be NULL */
  CFragConnectionID *  connectionIDs_o);      /* can be NULL */


extern OSStatus 
CFragGetConnectionsToContainer(
  CFragContainerID     containerID,
  MPProcessID          processID,
  ItemCount            requestedCount,
  ItemCount            skipCount,
  ItemCount *          totalCount_o,          /* can be NULL */
  CFragConnectionID *  connectionIDs_o);      /* can be NULL */



/*
   -------------------------------------------------------------------
   These are routines that return specific information about one item.
*/


extern OSStatus 
CFragGetContainerInfo(
  CFragContainerID      containerID,
  PBVersion             version,
  CFragContainerInfo *  info_o);


extern OSStatus 
CFragGetProcessInfo(
  MPProcessID         processID,
  PBVersion           version,
  CFragProcessInfo *  info_o);


extern OSStatus 
CFragGetClosureInfo(
  CFragClosureID      closureID,
  PBVersion           version,
  CFragClosureInfo *  info_o);


extern OSStatus 
CFragGetConnectionInfo(
  CFragConnectionID      connectionID,
  PBVersion              version,
  CFragConnectionInfo *  info_o);


extern OSStatus 
CFragGetSectionInfo(
  CFragConnectionID   connectionID,
  ItemCount           sectionIndex,
  PBVersion           version,
  CFragSectionInfo *  info_o);



/*
   ------------------------------------------------------
   Returns information about an arbitrary memory address.
*/

extern OSStatus 
CFragFindOwnerOfAddress(
  MPAddressSpaceID     spaceID,
  LogicalAddress       address,
  MPProcessID          processHint,
  CFragContainerID *   containerID_o,        /* can be NULL */
  MPProcessID *        processID_o,          /* can be NULL */
  CFragConnectionID *  connectionID_o,       /* can be NULL */
  CFContSignedIndex *  sectionIndex_o);      /* can be NULL */



/*
   ----------------------------------------------------------------------------
   These are routines that return information about the various CFM registries.
*/


/* *** To be added later.*/




#if PRAGMA_STRUCT_ALIGN
    #pragma options align=reset
#elif PRAGMA_STRUCT_PACKPUSH
    #pragma pack(pop)
#elif PRAGMA_STRUCT_PACK
    #pragma pack()
#endif

#ifdef __cplusplus
}
#endif

#endif /* __CODEFRAGMENTINFOPRIV__ */

