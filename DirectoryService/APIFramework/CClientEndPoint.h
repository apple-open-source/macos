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
 * @header CClientEndPoint
 * Mach endpoint for DirectoryService Framework.
 */

#ifndef __CClientEndPoint_h__
#define __CClientEndPoint_h__	1

#include <mach/message.h>

#include "PrivateTypes.h"
#include "SharedConsts.h"
#include "DirServicesTypesPriv.h"


//------------------------------------------------------------------------------
//	* CClientEndPoint
//------------------------------------------------------------------------------

class CClientEndPoint
{
public:
					CClientEndPoint			( const char *inSrvrName );
	virtual		   ~CClientEndPoint			( void );

	static	uInt32	fMessageID;
	static	uInt32	GetMessageID		( void );

	sInt32			Initialize			( void );
	sInt32			CheckForServer		( void );
	sInt32			SendServerMessage	( sComData *inMsg );
	sInt32			GetServerReply		( sComData **outMsg );

private:
	char		   *fSrvrName;

	mach_port_t		fServerPort;
	mach_port_t		fReplyPort;
	mach_port_t		fNotifyPort;
    mach_port_t		fRcvPortSet;

};

#endif
