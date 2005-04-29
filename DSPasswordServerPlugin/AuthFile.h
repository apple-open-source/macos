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


#ifndef __AUTHFILE_H__
#define __AUTHFILE_H__

#include <time.h>
#include <CoreFoundation/CoreFoundation.h>

#ifdef __cplusplus
	extern "C" {
#endif

#include "sasl.h"

#define kPWFileSignature				'pwfi'
#define kPWFileVersion					1
#define kPWFileInitialSlots				512
#define kPWFileMaxWeakMethods			25
#define kPWFileMaxDigests				10
#define kPWFileMaxPublicKeyBytes		1024
#define kPWFileMaxPrivateKeyBytes		2048
#define kPWFileMaxHistoryCount			15
#define kPWFileMaxReplicaName			32
#define kSMBNTStorageTag				"*cmusaslsecretSMBNT"

// password server error strings
#define kPasswordServerErrPrefixStr			"-ERR "
#define kPasswordServerAuthErrPrefixStr		"-AUTHERR "
#define kPasswordServerSASLErrPrefixStr		"SASL "

// file names and folder paths
#define kPWDirPath							"/var/db/authserver"
#define kPWFilePath							"/var/db/authserver/authservermain"
#define kFreeListFilePath					"/var/db/authserver/authserverfree"
#define kPWHistoryFileName					"histories"
#define kPWWeakFileStart					"/weakpasswords."
#define kTempKeyFile						"/tmp/passwordserverKey"
#define kPWReplicaLocalFile					"/var/db/authserver/authserverreplicas.local"
#define kPWReplicaPreConfiguredFile			"/var/db/authserver/authserverreplicas.manual"

// ascii identifiers for password policies
#define kPWPolicyStr_isDisabled						"isDisabled"
#define kPWPolicyStr_isAdminUser					"isAdminUser"
#define kPWPolicyStr_newPasswordRequired			"newPasswordRequired"
#define kPWPolicyStr_usingHistory					"usingHistory"
#define kPWPolicyStr_canModifyPasswordforSelf		"canModifyPasswordforSelf"
#define kPWPolicyStr_usingExpirationDate			"usingExpirationDate"
#define kPWPolicyStr_usingHardExpirationDate		"usingHardExpirationDate"
#define kPWPolicyStr_requiresAlpha					"requiresAlpha"
#define kPWPolicyStr_requiresNumeric				"requiresNumeric"
#define kPWPolicyStr_expirationDateGMT				"expirationDateGMT"
#define kPWPolicyStr_hardExpireDateGMT				"hardExpireDateGMT"
#define kPWPolicyStr_maxMinutesUntilChangePW		"maxMinutesUntilChangePassword"
#define kPWPolicyStr_maxMinutesUntilDisabled		"maxMinutesUntilDisabled"
#define kPWPolicyStr_maxMinutesOfNonUse				"maxMinutesOfNonUse"
#define kPWPolicyStr_maxFailedLoginAttempts			"maxFailedLoginAttempts"    
#define kPWPolicyStr_minChars						"minChars"
#define kPWPolicyStr_maxChars						"maxChars"
#define kPWPolicyStr_passwordCannotBeName			"passwordCannotBeName"
#define kPWPolicyStr_isSessionKeyAgent				"isSessionKeyAgent"
#define kPWPolicyStr_requiresMixedCase				"requiresMixedCase"
#define kPWPolicyStr_notGuessablePattern			"notGuessablePattern"
#define kPWPolicyStr_warnOfExpirationMinutes		"warnOfExpirationMinutes"
#define kPWPolicyStr_warnOfDisableMinutes			"warnOfDisableMinutes"

// meta policies
#define kPWPolicyStr_resetToGlobalDefaults			"resetToGlobalDefaults"
#define kPWPolicyStr_logOffTime						"logOffTime"
#define kPWPolicyStr_kickOffTime					"kickOffTime"
#define kPWPolicyStr_lastLoginTime					"lastLoginTime"
#define kPWPolicyStr_passwordLastSetTime			"passwordLastSetTime"
#define kPWPolicyStr_minutesUntilFailedLoginReset   "minutesUntilFailedLoginReset"
#define kPWPolicyStr_newPasswordRequiredForAll		"newPasswordRequiredForAll"
#define kPWPolicyStr_projectedPasswordExpireDate	"projectedPasswordExpireDate"
#define kPWPolicyStr_projectedAccountDisableDate	"projectedAccountDisableDate"

enum 
{
	kPWHashSlotSMB_NT					= 0,
	kPWHashSlotSMB_LAN_MANAGER			= 1,
	kPWHashSlotDIGEST_MD5				= 2,
	kPWHashSlotCRAM_MD5					= 3,
	kPWHashSlotKERBEROS					= 4,
	kPWHashSlotKERBEROS_NAME			= 5
};

typedef enum PWDisableReasonCode {
	kPWDisabledNotSet,
	kPWDisabledByAdmin,
	kPWDisabledExpired,
	kPWDisabledInactive,
	kPWDisabledTooManyFailedLogins
} PWDisableReasonCode;

// this is the version of the time struct that
// will be written to our db file.
// If the bsd header ever changes, the new version
// can be mapped to this one.
typedef struct BSDTimeStructCopy {
	int	tm_sec;		/* seconds after the minute [0-60] */
	int	tm_min;		/* minutes after the hour [0-59] */
	int	tm_hour;	/* hours since midnight [0-23] */
	int	tm_mday;	/* day of the month [1-31] */
	int	tm_mon;		/* months since January [0-11] */
	int	tm_year;	/* years since 1900 */
	int	tm_wday;	/* days since Sunday [0-6] */
	int	tm_yday;	/* days since January 1 [0-365] */
	int	tm_isdst;	/* Daylight Savings Time flag */
	long tm_gmtoff;	/* offset from CUT in seconds */
	char *tm_zone;	/* timezone abbreviation */
} BSDTimeStructCopy;

typedef struct AuthMethName {
    char method[SASL_MECHNAMEMAX + 1];
} AuthMethName;

typedef struct PasswordDigest {
    char method[SASL_MECHNAMEMAX + 1];
    char digest[256];
} PasswordDigest;

typedef struct PWGlobalAccessFeatures {
    unsigned int usingHistory:1;			// TRUE == users have password history files
    unsigned int usingExpirationDate:1;		// TRUE == look at expirationDateGMT
    unsigned int usingHardExpirationDate:1; // TRUE == look at hardExpirationDateGMT
    unsigned int requiresAlpha:1;			// TRUE == password must have one char in [A-Z], [a-z]
    unsigned int requiresNumeric:1;			// TRUE == password must have one char in [0-9]
	
	// db 1.1
    unsigned int passwordIsHash:1;
    unsigned int passwordCannotBeName:1;
	unsigned int historyCount:4;
	
	// db 1.2
	unsigned int requiresMixedCase:1;		// TRUE == password must have one char in [A-Z], and one char in [a-z]
	unsigned int newPasswordRequired:1;		// TRUE == new users must change their passwords on first login
	unsigned int noModifyPasswordforSelf:1;	// TRUE == users cannot change their own passwords.
	int unused:2;
    
    BSDTimeStructCopy expirationDateGMT;	// if exceeded, user is required to change the password at next login
	BSDTimeStructCopy hardExpireDateGMT;	// if exceeded, user is disabled
    
    UInt32 maxMinutesUntilChangePassword;	// if exceeded, users must change password. 0==not used
    UInt32 maxMinutesUntilDisabled;			// if exceeded, users are disabled. 0==not used
    UInt32 maxMinutesOfNonUse;				// if exceeded, users are disabled. 0==not used
    UInt16 maxFailedLoginAttempts;			// if exceeded, users are disabled. 0==not used
    
    UInt16 minChars;						// minimum characters in a valid password, 0==no limit
    UInt16 maxChars;						// maximum characters in a valid password, 0==limited to Server's limit (512 bytes)
    
} PWGlobalAccessFeatures;

typedef struct PWGlobalMoreAccessFeatures {
	UInt32 minutesUntilFailedLoginReset;		// 0=off, after this # of minutes, the failed login count is reset.
	UInt32 notGuessablePattern;					// 0=off, otherwise, # represents a password score for entropy.
} PWGlobalMoreAccessFeatures;

typedef struct PWAccessFeatures {
    int isDisabled:1;						// TRUE == cannot log in
    int isAdminUser:1;						// TRUE == can modify other slots in the db
    int newPasswordRequired:1;				// TRUE == user is required to change the password at next login
    int usingHistory:1;						// TRUE == user has a password history file
    int canModifyPasswordforSelf:1;			// TRUE == user can modify their own password
    int usingExpirationDate:1;				// TRUE == look at expirationDateGMT
    int usingHardExpirationDate:1;			// TRUE == look at hardExpirationDateGMT
    int requiresAlpha:1;					// TRUE == password must have one char in [A-Z], [a-z]
    int requiresNumeric:1;					// TRUE == password must have one char in [0-9]
    
	// db 1.1
	int passwordIsHash:1;
	int passwordCannotBeName:1;
	unsigned int historyCount:4;
	int isSessionKeyAgent:1;				// the user can retrieve (MPPE) session keys
    
    BSDTimeStructCopy expirationDateGMT;	// if exceeded, user is required to change the password at next login
	BSDTimeStructCopy hardExpireDateGMT;	// if exceeded, user is disabled
    
    UInt32 maxMinutesUntilChangePassword;	// if exceeded, users must change password. 0==not used
    UInt32 maxMinutesUntilDisabled;			// if exceeded, users are disabled. 0==not used
    UInt32 maxMinutesOfNonUse;				// if exceeded, user is disabled. 0==not used
    UInt16 maxFailedLoginAttempts;			// if exceeded, user is disabled. 0==not used
    
    UInt16 minChars;						// minimum characters in a valid password
    UInt16 maxChars;						// maximum characters in a valid password
    
} PWAccessFeatures;

typedef struct PWMoreAccessFeatures {
	UInt32 minutesUntilFailedLoginReset;		// 0=off, after this # of minutes, the failed login count is reset.
	UInt32 notGuessablePattern;					// 0=off, otherwise, # represents a password score for entropy.
	char userkey[64];							// random key for RC5
	UInt32 logOffTime;							// SMB data store
	UInt32 kickOffTime;							// SMB data store
	int recordIsDead:1;							// death certificate for deleted user
	int doNotReplicate:1;						// the password information does not get replicated
	int doNotMerge:1; 							// used for restoring from backup
	unsigned int requiresMixedCase:1;			// TRUE == password must have one char in [A-Z], and one char in [a-z]
	int unused511:12; 							// reserved
} PWMoreAccessFeatures;

typedef struct PWFileHeader {
    UInt32 signature;								// some value that means this is a real pw file
    UInt32 version;									// version for the file format
    UInt32 entrySize;								// sizeof(PWFileEntry)
    UInt32 sequenceNumber;							// incremented by one each time a pw is saved, never decremented.
    UInt32 numberOfSlotsCurrentlyInFile; 			// last slot for the current file size
    UInt32 deepestSlotUsed;							// if < numberOfSlotsCurrentlyInFile, issue to the end, then look at freelist

    PWGlobalAccessFeatures access;					// password policies that apply to all users unless overridden
    AuthMethName weakAuthMethods[kPWFileMaxWeakMethods];
                                                    // list of methods that are considered too weak for administration
    
    unsigned long publicKeyLen;
    unsigned char publicKey[kPWFileMaxPublicKeyBytes];
                                                    // 1024-bit RSA public key - expected size is 233
    unsigned long privateKeyLen;
    unsigned char privateKey[kPWFileMaxPrivateKeyBytes];
                                                    // 1024-bit RSA private key - expected size is 887 or so
													
	char replicationName[kPWFileMaxReplicaName];	// the name used by replication to identify this server
	UInt32 deepestSlotUsedByThisServer;				// deepest slot used in this replica's allocated range
	UInt32 accessModDate;							// time of last change to access field in seconds from 1970
	PWGlobalMoreAccessFeatures extraAccess;			// new policy data
	UInt32 fExtraData[244];							// not used yet, room for future growth - all zero's until used...
} PWFileHeader;

typedef struct PWFileEntry {
    UInt32 time;								// long #1 of the password's reference number
    UInt32 rnd;									// long #2 of the password's reference number
    UInt32 sequenceNumber;						// long #3 of the password's reference number
    UInt32 slot;								// long #4 of the password's reference number
                                                // (slot * sizeof(PWFileEntry)) + sizeof(PWFileHeader) == pos in file
                                                // we also require that candidatePasswordSlotID == filePasswordSlotID
    
    BSDTimeStructCopy creationDate;				// date/time this struct was originally added to the database
    BSDTimeStructCopy modificationDate;			// date/time this struct was last modified in the database
    BSDTimeStructCopy modDateOfPassword;		// date/time the password was last modified in the database
    BSDTimeStructCopy lastLogin;				// date/time of the user's last successful authentication.
    UInt16 failedLoginAttempts;					// counter of sequential failed logins
    
    PWAccessFeatures access;					// password policy data determined by marketing
    
    // WARNING: The following field is the key to the kindom. It should never, ever, ever,
    // ever, be sent across the network or dumped to any log file!!!!
    
    char passwordStr[512];						// the password
    
    // ****
    // The digests should be guarded also.
    
    PasswordDigest digest[kPWFileMaxDigests];	// password digests
    
    char usernameStr[256];						// the username
	char userdata[426];							// place for users to store their own information
	PWDisableReasonCode disableReason;			// indicates why an account was disabled
	PWMoreAccessFeatures extraAccess;			// password policy data determined by marketing
} PWFileEntry;

int TimeIsStale( BSDTimeStructCopy *inTime );
int LoginTimeIsStale( BSDTimeStructCopy *inLastLogin, unsigned long inMaxMinutesOfNonUse );
void PWGlobalAccessFeaturesToString( PWGlobalAccessFeatures *inAccessFeatures, char *outString );
void PWGlobalAccessFeaturesToStringExtra( PWGlobalAccessFeatures *inAccessFeatures, PWGlobalMoreAccessFeatures *inExtraFeatures, int inMaxLen, char *outString );
void PWAccessFeaturesToString( PWAccessFeatures *inAccessFeatures, char *outString );
void PWAccessFeaturesToStringExtra( PWAccessFeatures *inAccessFeatures, PWMoreAccessFeatures *inExtraFeatures, int inMaxLen, char *outString );
void PWActualAccessFeaturesToString( PWGlobalAccessFeatures *inGAccessFeatures, PWAccessFeatures *inAccessFeatures, char *outString );
void PWActualAccessFeaturesToStringExtra( PWGlobalAccessFeatures *inGAccessFeatures, PWAccessFeatures *inAccessFeatures, PWMoreAccessFeatures *inExtraFeatures, int inMaxLen, char *outString );
void PWAccessFeaturesToStringWithoutStateInfo( PWAccessFeatures *inAccessFeatures, char *outString );
void PWAccessFeaturesToStringWithoutStateInfoExtra( PWAccessFeatures *inAccessFeatures, PWMoreAccessFeatures *inExtraFeatures, int inMaxLen, char *outString );
Boolean StringToPWGlobalAccessFeatures( const char *inString, PWGlobalAccessFeatures *inOutAccessFeatures );
Boolean StringToPWGlobalAccessFeaturesExtra( const char *inString, PWGlobalAccessFeatures *inOutAccessFeatures, PWGlobalMoreAccessFeatures *inOutExtraFeatures );
Boolean StringToPWAccessFeatures( const char *inString, PWAccessFeatures *inOutAccessFeatures );
Boolean StringToPWAccessFeaturesExtra( const char *inString, PWAccessFeatures *inOutAccessFeatures, PWMoreAccessFeatures *inOutExtraFeatures );
Boolean StringToPWAccessFeatures_GetValue( const char *inString, unsigned long *outValue );
void CrashIfBuiltWrong(void);

int pwsf_GetPublicKey( char *outPublicKey );
int pwsf_GetPublicKeyFromFile( const char *inFile, char *outPublicKey );
void pwsf_CreateReplicaFile( const char *inIPStr, const char *inDNSStr, const char *inPublicKey );
void pwsf_ResetReplicaFile( const char *inPublicKey );
char* pwsf_GetPrincName( PWFileEntry *userRec );
int pwsf_ShadowHashDataToArray( const char *inAAData, CFMutableArrayRef *outHashTypeArray );
char * pwsf_ShadowHashArrayToData( CFArrayRef inHashTypeArray, long *outResultLen );
void pwsf_AppendUTF8StringToArray( const char *inUTF8Str, CFMutableArrayRef inArray );

// in CAuthFileBase.cpp
int pwsf_TestDisabledStatus( PWAccessFeatures *inAccess, PWGlobalAccessFeatures *inGAccess, struct tm *inCreationDate, struct tm *inLastLoginTime, UInt16 *inOutFailedLoginAttempts );
int pwsf_TestDisabledStatusWithReasonCode( PWAccessFeatures *inAccess, PWGlobalAccessFeatures *inGAccess, struct tm *inCreationDate, struct tm *inLastLoginTime, UInt16 *inOutFailedLoginAttempts, PWDisableReasonCode *outReasonCode );
int pwsf_ChangePasswordStatus( PWAccessFeatures *inAccess, PWGlobalAccessFeatures *inGAccess, struct tm *inModDateOfPassword );
int pwsf_RequiredCharacterStatus(PWAccessFeatures *access, PWGlobalAccessFeatures *inGAccess, const char *inUsername, const char *inPassword);
int pwsf_RequiredCharacterStatusExtra(PWAccessFeatures *access, PWGlobalAccessFeatures *inGAccess, const char *inUsername, const char *inPassword, PWMoreAccessFeatures *inExtraFeatures );
void pwsf_getHashCramMD5(const unsigned char *inPassword, long inPasswordLen, unsigned char *outHash, unsigned long *outHashLen );

// in CReplicaFile.cpp
CFDictionaryRef pwsf_GetStatusForReplicas( void );

#ifdef __cplusplus
	};
#endif

#endif

