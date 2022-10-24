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
//  acmeclient.h
//  XPCAcmeService
//

#import <Foundation/Foundation.h>

// See NSURLConnection completion handler
typedef void (^AcmeRequestCompletionBlock)(NSURLResponse *response, NSData *data, NSError *err);

void sendAcmeRequest(NSData *acmeReq, const char *acmeURL,
                     NSString *method, NSString *contentType,
                     AcmeRequestCompletionBlock completionBlock);

@interface AcmeClient : NSObject {
    __weak id delegate;
    NSURL *url;
    NSMutableURLRequest *urlRequest;
}
@property (weak) id delegate;
@property (retain) id url;
@property (retain) id urlRequest;

- (id)initWithURLString:(NSString *)urlStr;
- (void)post:(NSData *)data withMethod:(NSString *)method contentType:contentType;
- (void)start3:(AcmeRequestCompletionBlock)completionBlock;

@end
