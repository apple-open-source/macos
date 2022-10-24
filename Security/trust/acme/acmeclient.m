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

//
//  acmeclient.m
//  XPCAcmeService
//

#import "acmeclient.h"

const NSString *AcmeUserAgent = @"com.apple.security.acmeclient/1.0";

void sendAcmeRequest(NSData *acmeReq, const char *acmeURL,
                     NSString *method, NSString *contentType,
                     AcmeRequestCompletionBlock completionBlock) {
    @autoreleasepool {
        NSString *urlStr = [NSString stringWithCString:acmeURL encoding:NSUTF8StringEncoding];
        AcmeClient *client = [[AcmeClient alloc] initWithURLString:urlStr];
        [client post:(NSData *)acmeReq withMethod:method contentType:contentType];
        [client start3:completionBlock];
    }
}

@implementation AcmeClient

@synthesize delegate;
@synthesize url;
@synthesize urlRequest;
 
- (id)init {
    if ((self = [super init])) {
    }
    return self;
}

- (id)initWithURLString:(NSString *)urlStr {
    if ((self = [super init])) {
        NSString *escapedURLStr = [urlStr stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding];
        url = [[NSURL alloc] initWithString:escapedURLStr];
        urlRequest = [[NSMutableURLRequest alloc] initWithURL:self.url
            cachePolicy:NSURLRequestReloadIgnoringLocalCacheData timeoutInterval:(NSTimeInterval)15.0];
    }
    return self;
}
    
- (void)post:(NSData *)data withMethod:(NSString *)method contentType:(NSString *)contentType {
    NSMutableURLRequest *request = self.urlRequest;
    [request setHTTPMethod:method];
    [request setHTTPBody:data];
    [request setValue:(NSString *)AcmeUserAgent forHTTPHeaderField:@"User-Agent"];
    [request setValue:contentType forHTTPHeaderField:@"Content-Type"];
}	

- (void)start3:(AcmeRequestCompletionBlock)completionBlock {
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_LOW, 0), ^{
        NSOperationQueue *opq = [[NSOperationQueue alloc] init];
        [NSURLConnection sendAsynchronousRequest:self.urlRequest
            queue:opq completionHandler: completionBlock            
        ];
    });
}

@end




