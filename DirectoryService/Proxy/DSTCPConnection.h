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
 * @header DSTCPConnection
 * Connection maintaining object implementation using DSTCPEndpoint.
 */

#ifndef __DSTCPConnection_h__
#define __DSTCPConnection_h__	1

#include "DSCThread.h"
#include "SharedConsts.h"	// for sComData
#include "DSTCPEndpoint.h"

class DSTCPListener;

class DSTCPConnection : public DSCThread
{

public:
	enum
	{
		kEmptyQueueObj	= -128
	} eTypes;

							DSTCPConnection		( DSTCPListener  *inParent );
	virtual				   ~DSTCPConnection		( void );

	virtual	sInt32			ThreadMain			( void ); // we manage our own thread top level
	virtual	void			StartThread			( void );
	virtual	void			StopThread			( void );

	virtual void			SetEndpoint			( DSTCPEndpoint *inTCPEndpoint );

protected:
			DSTCPEndpoint  *fTCPEndPt;

private:
			bool			ListenForMessage	( void );
			sInt32			QueueMessage		( void );

			sComData	   *fMsgBlock;
			
			int				mCurrentState;
			uInt32			mConnectionType;
			DSTCPListener  *fParent;
			bool			bFirstMsg;

};

#endif
