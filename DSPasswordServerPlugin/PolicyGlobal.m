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

#import "PolicyGlobal.h"

@implementation PolicyGlobal

-(void)getPolicy:(PWGlobalAccessFeatures *)outPolicy extraPolicy:(PWGlobalMoreAccessFeatures *)outExtraPolicy
{
	memcpy(outPolicy, &mGlobalPolicy, sizeof(PWGlobalAccessFeatures));
	memcpy(outExtraPolicy, &mExtraGlobalPolicy, sizeof(PWGlobalMoreAccessFeatures));
}

-(void)setPolicy:(PWGlobalAccessFeatures *)inPolicy
{
	memcpy(&mGlobalPolicy, inPolicy, sizeof(PWGlobalAccessFeatures));
}

-(void)setPolicy:(PWGlobalAccessFeatures *)inPolicy extraPolicy:(PWGlobalMoreAccessFeatures *)inExtraPolicy
{
	[self setPolicy:inPolicy];
	memcpy(&mExtraGlobalPolicy, inExtraPolicy, sizeof(PWGlobalMoreAccessFeatures));
}

-(void)convertDictToStruct:(CFDictionaryRef)policyDict
{
	int historyCount = [self intValueForKey:CFSTR(kPWPolicyStr_usingHistory) inDictionary:policyDict];	
	mGlobalPolicy.usingHistory = (historyCount != 0);
	SetGlobalHistoryCount(mGlobalPolicy, historyCount - 1);
	
	mGlobalPolicy.noModifyPasswordforSelf
		= ([self intValueForKey:CFSTR(kPWPolicyStr_canModifyPasswordforSelf) inDictionary:policyDict] == 0);
	
	mGlobalPolicy.usingExpirationDate = [self intValueForKey:CFSTR(kPWPolicyStr_expirationDateGMT) inDictionary:policyDict];
	mGlobalPolicy.usingHardExpirationDate = [self intValueForKey:CFSTR(kPWPolicyStr_hardExpireDateGMT) inDictionary:policyDict];
	mGlobalPolicy.requiresAlpha = [self intValueForKey:CFSTR(kPWPolicyStr_requiresAlpha) inDictionary:policyDict];
	mGlobalPolicy.requiresNumeric = [self intValueForKey:CFSTR(kPWPolicyStr_requiresNumeric) inDictionary:policyDict];
	mGlobalPolicy.passwordCannotBeName = [self intValueForKey:CFSTR(kPWPolicyStr_passwordCannotBeName) inDictionary:policyDict];
	mGlobalPolicy.requiresMixedCase = [self intValueForKey:CFSTR(kPWPolicyStr_requiresMixedCase) inDictionary:policyDict];
	mGlobalPolicy.requiresSymbol = [self intValueForKey:CFSTR(kPWPolicyStr_requiresSymbol) inDictionary:policyDict];
	mGlobalPolicy.newPasswordRequired = [self intValueForKey:CFSTR(kPWPolicyStr_newPasswordRequired) inDictionary:policyDict];
	pwsf_ConvertCFDateToBSDTimeStructCopy( [self dateValueForKey:CFSTR(kPWPolicyStr_expirationDateGMT) inDictionary:policyDict],
		&mGlobalPolicy.expirationDateGMT );
	pwsf_ConvertCFDateToBSDTimeStructCopy( [self dateValueForKey:CFSTR(kPWPolicyStr_hardExpireDateGMT) inDictionary:policyDict],
		&mGlobalPolicy.hardExpireDateGMT );
	mGlobalPolicy.maxMinutesUntilChangePassword = [self intValueForKey:CFSTR(kPWPolicyStr_maxMinutesUntilChangePW)
		inDictionary:policyDict];
	mGlobalPolicy.maxMinutesUntilDisabled = [self intValueForKey:CFSTR(kPWPolicyStr_maxMinutesUntilDisabled)
		inDictionary:policyDict];
	mGlobalPolicy.maxMinutesOfNonUse = [self intValueForKey:CFSTR(kPWPolicyStr_maxMinutesOfNonUse) inDictionary:policyDict];

	mGlobalPolicy.maxFailedLoginAttempts = [self intValueForKey:CFSTR(kPWPolicyStr_maxFailedLoginAttempts)
		inDictionary:policyDict];
	mGlobalPolicy.minChars = [self intValueForKey:CFSTR(kPWPolicyStr_minChars) inDictionary:policyDict];
	mGlobalPolicy.maxChars = [self intValueForKey:CFSTR(kPWPolicyStr_maxChars) inDictionary:policyDict];
	
	mExtraGlobalPolicy.minutesUntilFailedLoginReset = [self intValueForKey:CFSTR(kPWPolicyStr_minutesUntilFailedLoginReset)
		inDictionary:policyDict];
	mExtraGlobalPolicy.notGuessablePattern = [self intValueForKey:CFSTR(kPWPolicyStr_notGuessablePattern) inDictionary:policyDict];
}


-(CFMutableDictionaryRef)convertStructToDict;
{
	CFMutableDictionaryRef policyDict;
	int historyNumber;
	CFDateRef expirationDateGMTRef;
	CFDateRef hardExpireDateGMTRef;
	unsigned int aBoolVal;
	
	policyDict = CFDictionaryCreateMutable( kCFAllocatorDefault, 21, &kCFTypeDictionaryKeyCallBacks,
					&kCFTypeDictionaryValueCallBacks );
	if ( policyDict != NULL )
	{
		// prep
		historyNumber = (mGlobalPolicy.usingHistory != 0);
		if ( historyNumber > 0 )
			historyNumber += GlobalHistoryCount(mGlobalPolicy);
		pwsf_ConvertBSDTimeStructCopyToCFDate( &(mGlobalPolicy.expirationDateGMT), &expirationDateGMTRef );
		pwsf_ConvertBSDTimeStructCopyToCFDate( &(mGlobalPolicy.hardExpireDateGMT), &hardExpireDateGMTRef );
	
		CFNumberRef usingHistoryRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &historyNumber );
			
		aBoolVal = (mGlobalPolicy.usingExpirationDate != 0);
		CFNumberRef usingExpirationDateRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &aBoolVal );
		
		aBoolVal = (mGlobalPolicy.usingHardExpirationDate != 0);
		CFNumberRef usingHardExpirationDateRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &aBoolVal );
		
		aBoolVal = (mGlobalPolicy.requiresAlpha != 0);
		CFNumberRef requiresAlphaRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &aBoolVal );
		
		aBoolVal = (mGlobalPolicy.requiresNumeric != 0);
		CFNumberRef requiresNumericRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &aBoolVal );
		
		aBoolVal = (mGlobalPolicy.passwordCannotBeName != 0);
		CFNumberRef passwordCannotBeNameRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &aBoolVal );
			
		aBoolVal = (mGlobalPolicy.requiresMixedCase != 0);
		CFNumberRef requiresMixedCaseRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &aBoolVal );

		aBoolVal = (mGlobalPolicy.requiresSymbol != 0);
		CFNumberRef requiresSymbolRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &aBoolVal );

		aBoolVal = (mGlobalPolicy.newPasswordRequired != 0);
		CFNumberRef newPasswordRequiredRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &aBoolVal );
		
		CFNumberRef notGuessablePatternRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberLongType,
			&(mExtraGlobalPolicy.notGuessablePattern) );
		CFNumberRef minutesUntilFailedLoginResetRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberLongType,
			&(mExtraGlobalPolicy.minutesUntilFailedLoginReset) );
		
		CFNumberRef maxMinutesUntilChangePasswordRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberLongType,
			&mGlobalPolicy.maxMinutesUntilChangePassword );
		CFNumberRef maxMinutesUntilDisabledRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberLongType,
			&mGlobalPolicy.maxMinutesUntilDisabled );
		CFNumberRef maxMinutesOfNonUseRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberLongType,
			&mGlobalPolicy.maxMinutesOfNonUse );
		
		CFNumberRef maxFailedLoginAttemptsRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberShortType,
			&mGlobalPolicy.maxFailedLoginAttempts );
		CFNumberRef minCharsRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberShortType, &mGlobalPolicy.minChars );
		CFNumberRef maxCharsRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberShortType, &mGlobalPolicy.maxChars );
		
		CFBooleanRef canModifyPasswordforSelfRef = mGlobalPolicy.noModifyPasswordforSelf ? kCFBooleanFalse : kCFBooleanTrue;
			
		// build dictionary
		if ( usingHistoryRef != NULL )
		{
			CFDictionaryAddValue( policyDict, CFSTR(kPWPolicyStr_usingHistory), usingHistoryRef );
			CFRelease( usingHistoryRef );
		}
		
		if ( canModifyPasswordforSelfRef != NULL )
			CFDictionaryAddValue( policyDict, CFSTR(kPWPolicyStr_canModifyPasswordforSelf), canModifyPasswordforSelfRef );
		
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
		
		if ( newPasswordRequiredRef != NULL )
		{
			CFDictionaryAddValue( policyDict, CFSTR(kPWPolicyStr_newPasswordRequired), newPasswordRequiredRef );
			CFRelease( newPasswordRequiredRef );
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
	}
	
	return policyDict;
}

-(CFMutableDictionaryRef)convertStructToDictWithState
{
	return [self convertStructToDict];
}

@end
