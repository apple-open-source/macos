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

#include <syslog.h>
#include "CPolicyGlobalXML.h"

// ----------------------------------------------------------------------------------------
#pragma mark -
#pragma mark C API
#pragma mark -
// ----------------------------------------------------------------------------------------

// ----------------------------------------------------------------------------------------
//  ConvertGlobalXMLPolicyToSpaceDelimited
//
//  Returns: -1 fail, or 0 success. 
//  <outPolicyStr> is malloc'd memory, caller must free.
// ----------------------------------------------------------------------------------------

int ConvertGlobalXMLPolicyToSpaceDelimited( const char *inXMLDataStr, char **outPolicyStr )
{
	if ( inXMLDataStr == NULL || outPolicyStr == NULL )
		return -1;

	CPolicyGlobalXML policyObj( inXMLDataStr );
	*outPolicyStr = policyObj.GetPolicyAsSpaceDelimitedData();
	
	if ( *outPolicyStr == NULL )
		return -1;
	
	return 0;
}


// ----------------------------------------------------------------------------------------
//  ConvertGlobalSpaceDelimitedPolicyToXML
//
//  Returns: -1 fail, or 0 success. 
//  <outXMLDataStr> is malloc'd memory, caller must free.
// ----------------------------------------------------------------------------------------

int ConvertGlobalSpaceDelimitedPolicyToXML( const char *inPolicyStr, char **outXMLDataStr )
{
	PWGlobalAccessFeatures policies = {0};
	PWGlobalMoreAccessFeatures morePolicies = {0};
	
	if ( inPolicyStr == NULL || outXMLDataStr == NULL )
		return -1;
	
	if ( ! StringToPWGlobalAccessFeaturesExtra( inPolicyStr, &policies, &morePolicies ) )
		return -1;
	
	CPolicyGlobalXML policyObj;
	
	policyObj.SetPolicyExtra( &policies, &morePolicies );
	*outXMLDataStr = policyObj.GetPolicyAsXMLData();
	
	if ( *outXMLDataStr == NULL )
		return -1;
	
	return 0;
}


// ----------------------------------------------------------------------------------------
#pragma mark -
#pragma mark Public Methods
#pragma mark -
// ----------------------------------------------------------------------------------------

// ----------------------------------------------------------------------------------------
//  CPolicyXML constructors
// ----------------------------------------------------------------------------------------

CPolicyGlobalXML::CPolicyGlobalXML() : CPolicyBase()
{
	CPolicyCommonInit();
}


CPolicyGlobalXML::CPolicyGlobalXML( CFDictionaryRef inPolicyDict ) : CPolicyBase()
{
	CPolicyCommonInit();
}


CPolicyGlobalXML::CPolicyGlobalXML( const char *xmlDataStr ) : CPolicyBase()
{
	CFDataRef xmlData;
	CFStringRef errorString;
	CFMutableDictionaryRef policyDict;

	CPolicyCommonInit();
	if ( xmlDataStr != NULL )
	{
		xmlData = CFDataCreate( kCFAllocatorDefault, (const unsigned char *)xmlDataStr, strlen(xmlDataStr) );
		if ( xmlData != NULL )
		{
			policyDict = (CFMutableDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault, xmlData, kCFPropertyListMutableContainersAndLeaves, &errorString );
			if ( policyDict != NULL ) {
				this->ConvertPropertyListPolicyToStruct( policyDict );
				CFRelease( policyDict );
			}
			CFRelease( xmlData );
		}
	}
}


// ----------------------------------------------------------------------------------------
//  CPolicyGlobalXML desstructor
// ----------------------------------------------------------------------------------------

CPolicyGlobalXML::~CPolicyGlobalXML()
{
}


// ----------------------------------------------------------------------------------------
//  CPolicyCommonInit
//
//  handles actions common to all constructors
// ----------------------------------------------------------------------------------------

void
CPolicyGlobalXML::CPolicyCommonInit( void )
{
	// set the defaults
	bzero( &mGlobalPolicy, sizeof(mGlobalPolicy) );
	
	mGlobalPolicy.usingHistory = false;
	mGlobalPolicy.usingExpirationDate = false;
	mGlobalPolicy.usingHardExpirationDate = false;
	mGlobalPolicy.requiresAlpha = false;
	mGlobalPolicy.requiresNumeric = false;
	mGlobalPolicy.passwordCannotBeName = false;
	SetGlobalHistoryCount(mGlobalPolicy, 0);
	mGlobalPolicy.maxMinutesUntilChangePassword = 0;
	mGlobalPolicy.maxMinutesUntilDisabled = 0;
	mGlobalPolicy.maxMinutesOfNonUse = 0;
	mGlobalPolicy.maxFailedLoginAttempts = 0;
	mGlobalPolicy.minChars = 0;
	mGlobalPolicy.maxChars = 0;
	
	bzero( &mExtraGlobalPolicy, sizeof(mExtraGlobalPolicy) );
}


// ----------------------------------------------------------------------------------------
//  GetPolicy
//
//  retrieves a copy of the current policy
// ----------------------------------------------------------------------------------------

void
CPolicyGlobalXML::GetPolicy( PWGlobalAccessFeatures *outPolicy )
{
	if ( outPolicy != NULL )
		memcpy( outPolicy, &mGlobalPolicy, sizeof(PWGlobalAccessFeatures) );
}


// ----------------------------------------------------------------------------------------
//  GetPolicyAsSpaceDelimitedData
//
//  Returns: a malloc'd copy of the current policy in space-delimited form.
//  Caller must free.
// ----------------------------------------------------------------------------------------

char *
CPolicyGlobalXML::GetPolicyAsSpaceDelimitedData( void )
{
	char *returnString = NULL;
	char featureString[2048];
	
	PWGlobalAccessFeaturesToStringExtra( &mGlobalPolicy, &mExtraGlobalPolicy, sizeof(featureString), featureString );
	returnString = strdup( featureString );
	
	return returnString;
}


// ----------------------------------------------------------------------------------------
//  SetPolicy
// ----------------------------------------------------------------------------------------

void
CPolicyGlobalXML::SetPolicy( PWGlobalAccessFeatures *inPolicy )
{
	if ( inPolicy != NULL )
	{
		memcpy( &mGlobalPolicy, inPolicy, sizeof(PWGlobalAccessFeatures) );
		this->ConvertStructToPropertyListPolicy();
	}
}


// ----------------------------------------------------------------------------------------
//  SetPolicyExtra
// ----------------------------------------------------------------------------------------

void
CPolicyGlobalXML::SetPolicyExtra( PWGlobalAccessFeatures *inPolicy, PWGlobalMoreAccessFeatures *inMorePolicy )
{
	if ( inPolicy != NULL && inMorePolicy != NULL )
	{
		memcpy( &mGlobalPolicy, inPolicy, sizeof(PWGlobalAccessFeatures) );
		memcpy( &mExtraGlobalPolicy, inMorePolicy, sizeof(PWGlobalMoreAccessFeatures) );
		this->ConvertStructToPropertyListPolicy();
	}
}


// ----------------------------------------------------------------------------------------
#pragma mark -
#pragma mark Protected Methods
#pragma mark -
// ----------------------------------------------------------------------------------------

// ----------------------------------------------------------------------------------------
//  ConvertPropertyListPolicyToStruct
//
//  Returns: -1 fail, or 0 success
// ----------------------------------------------------------------------------------------

int
CPolicyGlobalXML::ConvertPropertyListPolicyToStruct( CFMutableDictionaryRef inPolicyDict )
{
	int result = 0;
	short aShortValue;
	long aLongValue;
	bool aBoolValue;
	CFTypeRef valueRef;
	
	if ( inPolicyDict == NULL )
		return -1;
	
	CFRetain( inPolicyDict );
	if ( mPolicyDict != NULL )
		CFRelease( mPolicyDict );
	mPolicyDict = inPolicyDict;	

	if ( this->GetBooleanForKey( CFSTR(kPWPolicyStr_usingExpirationDate), &aBoolValue ) )
		mGlobalPolicy.usingExpirationDate = aBoolValue;
	
	if ( this->GetBooleanForKey( CFSTR(kPWPolicyStr_usingHardExpirationDate), &aBoolValue ) )
		mGlobalPolicy.usingHardExpirationDate = aBoolValue;
	
	if ( this->GetBooleanForKey( CFSTR(kPWPolicyStr_requiresAlpha), &aBoolValue ) )
		mGlobalPolicy.requiresAlpha = aBoolValue;
	
	if ( this->GetBooleanForKey( CFSTR(kPWPolicyStr_requiresNumeric), &aBoolValue ) )
		mGlobalPolicy.requiresNumeric = aBoolValue;
	
	if ( this->GetBooleanForKey( CFSTR(kPWPolicyStr_requiresMixedCase), &aBoolValue ) )
		mGlobalPolicy.requiresMixedCase = aBoolValue;

	if ( this->GetBooleanForKey( CFSTR(kPWPolicyStr_requiresSymbol), &aBoolValue ) )
		mGlobalPolicy.requiresSymbol = aBoolValue;
	
	if ( this->GetBooleanForKey( CFSTR(kPWPolicyStr_newPasswordRequired), &aBoolValue ) )
		mGlobalPolicy.newPasswordRequired = aBoolValue;
	
	// notGuessablePattern
	if ( CFDictionaryGetValueIfPresent( mPolicyDict, CFSTR(kPWPolicyStr_notGuessablePattern), (const void **)&valueRef ) &&
		CFGetTypeID(valueRef) == CFNumberGetTypeID() &&
		CFNumberGetValue( (CFNumberRef)valueRef, kCFNumberLongType, &aLongValue) )
	{
		mExtraGlobalPolicy.notGuessablePattern = (UInt32)aLongValue;
	}
	
    // expirationDateGMT
	if ( CFDictionaryGetValueIfPresent( mPolicyDict, CFSTR(kPWPolicyStr_expirationDateGMT), (const void **)&valueRef ) &&
		CFGetTypeID(valueRef) == CFDateGetTypeID() )
	{
		this->ConvertCFDateToBSDTime( (CFDateRef)valueRef, &mGlobalPolicy.expirationDateGMT );
	}
	
	// hardExpireDateGMT
	if ( CFDictionaryGetValueIfPresent( mPolicyDict, CFSTR(kPWPolicyStr_hardExpireDateGMT), (const void **)&valueRef ) &&
		CFGetTypeID(valueRef) == CFDateGetTypeID() )
	{
		this->ConvertCFDateToBSDTime( (CFDateRef)valueRef, &mGlobalPolicy.hardExpireDateGMT );
	}
	
	// maxMinutesUntilChangePassword
	if ( CFDictionaryGetValueIfPresent( mPolicyDict, CFSTR(kPWPolicyStr_maxMinutesUntilChangePW), (const void **)&valueRef ) &&
		CFGetTypeID(valueRef) == CFNumberGetTypeID() &&
		CFNumberGetValue( (CFNumberRef)valueRef, kCFNumberLongType, &aLongValue) )
	{
		mGlobalPolicy.maxMinutesUntilChangePassword = (UInt32)aLongValue;
	}
	
	// maxMinutesUntilDisabled
	if ( CFDictionaryGetValueIfPresent( mPolicyDict, CFSTR(kPWPolicyStr_maxMinutesUntilDisabled), (const void **)&valueRef ) &&
		CFGetTypeID(valueRef) == CFNumberGetTypeID() &&
		CFNumberGetValue( (CFNumberRef)valueRef, kCFNumberLongType, &aLongValue) )
	{
		mGlobalPolicy.maxMinutesUntilDisabled = (UInt32)aLongValue;
	}

	// maxMinutesOfNonUse
	if ( CFDictionaryGetValueIfPresent( mPolicyDict, CFSTR(kPWPolicyStr_maxMinutesOfNonUse), (const void **)&valueRef ) &&
		CFGetTypeID(valueRef) == CFNumberGetTypeID() &&
		CFNumberGetValue( (CFNumberRef)valueRef, kCFNumberLongType, &aLongValue) )
	{
		mGlobalPolicy.maxMinutesOfNonUse = (UInt32)aLongValue;
	}

	// maxFailedLoginAttempts
	if ( CFDictionaryGetValueIfPresent( mPolicyDict, CFSTR(kPWPolicyStr_maxFailedLoginAttempts), (const void **)&valueRef ) &&
		CFGetTypeID(valueRef) == CFNumberGetTypeID() &&
		CFNumberGetValue( (CFNumberRef)valueRef, kCFNumberShortType, &aShortValue) )
	{
		mGlobalPolicy.maxFailedLoginAttempts = aShortValue;
	}
	
	// minChars
	if ( CFDictionaryGetValueIfPresent( mPolicyDict, CFSTR(kPWPolicyStr_minChars), (const void **)&valueRef ) &&
		CFGetTypeID(valueRef) == CFNumberGetTypeID() &&
		CFNumberGetValue( (CFNumberRef)valueRef, kCFNumberShortType, &aShortValue) )
	{
		mGlobalPolicy.minChars = aShortValue;
	}
	
	// maxChars
	if ( CFDictionaryGetValueIfPresent( mPolicyDict, CFSTR(kPWPolicyStr_maxChars), (const void **)&valueRef ) &&
		CFGetTypeID(valueRef) == CFNumberGetTypeID() &&
		CFNumberGetValue( (CFNumberRef)valueRef, kCFNumberShortType, &aShortValue) )
	{
		mGlobalPolicy.maxChars = aShortValue;
	}
	
	// usingHistory
	if ( CFDictionaryGetValueIfPresent( mPolicyDict, CFSTR(kPWPolicyStr_usingHistory), (const void **)&valueRef ) &&
		CFGetTypeID(valueRef) == CFNumberGetTypeID() &&
		CFNumberGetValue( (CFNumberRef)valueRef, kCFNumberShortType, &aShortValue) )
	{
		if ( aShortValue > kPWFileMaxHistoryCount )
			aShortValue = kPWFileMaxHistoryCount;
		if ( aShortValue > 0 )
		{
			mGlobalPolicy.usingHistory = true;
			SetGlobalHistoryCount(mGlobalPolicy, aShortValue - 1);
		}
		else
		{
			mGlobalPolicy.usingHistory = false;
			SetGlobalHistoryCount(mGlobalPolicy, 0);
		}
	}
	
	if ( this->GetBooleanForKey( CFSTR(kPWPolicyStr_passwordCannotBeName), &aBoolValue ) )
		mGlobalPolicy.passwordCannotBeName = aBoolValue;
	
	if ( this->GetBooleanForKey( CFSTR(kPWPolicyStr_canModifyPasswordforSelf), &aBoolValue ) )
		mGlobalPolicy.noModifyPasswordforSelf = (aBoolValue==0);
	
	return result;
}


// ----------------------------------------------------------------------------------------
//  ConvertStructToPropertyListPolicy
//
//  Returns: -1 fail, or 0 success
// ----------------------------------------------------------------------------------------

int
CPolicyGlobalXML::ConvertStructToPropertyListPolicy( void )
{
	CFMutableDictionaryRef policyDict;
	int historyNumber;
	CFDateRef expirationDateGMTRef;
	CFDateRef hardExpireDateGMTRef;
	unsigned int aBoolVal;
	
	policyDict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	if ( policyDict == NULL )
		return -1;
	
	historyNumber = (mGlobalPolicy.usingHistory != 0);
	if ( historyNumber > 0 )
		historyNumber += GlobalHistoryCount(mGlobalPolicy);
	
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
	
	CFNumberRef notGuessablePatternRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberLongType, &(mExtraGlobalPolicy.notGuessablePattern) );
	
	CFNumberRef maxMinutesUntilChangePasswordRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberLongType, &mGlobalPolicy.maxMinutesUntilChangePassword );
	CFNumberRef maxMinutesUntilDisabledRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberLongType, &mGlobalPolicy.maxMinutesUntilDisabled );
	CFNumberRef maxMinutesOfNonUseRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberLongType, &mGlobalPolicy.maxMinutesOfNonUse );
	
	CFNumberRef maxFailedLoginAttemptsRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberShortType, &mGlobalPolicy.maxFailedLoginAttempts );
	CFNumberRef minCharsRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberShortType, &mGlobalPolicy.minChars );
	CFNumberRef maxCharsRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberShortType, &mGlobalPolicy.maxChars );
	
	CFBooleanRef canModifyPasswordforSelfRef = mGlobalPolicy.noModifyPasswordforSelf ? kCFBooleanFalse : kCFBooleanTrue;
	
    this->ConvertBSDTimeToCFDate( &(mGlobalPolicy.expirationDateGMT), &expirationDateGMTRef );
	this->ConvertBSDTimeToCFDate( &(mGlobalPolicy.hardExpireDateGMT), &hardExpireDateGMTRef );
	
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

	if ( mPolicyDict != NULL )
		CFRelease( mPolicyDict );
	mPolicyDict = policyDict;
	
	return 0;
}

