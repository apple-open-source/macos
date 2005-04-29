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
 * @header CHandlers
 */

#ifndef __CHandlers_h__
#define __CHandlers_h__ 1

#include "CInternalDispatchThread.h"
#include "DSTCPEndpoint.h"
#include "PrivateTypes.h"
#include "CPlugInList.h"
#include "DirServicesTypes.h"
#include "DSUtils.h"

class	CServerPlugin;

//Extern
extern DSMutexSemaphore	   *gTCPHandlerLock;

struct sRefEntry;

class CHandlerThread : public CInternalDispatchThread
{
public:
					CHandlerThread			( const FourCharCode inThreadSignature, uInt32 iThread );
	virtual		   ~CHandlerThread			( void );
	
	virtual	long	ThreadMain			( void );		// we manage our own thread top level
	virtual	void	StartThread			( void );
	virtual	void	StopThread			( void );
			uInt32	GetOurThreadRunState( void );
	static	sInt32	RefDeallocProc		( uInt32 inRefNum, uInt32 inRefType, CServerPlugin *inPluginPtr );

protected:
	virtual	void	LastChance			( void );
			
	DSTCPEndpoint   *fTCPEndPt;

private:
		void	HandleMessage					( void );
		void	LogQueueDepth					( void );

	uInt32				fThreadIndex;
};

class CMigHandlerThread : public CInternalDispatchThread
{
public:
					CMigHandlerThread			( void );
					CMigHandlerThread			( const FourCharCode inThreadSignature, bool bMigHelper );
	virtual		   ~CMigHandlerThread			( void );
	
	virtual	long	ThreadMain			( void );		// we manage our own thread top level
	virtual	void	StartThread			( void );
	virtual	void	StopThread			( void );

protected:
	virtual	void	LastChance			( void );
			
private:
	bool			bMigHelperThread;
};

class CRequestHandler
{
public:
					CRequestHandler		( void );
	
			bool	HandleRequest		( sComData **inRequest );
            static char*	GetCallName				( sInt32 inType );
			
			// for mig handler to be able to call directly
			sInt32	DoCheckUserNameAndPassword		( const char *userName, const char *password,
													  tDirPatternMatch inPatternMatch,
													  uid_t *outUID, char **outShortName );
			char*	BuildAPICallDebugDataTag		(	uInt32			inIPAddress,
														sInt32			inClientPID,
														char		   *inCallName,
														char		   *inName);
			
protected:
			sInt32	HandleServerCall	( sComData **inRequest );
			sInt32	HandlePluginCall	( sComData **inRequest );
			sInt32	HandleUnknownCall	( sComData *inRequest );
			//methods that call Add methods for sComData need ptr to ptr since the buffer can grow and the ptr might change

			bool	IsServerRequest		( sComData *inRequest );
			bool	IsPluginRequest		( sComData *inRequest );

			void*	GetRequestData		( sComData *inRequest, sInt32 *outResult, bool *outShouldProcess );
			sInt32	PackageReply		( void *inData, sComData **inRequest );
			uInt32	GetMsgType			( sComData *inRequest );
			sInt32	FailedCallRefCleanUp( void *inData, sInt32 inClientPID, uInt32 inMsgType, uInt32 inIPAddress );

			void	DoFreeMemory		( void *inData );

private:

	
		sInt32	SetRequestResult				( sComData *inMsg, sInt32 inResult );

		void*	DoOpenDirNode					( sComData *inRequest, sInt32 *outStatus );
		void*	DoFlushRecord					( sComData *inRequest, sInt32 *outStatus );
		void*	DoReleaseContinueData			( sComData *inRequest, sInt32 *outStatus );
		void*	DoPlugInCustomCall				( sComData *inRequest, sInt32 *outStatus );
		void*	DoAttributeValueSearch			( sComData *inRequest, sInt32 *outStatus );
		void*	DoAttributeValueSearchWithData	( sComData *inRequest, sInt32 *outStatus );
		void*	DoMultipleAttributeValueSearch			( sComData *inRequest, sInt32 *outStatus );
		void*	DoMultipleAttributeValueSearchWithData	( sComData *inRequest, sInt32 *outStatus );
		void*	DoFindDirNodes					( sComData *inRequest, sInt32 *outStatus );
		void*	DoCloseDirNode					( sComData *inRequest, sInt32 *outStatus );
		void*	DoGetDirNodeInfo				( sComData *inRequest, sInt32 *outStatus );
		void*	DoGetRecordList					( sComData *inRequest, sInt32 *outStatus );
		void*	DoGetRecordEntry				( sComData *inRequest, sInt32 *outStatus );
		void*	DoGetAttributeEntry				( sComData *inRequest, sInt32 *outStatus );
		void*	DoGetAttributeValue				( sComData *inRequest, sInt32 *outStatus );
		void*	DoCloseAttributeList			( sComData *inRequest, sInt32 *outStatus );
		void*	DoCloseAttributeValueList		( sComData *inRequest, sInt32 *outStatus );
		void*	DoOpenRecord					( sComData *inRequest, sInt32 *outStatus );
		void*	DoGetRecRefInfo					( sComData *inRequest, sInt32 *outStatus );
		void*	DoGetRecAttribInfo				( sComData *inRequest, sInt32 *outStatus );
		void*	DoGetRecordAttributeValueByIndex( sComData *inRequest, sInt32 *outStatus );
		void*	DoGetRecordAttributeValueByValue( sComData *inRequest, sInt32 *outStatus );
		void*	DoGetRecordAttributeValueByID	( sComData *inRequest, sInt32 *outStatus );
		void*	DoCloseRecord					( sComData *inRequest, sInt32 *outStatus );
		void*	DoSetRecordName					( sComData *inRequest, sInt32 *outStatus );
		void*	DoSetRecordType					( sComData *inRequest, sInt32 *outStatus );
		void*	DoDeleteRecord					( sComData *inRequest, sInt32 *outStatus );
		void*	DoCreateRecord					( sComData *inRequest, sInt32 *outStatus );
		void*	DoAddAttribute					( sComData *inRequest, sInt32 *outStatus );
		void*	DoRemoveAttribute				( sComData *inRequest, sInt32 *outStatus );
		void*	DoAddAttributeValue				( sComData *inRequest, sInt32 *outStatus );
		void*	DoRemoveAttributeValue			( sComData *inRequest, sInt32 *outStatus );
		void*	DoSetAttributeValue				( sComData *inRequest, sInt32 *outStatus );
		void*   DoSetAttributeValues			( sComData *inRequest, sInt32 *outStatus );
		void*	DoAuthentication				( sComData *inRequest, sInt32 *outStatus );
		void*	DoAuthenticationOnRecordType	( sComData *inRequest, sInt32 *outStatus );

		void*	GetNodeList						( sComData *inRequest, sInt32 *outStatus );
		void*	FindDirNodes					( sComData *inRequest, sInt32 *outStatus, char *inDebugDataTag );
		bool	UserIsAdmin						( const char* shortName );
		bool	UserIsMemberOfGroup				( tDirReference inDirRef, tDirNodeReference inDirNodeRef,
												  const char* shortName, const char* groupName );
		char*	GetNameForProcessID				( pid_t inPID );
		void	LogAPICall						(	double			inTime,
													char		   *inDebugDataTag,
													sInt32			inResult);
		void	DebugAPIPluginCall				(	void		   *inData,
													char		   *inDebugDataTag );
		void	DebugAPIPluginResponse			(	void		   *inData,
													char		   *inDebugDataTag,
													sInt32			inResult);
		
	CServerPlugin	   *fPluginPtr;
	bool				bClosePort;
};

#endif

