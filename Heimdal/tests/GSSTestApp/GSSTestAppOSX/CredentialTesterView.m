//
//  CredentialTesterView.m
//  GSSTestApp
//
//  Created by Love Hörnquist Åstrand on 2013-07-03.
//  Copyright (c) 2013 Apple, Inc. All rights reserved.
//

#import "CredentialTesterView.h"

#import <Foundation/Foundation.h>
#import <Foundation/NSURLCredential_Private.h>
#import <CoreServices/CoreServices.h>
#import <CoreServices/CoreServicesPriv.h>


@interface CredentialTesterView ()
@property (assign) gss_cred_id_t credential;
@property (retain) NSMutableData *content;
@property (retain) NSOperationQueue *opQueue;
@property (retain) NSURLResponse *response;

@end

@implementation CredentialTesterView

- (id)initWithGSSCredential:(gss_cred_id_t)credential   
{
    self = [super initWithNibName:@"CredentialTesterView" bundle:nil];
    if (self) {
        CFRetain(credential);
        self.credential = credential;
        
        gss_name_t gssName = GSSCredentialCopyName(credential);
        if (gssName) {
            self.name = (__bridge NSString *)GSSNameCreateDisplayString(gssName);
            CFRelease(gssName);
        } else {
            self.name = @"<credential have no name>";
        }
        
        OM_uint32 lifetime = GSSCredentialGetLifetime(credential);
        
        self.expire = [NSDate dateWithTimeIntervalSinceNow:lifetime];
        self.opQueue = [[NSOperationQueue alloc] init];
    }
    
    
    return self;
}

- (void)loadView {
    [super loadView];
    
    if ([self.name rangeOfString:@"@LKDC:"].location != NSNotFound) {
        [self.serverName setStringValue:@"host@WELLKNOWN:COM.APPLE.LKDC"];
    } else if ([self.name rangeOfString:@"@ADS.APPLE.COM"].location != NSNotFound) {
        [self.serverName setStringValue:@"HTTP@dc03.ads.apple.com"];
        [self.url setStringValue:@"http://dc03.ads.apple.com/negotiate/"];
    } else if ([self.name rangeOfString:@"@QAD.APPLE.COM"].location != NSNotFound) {
        [self.serverName setStringValue:@"HTTP@dc01qad.qad.apple.com"];
        [self.url setStringValue:@"http://dc01qad.qad.apple.com/negotiate/"];
    } else if ([self.name rangeOfString:@"@APPLECONNECT.APPLE.COM"].location != NSNotFound) {
        [self.url setStringValue:@"https://radar-webservices.apple.com/find/16000000"];
    }
}

- (IBAction)testISC:(id)sender {
    gss_ctx_id_t ctx = GSS_C_NO_CONTEXT;
    gss_name_t gssName = GSS_C_NO_NAME;
    gss_buffer_desc buffer;
    OM_uint32 maj_stat, min_stat;
    CFErrorRef error = NULL;
    
    NSString *serverName = [self.serverName stringValue];
    
    NSLog(@"isc: %@ to %@", self.credential, serverName);
    
    
    gssName = GSSCreateName((__bridge CFStringRef)serverName, GSS_C_NT_HOSTBASED_SERVICE, NULL);
    if (gssName == NULL) {
        NSLog(@"import_name %@", error);
        if (error) CFRelease(error);
        return;
    }
    
    maj_stat = gss_init_sec_context(&min_stat, self.credential,
                                    &ctx, gssName, GSS_KRB5_MECHANISM,
                                    GSS_C_REPLAY_FLAG|GSS_C_INTEG_FLAG, 0, GSS_C_NO_CHANNEL_BINDINGS,
                                    NULL, NULL, &buffer, NULL, NULL);
    if (maj_stat) {
        [self.iscStatus setStringValue:[NSString stringWithFormat:@"FAIL init_sec_context maj_stat: %d", (int)maj_stat]];
    } else {
        [self.iscStatus setStringValue:[NSString stringWithFormat:@"success: buffer of length: %d, success", (int)buffer.length]];
	}

    gss_delete_sec_context(&min_stat, &ctx, NULL);
    CFRelease(gssName);
    gss_release_buffer(&min_stat, &buffer);
}

- (IBAction)destroyCredential:(id)sender {
    OM_uint32 min_stat;
    gss_destroy_cred(&min_stat, &_credential);
    self.name = @"";
    self.expire = [NSDate distantPast];
    
    [self.tabViewItem.tabView removeTabViewItem:self.tabViewItem];
}

#pragma mark HTTP test


- (void)connection:(NSURLConnection *)connection didReceiveData:(NSData *)data
{
    [self.content appendData:data];
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
    self.response = response;
}

- (void)connectionDidFinishLoading:(NSURLConnection *)connection {
    __block NSString *status;
    NSLog(@"Finished...");

    NSLog(@"data:\n\n%@\n", [[NSString alloc] initWithData:self.content encoding:NSUTF8StringEncoding]);
    self.content = NULL;
    if ([self.response isKindOfClass:[NSHTTPURLResponse class]]) {
        NSHTTPURLResponse *urlResponse = (NSHTTPURLResponse *)self.response;
        if ([urlResponse statusCode] == 200) {
            status = [NSString stringWithFormat:@"success"];
        } else {
            status = [NSString stringWithFormat:@"failed with status: %d", (int)[urlResponse statusCode]];
        }
    } else {
        status = [NSString stringWithFormat:@"failed with unknown class returned %@", [self.response description]];
    }
    dispatch_async(dispatch_get_main_queue(), ^{
        [self.urlStatus setStringValue:status];
    });
}

- (void)connection:(NSURLConnection *)connection didFailWithError:(NSError *)error
{
	NSLog(@"didFailWithError");
    dispatch_async(dispatch_get_main_queue(), ^{
        [self.urlStatus setStringValue:@"failed"];
    });
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
    
    NSString *serverPrincipal = [NSString stringWithFormat:@"HTTP/%@", [protectionSpace host]];
    
    
	CFURLCredentialRef cfCredential = _CFURLCredentialCreateForKerberosTicket(NULL, (__bridge CFStringRef)self.name, (__bridge CFStringRef)serverPrincipal, NULL);
    if (cfCredential == NULL) {
        NSLog(@"failed to create credential");
        [[challenge sender] cancelAuthenticationChallenge:challenge];
        return;
    }
    
    NSURLCredential *credential = [[NSURLCredential alloc] _initWithCFURLCredential:cfCredential];
	
    [[challenge sender] useCredential:credential forAuthenticationChallenge:challenge];
    
    NSLog(@"using name %@ for credential: %@", self.name, credential);
    
    CFRelease(cfCredential);
}

- (IBAction)checkURL:(id)sender {
    NSURL *url = [NSURL URLWithString:[self.url stringValue]];
    
    NSURLRequest *request = [NSURLRequest requestWithURL:url];
    NSURLConnection *conn = [[NSURLConnection alloc] initWithRequest: request delegate: self startImmediately:NO];
    
    self.content = [NSMutableData data];
    [self.urlStatus setStringValue:@"performing test"];

    [conn setDelegateQueue:self.opQueue];
    [conn start];
}




@end
