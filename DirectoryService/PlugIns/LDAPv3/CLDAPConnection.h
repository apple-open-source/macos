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

#ifndef _CLAPCONNECTION_H
#define _CLAPCONNECTION_H

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <DirectoryServiceCore/DSMutexSemaphore.h>
#include <DirectoryServiceCore/DSSemaphore.h>
#include <DirectoryServiceCore/DSEventSemaphore.h>
#include <Kerberos/krb5.h>
#include <libkern/OSAtomic.h>
#include <ldap.h>
#include <netinet/in.h>	// struct sockaddr_in

#include "CObject.h"
#include "CLDAPDefines.h"

using namespace std;

class CLDAPReplicaInfo;
class CLDAPNodeConfig;

struct sLDAPReachabilityList
{
	struct sLDAPReachabilityList	*next;
	SCNetworkReachabilityRef		reachabilityRef;
	char							dstIP[INET6_ADDRSTRLEN]; // buffer long enough to hold address
	char							srcIP[INET6_ADDRSTRLEN]; // buffer long enough to hold address
	int								socket;
};

// uncomment the line below for debugging the session locks specifically
//#define DEBUG_LDAPSESSION_LOCKS

class CLDAPConnection : public CObject<CLDAPConnection>
{
	public:
		bool			fbAuthenticated;			// the connection is authenticated
		bool			fbKerberosAuthenticated;	// authenticated using Kerberos
		int32_t			fWriteable;					// does this require writeable replica
		CLDAPNodeConfig	*fNodeConfig;				// used for setting options, getting new Replicas, etc.
		char			*fKerberosID;				// KerberosID of LDAP user if present
		char			*fLDAPUsername;				// LDAP user name
		char			*fLDAPPassword;				// LDAP user authentication credential default is char* password
		char			*fLDAPRecordType;			// LDAP record type
	
	public:
						CLDAPConnection			( CLDAPNodeConfig *inNodeConfig, CLDAPReplicaInfo *inCurrentReplica = NULL );
	
		CLDAPConnection	*CreateCopy				( void );
	
		void			SetNeedWriteable		( void ) { OSAtomicCompareAndSwap32Barrier( false, true, &fWriteable ); }

#if defined(DEBUG_LOCKS) || defined(DEBUG_LOCKS_HISTORY) || defined(DEBUG_LDAPSESSION_LOCKS)
		#define LockLDAPSession()		LockLDAPSessionDebug( __FILE__, __LINE__ )
		LDAP			*LockLDAPSessionDebug	( char *inFile, int inLine );
	
		#define UnlockLDAPSession(a,b)	UnlockLDAPSessionDebug( a, b, __FILE__, __LINE__ )
		void			UnlockLDAPSessionDebug	( LDAP * &inLDAP, bool inFailed, char *inFile, int inLine );
#else
		LDAP			*LockLDAPSession		( void );
		void			UnlockLDAPSession		( LDAP * &inLDAP, bool inFailed );
#endif
	
		UInt32			SessionSecurityLevel	( LDAP *inLDAP );

		void			PeriodicTask			( void );
		void			NetworkTransition		( void );
		
		tDirStatus		Authenticate			( const char *inLDAPUsername, const char *inRecordType, const char *inKerberosID, 
												  const char *inPassword );
		tDirStatus		AuthenticateKerberos	( const char *inUsername, const char *inRecordType, krb5_creds *inCredsPtr, 
												  const char *inKerberosID );
		void			UpdateCredentials		( const char *inPassword );
	
		void			CheckFailed				( void );
	
		char			*CopyReplicaIPAddress	( void );
		char			*CopyReplicaServicePrincipal	( void );
	
		int32_t			ConnectionStatus		( void ) { return fConnectionStatus; }
		void			SetConnectionStatus		( int32_t inStatus );

		void			CloseConnectionIfPossible	( void );

		static void		ReachabilityCallback	( SCNetworkReachabilityRef inTarget, SCNetworkConnectionFlags inFlags, void *inInfo );
		static void		LDAPFrameworkCallback	( LDAP *inLD, int inDesc, int inOpening, void *inParams );

	private:
		DSSemaphore				fMutex;					// mutex for accessing private information and locking the session
		CLDAPReplicaInfo		*fReplicaInUse;			// the replica we are using for this connection
		LDAP					*fHost;					// LDAP session handle

		// only used if fbAuthenticated is set
		bool					fbBadCredentials;		// means we've failed with bad authentications, don't try again
		char					*fKerberosCache;		// Used when fbKerberosAuthenticated is set
	
		// settings for idle timeouts
		int						fIdleCount;				// count of 30 sec periodic task calls for idle connection release
		
		// we use a special lock for reachability list because it is done out of band
		DSSemaphore				fReachabilityLock;
		sLDAPReachabilityList	*fReachabilityList;
		int32_t					fConnectionStatus;
#if defined(DEBUG_LOCKS) || defined(DEBUG_LOCKS_HISTORY) || defined(DEBUG_LDAPSESSION_LOCKS)
		int						fPrefixLen;
#endif

	private:
		virtual			~CLDAPConnection		( void );
		
		void			ReachabilityNotification( SCNetworkReachabilityRef inTarget, SCNetworkConnectionFlags inFlags );
		void			StartReachability		( int inSocket );
		void			StopReachability		( int inSocket );
};

#endif
