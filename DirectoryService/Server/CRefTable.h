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

class CRefTable;
class CServerPlugin;

extern DSMutexSemaphore		*gTableMutex;
extern CRefTable			*gRefTable;

//#define		kMaxTableItems	4096	//KW80 - made smaller
#define		kMaxTableItems	512
#define		kMaxTables		0xFF

typedef struct sListInfo *sListInfoPtr;

//note fPID defined everywhere as sInt32 to remove warnings of comparing -1 to uInt32 since failed PID is -1

//struct to contain reference to the actual ref entry of the child ref
typedef struct sListInfo {
	uInt32			fRefNum;
	uInt32			fType;
	sInt32			fPID;
	uInt32			fIPAddress;
	sListInfoPtr	fNext;
} sListInfo;

//struct to contain PID of the child client process granted access to a ref from the parent client process
typedef struct sPIDInfo {
	sInt32			fPID;
	uInt32			fIPAddress;
	sPIDInfo	   *fNext;
} sPIDInfo;

//struct of the main ref entry
typedef struct sRefEntry {
	uInt32			fRefNum;
	uInt32			fType;
	uInt32			fParentID;
	sInt32			fPID;
	uInt32			fIPAddress;
	CServerPlugin  *fPlugin;
	sListInfo	   *fChildren;
	sPIDInfo	   *fChildPID;
} sRefEntry;

//struct of a ref cleanup entry
typedef struct sRefCleanUpEntry {
	uInt32					fRefNum;
	uInt32					fType;
	CServerPlugin		   *fPlugin;
	sRefCleanUpEntry	   *fNext;
} sRefCleanUpEntry;

typedef sInt32 RefDeallocateProc ( uInt32 inRefNum, uInt32 inRefType, CServerPlugin *inPluginPtr );

// -------------------------------------------

typedef struct sRefTable *sRefTablePtr;

typedef struct sRefTable {
	uInt32			fTableNum;
	uInt32			fCurRefNum;
	uInt32			fItemCnt;
	sRefEntry		*fTableData[ kMaxTableItems ];
} sRefTable;


//------------------------------------------------------------------------------------
//	* CRefTable
//------------------------------------------------------------------------------------

class CRefTable {
public:
					CRefTable			( RefDeallocateProc *deallocProc );
	virtual		   ~CRefTable			( void );

	void			Lock				( void );
	void			Unlock				( void );

	static tDirStatus	VerifyDirRef		( tDirReference inDirRef, CServerPlugin **outPlugin, sInt32 inPID, uInt32 inIPAddress );
	static tDirStatus	VerifyNodeRef		( tDirNodeReference inDirNodeRef, CServerPlugin **outPlugin, sInt32 inPID, uInt32 inIPAddress );
	static tDirStatus	VerifyRecordRef		( tRecordReference inRecordRef, CServerPlugin **outPlugin, sInt32 inPID, uInt32 inIPAddress, bool inDaemonPID_OK = false );
	static tDirStatus	VerifyAttrListRef	( tAttributeListRef inAttributeListRef, CServerPlugin **outPlugin, sInt32 inPID, uInt32 inIPAddress );
	static tDirStatus	VerifyAttrValueRef	( tAttributeValueListRef inAttributeValueListRef, CServerPlugin **outPlugin, sInt32 inPID, uInt32 inIPAddress );

	static tDirStatus	NewDirRef			( uInt32 *outNewRef, sInt32 inPID, uInt32 inIPAddress );
	static tDirStatus	NewNodeRef			( uInt32 *outNewRef, CServerPlugin *inPlugin, uInt32 inParentID, sInt32 inPID, uInt32 inIPAddress );
	static tDirStatus	NewRecordRef		( uInt32 *outNewRef, CServerPlugin *inPlugin, uInt32 inParentID, sInt32 inPID, uInt32 inIPAddress );
	static tDirStatus	NewAttrListRef		( uInt32 *outNewRef, CServerPlugin *inPlugin, uInt32 inParentID, sInt32 inPID, uInt32 inIPAddress );
	static tDirStatus	NewAttrValueRef		( uInt32 *outNewRef, CServerPlugin *inPlugin, uInt32 inParentID, sInt32 inPID, uInt32 inIPAddress );

	static tDirStatus	RemoveDirRef		( uInt32 inDirRef, sInt32 inPID, uInt32 inIPAddress );
	static tDirStatus	RemoveNodeRef		( uInt32 inNodeRef, sInt32 inPID, uInt32 inIPAddress );
	static tDirStatus	RemoveRecordRef		( uInt32 inRecRef, sInt32 inPID, uInt32 inIPAddress );
	static tDirStatus	RemoveAttrListRef	( uInt32 inAttrListRef, sInt32 inPID, uInt32 inIPAddress );
	static tDirStatus	RemoveAttrValueRef	( uInt32 InAttrValueRef, sInt32 inPID, uInt32 inIPAddress );

	static tDirStatus	SetNodePluginPtr	( tDirNodeReference inNodeRef, CServerPlugin *inPlugin );
	static tDirStatus	AddChildPIDToRef	( uInt32 inRefNum, uInt32 inParentPID, sInt32 inChildPID, uInt32 inIPAddress );
	
	static void			CheckClientPIDs		( bool inUseTimeOuts, uInt32 inIPAddress, uInt32 inPIDorPort );

private:
	tDirStatus	VerifyReference		( tDirReference inDirRef, uInt32 inType, CServerPlugin **outPlugin, sInt32 inPID, uInt32 inIPAddress, bool inDaemonPID_OK = false );
	tDirStatus	GetNewRef			( uInt32 *outRef, uInt32 inParentID, eRefTypes inType, CServerPlugin *inPlugin, sInt32 inPID, uInt32 inIPAddress );
	tDirStatus	RemoveRef			( uInt32 inRefNum, uInt32 inType, sInt32 inPID, uInt32 inIPAddress, bool inbAtTop = false);
	tDirStatus	SetPluginPtr		( uInt32 inRefNum, uInt32 inType, CServerPlugin *inPlugin );

	tDirStatus	GetReference		( uInt32 inRefNum, sRefEntry **outRefData );

	tDirStatus	LinkToParent		( uInt32 inRefNum, uInt32 inType, uInt32 inParentID, sInt32 inPID, uInt32 inIPAddress );
	tDirStatus	UnlinkFromParent	( uInt32 inRefNum );

	void		RemoveChildren		( sListInfo *inChildList, sInt32 inPID, uInt32 inIPAddress );

	sRefTable*	GetNextTable		( sRefTable *inCurTable );
	sRefTable*	GetThisTable		( uInt32 inTableNum );

	sRefEntry*	GetTableRef			( uInt32 inRefNum );

	uInt32		UpdateClientPIDRefCount
									( sInt32 inClientPID, uInt32 inIPAddress, bool inUpRefCount, uInt32 inDirRef=0 );

	void		DoCheckClientPIDs	( bool inUseTimeOuts, uInt32 inIPAddress, uInt32 inPIDorPort );

	uInt32		fTableCount;
	sRefTable	*fRefTables[ kMaxTables + 1 ];	//KW80 added 1 since table is 1-based and code depends upon having that last
												//index in as kMaxTables ie. note array is 0-based
	RefDeallocateProc *fDeallocProc;
	
	CFMutableDictionaryRef	fClientPIDList;
	CFMutableDictionaryRef	fClientIPList;
	DSMutexSemaphore	   *fClientPIDListLock;		//mutex on the client PID list tracking references per PID
	time_t					fSunsetTime;
	DSMutexSemaphore		fTableMutex;
	sRefCleanUpEntry	   *fRefCleanUpEntriesHead;
	sRefCleanUpEntry	   *fRefCleanUpEntriesTail;

};


#endif

