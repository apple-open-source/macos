/*
	File:		SLPUDPListener.h

	Contains:	A thread that will actively listen for communications via UDP for SLP requests

	Written by:	Kevin Arnold

	Copyright:	© 2000 by Apple Computer, Inc., all rights reserved.

	Change History (most recent first):


*/
#ifndef _SLPUDPListener_
#define _SLPUDPListener_
#pragma once

#include "LThread.h"
class UDPMessageObject;

class SLPUDPListener : public LThread
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
};

class SLPUDPHandler : public LThread
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







