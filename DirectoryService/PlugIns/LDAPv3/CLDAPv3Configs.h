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
 * @header CLDAPv3Configs
 * Code to parse a XML file and place the contents into a table of structs.
 */

#ifndef _CLDAPV3CONFIGS_H
#define _CLDAPV3CONFIGS_H

#include <map>
#include <string>
#include <CoreFoundation/CoreFoundation.h>
#include <DirectoryServiceCore/DSMutexSemaphore.h>
#include <DirectoryServiceCore/DSEventSemaphore.h>
#include "CLDAPNodeConfig.h"

#include "CLDAPDefines.h"

using namespace std;

class CLDAPConnection;

typedef map<string, CLDAPNodeConfig *>	LDAPNodeConfigMap;
typedef LDAPNodeConfigMap::iterator		LDAPNodeConfigMapI;

class CLDAPv3Configs
{
	public:
							CLDAPv3Configs				( UInt32 inSignature );
		virtual			   ~CLDAPv3Configs				( void );
	
		// some node related functions
		void				RegisterAllNodes			( void );
		void				UnregisterAllNodes			( void );
		char				**GetDHCPBasedLDAPNodes		( UInt32 *outCount );
	
		// public functions for reading or updating the complete config, causes nodes to get re-initialized
		CFDataRef			CopyLiveXMLConfig			( void );
	
		SInt32				NewXMLConfig				( CFDataRef inXMLData );
		SInt32				AddToXMLConfig				( CFDataRef inXMLData );
	
		// this updates data in the config, possibly from a CLDAPNodeConfig
		void				UpdateSecurityPolicyForUUID	( const char *inNodeName, 
														  CFStringRef inUUID, 
														  CFDictionaryRef inConfiguredSecPolicy, 
														  CFDictionaryRef inSupportedSecLevel );
	
		void				UpdateReplicaListForUUID	( const char *inNodeName,
														  CFStringRef inUUID,
														  CFArrayRef inReplicaHostnames, 
														  CFArrayRef inWriteableHostnames );
	
		void				UpdateServerMappingsForUUID	( const char *inNodeName, CFStringRef inUUID, CFArrayRef inAttrTypeMapArray, 
														  CFArrayRef inRecordTypeMapArray );

		// returns a CLDAPConnection if a node for the provided node name exists
		CLDAPConnection		*CreateConnectionForNode	( const char *inNodeName );
		
		// writes server mappings to a server with the provided credentials
		SInt32				WriteServerMappings			( char *userName,
														  char *password,
														  CFDataRef inMappings );
	
		void				NetworkTransition			( void );
		void				PeriodicTask				( void );
	
		void				WaitForNodeRegistration		( void ) { fNodeRegistrationEvent.WaitForEvent(10); }

	private:
		LDAPNodeConfigMap	fNodeConfigMap;
		LDAPNodeConfigMap	fDynamicNodeConfigMap;
		DSMutexSemaphore	fNodeConfigMapMutex;
		DSMutexSemaphore	fXMLConfigLock;
		DSEventSemaphore	fNodeRegistrationEvent;
		UInt32				fPlugInSignature;
		CFArrayRef			fDHCPLDAPServers;
	
	private:
		// Reinitializes the ConfigMap with the current configuration
		SInt32					InitializeWithXML		( CFDataRef inXMLData );
	
		// these are purely file operations
		SInt32					ReadXMLConfig			( CFDataRef *outXMLData );
		SInt32					WriteXMLConfig			( CFDataRef inXMLData );
		bool					VerifyXML				( CFDataRef *inOutXMLData );
		bool					ConvertLDAPv2Config		( CFDataRef *inOutXMLData );
		bool					LocalServerIsReplica	( void );
	
		bool					CheckForDHCPPacket		( void );
		void					WatchForDHCPPacket		( void );
	
		CFMutableDictionaryRef	FindMatchingUUIDAndName	( CFDictionaryRef inConfig, const char *inNodeName, CFStringRef inUUID );
	
		static void				DHCPLDAPConfigNotification	( SCDynamicStoreRef cfStore, CFArrayRef changedKeys, void *inInfo );
		static void				DHCPPacketStateNotification	( SCDynamicStoreRef cfStore, CFArrayRef changedKeys, void *inInfo );
};

#endif
