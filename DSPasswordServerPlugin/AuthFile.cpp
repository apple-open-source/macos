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
#include "CLog.h"
#endif

#include "AuthFile.h"

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
    
#if DEBUGTIME
    LogTimeAsString( (BSDTimeStructCopy *)inTime, "pwrec" );
    SRVLOG1( kLogMeta, "theTime: %l, %l", theGMTime, theTime );
    SRVLOG1( kLogMeta, "diff: %l", theGMTime - theTime );
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
    
#if DEBUGTIME
    LogTimeAsString( (BSDTimeStructCopy *)inLastLogin, "pwrec" );
    SRVLOG1( kLogMeta, "theGMTime, theTime: %l, %l", theGMTime, theTime );
    SRVLOG1( kLogMeta, "diff: %l", ((theGMTime - theTime)/60) );
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
		historyValue = 1 + inAccessFeatures->historyCount;
	
    sprintf( temp1Str, "%s=%d %s=%d %s=%d %s=%d %s=%d ",
                kPWPolicyStr_usingHistory, historyValue,
                kPWPolicyStr_usingExpirationDate, (inAccessFeatures->usingExpirationDate != 0),
                kPWPolicyStr_usingHardExpirationDate, (inAccessFeatures->usingHardExpirationDate != 0),
                kPWPolicyStr_requiresAlpha, (inAccessFeatures->requiresAlpha != 0),
                kPWPolicyStr_requiresNumeric, (inAccessFeatures->requiresNumeric != 0) );
                
    sprintf( temp2Str, "%s=%lu %s=%lu %s=%lu ",
                kPWPolicyStr_expirationDateGMT, timegm( (struct tm *)&inAccessFeatures->expirationDateGMT ),
                kPWPolicyStr_hardExpireDateGMT, timegm( (struct tm *)&inAccessFeatures->hardExpireDateGMT ),
                kPWPolicyStr_maxMinutesUntilChangePW, inAccessFeatures->maxMinutesUntilChangePassword );
    
    sprintf( temp3Str, "%s=%lu %s=%lu %s=%u %s=%u %s=%u %s=%d ",
                kPWPolicyStr_maxMinutesUntilDisabled, inAccessFeatures->maxMinutesUntilDisabled,
                kPWPolicyStr_maxMinutesOfNonUse, inAccessFeatures->maxMinutesOfNonUse,
                kPWPolicyStr_maxFailedLoginAttempts, inAccessFeatures->maxFailedLoginAttempts,
                kPWPolicyStr_minChars, inAccessFeatures->minChars,
                kPWPolicyStr_maxChars, inAccessFeatures->maxChars,
				kPWPolicyStr_passwordCannotBeName, (inAccessFeatures->passwordCannotBeName != 0) );
    
    strcpy( outString, temp1Str );
    strcat( outString, temp2Str );
    strcat( outString, temp3Str );
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
		historyValue = 1 + inGAccessFeatures->historyCount;
	
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
    unsigned long value;
    
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
	
    if ( StringToPWAccessFeatures_GetValue( usingExpirationDate, &value ) )
        inOutAccessFeatures->usingExpirationDate = value;
    
    if ( StringToPWAccessFeatures_GetValue( usingHardExpirationDate, &value ) )
        inOutAccessFeatures->usingHardExpirationDate = value;
    
    if ( StringToPWAccessFeatures_GetValue( requiresAlpha, &value ) )
        inOutAccessFeatures->requiresAlpha = value;
    
    if ( StringToPWAccessFeatures_GetValue( requiresNumeric, &value ) )
        inOutAccessFeatures->requiresNumeric = value;
    
    if ( StringToPWAccessFeatures_GetValue( expirationDateGMT, &value ) )
        memcpy( &inOutAccessFeatures->expirationDateGMT, (BSDTimeStructCopy *) gmtime((time_t *)&value), sizeof(BSDTimeStructCopy) );
    
    if ( StringToPWAccessFeatures_GetValue( hardExpireDateGMT, &value ) )
        memcpy( &inOutAccessFeatures->hardExpireDateGMT, (BSDTimeStructCopy *) gmtime((time_t *)&value), sizeof(BSDTimeStructCopy) ); 
    
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
	
    // no checking for now
    return true;
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
        memcpy( &inOutAccessFeatures->expirationDateGMT, (BSDTimeStructCopy *) gmtime((time_t *)&value), sizeof(BSDTimeStructCopy) );
    
    if ( StringToPWAccessFeatures_GetValue( hardExpireDateGMT, &value ) )
        memcpy( &inOutAccessFeatures->hardExpireDateGMT, (BSDTimeStructCopy *) gmtime((time_t *)&value), sizeof(BSDTimeStructCopy) ); 
    
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







