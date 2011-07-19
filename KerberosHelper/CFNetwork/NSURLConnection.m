/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
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

#import <KerberosHelper/NetworkAuthenticationHelper.h>
#import <KerberosHelper/KerberosHelper.h>
#import <Foundation/Foundation.h>
#import <Foundation/NSURLConnectionPrivate.h>
#import <CoreServices/CoreServices.h>
#import <CoreServices/CoreServicesPriv.h>

@interface NSURLCredential (NSURLCredentialInternal)
- (id) _initWithCFURLCredential:(CFURLCredentialRef)credential;
@end

@interface Foo : NSObject <NSURLConnectionDelegate> {
    NAHRef _nah;
    CFArrayRef _selections;
    CFIndex _selection_index;
}

- (void)req:(NSURL *)url;



- (BOOL)connection:(NSURLConnection *)connection canAuthenticateAgainstProtectionSpace:(NSURLProtectionSpace *)protectionSpace;
- (void)connection:(NSURLConnection *)connection didCancelAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge;
- (void)connection:(NSURLConnection *)connection didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge;
- (BOOL)connectionShouldUseCredentialStorage:(NSURLConnection *)connection;

- (NSCachedURLResponse *)connection:(NSURLConnection *)connection willCacheResponse:(NSCachedURLResponse *)cachedResponse;
- (void)connection:(NSURLConnection *)connection didReceiveResponse:(NSURLResponse *)response;
- (void)connection:(NSURLConnection *)connection didReceiveData:(NSData *)data;
- (void)connection:(NSURLConnection *)connection didSendBodyData:(NSInteger)bytesWritten totalBytesWritten:(NSInteger)totalBytesWritten totalBytesExpectedToWrite:(NSInteger)totalBytesExpectedToWrite;
- (NSURLRequest *)connection:(NSURLConnection *)connection willSendRequest:(NSURLRequest *)request redirectResponse:(NSURLResponse *)redirectResponse;

- (void)connection:(NSURLConnection *)connection  didFailWithError:(NSError *)error;
- (void)connectionDidFinishLoading:(NSURLConnection *)connection;


@end

@implementation Foo


- (NSCachedURLResponse *)connection:(NSURLConnection *)connection willCacheResponse:(NSCachedURLResponse *)cachedResponse
{
	return nil;
}

- (void)connection:(NSURLConnection *)connection didCancelAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
}


- (void)connection:(NSURLConnection *)connection didReceiveData:(NSData *)data
{
}

- (void)connection:(NSURLConnection *)connection didSendBodyData:(NSInteger)bytesWritten totalBytesWritten:(NSInteger)totalBytesWritten totalBytesExpectedToWrite:(NSInteger)totalBytesExpectedToWrite
{
}


- (BOOL)connection:(NSURLConnection *)connection canAuthenticateAgainstProtectionSpace:(NSURLProtectionSpace *)protectionSpace
{
    NSLog(@"canAuthenticateAgainstProtectionSpace: %@", [protectionSpace authenticationMethod]);
    if ([[protectionSpace authenticationMethod] isEqualToString:NSURLAuthenticationMethodNegotiate])
	return YES;
    
    return NO;
}

- (void)connection:(NSURLConnection *)connection didReceiveResponse:(NSURLResponse *)response {
    NSLog(@"Connection didReceiveResponse! Response - %@", response);
}
- (void)connectionDidFinishLoading:(NSURLConnection *)connection {
    NSLog(@"Finished...");
	exit(0);
}

- (void)connection:(NSURLConnection *)connection didFailWithError:(NSError *)error
{
	NSLog(@"didFailWithError");
}

- (NSURLRequest *)connection:(NSURLConnection *)connection willSendRequest:(NSURLRequest *)request redirectResponse:(NSURLResponse *)redirectResponse
{
	NSLog(@"willSendRequest");
	return request;
}

- (BOOL)connectionShouldUseCredentialStorage:(NSURLConnection *)connection
{
	NSLog(@"connectionShouldUseCredentialStorage");
	return NO;
}


- (void)connection:(NSURLConnection*)connection didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    NSURLProtectionSpace *protectionSpace = [challenge protectionSpace];
	
	NSLog(@"didReceiveAuthenticationChallenge: %@ %@", [protectionSpace authenticationMethod], [protectionSpace host]);

    if (_nah == NULL) {
	CFMutableDictionaryRef info = NULL;
	CFDictionaryRef krbtoken = NULL;

	info = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	krbtoken = KRBCreateNegTokenLegacyKerberos(NULL);

	CFDictionaryAddValue(info, kNAHNegTokenInit, krbtoken);
	CFRelease(krbtoken);

	/* XXX add user to info */
	/* XXX add password to info if we have one so that NAHAcquireCredential have something to do */

	_nah = NAHCreate(NULL, (CFStringRef)[protectionSpace host], CFSTR("HTTP"), info);
	if (_nah == NULL)
	    goto failed;

	_selections = NAHGetSelections(_nah);
	_selection_index = 0;
    }

 next:
    if (_selection_index >= CFArrayGetCount(_selections))
	goto failed;

    NAHSelectionRef sel = (NAHSelectionRef)CFArrayGetValueAtIndex(_selections, _selection_index);

    _selection_index += 1;

    if (!NAHSelectionAcquireCredential(sel, NULL, NULL))
	goto next;


    CFStringRef clientPrincipal = NAHSelectionGetInfoForKey(sel, kNAHClientPrincipal);
    CFStringRef serverPrincipal = NAHSelectionGetInfoForKey(sel, kNAHServerPrincipal);

	NSLog(@"trying: client: %@ server: %@", (NSString *)clientPrincipal, (NSString *)serverPrincipal);
		  
	CFURLCredentialRef cfCredential = _CFURLCredentialCreateForKerberosTicket(NULL, clientPrincipal, serverPrincipal, NULL);
    if (cfCredential)
	goto failed;

    NSURLCredential *credential = [[[NSURLCredential alloc] _initWithCFURLCredential:cfCredential] autorelease];
	
    [[challenge sender] useCredential:credential forAuthenticationChallenge:challenge];

    CFRelease(cfCredential);

    return;

 failed:

    [[challenge sender] continueWithoutCredentialForAuthenticationChallenge:challenge];

    if (_selections)
	CFRelease(_selections);
    if (_nah)
	CFRelease(_nah);
    _nah = NULL;
    _selections = NULL;
}





- (void)req:(NSURL *)url
{
    NSURLRequest *request = [NSURLRequest requestWithURL:url];
    NSURLConnection *conn;

    conn = [[NSURLConnection alloc] initWithRequest: request delegate: self];

    [conn scheduleInRunLoop: [NSRunLoop currentRunLoop] forMode: NSDefaultRunLoopMode];

}

@end

int
main(int argc, char **argv)
{
    NSURL *url;
    Foo *foo;
	
    url = [NSURL URLWithString:[NSString stringWithUTF8String:argv[1]]];

    foo = [[Foo alloc] init];

    [foo req: url];

    [[NSRunLoop currentRunLoop] run];

    NSLog(@"done");

    return 0;
}
