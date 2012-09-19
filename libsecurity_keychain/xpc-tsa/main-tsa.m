/*
 * Copyright (c) 2012 Apple Computer, Inc. All Rights Reserved.
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

#include <sysexits.h>
#include "timestampclient.h"
#include <syslog.h>
#include <xpc/xpc.h>

struct connection_info {
    xpc_connection_t peer;
    int processed;
    int done;
};

xpc_object_t keychain_prefs_path = NULL;
xpc_object_t home = NULL;

extern xpc_object_t
xpc_create_reply_with_format(xpc_object_t original, const char * format, ...);

void finalize_connection(void *not_used);
void handle_connection_event(const xpc_connection_t peer);
void handle_request_event(struct connection_info *info, xpc_object_t event);

#ifndef NDEBUG
    #define xpctsa_secdebug(format...) \
        do {  \
            syslog(LOG_WARNING, format);  \
        } while (0)
    #define xpctsaNSLog(format...) \
        do {  \
            NSLog(format);  \
        } while (0)

#else
    //empty
    #define xpctsa_secdebug(format...)
    #define xpctsaNSLog(format...)
#endif
#define xpctsaDebug(args...)			xpctsa_secdebug(args)

/*
    These came from:

#include <OSServices/NetworkUtilities.h>

    I have no idea why they aren't more accessible.
*/

#define kHTTPResponseCodeContinue             100
#define kHTTPResponseCodeOK                   200
#define kHTTPResponseCodeNoContent            204
#define kHTTPResponseCodeBadRequest           400
#define kHTTPResponseCodeUnauthorized         401
#define kHTTPResponseCodeForbidden            403
#define kHTTPResponseCodeConflict             409
#define kHTTPResponseCodeExpectationFailed    417
#define kHTTPResponseCodeServFail             500
#define kHTTPResponseCodeInsufficientStorage  507

//
// Turn a CFString into a UTF8-encoded C string.
//
static char *cfStringToCString(CFStringRef inStr)
{
	if (!inStr)
		return "";
	CFRetain(inStr);	// compensate for release on exit

	// need to extract into buffer
	CFIndex length = CFStringGetLength(inStr);  // in 16-bit character units
    size_t len = 6 * length + 1;
	char *buffer = malloc(len);                 // pessimistic
	if (!CFStringGetCString(inStr, buffer, len, kCFStringEncodingUTF8))
        buffer[0]=0;

    CFRelease(inStr);
	return buffer;
}

static void debugShowTSAResponseInfo(NSURLResponse *response, NSData *data, NSError *err)
{
#ifndef NDEBUG
    if (response)
    {
        NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)response;
        NSInteger statusCode = [httpResponse statusCode];
        NSDictionary *headers = [httpResponse allHeaderFields];
        NSString *errStr2 = [NSHTTPURLResponse localizedStringForStatusCode:(NSInteger)statusCode];

        xpctsaNSLog(@"TSA Response: %d, %@, %@", (int)statusCode, errStr2, headers);
    }
    
    if (err)
    {   xpctsaNSLog(@"TSARequestCompletionBlock error: %@", err); }

    if (data)
    {
        xpctsaDebug("TSARequestCompletionBlock: Data (%lu bytes)",(unsigned long)[data length]);
        xpctsaNSLog(@"TSARequestCompletionBlock: Data (%lu bytes)",(unsigned long)[data length]);

        NSString *path = @"/tmp/tsaresp.rsp";
        NSDataWritingOptions writeOptionsMask = 0;
        NSError *errorPtr = NULL;
        [data writeToFile:path options:writeOptionsMask error:&errorPtr];
        if (errorPtr)
        {
            xpctsaNSLog(@"TSA Response error dumping response: %@", errorPtr);
            [errorPtr release];
        }
    }
#endif
}

static void communicateWithTimeStampingServer(xpc_object_t event, const char *requestData, size_t requestLength, const char *tsaURL)
{
    if ((requestLength==0) || !tsaURL)
        return;
        
    xpctsaDebug("Request Length: %ld, URL: %s", requestLength, tsaURL);
    
    __block CFDataRef tsaReq = CFDataCreate(kCFAllocatorDefault, (const unsigned char *)requestData, requestLength);
    xpc_retain(event);

    // The completion block is called when we have a response
    TSARequestCompletionBlock reqCompletionBlock =
    ^(NSURLResponse *response, NSData *data, NSError *err)
    {
        xpc_object_t tsaError = NULL;
        xpc_object_t tsaStatusCode = NULL;
        NSString *errStr = NULL;
        
        debugShowTSAResponseInfo(response, data, err);
        
        /*
            Handle errors first. The bad responses seen so far tend to
            return a bad HTTP status code rather than setting the "err"
            parameter. In this case, the "data" parameter contains the
            HTML of the error response, which we do not want to try to
            parse as ASN.1 data.
        */
        
        if (response)
        {
            NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse*)response;
            NSInteger statusCode = [httpResponse statusCode];
            if (statusCode != kHTTPResponseCodeOK)
            {
                errStr = [NSHTTPURLResponse localizedStringForStatusCode:(NSInteger)statusCode];
                tsaStatusCode = xpc_int64_create((int64_t)statusCode);
                xpctsaNSLog(@"TSA Response-b: %d, %@", (int)statusCode, errStr);
            }
        }
        
        if (err && !errStr)
        {
            errStr = [err description];
            if (!tsaStatusCode)
            {
                NSInteger statusCode = [err code];
                tsaStatusCode = xpc_int64_create((int64_t)statusCode);
            }
        }

        if (errStr)
        {
            const char *cerrstr = cfStringToCString((CFStringRef)errStr);
            tsaError = xpc_string_create(cerrstr);
            xpctsaNSLog(@"TSA Response-c: %@", errStr);
        }
        
        size_t length = (errStr || !data) ? 0 : [data length];
        xpc_object_t tsaReply = xpc_data_create([data bytes], length);
        xpc_connection_t peer = xpc_dictionary_get_remote_connection(event);
        xpc_object_t reply = (tsaError)?
            xpc_create_reply_with_format(event, 
                "{TimeStampReply: %value, TimeStampError: %value, TimeStampStatus: %value}", tsaReply, tsaError, tsaStatusCode) :
            xpc_create_reply_with_format(event, "{TimeStampReply: %value}", tsaReply);
        xpc_connection_send_message(peer, reply);
        xpc_release(reply);
        xpc_release(event);

        if (tsaReq)
            CFRelease(tsaReq);
        if (tsaReply)
            xpc_release(tsaReply);
        if (tsaError)
            xpc_release(tsaError);
        if (tsaStatusCode)
            xpc_release(tsaStatusCode);
    };

    sendTSARequest(tsaReq, tsaURL, reqCompletionBlock);
}

void handle_request_event(struct connection_info *info, xpc_object_t event)
{
    xpc_connection_t peer = xpc_dictionary_get_remote_connection(event);
    xpc_type_t xtype = xpc_get_type(event);

    if (info->done)
    {
        xpctsaDebug("event %p while done", event);
        return;
    }
	if (xtype == XPC_TYPE_ERROR)
    {
        xpctsaDebug("handle_request_event: XPC_TYPE_ERROR");
		if (event == XPC_ERROR_TERMINATION_IMMINENT) {
            xpctsaDebug("handle_request_event: XPC_ERROR_TERMINATION_IMMINENT");
			// launchd would like us to die, but we have open transactions.   When we finish with them xpc_service_main
			// will exit for us, so there is nothing for us to do here.
			return;
		}
		
        if (!info->done) {
            info->done = true;
            xpc_release(info->peer);
        }
        if (peer == NULL && XPC_ERROR_CONNECTION_INVALID == event && 0 != info->processed) {
            xpctsaDebug("handle_request_event: XPC_ERROR_CONNECTION_INVALID");
            // this is a normal shutdown on a connection that has processed at least
            // one request.   Nothing intresting to log.
            return;
        }
		xpctsaDebug("listener event error (connection %p): %s", peer, xpc_dictionary_get_string(event, XPC_ERROR_KEY_DESCRIPTION));
	}
    else
    if (xtype == XPC_TYPE_DICTIONARY)
    {
        size_t length = 0;
        const char *operation = xpc_dictionary_get_string(event, "operation");
        if (operation && !strcmp(operation, "TimeStampRequest"))
        {
            xpctsaDebug("Handling TimeStampRequest event");
            const void *requestData = xpc_dictionary_get_data(event, "TimeStampRequest", &length);
            const char *url = xpc_dictionary_get_string(event, "ServerURL");

            communicateWithTimeStampingServer(event, requestData, length, url);
        }
        else
            xpctsaDebug("Unknown op=%s request from pid %d", operation, xpc_connection_get_pid(peer));
    }
    else
        xpctsaDebug("Unhandled request event=%p type=%p", event, xtype);
}

void finalize_connection(void *not_used)
{
	xpctsaDebug("finalize_connection");
    xpc_transaction_end();
}

void handle_connection_event(const xpc_connection_t peer) 
{
    __block struct connection_info info;
    info.peer = peer;
    info.processed = 0;
    info.done = false;
	xpctsaDebug("handle_connection_event %p", peer);
    
    xpc_connection_set_event_handler(peer, ^(xpc_object_t event)
    {
        handle_request_event(&info, event);
    });

    //  unlike dispatch objects xpc objects don't need a context set in order to run a finalizer.   (we use our finalizer to 
    // end the transaction we are about to begin...this keeps xpc from idle exiting us while we have a live connection)
    xpc_connection_set_finalizer_f(peer, finalize_connection);
    xpc_transaction_begin();
    
    // enable the peer connection to receive messages
    xpc_connection_resume(peer);
    xpc_retain(peer);
}

int main(int argc, const char *argv[])
{
    char *wait4debugger = getenv("WAIT4DEBUGGER");
    if (wait4debugger && !strcasecmp("YES", wait4debugger))
    {
        syslog(LOG_ERR, "Waiting for debugger");
        kill(getpid(), SIGSTOP);
    }

    xpc_main(handle_connection_event);
    
    return EX_OSERR;
}

