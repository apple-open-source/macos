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

#ifndef _CLDAPDEFINES_H
#define _CLDAPDEFINES_H

#define kLDAPv3Str					"/LDAPv3/"

//XML label tags
#define	kXMLLDAPVersionKey			"LDAP PlugIn Version"
#define kXMLConfigArrayKey			"LDAP Server Configs"
#define kXMLDHCPConfigArrayKey		"LDAP DHCP Server Configs"
#define kXMLServerConfigKey			"LDAP Server Config"

#define kXMLEnableUseFlagKey		"Enable Use"
#define kXMLUserDefinedNameKey		"UI Name"
#define kXMLNodeName				"Node Name"

#define kXMLOpenCloseTimeoutSecsKey	"OpenClose Timeout in seconds"
#define kXMLIdleTimeoutMinsKey		"Idle Timeout in minutes"
#define kXMLDelayedRebindTrySecsKey	"Delay Rebind Try in seconds"
#define kXMLPortNumberKey			"Port Number"
#define kXMLSearchTimeoutSecsKey	"Search Timeout in seconds"
#define kXMLSecureUseFlagKey		"Secure Use"
#define kXMLServerKey				"Server"
#define kXMLServerAccountKey		"Server Account"
#define kXMLServerPasswordKey		"Server Password"
#define kXMLKerberosId				"Kerberos Id"
#define kXMLUseDNSReplicasFlagKey	"Use DNS replicas"

// New Directory Binding functionality --------------
//

// kXMLBoundDirectoryKey => indicates the computer is bound to this directory.
//         This prevents them from changing:  server account, password, 
//         secure use, and port number.  It also means the config cannot  
//         be deleted, without unbinding.
#define kXMLBoundDirectoryKey			"Bound Directory"

// macosxodpolicy config Record flags
//
// These new flags are for determining config-record settings..
#define kXMLDirectoryBindingKey			"Directory Binding"

// Dictionary of keys
#define kXMLConfiguredSecurityKey		"Configured Security Level"
#define kXMLSupportedSecurityKey		"Supported Security Level"
#define kXMLLocalSecurityKey			"Local Security Level"

// Keys for above Dictionaries
#define kXMLSecurityBindingRequired		"Binding Required"
#define kXMLSecurityNoClearTextAuths	"No ClearText Authentications"
#define kXMLSecurityManInTheMiddle		"Man In The Middle"
#define kXMLSecurityPacketSigning		"Packet Signing"
#define kXMLSecurityPacketEncryption	"Packet Encryption"

// Corresponding bit flags for quick checks..
#define kSecNoSecurity			0
#define kSecDisallowCleartext	(1<<0)
#define kSecManInMiddle			(1<<1)
#define kSecPacketSigning		(1<<2)
#define kSecPacketEncryption	(1<<3)

#define kSecSecurityMask		(kSecDisallowCleartext | kSecManInMiddle | kSecPacketSigning | kSecPacketEncryption)

//
// End New Directory Binding functionality ---------------

#define kXMLStdMapUseFlagKey				"Standard Map Use"
#define kXMLDefaultAttrTypeMapArrayKey		"Default Attribute Type Map"
#define kXMLDefaultRecordTypeMapArrayKey	"Default Record Type Map"
#define kXMLAttrTypeMapArrayKey				"Attribute Type Map"
#define kXMLRecordTypeMapArrayKey			"Record Type Map"
#define kXMLReplicaHostnameListArrayKey		"Replica Hostname List"
#define kXMLWriteableHostnameListArrayKey	"Writeable Hostname List"
#define kXMLNativeMapArrayKey				"Native Map"
#define kXMLStdNameKey						"Standard Name"
#define kXMLSearchBase						"Search Base"
#define kXMLOneLevelSearchScope				"One Level Search Scope"
#define kXMLObjectClasses					"Object Classes"
#define kXMLGroupObjectClasses				"Group Object Classes"
#define kXMLMakeDefLDAPFlagKey				"Default LDAP Search Path"
#define kXMLServerMappingsFlagKey			"Server Mappings"
#define kXMLIsSSLFlagKey					"SSL"
#define kXMLMapSearchBase					"Map Search Base"
#define kXMLReferralFlagKey					"LDAP Referrals"
#define kXMLTemplateSearchBaseSuffix		"Template Search Base Suffix"
#define kXMLConfigurationUUID				"Configuration UUID"
#define kXMLDeniedSASLMethods				"Denied SASL Methods"

#define kXMLAttrTypeMapDictKey				"Attribute Type Map"

const int32_t kConnectionUnsafe		= 0;
const int32_t kConnectionSafe		= 1;
const int32_t kConnectionUnknown	= 2;

#define kLDAPDefaultOpenCloseTimeoutInSeconds			15
#define kLDAPDefaultRebindTryTimeoutInSeconds			120
#define kLDAPDefaultSearchTimeoutInSeconds				30

#define kLDAPDefaultNetworkTimeoutInSeconds				10

#endif
