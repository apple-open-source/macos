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
 *  @header SLPUDPListener
 *  A thread that will actively listen for communications via UDP for SLP requests
 */
 
#ifndef _SLPUDPListener_
#define _SLPUDPListener_
#pragma once

#include <DirectoryServiceCore/DSLThread.h>

#define kMaxNumFailures	10

class UDPMessageObject;

class SLPUDPListener : public DSLThread
{
public:

	SLPUDPListener( SAState* psa, OSStatus *status );
	~SLPUDPListener();

    void				Cancel( void );
	virtual void*		Run();
	
protected:			
	SLPUDPListener*		mSelfPtr;
    SAState*			mServerState;
    Boolean				mCanceled;
	int					mNumBadDescriptors;
};

class SLPUDPHandler : public DSLThread
{
public:

	SLPUDPHandler( SAState* psa );
	~SLPUDPHandler();

    void			Cancel( void );
	virtual void*		Run();

    void			AddUDPMessageToQueue( SAState *psa, char* pcInBuf, int bufSize, struct sockaddr_in sinIn );
	
    static void			QueueLock( void ) { pthread_mutex_lock( &mQueueLock ); }
    static void			QueueUnlock( void ) { pthread_mutex_unlock( &mQueueLock ); }
    static pthread_mutex_t	mQueueLock;
    
protected:			
    void			DoPeriodicTasks(void);
    void			HandleMessage( UDPMessageObject* udpMessage );
    
private:
    CFMutableArrayRef		mUDPQueue;
    SAState*			mServerState;
    Boolean			mCanceled;
};

class UDPMessageObject
{
public:
    UDPMessageObject(SAState *psa, char* pcInBuf, int bufSize, struct sockaddr_in sinIn);
    ~UDPMessageObject();

	SAState*			mServerState;
    char*				mInBuf;
    struct sockaddr_in	mSinIn;
    int					mBufSize;
};

#endif
