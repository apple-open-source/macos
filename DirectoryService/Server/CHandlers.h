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

class CHandlerThread : public CInternalDispatchThread
{
public:
					CHandlerThread			( const UInt32 inThreadSignature, UInt32 iThread );
	virtual		   ~CHandlerThread			( void );
	
	virtual	SInt32	ThreadMain			( void );		// we manage our own thread top level
	virtual	void	StartThread			( void );
	virtual	void	StopThread			( void );
			UInt32	GetOurThreadRunState( void );
	static	SInt32	RefDeallocProc		( UInt32 inRefNum, UInt32 inRefType, CServerPlugin *inPluginPtr );

protected:
	virtual	void	LastChance			( void );
			
	DSTCPEndpoint   *fTCPEndPt;

private:
		void	HandleMessage					( void );
		void	LogQueueDepth					( void );

	UInt32				fThreadIndex;
};

class CPluginRunLoopThread : public CInternalDispatchThread
{
	public:
						CPluginRunLoopThread	( void );
		virtual		   ~CPluginRunLoopThread	( void );
		
		virtual	SInt32	ThreadMain				( void );		// we manage our own thread top level
		virtual	void	StartThread				( void );
		virtual	void	StopThread				( void );
	
	protected:
		virtual	void	LastChance				( void );
};

class CMigHandlerThread : public CInternalDispatchThread
{
public:
					CMigHandlerThread			( const UInt32 inThreadSignature, bool bMigHelper );
	virtual		   ~CMigHandlerThread			( void );
	
	virtual	SInt32	ThreadMain			( void );		// we manage our own thread top level
	virtual	void	StartThread			( void );
	virtual	void	StopThread			( void );

protected:
	virtual	void	LastChance			( void );
			
private:
	bool			bMigHelperThread;
};

class CMemberdKernelHandlerThread : public CInternalDispatchThread
{
public:
					CMemberdKernelHandlerThread			( const UInt32 inThreadSignature );
	virtual		   ~CMemberdKernelHandlerThread			( void );
	
	virtual	SInt32	ThreadMain			( void );		// we manage our own thread top level
	virtual	void	StartThread			( void );
	virtual	void	StopThread			( void );

protected:
	virtual	void	LastChance			( void );
			
private:
};

class CRequestHandler
{
public:
					CRequestHandler		( void );
	
			bool	HandleRequest		( sComData **inRequest );
            static char*	GetCallName				( SInt32 inType );
			
			// for mig handler to be able to call directly
			SInt32	DoCheckUserNameAndPassword		( const char *userName, const char *password,
													  tDirPatternMatch inPatternMatch,
													  uid_t *outUID, char **outShortName );
			char*	BuildAPICallDebugDataTag		(	UInt32			inIPAddress,
														SInt32			inClientPID,
														char		   *inCallName,
														char		   *inName);
			
protected:
			SInt32	HandleServerCall	( sComData **inRequest );
			SInt32	HandlePluginCall	( sComData **inRequest );
			SInt32	HandleUnknownCall	( sComData *inRequest );
			//methods that call Add methods for sComData need ptr to ptr since the buffer can grow and the ptr might change

			bool	IsServerRequest		( sComData *inRequest );
			bool	IsPluginRequest		( sComData *inRequest );

			void*	GetRequestData		( sComData *inRequest, SInt32 *outResult, bool *outShouldProcess );
			SInt32	PackageReply		( void *inData, sComData **inRequest );
			UInt32	GetMsgType			( sComData *inRequest );
			SInt32	FailedCallRefCleanUp( void *inData, SInt32 inClientPID, UInt32 inMsgType, UInt32 inIPAddress );

			void	DoFreeMemory		( void *inData );

private:

	
		SInt32	SetRequestResult				( sComData *inMsg, SInt32 inResult );

		void*	DoOpenDirNode					( sComData *inRequest, SInt32 *outStatus );
		void*	DoFlushRecord					( sComData *inRequest, SInt32 *outStatus );
		void*	DoReleaseContinueData			( sComData *inRequest, SInt32 *outStatus );
		void*	DoPlugInCustomCall				( sComData *inRequest, SInt32 *outStatus );
		void*	DoAttributeValueSearch			( sComData *inRequest, SInt32 *outStatus );
		void*	DoAttributeValueSearchWithData	( sComData *inRequest, SInt32 *outStatus );
		void*	DoMultipleAttributeValueSearch			( sComData *inRequest, SInt32 *outStatus );
		void*	DoMultipleAttributeValueSearchWithData	( sComData *inRequest, SInt32 *outStatus );
		void*	DoFindDirNodes					( sComData *inRequest, SInt32 *outStatus );
		void*	DoCloseDirNode					( sComData *inRequest, SInt32 *outStatus );
		void*	DoGetDirNodeInfo				( sComData *inRequest, SInt32 *outStatus );
		void*	DoGetRecordList					( sComData *inRequest, SInt32 *outStatus );
		void*	DoGetRecordEntry				( sComData *inRequest, SInt32 *outStatus );
		void*	DoGetAttributeEntry				( sComData *inRequest, SInt32 *outStatus );
		void*	DoGetAttributeValue				( sComData *inRequest, SInt32 *outStatus );
		void*	DoCloseAttributeList			( sComData *inRequest, SInt32 *outStatus );
		void*	DoCloseAttributeValueList		( sComData *inRequest, SInt32 *outStatus );
		void*	DoOpenRecord					( sComData *inRequest, SInt32 *outStatus );
		void*	DoGetRecRefInfo					( sComData *inRequest, SInt32 *outStatus );
		void*	DoGetRecAttribInfo				( sComData *inRequest, SInt32 *outStatus );
		void*	DoGetRecordAttributeValueByIndex( sComData *inRequest, SInt32 *outStatus );
		void*	DoGetRecordAttributeValueByValue( sComData *inRequest, SInt32 *outStatus );
		void*	DoGetRecordAttributeValueByID	( sComData *inRequest, SInt32 *outStatus );
		void*	DoCloseRecord					( sComData *inRequest, SInt32 *outStatus );
		void*	DoSetRecordName					( sComData *inRequest, SInt32 *outStatus );
		void*	DoSetRecordType					( sComData *inRequest, SInt32 *outStatus );
		void*	DoDeleteRecord					( sComData *inRequest, SInt32 *outStatus );
		void*	DoCreateRecord					( sComData *inRequest, SInt32 *outStatus );
		void*	DoAddAttribute					( sComData *inRequest, SInt32 *outStatus );
		void*	DoRemoveAttribute				( sComData *inRequest, SInt32 *outStatus );
		void*	DoAddAttributeValue				( sComData *inRequest, SInt32 *outStatus );
		void*	DoRemoveAttributeValue			( sComData *inRequest, SInt32 *outStatus );
		void*	DoSetAttributeValue				( sComData *inRequest, SInt32 *outStatus );
		void*   DoSetAttributeValues			( sComData *inRequest, SInt32 *outStatus );
		void*	DoAuthentication				( sComData *inRequest, SInt32 *outStatus );
		void*	DoAuthenticationOnRecordType	( sComData *inRequest, SInt32 *outStatus );

		void*	GetNodeList						( sComData *inRequest, SInt32 *outStatus );
		void*	FindDirNodes					( sComData *inRequest, SInt32 *outStatus, char *inDebugDataTag );
		bool	UserInGivenGroup				( const char* shortName, const char* groupName );
		void	LogAPICall						(	double			inTime,
													char		   *inDebugDataTag,
													SInt32			inResult);
		void	DebugAPIPluginCall				(	void		   *inData,
													char		   *inDebugDataTag );
		void	DebugAPIPluginResponse			(	void		   *inData,
													char		   *inDebugDataTag,
													SInt32			inResult);
		
	CServerPlugin	   *fPluginPtr;
	bool				bClosePort;
};

#endif

