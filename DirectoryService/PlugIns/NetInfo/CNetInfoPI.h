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
 * @header CNetInfoPI
 */

#ifndef __CNetInfoPI_H__
#define __CNetInfoPI_H__	1


#include <stdio.h>
#include <netinfo/ni_prot.h>
#include <CoreFoundation/CoreFoundation.h>

#include "CDSServerModule.h"
#include "CNodeRegister.h"
#include "PrivateTypes.h"
#include "DSMutexSemaphore.h"

class CNiNodeList;

#define		kstrDelimiter				"/"
#define		kstrNetInfoName				"NetInfo"
#define		kstrRootOnly				"root"
#define		kstrPrefixName				"/NetInfo"
#define		kstrRootName				"/NetInfo/"
#define		kstrRootNodeName   			"/NetInfo/root"
#define		kstrLocalDomain				"/NetInfo/."
#define		kstrParentDomain			"/NetInfo/.."

class CNetInfoPI : public CDSServerModule
{
typedef	enum
{
	kUnknown		= 0x00000000,
	ktInited		= 0x00000001,
	kActive			= 0x00000002,
	kFailedToInti	= 0x00000004
} ePlugInState;

public:
						CNetInfoPI			( void );
	virtual			   ~CNetInfoPI			( void );

	virtual sInt32		Validate			( const char *inVersionStr, const uInt32 inSignature );
	virtual sInt32		Initialize			( void );
	virtual sInt32		PeriodicTask		( void );
	virtual sInt32		ProcessRequest		( void *inData );

	static	sInt32		SafeOpen			( const char *inDomainName, sInt32 inTimeoutSecs, ni_id *outNiDirID, void **outDomain, char **outDomName );
	static	sInt32		SafeClose			( const char *inDomainName );
	static	void		WakeUpRequests		( void );
	static	sInt32		UnregisterNode		( const uInt32 inToken, tDataList *inNode );
			void		NodeRegisterComplete( CNodeRegister *aRegisterThread );
			void		ReDiscoverNetwork	( void );

protected:
			void		WaitForInit			( void );
			void		HandleMultipleNetworkTransitions ( void );

public:
	// static public
	static	char			   *fLocalDomainName;

private:
	// static private
	static	CNiNodeList		   *fNiNodeList;
	static	uInt32				fSignature;

	// non-static private
			CNodeRegister	   *fRegisterNodePtr;
			DSMutexSemaphore	fRegisterMutex;
			uInt32				fState;
			CFRunLoopRef		fServerRunLoop;
			time_t				fTransitionCheckTime;
};

#endif	// __CNetInfoPI_H__
