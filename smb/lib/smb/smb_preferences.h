/*
 * Copyright (C) 2007-2010 Apple Inc. All rights reserved.
 * 
 */

#ifndef _SMB_SUBSYSTEM_PREFS_H_INCLUDED_
#define _SMB_SUBSYSTEM_PREFS_H_INCLUDED_

/* SMB SUBSYSTEM PREFERENCES
 *
 * This file contains all the information you should need to manipulate the SMB 
 * subsystem preferences.
 *
 * This file is available as
 *    /usr/local/include/smb_preferences.h
 *
 * To alter the preferences, use the SCPreferences API from the
 * SystemConfiguration framework. These changes are automatically synchronized
 * back to the SMB subsystem configuration file. The necessary system daemons are
 * automatically started or stopped depending on the configuration, so you
 * should not be manipulating these directly.
 *
 * If you need any features or extra preferences or documentation, please
 * file a Radar against the "SMB (New Bugs)" component.
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
    "/System/Library/CoreServices/SmbFileServer.bundle/Contents/Resources/DesktopDefaults.plist"

/* Path to the plist that contains default settings for Server systems. */
#define kSMBPreferencesServerDefaults \
    "/System/Library/CoreServices/SmbFileServer.bundle/Contents/Resources/ServerDefaults.plist"

/* Path to the tool that can be run to explicitly control preferences
 * synchronization. Preferences are synchronized automatically by the
 * com.apple.smb.server.preferences launchd job, but if you need more control,
 * here it is.
 */
#define kSMBPreferencesSyncTool "/usr/libexec/smb-sync-preferences"

/* Name of the launchd job that automatically synchronizes the SMB server
 * preferences. Note that synchronization takes place automatically, so you
 * don't need this in the normal case.
 */
#define kSMBPreferencesSyncJob "com.apple.smb.server.preferences"

/* Name of the plist file that defined the above launchd job. */
#define kSMBPreferencesSyncPlist \
    "/System/Library/LaunchDaemons/" kSMBPreferencesSyncJob ".plist"

/* All the actual preferences are named kSMBPrefXXXX. */

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

/* Name: Workgroup
 * Type: string
 * Legal values: ASCII (UTF8) characters, no spaces
 * Depending on the context, this is the workgroup name or the domain name.
 * Domain here does *not* refer to DNS.
 */
#define kSMBPrefWorkgroup "Workgroup"

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

/* Name: DOSCodePage
 * Type: string
 * The name of a DOS code page supported by the SMB server. "437" is the
 * default, but there's few more.
 */
#define kSMBPrefDOSCodePage "DOSCodePage"

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

/* Name: SMBSigningEnabled
 * Type: bool
 * Default SMB Signing policy whether signing is enabled on this server or not
 * Default to False for Desktop and Server.
 */
#define kSMBPrefSigningEnabled "SigningEnabled"

/* Name: SMBSigningRequired
 * Type: bool
 * Default SMB1 Signing policy whether signing is required on this server or not
 * Default to False for Desktop and Server.
 */
#define kSMBPrefSigningRequired "SigningRequired"

/* Name: SMBProtocolVersionMap
 * Type: number
 * Bit map indicating smb protocol versions the server should enable
 * 1 == 0001 => smb1 should be enabled
 * 2 == 0010 => smb2 should be enabled
 * 3 == 0011 => both smb1 and smb2 should be enabled 
 * If not specified the server will enable both smb1 and smb2 (default = 3)
 */
#define kSMBProtocolVersionMap "ProtocolVersionMap"

/* Name: SMBAllowDropboxShare
 * Type: bool
 * Whether to allow Mac clients to access dropbox (write-only) shares.
 * Defaults to True.
 */
#define kSMBPrefAllowDropboxShare "AllowDropboxShare"

/* Name: SMBSequesterDuration
 * Type: number
 * Duration for which disconnected sessions are saved for reconnect
 * Defaults to 1 day 1440 mins.
 */
#define kSMBSequesterDuration "SequesterDurationMins"

#endif /* _SMB_SERVER_PREFS_H_INCLUDED_ */

