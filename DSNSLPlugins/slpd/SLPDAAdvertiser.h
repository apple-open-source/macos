/*
 *  SLPDAAdvertiser.h
 *  NSLPlugins
 *
 *  Created by root on Fri Sep 29 2000.
 *  Copyright (c) 2000 Apple Computer. All rights reserved.
 *
 */

/*
	File:		SLPDAAdvertiser.h

	Contains:	A thread that will actively advertise the DA's presence

	Written by:	Kevin Arnold

	Copyright:	© 2000 by Apple Computer, Inc., all rights reserved.

	Change History (most recent first):


*/
#ifndef _SLPDAAdvertiser_
#define _SLPDAAdvertiser_
#pragma once

#include "LThread.h"

class SLPDAAdvertiser : public LThread
{
public:

                        SLPDAAdvertiser				( SAState* psa, Boolean isMainAdvertiser );
                        ~SLPDAAdvertiser			();

	virtual void*		Run							();
    
            SLPInternalError	Initialize					( void );
            
            void		SetRunForever				( Boolean runforever ) { mRunForever = runforever; };
            
            void		RestartAdvertisements		( void );
	
			Boolean		SafeToUse					( void ) { return this == mSelfPtr; };

protected:			
            SLPDAAdvertiser*		mSelfPtr;
            SAState*				mServerState;
            SOCKET					mSocket;
            struct sockaddr_in		mSockAddr_in;
            long					mTimeToMakeNextAdvert;
            Boolean					mRunForever;
};
#endif







