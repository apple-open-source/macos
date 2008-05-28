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

#ifndef _CLDAPCONNECTIONMANAGER_H
#define _CLDAPCONNECTIONMANAGER_H

#include <unistd.h>
#include <map>			// STL map class
#include <string>		// STL string class
#include <list>
#include <DirectoryService/DirServicesTypes.h>
#include <DirectoryServiceCore/DSMutexSemaphore.h>
#include <DirectoryServiceCore/DSEventSemaphore.h>

#include "CLDAPConnection.h"

using namespace std;

typedef struct sLDAPContinueData {
    int					fLDAPMsgId;			//LDAP session call handle mainly used for searches
	tDirNodeReference	fNodeRef;			//node reference associated with this context data
	CLDAPConnection	   *fLDAPConnection;	//the LDAP connection for this continue data
    LDAPMessage		   *fResult;			//LDAP message last result used for continued searches
	LDAP			   *fRefLD;				//LDAP * of the original connection for LDAPMsgID, since msgids are invalid on previous LDs
											//  should not be used directly, only for reference
    UInt32				fRecNameIndex;		//index used to cycle through all requested Rec Names
    UInt32				fRecTypeIndex;		//index used to cycle through all requested Rec Types
    UInt32				fTotalRecCount;		//count of all retrieved records
    UInt32				fLimitRecSearch;	//client specified limit of number of records to return
    void				*fAuthHndl;
	void				*fAuthHandlerProc;
	char				*fAuthAuthorityData;
    tContextData		fPassPlugContinueData;
} sLDAPContinueData;

class CLDAPv3Configs;

// Context data structure
struct sLDAPContextData : public CObject<sLDAPContextData>
{
	int					fType;				//KW type of reference entry - not used yet
    UInt32				offset;				//offset into the data buffer
    UInt32				index;
	
    char				*fOpenRecordType;	//record type used to open a record
    char				*fOpenRecordName;	//record name used to open a record
    char				*fOpenRecordDN;		//record name used to open a record

	uid_t				fUID;
    uid_t				fEffectiveUID;

    tDirReference		fPWSRef;
    tDirNodeReference	fPWSNodeRef;
	UInt32				fPWSUserIDLength;
	char				*fPWSUserID;
	
	CLDAPConnection		*fLDAPConnection;
	
	public:
				sLDAPContextData	( CLDAPConnection *inConnection = NULL );
				sLDAPContextData	( const sLDAPContextData& inContextData );
	
	protected:
				~sLDAPContextData	( void );
};

typedef map<string, CLDAPConnection *>	LDAPConnectionMap;
typedef LDAPConnectionMap::iterator		LDAPConnectionMapI;

typedef list<CLDAPConnection *>					LDAPAuthConnectionList;
typedef LDAPAuthConnectionList::const_iterator	LDAPAuthConnectionListI;

// tracks connections
class CLDAPConnectionManager
{
	public:
		static int32_t			fCheckThreadActive;
		static double			fCheckFailedLastRun;
		static DSEventSemaphore	fCheckFailedEvent;

	public:
							CLDAPConnectionManager	( CLDAPv3Configs *inConfigObject );
		virtual				~CLDAPConnectionManager	( void );

		bool				IsSASLMethodSupported	( CFStringRef inMethod );
	
		sLDAPContextData	*CreateContextForNode	( const char *inNodeName );
	
		CLDAPConnection		*GetConnection			( const char *inNodeName );
		tDirStatus			AuthConnection			( CLDAPConnection **inConnection, const char *inLDAPUsername, const char *inRecordType, 
													  const char *inKerberosID, const char *inPassword );
		tDirStatus			AuthConnectionKerberos	( CLDAPConnection **inConnection, const char *inUsername, const char *inRecordType, 
													  krb5_creds *inCredsPtr, const char *inKerberosID );
		tDirStatus			VerifyCredentials		( CLDAPConnection *inConnection, const char *inLDAPUsername, const char *inRecordType,
													  const char *inKerberosID, const char *inPassword );
	
		void				NodeDeleted				( const char *inNodeName );

		void				PeriodicTask			( void );
		void				NetworkTransition		( void );
		void				SystemGoingToSleep		( void );
		void				SystemWillPowerOn		( void );

	private:
		LDAPConnectionMap		fLDAPConnectionMap;
		LDAPAuthConnectionList	fLDAPAuthConnectionList;
		DSMutexSemaphore		fLDAPConnectionMapMutex;
		CLDAPv3Configs			*fConfigObject;
		CFArrayRef				fSupportedSASLMethods;
	
	private:
		void			CheckFailed				( void );
		void			LaunchCheckFailedThread	( bool bForceCheck );
	
		static void		*CheckFailedServers		( void *inInfo );
};

#endif	// __CLDAPNode_h__
