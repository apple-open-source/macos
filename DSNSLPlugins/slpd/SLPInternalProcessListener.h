/*
	File:		SLPInternalProcessListener.h

	Contains:	A thread that will actively listen for communications from our SA Plugin or
				RAdmin plugin to administer this deamon

	Written by:	Kevin Arnold

	Copyright:	© 2000 by Apple Computer, Inc., all rights reserved.

	Change History (most recent first):


*/
#ifndef _SLPInternalProcessListener_
#define _SLPInternalProcessListener_
#pragma once

#define	kUDPBufferSize		1400

class SLPInternalProcessListener : public LThread
{
public:

	SLPInternalProcessListener( SAState* psa, OSStatus *status );
	~SLPInternalProcessListener();

    OSStatus	Initialize();
    virtual	void*					Run();
	
    Boolean		SafeToUse( void ) { return this == mSelfPtr; };

protected:
//    void		HandleCommunication( int sockfd, sockaddr_un client_addr, int clientLen );
			
	SLPInternalProcessListener*	mSelfPtr;
	SAState*					mServerState;
	SLPHandle					mSLPSA;
    int							mSockfd;
};


class SLPInternalProcessHandlerThread : public LThread
{
public:
    SLPInternalProcessHandlerThread();
    ~SLPInternalProcessHandlerThread();
    
    virtual	void*					Run();

            void					Initialize( SOCKET newRequest, SAState* serverState );
protected:
            void					HandleCommunication();

	SAState*					mServerState;
    int							mRequestSD;
};


#endif







