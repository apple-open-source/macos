/*
 * Copyright (c) 2006 Apple Computer, Inc. All Rights Reserved.
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

#import "SFKerberosCredentialSelector.h"
#import "PrompterController.h"
#import <Kerberos/pkinit_cert_store.h>  

@implementation SFKerberosCredentialSelector

NSString *gAppId = @"krba";

#define kMaxNumberOfIdentityRetries	3

#define CredentialSelectorString(key) NSLocalizedStringFromTable (key, @"AuthenticationController", NULL)

BOOL gKerbAuthLogging = NO;

-(void)dealloc
{
	if ( _acquiredCacheName )
		[_acquiredCacheName release];
	if ( _controller )
		[_controller release];
	[super dealloc];
}


static char *kcItemPrintableName(
    SecKeychainItemRef certRef)
{
    char *crtn = NULL;

    /* just search for the one attr we want */
	#if USE_KEY_NAME
    UInt32 tag = kSecKeyPrintName;
	#else
    UInt32 tag = kSecLabelItemAttr;
	#endif
    SecKeychainAttributeInfo attrInfo;
    attrInfo.count = 1;
    attrInfo.tag = &tag;
    attrInfo.format = NULL;
    SecKeychainAttributeList *attrList = NULL;
    SecKeychainAttribute *attr = NULL;
    
    OSStatus ortn = SecKeychainItemCopyAttributesAndData(
	(SecKeychainItemRef)certRef, 
	&attrInfo,
	NULL,			// itemClass
	&attrList, 
	NULL,			// length - don't need the data
	NULL);			// outData
    if(ortn) {
		cssmPerror("SecKeychainItemCopyAttributesAndData", ortn);
		/* may want to be a bit more robust here, but this should
		 * never happen */
		return strdup("Unnamed KeychainItem");
    }
    /* subsequent errors to errOut: */
    
    if((attrList == NULL) || (attrList->count != 1)) {
		printf("***Unexpected result fetching label attr\n");
		crtn = strdup("Unnamed KeychainItem");
		goto errOut;
    }
    /* We're assuming 8-bit ASCII attribute data here... */
    attr = attrList->attr;
    crtn = (char *)malloc(attr->length + 1);
    memmove(crtn, attr->data, attr->length);
    crtn[attr->length] = '\0';
    
errOut:
    SecKeychainItemFreeAttributesAndData(attrList, NULL);
    return crtn;
}

-(void)createKeychainItemWithPrincipal:(KerberosPrincipal*)principal password:(NSString*)password
{
	SecKeychainItemRef itemRef = NULL;
	NSString* uName = NULL;
	NSString* theRealm = NULL;
	NSString *principalString = [principal displayString];
	NSRange separator = [principalString rangeOfString: @"@" options: (NSLiteralSearch | NSBackwardsSearch)];
	uName = [principalString substringToIndex: separator.location];
	theRealm = [principalString substringFromIndex: separator.location + separator.length];
	if ( theRealm && uName )
	{
		OSStatus result = noErr;
		result = SecKeychainAddGenericPassword(nil, [theRealm length], [theRealm UTF8String],
					[uName length], [uName UTF8String],
					[password length], [password UTF8String], &itemRef);
		KerbCredSelLog(@"createKeychainItemWithPrincipal - SecKeychainAddInternetPassword returned %d", result);
		//
		// Make the label attribute more descriptive than the default.
		//
		if ( result == noErr && itemRef )
		{
			[self _setPrintNameWithUserName:uName serverName:theRealm item:itemRef];
			CFRelease(itemRef);
		}
	}
}

-(void)modifyKeychainItem:(SecKeychainItemRef)itemRef withPassword:(NSString*)password
{
	OSStatus result = noErr;
	if ( [_controller foundKCItem] != NULL )
	{
		result = SecKeychainItemModifyAttributesAndData(itemRef, nil, [password length], [password UTF8String]);
		KerbCredSelLog(@"modifyKeychainItem - SecKeychainItemModifyAttributesAndData returned %d", result);
	}
	else
		KerbCredSelLog(@"modifyKeychainItem - couldn't modify kc item (not set yet)");
}

-(KLStatus)getTicketsWithUserName:(NSString *)name realm:(NSString *)realm serviceNameString:(NSString *)serviceNameString password:(NSString *)password principal:(KerberosPrincipal*)principal
{
    KLStatus err = klNoErr;
    KLLoginOptions loginOptions = NULL;
    
	KerberosPreferences *prefs = [KerberosPreferences sharedPreferences]; // Use the user prefs to determine login options when authenticating with the keychain.

	NSString *principalString = [NSString stringWithFormat: @"%@@%@", name, realm];
	KerbCredSelLog(@"credsel getTicketsWithUserName - principalString = %@",principalString);

    // Get Tickets:
    if (principal == NULL) { err = klBadPrincipalErr; }
    
    if (err == klNoErr) {
        err = KLCreateLoginOptions (&loginOptions);
    }
    
    if (err == klNoErr) {
        err = KLLoginOptionsSetTicketLifetime (loginOptions, [prefs defaultLifetime]);
    }
    
    if (err == klNoErr) {
        if ([prefs defaultRenewable]) {
            err = KLLoginOptionsSetRenewableLifetime (loginOptions, [prefs defaultRenewableLifetime]);
        } else {
            err = KLLoginOptionsSetRenewableLifetime (loginOptions, 0L);
        }
    }
    
    if (err == klNoErr) {
        err = KLLoginOptionsSetTicketStartTime (loginOptions, 0);
    }
    
    if (err == klNoErr) {
        err = KLLoginOptionsSetServiceName (loginOptions, ((serviceNameString == NULL) ? 
                                                           NULL : [serviceNameString UTF8String]));
    }
    
    if (err == klNoErr) {
        err = KLLoginOptionsSetForwardable (loginOptions, [prefs defaultForwardable]);
    }
    
    if (err == klNoErr) {
        err = KLLoginOptionsSetProxiable (loginOptions, [prefs defaultProxiable]);
    }
    
    if (err == klNoErr) {
        err = KLLoginOptionsSetAddressless (loginOptions, [prefs defaultAddressless]);
    }
    
	if (err == klNoErr) {
		err = [principal getTicketsWithPassword:password loginOptions:loginOptions cacheName:_acquiredCacheName];
    }
    
    if (loginOptions != NULL) { KLDisposeLoginOptions (loginOptions); }
    
	KerbCredSelLog(@"credsel getTicketsWithUserName - returning err=%d",err);
    return err;
}

-(BOOL)doesValidKeychainItemExistForPrincipal:(KerberosPrincipal*)principal serviceName:(NSString*)serviceName
{
    KLStatus err = klNoErr;
	NSString *keychainPassword = NULL;
	NSString *principalString = [principal displayString];
	NSRange separator = [principalString rangeOfString: @"@" options: (NSLiteralSearch | NSBackwardsSearch)];
	NSString* uName = NULL;
	NSString* uRealm = NULL;
	uName = [principalString substringToIndex: separator.location];
	uRealm = [principalString substringFromIndex: separator.location + separator.length];
	
	KerbCredSelLog(@"doesValidKeychainItemExistForPrincipal - user=%@, realm=%@",uName, uRealm);
	if ( [_controller findKeychainItemWithUser:uName
				serverName:uRealm
				password:&keychainPassword] == YES )
	{
		// Do Kerb auth with the keychain password...
		//
		err = [self getTicketsWithUserName:uName realm:uRealm serviceNameString:serviceName password:keychainPassword principal:principal];
		if ( err != klNoErr )
			NSLog(@"doesValidKeychainItemExistForPrincipal - keychain item failed to authenticate...");
	}
	else
		err = errSecItemNotFound;

	if ( err != klNoErr )
		return NO;
	return YES;
}

-(KLStatus)selectCredentialWithPrincipal:(KerberosPrincipal*)principal 
										serviceName:(NSString*)serviceName 
										applicationTask:(task_t)applicationTask 
										applicationPath:(NSString*)applicationPath 
                                        inLifetime:(KLIPCTime)inLifetime
                                        inRenewableLifetime:(KLIPCTime)inRenewableLifetime
										inFlags:(KLIPCFlags)inFlags
                                        inStartTime:(KLIPCTime)inStartTime
                                        inForwardable:(KLIPCBoolean)inForwardable
										inProxiable:(KLIPCBoolean)inProxiable
                                        inAddressless:(KLIPCBoolean)inAddressless
										isAutoPopup:(boolean_t)isAutoPopup
										inApplicationName:(NSString*)inApplicationName
										inApplicationIcon:(NSImage*)inApplicationIcon
{
	if ( [[[NSUserDefaults standardUserDefaults] persistentDomainForName:@"com.apple.kerberosAuthLogging"] objectForKey:@"KerberosAuthLogging"] )
		gKerbAuthLogging = YES;
	KerbCredSelLog(@"selectCredentialWithPrincipal; principal = %@",principal);

	_fromKeychain = NO;
	
	if ( _acquiredCacheName )
		[_acquiredCacheName release];
	_acquiredCacheName = [[NSMutableString alloc] init];
	if ( _acquiredCacheName == NULL ) {
		return klMemFullErr;
	}        
	
	if ( _controller ) 
		[_controller release];

	if ( principal != NULL /*%%% and [principal displayStringSimple] returns something useful*/ )
	{
		_controller = [[AuthenticationControllerSimple alloc] init];
		if ( !_controller )
			return klMemFullErr;
	}
	else
	{
		_controller = [[AuthenticationController alloc] init];
		if ( !_controller )
			return klMemFullErr;
	}
		
	NSString *principalString = [principal displayString];	// might be NULL.

	KLStatus result = noErr;
	BOOL authOK = NO;
	krb5_pkinit_signing_cert_t preferredPKInitIdentity = NULL;
	krb5_error_code krtn = noErr;
	
	if ( principal != NULL )	// principal passed in means go through the credendial picker (show UserName+Pwd UI otherwise)
	{
		KerbCredSelLog(@"selectCredentialWithPrincipal - looking for client_cert for %s",[principalString UTF8String]);
		//
		// Save the krb5 client identity.
		//
		krtn = krb5_pkinit_get_client_cert([principalString UTF8String], &preferredPKInitIdentity);
		if ( preferredPKInitIdentity == NULL )
		{
			KerbCredSelLog(@"selectCredentialWithPrincipal - krb5_pkinit_get_client_cert returned nil");
		}
		else
		{
			if ( gKerbAuthLogging )
			{
				SecKeychainItemRef testCert = NULL;
				SecIdentityCopyCertificate(preferredPKInitIdentity, (SecCertificateRef *)&testCert);
				KerbCredSelLog(@"selectCredentialWithPrincipal - we have a krb5_pkinit_get_client_cert configured (%s).",kcItemPrintableName(testCert));
				if ( testCert )
					CFRelease(testCert);
			}
			// Try authenticating with the krb5 client cert (with no password)
			//
			[_controller setCallerProvidedPrincipal: principal];
			result = [_controller getTicketsWithPassword:NULL]; 
			KerbCredSelLog(@"selectCredentialWithPrincipal - [_controller getTicketsWithPassword] returned %d", result);
			if ( result == klNoErr )
			{
				authOK = YES;
			}
		}
	}
	//
	// Error authenticating? We may have gone through the identity selector or used the krb5 client identity
	//
	if ( !authOK )	// On error, restore krb5_pkinit_signing_cert_t for this principal.
	{
		// Now, look in the user's keychain.
		//
		BOOL doesValidKeychainItemExist = NO;
		if ( principal != NULL )	// If the principal is passed in, use that to search for the keychain item (auth simple panel)
			doesValidKeychainItemExist = [self doesValidKeychainItemExistForPrincipal:principal serviceName:serviceName];
		if ( doesValidKeychainItemExist == NO )
		{
			[_controller setDoesMinimize: isAutoPopup];
			[_controller setCallerNameString: inApplicationName];
			[_controller setCallerIcon: inApplicationIcon];
			[_controller setCallerProvidedPrincipal: principal];
			[_controller setServiceName: serviceName];
			[_controller setStartTime: inStartTime];
			if (inFlags & KRB5_GET_INIT_CREDS_OPT_TKT_LIFE)     { [_controller setLifetime:    inLifetime]; }
			if (inFlags & KRB5_GET_INIT_CREDS_OPT_FORWARDABLE)  { [_controller setForwardable: inForwardable]; }
			if (inFlags & KRB5_GET_INIT_CREDS_OPT_PROXIABLE)    { [_controller setProxiable:   inProxiable]; }
			if (inFlags & KRB5_GET_INIT_CREDS_OPT_ADDRESS_LIST) { [_controller setAddressless: inAddressless]; }
			if (inFlags & KRB5_GET_INIT_CREDS_OPT_RENEW_LIFE) { 
				[_controller setRenewable: (inRenewableLifetime > 0)]; 
				[_controller setRenewableLifetime: inRenewableLifetime];
			}
			
			if (!isAutoPopup) { 
				[NSApp activateIgnoringOtherApps: YES]; 
			}
			result = [_controller runWindow];
			KerbCredSelLog(@"selectCredentialWithPrincipal - [_controller runWindow] returned %d",result);
			//
			// On success, store it in the keychain or modify what's there.
			//
			if ( result == noErr )
			{
				KerbCredSelLog(@"selectCredentialWithPrincipal - User authenticated in the UI!");
				if ( [_controller rememberPasswordInKeychain] == YES ) 
				{
					if ( [_controller keychainPasswordFromKeychain] != NULL && [[_controller keychainPasswordFromKeychain] isEqualToString:[_controller enteredPassword]] == NO )
						[self modifyKeychainItem:[_controller foundKCItem] withPassword:[_controller enteredPassword]];
					else if ( [_controller keychainPasswordFromKeychain] == NULL )
						[self createKeychainItemWithPrincipal:[_controller acquiredPrincipal] password:[_controller enteredPassword]];
				}
			}
			else if ( result != klUserCanceledErr )
				NSLog(@"selectCredentialWithPrincipal - User failed to authenticate in UI");
		}
		else
		{
			KerbCredSelLog(@"selectCredentialWithPrincipal - item found in kc and it authenticated!");
			_fromKeychain = YES;
			[self setAcquiredPrincipal:principal];
		}
	}
	
	if ( preferredPKInitIdentity )
		CFRelease(preferredPKInitIdentity);
		
	return result;
}

-(NSString*)acquiredCacheName
{
	if ( _fromKeychain )
		return _acquiredCacheName;
	return [_controller acquiredCacheName];
}

-(void)setAcquiredPrincipal:(KerberosPrincipal*)principal
{
	if ( principal )
		[principal retain];
	if ( _acquiredPrincipal != NULL )
		[_acquiredPrincipal release];
	_acquiredPrincipal = principal;
}

-(KerberosPrincipal*)acquiredPrincipal
{
	if ( _fromKeychain )
		return _acquiredPrincipal;

	// get the acquired principal from the UI (the user might have changed principals)
	return [_controller acquiredPrincipal];
}

-(void)_setPrintNameWithUserName:(NSString *)userName serverName:(NSString*)serverName item:(SecKeychainItemRef)itemRef
{
	OSStatus result = noErr;
	//
	// Consruct the information needed to read the attribute information for the label.
	//
	SecKeychainAttributeInfo attrInfo;
	attrInfo.count = 1;
	UInt32 tag = 7;
	attrInfo.tag = &tag;
	UInt32 format = CSSM_DB_ATTRIBUTE_FORMAT_STRING;
	attrInfo.format = &format;
	//
	// Copy the attribute information for the label first of the private key (we need to know the returned tag for this attr).
	//
	SecKeychainAttributeList* copiedAttrs = NULL;
	result = SecKeychainItemCopyAttributesAndData(itemRef, &attrInfo, NULL, &copiedAttrs, 0, NULL);
	if ( result == noErr )
	{
			NSString *newlabel = [NSString stringWithFormat:@"%@ (%@)", serverName, userName];
			SecKeychainAttributeList attrList;
			attrList.count = 1;
			SecKeychainAttribute attr;
			attrList.attr = &attr;
			//
			// Copy the tag they gave us.
			//
			attr.tag = copiedAttrs->attr->tag;
			//
			// Construct our own attribute with our new data.
			//
			attr.length = [newlabel length];
			attr.data = (void*)[newlabel UTF8String];
			//
			// And modify.
			//
			result = SecKeychainItemModifyAttributesAndData(itemRef, &attrList, 0, NULL);
			if ( result != noErr )
				NSLog(@"_setPrintNameWithUserName - SecKeychainItemModifyAttributesAndData returned %d",result);
	}
	else
	{
		NSLog(@"_setPrintNameWithUserName - SecKeychainItemCopyAttributesAndData returned %d",result);
	}
	if ( copiedAttrs )
		SecKeychainItemFreeAttributesAndData(copiedAttrs, NULL);
}

@end
