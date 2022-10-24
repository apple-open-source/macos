/*
 * Copyright (c) 2022 Apple Inc. All Rights Reserved.
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

#include <sandbox.h>
#include <dirhelper_priv.h>
#include <os/log.h>
#include <sysexits.h>
#include <syslog.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <pwd.h>

#include "acmeclient.h"
#include <syslog.h>
#include <Security/SecTask.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecDispatchRelease.h>
#include <utilities/SecXPCError.h>
#include <utilities/HTTPStatusCodes.h>
#include <utilities/debugging.h>
#include <xpc/private.h>

#include <Foundation/Foundation.h>
#include <Foundation/NSJSONSerialization.h>

struct connection_info {
    xpc_connection_t peer;
    int processed;
    int done;
};

static NSString *const AcmeContentType = @"application/json";
static NSString *const AcmeSignedContentType = @"application/jose+json";

extern xpc_object_t
xpc_create_reply_with_format(xpc_object_t original, const char * format, ...);

void finalize_connection(void *not_used);
void handle_connection_event(const xpc_connection_t peer);
void handle_request_event(struct connection_info *info, xpc_object_t event);

static void debugShowAcmeResponseInfo(NSURLResponse *response, NSData *data, NSError *error) {
#ifndef NDEBUG
    if (response) {
        NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)response;
        NSInteger statusCode = [httpResponse statusCode];
        NSDictionary *headers = [httpResponse allHeaderFields];
        NSString *errStr2 = [NSHTTPURLResponse localizedStringForStatusCode:statusCode];
        secdebug("acme", "Acme Response: %d, %@, headers: %@", (int)statusCode, errStr2, headers);
    }
    if (error) {
        secdebug("acme", "AcmeRequestCompletionBlock error: %@", error);
    }
    if (data) {
        NSString *string = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
        secdebug("acme", "AcmeRequestCompletionBlock received data: %@, %@", data, string);
    }
#endif
}

/* Check whether the caller can access the network. */
static bool callerHasNetworkEntitlement(audit_token_t auditToken) {
    bool result = true; /* until proven otherwise */
    SecTaskRef task = SecTaskCreateWithAuditToken(NULL, auditToken);
    if(task != NULL) {
        CFTypeRef appSandboxValue = SecTaskCopyValueForEntitlement(task,
                                    CFSTR("com.apple.security.app-sandbox"),
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

/* Set up signal handler */
static void handle_sigterm(void) {
    signal(SIGTERM, SIG_IGN);
    static dispatch_source_t terminateSource = NULL;
    terminateSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, SIGTERM, 0,
                                             dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0));
    dispatch_source_set_event_handler(terminateSource, ^{
        secnotice("acme", "Received signal SIGTERM. Will terminate when clean.");
        xpc_transaction_exit_clean();
    });
    dispatch_activate(terminateSource);
}

/* Set up sandbox */
static void enter_sandbox(void) {
#if TARGET_OS_OSX
    char buf[PATH_MAX] = "";

    char *home_env = getenv("HOME");
    if (home_env == NULL) {
        os_log_debug(OS_LOG_DEFAULT, "$HOME not set, falling back to using getpwuid");
        struct passwd *pwd = getpwuid(getuid());
        if (pwd == NULL) {
            os_log_error(OS_LOG_DEFAULT, "failed to get passwd entry for uid %u", getuid());
            exit(EXIT_FAILURE);
        }
        home_env = pwd->pw_dir;
    }

    char *homedir = realpath(home_env, NULL);
    if (homedir == NULL) {
        os_log_error(OS_LOG_DEFAULT, "failed to resolve home directory: %{darwin.errno}d", errno);
        exit(EXIT_FAILURE);
    }

    if (!_set_user_dir_suffix("com.apple.security.XPCAcmeService") ||
        confstr(_CS_DARWIN_USER_TEMP_DIR, buf, sizeof(buf)) == 0 ||
        (mkdir(buf, 0700) && errno != EEXIST)) {
        secerror("failed to initialize temporary directory (%d): %s", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    char *tempdir = realpath(buf, NULL);
    if (tempdir == NULL) {
        secerror("failed to resolve temporary directory (%d): %s", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (confstr(_CS_DARWIN_USER_CACHE_DIR, buf, sizeof(buf)) == 0 ||
        (mkdir(buf, 0700) && errno != EEXIST)) {
        secerror("failed to initialize cache directory (%d): %s", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    char *cachedir = realpath(buf, NULL);
    if (cachedir == NULL) {
        secerror("failed to resolve cache directory (%d): %s", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    const char *parameters[] = {
        "_HOME", homedir,
        "_TMPDIR", tempdir,
        "_DARWIN_CACHE_DIR", cachedir,
        NULL
    };

    char *sberror = NULL;
    if (sandbox_init_with_parameters("com.apple.security.XPCAcmeService", SANDBOX_NAMED, parameters, &sberror) != 0) {
        secerror("Failed to enter XPCAcmeService sandbox: %{public}s", sberror);
        exit(EXIT_FAILURE);
    }

    free(homedir);
    free(tempdir);
    free(cachedir);
#else // !TARGET_OS_OSX
    char buf[PATH_MAX] = "";
    _set_user_dir_suffix("com.apple.security.XPCAcmeService");
    confstr(_CS_DARWIN_USER_TEMP_DIR, buf, sizeof(buf));
#endif // !TARGET_OS_OSX
}

static xpc_object_t createXPCErrorWithError(NSError* error) {
    // The SecCreateXPCObjectWithCFError utility can create an XPC
    // dictionary from a CFErrorRef, but it fails to create the essential
    // userInfo portion when encountering an embedded NSUnderlyingError.
    // This function extracts the userInfo from the embedded error.
    NSDictionary *userInfo = [[[error userInfo] objectForKey:NSUnderlyingErrorKey] userInfo];
    if (!userInfo) {
        userInfo = [error userInfo];
    }
    NSMutableDictionary *newUserInfo = [NSMutableDictionary dictionaryWithCapacity:0];
    NSString *urlString = [userInfo objectForKey:NSErrorFailingURLStringKey];
    NSString *description = [userInfo objectForKey:NSLocalizedDescriptionKey];
    if (urlString) {
        newUserInfo[NSErrorFailingURLStringKey] = urlString;
    }
    if (description) {
        newUserInfo[NSLocalizedDescriptionKey] = description;
    }
    NSInteger code = [error code];
    NSString *domain = [error domain];
    NSError *newError = [NSError errorWithDomain:domain code:code userInfo:newUserInfo];
    return SecCreateXPCObjectWithCFError((__bridge CFErrorRef)newError);
}

static void communicateWithAcmeServer(xpc_object_t event, const char *requestData, size_t requestLength, const char *acmeURL, const char *method, int64_t state) {
    if (!acmeURL) {
        return;
    }
    __block CFDataRef acmeReq = CFDataCreate(kCFAllocatorDefault, (const unsigned char *)requestData, requestLength);
    secdebug("acme", "Request Length: %ld, URL: %s, State: %lld, Data: %@", requestLength, acmeURL, (long long)state, acmeReq);

    /* The completion block is called when we have a response from the ACME server */
    AcmeRequestCompletionBlock reqCompletionBlock =
    ^(NSURLResponse *response, NSData *data, NSError *err) {
    @autoreleasepool {
        __block xpc_object_t acmeError = NULL;
        __block xpc_object_t acmeNonce = NULL;
        __block xpc_object_t acmeLocation = NULL;
        xpc_object_t acmeStatus = NULL;
        NSInteger statusCode = 0;
        NSString *errStr = nil;
        NSData *nonceJsonData = nil;
        
        debugShowAcmeResponseInfo(response, data, err);

        if (response) {
            NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse*)response;
            NSString *locationKey = @"Location";
            NSString *locationStr = [httpResponse valueForHTTPHeaderField:locationKey];
            NSString *nonceKey = @"Replay-Nonce";
            NSString *nonceStr = [httpResponse valueForHTTPHeaderField:nonceKey];
            if (locationStr) {
                CFStringPerformWithCString((__bridge CFStringRef)locationStr, ^(const char *utf8Str) {
                    acmeLocation = xpc_string_create(utf8Str);
                });
            }
            if (nonceStr) {
                CFStringPerformWithCString((__bridge CFStringRef)nonceStr, ^(const char *utf8Str) {
                    acmeNonce = xpc_string_create(utf8Str);
                });
                NSError *error = nil;
                nonceJsonData = [NSJSONSerialization dataWithJSONObject:@{ nonceKey : nonceStr } options:0 error:&error];
                if (error) {
                    secdebug("acme", "Error serializing nonce to json: %@", error);
                }
            }
            statusCode = [httpResponse statusCode];
            if (statusCode == HTTPResponseCodeNoContent && nonceJsonData) {
                // not an error; we will treat the nonce json data as the content
            } else if (statusCode == HTTPResponseCodeCreated && locationStr.length > 0) {
                // not an error; new account or order was created and returned Location header
            } else if (statusCode != HTTPResponseCodeOK) {
                errStr = [NSHTTPURLResponse localizedStringForStatusCode:(NSInteger)statusCode];
                acmeStatus = xpc_int64_create((int64_t)statusCode);
                secdebug("acme", "Acme Response-b: %d, %@", (int)statusCode, errStr);
            }
        }
        if (err) {
            if (!errStr) {
                errStr = [err description];
            }
            if (!acmeError) {
                acmeError = createXPCErrorWithError(err);
            }
            if (!acmeStatus) {
                NSInteger statusCode = [err code];
                acmeStatus = xpc_int64_create((int64_t)statusCode);
            }
        } else if (errStr) {
            NSError *tmpError = [NSError errorWithDomain:NSURLErrorDomain code:statusCode userInfo:@{NSLocalizedDescriptionKey:errStr}];
            acmeError = createXPCErrorWithError(tmpError);
       }
        
        size_t length = (errStr || !data) ? 0 : [data length];
        xpc_object_t acmeReply = (data && length) ? xpc_data_create([data bytes], length) : NULL;
        xpc_object_t acmeNonceReply = (nonceJsonData) ? xpc_data_create([nonceJsonData bytes], [nonceJsonData length]) : NULL;
        xpc_connection_t peer = xpc_dictionary_get_remote_connection(event);
        xpc_object_t reply = xpc_dictionary_create_reply(event);
        if (reply) {
            if (acmeError) {
                xpc_dictionary_set_value(reply, "AcmeError", acmeError);
            }
            if (acmeStatus) {
                xpc_dictionary_set_value(reply, "AcmeStatus", acmeStatus);
            }
            if (acmeNonce) {
                xpc_dictionary_set_value(reply, "AcmeNonce", acmeNonce);
            }
            if (acmeLocation) {
                xpc_dictionary_set_value(reply, "AcmeLocation", acmeLocation);
            }
            if (acmeReply) {
                xpc_dictionary_set_value(reply, "AcmeReply", acmeReply);
            } else if (acmeNonceReply) {
                xpc_dictionary_set_value(reply, "AcmeReply", acmeNonceReply);
            }
            xpc_connection_send_message(peer, reply);
        }
    }};

    NSString *methodStr = [NSString stringWithUTF8String:method];
    bool isSignedContent = [methodStr isEqualToString:@"POST"];
    NSString *cTypeStr = (isSignedContent) ? AcmeSignedContentType : AcmeContentType;
    secdebug("acme", "Sending Acme Request: %@, url: %s, method: %@, ctype: %@",
          acmeReq, acmeURL, methodStr, cTypeStr);

    sendAcmeRequest((__bridge NSData*)acmeReq, acmeURL, methodStr, cTypeStr, reqCompletionBlock);
}

void handle_request_event(struct connection_info *info, xpc_object_t event) {
    xpc_connection_t peer = xpc_dictionary_get_remote_connection(event);
    xpc_type_t xtype = xpc_get_type(event);

    if (info->done) {
        secdebug("acme", "event %p while done", event);
        return;
    }
	if (xtype == XPC_TYPE_ERROR) {
		if (event == XPC_ERROR_TERMINATION_IMMINENT) {
			// launchd would like us to die, but we have open transactions.
            // When we finish with them xpc_service_main will exit for us,
            // so there is nothing for us to do here.
			return;
		}
        if (!info->done) {
            info->done = true;
        }
        if (peer == NULL && XPC_ERROR_CONNECTION_INVALID == event && 0 != info->processed) {
            // this is a normal shutdown on a connection that has processed at least
            // one request. Nothing interesting to log.
            return;
        }
        secdebug("acme", "listener event error (connection %p): %s",
                 peer, xpc_dictionary_get_string(event, XPC_ERROR_KEY_DESCRIPTION));
	} else if (xtype == XPC_TYPE_DICTIONARY) {
        size_t length = 0;
        const char *operation = xpc_dictionary_get_string(event, "operation");
        audit_token_t auditToken = {};
        xpc_connection_get_audit_token(peer, &auditToken);

        if (operation && !strcmp(operation, "AcmeRequest")) {
            if (callerHasNetworkEntitlement(auditToken)) {
                secdebug("acme", "Handling AcmeRequest event");
                const void *requestData = xpc_dictionary_get_data(event, "request", &length);
                const char *url = xpc_dictionary_get_string(event, "url");
                const char *method = xpc_dictionary_get_string(event, "method");
                int64_t state = xpc_dictionary_get_int64(event, "state");
                communicateWithAcmeServer(event, requestData, length, url, method, state);
            } else {
                secdebug("acme", "No network entitlement for pid %d", xpc_connection_get_pid(peer));
            }
        } else {
            secdebug("acme", "Unknown op=%s request from pid %d", operation, xpc_connection_get_pid(peer));
        }
    } else {
        secdebug("acme", "Unhandled request event=%p type=%p", event, xtype);
    }
}

void finalize_connection(void *not_used) {
	xpc_transaction_end();
}

void handle_connection_event(const xpc_connection_t peer) {
    __block struct connection_info info;
    info.peer = peer;
    info.processed = 0;
    info.done = false;
    
    xpc_connection_set_event_handler(peer, ^(xpc_object_t event) {
        handle_request_event(&info, event);
    });

    //  unlike dispatch objects xpc objects don't need a context set in order to run a finalizer.
    // (we use our finalizer to end the transaction we are about to begin...this keeps xpc from
    // idle exiting us while we have a live connection)
    xpc_connection_set_finalizer_f(peer, finalize_connection);
    xpc_transaction_begin();
    
    // enable the peer connection to receive messages
    xpc_connection_resume(peer);
}

int main(int argc, const char *argv[]) {
    char *wait4debugger = getenv("WAIT4DEBUGGER");
    if (wait4debugger && !strcasecmp("YES", wait4debugger)) {
        syslog(LOG_ERR, "Waiting for debugger");
        kill(getpid(), SIGSTOP);
    }
    enter_sandbox();
    handle_sigterm();
    xpc_main(handle_connection_event);
    return EX_OSERR;
}

