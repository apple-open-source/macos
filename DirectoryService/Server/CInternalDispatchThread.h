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
 * @header CInternalDispatchThread
 */

#ifndef __CInternalDispatchThread_h__
#define __CInternalDispatchThread_h__ 1

#include "DSCThread.h"
#include "PrivateTypes.h"
#include "SharedConsts.h"

class CInternalDispatchThread : public DSCThread
{
public:
					CInternalDispatchThread		( const OSType inThreadSig );
	virtual		   ~CInternalDispatchThread		( void );

	virtual long	ThreadMain			( void ) = 0;	// pure virtual

	//handles internal dispatch message buffers
	void			SetHandlerInternalMsgData   ( void );
	void			ResetHandlerInternalMsgData ( void );
	sComData	   *GetHandlerInternalMsgData   ( void );
	void			UpdateHandlerInternalMsgData( sComData* inOldMsgData, sComData* inNewMsgData);
	void			SetInternalDispatchActive   ( bool inInternalDispatchActive );
	bool			GetInternalDispatchActive   ( void );

protected:

private:
	sInt32			fInternalDispatchStackHeight;
	sComData	   *fHandlerInternalMsgData;
	sComData	   *fInternalMsgDataList[kMaxInternalDispatchRecursion];
	bool			bInternalDispatchActive;
};

#endif
