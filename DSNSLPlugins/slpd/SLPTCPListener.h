/*
	File:		SLPTCPListener.h

	Contains:	A thread that will actively listen for communications via TCP for SLP requests

	Written by:	Kevin Arnold

	Copyright:	© 2000 by Apple Computer, Inc., all rights reserved.

	Change History (most recent first):


*/
#ifndef _SLPTCPListener_
#define _SLPTCPListener_
#pragma once

#include "LThread.h"

class SLPTCPListener : public LThread
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

class TCPHandlerThread : public LThread
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







