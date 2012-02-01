/*
 * Copyright (c) 2004-2011 Apple Inc. All Rights Reserved.
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
 * ocspdServer.cpp - Server class for OCSP helper
 */
#if OCSP_DEBUG
#define OCSP_USE_SYSLOG	1
#endif
#include "ocspdServer.h"
#include <security_ocspd/ocspdDebug.h>
#include <security_ocspd/ocspdUtils.h>
#include <security_utilities/threading.h>
#include <security_cdsa_utils/cuFileIo.h>
#include "ocspdNetwork.h"
#include "ocspdDb.h"
#include "crlDb.h"
#include <CommonCrypto/CommonDigest.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <Security/SecTask.h>
#include <Security/SecAsn1Coder.h>
#include <Security/ocspTemplates.h>
#include <Security/cssmapple.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <bsm/libbsm.h>
#include <sys/stat.h>
#include <security_ocspd/ocspd.h>						/* created by MIG */


/* How long to wait for a CRL download to complete before ignoring result
 * and letting it complete in the background. */
#define CRL_MAX_DOWNLOAD_WAIT	3.0

/* Maximum CRL length to consider putting in the cache db (128KB);
 * larger CRLs will be written to individual files. */
#define CRL_MAX_DATA_LENGTH (1024*128)

/* Lock while a shared parameters struct is being read or updated */
Mutex gParamsLock;

/* Lock while a file is being written */
Mutex gFileWriteLock;

/* Lock while shared lists are being read or updated */
Mutex gListLock;

/* Global list of files being downloaded */
CFMutableArrayRef gDownloadList = NULL;

/* Global list of OCSP URIs currently being contacted */
CFMutableArrayRef gURIList = NULL;

/* Global dictionary of CRL issuers */
CFMutableDictionaryRef gIssuersDict = NULL;

const char *gCrlPath = "/var/db/crls/";


#pragma mark ----- OCSP utilities -----

/* 
 * Once we've gotten a response from a server, cook up a SecAsn1OCSPDReply.
 */
static SecAsn1OCSPDReply *ocspdGenReply(
	SecAsn1CoderRef coder, 
	const CSSM_DATA &resp,
	const CSSM_DATA &certID)
{
	SecAsn1OCSPDReply *ocspdRep = 
		(SecAsn1OCSPDReply *)SecAsn1Malloc(coder, sizeof(*ocspdRep));
	SecAsn1AllocCopyItem(coder, &resp, &ocspdRep->ocspResp);
	SecAsn1AllocCopyItem(coder, &certID, &ocspdRep->certID);
	return ocspdRep;
}

static SecAsn1OCSPDReply *ocspdHandleReq(
	SecAsn1CoderRef coder, 
	SecAsn1OCSPDRequest &request,
	bool recursing)
{
	CSSM_DATA derResp = {0, NULL};
	CSSM_RETURN crtn;	
	bool cacheReadDisable = false;
	bool cacheWriteDisable = false;
		
	if((request.cacheReadDisable != NULL) &&
	   (request.cacheReadDisable->Length != 0) &&
	   (request.cacheReadDisable->Data[0] != 0)) {
		cacheReadDisable = true;
	}
	if((request.cacheWriteDisable != NULL) &&
	   (request.cacheWriteDisable->Length != 0) &&
	   (request.cacheWriteDisable->Data[0] != 0)) {
		cacheWriteDisable = true;
	}

	if(!cacheReadDisable) {
		/* do a cache lookup */
		bool found = ocspdDbCacheLookup(coder, request.certID, request.localRespURI,
			derResp);
		if(found) {
			return ocspdGenReply(coder, derResp, request.certID);
		}
	}
	
	if(request.localRespURI) {
		if(request.ocspReq == NULL) {
			ocspdErrorLog("ocspdHandleReq: localRespURI but no request to send\n");
			return NULL;
		}
		crtn = ocspdHttpPost(coder, *request.localRespURI, *request.ocspReq, derResp);
		if(crtn == CSSM_OK) {
			SecAsn1OCSPDReply *reply = ocspdGenReply(coder, derResp, request.certID);
			if(!cacheWriteDisable) {
				ocspdDbCacheAdd(derResp, *request.localRespURI);
			}
			return reply;
		}
	}
	
	/* now try everything in requests.urls, the normal case */
	unsigned numUris = ocspdArraySize((const void **)request.urls);
	for(unsigned dex=0; dex<numUris; dex++) {
		CSSM_DATA *uri = request.urls[dex];
		CFStringRef uriStr = NULL;
		bool reqOK = (uri->Length > 0 && uri->Data != NULL);

		if(reqOK && recursing) {
			/* We are being called from within a concurrent request, so must
			 * be careful to avoid repeating the same lookups endlessly. */
			uriStr = CFStringCreateWithBytes(kCFAllocatorDefault,
				uri->Data, uri->Length, kCFStringEncodingUTF8, false);
			if(!uriStr) {
				reqOK = false;
			} else {
				StLock<Mutex> _(gListLock); /* lock before examining list */
				if(gURIList == NULL) {
					gURIList = CFArrayCreateMutable(kCFAllocatorDefault,
						0, &kCFTypeArrayCallBacks);
					if(!gURIList) {
						reqOK = false;
					}
				}
				if(reqOK) {
					bool inProgress = CFArrayContainsValue(gURIList,
						CFRangeMake(0, CFArrayGetCount(gURIList)), uriStr);
					if(!inProgress) {
						/* Add the URI to our "reentrant URIs in progress" list
						 * and proceed with the request. This allows legitimate
						 * reentrancy when processing redirects; however, if
						 * execution comes back here with the same URI before we
						 * finish this function and can remove it from the list,
						 * we shouldn't make the request. */
						CFArrayAppendValue(gURIList, uriStr);
					} else {
						/* Don't repeat this request */
						char *ustr = (char *)malloc(uri->Length + 1);
						memmove(ustr, uri->Data, uri->Length);
						ustr[uri->Length] = '\0';
						ocspdErrorLog("ocspdHandleReq: request for \"%s\" is already in progress\n", ustr);
						free(ustr);
						reqOK = false;
					}
				}
			}
		}

		if(reqOK) {
			/* go ahead with this OCSP request */
			crtn = ocspdHttpPost(coder, *uri, *request.ocspReq, derResp);
		} else {
			crtn = CSSMERR_APPLETP_OCSP_BAD_REQUEST;
		}
		if(uriStr) {
			if(reqOK) {
				/* remove URI from list */
				StLock<Mutex> _(gListLock);
				CFIndex idx =  CFArrayGetFirstIndexOfValue(gURIList,
					CFRangeMake(0, CFArrayGetCount(gURIList)), uriStr);
				if(idx >= 0) {
					CFArrayRemoveValueAtIndex(gURIList, idx);
				}
			}
			CFRelease(uriStr);
		}

		if(crtn == CSSM_OK) {
			SecAsn1OCSPDReply *reply = ocspdGenReply(coder, derResp, request.certID);
			if(!cacheWriteDisable) {
				ocspdDbCacheAdd(derResp, *uri);
			}
			return reply;
		}
	}

	return NULL;
}


#pragma mark ----- CRL utilities -----

/*
 * Create specified directory, including intermediate directories.
 */
static int
_mkpath_np(char *path, mode_t omode)
{
	char *apath = NULL;
	unsigned int depth = 0;
	mode_t chmod_mode = 0;
	int retval = 0;
	int old_errno = errno;

	/* Try the trivial case first. */
	if (0 == mkdir(path, omode)) {
		goto mkpath_exit;
	}

	/* Anything other than an ENOENT indicates an error that we need to
	 * send back to the caller.  ENOENT indicates that we need to try a
	 * lower level.
	 */
	if (errno != ENOENT) {
		retval = errno;
		goto mkpath_exit;
	}

	apath = strdup(path);
	if (apath == NULL) {
		retval = ENOMEM;
		goto mkpath_exit;
	}

	while (1) {
		/* Increase our depth and try making that directory */
		char *s = strrchr(apath, '/');
		if (!s) {
			/* We should never hit this under normal circumstances,
			 * but it can occur due to really unfortunate timing
			 */
			retval = ENOENT;
			goto mkpath_exit;
		}
		*s = '\0';
		depth++;

		if (0 == mkdir(apath, S_IRWXU | S_IRWXG | S_IRWXO)) {
			/* Found our starting point */

			/* POSIX 1003.2:
			 * For each dir operand that does not name an existing
			 * directory, effects equivalent to those cased by the
			 * following command shall occcur:
			 *
			 * mkdir -p -m $(umask -S),u+wx $(dirname dir) &&
			 *    mkdir [-m mode] dir
			 */

			struct stat dirstat;
			if (-1 == stat(apath, &dirstat)) {
				/* Really unfortunate timing ... */
				retval = ENOENT;
				goto mkpath_exit;
			}

			if ((dirstat.st_mode & (S_IWUSR | S_IXUSR)) != (S_IWUSR | S_IXUSR)) {
			        chmod_mode = dirstat.st_mode | S_IWUSR | S_IXUSR;
				if (-1 == chmod(apath, chmod_mode)) {
					/* Really unfortunate timing ... */
					retval = ENOENT;
					goto mkpath_exit;
				}
			}
			break;
		}
		if (errno != ENOENT) {
			retval = errno;
			goto mkpath_exit;
		}
	}

	while (depth > 1) {
		/* Decrease our depth and make that directory */
		char *s = strrchr(apath, '\0');
		*s = '/';
		depth--;

		if (-1 == mkdir(apath, S_IRWXU | S_IRWXG | S_IRWXO)) {
			retval = errno;
			goto mkpath_exit;
		}

		if (chmod_mode) {
			if (-1 == chmod(apath, chmod_mode)) {
				/* Really unfortunate timing ... */
				retval = ENOENT;
				goto mkpath_exit;
			}
		}
	}

	if (-1 == mkdir(path, omode)) {
		retval = errno;
	}

mkpath_exit:
	free(apath);

	errno = old_errno;
	return retval;
}

/*
 * Generate and malloc a CRL filename given a lookup key,
 * which can be either the issuer's distinguished name or
 * a distribution point URL. Caller must free.
 */
static char* crlGenerateFileName(
    unsigned char *key,
    size_t keyLen,
	const char *pathPrefix,
	const char *extension)
{
    if(!key || !keyLen) {
        return NULL;
    }
    const size_t prefixLen = strlen(pathPrefix);
    const size_t suffixLen = strlen(extension);
    const size_t fileNameLen = prefixLen+(CC_SHA1_DIGEST_LENGTH*2)+suffixLen+1;
    char *fileName = (char*)malloc(fileNameLen);

    if(!fileName) {
        return NULL;
    }
    unsigned char digest[CC_SHA1_DIGEST_LENGTH];
    unsigned char *dataPtr = key;
    size_t dataLen = keyLen;

    CC_SHA1(dataPtr, dataLen, digest);
    char *outPtr = &fileName[0];
    size_t outLen = fileNameLen;
    strlcpy(outPtr, pathPrefix, outLen);
    outPtr += prefixLen;
    outLen -= prefixLen;
    dataPtr = &digest[0];
    for(dataLen=CC_SHA1_DIGEST_LENGTH; dataLen > 0; dataLen--) {
        snprintf(outPtr, outLen, "%02X", *dataPtr++);
        outPtr+=2;
        outLen-=2;
        *outPtr='\0';
    }
    strncat(fileName, extension, outLen-1);

    return fileName;
}

/*
 * Given a pointer and length, malloc and return a string which contains
 * the hex representation of the data. Caller must free.
 */
static char* crlPrintableStringWithData(
    unsigned char *inData,
    size_t inLen)
{
    size_t outStrLen = (inLen*2)+1;
    char *outStr = (char*)malloc(outStrLen);
    if(!outStr) {
        return NULL;
    }
    unsigned char *dataPtr = inData;
    size_t dataLen = inLen;
    char *outPtr = &outStr[0];
    size_t outLen = outStrLen;
    for(; dataLen > 0; dataLen--) {
        snprintf(outPtr, outLen, "%02X", *dataPtr++);
        outPtr+=2;
        outLen-=2;
        *outPtr='\0';
    }
    return outStr;
}

/* Given a path to a DER-encoded CRL file and a path to a PEM-encoded
 * CA issuers file, use OpenSSL to validate the CRL. This is a hack,
 * necessitated by performance issues with inserting extremely large
 * numbers of CRL entries into a CSSM DB (see <rdar://8934440>).
 *
 * Returns true if the signature on crlFileName is valid, false otherwise.
 */
bool crlSignatureValid(
	const char *crlFileName,
	const char *issuersFileName,
	const char *updateFileName,
	const char *revokedFileName)
{
	const char *vc1 = "/usr/bin/openssl crl -inform DER -noout -in \"";
	const char *vc2 = "\" -CAfile \"";
	const char *vc3 = "\" 2>&1 | /usr/bin/grep OK";
	size_t cmdLen = strlen(vc1)+strlen(crlFileName)+
					strlen(vc2)+strlen(issuersFileName)+
					strlen(vc3)+1;
	char *command = (char*)malloc(cmdLen);
	size_t tmpLen = cmdLen;
	strlcpy(command, vc1, tmpLen);
	tmpLen -= strlen(vc1);
	strncat(command, crlFileName, tmpLen);
	tmpLen -= strlen(crlFileName);
	strncat(command, vc2, tmpLen);
	tmpLen -= strlen(vc2);
	strncat(command, issuersFileName, tmpLen);
	tmpLen -= strlen(issuersFileName);
	strncat(command, vc3, tmpLen);

	bool valid = (system(command) == 0);
	free(command);
	if(!valid) {
		ocspdCrlDebug("crlSignatureValid: CRL failed to verify: %s\n", crlFileName);
		return false;
	}

	if(updateFileName) {
		/* create .update file to hold nextUpdate value */
		const char *uc1 = "/usr/bin/openssl crl -inform DER -noout -nextupdate -in \"";
		const char *uc2 = "\" | /usr/bin/awk -F= '{print $2}' > \"";
		const char *uc3 = "\"";
		cmdLen = strlen(uc1)+strlen(crlFileName)+
			strlen(uc2)+strlen(updateFileName)+
			strlen(uc3)+1;
		command = (char*)malloc(cmdLen);
		tmpLen = cmdLen;
		strlcpy(command, uc1, tmpLen);
		tmpLen -= strlen(uc1);
		strncat(command, crlFileName, tmpLen);
		tmpLen -= strlen(crlFileName);
		strncat(command, uc2, tmpLen);
		tmpLen -= strlen(uc2);
		strncat(command, updateFileName, tmpLen);
		tmpLen -= strlen(updateFileName);
		strncat(command, uc3, tmpLen);

		system(command);
		free(command);

		if(chmod(updateFileName, 0644)) {
			ocspdErrorLog("crlSignatureValid: chmod error %d for %s",
				errno, updateFileName);
		}
	}

	if(revokedFileName) {
		/* create .revoked file to hold validated cache of revoked serials */
		const char *rc1 = "/usr/bin/openssl crl -inform DER -noout -text -in \"";
		const char *rc2 = "\" | /usr/bin/grep \"Number:\" | /usr/bin/awk '{print $3}' > \"";
		const char *rc3 = "\"";
		cmdLen = strlen(rc1)+strlen(crlFileName)+
			strlen(rc2)+strlen(revokedFileName)+
			strlen(rc3)+1;
		command = (char*)malloc(cmdLen);
		tmpLen = cmdLen;
		strlcpy(command, rc1, tmpLen);
		tmpLen -= strlen(rc1);
		strncat(command, crlFileName, tmpLen);
		tmpLen -= strlen(crlFileName);
		strncat(command, rc2, tmpLen);
		tmpLen -= strlen(rc2);
		strncat(command, revokedFileName, tmpLen);
		tmpLen -= strlen(revokedFileName);
		strncat(command, rc3, tmpLen);

		system(command);
		free(command);

		if(chmod(revokedFileName, 0644)) {
			ocspdErrorLog("crlSignatureValid: chmod error %d for %s",
				errno, revokedFileName);
		}
	}

	return true;
}

/* Given a path to a file containing the CRL's nextUpdate date,
 * return true if this date is greater than the current date,
 * otherwise false.
 */
bool crlUpdateValid(
	const char *updateFileName)
{
	bool result = false;
	unsigned char *updateBytes = NULL;
	unsigned int updateLen = 0;
	int err;
	if((err=readFile(updateFileName, &updateBytes, &updateLen) != 0)) {
		ocspdCrlDebug("crlUpdateValid: error %d reading %s\n",
			err, updateFileName);
		return result;
	}
	/* check for special case where nextUpdate value is NONE */
	if(updateLen >= 4 && !memcmp(updateBytes, "NONE", 4)) {
		ocspdCrlDebug("crlUpdateValid: nextUpdate is NONE\n");
		result = true;
	}
	else {
		/* update time is expressed in POSIX locale and GMT timezone */
		tm tm_next;
		const char *format = "%b %d %H:%M:%S %Y %Z";
		setlocale(LC_TIME, "POSIX");
		if(strptime((const char *)updateBytes, format, &tm_next)) {
			time_t now = time(NULL);
			time_t next = timegm(&tm_next);
			result = (now < next);

			#if OCSP_DEBUG
			char buf[updateLen+1];
			strncpy(buf, (char *)updateBytes, updateLen);
			buf[updateLen-1]='\0'; /* deliberately cutting off final LF byte */
			ocspdCrlDebug("crlUpdateValid: nextUpdate=%s (%s)\n",
				buf, (result) ? "valid" : "must refetch!");
			#endif
		}
		else {
			ocspdCrlDebug("crlUpdateValid: no nextUpdate date found!\n");
		}
	}
	free(updateBytes);
	return result;
}

/* Given a path to a validated cache file and a serial number string,
 * determine whether that serial number is revoked in the cache.
 * This should only be called after crlSignatureValid has confirmed the
 * validity of the CRL and the cache file.
 */
bool crlSerialNumberRevoked(
	const char *revokedFileName,
	const char *serialNumber)
{
	const char *sc1 = "/usr/bin/grep -e \"^";
	const char *sc2 = "$\" \"";
	const char *sc3 = "\" 2>&1 >/dev/null";
	size_t cmdLen = strlen(sc1)+strlen(serialNumber)+
					strlen(sc2)+strlen(revokedFileName)+
					strlen(sc3)+1;
	char *command = (char*)malloc(cmdLen);
	size_t tmpLen = cmdLen;
	strlcpy(command, sc1, tmpLen);
	tmpLen -= strlen(sc1);
	strncat(command, serialNumber, tmpLen);
	tmpLen -= strlen(serialNumber);
	strncat(command, sc2, tmpLen);
	tmpLen -= strlen(sc2);
	strncat(command, revokedFileName, tmpLen);
	tmpLen -= strlen(revokedFileName);
	strncat(command, sc3, tmpLen);

	bool revoked = (system(command) == 0);
	free(command);

	return revoked;
}

/*
 * Attempt to create the CRL cache path if it doesn't exist.
 */
int crlCheckCachePath()
{
	return _mkpath_np((char*)gCrlPath, 0755);
}


#pragma mark ----- Mig-referenced OCSP routines -----

/* all of these Mig-referenced routines are called out from ocspd_server() */

kern_return_t ocsp_server_ocspdFetch (
	mach_port_t serverport,
	audit_token_t auditToken,
	Data ocspd_req,
	mach_msg_type_number_t ocspd_reqCnt,
	Data *ocspd_rep,
	mach_msg_type_number_t *ocspd_repCnt)
{
	ServerActivity();
	ocspdDebug("ocsp_server_ocspFetch top");
	*ocspd_rep = NULL;
	*ocspd_repCnt = 0;
	kern_return_t krtn = 0;
	unsigned numRequests;
	SecAsn1OCSPReplies replies;
	unsigned numReplies = 0;
	uint8 version = OCSPD_REPLY_VERS;
	pid_t pid = -1;
	audit_token_to_au32(auditToken, NULL, NULL, NULL, NULL, NULL, &pid, NULL, NULL);
	bool recursing = (getpid() == pid);
	
	/* decode top-level SecAsn1OCSPDRequests */
	SecAsn1CoderRef coder;
	SecAsn1CoderCreate(&coder);
	SecAsn1OCSPDRequests requests;
	memset(&requests, 0, sizeof(requests));
	if(SecAsn1Decode(coder, ocspd_req, ocspd_reqCnt, kSecAsn1OCSPDRequestsTemplate,
			&requests)) {
		ocspdErrorLog("ocsp_server_ocspdFetch: decode error\n");
		krtn = CSSMERR_APPLETP_OCSP_BAD_REQUEST;
		goto errOut;
	}
	if((requests.version.Length == 0) ||
	   (requests.version.Data[0] != OCSPD_REQUEST_VERS)) {
		/* 
		 * Eventually handle backwards compatibility here
		 */
		ocspdErrorLog("ocsp_server_ocspdFetch: request version mismatch\n");
		krtn = CSSMERR_APPLETP_OCSP_BAD_REQUEST;
		goto errOut;
	}
	
	numRequests = ocspdArraySize((const void **)requests.requests);
	replies.replies = (SecAsn1OCSPDReply **)SecAsn1Malloc(coder, (numRequests + 1) * 
		sizeof(SecAsn1OCSPDReply *));
	memset(replies.replies, 0, (numRequests + 1) * sizeof(SecAsn1OCSPDReply *));
	replies.version.Data = &version;
	replies.version.Length = 1;
	
	/* preparing for net fetch: enable another thread */
	OcspdServer::active().longTermActivity();

	/* This may need to be threaded, one thread per request */
	for(unsigned dex=0; dex<numRequests; dex++) {
		SecAsn1OCSPDReply *reply = ocspdHandleReq(coder, *(requests.requests[dex]), recursing);
		if(reply != NULL) {
			replies.replies[numReplies++] = reply;
		}
	}
	
	/* if we got any replies, sent them back to client */
	if(replies.replies[0] != NULL) {
		CSSM_DATA derRep = {0, NULL};
		if(SecAsn1EncodeItem(coder, &replies, kSecAsn1OCSPDRepliesTemplate,
				&derRep)) {
			ocspdErrorLog("ocsp_server_ocspdFetch: encode error\n");
			krtn = CSSMERR_TP_INTERNAL_ERROR;
			goto errOut;
		}
		
		/* 
		 * Use server's persistent Allocator to alloc this and tell 
		 * MachServer to dealloc after the RPC completes
		 */
		Allocator &alloc = OcspdServer::active().alloc();
		*ocspd_rep = alloc.malloc(derRep.Length);
		memmove(*ocspd_rep, derRep.Data, derRep.Length);
		*ocspd_repCnt = derRep.Length;
		MachPlusPlus::MachServer::active().releaseWhenDone(alloc, *ocspd_rep);
	}
	ocspdDebug("ocsp_server_ocspFetch returning %u bytes of replies", 
		(unsigned)*ocspd_repCnt);

errOut:
	SecAsn1CoderRelease(coder);
	return krtn;
}

kern_return_t ocsp_server_ocspdCacheFlush (
	mach_port_t serverport,
	Data certID,
	mach_msg_type_number_t certIDCnt)
{
	ServerActivity();
	ocspdDebug("ocsp_client_ocspdCacheFlush");
	CSSM_DATA certIDData = {certIDCnt, (uint8 *)certID};
	ocspdDbCacheFlush(certIDData);
	return 0;
}

kern_return_t ocsp_server_ocspdCacheFlushStale (
	mach_port_t serverport)
{
	ServerActivity();
	ocspdDebug("ocsp_server_ocspdCacheFlushStale");
	ocspdDbCacheFlushStale();
	return 0;

}

/*
 * Given a CSSM_DATA which was allocated in our server's alloc space, 
 * pass referent data back to caller and schedule a dealloc after the RPC
 * completes with MachServer.
 */
void passDataToCaller(
	CSSM_DATA		&srcData,		// allocd in our server's alloc space
	Data			*outData,
	mach_msg_type_number_t *outDataCnt)
{
	Allocator &alloc = OcspdServer::active().alloc();
	*outData    = srcData.Data;
	*outDataCnt = srcData.Length;
	MachPlusPlus::MachServer::active().releaseWhenDone(alloc, srcData.Data);
}

/*
 * Check whether the caller can access the network. Currently, this applies
 * only to applications running under App Sandbox.
 */
bool callerHasNetworkEntitlement(
	audit_token_t auditToken)
{
	bool result = true; /* until proven otherwise */
	SecTaskRef task = SecTaskCreateWithAuditToken(NULL, auditToken);
	if(task != NULL) {
		CFTypeRef appSandboxValue = SecTaskCopyValueForEntitlement(task,
			CFSTR("com.apple.security.app-protection"),
			NULL);
		if(appSandboxValue != NULL) {
			if(!CFEqual(kCFBooleanFalse, appSandboxValue)) {
				CFTypeRef networkClientValue = SecTaskCopyValueForEntitlement(task,
					CFSTR("com.apple.security.network.client"),
					NULL);
				if(networkClientValue != NULL) {
					result = (!CFEqual(kCFBooleanFalse, networkClientValue));
					CFRelease(networkClientValue);
				} else {
					result = false;
				}
			}
			CFRelease(appSandboxValue);
		}
		CFRelease(task);
	}
	return result;
}


#pragma mark ----- Mig-referenced routines for cert and CRL maintenance -----

/* 
 * Fetch a cert from the net. Currently we don't do any validation at all of
 * the returned data; that is handled by the caller. However, we do check
 * that the caller has a network entitlement if running under App Sandbox.
 *
 * I'm sure someone will ask "why don't we cache these certs?"; the main reason
 * is that the keychain schema does not allow for the storage of an expiration
 * date attribute or a URI, so we'd have to cook up yet another cert schema
 * (The system already has two - the Open Group standard and the custom version
 * we use) and I really don't want to do that. 
 */
kern_return_t ocsp_server_certFetch (
	mach_port_t serverport,
	audit_token_t auditToken,
	Data cert_url,
	mach_msg_type_number_t cert_urlCnt,
	Data *cert_data,
	mach_msg_type_number_t *cert_dataCnt)
{
	ServerActivity();
	CSSM_DATA urlData = { cert_urlCnt, (uint8 *)cert_url};
	CSSM_DATA certData = {0, NULL};
	kern_return_t krtn;
	
	/* preparing for net fetch: enable another thread */
	OcspdServer::active().longTermActivity();

	if(!callerHasNetworkEntitlement(auditToken)) {
		/* client can't access network */
		krtn = CSSMERR_APPLETP_NETWORK_FAILURE;
	}
	else {
		krtn = ocspdNetFetch(OcspdServer::active().alloc(), urlData, LT_Cert, certData);
	}
	/* if we got any data, sent it back to client */
	if(krtn == 0) {
		if(certData.Length == 0) {
			ocspdErrorLog("ocsp_server_certFetch: no cert found\n");
			krtn = CSSMERR_APPLETP_NETWORK_FAILURE;
		}
		else {
			/* 
			 * We used server's persistent Allocator to alloc this; tell 
			 * MachServer to dealloc after the RPC completes
			 */
			passDataToCaller(certData, cert_data, cert_dataCnt);
		}
	}
	ocspdCrlDebug("ocsp_server_certFetch returning %lu bytes", certData.Length);
	return krtn;
}

/*
 * Get CRL status, given serial number and issuer (or URL).
 */
kern_return_t ocsp_server_crlStatus (
    mach_port_t serverport,
    Data serial_number,
    mach_msg_type_number_t serial_numberCnt,
    Data cert_issuers,
    mach_msg_type_number_t cert_issuersCnt,
    Data crl_issuer,                            // optional
    mach_msg_type_number_t crl_issuerCnt,
    Data crl_url,                               // optional
    mach_msg_type_number_t crl_urlCnt)
{
	ServerActivity();
	kern_return_t krtn;
	struct stat sb;
	size_t dataLen = (crl_issuerCnt) ? crl_issuerCnt : crl_urlCnt;
	unsigned char *dataPtr = (unsigned char *)((crl_issuerCnt) ? crl_issuer : crl_url);
	if(!dataLen || !dataPtr) {
		return CSSMERR_TP_INTERNAL_ERROR;
	}
	bool crlValid = false;
	crl_names_t names;
	names.crlFile = crlGenerateFileName(dataPtr, dataLen, gCrlPath, ".crl");
	names.pemFile = crlGenerateFileName(dataPtr, dataLen, gCrlPath, ".pem");
	names.updateFile = crlGenerateFileName(dataPtr, dataLen, gCrlPath, ".update");
	names.revokedFile = crlGenerateFileName(dataPtr, dataLen, gCrlPath, ".revoked");
	if(!names.crlFile || !names.pemFile || !names.updateFile || !names.revokedFile) {
		krtn = CSSMERR_TP_INTERNAL_ERROR;
		goto crlStatus_cleanup;
	}

	/* Are we currently downloading this CRL? */
	{
		StLock<Mutex> _(gListLock); /* lock before examining lists */
		if(gDownloadList == NULL) {
			gDownloadList = CFArrayCreateMutable(kCFAllocatorDefault,
				0, &kCFTypeArrayCallBacks);
			if(!gDownloadList) {
				krtn = CSSMERR_TP_INTERNAL_ERROR; /* can't continue */
				goto crlStatus_cleanup;
			}
		}
		Boolean downloadInProgress = false;
		CFStringRef crlNameStr = CFStringCreateWithCString(kCFAllocatorDefault,
			names.crlFile, kCFStringEncodingUTF8);
		if(crlNameStr) {
			downloadInProgress = CFArrayContainsValue(gDownloadList,
				CFRangeMake(0, CFArrayGetCount(gDownloadList)), crlNameStr);
			CFRelease(crlNameStr);
		}
		if(downloadInProgress) {
			krtn = CSSMERR_APPLETP_NETWORK_FAILURE; /* busy, download not yet complete! */
			goto crlStatus_cleanup;
		}

		/* Add issuers to dictionary, so we can find them later */
		if(gIssuersDict == NULL) {
			gIssuersDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
				&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
			if(!gIssuersDict) {
				krtn = CSSMERR_TP_INTERNAL_ERROR; /* can't continue */
				goto crlStatus_cleanup;
			}
		}
		if(cert_issuers != NULL && cert_issuersCnt > 0) {
			CFStringRef pemNameStr = CFStringCreateWithCString(kCFAllocatorDefault,
				names.pemFile, kCFStringEncodingUTF8);
			if(pemNameStr) {
				CFDataRef pemData = CFDataCreate(kCFAllocatorDefault,
					(const UInt8 *)cert_issuers, (CFIndex)cert_issuersCnt);
				if(pemData) {
					CFDictionarySetValue(gIssuersDict, pemNameStr, pemData);
					CFRelease(pemData);
				}
				CFRelease(pemNameStr);
			}
		}
	}

	/* Check whether the CRL file is present */
	if(stat(names.crlFile, &sb) != 0) {
		/* Note that returning "not found" will trigger a subsequent call
		 * to crlFetch, which will first look for the CRL in our cache
		 * before attempting to fetch it from the network. */
		krtn = CSSMERR_APPLETP_CRL_NOT_FOUND;
		goto crlStatus_cleanup;
	}

	/* Check whether we have previously validated the CRL, and if so, whether
	 * the nextUpdate date has passed.
	 */
	if(stat(names.updateFile, &sb) == 0) {
		if(!crlUpdateValid(names.updateFile) ||
			!(stat(names.revokedFile, &sb) == 0)) {
			/* Remove invalid files and bail out */
			StLock<Mutex> _(gFileWriteLock);
			remove(names.updateFile);
			remove(names.revokedFile);
			remove(names.pemFile);
			remove(names.crlFile);

			krtn = CSSMERR_APPLETP_CRL_NOT_FOUND;
			goto crlStatus_cleanup;
		}
		crlValid = true;
	}

	if(crlValid) {
		char *serialStr = crlPrintableStringWithData((unsigned char*)serial_number,
			serial_numberCnt);
		if(serialStr) {
			if(crlSerialNumberRevoked(names.revokedFile, serialStr)) {
				krtn = CSSMERR_TP_CERT_REVOKED;
				ocspdCrlDebug("crlSignatureValid: found revoked serial number %s\n",
					serialStr);
			}
			else {
				krtn = CSSM_OK;
				ocspdCrlDebug("crlSignatureValid: CRL did not contain serial number %s\n",
					serialStr);
			}
			free(serialStr);
		}
		else {
			krtn = CSSMERR_TP_INTERNAL_ERROR;
		}
	}
	else {
		/* CRL file isn't present or isn't valid; need to download it */
		krtn = CSSMERR_APPLETP_CRL_NOT_FOUND;
	}

crlStatus_cleanup:
	if(names.updateFile) free(names.updateFile);
	if(names.revokedFile) free(names.revokedFile);
	if(names.pemFile) free(names.pemFile);
	if(names.crlFile) free(names.crlFile);

	return krtn;
}

/*
 * Fetch a CRL from the net.
 */
kern_return_t ocsp_server_crlFetch (
	mach_port_t serverport,
	audit_token_t auditToken,
	Data crl_url,
	mach_msg_type_number_t crl_urlCnt,
	Data crl_issuer,					// optional
	mach_msg_type_number_t crl_issuerCnt,
	boolean_t cache_read,
	boolean_t cache_write,
	Data verifyTime,
	mach_msg_type_number_t verifyTimeCnt,
	Data *crl_data,
	mach_msg_type_number_t *crl_dataCnt)
{
	ServerActivity();
	const CSSM_DATA urlData = {crl_urlCnt, (uint8 *)crl_url};
	CSSM_DATA crlData = {0, NULL};
	Allocator &alloc = OcspdServer::active().alloc();

	/*
	 * 1. Read from cache if enabled. Look up by issuer if we have it, else
	 *    look up by URL. Per Radar 4565280, the same CRL might be
	 *    vended from different URLs; we don't care where we got it 
	 *    from at this point as long as the client knew - by the absence
	 *    of a crlIssuer field in the crlDistributionPoints extension - 
	 *    that the issuer of the CRL is the same as the issuer of the cert
	 *    being verified. 
	 */
	if(cache_read) {
		const CSSM_DATA vfyTimeData = {verifyTimeCnt, (uint8 *)verifyTime};
		const CSSM_DATA issuerData  = {crl_issuerCnt, (uint8 *)crl_issuer};
		const CSSM_DATA *issuerPtr;
		const CSSM_DATA *urlPtr;
		bool brtn;
		
		if(crl_issuerCnt) {
			/* look up by issuer */
			issuerPtr = &issuerData;
			urlPtr    = NULL;
		}
		else {
			/* look up by URL */
			issuerPtr = NULL;
			urlPtr    = &urlData;
		}
		brtn = crlCacheLookup(alloc, urlPtr, issuerPtr, vfyTimeData, crlData);
		if(brtn) {
			/* Cache hit: Pass CRL back to caller & dealloc */
			assert((crlData.Data != NULL) && (crlData.Length != 0));
			passDataToCaller(crlData, crl_data, crl_dataCnt);
			return 0;
		}
	}

	/*
	 * 2. Obtain from net
	 */
	CSSM_RETURN crtn;

	/* preparing for net fetch: enable another thread */
	OcspdServer::active().longTermActivity();

	if(!callerHasNetworkEntitlement(auditToken)) {
		/* client can't access network */
		return CSSMERR_APPLETP_NETWORK_FAILURE;
	}

	size_t dataLen = (crl_issuerCnt) ? crl_issuerCnt : crl_urlCnt;
	unsigned char *dataPtr = (unsigned char *)((crl_issuerCnt) ? crl_issuer : crl_url);
	if(!dataLen || !dataPtr) {
		return CSSMERR_TP_INTERNAL_ERROR;
	}
	async_fetch_t *fetchParams =
		(async_fetch_t *)malloc(sizeof(async_fetch_t));
	if(!fetchParams) {
		return CSSMERR_TP_INTERNAL_ERROR;
	}
	memset(fetchParams, 0, sizeof(async_fetch_t));
	fetchParams->alloc = &alloc;
	fetchParams->url.Data = (uint8*)malloc(urlData.Length);
	fetchParams->url.Length = urlData.Length;
	memmove(fetchParams->url.Data, urlData.Data, urlData.Length);
	fetchParams->lfType = LT_Crl;
	fetchParams->outFile = crlGenerateFileName(dataPtr, dataLen, gCrlPath, ".crl");
	fetchParams->crlNames.crlFile = crlGenerateFileName(dataPtr, dataLen,
		gCrlPath, ".crl");
	fetchParams->crlNames.pemFile = crlGenerateFileName(dataPtr, dataLen,
		gCrlPath, ".pem");
	fetchParams->crlNames.updateFile = crlGenerateFileName(dataPtr, dataLen,
		gCrlPath, ".update");
	fetchParams->crlNames.revokedFile = crlGenerateFileName(dataPtr, dataLen,
		gCrlPath, ".revoked");

	crtn = ocspdStartNetFetch(fetchParams);

	if(!crtn) {
		/* cycle the run loop until we finish downloading or time out */
		CFAbsoluteTime stopTime = CFAbsoluteTimeGetCurrent() + CRL_MAX_DOWNLOAD_WAIT;
		while (!fetchParams->finished) {
			(void)CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, TRUE);
			CFAbsoluteTime curTime = CFAbsoluteTimeGetCurrent();
			if (curTime > stopTime) {
				StLock<Mutex> _(gParamsLock);
				if(!fetchParams->finished) {
					/* fetchParams are now dead to us; other thread will clean up */
					fetchParams->freeOnDone = 1;
					ocspdCrlDebug("ocsp_server_crlFetch waited for %f seconds",
						CRL_MAX_DOWNLOAD_WAIT);
					return CSSMERR_APPLETP_NETWORK_FAILURE;
				}
			}
		}
		/* we finished the download and can provide data right away */
		crtn = fetchParams->result;
		if(fetchParams->fetched.Data && fetchParams->fetched.Length) {
			/* Pass CRL back to caller & schedule dealloc after we return */
			crlData = fetchParams->fetched;
			passDataToCaller(crlData, crl_data, crl_dataCnt);
			ocspdCrlDebug("ocsp_server_crlFetch got %lu bytes from net", crlData.Length);
		}
	}

	/* clean up allocations */
	if(fetchParams->url.Data) {
		free(fetchParams->url.Data);
	}
	if(fetchParams->outFile) {
		free(fetchParams->outFile);
	}
	if(fetchParams->crlNames.crlFile) {
		free(fetchParams->crlNames.crlFile);
	}
	if(fetchParams->crlNames.pemFile) {
		free(fetchParams->crlNames.pemFile);
	}
	if(fetchParams->crlNames.updateFile) {
		free(fetchParams->crlNames.updateFile);
	}
	if(fetchParams->crlNames.revokedFile) {
		free(fetchParams->crlNames.revokedFile);
	}
	free(fetchParams);

	if(crlData.Data == NULL || crlData.Length == 0) {
		ocspdCrlDebug("ocsp_server_crlFetch will not cache (length=%lu, data=%p)",
			crlData.Length, crlData.Data);
		return crtn;
	}

	/*
	 * 3. Add to cache if enabled
	 */
	if(cache_write) {
		crlCacheAdd(crlData, urlData);
		ocspdCrlDebug("ocsp_server_crlFetch added CRL to cache db");
	}

	return 0;
}

kern_return_t ocsp_server_crlRefresh
(
	mach_port_t serverport,
	uint32_t stale_days,
	uint32_t expire_overlap_seconds,
	boolean_t purge_all,
	boolean_t full_crypto_verify)
{
	/* preparing for possible CRL verify, requiring an RPC to ourself
     * for Trust Settings fetch. enable another thread. */
	ServerActivity();
	OcspdServer::active().longTermActivity();
	crlCacheRefresh(stale_days, expire_overlap_seconds, purge_all, 
		full_crypto_verify, true);
	return 0;
}

kern_return_t ocsp_server_crlFlush(
	mach_port_t serverport,
	Data cert_url,
	mach_msg_type_number_t cert_urlCnt)
{
	ServerActivity();
	CSSM_DATA urlData = {cert_urlCnt, (uint8 *)cert_url};
	crlCacheFlush(urlData);
	return 0;
}

#pragma mark ----- MachServer::Timer subclass to handle periodic flushes of DB caches -----

#define OCSPD_REFRESH_DEBUG		0

#if		!OCSPD_REFRESH_DEBUG
/* fire a minute after we launch, then once a week */
#define OCSPD_TIMER_FIRST		(60.0)
#define OCSPD_TIMER_INTERVAL	(60.0 * 60.0 * 24.0 * 7.0)

#else
#define OCSPD_TIMER_FIRST		(10.0)
#define OCSPD_TIMER_INTERVAL	(60.0)
#endif

void OcspdServer::OcspdTimer::action()
{
	secdebug("ocspdRefresh", "OcspdTimer firing");
	ocspdDbCacheFlushStale();
	crlCacheRefresh(0,		// stale_days
					0,		// expire_overlap_seconds, 
					false,	// purge_all
					false,	// full_crypto_verify
					false);	// do Refresh
	Time::Interval nextFire = OCSPD_TIMER_INTERVAL;
	secdebug("ocspdRefresh", "OcspdTimer scheduling");
	mServer.setTimer(this, nextFire);
}

#pragma mark ----- OcspdServer, trivial subclass of MachPlusPlus::MachServer -----

OcspdServer::OcspdServer(const char *bootstrapName) 
	: MachServer(bootstrapName),
	  mAlloc(Allocator::standard()),
	  mTimer(*this)
{
	maxThreads(MAX_OCSPD_THREADS);
	
	/* schedule a refresh */
	Time::Interval nextFire = OCSPD_TIMER_FIRST;
	setTimer(&mTimer, nextFire);
}

OcspdServer::~OcspdServer()
{
}

/* the boundary between MachServer and MIG-oriented code */

boolean_t ocspd_server(mach_msg_header_t *, mach_msg_header_t *);

boolean_t OcspdServer::handle(mach_msg_header_t *in, mach_msg_header_t *out)
{
	ocspdDebug("OcspdServer::handle msg_id %d", (int)in->msgh_id);
	return ocspd_server(in, out);
}

