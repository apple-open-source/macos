/*
 * Copyright (c) 2014 Apple Inc. All Rights Reserved.
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
#import <Foundation/Foundation.h>
#import <CrashReporterSupport/CrashReporterSupportPrivate.h>
#import <sys/utsname.h>

static bool useInternalServer() {
	struct utsname name;
	return ((uname(&name) == 0 && strstr(name.version, "DEVELOPMENT") != NULL)
	        && access("/var/db/gkoverride_use_internal", F_OK) == 0);
}

@interface OverrideClient : NSObject<NSURLConnectionDelegate> {
	BOOL allow;
	NSMutableData *buffer;
	NSURLConnection *connection;
	int state;
}
@property NSString *currentHash;
@property NSString *opaqueHash;
@property NSString *bundleID;
@property NSString *bundleVersion;
- (NSString *)serverHost;
- (void)sendQuery;
- (void)sendReport;
- (void)abort;
@end

@implementation OverrideClient

- (NSString *)serverHost {
	if (useInternalServer())
		return @"gkq-stg.siri.apple.com";
	else
		return @"gkq.apple.com";
}

- (id)init {
	self = [super init];
	allow = NO;
	buffer = [NSMutableData new];
	return self;
}

- (void)sendQuery {
	state = 1;
	NSMutableURLRequest *request = [NSMutableURLRequest new];
	NSString *url = [NSString stringWithFormat:@"https://%@/q/%@/%@", [self serverHost], self.currentHash, self.opaqueHash];
	[request setURL:[NSURL URLWithString:url]];
	[request setCachePolicy:NSURLRequestReloadIgnoringLocalCacheData];
	[request setTimeoutInterval:5];
	connection = [NSURLConnection connectionWithRequest:request delegate:self];
}

- (void)sendReport {
	state = 2;
	NSMutableURLRequest *request = [NSMutableURLRequest new];
	NSString *body = [NSString stringWithFormat:@"current=%@&bundleid=%@&version=%@",
		[self.currentHash stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding],
		[self.bundleID stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding],
		[self.bundleVersion stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding]];
	NSString *url = [NSString stringWithFormat:@"https://%@/report", [self serverHost]];
	[request setURL:[NSURL URLWithString:url]];
	[request setTimeoutInterval:5];
	[request setHTTPMethod:@"POST"];
	[request setHTTPBody:[body dataUsingEncoding:NSUTF8StringEncoding]];
	connection = [NSURLConnection connectionWithRequest:request delegate:self];
}

- (BOOL)connection:(NSURLConnection *)conn canAuthenticateAgainstProtectionSpace:(NSURLProtectionSpace *)protectionSpace {
	SecTrustRef trust = [protectionSpace serverTrust];
	// abort if untrusted
	SecTrustResultType trustResult;
	SecTrustEvaluate(trust, &trustResult);
	if (trustResult != kSecTrustResultProceed && trustResult != kSecTrustResultUnspecified) {
		[conn cancel];
		[self abort];
		return NO;
	}
	// allow if server presented an EV cert
	NSDictionary *result = CFBridgingRelease(SecTrustCopyResult(trust));
	if (result) {
		NSNumber *ev = [result objectForKey:(__bridge id)kSecTrustExtendedValidation];
		if (ev != NULL && [ev boolValue] == YES) {
			return NO;
		}
	}
	// allow if using internal server (EV not required)
	if (useInternalServer())
		return NO;
	// otherwise abort
	[conn cancel];
	[self abort];
	return NO;
}

- (void)connection:(NSURLConnection *)connection didReceiveData:(NSData *)data {
	[buffer appendData:data];
	if ([buffer length] > 100)
		[self abort];
}

- (void)connectionDidFinishLoading:(NSURLConnection *)connection {
	if (state == 1) {
		NSString *verdict = [[NSString alloc] initWithData:buffer encoding:NSUTF8StringEncoding];
		if ([verdict isEqualToString:@"allow"]) {
			allow = YES;
			[self abort];
		} else if (CRIsAutoSubmitEnabled()) {	// "Send diagnostic & usage data to Apple" checked
			[self sendReport];
		}
	} else {
		[self abort];	// report sent
	}
}

- (void)connection:(NSURLConnection *)connection didFailWithError:(NSError *)error {
	[self abort];
}

- (void)abort {
	if (allow) {
		printf("allow\n");
		exit(42);
	} else {
		printf("deny\n");
		exit(1);
	}
}

@end

int main(int argc, const char * argv[]) {
	if (argc != 5) {
		fprintf(stderr, "usage: %s <current> <opaque> <bundleid> <version>\n", argv[0]);
		return 1;
	}
	@autoreleasepool {
		OverrideClient *client = [OverrideClient new];
		client.currentHash = [NSString stringWithUTF8String:argv[1]];
		client.opaqueHash = [NSString stringWithUTF8String:argv[2]];
		client.bundleID = [NSString stringWithUTF8String:argv[3]];
		client.bundleVersion = [NSString stringWithUTF8String:argv[4]];
		[client sendQuery];
		[[NSRunLoop currentRunLoop] run];
	}
	return 0;
}
