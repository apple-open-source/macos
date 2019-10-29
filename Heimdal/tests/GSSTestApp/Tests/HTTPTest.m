//
//  HTTPTest.m
//  GSSTestApp
//
//  Created by Love Hörnquist Åstrand on 2013-06-09.
//  Copyright (c) 2013 Apple, Inc. All rights reserved.
//

#import "XCTestCase+GSS.h"

#import <Foundation/Foundation.h>
#import <Foundation/NSURLCredential_Private.h>
#if 0
#import <CoreServices/CoreServices.h>
#import <CoreServices/CoreServicesPriv.h>
#endif

#import "TestUsers.h"


@interface HTTPTest : XCTestCase

@end

@interface HTTPTest ()
@property (retain) dispatch_semaphore_t sema;
@property (retain) NSOperationQueue *opQueue;
@property (assign) bool testPassed;
@property (retain) NSString *client;
@property (retain) NSMutableData *content;

@end


@implementation HTTPTest


- (void)connection:(NSURLConnection *)connection didReceiveData:(NSData *)data
{
    [self.content appendData:data];
}

- (BOOL)connection:(NSURLConnection *)connection canAuthenticateAgainstProtectionSpace:(NSURLProtectionSpace *)protectionSpace
{
    [self XCTOutput:@"canAuthenticateAgainstProtectionSpace: %@", [protectionSpace authenticationMethod]];
    if ([[protectionSpace authenticationMethod] isEqualToString:NSURLAuthenticationMethodNegotiate])
        return YES;
    
    return NO;
}

- (void)connection:(NSURLConnection *)connection didReceiveResponse:(NSURLResponse *)response {
    [self XCTOutput:@"Connection didReceiveResponse! Response - %@", response];
}

- (void)connectionDidFinishLoading:(NSURLConnection *)connection {
    NSLog(@"Finished...");
    self.testPassed = YES;
    [self XCTOutput:@"data:\n\n%@\n", [[NSString alloc] initWithData:self.content encoding:NSUTF8StringEncoding]];
    self.content = NULL;
    dispatch_semaphore_signal(self.sema);
}

- (void)connection:(NSURLConnection *)connection didFailWithError:(NSError *)error
{
	[self XCTOutput:@"didFailWithError: %@", error];
    dispatch_semaphore_signal(self.sema);
}

- (NSURLRequest *)connection:(NSURLConnection *)connection willSendRequest:(NSURLRequest *)request redirectResponse:(NSURLResponse *)redirectResponse
{
	[self XCTOutput:@"willSendRequest"];
	return request;
}

- (BOOL)connectionShouldUseCredentialStorage:(NSURLConnection *)connection
{
	[self XCTOutput:@"connectionShouldUseCredentialStorage"];
	return NO;
}


- (void)connection:(NSURLConnection*)connection didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    NSURLProtectionSpace *protectionSpace = [challenge protectionSpace];
	
	[self XCTOutput:@"didReceiveAuthenticationChallenge: %@ %@", [protectionSpace authenticationMethod], [protectionSpace host]];
    
    NSString *serverPrincipal = [NSString stringWithFormat:@"HTTP/%@", [protectionSpace host]];
    
    
	CFURLCredentialRef cfCredential = _CFURLCredentialCreateForKerberosTicket(NULL, (__bridge CFStringRef)self.client, (__bridge CFStringRef)serverPrincipal, NULL);
    if (cfCredential == NULL) {
        [[challenge sender] cancelAuthenticationChallenge:challenge];
        return;
    }
    
    NSURLCredential *credential = [[NSURLCredential alloc] _initWithCFURLCredential:cfCredential];
	
    [[challenge sender] useCredential:credential forAuthenticationChallenge:challenge];
    
    CFRelease(cfCredential);
}



- (void)testHTTP_CEAD {
    gss_cred_id_t cred = NULL;
    
    self.sema = dispatch_semaphore_create(0);
    self.opQueue = [[NSOperationQueue alloc] init];
    self.testPassed = NO;
    self.content = [NSMutableData data];
    
    [self XTCDestroyCredential:GSS_C_NO_OID];
    
    self.client = @"heimdal001@CORP.CEAD.APPLE.COM";

    NSDictionary *options = @{
        (id)kGSSICPassword : passwordKtestuserCEAD
    };
    
    cred = [self XTCAcquireCredential:self.client withOptions:options mech:GSS_KRB5_MECHANISM];
    XCTAssertTrue(cred, @"Failed to acquire credential from ADS");
    if (cred == NULL)
        return;
    
    CFRelease(cred);
    
    NSURL *url = [NSURL URLWithString:@"http://negotiate.cead.apple.com/index.aspx"];

    NSURLRequest *request = [NSURLRequest requestWithURL:url];
    NSURLConnection *conn = [[NSURLConnection alloc] initWithRequest: request delegate: self startImmediately:NO];
    
    [conn setDelegateQueue:self.opQueue];
    [conn start];
    
    dispatch_semaphore_wait(self.sema, dispatch_time(DISPATCH_TIME_NOW, 30ULL * NSEC_PER_SEC));
    
    XCTAssertTrue(self.testPassed, @"http test didn't pass");
}

@end
