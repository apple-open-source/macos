/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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

#import "Controller.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#import <SecurityInterface/SFCertificatePanel.h>
#import <SecurityInterface/SFCertificateTrustPanel.h>
#import <Security/SecTrust.h>
#import <Security/SecureTransport.h>
#include <Security/oidsalg.h>
#include <Security/SecPolicySearch.h>
#include <Security/SecPolicy.h>
#include <SystemConfiguration/SCValidation.h>
#include <AssertMacros.h>

#define kSSLClientPropTLSServerCertificateChain CFSTR("TLSServerCertificateChain") /* array[data] */
#define kSSLClientPropTLSTrustClientStatus	CFSTR("TLSTrustClientStatus") /* CFNumberRef of kCFNumberSInt32Type (errSSLxxxx) */
#define kSSLClientPropTLSServerHostName	CFSTR("TLSServerHostName") /* CFString */

#define LOCSTRING(x) [[NSBundle mainBundle] localizedStringForKey: x value: x table: nil]

/*
 * Function: add_certs_to_keychain
 * Purpose:
 *   Adds an array of certificates to the default keychain.
 */
static void
add_certs_to_keychain(CFArrayRef cert_list)
{
	CFIndex count;
	int i;

	count = CFArrayGetCount(cert_list);
	for (i = 0; i < count; ++i)
	{
		SecCertificateRef cert;

		cert = (SecCertificateRef)CFArrayGetValueAtIndex(cert_list, i);
		SecCertificateAddToKeychain(cert, NULL);
	}
	
	return;
}


/*
 * Function: SSLSecPolicyCopy
 * Purpose:
 *   Returns a copy of the SSL policy.
 */
static OSStatus
SSLSecPolicyCopy(SecPolicyRef *ret_policy)
{
	SecPolicyRef policy;
	SecPolicySearchRef policy_search;
	OSStatus status;

	*ret_policy = NULL;
	status = SecPolicySearchCreate(CSSM_CERT_X_509v3, &CSSMOID_APPLE_TP_SSL, NULL, &policy_search);
	require_noerr(status, SecPolicySearchCreate);

	status = SecPolicySearchCopyNext(policy_search, &policy);
	require_noerr(status, SecPolicySearchCopyNext);
	
	*ret_policy = policy;

SecPolicySearchCopyNext:

	CFRelease(policy_search);
	
SecPolicySearchCreate:

	return (status);
}


/*
 * Function: CFDataCreateSecCertificate
 * Purpose:
 *   Creates a SecCertificateRef from a CFDataRef.
 */
static SecCertificateRef
CFDataCreateSecCertificate(CFDataRef data_cf)
{
	SecCertificateRef cert;
	CSSM_DATA data;
	OSStatus status;

	cert = NULL;
	require(data_cf != NULL, bad_input);

	data.Length = CFDataGetLength(data_cf);
	data.Data = (uint8 *)CFDataGetBytePtr(data_cf);
	status = SecCertificateCreateFromData(&data, CSSM_CERT_X_509v3, CSSM_CERT_ENCODING_DER, &cert);
	check_noerr(status);
	
bad_input:

	return (cert);
}


/*
 * Function: CFDataArrayCreateSecCertificateArray
 * Purpose:
 *   Convert a CFArray[CFData] to CFArray[SecCertificate].
 */
static CFArrayRef
CFDataArrayCreateSecCertificateArray(CFArrayRef certs)
{
    CFMutableArrayRef array;
    CFIndex count;
    int i;
    
	count = CFArrayGetCount(certs);
    array = CFArrayCreateMutable(NULL, count, &kCFTypeArrayCallBacks);
	require(array != NULL, CFArrayCreateMutable);
	
	for (i = 0; i < count; ++i)
	{
		SecCertificateRef cert;
		CFDataRef data;

		data = isA_CFData((CFDataRef)CFArrayGetValueAtIndex(certs, i));
		require(data != NULL, isA_CFData);
		
		cert = CFDataCreateSecCertificate(data);
		require(cert != NULL, CFDataCreateSecCertificate);
		
		CFArrayAppendValue(array, cert);
		CFRelease(cert);
	}
	
	return (array);
	
	/* error cases handled here */

CFDataCreateSecCertificate:
isA_CFData:

    CFRelease(array);
	
CFArrayCreateMutable:

    return (NULL);
}


/*
 * Function: show_cert_trust_panel
 * Purpose:
 *   Displays the modal cert panel.
 */
static int
show_cert_trust_panel(CFArrayRef cert_list, SInt32 trust_status, CFStringRef host_name)
{
	int exit_code;
	SFCertificateTrustPanel *panel;
	SecPolicyRef policy;
	int ret_val;
	OSStatus status;
	NSString *text;
	SecTrustRef trust;
	SecTrustResultType trust_result;

	status = SSLSecPolicyCopy(&policy);
	require_noerr_action(status, SSLSecPolicyCopy, exit_code = 2);

	status = SecTrustCreateWithCertificates(cert_list, policy, &trust);
	require_noerr_action(status, SecTrustCreateWithCertificates, exit_code = 2);

	(void)SecTrustEvaluate(trust, &trust_result);
	panel = [[SFCertificateTrustPanel alloc] init];
	
	switch (trust_status)
	{
		case errSSLCertExpired:
			text = @"MESSAGE_CERT_EXPIRED";
			break;
			
		case errSSLUnknownRootCert:
		case errSSLNoRootCert:
			text = @"MESSAGE_CERT_INVALID";
			break;
			
		case errSSLCertNotYetValid:
			text = @"MESSAGE_CERT_NOT_YET_VALID";
			break;
			
		case errSSLBadCert:
		case errSSLXCertChainInvalid:
		case errSSLHostNameMismatch:
		default:
			text = @"MESSAGE_CERT_UNKNOWN_AUTHORITY";
			break;
	}
	text = LOCSTRING(text);
	
	/* insert host_name into the text string */
	text = [NSString stringWithFormat: text, host_name];
	
	if ([panel respondsToSelector:@selector(setPolicies:)])
	{
		[panel setPolicies:(id)policy];
	}
	if ([panel respondsToSelector:@selector(setDefaultButtonTitle:)])
	{
		[panel setDefaultButtonTitle:LOCSTRING(@"MESSAGE_CERT_CONTINUE")];
	}
	if ([panel respondsToSelector:@selector(setAlternateButtonTitle:)])
	{
		[panel setAlternateButtonTitle:LOCSTRING(@"MESSAGE_CERT_CANCEL")];
	}
	if ([panel respondsToSelector:@selector(setShowsHelp:)])
	{
		[panel setShowsHelp:YES];
	}
	
	[[NSApplication sharedApplication] activateIgnoringOtherApps:YES];
	
	ret_val = (int)[panel runModalForTrust:trust message:text];
	
	switch (ret_val)
	{
		case NSOKButton:
			exit_code = 0;
			add_certs_to_keychain(cert_list);
			break;
			
		case NSCancelButton:
		default:
			exit_code = 1;
			break;
	}
	
	[panel release];

SecTrustCreateWithCertificates:

	CFRelease(policy);

SSLSecPolicyCopy:

	exit(exit_code);
}


extern CFDictionaryRef the_dict;


@implementation Controller
- (void)awakeFromNib
{
	CFArrayRef cert_data_list;
	CFArrayRef cert_list;
	CFStringRef host_name;
	CFNumberRef trust_status_cf;
	SInt32 trust_status;
	int error;

	cert_data_list = CFDictionaryGetValue(the_dict, kSSLClientPropTLSServerCertificateChain);
	require_action(isA_CFArray(cert_data_list) != NULL, isA_CFArray, error = 2);
	
	cert_list = CFDataArrayCreateSecCertificateArray(cert_data_list);
	require_action(cert_list != NULL, CFDataArrayCreateSecCertificateArray, error = 2);
	
	trust_status = errSSLUnknownRootCert;
	trust_status_cf = CFDictionaryGetValue(the_dict, kSSLClientPropTLSTrustClientStatus);
	if (isA_CFNumber(trust_status_cf) != NULL)
	{
		CFNumberGetValue(trust_status_cf, kCFNumberSInt32Type, &trust_status);
	}
	
	host_name = CFDictionaryGetValue(the_dict, kSSLClientPropTLSServerHostName);
	if ( host_name == NULL )
	{
		/* this should not happen, but just in case */
		host_name = CFSTR("unknown");
	}
	
	error = show_cert_trust_panel(cert_list, trust_status, host_name);
	require_noerr(error, show_cert_trust_panel);
	
	return;

	/* error cases handled here */
	
show_cert_trust_panel:
CFDataArrayCreateSecCertificateArray:
isA_CFArray:

	exit(error);
}
@end
