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
 * @header CDSRefMap
 * DirectoryService Framework reference map table to references of
 * all TCP connected "remote" DirectoryService daemons.
 * References here always use 0x00C00000 bits
 */

#ifndef __CDSRefMap_h__
#define	__CDSRefMap_h__		1

#include "DirServicesTypes.h"

#include "PrivateTypes.h"
#include "DSMutexSemaphore.h"
#include "CDSRefTable.h"

#ifdef __LITTLE_ENDIAN__
#include <map> //STL map class
#endif

class CDSRefMap;

extern DSMutexSemaphore		*gFWRefMapMutex;
extern CDSRefMap			*gFWRefMap;

//many constants, enums, and typedefs used from CDSRefTable.h
//KW: at some point need a base class that can handle this and the other two derived ones
//ie. complete with tracking of statistics using STL base containers?

//struct of the main ref entry
typedef struct sFWRefMapEntry {
	uInt32			fRefNum;
	uInt32			fType;
    uInt32			fRemoteRefNum;
	uInt32			fParentID;
	sInt32			fPID;
	sListFWInfo	   *fChildren;
	sPIDFWInfo	   *fChildPID;
	uInt32			fMessageTableIndex;
	char		   *fPluginName;
} sFWRefMapEntry;

typedef sInt32 RefMapDeallocateProc ( uInt32 inRefNum, sFWRefMapEntry *entry );

// -------------------------------------------

typedef struct sRefMapTable *sRefMapTablePtr;

typedef struct sRefMapTable {
	uInt32			fTableNum;
	uInt32			fCurRefNum;
	uInt32			fItemCnt;
	sFWRefMapEntry *fTableData[ kMaxFWTableItems ];
} sRefMapTable;

#ifdef __LITTLE_ENDIAN__
typedef std::map<uInt32, uInt32> tRefMap;
typedef tRefMap::iterator tRefMapI;
#endif

//------------------------------------------------------------------------------------
//	* CDSRefMap
//------------------------------------------------------------------------------------

class CDSRefMap {
public:
						CDSRefMap			( RefMapDeallocateProc *deallocProc );
	virtual			   ~CDSRefMap			( void );

	static tDirStatus	VerifyDirRef		( tDirReference inDirRef, sInt32 inPID );
	static tDirStatus	VerifyNodeRef		( tDirNodeReference inDirNodeRef, sInt32 inPID );
	static tDirStatus	VerifyRecordRef		( tRecordReference inRecordRef, sInt32 inPID );
	static tDirStatus	VerifyAttrListRef	( tAttributeListRef inAttributeListRef, sInt32 inPID );
	static tDirStatus	VerifyAttrValueRef	( tAttributeValueListRef inAttributeValueListRef, sInt32 inPID );

	static tDirStatus	NewDirRefMap		( uInt32 *outNewRef, sInt32 inPID, uInt32 serverRef, uInt32 messageIndex );
	static tDirStatus	NewNodeRefMap		( uInt32 *outNewRef, uInt32 inParentID, sInt32 inPID, uInt32 serverRef, uInt32 messageIndex, char* inPluginName );
	static tDirStatus	NewRecordRefMap		( uInt32 *outNewRef, uInt32 inParentID, sInt32 inPID, uInt32 serverRef, uInt32 messageIndex );
	static tDirStatus	NewAttrListRefMap	( uInt32 *outNewRef, uInt32 inParentID, sInt32 inPID, uInt32 serverRef, uInt32 messageIndex );
	static tDirStatus	NewAttrValueRefMap	( uInt32 *outNewRef, uInt32 inParentID, sInt32 inPID, uInt32 serverRef, uInt32 messageIndex );

	static tDirStatus	RemoveDirRef		( uInt32 inDirRef, sInt32 inPID );
	static tDirStatus	RemoveNodeRef		( uInt32 inNodeRef, sInt32 inPID );
	static tDirStatus	RemoveRecordRef		( uInt32 inRecRef, sInt32 inPID );
	static tDirStatus	RemoveAttrListRef	( uInt32 inAttrListRef, sInt32 inPID );
	static tDirStatus	RemoveAttrValueRef	( uInt32 InAttrValueRef, sInt32 inPID );

	static tDirStatus	AddChildPIDToRef	( uInt32 inRefNum, uInt32 inParentPID, sInt32 inChildPID );
	
    static tDirStatus	SetRemoteRefNum		( uInt32 inRefNum, uInt32 inType, uInt32 inRemoteRefNum, sInt32 inPID );

    static uInt32		GetMessageTableIndex( uInt32 inRefNum, uInt32 inType, sInt32 inPID );
    static tDirStatus	SetMessageTableIndex( uInt32 inRefNum, uInt32 inType, uInt32 inMsgTableIndex, sInt32 inPID );

	static char*		GetPluginName		( uInt32 inRefNum, sInt32 inPID );
	static tDirStatus	SetPluginName		( uInt32 inRefNum, uInt32 inType, char* inPluginName, sInt32 inPID );
	
	static uInt32		GetRefNum			( uInt32 inRefNum, uInt32 inType, sInt32 inPID );
	static uInt32		GetRefNumMap	 	( uInt32 inRefNum, uInt32 inType, sInt32 inPID );
#ifdef __LITTLE_ENDIAN__
	static void			MapServerRefToLocalRef
											( uInt32 inServerRef, uInt32 inLocalRef );
	static void			RemoveServerToLocalRefMap
											( uInt32 inServerRef );
	static uInt32		GetLocalRefFromServerMap
											( uInt32 inServerRef );
	static void			MapMsgIDToServerRef	( uInt32 inMsgID, uInt32 inServerRef );
	static void			RemoveMsgIDToServerRefMap
											( uInt32 inMsgID );
	static uInt32		GetServerRefFromMsgIDMap
											( uInt32 inMsgID );
	static void			MapMsgIDToCustomCode( uInt32 inMsgID, uInt32 inCustomCode );
	static void			RemoveMsgIDToCustomCodeMap
											( uInt32 inMsgID );
	static uInt32		GetCustomCodeFromMsgIDMap
											( uInt32 inMsgID );
#endif


private:
	tDirStatus		VerifyReference		( tDirReference inDirRef, uInt32 inType, sInt32 inPID );
	tDirStatus		GetNewRef			( uInt32 *outRef, uInt32 inParentID, eRefTypes inType, sInt32 inPID, uInt32 serverRef, uInt32 messageIndex );
	tDirStatus		RemoveRef			( uInt32 inRefNum, uInt32 inType, sInt32 inPID );

	tDirStatus		GetReference		( uInt32 inRefNum, sFWRefMapEntry **outRefData );

	tDirStatus		LinkToParent		( uInt32 inRefNum, uInt32 inType, uInt32 inParentID, sInt32 inPID );
	tDirStatus		UnlinkFromParent	( uInt32 inRefNum );

	void			RemoveChildren		( sListFWInfo *inChildList, sInt32 inPID );

	sRefMapTable*	GetNextTable		( sRefMapTable *inCurTable );
	sRefMapTable*	GetThisTable		( uInt32 inTableNum );

	sFWRefMapEntry*	GetTableRef			( uInt32 inRefNum );

	uInt32			fTableCount;
	sRefMapTable   *fRefMapTables[ kMaxFWTables + 1 ];//added 1 since table is 1-based and code depends upon having that last
													//index in as kMaxFWTables ie. note array is 0-based
	RefMapDeallocateProc *fDeallocProc;
	
	time_t					fSunsetTime;

};


#endif

