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

#define	kMinTimeToLogLockBeingHeld			1

//#define LOG_LDAPSessionMutex_Attempts
//#define LOG_LDAPNodeOpenMutex_Attempts

// LDAP Node Data - One exists for each open connection, shared via fRefCount
// Stored in a Map based on ConfigName : sLDAPContextData->fAuthUserName (e.g., ldap.apple.com:admin )
struct sLDAPNodeStruct
{
	char		   *fNodeName;				//used for map searches based on nodeName, replaces configIndex
											// if this is NULL, then no config
	char		   *fLDAPServer;			// This is the LDAP Server that was connected to (could be different than original)

	LDAP		   *fHost;					//LDAP session handle
	DSMutexSemaphore
				   *fLDAPSessionMutex;		//mutex for changing the session handle

	uInt32			fRefCount;				//ref count on host handle
	uInt32			fConnectionActiveCount;	//count of active use of connection

	char		   *fDirectServerName;		//server name used if directed open ie. fConfigName = NULL
	int				fDirectLDAPPort;		//port if directed open ie. fConfigName = NULL

	// These are populated when a connection is opened from the LDAPConfigData
	
	// If an authCall is active, then the local username and password always override config
    bool			bAuthCallActive;		//indicates if authentication was made through the API
	bool			bBadSession;			//this means don't try credentials due to password bad
	
	// If no Config available, the local credentials are used instead.  
	//    This is just in case the config is removed
	char		   *fKerberosId;			//KerberosID of LDAP user if present
	char		   *fLDAPUserName;			//LDAP user name
	void		   *fLDAPCredentials;		//LDAP user authentication credential default is char* password
	uInt32			fLDAPCredentialsLen;	//Length of LDAP Credentials
	char		   *fLDAPAuthType;			//LDAP authentication type ie. kDSStdAuthClearText means password used

	int				fConnectionStatus;		//The status of this connection, kUnknown, kSafe, kUnsafe, supersedes bHasFailed
	time_t			fDelayedBindTime;		//time after which to retry to bind
	
	int				fIdleTOCount;			//count of 30 sec periodic task calls for idle connection release
	int				fIdleTO;				//user defined idle timeout in minutes times 2 based on 30 sec periodic task
	int				fDelayRebindTry;		//Delay rebind try after bind failure in seconds

	double			fLDAPSessionMutexStartTime;
											// keep track of how long the mutex was held

	public:
			sLDAPNodeStruct( void );
			~sLDAPNodeStruct( void );
		
			sLDAPNodeStruct( const sLDAPNodeStruct &inLDAPNodeStruct);
		
#define SessionMutexWait()		SessionMutexWaitWithFunctionName(__PRETTY_FUNCTION__)
#define SessionMutexSignal()	SessionMutexSignalWithFunctionName(__PRETTY_FUNCTION__)
	void	SessionMutexWaitWithFunctionName	( const char* callingFunction );
	void	SessionMutexSignalWithFunctionName	( const char* callingFunction  );

	void	setLastLDAPServer( const char *inServer )
	{
		SessionMutexWait();
		if( fLDAPServer )
		{
			delete fLDAPServer;
		}
		fLDAPServer = (inServer ? strdup(inServer) : NULL);
		SessionMutexSignal();
	}
	void	updateCredentials( void *inLDAPCredentials, uInt32 inLDAPCredentialLen, char *inAuthType );
	
	inline void ChangeRefCountBy( int inValue )
	{
		SessionMutexWait();
		fRefCount += inValue;
		if ( bAuthCallActive && fRefCount <= 0 && fHost != NULL )
		{
			ldap_unbind(fHost);
			fHost = NULL;
		}
		SessionMutexSignal();
	}
};

// Context data structure
struct sLDAPContextData
{
	char		   *fNodeName;			//Node name used for sLDAPConfigDataMap
	int				fType;				//KW type of reference entry - not used yet
    uInt32			offset;				//offset into the data buffer
    uInt32			index;
    char		   *fOpenRecordType;	//record type used to open a record
    char		   *fOpenRecordName;	//record name used to open a record
    char		   *fOpenRecordDN;		//record name used to open a record
	
    bool			bLDAPv2ReadOnly;	//indicates that this is a LDAPv2 read only node

	// this Credential Information is stored here cause it is not LDAP oriented, it is Node Oriented
	char		   *fAuthUserName;		//Auth'd username, not an LDAP DN, recordname-style
	void		   *fAuthCredential;	//Credentials of user, can be any type, only cleartext/kerberos allows node authentications
	uInt32			fAuthCredentialLen; //Length of AuthCredentials
	char		   *fAuthType;			//Authentication type ie. kDSStdAuthClearText means cleartext password used
    
    tDirReference		fPWSRef;
    tDirNodeReference	fPWSNodeRef;
	unsigned long		fPWSUserIDLength;
	char				*fPWSUserID;
	
    uid_t				fUID;
    uid_t				fEffectiveUID;
	
	// The Node Connection information, specific LDAP connection information, this is ALWAYS present
	sLDAPNodeStruct		*fLDAPNodeStruct;
	
	public:
		sLDAPContextData( void );
		~sLDAPContextData( void );
		
		sLDAPContextData( const sLDAPContextData& inContextData );
		
		void setCredentials( char *inUserName, void *inCredential, uInt32 inCredentialLen, char *inAuthType );
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

	#define NodeOpenMutexWaitButNotForCheckFailedThread() \
		NodeOpenMutexWaitWithFunctionName(__PRETTY_FUNCTION__, false)
	#define NodeOpenMutexWait() \
		NodeOpenMutexWaitWithFunctionName(__PRETTY_FUNCTION__, true)
	void			NodeOpenMutexWaitWithFunctionName	( const char* callingFunction, bool waitForCheckFailedThreadToComplete );

	#define NodeOpenMutexSignal() \
		NodeOpenMutexSignalWithFunctionName(__PRETTY_FUNCTION__)
	void			NodeOpenMutexSignalWithFunctionName	( const char* callingFunction );
	
	sInt32			SafeOpen		(	char			*inNodeName,
										sLDAPNodeStruct **inLDAPNodeStruct );
									//if already open then just get host and config index if host okay
									//else try rebind first
									//if not open then bind to get host and search for node
									//called from OpenDirNode
	sInt32			AuthOpen		(	char			*inLDAPUserName,
										char			*inKerberosId,
										void			*inLDAPCredentials,
										uInt32			 inLDAPCredentialsLen,
										char			*inLDAPAuthType,
										sLDAPNodeStruct **outLDAPNodeStruct );
									//there must be a fLDAPNodeMap entry for this ie. SafeOpen was already called since
									//this would come from a known Node Ref, we will decremented refCount of original
									//and do a new nodeStruct to replace it, if successful
	sInt32			RebindSession	(	sLDAPNodeStruct *pLDAPNodeStruct );
									//must already be open
									//check if already rebound during a continue for some other node ref
									//use rebind if continue not set
									//called from CLDAPv3Plugin::RebindLDAPSession
	sInt32			SimpleAuth		(	sLDAPNodeStruct *inLDAPNodeStruct,
										char			*inLDAPUserName,
										void			*inLDAPCredentials,
										uInt32			 inLDAPCredentialsLen,
										char			*inKerberosId );
									//use rebind to do the auth
									//called from DoClearTextAuth
									
	void			GetSchema		( sLDAPContextData *inContext );
	void			SessionMapMutexLock
									( void ) {fLDAPNodeOpenMutex.Wait();}
	void			SessionMapMutexUnlock
									( void ) {fLDAPNodeOpenMutex.Signal();}
	LDAP* 			LockSession		( sLDAPContextData *inContext );
	void			UnLockSession	( sLDAPContextData *inContext, bool inHasFailed = false );
	void			CheckIdles		( void );
	void			CheckFailed		( void );
	void			EnsureCheckFailedConnectionsThreadIsRunning	( void );
	void			SystemGoingToSleep	( void );
	void			SystemWillPowerOn		( void );
	void			NetTransition		( void );
	LDAP*			InitLDAPConnection
									(	sLDAPNodeStruct *inLDAPNodeStruct,
										sLDAPConfigData *inConfig,
										bool bInNeedWriteable = false );
	static struct addrinfo*
					ResolveHostName	( 	CFStringRef inServerNameRef,
										int inPortNumber );
	static LDAP*	EstablishConnection
									( 	sLDAPNodeStruct *inLDAPNodeStruct,
										sReplicaInfo *inList,
										int inPort,
										int inOpenTimeout,
										bool bInNeedWriteable = false );
	bool			isSASLMethodSupported ( CFStringRef inMethod );
	void			ForcedSafeClose (   const char *inNodeName );
	void			CredentialChange(	sLDAPNodeStruct *inLDAPNodeStruct, char *inUserDN );

protected:
	
	sInt32			BindProc			(	sLDAPNodeStruct	   *inLDAPNodeStruct,
											bool bForceBind = false,
											bool bCheckPasswordOnly = false,
											bool bNeedWriteable = false );
	sInt32			ParseLDAPNodeName	(	char	   *inNodeName,
											char	  **outLDAPName,
											int		   *outLDAPPort );
											//parse a string with a name and possibly a suffix of ':' followed by a port number
	sInt32			GetSchemaMessage	(	LDAP *inHost,
											int inSearchTO,
											LDAPMessage **outResultMsg );
	char**			GetNamingContexts	(	LDAP *inHost,
											int inSearchTO,
											uInt32 *outCount );
    void            MergeArraysRemovingDuplicates
                                        (   CFMutableArrayRef   cfPrimaryArray,
                                            CFArrayRef          cfArrayToAdd );
    void            BuildReplicaInfoLinkList
                                        (   sReplicaInfo **inOutList,
                                            CFArrayRef inRepList,
                                            CFArrayRef inWriteableList,
                                            int inPort );
	sInt32			GetReplicaListFromConfigRecord
										(	LDAP *inHost,
											int inSearchTO, 
											sLDAPNodeStruct *inLDAPNodeStruct,
											CFMutableArrayRef inOutRepList,
											CFMutableArrayRef inOutWriteableList );
	sInt32			GetReplicaListFromAltServer
										(	LDAP *inHost,
											int inSearchTO,
											CFMutableArrayRef inOutRepList );
	sInt32			GetReplicaListFromDNS
										(	sLDAPNodeStruct *inLDAPNodeStruct,
											CFMutableArrayRef inOutRepList );
	sInt32			RetrieveDefinedReplicas
										(	sLDAPNodeStruct *inLDAPNodeStruct, 
											CFMutableArrayRef &inOutRepList,
											CFMutableArrayRef &inOutWriteableList,
											int inPort,
											sReplicaInfo **inOutList );
	bool			IsTokenNotATag		(	char *inToken );
	void			RetrieveServerMappingsIfRequired
										(   sLDAPNodeStruct *inLDAPNodeStruct );
	static char *   LDAPWithBlockingSocket
										(   struct addrinfo *addrInfo, int seconds );
	static char *   ConvertToIPAddress  (   struct addrinfo *addrInfo );
	static bool		IsLocalAddress		(   struct addrinfo *addrInfo );
	static bool		ReachableAddress	(   struct addrinfo *addrInfo );
	
	void			CheckSASLMethods	(   sLDAPNodeStruct *inLDAPNodeStruct );
	bool			LocalServerIsLDAPReplica	( void );

private:
	CFMutableArrayRef   fSupportedSASLMethods;
	LDAPNodeMap			fLDAPNodeMap;
	DSMutexSemaphore	fLDAPNodeOpenMutex; //used for the ldap_bind as well as the LDAPNodeMap container
	bool				fInStartupState;	//Whether we are in our initial startup state

};

#endif	// __CLDAPNode_h__
