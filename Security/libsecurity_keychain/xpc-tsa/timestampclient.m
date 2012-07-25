/*
 * Copyright (c) 2012 Apple, Inc. All Rights Reserved.
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

//
//  timestampclient.m
//  libsecurity_keychain
//

#import "timestampclient.h"

void sendTSARequest(CFDataRef tsaReq, const char *tsaURL, TSARequestCompletionBlock completionBlock)
{
    NSAutoreleasePool *pool = 	[[NSAutoreleasePool alloc] init];
    
    NSString *urlStr = [NSString stringWithCString:tsaURL encoding:NSUTF8StringEncoding];
    TimeStampClient *client = [[TimeStampClient alloc] initWithURLString:urlStr];
    [client post:(NSData *)tsaReq];
    [client start3:completionBlock];
    [client release];
    [pool release];
}

@implementation TimeStampClient

@synthesize delegate;
@synthesize url;
@synthesize urlRequest;
 
- (id)init
{
    self = [super init];
    return self;
}

- (id)initWithURLString:(NSString *)urlStr
{
    if ((self = [super init]))
    {
        NSString *escapedURLStr = [urlStr stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding];
        self.url = [NSURL URLWithString:escapedURLStr];
        self.urlRequest = [[NSMutableURLRequest alloc] initWithURL:self.url 
            cachePolicy:NSURLRequestReloadIgnoringLocalCacheData timeoutInterval:(NSTimeInterval)15.0];
    }
    return self;
}
    
- (void)dealloc
{
    [url release];
    [urlRequest release];
    [super dealloc];
}

- (void)post: (NSData *)data
{
    NSMutableURLRequest *request = self.urlRequest;
    
    [request setHTTPMethod:@"POST"];
    [request setHTTPBody:data];
    [request setValue:@"application/timestamp-query" forHTTPHeaderField:@"Content-Type"];
}	

- (void)start3:(TSARequestCompletionBlock)completionBlock
{    
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_LOW, 0),
    ^{
        NSOperationQueue *opq = [[NSOperationQueue alloc] init];
        [NSURLConnection sendAsynchronousRequest:self.urlRequest
            queue:opq completionHandler: completionBlock            
        ];
    });
}

@end




