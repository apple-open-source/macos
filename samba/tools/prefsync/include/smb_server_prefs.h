/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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

#ifndef _SMB_SERVER_PREFS_H_INCLUDED_
#define _SMB_SERVER_PREFS_H_INCLUDED_

/* SMB SERVER PREFERENCES
 *
 * This file contains all the information you should need to manipulate the SMB
 * server preferences.
 *
 * This file is available as
 *	/usr/local/include/smb_server_prefs.h
 *
 * To alter the preferences, use the SCPreferences API from the
 * SystemConfiguration framework. These changes are automatically synchronized
 * back to the SMB server configuration file. The necessary system daemons are
 * automatically started or stopped depending on the configuration, so you
 * should not be manipulating these directly.
 *
 * If you need any features or extra preferences or documentation, please
 * file a Radar against the "SMB File Server" component.
 *
 * Defaults for all the values listed here are provided in two separate
 * plists, one for Desktop and one for Server. If you need to display a value
 * that is not specified in the SCPreferences, you can take it from the
 * appropriate defaults plist.
 */

/* Application ID that should be passed to SCPreferencesCreate() to access
 * the SMB preferences.
 */
#define kSMBPreferencesAppID "com.apple.smb.server.plist"

/* Path to the plist that contains default settings for Desktop systems. */
#define kSMBPreferencesDesktopDefaults \
    "/System/Library/CoreServices/SmbFileServer.bundle/Resources/DesktopDefaults.plist"

/* Path to the plist that contains default settings for Server systems. */
#define kSMBPreferencesServerDefaults \
    "/System/Library/CoreServices/SmbFileServer.bundle/Resources/ServerDefaults.plist"

/* Path to the tool that can be run to explicitly control preferences
 * synchronization. Preferences are synchronized automatically by the
 * com.apple.smb.server.preferences launchd job, but if you need more control,
 * here it is.
 */
#define kSMBPreferencesSyncTool "/usr/libexec/samba/synchronize-preferences"

/* Name of the launchd job that automatically synchronizes the SMB server
 * preferences. Note tht synchronization takes place automatically, so you
 * don't need this in the normal case.
 */
#define kSMBPreferencesSyncJob "com.apple.smb.server.preferences"

/* Name of the plist file that defined the above launchd job. */
#define kSMBPreferencesSyncPlist \
    "/System/Library/LaunchDaemons/" kSMBPreferencesSyncJob ".plist"

/* All the actual preferences are named kSMBPrefXXXX. */

/* Name: ServerRole
 * Type: string 
 * Legal values: Standalone, ActiveDirectoryMember, DomainMember,
 *              PrimaryDomainController, BackupDomainController
 * This option defines the role this server is playing in the network.
 * Standalone - server in an unmanaged network
 * ActiveDirectoryMember - member of an AD domain
 * DomainMember - member of an NT4 domain
 * PrimaryDomainController - PDC in an NT4 domain
 * BackupDomainController - BDC in an NT4 domain
 */
#define kSMBPrefServerRole "ServerRole"

#define kSMBPrefServerRoleADS           "ActiveDirectoryMember"
#define kSMBPrefServerRolePDC           "PrimaryDomainController"
#define kSMBPrefServerRoleBDC           "BackupDomainController"
#define kSMBPrefServerRoleDomain        "DomainMember"
#define kSMBPrefServerRoleStandalone    "Standalone"

/* Name: NetBIOSName
 * Type: string
 * Legal values: Up to 15 ASCII characters.
 */
#define kSMBPrefNetBIOSName "NetBIOSName"

/* Name: NetBIOSNodeType
 * Type: N/A
 * This key is provided because the SystemConfiguration framework defines it.
 * It currently has no effect on the SMB configuration. If you can think of a
 * valid use for it, open a Radar.
 */
#define kSMBPrefNetBIOSNodeType "NetBIOSNodeType"

#define kSMBPrefNetBIOSNodeBroadcast    "Broadcast"
#define kSMBPrefNetBIOSNodePeer         "Peer"
#define kSMBPrefNetBIOSNodeMixed        "Mixed"
#define kSMBPrefNetBIOSNodeHybrid       "Hybrid"

/* Name: NetBIOSScope
 * Type: string
 * NetBIOS scopes are rather baroque and if you understand why you want to set
 * this, you can certainly figure out what the legal syntax is :)
 */
#define kSMBPrefNetBIOSScope "NetBIOSScope"

/* Name: Workgroup
 * Type: string
 * Legal values: ASCII (UTF8) characters, no spaces
 * Depending on the context, this is the workgroup name or the domain name.
 * Domain here does *not* refer to DNS.
 */
#define kSMBPrefWorkgroup "Workgroup"

/* Name: KerberosRealm
 * Type: string
 * The name of the managed Kerberos realm.
 */
#define kSMBPrefKerberosRealm "KerberosRealm"

/* Name: LocalKerberosRealm
 * Type: string
 * The name of the local Kerberos realm. This will generally be of the form
 * LKDC:SHA1.<big long hex string>
 */
#define kSMBPrefLocalKerberosRealm "LocalKerberosRealm"

/* Name: AllowKerberosAuth
 * Type: bool
 * Whether Kerberos authentication should be allowed. Currently, this is not
 * implemented (Kerberos is always allowed if configured). This preference is
 * provided for consistency. If you need it implemented please open a Radar.
 */
#define kSMBPrefAllowKerberosAuth "AllowKerberosAuth"

/* Name: AllowNTLM2Auth
 * Type: bool
 * Whether NTLMv2 authentication should be allowed. Currently, this is not
 * implemented (NTLMv2 is always allowed). If you need it implemented please
 * open a Radar.
 */
#define kSMBPrefAllowNTLM2Auth "AllowNTLM2Auth"

/* Name: AllowNTLMAuth
 * Type: bool
 * Whether NTLMv1 authentication should be allowed. Default is true.
 */
#define kSMBPrefAllowNTLMAuth "AllowNTLMAuth"

/* Name: AllowLanManAuth
 * Type: bool
 * Whether (insecure) LanManager authentication should be allowed. Default is
 * false.
 */
#define kSMBPrefAllowLanManAuth "AllowLanManAuth"

/* Name: ServerDescription
 * Type: string
 * Human-readable decription of the server.
 */
#define kSMBPrefServerDescription "ServerDescription"

/* Name: AllowGuestAccess
 * Type: bool
 * Default guest access policy. This can be overridden on a per-share basis.
 * Default to true for Desktop, False for Server.
 */
 /* XXX kSMBAllowGuestAccess is the wrong naming convention, but leave it for
  * now to maintain source compatibility with dependent projects.
  */
#define kSMBAllowGuestAccess "AllowGuestAccess"
#define kSMBPrefAllowGuestAccess "AllowGuestAccess"

/* Name: MaxClients
 * Type: number
 * The implementation of this leaves something to be desired, but the
 * intention is for this to be a limit on the number of client sessions.
 * Default is 10 for Desktop, no limit for Server. Setting a limit of 0
 * currently means no limit, but this behaviour is not guaranteed - better to
 * remove the key and let it default.
 */
#define kSMBPrefMaxClients "MaxClients"

/* Name: LoggingLevel
 * Type: number
 * Legal values: any positive integer, higher numbers mean more logging
 * For all practical purposes, level 10 means "log everything". Be warned that
 * high logging levels kill performance.
 */
#define kSMBPrefLoggingLevel "LoggingLevel"

/* Name: DOSCodePage
 * Type: string
 * The name of a DOS code page supported by the SMB server. "437" is the
 * default, but there's few more.
 */
#define kSMBPrefDOSCodePage "DOSCodePage"

/* Name: MasterBrowser
 * Type: string
 * Legal values: domain, local, none
 * Whether this server should act as a NetBIOS master browser. If set to
 * "local" we will act as a LMB, if set to "domain" we will act as a DMB.
 * If set to "none", we will never become any sort of master browser.
 *
 * If this is not set, we will disable becoming a master browser unless we are
 * in the PDC or BDC role. We will still participate in local master browser
 * elections if some other parameter has enabled NetBIOS.
 */
#define kSMBPrefMasterBrowser "MasterBrowser"

#define kSMBPrefMasterBrowserLocal  "local"
#define kSMBPrefMasterBrowserDomain "domain"
#define kSMBPrefMasterBrowserNone   "none"

/* Name: RegisterWINSName
 * Type: bool
 * Whether the NetBIOS name of this server should be registered with a WINS
 * server. You also need to set at least one address in WINSServerAddressList.
 */
#define kSMBPrefRegisterWINSName "RegisterWINSName"

/* Name: WINSServerAddressList
 * Type: array of strings
 * A list of WINS servers the NetBIOS name should be registered with. This is a
 * list of *IPv4* addresses in dotted-quad notation. No IPv6, no hostnames, no
 * NetBIOS names.
 */
#define kSMBPrefWINSServerAddressList "WINSServerAddressList"

/* Name: PasswordServer
 * Type: string
 * The IPv4 address of a server to use for pass-through authentication. This is
 * a server that you trust enough to authenticate users on your behalf.
 */
#define kSMBPrefPasswordServer "PasswordServer"

/* Name: EnabledServices
 * Type: array of strings
 * Legal values: "disk", "print", "wins"
 * The default is to enable no services. "disk" means to enable file sharing.
 * "print" means to enable printer sharing. "wins" means to enable the WINS
 * server. Any combination of these values is OK.
 */
#define kSMBPrefEnabledServices "EnabledServices"

#define kSMBPrefEnabledServicesDisk     "disk"
#define kSMBPrefEnabledServicesPrint    "print"
#define kSMBPrefEnabledServicesWins     "wins"

/* Name: SuspendServices
 * Type: bool
 * Whether to unconditionally leave all SMB services after configuration. This
 * is a big red button that can be used to turn of all SMB services without
 * otherwise disturbing the configuration.
 */
#define kSMBPrefSuspendServices "SuspendServices"

/* Name: VirtualHomesShares
 * Type: bool
 * Whether to automatically give authenticated users access to their home
 * directory.
 */
#define kSMBPrefVirtualHomeShares "VirtualHomeShares"

/* Name: VirtualAdminShares
 * Type: bool
 * Whether to automatically give authenticated admin users access to all local
 * volumes.
 */
#define kSMBPrefVirtualAdminShares "VirtualAdminShares"

#endif /* _SMB_SERVER_PREFS_H_INCLUDED_ */

