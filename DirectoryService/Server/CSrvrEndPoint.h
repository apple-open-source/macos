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
 * @header CSrvrEndPoint
 */

#ifndef __CSrvrEndPoint_h__
#define __CSrvrEndPoint_h__	1

#include <mach/message.h>

#include "SharedConsts.h"
#include "PrivateTypes.h"

typedef struct sMsgList
{
	bool				fComplete;
	uInt32				fTime;
	uInt32				fMsgID;
	uInt32				fPortID;
	uInt32				fOffset;
	sComData		   *fData;
	sMsgList		   *fNext;
} sMsgList;

//------------------------------------------------------------------------------
//	* CSrvrEndPoint
//------------------------------------------------------------------------------

class CSrvrEndPoint
{
public:
					CSrvrEndPoint			( char *inSrvrName );
	virtual		   ~CSrvrEndPoint			( void );

	sInt32			Initialize				( void );

	// Server comm
	sInt32			SendClientReply			( void *inMsg );
	void*			GetClientMessage		( void );

private:
	sComData*		GetNextCompletedMsg		( void );
	sComData*		MakeNewMsgPtr			( sIPCMsg *inMsg );
	void			AddNewMessage			( sComData *inMsg, sIPCMsg *inMsgData );
	void			AddDataToMessage		( sIPCMsg *inMsgData );

	char		   *fSrvrName;

	sMsgList	   *fHeadPtr;

	mach_port_t		fServerPort;
	mach_port_t		fMachInitPort;
};

#endif
