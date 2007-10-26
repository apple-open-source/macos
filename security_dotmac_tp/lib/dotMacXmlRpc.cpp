/*
 * Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
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
 * dotMacXmlRpc.cpp - perform authenticated XMLRPC invocation.
 *					  Based upon example provided by Scott Ryder.
 */

#include <CoreServices/../Frameworks/OSServices.framework/Headers/WSMethodInvocation.h>
#include <CoreServices/../Frameworks/CFNetwork.framework/Headers/CFHTTPMessage.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>

#include <stdio.h>
#include "dotMacXmlRpc.h"
#include "dotMacTpMutils.h"

#if		XML_DEBUG
#define xmlDebug(args...)   printf(args)
#else
#define xmlDebug(args...)
#endif

/* dump contents of XMLRPC response dictionary */
#if		DICTIONARY_DEBUG	
#define RESPONSE_DICTIONARY_DEBUG   0
#else
#define RESPONSE_DICTIONARY_DEBUG   0
#endif
static UInt32 getHTTPStatusCodeFromWSInvokationResponse(CFDictionaryRef response)
{
    CFHTTPMessageRef message = 
		(CFHTTPMessageRef)CFDictionaryGetValue(response, kWSHTTPResponseMessage);
    UInt32 responseCode;

    if (message != NULL) {
        responseCode = CFHTTPMessageGetResponseStatusCode(message);
        message = NULL;
    } else {
		xmlDebug("getHTTPStatusCode: no HTTP status\n");
        responseCode = 500;
    }
    return responseCode;
}

static Boolean addAuthenticationToWSInvokation(
	WSMethodInvocationRef wsRef, 
	CFStringRef username, 
	CFStringRef password, 
	CFDictionaryRef response)
{
    CFHTTPMessageRef message = 
		(CFHTTPMessageRef)CFDictionaryGetValue(response, kWSHTTPResponseMessage);
    CFHTTPMessageRef outgoingMessage = NULL;

    if (message != NULL) {
        CFURLRef theURL = CFHTTPMessageCopyRequestURL(message);
        if (theURL != NULL) {
            //Move the stuff that counts into our new message
            outgoingMessage = CFHTTPMessageCreateRequest(NULL, CFSTR("POST"), 
				theURL, kCFHTTPVersion1_1);
            CFRelease(theURL);
        }
    }

    Boolean successful = FALSE;

    if ((message != NULL) && (outgoingMessage != NULL)) {
        successful =  CFHTTPMessageAddAuthentication(outgoingMessage, message, 
			username, password, NULL, FALSE);
    }

    if (successful) {
		/* FIXME kWSHTTPMessage isn't exported by WebServicesCore
		 * I can't find the source but there is a string in the binary that
		 * looks like this, with the leading '/' */
        WSMethodInvocationSetProperty(wsRef, CFSTR("/kWSHTTPMessage"), outgoingMessage);
    }

    if (outgoingMessage) {
		CFRelease(outgoingMessage);
	}
    return successful;
}


OSStatus performAuthenticatedXMLRPC(
	CFURLRef		theURL, 
	CFStringRef		theMethod, 
	CFDictionaryRef argDict, 
	CFArrayRef		argOrder,
	CFStringRef		userName,
	CFStringRef		password,
	CFDictionaryRef *resultDict,	// RETURNED on success
	uint32_t		*httpErrStatus)	// possibly RETURNED on error (403, 500, etc.)
{
    CFDictionaryRef result = NULL;
	bool done = false;
	OSStatus ortn = -1;		// must set before return
	*httpErrStatus = 0;
	
    /*
	 * Create a WebServices Invocation
	 */
    WSMethodInvocationRef wsRef = WSMethodInvocationCreate(theURL, theMethod, 
		kWSXMLRPCProtocol);
    WSMethodInvocationSetParameters(wsRef, argDict, argOrder);

    for(unsigned attempt=0; attempt<2; attempt++) {
		xmlDebug("***************************************************************************\n");
		xmlDebug("performAuthenticatedXMLRPC: WSMethodInvocationInvoke (attempt %u)\n", attempt);
		xmlDebug("***************************************************************************\n");
        CFDictionaryRef response = WSMethodInvocationInvoke(wsRef);

        /*
		 * Since we can't reuse the Invocation dump it as we have our response
		 */
        if (wsRef) {
            CFRelease(wsRef);
            wsRef = NULL;
        }

        if (WSMethodResultIsFault(response)) {
            CFStringRef errorMsg = (CFStringRef)CFDictionaryGetValue(
				response, kWSFaultString);
            if (errorMsg != NULL) {
				#if XML_DEBUG
				logCFstr("performAuthenticatedXMLRPC: errorMsg", errorMsg);
				#endif
                UInt32 HTTPResponseCode = 
					getHTTPStatusCodeFromWSInvokationResponse(response);
				/* only try authentication once */
				xmlDebug("performAuthenticatedXMLRPC: HTTP status %lu\n", HTTPResponseCode);
                if ((HTTPResponseCode == 401) && (attempt == 0)) {
                    wsRef = WSMethodInvocationCreate(theURL, theMethod, kWSXMLRPCProtocol);
                    WSMethodInvocationSetParameters(wsRef, argDict, argOrder);
                    Boolean successful =  addAuthenticationToWSInvokation(wsRef, 
						userName, password, response);
                    if (!successful) {
						xmlDebug("performAuthenticatedXMLRPC: unable to add authentication\n");
						ortn = ioErr;
						done = TRUE;
                    }
					xmlDebug("performAuthenticatedXMLRPC: retrying with auth\n");
					/* else go one more time with authenticated invocation */
                } else {
					/* other fatal HTTP error */
					
                    /*
					 * From Scott Ryder:
					 *
					 * All other HTTP errs (eg 403 or 500) could / should be handled here
                     * this could include adding proxy support
					 *
					 * FIXME does this mean that this code won't work through proxies?
					 */
					xmlDebug("performAuthenticatedXMLRPC: fatal RPC error\n");
					*httpErrStatus = HTTPResponseCode;
					ortn = dotMacHttpStatToOs(HTTPResponseCode);
                    done = TRUE;
                }
            } 
			else {
				xmlDebug("performAuthenticatedXMLRPC: fault with no fault string!\n");
				ortn = ioErr;
				done = TRUE;
            }
        }   /* fault */
		else {
			/* success */
			xmlDebug("performAuthenticatedXMLRPC: success\n");
            result = CFDictionaryCreateCopy(NULL, 
				(CFDictionaryRef)CFDictionaryGetValue(
					response, kWSMethodInvocationResult));
			ortn = noErr;
			done = true;
        }
        if((response != NULL) /*&& done*/) {
			#if RESPONSE_DICTIONARY_DEBUG
			dumpDictionary("XMLRPC response", response);
			#endif
            CFRelease(response);
		}
		if(done) {
			break;
		}
    }
	if(result != NULL) {
		*resultDict = result;
	}
    return ortn;
}


