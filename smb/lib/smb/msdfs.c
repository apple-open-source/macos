/*
 * Copyright (c) 2009 - 2011 Apple Inc. All rights reserved.
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

/*
 * All terminology used in the file is comes from Microsoft. All comments that
 * come from Microsoft documentation is preceded by [MS-DFSC]. All comments that
 * come from C56 code is preceded by [C56]. All other comments are preceded by
 * [APPLE].
 *
 * [MS-DFSC]
 * The Distributed File System (DFS) enables file system clients to access remote 
 * file shares by using a DFS path (or virtual name) for the share, which is then 
 * transparently resolved to an actual share name on an actual file server. There 
 * is quite a bit of flexibility in this mapping. For example, DFS can enable the 
 * grouping of a set of shares located on different file servers into a unified 
 * namespace. DFS name resolution can also be used to transparently recover from 
 * a file server failure and to resolve a share to a geographically optimal file 
 * server for the specific client that is requesting access. Without DFS, users 
 * of a network file system, such as Server Message Block (SMB), are required to 
 * know the names of all file servers, and shares that reside on those file 
 * servers, for which they require access. With DFS, users can navigate a unified 
 * namespace to access files and folders without knowledge of the names of individual 
 * file servers and shares that host the data.
 *
 * After the DFS path has been resolved to an actual path, clients can directly 
 * access files on the identified servers by using standard remote file system 
 * protocols, such as the Server Message Block (SMB) Protocol (as specified in 
 * [MS-SMB]), Server Message Block (SMB) Version 2 Protocol (as specified in 
 * [MS-SMB2]), Network File System (NFS) (as specified in [RFC3530]), and NCP, 
 * as specified in [NOVELL]. When the client accesses the files and directories 
 * on the resolved share, DFS also provides a mechanism to cause the DFS client 
 * to perform a DFS referral request when accessing a link within the share.
 *
 * The DFS: Referral Protocol supports two types of namespaces: domain-based 
 * namespaces, which offer high availability and load balancing, and stand-alone 
 * namespaces, which reside on a single DFS root target server and do not require 
 * domain infrastructure. In stand-alone DFS namespaces, clients issue root 
 * referral requests and link referral requests directly to the DFS root target 
 * server.
 * For domain-based namespaces, clients issue DFS referral requests to domain 
 * controllers (DCs) to discover the existence of domains and the existence of 
 * DFS namespaces. Clients issue referral requests to DCs in order to discover 
 * the DFS root target servers hosting specific DFS namespaces. Clients can also 
 * issue referral requests to DFS root target servers to discover other DFS root 
 * target servers that host a DFS namespace. Clients issue referral requests to 
 * DFS root target servers to discover the locations of DFS link targets (shares 
 * on file servers). After the components of a DFS path have been resolved to 
 * specific targets, clients then issue file system requests directly to file 
 * servers, using the appropriate remote file system protocol for that server.
 * In crossing DFS namespaces, clients may attempt to issue file system requests 
 * against DFS links. File servers can notify the client of the requirement for 
 * name resolution by using designated error messages in file system protocols. 
 * Clients then issue referral requests to resolve DFS links into DFS link targets. 
 * With newly resolved paths, clients then reissue file system requests directly 
 * to the appropriate file servers. The DFS links in a DFS namespace can also 
 * include paths that point to other DFS namespaces. A DFS link that points to 
 * another DFS namespace is called a DFS interlink.
 * The DFS: Referral Protocol is a command/acknowledge protocol that sends out a 
 * sequence of referral requests to eventually resolve the DFS path to an actual 
 * path. There are five types of referral requests, as specified in section 2.2.2):
 *
 *	Domain referrals:	Which identify the domains in the forest to which the DFS 
 *						client is joined and the domains in other forests, which 
 *						are part of a trust relationship with the DFS client's 
 *						forest. A tailored set of domain referrals unique to each 
 *						client may be provided.
 *	DC referrals:		Which identify the DCs of a specific domain. 
 *	Root referrals:		Which identify the DFS root targets of a specific DFS 
 *						namespace. 
 *	Link referrals:		Which identity the DFS link targets of a specific link in 
 *						a DFS namespace. 
 *	Sysvol referrals:	Which identify the DCs that host a domain's SYSVOL or 
 *						NETLOGON shares.
 *
 * Domain-joined clients issue all five types of referral requests, while 
 * non-domain-joined clients issue only DFS root and DFS link referral requests. 
 * Optionally, clients can also be used to administer DFS namespaces (see [MS-ADTS]).
 * Clients can maintain local caches of information that are received through 
 * referral requests to avoid future referral requests and to improve the 
 * performance of DFS resource access, as specified in section 3.1.1.
 *
 * 
 * [APPLE]: We currently only deal with Root referrals and Link referrals. In the
 * future we should look at what is required to support Domain referrals, 
 * DC referrals and Sysvol referrals (Could be used by the AD Plugin). This is
 * not to say we don't parse these message, we just don't have any code to handle 
 * these correctly.
 *
 *
 * [MS-DFSC]
 * All strings in REQ_GET_DFS_REFERRAL (section 2.2.2) and RESP_GET_DFS_REFERRAL 
 * (section 2.2.3) messages MUST be encoded as null-terminated strings of UTF-16 
 * characters, as specified in [UNICODE].
 * Host Name:	Unless specified otherwise, a host name MUST be a null-terminated 
 *				Unicode character string, which may be a NetBIOS name, a DNS name 
 *				(for more information, see [RFC1034]) or other name formats supported 
 *				by name resolution mechanisms like the Service Location Protocol 
 *				(for more information, see [RFC2165]).
 * Sharename:	Unless specified otherwise, a sharename MUST be a null-terminated 
 *				Unicode character string whose format is dependent on the 
 *				underlying file server protocol that is used to access the share. 
 *				Examples of file server protocols are the SMB Protocol (as 
 *				specified in [MS-SMB]), NFS (for more information, see [RFC3530]), 
 *				and NCP (for more information, see [NOVELL]).
 * Domain Name:	Unless specified otherwise, a domain name MUST be a null-terminated 
 *				Unicode character string consisting of the name of a domain. This 
 *				can be either a NetBIOS name or a fully qualified domain name (FQDN), 
 *				as specified in [MS-ADTS].
 * UNC Path:	A UNC path can be used to access network resources, and MUST be 
 *				a null-terminated Unicode character string whose format MUST be 
 *				\\<hostname>\<sharename>[\<objectname>]*, where <hostname> is the 
 *				host name of a server or the domain name of a domain that hosts 
 *				resources, or an IP address; <sharename> is the share name or the 
 *				name of the resource being accessed; and <objectname> is the name 
 *				of an object, and is dependent on the actual resource being accessed. 
 * DFS Root:	A DFS root MUST be in one of the following UNC path formats. 
 *				\\<ServerName>\<DFSName> 
 *				\\<DomainName>\<DFSName>
 * DFS Link:	A DFS link MUST be in one of the following UNC path formats. 
 *				\\<ServerName>\<DFSName>\<LinkPath> 
 *				\\<DomainName>\<DFSName>\<LinkPath>
 * DFS Root Target:	A DFS root target is a UNC path that MUST be in the following format. 
 *				\\<servername>\<sharename>
 * DFS Link Target: A DFS link target is any UNC path that resolves to a directory.
 * DFS Target:	A DFS target is either a DFS root target or a DFS link target.
 */
/*
 * [C56]
 * Terminology used here and in documents from Microsoft
 *	Dfs				Distributed File System
 *	Dfs Namespace	A hierarchical arrangement of names consisting of a root name 
 *					that contains "links"; links contain information about where
 *					storage can be found.
 *	Dfs Root		The name of the base of a namespace.  Dfs root = root name; 
 *					A root name consists of two identifiers: /enterprise/root-id
 *					where 'enterprise' is the name of a domain or a server and 
 *					root-id uniquely identifies the root name.  A root name can 
 *					have different 'enterprise' ids:  /AD.FOO.NET/PUBLIC and 
 *					/FOO/PUBLIC refer to the same root name.
 *	Dfs Link		Information about the 'link' part of a namespace
 *	path			This is a conventional file path.  In this implementation, 
 *					the parts of the path are seperated by slashes and start with 
 *					a single slash For example:  /FOO/dfs/link-10/dfstester is 
 *					the path for the url cifs://FOO/dfs/link-10/dfstester
 * Referral			Information about a path that indicates the amount of the 
 *					path that is in a Dfs namespace and a list of paths that the 
 *					client can choose from that contain further namespace about 
 *					the path or contain the storage for the path. For example, 
 *					the client may ask for a referral for /FOO/dfs/link-10/dfstester.  
 *					One of the root servers for /FOO/dfs will return a referral.  
 *					The referral might indicate that "/FOO/dfs/link-10/" is being 
 *					referred.  It might indicate that there are two paths that the 
 *					client can choose from:  "/dc1/link-10" and "/dc2/link-test".  
 *					It will indicate if these two paths contain the actual storage 
 *					for "/dfstester" or if the client needs to obtain further referrals 
 *					from the paths.  Other information will be returned to help the 
 *					client with its caching strategy.
 * 
 * Dfs links are like pointers used to locate real storage.  They are similar to 
 * symbolic links or shortcuts, except that they can point to multiple places where 
 * the real storage might be found.
 * 
 * Dfs uses a protocol exchange to provide a lookup service to clients.  Using this 
 * protocol a client can perform three distinct functions:
 *	a) It can get a list of domain names known by the domain they are joined to.  
 *		This includes both FQDN and short names of all domains in the forest,
 *		and of other domains trusted by the forest.  It does this by passing an 
 *		empty "path" in the request
 *	b) It can get a list of root name servers by passing the "enterprise" part 
 *		of a namespace as the path in the request
 * c) It can get a list of referrals by passing a "path" in the request
 *  
 * The Dfs protocol message is named GET_DFS_REFERRAL, and is encoded using SMB 
 * and sent only to an IPC$ share. The client sends the referral containing a single 
 * "path" to one or more IPC$ shares.  The server IPC$ shares return a single "Referral".
 * 
 */

#include <netsmb/smbio.h>
#include <netsmb/upi_mbuf.h>
#include <sys/smb_byte_order.h>
#include <sys/mchain.h>
#include <netsmb/smb_converter.h>
#include <netsmb/rq.h>
#include <netsmb/smb_conn.h>
#include <parse_url.h>
#include "msdfs.h"
#include <smbclient/ntstatus.h>
#include <NetFS/NetFS.h>
#include <NetFS/NetFSPrivate.h>
#include <NetFS/NetFSUtilPrivate.h>

#define MAX_DFS_REFFERAL_SIZE 56 * 1024
#define REFERRAL_ENTRY_HEADER_SIZE	8

/*
 * [MS-DFSC]
 * ReferralHeaderFlags(4bytes): A 32 bit field representing as eries of flags
 * that are combined by using the bitwise OR operation. Only the R, S, and T bits 
 * are defined and used. The other bits MUST be set to 0 by the server and ignored 
 * upon receipt by the client.
 *
 * Value		Meaning
 * 0x00000001	R (ReferralServers): The R bit MUST be set to 1 if all of the targets 
 *				in the referral entries returned are DFS root targets capable of 
 *				handling DFS referral requests and set to 0 otherwise.
 * 0x00000002	S (StorageServers): The S bit MUST be set to 1 if all of the targets 
 *				in the referral response can be accessed without requiring further 
 *				referral requests and set to 0 otherwise.
 * 0x00000004	T (TargetFailback): The T bit MUST be set to 1 if DFS client target 
 *				failback is enabled for all targets in this referral response. This 
 *				value MUST be set to 0 by the server and ignored by the client for 
 *				all DFS referral versions except DFS referral version 4.
 */
enum {
    kDFSReferralServer		= 0x01,
    kDFSStorageServer		= 0x02,
    kDFSReferralMask		= 0x03,
    kDFSTargetFailback		= 0x04
};

#define MAX_LOOP_CNT		30

/* The only defined ReferralEntryFlag */
#define NAME_LIST_REFERRAL	0x0002

// DFS Version Levels
#define DFS_REFERRAL_V1		0x0001
#define DFS_REFERRAL_V2		0x0002
#define DFS_REFERRAL_V3		0x0003
#define DFS_REFERRAL_V4		0x0004

#define kReferralList			CFSTR("ReferralList")
 /* See [MS-DFSC] 2.2.2 REQ_GET_DFS_REFERRAL NOTE: In our case UTF8 */
#define kRequestFileName		CFSTR("RequestFileName")
#define kReferralRequestTime	CFSTR("RequestTime")
#define kPathConsumed			CFSTR("PathConsumed")
#define kNumberOfReferrals		CFSTR("NumberOfReferrals")
#define kReferralHeaderFlags	CFSTR("ReferralHeaderFlags")
#define kVersionNumber			CFSTR("VersionNumber")
#define kSize					CFSTR("Size")
#define kServerType				CFSTR("ServerType")
#define kReferralEntryFlags		CFSTR("ReferralEntryFlags")
#define	kProximity				CFSTR("Proximity")
#define	kTimeToLive				CFSTR("TimeToLive")
#define	kDFSPathOffset			CFSTR("DFSPathOffset")
#define	kDFSPath				CFSTR("DFSPath")
#define	kDFSAlternatePathOffset	CFSTR("DFSAlternatePathOffset")
#define	kDFSAlternatePath		CFSTR("DFSAlternatePath")
#define	kNetworkAddressOffset	CFSTR("NetworkAddressOffset")
#define	kNetworkAddress			CFSTR("NetworkAddress")
#define kNewReferral			CFSTR("NewReferral")
#define kDfsServerArray         CFSTR("DfsServerArray")
#define kDfsReferralArray		CFSTR("DfsReferralArray")
/* 
 * In the spec version one calls it Share Name, but it seems to behave
 * the same as Network Address. Until we decide lets treat them the same.
 */
#define kShareName				kNetworkAddress
#define	kSpecialNameOffset		CFSTR("SpecialNameOffset")
#define	kSpecialName			CFSTR("SpecialName")
#define	kNumberOfExpandedNames	CFSTR("NumberOfExpandedNames")
#define	kExpandedNameOffset		CFSTR("ExpandedNameOffset")
#define	kExpandedNameArray		CFSTR("ExpandedNameArray")
#define	kUnconsumedPath			CFSTR("UnconsumedPath")

/*
 * Helper routine that retrieves a uint16_t value from the dictionary.
 */
static uint16_t uint16FromDictionary(CFDictionaryRef dict, CFStringRef key)
{
	CFNumberRef num = CFDictionaryGetValue( dict, key);
	uint16_t value = 0;

	if( num ) {
		CFNumberGetValue(num, kCFNumberSInt16Type, &value);
	}
	return value;
}

/*
 * Helper routine that retrieves a uint32_t value from the dictionary.
 */
static uint32_t uint32FromDictionary(CFDictionaryRef dict, CFStringRef key)
{
	CFNumberRef num = CFDictionaryGetValue( dict, key);
	uint32_t value = 0;
	
	if( num ) {
		CFNumberGetValue(num, kCFNumberSInt32Type, &value);
	}
	return value;
}

/*
 * Helper routine that adds a number value to the dictionary.
 */
static void addNumberToDictionary(CFMutableDictionaryRef dict, CFStringRef key, 
								  CFNumberType theType, void *value)
{
	CFNumberRef num = CFNumberCreate( nil, theType, value);
	
	if( num ) {
		CFDictionarySetValue( dict, key, num);
		CFRelease(num);
	}
}

/*
 * Helper routine that adds a c-style string  to the dictionary.
 */
static void addCStringToCFStringArray(CFMutableArrayRef array, const char *str)
{
	CFStringRef cfStr = CFStringCreateWithCString(nil, str, kCFStringEncodingUTF8);
	if( cfStr )
	{
		CFArrayAppendValue( array, cfStr);
		CFRelease(cfStr);
	}
}

/*
 * Helper routine that adds a c-style string  to the dictionary.
 */
static void addCStringToDictionary(CFMutableDictionaryRef dict, CFStringRef key, 
								   const char *str)
{
	CFStringRef cfStr = CFStringCreateWithCString(nil, str, kCFStringEncodingUTF8);
	if( cfStr )
	{
		CFDictionarySetValue(dict, key, cfStr );
		CFRelease(cfStr);
	}
}

/*
 * [APPLE]
 *
 * May want to change this in the future, but for now we just worry about the
 * status not cover error. What should we do about other errors? For now we
 * just log them and return false.
 */
static int isReferralPathNotCovered(struct smb_ctx * inConn, CFStringRef referralStr)
{
	int error, rtValue = FALSE;
	uint32_t ntError;
	char *referral;

	referral = CStringCreateWithCFString(referralStr);
	if (!referral)
		return FALSE;
	
	error = smbio_check_directory(inConn, referral, SMB_FLAGS2_DFS, &ntError);
	if (error && (ntError == STATUS_PATH_NOT_COVERED)) {
		rtValue = TRUE;
	} else if (error) {
		smb_log_info("%s Dfs Check path on %s return ntstatus = 0x%x, syserr = %s", 
					 ASL_LEVEL_ERR, __FUNCTION__, referral, ntError, strerror(error));
	}
	free(referral);
	return rtValue;
}

/*
 * getUnconsumedPathV1
 *
 * This routine is really crude, but we do not expect to many level one 
 * referrals.
 */
static char * getUnconsumedPathV1(struct smb_ctx * inConn, uint16_t pathConsumed, 
								  const char *referral)
{
	int		error;
    size_t	unconsumedLen;
    size_t	utf16Len;
    char 	*utf16Str = NULL;
    char 	*unconsumedPath = NULL;
	
    utf16Len = strlen(referral) * 2;
	/*
	 * [APPLE]
	 * The original C56 code assume that if PathConsmed was zero or it was not
	 * greater than the referral stings UTF16 length then the whole thing got
	 * consume. Hard to test to be sure.
	 */
    if (!pathConsumed || (utf16Len <= pathConsumed)) {
		if (utf16Len != pathConsumed) {
			smb_log_info("%s PathConsumed = %d len = %ld",
						 ASL_LEVEL_DEBUG, __FUNCTION__, pathConsumed, utf16Len);
		}
        return NULL;
    }
    utf16Str = malloc(utf16Len);
	if (utf16Str == NULL) {
		error = ENOMEM;
	} else {
		error = smb_localpath_to_ntwrkpath(inConn, referral, strlen(referral),
										   utf16Str, &utf16Len, 0);
	}
	
	if (error == 0) {
		unconsumedLen = (utf16Len * 9) + 1;
		
		unconsumedPath = malloc(unconsumedLen);
		error =  smb_ntwrkpath_to_localpath(inConn, utf16Str+pathConsumed, 
											utf16Len-pathConsumed,
											unconsumedPath, &unconsumedLen, 0);
	}
	
    if (utf16Str) {
		free(utf16Str);
	}
	if (error) {
		smb_log_info("%s failed, syserr = %s",  ASL_LEVEL_ERR, __FUNCTION__, strerror(error));
	}
    return(unconsumedPath);
}

/*
 * getUnconsumedPath
 *
 * See if we have an unconsume path left.
 */
static char *getUnconsumedPath(size_t pathConsumed, const char *referral, 
							   size_t referralLen)
{
	size_t maxLen = referralLen-pathConsumed + 1;
    char *unconsumedPath = NULL;
	
 	/*
	 * [APPLE]
	 * The original C56 code assume that if PathConsmed was zero or it was not
	 * greater than the referral stings UTF16 length then the whole thing got
	 * consume. Hard to test to be sure.
	 */
	if (!pathConsumed || (referralLen <= pathConsumed)) {
		smb_log_info("%s PathConsumed = %ld referral len = %ld",
					 ASL_LEVEL_DEBUG, __FUNCTION__, pathConsumed, referralLen);
        return NULL;
    }
    unconsumedPath = malloc(referralLen-pathConsumed + 1);
    if (!unconsumedPath) {
		smb_log_info("%s malloc failed, syserr = %s", ASL_LEVEL_ERR, 
					 __FUNCTION__, strerror(ENOMEM));
		return NULL;
    }
    strlcpy(unconsumedPath, (char *)(referral+pathConsumed), maxLen);
	
    return(unconsumedPath);
}

/*
 * getUTF8String
 *
 * Get the Dfs string.
 */
static int 
getUTF8String(struct smb_ctx * inConn, mdchain_t mdp, uint16_t offset, 
						   size_t *utf16Lenp, size_t *utf8Lenp, char **outUTF8Str)
{
	struct mdchain	mdShadow;
	size_t utf8Len = 0;
	size_t utf16Len = 0;
	char *utf16Str = NULL;
	char *utf8Str = NULL;
	int error = 0;
	
    *outUTF8Str = NULL;
	if (utf8Lenp) {
		*utf8Lenp = 0;
	}
	if (utf16Lenp) {
		*utf16Lenp = 0;
	}
	
	md_shadow_copy(mdp, &mdShadow);
	error = md_get_mem(&mdShadow, NULL, offset, MB_MSYSTEM);
	if (error)
		return error;
	
	utf16Len = md_get_utf16_strlen(&mdShadow);
	if (utf16Len == 0)
		goto done;
	
	utf16Len += 2;	/* Add the two null bytes back on */
	utf8Len = utf16Len * 9;
	utf16Str = malloc(utf16Len);
	if (utf16Str) {
		utf8Str = malloc(utf8Len);
	}
	if (utf8Str == NULL) {
		error = ENOMEM;
	} else {
		error =  md_get_mem(&mdShadow, utf16Str, utf16Len, MB_MSYSTEM);
	}
	if (error == 0) {
		error = smb_ntwrkpath_to_localpath(inConn, utf16Str, utf16Len, 
										   utf8Str, &utf8Len, 0);
	}
	if (utf8Str) {
		*outUTF8Str = utf8Str;
	}
	if (utf16Str) {
		free(utf16Str);
	}
	
done:
	if (error == 0) {
		if (utf8Lenp) {
			*utf8Lenp = utf8Len;
		}
		if (utf16Lenp) {
			*utf16Lenp = utf16Len;
		}
	}
	return error;
}

/*
 * addDfsStringToDictionary
 *
 * Get the Dfs string and add it to the dictionary using the key provided.
 */
static int addDfsStringToDictionary(struct smb_ctx * inConn, mdchain_t mdp, uint16_t offset, 
                                    size_t *utf16Lenp, size_t *utf8Lenp,
                                    CFMutableDictionaryRef dict, CFStringRef key)
{
	char *utf8Str = NULL;
	int error = 0;
    
    error = getUTF8String(inConn, mdp, offset, utf16Lenp, utf8Lenp, &utf8Str);

	if ((error == 0) && (utf8Str)) {
		addCStringToDictionary(dict, key, utf8Str);
		if (CFStringCompare(key, kNetworkAddress, kCFCompareCaseInsensitive) == 0)
			smb_log_info("%s NetworkAddress: %s", ASL_LEVEL_DEBUG, __FUNCTION__, utf8Str);
		else if (CFStringCompare(key, kNewReferral, kCFCompareCaseInsensitive) == 0)
			smb_log_info("%s kNewReferral: %s", ASL_LEVEL_DEBUG, __FUNCTION__, utf8Str);
	}
	if (utf8Str) {
		free(utf8Str);
	}
	return error;
}

/*
 * decodeDfsReferral
 *
 * [APPLE]
 * The C56 code place some of this information into a CFDictionary, which it
 * would use for caching. Not sure we need to cache this info now? For now lets
 * parse everything and put it in the dictionary, we can always change this in
 * the fuuture.
 */
static int decodeDfsReferral(struct smb_ctx * inConn, mdchain_t mdp, 
							   const char * dfsReferral, 
							   CFMutableDictionaryRef* outReferralDict)
{
    uint16_t	pathConsumed;
    uint16_t	numberOfReferrals, ii;
	uint32_t	referralHeaderFlags;
	CFMutableDictionaryRef referralDict = NULL;
	CFMutableArrayRef referralList = NULL;
	int			error;
	time_t 		tnow = time(NULL);
   char	*unconsumedPath = NULL;
  
	/*
	 * Contains the referal header and array of referral entries
	 */
	referralDict = CFDictionaryCreateMutable( kCFAllocatorSystemDefault, 0, 
										 &kCFTypeDictionaryKeyCallBacks, 
										 &kCFTypeDictionaryValueCallBacks);
	if (referralDict) {
		referralList = CFArrayCreateMutable( kCFAllocatorSystemDefault, 0, 
											&kCFTypeArrayCallBacks );
		if (referralList) {
			CFDictionarySetValue( referralDict, kReferralList, referralList);
		} else {
			CFRelease(referralDict);
			referralDict = NULL;
		}
	}
	
	if(! referralDict ) {
		error = ENOMEM;
		smb_log_info("%s Dictionary create failed: for referral %s, syserr = %s",  
					 ASL_LEVEL_ERR, __FUNCTION__, dfsReferral, strerror(error));
		goto done;
	}
	
	addCStringToDictionary(referralDict, kRequestFileName, dfsReferral);
	addNumberToDictionary(referralDict, kReferralRequestTime, kCFNumberLongType, &tnow);
	smb_log_info("%s RequestFileName: %s", ASL_LEVEL_DEBUG, __FUNCTION__, dfsReferral);
	
	/*
	 * [MS-DFSC]
	 * PathConsumed (2 bytes): A 16-bit integer indicating the number of 
	 * bytes, not characters, in the prefix of the referral request path that is 
	 * matched in the referral response.	
	 */
	error = md_get_uint16le(mdp, &pathConsumed);
	if (error) {
		smb_log_info("%s: Failed to get path consumed, syserr = %s", 
					 ASL_LEVEL_ERR, __FUNCTION__, strerror(error));
		goto done;
	}
	addNumberToDictionary(referralDict, kPathConsumed, kCFNumberSInt16Type, &pathConsumed);
	
	/*
	 * [MS-DFSC]
	 * NumberOfReferrals(2bytes): A 16-bit integer indicating the number of referral
	 * entries immediately following the referral header.
	 */
	error = md_get_uint16le(mdp, &numberOfReferrals);
	if (error) {
		smb_log_info("%s: Failed to get the number of referrals, syserr = %s",
					 ASL_LEVEL_ERR, __FUNCTION__, strerror(error));
		goto done;
	}
	addNumberToDictionary(referralDict, kNumberOfReferrals, kCFNumberSInt16Type, &numberOfReferrals);
	smb_log_info("%s NumberOfReferrals: %d", ASL_LEVEL_DEBUG, __FUNCTION__, numberOfReferrals);
	
	/*
	 * [MS-DFSC]
	 * If the referral request is successful, but the NumberOfReferrals field in 
	 * the referral header (as specified in section 2.2.3) is 0, the DFS server 
	 * could not find suitable targets to return to the client. In this case, the 
	 * client MUST fail the original I/O operation with an implementation-defined 
	 * error .
	 */
    if (!numberOfReferrals) {
        /*
		 * [C56]
		 * This happens when the target of the referral is down or disabled, and 
		 * the referring server (the one returning this info) knows that it is 
		 * down.  In other words, we are being told that that storage is not 
		 * available right now.
		 */
		error = ENOENT;
		smb_log_info("%s NumberOfReferrals is zero, syserr = %s",
					 ASL_LEVEL_DEBUG, __FUNCTION__, strerror(error));
		goto done;
    }
	
	/*
	 * [MS-DFSC]
	 * ReferralHeaderFlags(4bytes): A 32-bit field representing a series of flags
	 * that are combined by using the bitwise OR operation. Only the R, S, and T 
	 * bits are defined and used. The other bits MUST be set to 0 by the server 
	 * and ignored upon receipt by the client.	
	 */
	error = md_get_uint32le(mdp, &referralHeaderFlags);
	if (error) {
		smb_log_info("%s Failed to get the referral header flags, syserr = %s",
					 ASL_LEVEL_ERR, __FUNCTION__, strerror(error));
		goto done;
	}
	addNumberToDictionary(referralDict, kReferralHeaderFlags, kCFNumberSInt32Type, &referralHeaderFlags);
	smb_log_info("%s ReferralHeaderFlags: %d", ASL_LEVEL_DEBUG, __FUNCTION__, referralHeaderFlags);

	/*
	 * [C56]
	 * These are the only ones we understand
	 */
    referralHeaderFlags &= kDFSReferralMask;

	/*
	 * [MS-DFSC]
	 * The DFS: Referral Protocol defines four structures used to encode referral 
	 * entries: DFS_REFERRAL_V1, DFS_REFERRAL_V2, DFS_REFERRAL_V3, and DFS_REFERRAL_V4.
	 *
	 * All referral entries in a RESP_GET_DFS_REFERRAL message MUST use the 
	 * same referral entry structure. As a consequence, all referral entries 
	 * in a RESP_GET_DFS_REFERRAL message MUST have the same version number.
	 *
	 * Each referral entry structure has a 16-bit Size field. The Size field 
	 * indicates the total size, in bytes, of the referral entry. Clients MUST 
	 * add the value of the Size field in a referral entry to the offset of that 
	 * referral entry to find the offset of the next referral entry in the 
	 * RESP_GET_DFS_REFERRAL message.
	 *
	 * The DFS_REFERRAL_V2, DFS_REFERRAL_V3, and DFS_REFERRAL_V4 structures contain 
	 * fields with offsets to strings. Clients MUST add the string offset to the 
	 * offset of the beginning of the referral entry to find the offset of the 
	 * string in the RESP_GET_DFS_REFERRAL message. The strings referenced from 
	 * the fields of a referral entry MAY immediately follow the referral entry 
	 * structure or MAY follow the last referral entry in the RESP_GET_DFS_REFERRAL 
	 * message. The Size field of a referral entry structure MUST include the size 
	 * in bytes of all immediately following strings so that a client can find 
	 * the next referral entry in the message. The Size field of a referral entry 
	 * structure MUST NOT include the size of referenced strings located after 
	 * the last referral entry in the message.
	 */	
	
	/*
	 * [APPLE]
	 * The C56 code treated DFS_REFERRAL_V2 the same as DFS_REFERRAL_V3, this
	 * is wrong. In fact the C56 code's DFS_REFERRAL_V2 never worked from what 
	 * I can tell. The DFS_REFERRAL_V3 is also broken, but since they thought
	 * DFS_REFERRAL_V2 and DFS_REFERRAL_V3 where the same it worked out for them.
	 * It is understandable why they got this wrong since there was no spec, but
	 * now that we have a spec we should follow it. Once we get this code working
	 * we should remove this comment. Place holder to when comparing the C56 code
	 * with my new code.
	 */
	for (ii=1; ii <= numberOfReferrals; ii++) {
		uint16_t VersionNumber;
		uint16_t ReferralSize;
		uint16_t ServerType;
		uint16_t ReferralEntryFlags;
		struct mdchain	md_referral_shadow;
		CFMutableDictionaryRef referralInfo;
		
		md_shadow_copy(mdp, &md_referral_shadow);
		referralInfo = CFDictionaryCreateMutable( kCFAllocatorSystemDefault, 0, 
												 &kCFTypeDictionaryKeyCallBacks, 
												 &kCFTypeDictionaryValueCallBacks);
		if( !referralInfo ) {
			error = ENOMEM;
			smb_log_info("%s: Creating referral info failed, syserr = %s",
						 ASL_LEVEL_ERR, __FUNCTION__, strerror(error));
			break;
		}

		/*
		 * [MS-DFSC]
		 * VersionNumber (2 bytes): A 16-bit integer indicating the version 
		 * number of the referral entry. 
		 * MUST always be 0x0001 for DFS_REFERRAL_V1.
		 * MUST always be 0x0002 for DFS_REFERRAL_V2.
		 * MUST always be 0x0003 for DFS_REFERRAL_V3.
		 * MUST always be 0x0004 for DFS_REFERRAL_V4.
		 * NOTE: All referral entries in a RESP_GET_DFS_REFERRAL message MUST 
		 * have the same version number. 
		 * [APPLE]
		 * Should we test for this here? If so do report an error? Do we care?
		 */
		error = md_get_uint16le(mdp, &VersionNumber);
		if (error) {
			smb_log_info("%s: Getting referral %d's version failed, syserr = %s", 
						 ASL_LEVEL_ERR, __FUNCTION__, ii, strerror(error));
			CFRelease(referralInfo);
			break;
		}
		addNumberToDictionary(referralInfo, kVersionNumber, kCFNumberSInt16Type, &VersionNumber);
		smb_log_info("%s VersionNumber: %d", ASL_LEVEL_DEBUG, __FUNCTION__, VersionNumber);

		/*
		 * [MS-DFSC]
		 * Size(2bytes): A 16-bit integer indicating the total size of the 
		 * referral entry inbytes.
		 */
		error = md_get_uint16le(mdp, &ReferralSize);		
		if (error) {
			smb_log_info("%s: Getting referral %d's size failed, syserr = %s", 
						 ASL_LEVEL_ERR, __FUNCTION__, ii, strerror(error));
			CFRelease(referralInfo);
			break;
		}
		addNumberToDictionary(referralInfo, kSize, kCFNumberSInt16Type, &ReferralSize);
		
		/* [APPLE] Make sure the ReferralSize contains the referral enty header size */ 
		if (ReferralSize < REFERRAL_ENTRY_HEADER_SIZE ) {
			error = EPROTO;
			smb_log_info("%s: Bad referral %d's size got %d, syserr = %s", 
						 ASL_LEVEL_ERR, __FUNCTION__, ii, ReferralSize, strerror(error));
			CFRelease(referralInfo);
			break;
		}
		/*  [APPLE] Now consume the header size */
		ReferralSize -= REFERRAL_ENTRY_HEADER_SIZE; 
		
		/*
		 * [MS-DFSC]
		 * ServerType (2 bytes): A 16-bit integer indicating the type of server 
		 * hosting the target. This field MUST be set to 0x0001 if DFS root targets 
		 * are returned, and to 0x0000 otherwise. Note that sysvol targets are 
		 * not DFS root targets; the field MUST be set to 0x0000 for a sysvol 
		 * referral response.
		 */
		error = md_get_uint16le(mdp, &ServerType);
		if (error) {
			smb_log_info("%s: Getting referral %d's server type failed, syserr = %s", 
						 ASL_LEVEL_ERR, __FUNCTION__, ii, strerror(error));
			CFRelease(referralInfo);
			break;
		}		
		addNumberToDictionary(referralInfo, kServerType, kCFNumberSInt16Type, &ServerType);
		
		/*
		 * [MS-DFSC]
		 * For DFS_REFERRAL_V1 and DFS_REFERRAL_V2 the following applies:
		 * ReferralEntryFlags (2 bytes): A series of bit flags. MUST be set to 
		 * 0x0000 and ignored on receipt.
		 * 
		 * For DFS_REFERRAL_V3 and DFS_REFERRAL_V4 the following applies:
		 * ReferralEntryFlags (2 bytes): A 16-bit field representing a series of 
		 * flags that are combined by using the bitwise OR operation. Only the 
		 * N bit is defined for DFS_REFERRAL_V3. The other bits MUST be set to 
		 * 0 by the server and ignored upon receipt by the client.
		 *	Value		Meaning
		 *	N 0x0002	MUST be set for a domain referral response or a DC 
		 *				referral response.
		 */
		error = md_get_uint16le(mdp, &ReferralEntryFlags);
		if (error) {
			smb_log_info("%s: Getting referral %d's entry flags failed, syserr = %s", 
						 ASL_LEVEL_ERR, __FUNCTION__, ii, strerror(error));
			CFRelease(referralInfo);
			break;
		}
		if ((VersionNumber == DFS_REFERRAL_V1) || (VersionNumber == DFS_REFERRAL_V2)) {
			/* 
			 * [APPLE]
			 * Version two and version three have very simliar layouts. We can
			 * treat the string offsets the same if the ReferralEntryFlags is
			 * alway zero. Seting it to zero, since with version one and two it 
			 * should be ignore anyways. See above notes.
			 */
			ReferralEntryFlags = 0;
		} else {
			ReferralEntryFlags &= NAME_LIST_REFERRAL;
		}
		
		addNumberToDictionary(referralInfo, kReferralEntryFlags, kCFNumberSInt16Type, &ReferralEntryFlags);
	
		/* [APPLE] Now it depends on the version number for the rest of the lay out */
		if (VersionNumber == DFS_REFERRAL_V1) {
			error = addDfsStringToDictionary(inConn, mdp, 0, NULL, NULL, referralInfo, kShareName);
			if (!error) {
				/* [APPLE] Now consume the share name */
				error =  md_get_mem(mdp, NULL, ReferralSize, MB_MSYSTEM);
			}
			if (error) {
				smb_log_info("%s: Getting referral %d's share name failed, syserr = %s", 
							 ASL_LEVEL_ERR, __FUNCTION__, ii, strerror(error));
				CFRelease(referralInfo);
				break;
			}
			unconsumedPath = getUnconsumedPathV1(inConn, pathConsumed, dfsReferral);
		} else if ((VersionNumber == DFS_REFERRAL_V2) ||
				   (VersionNumber == DFS_REFERRAL_V3) ||
				   (VersionNumber == DFS_REFERRAL_V4)) {
			uint32_t	Proximity, TimeToLive;
			uint16_t	Offset;
			size_t		ConsumedSize = 0;
			
			if (VersionNumber == DFS_REFERRAL_V2) {
				/*
				 * [MS-DFSC]
				 * Proximity(4bytes): MUST be set to 0x00000000 by the server and 
				 * ignored by the client.
				 */
				error = md_get_uint32le(mdp, &Proximity);
				if (error) {
					smb_log_info("%s: Getting referral %d's proximity failed, syserr = %s", 
								 ASL_LEVEL_ERR, __FUNCTION__, ii, strerror(error));
					CFRelease(referralInfo);
					break;
				}
				addNumberToDictionary(referralInfo, kProximity, kCFNumberSInt32Type, &Proximity);
				ReferralSize -= 4;
			}
			/*
			 * [MS-DFSC]
			 * TimeToLive (4 bytes): A 32-bit integer indicating the time-out 
			 * value, in seconds, of the DFS root or DFS link. MUST be set to the 
			 * time-out value of the DFS root or the DFS link in the DFS metadata 
			 * for which the referral response is being sent. When there is more 
			 * than one referral entry, the TimeToLive of each referral entry 
			 * MUST be the same.
			 */
			error = md_get_uint32le(mdp, &TimeToLive);
			if (error) {
				smb_log_info("%s: Getting referral %d's time to live failed, syserr = %s", 
							 ASL_LEVEL_ERR, __FUNCTION__, ii, strerror(error));
				CFRelease(referralInfo);
				break;
			}
			ReferralSize -= 4;
			addNumberToDictionary(referralInfo, kTimeToLive, kCFNumberSInt32Type, &TimeToLive);
			
			/*
			 * [APPLE]
			 * We currently do not support NAME_LIST_REFERRAL referrals. We do
			 * parse them and add them to the dictionary, but we never use them.
			 * In the future we may want to just skip this entry and go to the 
			 * next one. We should ifdef the code in case we want to use it in
			 * the future.
			 */
			if (ReferralEntryFlags & NAME_LIST_REFERRAL) {
				uint16_t NumberOfExpandedNames;
				
				/*
				 * [MS-DFSC]
				 * SpecialNameOffset (2 bytes): A 16-bit integer indicating the offset, 
				 * in bytes, from the beginning of the referral entry to a domain name. 
				 * For a domain referral response, this MUST be the domain name that 
				 * corresponds to the referral entry. For a DC referral response, 
				 * this MUST be the domain name that is specified in the DC referral 
				 * request. The domain name MUST be a null-terminated string.			 
				 */
				error = md_get_uint16le(mdp, &Offset);
				/*
				 * [APPLE]
				 * In some cases a zero offset is ok, so if we get a zero offset
				 * treat it like we got a string of zero length.
				 */
				if (!error && Offset)
					error = addDfsStringToDictionary(inConn, &md_referral_shadow, 
													 Offset, NULL, NULL, 
													 referralInfo, kSpecialName);
				
				if (error) {
					smb_log_info("%s: Getting referral %d's special name failed, syserr = %s", 
								 ASL_LEVEL_ERR, __FUNCTION__, ii, strerror(error));
					CFRelease(referralInfo);
					break;
				}
				ReferralSize -= 2;
				addNumberToDictionary(referralInfo, kSpecialNameOffset, kCFNumberSInt16Type, &Offset);
				
				/*
				 * [MS-DFSC]
				 * NumberOfExpandedNames (2 bytes): A 16-bit integer indicating the 
				 * number of DCs being returned for a DC referral request. MUST be 
				 * set to 0 for a domain referral response.
				 */
				error = md_get_uint16le(mdp, &NumberOfExpandedNames);
				if (error) {
					smb_log_info("%s: Getting referral %d's number of expaned names failed, syserr = %s", 
								 ASL_LEVEL_ERR, __FUNCTION__, ii, strerror(error));
					CFRelease(referralInfo);
					break;
				}
				ReferralSize -= 2;
				addNumberToDictionary(referralInfo, kNumberOfExpandedNames, 
									  kCFNumberSInt16Type, &NumberOfExpandedNames);
				
				/*
				 * [MS-DFSC]
				 * ExpandedNameOffset (2 bytes): A 16-bit integer indicating the 
				 * offset, in bytes, from the beginning of this referral entry 
				 * to the first DC name string returned in response to a DC 
				 * referral request. If multiple DC name strings are being 
				 * returned in response to a DC referral request, the first DC 
				 * name string must be followed immediately by the additional DC 
				 * name strings. The total number of consecutive name strings 
				 * MUST be equal to the value of the NumberOfExpandedNames field. 
				 * This field MUST be set to 0 for a domain referral response. 
				 * Each DC name MUST be a null-terminated string.
				 */
				error = md_get_uint16le(mdp, &Offset);
				/*
				 * [APPLE]
				 * In some cases a zero offset is ok, so if we get a zero offset
				 * treat it like we got a string of zero length.
				 * XXX - Not sure yet how to handle have more than one yet. Need 
				 * to find a test case and then fix this code.
                 *
                 * Currently we just grab the first one.
				 */
				if ((NumberOfExpandedNames) && Offset) {
                    int jj;
                    uint16_t expandedNameOffset = Offset;
                    char *utf8Str = NULL;
                    size_t utf16Len = 0;
                    CFMutableArrayRef expandedNameArray = CFArrayCreateMutable(kCFAllocatorSystemDefault, 0, &kCFTypeArrayCallBacks);
                  
                    if (!expandedNameArray) {
                        smb_log_info("%s: Getting referral %d's failed, internal error CFArrayCreateMutable failed", ASL_LEVEL_ERR, __FUNCTION__, ii);
                        CFRelease(referralInfo);
                        break;
                    }
                    for (jj=0; jj<NumberOfExpandedNames; jj++) {
                        error = getUTF8String(inConn,  &md_referral_shadow, expandedNameOffset, &utf16Len, NULL, &utf8Str);
                        if (error) {
                            break;
                        }
                        addCStringToCFStringArray(expandedNameArray, utf8Str);
                        free(utf8Str);
                        expandedNameOffset += utf16Len;
                        utf16Len = 0;
                     }
                    CFDictionarySetValue(referralInfo, kExpandedNameArray, expandedNameArray);
                    CFRelease(expandedNameArray);
				}
				if (error) {
					smb_log_info("%s: Getting referral %d's special name  failed, syserr = %s", 
								 ASL_LEVEL_ERR, __FUNCTION__, ii, strerror(error));
                    if (referralInfo) {
                        CFRelease(referralInfo);
                    }
					break;
				}
				ReferralSize -= 2;
				addNumberToDictionary(referralInfo, kExpandedNameOffset, 
									  kCFNumberSInt16Type, &Offset);

				/*
				 * [MS-DFSC]
				 * Padding (variable): The server MAY insert zero or 16 padding 
				 * bytes that MUST be ignored by the client
				 *
				 * [APPLE] - So what does that mean can they exist or not?
				 */
				if (ReferralSize) {
					error = md_get_mem(mdp, NULL, ReferralSize, MB_MSYSTEM);
					ReferralSize = 0;
				}
				/*
				 * [APPLE]
				 * Not sure how to handles these or if we want to handle them. So
				 * for now lets ignore them and just go to the next element in
				 * the list.
				 */
				unconsumedPath = NULL;
				smb_log_info("%s: Skipping referral %d's NAME_LIST_REFERRAL!", 
							 ASL_LEVEL_DEBUG, __FUNCTION__, ii);
			} else {
				/*
				 * [MS-DFSC]
				 * DFSPathOffset (2 bytes): A 16-bit integer indicating the offset, 
				 * in bytes, from the beginning of this referral entry to the DFS path 
				 * that corresponds to the DFS root or DFS link for which target 
				 * information is returned. The DFS path MUST be a null-terminated string.
				 */
				error = md_get_uint16le(mdp, &Offset);
				/*
				 * [APPLE]
				 * In some cases a zero offset is ok, so if we get a zero offset
				 * treat it like we got a string of zero length. In this case is
				 * it ok?
				 */
				if (!error && Offset) {
					error = addDfsStringToDictionary(inConn, &md_referral_shadow, Offset, NULL, 
											&ConsumedSize, referralInfo, kDFSPath);
				}
				if (error) {
					smb_log_info("%s: Getting referral %d's DFS path  failed, syserr = %s", 
								 ASL_LEVEL_ERR, __FUNCTION__, ii, strerror(error));
                    if (referralInfo) {
                        CFRelease(referralInfo);
                    }
					break;
				}
				ReferralSize -= 2;
				addNumberToDictionary(referralInfo, kDFSPathOffset, kCFNumberSInt16Type, &Offset);
				
				/*
				 * [MS-DFSC]
				 * DFSAlternatePathOffset(2bytes): A 16-bit integer indicating the 
				 * offset, in bytes, from the beginning of this referral entry to the 
				 * DFS path that corresponds to the DFS root or the DFS link for which 
				 * target information is returned. This path MAY either be the same 
				 * as the path as pointed to by the DFSPathOffset field or be an 
				 * 8.3 name. In the former case, the string referenced MAY be the 
				 * same as that in the DFSPathOffset field or a duplicate copy.
				 */
				error = md_get_uint16le(mdp, &Offset);
				/*
				 * [APPLE]
				 * In some cases a zero offset is ok, so if we get a zero offset
				 * treat it like we got a string of zero length. In this case we
				 * don't even use this string, in the future may want to skip
				 * getting it. For now get it but ignore any errors.
				 */
				if (!error && Offset) {
					(void)addDfsStringToDictionary(inConn, &md_referral_shadow, Offset, NULL, 
										  NULL, referralInfo, kDFSAlternatePath);
				}
				if (error) {
					smb_log_info("%s: Getting referral %d's DFS alternate path failed, syserr = %s", 
								 ASL_LEVEL_ERR, __FUNCTION__, ii, strerror(error));
					CFRelease(referralInfo);
					break;
				}
				ReferralSize -= 2;
				addNumberToDictionary(referralInfo, kDFSAlternatePathOffset, kCFNumberSInt16Type, &Offset);
				
				/*
				 * [MS-DFSC]
				 * NetworkAddressOffset (2 bytes): A 16-bit integer indicating the 
				 * offset, in bytes, from beginning of this referral entry to the 
				 * DFS target path that correspond to this entry.
				 */			
				error = md_get_uint16le(mdp, &Offset);
				/*
				 * [APPLE]
				 * In some cases a zero offset is ok, so if we get a zero offset
				 * treat it like we got a string of zero length. In this case is
				 * it ok?
				 */
				if (!error && Offset) {
					error = addDfsStringToDictionary(inConn, &md_referral_shadow, Offset, NULL, 
											NULL, referralInfo, kNetworkAddress);
				}
				if (error) {
					smb_log_info("%s: Getting referral %d's network address offset failed, syserr = %s", 
								 ASL_LEVEL_ERR, __FUNCTION__, ii, strerror(error));
					CFRelease(referralInfo);
					break;
				}
				ReferralSize -= 2;
				addNumberToDictionary(referralInfo, kNetworkAddressOffset, kCFNumberSInt16Type, &Offset);
				if (VersionNumber != DFS_REFERRAL_V2) {
					/*
					 * [MS-DFSC]
					 * ServiceSiteGuid(16bytes): These 16 bytes MUST always be set 
					 * to 0 by the server and ignored by the client. For historical 
					 * reasons, this field was defined in early implementations but 
					 * never used.
					 */
					error = md_get_mem(mdp, NULL, 16, MB_MSYSTEM);
					ReferralSize -= 16;
				}
				if (ReferralSize) {
					/* 
					 * What should we do if we still have a reveral size. For now
					 * lets log it and consume it. We don't treat that as an error.
					 */
					smb_log_info("%s: Referral %d's has a left over size of %d, syserr = %s", 
								 ASL_LEVEL_DEBUG, __FUNCTION__, ii, ReferralSize, strerror(error));
					error = md_get_mem(mdp, NULL, ReferralSize, MB_MSYSTEM);
					ReferralSize = 0;
				}
				unconsumedPath = getUnconsumedPath(ConsumedSize, dfsReferral, strlen(dfsReferral));
			}
		} else {
			error = EPROTO;	// We do not understand this referral
			smb_log_info("%s: Referral %d's UNKNOWN VERSION, syserr = %s",  
						 ASL_LEVEL_ERR, __FUNCTION__, ii, strerror(error));
		}
		
		if (unconsumedPath) {
			addCStringToDictionary(referralInfo, kUnconsumedPath, unconsumedPath);
			free(unconsumedPath);
			unconsumedPath = NULL;
		}
		
		if (error) {
			CFRelease( referralInfo );
			break;
		} else {
			CFMutableStringRef newReferralStr = NULL;
			CFStringRef ntwrkPath = CFDictionaryGetValue(referralInfo, kNetworkAddress);
			CFStringRef unconsumedPath = CFDictionaryGetValue(referralInfo, kUnconsumedPath);
			if (ntwrkPath) {
				newReferralStr = CFStringCreateMutableCopy(NULL, 1024, ntwrkPath);
			}
			if (newReferralStr && unconsumedPath) {
				CFStringAppend(newReferralStr, unconsumedPath);
			}
			if (newReferralStr) {
				CFDictionarySetValue(referralInfo, kNewReferral, newReferralStr );
				CFRelease(newReferralStr);
			}
		}
		CFArrayAppendValue( referralList, referralInfo);
		CFRelease( referralInfo );
	}	// end of for loop decoding referrals
	if( error == 0 && referralDict != NULL )
		*outReferralDict = referralDict;

done:
	/* referralDict holds a reference so we need to release our reference */
	if (referralList)
		CFRelease(referralList);
	if( error ) {
		if (referralDict)
			CFRelease(referralDict);
		*outReferralDict = NULL;
	}
	return error;
}

static int getDfsReferralDict(struct smb_ctx * inConn, CFStringRef referralStr, 
									   uint16_t maxReferralVersion, 
									   CFMutableDictionaryRef *outReferralDict)
{
	struct mbchain	mbp;
	struct mdchain	mdp;
	size_t len;
	uint16_t setup = SMB_TRANS2_GET_DFS_REFERRAL;	/* Kernel swaps setup data */
	void *tparam; 
	int error;
	uint16_t rparamcnt = 0, rdatacnt;
	uint32_t bufferOverflow = 1;
	void *rdata;
	CFMutableDictionaryRef referralDict = NULL;
	char * referral;
	
	referral = CStringCreateWithCFString(referralStr);
	if (!referral)
		return ENOMEM;

	mb_init(&mbp);
	md_init(&mdp);
	mb_put_uint16le(&mbp, maxReferralVersion);
	error = smb_rq_put_dstring(inConn, &mbp, referral, strlen(referral), 
							   SMB_UTF_SFM_CONVERSIONS, &len);
	/* We default buffer overflow to true so we enter the loop once. */ 
	while (bufferOverflow && (error == 0)) 
	{
		bufferOverflow = 0;
		rdatacnt = (uint16_t)mbuf_maxlen(mdp.md_top);
		tparam = mbuf_data(mbp.mb_top);
		rdata =  mbuf_data(mdp.md_top);
		error =  smb_t2_request(inConn, 1, &setup, NULL, (int32_t)mbuf_len(mbp.mb_top), 
								tparam, 0, NULL, &rparamcnt, NULL, &rdatacnt, 
								rdata, &bufferOverflow);
		/*
		 * [MS-DFSC]
		 * The buffer size used by Windows DFS clients for all DFS referral 
		 * requests (domain, DC, DFS root, DFS link and SYSVOL) is 8 KB. Windows 
		 * DFS clients retry on STATUS_BUFFER_OVERFLOW (0x80000005) by doubling 
		 * the buffer size up to a maximum of 56 KB.
		 */
		if ((error == 0) && bufferOverflow) {
			size_t newSize;
			/* smb_t2_request sets rdatacnt to zero, get the old length again */
			newSize = mbuf_maxlen(mdp.md_top);
			if (newSize >= MAX_DFS_REFFERAL_SIZE) {
				error = EMSGSIZE;
			} else {
				newSize *= 2;
				if (newSize > MAX_DFS_REFFERAL_SIZE)
					newSize = MAX_DFS_REFFERAL_SIZE;
				md_done(&mdp);
				md_init_rcvsize(&mdp, newSize);
				smb_log_info("%s retrying with receive buffer size of %ld \n", 
							 ASL_LEVEL_DEBUG, __FUNCTION__, newSize);
			}
		}
	}
	
	if (!error) {
#ifdef SMB_DFS_DEBUG
		smb_ctx_hexdump(__FUNCTION__, "rdatacnt buffer: ", (u_char *)rdata, rdatacnt);
#endif // SMB_DFS_DEBUG
		mbuf_setlen(mdp.md_top, rdatacnt);
		error = decodeDfsReferral(inConn, &mdp, referral, &referralDict);
	}
	if (error) {
		smb_log_info("%s failed, syserr = %s", ASL_LEVEL_DEBUG, __FUNCTION__, strerror(error));
	}
	mb_done(&mbp);
	md_done(&mdp);
	*outReferralDict = referralDict;
	if (referral)
		free(referral);
	return error;
}

#ifdef SMB_DEBUG
/*
 * Debug Code used to test getting all versions of a referral.
 */
int testGettingDfsReferralDict(struct smb_ctx * inConn, const char *referral)
{
	int error;
	int ii;
	CFMutableDictionaryRef referralDict = NULL;
	CFStringRef referralStr = CFStringCreateWithCString(NULL, referral, kCFStringEncodingUTF8);
	
	if (referralStr == NULL)
		return ENOMEM;
	
	for (ii=DFS_REFERRAL_V1; ii <= DFS_REFERRAL_V4; ii++) {
		error = getDfsReferralDict(inConn, referralStr, ii, &referralDict);
		if (error) {
			smb_log_info("%s DFS_REFERRAL_V%d failed, syserr = %s", 
						 ASL_LEVEL_ERR, __FUNCTION__, ii, strerror(error));
		}
		if (referralDict) {
			CFShow(referralDict);
			CFRelease(referralDict);
		}
	}
	CFRelease(referralStr);
	return error;
}
#endif // SMB_DEBUG

/*
 * connectToReferral
 * 
 * Take the connection handle passed in and clone it for an secuiry or local
 * info needed. Now convert the referral into a URL and make a connect to that
 * URL.
 */
static int connectToReferral(struct smb_ctx * inConn, struct smb_ctx ** outConn, 
							 CFStringRef referralStr, CFURLRef referralURL)
{
	CFDictionaryRef serverParams = NULL;
	struct smb_ctx * newConn = NULL;
	CFMutableDictionaryRef openOptions = NULL;
	CFURLRef url = NULL;
	int error = ENOMEM;
		
    if (referralURL) {
        url = CFURLCopyAbsoluteURL(referralURL);
    } else if (referralStr) {
        url = CreateURLFromReferral(referralStr);
    }
	if (url == NULL) {
		smb_log_info("%s creating url failed, syserr = %s", ASL_LEVEL_ERR, 
					 __FUNCTION__, strerror(error));
		goto done;
	}

	newConn = create_smb_ctx();
	if (newConn == NULL) {
		smb_log_info("%s creating newConn failed, syserr = %s", ASL_LEVEL_ERR, 
					 __FUNCTION__, strerror(error));
		goto done;
	}
	/*
	 * Create and set the default open options, these are used by the server
	 * info and open session. The clone ctx will up date these options for
	 * the authentication method being used.
	 */
	openOptions = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, 
											&kCFTypeDictionaryKeyCallBacks, 
											&kCFTypeDictionaryValueCallBacks);
	if (!openOptions) {
		smb_log_info("%s creating openOptions failed, syserr = %s", ASL_LEVEL_ERR, 
					 __FUNCTION__, strerror(error));
		goto done;
	}
	
	if (inConn->ct_setup.ioc_userflags & SMBV_HOME_ACCESS_OK) {
		CFDictionarySetValue(openOptions, kNetFSNoUserPreferencesKey, kCFBooleanFalse);
	} else {
		CFDictionarySetValue(openOptions, kNetFSNoUserPreferencesKey, kCFBooleanTrue);
	}
	/* 
	 * If they have a loopback in the referral we always allow it, no way for
	 * us to decided what is correct at this point.
	 */
	CFDictionarySetValue(openOptions, kNetFSAllowLoopbackKey, kCFBooleanTrue);
	
	/*
	 * Do a get server info call here, this will make sure we have a server
	 * name and address. We need this information before cloning the ctx.
	 */
	error = smb_get_server_info(newConn, url, openOptions, &serverParams);
	if (error) {
		smb_log_info("%s get server info to %s failed, syserr = %s", ASL_LEVEL_DEBUG, 
					 __FUNCTION__, newConn->serverName, strerror(error));
		goto done;
	}
	if (serverParams) {
		CFRelease(serverParams);
	}

	error = smb_ctx_clone(newConn, inConn, openOptions);
	if (error) {
		smb_log_info("%s clone failed, syserr = %s", ASL_LEVEL_ERR, 
					 __FUNCTION__, strerror(error));
		goto done;
	}
	error = smb_open_session(newConn, NULL, openOptions, NULL);
	if (error) {
		smb_log_info("%s open session to %s failed, syserr = %s", ASL_LEVEL_DEBUG, 
					 __FUNCTION__, newConn->serverName, strerror(error));
		goto done;
	}
	
done:
	if (error) {
		smb_ctx_done(newConn);	
	} else {
		*outConn = newConn;
	}
	if (url) {
		CFRelease(url);
	}
	if (openOptions) {
		CFRelease(openOptions);
	}
	return error;
}

/*
 * getDfsReferralDictFromReferral
 *
 * Connect to the referral passed in and then retrieve the new referral 
 * dictionary information from that server. On sucess we will have a connection
 * to that server and a referral dictionary.
 */
static int getDfsReferralDictFromReferral(struct smb_ctx * inConn, 
										  struct smb_ctx ** outConn,
										  CFStringRef referralStr, 
										  CFMutableDictionaryRef *outReferralDict)
{
	struct smb_ctx *newConn = NULL;
	int error;
	
	*outReferralDict = NULL;
	error = connectToReferral(inConn, &newConn, referralStr, NULL);
	if (error) {
		smb_log_info("%s connectToReferral failed, syserr = %s", 
					 ASL_LEVEL_DEBUG, __FUNCTION__, strerror(error));
		goto done;
	}
	smb_ctx_setshare(newConn, "IPC$");
	error = smb_share_connect(newConn);
	if (error) {
		smb_log_info("%s share connect failed, syserr = %s", 
					 ASL_LEVEL_DEBUG, __FUNCTION__, strerror(error));
		goto done;
	}
	
	error = getDfsReferralDict(newConn, referralStr, DFS_REFERRAL_V4, outReferralDict);
	if (error) {
		smb_log_info("%s DFS_REFERRAL_V4 failed, syserr = %s", 
					 ASL_LEVEL_DEBUG, __FUNCTION__, strerror(error));
		goto done;
	}
	(void)smb_share_disconnect(newConn);

done:
	if (error)
		smb_ctx_done(newConn);	
	else
		*outConn = newConn;
	
	return error;
}

/*
 * Build the referral from the smb context structure that was passed in.
 *
 * Referral should be in the following format:
 *		/server/share/path
 *
 */
static CFStringRef createReferralStringFromCString(const char *inStr)
{
	CFMutableStringRef referralStr;
	
	if (inStr == NULL) {
		return NULL;
	}
    referralStr = CFStringCreateMutableCopy(NULL, 0, CFSTR("/"));
	if (referralStr == NULL) {
		return NULL;
	}
	CFStringAppendCString(referralStr, inStr, kCFStringEncodingUTF8);
	return referralStr;
}

/*
 * Build the referral from the smb context structure that was passed in.
 *
 * Referral should be in the following format:
 *		/server/share/path
 *
 */
static CFStringRef createReferralString(struct smb_ctx *inConn, int shareOnly)
{
	CFMutableStringRef referralStr;
	
	if (inConn->serverName == NULL) {
		return NULL;
	}
    referralStr = CFStringCreateMutableCopy(NULL, 0, CFSTR("/"));
	if (referralStr == NULL) {
		return NULL;
	}
	CFStringAppendCString(referralStr, inConn->serverName, kCFStringEncodingUTF8);
	if (inConn->ct_origshare) {
		CFStringAppendCString(referralStr, "/", kCFStringEncodingUTF8);
		CFStringAppendCString(referralStr, inConn->ct_origshare, kCFStringEncodingUTF8);
	}
	if (inConn->mountPath && !shareOnly) {
		CFStringAppendCString(referralStr, "/", kCFStringEncodingUTF8);
		CFStringAppend(referralStr, inConn->mountPath);
	}
	return referralStr;
}

static int processDfsReferralDictionary(struct smb_ctx * inConn, 
										struct smb_ctx ** outConn,
										CFStringRef inReferralStr, int *loopCnt,
										CFMutableArrayRef dfsReferralDictArray)
{
	CFMutableDictionaryRef referralDict = NULL;
	uint32_t ii, referralHeaderFlags;
	uint16_t numberOfReferrals;
	CFArrayRef referralList;
	int error = 0;
	struct smb_ctx *tmpConn = NULL;
	
	if (*loopCnt > MAX_LOOP_CNT)
		return EMLINK;
	*loopCnt += 1;
	
	error = getDfsReferralDictFromReferral(inConn, &tmpConn, inReferralStr, &referralDict);
	if (error)
		goto done;
	
	/* If we find no items then return the correct error */ 
	error = ENOENT;
	referralHeaderFlags = uint32FromDictionary(referralDict, kReferralHeaderFlags);
	numberOfReferrals = uint16FromDictionary(referralDict, kNumberOfReferrals);
	referralList = CFDictionaryGetValue( referralDict, kReferralList);
	if (referralList == NULL)
		goto done;
	
	if ((uint16_t)CFArrayGetCount(referralList) < numberOfReferrals)
		numberOfReferrals = (uint16_t)CFArrayGetCount(referralList);
	
	for (ii = 0; ii < numberOfReferrals; ii++) {		
		CFDictionaryRef referralInfo;
		CFStringRef referralStr;
		
		referralInfo = CFArrayGetValueAtIndex(referralList, ii);
		if (referralInfo == NULL)
			continue;
		
		referralStr = CFDictionaryGetValue(referralInfo, kNewReferral);
		if (referralStr == NULL)
			continue;
		
		/* Have a least one storage item in the list see if we can reach it */
		if (referralHeaderFlags & kDFSStorageServer) {
			struct smb_ctx *newConn = NULL;
			
			/* Connect to the server */
			error = connectToReferral(tmpConn, &newConn, referralStr, NULL);
			if (error)
				continue;
			
			/* Connect to the share */
			error = smb_share_connect(newConn);
			if (error) {
				smb_ctx_done(newConn);
				continue;
			}
			/*
			 * The Spec says that if kDFSStorageServer is set in the referralHeaderFlags
			 * then all the referrals are storage. This does not seem to be true
			 * in the real world. So we now always do one more check unless the
			 * server and tree don't support Dfs.
			 */
			if (((newConn->ct_vc_caps & SMB_CAP_DFS) != SMB_CAP_DFS) ||
				(!(smb_tree_conn_optional_support_flags(newConn) & SMB_SHARE_IS_IN_DFS))) {
				/* This must be a storage referral, so we are done */
				*outConn = newConn;
			} else if (isReferralPathNotCovered(newConn, referralStr) ) {
				error = processDfsReferralDictionary(newConn, outConn, referralStr, loopCnt, dfsReferralDictArray);
				smb_ctx_done(newConn);
				if (error)
					continue;
				/* The out connection now contains a storage referral, so we are done */
			} else {
				/* This must be a storage referral, so we are done */
				*outConn = newConn;
			}
			break;
		} else {			
			error = processDfsReferralDictionary(tmpConn, outConn, referralStr, loopCnt, dfsReferralDictArray);
			/* The out connection now contains a storage referral, so we are done */
			if (! error)
				break;
		}
	}
	
done:
	if (referralDict) {
		if (dfsReferralDictArray) {
			CFArrayAppendValue(dfsReferralDictArray, referralDict);
		} 
		CFRelease(referralDict);
	}
	smb_ctx_done(tmpConn);
	return error;
}

static int
getDfsRootDomains(struct smb_ctx * inConn, CFMutableDictionaryRef *domainReferralDict)
{
    struct smb_ctx *newConn = NULL;
    int error = connectToReferral(inConn, &newConn, NULL, inConn->ct_url);
    
    if (error) {
        return error;
    }
	smb_ctx_setshare(newConn, "IPC$");
	error = smb_share_connect(newConn);
	if (error) {
		smb_log_info("%s share connect failed, syserr = %s", 
					 ASL_LEVEL_DEBUG, __FUNCTION__, strerror(error));
		goto done;
	}
    
	error = getDfsReferralDict(newConn, CFSTR(""), DFS_REFERRAL_V4, domainReferralDict);
    
done:
    if (newConn) {
        smb_ctx_done(newConn);
    }	
    return error;
}

static int
getDfsRootServersFromReferralDict(struct smb_ctx * inConn, 
                                  CFMutableDictionaryRef domainReferralDict, 
                                  CFMutableArrayRef domainArray)
{
    uint16_t ii, numRefs = uint16FromDictionary(domainReferralDict, kNumberOfReferrals);
    CFArrayRef  dfsDomainDictArray = CFDictionaryGetValue( domainReferralDict, kReferralList);
    struct smb_ctx *newConn = NULL;
    int error = ENOENT;
    
    if (CFArrayGetCount(dfsDomainDictArray) < numRefs) {
        goto done;
    }
    
    error = connectToReferral(inConn, &newConn, NULL, inConn->ct_url);
    if (error) {
        goto done;
    }
	smb_ctx_setshare(newConn, "IPC$");
	error = smb_share_connect(newConn);
	if (error) {
		smb_log_info("%s share connect failed, syserr = %s", 
					 ASL_LEVEL_DEBUG, __FUNCTION__, strerror(error));
		goto done;
	}
    
    for (ii=0; ii < numRefs; ii++) {
        CFMutableDictionaryRef serverReferralDict = NULL;
        CFDictionaryRef dict = CFArrayGetValueAtIndex(dfsDomainDictArray, ii);
        
        
        error = getDfsReferralDict(newConn, CFDictionaryGetValue( dict, kSpecialName), 
                                   DFS_REFERRAL_V4, &serverReferralDict);
        if (serverReferralDict) {
            if (domainArray) {
                CFArrayAppendValue(domainArray, serverReferralDict);
            } 
            CFRelease(serverReferralDict);
        }
        serverReferralDict = NULL;
    }
    
done:
    if (newConn) {
        smb_ctx_done(newConn);
    }	
    return error;
    
}

static CFMutableArrayRef
getDfsRootServers(struct smb_ctx * inConn, CFStringRef serverReferralStr)
{
    CFMutableArrayRef dcArray = NULL;
    CFMutableDictionaryRef serverReferralDict = NULL;
    struct smb_ctx *newConn = NULL;
    int error = ENOENT;
    
    dcArray = CFArrayCreateMutable(kCFAllocatorSystemDefault, 0, &kCFTypeArrayCallBacks);
    if (!dcArray) {
        return NULL;
    }
    error = connectToReferral(inConn, &newConn, NULL, inConn->ct_url);
    if (error) {
		goto done;
    }
	smb_ctx_setshare(newConn, "IPC$");
	error = smb_share_connect(newConn);
	if (error) {
		smb_log_info("%s share connect failed, syserr = %s", 
					 ASL_LEVEL_DEBUG, __FUNCTION__, strerror(error));
		goto done;
	}
    
    error = getDfsReferralDict(newConn, serverReferralStr, DFS_REFERRAL_V4, &serverReferralDict);
    if (serverReferralDict) {
        CFArrayAppendValue(dcArray, serverReferralDict);
        CFRelease(serverReferralDict);
    }    
done:
    if (newConn) {
        smb_ctx_done(newConn);
    }
	if (error) {
        CFRelease(dcArray);
        dcArray = NULL;
    }
    return dcArray;
}

/* 
 * Given a URL and a server name referral (domain controller name = "/servername")
 * make a new URL that is a copy of the original, but has the server name field
 * replaced by the server name contained inside the serverRefferalStr.
 */
static CFURLRef
createURLAndReplaceServerName(CFURLRef inUrl, CFStringRef serverRefferalStr)
{
    CFMutableDictionaryRef mutableDict = NULL;
    CFURLRef url = NULL;
    CFMutableStringRef serverStr;
    int error;

    /* No domain controller referral string, nothing to do here */
    if (!serverRefferalStr) {
        return NULL;
    }
    /* Now convert it to a server name */
    serverStr = CFStringCreateMutableCopy(NULL, 1024, serverRefferalStr);
    if (!serverStr) {
        return NULL;
    }
    CFStringTrim(serverStr, CFSTR("/"));
    /* Convert the URL into a dictionary that we can work on */
    error = smb_url_to_dictionary(inUrl, (CFDictionaryRef *)&mutableDict);
    if (error) {
		smb_log_info("%s smb_url_to_dictionary failed, syserr = %s", ASL_LEVEL_DEBUG, 
                     __FUNCTION__, strerror(error));
        goto done;
    }
    /* Replace the server name with the new server name */
    CFDictionaryRemoveValue(mutableDict, kNetFSHostKey);
    CFDictionarySetValue (mutableDict, kNetFSHostKey, serverStr);
    /* Now create the URL from the new dictionary */
    error = smb_dictionary_to_url(mutableDict, &url);
    if (error) {
		smb_log_info("%s smb_dictionary_to_url failed, syserr = %s", ASL_LEVEL_DEBUG, 
                     __FUNCTION__, strerror(error));
        goto done;
    }
done:
    if (serverStr) {
        CFRelease(serverStr);
    }
    if (mutableDict) {
        CFRelease(mutableDict);
    }
    return url;
}

/*
 * Given a server name, that we assume contains a domain name, get the list
 * of domain controls for that domain.
 */
static CFArrayRef 
getDomainControlsUsingServerName(struct smb_ctx * inConn, const char *serverName)
{
    CFStringRef serverReferralStr = createReferralStringFromCString(serverName);
    CFMutableArrayRef dfsDCDictArray = (serverReferralStr) ? getDfsRootServers(inConn, serverReferralStr) : NULL;
    CFArrayRef domainControlArray = NULL;
    CFDictionaryRef dict = NULL;
    CFArrayRef array = NULL;
    
    /* We are done with the server referral string */
    if (serverReferralStr) {
        CFRelease(serverReferralStr);
    }
    /*
     * So dfsDCDictArray is an array, but it only contains one dictionary
     * that contains another array that contains on one dictionary.
     */
    if (!dfsDCDictArray || !CFArrayGetCount(dfsDCDictArray)) {
        goto done;
    }
    /* Get the only dictionary item in this array */
    dict = CFArrayGetValueAtIndex(dfsDCDictArray, 0);
    if (dict) {
        /* Now get the array of referrals, but in our case we only have one */
        array = CFDictionaryGetValue(dict, kReferralList);
        dict = NULL; /* Done with it for now */
    }
    if (array && CFArrayGetCount(array)) {
        /* Now get the only dictionary in this array */
        dict = CFArrayGetValueAtIndex(array, 0);
    }
    if (dict) {
        /* Now lets get the array of domain controllers */ 
        domainControlArray = CFDictionaryGetValue(dict, kExpandedNameArray);
    }
done:
    if (domainControlArray) {
        CFRetain(domainControlArray);
        // return CFArrayCreateCopy(kCFAllocatorDefault, domainControlArray);
    }
    if (dfsDCDictArray) {
        CFRelease(dfsDCDictArray);
    }
    return domainControlArray;
}

/*
 * Check to see we are trying to mount a Dfs Referral.
 *
 * XXX - <rdar://problem/9557505>
 * This routine needs to be rewritten now that we have discovered how domain
 * and standalone referrals really work.
 */
int checkForDfsReferral(struct smb_ctx * inConn, struct smb_ctx ** outConn, char *tmscheme)
{
	CFStringRef referralStr = NULL;
	int error = 0;
	int loopCnt = 0;
	struct smb_ctx *newConn = NULL;
    
	*outConn = NULL;
    error = smb_share_connect(inConn);
	/* 
	 * At this point we expect the connection to have an authenticated connection
	 * to the server and a tree connect to the share.
	 */
	if ((inConn->ct_vc_caps & SMB_CAP_DFS) != SMB_CAP_DFS) {
		/* Server doesn't understand Dfs so we are done */
        if (error) {
            /* Tree connect failed, log it and return the error */
            smb_log_info("%s: smb_share_connect failed, syserr = %s", 
                         ASL_LEVEL_DEBUG, __FUNCTION__, strerror(error));
            if (tmscheme) {
                NetFSLogToMessageTracer(tmscheme, "share connect in SMB_Mount", error);
            }
        }
        return error;
	}
	
	/* 
	 * If we have an error then at this point we either have a DFS referral or 
	 * we have a bad share. See if its a Dfs Referral.
	 */
	if (error) {
		/* Grab the referral */
		referralStr = createReferralString(inConn, FALSE);
		if (referralStr == NULL) {
			goto done;	/* Nothing to do here */
		}
		error = processDfsReferralDictionary(inConn, outConn, referralStr, &loopCnt, NULL);
		if (! error) {
			/* It worked we are done */
			goto done;
		}
		/* Failed cleanup and continue */
		loopCnt = 0;
		if (referralStr) {
			CFRelease(referralStr);
			referralStr = NULL;
		}
	}

	/* 
	 * Couldn't find the referral the normal method, see if we need to
	 * resolving the DFS Root first. Then use that connection to find the
	 * real path.
	 */
	if (error) {
		struct smb_ctx *rootDfsConn = NULL;

		/* Grab the dfs root referral only */
		referralStr = createReferralString(inConn, TRUE);
		if (referralStr == NULL) {
			goto done;	/* Nothing to do here */
		}
		/* See if we can resolve the DFS root referral */
		error = processDfsReferralDictionary(inConn, &rootDfsConn, referralStr, &loopCnt, NULL);
		CFRelease(referralStr);
		referralStr = NULL;

		if (!error) {
			CFURLRef url = createURLAndReplaceServerName(inConn->ct_url, rootDfsConn->serverNameRef);
			struct smb_ctx *tmpConn = NULL;
		
            error = connectToReferral(inConn, &tmpConn, NULL, url);
            CFRelease(url); /* Done with the URL release it */
			
			if (!error) {
				/* Grab the full referral now */
				referralStr = createReferralString(tmpConn, FALSE);
				if (referralStr == NULL) {
					goto done;	/* Nothing to do here */
				}
				error = processDfsReferralDictionary(tmpConn, outConn, referralStr, &loopCnt, NULL);
			}
			smb_ctx_done(tmpConn);
		}
		smb_ctx_done(rootDfsConn);
		if (! error) {
			/* It worked we are done */
			goto done;
		}
		/* Failed cleanup and continue */
		loopCnt = 0;
		if (referralStr) {
			CFRelease(referralStr);
			referralStr = NULL;
		}
	}
	
	/* Everything else failed, see if they gave us a domain name as the server name */
    if (error) {
        int dcerror = 0;
        CFArrayRef domainControllerArray = getDomainControlsUsingServerName(inConn, inConn->serverName);
        CFIndex ii, count = (domainControllerArray) ? CFArrayGetCount(domainControllerArray) : 0;
       
        for (ii=0; ii < count; ii++) {
            CFURLRef url = createURLAndReplaceServerName(inConn->ct_url, CFArrayGetValueAtIndex(domainControllerArray, ii));
            
            if (!url) {
                continue;   /* No URL, see if we can find another entry */
            }
            dcerror = connectToReferral(inConn, &newConn, NULL, url);
            CFRelease(url); /* Done with the URL free it */
            if (dcerror) {
                continue;   /* Connection failed, try the next entry */
            }
            dcerror = smb_share_connect(newConn);
            if (dcerror) {
                smb_ctx_done(newConn);
                newConn = NULL;
                continue;   /* Tree connection failed, try the next entry */
            }
            inConn = newConn;   /* We need to use this one */
            error = 0;          /* Clear out the error */
            break;
        }
        if (domainControllerArray) {
            CFRelease(domainControllerArray);
        }
        if (error) {
            goto done;  /* Couldn't find one, just fail */ 
        }
    }
	
	/*
	 * We either didn't get an error or we fail to resolve it using the other
	 * methods. So try the old try and true method.
	 */
	if ((smb_tree_conn_optional_support_flags(inConn) & SMB_SHARE_IS_IN_DFS) != SMB_SHARE_IS_IN_DFS) {
		/* Share doesn't understand Dfs so we are done */
		goto done;
	}
	referralStr = createReferralString(inConn, FALSE);
	if (referralStr == NULL) {
		goto done;	/* Nothing to do here */
	}
	if (! isReferralPathNotCovered(inConn, referralStr)) {
		goto done;	/* Nothing to do here */
	}
	error = processDfsReferralDictionary(inConn, outConn, referralStr, &loopCnt, NULL);
	if (error) {
		smb_log_info("%s failed, syserr = %s", ASL_LEVEL_DEBUG, __FUNCTION__, strerror(error));
	}
	
done:
    if (newConn) {
		smb_ctx_done(newConn);
    }
	if (referralStr) {
		CFRelease(referralStr);
	}
    if (error) {
        smb_log_info("%s: mounting dfs url failed, syserr = %s", ASL_LEVEL_ERR,
                     __FUNCTION__, strerror(error));
        if (tmscheme) {
            NetFSLogToMessageTracer(tmscheme, "dfs url mount in SMB_Mount", error);
        }

    }
	return error;
	
}

/*
 * Check to see we are trying to mount a Dfs Referral.
 */
int 
getDfsReferralList(struct smb_ctx * inConn, CFMutableDictionaryRef dfsReferralDict)
{
	struct smb_ctx * outConn = NULL;
	struct smb_ctx * newConn = NULL;
	CFStringRef referralStr = NULL;
	int error = 0;
	int loopCnt = 0;
    CFMutableArrayRef dfsServerDictArray = NULL;
    CFMutableArrayRef dfsReferralDictArray = NULL;
    CFArrayRef domainControllerArray = NULL;
    CFIndex ii, count;
    
	/* 
	 * At this point we expect the connection to have an authenticated connection
	 * to the server and a tree connect to the share.
	 */
	if ((inConn->ct_vc_caps & SMB_CAP_DFS) != SMB_CAP_DFS) {
		/* Server doesn't understand Dfs so we are done */
		goto done;
	}
    dfsServerDictArray = CFArrayCreateMutable(kCFAllocatorSystemDefault, 0, &kCFTypeArrayCallBacks);
    if (dfsServerDictArray) {
        CFMutableDictionaryRef domainReferralDict = NULL;
        
        error = getDfsRootDomains(inConn, &domainReferralDict);
        if (!error && domainReferralDict) {
            (void)getDfsRootServersFromReferralDict(inConn, domainReferralDict, dfsServerDictArray);
            CFRelease(domainReferralDict);
       }
        if (CFArrayGetCount(dfsServerDictArray)) {
            CFDictionarySetValue(dfsReferralDict, kDfsServerArray, dfsServerDictArray);
        }
        CFRelease(dfsServerDictArray);
		dfsServerDictArray = NULL;
    }
    
    /* 
     * Check to see if the server name is a domain name. If we got a domain name
     * see if we can find a server that knows about this Dfs Root.
     */
    domainControllerArray = getDomainControlsUsingServerName(inConn, inConn->serverName);
    count = (domainControllerArray) ? CFArrayGetCount(domainControllerArray) : 0;
    
    for (ii=0; ii < count; ii++) {
        CFURLRef url = createURLAndReplaceServerName(inConn->ct_url, CFArrayGetValueAtIndex(domainControllerArray, ii));;
        
        if (!url) {
            continue;   /* No URL, see if we can find another entry */
        }
        error = connectToReferral(inConn, &newConn, NULL, url);
        CFRelease(url); /* Done with the URL free it */
        if (error) {
            continue;   /* Connection failed, try the next entry */
        }
        error = smb_share_connect(newConn);
        if (error) {
            smb_ctx_done(newConn);
            newConn = NULL;
            continue;   /* Tree connection failed, try the next entry */
        }
		if (newConn) {
			inConn = newConn;   /* We need to use this one */
		}
        break;
    }
    if (domainControllerArray) {
        CFRelease(domainControllerArray);
    }
    
    /* If we have a newConn then we already did the tree connect */
    if (newConn == NULL) {
        error = smb_share_connect(inConn);
		/* Tree connect failed try IPC$ */
		if (error) {
			error = smb_ctx_setshare(inConn, "IPC$");
			if (!error) {
				error = smb_share_connect(inConn);
			}
			/* Set the share name back */
			(void)ParseSMBURL(inConn);
		}
    }
	if (!(smb_tree_conn_optional_support_flags(inConn) & SMB_SHARE_IS_IN_DFS)) {
        smb_log_info("%s: %s is not a Dfs Share?", ASL_LEVEL_DEBUG, __FUNCTION__, inConn->ct_sh.ioc_share);
    }
    	
    referralStr = createReferralString(inConn, FALSE);
	if (! isReferralPathNotCovered(inConn, referralStr)) {
        char *referral = CStringCreateWithCFString(referralStr);

        smb_log_info("%s: %s is not covered?", ASL_LEVEL_DEBUG, __FUNCTION__, (referral) ? referral : "");
        if (referral) {
            free(referral);
        }
	}
    dfsReferralDictArray = CFArrayCreateMutable(kCFAllocatorSystemDefault, 0, &kCFTypeArrayCallBacks);
	if (!dfsReferralDictArray) {
		goto done;
	}
      
	if (referralStr) {
		error = processDfsReferralDictionary(inConn, &outConn, referralStr, 
											 &loopCnt, dfsReferralDictArray);
		/* Well that didn't work see if we can use the dfs root to find it */
		if (error) {
			struct smb_ctx *rootDfsConn = NULL;
			CFStringRef rootReferralStr = NULL;
			
			/* Grab the dfs root referral only */
			rootReferralStr = createReferralString(inConn, TRUE);
			if (rootReferralStr == NULL) {
				goto done;	/* Nothing to do here */
			}
			/* See if we can resolve the DFS root referral */
			error = processDfsReferralDictionary(inConn, &rootDfsConn, rootReferralStr, &loopCnt, NULL);
			CFRelease(rootReferralStr);
			
			if (!error) {
				CFURLRef url = createURLAndReplaceServerName(inConn->ct_url, rootDfsConn->serverNameRef);
				struct smb_ctx *tmpConn = NULL;
				
				error = connectToReferral(inConn, &tmpConn, NULL, url);
				CFRelease(url); /* Done with the URL release it */
				
				if (!error) {
					/* Grab the full referral now */
					referralStr = createReferralString(tmpConn, FALSE);
					if (referralStr == NULL) {
						goto done;	/* Nothing to do here */
					}
					error = processDfsReferralDictionary(tmpConn, &outConn, referralStr, &loopCnt, dfsReferralDictArray);
				}
				smb_ctx_done(tmpConn);
			}
			smb_ctx_done(rootDfsConn);			
		}
		CFRelease(referralStr);
	}
	if (CFArrayGetCount(dfsReferralDictArray)) {
		CFDictionarySetValue(dfsReferralDict, kDfsReferralArray, dfsReferralDictArray);
	}

	if (error) {
		smb_log_info("%s failed, syserr = %s", ASL_LEVEL_DEBUG, __FUNCTION__, 
					strerror(error));
	}

done:
	if (dfsReferralDictArray) {
		CFRelease(dfsReferralDictArray);
	}
    if (newConn) {
		smb_ctx_done(newConn);
    }
	if (outConn) {
		smb_ctx_done(outConn);
	}
	return error;
}
