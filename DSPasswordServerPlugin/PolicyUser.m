/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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

#import "PolicyUser.h"

// ----------------------------------------------------------------------------------------
#pragma mark -
#pragma mark C API
#pragma mark -
// ----------------------------------------------------------------------------------------

// ----------------------------------------------------------------------------------------
//  ConvertXMLPolicyToSpaceDelimited
//
//  Returns: -1 fail, or 0 success. 
//  <outPolicyStr> is malloc'd memory, caller must free.
// ----------------------------------------------------------------------------------------

int ConvertXMLPolicyToSpaceDelimited( const char *inXMLDataStr, char **outPolicyStr )
{
	PolicyUser *userPolicyObj = nil;
	CFStringRef userPolicyString = NULL;
	CFIndex needSize = 0;
	int result = 0;

	if ( inXMLDataStr == NULL || outPolicyStr == NULL )
		return -1;

	*outPolicyStr = NULL;

	userPolicyObj = [[PolicyUser alloc] initWithUTF8String:inXMLDataStr];
	if ( userPolicyObj != nil )
	{
		userPolicyString = [userPolicyObj policyAsSpaceDelimitedDataCopy];
		if ( userPolicyString != NULL )
		{
			needSize = CFStringGetMaximumSizeForEncoding( CFStringGetLength(userPolicyString), kCFStringEncodingUTF8 );
			*outPolicyStr = (char *) malloc( needSize + 1 );
			if ( *outPolicyStr != NULL )
			{
				if ( CFStringGetCString(userPolicyString, *outPolicyStr, needSize + 1, kCFStringEncodingUTF8) )
					result = 1;
			}
			CFRelease( userPolicyString );
		}
		[userPolicyObj release];
	}

	// clean up on failure
	if ( result == 0 && *outPolicyStr != NULL )
	{
		free( *outPolicyStr );
		*outPolicyStr = NULL;
	}
	
	return result;
}


// ----------------------------------------------------------------------------------------
//  ConvertSpaceDelimitedPolicyToXML
//
//  Returns: -1 fail, or 0 success. 
//  <outXMLDataStr> is malloc'd memory, caller must free.
// ----------------------------------------------------------------------------------------

int ConvertSpaceDelimitedPolicyToXML( const char *inPolicyStr, char **outXMLDataStr )
{
	return ConvertSpaceDelimitedPoliciesToXML( inPolicyStr, NO, outXMLDataStr );
}

int ConvertSpaceDelimitedPoliciesToXML( const char *inPolicyStr, int inPreserveStateInfo, char **outXMLDataStr )
{
	PWAccessFeatures policies;
	PWMoreAccessFeatures morePolicies = {0};
	PolicyUser *userPolicyObj = nil;
	
	if ( inPolicyStr == NULL || outXMLDataStr == NULL )
		return -1;
	
	*outXMLDataStr = NULL;
	
	GetDefaultUserPolicies( &policies );
	if ( ! StringToPWAccessFeaturesExtra( inPolicyStr, &policies, &morePolicies ) )
		return -1;
	
	userPolicyObj = [[PolicyUser alloc] init];
	if ( userPolicyObj != nil )
	{
		[userPolicyObj addMiscPolicies:inPolicyStr];
		[userPolicyObj setPolicy:&policies extraPolicy:&morePolicies];
		*outXMLDataStr = [userPolicyObj policyAsXMLDataCopy:(BOOL)inPreserveStateInfo];
		[userPolicyObj release];
	}
	
	return (*outXMLDataStr == NULL) ? -1 : 0;
}


@implementation PolicyUser

-(void)getPolicy:(PWAccessFeatures *)outPolicy extraPolicy:(PWMoreAccessFeatures *)outExtraPolicy
{
	memcpy(outPolicy, &mUserPolicy, sizeof(PWAccessFeatures));
	memcpy(outExtraPolicy, &mMoreUserPolicy, sizeof(PWMoreAccessFeatures));
}

-(void)setPolicy:(PWAccessFeatures *)inPolicy
{
	memcpy(&mUserPolicy, inPolicy, sizeof(PWAccessFeatures));
}

-(void)setPolicy:(PWAccessFeatures *)inPolicy extraPolicy:(PWMoreAccessFeatures *)inExtraPolicy
{
	[self setPolicy:inPolicy];
	memcpy(&mMoreUserPolicy, inExtraPolicy, sizeof(PWMoreAccessFeatures));
}

//------------------------------------------------------------------------------------------------
//	addMiscPolicies
//
//	warnOfExpirationMinutes, warnOfDisableMinutes
//------------------------------------------------------------------------------------------------

-(void)addMiscPolicies:(const char *)inPolicyStr
{
	const char *warnOfExpirationMinutes = strstr( inPolicyStr, kPWPolicyStr_warnOfExpirationMinutes );
	const char *warnOfDisableMinutes = strstr( inPolicyStr, kPWPolicyStr_warnOfDisableMinutes );
    unsigned long value;
	
	if ( StringToPWAccessFeatures_GetValue( warnOfExpirationMinutes, &value ) )
		mWarnOfExpirationMinutes = value;
		
	if ( StringToPWAccessFeatures_GetValue( warnOfDisableMinutes, &value ) )
		mWarnOfDisableMinutes = value;

	const char *passwordLastSetTime = strstr( inPolicyStr, kPWPolicyStr_passwordLastSetTime );
	if ( StringToPWAccessFeatures_GetValue( passwordLastSetTime, &value ) )
		mModDateOfPassword = value;
}



-(void)convertDictToStruct:(CFDictionaryRef)policyDict
{
	int historyCount = [self intValueForKey:CFSTR(kPWPolicyStr_usingHistory) inDictionary:policyDict];	
	mUserPolicy.usingHistory = (historyCount != 0);
	mUserPolicy.historyCount = historyCount - 1;
	
	mUserPolicy.canModifyPasswordforSelf = 1;
	if ( CFDictionaryContainsKey(policyDict, CFSTR(kPWPolicyStr_canModifyPasswordforSelf)) )
		mUserPolicy.canModifyPasswordforSelf = [self intValueForKey:CFSTR(kPWPolicyStr_canModifyPasswordforSelf)
			inDictionary:policyDict];
	
	mUserPolicy.usingExpirationDate = [self intValueForKey:CFSTR(kPWPolicyStr_expirationDateGMT) inDictionary:policyDict];
	mUserPolicy.usingHardExpirationDate = [self intValueForKey:CFSTR(kPWPolicyStr_hardExpireDateGMT) inDictionary:policyDict];
	mUserPolicy.requiresAlpha = [self intValueForKey:CFSTR(kPWPolicyStr_requiresAlpha) inDictionary:policyDict];
	mUserPolicy.requiresNumeric = [self intValueForKey:CFSTR(kPWPolicyStr_requiresNumeric) inDictionary:policyDict];
	mUserPolicy.passwordCannotBeName = [self intValueForKey:CFSTR(kPWPolicyStr_passwordCannotBeName) inDictionary:policyDict];
	mMoreUserPolicy.requiresMixedCase = [self intValueForKey:CFSTR(kPWPolicyStr_requiresMixedCase) inDictionary:policyDict];
	mMoreUserPolicy.requiresSymbol = [self intValueForKey:CFSTR(kPWPolicyStr_requiresSymbol) inDictionary:policyDict];
	mUserPolicy.newPasswordRequired = [self intValueForKey:CFSTR(kPWPolicyStr_newPasswordRequired) inDictionary:policyDict];
	pwsf_ConvertCFDateToBSDTimeStructCopy( [self dateValueForKey:CFSTR(kPWPolicyStr_expirationDateGMT) inDictionary:policyDict],
		&mUserPolicy.expirationDateGMT );
	pwsf_ConvertCFDateToBSDTimeStructCopy( [self dateValueForKey:CFSTR(kPWPolicyStr_hardExpireDateGMT) inDictionary:policyDict],
		&mUserPolicy.hardExpireDateGMT );
	mUserPolicy.maxMinutesUntilChangePassword = [self intValueForKey:CFSTR(kPWPolicyStr_maxMinutesUntilChangePW)
		inDictionary:policyDict];
	mUserPolicy.maxMinutesUntilDisabled = [self intValueForKey:CFSTR(kPWPolicyStr_maxMinutesUntilDisabled)
		inDictionary:policyDict];
	mUserPolicy.maxMinutesOfNonUse = [self intValueForKey:CFSTR(kPWPolicyStr_maxMinutesOfNonUse) inDictionary:policyDict];
	mUserPolicy.maxFailedLoginAttempts = [self intValueForKey:CFSTR(kPWPolicyStr_maxFailedLoginAttempts) inDictionary:policyDict];
	mUserPolicy.minChars = [self intValueForKey:CFSTR(kPWPolicyStr_minChars) inDictionary:policyDict];
	mUserPolicy.maxChars = [self intValueForKey:CFSTR(kPWPolicyStr_maxChars) inDictionary:policyDict];
	
	mMoreUserPolicy.minutesUntilFailedLoginReset = [self intValueForKey:CFSTR(kPWPolicyStr_minutesUntilFailedLoginReset)
		inDictionary:policyDict];
	mMoreUserPolicy.notGuessablePattern = [self intValueForKey:CFSTR(kPWPolicyStr_notGuessablePattern) inDictionary:policyDict];
	mMoreUserPolicy.isComputerAccount = [self intValueForKey:CFSTR(kPWPolicyStr_isComputerAccount) inDictionary:policyDict];

	if ( CFDictionaryContainsKey(policyDict, CFSTR(kPWPolicyStr_warnOfExpirationMinutes)) )
		mWarnOfExpirationMinutes = [self intValueForKey:CFSTR(kPWPolicyStr_warnOfExpirationMinutes) inDictionary:policyDict];
	if ( CFDictionaryContainsKey(policyDict, CFSTR(kPWPolicyStr_warnOfDisableMinutes)) )
		mWarnOfDisableMinutes = [self intValueForKey:CFSTR(kPWPolicyStr_warnOfDisableMinutes) inDictionary:policyDict];
	if ( CFDictionaryContainsKey(policyDict, CFSTR(kPWPolicyStr_passwordLastSetTime)) )
		mModDateOfPassword = [self intValueForKey:CFSTR(kPWPolicyStr_passwordLastSetTime) inDictionary:policyDict];

}


-(CFMutableDictionaryRef)convertStructToDict;
{
	CFMutableDictionaryRef policyDict;
	int historyNumber;
	CFDateRef expirationDateGMTRef;
	CFDateRef hardExpireDateGMTRef;
	unsigned int aBoolVal;
	CFNumberRef warnOfExpirationRef = NULL;
	CFNumberRef warnOfDisableRef = NULL;
	CFNumberRef passwordLastSetTimeRef = NULL;

	policyDict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	if ( policyDict != NULL )
	{
		// prep
		historyNumber = (mUserPolicy.usingHistory != 0);
		if ( historyNumber > 0 )
			historyNumber += mUserPolicy.historyCount;
		pwsf_ConvertBSDTimeStructCopyToCFDate( &(mUserPolicy.expirationDateGMT), &expirationDateGMTRef );
		pwsf_ConvertBSDTimeStructCopyToCFDate( &(mUserPolicy.hardExpireDateGMT), &hardExpireDateGMTRef );
		
		CFNumberRef usingHistoryRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &historyNumber );
		
		aBoolVal = (mUserPolicy.canModifyPasswordforSelf != 0);
		CFNumberRef canModifyPasswordforSelfRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &aBoolVal );
		
		aBoolVal = (mUserPolicy.usingExpirationDate != 0);
		CFNumberRef usingExpirationDateRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &aBoolVal );
		
		aBoolVal = (mUserPolicy.usingHardExpirationDate != 0);
		CFNumberRef usingHardExpirationDateRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &aBoolVal );
		
		aBoolVal = (mUserPolicy.requiresAlpha != 0);
		CFNumberRef requiresAlphaRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &aBoolVal );
		
		aBoolVal = (mUserPolicy.requiresNumeric != 0);
		CFNumberRef requiresNumericRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &aBoolVal );
		
		aBoolVal = (mMoreUserPolicy.requiresMixedCase != 0);
		CFNumberRef requiresMixedCaseRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &aBoolVal );
		
		aBoolVal = (mMoreUserPolicy.requiresSymbol != 0);
		CFNumberRef requiresSymbolRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &aBoolVal );
		
		CFNumberRef notGuessablePatternRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberLongType, &(mMoreUserPolicy.notGuessablePattern) );
		CFNumberRef minutesUntilFailedLoginResetRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberLongType, &(mMoreUserPolicy.minutesUntilFailedLoginReset) );
		
		aBoolVal = (mUserPolicy.passwordCannotBeName != 0);
		CFNumberRef passwordCannotBeNameRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &aBoolVal );
		
		aBoolVal = (mUserPolicy.isSessionKeyAgent != 0);
		CFNumberRef isSessionKeyAgentRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &aBoolVal );
		
		aBoolVal = (mMoreUserPolicy.isComputerAccount != 0);
		CFNumberRef isComputerAccountRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &aBoolVal );
		
		
		CFNumberRef maxMinutesUntilChangePasswordRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberLongType, &mUserPolicy.maxMinutesUntilChangePassword );
		CFNumberRef maxMinutesUntilDisabledRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberLongType, &mUserPolicy.maxMinutesUntilDisabled );
		CFNumberRef maxMinutesOfNonUseRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberLongType, &mUserPolicy.maxMinutesOfNonUse );
		
		CFNumberRef maxFailedLoginAttemptsRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberShortType, &mUserPolicy.maxFailedLoginAttempts );
		CFNumberRef minCharsRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberShortType, &mUserPolicy.minChars );
		CFNumberRef maxCharsRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberShortType, &mUserPolicy.maxChars );

		if ( mWarnOfExpirationMinutes > 0 )
			warnOfExpirationRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberLongType, &mWarnOfExpirationMinutes );
		if ( mWarnOfDisableMinutes > 0 )
			warnOfDisableRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberLongType, &mWarnOfDisableMinutes );

		if ( mModDateOfPassword > 0 )
			passwordLastSetTimeRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberLongType, &mModDateOfPassword );
		
		if ( usingHistoryRef != NULL )
		{
			CFDictionaryAddValue( policyDict, CFSTR(kPWPolicyStr_usingHistory), usingHistoryRef );
			CFRelease( usingHistoryRef );
		}
		
		if ( canModifyPasswordforSelfRef != NULL )
		{
			CFDictionaryAddValue( policyDict, CFSTR(kPWPolicyStr_canModifyPasswordforSelf), canModifyPasswordforSelfRef );
			CFRelease( canModifyPasswordforSelfRef );
		}
		
		if ( usingExpirationDateRef != NULL )
		{
			CFDictionaryAddValue( policyDict, CFSTR(kPWPolicyStr_usingExpirationDate), usingExpirationDateRef );
			CFRelease( usingExpirationDateRef );
		}

		if ( usingHardExpirationDateRef != NULL )
		{
			CFDictionaryAddValue( policyDict, CFSTR(kPWPolicyStr_usingHardExpirationDate), usingHardExpirationDateRef );
			CFRelease( usingHardExpirationDateRef );
		}
		
		if ( requiresAlphaRef != NULL )
		{
			CFDictionaryAddValue( policyDict, CFSTR(kPWPolicyStr_requiresAlpha), requiresAlphaRef );
			CFRelease( requiresAlphaRef );
		}

		if ( requiresNumericRef != NULL )
		{
			CFDictionaryAddValue( policyDict, CFSTR(kPWPolicyStr_requiresNumeric), requiresNumericRef );
			CFRelease( requiresNumericRef );
		}
		
		if ( requiresMixedCaseRef != NULL )
		{
			CFDictionaryAddValue( policyDict, CFSTR(kPWPolicyStr_requiresMixedCase), requiresMixedCaseRef );
			CFRelease( requiresMixedCaseRef );
		}
		
		if ( requiresSymbolRef != NULL )
		{
			CFDictionaryAddValue( policyDict, CFSTR(kPWPolicyStr_requiresSymbol), requiresSymbolRef );
			CFRelease( requiresSymbolRef );
		}
		
		if ( notGuessablePatternRef != NULL )
		{
			CFDictionaryAddValue( policyDict, CFSTR(kPWPolicyStr_notGuessablePattern), notGuessablePatternRef );
			CFRelease( notGuessablePatternRef );
		}

		if ( minutesUntilFailedLoginResetRef != NULL )
		{
			CFDictionaryAddValue( policyDict, CFSTR(kPWPolicyStr_minutesUntilFailedLoginReset), minutesUntilFailedLoginResetRef );
			CFRelease( minutesUntilFailedLoginResetRef );
		}
		
		if ( expirationDateGMTRef != NULL )
		{
			CFDictionaryAddValue( policyDict, CFSTR(kPWPolicyStr_expirationDateGMT), expirationDateGMTRef );
			CFRelease( expirationDateGMTRef );
		}
		
		if ( hardExpireDateGMTRef != NULL )
		{
			CFDictionaryAddValue( policyDict, CFSTR(kPWPolicyStr_hardExpireDateGMT), hardExpireDateGMTRef );
			CFRelease( hardExpireDateGMTRef );
		}

		if ( maxMinutesUntilChangePasswordRef != NULL )
		{
			CFDictionaryAddValue( policyDict, CFSTR(kPWPolicyStr_maxMinutesUntilChangePW), maxMinutesUntilChangePasswordRef );
			CFRelease( maxMinutesUntilChangePasswordRef );
		}
		
		if ( maxMinutesUntilDisabledRef != NULL )
		{
			CFDictionaryAddValue( policyDict, CFSTR(kPWPolicyStr_maxMinutesUntilDisabled), maxMinutesUntilDisabledRef );
			CFRelease( maxMinutesUntilDisabledRef );
		}
		
		if ( maxMinutesOfNonUseRef != NULL )
		{
			CFDictionaryAddValue( policyDict, CFSTR(kPWPolicyStr_maxMinutesOfNonUse), maxMinutesOfNonUseRef );
			CFRelease( maxMinutesOfNonUseRef );
		}

		if ( maxFailedLoginAttemptsRef != NULL )
		{
			CFDictionaryAddValue( policyDict, CFSTR(kPWPolicyStr_maxFailedLoginAttempts), maxFailedLoginAttemptsRef );
			CFRelease( maxFailedLoginAttemptsRef );
		}

		if ( minCharsRef != NULL )
		{
			CFDictionaryAddValue( policyDict, CFSTR(kPWPolicyStr_minChars), minCharsRef );
			CFRelease( minCharsRef );
		}

		if ( maxCharsRef != NULL )
		{
			CFDictionaryAddValue( policyDict, CFSTR(kPWPolicyStr_maxChars), maxCharsRef );
			CFRelease( maxCharsRef );
		}

		if ( passwordCannotBeNameRef != NULL )
		{
			CFDictionaryAddValue( policyDict, CFSTR(kPWPolicyStr_passwordCannotBeName), passwordCannotBeNameRef );
			CFRelease( passwordCannotBeNameRef );
		}

		if ( isSessionKeyAgentRef != NULL )
		{
			CFDictionaryAddValue( policyDict, CFSTR(kPWPolicyStr_isSessionKeyAgent), isSessionKeyAgentRef );
			CFRelease( isSessionKeyAgentRef );
		}
		
		if ( isComputerAccountRef != NULL )
		{
			CFDictionaryAddValue( policyDict, CFSTR(kPWPolicyStr_isComputerAccount), isComputerAccountRef );
			CFRelease( isComputerAccountRef );
		}
		
		if ( warnOfExpirationRef != NULL )
		{
			CFDictionaryAddValue( policyDict, CFSTR(kPWPolicyStr_warnOfExpirationMinutes), warnOfExpirationRef );
			CFRelease( warnOfExpirationRef );
		}
		
		if ( warnOfDisableRef != NULL )
		{
			CFDictionaryAddValue( policyDict, CFSTR(kPWPolicyStr_warnOfDisableMinutes), warnOfDisableRef );
			CFRelease( warnOfDisableRef );
		}

		if ( passwordLastSetTimeRef != NULL )
		{
			CFDictionaryAddValue( policyDict, CFSTR(kPWPolicyStr_passwordLastSetTime), passwordLastSetTimeRef );
			CFRelease( passwordLastSetTimeRef );
		}
	}
		
	return policyDict;
}

-(CFMutableDictionaryRef)convertStructToDictWithState
{
	unsigned int aBoolVal;
		
	aBoolVal = (mUserPolicy.isDisabled != 0);
	CFNumberRef isDisabledRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &aBoolVal );

	aBoolVal = (mUserPolicy.isAdminUser != 0);
	CFNumberRef isAdminUserRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &aBoolVal );

	aBoolVal = (mUserPolicy.newPasswordRequired != 0);
	CFNumberRef newPasswordRequiredRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &aBoolVal );

	CFMutableDictionaryRef theDict = [self convertStructToDict];
	if ( theDict != NULL )
	{
		if ( isDisabledRef != NULL )
		{
			CFDictionaryAddValue( theDict, CFSTR(kPWPolicyStr_isDisabled), isDisabledRef );
			CFRelease( isDisabledRef );
		}
		if ( isAdminUserRef != NULL )
		{
			CFDictionaryAddValue( theDict, CFSTR(kPWPolicyStr_isAdminUser), isAdminUserRef );
			CFRelease( isAdminUserRef );
		}
		if ( newPasswordRequiredRef != NULL )
		{
			CFDictionaryAddValue( theDict, CFSTR(kPWPolicyStr_newPasswordRequired), newPasswordRequiredRef );
			CFRelease( newPasswordRequiredRef );
		}
	}
	
	return theDict;
}

@end
