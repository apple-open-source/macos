/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
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


#ifndef __CLDAPPlugInPrefs_H__
#define __CLDAPPlugInPrefs_H__

#import <CoreFoundation/CoreFoundation.h>

// keys
#define kDSLDAPPrefs_LDAPPlugInVersion			"LDAP PlugIn Version"
#define kDSLDAPPrefs_LDAPServerConfigs			"LDAP Server Configs"
#define kDSLDAPPrefs_ServicePrincipalTypes		"Service Principals to Create"

// values
#define kDSLDAPPrefs_CurrentVersion				"DSLDAPv3PlugIn Version 1.5"

typedef struct DSPrefs {
	CFStringRef version;
	CFArrayRef configs;
	char services[512];
	CFArrayRef serviceArray;
	CFArrayRef defaultServiceArray;
	char path[PATH_MAX];
} DSPrefs;

class CLDAPPlugInPrefs
{
	public:
										CLDAPPlugInPrefs();
		virtual							~CLDAPPlugInPrefs();
	
		void							GetPrefs( DSPrefs *inOutPrefs );
		void							SetPrefs(DSPrefs *inPrefs);
		
		CFDataRef						GetPrefsXML( void );
	
		int								Save( void );

	protected:
		int								Load( void );
		CFDictionaryRef					LoadXML( void );
		char *							GetTempFileName( void );
		
	private:
		DSPrefs mPrefs;
};


#endif
