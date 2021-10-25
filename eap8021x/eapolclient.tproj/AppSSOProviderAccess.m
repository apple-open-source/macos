/*
 * Copyright (c) 2021 Apple Inc. All rights reserved.
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

#if TARGET_OS_IOS

#import <AppSSO/SOAuthorization.h>
#import "AppSSOProviderAccess.h"
#import "myCFUtil.h"
#import "EAPLog.h"

/* AppSSO auth request headers */
#define EAP_APPSSO_AUTH_REQ_KEY_TYPE      			@"X-SSO-Net-Authorization-Type"
#define EAP_APPSSO_AUTH_REQ_KEY_CRED      			@"X-SSO-Net-Authorization"
#define EAP_APPSSO_AUTH_REQ_KEY_RESOURCE       			@"X-SSO-Net-Resource"

/* AppSSO auth response headers */
#define EAP_APPSSO_AUTH_RESP_KEY_CREDS_TYPE			@"X-SSO-Net-Credentials"
#define EAP_APPSSO_AUTH_RESP_KEY_CREDS_TYPE_KEY_REF		@"IdentityPersistentReference"

#pragma mark - AppSSOProviderCredentialSource

@interface AppSSOProviderCredentialSource : NSObject <SOAuthorizationDelegate>
@property (nonatomic, retain) SOAuthorization * appSSOAuthorization;
@property (nonatomic, retain) dispatch_queue_t queue;
@property (nonatomic, retain) NSString * providerURL;
@property (nonatomic, retain) NSString * ssid;
@property AppSSOProviderAccessCredentialsCallback callback;
@property void * callbackContext;
@end

@implementation AppSSOProviderCredentialSource

- (nullable instancetype)initWithProviderURLString:(NSString *)url
						ssid:(NSString *)ssid
						queue:(dispatch_queue_t)queue
						callback:(AppSSOProviderAccessCredentialsCallback)callback
						context:(void *)context
{
    self = [super init];
    if (self) {
	_providerURL = url;
	_queue = queue;
	_callback = callback;
	_callbackContext = context;
	_ssid = ssid;
	_appSSOAuthorization = [[SOAuthorization alloc] init];
	_appSSOAuthorization.delegate = self;
	_appSSOAuthorization.delegateDispatchQueue = queue;
    }
    return self;
}

- (BOOL)canPerformAuthorization
{
    BOOL canAuthorize = [SOAuthorization canPerformAuthorizationWithURL:[NSURL URLWithString:_providerURL] responseCode:0 useInternalExtensions:NO];
    if (!canAuthorize) {
	EAPLOG_FL(LOG_ERR, "%@ AppSSO provider [%@] cannot perform the authorization", self, _providerURL);
	__block CFErrorRef error = CFErrorCreate(kCFAllocatorDefault, CFSTR("EAPAppSSOErrorDomain"), 2, NULL);
	dispatch_async(self.queue, ^{
	    if (self.callback) {
		self.callback(self.callbackContext, error, NULL);
	    }
	});
	return NO;
    }
    return YES;
}

- (void)startAuthorization
{
    if ([self canPerformAuthorization]) {
	EAPLOG_FL(LOG_INFO, "%@ starting AppSSO authorization", self);
	NSDictionary 	*httpHeaders = @{EAP_APPSSO_AUTH_REQ_KEY_TYPE: @"802.1x",
					 EAP_APPSSO_AUTH_REQ_KEY_CRED: @"EAP",
					 EAP_APPSSO_AUTH_REQ_KEY_RESOURCE: _ssid};
	[_appSSOAuthorization beginAuthorizationWithOperation:SOAuthorizationOperationFetchNetworkCredentials
							url:[NSURL URLWithString:_providerURL]
							httpHeaders:httpHeaders
							httpBody:[[NSData alloc] init]];
    }
}

- (CredentialsAppSSOProviderResponseRef)createResponseWithIdentityPref:(NSData *)httpBody
{
    CredentialsAppSSOProviderResponseRef response = NULL;

    response = (CredentialsAppSSOProviderResponseRef)malloc(sizeof(*response));
    bzero(response, sizeof(*response));
    response->authorized = true;
    response->remember_information = true;
    response->identity_persistent_ref = (__bridge_retained CFDataRef)httpBody;
    return response;
}

- (void)invalidate
{
    EAPLOG_FL(LOG_INFO, "%@ invalidate", self);
    if (_appSSOAuthorization) {
	_appSSOAuthorization.delegate = nil;
	[_appSSOAuthorization cancelAuthorization];
	_appSSOAuthorization = nil;
    }
}

#pragma mark - SOAuthorizationDelegate

- (void)authorization:(SOAuthorization *)authorization didCompleteWithHTTPResponse:(NSHTTPURLResponse *)httpResponse httpBody:(NSData *)httpBody
{
    BOOL 	failed = YES;
    NSString 	*credsHeaderField = nil;

    EAPLOG_FL(LOG_INFO, "%@ didCompleteWithHTTPResponse", self);
    if (httpResponse == nil) {
	goto done;
    }
    credsHeaderField = [httpResponse valueForHTTPHeaderField:EAP_APPSSO_AUTH_RESP_KEY_CREDS_TYPE];
    if ([credsHeaderField isEqualToString:EAP_APPSSO_AUTH_RESP_KEY_CREDS_TYPE_KEY_REF] && httpBody != nil) {
	__block CredentialsAppSSOProviderResponseRef response = [self createResponseWithIdentityPref:httpBody];
	dispatch_async(self.queue, ^{
	    if (self.callback) {
		self.callback(self.callbackContext, NULL, response);
	    }
	});
	failed = NO;
    }

done:
    if (failed) {
	EAPLOG_FL(LOG_ERR, "%@ received invalid response from AppSSO provider [%@]", self, _providerURL);
	__block CFErrorRef error = CFErrorCreate(kCFAllocatorDefault, CFSTR("EAPAppSSOErrorDomain"), 1, NULL);
	dispatch_async(self.queue, ^{
	    if (self.callback) {
		self.callback(self.callbackContext, error, NULL);
	    }
	});
    }
}

- (void)authorization:(SOAuthorization *)authorization didCompleteWithError:(NSError *)error
{
    EAPLOG_FL(LOG_ERR, "%@ didCompleteWithError error: %@", self, error);
    dispatch_async(self.queue, ^{
	if (self.callback) {
	    self.callback(self.callbackContext, (__bridge CFErrorRef)error, NULL);
	}
    });
}

@end

#pragma mark - C interface

CFTypeRef
AppSSOProviderAccessFetchCredentials(CFStringRef url, CFStringRef ssid, dispatch_queue_t queue, AppSSOProviderAccessCredentialsCallback callback, void *context)
{
    @autoreleasepool {
	AppSSOProviderCredentialSource *source = [[AppSSOProviderCredentialSource alloc] initWithProviderURLString:(__bridge NSString *)url
													    ssid:(__bridge NSString *)ssid
													    queue:queue callback:callback context:context];
	if (source) {
	    [source startAuthorization];
	    return CFBridgingRetain(source);
	} else {
	    EAPLOG_FL(LOG_ERR, "failed to initialize AppSSOProviderCredentialSource");
	    return NULL;
	}
    }
}

void
AppSSOProviderAccessInvalidate(CFTypeRef source)
{
    @autoreleasepool {
	AppSSOProviderCredentialSource *appssoSource = (__bridge_transfer AppSSOProviderCredentialSource *)source;
	[appssoSource invalidate];
    }
}

#endif /* TARGET_OS_IOS */
