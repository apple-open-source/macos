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
 * @header CConfigurePlugin
 */

#ifndef __CConfigurePlugin_H__
#define __CConfigurePlugin_H__	1

#include <stdio.h>

#include "DirServicesTypes.h"
#include "PrivateTypes.h"
#include "PluginData.h"
#include "CServerPlugin.h"

typedef struct {
	uInt32 offset;
	SCPreferencesRef session;
	uid_t fUID;
	uid_t fEffectiveUID;
} sConfigContextData;

class CConfigurePlugin : public CServerPlugin
{

public:
					CConfigurePlugin		( FourCharCode inSig, const char *inName );
	virtual		   ~CConfigurePlugin		( void );

	static	void	WakeUpRequests			( void );
	static	void	ContinueDeallocProc		( void* inContinueData );
	static	void	ContextDeallocProc		( void* inContextData );

	virtual sInt32	Validate				( const char *inVersionStr, const uInt32 inSignature );
	virtual sInt32	Initialize				( void );
	//virtual sInt32	Configure				( void );
	virtual sInt32	SetPluginState			( const uInt32 inState );
	virtual sInt32	PeriodicTask			( void );
	virtual sInt32	ProcessRequest			( void *inData );
	//virtual sInt32	Shutdown				( void );

protected:
	sInt32			HandleRequest			( void *inData );
	void			WaitForInit				( void );

private:
	sInt32			OpenDirNode				( sOpenDirNode *inData );
	sInt32			CloseDirNode			( sCloseDirNode *inData );
	sInt32			GetDirNodeInfo			( sGetDirNodeInfo *inData );
	sInt32			GetRecordList			( sGetRecordList *inData );
	sInt32			GetRecordEntry			( sGetRecordEntry *inData );
	sInt32			GetAttributeEntry		( sGetAttributeEntry *inData );
	sInt32			GetAttributeValue		( sGetAttributeValue *inData );
	sInt32			CloseAttributeList		( sCloseAttributeList *inData );
	sInt32			CloseAttributeValueList	( sCloseAttributeValueList *inData );

	sInt32			ReleaseContinueData		( sReleaseContinueData *inData );
    sConfigContextData   *MakeContextData			( void );
    sInt32			DoPlugInCustomCall		( sDoPlugInCustomCall *inData );
    sInt32          DoDirNodeAuth           ( sDoDirNodeAuth *inData );

	tDataList	   *fConfigNodeName;
	uInt32			fNodeCount;
	char		   *fConfigPath;
	uInt32			fState;
};

#endif	// __CConfigurePlugin_H__
