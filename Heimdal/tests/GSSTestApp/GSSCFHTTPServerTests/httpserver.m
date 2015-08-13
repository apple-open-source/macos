//
//  Copyright (c) 2014-2015 Apple. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <CFNetwork/CFNetwork.h>
#import <CFNetwork/CFHTTPServerPriv.h>
#import <GSS/GSS.h>

#import "httpserver.h"

@interface GSSHTTPServer ()
@property (nonatomic, readonly) _CFHTTPServerRef httpServer;
@end

@implementation GSSHTTPServer


static void
_connection_didBecomeInvalid(const void* arg)
{
}

static void
_connection_didReceiveError(const void* arg, CFErrorRef err)
{
}

static void
provideError(_CFHTTPServerRequestRef req, int statusCode, NSData *body, const char *message)
{
    CFHTTPMessageRef msg = _CFHTTPServerRequestCreateResponseMessage(req, statusCode);
    if (statusCode == 401) {
        NSString *header = @"NTLM";
        if (body) {
            NSString *b64 = [body base64EncodedStringWithOptions:0];
            header = [NSString stringWithFormat:@"NTLM %@", b64];
        }

        CFHTTPMessageSetHeaderFieldValue(msg, CFSTR("WWW-Authenticate"), (__bridge CFStringRef)header);
    }
    const char* kErrorText = "Authentencation failure";
    CFDataRef d = CFDataCreate(kCFAllocatorDefault, (const UInt8*) kErrorText, strlen(kErrorText));
    _CFHTTPServerResponseRef response = _CFHTTPServerResponseCreateWithData(req, msg, d);
    CFRelease(d);
    CFRelease(msg);
    _CFHTTPServerResponseEnqueue(response);
    CFRelease(response);

}

static void
_connection_didReceiveRequest(const void* arg, _CFHTTPServerRequestRef req)
{
    _CFHTTPServerResponseRef response = NULL;
    static gss_ctx_id_t handle = NULL;
    OM_uint32 min_stat, maj_stat;

    CFStringRef header = _CFHTTPServerRequestCopyProperty(req, _kCFHTTPServerRequestMethod);
    if (header == NULL || !CFEqual(header, CFSTR("GET"))) {
        CFHTTPMessageRef msg = _CFHTTPServerRequestCreateResponseMessage(req, 500);
        const char* kErrorText = "Request not a GET";
        CFDataRef d = CFDataCreate(kCFAllocatorDefault, (const UInt8*) kErrorText, strlen(kErrorText));
        _CFHTTPServerResponseRef response = _CFHTTPServerResponseCreateWithData(req, msg, d);
        CFRelease(d);
        CFRelease(msg);
        _CFHTTPServerResponseEnqueue(response);
        CFRelease(response);
        return;
    }

    header = _CFHTTPServerRequestCopyProperty(req, CFSTR("Authorization"));
    if (header == NULL) {
        provideError(req, 401, NULL, "Authentication failure");
        return;
    }

    NSArray *components = [((__bridge NSString *)header) componentsSeparatedByString:@" "];
    if ([components count] != 2 || ![[components objectAtIndex:0] isEqualToString:@"NTLM"]) {
        provideError(req, 401, NULL, "Authentication failure, not NTLM");
        gss_delete_sec_context(&min_stat, &handle, NULL);
        return;
    }

    NSData *data = [[NSData alloc] initWithBase64EncodedString:[components objectAtIndex:1] options:0];

    gss_buffer_desc input = {
        .value = (void *)[data bytes],
        .length = [data length],
    };
    gss_buffer_desc output;
    gss_OID mech = NULL;


    maj_stat = gss_accept_sec_context(&min_stat, &handle, NULL, &input, NULL, NULL, &mech, &output, NULL, NULL, NULL);
    if (maj_stat == GSS_S_CONTINUE_NEEDED) {
        NSData *reply = [NSData dataWithBytes:output.value length:output.length];
        gss_release_buffer(&min_stat, &output);
        provideError(req, 401, reply, "Authentication continue needed");
        return;
    } else if (maj_stat != GSS_S_COMPLETE) {
        gss_delete_sec_context(&min_stat, &handle, NULL);
        gss_release_buffer(&min_stat, &output);
        provideError(req, 403, NULL, "Authentication failed");
        return;
    } else {
        gss_delete_sec_context(&min_stat, &handle, NULL);
    }
    gss_release_buffer(&min_stat, &output);

    CFDataRef d = CFDataCreate(kCFAllocatorDefault, (void *)"hello", 5);

    CFHTTPMessageRef msg = _CFHTTPServerRequestCreateResponseMessage(req, 200);
    response = _CFHTTPServerResponseCreateWithData(req, msg, d);
    CFRelease(d);
    CFRelease(msg);

    if (response) {
        _CFHTTPServerResponseEnqueue(response);
        CFRelease(response);
    }
}

static void
_connection_didSendResponse(const void* arg, _CFHTTPServerRequestRef req, _CFHTTPServerResponseRef response)
{
}

static void
_connection_didFailToSendResponse(const void* arg, _CFHTTPServerRequestRef req, _CFHTTPServerResponseRef response)
{
}


static void
_server_didCloseConnection(const void* info, _CFHTTPServerConnectionRef conn)
{
}

static void
_server_didBecomeInvalid(const void* info)
{
}

static void
_server_didReceiveError(const void* info, CFErrorRef err)
{
}

static void
_server_didOpenConnection(const void* info, _CFHTTPServerConnectionRef conn)
{
    _CFHTTPServerClient client = {
        0,
        NULL,
        NULL,
        NULL,
        NULL
    };

    _CFHTTPServerConnectionCallbacks callbacks = {
        _kCFHTTPServerConnectionCallbacksCurrentVersion,
        _connection_didBecomeInvalid,
        _connection_didReceiveError,
        _connection_didReceiveRequest,
        _connection_didSendResponse,
        _connection_didFailToSendResponse
    };

    _CFHTTPServerConnectionSetClient(conn, &client, &callbacks);
    _CFHTTPServerConnectionScheduleWithRunLoopAndMode(conn, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
}


_CFHTTPServerClient serverClient = {
    _kCFHTTPServerClientCurrentVersion,
    NULL,
    NULL,
    NULL,
    NULL
};

_CFHTTPServerCallbacks callbacks = {
    _kCFHTTPServerCallbacksCurrentVersion,
    _server_didBecomeInvalid,
    _server_didReceiveError,
    _server_didOpenConnection,
    _server_didCloseConnection,
};


- (void)start:(NSNumber *)servicePort
{

    CFStringRef s = CFSTR("GSSCFHTTPServerTests");
    _httpServer = _CFHTTPServerCreateService(kCFAllocatorDefault, &serverClient, &callbacks, s, CFSTR("_gsscfhttpserver._tcp."), [servicePort intValue]);
    CFRelease(s);

    _CFHTTPServerScheduleWithRunLoopAndMode(_httpServer, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);

    NSLog(@"http server running on port: %@", servicePort);

    while (_httpServer && _CFHTTPServerIsValid(_httpServer)) {
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1E30, true);
    }

    NSLog(@"server done");

}

@end
