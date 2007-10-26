/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*!
 * @header CRefTable
 * References for the API calls.
 */

#ifndef __CRefTable_h__
#define	__CRefTable_h__		1

#include "DirServicesTypes.h"
#include "PrivateTypes.h"
#include "DSMutexSemaphore.h"

#include <CoreFoundation/CoreFoundation.h>
#include <map>
#include <set>

class CRefTable;
class CServerPlugin;

//#define		kMaxTableItems	4096	//KW80 - made smaller
#define		kMaxTableItems	512
#define		kMaxTables		0xFF

typedef struct sListInfo *sListInfoPtr;

//note fPID defined everywhere as SInt32 to remove warnings of comparing -1 to UInt32 since failed PID is -1

//struct to contain reference to the actual ref entry of the child ref
typedef struct sListInfo {
	UInt32			fRefNum;
	UInt32			fType;
	SInt32			fPID;
	UInt32			fIPAddress;
	sListInfoPtr	fNext;
} sListInfo;

//struct to contain PID of the child client process granted access to a ref from the parent client process
typedef struct sPIDInfo {
	SInt32			fPID;
	UInt32			fIPAddress;
	sPIDInfo	   *fNext;
} sPIDInfo;

//struct of the main ref entry
typedef struct sRefEntry {
	UInt32			fRefNum;
	UInt32			fType;
	UInt32			fParentID;
	SInt32			fPID;
	UInt32			fIPAddress;
	CServerPlugin  *fPlugin;
	sListInfo	   *fChildren;
	sPIDInfo	   *fChildPID;
	char		   *fNodeName;	//only retained for an OpenDirNode call inside the daemon for record type restyrictions support
} sRefEntry;

//struct of a ref cleanup entry
typedef struct sRefCleanUpEntry {
	UInt32					fRefNum;
	UInt32					fType;
	CServerPlugin		   *fPlugin;
	sRefCleanUpEntry	   *fNext;
} sRefCleanUpEntry;

typedef SInt32 RefDeallocateProc ( UInt32 inRefNum, UInt32 inRefType, CServerPlugin *inPluginPtr );

// -------------------------------------------

typedef struct sRefTable *sRefTablePtr;

typedef struct sRefTable {
	UInt32			fTableNum;
	UInt32			fCurRefNum;
	UInt32			fItemCnt;
	sRefEntry		*fTableData[ kMaxTableItems ];
} sRefTable;

// IP -> PID -> DirRef (Set)
typedef std::set<UInt32>					tDirRefSet;
typedef std::map<UInt32, tDirRefSet>		tPIDDirRefMap;
typedef std::map<UInt32, tPIDDirRefMap>		tIPPIDDirRefMap;

// IP -> PID -> RefCount
typedef std::map<UInt32, UInt32>			tPIDRefCountMap;
typedef std::map<UInt32, tPIDRefCountMap>	tIPPIDRefCountMap;

//------------------------------------------------------------------------------------
//	* CRefTable
//------------------------------------------------------------------------------------

class CRefTable {
public:
					CRefTable			( RefDeallocateProc *deallocProc );
	virtual		   ~CRefTable			( void );

	void			Lock				( void );
	void			Unlock				( void );

	static tDirStatus	VerifyDirRef		( tDirReference inDirRef, CServerPlugin **outPlugin, SInt32 inPID, UInt32 inIPAddress );
	static tDirStatus	VerifyNodeRef		( tDirNodeReference inDirNodeRef, CServerPlugin **outPlugin, SInt32 inPID, UInt32 inIPAddress );
	static char*		GetNodeRefName		( tDirNodeReference inDirNodeRef );
	static tDirStatus	VerifyRecordRef		( tRecordReference inRecordRef, CServerPlugin **outPlugin, SInt32 inPID, UInt32 inIPAddress, bool inDaemonPID_OK = false );
	static tDirStatus	VerifyAttrListRef	( tAttributeListRef inAttributeListRef, CServerPlugin **outPlugin, SInt32 inPID, UInt32 inIPAddress );
	static tDirStatus	VerifyAttrValueRef	( tAttributeValueListRef inAttributeValueListRef, CServerPlugin **outPlugin, SInt32 inPID, UInt32 inIPAddress );

	static tDirStatus	NewDirRef			( UInt32 *outNewRef, SInt32 inPID, UInt32 inIPAddress );
	static tDirStatus	NewNodeRef			( UInt32 *outNewRef, CServerPlugin *inPlugin, UInt32 inParentID, SInt32 inPID, UInt32 inIPAddress, const char *inNodeName );
	static tDirStatus	NewRecordRef		( UInt32 *outNewRef, CServerPlugin *inPlugin, UInt32 inParentID, SInt32 inPID, UInt32 inIPAddress );
	static tDirStatus	NewAttrListRef		( UInt32 *outNewRef, CServerPlugin *inPlugin, UInt32 inParentID, SInt32 inPID, UInt32 inIPAddress );
	static tDirStatus	NewAttrValueRef		( UInt32 *outNewRef, CServerPlugin *inPlugin, UInt32 inParentID, SInt32 inPID, UInt32 inIPAddress );

	static tDirStatus	RemoveDirRef		( UInt32 inDirRef, SInt32 inPID, UInt32 inIPAddress );
	static tDirStatus	RemoveNodeRef		( UInt32 inNodeRef, SInt32 inPID, UInt32 inIPAddress );
	static tDirStatus	RemoveRecordRef		( UInt32 inRecRef, SInt32 inPID, UInt32 inIPAddress );
	static tDirStatus	RemoveAttrListRef	( UInt32 inAttrListRef, SInt32 inPID, UInt32 inIPAddress );
	static tDirStatus	RemoveAttrValueRef	( UInt32 InAttrValueRef, SInt32 inPID, UInt32 inIPAddress );

	static tDirStatus	SetNodePluginPtr	( tDirNodeReference inNodeRef, CServerPlugin *inPlugin );
	static tDirStatus	AddChildPIDToRef	( UInt32 inRefNum, UInt32 inParentPID, SInt32 inChildPID, UInt32 inIPAddress );
	
	static void			CleanClientRefs		( UInt32 inIPAddress, UInt32 inPIDorPort );
	
	void				LockClientPIDList	( void );
	void				UnlockClientPIDList	( void );
	char*				CreateNextClientPIDListString
											( bool inStart, tIPPIDDirRefMap::iterator &inOutIPEntry, tPIDDirRefMap::iterator &inOutPIDEntry );
	char*				CreateNextClientPIDListRecordName
											( bool inStart,
											tIPPIDDirRefMap::iterator &inOutIPEntry, tPIDDirRefMap::iterator &inOutPIDEntry,
											char** outIPAddress, char** outPIDValue,
											SInt32 &outIP, UInt32 &outPID,
											UInt32 &outTotalRefCount,
											UInt32 &outDirRefCount, char** &outDirRefs );
	void				RetrieveRefDataPerClientPIDAndIP
											(	SInt32 inIP, UInt32 inPID, char** inDirRefs,
												UInt32 &outNodeRefCount, char** &outNodeRefs,
												UInt32 &outRecRefCount, char** &outRecRefs,
												UInt32 &outAttrListRefCount, char** &outAttrListRefs,
												UInt32 &outAttrListValueRefCount, char** &outAttrListValueRefs  );

private:
	tDirStatus	VerifyReference		( tDirReference inDirRef, UInt32 inType, CServerPlugin **outPlugin, SInt32 inPID, UInt32 inIPAddress, bool inDaemonPID_OK = false );
	char*		GetNodeName			( tDirReference inDirRef );
	tDirStatus	GetNewRef			( UInt32 *outRef, UInt32 inParentID, eRefTypes inType, CServerPlugin *inPlugin, SInt32 inPID, UInt32 inIPAddress, const char *inNodeName = NULL );
	tDirStatus	RemoveRef			( UInt32 inRefNum, UInt32 inType, SInt32 inPID, UInt32 inIPAddress, bool inbAtTop = false);
	tDirStatus	SetPluginPtr		( UInt32 inRefNum, UInt32 inType, CServerPlugin *inPlugin );

	tDirStatus	GetReference		( UInt32 inRefNum, sRefEntry **outRefData );

	tDirStatus	LinkToParent		( UInt32 inRefNum, UInt32 inType, UInt32 inParentID, SInt32 inPID, UInt32 inIPAddress );
	tDirStatus	UnlinkFromParent	( UInt32 inRefNum );

	void		RemoveChildren		( sListInfo *inChildList, SInt32 inPID, UInt32 inIPAddress );

	sRefTable*	GetNextTable		( sRefTable *inCurTable );
	sRefTable*	GetThisTable		( UInt32 inTableNum );

	sRefEntry*	GetTableRef			( UInt32 inRefNum );

	UInt32		UpdateClientPIDRefCount
									( SInt32 inClientPID, UInt32 inIPAddress, bool inUpRefCount, UInt32 inDirRef=0 );

	void		DoCleanClientRefs	( UInt32 inIPAddress, UInt32 inPIDorPort );

	UInt32		fTableCount;
	sRefTable	*fRefTables[ kMaxTables + 1 ];	//KW80 added 1 since table is 1-based and code depends upon having that last
												//index in as kMaxTables ie. note array is 0-based
	RefDeallocateProc *fDeallocProc;
	
	tIPPIDDirRefMap			fClientDirRefMap;
	tIPPIDRefCountMap		fClientRefCountMap;
	DSMutexSemaphore	   *fClientPIDListLock;		//mutex on the client PID list tracking references per PID
	DSMutexSemaphore		fTableMutex;
	sRefCleanUpEntry	   *fRefCleanUpEntriesHead;
	sRefCleanUpEntry	   *fRefCleanUpEntriesTail;

};

void GrowList(char** &inOutRefs, UInt32 &inOutRefListSize);

#endif

