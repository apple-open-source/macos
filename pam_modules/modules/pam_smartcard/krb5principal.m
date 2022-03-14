//
//  krb5principal.m
//  SecurityAgent
//
//  Created by Conrad Sauerwald on Mon Mar 24 2003.
//  Copyright (c) 2003,2005 Apple Computer, Inc. All Rights Reserved.
//

/*
Some documentation on the Kerberos Auth authority:

Kerberos Authentication Authority
	Version = 1.0.0
	AuthorityTag = 'Kerberosv5' (kDSTagAuthAuthorityKerberosv5)
	AuthorityData = [128-bit uid]; [user principal (with realm)]; realm; [realm public key]


The optional 128-bit UID is encoded in the same fashion as the ID in the Password Server Auth Authority.
The optional user principal is the user principal for this user within the Kerberos system.
The required realm name is the name of the Kerberos realm to which the user belongs
The optional realm public key may be used to authenticate the KDC in a future release.

if user principal is not present, the short name of the user record is taken to be the username for the principal. This allows a fixed auth authority to be set up and applied to all user records in a database. If the UID is present, it will be used before the short name is used.

The authentication authority is parsed to find the user principal during login, which is handed to the Kerberos framework for the actual authentication.

For example:
1.0;Kerberosv5;35382900696e4469725265663a702831;kerbdude@APPLE.COM;APPLE.COM;4346537472696e673a28312c33332900
Would yield a user principal of kerbdude@APPLE.COM

1.0;Kerberosv5;35382900696e4469725265663a702831;;APPLE.COM;4346537472696e673a28312c33332900
Would yield a user principal of 35382900696e4469725265663a702831@APPLE.COM

if the user record is named "kirby"
;Kerberosv5;;;APPLE.COM;4346537472696e673a28312c33332900
Would yield a user principal of kirby@APPLE.COM

Here's an example of the local kdc authentication authority.
;Kerberosv5;;local@LKDC:SHA1.3D1A965A4624679D7E25C300A2ABAF13C1E80447;LKDC:SHA1.3D1A965A4624679D7E25C300A2ABAF13C1E80447;
*/

#import <CoreFoundation/CoreFoundation.h>
#import <DirectoryService/DirServicesConst.h>


CF_RETURNS_RETAINED
CFStringRef	GetPrincipalFromUser(CFDictionaryRef inUserRecord);
OSStatus	ParseKerbAuthAuthority(CFStringRef inAuthority, CFArrayRef *outTokens);

/*
    The Kerberos auth authority is a CFString of the form:
    1.0;Kerbv5;[128-bit uid]; [user principal (with realm)]; realm; [realm public key];

*/
OSStatus ParseKerbAuthAuthority(CFStringRef inAuthority, CFArrayRef *outTokens)
{
    OSStatus			theError = -1;
    CFComparisonResult	result;
    CFIndex				max;
    CFStringRef			tmpString = NULL;

	// No separators in the string returns array with that string; string == sep returns two empty strings
    *outTokens = CFStringCreateArrayBySeparatingStrings(NULL, inAuthority, CFSTR(";"));
    if(*outTokens == NULL) {
        theError = -1;
    } else {
        while (true) {
            theError = 0;
			// check that the second token "Kerbv5" and that we have enough elements
			max = CFArrayGetCount(*outTokens);
			if (max < 6) { // The 1.0 version of the auth authority has 6 elements
				theError = -1;
				break;
			}

			result = CFStringCompare(CFSTR(kDSTagAuthAuthorityKerberosv5), (CFStringRef) CFArrayGetValueAtIndex(*outTokens, 1), kCFCompareCaseInsensitive);
			if (result != kCFCompareEqualTo)
            {
				theError = -1;
            }

			tmpString = (CFStringRef) CFArrayGetValueAtIndex(*outTokens, 4); // should be the realm
			if (CFStringGetLength(tmpString) == 0)	// realm is required
            {
				theError = -1;
            }

			break;
        }

		if (theError != noErr) {
			// Clean up
            CFRelease(*outTokens);
            *outTokens = NULL;
        }
    }

    return theError;
}



// Get the first auth-authority value that mentions Kerberos and use that to build the user principal.

CF_RETURNS_RETAINED
CFStringRef	GetPrincipalFromUser(CFDictionaryRef inUserRecord)
{
	CFArrayRef	tokens = NULL;
	CFArrayRef	tmpArray = NULL;
	CFTypeRef	tmpTypeRef = NULL;
	CFStringRef	authAuthority = NULL;
	CFStringRef	tmpString = NULL;
	CFStringRef	realmString = NULL;
	CFStringRef	principal = NULL;
	CFIndex		index;

	// extract the auth-authority from the original attribute first, in case it is cached user
	// @notes:  We could check the current auth authority to see if it is a cached user, but that would be
	//          lots of extra code.  We could check the node to ensure it is local node only, but since
	//          this isn't used for authentications, it is only for getting the TGT, we really don't care
	CFStringRef originalAuth = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@original_authentication_authority"), CFSTR(kDSNativeAttrTypePrefix));
	tmpTypeRef = CFDictionaryGetValue(inUserRecord, originalAuth);
	CFRelease( originalAuth );
	if (tmpTypeRef == NULL)	{
		// if not a cached user with original auth authority, then let's check the real thing
		tmpTypeRef = CFDictionaryGetValue(inUserRecord, CFSTR(kDSNAttrAuthenticationAuthority));
        if(tmpTypeRef == NULL) {
			return NULL;
        }
	}
		
	if (CFArrayGetTypeID() == CFGetTypeID(tmpTypeRef)) {
		// tmpTypeRef is an array of auth-authority strings
		CFArrayRef authArray = (CFArrayRef)tmpTypeRef;
		CFIndex authArrayCount = CFArrayGetCount(authArray);
		for (index = 0; index < authArrayCount; index++) {
			authAuthority = CFArrayGetValueAtIndex(authArray, index);
			if ((authAuthority != NULL) && (CFStringGetTypeID() == CFGetTypeID(authAuthority)))	{
				if (ParseKerbAuthAuthority(authAuthority, &tokens) == noErr) {
					break;
				}
			}
		}
	} else if (CFStringGetTypeID() == CFGetTypeID(tmpTypeRef)) {
		ParseKerbAuthAuthority((CFStringRef)tmpTypeRef, &tokens);
	}

    if (tokens == NULL)	// we didn't find a Kerberos auth authority
        return NULL;

    // stupid hack for <rdar://problem/5236943> Security::SecurityServer::ClientSession::authCopyRights blocks for ~20 seconds
	// we're going to put a kdc in the auth authority and then not run it because we're idiots
    // when the local kdc starts running per default this hack must be removed.
	realmString = (CFStringRef) CFArrayGetValueAtIndex(tokens, 4);
	if (realmString && CFStringHasPrefix(realmString, CFSTR("LKDC:"))) {
		CFRelease(tokens);
		return NULL;
	}
	// end of stupid hack

    // do we have a principal name?
	tmpString = (CFStringRef) CFArrayGetValueAtIndex(tokens, 3);
	if (CFStringGetLength(tmpString) > 3) {	// smallest principal name is a@a
		CFRetain(tmpString);
		CFRelease(tokens);
		return tmpString;
	} else {
		// No principal name in auth-authority, is there an id?
		tmpString = (CFStringRef) CFArrayGetValueAtIndex(tokens, 2);
		if (CFStringGetLength(tmpString) > 0) {
			// Non-empty is good enough for an id
			realmString = (CFStringRef) CFArrayGetValueAtIndex(tokens, 4);
			principal = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@@%@"), tmpString, realmString);
			CFRelease(tokens);
			return principal;
		}
	 }
    // no id, get the user's short name
    tmpArray = (CFArrayRef) CFDictionaryGetValue(inUserRecord, CFSTR(kDSNAttrRecordName));
    tmpString = CFArrayGetValueAtIndex(tmpArray, 0);

    realmString = (CFStringRef) CFArrayGetValueAtIndex(tokens, 4);
    principal = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@@%@"), tmpString, realmString);
    CFRelease(tokens);
    return principal;
}


