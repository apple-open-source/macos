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
 *  @header SLPTCPListener
 *  A thread that will actively listen for communications via TCP for SLP requests
 */
 
#ifndef _SLPTCPListener_
#define _SLPTCPListener_
#pragma once

#include <DirectoryServiceCore/DSLThread.h>

class SLPTCPListener : public DSLThread
{
public:

	SLPTCPListener( SAState* psa, OSStatus *status );
	~SLPTCPListener();

	virtual void*		Run();
            void		Cancel( void );
			Boolean		SafeToUse( void ) { return this == mSelfPtr; };

protected:			
	SLPTCPListener*		mSelfPtr;
	SAState*			mServerState;
    Boolean				mCanceled;
};

class TCPHandlerThread : public DSLThread
{
public:

	TCPHandlerThread( OSStatus *status );
	~TCPHandlerThread();

    void				Initialize( SOCKET newRequest, SAState* serverState, struct sockaddr_in sinIn  );
	virtual void*		Run();
	
protected:			
	SAState*			mServerState;
    SOCKET				mRequestSD;
    sockaddr_in			mSinIn;
};

#endif
