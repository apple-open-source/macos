/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
 * @header CDSRefTable
 * DirectoryService Framework reference table for all
 * "client side buffer parsing" operations on plugin data
 * returned in standard buffer format.
 * References here always use 0x00300000 bits
 */

#ifndef __CDSRefTable_h__
#define	__CDSRefTable_h__		1

// App
#include "DirServicesTypes.h"

// Misc
#include "PrivateTypes.h"
#include "DSMutexSemaphore.h"

//RCF#include <CoreFoundation/CoreFoundation.h>

class CDSRefTable;

extern DSMutexSemaphore		*gFWTableMutex;
extern CDSRefTable			*gFWRefTable;

#define		kMaxFWTableItems	512
#define		kMaxFWTables		0x0F

typedef struct sListFWInfo *sListFWInfoPtr;

//note fPID defined everywhere as sInt32 to remove warnings of comparing -1 to uInt32 since failed PID is -1

//struct to contain reference to the actual ref entry of the child ref
typedef struct sListFWInfo {
	uInt32			fRefNum;
	uInt32			fType;
	sInt32			fPID;
	sListFWInfoPtr	fNext;
} sListFWInfo;

//struct to contain PID of the child client process granted access to a ref from the parent client process
typedef struct sPIDFWInfo {
	sInt32			fPID;
	sPIDFWInfo	   *fNext;
} sPIDFWInfo;

//struct of the main ref entry
typedef struct sFWRefEntry {
	uInt32			fRefNum;
	uInt32			fType;
    uInt32			fOffset;
	uInt32			fBufTag;
	uInt32			fParentID;
	sInt32			fPID;
	sListFWInfo	   *fChildren;
	sPIDFWInfo	   *fChildPID;
} sFWRefEntry;

typedef sInt32 RefFWDeallocateProc ( uInt32 inRefNum, sFWRefEntry *entry );

// -------------------------------------------

typedef struct sRefFWTable *sRefFWTablePtr;

typedef struct sRefFWTable {
	uInt32			fTableNum;
	uInt32			fCurRefNum;
	uInt32			fItemCnt;
	sFWRefEntry		*fTableData[ kMaxFWTableItems ];
} sRefFWTable;

//------------------------------------------------------------------------------------
//	* CDSRefTable
//------------------------------------------------------------------------------------

class CDSRefTable {
public:
					CDSRefTable			( RefFWDeallocateProc *deallocProc );
	virtual		   ~CDSRefTable			( void );

	static tDirStatus	VerifyDirRef		( tDirReference inDirRef, sInt32 inPID );
	static tDirStatus	VerifyNodeRef		( tDirNodeReference inDirNodeRef, sInt32 inPID );
	static tDirStatus	VerifyRecordRef		( tRecordReference inRecordRef, sInt32 inPID );
	static tDirStatus	VerifyAttrListRef	( tAttributeListRef inAttributeListRef, sInt32 inPID );
	static tDirStatus	VerifyAttrValueRef	( tAttributeValueListRef inAttributeValueListRef, sInt32 inPID );

	static tDirStatus	NewDirRef			( uInt32 *outNewRef, sInt32 inPID );
	static tDirStatus	NewNodeRef			( uInt32 *outNewRef, uInt32 inParentID, sInt32 inPID );
	static tDirStatus	NewRecordRef		( uInt32 *outNewRef, uInt32 inParentID, sInt32 inPID );
	static tDirStatus	NewAttrListRef		( uInt32 *outNewRef, uInt32 inParentID, sInt32 inPID );
	static tDirStatus	NewAttrValueRef		( uInt32 *outNewRef, uInt32 inParentID, sInt32 inPID );

	static tDirStatus	RemoveDirRef		( uInt32 inDirRef, sInt32 inPID );
	static tDirStatus	RemoveNodeRef		( uInt32 inNodeRef, sInt32 inPID );
	static tDirStatus	RemoveRecordRef		( uInt32 inRecRef, sInt32 inPID );
	static tDirStatus	RemoveAttrListRef	( uInt32 inAttrListRef, sInt32 inPID );
	static tDirStatus	RemoveAttrValueRef	( uInt32 InAttrValueRef, sInt32 inPID );

	static tDirStatus	AddChildPIDToRef	( uInt32 inRefNum, uInt32 inParentPID, sInt32 inChildPID );
	
	static void			CheckClientPIDs		( bool inUseTimeOuts );
    
    static tDirStatus	GetOffset			( uInt32 inRefNum, uInt32 inType, uInt32* outOffset, sInt32 inPID );
    static tDirStatus	SetOffset			( uInt32 inRefNum, uInt32 inType, uInt32 inOffset, sInt32 inPID );
    static tDirStatus	GetBufTag			( uInt32 inRefNum, uInt32 inType, uInt32* outBufTag, sInt32 inPID );
    static tDirStatus	SetBufTag			( uInt32 inRefNum, uInt32 inType, uInt32 inBufTag, sInt32 inPID );

private:
	tDirStatus	VerifyReference		( tDirReference inDirRef, uInt32 inType, sInt32 inPID );
	tDirStatus	GetNewRef			( uInt32 *outRef, uInt32 inParentID, eRefTypes inType, sInt32 inPID );
	tDirStatus	RemoveRef			( uInt32 inRefNum, uInt32 inType, sInt32 inPID );

	tDirStatus	GetReference		( uInt32 inRefNum, sFWRefEntry **outRefData );

	tDirStatus	LinkToParent		( uInt32 inRefNum, uInt32 inType, uInt32 inParentID, sInt32 inPID );
	tDirStatus	UnlinkFromParent	( uInt32 inRefNum );

	void		RemoveChildren		( sListFWInfo *inChildList, sInt32 inPID );

	sRefFWTable*	GetNextTable		( sRefFWTable *inCurTable );
	sRefFWTable*	GetThisTable		( uInt32 inTableNum );

	sFWRefEntry*	GetTableRef			( uInt32 inRefNum );

	uInt32		UpdateClientPIDRefCount
									( sInt32 inClientPID, bool inUpRefCount, uInt32 inDirRef=0 );

	void		DoCheckClientPIDs	( bool inUseTimeOuts );

	uInt32		fTableCount;
	sRefFWTable	*fRefTables[ kMaxFWTables + 1 ];	//added 1 since table is 1-based and code depends upon having that last
													//index in as kMaxFWTables ie. note array is 0-based
	RefFWDeallocateProc *fDeallocProc;
	
//RCF	CFMutableDictionaryRef	fClientPIDList;
//RCF	DSMutexSemaphore	   *fClientPIDListLock;		//mutex on the client PID list tracking references per PID
	time_t					fSunsetTime;

};


#endif

