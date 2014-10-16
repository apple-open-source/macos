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
 * namespace to access files and folders without knowledge of the names of  
 * individual file servers and shares that host the data.
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

#include "smbclient.h"
#include <netsmb/smbio.h>
#include <netsmb/smbio_2.h>
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
#include <netsmb/smb_lib.h>

#define MAX_DFS_REFFERAL_SIZE 56 * 1024
#define REFERRAL_ENTRY_HEADER_SIZE	8

/*
 * [MS-DFSC]
 * ReferralHeaderFlags (4 bytes): A 32 bit field representing as eries of flags
 * that are combined by using the bitwise OR operation. Only the R, S, and T  
 * bits are defined and used. The other bits MUST be set to 0 by the server and  
 * ignored upon receipt by the client.
 *
 * Value		Meaning
 * 0x00000001	R (ReferralServers): The R bit MUST be set to 1 if all of the  
 *				targets in the referral entries returned are DFS root targets  
 *				capable of handling DFS referral requests and set to 0 
 *              otherwise.
 * 0x00000002	S (StorageServers): The S bit MUST be set to 1 if all of the  
 *				targets in the referral response can be accessed without  
 *				requiring further referral requests and set to 0 otherwise.
 * 0x00000004	T (TargetFailback): The T bit MUST be set to 1 if DFS client  
 *				target failback is enabled for all targets in this referral  
 *				response. This value MUST be set to 0 by the server and ignored  
 *				by the client for all DFS referral versions except DFS referral
 *				version 4.
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
#define kReferralRequestTime	CFSTR("RequestTime")            /* future use */

/* 
 * GET_DFS_REFERRAL reply header 
 */
#define kPathConsumed			CFSTR("PathConsumed")           /* unused */
#define kNumberOfReferrals		CFSTR("NumberOfReferrals")
#define kReferralHeaderFlags	CFSTR("ReferralHeaderFlags")

/* 
 * GET_DFS_REFERRAL Referral entry 
 */
#define kVersionNumber			CFSTR("VersionNumber")
#define kSize					CFSTR("Size")
#define kServerType				CFSTR("ServerType")             /* unused */
#define kReferralEntryFlags		CFSTR("ReferralEntryFlags")
/* ShareName is only in Version 1 and is same as NetworkAddress */
#define kShareName				kNetworkAddress
/* Proximity is only in Version 2 */
#define	kProximity				CFSTR("Proximity")              /* unused */
#define	kTimeToLive				CFSTR("TimeToLive")             /* unused */

/* kDFSPath shows exactly what was consumed by the server */
#define	kDFSPathOffset			CFSTR("DFSPathOffset")          /* unused */
#define	kDFSPath				CFSTR("DFSPath")                /* unused */

#define	kDFSAlternatePathOffset	CFSTR("DFSAlternatePathOffset") /* unused */
#define	kDFSAlternatePath		CFSTR("DFSAlternatePath")       /* unused */

/* kNewReferral (New referral string) = kNetworkAddress + unconsumed path */
#define	kNetworkAddressOffset	CFSTR("NetworkAddressOffset")   /* unused */
#define	kNetworkAddress			CFSTR("NetworkAddress")

/*
 * When NAME_LIST_REFERRAL is set in ReferralEntryFlags in Version 3 or 4, then
 * instead of getting DFSPath, DFSAlternatePath, NetworkAddress and
 * ServiceSiteGuid, you get back a domain controller information of SpecialName,
 * NumberOfExpandedNames and ExpandedNames
 * 
 * kSpecialName is the Domain Name
 * kNumberOfExpandedNames is number of domain controllers returned
 * kExpandedNameArray is a list of domain controllers
 */
#define	kSpecialNameOffset		CFSTR("SpecialNameOffset")      /* unused */
#define	kSpecialName			CFSTR("SpecialName")            /* unused */
#define	kNumberOfExpandedNames	CFSTR("NumberOfExpandedNames")
#define	kExpandedNameOffset		CFSTR("ExpandedNameOffset")
#define	kExpandedNameArray		CFSTR("ExpandedNameArray")

/*
 * Keys that are filled in by us
 */
#define kNewReferral			CFSTR("NewReferral")
#define	kUnconsumedPath			CFSTR("UnconsumedPath")

/* 
 * Used by smbutil 
 */
#define kDfsServerArray         CFSTR("DfsServerArray")
#define kDfsReferralArray		CFSTR("DfsReferralArray")
#define kDfsADServerArray       CFSTR("DfsADServerArray")


static int smb_get_uint32le(mdchain_t mdp, void **curr_ptr,
                            uint32_t *bytes_unparsed, uint32_t *ret_value)
{
    int error = 0;
    
    if (mdp == NULL) {
        /* SMB 2/3 */
        if (*bytes_unparsed >= 4) {
            *ret_value = letohl(*((uint32_t *) *curr_ptr));
            *curr_ptr = ((uint32_t *) *curr_ptr) + 1;
            *bytes_unparsed -= 4;
        }
        else {
            /* No more bytes left to parse */
            error = EBADRPC;
        }
    }
    else {
        /* SMB 1 */
        error = md_get_uint32le(mdp, ret_value);
    }
    
    return(error);
}

static int smb_get_uint16le(mdchain_t mdp, void **curr_ptr,
                            uint32_t *bytes_unparsed, uint16_t *ret_value)
{
    int error = 0;
    
    if (mdp == NULL) {
        /* SMB 2/3 */
        if (*bytes_unparsed >= 2) {
            *ret_value = letohs(*((uint16_t *) *curr_ptr));
            *curr_ptr = ((uint16_t *) *curr_ptr) + 1;
            *bytes_unparsed -= 2;
        }
        else {
            /* No more bytes left to parse */
            error = EBADRPC;
        }
    }
    else {
        /* SMB 1 */
        error = md_get_uint16le(mdp, ret_value);
    }
    
    return(error);
}

static int smb_get_mem(mdchain_t mdp, void **curr_ptr, uint32_t *bytes_unparsed,
                       caddr_t target, size_t size, int type)
{
    int error = 0;
    
    if (mdp == NULL) {
        /* SMB 2/3 */
        if (*bytes_unparsed >= size) {
            if (target != NULL) {
                bcopy(*curr_ptr, target, size);
            }
            
            *curr_ptr = ((uint8_t *) *curr_ptr) + size;
            *bytes_unparsed -= size;
        }
        else {
            /* No more bytes left to parse */
            error = EBADRPC;
        }
    }
    else {
        /* SMB 1 */
        error = md_get_mem(mdp, target, size, type);
    }
    
    return(error);
}

static size_t smb_get_utf16_strlen(mdchain_t mdp, void **curr_ptr,
                                   uint32_t *bytes_unparsed)
{
	u_char *s = *curr_ptr; /* Points to start of the utf16 string in buffer */
	size_t size = 0;
	size_t max_count, count, ii;
    uint16_t *ustr;

    if (mdp == NULL) {
        /* SMB 2/3 */
		/* Max amount of data we can scan in this mbuf */
		max_count = count = *bytes_unparsed;

		/* Scan the buffer counting the bytes */
		ustr = (uint16_t *)((void *) s);
		for (ii = 0; ii < max_count; ii += 2) {
			if (*ustr++ == 0) {
				/* Found the end we are done */
				goto done;
			}
			size += 2;
		}
    }
    else {
        /* SMB 1 */
        size = md_get_utf16_strlen(mdp);
    }

done:
    return(size);
}

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
	CFStringRef cfStr = CFStringCreateWithCString(nil, str,
                                                  kCFStringEncodingUTF8);
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
	CFStringRef cfStr = CFStringCreateWithCString(nil, str,
                                                  kCFStringEncodingUTF8);
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
static int isReferralPathNotCovered(struct smb_ctx * inConn,
                                    CFStringRef referralStr)
{
	int error, rtValue = FALSE;
	uint32_t ntError;
	char *referral;

	referral = CStringCreateWithCFString(referralStr);
	if (!referral)
		return FALSE;
	
	error = smb2io_check_directory(inConn, referral, SMB_FLAGS2_DFS, &ntError);
	if (error && (ntError == STATUS_PATH_NOT_COVERED)) {
		rtValue = TRUE;
	} else if (error) {
		smb_log_info("%s Dfs Check path on %s return ntstatus = 0x%x, syserr = %s", 
					 ASL_LEVEL_ERR, __FUNCTION__, referral, ntError,
                     strerror(error));
	}
	free(referral);
    
	return rtValue;
}

/*
 * getUnconsumedPathV1
 *
 * This routine is really crude, but we do not expect too many level one 
 * referrals.
 */
static char *getUnconsumedPathV1(struct smb_ctx *inConn, uint16_t pathConsumed, 
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
			smb_log_info("%s PathConsumed (%d) != utf16Len (%ld)",
						 ASL_LEVEL_DEBUG, __FUNCTION__, pathConsumed, utf16Len);
		}
        return NULL;
    }
    
    utf16Str = malloc(utf16Len);
	if (utf16Str == NULL) {
		error = ENOMEM;
	}
    else {
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
		smb_log_info("%s failed, syserr = %s",  ASL_LEVEL_ERR, __FUNCTION__,
                     strerror(error));
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
	 * The original C56 code assume that if PathConsumed was zero or it was not
	 * greater than the referral stings UTF16 length then the whole thing got
	 * consumed. Hard to test to be sure.
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
getUTF8String(struct smb_ctx *inConn, mdchain_t mdp,
              void *curr_ptr, uint32_t *bytes_unparsed,
              uint16_t offset, size_t *utf16Lenp,
              size_t *utf8Lenp, char **outUTF8Str)
{
	struct mdchain mdShadow;
	size_t utf8Len = 0;
	size_t utf16Len = 0;
	char *utf16Str = NULL;
	char *utf8Str = NULL;
	int error = 0;
    void *shadow_ptr = NULL;
    uint32_t shadow_bytes_unparsed = 0;
    mdchain_t shadow_mdp = NULL;
	
    *outUTF8Str = NULL;
	if (utf8Lenp) {
		*utf8Lenp = 0;
	}
    
	if (utf16Lenp) {
		*utf16Lenp = 0;
	}
	
    /* Dont alter the original mdp or curr_ptr */
    if (curr_ptr != NULL) {
        /* SMB 2/3 */
        shadow_ptr = curr_ptr;
        shadow_bytes_unparsed = *bytes_unparsed;
    }
    else {
        /* SMB 1 */
        md_shadow_copy(mdp, &mdShadow);
        shadow_mdp = &mdShadow;
    }

    error = smb_get_mem(shadow_mdp, &shadow_ptr, &shadow_bytes_unparsed, NULL,
                        offset, MB_MSYSTEM);
	if (error) {
		return error;
    }
	
    /* Get length of utf16 string */
    utf16Len = smb_get_utf16_strlen(shadow_mdp, &shadow_ptr,
                                    &shadow_bytes_unparsed);
	if (utf16Len == 0) {
		goto done;
    }
	
    /* Malloc space for utf16 and utf8 buffers */
	utf16Len += 2;	/* Add the two null bytes back on */
	utf8Len = utf16Len * 9;
	utf16Str = malloc(utf16Len);
	if (utf16Str) {
		utf8Str = malloc(utf8Len);
	}
    
	if (utf8Str == NULL) {
		error = ENOMEM;
	}
    else {
        /* Copy the utf16 into utf16Str */
        error = smb_get_mem(shadow_mdp, &shadow_ptr, &shadow_bytes_unparsed,
                            utf16Str, utf16Len, MB_MSYSTEM);
	}
    
	if (error == 0) {
        /* Convert utf16 to a utf8 string */
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
static int addDfsStringToDictionary(struct smb_ctx *inConn, mdchain_t mdp,
                                    void *curr_ptr, uint32_t *bytes_unparsed,
                                    uint16_t offset,
                                    size_t *utf16Lenp, size_t *utf8Lenp,
                                    CFMutableDictionaryRef dict,
                                    CFStringRef key)
{
	char *utf8Str = NULL;
	int error = 0;
    
    error = getUTF8String(inConn, mdp,
                          curr_ptr, bytes_unparsed,
                          offset, utf16Lenp,
                          utf8Lenp, &utf8Str);

	if ((error == 0) && (utf8Str)) {
		addCStringToDictionary(dict, key, utf8Str);
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
 * The C56 code places some of this information into a CFDictionary, which it
 * would use for caching. Not sure we need to cache this info now? For now lets
 * parse everything and put it in the dictionary, we can always change this in
 * the future.
 */
int decodeDfsReferral(struct smb_ctx *inConn, mdchain_t mdp,
                      char *rcv_buffer, uint32_t rcv_buffer_len,
                      const char *dfs_referral_str,
                      CFMutableDictionaryRef *outReferralDict)
{
    uint16_t path_consumed;
    uint16_t number_referrals, ii;
	uint32_t referral_flags;
	CFMutableDictionaryRef referral_dict = NULL;
	CFMutableArrayRef referral_list = NULL;
	int error = 0;
	time_t tnow = time(NULL);
    char *unconsumed_path = NULL;
    uint32_t bytes_unparsed = rcv_buffer_len;
    void *curr_ptr = rcv_buffer;
  
    /*
	 * Create dictionary to return. Contains the referral header and array of 
     * referral entries
	 */
	referral_dict = CFDictionaryCreateMutable(kCFAllocatorSystemDefault, 0,
                                             &kCFTypeDictionaryKeyCallBacks, 
                                             &kCFTypeDictionaryValueCallBacks);
	if (referral_dict) {
        /*
         * Create array of referrals
         */
		referral_list = CFArrayCreateMutable(kCFAllocatorSystemDefault, 0,
											&kCFTypeArrayCallBacks );
		if (referral_list) {
            /*
             * Add empty arrary into dictionary
             */
			CFDictionarySetValue(referral_dict, kReferralList, referral_list);
		}
        else {
            /* Error - failed to create the array */
			CFRelease(referral_dict);
			referral_dict = NULL;
		}
	}
	
	if (!referral_dict) {
        /* Either dictionary or array failed to allocate, so leave */
		error = ENOMEM;
		smb_log_info("%s Dictionary create failed: for referral %s, syserr = %s",  
					 ASL_LEVEL_ERR, __FUNCTION__, dfs_referral_str,
                     strerror(error));
		goto done;
	}
	
    /*
     * Add DFS Referral string into dictionary
     */
	addCStringToDictionary(referral_dict, kRequestFileName, dfs_referral_str);
    
    /*
     * Add current time into dictionary
     */
	addNumberToDictionary(referral_dict, kReferralRequestTime,
                          kCFNumberLongType, &tnow);
	
	/*
	 * [MS-DFSC]
	 * path_consumed (2 bytes): A 16-bit integer indicating the number of
	 * bytes, not characters, in the prefix of the referral request path that is 
	 * matched in the referral response.	
	 */
    error = smb_get_uint16le(mdp, &curr_ptr, &bytes_unparsed, &path_consumed);
	if (error) {
		smb_log_info("%s: Failed to get path consumed, syserr = %s", 
					 ASL_LEVEL_ERR, __FUNCTION__, strerror(error));
		goto done;
	}
    
    /*
     * Add Path Consumed into dictionary
     */
	addNumberToDictionary(referral_dict, kPathConsumed, kCFNumberSInt16Type,
                          &path_consumed);
	
	/*
	 * [MS-DFSC]
	 * number_referrals (2 bytes): A 16-bit integer indicating the number of 
	 * referral entries immediately following the referral header.
	 */
    error = smb_get_uint16le(mdp, &curr_ptr, &bytes_unparsed,
                             &number_referrals);
	if (error) {
		smb_log_info("%s: Failed to get the number of referrals, syserr = %s",
					 ASL_LEVEL_ERR, __FUNCTION__, strerror(error));
		goto done;
	}
    
    /*
     * Add Number of Referrals into dictionary
     */
	addNumberToDictionary(referral_dict, kNumberOfReferrals,
                          kCFNumberSInt16Type, &number_referrals);
	
	/*
	 * [MS-DFSC]
	 * If the referral request is successful, but the number_referrals field in
	 * the referral header (as specified in section 2.2.3) is 0, the DFS server 
	 * could not find suitable targets to return to the client. In this case,  
	 * the client MUST fail the original I/O operation with an  
	 * implementation-defined error.
	 */
    if (!number_referrals) {
        /*
		 * [C56]
		 * This happens when the target of the referral is down or disabled, and 
		 * the referring server (the one returning this info) knows that it is 
		 * down.  In other words, we are being told that that storage is not 
		 * available right now.
		 */
		error = ENOENT;
		smb_log_info("%s number_referrals is zero, syserr = %s",
					 ASL_LEVEL_DEBUG, __FUNCTION__, strerror(error));
		goto done;
    }
	
	/*
	 * [MS-DFSC]
	 * referral_flags (4 bytes): A 32-bit field representing a series of flags
	 * that are combined by using the bitwise OR operation. Only the R, S, and T 
	 * bits are defined and used. The other bits MUST be set to 0 by the server 
	 * and ignored upon receipt by the client.	
	 */
    error = smb_get_uint32le(mdp, &curr_ptr, &bytes_unparsed, &referral_flags);
	if (error) {
		smb_log_info("%s Failed to get the referral header flags, syserr = %s",
					 ASL_LEVEL_ERR, __FUNCTION__, strerror(error));
		goto done;
	}
    
    /*
     * Add Referral Flags into dictionary
     */
	addNumberToDictionary(referral_dict, kReferralHeaderFlags,
                          kCFNumberSInt32Type, &referral_flags);

	/*
	 * [C56]
	 * Only keep the referral flags that we understand
	 */
    referral_flags &= kDFSReferralMask;

	/*
	 * [MS-DFSC]
	 * The DFS: Referral Protocol defines four structures used to encode  
	 * referral entries: DFS_REFERRAL_V1, DFS_REFERRAL_V2, DFS_REFERRAL_V3, and 
     * DFS_REFERRAL_V4.
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
	 * The DFS_REFERRAL_V2, DFS_REFERRAL_V3, and DFS_REFERRAL_V4 structures  
	 * contain fields with offsets to strings. Clients MUST add the string  
	 * offset to the offset of the beginning of the referral entry to find the  
	 * offset of the string in the RESP_GET_DFS_REFERRAL message. The strings  
	 * referenced from the fields of a referral entry MAY immediately follow  
	 * the referral entry structure or MAY follow the last referral entry in  
	 * the RESP_GET_DFS_REFERRAL message. The Size field of a referral entry  
	 * structure MUST include the size in bytes of all immediately following 
     * strings so that a client can find the next referral entry in the message. 
     * The Size field of a referral entry structure MUST NOT include the size 
     * of referenced strings located after the last referral entry in the 
     * message.
	 */	
	
	/*
	 * [APPLE]
	 * The C56 code treated DFS_REFERRAL_V2 the same as DFS_REFERRAL_V3, this
	 * is wrong. In fact the C56 code's DFS_REFERRAL_V2 never worked from what 
	 * I can tell. The DFS_REFERRAL_V3 is also broken, but since they thought
	 * DFS_REFERRAL_V2 and DFS_REFERRAL_V3 where the same it worked out for 
	 * them. It is understandable why they got this wrong since there was no 
	 * spec, but now that we have a spec we should follow it. Once we get this 
	 * code working we should remove this comment. Place holder to when 
	 * comparing the C56 code with my new code.
	 */
	for (ii = 1; ii <= number_referrals; ii++) {
		uint16_t version_number;
		uint16_t referral_size;
		uint16_t server_type;
		uint16_t referral_entry_flags;
		struct mdchain	md_referral_shadow;
		CFMutableDictionaryRef referral_info;
        uint32_t ref_bytes_unparsed = 0;
        void *ref_ptr = NULL;

        /*
         * Several offsets are from the beginning of the referral, so save a 
         * copy of the beginning of the referral.
         */
        if (curr_ptr != NULL) {
            /* SMB 2/3 */
            ref_bytes_unparsed = bytes_unparsed;
            ref_ptr = curr_ptr;
        }
        else {
            /* SMB 1 */
            md_shadow_copy(mdp, &md_referral_shadow);
        }
        
        /*
         * Create a dictionary to hold this Referral's info
         */
		referral_info = CFDictionaryCreateMutable(kCFAllocatorSystemDefault, 0,
												 &kCFTypeDictionaryKeyCallBacks, 
												 &kCFTypeDictionaryValueCallBacks);
		if (!referral_info) {
			error = ENOMEM;
			smb_log_info("%s: Creating referral info failed, syserr = %s",
						 ASL_LEVEL_ERR, __FUNCTION__, strerror(error));
			break;
		}

		/*
		 * [MS-DFSC]
		 * version_number (2 bytes): A 16-bit integer indicating the version
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
        error = smb_get_uint16le(mdp, &curr_ptr, &bytes_unparsed,
                                 &version_number);
		if (error) {
			smb_log_info("%s: Getting referral %d's version failed, syserr = %s", 
						 ASL_LEVEL_ERR, __FUNCTION__, ii, strerror(error));
			CFRelease(referral_info);
			break;
		}
		
        /*
         * Add Version Number into Referral
         */
		addNumberToDictionary(referral_info, kVersionNumber,
                              kCFNumberSInt16Type, &version_number);

		/*
		 * [MS-DFSC]
		 * referral_size (2 bytes): A 16-bit integer indicating the total size  
		 * of the referral entry in bytes.
		 */
        error = smb_get_uint16le(mdp, &curr_ptr, &bytes_unparsed,
                                 &referral_size);
		if (error) {
			smb_log_info("%s: Getting referral %d's size failed, syserr = %s", 
						 ASL_LEVEL_ERR, __FUNCTION__, ii, strerror(error));
			CFRelease(referral_info);
			break;
		}
        
        /*
         * Add Referral Size into Referral
         */
		addNumberToDictionary(referral_info, kSize, kCFNumberSInt16Type,
                              &referral_size);
		
		/* 
         * [APPLE] Make sure the referral_size contains the referral entry
         * header size 
         */
		if (referral_size < REFERRAL_ENTRY_HEADER_SIZE ) {
			error = EPROTO;
			smb_log_info("%s: Bad referral %d's size got %d, syserr = %s", 
						 ASL_LEVEL_ERR, __FUNCTION__, ii, referral_size,
                         strerror(error));
			CFRelease(referral_info);
			break;
		}
        
		/* [APPLE] Now consume the header size */
		referral_size -= REFERRAL_ENTRY_HEADER_SIZE;
		
		/*
		 * [MS-DFSC]
		 * server_type (2 bytes): A 16-bit integer indicating the type of server
		 * hosting the target. This field MUST be set to 0x0001 if DFS root  
		 * targets are returned, and to 0x0000 otherwise. Note that sysvol  
		 * targets are not DFS root targets; the field MUST be set to 0x0000  
		 * for a sysvol referral response.
		 */
        error = smb_get_uint16le(mdp, &curr_ptr, &bytes_unparsed, &server_type);
		if (error) {
			smb_log_info("%s: Getting referral %d's server type failed, syserr = %s", 
						 ASL_LEVEL_ERR, __FUNCTION__, ii, strerror(error));
			CFRelease(referral_info);
			break;
		}		

        /*
         * Add Server Type into Referral
         */
        addNumberToDictionary(referral_info, kServerType, kCFNumberSInt16Type,
                              &server_type);
		
		/*
		 * [MS-DFSC]
		 * For DFS_REFERRAL_V1 and DFS_REFERRAL_V2 the following applies:
		 * referral_entry_flags (2 bytes): A series of bit flags. MUST be set to
		 * 0x0000 and ignored on receipt.
		 * 
		 * For DFS_REFERRAL_V3 and DFS_REFERRAL_V4 the following applies:
		 * referral_entry_flags (2 bytes): A 16-bit field representing a series of
		 * flags that are combined by using the bitwise OR operation. Only the 
		 * N bit is defined for DFS_REFERRAL_V3. The other bits MUST be set to 
		 * 0 by the server and ignored upon receipt by the client.
		 *	Value		Meaning
		 *	N 0x0002	MUST be set for a domain referral response or a DC 
		 *				referral response.
		 */
        error = smb_get_uint16le(mdp, &curr_ptr, &bytes_unparsed,
                                 &referral_entry_flags);
		if (error) {
			smb_log_info("%s: Getting referral %d's entry flags failed, syserr = %s", 
						 ASL_LEVEL_ERR, __FUNCTION__, ii, strerror(error));
			CFRelease(referral_info);
			break;
		}
        
		if ((version_number == DFS_REFERRAL_V1) ||
            (version_number == DFS_REFERRAL_V2)) {
			/* 
			 * [APPLE]
			 * Version two and version three have very similiar layouts. We can
			 * treat the string offsets the same if the referral_entry_flags is
			 * alway zero. Setting it to zero, since with version one and two it 
			 * should be ignored anyways. See above notes.
			 */
			referral_entry_flags = 0;
		}
        else {
            /* Only keep the referral_entry_flags flags we support */
			referral_entry_flags &= NAME_LIST_REFERRAL;
		}
		
        /*
         * Add Flags into Referral
         */
		addNumberToDictionary(referral_info, kReferralEntryFlags,
                              kCFNumberSInt16Type, &referral_entry_flags);
	
		/* 
         * [APPLE] Now it depends on the version number for the rest of the 
         * lay out 
         */
		if (version_number == DFS_REFERRAL_V1) {
            /*
             * Version 1, add Share Name into Referral
             */
			error = addDfsStringToDictionary(inConn, mdp,
                                             curr_ptr, &bytes_unparsed,
                                             0,
                                             NULL, NULL,
                                             referral_info, kShareName);
			if (!error) {
				/* 
                 * [APPLE] Now consume Share Name and any pad bytes 
                 * 
                 * REFERRAL_ENTRY_HEADER_SIZE has already been subtracted from 
                 * referral_size so all that should be left is the Share Name and 
                 * pad bytes
                 */
                error = smb_get_mem(mdp, &curr_ptr, &bytes_unparsed, NULL,
                                    referral_size, MB_MSYSTEM);
			}

			if (error) {
				smb_log_info("%s: Getting referral %d's share name failed, syserr = %s", 
							 ASL_LEVEL_ERR, __FUNCTION__, ii, strerror(error));
				CFRelease(referral_info);
				break;
			}
			unconsumed_path = getUnconsumedPathV1(inConn, path_consumed,
                                                  dfs_referral_str);
		} else if ((version_number == DFS_REFERRAL_V2) ||
				   (version_number == DFS_REFERRAL_V3) ||
				   (version_number == DFS_REFERRAL_V4)) {
			uint32_t proximity, time_to_live;
			uint16_t special_name_offset;
			uint16_t expanded_name_offset;
			uint16_t dfs_path_offset;
			uint16_t dfs_alt_path_offset;
			uint16_t network_addr_offset;
			size_t consumed_size = 0;
			
			if (version_number == DFS_REFERRAL_V2) {
				/*
                 * Version 2 has a Proximity field before the Time To Live field
                 *
				 * [MS-DFSC]
				 * proximity (4 bytes): MUST be set to 0x00000000 by the server
				 * and ignored by the client.
				 */
                error = smb_get_uint32le(mdp, &curr_ptr, &bytes_unparsed,
                                         &proximity);
				if (error) {
					smb_log_info("%s: Getting referral %d's proximity failed, syserr = %s",
								 ASL_LEVEL_ERR, __FUNCTION__, ii,
                                 strerror(error));
					CFRelease(referral_info);
					break;
				}
                
                /*
                 * Version 2, add Proximity into Referral
                 */
				addNumberToDictionary(referral_info, kProximity,
                                      kCFNumberSInt32Type, &proximity);
                
                /* Decrement referral_size */
				referral_size -= 4;
			}
            
			/*
			 * [MS-DFSC]
			 * time_to_live (4 bytes): A 32-bit integer indicating the time-out
			 * value, in seconds, of the DFS root or DFS link. MUST be set to the 
			 * time-out value of the DFS root or the DFS link in the DFS metadata 
			 * for which the referral response is being sent. When there is more 
			 * than one referral entry, the time_to_live of each referral entry
			 * MUST be the same.
			 */
            error = smb_get_uint32le(mdp, &curr_ptr, &bytes_unparsed,
                                     &time_to_live);
			if (error) {
				smb_log_info("%s: Getting referral %d's time to live failed, syserr = %s", 
							 ASL_LEVEL_ERR, __FUNCTION__, ii, strerror(error));
				CFRelease(referral_info);
				break;
			}
            
            /*
             * Version 2, 3, 4 add Time To Live into Referral
             */
			addNumberToDictionary(referral_info, kTimeToLive,
                                  kCFNumberSInt32Type, &time_to_live);

            /* Decrement referral_size */
			referral_size -= 4;
			
			/*
			 * [APPLE]
			 * We only use NAME_LIST_REFERRAL referrals in the case where we
             * are not bound to AD or the AD plugin failed to find us a valid
             * domain controller to talk to.
             *
             * Note: Version 2 will never set NAME_LIST_REFERRAL
			 */
			if (referral_entry_flags & NAME_LIST_REFERRAL) {
                /* NAME_LIST_REFERRAL are a list of Domain Controllers */
				uint16_t num_of_expanded_names;
				
				/*
				 * [MS-DFSC]
				 * special_name_offset (2 bytes): A 16-bit integer indicating  
				 * the offset, in bytes, from the beginning of the referral  
				 * entry to a domain name. For a domain referral response, this  
				 * MUST be the domain name that corresponds to the referral  
				 * entry. For a DC referral response, this MUST be the domain 
                 * name that is specified in the DC referral request.
				 * The domain name MUST be a null-terminated string.			 
				 */
                error = smb_get_uint16le(mdp, &curr_ptr, &bytes_unparsed,
                                         &special_name_offset);
                
				/*
				 * [APPLE]
				 * In some cases a zero offset is ok, so if we get a zero offset
				 * treat it like we got a string of zero length.
                 *
                 * Note: Because the offset is from the beginning of the 
                 * referral we use md_referral_shadow (or ref_ptr) instead.
				 */
				if (!error && special_name_offset) {
					error = addDfsStringToDictionary(inConn, &md_referral_shadow,
                                                     ref_ptr, &ref_bytes_unparsed,
													 special_name_offset, NULL,
                                                     NULL, referral_info,
                                                     kSpecialName);
                }
				
				if (error) {
					smb_log_info("%s: Getting referral %d's special name failed, syserr = %s", 
								 ASL_LEVEL_ERR, __FUNCTION__, ii,
                                 strerror(error));
					CFRelease(referral_info);
					break;
				}
                
                /*
                 * Version 3, 4 add Special Name Offset to Referral
                 */
				addNumberToDictionary(referral_info, kSpecialNameOffset,
                                      kCFNumberSInt16Type,
                                      &special_name_offset);
				
                /* Decrement referral_size */
				referral_size -= 2;

				/*
				 * [MS-DFSC]
				 * num_of_expanded_names (2 bytes): A 16-bit integer indicating 
				 * the number of DCs being returned for a DC referral request.  
				 * MUST be set to 0 for a domain referral response.
				 */
                error = smb_get_uint16le(mdp, &curr_ptr, &bytes_unparsed,
                                         &num_of_expanded_names);
				if (error) {
					smb_log_info("%s: Getting referral %d's number of expaned names failed, syserr = %s", 
								 ASL_LEVEL_ERR, __FUNCTION__, ii,
                                 strerror(error));
					CFRelease(referral_info);
					break;
				}
                
                /*
                 * Version 3, 4 add Number of Expanded Names to Referral
                 */
				addNumberToDictionary(referral_info, kNumberOfExpandedNames,
									  kCFNumberSInt16Type,
                                      &num_of_expanded_names);
				
                /* Decrement referral_size */
				referral_size -= 2;

				/*
				 * [MS-DFSC]
				 * expanded_name_offset (2 bytes): A 16-bit integer indicating 
				 * the offset, in bytes, from the beginning of this referral 
                 * entry to the first DC name string returned in response to a  
				 * DC referral request. If multiple DC name strings are being 
				 * returned in response to a DC referral request, the first DC 
				 * name string must be followed immediately by the additional DC 
				 * name strings. The total number of consecutive name strings 
				 * MUST be equal to the value of the num_of_expanded_names 
                 * field. This field MUST be set to 0 for a domain referral 
				 * response. Each DC name MUST be a null-terminated string.
				 */
                error = smb_get_uint16le(mdp, &curr_ptr, &bytes_unparsed,
                                         &expanded_name_offset);

				/*
				 * [APPLE]
				 * In some cases a zero offset is ok, so if we get a zero offset
				 * treat it like we got a string of zero length.
				 * XXX - Not sure yet how to handle have more than one yet. Need 
				 * to find a test case and then fix this code.
                 *
                 * Currently we just grab the first one.
				 */
				if ((num_of_expanded_names) && expanded_name_offset) {
                    int jj;
                    uint16_t temp_exp_name_offset = expanded_name_offset;
                    char *utf8_str = NULL;
                    size_t utf16_len = 0;
                    CFMutableArrayRef expanded_name_array = CFArrayCreateMutable(kCFAllocatorSystemDefault, 0,
                                                                                 &kCFTypeArrayCallBacks);
                  
                    /* Create array to store expanded names in */
                    if (!expanded_name_array) {
                        smb_log_info("%s: Getting referral %d's failed, internal error CFArrayCreateMutable failed", ASL_LEVEL_ERR, __FUNCTION__, ii);
                        CFRelease(referral_info);
                        break;
                    }
                    
                    for (jj = 0; jj < num_of_expanded_names; jj++) {
                        /*
                        * Note: Because the offset is from the beginning of the 
                        * referral we use md_referral_shadow (or ref_ptr) 
                         * instead.
                         */
                        error = getUTF8String(inConn, &md_referral_shadow,
                                              ref_ptr, &ref_bytes_unparsed,
                                              temp_exp_name_offset, &utf16_len,
                                              NULL, &utf8_str);
                        if (error) {
                            break;
                        }
                        
                        addCStringToCFStringArray(expanded_name_array,
                                                  utf8_str);
                        free(utf8_str);
                        temp_exp_name_offset += utf16_len;
                        utf16_len = 0;
                    }
                    
                    /*
                     * Version 3, 4 add Expanded Names Array to Referral
                     */
                    CFDictionarySetValue(referral_info, kExpandedNameArray, expanded_name_array);
                    CFRelease(expanded_name_array);
				}
                
				if (error) {
					smb_log_info("%s: Getting referral %d's special name failed, syserr = %s", 
								 ASL_LEVEL_ERR, __FUNCTION__, ii,
                                 strerror(error));
                    if (referral_info) {
                        CFRelease(referral_info);
                    }
					break;
				}
                
                /*
                 * Version 3, 4 add Expanded Names Offset to Referral
                 */
				addNumberToDictionary(referral_info, kExpandedNameOffset,
									  kCFNumberSInt16Type,
                                      &expanded_name_offset);

                /* Decrement referral_size */
				referral_size -= 2;

				/*
				 * [MS-DFSC]
				 * Padding (variable): The server MAY insert zero or 16 padding 
				 * bytes that MUST be ignored by the client
				 *
				 * {APPLE] Consume any pad bytes
				 */
				if (referral_size) {
                    error = smb_get_mem(mdp, &curr_ptr, &bytes_unparsed, NULL,
                                        referral_size, MB_MSYSTEM);
					referral_size = 0;
				}
                
				/*
				 * [APPLE]
				 * Not sure how to handle these or if we want to handle them. So
				 * for now lets ignore them and just go to the next element in
				 * the list.
				 */
				unconsumed_path = NULL;
				smb_log_info("%s: Skipping referral %d's NAME_LIST_REFERRAL!", 
							 ASL_LEVEL_DEBUG, __FUNCTION__, ii);
			}
            else {
				/*
                 * NAME_LIST_REFERRAL is NOT set
                 *
				 * [MS-DFSC]
				 * dfs_path_offset (2 bytes): A 16-bit integer indicating the 
                 * offset, in bytes, from the beginning of this referral entry 
                 * to the DFS path that corresponds to the DFS root or DFS link 
                 * for which target information is returned. The DFS path MUST 
                 * be a null-terminated string.
				 */
                error = smb_get_uint16le(mdp, &curr_ptr, &bytes_unparsed,
                                         &dfs_path_offset);

				/*
				 * [APPLE]
				 * In some cases a zero offset is ok, so if we get a zero offset
				 * treat it like we got a string of zero length. In this case is
				 * it ok?
				 */
				if (!error && dfs_path_offset) {
                    /*
                    * Note: Because the offset is from the beginning of the
                    * referral we use md_referral_shadow (or ref_ptr) instead.
                    */
					error = addDfsStringToDictionary(inConn, &md_referral_shadow,
                                                     ref_ptr, &ref_bytes_unparsed,
                                                     dfs_path_offset, NULL,
                                                     &consumed_size,
                                                     referral_info, kDFSPath);
				}
				if (error) {
					smb_log_info("%s: Getting referral %d's DFS path failed, syserr = %s", 
								 ASL_LEVEL_ERR, __FUNCTION__, ii,
                                 strerror(error));
                    if (referral_info) {
                        CFRelease(referral_info);
                    }
					break;
				}
                
                /*
                 * Version 2, 3, 4 add DFS Path Offset to Referral
                 */
				addNumberToDictionary(referral_info, kDFSPathOffset,
                                      kCFNumberSInt16Type, &dfs_path_offset);
				
                /* Decrement referral_size */
				referral_size -= 2;

				/*
				 * [MS-DFSC]
				 * dfs_alt_path_offset (2 bytes): A 16-bit integer indicating  
				 * the offset, in bytes, from the beginning of this referral   
				 * entry to the DFS path that corresponds to the DFS root or the 
				 * DFS link for which target information is returned. This path 
				 * MAY either be the same as the path as pointed to by the 
                 * DFSPathOffset field or be an 8.3 name. In the former case, 
                 * the string referenced MAY be the same as that in the 
                 * DFSPathOffset field or a duplicate copy.
				 */
                error = smb_get_uint16le(mdp, &curr_ptr, &bytes_unparsed,
                                         &dfs_alt_path_offset);

				/*
				 * [APPLE]
				 * In some cases a zero offset is ok, so if we get a zero offset
				 * treat it like we got a string of zero length. In this case we
				 * don't even use this string, in the future may want to skip
				 * getting it. For now get it but ignore any errors.
				 */
				if (!error && dfs_alt_path_offset) {
                    /*
                     * Note: Because the offset is from the beginning of the 
                     * referral we use md_referral_shadow (or ref_ptr) instead.
                     */
					(void)addDfsStringToDictionary(inConn, &md_referral_shadow,
                                                   ref_ptr, &ref_bytes_unparsed,
                                                   dfs_alt_path_offset, NULL,
                                                   NULL, referral_info,
                                                   kDFSAlternatePath);
				}
                
				if (error) {
					smb_log_info("%s: Getting referral %d's DFS alternate path failed, syserr = %s", 
								 ASL_LEVEL_ERR, __FUNCTION__, ii,
                                 strerror(error));
					CFRelease(referral_info);
					break;
				}
                
                /*
                 * Version 2, 3, 4 add DFS Alt Path Offset into Referral
                 */
				addNumberToDictionary(referral_info, kDFSAlternatePathOffset,
                                      kCFNumberSInt16Type, &dfs_alt_path_offset);
				
                /* Decrement referral_size */
				referral_size -= 2;
                
				/*
				 * [MS-DFSC]
				 * network_addr_offset (2 bytes): A 16-bit integer indicating the 
				 * offset, in bytes, from beginning of this referral entry to the 
				 * DFS target path that correspond to this entry.
				 */			
                error = smb_get_uint16le(mdp, &curr_ptr, &bytes_unparsed,
                                         &network_addr_offset);
                
				/*
				 * [APPLE]
				 * In some cases a zero offset is ok, so if we get a zero offset
				 * treat it like we got a string of zero length. In this case is
				 * it ok?
				 */
				if (!error && network_addr_offset) {
                    /*
                     * Note: Because the offset is from the beginning of the 
                     * referral we use md_referral_shadow (or ref_ptr) instead.
                     */
					error = addDfsStringToDictionary(inConn, &md_referral_shadow,
                                                     ref_ptr, &ref_bytes_unparsed,
                                                     network_addr_offset, NULL,
                                                     NULL, referral_info,
                                                     kNetworkAddress);
				}
				if (error) {
					smb_log_info("%s: Getting referral %d's network address offset failed, syserr = %s",
								 ASL_LEVEL_ERR, __FUNCTION__, ii,
                                 strerror(error));
					CFRelease(referral_info);
					break;
				}
                
                /*
                 * Version 2, 3, 4 add Network Address Offset into Referral
                 */
				addNumberToDictionary(referral_info, kNetworkAddressOffset,
                                      kCFNumberSInt16Type, &network_addr_offset);

                /* Decrement referral_size */
				referral_size -= 2;

				if (version_number != DFS_REFERRAL_V2) {
					/*
					 * [MS-DFSC]
					 * Service Site Guid(16 bytes): These 16 bytes MUST always  
					 * be set to 0 by the server and ignored by the client. For  
					 * historical reasons, this field was defined in early 
                     * implementations but never used.
					 */
                    error = smb_get_mem(mdp, &curr_ptr, &bytes_unparsed, NULL,
                                        16, MB_MSYSTEM);

                    /* Decrement referral_size */
					referral_size -= 16;
				}
                
				if (referral_size) {
					/* 
					 * What should we do if we still have a referral size. For
					 * now lets log it and consume it. We don't treat that as 
                     * an error.
					 */
					smb_log_info("%s: Referral %d's has a left over size of %d, syserr = %s", 
								 ASL_LEVEL_DEBUG, __FUNCTION__, ii,
                                 referral_size, strerror(error));
                    error = smb_get_mem(mdp, &curr_ptr, &bytes_unparsed, NULL,
                                        referral_size, MB_MSYSTEM);
					referral_size = 0;
				}
                
				unconsumed_path = getUnconsumedPath(consumed_size,
                                                    dfs_referral_str,
                                                    strlen(dfs_referral_str));
			}
		}
        else {
			error = EPROTO;	/* We do not understand this referral */
			smb_log_info("%s: Referral %d's UNKNOWN VERSION, syserr = %s",  
						 ASL_LEVEL_ERR, __FUNCTION__, ii, strerror(error));
		}
		
        /*
         * Version 1, 2, 3, 4 add any Unconsumed Path to Referral
         */
		if (unconsumed_path) {
			addCStringToDictionary(referral_info, kUnconsumedPath,
                                   unconsumed_path);
			free(unconsumed_path);
			unconsumed_path = NULL;
		}
		
		if (error) {
			CFRelease(referral_info);
			break;
		}
        else {
			CFMutableStringRef new_referral_str = NULL;
			CFStringRef network_path = CFDictionaryGetValue(referral_info,
                                                            kNetworkAddress);
			CFStringRef unconsumed_path = CFDictionaryGetValue(referral_info,
                                                               kUnconsumedPath);

			if (network_path) {
				new_referral_str = CFStringCreateMutableCopy(NULL, 1024,
                                                             network_path);
			}

			if (new_referral_str && unconsumed_path) {
				CFStringAppend(new_referral_str, unconsumed_path);
			}

            /*
             * Version 1, 2, 3, 4 add New Referral String to Referral
             */
			if (new_referral_str) {
				CFDictionarySetValue(referral_info, kNewReferral,
                                     new_referral_str);
				CFRelease(new_referral_str);
			}
		}

        /*
         * Add this Referral to the array of Referrals
         */
		CFArrayAppendValue(referral_list, referral_info);
		CFRelease(referral_info);
	}	// end of for loop decoding referrals
    
    /*
     * If no error, then return the dictionary
     */
	if (error == 0 && referral_dict != NULL) {
		*outReferralDict = referral_dict;
    }

done:
	/* referral_dict holds a reference so we need to release our reference */
	if (referral_list) {
		CFRelease(referral_list);
    }
    
	if (error) {
		if (referral_dict) {
			CFRelease(referral_dict);
        }
		*outReferralDict = NULL;
	}
    
	return error;
}

int getDfsReferralDict(struct smb_ctx * inConn, CFStringRef referralStr,
                       uint16_t maxReferralVersion, 
                       CFMutableDictionaryRef *outReferralDict)
{
	struct mbchain	mbp;
	struct mdchain	mdp;
	size_t len;
	uint16_t setup = SMB_TRANS2_GET_DFS_REFERRAL;  /* Kernel swaps setup data */
	void *tparam; 
	int error;
	uint16_t rparamcnt = 0, rdatacnt;
	uint32_t bufferOverflow = 1;
	void *rdata;
	CFMutableDictionaryRef referralDict = NULL;
	char * referral;
	
	referral = CStringCreateWithCFString(referralStr);
	if (!referral) {
		return ENOMEM;
    }

	mb_init(&mbp);
	md_init(&mdp);
	mb_put_uint16le(&mbp, maxReferralVersion);
	error = smb_usr_rq_put_dstring(inConn, &mbp, referral, strlen(referral),
                                   SMB_UTF_SFM_CONVERSIONS, &len);
    
	/* We default buffer overflow to true so we enter the loop once. */ 
	while (bufferOverflow && (error == 0)) 
	{
		bufferOverflow = 0;
		rdatacnt = (uint16_t)mbuf_maxlen(mdp.md_top);
		tparam = mbuf_data(mbp.mb_top);
		rdata =  mbuf_data(mdp.md_top);
		error =  smb_usr_t2_request(inConn, 1, &setup, NULL,
                                    (int32_t)mbuf_len(mbp.mb_top),
									tparam, 0, NULL, &rparamcnt, NULL,
                                    &rdatacnt, rdata, &bufferOverflow);
		/*
		 * [MS-DFSC]
		 * The buffer size used by Windows DFS clients for all DFS referral 
		 * requests (domain, DC, DFS root, DFS link and SYSVOL) is 8 KB. Windows 
		 * DFS clients retry on STATUS_BUFFER_OVERFLOW (0x80000005) by doubling 
		 * the buffer size up to a maximum of 56 KB.
		 */
		if ((error == 0) && bufferOverflow) {
			size_t newSize;
			/* 
             * smb_usr_t2_request sets rdatacnt to zero, get the old length
             * again 
             */
			newSize = mbuf_maxlen(mdp.md_top);
			if (newSize >= MAX_DFS_REFFERAL_SIZE) {
				error = EMSGSIZE;
			}
            else {
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
		mbuf_setlen(mdp.md_top, rdatacnt);
		error = decodeDfsReferral(inConn, &mdp, NULL, 0, referral,
                                  &referralDict);
	}
	else {
		smb_log_info("%s failed, syserr = %s", ASL_LEVEL_DEBUG, __FUNCTION__,
                     strerror(error));
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
	CFStringRef referralStr = CFStringCreateWithCString(NULL, referral,
                                                        kCFStringEncodingUTF8);
	
	if (referralStr == NULL)
		return ENOMEM;
	
	for (ii = DFS_REFERRAL_V1; ii <= DFS_REFERRAL_V4; ii++) {
		error = smb2io_get_dfs_referral(inConn, referralStr, ii, &referralDict);
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
 * Take the connection handle passed in and clone it for any security or local
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
		smb_log_info("%s creating openOptions failed, syserr = %s",
                     ASL_LEVEL_ERR, __FUNCTION__, strerror(error));
		goto done;
	}
	
	if (inConn->ct_setup.ioc_userflags & SMBV_HOME_ACCESS_OK) {
		CFDictionarySetValue(openOptions, kNetFSNoUserPreferencesKey,
                             kCFBooleanFalse);
	}
    else {
		CFDictionarySetValue(openOptions, kNetFSNoUserPreferencesKey,
                             kCFBooleanTrue);
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
		smb_log_info("%s get server info to %s failed, syserr = %s",
                     ASL_LEVEL_DEBUG, __FUNCTION__, newConn->serverName,
                     strerror(error));
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
		smb_log_info("%s open session to %s failed, syserr = %s",
                     ASL_LEVEL_DEBUG, __FUNCTION__, newConn->serverName,
                     strerror(error));
		goto done;
	}
	
done:
	if (error) {
		smb_ctx_done(newConn);	
	}
    else {
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
	
	error = smb2io_get_dfs_referral(newConn, referralStr, DFS_REFERRAL_V4,
                                    outReferralDict);
	if (error) {
		smb_log_info("%s DFS_REFERRAL_V4 failed, syserr = %s", 
					 ASL_LEVEL_DEBUG, __FUNCTION__, strerror(error));
		goto done;
	}
    
	(void)smb_share_disconnect(newConn);

done:
	if (error) {
		smb_ctx_done(newConn);
    }
	else {
		*outConn = newConn;
    }
	
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
    
	CFStringAppendCString(referralStr, inConn->serverName,
                          kCFStringEncodingUTF8);
    
	if (inConn->ct_origshare) {
		CFStringAppendCString(referralStr, "/", kCFStringEncodingUTF8);
		CFStringAppendCString(referralStr, inConn->ct_origshare,
                              kCFStringEncodingUTF8);
	}
    
	if (inConn->mountPath && !shareOnly) {
		CFStringAppendCString(referralStr, "/", kCFStringEncodingUTF8);
		CFStringAppend(referralStr, inConn->mountPath);
	}
    
	return referralStr;
}

static int processDfsReferralDictionary(struct smb_ctx * inConn,
										struct smb_ctx ** outConn,
										CFStringRef inReferralStr,
										CFStringRef inUnConsumedStr,
                                        int *loopCnt,
										CFMutableArrayRef dfsReferralDictArray)
{
	CFMutableDictionaryRef referralDict = NULL;
	uint32_t ii, referralHeaderFlags;
	uint16_t numberOfReferrals;
	CFArrayRef referralList;
	int error = 0;
	struct smb_ctx *tmpConn = NULL;
    CFMutableStringRef new_referral_str = NULL;
    int add_unconsumed = 0;
	
    /* This is a recursive function, make sure we dont recurse too much */
	if (*loopCnt > MAX_LOOP_CNT) {
		return EMLINK;
    }
	*loopCnt += 1;
	
    /* 
     * Send first GET_DFS_REFERRAL and get the reply results back in a 
     * dictionary.
     * 
     * The dictionary has the reply referral header information like
     * PathConsumed, NumberOfReferrals, ReferralHeaderFlags, etc as key values.
     * The actual referrals are stored as an array of dictionaries with one
     * dictionary per referral. Each referral dictionary contains the referral
     * information like VersionNumber, Size, ServerType, etc.
     */
	error = getDfsReferralDictFromReferral(inConn, &tmpConn, inReferralStr,
                                           &referralDict);
	if (error) {
        /* 
         * If there was unconsumed path, try the referral string just by 
         * itself without the unconsumed parth 
         */
        if (inUnConsumedStr != NULL) {
            /* Make mutable copy of current referral string */
            new_referral_str = CFStringCreateMutableCopy(NULL, 1024, inReferralStr);
            if (new_referral_str == NULL) {
                smb_log_info("%s: CFStringCreateMutableCopy failed",
                             ASL_LEVEL_ERR, __FUNCTION__);
                goto done;
            }
            
            /* Build new referral string without unconsumed part */
            CFStringFindAndReplace(new_referral_str, inUnConsumedStr, CFSTR(""),
                                   CFRangeMake(0, CFStringGetLength(new_referral_str)),
                                   0);

            /* Try GET_DFS_REFERRAL again without unconsumed part */
            error = getDfsReferralDictFromReferral(inConn, &tmpConn, new_referral_str,
                                                   &referralDict);

            CFRelease(new_referral_str);
            new_referral_str = NULL;

            if (!error) {
                add_unconsumed = 1;
                goto found_referral;
            }
            else {
                /* Failed again, give up */
                smb_log_info("%s: getDfsReferralDictFromReferral w/o unconsumed failed (%s)",
                             ASL_LEVEL_ERR, __FUNCTION__, strerror(error));
            }
        }
		goto done;
    }

found_referral:
    
	/* If we find no items then return the correct error */
	error = ENOENT;
    
    /* Get the reply referral header info */
	referralHeaderFlags = uint32FromDictionary(referralDict,
                                               kReferralHeaderFlags);
	numberOfReferrals = uint16FromDictionary(referralDict,
                                             kNumberOfReferrals);

    /* Get array that contains the referral dictionaries */
	referralList = CFDictionaryGetValue(referralDict, kReferralList);
	if (referralList == NULL) {
        /* No array found, so no referrals. Leave. */
		goto done;
    }
	
    /* 
     * Check kNumberOfReferrals matches actual number of referrals in the array
     * and if not, then use the actual number of referrals in the array
     */
	if ((uint16_t)CFArrayGetCount(referralList) < numberOfReferrals) {
		numberOfReferrals = (uint16_t)CFArrayGetCount(referralList);
    }
	
    /* For each referral returned, try to connect to it */
	for (ii = 0; ii < numberOfReferrals; ii++) {
		CFDictionaryRef referralInfo;
		CFStringRef referralStr;
		CFStringRef unconsumedPathStr = NULL;
		
        if (new_referral_str != NULL) {
            /* must be from a previous iteration */
            CFRelease(new_referral_str);
            new_referral_str = NULL;
        }
        
        /* Get the dictionary for this referral */
		referralInfo = CFArrayGetValueAtIndex(referralList, ii);
		if (referralInfo == NULL) {
            /* missing entry, so try next referral */
			continue;
        }
		
        /* 
         * Get the new referral string which has the new server address 
         * with any unconsumed path appended on.
         */
		referralStr = CFDictionaryGetValue(referralInfo, kNewReferral);
		if (referralStr == NULL) {
            /* missing entry, so try next referral */
			continue;
        }
        
        if ((add_unconsumed == 1) && (inUnConsumedStr != NULL)) {
            new_referral_str = CFStringCreateMutableCopy(NULL, 1024,
                                                         referralStr);
            if (new_referral_str == NULL) {
                smb_log_info("%s: CFStringCreateMutableCopy 2 failed",
                             ASL_LEVEL_ERR, __FUNCTION__);
            }
            else {
                CFStringAppend(new_referral_str, inUnConsumedStr);
                referralStr = new_referral_str;
            }
        }
		
        unconsumedPathStr = CFDictionaryGetValue(referralInfo, kUnconsumedPath);
        
		/*
         * If kDFSStorageServer is set in the header's ReferralHeaderFlags 
         * then that means we should be at the end of the referrals.
         * This does not always seem to be true and thus we need to continue
         * checking to be sure.
         */
		if (referralHeaderFlags & kDFSStorageServer) {
			struct smb_ctx *newConn = NULL;
			
			/* Connect to the server */
			error = connectToReferral(tmpConn, &newConn, referralStr, NULL);
			if (error) {
                /* if cant connect to the server, try next referral */
				continue;
            }
			
			/* Connect to the share */
			error = smb_share_connect(newConn);
			if (error) {
                /* if cant connect to the share, try next referral */
				smb_ctx_done(newConn);
				continue;
			}
            
			/*
			 * The Spec says that if kDFSStorageServer is set in the
             * ReferralHeaderFlags then ALL of the referrals are storage. This 
             * does not seem to be true in the real world. So we now always do 
             * one more check unless the server and tree don't support DFS.
			 */
			if (((newConn->ct_vc_caps & SMB_CAP_DFS) != SMB_CAP_DFS) ||
				(!(smb_tree_conn_optional_support_flags(newConn) & SMB_SHARE_IS_IN_DFS))) {
				/* 
                 * Server does not support DFS or share does not support DFS,
                 * so assume this is the final storage server and thus we are
                 * done.
                 */
				*outConn = newConn;
			} else if (isReferralPathNotCovered(newConn, referralStr)) {
                /* 
                 * Referral not here (even though it should be here), recurse 
                 * again using the new referral string 
                 */
				error = processDfsReferralDictionary(newConn, outConn,
                                                     referralStr, unconsumedPathStr,
                                                     loopCnt, dfsReferralDictArray);
				smb_ctx_done(newConn);
				if (error) {
                    /* if couldnt resolve, try next referral */
					continue;
                }

				/* 
                 * No error, so the out connection now contains a storage 
                 * server, so we are done and can stop recursing. 
                 */
			}
            else {
				/* 
                 * Referral resolved on this server and this is a storage
                 * referral, so we are done 
                 */
				*outConn = newConn;
			}
            
			break;
		}
        else {
            /* 
             * No storage servers returned, so keep recursing to resolve using
             * the new referral string given to us.
             */
			error = processDfsReferralDictionary(tmpConn, outConn,
                                                 referralStr, unconsumedPathStr,
                                                 loopCnt, dfsReferralDictArray);
            
			if (!error) {
				/*
                 * No error, so the out connection now contains a storage
                 * server, so we are done and can stop recursing.
                 */
				break;
            }
		}
	}
	
done:
	if (referralDict) {
		if (dfsReferralDictArray) {
            /* Return resolved references found, usually for debugging */
			CFArrayAppendValue(dfsReferralDictArray, referralDict);
		}
        
		CFRelease(referralDict);
	}

    if (new_referral_str != NULL) {
        CFRelease(new_referral_str);
        new_referral_str = NULL;
    }

	smb_ctx_done(tmpConn);
	return error;
}

static CFMutableArrayRef
getDfsRootServers(struct smb_ctx * inConn, CFStringRef serverReferralStr)
{
    CFMutableArrayRef dcArray = NULL;
    CFMutableDictionaryRef serverReferralDict = NULL;
    struct smb_ctx *newConn = NULL;
    int error = ENOENT;
    
    dcArray = CFArrayCreateMutable(kCFAllocatorSystemDefault, 0,
                                   &kCFTypeArrayCallBacks);
    if (!dcArray) {
        return NULL;
    }
    
    /* Connect to the server */
    error = connectToReferral(inConn, &newConn, NULL, inConn->ct_url);
    if (error) {
		smb_log_info("%s server connect failed, syserr = %s",
					 ASL_LEVEL_DEBUG, __FUNCTION__, strerror(error));
		goto done;
    }
    
    /* Connect to IPC$ share */
	smb_ctx_setshare(newConn, "IPC$");
	error = smb_share_connect(newConn);
	if (error) {
		smb_log_info("%s share connect failed, syserr = %s", 
					 ASL_LEVEL_DEBUG, __FUNCTION__, strerror(error));
		goto done;
	}
    
    error = smb2io_get_dfs_referral(newConn, serverReferralStr, DFS_REFERRAL_V4,
                                    &serverReferralDict);
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
 * Given a URL and server name referral (domain controller name = "/servername")
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
		smb_log_info("%s smb_url_to_dictionary failed, syserr = %s",
                     ASL_LEVEL_DEBUG, __FUNCTION__, strerror(error));
        goto done;
    }
    
    /* Replace the server name with the new server name */
    CFDictionaryRemoveValue(mutableDict, kNetFSHostKey);
    
    CFDictionarySetValue (mutableDict, kNetFSHostKey, serverStr);
    
    /* Now create the URL from the new dictionary */
    error = smb_dictionary_to_url(mutableDict, &url);
    if (error) {
		smb_log_info("%s smb_dictionary_to_url failed, syserr = %s",
                     ASL_LEVEL_DEBUG, __FUNCTION__, strerror(error));
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
 * Given a server name that we assume is really a domain name, try to get the 
 * list of domain controls for that domain. Send a GET_DFS_REFERRAL and try to 
 * get back a Name List Referral which lists all the domain controllers.
 */
static CFArrayRef 
createDomainControlsUsingServerName(struct smb_ctx * inConn,
                                    const char *serverName,
                                    CFMutableArrayRef dfsReferralDictArray)
{
    CFStringRef serverReferralStr = createReferralStringFromCString(serverName);
    CFMutableArrayRef dfsDCDictArray = (serverReferralStr) ? getDfsRootServers(inConn, serverReferralStr) : NULL;
    CFArrayRef domainControlArray = NULL;
    CFDictionaryRef serverReferralDict = NULL;
    CFDictionaryRef referralEntryDict = NULL;
    CFArrayRef array = NULL;
    
    /* We are done with the server referral string */
    if (serverReferralStr) {
        CFRelease(serverReferralStr);
    }
    
    /*
     * dfsDCDictArray is an array, but it only contains one dictionary
     * that contains another array that contains only one dictionary. Sigh.
     */
    if (!dfsDCDictArray || !CFArrayGetCount(dfsDCDictArray)) {
        goto done;
    }
    
    /* 
     * Get the only dictionary item in this array which contains the Name List
     * referral info including referral header and array of referral entries.
     */
    serverReferralDict = CFArrayGetValueAtIndex(dfsDCDictArray, 0);
    if (serverReferralDict) {
        if (dfsReferralDictArray) {
            /* Return list of domain controllers found, usually for debugging */
            CFArrayAppendValue(dfsReferralDictArray, serverReferralDict);
        }

        /* Get the array of referral entries */
        array = CFDictionaryGetValue(serverReferralDict, kReferralList);
    }
    
    if (array && CFArrayGetCount(array)) {
        /* 
         * Since its a Name List Referral, there should only be one referral
         * entry returned which contains the domain controller list info
         */
        referralEntryDict = CFArrayGetValueAtIndex(array, 0);
    }
    
    if (referralEntryDict) {
        /* Now lets get the array of domain controllers */ 
        domainControlArray = CFDictionaryGetValue(referralEntryDict,
                                                  kExpandedNameArray);
    }

done:
    if (domainControlArray) {
        CFRetain(domainControlArray);
    }
    
    if (dfsDCDictArray) {
        CFRelease(dfsDCDictArray);
    }
    
    /* return the list of domain controllers */
    return domainControlArray;
}

/*
 * Check to see we are trying to mount a DFS Referral.
 */
int checkForDfsReferral(struct smb_ctx * inConn, struct smb_ctx ** outConn,
                        char *tmscheme, CFMutableArrayRef dfsReferralDictArray)
{
	CFStringRef referralStr = NULL;
	int error = 0;
	int loopCnt = 0;
	struct smb_ctx *newConn = NULL;
    struct smb_ctx *rootDfsConn = NULL;
    
	*outConn = NULL;
    
    /* 
     * Whether or not we use DFS, we always need to do a tree connect so do
     * it now.
     */
    error = smb_share_connect(inConn);

	/* 
	 * At this point we expect the connection to have an authenticated 
	 * connection to the server and a tree connect to the share.
	 */
    
    /* Does Server support DFS? */
	if ((inConn->ct_vc_caps & SMB_CAP_DFS) != SMB_CAP_DFS) {
		/* Server doesn't understand DFS so we are done. */
        if (error) {
            /* Tree connect failed, log it and return the error */
            smb_log_info("%s: smb_share_connect failed and no DFS, syserr = %s", 
                         ASL_LEVEL_DEBUG, __FUNCTION__, strerror(error));
            if (tmscheme) {
                NetFSLogToMessageTracer(tmscheme, "share connect in SMB_Mount",
                                        error);
            }
        }
        
        return error;
	}
	
    /* If tree connect worked, go try to resolve the referral string */
    if (!error) {
        goto tree_connect_worked;
    }
    
    /* 
	 * Tree connect failed so we either have a DFS referral or we have a bad
	 * share. See if its a DFS Referral.
	 */
    /* Create the referral string */
    referralStr = createReferralString(inConn, FALSE);
    if (referralStr == NULL) {
        /* Failed to create referral string, so leave */
        smb_log_info("%s: createReferralString failed",
                     ASL_LEVEL_DEBUG, __FUNCTION__);
        goto done;
    }
    
    /* Try to resolve the entire referral string */
    error = processDfsReferralDictionary(inConn, outConn, referralStr, NULL,
                                         &loopCnt, dfsReferralDictArray);
    if (!error) {
        /* It worked so we are done */
        goto done;
    }
    else {
        smb_log_info("%s: processDfsReferralDictionary failed (%s)",
                     ASL_LEVEL_DEBUG, __FUNCTION__, strerror(error));
    }
    
    /* Failed to resolve, cleanup and continue trying to resolve */
    loopCnt = 0;
    if (referralStr) {
        CFRelease(referralStr);
        referralStr = NULL;
    }

	/* 
	 * Couldn't resolve the referral the normal method, see if we need to
	 * start resolving from the DFS Root first and work our way down the string
	 */
    /* Grab the DFS root referral which only has host and share in it */
    referralStr = createReferralString(inConn, TRUE);
    if (referralStr == NULL) {
        /* Failed to create referral string, so leave */
        smb_log_info("%s: createReferralString failed",
                     ASL_LEVEL_DEBUG, __FUNCTION__);
        goto done;
    }
    
    /* See if we can resolve just the DFS root referral */
    error = processDfsReferralDictionary(inConn, &rootDfsConn, referralStr,
                                         NULL, &loopCnt, dfsReferralDictArray);

    CFRelease(referralStr);
    referralStr = NULL;

    if (!error) {
        /* Resolved just the DFS root referral */
        CFURLRef url = createURLAndReplaceServerName(inConn->ct_url,
                                                     rootDfsConn->serverNameRef);
        struct smb_ctx *tmpConn = NULL;
    
        error = connectToReferral(inConn, &tmpConn, NULL, url);
        CFRelease(url); /* Done with the URL release it */
        
        if (!error) {
            /* Create the full referral string with new current server name */
            referralStr = createReferralString(tmpConn, FALSE);
            if (referralStr == NULL) {
                /* Failed to create referral string, so leave */
                smb_log_info("%s: createReferralString failed",
                             ASL_LEVEL_DEBUG, __FUNCTION__);
                goto done;
            }
            
            /* See if we can resolve the full DFS referral */
            error = processDfsReferralDictionary(tmpConn, outConn, referralStr,
                                                 NULL, &loopCnt,
                                                 dfsReferralDictArray);
            if (error) {
                smb_log_info("%s: processDfsReferralDictionary (root/referral) failed (%s)",
                             ASL_LEVEL_DEBUG, __FUNCTION__, strerror(error));
            }
        }
        else {
            smb_log_info("%s: connectToReferral (root) failed (%s)",
                         ASL_LEVEL_DEBUG, __FUNCTION__, strerror(error));
        }
        smb_ctx_done(tmpConn);
    }
    else {
        smb_log_info("%s: processDfsReferralDictionary (root) failed (%s)",
                     ASL_LEVEL_DEBUG, __FUNCTION__, strerror(error));
    }
    
    smb_ctx_done(rootDfsConn);
    
    if (!error) {
        /* It worked so we are done */
        goto done;
    }
    
    /* Failed so cleanup and continue trying to resolve */
    loopCnt = 0;
    if (referralStr) {
        CFRelease(referralStr);
        referralStr = NULL;
    }
	
	/* 
     * Everything else failed, see if they gave us a domain name as the 
     * server name. If serverIsDomainController is true, then we already 
     * found a domain controller via the AD plugin so we do not need to 
     * try this. This is to handle when the client is not bound to AD.
     */
    if ((error) && (inConn->serverIsDomainController == FALSE)) {
        int dcerror = 0;
        CFArrayRef domainControllerArray = createDomainControlsUsingServerName(inConn,
                                                                               inConn->serverName,
                                                                               NULL);
        CFIndex ii, count = (domainControllerArray) ? CFArrayGetCount(domainControllerArray) : 0;
       
        for (ii = 0; ii < count; ii++) {
            CFURLRef url = createURLAndReplaceServerName(inConn->ct_url,
                                                         CFArrayGetValueAtIndex(domainControllerArray, ii));
            
            if (!url) {
                continue;   /* No URL, see if we can find another entry */
            }
            
            /* Try to connect to this domain controller */
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
            
            inConn = newConn;   /* We will use this domain controller */
            error = 0;          /* Clear out the error */
            break;
        }
        
        if (domainControllerArray) {
            CFRelease(domainControllerArray);
        }
        
        if (error) {
            /* Couldn't find any domain controllers, so we are done */
            smb_log_info("%s: No AD and no domain controllers found",
                         ASL_LEVEL_DEBUG, __FUNCTION__);
            goto done;
        }
    }
	
tree_connect_worked:
	/*
	 * We are here because
     * (1) The Tree Connect worked or
     * (2) We have not been able to resolve the referral string, must not be
     * bound to AD but we did find a domain controller from the server name.
     */
    
    /* Does Share support DFS? */
	if ((smb_tree_conn_optional_support_flags(inConn) & SMB_SHARE_IS_IN_DFS) != SMB_SHARE_IS_IN_DFS) {
		/* Share doesn't understand DFS so we are done */
		goto done;
	}
    
	referralStr = createReferralString(inConn, FALSE);
	if (referralStr == NULL) {
        /* Failed to create referral string, so leave */
        smb_log_info("%s: createReferralString failed",
                     ASL_LEVEL_DEBUG, __FUNCTION__);
		goto done;
	}
    
	if (!isReferralPathNotCovered(inConn, referralStr)) {
		goto done;	/* Nothing to do here */
	}
    
    /* See if we can resolve the full DFS referral */
	error = processDfsReferralDictionary(inConn, outConn, referralStr, NULL,
                                         &loopCnt, dfsReferralDictArray);
	if (error) {
        smb_log_info("%s: processDfsReferralDictionary (non AD) failed (%s)",
                     ASL_LEVEL_DEBUG, __FUNCTION__, strerror(error));
	}
	
done:
    /* All done, so clean up */
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
            NetFSLogToMessageTracer(tmscheme, "dfs url mount in SMB_Mount",
                                    error);
        }
    }
    
	return error;
}

/*
 * Check to see we are trying to mount a DFS Referral.
 * This function is used by smbutil to debug DFS problems
 */
int 
getDfsReferralList(struct smb_ctx * inConn,
                   CFMutableDictionaryRef dfsReferralDict)
{
	struct smb_ctx * outConn = NULL;
	struct smb_ctx * newConn = NULL;
	int error = 0;
    CFMutableArrayRef dfsServerDictArray = NULL;
    CFMutableArrayRef dfsReferralDictArray = NULL;
	CFArrayRef dcArrayRef = NULL;
    
	/* 
	 * At this point we expect the connection to have an authenticated
	 * connection to the server and a tree connect to the share.
	 */
	if ((inConn->ct_vc_caps & SMB_CAP_DFS) != SMB_CAP_DFS) {
		/* Server doesn't understand DFS so we are done */
        smb_log_info("%s: server does not support DFS", ASL_LEVEL_DEBUG,
                     __FUNCTION__);
		goto done;
	}

    /* Create dictionary to hold domain controllers we have found */
    dfsServerDictArray = CFArrayCreateMutable(kCFAllocatorSystemDefault, 0,
                                              &kCFTypeArrayCallBacks);
	if (!dfsServerDictArray) {
        smb_log_info("%s: CFArrayCreateMutable failed for server dict",
                     ASL_LEVEL_DEBUG, __FUNCTION__);
		goto done;
	}

    if (inConn->serverIsDomainController == TRUE) {
        /* 
         * Must be bound to AD and already connected to a Domain Controller.
         * Get back the list of Domain Controllers from the AD plugin.
         */
        dcArrayRef = smb_resolve_domain(inConn->serverNameRef);

        /* 
         * If we have an array of domain controller names, save them into 
         * the dictionary 
         */
        if (dcArrayRef && CFArrayGetCount(dcArrayRef)) {
            CFDictionarySetValue(dfsReferralDict, kDfsADServerArray, dcArrayRef);
        }
    }
    else {
        /* Not bound to AD or AD failed to get domain controllers */
        dcArrayRef = createDomainControlsUsingServerName(inConn,
                                                         inConn->serverName,
                                                         dfsServerDictArray);

        /* 
         * If we have a Name List referral entry dictionary, save it into 
         * the dictionary
         */
        if (CFArrayGetCount(dfsServerDictArray)) {
            CFDictionarySetValue(dfsReferralDict, kDfsServerArray,
                                 dfsServerDictArray);
        }
    }
    
    if (dcArrayRef) {
        CFRelease(dcArrayRef);
    }
    
    /* Create dictionary to track all the referrals we resolve */
    dfsReferralDictArray = CFArrayCreateMutable(kCFAllocatorSystemDefault, 0,
                                                &kCFTypeArrayCallBacks);
	if (!dfsReferralDictArray) {
        smb_log_info("%s: CFArrayCreateMutable failed", ASL_LEVEL_DEBUG,
                     __FUNCTION__);
		goto done;
	}

    error = checkForDfsReferral(inConn, &outConn, NULL, dfsReferralDictArray);
	
    if (CFArrayGetCount(dfsReferralDictArray)) {
		CFDictionarySetValue(dfsReferralDict, kDfsReferralArray,
                             dfsReferralDictArray);
	}

    if (error) {
        smb_log_info("%s checkForDfsReferral failed syserr = %s",
                     ASL_LEVEL_DEBUG, __FUNCTION__, strerror(error));
        goto done;
    }

done:
	if (dfsServerDictArray) {
		CFRelease(dfsServerDictArray);
	}

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
