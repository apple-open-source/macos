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
 * @header CMessaging
 * Communications class for the DirectoryService Framework providing
 * connection with a DirectoryService daemon via either a mach or TCP endpoint
 * and handling all the message data packing and unpacking.
 */

#ifndef __CMessaging_h__
#define __CMessaging_h__		1

#include "PrivateTypes.h"
#include "SharedConsts.h"
#include "DirServicesTypes.h"

#ifndef SERVERINTERNAL
#include "CClientEndPoint.h"
#include "DSTCPEndpoint.h"
#endif

class		DSMutexSemaphore;

class CMessaging {
public:

	   			CMessaging					( void );
	   			CMessaging					( Boolean inMachEndpoint );
virtual		   ~CMessaging					( void );

		sInt32	ConfigTCP					(	const char *inRemoteIPAddress,
												uInt32 inRemotePort );
#ifndef SERVERINTERNAL
		sInt32	SendServerMessage			( void );
		sInt32	SendRemoteMessage			( void );
#endif
		sInt32	GetReplyMessage				( void );

		sInt32	OpenCommPort				( void );
		sInt32	CloseCommPort				( void );
		sInt32	OpenTCPEndpoint				( void );
		sInt32	CloseTCPEndpoint			( void );

		sInt32	Add_tDataBuff_ToMsg			( tDataBuffer *inBuff, eValueType inType );
		sInt32	Add_tDataList_ToMsg			( tDataList *inList, eValueType inType );
		sInt32	Add_Value_ToMsg				( uInt32 inValue, eValueType inType );
		sInt32	Add_tAttrEntry_ToMsg		( tAttributeEntry *inData );
		sInt32	Add_tAttrValueEntry_ToMsg	( tAttributeValueEntry *inData );
		sInt32	Add_tRecordEntry_ToMsg		( tRecordEntry *inData );

		sInt32	Get_tDataBuff_FromMsg		( tDataBuffer **outBuff, eValueType inType );
		sInt32	Get_tDataList_FromMsg		( tDataList **outList, eValueType inType );
		sInt32	Get_Value_FromMsg			( uInt32 *outValue, eValueType inType );
		sInt32	Get_tAttrEntry_FromMsg		( tAttributeEntry **outAttrEntry, eValueType inType );
		sInt32	Get_tAttrValueEntry_FromMsg	( tAttributeValueEntry **outAttrValue, eValueType inType );
		sInt32	Get_tRecordEntry_FromMsg	( tRecordEntry **outRecEntry, eValueType inType );

		sInt32	SendInlineMessage			( uInt32 inMsgType );

		void	Lock						( void );
		void	Unlock						( void );
		void	ClearMessageBlock			( void );
		
private:
		sInt32	GetEmptyObj					( sComData *inMsg, eValueType inType, sObject **outObj );
		sInt32	GetThisObj					( sComData *inMsg, eValueType inType, sObject **outObj );

		void	Grow						( uInt32 inOffset, uInt32 inSize );

#ifndef SERVERINTERNAL
		CClientEndPoint	   *fCommPort;
		
		DSTCPEndpoint	   *fTCPEndpoint;
		uInt32				fRemoteIPAddress;
		uInt32				fRemotePort;
#endif
		
		DSMutexSemaphore   *fLock;

		sComData		   *fMsgData;
		Boolean				bMachEndpoint;	//mach = true and TCP = false
};

#endif
