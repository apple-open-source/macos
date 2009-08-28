/*
 * Copyright (c) 2000-2009 Apple Inc. All rights reserved.
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

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCSchemaDefinitions.h>
#include <CoreServices/CoreServices.h>
#include <CoreServices/CoreServicesPriv.h>
#include <netdb.h>
#include <syslog.h>
#include "webdavlib.h"

#define STREAM_EVENT_BUFSIZE 4096

// This limits how many times a request will be retried
// due to receiving EAGAIN from the handleXXXError() routines.
#define WEBDAVLIB_MAX_AGAIN_COUNT 10

// This is what we send as the User Agent header
#define WEBAVLIB_USER_AGENT_STRING "WebDAVLib/1.1"

/* Macro to simplify common CFRelease usage */
#define CFReleaseNull(obj) do { if(obj != NULL) { CFRelease(obj); obj = NULL; } } while (0)

// Context for the callbacks
enum CheckAuthCallbackStatus {CheckAuthInprogress = 0, CheckAuthCallbackDone = 1, CheckAuthCallbackStreamError = 2};
struct callback_ctx {
	CFMutableDataRef		theData;		// buffer for the reply message
	CFHTTPMessageRef		response;		// holds the response message
	CFMutableDictionaryRef	sslPropDict;	// holds ssl properties for the stream
	CFHTTPAuthenticationRef serverAuth;		// holds the authentication object for the server
	
	uint32_t				againCount;		// Counts how many retries due to EAGAIN
	
	boolean_t	triedServerCredentials;			// TRUE if we have tried server credentials
	boolean_t	triedProxyServerCredentials;	// TRUE if we have tried proxy server credentials
	
	// Proxy
	CFStringRef				proxyRealm;
	boolean_t				httpProxyEnabled;	// true if an http proxy is configured (according to SCDynamicStore)
	CFStringRef				httpProxyServer;	// name or address of the http proxy server
	int						httpProxyPort;
	boolean_t				httpsProxyEnabled;	// true if an secure proxy (https) is configured (according to SCDynamicStore)
	CFStringRef				httpsProxyServer;	// name or address of the https proxy server
	int						httpsProxyPort;
	CFDictionaryRef			proxyDict;		// hold the proxy dictionary
	SCDynamicStoreRef		proxyStore;		// dynamic store for proxy info
	CFHTTPAuthenticationRef proxyAuth;		// holds the authentication object for the proxy server
	CFIndex					statusCode;		//  only valid when status is CheckAuthCallbackDone
	CFStreamError			streamError;	// only valid when status is CheckAuthCallbackStreamError
	enum CheckAuthCallbackStatus status;
};

// function prototypes
// enum WEBDAVLIBAuthStatus checkServerAuth(CFURLRef a_url);
static enum WEBDAVLIBAuthStatus finalStatusFromStatusCode(struct callback_ctx *ctx, int *error);
static int handleStreamError(struct callback_ctx *ctx, boolean_t *tryAgain);
static int handleSSLErrors(struct callback_ctx *ctx, boolean_t *tryAgain);
static enum WEBDAVLIBAuthStatus sendOptionsRequest(CFURLRef a_url, struct callback_ctx *ctx, int *result);
static enum WEBDAVLIBAuthStatus sendOptionsRequestAuthenticated(CFURLRef a_url, struct callback_ctx *ctx, CFDictionaryRef creds, int *result);
static void applyCredentialsToRequest(struct callback_ctx *ctx, CFDictionaryRef creds, CFHTTPMessageRef request);
static void checkServerAuth_handleStreamEvent(CFReadStreamRef stream, CFStreamEventType type, void *clientCallBackInfo);
static int updateNetworkProxies(struct callback_ctx *ctx);
static void releaseContextItems(struct callback_ctx *ctx);
static void initContext(struct callback_ctx *ctx);

// Globals
static SCDynamicStoreRef gProxyStore;
	
enum
{
	kHttpDefaultPort = 80,	// default port for HTTP
	kHttpsDefaultPort = 443	// default port for HTTPS
};

	
enum WEBDAVLIBAuthStatus
queryForProxy(CFURLRef a_url, CFMutableDictionaryRef proxyInfo, int *error)
{
	enum WEBDAVLIBAuthStatus finalStatus;
	int result;
	struct callback_ctx ctx;
	CFStringRef cf_port;
	
	initContext(&ctx);
	
	finalStatus = sendOptionsRequest(a_url, &ctx, &result);
	*error = result;
	
	switch (finalStatus) {
		case WEBDAVLIB_Success:
			syslog(LOG_DEBUG, "%s: Returning WEBDAVLIB_Success", __FUNCTION__);
			break;
		case WEBDAVLIB_ProxyAuth:
			syslog(LOG_DEBUG, "%s: Returning WEBDAVLIB_ProxyAuth", __FUNCTION__);
			if(ctx.httpProxyEnabled == TRUE) {
				// Return http proxy server info in proxyInfo dictionary
				CFDictionarySetValue(proxyInfo, kWebDAVLibProxySchemeKey, CFSTR("http"));
				CFDictionarySetValue(proxyInfo, kWebDAVLibProxyServerNameKey, ctx.httpProxyServer);

				if (ctx.proxyRealm != NULL)
					CFDictionarySetValue(proxyInfo, kWebDAVLibProxyRealmKey, ctx.proxyRealm);
				
				cf_port = CFStringCreateWithFormat(NULL, NULL, CFSTR("%d"), ctx.httpProxyPort);

				if (cf_port != NULL) {
					CFDictionarySetValue(proxyInfo, kWebDAVLibProxyPortKey, cf_port);
					CFRelease(cf_port);
				}
			}
			else {
				// Return https proxy server info in proxyInfo dictionary
				CFDictionarySetValue(proxyInfo, kWebDAVLibProxySchemeKey, CFSTR("https"));
				CFDictionarySetValue(proxyInfo, kWebDAVLibProxyServerNameKey, ctx.httpsProxyServer);
				
				if (ctx.proxyRealm != NULL)
					CFDictionarySetValue(proxyInfo, kWebDAVLibProxyRealmKey, ctx.proxyRealm);
				
				cf_port = CFStringCreateWithFormat(NULL, NULL, CFSTR("%d"), ctx.httpsProxyPort);
				if (cf_port != NULL) {
					CFDictionarySetValue(proxyInfo, kWebDAVLibProxyPortKey, cf_port);
					CFRelease(cf_port);
				}
			}			
			break;
		case WEBDAVLIB_ServerAuth:
			syslog(LOG_DEBUG, "%s: Returning WEBDAVLIB_ServerAuth", __FUNCTION__);
			break;
		case WEBDAVLIB_UnexpectedStatus:
			syslog(LOG_DEBUG, "%s: Returning WEBDAVLIB_UnexpectedStatus, errno: %d", __FUNCTION__, result);
			break;
		case WEBDAVLIB_IOError:
			syslog(LOG_DEBUG, "%s: Returning WEBDAVLIB_IOError, errno: %d", __FUNCTION__, result);
			break;		
	}
	
	// Release context items
	releaseContextItems(&ctx);
	
	return (finalStatus);
}

enum WEBDAVLIBAuthStatus
connectToServer(CFURLRef a_url, CFDictionaryRef creds, int *error)	
{	
	enum WEBDAVLIBAuthStatus finalStatus;
	int result;
	struct callback_ctx ctx;
	CFStringRef cf_port;
	
	initContext(&ctx);

	finalStatus = sendOptionsRequestAuthenticated(a_url, &ctx, creds, &result);
	*error = result;
	
	switch (finalStatus) {
		case WEBDAVLIB_Success:
			syslog(LOG_DEBUG, "%s: Returning WEBDAVLIB_Success", __FUNCTION__);
			break;
		case WEBDAVLIB_ProxyAuth:
			syslog(LOG_DEBUG, "%s: Returning WEBDAVLIB_ProxyAuth", __FUNCTION__);
			break;
		case WEBDAVLIB_ServerAuth:
			syslog(LOG_DEBUG, "%s: Returning WEBDAVLIB_ServerAuth", __FUNCTION__);
			break;
		case WEBDAVLIB_UnexpectedStatus:
			syslog(LOG_DEBUG, "%s: Returning WEBDAVLIB_UnexpectedStatus, errno %d", __FUNCTION__, result);
			break;
		case WEBDAVLIB_IOError:
			syslog(LOG_DEBUG, "%s: Returning WEBDAVLIB_IOError, errno %d", __FUNCTION__, result);
			break;		
	}
	
	// Release context items
	releaseContextItems(&ctx);
	
	return (finalStatus);
}
	

static enum WEBDAVLIBAuthStatus
sendOptionsRequest(CFURLRef a_url, struct callback_ctx *ctx, int *err)
{
	CFHTTPMessageRef message;
	CFReadStreamRef rdStream;
	boolean_t done, tryAgain;
	enum WEBDAVLIBAuthStatus finalStatus;

	*err = 0;
	
	// initialize the context struct
	ctx->status = CheckAuthInprogress;
	ctx->theData = CFDataCreateMutable(NULL, 0);
	ctx->sslPropDict = NULL;
	
	CFStreamClientContext context = {0, ctx, NULL, NULL, NULL};
	
	// update proxy information
	updateNetworkProxies(ctx);
	
	done = FALSE;
	while (done == FALSE) {		
		// create a CFHTTP message object
		message = CFHTTPMessageCreateRequest(kCFAllocatorDefault, CFSTR("OPTIONS"), a_url, kCFHTTPVersion1_1);		
		CFHTTPMessageSetHeaderFieldValue(message, CFSTR("User-Agent"), CFSTR(WEBAVLIB_USER_AGENT_STRING));
		CFHTTPMessageSetHeaderFieldValue(message, CFSTR("Accept"), CFSTR("*/*"));
		
		rdStream = CFReadStreamCreateForHTTPRequest(kCFAllocatorDefault, message);
		CFRelease(message);
		message = NULL;
		
		ctx->status = CheckAuthInprogress;
		
		// turn on automatic redirection
		CFReadStreamSetProperty(rdStream, kCFStreamPropertyHTTPShouldAutoredirect, kCFBooleanTrue);

		// apply http/https proxy properties
		if (ctx->proxyDict != NULL)
			CFReadStreamSetProperty(rdStream, kCFStreamPropertyHTTPProxy, ctx->proxyDict);
		
		// apply SSL properties
		if (ctx->sslPropDict != NULL)
			CFReadStreamSetProperty(rdStream, kCFStreamPropertySSLSettings, ctx->sslPropDict);
		
		// Set up the callback and schedule
		CFReadStreamSetClient(rdStream,
							  kCFStreamEventHasBytesAvailable | kCFStreamEventEndEncountered | kCFStreamEventErrorOccurred,
							  checkServerAuth_handleStreamEvent,
							  &context);
		CFReadStreamScheduleWithRunLoop(rdStream, CFRunLoopGetCurrent(), kCFRunLoopCommonModes);
	
		// Open the stream and run the runloop
		CFReadStreamOpen(rdStream);

		while (ctx->status == CheckAuthInprogress) {
			CFRunLoopRunInMode(kCFRunLoopDefaultMode, 20, TRUE);
		}
		
		if (ctx->status == CheckAuthCallbackDone) {
			// We received an http status code
			finalStatus = finalStatusFromStatusCode(ctx, err);
			
			if (finalStatus == WEBDAVLIB_ProxyAuth) {
				// create an authentication object so we can fetch the realm
				ctx->proxyAuth = CFHTTPAuthenticationCreateFromResponse(kCFAllocatorDefault, ctx->response);
				ctx->proxyRealm = CFHTTPAuthenticationCopyRealm(ctx->proxyAuth);
			}
			
			done = TRUE;
		}
		else if (ctx->status = CheckAuthCallbackStreamError) {
			*err = handleStreamError(ctx, &tryAgain);
			
			if ((tryAgain == FALSE) || (++ctx->againCount > WEBDAVLIB_MAX_AGAIN_COUNT)) {
					finalStatus = WEBDAVLIB_IOError;
				done = TRUE;
			}
		}
		
		// Unschedule the callback and close the read stream
		CFReadStreamSetClient(rdStream, kCFStreamEventNone, NULL, NULL);
		CFReadStreamUnscheduleFromRunLoop(rdStream, CFRunLoopGetCurrent(), kCFRunLoopCommonModes);
		CFReadStreamClose(rdStream);
		CFRelease(rdStream);
		rdStream = NULL;
		
		CFRelease(ctx->theData);
		ctx->theData = CFDataCreateMutable(NULL, 0);		
	}
	return (finalStatus);
}

static enum WEBDAVLIBAuthStatus
sendOptionsRequestAuthenticated(CFURLRef a_url, struct callback_ctx *ctx, CFDictionaryRef creds, int *err)
{
	CFHTTPMessageRef message;
	CFReadStreamRef rdStream;
	boolean_t done, tryAgain;

	enum WEBDAVLIBAuthStatus finalStatus;
	
	*err = 0;
	
	// initialize the context struct
	ctx->status = CheckAuthInprogress;
	ctx->theData = CFDataCreateMutable(NULL, 0);
	ctx->sslPropDict = NULL;
		
	CFStreamClientContext context = {0, ctx, NULL, NULL, NULL};
		
	// update proxy information
	updateNetworkProxies(ctx);
		
	done = FALSE;
	while (done == FALSE) {		
		// create a CFHTTP message object
		message = CFHTTPMessageCreateRequest(kCFAllocatorDefault, CFSTR("OPTIONS"), a_url, kCFHTTPVersion1_1);		
		CFHTTPMessageSetHeaderFieldValue(message, CFSTR("User-Agent"), CFSTR(WEBAVLIB_USER_AGENT_STRING));
		CFHTTPMessageSetHeaderFieldValue(message, CFSTR("Accept"), CFSTR("*/*"));
		
		// Apply authentication objects to the request
		applyCredentialsToRequest(ctx, creds, message);
			
		rdStream = CFReadStreamCreateForHTTPRequest(kCFAllocatorDefault, message);
		CFRelease(message);
		message = NULL;
			
		ctx->status = CheckAuthInprogress;
			
		// turn on automatic redirection
		CFReadStreamSetProperty(rdStream, kCFStreamPropertyHTTPShouldAutoredirect, kCFBooleanTrue);
			
		// apply http/https proxy properties
		if (ctx->proxyDict != NULL)
			CFReadStreamSetProperty(rdStream, kCFStreamPropertyHTTPProxy, ctx->proxyDict);
			
		// apply SSL properties
		if (ctx->sslPropDict != NULL)
			CFReadStreamSetProperty(rdStream, kCFStreamPropertySSLSettings, ctx->sslPropDict);
			
		// Set up the callback and schedule
		CFReadStreamSetClient(rdStream,
								kCFStreamEventHasBytesAvailable | kCFStreamEventEndEncountered | kCFStreamEventErrorOccurred,
								checkServerAuth_handleStreamEvent,
								&context);
		CFReadStreamScheduleWithRunLoop(rdStream, CFRunLoopGetCurrent(), kCFRunLoopCommonModes);
			
		// Open the stream and run the runloop
		CFReadStreamOpen(rdStream);
			
		while (ctx->status == CheckAuthInprogress) {
			CFRunLoopRunInMode(kCFRunLoopDefaultMode, 20, TRUE);
		}
			
		if (ctx->status == CheckAuthCallbackDone) {
			// We received an http status code
			finalStatus = finalStatusFromStatusCode(ctx, err);

			if (finalStatus == WEBDAVLIB_ServerAuth) {
				
				// do we have an authentication object?
				if (ctx->serverAuth == NULL) {					
					// create a authentication object for server credentials on the next loop
					ctx->serverAuth = CFHTTPAuthenticationCreateFromResponse(kCFAllocatorDefault, ctx->response);
					
					if (ctx->serverAuth != NULL) {
						if (CFHTTPAuthenticationIsValid(ctx->serverAuth, NULL) == FALSE) {
							// game over
							syslog(LOG_DEBUG, "%s: Server CFHTTPAuthenticationIsValid is FALSE", __FUNCTION__);
							*err = EIO;
							done = TRUE;
							continue;
						}
						
						// Do we need credentials at this point?
						if (CFHTTPAuthenticationRequiresUserNameAndPassword(ctx->serverAuth) == TRUE) {
							// Were we given server credentials?
							if (CFDictionaryContainsKey(creds, kWebDAVLibUserNameKey) == FALSE) {
								// No server credentials, so just return WEBDAVLIB_ServerAuth
								syslog(LOG_DEBUG, "%s: No server credentials in dictionary", __FUNCTION__);
								done = TRUE;
							}						
						}
					}
					else {
						// game over
						syslog(LOG_DEBUG, "%s: Initial Server CFHTTPAuthenticationCreateFromResponse returned NULL", __FUNCTION__);
						done = TRUE;
						*err = EIO;
					}
				}
				else {
					// We have an auth abject for the server, we need to update it and use it
					if (CFHTTPAuthenticationIsValid(ctx->serverAuth, NULL) == FALSE) {
						CFRelease(ctx->serverAuth);
						ctx->serverAuth = CFHTTPAuthenticationCreateFromResponse(kCFAllocatorDefault, ctx->response);
						
						if (ctx->serverAuth != NULL) {
							if (CFHTTPAuthenticationIsValid(ctx->serverAuth, NULL) == FALSE) {
								// game over
								syslog(LOG_DEBUG, "%s: Server CFHTTPAuthenticationIsValid is FALSE", __FUNCTION__);
								*err = EIO;
								done = TRUE;
								continue;
							}
							
							// Do we need server credentials at this point?
							if (CFHTTPAuthenticationRequiresUserNameAndPassword(ctx->serverAuth) == TRUE) {
								// Were we given server credentials?
								if (CFDictionaryContainsKey(creds, kWebDAVLibUserNameKey) == FALSE) {
									// No server credentials, so just return WEBDAVLIB_ServerAuth
									syslog(LOG_DEBUG, "%s: No server credentials in dictionary", __FUNCTION__);
									done = TRUE;
								}
								
								// Have we already tried server credentials?
								if (ctx->triedServerCredentials == TRUE) {
									// The server credentials were rejected, just return WEBDAVLIB_ServerAuth
									syslog(LOG_DEBUG, "%s: Server credentials were not accepted", __FUNCTION__);
									done = TRUE;
								}
							}
						}
						else {
							// game over
							syslog(LOG_DEBUG, "%s: Server CFHTTPAuthenticationCreateFromResponse returned NULL", __FUNCTION__);
							*err = EIO;
							done = TRUE;
						}
					}
				}
			}
			else if (finalStatus == WEBDAVLIB_ProxyAuth) {
				// do we have an authentication object?
				if (ctx->proxyAuth == NULL) {
					if (CFDictionaryContainsKey(creds, kWebDAVLibProxyUserNameKey) == FALSE) {
						// No proxy server creds, so just return WEBAVLIB_ProxyAuth
						syslog(LOG_DEBUG, "%s: No proxy server credentials in dictionary", __FUNCTION__);
						done = TRUE;
						continue;
					}

					// create an authentication object for proxy server credentials on the next loop
					ctx->proxyAuth = CFHTTPAuthenticationCreateFromResponse(kCFAllocatorDefault, ctx->response);
					
					if (ctx->proxyAuth != NULL) {
						if (CFHTTPAuthenticationIsValid(ctx->proxyAuth, NULL) == FALSE) {
							// game over
							syslog(LOG_DEBUG, "%s: Proxy CFHTTPAuthenticationIsValid is FALSE", __FUNCTION__);
							*err = EIO;
							done = TRUE;
							continue;
						}
						
						// Do we need proxy server credentials at this point?
						if (CFHTTPAuthenticationRequiresUserNameAndPassword(ctx->proxyAuth) == TRUE) {
							// Were we given proxy server credentials?
							if (CFDictionaryContainsKey(creds, kWebDAVLibProxyUserNameKey) == FALSE) {
								// No proxyserver credentials, so just return WEBDAVLIB_ProxyAuth
								syslog(LOG_DEBUG, "%s: No proxy server credentials in dictionary", __FUNCTION__);
								done = TRUE;
							}
						}
					}
					else {
						// game over
						syslog(LOG_DEBUG, "%s: Server CFHTTPAuthenticationCreateFromResponse returned NULL", __FUNCTION__);
						*err = EIO;
						done = TRUE;
					}
				}
				else {
					// We have an auth abject for the proxy server, we need to update it and use it
					if (CFHTTPAuthenticationIsValid(ctx->proxyAuth, NULL) == FALSE) {
						CFRelease(ctx->proxyAuth);
						ctx->proxyAuth = CFHTTPAuthenticationCreateFromResponse(kCFAllocatorDefault, ctx->response);
						
						if (ctx->proxyAuth != NULL) {
							if (CFHTTPAuthenticationIsValid(ctx->proxyAuth, NULL) == FALSE) {
								// game over
								syslog(LOG_DEBUG, "%s: Proxy server CFHTTPAuthenticationIsValid is FALSE", __FUNCTION__);
								*err = EIO;
								done = TRUE;
								continue;
							}
							
							// Do we need proxy server credentials at this point?
							if (CFHTTPAuthenticationRequiresUserNameAndPassword(ctx->proxyAuth) == TRUE) {
								// Were we given proxy server credentials?
								if (CFDictionaryContainsKey(creds, kWebDAVLibProxyUserNameKey) == FALSE) {
									// No proxyserver credentials, so just return WEBDAVLIB_ProxyAuth
									syslog(LOG_DEBUG, "%s: No proxy server creds in dictionary", __FUNCTION__);
									done = TRUE;
								}
								
								// Have we already tried proxy server credentials?
								if (ctx->triedProxyServerCredentials == TRUE) {
									// The proxy server credentials were rejected, just return WEBDAVLIB_ProxyAuth
									syslog(LOG_DEBUG, "%s: Proxy server credentials were not accepted", __FUNCTION__);
									done = TRUE;
								}								
							}
						}
						else {
							// game over
							syslog(LOG_DEBUG, "%s: Proxy server CFHTTPAuthenticationCreateFromResponse returned NULL", __FUNCTION__);
							*err = EIO;
							done = TRUE;
						}
					}
					
				}
			}
			else {
				done = TRUE;
			}
		}
		else if (ctx->status = CheckAuthCallbackStreamError) {
			*err = handleStreamError(ctx, &tryAgain);
				
			if ((tryAgain == FALSE) || (++ctx->againCount > WEBDAVLIB_MAX_AGAIN_COUNT)) {
				finalStatus = WEBDAVLIB_IOError;
				done = TRUE;
			}
		}
			
		// Unschedule the callback and close the read stream
		CFReadStreamSetClient(rdStream, kCFStreamEventNone, NULL, NULL);
		CFReadStreamUnscheduleFromRunLoop(rdStream, CFRunLoopGetCurrent(), kCFRunLoopCommonModes);
		CFReadStreamClose(rdStream);
		CFRelease(rdStream);
		rdStream = NULL;
			
		CFRelease(ctx->theData);
		ctx->theData = CFDataCreateMutable(NULL, 0);		
	}
	return (finalStatus);
}

static void applyCredentialsToRequest(struct callback_ctx *ctx, CFDictionaryRef creds, CFHTTPMessageRef request)
{
	CFStringRef user, password;
	
	// Check for server credentials
	if (ctx->serverAuth != NULL) {
		user = CFDictionaryGetValue(creds, kWebDAVLibUserNameKey);
		password = CFDictionaryGetValue(creds, kWebDAVLibPasswordKey);
		
		// Note: To support NTLM, we need to obtain the NT Domain from the user.
		CFHTTPMessageApplyCredentials(request, ctx->serverAuth, user, password,  NULL);
		ctx->triedServerCredentials = TRUE;
	}
	
	// Check for proxy server credentials
	if (ctx->proxyAuth != NULL) {
		user = CFDictionaryGetValue(creds, kWebDAVLibProxyUserNameKey);
		password = CFDictionaryGetValue(creds, kWebDAVLibProxyPasswordKey);
		
		// Note: To support NTLM, we need to obtain the NT Domain from the user.
		CFHTTPMessageApplyCredentials(request, ctx->proxyAuth, user, password,  NULL);
		ctx->triedProxyServerCredentials = TRUE;
	}
}	
	
static enum WEBDAVLIBAuthStatus
finalStatusFromStatusCode(struct callback_ctx *ctx, int *error)
{
	enum WEBDAVLIBAuthStatus finalStatus;
	*error = 0;
	
	// Remember our main goal is to determine whether or not
	// there exists an http/https proxy server to deal with.
	// Since we only send an OPTIONS request, many http status codes
	// may not make sense (such as 423 Lock Failed).
	switch (ctx->statusCode / 100) {
		case 2:		// 2xx Successfull
			finalStatus = WEBDAVLIB_Success;
			break;
		case 4:
			if (ctx->statusCode == 401) {
				finalStatus = WEBDAVLIB_ServerAuth;
				*error = EAUTH;
			}
			else if (ctx->statusCode == 407) {
				finalStatus = WEBDAVLIB_ProxyAuth;
				*error = EAUTH;
			}
			else {
				syslog(LOG_ERR, "%s: unexpected http status code %d\n", __FUNCTION__, ctx->statusCode);
				*error = EIO;
				finalStatus = WEBDAVLIB_UnexpectedStatus;
			}
			break;
		case 1:		// Informational 1xx
		case 3:		// Redirection   3xx
		case 5:		// Server error  5xx
		default:
			syslog(LOG_ERR, "%s: unexpected http status code %d\n", __FUNCTION__, ctx->statusCode);
			finalStatus = WEBDAVLIB_UnexpectedStatus;
			*error = EIO;
			break;
	}
	return (finalStatus);
}
	
static int
handleStreamError(struct callback_ctx *ctx, boolean_t *tryAgain)
{
	int result = EIO;
	
	// we only retry under certain error conditions
	*tryAgain = FALSE;
	
	if (ctx->streamError.domain == kCFStreamErrorDomainSSL) {
		result = handleSSLErrors(ctx, tryAgain);
	}
	else if (ctx->streamError.domain == kCFStreamErrorDomainPOSIX) {
		result = ctx->streamError.error;
		
		if (result == EPIPE) {
				// busy server, just try again
			syslog(LOG_DEBUG, "%s: retrying stream error domain: posix error: EPIPE\n", __FUNCTION__);
				*tryAgain = TRUE;
		}
		else
			syslog(LOG_ERR, "%s: stream error domain: posix error: %d\n", __FUNCTION__, ctx->streamError.error);	
	}
	else if (ctx->streamError.domain == kCFStreamErrorDomainHTTP) {
		if (ctx->streamError.error == kCFStreamErrorHTTPConnectionLost) {
			// connection was dropped, we can try again
			syslog(LOG_DEBUG, "%s: retrying, stream error domain: http error: kCFStreamErrorHTTPConnectionLost", __FUNCTION__);
			*tryAgain = TRUE;
			result = ECONNRESET;
		}
		else {
			syslog(LOG_ERR, "%s: stream error domain: http error: %d", __FUNCTION__, ctx->streamError.error);
			result = EIO;
		}
	}
	else if (ctx->streamError.domain == kCFStreamErrorDomainNetDB) {
		switch (ctx->streamError.error) {
		case EAI_NODATA:
			// no address associated with host name
			// the network interface was changed
			// okay to try again
			syslog(LOG_DEBUG, "%s: retrying, stream error domain: netdb error: EAI_NODATA", __FUNCTION__);
			*tryAgain = TRUE;
			result = EADDRNOTAVAIL;
			break;
		default:
				syslog(LOG_ERR, "%s: stream error domain: netdb error: %d\n", __FUNCTION__, ctx->streamError.error);
			result = EIO;
			break;
		}
	}
	else {
		syslog(LOG_ERR, "%s: stream error domain: %ld error: %d\n", __FUNCTION__, ctx->streamError.domain, ctx->streamError.error);
		result = EIO;
	}
	return (result);
}

static int
handleSSLErrors(struct callback_ctx *ctx, boolean_t *tryAgain)
{
	SInt32 error;
	int result;
	
	// no good errno to indicate ssl errors, so we just use EIO
	result = EIO;
	
	// in most cases we try again
	*tryAgain = TRUE;
	
	error = ctx->streamError.error;

	syslog(LOG_DEBUG, "%s: stream error domain: ssl error: %d", __FUNCTION__, ctx->streamError.error);
	
	if (ctx->sslPropDict == NULL)
		ctx->sslPropDict = CFDictionaryCreateMutable(kCFAllocatorDefault,
													 0, 
													 &kCFTypeDictionaryKeyCallBacks,
													 &kCFTypeDictionaryValueCallBacks);
	if (ctx->sslPropDict == NULL) {
		syslog(LOG_ERR, "%s: no memory for sslPropDictionary", __FUNCTION__);
		return ENOMEM;
	}
	
	if ( (CFDictionaryGetValue(ctx->sslPropDict, kCFStreamSSLLevel) == NULL) &&
		(((error <= errSSLProtocol) && (error > errSSLXCertChainInvalid)) ||
		 ((error <= errSSLCrypto) && (error > errSSLUnknownRootCert)) ||
		 ((error <= errSSLClosedNoNotify) && (error > errSSLPeerBadCert)) ||
		 (error == errSSLIllegalParam) ||
		 ((error <= errSSLPeerAccessDenied) && (error > errSSLLast))) )
	{
		// retry with fall back from TLS to SSL */
		CFDictionarySetValue(ctx->sslPropDict, kCFStreamSSLLevel, kCFStreamSocketSecurityLevelSSLv3);
		return (result);
	}
	
	switch ( ctx->streamError.error )
	{
		case errSSLCertExpired:
		case errSSLCertNotYetValid:
			/* The certificate for this server has expired or is not yet valid */
			if ( (CFDictionaryGetValue(ctx->sslPropDict, kCFStreamSSLAllowsExpiredCertificates) == NULL) )
			{
				// We are just trying to check for a proxy server, so it's okay to continue
				CFDictionarySetValue(ctx->sslPropDict, kCFStreamSSLAllowsExpiredCertificates, kCFBooleanTrue);
				CFDictionarySetValue(ctx->sslPropDict, kCFStreamSSLAllowsExpiredRoots, kCFBooleanTrue);
			}		
			break;
				
		case errSSLBadCert:
		case errSSLXCertChainInvalid:
		case errSSLHostNameMismatch:
			/* The certificate for this server is invalid */
			if ( (CFDictionaryGetValue(ctx->sslPropDict, kCFStreamSSLValidatesCertificateChain) == NULL) )
			{
				CFDictionarySetValue(ctx->sslPropDict, kCFStreamSSLValidatesCertificateChain, kCFBooleanFalse);
			}
			break;
				
		case errSSLUnknownRootCert:
		case errSSLNoRootCert:
			/* The certificate for this server was signed by an unknown certifying authority */
			if ( (CFDictionaryGetValue(ctx->sslPropDict, kCFStreamSSLAllowsAnyRoot) == NULL) )
			{
				CFDictionarySetValue(ctx->sslPropDict, kCFStreamSSLAllowsAnyRoot, kCFBooleanTrue);
			}
			break;
				
		default:
			syslog(LOG_ERR, "%s: stream error domain: ssl error: %d", __FUNCTION__, ctx->streamError.error);
			// no sense in retrying
			*tryAgain = TRUE;
			break;
	}
	
	return (result);
}

static void checkServerAuth_handleStreamEvent(CFReadStreamRef stream, CFStreamEventType type, void *clientCallBackInfo)
{
	struct callback_ctx *ctx;
	CFTypeRef theResponsePropertyRef;
	CFIndex bytesRead;
	CFStreamError streamError;
	UInt8 buffer[STREAM_EVENT_BUFSIZE];
	
	ctx = (struct callback_ctx *)clientCallBackInfo;
	
	switch (type) {
		case kCFStreamEventHasBytesAvailable:
            bytesRead = CFReadStreamRead(stream, buffer, STREAM_EVENT_BUFSIZE);
            if (bytesRead > 0) {
                CFDataAppendBytes(ctx->theData, buffer, bytesRead);
            }
            // Don't worry about bytesRead <= 0, because those will generate other events
			break;
			
        case kCFStreamEventEndEncountered: 
			theResponsePropertyRef = CFReadStreamCopyProperty(stream, kCFStreamPropertyHTTPResponseHeader);
			ctx->response = *((CFHTTPMessageRef*)((void*)&theResponsePropertyRef));
			ctx->statusCode = CFHTTPMessageGetResponseStatusCode(ctx->response);
			ctx->status = CheckAuthCallbackDone;
			syslog(LOG_DEBUG, "%s: StreamEventEndEncountered, status code %d\n", __FUNCTION__, ctx->statusCode);
			break;
			
		case kCFStreamEventErrorOccurred:
			streamError = CFReadStreamGetError(stream);			
			syslog(LOG_DEBUG,"%s: EventHasErrorOccurred: domain %ld, error %d",
				   __FUNCTION__, streamError.domain, streamError.error);
			ctx->streamError = streamError;
			ctx->status = CheckAuthCallbackStreamError;
			break;
        
		default:
			syslog(LOG_DEBUG, "%s: Received unexpected stream event %d\n", __FUNCTION__, type);
			break;
	}	
}

static int updateNetworkProxies(struct callback_ctx *ctx)
{
	CFNumberRef cf_enabled;
	CFNumberRef cf_port;
	int enabled;
	int err;

	if (ctx->proxyStore == NULL) {
		ctx->proxyStore = SCDynamicStoreCreate(kCFAllocatorDefault, CFSTR("WebDAVFSPlugin"), NULL, NULL);
		if (ctx->proxyStore == NULL)
			return ENOMEM;
	}
	
	// Reset all proxy info
	if (ctx->proxyRealm != NULL) {
		CFRelease(ctx->proxyRealm);
		ctx->proxyRealm = NULL;
	}	
	
	ctx->httpProxyEnabled = FALSE;
	ctx->httpsProxyEnabled = FALSE;
	
	if (ctx->httpProxyServer != NULL)
	{
		CFRelease(ctx->httpProxyServer);
		ctx->httpProxyServer = NULL;
	}
	
	if (ctx->httpsProxyServer != NULL)
	{
		CFRelease(ctx->httpsProxyServer);
		ctx->httpsProxyServer = NULL;
	}	
	
	if (ctx->proxyDict != NULL)
	{
		CFRelease(ctx->proxyDict);
		ctx->proxyDict = NULL;
	}	

	ctx->httpProxyEnabled = FALSE;
	ctx->httpsProxyEnabled = FALSE;
	
	// fetch the current internet proxy dictionary
	ctx->proxyDict = SCDynamicStoreCopyProxies(gProxyStore);
	
	if (ctx->proxyDict != NULL) {
		// *********************
		// handle HTTP proxies
		// *********************
		
		// are HTTP proxies enabled?
		cf_enabled = CFDictionaryGetValue(ctx->proxyDict, kSCPropNetProxiesHTTPEnable);
		if ( (cf_enabled != NULL) && CFNumberGetValue(cf_enabled, kCFNumberIntType, &enabled) && enabled )
		{
			// fetch the HTTP proxy host
			ctx->httpProxyServer = CFDictionaryGetValue(ctx->proxyDict, kSCPropNetProxiesHTTPProxy);
			if ( ctx->httpProxyServer != NULL )
			{
				CFRetain(ctx->httpProxyServer);
				
				// fetch the HTTP proxy port
				cf_port = CFDictionaryGetValue(ctx->proxyDict, kSCPropNetProxiesHTTPPort);
				if ( (cf_port != NULL) && CFNumberGetValue(cf_port, kCFNumberIntType, &ctx->httpProxyPort) ) 
				{
					if ( ctx->httpProxyPort == 0 )
					{
						//no port specified so use the default HTTP port
						ctx->httpProxyPort = kHttpDefaultPort;
					}
					ctx->httpProxyEnabled = TRUE;
				}
			}
		}
		
		// *********************
		// handle HTTPS proxies
		// *********************

		// are HTTPS proxies enabled?
		cf_enabled = CFDictionaryGetValue(ctx->proxyDict, kSCPropNetProxiesHTTPSEnable);
		if ( (cf_enabled != NULL) && CFNumberGetValue(cf_enabled, kCFNumberIntType, &enabled) && enabled )
		{
			// fetch the HTTPS proxy host
			ctx->httpsProxyServer = CFDictionaryGetValue(ctx->proxyDict, kSCPropNetProxiesHTTPSProxy);
			if ( ctx->httpsProxyServer != NULL )
			{
				CFRetain(ctx->httpsProxyServer);
				
				// fetch the HTTPS proxy port
				cf_port = CFDictionaryGetValue(ctx->proxyDict, kSCPropNetProxiesHTTPSPort);
				if ( (cf_port != NULL) && CFNumberGetValue(cf_port, kCFNumberIntType, &ctx->httpsProxyPort) ) 
				{
					if ( ctx->httpsProxyPort == 0 )
					{
						// no port specified so use the default HTTPS port
						ctx->httpsProxyPort = kHttpsDefaultPort;
					}
					ctx->httpsProxyEnabled = TRUE;
				}
			}
		}
	}
}

static void initContext(struct callback_ctx *ctx)
{
	ctx->theData = NULL;
	ctx->response = NULL;
	ctx->sslPropDict = NULL;
	ctx->serverAuth = NULL;
	ctx->againCount = 0;
	ctx->triedServerCredentials = FALSE;
	ctx->triedProxyServerCredentials = FALSE;
	ctx->proxyRealm = NULL;
	ctx->httpProxyEnabled = FALSE;
	ctx->httpProxyServer = NULL;
	ctx->httpsProxyEnabled = FALSE;
	ctx->httpsProxyServer = NULL;
	ctx->proxyDict = NULL;
	ctx->proxyStore = NULL;
	ctx->proxyAuth = NULL;
}

static void releaseContextItems(struct callback_ctx *ctx)
{
	CFReleaseNull(ctx->theData);
	CFReleaseNull(ctx->response);
	CFReleaseNull(ctx->sslPropDict);
	CFReleaseNull(ctx->serverAuth);
	CFReleaseNull(ctx->proxyRealm);
	CFReleaseNull(ctx->httpProxyServer);
	CFReleaseNull(ctx->httpsProxyServer);
	CFReleaseNull(ctx->proxyDict);
	CFReleaseNull (ctx->proxyStore);
	CFReleaseNull(ctx->proxyAuth);
}