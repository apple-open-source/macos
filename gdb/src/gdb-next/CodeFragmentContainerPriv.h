/*
     File:       CarbonCore/CodeFragmentContainerPriv.h
 
     Contains:   Physical container routines of the ModernOS version of CFM.
 
     Version:    Technology: Forte CFM
                 Release:    Cheetah4I
 
     Copyright:  (c) 1994-2000 by Apple Computer, Inc., all rights reserved.
 
     Bugs?:      For bug reports, consult the following page on
                 the World Wide Web:
 
                     http://developer.apple.com/bugreporter/
 
*/
/*
   -------------------------------------------------------------------------------------------
   This file contains what used to be called the CFLoader interface.  The name was changed to
   fit the newer convention of having CodeFragment as a common prefix, and to reduce pervasive
   confusion between the Code Fragment Manager and the Code Fragment Loaders, promulgated by
   the long history of the Segment Loader.  This file defines the abstract interface to the
   physical representation of code fragments.
*/


/* !!! This version has minimal comments, the main purpose is to get things compiled.*/


#ifndef __CODEFRAGMENTCONTAINERPRIV__
#define __CODEFRAGMENTCONTAINERPRIV__

#ifndef __MACTYPES__
#include <MacTypes.h>
#endif


#ifndef __CODEFRAGMENTS__
#include <CodeFragments.h>
#endif

#ifndef __MULTIPROCESSING__
#include <Multiprocessing.h>
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
   General Types and Constants
   ===========================
*/


typedef SInt32                          CFContSignedIndex;
typedef UInt32                          CFContStringHash;
#define CFContHashedStringLength(hashValue) ((hashValue) >> 16)

struct CFContHashedName {
  CFContStringHash    nameHash;               /* ! Includes the name length.*/
  BytePtr             nameText;
};
typedef struct CFContHashedName         CFContHashedName;
/*
   ------------------------------------------
   Declarations for code fragment containers.
*/

enum {
  kCFContContainerInfoVersion   = 0x00010001
};


struct CFContContainerInfo {
  CFContHashedName    cfragName;
  UInt32              modDate;                /* !!! Abstract type?*/
  OSType              architecture;
  CFragVersionNumber  currentVersion;
  CFragVersionNumber  oldImpVersion;
  CFragVersionNumber  oldDefVersion;
  UInt32              reservedA;
  void *              reservedB;
};
typedef struct CFContContainerInfo      CFContContainerInfo;
/*
   ----------------------------------------
   Declarations for code fragment sections.
*/



struct CFContLogicalLocation {
  CFContSignedIndex   section;                /* "Real" sections use zero based indices, special ones are negative.*/
  ByteCount           offset;
};
typedef struct CFContLogicalLocation    CFContLogicalLocation;
enum {
                                        /* Special values for CFContLogicalLocation.*/
  kCFContNoSectionIndex         = -1,
  kCFContAbsoluteSectionIndex   = -2,
  kCFContReexportSectionIndex   = -3
};



typedef UInt8                           CFContSectionSharing;
enum {
                                        /* Values for CFContSectionSharing.*/
  kCFContShareSectionInClosure  = 0,    /* ! Not supported at present!*/
  kCFContShareSectionInProcess  = 1,
  kCFContShareSectionAcrossSystem = 4,
  kCFContShareSectionWithProtection = 5
};


typedef UInt8                           CFContMemoryAccess;
enum {
                                        /* Values for CFContMemoryAccess.*/
  kCFContMemReadMask            = 0x01, /* Readable memory can also be executed.*/
  kCFContMemWriteMask           = 0x02,
  kCFContMemExecuteMask         = 0x04, /* ! Affects cache actions, not protection!*/
  kCFContReadOnlyData           = kCFContMemReadMask,
  kCFContWriteableData          = kCFContMemReadMask | kCFContMemWriteMask,
  kCFContNormalCode             = kCFContMemReadMask | kCFContMemExecuteMask,
  kCFContExcludedMemory         = 0
};

typedef UInt32                          CFContSectionOptions;
enum {
                                        /* Values for CFContSectionOptions.*/
  kPackedCFContSectionMask      = 0x01, /* Stored contents are compressed.*/
  kRelocatedCFContSectionMask   = 0x02, /* Section contents have relocations.*/
  kEmptyFillCFContSectionMask   = 0x04, /* The extension part may be left untouched.*/
  kResidentCFContSectionMask    = 0x08,
  kPrefaultCFContSectionMask    = 0x10
};

enum {
  kCFContSectionInfoVersion     = 0x00010001
};


struct CFContSectionInfo {
  CFContHashedName    sectionName;
  CFContMemoryAccess  access;
  CFContSectionSharing  sharing;
  UInt8               alignment;              /* ! The power of 2, a.k.a. number of low order zero bits.*/
  UInt8               reservedA;
  CFContSectionOptions  options;
  ByteCount           containerOffset;
  ByteCount           containerLength;
  ByteCount           unpackedLength;
  ByteCount           totalLength;
  LogicalAddress      defaultAddress;
  UInt32              reservedB;
  void *              reservedC;
};
typedef struct CFContSectionInfo        CFContSectionInfo;
/*
   ----------------------------------
   Declarations for exported symbols.
*/


typedef UInt32                          CFContExportedSymbolOptions;
/*
   ! enum { // Values for CFContExportedSymbolOptions.
   !    // ! No options at present.
   ! };
*/
enum {
  kCFContExportedSymbolInfoVersion = 0x00010001
};


struct CFContExportedSymbolInfo {
  CFContHashedName    symbolName;
  CFContLogicalLocation  location;
  CFContExportedSymbolOptions  options;
  CFragSymbolClass    symbolClass;
  UInt8               reservedA;
  UInt16              reservedB;
  UInt32              reservedC;
  void *              reservedD;
};
typedef struct CFContExportedSymbolInfo CFContExportedSymbolInfo;
/*
   ------------------------------------------------
   Declarations for imported libraries and symbols.
*/


typedef UInt32                          CFContImportedLibraryOptions;
enum {
                                        /* Values for CFContImportedLibraryOptions.*/
  kCFContWeakLibraryMask        = 0x01, /* ! Same as kCFContWeakSymbolMask to reduce errors.*/
  kCFContInitBeforeMask         = 0x02,
  kCFContDeferredBindMask       = 0x04
};

enum {
  kCFContImportedLibraryInfoVersion = 0x00010001
};


struct CFContImportedLibraryInfo {
  CFContHashedName    libraryName;
  CFragVersionNumber  linkedVersion;
  CFragVersionNumber  oldImpVersion;
  CFContImportedLibraryOptions  options;
};
typedef struct CFContImportedLibraryInfo CFContImportedLibraryInfo;

typedef UInt32                          CFContImportedSymbolOptions;
enum {
                                        /* Values for CFContImportedSymbolOptions.*/
  kCFContWeakSymbolMask         = 0x01  /* ! Same as kCFContWeakLibraryMask to reduce errors.*/
};

enum {
  kCFContImportedSymbolInfoVersion = 0x00010001
};


struct CFContImportedSymbolInfo {
  CFContHashedName    symbolName;
  ItemCount           libraryIndex;
  CFContImportedSymbolOptions  options;
  CFragSymbolClass    symbolClass;
  UInt8               reservedA;
  UInt16              reservedB;
  UInt32              reservedC;
  void *              reservedD;
};
typedef struct CFContImportedSymbolInfo CFContImportedSymbolInfo;
/*
   -------------------------------------------------
   Declarations for dealing with container handlers.
*/


typedef UInt32                          CFContOpenOptions;
enum {
                                        /* Values for CFContOpenOptions.*/
  kCFContPrepareInPlaceMask     = 0x01,
  kCFContMinimalOpenMask        = 0x02
};

typedef UInt32                          CFContCloseOptions;
enum {
                                        /* Values for CFContCloseOptions.*/
  kCFContPartialCloseMask       = 0x01
};


typedef UInt8                           CFContPortionTag;
enum {
                                        /* Values for CFContPortionTag.*/
  kCFContTablesPortion          = 0,
  kCFContReadOnlyPortion        = 1,
  kCFContWriteablePortion       = 2,
  kCFContSpecifiedPortion       = 3
};


typedef struct OpaqueCFContHandlerRef*  CFContHandlerRef;
typedef struct CFContHandlerProcs       CFContHandlerProcs;
typedef CALLBACK_API_C( LogicalAddress , CFContAllocateMem )(ByteCount size, UInt8 alignment, OptionBits options, void *refCon);
typedef CALLBACK_API_C( void , CFContReleaseMem )(LogicalAddress address, void *refCon);
/*
   .
   ===========================================================================================
   Container Handler Routines
   ==========================
*/


typedef CALLBACK_API_C( OSStatus , CFCont_OpenContainer )(LogicalAddress address, ByteCount length, MPProcessID processID, const CFContHashedName *cfragName, CFContOpenOptions options, CFContAllocateMem AllocateMem, CFContReleaseMem ReleaseMem, void *memRefCon, CFContHandlerRef *containerRef_o, CFContHandlerProcs **handlerProcs_o);
typedef CALLBACK_API_C( OSStatus , CFCont_CloseContainer )(CFContHandlerRef containerRef, CFContCloseOptions options);
typedef CALLBACK_API_C( OSStatus , CFCont_GetContainerInfo )(CFContHandlerRef containerRef, PBVersion infoVersion, CFContContainerInfo *containerInfo_o);
typedef CALLBACK_API_C( OSStatus , CFCont_GetOldProcs )(CFContHandlerRef containerRef, UInt32 abiVersion, void **oldProcs_o);
/*
   This is here to provide binary compatibility for code outside of CFM that uses the
   original container handler functions as documented in old versions of CodeFragmentsPriv.
*/
/* -------------------------------------------------------------------------------------------*/
typedef CALLBACK_API_C( OSStatus , CFCont_GetSectionCount )(CFContHandlerRef containerRef, ItemCount *sectionCount_o);
typedef CALLBACK_API_C( OSStatus , CFCont_GetSectionInfo )(CFContHandlerRef containerRef, ItemCount sectionIndex, PBVersion infoVersion, CFContSectionInfo *sectionInfo_o);
typedef CALLBACK_API_C( OSStatus , CFCont_FindSectionInfo )(CFContHandlerRef containerRef, const CFContHashedName *sectionName, PBVersion infoVersion, ItemCount *sectionIndex_o, CFContSectionInfo *sectionInfo_o);
typedef CALLBACK_API_C( OSStatus , CFCont_SetSectionAddress )(CFContHandlerRef containerRef, ItemCount sectionIndex, LogicalAddress address);
/* -------------------------------------------------------------------------------------------*/
typedef CALLBACK_API_C( OSStatus , CFCont_GetAnonymousSymbolLocations )(CFContHandlerRef containerRef, CFContLogicalLocation *mainLocation_o, CFContLogicalLocation *initLocation_o, CFContLogicalLocation *termLocation_o);
/* -------------------------------------------------------------------------------------------*/
typedef CALLBACK_API_C( OSStatus , CFCont_GetExportedSymbolCount )(CFContHandlerRef containerRef, ItemCount *exportCount_o);
typedef CALLBACK_API_C( OSStatus , CFCont_GetExportedSymbolInfo )(CFContHandlerRef containerRef, CFContSignedIndex exportedIndex, PBVersion infoVersion, CFContExportedSymbolInfo *exportInfo_o);
typedef CALLBACK_API_C( OSStatus , CFCont_FindExportedSymbolInfo )(CFContHandlerRef containerRef, const CFContHashedName *exportName, PBVersion infoVersion, ItemCount *exportIndex_o, CFContExportedSymbolInfo *exportInfo_o);
/* -------------------------------------------------------------------------------------------*/
typedef CALLBACK_API_C( OSStatus , CFCont_GetImportCounts )(CFContHandlerRef containerRef, ItemCount *libraryCount_o, ItemCount *symbolCount_o);
typedef CALLBACK_API_C( OSStatus , CFCont_GetImportedLibraryInfo )(CFContHandlerRef containerRef, ItemCount libraryIndex, PBVersion infoVersion, CFContImportedLibraryInfo *libraryInfo_o);
typedef CALLBACK_API_C( OSStatus , CFCont_GetImportedSymbolInfo )(CFContHandlerRef containerRef, ItemCount symbolIndex, PBVersion infoVersion, CFContImportedSymbolInfo *symbolInfo_o);
typedef CALLBACK_API_C( OSStatus , CFCont_SetImportedSymbolAddress )(CFContHandlerRef containerRef, ItemCount symbolIndex, LogicalAddress symbolAddress);
/* -------------------------------------------------------------------------------------------*/
typedef CALLBACK_API_C( OSStatus , CFCont_UnpackSection )(CFContHandlerRef containerRef, ItemCount sectionIndex, ByteCount sectionOffset, LogicalAddress bufferAddress, ByteCount bufferLength);
typedef CALLBACK_API_C( OSStatus , CFCont_RelocateSection )(CFContHandlerRef containerRef, ItemCount sectionIndex);
typedef CALLBACK_API_C( OSStatus , CFCont_RelocateImportsOnly )(CFContHandlerRef containerRef, ItemCount sectionIndex, ItemCount libraryIndex);
/* -------------------------------------------------------------------------------------------*/
typedef CALLBACK_API_C( OSStatus , CFCont_AllocateSection )(CFContHandlerRef containerRef, ItemCount sectionIndex, LogicalAddress *address_o);
typedef CALLBACK_API_C( OSStatus , CFCont_ReleaseSection )(CFContHandlerRef containerRef, ItemCount sectionIndex);
/* -------------------------------------------------------------------------------------------*/
typedef CALLBACK_API_C( void , CFCont_MakeSectionExecutable )(CFContHandlerRef containerRef, ItemCount sectionIndex);
typedef CALLBACK_API_C( void , CFCont_PrepageContainerPortion )(CFContHandlerRef containerRef, CFContPortionTag containerPortion, ItemCount sectionIndex);
typedef CALLBACK_API_C( void , CFCont_ReleaseContainerPortion )(CFContHandlerRef containerRef, CFContPortionTag containerPortion, ItemCount sectionIndex);
/* -------------------------------------------------------------------------------------------*/


struct CFContHandlerProcs {
  ItemCount           procCount;
  UInt32              abiVersion;             /* ! Really a CFragShortVersionPair.*/
  OSType              handlerName;

  CFCont_OpenContainer  OpenContainer;        /*  1*/
  CFCont_CloseContainer  CloseContainer;      /*  2*/
  CFCont_GetContainerInfo  GetContainerInfo;  /*  3*/

  CFCont_GetSectionCount  GetSectionCount;    /*  4*/
  CFCont_GetSectionInfo  GetSectionInfo;      /*  5*/
  CFCont_FindSectionInfo  FindSectionInfo;    /*  6*/
  CFCont_SetSectionAddress  SetSectionAddress; /*  7*/

  CFCont_GetAnonymousSymbolLocations  GetAnonymousSymbolLocations; /*  8*/

  CFCont_GetExportedSymbolCount  GetExportedSymbolCount; /*    9*/
  CFCont_GetExportedSymbolInfo  GetExportedSymbolInfo; /* 10*/
  CFCont_FindExportedSymbolInfo  FindExportedSymbolInfo; /* 11*/

  CFCont_GetImportCounts  GetImportCounts;    /* 12*/
  CFCont_GetImportedLibraryInfo  GetImportedLibraryInfo; /* 13*/
  CFCont_GetImportedSymbolInfo  GetImportedSymbolInfo; /* 14*/
  CFCont_SetImportedSymbolAddress  SetImportedSymbolAddress; /* 15*/

  CFCont_UnpackSection  UnpackSection;        /* 16*/
  CFCont_RelocateSection  RelocateSection;    /* 17*/
  CFCont_RelocateImportsOnly  RelocateImportsOnly; /* 18*/

  CFCont_AllocateSection  AllocateSection;    /* 19 (Opt.)*/
  CFCont_ReleaseSection  ReleaseSection;      /* 20 (Opt.)*/

  CFCont_MakeSectionExecutable  MakeSectionExecutable; /* 21 (Opt.)*/
  CFCont_PrepageContainerPortion  PrepageContainerPortion; /* 22 (Opt.)*/
  CFCont_ReleaseContainerPortion  ReleaseContainerPortion; /* 23 (Opt.)*/

  CFCont_GetOldProcs  GetOldProcs;            /* 24 (Opt.)*/
};

typedef CFContHandlerProcs *            CFContHandlerProcsPtr;
enum {
  kCFContHandlerABIVersion      = 0x00010001,
  kCFContMinimumProcCount       = 18,
  kCFContCurrentProcCount       = 24
};

#define CFContHasAllocateSection(h)         ( ((h)->procCount >= 19) && ((h)->AllocateSection != NULL) )
#define CFContHasReleaseSection(h)          ( ((h)->procCount >= 20) && ((h)->ReleaseSection != NULL) )
#define CFContHasMakeSectionExecutable(h)   ( ((h)->procCount >= 21) && ((h)->MakeSectionExecutable != NULL) )
#define CFContHasPrepageContainerPortion(h) ( ((h)->procCount >= 22) && ((h)->PrepageContainerPortion != NULL) )
#define CFContHasReleaseContainerPortion(h) ( ((h)->procCount >= 23) && ((h)->ReleaseContainerPortion != NULL) )
#define CFContHasGetOldProcs(h)             ( ((h)->procCount >= 24) && ((h)->GetOldProcs != NULL) )

enum {
  kCFContHandlerInfoVersion     = 0x00010001
};


struct CFContHandlerInfo {
  OSType              handlerName;
  CFCont_OpenContainer  OpenHandlerProc;
};
typedef struct CFContHandlerInfo        CFContHandlerInfo;
/*
   -----------------------------------------------------------------------------------------
   The ABI version is a pair of UInt16s used as simple counters.  The high order part is the
   current version number, the low order part is the oldest compatible definition version.
   number.  This pair is to be used by the specific container handlers to describe what
   version of the container handler ABI they support.
    0x00010001
    ----------
    The initial release of this ABI.  (The old CFLoader ABI does not count.)
*/


/*
   .
   ===========================================================================================
   General Routines
   ================
*/


extern CFContStringHash 
CFContGetStringHash(
  BytePtr     nameText,
  ByteCount   nameLimit);


/* -------------------------------------------------------------------------------------------*/

extern OSStatus 
CFContOpenContainer(
  LogicalAddress            address,
  ByteCount                 length,
  MPProcessID               processID,
  const CFContHashedName *  cfragName,            /* can be NULL */
  CFContOpenOptions         options,
  CFContAllocateMem         Allocate,
  CFContReleaseMem          Release,
  void *                    memRefCon,
  CFContHandlerRef *        containerRef_o,       /* can be NULL */
  CFContHandlerProcsPtr *   handlerProcs_o);      /* can be NULL */


/* -------------------------------------------------------------------------------------------*/

extern OSStatus 
CFContRegisterContainerHandler(
  OSType                 handlerName,
  CFCont_OpenContainer   OpenHandlerProc);


extern OSStatus 
CFContUnregisterContainerHandler(OSType handlerName);


extern OSStatus 
CFContGetContainerHandlers(
  ItemCount            requestedCount,
  ItemCount            skipCount,
  ItemCount *          totalCount_o,
  PBVersion            infoVersion,
  CFContHandlerInfo *  info_o);              /* can be NULL */



/* ===========================================================================================*/




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

#endif /* __CODEFRAGMENTCONTAINERPRIV__ */

