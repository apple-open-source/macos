/*
 * Copyright (c) 2012-2014 Apple Inc. All rights reserved.
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
 * ocspdNetwork.mm - Network support for ocspd and CRL/cert fetch
 */

#if OCSP_DEBUG
#define OCSP_USE_SYSLOG	1
#endif
#include <security_ocspd/ocspdDebug.h>
#include "appleCrlIssuers.h"
#include "ocspdNetwork.h"
#include <security_ocspd/ocspdUtils.h>
#include <CoreFoundation/CoreFoundation.h>
#include <security_cdsa_utils/cuEnc64.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <dispatch/dispatch.h>
#include <Security/cssmapple.h>
#include <security_utilities/cfutilities.h>
#include <CoreServices/CoreServices.h>
#include <SystemConfiguration/SCDynamicStoreCopySpecific.h>
#include <SystemConfiguration/SCNetworkReachability.h>

#include <Foundation/Foundation.h>

/* enable deprecated function declarations */
#ifndef LDAP_DEPRECATED
#define LDAP_DEPRECATED 1
#endif
#include <LDAP/ldap.h>

/* useful macros for CF */
#define CFReleaseSafe(CF) { CFTypeRef _cf = (CF); if (_cf) CFRelease(_cf); }
#define CFReleaseNull(CF) { CFTypeRef _cf = (CF); \
	if (_cf) { (CF) = NULL; CFRelease(_cf); } }

extern Mutex gParamsLock;
extern Mutex gFileWriteLock;
extern Mutex gListLock;
extern CFMutableArrayRef gDownloadList;
extern CFMutableDictionaryRef gIssuersDict;
extern CFAbsoluteTime gLastActivity;

extern bool crlSignatureValid(
	const char *crlFileName,
	const char *issuersFileName,
	const char *updateFileName,
	const char *revokedFileName);

extern int crlCheckCachePath();

static const char* SYSTEM_KC_PATH = "/Library/Keychains/System.keychain";

/* how long to wait for more data to come in before we give up */
#define READ_STREAM_TIMEOUT	7.0

/* read buffer size */
#define READ_BUFFER_SIZE	4096

/* post buffer size */
#define POST_BUFFER_SIZE	1024

/* max age of upstream cached response */
#define CACHED_MAX_AGE		300


#pragma mark -- SecURLLoader --

@interface SecURLLoader : NSObject
{
	NSURL *_url;
	NSMutableURLRequest *_request;
	NSURLConnection *_connection;
	NSMutableData *_receivedData;
	CFAbsoluteTime _timeToGiveUp;
	NSData *_data;
	NSTimer *_timer;
	NSError *_error;
	BOOL _finished;
}

- (id)initWithURL:(NSURL *)theURL;
- (NSMutableURLRequest *)request;
- (void)startLoad;
- (void)syncLoad;
- (void)cancelLoad;
- (void)resetTimeout;
- (BOOL)finished;
- (NSData *)data;
- (NSError*)error;
@end

@implementation SecURLLoader

- (id)initWithURL:(NSURL *)theURL
{
	if (self = [super init]) {
		_url = [theURL copy];
	}
	return self;
}

- (void) dealloc
{
	[self cancelLoad];
	[_url release];
	[_request release];
	[_data release];
	[_error release];
	[super dealloc];
}

- (NSMutableURLRequest *)request
{
	if (!_request) {
		_request = [[NSMutableURLRequest alloc] initWithURL:_url];
	}

	// Set cache policy to always load from origin and not the local cache
	[_request setCachePolicy:NSURLRequestReloadIgnoringLocalCacheData];

	return _request;
}

- (void)startLoad
{
	// Cancel any load currently in progress and clear instance variables
	[self cancelLoad];

	_finished = NO;

	// Create an empty data to receive bytes from the download
	_receivedData = [[NSMutableData alloc] init];

	// Start the download
	_connection = [[NSURLConnection alloc] initWithRequest:[self request] delegate:self];
	if (!_connection) {
		ocspdDebug("Failed to open connection (is network available?)");
		[self timeout];
		return;
	}

	// Start the timer
	[self resetTimeout];
	_timer = [NSTimer scheduledTimerWithTimeInterval:1.0 target:self
		selector:@selector(timeoutCheck)
		userInfo:nil repeats:YES];
	[_timer retain];
}

- (void)syncLoad
{
	[self startLoad];

	// cycle the run loop until we get a response or time out
	CFAbsoluteTime stopTime = CFAbsoluteTimeGetCurrent();
	while (![self finished]) {
		(void)CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, TRUE);
		CFAbsoluteTime curTime = CFAbsoluteTimeGetCurrent();
		if (curTime != stopTime) {
			stopTime = curTime;
		}
	}
}

- (void)cancelLoad
{
	if (_timer) {
		[_timer invalidate];
		[_timer release];
		_timer = nil;
	}
	if (_connection) {
		[_connection cancel];
		[_connection release];
		_connection = nil;
	}
	if (_receivedData) {
		// Hold onto the last data we received
		if (_data) [_data release];
		_data = _receivedData;
		_receivedData = nil;
	}
}

- (void)timeoutCheck
{
	if (_finished) {
		return; // already completed
	}
	if (_timeToGiveUp > CFAbsoluteTimeGetCurrent()) {
		return; // not time yet...
	}
	// give up and cancel the download
	[self cancelLoad];
	OSStatus err = CSSMERR_APPLETP_NETWORK_FAILURE;
	_error = [[NSError alloc] initWithDomain:NSOSStatusErrorDomain code:err userInfo:nil];
	_finished = YES;
}

- (void)resetTimeout
{
	gLastActivity = CFAbsoluteTimeGetCurrent();
	_timeToGiveUp = gLastActivity + READ_STREAM_TIMEOUT;
}

- (BOOL)finished
{
	return _finished;
}

- (NSData *)data
{
	return _data;
}

- (NSError *)error
{
	return _error;
}

/* NSURLConnection delegate methods */
- (void)connection:(NSURLConnection *)connection didReceiveData:(NSData *)newData
{
	if (![newData length])
		return;

	[_receivedData appendData:newData];
	[self resetTimeout];
}

- (void)connectionDidFinishLoading:(NSURLConnection *)connection
{
	[self cancelLoad];
	if (_error) {
		[_error release];
		_error = nil;
	}
	_finished = YES;
}

- (void)connection:(NSURLConnection *)connection didFailWithError:(NSError *)error
{
	//CFNetDiagnosticRef diagnostics = CFNetDiagnosticCreateWithURL(NULL, (CFURLRef)url);
	//%%% add debug code to print diagnostics

	[self cancelLoad];
	_error = [error retain];
	_finished = YES;
}

- (BOOL)connection:(NSURLConnection *)connection
	canAuthenticateAgainstProtectionSpace:(NSURLProtectionSpace *)protectionSpace
{
	return ([protectionSpace isProxy]);
}

- (void)connection:(NSURLConnection *)connection
	didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
	NSInteger failCount = [challenge previousFailureCount];
	NSURLProtectionSpace *protectionSpace = [challenge protectionSpace];
	if (failCount > 0) {
		#if OCSP_DEBUG
		NSLog(@"Cancelling authentication challenge (failure count = %ld). Is proxy password correct?", (long)failCount);
		#endif
		[[challenge sender] cancelAuthenticationChallenge:challenge];
		return;
	}
	// Try to look up our proxy credential in the System keychain.
	// Don't specify protocol (kSecProtocolTypeHTTPProxy, etc.) in our item since that
	// can cause it to be found by AuthBrokerAgent, and since it's running as the user,
	// it won't have access to the System keychain.
	//
	NSURLCredential *credential = NULL;
	SecKeychainRef keychain = NULL;
	NSString *host = [protectionSpace host];
	NSInteger port = [protectionSpace port];
	OSStatus status = SecKeychainOpen(SYSTEM_KC_PATH, &keychain);
	if (status == noErr) {
		// ask for attributes as well, so we can get the account name
		NSDictionary *query = [NSDictionary dictionaryWithObjectsAndKeys:
                               (id)keychain, kSecUseKeychain,
                               (id)kSecClassInternetPassword, kSecClass,
                               (id)host, kSecAttrServer,
                               (id)[NSNumber numberWithInteger:port], kSecAttrPort,
                               (id)kSecMatchLimitOne, kSecMatchLimit,
                               (id)[NSNumber numberWithBool:YES], kSecReturnAttributes,
                               (id)[NSNumber numberWithBool:YES], kSecReturnData,
                               (id)nil, (id)nil];

		CFDictionaryRef result = NULL;
		status = SecItemCopyMatching((CFDictionaryRef)query, (CFTypeRef *)&result);
		#if OCSP_DEBUG
		NSLog(@"SecItemCopyMatching returned %d looking up host=%@, port=%d",
			(int)status, host, (int)port);
		#endif
		if (!status && result) {
			if (CFDictionaryGetTypeID() == CFGetTypeID(result)) {
				NSString *account = [(NSDictionary *)result objectForKey:(id)kSecAttrAccount];
				NSData *passwordData = [(NSDictionary *)result objectForKey:(id)kSecValueData];
				NSString *password = [[NSString alloc] initWithData:passwordData encoding:NSUTF8StringEncoding];
				if (account && password) {
					#if OCSP_DEBUG
					NSLog(@"Found credential for %@:%d (length=%ld)", host, (int)port, (long)[password length]);
					#endif
					credential = [NSURLCredential credentialWithUser:account password:password persistence:NSURLCredentialPersistenceForSession];
				}
				[password release];
			}
			CFRelease(result);
		}
	}
	else {
		#if OCSP_DEBUG
		NSLog(@"SecKeychainOpen error %d", (int)status);
		#endif
	}
	if (keychain) {
		CFRelease(keychain);
	}
	if (credential) {
		#if OCSP_DEBUG
		NSLog(@"Authentication challenge received, setting proxy credential");
		#endif
		[[challenge sender] useCredential:credential forAuthenticationChallenge:challenge];
		return;
	}

	NSLog(@"Authentication challenge received for \"%@:%d\", unable to obtain proxy server credential from System keychain.", host, (int)port);
	static bool printedHint = false;
	if (!printedHint) {
		NSLog(@"You can specify the username and password for a proxy server with /usr/bin/security. Example:  sudo security add-internet-password -a squiduser -l \"HTTP Proxy\" -P 3128 -r 'http' -s localhost -w squidpass -U -T /usr/sbin/ocspd /Library/Keychains/System.keychain");
		printedHint = true;
	}
	// Cancel the challenge since we do not have a credential to present (14761252)
	[[challenge sender] cancelAuthenticationChallenge:challenge];
}

/* NSURLConnectionDataDelegate methods */
- (NSCachedURLResponse *)connection:(NSURLConnection *)connection willCacheResponse:(NSCachedURLResponse *)cachedResponse
{
	// Returning nil prevents the loader from passing the response to the URL cache sub-system for caching.
	return (NSCachedURLResponse *)nil;
}

@end

void enableAutoreleasePool(int enable);
void enableAutoreleasePool(int enable)
{
	static NSAutoreleasePool *_pool = NULL;
	if (!_pool) {
		_pool = [[NSAutoreleasePool alloc] init];
	}
	if (!enable && _pool) {
		[_pool drain];
	}
}

#pragma mark ----- OCSP fetch -----

/* POST method has Content-Type header line equal to "application/ocsp-request" */
static NSString* kContentType       = @"Content-Type";
static NSString* kAppOcspRequest    = @"application/ocsp-request";
static NSString* kContentLength     = @"Content-Length";
static NSString* kUserAgent         = @"User-Agent";
static NSString* kAppUserAgent      = @"ocspd/1.0.3";
static NSString* kCacheControl      = @"Cache-Control";

#if OCSP_DEBUG
#define DUMP_BLOBS	1
#endif

#define OCSP_GET_FILE	"/tmp/ocspGet"
#define OCSP_RESP_FILE	"/tmp/ocspResp"

#if		DUMP_BLOBS

static void writeBlob(
	const char *fileName,
	const char *whatIsIt,
	const unsigned char *data,
	unsigned dataLen)
{
	if(writeFile(fileName, data, dataLen)) {
		printf("***Error writing %s to %s\n", whatIsIt, fileName);
	}
	else {
		printf("...wrote %u bytes of %s to %s\n", dataLen, whatIsIt, fileName);
	}
}

#else

#define writeBlob(f,w,d,l)

#endif	/* DUMP_BLOBS */

/* OCSP fetch via HTTP using GET (preferred) or POST (if required) */

CSSM_RETURN ocspdHttpFetch(
    SecAsn1CoderRef		coder,
    const CSSM_DATA 	&url,
    const CSSM_DATA		&ocspReq,	// DER encoded
    CSSM_DATA			&fetched)	// mallocd in coder space and RETURNED
{
	CSSM_RETURN result = CSSM_OK;
	unsigned char *fullUrl = NULL;
	CFURLRef cfUrl = NULL;
	CFStringRef urlStr = NULL;
    CFDataRef postData = NULL;
    SecURLLoader *urlLoader = nil;
    bool done = false;
    size_t totalLen;
    CFIndex len;

	/* trim off possible NULL terminator from incoming URL */
	size_t urlLen = url.Length;
	if(url.Data[urlLen - 1] == '\0') {
		urlLen--;
	}

    /* base64 encode the OCSP request; that's used as a path */
	unsigned char *endp, *req64 = NULL;
	unsigned req64Len = 0;
	req64 = cuEnc64(ocspReq.Data, (unsigned)ocspReq.Length, &req64Len);
	if(req64 == NULL) {
		ocspdErrorLog("ocspdHttpFetch: error base64-encoding request\n");
		result = CSSMERR_TP_INTERNAL_ERROR;
		goto cleanup;
	}

	/* trim off trailing NULL and newline */
    endp = req64 + req64Len - 1;
	for(;;) {
		switch(*endp) {
			case '\0':
			case '\n':
			case '\r':
				endp--;
				req64Len--;
				break;
			default:
				done = true;
				break;
		}
		if(done) {
			break;
		}
	}

    /* sanity check length of request */
	if( (req64Len >= INT_MAX) || (urlLen > (INT_MAX - (1 + req64Len))) ) {
		/* long URL is long; concatenating these components would overflow totalLen */
		result = CSSMERR_TP_INVALID_DATA;
		goto cleanup;
	}

	if(urlLen && req64Len) {
		CFStringRef incomingURLStr = CFStringCreateWithBytes(NULL, (const UInt8 *)url.Data, urlLen, kCFStringEncodingUTF8, false);
		CFStringRef requestPathStr = CFStringCreateWithBytes(NULL, (const UInt8 *)req64, req64Len, kCFStringEncodingUTF8, false);
		if(incomingURLStr && requestPathStr) {
			/* percent-encode all reserved characters from RFC 3986 [2.2] */
			CFStringRef encodedRequestStr = CFURLCreateStringByAddingPercentEscapes(NULL,
				requestPathStr, NULL, CFSTR(":/?#[]@!$&'()*+,;="), kCFStringEncodingUTF8);
			if(encodedRequestStr) {
				CFMutableStringRef tempStr = CFStringCreateMutable(NULL, 0);
				if (tempStr) {
					CFStringAppend(tempStr, incomingURLStr);
					CFStringAppend(tempStr, CFSTR("/"));
					CFStringAppend(tempStr, encodedRequestStr);
					urlStr = (CFStringRef)tempStr;
				}
			}
			CFReleaseSafe(encodedRequestStr);
		}
		CFReleaseSafe(incomingURLStr);
		CFReleaseSafe(requestPathStr);
	}
	if(urlStr == NULL) {
		ocspdErrorLog("ocspdHttpFetch: error percent-encoding request\n");
		result = CSSMERR_TP_INTERNAL_ERROR;
		goto cleanup;
	}

    /* RFC 5019 says we MUST use the GET method if the URI is less than 256 bytes
     * (to enable response caching), otherwise we SHOULD use the POST method.
     */
	totalLen = CFStringGetLength(urlStr);
    if (totalLen < 256) {
        /* we can safely use GET */
		cfUrl = CFURLCreateWithString(NULL, urlStr, NULL);
    }
    else {
        /* request too big for GET; use POST instead */
        writeBlob(OCSP_GET_FILE, "OCSP Request as POST data", ocspReq.Data, (unsigned int)ocspReq.Length);
        postData = CFDataCreate(NULL, ocspReq.Data, ocspReq.Length);
        cfUrl = CFURLCreateWithBytes(NULL, url.Data, urlLen,
                                     kCFStringEncodingUTF8,
                                     NULL);
    }

    if(cfUrl) {
        /* create URL with explicit path (see RFC 2616 3.2.2, 5.1.2) */
        CFStringRef pathStr = CFURLCopyLastPathComponent(cfUrl);
        if(pathStr) {
            if (CFStringGetLength(pathStr) == 0) {
                CFURLRef tmpUrl = CFURLCreateCopyAppendingPathComponent(NULL, cfUrl, CFSTR(""), FALSE);
                CFRelease(cfUrl);
                cfUrl = tmpUrl;
            }
            CFRelease(pathStr);
        }
    }
    if(!cfUrl) {
        ocspdErrorLog("ocspdHttpFetch: CFURLCreateWithBytes returned NULL\n");
        result = CSSMERR_APPLETP_CRL_BAD_URI;
        goto cleanup;
    }

#if OCSP_DEBUG
	{
		size_t len = (fullUrl) ? totalLen : urlLen;
		char *ubuf = (char *)((fullUrl) ? fullUrl : url.Data);
		char *ustr = (char *)malloc(len + 1);
		memmove(ustr, ubuf, len);
		ustr[len] = '\0';
		ocspdDebug("ocspdHttpFetch via %s to URI %s\n",
                   (fullUrl) ? "GET" : "POST", ustr);
		free(ustr);
	}
#endif

@autoreleasepool {
	/* set up the URL request */
	urlLoader = [[SecURLLoader alloc] initWithURL:(NSURL*)cfUrl];
	if (urlLoader) {
		NSMutableURLRequest *request = [urlLoader request];
		if (postData) {
			[request setHTTPMethod:@"POST"];
			[request setHTTPBody:(NSData*)postData];
			// Content-Type header
			[request setValue:kAppOcspRequest forHTTPHeaderField:kContentType];
			// Content-Length header
			NSString *postLength = [NSString stringWithFormat:@"%lu",(unsigned long)[(NSData*)postData length]];
			[request setValue:postLength forHTTPHeaderField:kContentLength];
		}
		else {
			[request setHTTPMethod:@"GET"];
			// Cache-Control header
			NSString *maxAge = [NSString stringWithFormat:@"max-age=%d", CACHED_MAX_AGE];
			[request setValue:maxAge forHTTPHeaderField:kCacheControl];
		}
		// User-Agent header
		[request setValue:kAppUserAgent forHTTPHeaderField:kUserAgent];
	}

	/* load it! */
	[urlLoader syncLoad];

	/* return the response data */
	len = [[urlLoader data] length];
	if (!len || [urlLoader error]) {
		result = CSSMERR_APPLETP_NETWORK_FAILURE;
	}
	else {
		fetched.Data = (uint8 *)SecAsn1Malloc(coder, len);
		fetched.Length = len;
		memmove(fetched.Data, [[urlLoader data] bytes], len);
		writeBlob(OCSP_RESP_FILE, "OCSP Response", fetched.Data, (unsigned int)fetched.Length);
		result = CSSM_OK;
	}
    [urlLoader release];
}	/* end autorelease scope */
cleanup:
	CFReleaseSafe(postData);
	CFReleaseSafe(urlStr);
	CFReleaseSafe(cfUrl);
	if(fullUrl) {
		free(fullUrl);
	}
	if(req64) {
		free(req64);
	}
	return result;
}


#pragma mark ----- LDAP fetch -----

/*
 * LDAP attribute names, used if not present in URI.
 */
#define LDAP_ATTR_CERT		"cacertificate;binary"
#define LDAP_ATTR_CRL		"certificaterevocationlist;binary"

/*
 * Default LDAP options.
 */
#define LDAP_REFERRAL_DEFAULT	LDAP_OPT_ON

static CSSM_RETURN ldapRtnToCssm(
	int rtn)
{
	switch(rtn) {
		case LDAP_SERVER_DOWN:
		case LDAP_TIMEOUT:
		case LDAP_CONNECT_ERROR:
			return CSSMERR_APPLETP_CRL_SERVER_DOWN;
		case LDAP_PARAM_ERROR:
		case LDAP_FILTER_ERROR:
			return CSSMERR_APPLETP_CRL_BAD_URI;
		default:
			return CSSMERR_APPLETP_CRL_NOT_FOUND;
	}
}

static CSSM_RETURN ldapFetch(
	Allocator			&alloc,
	const CSSM_DATA 	&url,
	LF_Type				lfType,
	CSSM_DATA			&fetched)	// mallocd in alloc space and RETURNED
{
	BerValue 		**value = NULL;
	LDAPURLDesc 	*urlDesc = NULL;
	int 			rtn;
	LDAPMessage 	*msg = NULL;
	LDAP 			*ldap = NULL;
	LDAPMessage 	*entry = NULL;
	bool 			mallocdString = false;
	char 			*urlStr;
	int 			numEntries;
	CSSM_RETURN 	ourRtn = CSSM_OK;
	/* attr input to ldap_search_s() */
	char			*attrArray[2];
	char			**attrArrayP = NULL;

	/* don't assume URL string is NULL terminated */
	if(url.Data[url.Length - 1] == '\0') {
		urlStr = (char *)url.Data;
	}
	else {
		urlStr = (char *)malloc(url.Length + 1);
		memmove(urlStr, url.Data, url.Length);
		urlStr[url.Length] = '\0';
		mallocdString = true;
	}

	/* break up the URL into something usable */
	rtn = ldap_url_parse(urlStr, &urlDesc);
	if(rtn) {
		ocspdErrorLog("ldap_url_parse returned %d", rtn);
        if(mallocdString) {
            free(urlStr);
        }
		return CSSMERR_APPLETP_CRL_BAD_URI;
	}

	/*
	 * Determine what attr we're looking for.
	 */
	if((urlDesc->lud_attrs != NULL) &&		// attrs present in URL
	   (urlDesc->lud_attrs[0] != NULL) &&	// at least one attr present
	   (urlDesc->lud_attrs[1] == NULL))	{
		/*
		 * Exactly one attr present in the caller-specified URL;
		 * assume that this is exactly what we want.
		 */
		attrArrayP = &urlDesc->lud_attrs[0];
	}
	else {
		/* use caller-specified attr */
		switch(lfType) {
			case LT_Crl:
				attrArray[0] = (char *)LDAP_ATTR_CRL;
				break;
			case LT_Cert:
				attrArray[0] = (char *)LDAP_ATTR_CERT;
				break;
			default:
				printf("***ldapFetch screwup: bogus lfType (%d)\n",
					(int)lfType);
				return CSSMERR_CSSM_INTERNAL_ERROR;
		}
		attrArray[1] = NULL;
		attrArrayP = &attrArray[0];
	}

	/* establish connection */
	rtn = ldap_initialize(&ldap, urlStr);
	if(rtn) {
		ocspdErrorLog("ldap_initialize returned %d\n", rtn);
        if(mallocdString) {
            free(urlStr);
        }
		return ldapRtnToCssm(rtn);
	}
	/* subsequent errors to cleanup: */
	rtn = ldap_simple_bind_s(ldap, NULL, NULL);
	if(rtn) {
		ocspdErrorLog("ldap_simple_bind_s returned %d\n", rtn);
		ourRtn = ldapRtnToCssm(rtn);
		goto cleanup;
	}

	rtn = ldap_set_option(ldap, LDAP_OPT_REFERRALS, LDAP_REFERRAL_DEFAULT);
	if(rtn) {
		ocspdErrorLog("ldap_set_option(referrals) returned %d\n", rtn);
		ourRtn = ldapRtnToCssm(rtn);
		goto cleanup;
	}

	rtn = ldap_search_s(
		ldap,
		urlDesc->lud_dn,
		LDAP_SCOPE_SUBTREE,
		urlDesc->lud_filter,
		urlDesc->lud_attrs,
		0, 			// attrsonly
		&msg);
	if(rtn) {
		ocspdErrorLog("ldap_search_s returned %d\n", rtn);
		ourRtn = ldapRtnToCssm(rtn);
		goto cleanup;
	}

	/*
	 * We require exactly one entry (for now).
	 */
	numEntries = ldap_count_entries(ldap, msg);
	if(numEntries != 1) {
		ocspdErrorLog("tpCrlViaLdap: numEntries %d\n", numEntries);
		ourRtn = CSSMERR_APPLETP_CRL_NOT_FOUND;
		goto cleanup;
	}

	entry = ldap_first_entry(ldap, msg);
    if(entry) {
        ocspdErrorLog("ldapFetch: first entry %p\n", entry);
    }
	value = ldap_get_values_len(ldap, msg, attrArrayP[0]);
	if(value == NULL) {
		ocspdErrorLog("Error on ldap_get_values_len\n");
		ourRtn = CSSMERR_APPLETP_CRL_NOT_FOUND;
		goto cleanup;
	}

	fetched.Length = value[0]->bv_len;
	fetched.Data = (uint8 *)alloc.malloc(fetched.Length);
	memmove(fetched.Data, value[0]->bv_val, fetched.Length);

	ldap_value_free_len(value);
	ourRtn = CSSM_OK;
cleanup:
	if(msg) {
		ldap_msgfree(msg);
	}
	if(mallocdString) {
		free(urlStr);
	}
	ldap_free_urldesc(urlDesc);
	rtn = ldap_unbind(ldap);
	if(rtn) {
		ocspdErrorLog("Error %d on ldap_unbind\n", rtn);
		/* oh well */
	}
	return ourRtn;
}

#pragma mark ----- HTTP fetch via GET -----

/* fetch via HTTP */
static CSSM_RETURN httpFetch(
	Allocator			&alloc,
	const CSSM_DATA 	&url,
	LF_Type				lfType,
	CSSM_DATA			&fetched)	// mallocd in alloc space and RETURNED
{
	#pragma unused (lfType)
	ocspdHttpDebug("httpFetch: start\n");

	CSSM_RETURN result = CSSM_OK;
	CFURLRef cfUrl = NULL;
    SecURLLoader *urlLoader = nil;
	CFAbsoluteTime startTime, stopTime;
    CFIndex len;

	/* trim off possible NULL terminator from incoming URL */
	CSSM_DATA theUrl = url;
	if(theUrl.Data[theUrl.Length - 1] == '\0') {
		theUrl.Length--;
	}

	/* create URL with explicit path (see RFC 2616 3.2.2, 5.1.2) */
	cfUrl = CFURLCreateWithBytes(NULL,
                                 theUrl.Data, theUrl.Length,
                                 kCFStringEncodingUTF8,
                                 NULL);
	if(cfUrl) {
		CFStringRef pathStr = CFURLCopyLastPathComponent(cfUrl);
		if(pathStr) {
			if (CFStringGetLength(pathStr) == 0) {
				CFURLRef tmpUrl = CFURLCreateCopyAppendingPathComponent(NULL,
					cfUrl, CFSTR(""), FALSE);
				CFRelease(cfUrl);
				cfUrl = tmpUrl;
			}
			CFRelease(pathStr);
		}
	}
	if(!cfUrl) {
		ocspdErrorLog("httpFetch: CFURLCreateWithBytes returned NULL\n");
		result = CSSMERR_APPLETP_CRL_BAD_URI;
		goto cleanup;
	}

	#if OCSP_DEBUG
	{
		char *ustr = (char *)malloc(theUrl.Length + 1);
		memmove(ustr, theUrl.Data, theUrl.Length);
		ustr[theUrl.Length] = '\0';
		ocspdDebug("httpFetch: GET URI %s\n", ustr);
		free(ustr);
	}
	#endif

@autoreleasepool {
    /* set up the URL request */
    urlLoader = [[SecURLLoader alloc] initWithURL:(NSURL*)cfUrl];
    if (urlLoader) {
        NSMutableURLRequest *request = [urlLoader request];
        [request setHTTPMethod:@"GET"];
        // User-Agent header
        [request setValue:kAppUserAgent forHTTPHeaderField:kUserAgent];
    }

    /* load it! */
    startTime = stopTime = CFAbsoluteTimeGetCurrent();
    [urlLoader syncLoad];
	stopTime = CFAbsoluteTimeGetCurrent();

    /* return the data */
    len = [[urlLoader data] length];
    if (!len || [urlLoader error]) {
        result = CSSMERR_APPLETP_NETWORK_FAILURE;
    }
	else {
		ocspdDebug("httpFetch: total %lu bytes read in %f seconds\n",
			       (unsigned long)len, stopTime-startTime);
		fetched.Data = (uint8 *)alloc.malloc(len);
		fetched.Length = len;
		memmove(fetched.Data, [[urlLoader data] bytes], len);
		result = CSSM_OK;
		#if OCSP_DEBUG
		writeBlob("/tmp/httpGetFile", "HTTP Fetch", fetched.Data, (unsigned int)fetched.Length);
		#endif
	}
    [urlLoader release];
}	/* end autorelease scope */
cleanup:
	CFReleaseSafe(cfUrl);

	return result;
}

/* Fetch cert or CRL from net, we figure out the schema */
CSSM_RETURN ocspdNetFetch(
	Allocator			&alloc,
	const CSSM_DATA 	&url,
	LF_Type				lfType,
	CSSM_DATA			&fetched)	// mallocd in alloc space and RETURNED
{
	#if OCSP_DEBUG
	{
		char *ustr = (char *)malloc(url.Length + 1);
		memmove(ustr, url.Data, url.Length);
		ustr[url.Length] = '\0';
		ocspdDebug("ocspdNetFetch: fetching from URI %s\n", ustr);
		free(ustr);
	}
	#endif

	if(url.Length < 5) {
		return CSSMERR_APPLETP_CRL_BAD_URI;
	}
	if(!strncmp((char *)url.Data, "ldap:", 5)) {
		return ldapFetch(alloc, url, lfType, fetched);
	}
	if(!strncmp((char *)url.Data, "http:", 5) ||
	   !strncmp((char *)url.Data, "https:", 6)) {
		return httpFetch(alloc, url, lfType, fetched);
	}
	return CSSMERR_APPLETP_CRL_BAD_URI;
}

/* Maximum CRL length to consider putting in the cache db (128KB) */
#define CRL_MAX_DATA_LENGTH (1024*128)

/* Post-process network fetched data after finishing download. */
CSSM_RETURN ocspdFinishNetFetch(
	async_fetch_t *fetchParams)
{
	CSSM_RETURN crtn = CSSMERR_APPLETP_NETWORK_FAILURE;
	if(!fetchParams) {
		return crtn;
	}
	StLock<Mutex> _(gParamsLock); /* lock before accessing parameters */
	if(fetchParams->result != CSSM_OK) {
		ocspdErrorLog("ocspdFinishNetFetch: CRL not found on net");
		crtn = fetchParams->result;
	}
	else if(fetchParams->fetched.Length == 0) {
		ocspdErrorLog("ocspdFinishNetFetch: no CRL data found");
		crtn = CSSMERR_APPLETP_NETWORK_FAILURE;
	}
	else if(fetchParams->fetched.Length > CRL_MAX_DATA_LENGTH) {
		if (fetchParams->fetched.Data) {
			/* Write oversize CRL data to file */
			StLock<Mutex> w_(gFileWriteLock);
			crlCheckCachePath();
			int rtn = writeFile(fetchParams->outFile, fetchParams->fetched.Data,
				(unsigned int)fetchParams->fetched.Length);
			if(rtn) {
				ocspdErrorLog("Error %d writing %s\n", rtn, fetchParams->outFile);
			}
			else {
				ocspdCrlDebug("ocspdFinishNetFetch wrote %lu bytes to %s",
					fetchParams->fetched.Length, fetchParams->outFile);

				if(chmod(fetchParams->outFile, 0644)) {
					ocspdErrorLog("ocspdFinishNetFetch: chmod error %d for %s",
						errno, fetchParams->outFile);
				}
			}
			(*(fetchParams->alloc)).free(fetchParams->fetched.Data);
			fetchParams->fetched.Data = NULL;
		}
		crtn = CSSMERR_APPLETP_NETWORK_FAILURE;
	}
	return crtn;
}

/* Fetch cert or CRL from net asynchronously. */
static void ocspdNetFetchAsync(
	void *context)
{
	async_fetch_t *params = (async_fetch_t *)context;
	ocspdCrlDebug("ocspdNetFetchAsync with context %p", context);
	CSSM_RETURN crtn = 0;
	CFStringRef fileNameStr = NULL;
	CFStringRef pemNameStr = NULL;
	CFAbsoluteTime fetchTime, verifyTime;
	CFAbsoluteTime startTime = CFAbsoluteTimeGetCurrent();
	Boolean downloadInProgress = false;
	Boolean wroteFile = false;
	Boolean isCRL = false;

	if(params) {
		StLock<Mutex> _(gParamsLock); /* lock before accessing parameters */
		params->finished = 0;
		isCRL = (params->lfType == LT_Crl);
		if(params->crlNames.pemFile) {
			pemNameStr = CFStringCreateWithCString(kCFAllocatorDefault,
				params->crlNames.pemFile, kCFStringEncodingUTF8);
		}
		if(params->outFile) {
			fileNameStr = CFStringCreateWithCString(kCFAllocatorDefault,
				params->outFile, kCFStringEncodingUTF8);
		}
		if(fileNameStr) {
			/* make sure we aren't already downloading this file */
			StLock<Mutex> _(gListLock); /* lock before examining list */
			if(gDownloadList == NULL) {
				gDownloadList = CFArrayCreateMutable(kCFAllocatorDefault,
					0, &kCFTypeArrayCallBacks);
				crtn = (gDownloadList) ? crtn : CSSMERR_TP_INTERNAL_ERROR;
				params->result = crtn;
			}
			if(!crtn) {
				downloadInProgress = CFArrayContainsValue(gDownloadList,
					CFRangeMake(0, CFArrayGetCount(gDownloadList)), fileNameStr);
				if(!downloadInProgress) {
					/* add this filename to the global list which tells other
					 * callers of the crlStatus MIG function that we are
					 * already downloading this file.
					 */
					CFArrayAppendValue(gDownloadList, fileNameStr);
				} else {
					/* already downloading; indicate "busy, try later" status */
					crtn = CSSMERR_APPLETP_NETWORK_FAILURE;
					params->result = crtn;
				}
			}
		}
	}

	if(params && !crtn && !downloadInProgress) {
		/* fetch data into buffer */
		crtn = ocspdNetFetch(*(params->alloc),
			params->url, params->lfType, params->fetched);
		{
			StLock<Mutex> _(gParamsLock);
			params->result = crtn;
		}
		/* potentially write data to file */
		crtn = ocspdFinishNetFetch(params);
		{
			StLock<Mutex> _(gParamsLock);
			params->result = crtn;
			wroteFile = (!params->fetched.Data && params->fetched.Length > CRL_MAX_DATA_LENGTH);
		}
		fetchTime = CFAbsoluteTimeGetCurrent() - startTime;
		ocspdCrlDebug("%f seconds to download file", fetchTime);

		if(isCRL && wroteFile) {
			/* write issuers to .pem file */
			StLock<Mutex> _(gListLock); /* lock before examining list */
			CFDataRef issuersData = NULL;
			if(gIssuersDict) {
				issuersData = (CFDataRef)CFDictionaryGetValue(gIssuersDict,
					pemNameStr);
			} else {
				ocspdCrlDebug("No issuers available for %s",
					params->crlNames.pemFile);
				gIssuersDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
					&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
			}
			if(!issuersData) {
				/* add the Apple issuers if we have nothing else */
				issuersData = CFDataCreate(kCFAllocatorDefault,
					(const UInt8 *)Apple_CRL_Issuers, (CFIndex)Apple_CRL_Issuers_Length);
				if(issuersData) {
					CFDictionarySetValue(gIssuersDict, pemNameStr, issuersData);
					CFRelease(issuersData);
				}
			}
			if(issuersData) {
				StLock<Mutex> _(gFileWriteLock); /* obtain lock before writing */
				crlCheckCachePath();
				int rtn = writeFile(params->crlNames.pemFile,
					(const unsigned char *)CFDataGetBytePtr(issuersData),
					(unsigned int)CFDataGetLength(issuersData));
				if(rtn) {
					ocspdErrorLog("Error %d writing %s\n",
						rtn, params->crlNames.pemFile);
				}
				else if(chmod(params->crlNames.pemFile, 0644)) {
					ocspdErrorLog("ocsp_server_crlStatus: chmod error %d for %s",
						errno, params->crlNames.pemFile);
				}
			}
		}

		if(isCRL && wroteFile) {
			/* validate .crl signature (creates .update and .revoked files) */
			crlSignatureValid(params->crlNames.crlFile,
				params->crlNames.pemFile,
				params->crlNames.updateFile,
				params->crlNames.revokedFile);
			verifyTime = ( CFAbsoluteTimeGetCurrent() - startTime ) - fetchTime;
			ocspdCrlDebug("%f seconds to validate CRL", verifyTime);
		}

		if(fileNameStr) {
			/* all finished downloading, so remove filename from global list */
			StLock<Mutex> _(gListLock);
			CFIndex idx =  CFArrayGetFirstIndexOfValue(gDownloadList,
				CFRangeMake(0, CFArrayGetCount(gDownloadList)), fileNameStr);
			if(idx >= 0) {
				CFArrayRemoveValueAtIndex(gDownloadList, idx);
			}
		}
	}

	if(params) {
		StLock<Mutex> _(gParamsLock);
		params->finished = 1;

		if(params->freeOnDone) {
			/* caller does not expect a reply; we must clean up everything. */
			if(params->url.Data) {
				free(params->url.Data);
			}
			if(params->outFile) {
				free(params->outFile);
			}
			if(params->crlNames.crlFile) {
				free(params->crlNames.crlFile);
			}
			if(params->crlNames.pemFile) {
				free(params->crlNames.pemFile);
			}
			if(params->crlNames.updateFile) {
				free(params->crlNames.updateFile);
			}
			if(params->crlNames.revokedFile) {
				free(params->crlNames.revokedFile);
			}
			if(params->fetched.Data) {
				(*(params->alloc)).free(params->fetched.Data);
			}
			free(params);
		}
	}

	if(fileNameStr) {
		CFRelease(fileNameStr);
	}
	if(pemNameStr) {
		CFRelease(pemNameStr);
	}
}

/*
 * This is a basic "we have an internet connection" check.
 * It tests whether IP 0.0.0.0 is routable.
 */
bool shouldAttemptNetFetch()
{
	bool result = true;
	SCNetworkReachabilityRef scnr;
	struct sockaddr_in addr;
	bzero(&addr, sizeof(addr));
	addr.sin_len = sizeof(addr);
	addr.sin_family = AF_INET;

	scnr = SCNetworkReachabilityCreateWithAddress(kCFAllocatorDefault, (const struct sockaddr *)&addr);
	if (scnr) {
		SCNetworkReachabilityFlags flags = 0;
		if (SCNetworkReachabilityGetFlags(scnr, &flags)) {
			if ((flags & kSCNetworkReachabilityFlagsReachable) == 0)
				result = false;
		}
		CFRelease(scnr);
	}
	else { ocspdDebug("Failed to create reachability reference"); }
	ocspdDebug("Finished reachability check, result=%s", (result) ? "YES" :"NO");

	return result;
}

/* Kick off net fetch of a cert or a CRL and return immediately. */
CSSM_RETURN ocspdStartNetFetch(
	async_fetch_t		*fetchParams)
{
	if (!shouldAttemptNetFetch())
		return CSSMERR_APPLETP_NETWORK_FAILURE;

	dispatch_queue_t queue = dispatch_get_global_queue(
		DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);

	ocspdCrlDebug("ocspdStartNetFetch with context %p", (void*)fetchParams);

	dispatch_async_f(queue, fetchParams, ocspdNetFetchAsync);

	return 0;
}

