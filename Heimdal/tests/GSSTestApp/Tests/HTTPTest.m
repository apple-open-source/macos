//
//  HTTPTest.m
//  GSSTestApp
//
//  Created by Love Hörnquist Åstrand on 2013-06-09.
//  Copyright (c) 2013 Apple, Inc. All rights reserved.
//

#import "HTTPTest.h"
#import "SenTestCase+GSS.h"

#import <Foundation/Foundation.h>
#import <Foundation/NSURLCredential_Private.h>
#import <CoreServices/CoreServices.h>
#import <CoreServices/CoreServicesPriv.h>


@interface HTTPTest ()
@property (retain) dispatch_semaphore_t sema;
@property (retain) NSOperationQueue *opQueue;
@property (assign) bool testPassed;
@property (retain) NSString *client;

@end


@implementation HTTPTest


- (void)connection:(NSURLConnection *)connection didReceiveData:(NSData *)data
{
}

- (BOOL)connection:(NSURLConnection *)connection canAuthenticateAgainstProtectionSpace:(NSURLProtectionSpace *)protectionSpace
{
    [self STCOutput:[NSString stringWithFormat:@"canAuthenticateAgainstProtectionSpace: %@", [protectionSpace authenticationMethod]]];
    if ([[protectionSpace authenticationMethod] isEqualToString:NSURLAuthenticationMethodNegotiate])
        return YES;
    
    return NO;
}

- (void)connection:(NSURLConnection *)connection didReceiveResponse:(NSURLResponse *)response {
    [self STCOutput:[NSString stringWithFormat:@"Connection didReceiveResponse! Response - %@", response]];
}

- (void)connectionDidFinishLoading:(NSURLConnection *)connection {
    NSLog(@"Finished...");
    self.testPassed = YES;
    dispatch_semaphore_signal(self.sema);
}

- (void)connection:(NSURLConnection *)connection didFailWithError:(NSError *)error
{
	[self STCOutput:[NSString stringWithFormat:@"didFailWithError"]];
    dispatch_semaphore_signal(self.sema);
}

- (NSURLRequest *)connection:(NSURLConnection *)connection willSendRequest:(NSURLRequest *)request redirectResponse:(NSURLResponse *)redirectResponse
{
	[self STCOutput:[NSString stringWithFormat:@"willSendRequest"]];
	return request;
}

- (BOOL)connectionShouldUseCredentialStorage:(NSURLConnection *)connection
{
	[self STCOutput:[NSString stringWithFormat:@"connectionShouldUseCredentialStorage"]];
	return NO;
}


- (void)connection:(NSURLConnection*)connection didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    NSURLProtectionSpace *protectionSpace = [challenge protectionSpace];
	
	[self STCOutput:[NSString stringWithFormat:@"didReceiveAuthenticationChallenge: %@ %@", [protectionSpace authenticationMethod], [protectionSpace host]]];
    
    NSString *serverPrincipal = [NSString stringWithFormat:@"HTTP@%@", [protectionSpace host]];
    
    
	CFURLCredentialRef cfCredential = _CFURLCredentialCreateForKerberosTicket(NULL, (__bridge CFStringRef)self.client, (__bridge CFStringRef)serverPrincipal, NULL);
    if (cfCredential == NULL) {
        [[challenge sender] cancelAuthenticationChallenge:challenge];
        return;
    }
    
    NSURLCredential *credential = [[NSURLCredential alloc] _initWithCFURLCredential:cfCredential];
	
    [[challenge sender] useCredential:credential forAuthenticationChallenge:challenge];
    
    CFRelease(cfCredential);
}



- (void)testHTTP_ADS {
    gss_cred_id_t cred = NULL;
    
    self.sema = dispatch_semaphore_create(0);
    self.opQueue = [[NSOperationQueue alloc] init];
    self.testPassed = NO;
    
    [self STCDestroyCredential:GSS_C_NO_OID];
    
    self.client = @"ktestuser@ADS.APPLE.COM";
    
    cred = [self STCAcquireCredential:self.client withPassword:@"foobar" mech:GSS_KRB5_MECHANISM];
    STAssertTrue(cred, @"Failed to acquire credential from ADS");
    if (cred == NULL)
        return;
    
    CFRelease(cred);
    
    NSURL *url = [NSURL URLWithString:@"http://dc03.ads.apple.com/negotiate/"];

    NSURLRequest *request = [NSURLRequest requestWithURL:url];
    NSURLConnection *conn = [[NSURLConnection alloc] initWithRequest: request delegate: self startImmediately:NO];
    
    [conn setDelegateQueue:self.opQueue];
    [conn start];
    
    dispatch_semaphore_wait(self.sema, dispatch_time(DISPATCH_TIME_NOW, 30ULL * NSEC_PER_SEC));
    
    STAssertTrue(self.testPassed, @"http test didn't pass");
}

@end
