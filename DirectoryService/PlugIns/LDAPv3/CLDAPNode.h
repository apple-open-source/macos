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
 * @header CLDAPNode
 * LDAP node management class.
 */

#ifndef __CLDAPNode_h__
#define __CLDAPNode_h__	1

#include <stdio.h>
#include <string.h>		//used for strcpy, etc.
#include <stdlib.h>		//used for malloc
#include <map>			//STL map class
#include <string>		//STL string class
#include <vector>		//STL vector class
#include <time.h>		//time_t usage

#include "DirServices.h"
#include "DirServicesUtils.h"
#include "DirServicesConst.h"

#include "PrivateTypes.h"
#include "DSCThread.h"
#include "DSMutexSemaphore.h"
#include "CLDAPv3Configs.h"

#include <lber.h>
#include <ldap.h>

using namespace std;

typedef struct sLDAPNodeStruct sLDAPNodeStruct;

// Context data structure
//KW need to get away from using UserDefName, Name, and Port in the context but get it
// from the config table
typedef struct sLDAPContextData {
	LDAP		   *fHost;				//LDAP session handle
	DSMutexSemaphore
				   *fLDAPSessionMutex;	//mutex for changing the session handle if authCallActive
	uInt32			fConfigTableIndex;	//gLDAPConfigTable Hash index
	char		   *fName;				//LDAP domain name ie. ldap.apple.com
	int				fPort;				//LDAP port number - default is 389 - SSL default port is 636
	int				fType;				//KW type of reference entry - not used yet
    bool			authCallActive;		//indicates if authentication was made through the API
    									//call and if set means don't use config file auth name/password
    uInt32			offset;				//offset into the data buffer
    uInt32			index;
    char		   *fOpenRecordType;	//record type used to open a record
    char		   *fOpenRecordName;	//record name used to open a record
    char		   *fOpenRecordDN;		//record name used to open a record
	char		   *fUserName;			//LDAP user name
	void		   *fAuthCredential;	//LDAP user authentication credential default is char* password
	char		   *fAuthType;			//LDAP authentication type ie. kDSStdAuthClearText means password used
    
    tDirReference	fPWSRef;
    tDirNodeReference	fPWSNodeRef;
    uid_t			fUID;
    uid_t			fEffectiveUID;
	sLDAPNodeStruct		*fLDAPNodeStruct;
} sLDAPContextData;

struct sLDAPNodeStruct {
	LDAP		   *fHost;					//LDAP session handle
	DSMutexSemaphore
				   *fLDAPSessionMutex;		//mutex for changing the session handle
	uInt32			fRefCount;				//ref count on host handle
	uInt32			fOperationsCount;		//ref count of operations in progress
	uInt32			fLDAPConfigTableIndex;	//gLDAPConfigTable index
	char		   *fServerName;			//server name used if directed open ie. fLDAPConfigTableIndex = 0
	int				fDirectLDAPPort;		//port if directed open ie. fLDAPConfigTableIndex = 0
	char		   *fUserName;				//LDAP user name
	void		   *fAuthCredential;		//LDAP user authentication credential default is char* password
	char		   *fAuthType;				//LDAP authentication type ie. kDSStdAuthClearText means password used
	int				fConnectionStatus;		//The status of this connection, kUnknown, kSafe, kUnsafe, supersedes bHasFailed
	time_t			fDelayedBindTime;		//time after which to retry to bind
	uInt32			fConnectionActiveCount;	//count of active use of connection
	int				fIdleTOCount;			//count of 30 sec periodic task calls for idle connection release
	int				fIdleTO;				//user defined idle timeout in minutes times 2 based on 30 sec periodic task
	int				fDelayRebindTry;		//Delay rebind try after bind failure in seconds
};

typedef map<string, sLDAPNodeStruct*>	LDAPNodeMap;
typedef LDAPNodeMap::iterator			LDAPNodeMapI;
typedef vector<sLDAPNodeStruct*>		LDAPNodeVector;
typedef LDAPNodeVector::iterator		LDAPNodeVectorI;

enum {
	kConnectionSafe = 0,	
	kConnectionUnsafe,
	kConnectionUnknown
};

//KW current concept is to NOT save the AuthNodeMaps for reuse but allow rebinding if no Continue

class CLDAPNode
{
public:
	static bool		fCheckThreadActive;

public:
                	CLDAPNode		(	void );
	virtual		   ~CLDAPNode		(	void );
	sInt32			SafeOpen		(	char	   *inNodeName,
										LDAP	  **outLDAPHost,
										uInt32	   *outLDAPConfigTableIndex,
                                        CLDAPv3Configs *inConfigFromXML );
									//if already open then just get host and config index if host okay
									//else try rebind first
									//if not open then bind to get host and search for config index
									//called from OpenDirNode
	sInt32			AuthOpen		(	char	   *inNodeName,
										LDAP	   *inHost,
										char	   *inUserName,
										void	   *inAuthCredential,
										char	   *inAuthType,
										LDAP	  **outLDAPHost,
										uInt32	   *inOutLDAPConfigTableIndex,
										bool	   shouldCloseOld );
									//there must be a fLDAPNodeMap entry for this ie. SafeOpen was already called since
									//this would come from a known Node Ref
									//this is not ref counted NOR reused
									//inOutLDAPConfigTableIndex could be passed in for a directed open ie. not zero
									//called from the hierarchy below DoAuthentication
	sInt32			RebindSession	(	char	   *inNodeName,
										LDAP	   *inHost,
										CLDAPv3Configs *inConfigFromXML,
										LDAP	  **outLDAPHost );
									//must already be open
									//check if already rebound during a continue for some other node ref
									//use rebind if continue not set
									//called from CLDAPv3Plugin::RebindLDAPSession
	sInt32			SimpleAuth		(	char	   *inNodeName,
										char	   *inUserName,
										void	   *inAuthCredential );
									//use rebind to do the auth
									//called from DoClearTextAuth
	sInt32			RebindAuthSession(	char	   *inNodeName,
										LDAP	   *inHost,
										char	   *inUserName,
										void	   *inAuthCredential,
										char	   *inAuthType,
										uInt32	   inLDAPConfigTableIndex,
										LDAP	  **outLDAPHost );
									//use rebind if continue not set
									//called from CLDAPv3Plugin::RebindLDAPSession
	sInt32			SafeClose		(	char	   *inNodeName,
										LDAP	   *inHost);
									//decrement ref count and delete if ref count zero
									//called from CloseDirNode
	sInt32			ForcedSafeClose	(	char	   *inNodeName);
									//delete regardless of refcount since config has been removed
									//called from Initialize due to config removal

	void			GetSchema		( sLDAPContextData *inContext );
	LDAP* 			LockSession		( sLDAPContextData *inContext );
	void			UnLockSession	( sLDAPContextData *inContext, bool inHasFailed = false, bool inNewMutex = false );
	void			CheckIdles		( void );
	void			CheckFailed		( void );
	void			EnsureCheckFailedConnectionsThreadIsRunning	( void );
	void			NetTransition   ( void );
	void			ActiveConnection( char *inNodeName );
	void			IdleConnection	( char *inNodeName );
	LDAP*			InitLDAPConnection
									(	sLDAPNodeStruct *inLDAPNodeStruct,
										sLDAPConfigData *inConfig,
										CLDAPv3Configs *inConfigFromXML = nil,
										bool bInNeedWriteable = false );
	static struct addrinfo*
					ResolveHostName	( 	CFStringRef inServerNameRef,
										int inPortNumber );
	static LDAP*	EstablishConnection
									( 	sReplicaInfo *inList,
										int inPort,
										int inOpenTimeout,
										bool bInNeedWriteable = false );

protected:
	
	sInt32			CleanLDAPNodeStruct	(	sLDAPNodeStruct	   *inLDAPNodeStruct );
	sInt32			BindProc			(	sLDAPNodeStruct	   *inLDAPNodeStruct,
											CLDAPv3Configs *inConfigFromXML = nil,
											bool bSessionBased = false, bool bForceBind = false );
	sInt32			ParseLDAPNodeName	(	char	   *inNodeName,
											char	  **outLDAPName,
											int		   *outLDAPPort );
											//parse a string with a name and possibly a suffix of ':' followed by a port number
	sInt32			GetSchemaMessage	(	LDAP *inHost,
											int inSearchTO,
											LDAPMessage **outResultMsg );
	sInt32			GetReplicaListMessage
										(	LDAP *inHost,
											int inSearchTO,
											char *inConfigServerString,
											CFMutableArrayRef outList,
											CFMutableArrayRef outWriteableList );
	char**			GetNamingContexts	(	LDAP *inHost,
											int inSearchTO,
											uInt32 *outCount );
	sInt32			ExtractReplicaListMessage
										(	LDAP *inHost,
											int inSearchTO, 
											sLDAPNodeStruct *inLDAPNodeStruct,
											CLDAPv3Configs *inConfigFromXML,
											CFMutableArrayRef outList,
											CFMutableArrayRef outWriteableList );
	sInt32			RetrieveDefinedReplicas
										(	sLDAPNodeStruct *inLDAPNodeStruct, 
											CLDAPv3Configs *inConfigFromXML,
											char *inConfigServerString,
											CFMutableArrayRef &inOutRepList,
											CFMutableArrayRef &inOutWriteableList,
											int inPort,
											sReplicaInfo **inOutList );
	bool			IsTokenNotATag		(	char *inToken );
	void			RetrieveServerMappingsIfRequired
										(   sLDAPNodeStruct *inLDAPNodeStruct,
											CLDAPv3Configs *inConfigFromXML);
	void			FreeReplicaList		(   sReplicaInfo *inList );
	static char *   LDAPWithBlockingSocket
										(   struct addrinfo *addrInfo, int seconds );
	static char *   ConvertToIPAddress  (   struct addrinfo *addrInfo );
	static bool		IsLocalAddress		(   struct addrinfo *addrInfo );
	static bool		ReachableAddress	(   struct addrinfo *addrInfo );
	
	void			CheckSASLMethods	(   sLDAPNodeStruct *inLDAPNodeStruct,
							 CLDAPv3Configs *inConfigFromXML );
	bool			LocalServerIsLDAPReplica	( void );

private:

	LDAPNodeMap			fLDAPNodeMap;
	LDAPNodeVector		fDeadPoolLDAPNodeVector;
	DSMutexSemaphore	fLDAPNodeOpenMutex; //used for the ldap_bind as well as the LDAPNodeMap container

};

#endif	// __CLDAPNode_h__
