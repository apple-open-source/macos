/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*!
 * @header CPlugInRef
 */

#ifndef __CPlugInRef_h__
#define	__CPlugInRef_h__	1

#include "PrivateTypes.h"
#include "DSMutexSemaphore.h"

typedef void DeallocateProc ( void *inData );
typedef void OperationProc ( void *inData );

class CPlugInRef {

enum {
	kErrItemNotFound	= -3020,
	kErrDuplicateFound	= -3021
} eRefErrors;

enum {
	kTableSize	= 1024
};


typedef struct sTableEntry {
	uInt32			fRefNum;
	uInt32			fTimeStamp;
	void		   *fData;
	sTableEntry	   *fNext;
} sTableEntry;

public:
					CPlugInRef		( DeallocateProc *inProcPtr );
	virtual		   ~CPlugInRef		( void );

	sInt32			AddItem			( uInt32 inRefNum, void *inData );
	sInt32			RemoveItem		( uInt32 inRefNum );
	void*			GetItemData		( uInt32 inRefNum );
	void			DoOnAllItems	( OperationProc *inProcPtr );

private:
			sTableEntry			*fLookupTable[ kTableSize ];
			DeallocateProc		*fDeallocProcPtr;

			DSMutexSemaphore		fMutex;
};

#endif

