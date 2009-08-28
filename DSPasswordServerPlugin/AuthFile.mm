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

#include <TargetConditionals.h>
#include <CoreServices/CoreServices.h>
#include <CommonCrypto/CommonDigest.h>

#include <sys/param.h>
#include <sys/stat.h>
#include <grp.h>
#include <membership.h>

#include "AuthFile.h"
#include "AuthFilePriv.h"
#include "CAuthFileBase.h"
#include "ReplicaFile.h"
#include "SASLCode.h"
#include "SMBAuth.h"
#include "PSUtilitiesDefs.h"

#define kMaxPolicyStrLen		2048

// from DirServicesConst.h (our layer is below DS)
#define kDSValueAuthAuthorityShadowHash				";ShadowHash;"
#define kDSTagAuthAuthorityShadowHash				"ShadowHash"
#define kDSTagAuthAuthorityBetterHashOnly			"BetterHashOnly"
#define kHashNameListPrefix							"HASHLIST:"

static void pwsf_EndianAdjustPWFileEntryNoTimes( PWFileEntry *inOutEntry, int native );

int32_t gDESKeyArray[] = {	3785, 1706062950, 57253, 290150789, 20358, -1895669319, 31632, -1897719547, 
							14472, -356383814, 45160, 1045764877, 42031, 340922616, 17982, 893499693, 
							26821, 1697433225, 49601, 1067086915, 42371, -1280392416, 46874, -1550815422, 
							47922, -788758000, 7254, -590256566, 22097, 1547481608, 22125, -1323694915 };


void BSDTimeStructCopy2StructTM(const BSDTimeStructCopy* ourTM, struct tm* sysTM)
{
    sysTM->tm_sec    = ourTM->tm_sec;
    sysTM->tm_min    = ourTM->tm_min;
    sysTM->tm_hour   = ourTM->tm_hour;
    sysTM->tm_mday   = ourTM->tm_mday;
    sysTM->tm_mon    = ourTM->tm_mon;
    sysTM->tm_year   = ourTM->tm_year;
    sysTM->tm_wday   = ourTM->tm_wday;
    sysTM->tm_yday   = ourTM->tm_yday;
    sysTM->tm_isdst  = ourTM->tm_isdst;
    sysTM->tm_gmtoff = (long)ourTM->tm_gmtoff;
    sysTM->tm_zone   = NULL;
}

void StructTM2BSDTimeStructCopy(const struct tm* sysTM, BSDTimeStructCopy* ourTM)
{
    ourTM->tm_sec    = sysTM->tm_sec;
    ourTM->tm_min    = sysTM->tm_min;
    ourTM->tm_hour   = sysTM->tm_hour;
    ourTM->tm_mday   = sysTM->tm_mday;
    ourTM->tm_mon    = sysTM->tm_mon;
    ourTM->tm_year   = sysTM->tm_year;
    ourTM->tm_wday   = sysTM->tm_wday;
    ourTM->tm_yday   = sysTM->tm_yday;
    ourTM->tm_isdst  = sysTM->tm_isdst;
    ourTM->tm_gmtoff = (uint32_t)sysTM->tm_gmtoff;
    ourTM->tm_zone   = 0;
}

time_t BSDTimeStructCopy_timegm(const BSDTimeStructCopy* ourTM)
{
    struct tm bsdTimeStruct;
    BSDTimeStructCopy2StructTM(ourTM, &bsdTimeStruct);
    return timegm(&bsdTimeStruct);
}

void BSDTimeStructCopy_gmtime_r(const time_t* clock, BSDTimeStructCopy* ourTM)
{
    struct tm bsdTimeStruct;
    gmtime_r(clock, &bsdTimeStruct);
    StructTM2BSDTimeStructCopy(&bsdTimeStruct, ourTM);
}

size_t BSDTimeStructCopy_strftime(char *s, size_t maxsize, const char *format, const BSDTimeStructCopy *ourTM)
{
    struct tm bsdTimeStruct;
    BSDTimeStructCopy2StructTM(ourTM, &bsdTimeStruct);
    return strftime(s, maxsize, format, &bsdTimeStruct);
}

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
    theTime = BSDTimeStructCopy_timegm( inTime );
    if ( theTime <= 0 )
		theTime = 0x7FFFFFFF;
	
#if DEBUGTIME
    LogTimeAsString( inTime, "pwrec" );
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
    theTime = BSDTimeStructCopy_timegm( inLastLogin );
//    if ( theTime <= 0 )
//		theTime = theGMTime;

#if DEBUGTIME
    LogTimeAsString( inLastLogin, "pwrec" );
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
                
    sprintf( temp2Str,
#ifdef __LP64__
				"%s=%lu %s=%lu %s=%u ",
#else
				"%s=%lu %s=%lu %s=%lu ",
#endif
                kPWPolicyStr_expirationDateGMT, BSDTimeStructCopy_timegm( &inAccessFeatures->expirationDateGMT ),
                kPWPolicyStr_hardExpireDateGMT, BSDTimeStructCopy_timegm( &inAccessFeatures->hardExpireDateGMT ),
                kPWPolicyStr_maxMinutesUntilChangePW, inAccessFeatures->maxMinutesUntilChangePassword );
    
    sprintf( temp3Str,
#ifdef __LP64__
				"%s=%u %s=%u %s=%u %s=%u %s=%u %s=%d %s=%d %s=%d %s=%d ",
#else
				"%s=%lu %s=%lu %s=%u %s=%u %s=%u %s=%d %s=%d %s=%d %s=%d ",
#endif
                kPWPolicyStr_maxMinutesUntilDisabled, inAccessFeatures->maxMinutesUntilDisabled,
                kPWPolicyStr_maxMinutesOfNonUse, inAccessFeatures->maxMinutesOfNonUse,
                kPWPolicyStr_maxFailedLoginAttempts, inAccessFeatures->maxFailedLoginAttempts,
                kPWPolicyStr_minChars, inAccessFeatures->minChars,
                kPWPolicyStr_maxChars, inAccessFeatures->maxChars,
				kPWPolicyStr_passwordCannotBeName, (inAccessFeatures->passwordCannotBeName != 0),
				kPWPolicyStr_requiresMixedCase, (inAccessFeatures->requiresMixedCase != 0),
				kPWPolicyStr_requiresSymbol, (inAccessFeatures->requiresSymbol != 0),
				kPWPolicyStr_newPasswordRequired, (inAccessFeatures->newPasswordRequired != 0) );
    
    strcpy( outString, temp1Str );
    strcat( outString, temp2Str );
    strcat( outString, temp3Str );
}


//------------------------------------------------------------------------------------------------
//	PWGlobalAccessFeaturesToStringExtra
//------------------------------------------------------------------------------------------------

void PWGlobalAccessFeaturesToStringExtra( PWGlobalAccessFeatures *inAccessFeatures, PWGlobalMoreAccessFeatures *inExtraFeatures,
	size_t inMaxLen, char *outString )
{
    char tempStr[2048];
	size_t len;
	
    if ( outString == NULL || inAccessFeatures == NULL )
        return;
	
	PWGlobalAccessFeaturesToString( inAccessFeatures, tempStr );
	len = snprintf( outString, inMaxLen, "%s", tempStr );
	if ( len >= inMaxLen )
		return;
	
	snprintf( outString + len, inMaxLen - len,
#ifdef __LP64__
				"%s=%u %s=%u",
#else
				"%s=%lu %s=%lu",
#endif
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

void PWAccessFeaturesToStringExtra( PWAccessFeatures *inAccessFeatures, PWMoreAccessFeatures *inExtraFeatures, size_t inMaxLen,
	char *outString )
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

	snprintf( outString + segment1Length + segment2Length, inMaxLen, " %s=%d %s=%d %s=%u %s=%d %s=%d %s=%d %s=%d %s=%d %s=%d",
				kPWPolicyStr_isSessionKeyAgent, (inAccessFeatures->isSessionKeyAgent != 0),
				kPWPolicyStr_isComputerAccount, (inExtraFeatures->isComputerAccount != 0),
				kPWPolicyStr_adminClass, inExtraFeatures->adminClass,
				kPWPolicyStr_adminNoChangePasswords, (inExtraFeatures->adminNoChangePasswords != 0),
				kPWPolicyStr_adminNoSetPolicies, (inExtraFeatures->adminNoSetPolicies != 0),
				kPWPolicyStr_adminNoCreate, (inExtraFeatures->adminNoCreate != 0),
				kPWPolicyStr_adminNoDelete, (inExtraFeatures->adminNoDelete != 0),
				kPWPolicyStr_adminNoClearState, (inExtraFeatures->adminNoClearState != 0),
				kPWPolicyStr_adminNoPromoteAdmins, (inExtraFeatures->adminNoPromoteAdmins != 0) );
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
                kPWPolicyStr_expirationDateGMT, BSDTimeStructCopy_timegm( &inAccessFeatures->expirationDateGMT ),
                kPWPolicyStr_hardExpireDateGMT, BSDTimeStructCopy_timegm( &inAccessFeatures->hardExpireDateGMT ) );
    
    snprintf( temp4Str, sizeof(temp4Str),
#ifdef __LP64__
				"%s=%u %s=%u %s=%u %s=%u %s=%u %s=%u %s=%d %s=%d",
#else
				"%s=%lu %s=%lu %s=%lu %s=%u %s=%u %s=%u %s=%d %s=%d",
#endif
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
//	PWActualAccessFeaturesToStringWithoutStateInfoExtra
//
//	Prepares the PWAccessFeatures struct and PWGlobalAccessFeatures defaults for transmission
//	over our text-based protocol
//------------------------------------------------------------------------------------------------

void PWActualAccessFeaturesToStringWithoutStateInfoExtra(
	PWGlobalAccessFeatures *inGAccessFeatures,
	PWAccessFeatures *inAccessFeatures,
	char **outString )
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
	/*
	unsigned int requiresMixedCase = (inExtraFeatures->requiresMixedCase || inGAccessFeatures->requiresMixedCase);
	unsigned int requiresSymbol = (inExtraFeatures->requiresSymbol || inGAccessFeatures->requiresSymbol);
	unsigned long notGuessablePattern = inExtraFeatures->notGuessablePattern;
	*/
	
	char tempStr[512];
	
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
	
	*outString = (char *)calloc( 1, kMaxPolicyStrLen );
	
    snprintf( *outString, kMaxPolicyStrLen, "%s=%d %s=%d %s=%d %s=%d %s=%d %s=%d %s=%d %s=%lu %s=%lu ",
                kPWPolicyStr_newPasswordRequired, (inAccessFeatures->newPasswordRequired != 0),
                kPWPolicyStr_usingHistory, historyValue,
                kPWPolicyStr_canModifyPasswordforSelf, (inAccessFeatures->canModifyPasswordforSelf != 0),
                kPWPolicyStr_usingExpirationDate, usingExpirationDate,
                kPWPolicyStr_usingHardExpirationDate, usingHardExpirationDate,
                kPWPolicyStr_requiresAlpha, requiresAlpha,
                kPWPolicyStr_requiresNumeric, requiresNumeric,
                kPWPolicyStr_expirationDateGMT, BSDTimeStructCopy_timegm( &inAccessFeatures->expirationDateGMT ),
                kPWPolicyStr_hardExpireDateGMT, BSDTimeStructCopy_timegm( &inAccessFeatures->hardExpireDateGMT ) );
    
    snprintf( tempStr, sizeof(tempStr),
#ifdef __LP64__
				"%s=%u %s=%u %s=%u %s=%u %s=%u %s=%u %s=%d %s=%d",
#else
				"%s=%lu %s=%lu %s=%lu %s=%u %s=%u %s=%u %s=%d %s=%d",
#endif
                kPWPolicyStr_maxMinutesUntilChangePW, maxMinutesUntilChangePassword,
                kPWPolicyStr_maxMinutesUntilDisabled, maxMinutesUntilDisabled,
                kPWPolicyStr_maxMinutesOfNonUse, maxMinutesOfNonUse,
                kPWPolicyStr_maxFailedLoginAttempts, maxFailedLoginAttempts,
                kPWPolicyStr_minChars, minChars,
                kPWPolicyStr_maxChars, maxChars,
				kPWPolicyStr_passwordCannotBeName, passwordCannotBeName,
				kPWPolicyStr_isSessionKeyAgent, (inAccessFeatures->isSessionKeyAgent != 0) );
    
    strlcat( *outString, tempStr, kMaxPolicyStrLen );
	
	/*
	snprintf( tempStr, sizeof(tempStr), " %s=%d %s=%d %s=%lu %s=%u %s=%d %s=%d %s=%d %s=%d %s=%d %s=%d",
			kPWPolicyStr_requiresMixedCase, (requiresMixedCase != 0),
			kPWPolicyStr_requiresSymbol, (requiresSymbol != 0),
			kPWPolicyStr_notGuessablePattern, notGuessablePattern,
			kPWPolicyStr_adminClass, inExtraFeatures->adminClass,
			kPWPolicyStr_adminNoChangePasswords, (inExtraFeatures->adminNoChangePasswords != 0),
			kPWPolicyStr_adminNoSetPolicies, (inExtraFeatures->adminNoSetPolicies != 0),
			kPWPolicyStr_adminNoCreate, (inExtraFeatures->adminNoCreate != 0),
			kPWPolicyStr_adminNoDelete, (inExtraFeatures->adminNoDelete != 0),
			kPWPolicyStr_adminNoClearState, (inExtraFeatures->adminNoClearState != 0),
			kPWPolicyStr_adminNoPromoteAdmins, (inExtraFeatures->adminNoPromoteAdmins != 0) );
	
    strlcat( *outString, tempStr, kMaxPolicyStrLen );
	*/
}


//------------------------------------------------------------------------------------------------
//	PWActualAccessFeaturesToStringExtra
//
//	Prepares the PWAccessFeatures struct and PWGlobalAccessFeatures defaults for transmission
//	over our text-based protocol
//------------------------------------------------------------------------------------------------

void PWActualAccessFeaturesToStringExtra( PWGlobalAccessFeatures *inGAccessFeatures, PWAccessFeatures *inAccessFeatures,
	PWMoreAccessFeatures *inExtraFeatures, size_t inMaxLen, char *outString )
{
	size_t segment1Length = 0;
	size_t segment2Length = 0;
	unsigned int requiresMixedCase = (inExtraFeatures->requiresMixedCase || inGAccessFeatures->requiresMixedCase);
	unsigned int requiresSymbol = (inExtraFeatures->requiresSymbol || inGAccessFeatures->requiresSymbol);
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
	
	segment2Length = snprintf( outString + segment1Length, inMaxLen, " %s=%d %s=%d %s=%lu",
			kPWPolicyStr_requiresMixedCase, (requiresMixedCase != 0),
			kPWPolicyStr_requiresSymbol, (requiresSymbol != 0),
			kPWPolicyStr_notGuessablePattern, notGuessablePattern );
	inMaxLen -= segment2Length;
	
	snprintf( outString + segment1Length + segment2Length, inMaxLen, " %s=%u %s=%d %s=%d %s=%d %s=%d %s=%d %s=%d",
			kPWPolicyStr_adminClass, inExtraFeatures->adminClass,
			kPWPolicyStr_adminNoChangePasswords, (inExtraFeatures->adminNoChangePasswords != 0),
			kPWPolicyStr_adminNoSetPolicies, (inExtraFeatures->adminNoSetPolicies != 0),
			kPWPolicyStr_adminNoCreate, (inExtraFeatures->adminNoCreate != 0),
			kPWPolicyStr_adminNoDelete, (inExtraFeatures->adminNoDelete != 0),
			kPWPolicyStr_adminNoClearState, (inExtraFeatures->adminNoClearState != 0),
			kPWPolicyStr_adminNoPromoteAdmins, (inExtraFeatures->adminNoPromoteAdmins != 0) );
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
#ifdef __LP64__
				"%s=%d %s=%d %s=%d %s=%d %s=%d %s=%d %s=%lu %s=%lu %s=%u %s=%u %s=%u %s=%u %s=%u %s=%u %s=%d",
#else
				"%s=%d %s=%d %s=%d %s=%d %s=%d %s=%d %s=%lu %s=%lu %s=%lu %s=%lu %s=%lu %s=%u %s=%u %s=%u %s=%d",
#endif
				kPWPolicyStr_usingHistory, historyValue,
				kPWPolicyStr_canModifyPasswordforSelf, (inAccessFeatures->canModifyPasswordforSelf != 0),
                kPWPolicyStr_usingExpirationDate, (inAccessFeatures->usingExpirationDate != 0),
                kPWPolicyStr_usingHardExpirationDate, (inAccessFeatures->usingHardExpirationDate != 0),
                kPWPolicyStr_requiresAlpha, (inAccessFeatures->requiresAlpha != 0),
				kPWPolicyStr_requiresNumeric, (inAccessFeatures->requiresNumeric != 0),
                kPWPolicyStr_expirationDateGMT, BSDTimeStructCopy_timegm( &inAccessFeatures->expirationDateGMT ),
                kPWPolicyStr_hardExpireDateGMT, BSDTimeStructCopy_timegm( &inAccessFeatures->hardExpireDateGMT ),
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

void PWAccessFeaturesToStringWithoutStateInfoExtra( PWAccessFeatures *inAccessFeatures, PWMoreAccessFeatures *inExtraFeatures,
	size_t inMaxLen, char *outString )
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
#ifdef __LP64__
			" %s=%d %s=%d %s=%u",
#else
			" %s=%d %s=%d %s=%lu",
#endif
			kPWPolicyStr_requiresMixedCase, (inExtraFeatures->requiresMixedCase != 0),
			kPWPolicyStr_requiresSymbol, (inExtraFeatures->requiresSymbol != 0),
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
    const char *requiresSymbol = strstr( inString, kPWPolicyStr_requiresSymbol );
    const char *newPasswordRequired = strstr( inString, kPWPolicyStr_newPasswordRequired );
//    const char *notGuessablePattern = strstr( inString, kPWPolicyStr_notGuessablePattern );
    unsigned long value;
    time_t timetTime;
    
    if ( StringToPWAccessFeatures_GetValue( usingHistory, &value ) )
	{
		if ( value > 0 )
		{
			// clamp to the password server's maximum value
			if ( value > kPWFileMaxHistoryCount )
				value = kPWFileMaxHistoryCount;
			
			inOutAccessFeatures->usingHistory = 1;
			SetGlobalHistoryCount(*inOutAccessFeatures, (unsigned int)value - 1);
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
        inOutAccessFeatures->usingExpirationDate = (unsigned int)value;
    
    if ( StringToPWAccessFeatures_GetValue( usingHardExpirationDate, &value ) )
        inOutAccessFeatures->usingHardExpirationDate = (unsigned int)value;
    
    if ( StringToPWAccessFeatures_GetValue( requiresAlpha, &value ) )
        inOutAccessFeatures->requiresAlpha = (unsigned int)value;
    
    if ( StringToPWAccessFeatures_GetValue( requiresNumeric, &value ) )
        inOutAccessFeatures->requiresNumeric = (unsigned int)value;
    
    if ( StringToPWAccessFeatures_GetValue( expirationDateGMT, &value ) )
    {
        timetTime = (time_t)value;
        BSDTimeStructCopy_gmtime_r( &timetTime, &inOutAccessFeatures->expirationDateGMT );
    }
    
    if ( StringToPWAccessFeatures_GetValue( hardExpireDateGMT, &value ) )
    {
        timetTime = (time_t)value;
        BSDTimeStructCopy_gmtime_r( &timetTime, &inOutAccessFeatures->hardExpireDateGMT );
    }
	
    if ( StringToPWAccessFeatures_GetValue( maxMinutesUntilChangePassword, &value ) )
        inOutAccessFeatures->maxMinutesUntilChangePassword = (UInt32)value;
    
    if ( StringToPWAccessFeatures_GetValue( maxMinutesUntilDisabled, &value ) )
        inOutAccessFeatures->maxMinutesUntilDisabled = (UInt32)value;
    
    if ( StringToPWAccessFeatures_GetValue( maxMinutesOfNonUse, &value ) )
        inOutAccessFeatures->maxMinutesOfNonUse = (UInt32)value;
    
    if ( StringToPWAccessFeatures_GetValue( maxFailedLoginAttempts, &value ) )
        inOutAccessFeatures->maxFailedLoginAttempts = (UInt16)value;
    
    if ( StringToPWAccessFeatures_GetValue( minChars, &value ) )
        inOutAccessFeatures->minChars = (UInt16)value;
        
    if ( StringToPWAccessFeatures_GetValue( maxChars, &value ) )
        inOutAccessFeatures->maxChars = (UInt16)value;
        
	if ( StringToPWAccessFeatures_GetValue( passwordCannotBeName, &value ) )
		inOutAccessFeatures->passwordCannotBeName = (unsigned int)value;
	
	if ( StringToPWAccessFeatures_GetValue( requiresMixedCase, &value ) )
		inOutAccessFeatures->requiresMixedCase = (unsigned int)value;
	
	if ( StringToPWAccessFeatures_GetValue( requiresSymbol, &value ) )
		inOutAccessFeatures->requiresSymbol = (unsigned int)value;
	
	if ( StringToPWAccessFeatures_GetValue( newPasswordRequired, &value ) )
		inOutAccessFeatures->newPasswordRequired = (unsigned int)value;
	
    // no checking for now
    return true;
}


//------------------------------------------------------------------------------------------------
//	StringToPWGlobalAccessFeaturesExtra
//
//	Returns: TRUE if the string is successfully parsed
//------------------------------------------------------------------------------------------------

Boolean StringToPWGlobalAccessFeaturesExtra( const char *inString, PWGlobalAccessFeatures *inOutAccessFeatures,
	PWGlobalMoreAccessFeatures *inOutExtraFeatures )
{
	Boolean result = StringToPWGlobalAccessFeatures( inString, inOutAccessFeatures );
	const char *notGuessablePattern = strstr( inString, kPWPolicyStr_notGuessablePattern );
	const char *minutesUntilFailedLoginReset = strstr( inString, kPWPolicyStr_minutesUntilFailedLoginReset );
    unsigned long value;
	
	if ( StringToPWAccessFeatures_GetValue( notGuessablePattern, &value ) )
		inOutExtraFeatures->notGuessablePattern = (UInt32)value;
		
	if ( StringToPWAccessFeatures_GetValue( minutesUntilFailedLoginReset, &value ) )
		inOutExtraFeatures->minutesUntilFailedLoginReset = (UInt32)value;
	
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
    time_t timetTime;
    
    if ( StringToPWAccessFeatures_GetValue( isDisabled, &value ) )
        inOutAccessFeatures->isDisabled = (int)value;
    
    if ( StringToPWAccessFeatures_GetValue( isAdminUser, &value ) )
        inOutAccessFeatures->isAdminUser = (int)value;
    
    if ( StringToPWAccessFeatures_GetValue( newPasswordRequired, &value ) )
        inOutAccessFeatures->newPasswordRequired = (int)value;
    
    if ( StringToPWAccessFeatures_GetValue( usingHistory, &value ) )
	{
		if ( value > 0 )
		{
			// clamp to the password server's maximum value
			if ( value > kPWFileMaxHistoryCount )
				value = kPWFileMaxHistoryCount;
			
			inOutAccessFeatures->usingHistory = 1;
			inOutAccessFeatures->historyCount = (unsigned int)value - 1;
		}
		else
		{
			inOutAccessFeatures->usingHistory = 0;
			inOutAccessFeatures->historyCount = 0;
		}
    }
	
    if ( StringToPWAccessFeatures_GetValue( canModifyPasswordforSelf, &value ) )
        inOutAccessFeatures->canModifyPasswordforSelf = (int)value;
    
    if ( StringToPWAccessFeatures_GetValue( usingExpirationDate, &value ) )
        inOutAccessFeatures->usingExpirationDate = (int)value;
    
    if ( StringToPWAccessFeatures_GetValue( usingHardExpirationDate, &value ) )
        inOutAccessFeatures->usingHardExpirationDate = (int)value;
    
    if ( StringToPWAccessFeatures_GetValue( requiresAlpha, &value ) )
        inOutAccessFeatures->requiresAlpha = (int)value;
    
    if ( StringToPWAccessFeatures_GetValue( requiresNumeric, &value ) )
        inOutAccessFeatures->requiresNumeric = (int)value;
    
    if ( StringToPWAccessFeatures_GetValue( expirationDateGMT, &value ) )
    {
        timetTime = (time_t)value;
		BSDTimeStructCopy_gmtime_r( &timetTime, &inOutAccessFeatures->expirationDateGMT );
    }
    
    if ( StringToPWAccessFeatures_GetValue( hardExpireDateGMT, &value ) )
    {
        timetTime = (time_t)value;
		BSDTimeStructCopy_gmtime_r( &timetTime, &inOutAccessFeatures->hardExpireDateGMT );
    }
	
	if ( StringToPWAccessFeatures_GetValue( maxMinutesUntilChangePassword, &value ) )
        inOutAccessFeatures->maxMinutesUntilChangePassword = (UInt32)value;
    
    if ( StringToPWAccessFeatures_GetValue( maxMinutesUntilDisabled, &value ) )
        inOutAccessFeatures->maxMinutesUntilDisabled = (UInt32)value;
    
    if ( StringToPWAccessFeatures_GetValue( maxMinutesOfNonUse, &value ) )
        inOutAccessFeatures->maxMinutesOfNonUse = (UInt32)value;
    
    if ( StringToPWAccessFeatures_GetValue( maxFailedLoginAttempts, &value ) )
        inOutAccessFeatures->maxFailedLoginAttempts = (UInt16)value;
    
    if ( StringToPWAccessFeatures_GetValue( minChars, &value ) )
        inOutAccessFeatures->minChars = (UInt16)value;
        
    if ( StringToPWAccessFeatures_GetValue( maxChars, &value ) )
        inOutAccessFeatures->maxChars = (UInt16)value;
	
	if ( StringToPWAccessFeatures_GetValue( passwordCannotBeName, &value ) )
		inOutAccessFeatures->passwordCannotBeName = (int)value;
	
	if ( StringToPWAccessFeatures_GetValue( isSessionKeyAgent, &value ) )
		inOutAccessFeatures->isSessionKeyAgent = (int)value;

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

Boolean StringToPWAccessFeaturesExtra( const char *inString, PWAccessFeatures *inOutAccessFeatures,
	PWMoreAccessFeatures *inOutExtraFeatures )
{
	const char *requiresMixedCase = strstr( inString, kPWPolicyStr_requiresMixedCase );
	const char *requiresSymbol = strstr( inString, kPWPolicyStr_requiresSymbol );
	const char *notGuessablePattern = strstr( inString, kPWPolicyStr_notGuessablePattern );
	const char *isComputerAccount = strstr( inString, kPWPolicyStr_isComputerAccount );
	const char *adminNoChangePasswords = strstr( inString, kPWPolicyStr_adminNoChangePasswords );
	const char *adminNoSetPolicies = strstr( inString, kPWPolicyStr_adminNoSetPolicies );
	const char *adminNoCreate = strstr( inString, kPWPolicyStr_adminNoCreate );
	const char *adminNoDelete = strstr( inString, kPWPolicyStr_adminNoDelete );
	const char *adminNoClearState = strstr( inString, kPWPolicyStr_adminNoClearState );
	const char *adminNoPromoteAdmins = strstr( inString, kPWPolicyStr_adminNoPromoteAdmins );
	const char *adminClass = strstr( inString, kPWPolicyStr_adminClass );
	const char *resetToGlobalDefaults = strstr( inString, kPWPolicyStr_resetToGlobalDefaults );
    unsigned long value;
    
	if ( inOutExtraFeatures == NULL )
		return false;
	
	Boolean result = StringToPWAccessFeatures( inString, inOutAccessFeatures );
	if ( result )
	{   
		if ( StringToPWAccessFeatures_GetValue( requiresMixedCase, &value ) )
			inOutExtraFeatures->requiresMixedCase = (UInt32)value;
		
		if ( StringToPWAccessFeatures_GetValue( requiresSymbol, &value ) )
			inOutExtraFeatures->requiresSymbol = (unsigned int)value;
		
		if ( StringToPWAccessFeatures_GetValue( notGuessablePattern, &value ) )
			inOutExtraFeatures->notGuessablePattern = (unsigned int)value;
		
		if ( StringToPWAccessFeatures_GetValue( isComputerAccount, &value ) )
			inOutExtraFeatures->isComputerAccount = (unsigned int)value;
		
		if ( StringToPWAccessFeatures_GetValue( adminNoChangePasswords, &value ) )
			inOutExtraFeatures->adminNoChangePasswords = (unsigned int)value;
			
		if ( StringToPWAccessFeatures_GetValue( adminNoSetPolicies, &value ) )
			inOutExtraFeatures->adminNoSetPolicies = (unsigned int)value;
			
		if ( StringToPWAccessFeatures_GetValue( adminNoCreate, &value ) )
			inOutExtraFeatures->adminNoCreate = (unsigned int)value;
			
		if ( StringToPWAccessFeatures_GetValue( adminNoDelete, &value ) )
			inOutExtraFeatures->adminNoDelete = (unsigned int)value;
			
		if ( StringToPWAccessFeatures_GetValue( adminNoClearState, &value ) )
			inOutExtraFeatures->adminNoClearState = (unsigned int)value;
			
		if ( StringToPWAccessFeatures_GetValue( adminNoPromoteAdmins, &value ) )
			inOutExtraFeatures->adminNoPromoteAdmins = (unsigned int)value;
			
		if ( StringToPWAccessFeatures_GetValue( adminClass, &value ) )
			inOutExtraFeatures->adminClass = (unsigned int)value;
		
		if ( StringToPWAccessFeatures_GetValue( resetToGlobalDefaults, &value ) && value > 0 )
		{
			inOutExtraFeatures->requiresMixedCase = 0;
			inOutExtraFeatures->requiresSymbol = 0;
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

		curLen = strlen( inOutString );
	}

	const char *passwordLastSetTime = strstr( inOriginalStr, kPWPolicyStr_passwordLastSetTime );
	if( passwordLastSetTime != NULL )
	{
		endPtr = strchr( passwordLastSetTime, ' ' );
		if( endPtr != NULL )
			strlcat( inOutString,
					 passwordLastSetTime, 
					 MIN(curLen + (endPtr - passwordLastSetTime) + 1, inMaxLen) );
		else
			strlcat( inOutString, passwordLastSetTime, inMaxLen );
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
	ReplicaFile *replicaFile = nil;
	struct stat sb;
	
	if ( lstat(kPWReplicaFile, &sb) == 0 )
	{
		// replace the existing file
		unlink( kPWReplicaFile );
	}
	
	replicaFile = [[ReplicaFile alloc] init];
	if ( replicaFile != nil )
	{
		[replicaFile setParentWithIP:inIPStr andDNS:(inDNSStr != NULL && inDNSStr[0] != '\0') ? inDNSStr : NULL];

		if ( inPublicKey != NULL )
		{
			[replicaFile addServerUniqueID:inPublicKey];
		}
		else
		{
			CAuthFileBase authFile;
			char rsaKey[kPWFileMaxPublicKeyBytes];
			
			if ( authFile.getRSAPublicKey( rsaKey ) == 0 )
				[replicaFile addServerUniqueID:rsaKey];
		}
		
		[replicaFile setReplicaPolicy:kReplicaAllowAll];
		[replicaFile saveXMLData];
		[replicaFile release];
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
	ReplicaFile *replicaFile = nil;
	CFMutableDictionaryRef serverDict = NULL;
	char ipStr[256] = {0,};
	char dnsStr[256] = {0,};
	
	strcpy( ipStr, "0.0.0.0" );
	
	replicaFile = [[ReplicaFile alloc] init];
	if ( replicaFile != nil )
	{
		serverDict = (CFMutableDictionaryRef)[replicaFile getParent];
		if ( serverDict != NULL )
		{
			// fetch data to preserve
			ipAddressArray = [replicaFile getIPAddressesFromDict:serverDict];
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
		[replicaFile release];
	}
	
	// replace the replica file
	pwsf_CreateReplicaFile( ipStr, dnsStr, inPublicKey );
	
	// restore IP list (if > 1 address)
	if ( ipAddressCount > 1 )
	{
		ReplicaFile *newReplicaFile = [[ReplicaFile alloc] init];
		if ( newReplicaFile != nil )
		{
			serverDict = (CFMutableDictionaryRef) [newReplicaFile getParent];
			if ( serverDict != NULL )
			{
				CFDictionarySetValue( serverDict, CFSTR(kPWReplicaIPKey), ipAddressArray );
				[newReplicaFile setParentWithDict:serverDict];
				[newReplicaFile saveXMLData];
			}
			[newReplicaFile release];
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

char *pwsf_ShadowHashArrayToData( CFArrayRef inHashTypeArray, long *outResultLen )
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
	if ( newDataString != NULL )
		CFRelease( newDataString );
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
	if ( stringRef != NULL ) {
		CFArrayAppendValue( inArray, stringRef );
		CFRelease( stringRef );
	}
}


#pragma mark -
#pragma mark ENDIAN UTILS
#pragma mark -

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
		
		pwsf_EndianAdjustTimeStruct(&(inOutHeader->access.expirationDateGMT), native);
		pwsf_EndianAdjustTimeStruct(&(inOutHeader->access.hardExpireDateGMT), native);
		
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
		inOutHeader->access.maxFailedLoginAttempts = EndianU16_NtoB(inOutHeader->access.maxFailedLoginAttempts);
		pwsf_EndianAdjustTimeStruct(&(inOutHeader->access.expirationDateGMT), native);
		pwsf_EndianAdjustTimeStruct(&(inOutHeader->access.hardExpireDateGMT), native);
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


// ----------------------------------------------------------------------------------------
//  pwsf_EndianAdjustPWFileEntry
// ----------------------------------------------------------------------------------------

void pwsf_EndianAdjustPWFileEntry( PWFileEntry *inOutEntry, int native )
{
#if TARGET_RT_LITTLE_ENDIAN
	if ( native == 1 )
	{
		// adjust to native byte order
		inOutEntry->time = EndianU32_BtoN(inOutEntry->time);
		inOutEntry->rnd = EndianU32_BtoN(inOutEntry->rnd);
		inOutEntry->sequenceNumber = EndianU32_BtoN(inOutEntry->sequenceNumber);
		inOutEntry->slot = EndianU32_BtoN(inOutEntry->slot);
		
		pwsf_EndianAdjustTimeStruct(&inOutEntry->creationDate, native);
		pwsf_EndianAdjustTimeStruct(&inOutEntry->modificationDate, native);
		pwsf_EndianAdjustTimeStruct(&inOutEntry->modDateOfPassword, native);
		pwsf_EndianAdjustTimeStruct(&inOutEntry->lastLogin, native);
		
		inOutEntry->failedLoginAttempts = EndianU16_BtoN(inOutEntry->failedLoginAttempts);
		
		pwsf_EndianAdjustTimeStruct(&(inOutEntry->access.expirationDateGMT), native);
		pwsf_EndianAdjustTimeStruct(&(inOutEntry->access.hardExpireDateGMT), native);

		inOutEntry->access.maxMinutesUntilChangePassword = EndianU32_BtoN(inOutEntry->access.maxMinutesUntilChangePassword);
		inOutEntry->access.maxMinutesUntilDisabled = EndianU32_BtoN(inOutEntry->access.maxMinutesUntilDisabled);
		inOutEntry->access.maxMinutesOfNonUse = EndianU32_BtoN(inOutEntry->access.maxMinutesOfNonUse);
		inOutEntry->access.maxFailedLoginAttempts = EndianU16_BtoN(inOutEntry->access.maxFailedLoginAttempts);
		inOutEntry->access.minChars = EndianU16_BtoN(inOutEntry->access.minChars);
		inOutEntry->access.maxChars = EndianU16_BtoN(inOutEntry->access.maxChars);
		
		inOutEntry->disableReason = (PWDisableReasonCode) EndianS32_BtoN(inOutEntry->disableReason);

		inOutEntry->extraAccess.minutesUntilFailedLoginReset = EndianU32_BtoN(inOutEntry->extraAccess.minutesUntilFailedLoginReset);
		inOutEntry->extraAccess.notGuessablePattern = EndianU32_BtoN(inOutEntry->extraAccess.notGuessablePattern);
		inOutEntry->extraAccess.logOffTime = EndianU32_BtoN(inOutEntry->extraAccess.logOffTime);
		inOutEntry->extraAccess.kickOffTime = EndianU32_BtoN(inOutEntry->extraAccess.kickOffTime);
		
		inOutEntry->changeTransactionID = EndianS64_BtoN(inOutEntry->changeTransactionID);
	}
	else
	{
		// adjust to big-endian byte order
		inOutEntry->time = EndianU32_NtoB(inOutEntry->time);
		inOutEntry->rnd = EndianU32_NtoB(inOutEntry->rnd);
		inOutEntry->sequenceNumber = EndianU32_NtoB(inOutEntry->sequenceNumber);
		inOutEntry->slot = EndianU32_NtoB(inOutEntry->slot);
		
		pwsf_EndianAdjustTimeStruct(&inOutEntry->creationDate, native);
		pwsf_EndianAdjustTimeStruct(&inOutEntry->modificationDate, native);
		pwsf_EndianAdjustTimeStruct(&inOutEntry->modDateOfPassword, native);
		pwsf_EndianAdjustTimeStruct(&inOutEntry->lastLogin, native);
		
		inOutEntry->failedLoginAttempts = EndianU16_NtoB(inOutEntry->failedLoginAttempts);
		
		pwsf_EndianAdjustTimeStruct(&(inOutEntry->access.expirationDateGMT), native);
		pwsf_EndianAdjustTimeStruct(&(inOutEntry->access.hardExpireDateGMT), native);

		inOutEntry->access.maxMinutesUntilChangePassword = EndianU32_NtoB(inOutEntry->access.maxMinutesUntilChangePassword);
		inOutEntry->access.maxMinutesUntilDisabled = EndianU32_NtoB(inOutEntry->access.maxMinutesUntilDisabled);
		inOutEntry->access.maxMinutesOfNonUse = EndianU32_NtoB(inOutEntry->access.maxMinutesOfNonUse);
		inOutEntry->access.maxFailedLoginAttempts = EndianU16_NtoB(inOutEntry->access.maxFailedLoginAttempts);
		inOutEntry->access.minChars = EndianU16_NtoB(inOutEntry->access.minChars);
		inOutEntry->access.maxChars = EndianU16_NtoB(inOutEntry->access.maxChars);
		
		inOutEntry->disableReason = (PWDisableReasonCode) EndianS32_NtoB(inOutEntry->disableReason);

		inOutEntry->extraAccess.minutesUntilFailedLoginReset = EndianU32_NtoB(inOutEntry->extraAccess.minutesUntilFailedLoginReset);
		inOutEntry->extraAccess.notGuessablePattern = EndianU32_NtoB(inOutEntry->extraAccess.notGuessablePattern);
		inOutEntry->extraAccess.logOffTime = EndianU32_NtoB(inOutEntry->extraAccess.logOffTime);
		inOutEntry->extraAccess.kickOffTime = EndianU32_NtoB(inOutEntry->extraAccess.kickOffTime);

		inOutEntry->changeTransactionID = EndianS64_NtoB(inOutEntry->changeTransactionID);
	}
#endif
}


// ----------------------------------------------------------------------------------------
//  pwsf_EndianAdjustPWFileEntryNoTimes
//
//	Discussion:
//		in the compress_slot/expand_slot case, the compressed dates are sent
//		in net byte order. They must be converted to host byte order in order
//		to expand them. Therefore, the dates are in host byte order but the
//		rest of the record is still in net byte order.
// ----------------------------------------------------------------------------------------

void pwsf_EndianAdjustPWFileEntryNoTimes( PWFileEntry *inOutEntry, int native )
{
#if TARGET_RT_LITTLE_ENDIAN
	if ( native == 1 )
	{
		// adjust to native byte order
		inOutEntry->time = EndianU32_BtoN(inOutEntry->time);
		inOutEntry->rnd = EndianU32_BtoN(inOutEntry->rnd);
		inOutEntry->sequenceNumber = EndianU32_BtoN(inOutEntry->sequenceNumber);
		inOutEntry->slot = EndianU32_BtoN(inOutEntry->slot);
				
		inOutEntry->failedLoginAttempts = EndianU16_BtoN(inOutEntry->failedLoginAttempts);
		
		inOutEntry->access.maxMinutesUntilChangePassword = EndianU32_BtoN(inOutEntry->access.maxMinutesUntilChangePassword);
		inOutEntry->access.maxMinutesUntilDisabled = EndianU32_BtoN(inOutEntry->access.maxMinutesUntilDisabled);
		inOutEntry->access.maxMinutesOfNonUse = EndianU32_BtoN(inOutEntry->access.maxMinutesOfNonUse);
		inOutEntry->access.maxFailedLoginAttempts = EndianU16_BtoN(inOutEntry->access.maxFailedLoginAttempts);
		inOutEntry->access.minChars = EndianU16_BtoN(inOutEntry->access.minChars);
		inOutEntry->access.maxChars = EndianU16_BtoN(inOutEntry->access.maxChars);
		
		inOutEntry->disableReason = (PWDisableReasonCode) EndianS32_BtoN(inOutEntry->disableReason);

		inOutEntry->extraAccess.minutesUntilFailedLoginReset = EndianU32_BtoN(inOutEntry->extraAccess.minutesUntilFailedLoginReset);
		inOutEntry->extraAccess.notGuessablePattern = EndianU32_BtoN(inOutEntry->extraAccess.notGuessablePattern);
		inOutEntry->extraAccess.logOffTime = EndianU32_BtoN(inOutEntry->extraAccess.logOffTime);
		inOutEntry->extraAccess.kickOffTime = EndianU32_BtoN(inOutEntry->extraAccess.kickOffTime);
		
		inOutEntry->changeTransactionID = EndianS64_BtoN(inOutEntry->changeTransactionID);
	}
	else
	{
		// adjust to big-endian byte order
		inOutEntry->time = EndianU32_NtoB(inOutEntry->time);
		inOutEntry->rnd = EndianU32_NtoB(inOutEntry->rnd);
		inOutEntry->sequenceNumber = EndianU32_NtoB(inOutEntry->sequenceNumber);
		inOutEntry->slot = EndianU32_NtoB(inOutEntry->slot);
				
		inOutEntry->failedLoginAttempts = EndianU16_NtoB(inOutEntry->failedLoginAttempts);
		
		inOutEntry->access.maxMinutesUntilChangePassword = EndianU32_NtoB(inOutEntry->access.maxMinutesUntilChangePassword);
		inOutEntry->access.maxMinutesUntilDisabled = EndianU32_NtoB(inOutEntry->access.maxMinutesUntilDisabled);
		inOutEntry->access.maxMinutesOfNonUse = EndianU32_NtoB(inOutEntry->access.maxMinutesOfNonUse);
		inOutEntry->access.maxFailedLoginAttempts = EndianU16_NtoB(inOutEntry->access.maxFailedLoginAttempts);
		inOutEntry->access.minChars = EndianU16_NtoB(inOutEntry->access.minChars);
		inOutEntry->access.maxChars = EndianU16_NtoB(inOutEntry->access.maxChars);
		
		inOutEntry->disableReason = (PWDisableReasonCode) EndianS32_NtoB(inOutEntry->disableReason);

		inOutEntry->extraAccess.minutesUntilFailedLoginReset = EndianU32_NtoB(inOutEntry->extraAccess.minutesUntilFailedLoginReset);
		inOutEntry->extraAccess.notGuessablePattern = EndianU32_NtoB(inOutEntry->extraAccess.notGuessablePattern);
		inOutEntry->extraAccess.logOffTime = EndianU32_NtoB(inOutEntry->extraAccess.logOffTime);
		inOutEntry->extraAccess.kickOffTime = EndianU32_NtoB(inOutEntry->extraAccess.kickOffTime);

		inOutEntry->changeTransactionID = EndianS64_NtoB(inOutEntry->changeTransactionID);
	}
#endif
}


#pragma mark -
#pragma mark HASH UTILS
#pragma mark -

// --------------------------------------------------------------------------------
//	AddHashesToPWRecord
//
//	inRealm				 ->		the realm to use for the DIGEST-MD5 hash
//	inOutPasswordRec	<->		in clear-text, out hash values
//	Takes the clear-text password and adds the hashes for auth methods
// --------------------------------------------------------------------------------

void pwsf_AddHashesToPWRecord( const char *inRealm, bool inAddNT, bool inAddLM, PWFileEntry *inOutPasswordRec )
{
	unsigned char smbntHash[32];
	unsigned char smblmHash[16];
	
	// SMB-NT			[ 0 ]
	if ( inAddNT )
	{
		pwsf_CalculateSMBNTHash(inOutPasswordRec->passwordStr, smbntHash);
		strcpy( inOutPasswordRec->digest[0].method, kSMBNTStorageTag );
		inOutPasswordRec->digest[0].digest[0] = 64;
		ConvertBinaryToHex( smbntHash, 32, &inOutPasswordRec->digest[0].digest[1] );
	}
	else
	{
		bzero( &inOutPasswordRec->digest[0], sizeof(inOutPasswordRec->digest[0]) );
	}
	
	// SMB-LAN-MANAGER	[ 1 ]
	if ( inAddLM )
	{
		pwsf_CalculateSMBLANManagerHash(inOutPasswordRec->passwordStr, smblmHash);
		strcpy( inOutPasswordRec->digest[1].method, "*cmusaslsecretSMBLM" );
		inOutPasswordRec->digest[1].digest[0] = 32;
		ConvertBinaryToHex( smblmHash, 16, &inOutPasswordRec->digest[1].digest[1] );
	}
	else
	{
		bzero( &inOutPasswordRec->digest[1], sizeof(inOutPasswordRec->digest[1]) );
	}
	
	// DIGEST-MD5		[ 2 ]
	pwsf_addHashDigestMD5( inRealm, inOutPasswordRec );
	
	// CRAM-MD5			[ 3 ]
	pwsf_addHashCramMD5( inOutPasswordRec );
	
	// KERBEROS			[ 4 ]
	// Kerberos doesn't currently store a hash here, we just store the realm name.
	// combined with the user name, we can call the KDC to get the kerberos hashes
	
	// KERBEROS_NAME	[ 5 ]
	// The principal name to use
	
	// SALTED_SHA1		[ 6 ]
	pwsf_addHashSaltedSHA1( inOutPasswordRec );
}


// ----------------------------------------------------------------------------------------
//  pwsf_getHashCramMD5
// ----------------------------------------------------------------------------------------

void
pwsf_getHashCramMD5(const unsigned char *inPassword, size_t inPasswordLen, unsigned char *outHash, size_t *outHashLen )
{
	HMAC_MD5_STATE state;
	
	if ( inPassword == NULL || outHash == NULL || outHashLen == NULL )
		return;
	
	pwsf_hmac_md5_precalc( &state, inPassword, inPasswordLen );
	*outHashLen = sizeof(HMAC_MD5_STATE);
	memcpy( outHash, &state, sizeof(HMAC_MD5_STATE) );
}


// ----------------------------------------------------------------------------------------
//  pwsf_getSaltedSHA1
// ----------------------------------------------------------------------------------------

void
pwsf_getSaltedSHA1(const unsigned char *inPassword, size_t inPasswordLen, unsigned char *outHash, size_t *outHashLen)
{
	CC_SHA1_CTX ctx;
	unsigned long salt = pwsf_getRandom();
	
	if ( inPassword == NULL || outHash == NULL || outHashLen == NULL )
		return;
	
	CC_SHA1_Init(&ctx);
	CC_SHA1_Update(&ctx, &salt, 4);
	CC_SHA1_Update(&ctx, inPassword, (CC_LONG)inPasswordLen);
	CC_SHA1_Final(outHash + 4, &ctx);
	memcpy(outHash, &salt, 4);
	*outHashLen = (4 + CC_SHA1_DIGEST_LENGTH);
}


//----------------------------------------------------------------------------------------------------
//	pwsf_addHashDigestMD5
//----------------------------------------------------------------------------------------------------

void pwsf_addHashDigestMD5( const char *inRealm, PWFileEntry *inOutPasswordRec )
{
	long pwLen;
	HASH HA1;
	char userID[35];
		
	// DIGEST-MD5		[ 2 ]
	pwLen = strlen(inOutPasswordRec->passwordStr);
	
	pwsf_passwordRecRefToString( inOutPasswordRec, userID );
	DigestCalcSecret( (unsigned char *)userID,
						(unsigned char *)inRealm,
						(unsigned char *)inOutPasswordRec->passwordStr,
						pwLen,
						HA1 );
	
	/*
		* A1 = { H( { username-value, ":", realm-value, ":", passwd } ),
		* ":", nonce-value, ":", cnonce-value }
		*/
	
	// not enough room to store "*cmusaslsecretDIGEST-MD5" so truncate to 20 chars
	strncpy( inOutPasswordRec->digest[kPWHashSlotDIGEST_MD5].method, "*cmusaslsecretDIGEST-MD5", SASL_MECHNAMEMAX );
	inOutPasswordRec->digest[kPWHashSlotDIGEST_MD5].method[SASL_MECHNAMEMAX] = '\0';
	inOutPasswordRec->digest[kPWHashSlotDIGEST_MD5].digest[0] = HASHLEN;
	memcpy( &inOutPasswordRec->digest[kPWHashSlotDIGEST_MD5].digest[1], HA1, HASHLEN );
}


//----------------------------------------------------------------------------------------------------
//	pwsf_addHashCramMD5
//----------------------------------------------------------------------------------------------------

void pwsf_addHashCramMD5( PWFileEntry *inOutPasswordRec )
{
	unsigned long pwLen;
	
	// CRAM-MD5			[ 3 ]
	strncpy( inOutPasswordRec->digest[kPWHashSlotCRAM_MD5].method, "*cmusaslsecretCRAM-MD5", SASL_MECHNAMEMAX );
	inOutPasswordRec->digest[kPWHashSlotCRAM_MD5].method[SASL_MECHNAMEMAX] = '\0';
	
	pwsf_getHashCramMD5( (unsigned char *)inOutPasswordRec->passwordStr,
						  strlen(inOutPasswordRec->passwordStr),
						  (unsigned char *)&inOutPasswordRec->digest[kPWHashSlotCRAM_MD5].digest[1],
						  &pwLen );
						  
	inOutPasswordRec->digest[kPWHashSlotCRAM_MD5].digest[0] = (unsigned char)pwLen;
}


//----------------------------------------------------------------------------------------------------
//	pwsf_addHashSaltedSHA1
//----------------------------------------------------------------------------------------------------

void pwsf_addHashSaltedSHA1( PWFileEntry *inOutPasswordRec )
{
	unsigned long pwLen = 0;

	if ( inOutPasswordRec == NULL )
		return;
	
	strcpy( inOutPasswordRec->digest[kPWHashSlotSALTED_SHA1].method, "*cmusaslsecretPPS" );
	pwsf_getSaltedSHA1( (const unsigned char *)inOutPasswordRec->passwordStr, strlen(inOutPasswordRec->passwordStr),
						(unsigned char *)&inOutPasswordRec->digest[kPWHashSlotSALTED_SHA1].digest[1],
						&pwLen );
	
	inOutPasswordRec->digest[kPWHashSlotSALTED_SHA1].digest[0] = (unsigned char)pwLen;
}


#pragma mark -
#pragma mark POLICY UTILS
#pragma mark -

//------------------------------------------------------------------------------------
//	pwsf_TestDisabledStatus
//
//	Returns: kAuthUserDisabled or kAuthOK
//
//  <inOutFailedLoginAttempts> is set to 0 if the failed login count is exceeded.
//------------------------------------------------------------------------------------

int pwsf_TestDisabledStatus( PWAccessFeatures *inAccess, PWGlobalAccessFeatures *inGAccess, struct tm *inCreationDate, struct tm *inLastLoginTime, UInt16 *inOutFailedLoginAttempts )
{
	PWDisableReasonCode ignored;
    BSDTimeStructCopy creationDate;
    BSDTimeStructCopy lastLoginTime;
    
    StructTM2BSDTimeStructCopy( inCreationDate, &creationDate );
    StructTM2BSDTimeStructCopy( inLastLoginTime, &lastLoginTime );
	
	return pwsf_TestDisabledStatusWithReasonCode( inAccess, inGAccess, &creationDate, &lastLoginTime,
                                                  inOutFailedLoginAttempts, &ignored );
}


//------------------------------------------------------------------------------------
//	pwsf_TestDisabledStatus
//
//	Returns: kAuthUserDisabled or kAuthOK
//
//  <inOutFailedLoginAttempts> is set to 0 if the failed login count is exceeded.
//------------------------------------------------------------------------------------
int pwsf_TestDisabledStatusPWS( PWAccessFeatures *inAccess, PWGlobalAccessFeatures *inGAccess, BSDTimeStructCopy *inCreationDate, BSDTimeStructCopy *inLastLoginTime, UInt16 *inOutFailedLoginAttempts )
{
	PWDisableReasonCode ignored;
	
	return pwsf_TestDisabledStatusWithReasonCode( inAccess, inGAccess, inCreationDate, inLastLoginTime,
                                                  inOutFailedLoginAttempts, &ignored );
}


//------------------------------------------------------------------------------------
//	pwsf_TestDisabledStatusWithReasonCode
//
//	Returns: kAuthUserDisabled or kAuthOK
//
//  <inOutFailedLoginAttempts> is set to 0 if the failed login count is exceeded.
//  <outReasonCode> is only valid if the return value is kAuthUserDisabled.
//------------------------------------------------------------------------------------

int pwsf_TestDisabledStatusWithReasonCode( PWAccessFeatures *inAccess, PWGlobalAccessFeatures *inGAccess, BSDTimeStructCopy *inCreationDate, BSDTimeStructCopy *inLastLoginTime, UInt16 *inOutFailedLoginAttempts, PWDisableReasonCode *outReasonCode )
{
	bool setToDisabled = false;
	
	*outReasonCode = kPWDisabledNotSet;
	
    // test policies in the user record
	if ( inAccess->maxFailedLoginAttempts > 0 )
	{
		if ( *inOutFailedLoginAttempts >= inAccess->maxFailedLoginAttempts )
		{
			// for failed login attempts, if the maximum is exceeded, set the isDisabled flag on the record
			// and reset <maxFailedLoginAttempts> so that the account can be re-enabled later.
			*inOutFailedLoginAttempts = 0;
			*outReasonCode = kPWDisabledTooManyFailedLogins;
			setToDisabled = true;
		}
    }
	else
	// test policies in the global record
    if ( inGAccess->maxFailedLoginAttempts > 0 &&
         *inOutFailedLoginAttempts >= inGAccess->maxFailedLoginAttempts )
    {
        // for failed login attempts, if the maximum is exceeded, set the isDisabled flag on the record
        // and reset <maxFailedLoginAttempts> so that the account can be re-enabled later.
		*inOutFailedLoginAttempts = 0;
		*outReasonCode = kPWDisabledTooManyFailedLogins;
		setToDisabled = true;
    }
	
	// usingHardExpirationDate
	if ( inAccess->usingHardExpirationDate )
	{
		if ( TimeIsStale(&(inAccess->hardExpireDateGMT)) )
		{
			*outReasonCode = kPWDisabledExpired;
			setToDisabled = true;
		}
	}
	else
	if ( inGAccess->usingHardExpirationDate && TimeIsStale(&inGAccess->hardExpireDateGMT) )
	{
		*outReasonCode = kPWDisabledExpired;
		setToDisabled = true;
	}
	
	// maxMinutesUntilDisabled
	if ( inAccess->maxMinutesUntilDisabled > 0 )
	{
		if ( LoginTimeIsStale(inCreationDate, inAccess->maxMinutesUntilDisabled) )
		{
			*outReasonCode = kPWDisabledExpired;
			setToDisabled = true;
		}
	}
	else
	if ( inGAccess->maxMinutesUntilDisabled > 0 && LoginTimeIsStale(inCreationDate, inGAccess->maxMinutesUntilDisabled) )
	{
		*outReasonCode = kPWDisabledExpired;
		setToDisabled = true;
	}
	
	if ( inAccess->maxMinutesOfNonUse > 0 )
	{
		if ( LoginTimeIsStale( inLastLoginTime, inAccess->maxMinutesOfNonUse) )
		{
			*outReasonCode = kPWDisabledInactive;
			setToDisabled = true;
		}
	}
	else
    if ( inGAccess->maxMinutesOfNonUse > 0 &&
		 LoginTimeIsStale( inLastLoginTime, inGAccess->maxMinutesOfNonUse) )
    {
		*outReasonCode = kPWDisabledInactive;
		setToDisabled = true;
    }
	
	return ( setToDisabled ? kAuthUserDisabled : kAuthOK );
}


//------------------------------------------------------------------------------------------------
//	pwsf_ChangePasswordStatus
//
//	Returns: kAuthOK, kAuthPasswordNeedsChange, kAuthPasswordExpired
//------------------------------------------------------------------------------------------------

int pwsf_ChangePasswordStatus( PWAccessFeatures *inAccess, PWGlobalAccessFeatures *inGAccess, struct tm *inModDateOfPassword )
{
    BSDTimeStructCopy modDateOfPassword;
    StructTM2BSDTimeStructCopy( inModDateOfPassword, &modDateOfPassword );
    return pwsf_ChangePasswordStatusPWS( inAccess, inGAccess, &modDateOfPassword );
}


//------------------------------------------------------------------------------------------------
//	pwsf_ChangePasswordStatusPWS
//
//	Returns: kAuthOK, kAuthPasswordNeedsChange, kAuthPasswordExpired
//------------------------------------------------------------------------------------------------

int pwsf_ChangePasswordStatusPWS( PWAccessFeatures *inAccess, PWGlobalAccessFeatures *inGAccess, BSDTimeStructCopy *inModDateOfPassword )
{
	bool needsChange = false;
	
	if ( inAccess->newPasswordRequired )
	{
		needsChange = true;
	}
	else
	{
		// usingExpirationDate
		if ( inAccess->usingExpirationDate )
		{
			if ( TimeIsStale( &inAccess->expirationDateGMT ) )
				needsChange = true;
		}
		else
		if ( inGAccess->usingExpirationDate && TimeIsStale( &inGAccess->expirationDateGMT ) )
		{
			needsChange = true;
		}
		
		// maxMinutesUntilChangePassword
		if ( inAccess->maxMinutesUntilChangePassword > 0 )
		{
			if ( LoginTimeIsStale( inModDateOfPassword, inAccess->maxMinutesUntilChangePassword ) )
				needsChange = true;
		}
		else
		if ( inGAccess->maxMinutesUntilChangePassword > 0 && LoginTimeIsStale( inModDateOfPassword, inGAccess->maxMinutesUntilChangePassword ) )
		{
			needsChange = true;
		}
	}
	
	if ( needsChange )
    {
        if ( inAccess->canModifyPasswordforSelf )
            return kAuthPasswordNeedsChange;
        else
        	return kAuthPasswordExpired;
    }
    
    // not implemented
    return kAuthOK;
}


//------------------------------------------------------------------------------------------------
//	pwsf_RequiredCharacterStatus
//
//	Returns: enum of Reposonse Codes (CAuthFileCPP.h)
//------------------------------------------------------------------------------------------------

int pwsf_RequiredCharacterStatus(PWAccessFeatures *access, PWGlobalAccessFeatures *inGAccess, const char *inUsername, const char *inPassword)
{
    Boolean requiresAlpha = (access->requiresAlpha || inGAccess->requiresAlpha );
    Boolean requiresNumeric = (access->requiresNumeric || inGAccess->requiresNumeric );
    UInt16 minChars = (access->minChars > 0) ? access->minChars : inGAccess->minChars;
    UInt16 maxChars = (access->maxChars > 0) ? access->maxChars : inGAccess->maxChars;
	Boolean passwordCannotBeName = (access->passwordCannotBeName || inGAccess->passwordCannotBeName );
    UInt16 len;
	Boolean passwordIsEmptySubstituteString = false;
	
	if ( inPassword == NULL )
		return kAuthPasswordTooShort;
		
	len = strlen(inPassword);
    passwordIsEmptySubstituteString = (strcmp(inPassword, kEmptyPasswordAltStr) == 0);
	
	// The password server is not accepting blank passwords because some auth 
	// methods, such as DIGEST-MD5, will not authenticate them.
	if ( len == 0 )
		return kAuthPasswordTooShort;
	
    if ( len < minChars || (minChars > 0 && passwordIsEmptySubstituteString) )
        return kAuthPasswordTooShort;
    
    if ( maxChars > 0 && len > maxChars && !passwordIsEmptySubstituteString )
        return kAuthPasswordTooLong;
    
    if ( requiresAlpha )
    {
        Boolean hasAlpha = false;
        
		if ( !passwordIsEmptySubstituteString )
		{
			for ( int index = 0; index < len; index++ )
			{
				if ( isalpha(inPassword[index]) )
				{
					hasAlpha = true;
					break;
				}
			}
        }
		
        if ( !hasAlpha )
            return kAuthPasswordNeedsAlpha;
    }
	
    if ( requiresNumeric )
    {
        Boolean hasDecimal = false;
        
        if ( !passwordIsEmptySubstituteString )
		{
			for ( int index = 0; index < len; index++ )
			{
				if ( isdigit(inPassword[index]) )
				{
					hasDecimal = true;
					break;
				}
			}
        }
		
        if ( !hasDecimal )
            return kAuthPasswordNeedsDecimal;
    }
	
	if ( passwordCannotBeName )
	{
		UInt16 unameLen = strlen( inUsername );
		UInt16 smallerLen = ((len < unameLen) ? len : unameLen);
		
		// disallow the smaller substring, case-insensitive
		if ( strncasecmp( inPassword, inUsername, smallerLen ) == 0 )
			return kAuthPasswordCannotBeUsername;
	}
	
    return kAuthOK;
}


//------------------------------------------------------------------------------------------------
//	pwsf_RequiredCharacterStatusExtra
//
//	Returns: enum of Response Codes (CAuthFileCPP.h)
//------------------------------------------------------------------------------------------------

int pwsf_RequiredCharacterStatusExtra(PWAccessFeatures *access, PWGlobalAccessFeatures *inGAccess, const char *inUsername, const char *inPassword, PWMoreAccessFeatures *inExtraFeatures )
{
	int responseCode;
	int index;
	
	responseCode = pwsf_RequiredCharacterStatus( access, inGAccess, inUsername, inPassword );
	if ( responseCode != kAuthOK )
		return responseCode;
	
	UInt16 len = strlen( inPassword );
	
	if ( inGAccess->requiresMixedCase || inExtraFeatures->requiresMixedCase )
	{
		Boolean hasUpper = false;
        Boolean hasLower = false;
   
		if ( strcmp(inPassword, kEmptyPasswordAltStr) != 0 )
		{
			for ( index = 0; index < len; index++ )
			{
				if ( inPassword[index] >= 'A' && inPassword[index] <= 'Z' )
					hasUpper = true;
				else
				if ( inPassword[index] >= 'a' && inPassword[index] <= 'z' )
					hasLower = true;
				
				if ( hasUpper && hasLower )
					break;
			}
        }
		
        if ( !(hasUpper && hasLower) )
            return kAuthPasswordNeedsMixedCase;
	}
	
	if ( inGAccess->requiresSymbol || inExtraFeatures->requiresSymbol )
	{
		Boolean hasSymbol = false;
			
		if ( strcmp(inPassword, kEmptyPasswordAltStr) != 0 )
		{
			for ( index = 0; index < len; index++ )
			{
				if ( inPassword[index] >= 'A' && inPassword[index] <= 'Z' )
					continue;
				if ( inPassword[index] >= 'a' && inPassword[index] <= 'z' )
					continue;
				if ( inPassword[index] >= '0' && inPassword[index] <= '9' )
					continue;
				
				hasSymbol = true;
				break;
			}
        }
		
        if ( !hasSymbol )
            return kAuthPasswordNeedsSymbol;
	}
	
	/*if ( inGAccess->notGuessablePattern || inExtraFeatures->notGuessablePattern )
	{
	}*/
	
	return kAuthOK;
}


#pragma mark -

//----------------------------------------------------------------------------------------------------
//	pwsf_slotToOffset
//
//	Returns: the position in the database file (from SEEK_SET) for the slot
//----------------------------------------------------------------------------------------------------

off_t pwsf_slotToOffset(uint32_t slot)
{
   	return sizeof(PWFileHeader) + (slot - 1) * sizeof(PWFileEntry);
}


//----------------------------------------------------------------------------------------------------
//	getGMTime
//
//	Returns: a time struct based on GMT
//----------------------------------------------------------------------------------------------------

void pwsf_getGMTime(BSDTimeStructCopy *outGMT)
{
    if ( outGMT != NULL )
    {
        time_t theTime;
    
        time(&theTime);
        BSDTimeStructCopy_gmtime_r(&theTime, outGMT);
    }
}


//----------------------------------------------------------------------------------------------------
//	getTimeForRef
//
//	Returns: a timestamp based on GMT
//----------------------------------------------------------------------------------------------------

unsigned long pwsf_getTimeForRef(void)
{
    time_t theTime;
    
    time(&theTime);
    return (unsigned long)theTime;
}


//----------------------------------------------------------------------------------------------------
//	getRandom
//
//	Returns: a random number for user IDs
//----------------------------------------------------------------------------------------------------

unsigned long pwsf_getRandom(void)
{
	unsigned long result;
	unsigned long uiNow;
	time_t now;
	
	result = (unsigned long) random();
	
	time(&now);
	uiNow = now + result;
	srandom((unsigned int)uiNow);
	
	return result;
}


//-----------------------------------------------------------------------------
//	ConvertBinaryToHex
//-----------------------------------------------------------------------------

bool pwsf_ConvertBinaryToHex( const unsigned char *inData, long len, char *outHexStr )
{
    bool result = true;
	char *tptr = outHexStr;
	static const char base16table[16] = { '0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f' };
	
    if ( inData == nil || outHexStr == nil )
        return false;
    
	for ( int idx = 0; idx < len; idx++ )
	{
		*tptr++ = base16table[(inData[idx] >> 4) & 0x0F];
		*tptr++ = base16table[(inData[idx] & 0x0F)];
	}
	*tptr = '\0';
		
	return result;
}


//----------------------------------------------------------------------------------------------------
//	pwsf_passwordRecRefToString
//----------------------------------------------------------------------------------------------------

void pwsf_passwordRecRefToString(PWFileEntry *inPasswordRec, char *outRefStr)
{
#if TARGET_RT_BIG_ENDIAN
	pwsf_ConvertBinaryToHex( (const unsigned char *)inPasswordRec, 16, outRefStr + 2 );
	*((uint16_t *)outRefStr) = 0x3078;
#else
	sprintf( outRefStr,
#ifdef __LP64__
			"0x%.8x%.8x%.8x%.8x",
#else
			"0x%.8lx%.8lx%.8lx%.8lx",
#endif
			inPasswordRec->time,
			inPasswordRec->rnd,
			inPasswordRec->sequenceNumber,
			inPasswordRec->slot );

//	*((uint16_t *)outRefStr) = 0x7830;
#endif
}


//-----------------------------------------------------------------------------
//	ConvertHexToBinaryFastWithNtoHL
//-----------------------------------------------------------------------------

static inline void ConvertHexToBinaryFastWithNtoHL( const char *inHex, int len, unsigned char *outBin )
{
	static const unsigned char nibbleTable[] = {	0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
													0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 
													0xA, 0xB, 0xC, 0xD, 0xE, 0xF,
													0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
													0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
													0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60,
													0xA, 0xB, 0xC, 0xD, 0xE, 0xF };

	register int idx;
	register unsigned char *tptr = outBin;

#if TARGET_RT_BIG_ENDIAN
	for ( idx = 0; idx < len; idx++ ) {
		*tptr = nibbleTable[(int)(*inHex++) - 0x30] << 4;
		*tptr++ |= nibbleTable[(int)(*inHex++) - 0x30];
	}
#else
	register int idx2;
	const int lc = len / (int)(sizeof(UInt32)*2);
	
	tptr += sizeof(UInt32) - 1;
	for ( idx2 = 0; idx2 < lc; idx2++ ) {
		for ( idx = 0; idx < (int)sizeof(UInt32); idx++ ) {
			*tptr = nibbleTable[(int)(*inHex++) - 0x30] << 4;
			*tptr-- |= nibbleTable[(int)(*inHex++) - 0x30];
		}
		tptr += sizeof(UInt32) * 2;
	}
#endif
}


//----------------------------------------------------------------------------------------------------
//	pwsf_stringToPasswordRecRef
//
//	Returns: Boolean (1==valid ref, 0==fail)
//----------------------------------------------------------------------------------------------------

int pwsf_stringToPasswordRecRef(const char *inRefStr, PWFileEntry *outPasswordRec)
{
    int result = false;
    
    // invalid slot value
    outPasswordRec->slot = 0;    
    if ( *inRefStr++ == '0' && *inRefStr++ == 'x' )
    {
        ConvertHexToBinaryFastWithNtoHL( inRefStr, 32, (unsigned char *)outPasswordRec );
        result = true;
    }
    
    return result;
}


//----------------------------------------------------------------------------------------------------
//	pwsf_compress_header
//----------------------------------------------------------------------------------------------------

int pwsf_compress_header( PWFileHeader *inHeader, unsigned char **outCompressedHeader, size_t *outCompressedHeaderLength )
{
	PWFileHeaderCompressed *outPWHeader = NULL;
	
	outPWHeader = (PWFileHeaderCompressed *) calloc( 1, sizeof(PWFileHeaderCompressed) );
	if ( outPWHeader == NULL )
		return -1;
	
#if TARGET_RT_BIG_ENDIAN
	outPWHeader->signature = inHeader->signature;
	outPWHeader->version = inHeader->version;
	outPWHeader->entrySize = inHeader->entrySize;
	outPWHeader->sequenceNumber = inHeader->sequenceNumber;
	outPWHeader->numberOfSlotsCurrentlyInFile = inHeader->numberOfSlotsCurrentlyInFile;
	outPWHeader->deepestSlotUsed = inHeader->deepestSlotUsed;
	memcpy( &outPWHeader->access, &inHeader->access, sizeof(PWGlobalAccessFeatures) );
	memcpy( &outPWHeader->weakAuthMethods, &inHeader->weakAuthMethods, sizeof(AuthMethName) * kPWFileMaxWeakMethods );
	strlcpy( outPWHeader->replicationName, inHeader->replicationName, kPWFileMaxReplicaName );
	outPWHeader->deepestSlotUsedByThisServer = inHeader->deepestSlotUsedByThisServer;
	outPWHeader->accessModDate = inHeader->accessModDate;
	memcpy( &outPWHeader->extraAccess, &inHeader->extraAccess, sizeof(PWGlobalMoreAccessFeatures) );
#else
	PWFileHeader bigEndianHeader = *inHeader;
		
	pwsf_EndianAdjustPWFileHeader( &bigEndianHeader, 0 );
	
	outPWHeader->signature = bigEndianHeader.signature;
	outPWHeader->version = bigEndianHeader.version;
	outPWHeader->entrySize = bigEndianHeader.entrySize;
	outPWHeader->sequenceNumber = bigEndianHeader.sequenceNumber;
	outPWHeader->numberOfSlotsCurrentlyInFile = bigEndianHeader.numberOfSlotsCurrentlyInFile;
	outPWHeader->deepestSlotUsed = bigEndianHeader.deepestSlotUsed;
	memcpy( &outPWHeader->access, &bigEndianHeader.access, sizeof(PWGlobalAccessFeatures) );
	memcpy( &outPWHeader->weakAuthMethods, &bigEndianHeader.weakAuthMethods, sizeof(AuthMethName) * kPWFileMaxWeakMethods );
	strlcpy( outPWHeader->replicationName, bigEndianHeader.replicationName, kPWFileMaxReplicaName );
	outPWHeader->deepestSlotUsedByThisServer = bigEndianHeader.deepestSlotUsedByThisServer;
	outPWHeader->accessModDate = bigEndianHeader.accessModDate;
	memcpy( &outPWHeader->extraAccess, &bigEndianHeader.extraAccess, sizeof(PWGlobalMoreAccessFeatures) );
#endif
	
	*outCompressedHeader = (unsigned char *)outPWHeader;
	*outCompressedHeaderLength = sizeof(PWFileHeaderCompressed);
	
	return 0;
}


//----------------------------------------------------------------------------------------------------
//	pwsf_expand_header
//----------------------------------------------------------------------------------------------------

int pwsf_expand_header( const unsigned char *inCompressedHeader, size_t inCompressedHeaderLength, PWFileHeader *outHeader )
{
	PWFileHeaderCompressed *cHeader = (PWFileHeaderCompressed *)inCompressedHeader;
	
	bzero( outHeader, sizeof(PWFileHeader) );
	
	outHeader->signature = cHeader->signature;
	outHeader->version = cHeader->version;
	outHeader->entrySize = cHeader->entrySize;
	outHeader->sequenceNumber = cHeader->sequenceNumber;
	outHeader->numberOfSlotsCurrentlyInFile = cHeader->numberOfSlotsCurrentlyInFile;
	outHeader->deepestSlotUsed = cHeader->deepestSlotUsed;
	memcpy( &outHeader->access, &cHeader->access, sizeof(PWGlobalAccessFeatures) );
	memcpy( &outHeader->weakAuthMethods, &cHeader->weakAuthMethods, sizeof(AuthMethName) * kPWFileMaxWeakMethods );
	strlcpy( outHeader->replicationName, cHeader->replicationName, kPWFileMaxReplicaName );
	outHeader->deepestSlotUsedByThisServer = cHeader->deepestSlotUsedByThisServer;
	outHeader->accessModDate = cHeader->accessModDate;
	memcpy( &outHeader->extraAccess, &cHeader->extraAccess, sizeof(PWGlobalMoreAccessFeatures) );
	
	pwsf_EndianAdjustPWFileHeader( outHeader, 1 );
						  
	return 0;
}


//----------------------------------------------------------------------------------------------------
//	pwsf_compress_slot
//----------------------------------------------------------------------------------------------------

int pwsf_compress_slot( PWFileEntry *inPasswordRec, unsigned char **outCompressedRecord, size_t *outCompressedRecordLength )
{
	PWFileEntryCompressed *outPWRec = NULL;
	size_t requiredDigestLength = sizeof(UInt16);		// 2 bytes for the in-use flags
	size_t requiredLength = 0;
	unsigned long len;
	UInt16 digestFlags = 0x000;
	SInt16 groupCount = 0;
	size_t usernameStrLen = 0;
	char *tptr = NULL;
	uuid_t *groupList = NULL;
	int idx;
	PWFileEntry bigEndianEntry = *inPasswordRec;
	
	pwsf_EndianAdjustPWFileEntry( &bigEndianEntry, kPWByteOrderDiskAndNet );
	
	for ( idx = kPWHashSlotSMB_NT; idx <= kPWHashSlotSALTED_SHA1; idx++ )
	{
		if ( bigEndianEntry.digest[idx].method[0] != '\0' )
		{
			digestFlags |= (1 << idx);
			switch( idx )
			{
				case kPWHashSlotSMB_NT:
					requiredDigestLength += 32;
					break;
				case kPWHashSlotSMB_LAN_MANAGER:
				case kPWHashSlotDIGEST_MD5:
					requiredDigestLength += 16;
					break;
				
				case kPWHashSlotCRAM_MD5:
					requiredDigestLength += 32;
					break;
				
				case kPWHashSlotKERBEROS:
				case kPWHashSlotKERBEROS_NAME:
					requiredDigestLength += strlen(bigEndianEntry.digest[idx].digest) + 1;
					break;
				
				case kPWHashSlotSALTED_SHA1:
					requiredDigestLength += 24;
					break;
			}
		}
	}
	
	usernameStrLen = strlen(bigEndianEntry.usernameStr);
	
	if ( bigEndianEntry.access.isAdminUser )
		groupCount = pwsf_GetGroupList( inPasswordRec, &groupList );	
	
	requiredLength = sizeof(PWFileEntryCompressed)
						+ sizeof(bigEndianEntry.passwordStr)
						+ usernameStrLen + 1
						+ requiredDigestLength;
	
	if ( groupCount > 0 )
		requiredLength += groupCount * sizeof(uuid_t);
	
	outPWRec = (PWFileEntryCompressed *) calloc( 1, requiredLength );
	if ( outPWRec == NULL )
		return -1;
	
	outPWRec->time = bigEndianEntry.time;
	outPWRec->rnd = bigEndianEntry.rnd;
	outPWRec->sequenceNumber = bigEndianEntry.sequenceNumber;
	outPWRec->slot = bigEndianEntry.slot;
	
	// times need to be converted from host byte order
	outPWRec->creationDate = htonl( (uint32_t)BSDTimeStructCopy_timegm(&inPasswordRec->creationDate) );
	outPWRec->modificationDate = htonl( (uint32_t)BSDTimeStructCopy_timegm(&inPasswordRec->modificationDate) );
	outPWRec->modDateOfPassword = htonl( (uint32_t)BSDTimeStructCopy_timegm(&inPasswordRec->modDateOfPassword) );
	outPWRec->lastLogin = htonl( (uint32_t)BSDTimeStructCopy_timegm(&inPasswordRec->lastLogin) );
	
	outPWRec->failedLoginAttempts = bigEndianEntry.failedLoginAttempts;
	
	// copy over the bit fields in two bytes
	memcpy( &outPWRec->access, &bigEndianEntry.access, sizeof(char) * 2 );
	
	// times need to be converted from host byte order
	outPWRec->access.expirationDateGMT = htonl( (uint32_t)BSDTimeStructCopy_timegm(&inPasswordRec->access.expirationDateGMT) );
	outPWRec->access.hardExpireDateGMT = htonl( (uint32_t)BSDTimeStructCopy_timegm(&inPasswordRec->access.hardExpireDateGMT) );
	
	outPWRec->access.maxMinutesUntilChangePassword = bigEndianEntry.access.maxMinutesUntilChangePassword;
	outPWRec->access.maxMinutesUntilDisabled = bigEndianEntry.access.maxMinutesUntilDisabled;
	outPWRec->access.maxMinutesOfNonUse = bigEndianEntry.access.maxMinutesOfNonUse;
	outPWRec->access.maxFailedLoginAttempts = bigEndianEntry.access.maxFailedLoginAttempts;
	outPWRec->access.minChars = bigEndianEntry.access.minChars;
	outPWRec->access.maxChars = bigEndianEntry.access.maxChars;
	
	memcpy( outPWRec->userGUID, bigEndianEntry.userGUID, sizeof(uuid_t) );
	memcpy( outPWRec->userdata, bigEndianEntry.userdata, sizeof(bigEndianEntry.userdata) );
	outPWRec->disableReason = bigEndianEntry.disableReason;
	outPWRec->extraAccess = bigEndianEntry.extraAccess;
	
	tptr = outPWRec->data;
	memcpy( tptr, bigEndianEntry.passwordStr, sizeof(bigEndianEntry.passwordStr) );
	tptr += sizeof(bigEndianEntry.passwordStr);
	memcpy( tptr, bigEndianEntry.usernameStr, usernameStrLen + 1 );
	tptr += usernameStrLen + 1;
	
	*((UInt16 *)tptr) = htons(digestFlags);
	tptr += sizeof(UInt16);
	
	for ( idx = kPWHashSlotSMB_NT; idx <= kPWHashSlotSALTED_SHA1; idx++ )
	{
		if ( bigEndianEntry.digest[idx].method[0] != '\0' )
		{
			switch( idx )
			{
				case kPWHashSlotSMB_NT:
					ConvertHexToBinary( &bigEndianEntry.digest[idx].digest[1], (unsigned char *)tptr, &len );
					tptr += 32;
					break;
				case kPWHashSlotSMB_LAN_MANAGER:
					ConvertHexToBinary( &bigEndianEntry.digest[idx].digest[1], (unsigned char *)tptr, &len );
					tptr += 16;
					break;
					
				case kPWHashSlotDIGEST_MD5:
					memcpy( tptr, &bigEndianEntry.digest[idx].digest[1], 16 );
					tptr += 16;
					break;
				
				case kPWHashSlotCRAM_MD5:
					memcpy( tptr, &bigEndianEntry.digest[idx].digest[1], 32 );
					tptr += 32;
					break;
				
				case kPWHashSlotKERBEROS:
				case kPWHashSlotKERBEROS_NAME:
					strcpy( tptr, bigEndianEntry.digest[idx].digest );
					tptr += strlen(bigEndianEntry.digest[idx].digest) + 1;
					break;
				
				case kPWHashSlotSALTED_SHA1:
					memcpy( tptr, &bigEndianEntry.digest[idx].digest[1], 24 );
					tptr += 24;
					break;
			}
		}
	}

	// groups
	*((SInt16 *)tptr) = htons(groupCount);
	tptr += sizeof(SInt16);
	
	for ( idx = 0; idx < groupCount; idx++ )
	{
		memcpy( tptr, &groupList[idx], sizeof(uuid_t) );
		tptr += sizeof(uuid_t);
	}
	
	// set out params
	*outCompressedRecord = (unsigned char *)outPWRec;
	*outCompressedRecordLength = requiredLength;
	return 0;
}


//----------------------------------------------------------------------------------------------------
//	pwsf_expand_slot
//----------------------------------------------------------------------------------------------------

int pwsf_expand_slot( const unsigned char *inCompressedRecord, size_t inCompressedRecordLength, PWFileEntry *inOutPasswordRec )
{
	PWFileEntryCompressed *inRec = (PWFileEntryCompressed *)inCompressedRecord;
	UInt16 digestFlags = 0x000;
	SInt16 groupCount = 0;
	CFMutableDictionaryRef groupDict = NULL;
	char *tptr;
	int idx;
	char adminID[35] = {0};
	char filePath[PATH_MAX];
    time_t timetTime;
	
	if ( inCompressedRecord == NULL || inCompressedRecordLength < sizeof(PWFileEntryCompressed) )
		return -1;
	
	bzero( inOutPasswordRec, sizeof(PWFileEntry) );
	
	inOutPasswordRec->time = inRec->time;
	inOutPasswordRec->rnd = inRec->rnd;
	inOutPasswordRec->sequenceNumber = inRec->sequenceNumber;
	inOutPasswordRec->slot = inRec->slot;
	
	// convert times
	inRec->creationDate = ntohl( inRec->creationDate );
	if ( inRec->creationDate != -1 )
    {
        timetTime = (time_t)inRec->creationDate;
		BSDTimeStructCopy_gmtime_r(&timetTime, &inOutPasswordRec->creationDate);
    }

	inRec->modificationDate = ntohl( inRec->modificationDate );
	if ( inRec->modificationDate != -1 )
    {
        timetTime = (time_t)inRec->modificationDate;
		BSDTimeStructCopy_gmtime_r(&timetTime, &inOutPasswordRec->modificationDate);
    }

	inRec->modDateOfPassword = ntohl( inRec->modDateOfPassword );
	if ( inRec->modDateOfPassword != -1 )
    {
        timetTime = (time_t)inRec->modDateOfPassword;
		BSDTimeStructCopy_gmtime_r(&timetTime, &inOutPasswordRec->modDateOfPassword);
    }

	inRec->lastLogin = ntohl( inRec->lastLogin );
	if ( inRec->lastLogin != -1 )
    {
        timetTime = (time_t)inRec->lastLogin;
		BSDTimeStructCopy_gmtime_r(&timetTime, &inOutPasswordRec->lastLogin);
    }
	
	inOutPasswordRec->failedLoginAttempts = inRec->failedLoginAttempts;
	
	// copy over the bit fields in two bytes
	memcpy( &inOutPasswordRec->access, &inRec->access, sizeof(char) * 2 );
	
	// convert times
	if ( inRec->access.expirationDateGMT != -1 ) {
		inRec->access.expirationDateGMT = ntohl( inRec->access.expirationDateGMT );
		BSDTimeStructCopy_gmtime_r((const time_t*)&inRec->access.expirationDateGMT, &inOutPasswordRec->access.expirationDateGMT);
	}
	if ( inRec->access.hardExpireDateGMT != -1 ) {
		inRec->access.hardExpireDateGMT = ntohl( inRec->access.hardExpireDateGMT );
		BSDTimeStructCopy_gmtime_r((const time_t*)&inRec->access.hardExpireDateGMT, &inOutPasswordRec->access.hardExpireDateGMT);
	}
	
	inOutPasswordRec->access.maxMinutesUntilChangePassword = inRec->access.maxMinutesUntilChangePassword;
	inOutPasswordRec->access.maxMinutesUntilDisabled = inRec->access.maxMinutesUntilDisabled;
	inOutPasswordRec->access.maxMinutesOfNonUse = inRec->access.maxMinutesOfNonUse;
	inOutPasswordRec->access.maxFailedLoginAttempts = inRec->access.maxFailedLoginAttempts;
	inOutPasswordRec->access.minChars = inRec->access.minChars;
	inOutPasswordRec->access.maxChars = inRec->access.maxChars;
	
	memcpy( inOutPasswordRec->userGUID, inRec->userGUID, sizeof(uuid_t) );
	memcpy( inOutPasswordRec->userdata, inRec->userdata, sizeof(inRec->userdata) );
	inOutPasswordRec->disableReason = inRec->disableReason;
	inOutPasswordRec->extraAccess = inRec->extraAccess;
	
	tptr = inRec->data;
	memcpy( inOutPasswordRec->passwordStr, tptr, sizeof(inOutPasswordRec->passwordStr) );
	tptr += sizeof(inOutPasswordRec->passwordStr);
	strcpy( inOutPasswordRec->usernameStr, tptr );
	tptr += strlen(inOutPasswordRec->usernameStr) + 1;
	
	digestFlags = ntohs( *((UInt16 *)tptr) );
	tptr += sizeof(UInt16);
	
	for ( idx = kPWHashSlotSMB_NT; idx <= kPWHashSlotSALTED_SHA1; idx++ )
	{
		if ( (digestFlags & 0x01) != 0 )
		{
			switch( idx )
			{
				case kPWHashSlotSMB_NT:
					strcpy( inOutPasswordRec->digest[kPWHashSlotSMB_NT].method, kSMBNTStorageTag );
					ConvertBinaryToHex( (unsigned char *)tptr, 32, &inOutPasswordRec->digest[kPWHashSlotSMB_NT].digest[1] );
					inOutPasswordRec->digest[kPWHashSlotSMB_NT].digest[0] = 64;
					tptr += 32;
					break;
				
				case kPWHashSlotSMB_LAN_MANAGER:
					strcpy( inOutPasswordRec->digest[kPWHashSlotSMB_LAN_MANAGER].method, "*cmusaslsecretSMBLM" );
					ConvertBinaryToHex( (unsigned char *)tptr, 16, &inOutPasswordRec->digest[kPWHashSlotSMB_LAN_MANAGER].digest[1] );
					inOutPasswordRec->digest[kPWHashSlotSMB_LAN_MANAGER].digest[0] = 32;
					tptr += 16;
					break;
				
				case kPWHashSlotDIGEST_MD5:
					strncpy( inOutPasswordRec->digest[kPWHashSlotDIGEST_MD5].method, "*cmusaslsecretDIGEST-MD5", SASL_MECHNAMEMAX );
					memcpy( &inOutPasswordRec->digest[idx].digest[1], tptr, 16 );
					inOutPasswordRec->digest[kPWHashSlotDIGEST_MD5].digest[0] = 16;
					tptr += 16;
					break;
				
				case kPWHashSlotCRAM_MD5:
					strncpy( inOutPasswordRec->digest[idx].method, "*cmusaslsecretCRAM-MD5", SASL_MECHNAMEMAX );
					memcpy( &inOutPasswordRec->digest[idx].digest[1], tptr, 32 );
					inOutPasswordRec->digest[idx].digest[0] = 32;
					tptr += 32;
					break;
				
				case kPWHashSlotKERBEROS:
					strcpy( inOutPasswordRec->digest[idx].method, "KerberosRealmName" );
					strlcpy( inOutPasswordRec->digest[idx].digest, tptr, sizeof(inOutPasswordRec->digest[idx].digest) );
					tptr += strlen(tptr) + 1;
					break;
					
				case kPWHashSlotKERBEROS_NAME:
					strcpy( inOutPasswordRec->digest[idx].method, "KerberosPrincName" );
					strlcpy( inOutPasswordRec->digest[idx].digest, tptr, sizeof(inOutPasswordRec->digest[idx].digest) );
					tptr += strlen(tptr) + 1;
					break;
				
				case kPWHashSlotSALTED_SHA1:
					strncpy( inOutPasswordRec->digest[idx].method, "*cmusaslsecretPPS", SASL_MECHNAMEMAX );
					memcpy( &inOutPasswordRec->digest[idx].digest[1], tptr, 24 );
					inOutPasswordRec->digest[idx].digest[0] = 24;
					tptr += 24;
					break;
			}
		}
		
		digestFlags >>= 1;
	}
	
	pwsf_EndianAdjustPWFileEntryNoTimes( inOutPasswordRec, kPWByteOrderHost );
	
	// groups
	groupCount = ntohs( *((SInt16 *)tptr) );
	tptr += sizeof(SInt16);
	switch( groupCount )
	{
		case -1:
		case 0:
			inOutPasswordRec->admingroup.list_type = kPWGroupNotSet;
			break;
		
		case 1:
			inOutPasswordRec->admingroup.list_type = kPWGroupInSlot;
			memcpy( &inOutPasswordRec->admingroup.group_uuid, tptr, sizeof(uuid_t) );
			tptr += sizeof(uuid_t);
			break;
		
		default:
			inOutPasswordRec->admingroup.list_type = kPWGroupInFile;
			groupDict = pwsf_CreateAdditionalDataDictionaryWithUUIDList( groupCount, (uuid_t *)tptr );
			if ( groupDict != NULL ) {
				pwsf_passwordRecRefToString( inOutPasswordRec, adminID );
				snprintf( filePath, sizeof(filePath), "%s/%s.plist", kPWAuxDirPath, adminID );
				if ( pwsf_savexml(filePath, groupDict) != 0 ) {
					inOutPasswordRec->admingroup.list_type = kPWGroupInSlot;
					memcpy( &inOutPasswordRec->admingroup.group_uuid, tptr, sizeof(uuid_t) );
				}
			}
			tptr += groupCount * sizeof(uuid_t);
	}
	
	return 0;
}


/*----------------------------------------------------------------------------------*/

#pragma mark -
#pragma mark DES ACCESSORS
#pragma mark -


//----------------------------------------------------------------------------------------------------
//	pwsf_DESEncode
//----------------------------------------------------------------------------------------------------

void pwsf_DESEncode(void *data, unsigned long inDataLen)
{
	char *tptr = (char *)data;
        
    while ( inDataLen > 0 )
    {
        Encode(gDESKeyArray, kFixedDESChunk, tptr);
        tptr += kFixedDESChunk;
        inDataLen -= kFixedDESChunk;
    }
}


//----------------------------------------------------------------------------------------------------
//	pwsf_DESDecode
//----------------------------------------------------------------------------------------------------

void pwsf_DESDecode(void *data, unsigned long inDataLen)
{
    char *tptr = (char *)data;
    
    while ( inDataLen > 0 )
    {
        Decode(gDESKeyArray, kFixedDESChunk, tptr);
        tptr += kFixedDESChunk;
        inDataLen -= kFixedDESChunk;
    }
}


//----------------------------------------------------------------------------------------------------
//	pwsf_DESAutoDecode
//----------------------------------------------------------------------------------------------------

void pwsf_DESAutoDecode(void *data)
{
	PWFileEntry anEntry;
	unsigned long offset;
	unsigned char *dataPtr;
	
	// decrypt each block of 8
	// because decryption is expensive, look for the 0-terminator in the decrypted data
	// to determine the stopping point.
	
	for ( offset = 0; offset <= sizeof(anEntry.passwordStr) - kFixedDESChunk; offset += kFixedDESChunk )
	{
		dataPtr = (unsigned char *)data + offset;
		pwsf_DESDecode( dataPtr, kFixedDESChunk );
		
		if ( dataPtr[0] == 0 || dataPtr[1] == 0 || dataPtr[2] == 0 ||
			 dataPtr[3] == 0 || dataPtr[4] == 0 || dataPtr[5] == 0 ||
			 dataPtr[6] == 0 || dataPtr[7] == 0 )
			break;
	}
}

#pragma mark -
#pragma mark GROUP LIST HANDLERS
#pragma mark -

//------------------------------------------------------------------------------------------------
//	pwsf_is_guid
//------------------------------------------------------------------------------------------------

bool pwsf_is_guid( const char *inStr )
{
	int idx;
	
	if ( inStr != NULL && strlen(inStr) == 36 &&
		 inStr[8] == '-' && inStr[13] == '-' &&
		 inStr[18] == '-' && inStr[23] == '-' )
	{
		for ( idx = 0; idx < 8; idx++ )
			if ( !ishexnumber(inStr[idx]) )
				return false;
		for ( idx = 9; idx < 13; idx++ )
			if ( !ishexnumber(inStr[idx]) )
				return false;
		for ( idx = 14; idx < 18; idx++ )
			if ( !ishexnumber(inStr[idx]) )
				return false;
		for ( idx = 19; idx < 23; idx++ )
			if ( !ishexnumber(inStr[idx]) )
				return false;
		for ( idx = 24; idx < 36; idx++ )
			if ( !ishexnumber(inStr[idx]) )
				return false;
				
		return true;
	}
	
	return false;
}


//----------------------------------------------------------------------------------------------------
//	pwsf_uuid_for_group
//----------------------------------------------------------------------------------------------------

bool pwsf_uuid_for_group( const char *groupName, uuid_t uuid )
{
	gid_t gid = 0;
	int result = -1;
	struct group *theGroup = NULL;
	
	if ( groupName != NULL )
	{
		theGroup = getgrnam( groupName );
		if ( theGroup != NULL )
		{
			gid = theGroup->gr_gid;
			result = mbr_gid_to_uuid( gid, uuid );
		}
	}
	
	return (result == 0);
}


//----------------------------------------------------------------------------------------------------
//	pwsf_PolicyStringToGroupList
//
//	Returns: The number of groups in the list. <outGroupList> is a NULL terminated
//				array. The group count is returned as a convenience.
//				-1 is returned on error.
//----------------------------------------------------------------------------------------------------

int pwsf_PolicyStringToGroupList( const char *inString, uuid_t *outGroupList[] )
{
	uuid_t *groupArray = NULL;
	const char *groupPtr = NULL;
	char *groupStrCopy = NULL;
	char *tptr = NULL;
	char *curptr = NULL;
	int groupCount = 0;
	int idx = 0;
	uuid_t aGroupUUID;
	
	if ( inString == NULL || outGroupList == NULL )
		return -1;
	
	*outGroupList = NULL;
	
	groupPtr = strstr( inString, kPWPolicyStr_adminAuthorityGroups );
	if ( groupPtr == NULL )
		return -1;
	
	groupPtr += sizeof( kPWPolicyStr_adminAuthorityGroups );
	if ( *groupPtr == '\0' )
		return -1;
	
	groupStrCopy = strdup( groupPtr );
	if ( groupStrCopy != NULL )
	{
		// find the end of the group list and chop
		tptr = strchr( groupStrCopy, ' ' );
		if ( tptr != NULL )
			*tptr = '\0';
		
		if ( *groupStrCopy == '\0' ) {
			free( groupStrCopy );
			return 0;
		}
		
		// get a count
		for ( tptr = groupStrCopy; tptr != NULL; ) {
			groupCount++;
			tptr = strchr( tptr, ',' );
			if ( tptr != NULL )
				tptr++;
		}
		
		// create the array
		groupArray = (uuid_t *) calloc( groupCount + 1, sizeof(uuid_t) );
		if ( groupArray != NULL )
		{
			for ( tptr = groupStrCopy; tptr != NULL; )
			{
				curptr = strsep( &tptr, "," );
				if ( curptr != NULL )
				{
					if ( pwsf_is_guid(curptr) && pwsf_UUIDStrToUUID(curptr, &aGroupUUID) )
					{
						memcpy( groupArray[idx++], &aGroupUUID, sizeof(uuid_t) );
					}
					else
					if ( pwsf_uuid_for_group(curptr, aGroupUUID) )
					{
						memcpy( groupArray[idx++], &aGroupUUID, sizeof(uuid_t) );
					}
					else
					{
						// invalid group
						idx = -1;
						break;
					}
				}
			}
			
			if ( idx > 0 ) {
				*outGroupList = groupArray;
			}
			else {
				// invalid group in the list.
				free( groupArray );
				idx = -1;
			}
		}
		
		free( groupStrCopy );
	}
	
	return idx;
}


// ----------------------------------------------------------------------------------------
//  pwsf_CreateAdditionalDataDictionaryWithUUIDList
//
//	Returns: A dictionary that contains the key kPWKey_ScopeOfAuthority. The value is a
//			 CFArray of UUIDs in string form.
//
// ----------------------------------------------------------------------------------------

CFMutableDictionaryRef pwsf_CreateAdditionalDataDictionaryWithUUIDList( int uuidCount, uuid_t *uuidList )
{
	CFMutableDictionaryRef groupDict = NULL;
	CFMutableArrayRef groupArray = NULL;
	CFStringRef uuidString = NULL;
	BOOL complete = NO;
	int idx;
	
	groupDict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	if ( groupDict == NULL )
		return NULL;

	do
	{
		groupArray = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
		if ( groupArray == NULL )
			break;
		
		for ( idx = 0; idx < uuidCount; idx++ )
		{
			if ( uuidList != NULL )
			{
				uuidString = pwsf_UUIDToString( uuidList[idx] );
				if ( uuidString == NULL )
					break;
				
				CFArrayAppendValue( groupArray, uuidString );
				CFRelease( uuidString );
				uuidString = NULL;
			}
		}
		
		CFDictionarySetValue( groupDict, CFSTR(kPWKey_ScopeOfAuthority), groupArray );
		CFRelease( groupArray );
		groupArray = NULL;
		
		complete = YES;
	}
	while (0);
	
	if ( complete == NO && groupDict != NULL )
	{
		if ( groupArray != NULL ) {
			CFRelease( groupArray );
			groupArray = NULL;
		}
		
		CFRelease( groupDict );
		groupDict = NULL;
	}
	
	return groupDict;
}


// ----------------------------------------------------------------------------------------
//  pwsf_CreateAdditionalDataDictionaryWithOwners
//
//	Returns: A dictionary that contains the key kPWKey_ComputerAccountOwnerList. The value
//			 is a CFArray of slot IDs in string form.
//
// ----------------------------------------------------------------------------------------

CFMutableDictionaryRef pwsf_CreateAdditionalDataDictionaryWithOwners( const char *inSlotIDList )
{
	CFMutableDictionaryRef additionalDataDict = NULL;
	CFStringRef slotListString = NULL;
	CFArrayRef slotListArray = NULL;
	
	try
	{
		slotListString = CFStringCreateWithCString( kCFAllocatorDefault, inSlotIDList, kCFStringEncodingUTF8 );
		if ( slotListString == NULL )
			throw( 1 );
		
		slotListArray = CFStringCreateArrayBySeparatingStrings( kCFAllocatorDefault, slotListString, CFSTR(",") );
		if ( slotListArray == NULL )
			throw( 1 );
		
		additionalDataDict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
		if ( additionalDataDict == NULL )
			throw( 1 );
		
		CFDictionarySetValue( additionalDataDict, CFSTR(kPWKey_ComputerAccountOwnerList), slotListArray );
	}
	catch( ... )
	{
	}
	
	if ( slotListArray != NULL )
		CFRelease( slotListArray );
	if ( slotListString != NULL )
		CFRelease( slotListString );
	
	return additionalDataDict;
}


// ----------------------------------------------------------------------------------------
//  pwsf_UUIDToString
// ----------------------------------------------------------------------------------------

CFStringRef pwsf_UUIDToString( uuid_t uuid )
{
	CFUUIDRef uuidRef = NULL;
	CFStringRef uuidString = NULL;
	
	uuidRef = CFUUIDCreateWithBytes( kCFAllocatorDefault, uuid[0], uuid[1], uuid[2], uuid[3], uuid[4], uuid[5], uuid[6], uuid[7],
										uuid[8], uuid[9], uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15] );
	if ( uuidRef != NULL )
	{
		uuidString = CFUUIDCreateString( kCFAllocatorDefault, uuidRef );
		CFRelease( uuidRef );
	}
	
	return uuidString;
}


//----------------------------------------------------------------------------------------------------
//	pwsf_UUIDStrToUUID
//----------------------------------------------------------------------------------------------------

bool pwsf_UUIDStrToUUID( const char *inUUIDStr, uuid_t *outUUID )
{
	CFStringRef uuidString = NULL;
	CFUUIDRef uuidRef = NULL;
	CFUUIDBytes uuidBytes;
	bool result = false;
	
	if ( inUUIDStr == NULL || outUUID == NULL )
		return false;
	
	uuidString = CFStringCreateWithCString( kCFAllocatorDefault, inUUIDStr, kCFStringEncodingUTF8 );
	if ( uuidString != NULL )
	{
		uuidRef = CFUUIDCreateFromString( kCFAllocatorDefault, uuidString );
		if ( uuidRef != NULL )
		{
			uuidBytes = CFUUIDGetUUIDBytes( uuidRef );
			memcpy( outUUID, &uuidBytes, sizeof(uuid_t) );
			CFRelease( uuidRef );
			result = true;
		}
		
		CFRelease( uuidString );
	}
	
	return result;
}


//----------------------------------------------------------------------------------------------------
//	pwsf_GetGroupList
//
//  Returns: The number of groups in the scope-of-authority list, or -1 for invalid.
//
//	<outGroupList> is a NULL-terminated array. The count is returned as a convenience.
//----------------------------------------------------------------------------------------------------

int pwsf_GetGroupList( PWFileEntry *adminRec, uuid_t **outGroupList )
{
	return pwsf_GetGroupListWithPath( kPWAuxDirPath, adminRec, outGroupList );
}


//----------------------------------------------------------------------------------------------------
//	pwsf_GetGroupListWithPath
//
//  Returns: The number of groups in the scope-of-authority list, or -1 for invalid.
//
//	<outGroupList> is a NULL-terminated array. The count is returned as a convenience.
//----------------------------------------------------------------------------------------------------

int pwsf_GetGroupListWithPath( const char *basePath, PWFileEntry *adminRec, uuid_t **outGroupList )
{
	CFMutableDictionaryRef adminInfoDict = NULL;
	CFArrayRef groupArray = NULL;
	CFStringRef uuidString = NULL;
	CFUUIDRef uuidRef = NULL;
	CFUUIDBytes uuidBytes;
	CFIndex groupCount = -1;
	CFIndex idx = 0;
	char slotStr[35];
	char pathStr[PATH_MAX];
	
	if ( adminRec == NULL || outGroupList == NULL )
		return (int)groupCount;
		
	*outGroupList = NULL;
			
	switch( adminRec->admingroup.list_type )
	{
		case kPWGroupNotSet:
			groupCount = 0;
			break;
		
		case kPWGroupInSlot:
			*outGroupList = (uuid_t *) calloc( 2, sizeof(uuid_t) );
			if ( *outGroupList != NULL )
			{
				groupCount = 1;
				memcpy( (*outGroupList), adminRec->admingroup.group_uuid, sizeof(uuid_t) );
			}
			break;
			
		case kPWGroupInFile:
			pwsf_passwordRecRefToString( adminRec, slotStr );
			snprintf( pathStr, sizeof(pathStr), "%s/%s.plist", basePath, slotStr );
			if ( pwsf_loadxml(pathStr, &adminInfoDict) == 0 )
			{
				groupArray = (CFArrayRef) CFDictionaryGetValue( adminInfoDict, CFSTR(kPWKey_ScopeOfAuthority) );
				if ( groupArray != NULL )
				{
					groupCount = CFArrayGetCount( groupArray );
					*outGroupList = (uuid_t *) calloc( groupCount + 1, sizeof(uuid_t) );
					if ( *outGroupList != NULL )
					{
						for ( idx = 0; idx < groupCount; idx++ )
						{
							uuidString = (CFStringRef) CFArrayGetValueAtIndex( groupArray, idx );
							if ( uuidString == NULL )
								break;
							
							// convert string to uuid
							uuidRef = CFUUIDCreateFromString( kCFAllocatorDefault, uuidString );
							if ( uuidRef == NULL )
								break;
							
							uuidBytes = CFUUIDGetUUIDBytes( uuidRef );
							memcpy( &((*outGroupList)[idx]), &uuidBytes, sizeof(uuid_t) );
							CFRelease( uuidRef );
						}
					}
				}
			}
			break;
		
		default:
			break;
	}
	
	return (int)groupCount;
}


//----------------------------------------------------------------------------------------------------
//	pwsf_loadxml
//
//	Returns: 0 = success, -1 = failure.
//----------------------------------------------------------------------------------------------------

int pwsf_loadxml( const char *inFilePath, CFMutableDictionaryRef *outPList )
{
	CFStringRef myDataFilePathRef;
	CFURLRef myDataFileRef;
	CFReadStreamRef myReadStreamRef;
	CFPropertyListRef myPropertyListRef;
	CFStringRef errorString;
	CFPropertyListFormat myPLFormat;
	int result = -1;
	
	if ( inFilePath == NULL || outPList == NULL )
		return result;
	
	do
	{
		myDataFilePathRef = CFStringCreateWithCString( kCFAllocatorDefault, inFilePath, kCFStringEncodingUTF8 );
		if ( myDataFilePathRef == NULL )
			break;
		
		myDataFileRef = CFURLCreateWithFileSystemPath( kCFAllocatorDefault, myDataFilePathRef, kCFURLPOSIXPathStyle, false );
		
		CFRelease( myDataFilePathRef );
		
		if ( myDataFileRef == NULL )
			break;
		
		myReadStreamRef = CFReadStreamCreateWithFile( kCFAllocatorDefault, myDataFileRef );
		
		CFRelease( myDataFileRef );
		
		if ( myReadStreamRef == NULL )
			break;
		
		if ( ! CFReadStreamOpen( myReadStreamRef ) ) {
			CFRelease( myReadStreamRef );
			break;
		}
		
		errorString = NULL;
		myPLFormat = kCFPropertyListXMLFormat_v1_0;
		myPropertyListRef = CFPropertyListCreateFromStream( kCFAllocatorDefault, myReadStreamRef, 0,
															kCFPropertyListMutableContainersAndLeaves, &myPLFormat,
															&errorString );
		CFReadStreamClose( myReadStreamRef );
		CFRelease( myReadStreamRef );
		
		if ( errorString != NULL )
			CFRelease( errorString );
		
		if ( myPropertyListRef == NULL )
			break;
		
		if ( CFGetTypeID(myPropertyListRef) != CFDictionaryGetTypeID() ) {
			CFRelease( myPropertyListRef );
			break;
		}
		
		*outPList = (CFMutableDictionaryRef) myPropertyListRef;
		result = 0;
	}
	while (0);
	
	return result;
}


//----------------------------------------------------------------------------------------------------
//	pwsf_savexml
//
//	Returns: 0 = success, -1 = failure.
//----------------------------------------------------------------------------------------------------

int pwsf_savexml(const char *inSaveFile, CFDictionaryRef inPList )
{
	CFStringRef myDataFilePathRef;
	CFURLRef myDataFileRef;
	CFWriteStreamRef myWriteStreamRef = NULL;
	CFStringRef errorString;
	int returnValue = -1;
	struct stat sb;
	int err;
	char *saveDir;
	char *slash;
	mode_t saved_umask;
	
	// ensure the directory exists
	saveDir = strdup( inSaveFile );
	if ( saveDir != NULL ) {
		slash = rindex( saveDir, '/' );
		if ( slash != NULL ) {
			*slash = '\0';
			if ( lstat(saveDir, &sb) != 0 ) {
				err = pwsf_mkdir_p( saveDir, 0700 );
				if ( err != 0 ) {
					free( saveDir );
					return err;
				}
			}
		}
		free( saveDir );
	}
	
	saved_umask = umask((S_IRWXG | S_IRWXO));
	
	do
	{
		myDataFilePathRef = CFStringCreateWithCString( kCFAllocatorDefault, inSaveFile, kCFStringEncodingUTF8 );
		if ( myDataFilePathRef == NULL )
			break;
		
		myDataFileRef = CFURLCreateWithFileSystemPath( kCFAllocatorDefault, myDataFilePathRef, kCFURLPOSIXPathStyle, false );
		
		CFRelease( myDataFilePathRef );
		
		if ( myDataFileRef == NULL )
			break;
		
		myWriteStreamRef = CFWriteStreamCreateWithFile( kCFAllocatorDefault, myDataFileRef );
		
		CFRelease( myDataFileRef );
		
		if ( myWriteStreamRef == NULL )
			break;
		
		if ( CFWriteStreamOpen( myWriteStreamRef ) )
		{		
			errorString = NULL;
			CFPropertyListWriteToStream( (CFPropertyListRef)inPList, myWriteStreamRef, kCFPropertyListXMLFormat_v1_0, &errorString );
			CFWriteStreamClose( myWriteStreamRef );
			
			if ( errorString != NULL ) {
				CFRelease( errorString );
				break;
			}
			
			returnValue = 0;
		}
	}
	while (0);
	
	if ( myWriteStreamRef != NULL )
		CFRelease( myWriteStreamRef );
	
	umask( saved_umask );
	
	return returnValue;
}



