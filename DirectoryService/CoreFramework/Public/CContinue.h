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
 * @header CContinue
 */

#ifndef __CContinue_h__
#define	__CContinue_h__	1

#include "PrivateTypes.h"
#include "DSMutexSemaphore.h"

typedef void DeallocateProc ( void *inData );

class CContinue {
public:
enum {
	kErrItemNotFound	= -3020,
	kErrDuplicateFound	= -3021
} eRefErrors;

enum {
	kTableSize	= 1024
};

private:
typedef struct sTableEntry {
	uInt32			fRefNum;
	uInt32			fTimeStamp;
	void		   *fData;
	sTableEntry	   *fNext;
} sTableEntry;

public:
					CContinue		( DeallocateProc *inProcPtr );
	virtual		   ~CContinue		( void );

	sInt32			AddItem			( void *inData, uInt32 inRefNum );
	sInt32			RemoveItem		( void *inData );
	sInt32			RemoveItems		( uInt32 inRefNum );
	bool			VerifyItem		( void *inData );

private:
			sTableEntry			*fLookupTable[ kTableSize ];
			DeallocateProc		*fDeallocProcPtr;

			DSMutexSemaphore		fMutex;
};

#endif

