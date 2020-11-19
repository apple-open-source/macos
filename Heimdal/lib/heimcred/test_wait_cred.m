/*-
 * Copyright (c) 2016 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#import <TargetConditionals.h>

#import <Foundation/Foundation.h>
#import <err.h>
#import <getarg.h>
#import <notify.h>

#import "heimcred.h"
#import "common.h"

static char *wait_name;
static int help_flag	= 0;
static int token;

static struct getargs args[] = {
    {"help",	0,	arg_flag,	&help_flag,  NULL, NULL }
};

static void
usage (int ret)
{
    arg_printusage (args, sizeof(args)/sizeof(*args), NULL, "");
    exit (ret);
}

static NSDate * getMostRecentAuthTimeForUPN(NSString *waitName) {
    NSDictionary *query = @{
	(id)kHEIMAttrType:(id)kHEIMTypeKerberos,
	(id)kHEIMAttrLeadCredential:(id)kCFBooleanTrue,
	(id)kHEIMAttrClientName:waitName
	
    };
    NSArray *caches = (__bridge NSArray*)HeimCredCopyQuery((__bridge CFDictionaryRef)query);
    
    HeimCredRef credToReturn = NULL;
    for (NSObject *obj in caches) {
	HeimCredRef cred = (HeimCredRef)obj;
	if (credToReturn == NULL) {
	    credToReturn = cred;
	} else {
	    NSDate *credDate = HeimCredCopyAttribute(cred, kHEIMAttrStoreTime);
	    NSDate *credToReturnDate = HeimCredCopyAttribute(credToReturn, kHEIMAttrStoreTime);
	    if ([credDate compare:credToReturnDate] == kCFCompareGreaterThan) {
		credToReturn = cred;
	    }
	}
    }
    
    if (credToReturn!=NULL) {
	NSDate *result = HeimCredCopyAttribute(credToReturn, kHEIMAttrStoreTime);
	return result;
    }
    return nil;
}

int
main(int argc, char **argv)
{
    int optidx = 0;
    setprogname(argv[0]);
    if(getarg(args, sizeof(args) / sizeof(args[0]), argc, argv, &optidx))
	usage(1);
    
    if (help_flag)
	usage (0);
    
    argc -= optidx;
    argv += optidx;
    
    if (argc > 1)
	errx(1, "argc > 1");
    
    @autoreleasepool {
	
	NSString *waitName = nil;
	bool hasUpnToWatch = false;
	if (argc == 1) {
	    //the parameter is the upn to watch
	    wait_name = argv[0];
	    waitName = [NSString stringWithCString:wait_name encoding:NSUTF8StringEncoding];
	    hasUpnToWatch = true;
	}
	
	//handle interrupts to cancel the notification then exit
	dispatch_source_t source = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, SIGINT, 0, dispatch_get_global_queue(0, 0));
	dispatch_source_set_event_handler(source, ^{
	    printf("got SIGINT, exiting\n");
	    if (token!=0) {
		notify_cancel(token);
	    }
	    exit(1);
	});
	dispatch_resume(source);
	
	struct sigaction action = { 0 };
	action.sa_handler = SIG_IGN;
	sigaction(SIGINT, &action, NULL);
	
	NSDate *beforeDate = nil;
	if (hasUpnToWatch) {
	    // if the upn to watch has been updated within the last second, then return.
	    // the use case is in a script and if the commands immediately before this have completed, then do not wait.
	    beforeDate = getMostRecentAuthTimeForUPN(waitName);
	    printf("before age: %f\n", [beforeDate timeIntervalSinceNow]);
	    if (beforeDate!=nil && [beforeDate timeIntervalSinceNow] > -1.0) {
		int rounded = round([beforeDate timeIntervalSince1970]);
		printf("%i", rounded);
	    }
	}
	
	dispatch_queue_t queue = dispatch_queue_create("com.apple.GSSCred.test_wait_cred", DISPATCH_QUEUE_SERIAL);
	
	__block dispatch_semaphore_t sema = dispatch_semaphore_create(0);
	notify_register_dispatch("com.apple.Kerberos.cache.changed", &token, queue, ^(int t) {
	    if (hasUpnToWatch) {
		//if the principal we are interested in has changed, then return
		NSDate *result = getMostRecentAuthTimeForUPN(waitName);
		if (result!=nil && ![result isEqualToDate:beforeDate]) {
		    printf("cred updated\n");
		    int rounded = round([result timeIntervalSince1970]);
		    printf("%i\n", rounded);
		    dispatch_semaphore_signal(sema);
		}
	    } else {
		dispatch_semaphore_signal(sema);
	    }
	});
	
	//wait up to 60 seconds
	if(dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, ((long long)60) * NSEC_PER_SEC))) {
	    printf("timeout waiting for creds\n");
	    notify_cancel(token);
	    exit(1);
	}
	
	notify_cancel(token);
	return 0;
    }
}
