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
 * @header CNetInfoPlugin
 */

#ifndef __CNetInfoPlugin_H__
#define __CNetInfoPlugin_H__	1


#include <stdio.h>
#include <netinfo/ni_prot.h>
#include <CoreFoundation/CoreFoundation.h>

#include "CNodeRegister.h"
#include "PrivateTypes.h"
#include "DSMutexSemaphore.h"
#include "PluginData.h"
#include "CServerPlugin.h"


class CNiNodeList;

#define		kstrLocalDot				"."
#define		kstrParentDotDot			".."
#define		kstrDelimiter				"/"
#define		kstrNetInfoName				"NetInfo"
#define		kstrRootOnly				"root"
#define		kstrPrefixName				"/NetInfo/"
#define		kstrRootNodeName   			"/NetInfo/root"
#define		kstrLocalDomain				"/NetInfo/."
#define		kstrParentDomain			"/NetInfo/.."
#define		kstrRootLocalDomain			"/NetInfo/root/."
#define		kstrDefaultLocalNode		"DefaultLocalNode"

class CNetInfoPlugin : public CServerPlugin
{

public:
						CNetInfoPlugin		( FourCharCode inSig, const char *inName );
	virtual			   ~CNetInfoPlugin		( void );

	virtual sInt32		Validate			( const char *inVersionStr, const uInt32 inSignature );
	virtual sInt32		Initialize			( void );
	//virtual sInt32		Configure			( void );
	virtual sInt32		SetPluginState		( const uInt32 inState );
	virtual sInt32		PeriodicTask		( void );
	virtual sInt32		ProcessRequest		( void *inData );
	//virtual sInt32		Shutdown			( void );

	static	sInt32		SafeOpen			( const char *inDomainName, sInt32 inTimeoutSecs, char **outDomName );
	static	void*		GetNIDomain			( const char *inDomainName);
	static	sInt32		SafeClose			( const char *inDomainName );
	static	void		WakeUpRequests		( void );
	static	sInt32		UnregisterNode		( const uInt32 inToken, tDataList *inNode );
    
			void		NodeRegisterComplete( CNodeRegister *aRegisterThread );
			void		ReDiscoverNetwork	( void );

protected:
			void		WaitForInit			( void );
			void		HandleMultipleNetworkTransitions ( void );

private:
			void		SystemGoingToSleep	( void );
			void		SystemWillPowerOn		( void );

	// static private
    static	FourCharCode		fToken; //dupe of the signature

	// non-static private
			CNodeRegister	   *fRegisterNodePtr;
			DSMutexSemaphore	fRegisterMutex;
			uInt32				fState;
			CFRunLoopRef		fServerRunLoop;
			time_t				fTransitionCheckTime;
			bool				bFirstNetworkTransition;
};

#endif	// __CNetInfoPlugin_H__
