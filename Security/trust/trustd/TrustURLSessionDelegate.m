/*
 * Copyright (c) 2018 Apple Inc. All Rights Reserved.
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
 *
 */

#import <AssertMacros.h>
#import <Foundation/Foundation.h>
#include <mach/mach_time.h>
#include <utilities/SecCFWrappers.h>
#include <Security/SecInternalReleasePriv.h>
#include "TrustURLSessionDelegate.h"

#define MAX_TASKS 3

/* There has got to be an easier way to do this.  For now we based this code
 on CFNetwork/Connection/URLResponse.cpp. */
static CFStringRef copyParseMaxAge(CFStringRef cacheControlHeader) {
    if (!cacheControlHeader) { return NULL; }

    /* The format of the cache control header is a comma-separated list, but
     each list element could be a key-value pair, with the value quoted and
     possibly containing a comma. */
    CFStringInlineBuffer inlineBuf = {};
    CFRange componentRange;
    CFIndex length = CFStringGetLength(cacheControlHeader);
    bool done = false;
    CFCharacterSetRef whitespaceSet = CFCharacterSetGetPredefined(kCFCharacterSetWhitespace);
    CFStringRef maxAgeValue = NULL;

    CFStringInitInlineBuffer(cacheControlHeader, &inlineBuf, CFRangeMake(0, length));
    componentRange.location = 0;

    while (!done) {
        bool inQuotes = false;
        bool foundComponentStart = false;
        CFIndex charIndex = componentRange.location;
        CFIndex componentEnd = -1;
        CFRange maxAgeRg;
        componentRange.length = 0;

        while (charIndex < length) {
            UniChar ch = CFStringGetCharacterFromInlineBuffer(&inlineBuf, charIndex);
            if (!inQuotes && ch == ',') {
                componentRange.length = charIndex - componentRange.location;
                break;
            }
            if (!CFCharacterSetIsCharacterMember(whitespaceSet, ch)) {
                if (!foundComponentStart) {
                    foundComponentStart = true;
                    componentRange.location = charIndex;
                } else {
                    componentEnd = charIndex;
                }
                if (ch == '\"') {
                    inQuotes = (inQuotes == false);
                }
            }
            charIndex ++;
        }

        if (componentEnd == -1) {
            componentRange.length = charIndex - componentRange.location;
        } else {
            componentRange.length = componentEnd - componentRange.location + 1;
        }

        if (charIndex == length) {
            /* Fell off the end; this is the last component. */
            done = true;
        }

        /* componentRange should now contain the range of the current
         component; trimmed of any whitespace. */

        /* We want to look for a max-age value. */
        if (!maxAgeValue && CFStringFindWithOptions(cacheControlHeader, CFSTR("max-age"), componentRange, kCFCompareCaseInsensitive | kCFCompareAnchored, &maxAgeRg)) {
            CFIndex equalIdx;
            CFIndex maxCompRg = componentRange.location + componentRange.length;
            for (equalIdx = maxAgeRg.location + maxAgeRg.length; equalIdx < maxCompRg; equalIdx ++) {
                UniChar equalCh = CFStringGetCharacterFromInlineBuffer(&inlineBuf, equalIdx);
                if (equalCh == '=') {
                    // Parse out max-age value
                    equalIdx ++;
                    while (equalIdx < maxCompRg && CFCharacterSetIsCharacterMember(whitespaceSet, CFStringGetCharacterAtIndex(cacheControlHeader, equalIdx))) {
                        equalIdx ++;
                    }
                    if (equalIdx < maxCompRg) {
                        CFReleaseNull(maxAgeValue);
                        maxAgeValue = CFStringCreateWithSubstring(kCFAllocatorDefault, cacheControlHeader, CFRangeMake(equalIdx, maxCompRg-equalIdx));
                    }
                } else if (!CFCharacterSetIsCharacterMember(whitespaceSet, equalCh)) {
                    // Not a valid max-age header; break out doing nothing
                    break;
                }
            }
        }

        if (!done && maxAgeValue) {
            done = true;
        }
        if (!done) {
            /* Advance to the next component; + 1 to get past the comma. */
            componentRange.location = charIndex + 1;
        }
    }

    return maxAgeValue;
}

@implementation TrustURLSessionDelegate
- (id)init {
    /* Protect future developers from themselves */
    if ([self class] == [TrustURLSessionDelegate class]) {
        NSException *e = [NSException exceptionWithName:@"AbstractClassException"
                                                 reason:@"This is an abstract class. To use it, please subclass."
                                               userInfo:nil];
        @throw e;
    } else {
        return [super init];
    }
}

- (NSURLRequest *)createNextRequest:(NSURL *)uri {
    return [NSURLRequest requestWithURL:uri];
}

- (BOOL)fetchNext:(NSURLSession *)session {
    if (self.numTasks >= MAX_TASKS) {
        secnotice("http", "Too many fetch %@ requests for this cert", [self class]);
        return true;
    }

    for (NSUInteger ix = self.URIix; ix < [self.URIs count]; ix++) {
        NSURL *uri = self.URIs[ix];
        if ([[uri scheme] isEqualToString:@"http"]) {
            self.URIix = ix + 1; // Next time we'll start with the next index
            self.numTasks++;
            NSURLSessionTask *task = [session dataTaskWithRequest:[self createNextRequest:uri]];
            [task resume];
            secinfo("http", "request for uri: %@", uri);
            return false; // we scheduled a job
        } else {
            secnotice("http", "skipping unsupported scheme %@", [uri scheme]);
        }
    }

    /* No more issuers left to try, we're done. Report that no async jobs were started. */
    secdebug("http", "no request issued");
    return true;
}

- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask didReceiveData:(NSData *)data {
    /* Append the data to the response data*/
    if (!_response) {
        _response = [NSMutableData data];
    }
    [_response appendData:data];
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didCompleteWithError:(NSError *)error {
    /* Protect future developers from themselves */
    if ([self class] == [TrustURLSessionDelegate class]) {
        NSException *e = [NSException exceptionWithName:@"AbstractClassException"
                                                 reason:@"This is an abstract class. To use it, please subclass and override didCompleteWithError."
                                               userInfo:nil];
        @throw e;
    } else {
        _expiration = 60.0 * 60.0 * 24.0 * 7; /* Default is 7 days */
        if ([_response length] > 0 && [[task response] isKindOfClass:[NSHTTPURLResponse class]]) {
            NSString *cacheControl = [[(NSHTTPURLResponse *)[task response] allHeaderFields] objectForKey:@"cache-control"];
            NSString *maxAge = CFBridgingRelease(copyParseMaxAge((__bridge CFStringRef)cacheControl));
            if (maxAge && [maxAge doubleValue] > _expiration) {
                _expiration = [maxAge doubleValue];
            }
        }
    }
}

- (void)URLSession:(NSURLSession *)session
              task:(NSURLSessionTask *)task
willPerformHTTPRedirection:(NSHTTPURLResponse *)redirectResponse
        newRequest:(NSURLRequest *)request
 completionHandler:(void (^)(NSURLRequest *))completionHandler {
    /* The old code didn't allow re-direction, so we won't either. */
    secnotice("http", "failed redirection for %@", task.originalRequest.URL);
    [task cancel];
}
@end

NSTimeInterval TrustURLSessionGetResourceTimeout(void) {
    return (NSTimeInterval)3.0;
}
