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
#include "CPolicyXML.h"

// ----------------------------------------------------------------------------------------
#pragma mark -
#pragma mark C API
#pragma mark -
// ----------------------------------------------------------------------------------------

// ----------------------------------------------------------------------------------------
//  GetDefaultUserPolicies
//
//  Returns: void
// ----------------------------------------------------------------------------------------

void GetDefaultUserPolicies( PWAccessFeatures *inOutUserPolicies )
{
	CPolicyXML::CPolicyXMLCommonInitStatic( inOutUserPolicies );
}


// ----------------------------------------------------------------------------------------
#pragma mark -
#pragma mark Public Methods
#pragma mark -
// ----------------------------------------------------------------------------------------

// ----------------------------------------------------------------------------------------
//  CPolicyXML constructors
// ----------------------------------------------------------------------------------------

CPolicyXML::CPolicyXML() : CPolicyBase()
{
	CPolicyCommonInit();
}


CPolicyXML::CPolicyXML( CFDictionaryRef inPolicyDict ) : CPolicyBase()
{
	CPolicyCommonInit();
	this->ConvertPropertyListPolicyToStruct( (CFMutableDictionaryRef)inPolicyDict );
}


CPolicyXML::CPolicyXML( const char *xmlDataStr ) : CPolicyBase()
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
//  CPolicyXML desstructor
// ----------------------------------------------------------------------------------------

CPolicyXML::~CPolicyXML()
{
}


// ----------------------------------------------------------------------------------------
//  CPolicyXMLCommonInit
//
//  handles actions common to all constructors
// ----------------------------------------------------------------------------------------

void
CPolicyXML::CPolicyCommonInit( void )
{
	CPolicyXML::CPolicyXMLCommonInitStatic( &mPolicy );
	bzero( &mExtraPolicy, sizeof(mExtraPolicy) );
	mWarnOfExpirationMinutes = 0;
	mWarnOfDisableMinutes = 0;
	mProjectedPasswordExpireDate = 0;
	mProjectedAccountDisableDate = 0;
	mModDateOfPassword = 0;
}


// ----------------------------------------------------------------------------------------
//  CPolicyXMLCommonInitStatic
//
//  handles actions common to all constructors
// ----------------------------------------------------------------------------------------

void
CPolicyXML::CPolicyXMLCommonInitStatic( PWAccessFeatures *inOutPolicies )
{
	if ( inOutPolicies == NULL )
		return;
	
	// set the defaults
	bzero( inOutPolicies, sizeof(PWAccessFeatures) );
	
	inOutPolicies->usingHistory = false;
	inOutPolicies->canModifyPasswordforSelf = true;
	inOutPolicies->usingExpirationDate = false;
	inOutPolicies->usingHardExpirationDate = false;
	inOutPolicies->requiresAlpha = false;
	inOutPolicies->requiresNumeric = false;
	inOutPolicies->passwordCannotBeName = false;
	inOutPolicies->historyCount = 0;
	inOutPolicies->isSessionKeyAgent = false;
	inOutPolicies->maxMinutesUntilChangePassword = 0;
	inOutPolicies->maxMinutesUntilDisabled = 0;
	inOutPolicies->maxMinutesOfNonUse = 0;
	inOutPolicies->maxFailedLoginAttempts = 0;
	inOutPolicies->minChars = 0;
	inOutPolicies->maxChars = 0;
}


// ----------------------------------------------------------------------------------------
//  GetPolicy
//
//  retrieves a copy of the current policy
// ----------------------------------------------------------------------------------------

void
CPolicyXML::GetPolicy( PWAccessFeatures *outPolicy )
{
	if ( outPolicy != NULL )
		memcpy( outPolicy, &mPolicy, sizeof(PWAccessFeatures) );
}


// ----------------------------------------------------------------------------------------
//  GetPolicyAsSpaceDelimitedData
//
//  Returns: a malloc'd copy of the current policy in space-delimited form.
//  Caller must free.
// ----------------------------------------------------------------------------------------

char *
CPolicyXML::GetPolicyAsSpaceDelimitedData( void )
{
	char *returnStr = NULL;
	long metaFeatureStrLen = 0;
	char featureStr[2048];
	char metaFeatureStr[512] = { 0, };
	
	PWAccessFeaturesToStringWithoutStateInfoExtra( &mPolicy, &mExtraPolicy, sizeof(featureStr), featureStr );
	
	if ( mWarnOfExpirationMinutes > 0 )
	{
		metaFeatureStrLen = snprintf( metaFeatureStr, sizeof(metaFeatureStr),
				 " warnOfExpirationMinutes=%lu projectedPasswordExpireDate=%lu",
				 mWarnOfExpirationMinutes,
				 (unsigned long)mProjectedPasswordExpireDate );
	}
	
	if ( mWarnOfDisableMinutes > 0 )
	{
		metaFeatureStrLen += snprintf( metaFeatureStr + metaFeatureStrLen, sizeof(metaFeatureStr) - metaFeatureStrLen,
				 " warnOfDisableMinutes=%lu projectedAccountDisableDate=%lu",
				 mWarnOfDisableMinutes,
				 (unsigned long)mProjectedAccountDisableDate );
	}

	if ( mModDateOfPassword > 0 )
	{
		snprintf( metaFeatureStr + metaFeatureStrLen, sizeof(metaFeatureStr) - metaFeatureStrLen, " %s=%lu", kPWPolicyStr_passwordLastSetTime, (unsigned long)mModDateOfPassword );
	}
	
	returnStr = (char *) malloc( strlen(featureStr) + strlen(metaFeatureStr) + 1 );
	if ( returnStr != NULL )
	{
		strcpy( returnStr, featureStr );
		strcat( returnStr, metaFeatureStr );
	}
	
	return returnStr;
}


// ----------------------------------------------------------------------------------------
//  SetPolicy
// ----------------------------------------------------------------------------------------

void
CPolicyXML::SetPolicy( PWAccessFeatures *inPolicy )
{
	if ( inPolicy != NULL )
	{
		memcpy( &mPolicy, inPolicy, sizeof(PWAccessFeatures) );
		this->ConvertStructToPropertyListPolicy();
	}
}


// ----------------------------------------------------------------------------------------
//  SetPolicy
// ----------------------------------------------------------------------------------------

void
CPolicyXML::SetPolicy( CFDictionaryRef inPolicyDict )
{
	this->ConvertPropertyListPolicyToStruct( (CFMutableDictionaryRef)inPolicyDict );
}


// ----------------------------------------------------------------------------------------
//  SetPolicyExtra
// ----------------------------------------------------------------------------------------

void
CPolicyXML::SetPolicyExtra( PWAccessFeatures *inPolicy, PWMoreAccessFeatures *inExtraPolicy )
{
	if ( inPolicy != NULL && inExtraPolicy != NULL )
	{
		memcpy( &mPolicy, inPolicy, sizeof(PWAccessFeatures) );
		memcpy( &mExtraPolicy, inExtraPolicy, sizeof(PWMoreAccessFeatures) );
		this->ConvertStructToPropertyListPolicy();
	}
}


//------------------------------------------------------------------------------------------------
//	AddMiscPolicies
//
//	warnOfExpirationMinutes, warnOfDisableMinutes
//------------------------------------------------------------------------------------------------

void
CPolicyXML::AddMiscPolicies( const char *inPolicyStr )
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
CPolicyXML::ConvertPropertyListPolicyToStruct( CFMutableDictionaryRef inPolicyDict )
{
	int result = 0;
	short aShortValue;
	long aLongValue;
	bool aBoolValue;
	CFTypeRef valueRef;
	struct tm bsdTimeStruct;
	
	if ( inPolicyDict == NULL )
		return -1;
	
	CFRetain( inPolicyDict );
	if ( mPolicyDict != NULL )
		CFRelease( mPolicyDict );
	mPolicyDict = inPolicyDict;	
	
	if ( this->GetBooleanForKey( CFSTR(kPWPolicyStr_canModifyPasswordforSelf), &aBoolValue ) )
		mPolicy.canModifyPasswordforSelf = aBoolValue;
	
	if ( this->GetBooleanForKey( CFSTR(kPWPolicyStr_usingExpirationDate), &aBoolValue ) )
		mPolicy.usingExpirationDate = aBoolValue;
	
	if ( this->GetBooleanForKey( CFSTR(kPWPolicyStr_usingHardExpirationDate), &aBoolValue ) )
		mPolicy.usingHardExpirationDate = aBoolValue;
	
	if ( this->GetBooleanForKey( CFSTR(kPWPolicyStr_requiresAlpha), &aBoolValue ) )
		mPolicy.requiresAlpha = aBoolValue;
	
	if ( this->GetBooleanForKey( CFSTR(kPWPolicyStr_requiresNumeric), &aBoolValue ) )
		mPolicy.requiresNumeric = aBoolValue;
	
	if ( this->GetBooleanForKey( CFSTR(kPWPolicyStr_requiresMixedCase), &aBoolValue ) )
		mExtraPolicy.requiresMixedCase = aBoolValue;
	
	if ( this->GetBooleanForKey( CFSTR(kPWPolicyStr_isComputerAccount), &aBoolValue ) )
		mExtraPolicy.isComputerAccount = aBoolValue;
	
	// notGuessablePattern
	if ( CFDictionaryGetValueIfPresent( mPolicyDict, CFSTR(kPWPolicyStr_notGuessablePattern), (const void **)&valueRef ) &&
		CFGetTypeID(valueRef) == CFNumberGetTypeID() &&
		CFNumberGetValue( (CFNumberRef)valueRef, kCFNumberLongType, &aLongValue) )
	{
		mExtraPolicy.notGuessablePattern = (UInt32)aLongValue;
	}
	
    // expirationDateGMT
	if ( CFDictionaryGetValueIfPresent( mPolicyDict, CFSTR(kPWPolicyStr_expirationDateGMT), (const void **)&valueRef ) &&
		CFGetTypeID(valueRef) == CFDateGetTypeID() )
	{
		this->ConvertCFDateToBSDTime( (CFDateRef)valueRef, &mPolicy.expirationDateGMT );
	}
	
	// hardExpireDateGMT
	if ( CFDictionaryGetValueIfPresent( mPolicyDict, CFSTR(kPWPolicyStr_hardExpireDateGMT), (const void **)&valueRef ) &&
		CFGetTypeID(valueRef) == CFDateGetTypeID() )
	{
		this->ConvertCFDateToBSDTime( (CFDateRef)valueRef, &mPolicy.hardExpireDateGMT );
	}
	
	// maxMinutesUntilChangePassword
	if ( CFDictionaryGetValueIfPresent( mPolicyDict, CFSTR(kPWPolicyStr_maxMinutesUntilChangePW), (const void **)&valueRef ) &&
		CFGetTypeID(valueRef) == CFNumberGetTypeID() &&
		CFNumberGetValue( (CFNumberRef)valueRef, kCFNumberLongType, &aLongValue) )
	{
		mPolicy.maxMinutesUntilChangePassword = (UInt32)aLongValue;
	}
	
	// maxMinutesUntilDisabled
	if ( CFDictionaryGetValueIfPresent( mPolicyDict, CFSTR(kPWPolicyStr_maxMinutesUntilDisabled), (const void **)&valueRef ) &&
		CFGetTypeID(valueRef) == CFNumberGetTypeID() &&
		CFNumberGetValue( (CFNumberRef)valueRef, kCFNumberLongType, &aLongValue) )
	{
		mPolicy.maxMinutesUntilDisabled = (UInt32)aLongValue;
	}

	// maxMinutesOfNonUse
	if ( CFDictionaryGetValueIfPresent( mPolicyDict, CFSTR(kPWPolicyStr_maxMinutesOfNonUse), (const void **)&valueRef ) &&
		CFGetTypeID(valueRef) == CFNumberGetTypeID() &&
		CFNumberGetValue( (CFNumberRef)valueRef, kCFNumberLongType, &aLongValue) )
	{
		mPolicy.maxMinutesOfNonUse = (UInt32)aLongValue;
	}

	// maxFailedLoginAttempts
	if ( CFDictionaryGetValueIfPresent( mPolicyDict, CFSTR(kPWPolicyStr_maxFailedLoginAttempts), (const void **)&valueRef ) &&
		CFGetTypeID(valueRef) == CFNumberGetTypeID() &&
		CFNumberGetValue( (CFNumberRef)valueRef, kCFNumberShortType, &aShortValue) )
	{
		mPolicy.maxFailedLoginAttempts = aShortValue;
	}
	
	// minChars
	if ( CFDictionaryGetValueIfPresent( mPolicyDict, CFSTR(kPWPolicyStr_minChars), (const void **)&valueRef ) &&
		CFGetTypeID(valueRef) == CFNumberGetTypeID() &&
		CFNumberGetValue( (CFNumberRef)valueRef, kCFNumberShortType, &aShortValue) )
	{
		mPolicy.minChars = aShortValue;
	}
	
	// maxChars
	if ( CFDictionaryGetValueIfPresent( mPolicyDict, CFSTR(kPWPolicyStr_maxChars), (const void **)&valueRef ) &&
		CFGetTypeID(valueRef) == CFNumberGetTypeID() &&
		CFNumberGetValue( (CFNumberRef)valueRef, kCFNumberShortType, &aShortValue) )
	{
		mPolicy.maxChars = aShortValue;
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
			mPolicy.usingHistory = true;
			mPolicy.historyCount = aShortValue - 1;
		}
		else
		{
			mPolicy.usingHistory = false;
			mPolicy.historyCount = 0;
		}
	}
	
	// passwordCannotBeName
	if ( this->GetBooleanForKey( CFSTR(kPWPolicyStr_passwordCannotBeName), &aBoolValue ) )
		mPolicy.passwordCannotBeName = aBoolValue;
	
	// isSessionKeyAgent
	if ( this->GetBooleanForKey( CFSTR(kPWPolicyStr_isSessionKeyAgent), &aBoolValue ) )
		mPolicy.isSessionKeyAgent = aBoolValue;
	
	// isComputerAccount
	if ( this->GetBooleanForKey( CFSTR(kPWPolicyStr_isComputerAccount), &aBoolValue ) )
		mExtraPolicy.isComputerAccount = aBoolValue;
	
	// warnOfExpirationMinutes
	if ( CFDictionaryGetValueIfPresent( mPolicyDict, CFSTR(kPWPolicyStr_warnOfExpirationMinutes), (const void **)&valueRef ) &&
		CFGetTypeID(valueRef) == CFNumberGetTypeID() &&
		CFNumberGetValue( (CFNumberRef)valueRef, kCFNumberLongType, &aLongValue) )
	{
		mWarnOfExpirationMinutes = aLongValue;
	}

	// warnOfDisableMinutes
	if ( CFDictionaryGetValueIfPresent( mPolicyDict, CFSTR(kPWPolicyStr_warnOfDisableMinutes), (const void **)&valueRef ) &&
		CFGetTypeID(valueRef) == CFNumberGetTypeID() &&
		CFNumberGetValue( (CFNumberRef)valueRef, kCFNumberLongType, &aLongValue) )
	{
		mWarnOfDisableMinutes = aLongValue;
	}

	// passwordLastSetTime
	if ( CFDictionaryGetValueIfPresent( mPolicyDict, CFSTR(kPWPolicyStr_passwordLastSetTime), (const void **)&valueRef ) &&
		CFGetTypeID(valueRef) == CFNumberGetTypeID() &&
		CFNumberGetValue( (CFNumberRef)valueRef, kCFNumberLongType, &aLongValue) )
	{
		mModDateOfPassword = aLongValue;
	}

	if ( CFDictionaryGetValueIfPresent( mPolicyDict, CFSTR(kPWPolicyStr_passwordLastSetTime), (const void **)&valueRef ) &&
		CFGetTypeID(valueRef) == CFNumberGetTypeID() &&
		CFNumberGetValue( (CFNumberRef)valueRef, kCFNumberLongType, &aLongValue) )
	{
		mModDateOfPassword = aLongValue;
	}
	
	// projectedPasswordExpireDate
	if ( CFDictionaryGetValueIfPresent( mPolicyDict, CFSTR(kPWPolicyStr_projectedPasswordExpireDate), (const void **)&valueRef ) &&
		CFGetTypeID(valueRef) == CFDateGetTypeID() )
	{
		this->ConvertCFDateToBSDTime( (CFDateRef)valueRef, &bsdTimeStruct );
		mProjectedPasswordExpireDate = timegm( &bsdTimeStruct );
	}
	
	// projectedAccountDisableDate
	if ( CFDictionaryGetValueIfPresent( mPolicyDict, CFSTR(kPWPolicyStr_projectedAccountDisableDate), (const void **)&valueRef ) &&
		CFGetTypeID(valueRef) == CFDateGetTypeID() )
	{
		this->ConvertCFDateToBSDTime( (CFDateRef)valueRef, &bsdTimeStruct );
		mProjectedAccountDisableDate = timegm( &bsdTimeStruct );
	}
	
	return result;
}


// ----------------------------------------------------------------------------------------
//  ConvertStructToPropertyListPolicy
//
//  Returns: -1 fail, or 0 success
// ----------------------------------------------------------------------------------------

int
CPolicyXML::ConvertStructToPropertyListPolicy( void )
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
	if ( policyDict == NULL )
		return -1;
	
	historyNumber = (mPolicy.usingHistory != 0);
	if ( historyNumber > 0 )
		historyNumber += mPolicy.historyCount;
	
	CFNumberRef usingHistoryRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &historyNumber );
	
	aBoolVal = (mPolicy.canModifyPasswordforSelf != 0);
	CFNumberRef canModifyPasswordforSelfRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &aBoolVal );
	
	aBoolVal = (mPolicy.usingExpirationDate != 0);
	CFNumberRef usingExpirationDateRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &aBoolVal );
	
	aBoolVal = (mPolicy.usingHardExpirationDate != 0);
	CFNumberRef usingHardExpirationDateRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &aBoolVal );
	
	aBoolVal = (mPolicy.requiresAlpha != 0);
	CFNumberRef requiresAlphaRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &aBoolVal );
	
	aBoolVal = (mPolicy.requiresNumeric != 0);
	CFNumberRef requiresNumericRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &aBoolVal );
	
	aBoolVal = (mExtraPolicy.requiresMixedCase != 0);
	CFNumberRef requiresMixedCaseRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &aBoolVal );
	
	CFNumberRef notGuessablePatternRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberLongType, &(mExtraPolicy.notGuessablePattern) );
	
	aBoolVal = (mPolicy.passwordCannotBeName != 0);
	CFNumberRef passwordCannotBeNameRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &aBoolVal );
	
	aBoolVal = (mPolicy.isSessionKeyAgent != 0);
	CFNumberRef isSessionKeyAgentRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &aBoolVal );

	aBoolVal = (mExtraPolicy.isComputerAccount != 0);
	CFNumberRef isComputerAccountRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &aBoolVal );
	
	CFNumberRef maxMinutesUntilChangePasswordRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberLongType, &mPolicy.maxMinutesUntilChangePassword );
	CFNumberRef maxMinutesUntilDisabledRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberLongType, &mPolicy.maxMinutesUntilDisabled );
	CFNumberRef maxMinutesOfNonUseRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberLongType, &mPolicy.maxMinutesOfNonUse );
	
	CFNumberRef maxFailedLoginAttemptsRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberShortType, &mPolicy.maxFailedLoginAttempts );
	CFNumberRef minCharsRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberShortType, &mPolicy.minChars );
	CFNumberRef maxCharsRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberShortType, &mPolicy.maxChars );
	
	this->ConvertBSDTimeToCFDate( &(mPolicy.expirationDateGMT), &expirationDateGMTRef );
	this->ConvertBSDTimeToCFDate( &(mPolicy.hardExpireDateGMT), &hardExpireDateGMTRef );
	
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
	
	if ( mPolicyDict != NULL )
		CFRelease( mPolicyDict );
	mPolicyDict = policyDict;
	
	return 0;
}




