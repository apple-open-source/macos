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
/*
 *  AuthFile.cpp
 *  PasswordServer
 */

#if 0
#define DEBUGTIME	1
#else
#define DEBUGTIME	0
#endif

#if DEBUGTIME
//#include "CLog.h"
#include <syslog.h>
#define SRVLOG1(A,B, args...)	syslog(LOG_ALERT, (B), ##args)
#endif

#include <sys/param.h>
#include <sys/stat.h>

#include <CoreServices/CoreServices.h>
#include <TargetConditionals.h>
#include "AuthFile.h"
#include "CAuthFileBase.h"
#include "CReplicaFile.h"

// from DirServicesConst.h (our layer is below DS)
#define kDSValueAuthAuthorityShadowHash				";ShadowHash;"
#define kDSTagAuthAuthorityShadowHash				"ShadowHash"
#define kDSTagAuthAuthorityBetterHashOnly			"BetterHashOnly"
#define kHashNameListPrefix							"HASHLIST:"

#if DEBUGTIME
//------------------------------------------------------------------------------------------------
//	GetTimeAsString
//------------------------------------------------------------------------------------------------

void GetTimeAsString( BSDTimeStructCopy *inTime, char *outString )
{
    sprintf( outString, "%d/%d/%d   %d:%d", 
                inTime->tm_mon + 1,
                inTime->tm_mday,
                inTime->tm_year + 1900,
                inTime->tm_hour,
                inTime->tm_min );
}

void LogTimeAsString( BSDTimeStructCopy *inTime, char *preStr )
{
    char timeStr[256];
    
    GetTimeAsString( inTime, timeStr );
    SRVLOG2( kLogMeta, "%s: %s", preStr, timeStr );
}

#else
#define LogTimeAsString(A,B)
#endif

//------------------------------------------------------------------------------------------------
//	TimeIsStale
//
//	Returns: Boolean value
//------------------------------------------------------------------------------------------------

int TimeIsStale( BSDTimeStructCopy *inTime )
{
    time_t theTime, theGMTime;
    
    // get GMT in seconds
    time(&theGMTime);
    
    // get user time in seconds
    theTime = timegm( (struct tm *)inTime );
    if ( theTime <= 0 )
		theTime = 0x7FFFFFFF;
	
#if DEBUGTIME
    LogTimeAsString( (BSDTimeStructCopy *)inTime, "pwrec" );
    SRVLOG1( kLogMeta, "theTime: %ld, %ld", theGMTime, theTime );
    SRVLOG1( kLogMeta, "diff: %ld", theGMTime - theTime );
#endif
    // broken for dates that push the signing bit?
    //return ( difftime( theGMTime, theTime ) > 0 );
    
    return ( (unsigned long)theGMTime > (unsigned long)theTime );
}


//------------------------------------------------------------------------------------------------
//	LoginTimeIsStale
//
//	Returns: Boolean value
//------------------------------------------------------------------------------------------------

int LoginTimeIsStale( BSDTimeStructCopy *inLastLogin, unsigned long inMaxMinutesOfNonUse )
{
    time_t theTime, theGMTime;
    long maxMinutesOfNonUse = (long)(inMaxMinutesOfNonUse & 0x7FFFFFFF);
	
    // get GMT in seconds
    time(&theGMTime);
    
    // get user time in seconds
    theTime = timegm( (struct tm *)inLastLogin );
//    if ( theTime <= 0 )
//		theTime = theGMTime;

#if DEBUGTIME
    LogTimeAsString( (BSDTimeStructCopy *)inLastLogin, "pwrec" );
    SRVLOG1( kLogMeta, "theGMTime, theTime: %ld, %ld", theGMTime, theTime );
    SRVLOG1( kLogMeta, "diff: %ld", ((theGMTime - theTime)/60) );
    SRVLOG1( kLogMeta, "min-nonuse: %ld", maxMinutesOfNonUse );
    SRVLOG1( kLogMeta, "returning: %d", ( ((theGMTime - theTime)/60) > maxMinutesOfNonUse ) );
#endif
    
    return ( ((theGMTime - theTime)/60) > maxMinutesOfNonUse );
}


//------------------------------------------------------------------------------------------------
//	PWGlobalAccessFeaturesToString
//
//	Prepares the PWGlobalAccessFeatures struct for transmission over our text-based protocol
//------------------------------------------------------------------------------------------------

void PWGlobalAccessFeaturesToString( PWGlobalAccessFeatures *inAccessFeatures, char *outString )
{
    char temp1Str[256];
    char temp2Str[256];
    char temp3Str[512];
	int historyValue = 0;
	
    if ( outString == NULL || inAccessFeatures == NULL )
        throw(-1);
    
    // Boolean values are stored in the struct as single bits. They must be unsigned for
    // display.
    
	if ( inAccessFeatures->usingHistory )
		historyValue = 1 + GlobalHistoryCount(*inAccessFeatures);
	
    sprintf( temp1Str, "%s=%d %s=%d %s=%d %s=%d %s=%d %s=%d ",
                kPWPolicyStr_usingHistory, historyValue,
				kPWPolicyStr_canModifyPasswordforSelf, (inAccessFeatures->noModifyPasswordforSelf == 0),
                kPWPolicyStr_usingExpirationDate, (inAccessFeatures->usingExpirationDate != 0),
                kPWPolicyStr_usingHardExpirationDate, (inAccessFeatures->usingHardExpirationDate != 0),
                kPWPolicyStr_requiresAlpha, (inAccessFeatures->requiresAlpha != 0),
                kPWPolicyStr_requiresNumeric, (inAccessFeatures->requiresNumeric != 0) );
                
    sprintf( temp2Str, "%s=%lu %s=%lu %s=%lu ",
                kPWPolicyStr_expirationDateGMT, timegm( (struct tm *)&inAccessFeatures->expirationDateGMT ),
                kPWPolicyStr_hardExpireDateGMT, timegm( (struct tm *)&inAccessFeatures->hardExpireDateGMT ),
                kPWPolicyStr_maxMinutesUntilChangePW, inAccessFeatures->maxMinutesUntilChangePassword );
    
    sprintf( temp3Str, "%s=%lu %s=%lu %s=%u %s=%u %s=%u %s=%d %s=%d %s=%d ",
                kPWPolicyStr_maxMinutesUntilDisabled, inAccessFeatures->maxMinutesUntilDisabled,
                kPWPolicyStr_maxMinutesOfNonUse, inAccessFeatures->maxMinutesOfNonUse,
                kPWPolicyStr_maxFailedLoginAttempts, inAccessFeatures->maxFailedLoginAttempts,
                kPWPolicyStr_minChars, inAccessFeatures->minChars,
                kPWPolicyStr_maxChars, inAccessFeatures->maxChars,
				kPWPolicyStr_passwordCannotBeName, (inAccessFeatures->passwordCannotBeName != 0),
				kPWPolicyStr_requiresMixedCase, (inAccessFeatures->requiresMixedCase != 0),
				kPWPolicyStr_newPasswordRequired, (inAccessFeatures->newPasswordRequired != 0) );
    
    strcpy( outString, temp1Str );
    strcat( outString, temp2Str );
    strcat( outString, temp3Str );
}


//------------------------------------------------------------------------------------------------
//	PWGlobalAccessFeaturesToStringExtra
//------------------------------------------------------------------------------------------------

void PWGlobalAccessFeaturesToStringExtra( PWGlobalAccessFeatures *inAccessFeatures, PWGlobalMoreAccessFeatures *inExtraFeatures, int inMaxLen, char *outString )
{
    char tempStr[2048];
	long len;
	
    if ( outString == NULL || inAccessFeatures == NULL )
        return;
	
	PWGlobalAccessFeaturesToString( inAccessFeatures, tempStr );
	len = snprintf( outString, inMaxLen, "%s", tempStr );
	if ( len >= inMaxLen )
		return;
	
	snprintf( outString + len, inMaxLen - len, "%s=%lu %s=%lu", 
				kPWPolicyStr_minutesUntilFailedLoginReset, inExtraFeatures->minutesUntilFailedLoginReset,
				kPWPolicyStr_notGuessablePattern, inExtraFeatures->notGuessablePattern );
}


//------------------------------------------------------------------------------------------------
//	PWAccessFeaturesToString
//
//	Prepares the PWAccessFeatures struct for transmission over our text-based protocol
//------------------------------------------------------------------------------------------------

void PWAccessFeaturesToString( PWAccessFeatures *inAccessFeatures, char *outString )
{
    char temp1Str[256];
    char temp2Str[2048];
    char temp3Str[64];
	
    if ( outString == NULL || inAccessFeatures == NULL )
        throw(-1);

    // Boolean values are stored in the struct as single bits. They must be unsigned for
    // display.
    
    snprintf( temp1Str, sizeof(temp1Str), "%s=%d %s=%d %s=%d ",
                kPWPolicyStr_isDisabled, (inAccessFeatures->isDisabled != 0),
                kPWPolicyStr_isAdminUser, (inAccessFeatures->isAdminUser != 0),
                kPWPolicyStr_newPasswordRequired, (inAccessFeatures->newPasswordRequired != 0) );
    
	PWAccessFeaturesToStringWithoutStateInfo( inAccessFeatures, temp2Str );
	
	snprintf( temp3Str, sizeof(temp3Str), " %s=%d",
				kPWPolicyStr_isSessionKeyAgent, (inAccessFeatures->isSessionKeyAgent != 0) );
    
    strcpy( outString, temp1Str );
    strcat( outString, temp2Str );
    strcat( outString, temp3Str );
}


//------------------------------------------------------------------------------------------------
//	PWAccessFeaturesToStringExtra
//
//	Prepares the PWAccessFeatures struct for transmission over our text-based protocol
//------------------------------------------------------------------------------------------------

void PWAccessFeaturesToStringExtra( PWAccessFeatures *inAccessFeatures, PWMoreAccessFeatures *inExtraFeatures, int inMaxLen, char *outString )
{
    char temp2Str[2048];
	
    if ( outString == NULL || inAccessFeatures == NULL )
        return;
	
    // Boolean values are stored in the struct as single bits. They must be unsigned for
    // display.
    
    int segment1Length = snprintf( outString, inMaxLen, "%s=%d %s=%d %s=%d ",
							kPWPolicyStr_isDisabled, (inAccessFeatures->isDisabled != 0),
							kPWPolicyStr_isAdminUser, (inAccessFeatures->isAdminUser != 0),
							kPWPolicyStr_newPasswordRequired, (inAccessFeatures->newPasswordRequired != 0) );
	inMaxLen -= segment1Length;
	
	PWAccessFeaturesToStringWithoutStateInfoExtra( inAccessFeatures, inExtraFeatures, sizeof(temp2Str), temp2Str );
	int segment2Length = snprintf( outString + segment1Length, inMaxLen, "%s", temp2Str );
	inMaxLen -= segment2Length;
	
	snprintf( outString + segment1Length + segment2Length, inMaxLen, " %s=%d",
				kPWPolicyStr_isSessionKeyAgent, (inAccessFeatures->isSessionKeyAgent != 0) );
}


//------------------------------------------------------------------------------------------------
//	PWActualAccessFeaturesToString
//
//	Prepares the PWAccessFeatures struct and PWGlobalAccessFeatures defaults for transmission
//	over our text-based protocol
//------------------------------------------------------------------------------------------------

void PWActualAccessFeaturesToString( PWGlobalAccessFeatures *inGAccessFeatures, PWAccessFeatures *inAccessFeatures, char *outString )
{
	int historyValue = 0;
	int usingExpirationDate;
    int usingHardExpirationDate;
    int requiresAlpha;
    int requiresNumeric;
	int passwordCannotBeName;
	UInt32 maxMinutesUntilChangePassword;
    UInt32 maxMinutesUntilDisabled;
    UInt32 maxMinutesOfNonUse;
    UInt16 maxFailedLoginAttempts;
    UInt16 minChars;
    UInt16 maxChars;
	
	// TODO: get the actual time values for expiration dates
	
	char temp1Str[256];
    char temp2Str[256];
    char temp3Str[256];
    char temp4Str[512];
	
    if ( outString == NULL || inAccessFeatures == NULL )
        throw(-1);

	// get values for policies that can be in either the user record
	// or the global record
	usingExpirationDate = (inAccessFeatures->usingExpirationDate != 0);
	if ( usingExpirationDate == 0 )
		usingExpirationDate = (inGAccessFeatures->usingExpirationDate != 0);
	
	usingHardExpirationDate = (inAccessFeatures->usingHardExpirationDate != 0);
	if ( usingHardExpirationDate == 0 )
		usingHardExpirationDate = (inGAccessFeatures->usingHardExpirationDate != 0);
	
	requiresAlpha = (inAccessFeatures->requiresAlpha != 0);
	if ( requiresAlpha == 0 )
		requiresAlpha = (inGAccessFeatures->requiresAlpha != 0);
	
	requiresNumeric = (inAccessFeatures->requiresNumeric != 0);
	if ( requiresNumeric == 0 )
		requiresNumeric = (inGAccessFeatures->requiresNumeric != 0);

	passwordCannotBeName = (inAccessFeatures->passwordCannotBeName != 0);
	if ( passwordCannotBeName == 0 )
		passwordCannotBeName = (inGAccessFeatures->passwordCannotBeName != 0);
	
	maxMinutesUntilChangePassword = inAccessFeatures->maxMinutesUntilChangePassword;
	if ( maxMinutesUntilChangePassword == 0 )
		maxMinutesUntilChangePassword = inGAccessFeatures->maxMinutesUntilChangePassword;
	
	maxMinutesUntilDisabled = inAccessFeatures->maxMinutesUntilDisabled;
	if ( maxMinutesUntilDisabled == 0 )
		maxMinutesUntilDisabled = inGAccessFeatures->maxMinutesUntilDisabled;
	
	maxMinutesOfNonUse = inAccessFeatures->maxMinutesOfNonUse;
	if ( maxMinutesOfNonUse == 0 )
		maxMinutesOfNonUse = inGAccessFeatures->maxMinutesOfNonUse;
	
	maxFailedLoginAttempts = inAccessFeatures->maxFailedLoginAttempts;
	if ( maxFailedLoginAttempts == 0 )
		maxFailedLoginAttempts = inGAccessFeatures->maxFailedLoginAttempts;
	
	minChars = inAccessFeatures->minChars;
	if ( minChars == 0 )
		minChars = inGAccessFeatures->minChars;
	
	maxChars = inAccessFeatures->maxChars;
	if ( maxChars == 0 )
		maxChars = inGAccessFeatures->maxChars;
	
	
    // Boolean values are stored in the struct as single bits. They must be unsigned for
    // display.
    
	if ( inAccessFeatures->usingHistory )
		historyValue = 1 + inAccessFeatures->historyCount;
	else
	if ( inGAccessFeatures->usingHistory )
		historyValue = 1 + GlobalHistoryCount(*inGAccessFeatures);
	
    sprintf( temp1Str, "%s=%d %s=%d %s=%d %s=%d ",
                kPWPolicyStr_isDisabled, (inAccessFeatures->isDisabled != 0),
                kPWPolicyStr_isAdminUser, (inAccessFeatures->isAdminUser != 0),
                kPWPolicyStr_newPasswordRequired, (inAccessFeatures->newPasswordRequired != 0),
                kPWPolicyStr_usingHistory, historyValue );
    
	sprintf( temp2Str, "%s=%d %s=%d %s=%d %s=%d ",
                kPWPolicyStr_canModifyPasswordforSelf, (inAccessFeatures->canModifyPasswordforSelf != 0),
                kPWPolicyStr_usingExpirationDate, usingExpirationDate,
                kPWPolicyStr_usingHardExpirationDate, usingHardExpirationDate,
                kPWPolicyStr_requiresAlpha, requiresAlpha );
    
    sprintf( temp3Str, "%s=%d %s=%lu %s=%lu ",
                kPWPolicyStr_requiresNumeric, requiresNumeric,
                kPWPolicyStr_expirationDateGMT, timegm( (struct tm *)&inAccessFeatures->expirationDateGMT ),
                kPWPolicyStr_hardExpireDateGMT, timegm( (struct tm *)&inAccessFeatures->hardExpireDateGMT ) );
    
    snprintf( temp4Str, sizeof(temp4Str), "%s=%lu %s=%lu %s=%lu %s=%u %s=%u %s=%u %s=%d %s=%d",
                kPWPolicyStr_maxMinutesUntilChangePW, maxMinutesUntilChangePassword,
                kPWPolicyStr_maxMinutesUntilDisabled, maxMinutesUntilDisabled,
                kPWPolicyStr_maxMinutesOfNonUse, maxMinutesOfNonUse,
                kPWPolicyStr_maxFailedLoginAttempts, maxFailedLoginAttempts,
                kPWPolicyStr_minChars, minChars,
                kPWPolicyStr_maxChars, maxChars,
				kPWPolicyStr_passwordCannotBeName, passwordCannotBeName,
				kPWPolicyStr_isSessionKeyAgent, (inAccessFeatures->isSessionKeyAgent != 0) );
    
    strcpy( outString, temp1Str );
    strcat( outString, temp2Str );
    strcat( outString, temp3Str );
    strcat( outString, temp4Str );
}


//------------------------------------------------------------------------------------------------
//	PWActualAccessFeaturesToStringExtra
//
//	Prepares the PWAccessFeatures struct and PWGlobalAccessFeatures defaults for transmission
//	over our text-based protocol
//------------------------------------------------------------------------------------------------

void PWActualAccessFeaturesToStringExtra( PWGlobalAccessFeatures *inGAccessFeatures, PWAccessFeatures *inAccessFeatures, PWMoreAccessFeatures *inExtraFeatures, int inMaxLen, char *outString )
{
	int segment1Length = 0;
	unsigned int requiresMixedCase = (inExtraFeatures->requiresMixedCase || inGAccessFeatures->requiresMixedCase);
	unsigned long notGuessablePattern = inExtraFeatures->notGuessablePattern;
	
	if ( inMaxLen > 1280 )
	{
		PWActualAccessFeaturesToString( inGAccessFeatures, inAccessFeatures, outString );
		segment1Length = strlen( outString );
	}
	else
	{
		char tempStr[1280];
		
		PWActualAccessFeaturesToString( inGAccessFeatures, inAccessFeatures, tempStr );
		segment1Length = snprintf( outString, inMaxLen, "%s", tempStr );
	}
	inMaxLen -= segment1Length;
	
	if ( notGuessablePattern == 0 )
		notGuessablePattern = 0/*inGAccessFeatures->notGuessablePattern*/;
	
	snprintf( outString + segment1Length, inMaxLen, " %s=%d, %s=%lu",
			kPWPolicyStr_requiresMixedCase, (requiresMixedCase != 0),
			kPWPolicyStr_notGuessablePattern, notGuessablePattern );
}


//------------------------------------------------------------------------------------------------
//	PWAccessFeaturesToStringWithoutStateInfo
//
//	Prepares the PWAccessFeatures struct for transmission over our text-based protocol
//  Returns in <outString> the subset of policies that are true policies and not state
//  information, such as: isDisabled, isAdminUser, and newPasswordRequired.
//------------------------------------------------------------------------------------------------

void PWAccessFeaturesToStringWithoutStateInfo( PWAccessFeatures *inAccessFeatures, char *outString )
{
	int historyValue = 0;
	
    if ( outString == NULL || inAccessFeatures == NULL )
        throw(-1);
	
    // Boolean values are stored in the struct as single bits. They must be unsigned for
    // display.
    
	if ( inAccessFeatures->usingHistory )
		historyValue = 1 + inAccessFeatures->historyCount;
	
    snprintf( outString, 2048,
				"%s=%d %s=%d %s=%d %s=%d %s=%d %s=%d %s=%lu %s=%lu %s=%lu %s=%lu %s=%lu %s=%u %s=%u %s=%u %s=%d",
				kPWPolicyStr_usingHistory, historyValue,
				kPWPolicyStr_canModifyPasswordforSelf, (inAccessFeatures->canModifyPasswordforSelf != 0),
                kPWPolicyStr_usingExpirationDate, (inAccessFeatures->usingExpirationDate != 0),
                kPWPolicyStr_usingHardExpirationDate, (inAccessFeatures->usingHardExpirationDate != 0),
                kPWPolicyStr_requiresAlpha, (inAccessFeatures->requiresAlpha != 0),
				kPWPolicyStr_requiresNumeric, (inAccessFeatures->requiresNumeric != 0),
                kPWPolicyStr_expirationDateGMT, timegm( (struct tm *)&inAccessFeatures->expirationDateGMT ),
                kPWPolicyStr_hardExpireDateGMT, timegm( (struct tm *)&inAccessFeatures->hardExpireDateGMT ),
                kPWPolicyStr_maxMinutesUntilChangePW, inAccessFeatures->maxMinutesUntilChangePassword,
                kPWPolicyStr_maxMinutesUntilDisabled, inAccessFeatures->maxMinutesUntilDisabled,
                kPWPolicyStr_maxMinutesOfNonUse, inAccessFeatures->maxMinutesOfNonUse,
                kPWPolicyStr_maxFailedLoginAttempts, inAccessFeatures->maxFailedLoginAttempts,
                kPWPolicyStr_minChars, inAccessFeatures->minChars,
                kPWPolicyStr_maxChars, inAccessFeatures->maxChars,
				kPWPolicyStr_passwordCannotBeName, (inAccessFeatures->passwordCannotBeName != 0) );
}


//------------------------------------------------------------------------------------------------
//	PWAccessFeaturesToStringWithoutStateInfoExtra
//
//	Prepares the policy data for transmission over our text-based protocol.
//  Returns in <outString> the subset of policies that are true policies and not state
//  information, such as: isDisabled, isAdminUser, and newPasswordRequired.
//------------------------------------------------------------------------------------------------

void PWAccessFeaturesToStringWithoutStateInfoExtra( PWAccessFeatures *inAccessFeatures, PWMoreAccessFeatures *inExtraFeatures, int inMaxLen, char *outString )
{
	if ( inMaxLen >= 2048 )
	{
		PWAccessFeaturesToStringWithoutStateInfo( inAccessFeatures, outString );
	}
	else
	{
		char tempStr[2048];
		
		PWAccessFeaturesToStringWithoutStateInfo( inAccessFeatures, tempStr );
		snprintf( outString, inMaxLen, "%s", tempStr );
	}
	
	long firstSegmentLen = strlen( outString );
	inMaxLen -= firstSegmentLen;
	if ( inMaxLen > 0 )
	{
		snprintf( outString + firstSegmentLen, inMaxLen,
			" %s=%d %s=%lu",
			kPWPolicyStr_requiresMixedCase, (inExtraFeatures->requiresMixedCase != 0),
			kPWPolicyStr_notGuessablePattern, inExtraFeatures->notGuessablePattern );
	}
}


//------------------------------------------------------------------------------------------------
//	StringToPWGlobalAccessFeatures
//
//	Returns: TRUE if the string is successfully parsed
//
//	Features specified in the string overwrite features in <inOutAccessFeatures>. Features
//	not specified in the string remain as-is. If they were undefined before, they are undefined
//	on exit.
//------------------------------------------------------------------------------------------------

Boolean StringToPWGlobalAccessFeatures( const char *inString, PWGlobalAccessFeatures *inOutAccessFeatures )
{
    const char *usingHistory = strstr( inString, kPWPolicyStr_usingHistory );
	const char *canModifyPasswordforSelf = strstr( inString, kPWPolicyStr_canModifyPasswordforSelf );
    const char *usingExpirationDate = strstr( inString, kPWPolicyStr_usingExpirationDate );
    const char *usingHardExpirationDate = strstr( inString, kPWPolicyStr_usingHardExpirationDate );
    const char *requiresAlpha = strstr( inString, kPWPolicyStr_requiresAlpha );
    const char *requiresNumeric = strstr( inString, kPWPolicyStr_requiresNumeric );
    const char *expirationDateGMT = strstr( inString, kPWPolicyStr_expirationDateGMT );
    const char *hardExpireDateGMT = strstr( inString, kPWPolicyStr_hardExpireDateGMT );
    const char *maxMinutesUntilChangePassword = strstr( inString, kPWPolicyStr_maxMinutesUntilChangePW );
    const char *maxMinutesUntilDisabled = strstr( inString, kPWPolicyStr_maxMinutesUntilDisabled );
    const char *maxMinutesOfNonUse = strstr( inString, kPWPolicyStr_maxMinutesOfNonUse );
    const char *maxFailedLoginAttempts = strstr( inString, kPWPolicyStr_maxFailedLoginAttempts );
    const char *minChars = strstr( inString, kPWPolicyStr_minChars );
    const char *maxChars = strstr( inString, kPWPolicyStr_maxChars );
    const char *passwordCannotBeName = strstr( inString, kPWPolicyStr_passwordCannotBeName );
    const char *requiresMixedCase = strstr( inString, kPWPolicyStr_requiresMixedCase );
    const char *newPasswordRequired = strstr( inString, kPWPolicyStr_newPasswordRequired );
    unsigned long value;
    
    if ( StringToPWAccessFeatures_GetValue( usingHistory, &value ) )
	{
		if ( value > 0 )
		{
			// clamp to the password server's maximum value
			if ( value > kPWFileMaxHistoryCount )
				value = kPWFileMaxHistoryCount;
			
			inOutAccessFeatures->usingHistory = 1;
			SetGlobalHistoryCount(*inOutAccessFeatures, value - 1);
		}
		else
		{
			inOutAccessFeatures->usingHistory = 0;
			SetGlobalHistoryCount(*inOutAccessFeatures, 0);
		}
    }
	
	if ( StringToPWAccessFeatures_GetValue( canModifyPasswordforSelf, &value ) )
		inOutAccessFeatures->noModifyPasswordforSelf = (value==0);
	
    if ( StringToPWAccessFeatures_GetValue( usingExpirationDate, &value ) )
        inOutAccessFeatures->usingExpirationDate = value;
    
    if ( StringToPWAccessFeatures_GetValue( usingHardExpirationDate, &value ) )
        inOutAccessFeatures->usingHardExpirationDate = value;
    
    if ( StringToPWAccessFeatures_GetValue( requiresAlpha, &value ) )
        inOutAccessFeatures->requiresAlpha = value;
    
    if ( StringToPWAccessFeatures_GetValue( requiresNumeric, &value ) )
        inOutAccessFeatures->requiresNumeric = value;
    
    if ( StringToPWAccessFeatures_GetValue( expirationDateGMT, &value ) )
		gmtime_r( (time_t *)&value, (struct tm *)&inOutAccessFeatures->expirationDateGMT );
    
    if ( StringToPWAccessFeatures_GetValue( hardExpireDateGMT, &value ) )
		gmtime_r( (time_t *)&value, (struct tm *)&inOutAccessFeatures->hardExpireDateGMT );
	
    if ( StringToPWAccessFeatures_GetValue( maxMinutesUntilChangePassword, &value ) )
        inOutAccessFeatures->maxMinutesUntilChangePassword = value;
    
    if ( StringToPWAccessFeatures_GetValue( maxMinutesUntilDisabled, &value ) )
        inOutAccessFeatures->maxMinutesUntilDisabled = value;
    
    if ( StringToPWAccessFeatures_GetValue( maxMinutesOfNonUse, &value ) )
        inOutAccessFeatures->maxMinutesOfNonUse = value;
    
    if ( StringToPWAccessFeatures_GetValue( maxFailedLoginAttempts, &value ) )
        inOutAccessFeatures->maxFailedLoginAttempts = (UInt16)value;
    
    if ( StringToPWAccessFeatures_GetValue( minChars, &value ) )
        inOutAccessFeatures->minChars = value;
        
    if ( StringToPWAccessFeatures_GetValue( maxChars, &value ) )
        inOutAccessFeatures->maxChars = value;
        
	if ( StringToPWAccessFeatures_GetValue( passwordCannotBeName, &value ) )
		inOutAccessFeatures->passwordCannotBeName = value;
	
	if ( StringToPWAccessFeatures_GetValue( requiresMixedCase, &value ) )
		inOutAccessFeatures->requiresMixedCase = value;
	
	if ( StringToPWAccessFeatures_GetValue( newPasswordRequired, &value ) )
		inOutAccessFeatures->newPasswordRequired = value;
	
    // no checking for now
    return true;
}


//------------------------------------------------------------------------------------------------
//	StringToPWGlobalAccessFeaturesExtra
//
//	Returns: TRUE if the string is successfully parsed
//------------------------------------------------------------------------------------------------

Boolean StringToPWGlobalAccessFeaturesExtra( const char *inString, PWGlobalAccessFeatures *inOutAccessFeatures, PWGlobalMoreAccessFeatures *inOutExtraFeatures )
{
	Boolean result = StringToPWGlobalAccessFeatures( inString, inOutAccessFeatures );
	const char *notGuessablePattern = strstr( inString, kPWPolicyStr_notGuessablePattern );
	const char *minutesUntilFailedLoginReset = strstr( inString, kPWPolicyStr_minutesUntilFailedLoginReset );
    unsigned long value;
	
	if ( StringToPWAccessFeatures_GetValue( notGuessablePattern, &value ) )
		inOutExtraFeatures->notGuessablePattern = value;
		
	if ( StringToPWAccessFeatures_GetValue( minutesUntilFailedLoginReset, &value ) )
		inOutExtraFeatures->minutesUntilFailedLoginReset = value;
	
	return result;
}


//------------------------------------------------------------------------------------------------
//	StringToPWAccessFeatures
//
//	Returns: TRUE if the string is successfully parsed
//
//	Features specified in the string overwrite features in <inOutAccessFeatures>. Features
//	not specified in the string remain as-is. If they were undefined before, they are undefined
//	on exit.
//------------------------------------------------------------------------------------------------

Boolean StringToPWAccessFeatures( const char *inString, PWAccessFeatures *inOutAccessFeatures )
{
    const char *isDisabled = strstr( inString, kPWPolicyStr_isDisabled );
    const char *isAdminUser = strstr( inString, kPWPolicyStr_isAdminUser );
    const char *newPasswordRequired = strstr( inString, kPWPolicyStr_newPasswordRequired );
    const char *usingHistory = strstr( inString, kPWPolicyStr_usingHistory );
    const char *canModifyPasswordforSelf = strstr( inString, kPWPolicyStr_canModifyPasswordforSelf );
    const char *usingExpirationDate = strstr( inString, kPWPolicyStr_usingExpirationDate );
    const char *usingHardExpirationDate = strstr( inString, kPWPolicyStr_usingHardExpirationDate );
    const char *requiresAlpha = strstr( inString, kPWPolicyStr_requiresAlpha );
    const char *requiresNumeric = strstr( inString, kPWPolicyStr_requiresNumeric );
    const char *expirationDateGMT = strstr( inString, kPWPolicyStr_expirationDateGMT );
    const char *hardExpireDateGMT = strstr( inString, kPWPolicyStr_hardExpireDateGMT );
    const char *maxMinutesUntilChangePassword = strstr( inString, kPWPolicyStr_maxMinutesUntilChangePW );
    const char *maxMinutesUntilDisabled = strstr( inString, kPWPolicyStr_maxMinutesUntilDisabled );
    const char *maxMinutesOfNonUse = strstr( inString, kPWPolicyStr_maxMinutesOfNonUse );
    const char *maxFailedLoginAttempts = strstr( inString, kPWPolicyStr_maxFailedLoginAttempts );
    const char *minChars = strstr( inString, kPWPolicyStr_minChars );
    const char *maxChars = strstr( inString, kPWPolicyStr_maxChars );
    const char *passwordCannotBeName = strstr( inString, kPWPolicyStr_passwordCannotBeName );
	const char *isSessionKeyAgent = strstr( inString, kPWPolicyStr_isSessionKeyAgent );
	const char *resetToGlobalDefaults = strstr( inString, kPWPolicyStr_resetToGlobalDefaults );
    unsigned long value;
    
    if ( StringToPWAccessFeatures_GetValue( isDisabled, &value ) )
        inOutAccessFeatures->isDisabled = value;
    
    if ( StringToPWAccessFeatures_GetValue( isAdminUser, &value ) )
        inOutAccessFeatures->isAdminUser = value;
    
    if ( StringToPWAccessFeatures_GetValue( newPasswordRequired, &value ) )
        inOutAccessFeatures->newPasswordRequired = value;
    
    if ( StringToPWAccessFeatures_GetValue( usingHistory, &value ) )
	{
		if ( value > 0 )
		{
			// clamp to the password server's maximum value
			if ( value > kPWFileMaxHistoryCount )
				value = kPWFileMaxHistoryCount;
			
			inOutAccessFeatures->usingHistory = 1;
			inOutAccessFeatures->historyCount = value - 1;
		}
		else
		{
			inOutAccessFeatures->usingHistory = 0;
			inOutAccessFeatures->historyCount = 0;
		}
    }
	
    if ( StringToPWAccessFeatures_GetValue( canModifyPasswordforSelf, &value ) )
        inOutAccessFeatures->canModifyPasswordforSelf = value;
    
    if ( StringToPWAccessFeatures_GetValue( usingExpirationDate, &value ) )
        inOutAccessFeatures->usingExpirationDate = value;
    
    if ( StringToPWAccessFeatures_GetValue( usingHardExpirationDate, &value ) )
        inOutAccessFeatures->usingHardExpirationDate = value;
    
    if ( StringToPWAccessFeatures_GetValue( requiresAlpha, &value ) )
        inOutAccessFeatures->requiresAlpha = value;
    
    if ( StringToPWAccessFeatures_GetValue( requiresNumeric, &value ) )
        inOutAccessFeatures->requiresNumeric = value;
    
    if ( StringToPWAccessFeatures_GetValue( expirationDateGMT, &value ) )
		gmtime_r( (time_t *)&value, (struct tm *)&inOutAccessFeatures->expirationDateGMT );
    
    if ( StringToPWAccessFeatures_GetValue( hardExpireDateGMT, &value ) )
		gmtime_r( (time_t *)&value, (struct tm *)&inOutAccessFeatures->hardExpireDateGMT );
	
	if ( StringToPWAccessFeatures_GetValue( maxMinutesUntilChangePassword, &value ) )
        inOutAccessFeatures->maxMinutesUntilChangePassword = value;
    
    if ( StringToPWAccessFeatures_GetValue( maxMinutesUntilDisabled, &value ) )
        inOutAccessFeatures->maxMinutesUntilDisabled = value;
    
    if ( StringToPWAccessFeatures_GetValue( maxMinutesOfNonUse, &value ) )
        inOutAccessFeatures->maxMinutesOfNonUse = value;
    
    if ( StringToPWAccessFeatures_GetValue( maxFailedLoginAttempts, &value ) )
        inOutAccessFeatures->maxFailedLoginAttempts = (UInt16)value;
    
    if ( StringToPWAccessFeatures_GetValue( minChars, &value ) )
        inOutAccessFeatures->minChars = value;
        
    if ( StringToPWAccessFeatures_GetValue( maxChars, &value ) )
        inOutAccessFeatures->maxChars = value;
	
	if ( StringToPWAccessFeatures_GetValue( passwordCannotBeName, &value ) )
		inOutAccessFeatures->passwordCannotBeName = value;
	
	if ( StringToPWAccessFeatures_GetValue( isSessionKeyAgent, &value ) )
		inOutAccessFeatures->isSessionKeyAgent = value;
	
	// this policy must be processed last
	if ( StringToPWAccessFeatures_GetValue( resetToGlobalDefaults, &value ) && value > 0 )
	{
		inOutAccessFeatures->usingHistory = 0;
		inOutAccessFeatures->canModifyPasswordforSelf = 1;
		inOutAccessFeatures->usingExpirationDate = 0;
		inOutAccessFeatures->usingHardExpirationDate = 0;
		inOutAccessFeatures->requiresAlpha = 0;
		inOutAccessFeatures->requiresNumeric = 0;
		inOutAccessFeatures->passwordCannotBeName = 0;
		inOutAccessFeatures->historyCount = 0;
		inOutAccessFeatures->maxMinutesUntilChangePassword = 0;
		inOutAccessFeatures->maxMinutesUntilDisabled = 0;
		inOutAccessFeatures->maxMinutesOfNonUse = 0;
		inOutAccessFeatures->maxFailedLoginAttempts = 0;
		inOutAccessFeatures->minChars = 0;
		inOutAccessFeatures->maxChars = 0;
	}
	
    // no checking for now
    return true;
}


//------------------------------------------------------------------------------------------------
//	StringToPWAccessFeaturesExtra
//
//	Returns: TRUE if the string is successfully parsed
//
//	Features specified in the string overwrite features in <inOutAccessFeatures>. Features
//	not specified in the string remain as-is. If they were undefined before, they are undefined
//	on exit.
//------------------------------------------------------------------------------------------------

Boolean StringToPWAccessFeaturesExtra( const char *inString, PWAccessFeatures *inOutAccessFeatures, PWMoreAccessFeatures *inOutExtraFeatures )
{
	const char *requiresMixedCase = strstr( inString, kPWPolicyStr_requiresMixedCase );
	const char *notGuessablePattern = strstr( inString, kPWPolicyStr_notGuessablePattern );
	const char *resetToGlobalDefaults = strstr( inString, kPWPolicyStr_resetToGlobalDefaults );
	unsigned long value;
    
	if ( inOutExtraFeatures == NULL )
		return false;
	
	Boolean result = StringToPWAccessFeatures( inString, inOutAccessFeatures );
	if ( result )
	{   
		if ( StringToPWAccessFeatures_GetValue( requiresMixedCase, &value ) )
			inOutExtraFeatures->requiresMixedCase = value;
		
		if ( StringToPWAccessFeatures_GetValue( notGuessablePattern, &value ) )
			inOutExtraFeatures->notGuessablePattern = value;
		
		if ( StringToPWAccessFeatures_GetValue( resetToGlobalDefaults, &value ) && value > 0 )
		{
			inOutExtraFeatures->requiresMixedCase = 0;
			inOutExtraFeatures->notGuessablePattern = 0;
		}
		
		// no checking for now
		result = true;
	}
	
	return result;
}


//------------------------------------------------------------------------------------------------
//	StringToPWAccessFeatures_GetValue
//
//	Returns: TRUE if a value is discovered
//
//	Takes a value like, "isDisabled=0" and returns the "0" in <outValue>.
//	If <inString> is NULL, the function just returns false.
//------------------------------------------------------------------------------------------------

Boolean StringToPWAccessFeatures_GetValue( const char *inString, unsigned long *outValue )
{
    const char *valueStr;
    char valBuffer[64];
    unsigned int idx = 0;
    
    if ( inString == NULL )
        return false;
    
	valueStr = strchr( inString, '=' );
    if ( valueStr == NULL )
        return false;
    
    valueStr++;
   	while ( *valueStr && *valueStr > ' ' && idx < sizeof(valBuffer) - 1 )
        valBuffer[idx++] = *valueStr++;
    valBuffer[idx] = '\0';
    
    if ( idx <= 0 )
        return false;
    
    sscanf( valBuffer, "%lu", outValue ); 
    
    return true;
}


//------------------------------------------------------------------------------------------------
//	CrashIfBuiltWrong
//
//	Compares the current database struct sizes to our hand-checked constant.
//	If the size changes, don't run.
//------------------------------------------------------------------------------------------------

void CrashIfBuiltWrong(void)
{
    if ( sizeof(PWFileEntry) != 4360 ||
         sizeof(PWAccessFeatures) != 112 || 
         sizeof(PWFileHeader) != 4768 || 
         sizeof(BSDTimeStructCopy) != 44 )
    {
        fprintf( stderr, "PasswordServer has been built wrong!!!\n" );
        fprintf( stderr, "DO NOT SHIP THIS PRODUCT\n" );
        
        fprintf( stderr, "PWFileEntry=%ld\n", sizeof(PWFileEntry) );
        fprintf( stderr, "PWAccessFeatures=%ld\n", sizeof(PWAccessFeatures) );
        fprintf( stderr, "PWFileHeader=%ld\n", sizeof(PWFileHeader) );
        fprintf( stderr, "BSDTimeStructCopy=%ld\n", sizeof(BSDTimeStructCopy) );
        
        char *someBadAddress = (char *)0xFFFFFFFF;
        someBadAddress[0] = 0;
    }
}


//------------------------------------------------------------------------------------------------
//	pwsf_PreserveUnrepresentedPolicies
//
//	concatenates policies that are not in one of the policy structs
//------------------------------------------------------------------------------------------------

void pwsf_PreserveUnrepresentedPolicies( const char *inOriginalStr, int inMaxLen, char *inOutString )
{
	if ( inOriginalStr == NULL || inOutString == NULL )
		return;
	
	const char *warnOfExpirationMinutes = strstr( inOriginalStr, kPWPolicyStr_warnOfExpirationMinutes );
	const char *warnOfDisableMinutes = strstr( inOriginalStr, kPWPolicyStr_warnOfDisableMinutes );
	const char *endPtr = NULL;
	long curLen = strlen( inOutString );
	
	if ( warnOfExpirationMinutes != NULL )
	{
		endPtr = strchr( warnOfExpirationMinutes, ' ' );
		if ( endPtr != NULL )
			strlcat( inOutString, 
					 warnOfExpirationMinutes,
					 MIN(curLen + (endPtr - warnOfExpirationMinutes) + 1, inMaxLen) );
		else
			strlcat( inOutString, warnOfExpirationMinutes, inMaxLen );
		
		curLen = strlen( inOutString );
	}
	
	if ( warnOfDisableMinutes != NULL )
	{
		endPtr = strchr( warnOfDisableMinutes, ' ' );
		if ( endPtr != NULL )
			strlcat( inOutString,
					 warnOfDisableMinutes, 
					 MIN(curLen + (endPtr - warnOfDisableMinutes) + 1, inMaxLen) );
		else
			strlcat( inOutString, warnOfDisableMinutes, inMaxLen );
	}
}


//------------------------------------------------------------------------------------------------
//	pwsf_GetPublicKey
//
//  RETURNS: 0=ok, -1 fail
//
//	Retrieves the RSA public key from the password server database.
//  <outPublicKey> must be a pointer to a buffer at least kPWFileMaxPublicKeyBytes long.
//------------------------------------------------------------------------------------------------

int pwsf_GetPublicKey(char *outPublicKey)
{
	int err = -1;
	CAuthFileBase authFile;
		
	err = authFile.validateFiles();
	if ( err == 0 )
		err = authFile.getRSAPublicKey( outPublicKey );
	
	return err;
}


//------------------------------------------------------------------------------------------------
//	pwsf_GetPublicKeyFromFile
//
//  RETURNS: 0=ok, -1 fail
//
//	Retrieves the RSA public key from the password server database.
//  <outPublicKey> must be a pointer to a buffer at least kPWFileMaxPublicKeyBytes long.
//------------------------------------------------------------------------------------------------

int pwsf_GetPublicKeyFromFile(const char *inFile, char *outPublicKey)
{
	int err = -1;
	CAuthFileBase authFile(inFile);
	
	err = authFile.validateFiles();
	if ( err == 0 )
		err = authFile.getRSAPublicKey( outPublicKey );
	
	return err;
}


//------------------------------------------------------------------------------------------------
//	pwsf_CreateReplicaFile
//
//  Creates the '/var/db/authserver/authserverreplicas' file.
//  Client must be running as root.
//------------------------------------------------------------------------------------------------

void pwsf_CreateReplicaFile( const char *inIPStr, const char *inDNSStr, const char *inPublicKey )
{
	struct stat sb;
	
	if ( stat(kPWReplicaFile, &sb) == 0 )
	{
		// replace the existing file
		remove( kPWReplicaFile );
	}
	
	{
		CReplicaFile replicaFile;
		replicaFile.SetParent( inIPStr, (inDNSStr != NULL && inDNSStr[0] != '\0') ? inDNSStr : NULL );
		
		if ( inPublicKey != NULL )
		{
			replicaFile.AddServerUniqueID( inPublicKey );
		}
		else
		{
			CAuthFileBase authFile;
			char rsaKey[kPWFileMaxPublicKeyBytes];
			
			if ( authFile.getRSAPublicKey( rsaKey ) == 0 )
				replicaFile.AddServerUniqueID( rsaKey );
		}
		
		replicaFile.SetReplicaPolicy( kReplicaAllowAll );
		replicaFile.SaveXMLData();
	}
}


//------------------------------------------------------------------------------------------------
//	pwsf_ResetReplicaFile
//
//  Resets the '/var/db/authserver/authserverreplicas' file to have no replicas.
//  Client must be running as root.
//------------------------------------------------------------------------------------------------

void pwsf_ResetReplicaFile( const char *inPublicKey )
{
	CFStringRef firstIPAddressString;
	CFStringRef dnsString;
	CFMutableArrayRef ipAddressArray = NULL;
	CFIndex ipAddressCount = 0;
	CReplicaFile replicaFile;
	CFMutableDictionaryRef serverDict = (CFMutableDictionaryRef) replicaFile.GetParent();
	char ipStr[256] = {0,};
	char dnsStr[256] = {0,};
	
	strcpy( ipStr, "0.0.0.0" );
	
	if ( serverDict != NULL )
	{
		// fetch data to preserve
		ipAddressArray = replicaFile.GetIPAddresses( serverDict );
		if ( ipAddressArray != NULL )
		{
			ipAddressCount = CFArrayGetCount( ipAddressArray );
			firstIPAddressString = (CFStringRef) CFArrayGetValueAtIndex( ipAddressArray, 0 );
			if ( firstIPAddressString != NULL )
				CFStringGetCString( firstIPAddressString, ipStr, sizeof(ipStr), kCFStringEncodingUTF8 );
		}
		
		if ( CFDictionaryGetValueIfPresent(serverDict, CFSTR(kPWReplicaDNSKey), (const void **)&dnsString) )
			CFStringGetCString( dnsString, dnsStr, sizeof(dnsStr), kCFStringEncodingUTF8 );
	}
	
	// replace the replica file
	pwsf_CreateReplicaFile( ipStr, dnsStr, inPublicKey );
	
	// restore IP list (if > 1 address)
	if ( ipAddressCount > 1 )
	{
		CReplicaFile newReplicaFile;
		serverDict = (CFMutableDictionaryRef) newReplicaFile.GetParent();
		if ( serverDict != NULL )
		{
			newReplicaFile.AddOrReplaceValue( serverDict, CFSTR(kPWReplicaIPKey), ipAddressArray );
			newReplicaFile.SetParent( serverDict );
			newReplicaFile.SaveXMLData();
		}
	}
}


// ----------------------------------------------------------------------------------------
//  pwsf_GetPrincName
//
//  Returns: the kerberos principal name for the record; must never return NULL.
// ----------------------------------------------------------------------------------------

char* pwsf_GetPrincName( PWFileEntry *userRec )
{
	if ( strcmp( userRec->digest[kPWHashSlotKERBEROS_NAME].method, "KerberosPrincName" ) == 0 )
		return userRec->digest[kPWHashSlotKERBEROS_NAME].digest;
	
	return userRec->usernameStr;
}


// ----------------------------------------------------------------------------------------
//  pwsf_ShadowHashDataToArray
//
//	Returns: TRUE if an array is returned in <outHashTypeArray>.
// ----------------------------------------------------------------------------------------

int pwsf_ShadowHashDataToArray( const char *inAAData, CFMutableArrayRef *outHashTypeArray )
{
	CFMutableArrayRef hashTypeArray = NULL;
	char hashType[256];
	
	if ( inAAData == NULL || outHashTypeArray == NULL || *inAAData == '\0' )
		return 0;
	
	*outHashTypeArray = NULL;
	
	// get the existing list (if any)
	if ( strncmp( inAAData, kDSTagAuthAuthorityBetterHashOnly, sizeof(kDSTagAuthAuthorityBetterHashOnly)-1 ) == 0 )
	{	
		hashTypeArray = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
		if ( hashTypeArray == NULL )
			return 0;
		
		pwsf_AppendUTF8StringToArray( "SMB-NT", hashTypeArray );
		pwsf_AppendUTF8StringToArray( "SALTED-SHA1", hashTypeArray );
	}
	else
	if ( strncmp( inAAData, kHashNameListPrefix, sizeof(kHashNameListPrefix)-1 ) == 0 )
	{
		// comma delimited list
		const char *endPtr;
		const char *tptr = inAAData + sizeof(kHashNameListPrefix) - 1;
		
		hashTypeArray = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
		if ( hashTypeArray == NULL )
			return 0;
		
		if ( *tptr++ == '<' && strchr(tptr, '>') != NULL )
		{
			while ( (endPtr = strchr( tptr, ',' )) != NULL )
			{
				strlcpy( hashType, tptr, (endPtr - tptr) + 1 );
				pwsf_AppendUTF8StringToArray( hashType, hashTypeArray );
				
				tptr += (endPtr - tptr) + 1;
			}
			
			endPtr = strchr( tptr, '>' );
			if ( endPtr != NULL )
			{
				strlcpy( hashType, tptr, (endPtr - tptr) + 1 );
				pwsf_AppendUTF8StringToArray( hashType, hashTypeArray );
			}
		}
	}
	
	*outHashTypeArray = hashTypeArray;
	
	return 1;
}


// ----------------------------------------------------------------------------------------
//  pwsf_ShadowHashArrayToData
// ----------------------------------------------------------------------------------------

char * pwsf_ShadowHashArrayToData( CFArrayRef inHashTypeArray, long *outResultLen )
{
	char *aaNewData = NULL;
	char *newDataCStr = NULL;
	long len = 0;
	CFMutableStringRef newDataString = NULL;
	CFStringRef stringRef;
	CFIndex stringLen;
	
	// build the new string
	CFIndex typeCount = CFArrayGetCount( inHashTypeArray );
	if ( typeCount > 0 )
	{
		newDataString = CFStringCreateMutable( kCFAllocatorDefault, 0 );
		if ( newDataString == NULL )
			return NULL;
		
		for ( CFIndex index = 0; index < typeCount; index++ )
		{
			stringRef = (CFStringRef) CFArrayGetValueAtIndex( inHashTypeArray, index );
			if ( stringRef != NULL )
			{
				if ( CFStringGetLength(newDataString) > 0 )
					CFStringAppend( newDataString, CFSTR(",") );
				CFStringAppend( newDataString, stringRef );
			}
		}
	}
	
	// build the auth-authority
	stringLen = CFStringGetLength( newDataString );
	newDataCStr = (char *) calloc( 1, stringLen + 1 );
	CFStringGetCString( newDataString, newDataCStr, stringLen + 1, kCFStringEncodingUTF8 );
	aaNewData = (char *) calloc( 1, sizeof(kDSValueAuthAuthorityShadowHash) + sizeof(kHashNameListPrefix) + stringLen + 2 );
	
	// build string
	if ( newDataCStr != NULL && aaNewData != NULL )
		len = sprintf( aaNewData, "%s<%s>", kHashNameListPrefix, newDataCStr );
	
	// clean up
	if ( newDataCStr != NULL )
		free( newDataCStr );
	
	// return string length (if requested)
	if ( outResultLen != NULL )
		*outResultLen = len;
	
	return aaNewData;
}
	

// ----------------------------------------------------------------------------------------
//  pwsf_AppendUTF8StringToArray
// ----------------------------------------------------------------------------------------

void pwsf_AppendUTF8StringToArray( const char *inUTF8Str, CFMutableArrayRef inArray )
{
	CFStringRef stringRef = CFStringCreateWithCString( kCFAllocatorDefault, inUTF8Str, kCFStringEncodingUTF8 );
	CFArrayAppendValue( inArray, stringRef );
	CFRelease( stringRef );
}


// ----------------------------------------------------------------------------------------
//  pwsf_EndianAdjustTimeStruct
// ----------------------------------------------------------------------------------------

void pwsf_EndianAdjustTimeStruct( BSDTimeStructCopy *inOutTimeStruct, int native )
{
#if TARGET_RT_LITTLE_ENDIAN
	if ( native == 1 )
	{
		// adjust to native byte order
		inOutTimeStruct->tm_sec = EndianS32_BtoN(inOutTimeStruct->tm_sec);
		inOutTimeStruct->tm_min = EndianS32_BtoN(inOutTimeStruct->tm_min);
		inOutTimeStruct->tm_hour = EndianS32_BtoN(inOutTimeStruct->tm_hour);
		inOutTimeStruct->tm_mday = EndianS32_BtoN(inOutTimeStruct->tm_mday);
		inOutTimeStruct->tm_mon = EndianS32_BtoN(inOutTimeStruct->tm_mon);
		inOutTimeStruct->tm_year = EndianS32_BtoN(inOutTimeStruct->tm_year);
		inOutTimeStruct->tm_wday = EndianS32_BtoN(inOutTimeStruct->tm_wday);
		inOutTimeStruct->tm_yday = EndianS32_BtoN(inOutTimeStruct->tm_yday);
		inOutTimeStruct->tm_isdst = EndianS32_BtoN(inOutTimeStruct->tm_isdst);
		inOutTimeStruct->tm_gmtoff = EndianS32_BtoN(inOutTimeStruct->tm_gmtoff);
	}
	else
	{
		// adjust to big-endian byte order
		inOutTimeStruct->tm_sec = EndianS32_NtoB(inOutTimeStruct->tm_sec);
		inOutTimeStruct->tm_min = EndianS32_NtoB(inOutTimeStruct->tm_min);
		inOutTimeStruct->tm_hour = EndianS32_NtoB(inOutTimeStruct->tm_hour);
		inOutTimeStruct->tm_mday = EndianS32_NtoB(inOutTimeStruct->tm_mday);
		inOutTimeStruct->tm_mon = EndianS32_NtoB(inOutTimeStruct->tm_mon);
		inOutTimeStruct->tm_year = EndianS32_NtoB(inOutTimeStruct->tm_year);
		inOutTimeStruct->tm_wday = EndianS32_NtoB(inOutTimeStruct->tm_wday);
		inOutTimeStruct->tm_yday = EndianS32_NtoB(inOutTimeStruct->tm_yday);
		inOutTimeStruct->tm_isdst = EndianS32_NtoB(inOutTimeStruct->tm_isdst);
		inOutTimeStruct->tm_gmtoff = EndianS32_NtoB(inOutTimeStruct->tm_gmtoff);
	}
#endif
}


// ----------------------------------------------------------------------------------------
//  pwsf_EndianAdjustPWFileHeader
// ----------------------------------------------------------------------------------------

void pwsf_EndianAdjustPWFileHeader( PWFileHeader *inOutHeader, int native )
{
#if TARGET_RT_LITTLE_ENDIAN
	if ( native == 1 )
	{
		// adjust to native byte order
		inOutHeader->signature = EndianU32_BtoN(inOutHeader->signature);
		inOutHeader->version = EndianU32_BtoN(inOutHeader->version);
		inOutHeader->entrySize = EndianU32_BtoN(inOutHeader->entrySize);
		inOutHeader->sequenceNumber = EndianU32_BtoN(inOutHeader->sequenceNumber);
		inOutHeader->numberOfSlotsCurrentlyInFile = EndianU32_BtoN(inOutHeader->numberOfSlotsCurrentlyInFile);
		inOutHeader->deepestSlotUsed = EndianU32_BtoN(inOutHeader->deepestSlotUsed);

		inOutHeader->access.maxMinutesUntilChangePassword = EndianU32_BtoN(inOutHeader->access.maxMinutesUntilChangePassword);
		inOutHeader->access.maxMinutesUntilDisabled = EndianU32_BtoN(inOutHeader->access.maxMinutesUntilDisabled);
		inOutHeader->access.maxMinutesOfNonUse = EndianU32_BtoN(inOutHeader->access.maxMinutesOfNonUse);
		inOutHeader->access.maxFailedLoginAttempts = EndianU16_BtoN(inOutHeader->access.maxFailedLoginAttempts);
		inOutHeader->access.minChars = EndianU16_BtoN(inOutHeader->access.minChars);
		inOutHeader->access.maxChars = EndianU16_BtoN(inOutHeader->access.maxChars);
		
		pwsf_EndianAdjustTimeStruct(&(inOutHeader->access.expirationDateGMT), 1);
		pwsf_EndianAdjustTimeStruct(&(inOutHeader->access.hardExpireDateGMT), 1);
		
		inOutHeader->publicKeyLen = EndianU32_BtoN(inOutHeader->publicKeyLen);                                                    // 1024-bit RSA public key - expected size is 233
		inOutHeader->privateKeyLen = EndianU32_BtoN(inOutHeader->privateKeyLen);
		inOutHeader->deepestSlotUsedByThisServer = EndianU32_BtoN(inOutHeader->deepestSlotUsedByThisServer);
		inOutHeader->accessModDate = EndianU32_BtoN(inOutHeader->accessModDate);
		inOutHeader->extraAccess.minutesUntilFailedLoginReset = EndianU32_BtoN(inOutHeader->extraAccess.minutesUntilFailedLoginReset);
		inOutHeader->extraAccess.notGuessablePattern = EndianU32_BtoN(inOutHeader->extraAccess.notGuessablePattern);
	}
	else
	{
		// adjust to big-endian byte order
		inOutHeader->signature = EndianU32_NtoB(inOutHeader->signature);
		inOutHeader->version = EndianU32_NtoB(inOutHeader->version);
		inOutHeader->entrySize = EndianU32_NtoB(inOutHeader->entrySize);
		inOutHeader->sequenceNumber = EndianU32_NtoB(inOutHeader->sequenceNumber);
		inOutHeader->numberOfSlotsCurrentlyInFile = EndianU32_NtoB(inOutHeader->numberOfSlotsCurrentlyInFile);
		inOutHeader->deepestSlotUsed = EndianU32_NtoB(inOutHeader->deepestSlotUsed);
		inOutHeader->access.maxMinutesUntilChangePassword = EndianU32_NtoB(inOutHeader->access.maxMinutesUntilChangePassword);
		inOutHeader->access.maxMinutesUntilDisabled = EndianU32_NtoB(inOutHeader->access.maxMinutesUntilDisabled);
		inOutHeader->access.maxMinutesOfNonUse = EndianU32_NtoB(inOutHeader->access.maxMinutesOfNonUse);
		inOutHeader->access.maxFailedLoginAttempts = EndianU32_NtoB(inOutHeader->access.maxFailedLoginAttempts);
		pwsf_EndianAdjustTimeStruct(&(inOutHeader->access.expirationDateGMT), 0);
		pwsf_EndianAdjustTimeStruct(&(inOutHeader->access.hardExpireDateGMT), 0);
		inOutHeader->access.minChars = EndianU16_NtoB(inOutHeader->access.minChars);
		inOutHeader->access.maxChars = EndianU16_NtoB(inOutHeader->access.maxChars);
		inOutHeader->publicKeyLen = EndianU32_NtoB(inOutHeader->publicKeyLen);
		inOutHeader->privateKeyLen = EndianU32_NtoB(inOutHeader->privateKeyLen);
		inOutHeader->deepestSlotUsedByThisServer = EndianU32_NtoB(inOutHeader->deepestSlotUsedByThisServer);
		inOutHeader->accessModDate = EndianU32_NtoB(inOutHeader->accessModDate);
		inOutHeader->extraAccess.minutesUntilFailedLoginReset = EndianU32_NtoB(inOutHeader->extraAccess.minutesUntilFailedLoginReset);
		inOutHeader->extraAccess.notGuessablePattern = EndianU32_NtoB(inOutHeader->extraAccess.notGuessablePattern);
	}
#endif
}


