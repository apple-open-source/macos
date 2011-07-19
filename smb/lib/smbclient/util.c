/*
 * Copyright (c) 2009 - 2010 Apple Inc. All rights reserved.
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

#include "ntstatus.h"
#include "smbclient.h"
#include "netbios.h"
#include "smbclient_internal.h"
#include "smbclient_private.h"

#include <netsmb/netbios.h>
#include <netsmb/smb_lib.h>
#include <netsmb/nb_lib.h>
#include <netsmb/smb_conn.h>
#include "charsets.h"
#include "parse_url.h"
#include "remount.h"
#include "msdfs.h"
#include <smbfs/smbfs.h>

void SMBLogInfo(const char *fmt, int log_level,...) 
{
	int save_errno = errno;
	va_list ap;
	aslmsg m = asl_new(ASL_TYPE_MSG);
	
	
	va_start(ap, log_level);
	asl_vlog(NULL, m, log_level, fmt, ap);
	va_end(ap);
	asl_free(m);        
	errno = save_errno; /* Never let this routine change errno */
}

/* Find all address associated with the NetBIOS name */
struct sockaddr_storage *
SMBResolveNetBIOSNameEx(const char *hostName, uint8_t nodeType, 
						 const char *winServer, uint32_t timeout,
						 struct sockaddr_storage *respAddr, int32_t *outCount)
{
	struct sockaddr_storage *outAddr = NULL, *listAddr;
	char *netbios_name = NULL;
	struct nb_ctx ctx;
	CFMutableArrayRef addressArray = NULL;
	CFMutableDataRef addressData;
	struct connectAddress *conn;
	CFIndex ii;
	int error = 0;
	struct smb_prefs prefs;
	
	bzero(&ctx, sizeof(struct nb_ctx));
	/* Read the preference files */
	readPreferences(&prefs, NULL, NULL, FALSE, TRUE);

	/* They gave us a wins server use it */
	if (winServer) {
		setWINSAddress(&prefs, winServer, 1);
	}
	/* They gave us a timeout value use it */
	if ((int32_t)timeout > 0) {
		prefs.NetBIOSResolverTimeout = timeout;
	}
	
	/*
	 * We uppercase and convert the server name given in the URL to Windows Code 
	 * Page.
	 */
	netbios_name = convert_utf8_to_wincs(hostName, prefs.WinCodePage, TRUE);
	if (netbios_name == NULL) {
		error = ENOMEM;
		goto done;
	}
	/* Only returns IPv4 address */
	error = nbns_resolvename(&ctx, &prefs, netbios_name, nodeType, &addressArray, 
							 SMB_TCP_PORT_445, TRUE, FALSE, NULL);
	if (error) {
		goto done;
	}
	
	if (respAddr) {
		memcpy(respAddr, &ctx.nb_sender, sizeof(ctx.nb_sender)); 
	}
	
	listAddr = outAddr = malloc(CFArrayGetCount(addressArray) * sizeof(struct sockaddr_storage));
	if (outAddr == NULL) {
		error = ENOMEM;
		goto done;
	}

	for (ii=0; ii < CFArrayGetCount(addressArray); ii++) {
		addressData = (CFMutableDataRef)CFArrayGetValueAtIndex(addressArray, ii);
		if (addressData) {
			conn = (struct connectAddress *)(void *)CFDataGetMutableBytePtr(addressData);
			if (conn) {
				*outCount += 1;
				*listAddr++ = conn->storage;
			}
		}
	}
	if (*outCount == 0) {
		free(outAddr);
		outAddr = NULL; /* Didn't really find any */
		error = EHOSTUNREACH;
	}
done:	
	if (addressArray)
		CFRelease(addressArray);

	if (netbios_name) {
		free(netbios_name);
	}
	releasePreferenceInfo(&prefs);
	errno = error;
	return outAddr;
}


/* Find all address associated with the NetBIOS name */
ssize_t SMBResolveNetBIOSName(const char * hostName, uint8_t nodeType, 
							  uint32_t timeout, struct sockaddr_storage ** results)
{
	int32_t outCount = 0;
	
	*results = SMBResolveNetBIOSNameEx(hostName, nodeType, NULL, timeout, NULL, &outCount);
	/* We didn't time out, report to the calling process that we had an error */
	if (errno && (errno != EHOSTUNREACH)) {
		return -1;
	}

	return outCount;
}

int 
SMBFrameworkVersion(void) {
	return SMBFS_VERSION;
}

/* Convert from UTF8 using system code page */
char * 
SMBConvertFromUTF8ToCodePage(const char *utf8Str, int uppercase)
{
	return convert_utf8_to_wincs(utf8Str, getPrefsCodePage(), uppercase);
	
}

/* Convert to UTF8 using system code page */
char * 
SMBConvertFromCodePageToUTF8(const char *cpStr)
{
	return convert_wincs_to_utf8(cpStr, getPrefsCodePage());

}

/* Convert UTF16 to UTF8 string */
char *
SMBConvertFromUTF16ToUTF8(const uint16_t *utf16str, size_t maxLen, uint64_t options)
{
#pragma unused(options)
	/* XXX - TBD Currently we ignore the options */
	return convert_unicode_to_utf8(utf16str, maxLen);

}

/* Convert UTF16 to UTF8 string */
uint16_t *
SMBConvertFromUTF8ToUTF16(const char *utf8str, size_t maxLen, uint64_t options)
{
#pragma unused(maxLen, options)
	/* XXX - TBD Currently we ignore the length field and options */
	return convert_utf8_to_leunicode(utf8str);
	
}

/* Find all the NetBIOS names associated using supplied host name */
struct NodeStatusInfo *
SMBGetNodeStatus(const char *hostName, uint32_t *outCount)
{
	struct NodeStatusInfo *outAddr = NULL, *listAddr;
	CFMutableArrayRef addressArray = NULL;
	CFMutableDataRef theData;
	CFMutableArrayRef nbrrArray;
	CFIndex ii;
	uint32_t jj;
	struct connectAddress *conn;
	struct nb_ctx ctx;
	struct smb_prefs prefs;
	int error;
	
	bzero(&ctx, sizeof(struct nb_ctx));
	/* Read the preference files */
	readPreferences(&prefs, NULL, NULL, FALSE, TRUE);
	/* Resolve the host name into a list of address */
	error = resolvehost(hostName, &addressArray, NULL, NBNS_UDP_PORT_137, TRUE, FALSE);
	if (error) {
		goto done;
	}
	/* Allocate the memory needed to hold the list of address and info */
	listAddr = outAddr = malloc(CFArrayGetCount(addressArray) * sizeof(struct NodeStatusInfo));
	if (outAddr == NULL) {
		error = ENOMEM;
		goto done;
	}

	for (ii=0; ii < CFArrayGetCount(addressArray); ii++) {		
		theData = (CFMutableDataRef)CFArrayGetValueAtIndex(addressArray, ii);
		if (! theData) {
			continue;
		}
		conn = (struct connectAddress *)(void *)CFDataGetMutableBytePtr(theData);
		if (!conn) {
			continue;
		}
		memcpy(&listAddr->node_storage, &conn->storage, sizeof(conn->storage));
		listAddr->node_servername[0] = (char)0;
		listAddr->node_workgroupname[0] = (char)0;
		listAddr->node_nbrrArray = NULL;
		/* Creating the array causes nbns_getnodestatus to return a full list */
		nbrrArray = CFArrayCreateMutable(kCFAllocatorSystemDefault, 0, 
													   &kCFTypeArrayCallBacks );
		listAddr->node_errno = nbns_getnodestatus(&conn->addr, &ctx, &prefs, NULL, 
												  listAddr->node_servername,  
												  listAddr->node_workgroupname, 
												  nbrrArray);
					
		if (nbrrArray && (listAddr->node_errno == 0)) {
			struct NBResourceRecord *nbrrDest;
			
			listAddr->node_nbrrArrayCnt = (uint32_t)CFArrayGetCount(nbrrArray);
			if (listAddr->node_nbrrArrayCnt) {
				listAddr->node_nbrrArray = malloc(listAddr->node_nbrrArrayCnt * sizeof(struct NBResourceRecord));
			}
			nbrrDest = listAddr->node_nbrrArray;
			if (listAddr->node_nbrrArray)
			for (jj=0; jj < listAddr->node_nbrrArrayCnt; jj++) {
				struct NBResourceRecord *nbrrSrc = NULL;
				
				theData = (CFMutableDataRef)CFArrayGetValueAtIndex(nbrrArray, jj);
				if (theData) {
					nbrrSrc = (struct NBResourceRecord *)(void *)CFDataGetMutableBytePtr(theData);
				}
				if (nbrrSrc == NULL) {
					listAddr->node_errno = ENOMEM;
					break;
				}
				memcpy(nbrrDest, nbrrSrc, sizeof(*nbrrSrc));
				nbrrDest++;
			}
		}
		if ((listAddr->node_errno) && (listAddr->node_nbrrArray)) {
			free(listAddr->node_nbrrArray);
			listAddr->node_nbrrArray = NULL;
			listAddr->node_nbrrArrayCnt = 0;
		}
		/* Done release it */
		if (nbrrArray) {
			CFRelease(nbrrArray);
		}
		listAddr++;
		*outCount += 1;
	}
	
	if (*outCount == 0) {
		free(outAddr);
		outAddr = NULL; /* Didn't really find any */
		error = EHOSTUNREACH;
	}
	
done:	
	if (addressArray) {
		CFRelease(addressArray);
	}
	releasePreferenceInfo(&prefs);
	errno = error;
	return outAddr;
}

int SMBCheckForAlreadyMountedShare(SMBHANDLE inConnection,
						  CFStringRef shareRef, CFMutableDictionaryRef mdictRef,
						  struct statfs *fs, int fs_cnt)
{
	void * hContext;
	NTSTATUS status;
	CFMutableStringRef upperStringRef;
	char ShareName[SMB_MAXSHARENAMELEN + 1];

	memset(ShareName, 0, sizeof(ShareName));
	status = SMBServerContext(inConnection, &hContext);
	if (!NT_SUCCESS(status)) {
		return EBADF;
	}
	upperStringRef = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, shareRef);
	if (upperStringRef == NULL) {
		return ENOMEM;
	}
	CFStringUppercase(upperStringRef, NULL);
	CFStringGetCString(upperStringRef, ShareName, SMB_MAXSHARENAMELEN + 1, kCFStringEncodingUTF8);
	CFRelease(upperStringRef);
	return already_mounted(hContext, ShareName, fs, fs_cnt, mdictRef, 0);
}

int SMBSetNetworkIdentity(SMBHANDLE inConnection, void *network_sid, char *account, char *domain)
{
	NTSTATUS	status;
	int			error;
    void		*hContext = NULL;
	ntsid_t		*ntsid = (ntsid_t *)network_sid;
	struct smbioc_ntwrk_identity ntwrkID;
	
	status = SMBServerContext(inConnection, &hContext);
	if (!NT_SUCCESS(status)) {
		/* Couldn't get the context? */
        return EINVAL;
    }	
	memset(&ntwrkID, 0, sizeof(ntwrkID));
	ntwrkID.ioc_version = SMB_IOC_STRUCT_VERSION;
	ntwrkID.ioc_ntsid_len = sizeof(*ntsid);
	ntwrkID.ioc_ntsid = *ntsid;
	if (account) {
		strlcpy(ntwrkID.ioc_ntwrk_account, account, sizeof(ntwrkID.ioc_ntwrk_account));
	}
	if (domain) {
		strlcpy(ntwrkID.ioc_ntwrk_domain, domain, sizeof(ntwrkID.ioc_ntwrk_domain));
	}
	if (smb_ioctl_call(((struct smb_ctx *)hContext)->ct_fd, SMBIOC_NTWRK_IDENTITY, &ntwrkID) == -1) {
		error = errno;
		smb_log_info("The SMBIOC_NTWRK_IDENTITY call failed, syserr = %s", 
					 ASL_LEVEL_DEBUG, strerror(error));
		return error;
	}
	return 0;
}

char *SMBCreateURLString(const char *domain, const char * user, const char * passwd, 
						 const char *server, const char *path, int32_t port)
{
	CFStringRef DomainName = NULL;
	CFStringRef Username = NULL;
	CFStringRef Password = NULL;
	CFStringRef ServerName = NULL;
	CFStringRef FullPath = NULL;
	CFStringRef PortNumber = NULL;
	CFStringRef	URLString = NULL;
	char *urlStr = NULL;

	if (domain) {
		DomainName = CFStringCreateWithCString(kCFAllocatorDefault, domain, kCFStringEncodingUTF8);
	}
	
	if (user) {
		Username = CFStringCreateWithCString(kCFAllocatorDefault, user, kCFStringEncodingUTF8);
	}
	
	if (passwd) {
		Password = CFStringCreateWithCString(kCFAllocatorDefault, passwd, kCFStringEncodingUTF8);
	}
	
	if (server) {
		ServerName = CFStringCreateWithCString(kCFAllocatorDefault, server, kCFStringEncodingUTF8);
	}
	
	if (path) {
		FullPath = CFStringCreateWithCString(kCFAllocatorDefault, path, kCFStringEncodingUTF8);
	}
	
	if (port != -1) {
		PortNumber = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR("%d"), port);
	}
	URLString = CreateURLCFString(DomainName, Username, Password, ServerName, FullPath, PortNumber);
	if (URLString) {
		CFIndex maxLen = (CFStringGetLength(URLString) * 3) + 1;
		urlStr = calloc(1, maxLen);
		if (urlStr) {
			CFStringGetCString(URLString, urlStr, maxLen, kCFStringEncodingUTF8);
		}
		CFRelease(URLString);
	}
	if (DomainName) {
		CFRelease(DomainName);
	}
	if (Username) {
		CFRelease(Username);
	}
	if (Password) {
		CFRelease(Password);
	}
	if (ServerName) {
		CFRelease(ServerName);
	}
	if (FullPath) {
		CFRelease(FullPath);
	}
	if (PortNumber) {
		CFRelease(PortNumber);
	}
	return urlStr;
}

CFStringRef
SMBCreateNetBIOSName(CFStringRef proposedName)
{
	CFMutableStringRef  composedName;
	CFStringEncoding    codepage;
	CFIndex		    nconverted = 0;
	CFIndex		    nused = 0;

	uint8_t		    name_buffer[NetBIOS_NAME_LEN];

	if (proposedName == NULL) {
	    return NULL;
	}

	codepage = getPrefsCodePage();

	composedName = CFStringCreateMutableCopy(kCFAllocatorDefault,
		0, proposedName);
	if (!composedName) {
	    return NULL;
	}
	CFStringTrimWhitespace(composedName);
	CFStringUppercase(composedName, CFLocaleGetSystem());

	nconverted = CFStringGetBytes(composedName,
		CFRangeMake(0,
		    MIN((CFIndex)sizeof(name_buffer), CFStringGetLength(composedName))),
		codepage, 0 /* loss byte */, false /* no BOM */,
		name_buffer, sizeof(name_buffer), &nused);

	/* We expect the conversion above to always succeed, given that we
	 * tried to remove anything that might not convert to a code page.
	 */
	if (nconverted == 0) {
	    char buf[256];

	    buf[0] = '\0';
	    CFStringGetCString(composedName, buf, sizeof(buf), kCFStringEncodingUTF8);
	    SMBLogInfo("failed to compose a NetBIOS name string from '%s'",
		    ASL_LEVEL_DEBUG, buf);

	    CFRelease(composedName);
	    return NULL;
	}

	CFRelease(composedName);

	/* Null terminate for the benefit of CFStringCreate. Be careful to be
	 * no more that 15 bytes, since the last byte is reserved for the name
	 * type.
	 */
	name_buffer[MIN(nused, (CFIndex)sizeof(name_buffer) - 1)] = '\0';

	composedName = CFStringCreateMutable(kCFAllocatorDefault,
		NetBIOS_NAME_LEN);
	if (composedName == NULL) {
	    return NULL;
	}

	CFStringAppendCString(composedName, (const char *)name_buffer, codepage);
	return composedName;
}


void
SMBRemountServer(const void	*inputBuffer, size_t inputBufferLen)
{
	fsid_t fsid = *(fsid_t *)inputBuffer;
	
	if (inputBufferLen == sizeof(fsid)) {
		int error = smb_remount_with_fsid(fsid);
		if (error) {
			SMBLogInfo("%s: error = %d inputBuffer = %p or inputBufferLen = %zu",
				   ASL_LEVEL_DEBUG, __FUNCTION__, error, inputBuffer, inputBufferLen);
		}
	} else {
		SMBLogInfo("%s: Bad inputBuffer = %p or inputBufferLen = %zu",
			   ASL_LEVEL_DEBUG, __FUNCTION__, inputBuffer, inputBufferLen);
	}
}

int
SMBGetDfsReferral(const char * url, CFMutableDictionaryRef dfsReferralDict)
{
	int			error;
	NTSTATUS	status;
	SMBHANDLE	serverConnection;
	void *      hContext = NULL;
	
	if (!dfsReferralDict) {
		errno = EINVAL;
		return -1;
	}
    
	status = SMBOpenServerEx(url, &serverConnection,kSMBOptionSessionOnly);
	/* SMBOpenServerEx sets errno, */
	if (!NT_SUCCESS(status)) {
		return -1;
	}
	status = SMBServerContext(serverConnection, &hContext);
	if (!NT_SUCCESS(status)) {
		/* Couldn't get the context? */
		SMBReleaseServer(serverConnection);	
		return -1;
	}
	error = getDfsReferralList(hContext, dfsReferralDict);
	if (error) {
		SMBReleaseServer(serverConnection);	
		errno = error;
		return -1;
	}
	SMBReleaseServer(serverConnection);	
	return 0;
}
