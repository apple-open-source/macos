/*
 * Copyright (c) 2010 - 2011 Apple Inc. All rights reserved.
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

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <netsmb/smb_lib.h>
#include <smbclient/smbclient.h>
#include <smbclient/smbclient_internal.h>
#include "rcfile.h"
#include "preference.h"
#include "smb_preferences.h"

#define defaultCodePage  437


/*
 * level values:
 * 0 - default/global
 * 1 - server
 * 2 - server:share
 */
static void readPreferenceSection(struct rcfile *rcfile, struct smb_prefs *prefs, 
								   const char *sname, int level)
{
	char	*p;
	int32_t	altflags;
	
	/* global only preferences */
	if (level == 0) {
		/* 
		 * Neither of these are defined in the man pages. We should add
		 * kernel log level, then in future remove debug level.
		 */
		rc_getint(rcfile, sname, "debug_level", &prefs->KernelLogLevel);
		rc_getint(rcfile, sname, "kloglevel", &prefs->KernelLogLevel);
	}
	
	/* server only preferences */
	if (level == 1) {
		rc_getstringptr(rcfile, sname, "addr", &p);
		if (p) {
			if (prefs->NetBIOSDNSName) {
				CFRelease(prefs->NetBIOSDNSName);
			}
			prefs->NetBIOSDNSName = CFStringCreateWithCString(kCFAllocatorSystemDefault, p, kCFStringEncodingUTF8);
		}
	}
	
	/* global or server preferences */
	if ((level == 0) || (level == 1)) {
		rc_getint(rcfile, sname, "nbtimeout", &prefs->NetBIOSResolverTimeout);
		/* Make sure they set it to something */
		if (prefs->NetBIOSResolverTimeout == 0) {
			prefs->NetBIOSResolverTimeout = DefaultNetBIOSResolverTimeout;
		}
		/* 
		 * We default to trying both ports, if this is not set then the URL
		 * is overriding the preference, ignore the preference file setting.
		 */
		if (prefs->tryBothPorts) {
			rc_getstringptr(rcfile, sname, "port445", &p);
			/* See if the configuration file wants us to use a specific port.  */
			if (p) {
				if (strcmp(p, "netbios_only") == 0) {
					prefs->tryBothPorts = FALSE;
					prefs->tcp_port = NBSS_TCP_PORT_139;
				}
				else if (strcmp(p, "no_netbios") == 0) {
					prefs->tryBothPorts = FALSE;
					prefs->tcp_port = SMB_TCP_PORT_445;
				}
			}
		}
		/* Really should be getting this from System Configuration */
		rc_getstringptr(rcfile, sname, "minauth", &p);
		if (p) {
			/*
			 * "minauth" was set in this section; override
			 * the current minimum authentication setting.
			 */
			if (strcmp(p, "kerberos") == 0) {
				/*
				 * Don't fall back to NTLMv2, NTLMv1, or
				 * a clear text password.
				 */
				prefs->minAuthAllowed = SMB_MINAUTH_KERBEROS;
			} else if (strcmp(p, "ntlmv2") == 0) {
				/*
				 * Don't fall back to NTLMv1 or a clear
				 * text password.
				 */
				prefs->minAuthAllowed = SMB_MINAUTH_NTLMV2;
			} else if (strcmp(p, "ntlm") == 0) {
				/*
				 * Don't send the LM response over the wire.
				 */
				prefs->minAuthAllowed = SMB_MINAUTH_NTLM;
			} else if (strcmp(p, "lm") == 0) {
				/*
				 * Fail if the server doesn't do encrypted
				 * passwords.
				 */
				prefs->minAuthAllowed = SMB_MINAUTH_LM;
			} else if (strcmp(p, "none") == 0) {
				/*
				 * Anything goes.
				 * (The following statement should be
				 * optimized away.)
				 */
				prefs->minAuthAllowed = SMB_MINAUTH;
			}
		}
	}
	
	/* global, server, user, or share preferences */
	
	/* Only get the value if it exist */
	if (rc_getbool(rcfile, sname, "compound_on", &altflags) == 0) {			
		if (altflags)
			prefs->altflags |= SMBFS_MNT_COMPOUND_ON;
		else
			prefs->altflags &= ~SMBFS_MNT_COMPOUND_ON;			
	}
	
	/* Only get the value if it exist */
	if (rc_getbool(rcfile, sname, "notify_off", &altflags) == 0) {			
		if (altflags)
			prefs->altflags |= SMBFS_MNT_NOTIFY_OFF;
		else
			prefs->altflags &= ~SMBFS_MNT_NOTIFY_OFF;			
	}
	
	/* Only get the value if it exist */
	if (rc_getbool(rcfile, sname, "streams", &altflags) == 0) {
		if (altflags)
			prefs->altflags |= SMBFS_MNT_STREAMS_ON;
		else
			prefs->altflags &= ~SMBFS_MNT_STREAMS_ON;			
	}
	
	/* 
	 * We are not adding this in the man pages, because we do not want to keep
	 * this as a configuration option. This is for debug purposes only and 
	 * should be removed once <rdar://problem/7236779> is complete. Force
	 * ACLs on if we have a network sid.
	 */
	if (rc_getbool(rcfile, sname, "debug_acl_on", &altflags) == 0) {
		if (altflags)
			prefs->altflags |= SMBFS_MNT_DEBUG_ACL_ON;
		else
			prefs->altflags &= ~SMBFS_MNT_DEBUG_ACL_ON;			
	}
	
	/* Only get the value if it exist */
	if ( rc_getbool(rcfile, sname, "soft", &altflags) == 0) {
		if (altflags)
			prefs->altflags |= SMBFS_MNT_SOFT;
		else
			prefs->altflags &= ~SMBFS_MNT_SOFT;
	}
}

static CFStringRef getLocalNetBIOSNameUsingHostName()
{
	CFMutableStringRef NetBIOSName = NULL;
	char buf[_POSIX_HOST_NAME_MAX+1], *cp;
	
	if (gethostname(buf, sizeof(buf)) != 0) {
		smb_log_info("%s: Couldn't obtain the Local NetBIOS Name using gethostname", 
					 ASL_LEVEL_DEBUG, __FUNCTION__);
		return NULL;
	}
	cp = strchr(buf, '.');
	if (cp)
		*cp = 0;
	buf[MIN(SMB_MAXNetBIOSNAMELEN, _POSIX_HOST_NAME_MAX)] = 0;
	NetBIOSName = CFStringCreateMutable(kCFAllocatorSystemDefault, 0);
	if (NetBIOSName) {
		CFStringAppendCString(NetBIOSName, buf, kCFStringEncodingUTF8);
		CFStringUppercase(NetBIOSName, CFLocaleGetSystem());
	}
	return NetBIOSName;
}


/*
 * Retrieve any SMB System Configuration Preference. This routine always succeeds,
 * on any failure we fill in the default values.
 */
static void getSCPreferences(struct smb_prefs *prefs)
{
	SCPreferencesRef scPrefs;
	CFMutableStringRef NetBIOSName = NULL;
	CFStringRef DOSCodePage;
	
	scPrefs = SCPreferencesCreate(kCFAllocatorDefault, CFSTR("SMB Client"), CFSTR(kSMBPreferencesAppID));
	if (!scPrefs) {
		smb_log_info("%s: Couldn't obtain system config preferences: %s", 
					 ASL_LEVEL_DEBUG, __FUNCTION__, strerror(errno));
		prefs->WINSAddresses = NULL;
		prefs->WinCodePage = CFStringConvertWindowsCodepageToEncoding(defaultCodePage);
		goto done;
	}
	NetBIOSName = (CFMutableStringRef)SCPreferencesGetValue(scPrefs, CFSTR(kSMBPrefNetBIOSName));
	if (NetBIOSName) {
		NetBIOSName = CFStringCreateMutableCopy(kCFAllocatorSystemDefault, 0, NetBIOSName);
	}
	if (NetBIOSName) {
		CFStringUppercase(NetBIOSName, CFLocaleGetSystem());
	}
	prefs->LocalNetBIOSName = NetBIOSName;
	
	prefs->WINSAddresses = SCPreferencesGetValue(scPrefs, CFSTR(kSMBPrefWINSServerAddressList));
	if (prefs->WINSAddresses) {
		CFRetain(prefs->WINSAddresses);
#ifdef SMB_DEBUG
		char wins[SMB_MAX_DNS_SRVNAMELEN+1];
		CFIndex ii, count = CFArrayGetCount(prefs->WINSAddresses);

		for (ii=0; ii < count; ii++) {
			CFStringRef winString = CFArrayGetValueAtIndex(prefs->WINSAddresses, ii);
			wins[0] = 0;
			CFStringGetCString(winString, wins, sizeof(wins), kCFStringEncodingUTF8);
			smb_log_info("WINS[%d] \"%s\" ", ASL_LEVEL_ERR, (int)ii, wins);
		}
#endif // SMB_DEBUG		
	}
	DOSCodePage = SCPreferencesGetValue(scPrefs, CFSTR(kSMBPrefDOSCodePage));
	if (DOSCodePage) {
		if ((CFStringHasPrefix(DOSCodePage, CFSTR("CP")) == FALSE) && (CFStringHasPrefix(DOSCodePage, CFSTR("cp")) == FALSE)) {
			CFMutableStringRef WinCodePageStr = CFStringCreateMutableCopy(kCFAllocatorSystemDefault, 0, CFSTR("cp"));
			if (WinCodePageStr) {
				CFStringAppend(WinCodePageStr, DOSCodePage);
				prefs->WinCodePage = CFStringConvertIANACharSetNameToEncoding(WinCodePageStr);
				CFRelease(WinCodePageStr);
				goto done;
			}
		} else {
			prefs->WinCodePage = CFStringConvertIANACharSetNameToEncoding(DOSCodePage);
			goto done;
		}
	}
	/* We have no other choice use the default code page */
	prefs->WinCodePage = CFStringConvertWindowsCodepageToEncoding(defaultCodePage);
done:
	if (scPrefs) {
		CFRelease(scPrefs);
	}
	if (prefs->LocalNetBIOSName == NULL) {
		/* If all else fail try using the host name */
		prefs->LocalNetBIOSName = getLocalNetBIOSNameUsingHostName();
	}
	/* Test to make sure we have a code page we can use. */
	if (CFStringIsEncodingAvailable(prefs->WinCodePage) == FALSE) {
		prefs->WinCodePage = CFStringConvertWindowsCodepageToEncoding(defaultCodePage);
	}

}

void getDefaultPreferences(struct smb_prefs *prefs)
{
	memset(prefs, 0, sizeof(*prefs));
	prefs->tryBothPorts = TRUE;
	prefs->tcp_port = SMB_TCP_PORT_445;
	prefs->altflags = SMBFS_MNT_STREAMS_ON | SMBFS_MNT_COMPOUND_ON;
	prefs->minAuthAllowed = SMB_MINAUTH_NTLM;
	prefs->NetBIOSResolverTimeout = DefaultNetBIOSResolverTimeout;
	/* Now get any values stored in the System Configuration */
	getSCPreferences(prefs);
}

void releasePreferenceInfo(struct smb_prefs *prefs)
{
	if (prefs->LocalNetBIOSName) {
		CFRelease(prefs->LocalNetBIOSName);
		prefs->LocalNetBIOSName = NULL;
	}
	if (prefs->WINSAddresses) {
		CFRelease(prefs->WINSAddresses);
		prefs->WINSAddresses = NULL;
	}
	if (prefs->NetBIOSDNSName) {
		CFRelease(prefs->NetBIOSDNSName);
		prefs->NetBIOSDNSName = NULL;
	}
}

void setWINSAddress(struct smb_prefs *prefs, const char *winsAddress, int count)
{
	int ii;
	CFMutableArrayRef winsArray = CFArrayCreateMutable( kCFAllocatorSystemDefault, 0, 
													   &kCFTypeArrayCallBacks );
	if (winsArray == NULL) {
		return;
	}
	for (ii=0; ii < count; ii++) {
		CFStringRef winsName = CFStringCreateWithCString(kCFAllocatorSystemDefault, winsAddress, kCFStringEncodingUTF8);
		if (winsName) {
			CFArrayAppendValue(winsArray, winsName);
			CFRelease(winsName);
		}
		winsAddress += strlen(winsAddress) + 1;
	}
	if (CFArrayGetCount(winsArray) == 0) {
		CFRelease(winsArray);
	}
	if (prefs->WINSAddresses) {
		CFRelease(prefs->WINSAddresses);
	}
	prefs->WINSAddresses = winsArray;
}

void readPreferences(struct smb_prefs *prefs, char *serverName, char *shareName, 
					 int noUserPrefs, int resetPrefs)
{
	struct rcfile *rcfile;
	char sname[SMB_MAX_DNS_SRVNAMELEN + SMB_MAXSHARENAMELEN + 4];
	
	/* Set the default values */
	if (resetPrefs) {
		getDefaultPreferences(prefs);
	}

	/* Now read the nsmb.conf file preference */
	rcfile = smb_open_rcfile(noUserPrefs);
	if (rcfile == NULL) {
		return;
	}
	/* Read the global preference section */
	readPreferenceSection(rcfile, prefs, "default", 0);
	
	/* Need a server name, before we can read any of the [server] sections. */
	if (serverName) {
		readPreferenceSection(rcfile, prefs, serverName, 1);
		/* Need a server and share name, before we can read any of the [server:share] sections. */
		if (shareName) {
			snprintf(sname, sizeof(sname), "%s:%s", serverName, shareName);
			readPreferenceSection(rcfile, prefs, sname, 2);
		}
	}
	/* Done with it close the preference file */
	rc_close(rcfile);
}


CFStringEncoding getPrefsCodePage( void )
{
	struct smb_prefs prefs;
	
	getDefaultPreferences(&prefs);
	releasePreferenceInfo(&prefs);
	return prefs.WinCodePage;
}
