/*
 * Copyright (c) 2007 - 2010 Apple Inc. All rights reserved.
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

#include <SMBClient/netbios.h>
#include "libtest.h"

#include <NetFS/NetFS.h>
#include <sys/stat.h>
#include <asl.h>
#include "smb_netfs.h"
#include <netsmb/smb_lib.h>
#include <netsmb/smb_lib.h>
#include <parse_url.h>
#include <netsmb/smb_conn.h>

#include <unistd.h>
#include <sys/stat.h>

#include <CoreFoundation/CoreFoundation.h>

#include <asl.h>
#include "smb_netfs.h"
#include <netsmb/smb_lib.h>
#include <netsmb/smb_lib.h>
#include <parse_url.h>
#include <netsmb/upi_mbuf.h>
#include <sys/mchain.h>
#include <netsmb/smb_lib.h>
#include <netsmb/rq.h>
#include <netsmb/smbio.h>
#include <netsmb/smb_converter.h>
#include "msdfs.h"
#include "libtest.h"
#include "smbclient.h"
#include "smbclient_netfs.h"
#include "smbclient_private.h"
#include "smbclient_internal.h"
#include "ntstatus.h"
#include "netshareenum.h"
#include "LsarLookup.h"
#include "SetNetworkAccountSID.h"
#include "remount.h"

int verbose = FALSE;
#define MAX_SID_PRINTBUFFER	256	/* Used to print out the sid in case of an error */

static
void print_ntsid(ntsid_t *sidptr, const char *printstr)
{
	char sidprintbuf[MAX_SID_PRINTBUFFER];
	char *s = sidprintbuf;
	int subs;
	uint64_t auth = 0;
	unsigned i;
	uint32_t *ip;
	size_t len;
	
	bzero(sidprintbuf, MAX_SID_PRINTBUFFER);
	for (i = 0; i < sizeof(sidptr->sid_authority); i++)
		auth = (auth << 8) | sidptr->sid_authority[i];
	s += snprintf(s, MAX_SID_PRINTBUFFER, "S-%u-%llu", sidptr->sid_kind, auth);
	
	subs = sidptr->sid_authcount;
	
	for (ip = sidptr->sid_authorities; subs--; ip++)  { 
		len = MAX_SID_PRINTBUFFER - (s - sidprintbuf);
		s += snprintf(s, len, "-%u", *ip); 
	}
	fprintf(stderr, "%s: sid = %s \n", printstr, sidprintbuf);
}

static int test_netbios_name_conversion(void)
{
    static const struct {
	CFStringRef	proposed;
	CFStringRef	expected;
    } test_names[] = {
	{ NULL , NULL },
	{ CFSTR(""), NULL },
	{ CFSTR("james"), CFSTR("JAMES") },
	{ CFSTR("colley-xp4"), CFSTR("COLLEY-XP4") },
	{ CFSTR("jåmes"), CFSTR("JAMES") },
	{ CFSTR("longnameshouldbetruncated"), CFSTR("LONGNAMESHOULDB") },
	{ CFSTR("iPadæøåýÐ만돌"), CFSTR("IPADAYMANDOL") }
    };

    unsigned ii;
    unsigned count = (unsigned)(sizeof(test_names) / sizeof(test_names[0]));


    for (ii = 0; ii < sizeof(test_names) / sizeof(test_names[0]); ++ii) {
	CFStringRef converted;

	converted = SMBCreateNetBIOSName(test_names[ii].proposed);

	if (converted == NULL && test_names[ii].expected == NULL) {
	    /* Expected this to fail ... good! */
	    --count;
	    continue;
	}

	if (!converted) {
	    char buf[256];

	    buf[0] = '\0';
	    CFStringGetCString(test_names[ii].proposed, buf, sizeof(buf), kCFStringEncodingUTF8);

	    fprintf(stderr, "failed to convert '%s'\n", buf);
	    continue;
	}

	// CFShow(converted);

	if (CFEqual(converted, test_names[ii].expected)) {
	    /* pass */
	    --count;
	} else {
	    char buf1[256];
	    char buf2[256];

	    buf1[0] = buf2[0] = '\0';
	    CFStringGetCString(test_names[ii].expected, buf1, sizeof(buf1), kCFStringEncodingUTF8);
	    CFStringGetCString(converted, buf2, sizeof(buf2), kCFStringEncodingUTF8);

	    fprintf(stderr, "expected '%s' but received '%s'\n", buf1, buf2);
	}

	CFRelease(converted);
    }

    return count;
}

static int test_guest_anonymous_only(const char *url, int guest)
{
	SMBHANDLE	serverConnection = NULL;
	uint64_t	options = (guest) ? kSMBOptionUseGuestOnlyAuth : kSMBOptionUseAnonymousOnlyAuth;
	uint32_t	status = 0;
	SMBServerPropertiesV1 properties;
	
	status = SMBOpenServerEx(url, &serverConnection, options);
	if (!NT_SUCCESS(status)) {
		if (verbose)
			fprintf(stdout, "SMBOpenServerEx failed %d guest = %d\n", errno, guest);
		return errno;
	}
	memset(&properties, 0, sizeof(properties));
	status = SMBGetServerProperties(serverConnection, &properties, kPropertiesVersion, sizeof(properties));
	SMBReleaseServer(serverConnection);
	if (!NT_SUCCESS(status)) {
		/* Should never happen */
		if (verbose)
			fprintf(stdout, "SMBGetServerProperties failed %d guest = %d\n", errno, guest);
		return errno;
	}
	if (guest && (properties.authType == kSMBAuthTypeGuest)) {
		return 0;
	} else if (!guest && (properties.authType == kSMBAuthTypeAnonymous)) {
		return 0;
	}
	return -1;
}

static int test_accountname_sid(const char *url)
{
	ntsid_t	*sid = NULL;
	SMBHANDLE	serverConnection = NULL;
	uint64_t	options = kSMBOptionAllowGuestAuth | kSMBOptionAllowAnonymousAuth;
	uint32_t	status = 0;
	SMBServerPropertiesV1 properties;
	char *AccountName = NULL, *DomainName = NULL;

	status = SMBOpenServerEx(url, &serverConnection, options);
	if (!NT_SUCCESS(status)) {
		if (verbose)
			fprintf(stdout, "SMBOpenServerEx failed %d\n", errno);
		return errno;
	}
	status = SMBGetServerProperties(serverConnection, &properties, kPropertiesVersion, sizeof(properties));
	if (!NT_SUCCESS(status)) {
		if (verbose)
			fprintf(stdout, "SMBGetServerProperties failed %d\n", errno);
		return errno;
	}

	status = GetNetworkAccountSID(properties.serverName, &AccountName, &DomainName, &sid);
	if (!NT_SUCCESS(status)) {
		if (verbose)
			fprintf(stdout, "GetNetworkAccountSID failed %d\n", errno);
		return errno;
	}
	if (serverConnection)
		SMBReleaseServer(serverConnection);
	if (verbose) {
		fprintf(stderr, "user = %s domain = %s\n", AccountName, DomainName);
		print_ntsid(sid, "Network Users ");
	}
	if (sid) {
		free(sid);
	}
	if (AccountName) {
		free(AccountName);
	}
	if (DomainName) {
		free(DomainName);
	}
	return 0;
}

static CFURLRef create_url_with_share(CFURLRef url, const char *share)
{
	int error = ENOMEM;
	CFMutableDictionaryRef urlParms = NULL;
	CFStringRef sharePoint = CFStringCreateWithCString(kCFAllocatorDefault, share, kCFStringEncodingUTF8);
	
	if (sharePoint == NULL) {
		url = NULL;
		goto done;
	}
	errno = SMBNetFsParseURL(url, (CFDictionaryRef *)&urlParms);
	if (errno) {
		url = NULL;
		goto done;
	}
	
	CFDictionarySetValue (urlParms, kNetFSPathKey, sharePoint);
	url = NULL;
	error = SMBNetFsCreateURL(urlParms, &url);
done:
	if (sharePoint) {
		CFRelease(sharePoint);
	}
	if (error) {
		errno = error;
	}
	return url;

}

static int netfs_test_mounts(SMBHANDLE inConnection, CFURLRef url)
{
	const char *mp1 = "/tmp/mp1";
	const char *mp2 = "/tmp/mp2";
	const char *mp3 = "/tmp/mp3";
	const char *share1 = "TestShare";
	const char *share2 = "ntfsshare";
	const char *share3 = "fat32share";
	CFStringRef mountPoint = NULL; 
	CFURLRef mount_url = NULL;
	CFMutableDictionaryRef mountOptions = NULL;
	CFDictionaryRef mountInfo = NULL;
	int error = 0;
	
	if ((mkdir(mp1, S_IRWXU | S_IRWXG) == -1) && (errno != EEXIST))
		return errno;
	if ((mkdir(mp2, S_IRWXU | S_IRWXG) == -1) && (errno != EEXIST))
		return errno;
	if ((mkdir(mp3, S_IRWXU | S_IRWXG) == -1) && (errno != EEXIST))
		return errno;
	
	mountOptions = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (!mountOptions) {
		goto WeAreDone;
	}
	CFDictionarySetValue (mountOptions, kNetFSForceNewSessionKey, kCFBooleanFalse);
	
	mount_url = create_url_with_share(url, share1);
	if (mount_url == NULL) {
		error = errno;
		goto WeAreDone;
	}
	
	mountPoint = CFStringCreateWithCString(kCFAllocatorDefault, mp1, kCFStringEncodingUTF8);
	if (mountPoint) {
		error = SMBNetFsMount(inConnection, mount_url, mountPoint, mountOptions, &mountInfo, setNetworkAccountSID, NULL);
		CFRelease(mountPoint);
	} else {
		error = ENOMEM;
	}

	if (error) {
		if (verbose) {
			fprintf(stderr, "SMBNetFsMount failed %s %s\n", mp1, share1);
		}
		goto WeAreDone;
	}
	if (mountInfo) {
		if (verbose)
			CFShow(mountInfo);
		CFRelease(mountInfo);
	}
	mountInfo = NULL;
	CFRelease(mount_url);
	mount_url = NULL;
	mount_url = create_url_with_share(url, share2);
	if (mount_url == NULL) {
		error = errno;
		goto WeAreDone;
	}
	
	mountPoint = CFStringCreateWithCString(kCFAllocatorDefault, mp2, kCFStringEncodingUTF8);
	if (mountPoint) {
		error = SMBNetFsMount(inConnection, mount_url, mountPoint, mountOptions, &mountInfo, setNetworkAccountSID, NULL);
		CFRelease(mountPoint);
	} else {
		error = ENOMEM;
	}

	if (error) {
		if (verbose) {
			fprintf(stderr, "SMBNetFsMount failed %s %s\n", mp2, share2);
		}
		goto WeAreDone;
	}
	if (mountInfo) {
		if (verbose)
			CFShow(mountInfo);
		CFRelease(mountInfo);
	}
	mountInfo = NULL;
	CFRelease(mount_url);
	mount_url = NULL;
	mount_url = create_url_with_share(url, share3);
	if (mount_url == NULL) {
		error = errno;
		goto WeAreDone;
	}
	mountPoint = CFStringCreateWithCString(kCFAllocatorDefault, mp3, kCFStringEncodingUTF8);
	if (mountPoint) {
		error = SMBNetFsMount(inConnection, mount_url, mountPoint, mountOptions, &mountInfo, setNetworkAccountSID, NULL);
		CFRelease(mountPoint);
	} else {
		error = ENOMEM;
	}
	if (error) {
		if (verbose) {
			fprintf(stderr, "SMBNetFsMount failed %s %s\n", mp3, share3);
		}
		goto WeAreDone;
	}
	if (mountInfo) {
		if (verbose)
			CFShow(mountInfo);
		CFRelease(mountInfo);
	}
WeAreDone:
	unmount(mp1, 0);
	unmount(mp2, 0);
	unmount(mp3, 0);
	if (mountOptions)
		CFRelease(mountOptions);
	if (mount_url)
		CFRelease(mount_url);
	return error;
}


static int do_mount(const char *mp, CFURLRef url, CFDictionaryRef openOptions, int mntflags)
{

	SMBHANDLE theConnection = NULL;
	CFStringRef mountPoint = NULL; 
	CFDictionaryRef mountInfo = NULL;
	int error;
	CFNumberRef numRef = NULL;
	CFMutableDictionaryRef mountOptions = NULL;
	
	if ((mkdir(mp, S_IRWXU | S_IRWXG) == -1) && (errno != EEXIST))
		return errno;

	error = SMBNetFsCreateSessionRef(&theConnection);
	if (error)
		return error;
	error = SMBNetFsOpenSession(url, theConnection, openOptions, NULL);
	if (error) {
		(void)SMBNetFsCloseSession(theConnection);
		return error;
	}
	
	mountOptions = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (!mountOptions) {
		(void)SMBNetFsCloseSession(theConnection);
		return error;
	}
	numRef = CFNumberCreate( nil, kCFNumberIntType, &mntflags);
	if (numRef) {
		CFDictionarySetValue( mountOptions, kNetFSMountFlagsKey, numRef);
		CFRelease(numRef);
	}
	
	mountPoint = CFStringCreateWithCString(kCFAllocatorDefault, mp, kCFStringEncodingUTF8);
	if (mountPoint) {
		error = SMBNetFsMount(theConnection, url, mountPoint, mountOptions, &mountInfo, NULL, NULL);
		CFRelease(mountPoint);
		if (mountInfo)
			CFRelease(mountInfo);
	} else {
		error = ENOMEM;
	}
	CFRelease(mountOptions);
	(void)SMBNetFsCloseSession(theConnection);
	return error;
}

static int test_mount_exist(CFStringRef urlString)
{
	int error;
	CFURLRef url = NULL;
	const char *mp1 = "/private/tmp/mp1";
	const char *mp2 = "/private/tmp/mp2";

	url = CFURLCreateWithString (NULL, urlString, NULL);
	if (!url) {
		error = ENOMEM;
		goto done;
	}
	/* Test one: mount no browse twice */
	error = do_mount(mp1, url, NULL, MNT_DONTBROWSE);
	if (error) {
		fprintf(stdout, "TEST 1 mntFlags = MNT_DONTBROWSE: Couldn't mount first volume: shouldn't happen %d\n", error);
		goto done;
	}
	error = do_mount(mp2, url, NULL, MNT_DONTBROWSE);
	if (unmount(mp1, 0) == -1) {
		fprintf(stdout, "TEST 1 unmount mp1 failed %d\n", errno);
	}
	if (error != EEXIST) {
		fprintf(stdout, "TEST 1, mntFlags = MNT_DONTBROWSE: Second volume return wrong error: %d\n", error);
		if ((error == 0) && (unmount(mp2, 0) == -1)) {
			fprintf(stdout, "TEST 1 unmount mp2 failed %d\n", errno);
		}
		goto done;
	}
	
	/* Test two: mount first no browse second browse */
	error = do_mount(mp1, url, NULL, MNT_DONTBROWSE);
	if (error) {
		fprintf(stdout, "TEST 2, mntFlags = MNT_DONTBROWSE: Couldn't mount first volume: shouldn't happen %d\n", error);
		goto done;
	}
	error = do_mount(mp2, url, NULL, 0);
	if (unmount(mp1, 0) == -1) {
		fprintf(stdout, "TEST 2 unmount mp1 failed %d\n", errno);
	}
	if (error != 0) {
		fprintf(stdout, "TEST 2, mntFlags = 0: Second volume return wrong error: %d\n", error);
		goto done;
	}
	if (unmount(mp2, 0) == -1) {
		fprintf(stdout, "TEST 2 unmount mp2 failed %d\n", errno);
	}
			
	/* Test three: mount first browse second no  browse */
	error = do_mount(mp1, url, NULL, 0);
	if (error) {
		fprintf(stdout, "TEST 2, mntFlags = 0: Couldn't mount first volume: shouldn't happen %d\n", error);
		goto done;
	}
	error = do_mount(mp2, url, NULL, MNT_DONTBROWSE);
	if (unmount(mp1, 0) == -1) {
		fprintf(stdout, "TEST 3 mp1 unmount failed %d\n", errno);
	}
	if (error != 0) {
		fprintf(stdout, "TEST 2, mntFlags = MNT_DONTBROWSE: Second volume return wrong error: %d\n", error);
		goto done;
	}
	if (unmount(mp2, 0) == -1) {
		fprintf(stdout, "TEST 3 unmount mp2 failed %d\n", errno);
	}
	
	/* Test four: mount first browse second browse */
	error = do_mount(mp1, url, NULL, 0);
	if (error) {
		fprintf(stdout, "TEST 4, mntFlags = 0: Couldn't mount first volume: shouldn't happen %d\n", error);
		goto done;
	}
	error = do_mount(mp2, url, NULL, 0);
	if (unmount(mp1, 0) == -1) {
		fprintf(stdout, "TEST 4 mp1 unmount failed %d\n", errno);
	}
	if (error != EEXIST) {
		fprintf(stdout, "TEST 4, mntFlags = 0: Second volume return wrong error: %d\n", error);
		if ((error == 0) && (unmount(mp2, 0) == -1)) {
			fprintf(stdout, "TEST 4 unmount mp2 failed %d\n", errno);
		}
		goto done;
	}

	/* Test five: mount first automount second no flags */
	error = do_mount(mp1, url, NULL, MNT_AUTOMOUNTED);
	if (error) {
		fprintf(stdout, "TEST 5, mntFlags = MNT_AUTOMOUNTED: Couldn't mount first volume: shouldn't happen %d\n", error);
		goto done;
	}
	error = do_mount(mp2, url, NULL, 0);
	if (unmount(mp1, 0) == -1) {
		fprintf(stdout, "TEST 5 unmount mp1 failed %d\n", errno);
	}
	if (error != 0) {
		fprintf(stdout, "TEST 5, mntFlags = 0: Second volume return wrong error: %d\n", error);
		goto done;
	}
	if (unmount(mp2, 0) == -1) {
		fprintf(stdout, "TEST 5 unmount mp2 failed %d\n", errno);
	}
	
	/* Test six: mount first no flags second automount */
	error = do_mount(mp1, url, NULL, 0);
	if (error) {
		fprintf(stdout, "TEST 6, mntFlags = 0: Couldn't mount first volume: shouldn't happen %d\n", error);
		goto done;
	}
	error = do_mount(mp2, url, NULL, MNT_AUTOMOUNTED);
	if (unmount(mp1, 0) == -1) {
		fprintf(stdout, "TEST 6 unmount mp1 failed %d\n", errno);
	}
	if (error != 0) {
		fprintf(stdout, "TEST 6, mntFlags = MNT_AUTOMOUNTED: Second volume return wrong error: %d\n", error);
		goto done;
	}
	if (unmount(mp2, 0) == -1) {
		fprintf(stdout, "TEST 6 unmount mp2 failed %d\n", errno);
	}
	
	
done:
	rmdir(mp1);
	rmdir(mp2);
	if (url) 
		CFRelease(url);
	return error;
}
static int do_user_test(CFStringRef urlString)
{
	CFURLRef	url;
	void		*ref;
	int error;
	CFMutableDictionaryRef OpenOptions = NULL;
	CFDictionaryRef ServerParams = NULL;
	
	error = smb_load_library();
	if (error)
		return error;
	url = CFURLCreateWithString (NULL, urlString, NULL);
	if (!url)
		return errno;
	OpenOptions = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (OpenOptions == NULL) {
		error = errno;
		CFRelease(url);
		return error;
	}
	/* Unit test always allows loopback */
	CFDictionarySetValue(OpenOptions, kNetFSAllowLoopbackKey, kCFBooleanTrue);
	
	ref = create_smb_ctx();
	if (ref == NULL)
		exit(-1);
	error = smb_get_server_info(ref, url, OpenOptions, &ServerParams);
	if ((error == 0) && ServerParams) {
		if (verbose)
			CFShow(ServerParams);
	}
	if (error == 0) {
		error = smb_open_session(ref, url, OpenOptions, NULL);
	}
	if (error == 0)
		error = mkdir("/Volumes/george/TestShare", S_IRWXU | S_IRWXG);
	if (error == 0)
		error = smb_mount(ref, CFSTR("/Volumes/george/TestShare"), NULL, NULL, NULL, NULL);
	
	if (ServerParams)
		CFRelease(ServerParams);
	if (url)
		CFRelease(url);
	if (OpenOptions)
		CFRelease(OpenOptions);
	smb_ctx_done(ref);
	return error;
}

static int mount_one_volume(CFStringRef urlString)
{
	CFURLRef url = NULL;
	void *ref = NULL;
	int error = 0;
	CFMutableDictionaryRef OpenOptions = NULL;
	
	
	if ((mkdir("/tmp/mp", S_IRWXU | S_IRWXG) == -1) && (errno != EEXIST))
		return errno;
	error = smb_load_library();
	if (error)
		return error;
	url = CFURLCreateWithString (NULL, urlString, NULL);
	if (url == NULL)
		return errno;
	OpenOptions = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (OpenOptions == NULL) {
		error = errno;
		CFRelease(url);
		return error;
	}
	/* Unit test always allows loopback */
	CFDictionarySetValue(OpenOptions, kNetFSAllowLoopbackKey, kCFBooleanTrue);
	
	ref = create_smb_ctx();
	if (ref == NULL) {
		CFRelease(OpenOptions);
		CFRelease(url);
		return -1;
	}
	error = smb_open_session(ref, url, OpenOptions, NULL);
	if(error == 0)
		error = smb_mount(ref, CFSTR("/tmp/mp"), NULL, NULL, NULL, NULL);
	
	if (url)
		CFRelease(url);
	if (OpenOptions)
		CFRelease(OpenOptions);

	smb_ctx_done(ref);
	unmount("/tmp/mp", 0);
	return error;
	
}

static int test_path(struct smb_ctx *ctx, const char *instring, const char *comparestring, uint32_t flags)
{
	char utf8str[1024];
	char ntwrkstr[1024];
	size_t ntwrk_len = 1024;
	size_t utf8_len = 1024;

	if (verbose)
		printf("\nTesting string  %s\n", instring);
	
	ntwrk_len = 1024;
	memset(ntwrkstr, 0, 1024);
	if (smb_localpath_to_ntwrkpath(ctx, instring, strlen(instring), ntwrkstr, &ntwrk_len, flags)) {
		printf("smb_localpath_to_ntwrkpath: %s failed %d\n", instring, errno);
		return -1;
	}
	if (verbose)
		smb_ctx_hexdump(__FUNCTION__, "network name  =", (u_char *)ntwrkstr, ntwrk_len);
	
	memset(utf8str, 0, 1024);
	if (smb_ntwrkpath_to_localpath(ctx, ntwrkstr, ntwrk_len, utf8str, &utf8_len, flags)) {
		printf("smb_ntwrkpath_to_localpath: %s failed %d\n", instring, errno);
		return -1;
	}
	if (strcmp(comparestring, utf8str) != 0) {
		printf("UTF8 string didn't match: %s != %s\n", instring, utf8str);
		return -1;
	}
	if (verbose)
		printf("utf8str = %s len = %ld\n", utf8str, utf8_len);
	return 0;
}

const char *teststr[MAX_NUMBER_TESTSTRING] = {TESTSTRING1, TESTSTRING2, TESTSTRING3, TESTSTRING4, TESTSTRING5, TESTSTRING6 };
const char *testslashstr[MAX_NUMBER_TESTSTRING] = {TESTSTRING1, TESTSTRING2, TESTSTRING2, TESTSTRING1, TESTSTRING7, TESTSTRING6 };

static int test_path_conversion(struct smb_ctx *ctx)
{
	int ii, failcnt = 0, passcnt = 0;
	int maxcnt = MAX_NUMBER_TESTSTRING;
	uint32_t flags = SMB_UTF_SFM_CONVERSIONS;
	
	for (ii = 0; ii < maxcnt; ii++) {
		if (test_path(ctx, teststr[ii], teststr[ii], flags) == -1)
			failcnt++;
		else 
			passcnt++;
	}
	
	flags |= SMB_FULLPATH_CONVERSIONS;
	for (ii = 0; ii < maxcnt; ii++) {
		if (test_path(ctx, teststr[ii], testslashstr[ii], flags) == -1)
			failcnt++;
		else 
			passcnt++;
	}
	if (verbose) {
		printf("Path Conversion Test: Total = %d Passed = %d Failed = %d\n", (maxcnt * 2), passcnt, failcnt);
	}
	return (failcnt) ? EIO : 0;
}

static int list_shares_once(const char *urlCStr)
{
	CFURLRef	url = NULL;
	SMBHANDLE	serverConnection = NULL;
	int error;
	CFMutableDictionaryRef OpenOptions = NULL;
	CFDictionaryRef shares = NULL;
	
	error = SMBNetFsCreateSessionRef(&serverConnection);
	if (error) {
		goto done;
	}
	url = CreateSMBURL(urlCStr);
	if (!url) {
		error = errno;
		goto done;
	}
	OpenOptions = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (OpenOptions == NULL) {
		error = errno;
		goto done;
	}
	error = SMBNetFsOpenSession(url, serverConnection, OpenOptions, NULL);
	if (error == 0)
		error = smb_netshareenum(serverConnection, &shares, FALSE);
	if (error == 0) {
		if (verbose)
			CFShow(shares);
	} else if (verbose)
		CFShow(CFSTR("smb_enumerate_shares returned and error"));
done:
	if (url)
		CFRelease(url);
	if (OpenOptions)
		CFRelease(OpenOptions);
	if (shares)
		CFRelease(shares);
	if (serverConnection) {
		SMBNetFsCloseSession(serverConnection);
	}
	return error;
}

static int test_name_conversion(CFStringRef Domain, CFStringRef Username, CFStringRef Password, 
								CFStringRef ServerName, CFStringRef Path, CFStringRef PortNumber)
{
	CFURLRef	url = NULL;
	void		*ref = NULL;
	int			error = 0;
	CFMutableDictionaryRef OpenOptions = NULL;
	CFStringRef	urlString;
	
	error = smb_load_library();
	if (error)
		return error;
	urlString = CreateURLCFString(Domain, Username, Password, ServerName, Path, PortNumber);

	if (!urlString) {
		error = errno;
		goto done;
	}
	
	url = CFURLCreateWithString (NULL, urlString, NULL);
	if (!url) {
		error = errno;
		goto done;
	}
	OpenOptions = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (OpenOptions == NULL) {
		error = errno;
		goto done;
	}	
	/* Unit test always allows loopback */
	CFDictionarySetValue(OpenOptions, kNetFSAllowLoopbackKey, kCFBooleanTrue);
	ref = create_smb_ctx();
	if (ref == NULL) {
		error = errno;
		goto done;
	}
	error = smb_open_session(ref, url, OpenOptions, NULL);
	if (error) {
		CFShow(CFSTR("smb_open_session returned and error"));
		goto done;
	}
	error = test_path_conversion((struct smb_ctx *)ref);

done:
	if (OpenOptions)
		CFRelease(OpenOptions);
	if (urlString)
		CFRelease(urlString);
	if (url)
		CFRelease(url);

	smb_ctx_done(ref);
	return error;
}

static int test_user_smb_calls(CFStringRef Domain, CFStringRef Username, CFStringRef Password, 
							   CFStringRef ServerName, CFStringRef Path, CFStringRef PortNumber)
{
	CFURLRef	url = NULL;
	void		*ref = NULL;
	int			error = 0;
	CFMutableDictionaryRef OpenOptions = NULL;
	CFStringRef	urlString;
	
	error = smb_load_library();
	if (error)
		return error;
	urlString = CreateURLCFString(Domain, Username, Password, ServerName, Path, PortNumber);
	if (!urlString) {
		error = errno;
		goto done;
	}
	url = CFURLCreateWithString (NULL, urlString, NULL);
	if (!url) {
		error = errno;
		goto done;
	}
	OpenOptions = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (OpenOptions == NULL) {
		error = errno;
		goto done;
	}	
	/* Unit test always allows loopback */
	CFDictionarySetValue(OpenOptions, kNetFSAllowLoopbackKey, kCFBooleanTrue);
	ref = create_smb_ctx();
	if (ref == NULL) {
		error = errno;
		goto done;
	}
	error = smb_open_session(ref, url, OpenOptions, NULL);
	if (error) {
		CFShow(CFSTR("smb_open_session returned and error"));
		goto done;
	}
	
	error = smb_share_connect(ref);
	if (error) {
		CFShow(CFSTR("smb_share_connect returned and error"));
		goto done;
	}
	/* TEST CHECK DIRECTORY */
	error =  smbio_check_directory(ref, "/UnitTest/Library/Preferences/ByHost", 0, NULL);
	if (error)
		printf("check directory lookup of 'ByHost' failed with error = %d\n", error);
	error =  smbio_check_directory(ref, "UnitTest/Library", 0, NULL);
	if (error)
		printf("check directory lookup of 'UnitTest/Library' failed with error = %d\n", error);
	error =  smbio_check_directory(ref, "/UnitTest/Library", 0, NULL);
	if (error)
		printf("check directory lookup of '/UnitTest/Library' failed with error = %d\n", error);
	error =  smbio_check_directory(ref, "/UnitTest/Downloadsx", 0, NULL);
	if (error != ENOENT) {
		printf("check directory lookup of '/UnitTest/Downloadsx' should failed with ENENT, goterror = %d?\n", error);
	}
	error =  smbio_check_directory(ref, "/UnitTest/myFile", 0, NULL);
	if (error != ENOTDIR) {
		printf("check directory lookup of 'myFile' should failed with ENENT, goterror = %d?\n", error);
	}
	/* END OF CHECK DIRECTORY TEST */
	/* TEST OPEN/CLOSE */
	{
		int fid; 
		error = smbio_open_pipe(ref, "UnitTest/myFile", &fid);
		if (error)
			printf("smbio_open_pipe of '/UnitTest/myFile' failed with error = %d\n", error);
		else {
			error = smbio_close_file(ref, fid);
			if (error)
				printf("smbio_close_file of '/UnitTest/myFile' failed with error = %d\n", error);
		}
	}
	/* END OF OPEN/CLOSE TEST */

done:
	smb_ctx_done(ref);
	if (urlString)
		CFRelease(urlString);
	if (url)
		CFRelease(url);
	if (OpenOptions)
		CFRelease(OpenOptions);
	return error;
}

static int test_dfs_smb_calls(CFStringRef Domain, CFStringRef Username, CFStringRef Password, 
							  CFStringRef ServerName, CFStringRef Path, CFStringRef PortNumber, 
							  const char *mountpt, const char * server,  int dfsVersionsReferralTest)
{
#ifndef SMB_DEBUG
#pragma unused(server)
#endif // SMB_DEBUG

	CFURLRef	url = NULL;
	void		*ref = NULL;
	int			error = 0;
	CFMutableDictionaryRef OpenOptions = NULL;
	CFStringRef	urlString;
	
	error = smb_load_library();
	if (error)
		return error;
	urlString = CreateURLCFString(Domain, Username, Password, ServerName, Path, PortNumber);
	if (urlString == NULL) {
		error = errno;
		goto done;
	}
	url = CFURLCreateWithString (NULL, urlString, NULL);
	if (url == NULL) {
		error = errno;
		goto done;
	}
	OpenOptions = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (OpenOptions == NULL) {
		error = errno;
		goto done;
	}	
	/* Unit test always allows loopback */
	CFDictionarySetValue(OpenOptions, kNetFSAllowLoopbackKey, kCFBooleanTrue);
	ref = create_smb_ctx();
	if (ref == NULL) {
		error = errno;
		goto done;
	}
	
	error = smb_open_session(ref, url, OpenOptions, NULL);
	if (error) {
		CFShow(CFSTR("smb_open_session returned and error"));
		goto done;
	}
	if (url)
		CFRelease(url);
#ifdef SMB_DEBUG
	{
		CFIndex urlrefct = CFGetRetainCount(((struct smb_ctx *)ref)->ct_url);
		if (urlrefct > 1)
			CFShow(CFSTR("testing reference counting for leaks."));
	}
#endif // SMB_DEBUG
	url = NULL;
	
	if (dfsVersionsReferralTest) {
#ifdef SMB_DEBUG
		smb_ctx_setshare(ref, "IPC$");
		error = smb_share_connect(ref);
		if (error) {
			CFShow(CFSTR("smb_share_connect returned and error"));
			goto done;
		}
		
		if (strcmp(server, "waldorf.apple.com") == 0) {			
			error = testGettingDfsReferralDict(ref, "/waldorf.apple.com/dfs_share");
			if (error) {
				CFShow(CFSTR("testGettingDfsReferralDict dfs_share returned and error"));
			}
			error = testGettingDfsReferralDict(ref, "/waldorf.apple.com/dfs_share/crossdomain");
			if (error) {
				CFShow(CFSTR("testGettingDfsReferralDict crossdomain returned and error"));
			}
			error = testGettingDfsReferralDict(ref, "/waldorf.apple.com/dfs_share/test");
			if (error) {
				CFShow(CFSTR("testGettingDfsReferralDict test returned and error"));
			}
		} else {						
			error = testGettingDfsReferralDict(ref, "/smb-win2003.apple.com/DfsRoot");
			if (error) {
				CFShow(CFSTR("testGettingDfsReferralDict DfsRoot returned and error"));
			}		
			error = testGettingDfsReferralDict(ref, "/smb-win2003.apple.com/DfsRoot/DfsLink1");
			if (error) {
				CFShow(CFSTR("testGettingDfsReferralDict DfsLink1 returned and error"));
			}		
			error = testGettingDfsReferralDict(ref, "/smb-win2003.apple.com/DfsRoot2/LinkToRoot");
			if (error) {
				CFShow(CFSTR("testGettingDfsReferralDict LinkToRoot returned and error"));
			}		
		}
		error = smb_share_disconnect(ref);
#endif // SMB_DEBUG
	} else {
		CFMutableDictionaryRef mountOptions = NULL;
		CFStringRef mountPt = CFStringCreateWithCString(NULL, mountpt, kCFStringEncodingUTF8);
		
		if ((mkdir(mountpt, S_IRWXU | S_IRWXG) == -1) && (errno != EEXIST))
			CFShow(CFSTR("make dir faled"));
		else {
			mountOptions = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
			error = smb_mount(ref, mountPt, mountOptions, NULL, NULL, NULL);
			if (error && verbose)
				fprintf(stderr, "Dfs Mounting %s failed error =  %d\n", mountpt, error);

		}
		if (mountPt)
			CFRelease(mountPt);
		if (mountOptions)
			CFRelease(mountOptions);
	}
	
done:
	smb_ctx_done(ref);
	if (urlString)
		CFRelease(urlString);
	if (url)
		CFRelease(url);
	if (OpenOptions)
		CFRelease(OpenOptions);
	return error;
}

/* 
* Test based on the netfs routines 
 */
static int do_netfs_test(CFStringRef urlString)
{
	CFURLRef url = NULL;
	CFMutableDictionaryRef openOptions = NULL;
	CFDictionaryRef serverParms = NULL;
	SMBHANDLE inConnection = NULL;
	CFDictionaryRef sharePoints = NULL;
	int error = 0;

	url = CFURLCreateWithString (NULL, urlString, NULL);
	if (!url)
		goto WeAreDone;
	openOptions = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (!openOptions)
		goto WeAreDone;
	
	error = SMBNetFsCreateSessionRef(&inConnection);
	if (error)
		goto WeAreDone;
	error = SMBNetFsGetServerInfo(url, inConnection, openOptions, &serverParms);
	if (error)
		goto WeAreDone;
	if (verbose) {
		CFShow(serverParms);
	}
	
	/* XXX Need work here, we should add the mech type here in the future */

	error = SMBNetFsOpenSession(url, inConnection, openOptions, NULL);
	if (error)
		goto WeAreDone;
	error = smb_netshareenum(inConnection, &sharePoints, FALSE);
	if (error)
		goto WeAreDone;
	if (verbose) {
		CFShow(sharePoints);
	}
	if (sharePoints == NULL) {
		fprintf(stderr, "NetFS No Shares returned? \n");
	}
	if (error == 0)
		error = netfs_test_mounts(inConnection, url);
	
WeAreDone:
	if (url)
		CFRelease(url);
	if (openOptions)
		CFRelease(openOptions);
	if (serverParms)
		CFRelease(serverParms);
	if (sharePoints)
		CFRelease(sharePoints);
	if (inConnection)
		error = SMBNetFsCloseSession(inConnection);
	return error;
}

/* Test converting ULR ot a Dictionary and back to a URL */ 
const char *urlToDictToUrlCstrings[MAX_URL_TO_DICT_TO_URL_TEST] = {
					URL_TO_DICT_TO_URL_TEST_STR1, 
					URL_TO_DICT_TO_URL_TEST_STR2, 
					URL_TO_DICT_TO_URL_TEST_STR3,
					URL_TO_DICT_TO_URL_TEST_STR4};

static int testUrlToDictToURL()
{
	CFURLRef startURL, endURL;
	CFDictionaryRef dict;
	int ii, error = 0;
	
	for (ii = 0; ii < MAX_URL_TO_DICT_TO_URL_TEST; ii++) {
		startURL = CreateSMBURL(urlToDictToUrlCstrings[ii]);
		if (!startURL) {
			error = errno;
			fprintf(stderr, "CreateSMBURL with %s failed, %d\n", urlToDictToUrlCstrings[ii], error);
			break;
		}
		error = smb_url_to_dictionary(startURL, &dict);
		if (error) {
			fprintf(stderr, "smb_url_to_dictionary with %s failed, %d\n", urlToDictToUrlCstrings[ii], error);
			CFRelease(startURL);
			break;
		}
		error = smb_dictionary_to_url(dict, &endURL);
		if (error) {
			fprintf(stderr, "smb_dictionary_to_url with %s failed, %d\n", urlToDictToUrlCstrings[ii],  error);
			CFRelease(dict);
			CFRelease(startURL);
			break;
		}
		/* We only want to compare the URL string currently, may want to add more later */
		if (CFStringCompare(CFURLGetString(startURL), CFURLGetString(endURL), kCFCompareCaseInsensitive) != kCFCompareEqualTo) {
			fprintf(stderr, "%s round tripped failed\n", urlToDictToUrlCstrings[ii]);
			if (verbose) {
				CFShow(dict);
				CFShow(startURL);
				CFShow(endURL);
			}
		}
		if (verbose) {
			fprintf(stderr, "%s round tripped successfully\n", urlToDictToUrlCstrings[ii]);
		}
		CFRelease(dict);
		CFRelease(startURL);
		CFRelease(endURL);
	}
	return error;
}

static int testSMBOpenServerWithMountPoint()
{
	SMBHANDLE	mpConnection = NULL;
	SMBHANDLE	shareConnection = NULL;
	int			error = 0;
	uint32_t	status = 0;
	
	/* First mount a volume */
	if ((mkdir("/tmp/mp", S_IRWXU | S_IRWXG) == -1) && (errno != EEXIST))
		return errno;
	
	status = SMBOpenServerEx("smb://local1:local@smb-win2003/EmptyShare", 
							 &mpConnection, kSMBOptionNoPrompt | kSMBOptionSessionOnly);
	
	if (!NT_SUCCESS(status)) {
		if (verbose)
			fprintf(stdout, "SMBOpenServerEx failed %d\n", errno);
		return errno;
	}
	status = SMBMountShareEx(mpConnection, NULL, "/tmp/mp", 0, 0, 0, 0, NULL, NULL);
	if (!NT_SUCCESS(status)) {
		if (verbose)
			fprintf(stdout, "smb_mount failed %d\n", errno);
		return errno;
	}
	/* Now that we have a mounted volume we run the real test */
	status =  SMBOpenServerWithMountPoint("/tmp/mp", "IPC$", &shareConnection, 0);

	if (!NT_SUCCESS(status)) {
		if (verbose)
			fprintf(stdout, "SMBOpenServerWithMountPoint failed %d\n", errno);
		error = errno;
	} else {
		CFDictionaryRef shares = NULL;
		
		error = smb_netshareenum(shareConnection, &shares, FALSE);
		if (error == 0) {
			if (verbose)
				CFShow(shares);
		}
		if (shares)
			CFRelease(shares);
	}
		
	/* Now cleanup everything */
	if (shareConnection)
		SMBReleaseServer(shareConnection);
	if (mpConnection)
		SMBReleaseServer(mpConnection);
	unmount("/tmp/mp", 0);
	return error;
}


/*
 * Get a list of all mount volumes. The calling routine will need to free the memory.
 */
static struct statfs *smblib_getfsstat(int *fs_cnt)
{
	struct statfs *fs;
	int bufsize = 0;
	
	/* See what we need to allocate */
	*fs_cnt = getfsstat(NULL, bufsize, MNT_NOWAIT);
	if (*fs_cnt <=  0)
		return NULL;
	bufsize = *fs_cnt * (int)sizeof(*fs);
	fs = malloc(bufsize);
	if (fs == NULL)
		return NULL;
	
	*fs_cnt = getfsstat(fs, bufsize, MNT_NOWAIT);
	if (*fs_cnt < 0) {
		*fs_cnt = 0;
		free (fs);
		fs = NULL;
	}
	return fs;
}

static int test_remount()
{
	int found = 0;
	int error = 0;
	int fs_cnt = 0;
	int ii;
	struct statfs *fs = smblib_getfsstat(&fs_cnt);	/* Get the list of  mounted volumes */
	struct statfs *hold = fs;
	
	if (fs == NULL) {
		fprintf(stdout, "%s: smb_getfsstat failed\n", __FUNCTION__);
		return errno;
	}
	
	for (ii = 0; ii < fs_cnt; ii++, fs++) {		
		if (strcmp(fs->f_fstypename, "smbfs") != 0) {
			continue;
		}
		if (fs->f_flags & MNT_DONTBROWSE)  {
			found++;
			fprintf(stdout, "%s: %s:%s f_owner = %d\n", __FUNCTION__,
					fs->f_fstypename, fs->f_mntfromname, fs->f_owner);
#ifdef SMBCLIENT_TEST
			SMBRemountServer(&fs->f_fsid, sizeof(fs->f_fsid));
#else // SMBCLIENT_TEST
			error = smb_remount_with_fsid(fs->f_fsid);
			if (error) {
				break;
			}
#endif // SMBCLIENT_TEST
		}
	}
	if (hold)	/* Done with free it */
		free(hold);
	if (!found) {
		fprintf(stdout, "%s: no smbfs mount points found!\n", __FUNCTION__);
	}
	return error;
}

static int dispalyDfsReferrals(const char *url)
{
	CFMutableDictionaryRef dfsReferralDict;
	
	dfsReferralDict = CFDictionaryCreateMutable(kCFAllocatorSystemDefault, 0, 
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks);
	if (!dfsReferralDict) {
		return ENOMEM;
	}
	SMBGetDfsReferral(url,dfsReferralDict);
	CFShow(dfsReferralDict);
	CFRelease(dfsReferralDict);
	return 0;
}
/* 
 * Test low level smb library routines. This routine
 * will change depending on why routine is being tested.
 */
static int do_ctx_test(int type_of_test)
{
	int ErrorCnt = 0;
	int error = 0;
	
	switch (type_of_test) {
		case REMOUNT_UNIT_TEST:
			if (test_remount()) {
				ErrorCnt++;
			}
			break;
		case LIST_DFS_REFERRALS:
			if (dispalyDfsReferrals("smb://local1:local@smb-win2003/DrillDownRoot/TwoShare")) {
				ErrorCnt++;
			}
			if (dispalyDfsReferrals("smb://COS;dfswriter:%40pple1nc@cosdfs/DfsRoot/DfsLink1/Shares")) {
				ErrorCnt++;
			}
			break;
		case LIST_SHARE_CONNECT_TEST:
		{
			char *urlString;
#ifdef TEST_REQUIRES_SPECIAL_SERVER
			urlString = SMBCreateURLString(NULL, "local", "local",  "iPadæøåýÐ만돌._smb._tcp.local", NULL, -1);
			if (urlString) {
				error = list_shares_once(urlString);
				free(urlString);
				if (error) {
					fprintf(stdout, "iPadæøåýÐ만돌._smb._tcp.local: Share test failed %d\n", error);
					ErrorCnt++;
				}
			} else {
				fprintf(stdout, "SMBCreateURLString failed %d\n", errno);
			}
#endif // TEST_REQUIRES_SPECIAL_SERVER
			
			/* Try 2008 DNS name */
			urlString = SMBCreateURLString(NULL, "Administrator", "Ranger#1",  "netfs-win2k8.apple.com", NULL, -1);
			if (urlString) {
				error = list_shares_once(urlString);
				free(urlString);
				if (error) {
					fprintf(stdout, "netfs-win2k8.apple.com: Share test failed %d\n", error);
					ErrorCnt++;
				}
			} else {
				fprintf(stdout, "SMBCreateURLString failed %d\n", errno);
			}
			/* Try XP DNS name */
			error = list_shares_once("smb://local1:local@smb-xp4.apple.com");
			if (error) {
				fprintf(stdout, "smb-xp4.apple.com: Share test failed %d\n", error);
				ErrorCnt++;
			}
			/* Try Weird DNS name */
			error = list_shares_once("smb://local1:local@msfilsys.filsys.ads.apple.com");
			if (error) {
				fprintf(stdout, "msfilsys.filsys.ads.apple.com: Share test failed %d\n", error);
				ErrorCnt++;
			}
			/* Try Bonjour name */
			error = list_shares_once("smb://smbtest:smbtest@homedepot._smb._tcp.local");
			if (error) {
				fprintf(stdout, "homedepot._smb._tcp.local: Share test failed %d\n", error);
				ErrorCnt++;
			}
			/* Try DNS non fully qualified name */
			error = list_shares_once("smb://local1:local@smb-win2003");
			if (error) {
				fprintf(stdout, "smb-win2003: Share test failed %d\n", error);
				ErrorCnt++;
			}
			/* Try DNS  fully qualified name */
			error = list_shares_once("smb://local1:local@smb-win2003.apple.com");
			if (error) {
				fprintf(stdout, "smb-win2003.apple.com: Share test failed %d\n", error);
				ErrorCnt++;
			}
			/* Try NetBIOS name */
			error = list_shares_once("smb://local1:local@colley-xp4");
			if (error) {
				fprintf(stdout, "colley-xp4: Share test failed %d\n", error);
				ErrorCnt++;
			}
			/* Try NetBIOS name, require WINS */
			error = list_shares_once("smb://local1:local@msfilsys");
			if (error) {
				fprintf(stdout, "msfilsys: Share test failed %d (WINS Required)\n", error);
				ErrorCnt++;
			}
			/* Try IPV6 name, currently nothing thats up all the time */
			error = list_shares_once("smb://local1:local@[fe80::d9b6:f149:a17c:8307%25en1]");
			if (error) {
				fprintf(stdout, "IPv6: Share test failed %d\n", error);
				ErrorCnt++;
			}
			break;
		}
		case MOUNT_WIN2003_VOLUME_TEST:
			error = mount_one_volume(CFSTR("smb://local1:local@smb-win2003.apple.com/EmptyShare"));
			if (error) {
				fprintf(stderr, " mount_one_volume returned %d\n", error);
				ErrorCnt++;
			}
			break;
		case 2:
			error = do_user_test(CFSTR("smb://local1:local@smb-win2003.apple.com/TestShare"));
			if (error) {
				fprintf(stderr, " do_user_test returned %d\n", error);
				ErrorCnt++;
			}
			break;
		case URL_TO_DICT_TO_URL_TEST:
			error = testUrlToDictToURL();
			if (error) {
				fprintf(stderr, " testUrlToDictToURL returned %d\n", error);
				ErrorCnt++;
			}
			break;
		case 4:
			error = test_name_conversion(NULL, CFSTR("local1"), CFSTR("local"), CFSTR("smb-win2003.apple.com"), NULL, NULL);
			if (error) {
				fprintf(stderr, " test_name_conversion returned %d, if EIO then turn on verbose\n", error);
				ErrorCnt++;
			}
			break;
		case 5:
			error = test_user_smb_calls(NULL, CFSTR("local1"), CFSTR("local"), CFSTR("smb-win2003.apple.com"), NULL, NULL);
			if (error) {
				fprintf(stderr, " test_user_smb_calls returned %d\n", error);
				ErrorCnt++;
			}
			break;
		case DFS_MOUNT_TEST:
			error = test_dfs_smb_calls(CFSTR("COS"), CFSTR("dfswriter"), CFSTR("@pple1nc"), CFSTR("cosdfs.apple.com"), 
									   CFSTR("DfsRoot/NSSHARE"), NULL, "/tmp/NTSHARE", "cosdfs", 0);
			if (error) {
				fprintf(stderr, "DFS path DfsRoot returned an error %d\n", error);
				ErrorCnt++;
			}
			
			error = test_dfs_smb_calls(NULL, CFSTR("local1"), CFSTR("local"), CFSTR("smb-win2003.apple.com"), 
									   CFSTR("DfsRoot"), NULL, "/tmp/DfsRoot", "smb-win2003.apple.com", 0);
			if (error) {
				fprintf(stderr, "DFS path DfsRoot returned an error %d\n", error);
				ErrorCnt++;
			}
			error = test_dfs_smb_calls(NULL, CFSTR("local1"), CFSTR("local"), CFSTR("smb-win2003.apple.com"), 
									   CFSTR("DfsRoot/DfsLink1"), NULL, "/tmp/DfsLink1", "smb-win2003.apple.com", 0);
			if (error) {
				fprintf(stderr, "DFS path DfsRoot/DfsLink1 returned an error %d\n", error);
				ErrorCnt++;
			}
			error = test_dfs_smb_calls(NULL, CFSTR("local1"), CFSTR("local"), CFSTR("smb-win2003.apple.com"), 
									   CFSTR("DfsRoot2"), NULL, "/tmp/DfsRoot2", "smb-win2003.apple.com", 0);
			if (error) {
				fprintf(stderr, "DFS path DfsRoot2 returned an error %d\n", error);
				ErrorCnt++;
			}
			error = test_dfs_smb_calls(NULL, CFSTR("local1"), CFSTR("local"), CFSTR("smb-win2003.apple.com"), 
									   CFSTR("DfsRoot2/LinkToRoot"), NULL, "/tmp/LinkToRoot", "smb-win2003.apple.com", 0);
			if (error) {
				fprintf(stderr, "DFS path DfsRoot2/LinkToRoot returned an error %d\n", error);
				ErrorCnt++;
			}
			error = test_dfs_smb_calls(NULL, CFSTR("local1"), CFSTR("local"), CFSTR("smb-win2003.apple.com"), 
									   CFSTR("DfsRoot2/LinkToRoot/DfsLink1/George"), NULL, "/tmp/George", "smb-win2003.apple.com", 0);
			if (error) {
				fprintf(stderr, "DFS path DfsRoot2/LinkToRoot/DfsLink1/George returned an error %d\n", error);
				ErrorCnt++;
			}
			error = test_dfs_smb_calls(CFSTR("ets"), CFSTR("gcolley"), CFSTR("1finiteLoop"), CFSTR("waldorf.apple.com"), 
									   CFSTR("dfs_share"), NULL, "/tmp/dfs_share", "waldorf.apple.com", 0);
			if (error) {
				fprintf(stderr, "DFS path dfs_share  returned an error %d\n", error);
				ErrorCnt++;
			}
			error = test_dfs_smb_calls(NULL, CFSTR("gcolley"), CFSTR("1finiteLoop"), CFSTR("waldorf.apple.com"), 
									   CFSTR("dfs_share/crossdomain"), NULL, "/tmp/crossdomain", "waldorf.apple.com", 0);
			if (error != EAUTH) {
				fprintf(stderr, "DFS path dfs_share/crossdomain  returned the wrong error %d\n", error);
				ErrorCnt++;
			}
			error = test_dfs_smb_calls(CFSTR("ets"), CFSTR("gcolley"), CFSTR("1finiteLoop"), CFSTR("waldorf.apple.com"), 
									   CFSTR("dfs_share/crossdomain"), NULL, "/tmp/ets_crossdomain", "waldorf.apple.com", 0);
			if (error) {
				fprintf(stderr, "DFS path dfs_share/crossdomain returned an error %d\n", error);
				ErrorCnt++;
			}
			error = test_dfs_smb_calls(CFSTR("ets"), CFSTR("gcolley"), CFSTR("1finiteLoop"), CFSTR("waldorf.apple.com"), 
									   CFSTR("dfs_share/test"), NULL, "/tmp/test", "waldorf.apple.com", 0);
			if (error) {
				fprintf(stderr, "DFS path dfs_share/test  returned an error %d\n", error);
				ErrorCnt++;
			}
			unmount("/tmp/DfsRoot", 0);
			unmount("/tmp/DfsLink1", 0);
			unmount("/tmp/DfsRoot2", 0);
			unmount("/tmp/LinkToRoot", 0);
			unmount("/tmp/George", 0);
			unmount("/tmp/dfs_share", 0);
			unmount("/tmp/crossdomain", 0);
			unmount("/tmp/ets_crossdomain", 0);
			unmount("/tmp/test", 0);
			break;
		case DFS_LOOP_TEST:
			error = test_dfs_smb_calls(NULL, CFSTR("local1"), CFSTR("local"), CFSTR("smb-win2003.apple.com"), 
									   CFSTR("DfsRoot2/LoopLinkToLoopLink"), NULL, "/tmp/LoopTest", "smb-win2003.apple.com", 0);
			if (error != EMLINK) {
				fprintf(stderr, "LoopTest returned the wrong error %d\n", error);
				ErrorCnt++;
			}
			unmount("/tmp/LoopTest", 0);
			break;
		case URL_TO_DICTIONARY:
		{
			CFDictionaryRef dict;
			CFURLRef url;
			
			url = CFURLCreateWithString (NULL, CFSTR("smb://;homedepot"), NULL);
			error = smb_url_to_dictionary(url, &dict);
			if (error) {
				fprintf(stderr, "empty workgroup smb_url_to_dictionary returned %d\n", error);
				ErrorCnt++;
			} else if (dict) {
				if (verbose)
					CFShow(dict);
				CFRelease(dict);
			} 
			CFRelease(url);
			
			url = CFURLCreateWithString (NULL, CFSTR("smb://;@homedepot"), NULL);
			error = smb_url_to_dictionary(url, &dict);
			if (error) {
				fprintf(stderr, "empty workgroup and userame smb_url_to_dictionary returned %d\n", error);
				ErrorCnt++;
			} else if (dict) {
				if (verbose)
					CFShow(dict);
				CFRelease(dict);
			} 
			CFRelease(url);
			
			url = CFURLCreateWithString (NULL, CFSTR("smb://;:homedepot"), NULL);
			error = smb_url_to_dictionary(url, &dict);
			if (error) {
				fprintf(stderr, "empty workgroup and password smb_url_to_dictionary returned %d\n", error);
				ErrorCnt++;
			} else if (dict) {
				if (verbose)
					CFShow(dict);
				CFRelease(dict);
			} 
			CFRelease(url);
			
			url = CFURLCreateWithString (NULL, CFSTR("smb://;:@homedepot"), NULL);
			error = smb_url_to_dictionary(url, &dict);
			if (error) {
				fprintf(stderr, "anonymous smb_url_to_dictionary returned %d\n", error);
				ErrorCnt++;
			} else if (dict) {
				if (verbose)
					CFShow(dict);
				CFRelease(dict);
			} 
			CFRelease(url);
		}
			break;
		case FIND_VC_FROM_MP_TEST:
			error = testSMBOpenServerWithMountPoint();
			if (error) {
				fprintf(stderr, "FIND_VC_FROM_MP_TEST failed\n");
				ErrorCnt++;
			}
			break;
		case NETFS_TEST:
			error = do_netfs_test(CFSTR("smb://local1:local@smb-win2003.apple.com"));
			if (error) {
				fprintf(stderr, "NETFS_TEST failed\n");
				ErrorCnt++;
			}
			break;
		case GETACCOUNTNAME_AND_SID_TEST:
			error = test_accountname_sid("smb://local1:local@smb-win2003.apple.com");
			if (error) {
				fprintf(stderr, "test_accountname_sid failed for smb-win2003.apple.com\n");
				ErrorCnt++;
			}
			error = test_accountname_sid("smb://cindy:Ranger%231@msfilsys.filsys.ads.apple.com");
			if (error) {
				fprintf(stderr, "test_accountname_sid failed for msfilsys.filsys.ads.apple.com\n");
				ErrorCnt++;
			}
			error = test_accountname_sid("smb://cindy:Ranger%231@msfilsys");
			if (error) {
				fprintf(stderr, "test_accountname_sid failed for msfilsys (WINS required)\n");
				ErrorCnt++;
			}
			error = test_accountname_sid("smb://smbtest:smbtest@homedepot.apple.com");
			if (error) {
				fprintf(stderr, "test_accountname_sid failed for homedepot.apple.com\n");
				ErrorCnt++;
			}
			break;
		case FORCE_GUEST_ANON_TEST:
			error = test_guest_anonymous_only("smb://homedepot.apple.com", TRUE);
			if (error) {
				fprintf(stderr, "Guest Only failed for homedepot %d\n", error);
				ErrorCnt++;
			}
			error = test_guest_anonymous_only("smb://msfilsys.filsys.ads.apple.com", FALSE);
			if (error) {
				fprintf(stderr, "Anonymous Only failed for msfilsys\n");
				ErrorCnt++;
			}
			break;
		case MOUNT_EXIST_TEST:
			error = test_mount_exist(CFSTR("smb://local1:local@smb-win2003/TestShare"));
			if (error) {
				fprintf(stderr, "Mount exist test faile with smb-win2003/TestShare\n");
				ErrorCnt++;
			}
			break;
		case 16:
		{
		    error = test_netbios_name_conversion();
		    if (error) {
			    fprintf(stderr, "NetBIOS name conversion test failed\n");
			    ErrorCnt++;
		    }

		    break;
		}

		default:
			fprintf(stderr, " Unknown command %d\n", type_of_test);
			break;
			
	};
#ifdef TEST_MEMORY_LEAKS
	while (1) {
		;
	}
#endif // TEST_MEMORY_LEAKS
	return ErrorCnt;
}

/*
 * Need to rewrite this test code, we should make this our library unit test
 * code. 
 */
int main(int argc, char **argv)
{
	int opt;
	int ErrorCnt = 0;
	int type_of_test = RUN_ALL_TEST;
	int ii;
	
	while ((opt = getopt(argc, argv, "hvn:")) != EOF) {
		switch (opt) {
		    case 'v':
				verbose = TRUE;
				break;
			case 'n':
				type_of_test = (int)strtol(optarg, NULL, 0);
				fprintf(stderr, " type_of_test  %d \n", type_of_test);
				break;
		    case 'h':
		    default:
				fprintf(stderr, " Bad value\n");
				return 1;
				break;
		}
	}
	
	if (type_of_test != RUN_ALL_TEST) {
		ErrorCnt += do_ctx_test(type_of_test);
	} else  {
		for (ii = START_UNIT_TEST; ii <= END_UNIT_TEST; ii++)
			ErrorCnt += do_ctx_test(ii);
	}
	if (ErrorCnt) {
		fprintf(stderr, " Failed  %d test\n", ErrorCnt);
	} else {
		fprintf(stderr, " Passed all test\n");
	}
	return 0;
}
