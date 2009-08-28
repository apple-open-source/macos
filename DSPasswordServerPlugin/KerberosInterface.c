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

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <syslog.h>
#include <CommonCrypto/CommonDigest.h>

#include "KerberosInterface.h"
#include "PSUtilitiesDefs.h"

//--------------------------------------------------------------------------------------------------
//	pwsf_AllKerberosToolsInstalled
//
//	Returns: TRUE if all the required kerberos tools are installed.
//--------------------------------------------------------------------------------------------------

bool pwsf_AllKerberosToolsInstalled( void )
{
	struct stat sb;
	bool kerberosToolsInstalled;
	
	kerberosToolsInstalled = (
			(lstat( kKAdminLocalFilePath, &sb ) == 0) &&
			(lstat( kKDBUtilLocalFilePath, &sb ) == 0) &&
			(lstat( kKAdminUtilFilePath, &sb ) == 0) );
	
	return kerberosToolsInstalled;
}


//--------------------------------------------------------------------------------------------------
//	AddPrincipal
//	AddPrincipalWithBuffer
//
//	Returns: -1 if call to pwsf_LaunchTaskWithIO() fails.
//--------------------------------------------------------------------------------------------------

int pwsf_AddPrincipal(const char* userName, const char* password, char* outRealmName, size_t maxRealmName)
{
	char *buffer = NULL;
	size_t bufferSize = 0;
	int result;
	
	result = pwsf_AddPrincipalWithBuffer(userName, password, outRealmName, maxRealmName, &buffer, &bufferSize);
	if ( buffer != NULL )
		free( buffer );
	
	return result;
}

int pwsf_AddPrincipalWithBuffer(const char* userName, const char* password, char* outRealmName, size_t maxRealmName, char **inOutBuff, size_t *inOutBuffLen)
{
	char* commandBuf = NULL;
	size_t commandBufSize = 0;
	size_t commandBufSizeNeeded = 0;
	char* argv[4];
	char* passwordBuf = NULL;
	size_t passLen = 0;
	char outputBuf[512];
	int result = 0;
	
	if ( password == NULL || inOutBuff == NULL || inOutBuffLen == NULL )
		return -1;
		
	passLen = strlen( password );
	
	// kerberos doesn't allow blank passwords.
	if ( passLen <= 0 )
		return -1;
	
	commandBuf = *inOutBuff;
	commandBufSize = *inOutBuffLen;
	
	commandBufSizeNeeded = strlen(userName) + kKAdminArgsSlushFactor;
	if ( commandBuf == NULL || commandBufSizeNeeded > commandBufSize )
	{
		if ( commandBuf != NULL )
		{
			free( commandBuf );
			*inOutBuff = commandBuf = NULL;
			*inOutBuffLen = commandBufSize = 0;
		}
		
		// add some extra to minimize reallocations
		*inOutBuffLen = commandBufSize = commandBufSizeNeeded + 32;
		*inOutBuff = commandBuf = (char *) malloc( commandBufSize );
	}
	
	if ( commandBuf == NULL )
		return -1;
	
	strcpy( commandBuf, "add_principal +requires_preauth " );
	strcat( commandBuf, userName );
	
	argv[0] = "kadmin.local";
	argv[1] = "-q";
	argv[2] = commandBuf;
	argv[3] = NULL;
	
	passwordBuf = (char *) malloc(passLen * 2 + 3);
	if ( passwordBuf == NULL )
		return -1;
	
	memcpy(passwordBuf, password, passLen);
	memcpy(passwordBuf+passLen+1, password, passLen);
	passwordBuf[passLen] = passwordBuf[passLen*2 + 1] = '\n';
	passwordBuf[passLen*2 + 2] = 0;
	
	result = pwsf_LaunchTaskWithIO(kKAdminLocalFilePath, argv, passwordBuf, outputBuf, sizeof(outputBuf), NULL);
	if ( result == 0 )
	{
		// get_principal is costing 8ms per principal so try to get the realm from
		// the output of the add_principal command.
		if ( ! pwsf_ScanForRealm( outputBuf, outRealmName, maxRealmName - 1 ) )
		{
			// now retrieve the principal name
			strcpy( commandBuf, "get_principal -terse " );
			strcat( commandBuf, userName );
			result = pwsf_LaunchTaskWithIO(kKAdminLocalFilePath, argv, passwordBuf, outputBuf, sizeof(outputBuf), NULL);
			if ( result == 0 )
			{
				pwsf_ScanForRealm( outputBuf, outRealmName, maxRealmName - 1 );
			}
			
			//	syslog(LOG_INFO, "Output = <%s> pn = <%s@%s>\n", outputBuf, userName, outRealmName);
		}
	}
	
	if (passwordBuf != NULL) {
		bzero(passwordBuf, passLen);
		free(passwordBuf);
	}
	
	return result;
}


int pwsf_AddPrincipalToLocalRealm(const char* userName, const char* password, const char* inRealmName)
{
	int				result					= 0;
	char			*commandBuf				= NULL;
	size_t			commandBufSize			= 0;
	char			*passwordBuf			= NULL;
	size_t			passwordBufLen			= 0;
	size_t			passLen					= 0;
	register int	ai						= 0;
	char			outputBuf[512];
	char			errorBuf[512];
	const char		*argv[6];
	
	if ( password == NULL || inRealmName == NULL )
		return -1;
	
	passLen = strlen( password );
	
	// kerberos doesn't allow blank passwords.
	if ( passLen <= 0 )
		return -1;
	
	commandBufSize = strlen(userName) + kKAdminArgsSlushFactor;
	commandBuf = (char *) malloc( commandBufSize );	
	if ( commandBuf == NULL )
		return -1;
	
	strlcpy( commandBuf, "add_principal +requires_preauth -allow_svr ", commandBufSize );
	strlcat( commandBuf, userName, commandBufSize );
	
	argv[ai++] = "kadmin.local";
	argv[ai++] = "-r";
	argv[ai++] = inRealmName;
	argv[ai++] = "-q";
	argv[ai++] = commandBuf;
	argv[ai] = NULL;
	
	passwordBufLen = passLen * 2 + 3;
	passwordBuf = (char *) malloc( passwordBufLen );
	if ( passwordBuf == NULL ) {
		free( commandBuf );
		return -1;
	}
	
	snprintf( passwordBuf, passwordBufLen, "%s\n%s\n", password, password );	
	result = pwsf_LaunchTaskWithIO2(
					kKAdminLocalFilePath, (char * const *)argv, passwordBuf,
					outputBuf, sizeof(outputBuf),
					errorBuf, sizeof(errorBuf) );
	
	bzero( passwordBuf, passLen );
	free( passwordBuf );
	free( commandBuf );
	
	return result;
}


void pwsf_DeletePrincipal(const char* principalName)
{
	char* argv[4];
	char* principalBuf;
	char outBuff[512];
	
	if ( principalName == NULL )
		return;
	
	principalBuf = (char *) malloc( strlen(principalName) + kKAdminArgsSlushFactor );
	if ( principalBuf == NULL )
		return;
	strcpy( principalBuf, "delete_principal -force " );
	strcat( principalBuf, principalName );
	
	argv[0] = "kadmin.local";
	argv[1] = "-q";
	argv[2] = principalBuf;
	argv[3] = NULL;
	
	pwsf_LaunchTaskWithIO( kKAdminLocalFilePath, argv, NULL, outBuff, sizeof(outBuff), NULL );
	free( principalBuf );
}


void pwsf_DeletePrincipalInLocalRealm(const char* principalName, const char *realmName)
{
	char*	principalBuf		= NULL;
	size_t	principalBufLen		= 0;
	int		ai					= 0;
	const char* argv[6];
	char outBuff[512];
	char errBuff[512];
	
	if ( principalName == NULL || realmName == NULL )
		return;
	
	principalBufLen = strlen( principalName ) + kKAdminArgsSlushFactor;
	principalBuf = (char *) malloc( principalBufLen );
	if ( principalBuf == NULL )
		return;
	
	strlcpy( principalBuf, "delete_principal -force ", principalBufLen );
	strlcat( principalBuf, principalName, principalBufLen );
	
	argv[ai++] = "kadmin.local";
	argv[ai++] = "-r";
	argv[ai++] = realmName;
	argv[ai++] = "-q";
	argv[ai++] = principalBuf;
	argv[ai] = NULL;
	
	pwsf_LaunchTaskWithIO2( kKAdminLocalFilePath, (char * const *)argv, NULL, outBuff, sizeof(outBuff), errBuff, sizeof(errBuff) );
	free( principalBuf );
}


void pwsf_SetCertHash( const char *certHash, const char *principalName )
{
	char* commandBuf = NULL;
	size_t commandBufSize = 0;
	char* argv[4];
	char outBuff[512];

	if ( certHash == NULL || principalName == NULL )
		return;
	
	commandBufSize = strlen(certHash) + strlen(principalName) + kKAdminArgsSlushFactor;
	commandBuf = (char *) malloc( commandBufSize );
	if ( commandBuf == NULL )
		return;
	
	argv[0] = "kadmin.local";
	argv[1] = "-q";
	argv[2] = commandBuf;
	argv[3] = NULL;
	
	snprintf( commandBuf, commandBufSize, "modprinc +requires_preauth -certhash %s %s", certHash, principalName );
	pwsf_LaunchTaskWithIO( kKAdminLocalFilePath, argv, NULL, outBuff, sizeof(outBuff), NULL );
	free( commandBuf );
}


void pwsf_SetCertHashInLocalRealm( const char *certHash, const char *principalName, const char *realmName )
{
	char* commandBuf = NULL;
	size_t commandBufSize = 0;
	register int ai = 0;
	const char *argv[6];
	char outBuff[512];
	
	if ( certHash == NULL || principalName == NULL )
		return;
	
	commandBufSize = strlen(certHash) + strlen(principalName) + kKAdminArgsSlushFactor;
	commandBuf = (char *) malloc( commandBufSize );
	if ( commandBuf == NULL )
		return;
	
	argv[ai++] = "kadmin.local";
	argv[ai++] = "-r";
	argv[ai++] = realmName;
	argv[ai++] = "-q";
	argv[ai++] = commandBuf;
	argv[ai] = NULL;
	
	snprintf( commandBuf, commandBufSize, "modprinc +requires_preauth -certhash %s %s", certHash, principalName );
	pwsf_LaunchTaskWithIO( kKAdminLocalFilePath, (char * const *)argv, NULL, outBuff, sizeof(outBuff), NULL );
	free( commandBuf );
}


//--------------------------------------------------------------------------------------------------
//	pwsf_ModifyPrincipalWithBuffer
//--------------------------------------------------------------------------------------------------

static char *pwsf_ModifyPrincipalSetupBuffer(const char* principalName, char **inOutBuff, size_t *inOutBuffLen)
{
	char *commandBuf = NULL;
	size_t commandBufSize = 0;
	size_t commandBufSizeNeeded = 0;
	
	if ( inOutBuff == NULL || inOutBuffLen == NULL )
		return NULL;
	
	commandBuf = *inOutBuff;
	commandBufSize = *inOutBuffLen;
	
	commandBufSizeNeeded = strlen(principalName) + kKAdminArgsSlushFactor;
	if ( commandBuf == NULL || commandBufSizeNeeded > commandBufSize )
	{
		if ( commandBuf != NULL )
		{
			free( commandBuf );
			*inOutBuff = commandBuf = NULL;
			*inOutBuffLen = commandBufSize = 0;
		}
		
		// add some extra to minimize reallocations
		*inOutBuffLen = commandBufSize = commandBufSizeNeeded + 32;
		*inOutBuff = commandBuf = (char *) malloc( commandBufSize );
	}
	
	return commandBuf;
}


void pwsf_ModifyPrincipalWithBuffer(char* principalName, PWAccessFeatures* access, UInt32 oldDuration,
	char **inOutBuff, size_t *inOutBuffLen)
{
	char* commandBuf = NULL;
	size_t commandBufSize = 0;
	char* argv[4];
	char expireTime[100];
	char pwExpireTime[100];
	char needsPWChange;
	char policyStr[30];
	char canLogin;
	UInt32 newDuration = access->maxMinutesUntilChangePassword;
	char stdoutBuff[512];
	
	if ( principalName == NULL )
		return;
	
	if ( (commandBuf = pwsf_ModifyPrincipalSetupBuffer(principalName, inOutBuff, inOutBuffLen)) == NULL )
		return;
	commandBufSize = *inOutBuffLen;
	
	argv[0] = "kadmin.local";
	argv[1] = "-q";
	argv[2] = commandBuf;
	argv[3] = NULL;
	
	if ((newDuration > 0) && (newDuration != oldDuration))
	{
		snprintf(commandBuf, commandBufSize,
#if __LP64__
			"add_policy -maxlife \"%d minutes\" policy%dmin",
#else
			"add_policy -maxlife \"%ld minutes\" policy%ldmin",
#endif
			newDuration, newDuration);

		pwsf_LaunchTaskWithIO(kKAdminLocalFilePath, argv, NULL, stdoutBuff, sizeof(stdoutBuff), NULL);
		sprintf(policyStr,
#if __LP64__
			"-policy policy%dmin",
#else
			"-policy policy%ldmin",
#endif
			 newDuration);
	}
	else
		strcpy(policyStr, "-clearpolicy");
		
	if (access->usingExpirationDate)
		BSDTimeStructCopy_strftime(pwExpireTime, sizeof(pwExpireTime), "\"%m/%d/%Y %H:%M:%S GMT\"", &access->expirationDateGMT);
	else
		strcpy(pwExpireTime, "never");
	
	if (access->usingHardExpirationDate)
		BSDTimeStructCopy_strftime(expireTime, sizeof(expireTime), "\"%m/%d/%Y %H:%M:%S GMT\"", &access->hardExpireDateGMT);
	else
		strcpy(expireTime, "never");
	
	needsPWChange = access->newPasswordRequired ? '+':'-';
	
	canLogin = access->isDisabled ? '-':'+';
	
	snprintf(commandBuf, commandBufSize, "modify_principal +requires_preauth -expire %s -pwexpire %s %cneedchange %callow_tix %s %s", 
		expireTime, pwExpireTime, needsPWChange, canLogin, policyStr, principalName);
	
	pwsf_LaunchTaskWithIO(kKAdminLocalFilePath, argv, NULL, stdoutBuff, sizeof(stdoutBuff), NULL);

	if ((oldDuration > 0) && (newDuration != oldDuration))
	{
		snprintf(commandBuf, commandBufSize,
#if __LP64__
		"delete_policy policy%dmin",
#else
		"delete_policy policy%ldmin",
#endif
		oldDuration);
		
		pwsf_LaunchTaskWithIO(kKAdminLocalFilePath, argv, NULL, stdoutBuff, sizeof(stdoutBuff), NULL);
	}
}


void pwsf_ModifyPrincipalInLocalRealm(char* principalName, const char *realmName, PWAccessFeatures* access, UInt32 oldDuration,
	char **inOutBuff, size_t *inOutBuffLen)
{
	char* commandBuf = NULL;
	size_t commandBufSize = 0;
	const char* argv[6];
	char expireTime[100];
	char pwExpireTime[100];
	char needsPWChange;
	char policyStr[30];
	char canLogin;
	UInt32 newDuration = access->maxMinutesUntilChangePassword;
	register int ai = 0;
	char stdoutBuff[512];
	char stderrBuff[512];
	
	if ( principalName == NULL || realmName == NULL )
		return;
	
	if ( (commandBuf = pwsf_ModifyPrincipalSetupBuffer(principalName, inOutBuff, inOutBuffLen)) == NULL )
		return;
	commandBufSize = *inOutBuffLen;
	
	argv[ai++] = "kadmin.local";
	argv[ai++] = "-r";
	argv[ai++] = realmName;
	argv[ai++] = "-q";
	argv[ai++] = commandBuf;
	argv[ai] = NULL;
	
	if ((newDuration > 0) && (newDuration != oldDuration))
	{
		snprintf(commandBuf, commandBufSize,
#if __LP64__
			"add_policy -maxlife \"%d minutes\" policy%dmin",
#else
			"add_policy -maxlife \"%ld minutes\" policy%ldmin",
#endif
			newDuration, newDuration);

		pwsf_LaunchTaskWithIO(kKAdminLocalFilePath, (char * const *)argv, NULL, stdoutBuff, sizeof(stdoutBuff), NULL);
		sprintf(policyStr,
#if __LP64__
			"-policy policy%dmin",
#else
			"-policy policy%ldmin",
#endif
			 newDuration);
	}
	else
		strcpy(policyStr, "-clearpolicy");
		
	if (access->usingExpirationDate)
		BSDTimeStructCopy_strftime(pwExpireTime, sizeof(pwExpireTime), "\"%m/%d/%Y %H:%M:%S GMT\"", &access->expirationDateGMT);
	else
		strcpy(pwExpireTime, "never");
	
	if (access->usingHardExpirationDate)
		BSDTimeStructCopy_strftime(expireTime, sizeof(expireTime), "\"%m/%d/%Y %H:%M:%S GMT\"", &access->hardExpireDateGMT);
	else
		strcpy(expireTime, "never");
	
	needsPWChange = access->newPasswordRequired ? '+':'-';
	
	canLogin = access->isDisabled ? '-':'+';
	
	snprintf(commandBuf, commandBufSize, "modify_principal +requires_preauth -expire %s -pwexpire %s %cneedchange %callow_tix %s %s", 
		expireTime, pwExpireTime, needsPWChange, canLogin, policyStr, principalName);
	
	pwsf_LaunchTaskWithIO2(
			kKAdminLocalFilePath, (char * const *)argv, NULL,
			stdoutBuff, sizeof(stdoutBuff),
			stderrBuff, sizeof(stderrBuff) );
	
	if ((oldDuration > 0) && (newDuration != oldDuration))
	{
		snprintf(commandBuf, commandBufSize,
#if __LP64__
		"delete_policy policy%dmin",
#else
		"delete_policy policy%ldmin",
#endif
		oldDuration);
		
		pwsf_LaunchTaskWithIO(kKAdminLocalFilePath, (char * const *)argv, NULL, stdoutBuff, sizeof(stdoutBuff), NULL);
	}
}


//--------------------------------------------------------------------------------------------------
//	pwsf_ChangePassword
//--------------------------------------------------------------------------------------------------

static void pwsf_ChangePasswordV(const char *argv[], const char* password)
{
	char* passwordBuf;
	size_t passLen = strlen(password);
	char outBuff[512];
			
	passwordBuf = (char *) malloc(passLen * 2 + 3);
	memcpy(passwordBuf, password, passLen);
	memcpy(passwordBuf+passLen+1, password, passLen);
	passwordBuf[passLen] = passwordBuf[passLen*2 + 1] = '\n';
	passwordBuf[passLen*2 + 2] = 0;
	
	pwsf_LaunchTaskWithIO(kKAdminLocalFilePath, (char * const *)argv, passwordBuf, outBuff, sizeof(outBuff), NULL);
	
	free(passwordBuf);
}


void pwsf_ChangePassword(const char* principalName, const char* password)
{
	const char *argv[4];
	char* principalBuf;
	const char* changeCommand = "change_password ";
	size_t changeCommandLen = strlen(changeCommand);
	
	principalBuf = (char *) malloc(strlen(principalName) + changeCommandLen + 1);
	if (principalBuf != NULL)
	{
		memcpy(principalBuf, changeCommand, changeCommandLen);
		memcpy(principalBuf + changeCommandLen, principalName, strlen(principalName) + 1);
			
		argv[0] = "kadmin.local";
		argv[1] = "-q";
		argv[2] = principalBuf;
		argv[3] = NULL;
		
		pwsf_ChangePasswordV(argv, password);
		free(principalBuf);
	}
}


void pwsf_ChangePasswordInLocalRealm(const char* principalName, const char *realmName, const char* password)
{
	const char *argv[6];
	char* principalBuf;
	const char* changeCommand = "change_password ";
	size_t changeCommandLen = strlen(changeCommand);
	register int ai = 0;
	
	if (realmName != NULL)
	{
		principalBuf = (char *) malloc(strlen(principalName) + changeCommandLen + 1);
		if (principalBuf != NULL)
		{
			memcpy(principalBuf, changeCommand, changeCommandLen);
			memcpy(principalBuf + changeCommandLen, principalName, strlen(principalName) + 1);
			
			argv[ai++] = "kadmin.local";
			argv[ai++] = "-r";
			argv[ai++] = realmName;
			argv[ai++] = "-q";
			argv[ai++] = principalBuf;
			argv[ai] = NULL;
			
			pwsf_ChangePasswordV(argv, password);
			free(principalBuf);
		}
	}
}


//--------------------------------------------------------------------------------------------------
//	pwsf_ScanForRealm
//
//	Returns: TRUE if the realm is found.
//--------------------------------------------------------------------------------------------------

bool pwsf_ScanForRealm( const char *inKAdminText, char *outRealm, size_t inRealmMaxSize )
{
	const char *start;
	const char *end = NULL;
	ptrdiff_t len;
	bool result = false;
	
	if ( inKAdminText == NULL || outRealm == NULL )
		return false;

	*outRealm = '\0';
	
	start = strstr(inKAdminText, "\"");
	if (start != NULL)
		start = strstr(start+1, "@");
	if (start != NULL)
		end = strstr(start+1, "\"");
	if ((start != NULL) && (end != NULL))
	{
		len = (end - (start+1));
		if ( len > 0 && len < inRealmMaxSize )
		{
			strlcpy(outRealm, start + 1, len + 1 );
			result = true;
		}
	}
	
	return result;
}


//--------------------------------------------------------------------------------------------------
//	SetPrincipalAdministratorState
//
//	Returns: 0 = success, -n = failure.
//--------------------------------------------------------------------------------------------------

int pwsf_SetPrincipalAdministratorState( const char *inPrincipal, bool inAdmin, bool inSignalHUP )
{
	char* argv[6] = {NULL};
	char outBuff[512];
	
	if ( inPrincipal == NULL )
		return -1;
	
	argv[0] = kKAdminUtilFilePath;
	argv[1] = (char *)(inAdmin ? "-a" : "-d");
	argv[2] = (char *)inPrincipal;
	argv[3] = "-p";
	if ( inSignalHUP )
		argv[4] = "-h";
	
	return pwsf_LaunchTaskWithIO( kKAdminUtilFilePath, argv, NULL, outBuff, sizeof(outBuff), NULL );
}


//--------------------------------------------------------------------------------------------------
//	pwsf_GeneratePasswordForPrincipal
//
//	The <outPassword> parameter must hold a string with 20 characters and a null-terminator.
//--------------------------------------------------------------------------------------------------

void pwsf_GeneratePasswordForPrincipal( const char *inPassword, const char *inPrincipal, char *outPassword )
{
	int idx = 0;
	CC_SHA1_CTX ctx;
	unsigned char digest[CC_SHA1_DIGEST_LENGTH];
	
	CC_SHA1_Init( &ctx );
	CC_SHA1_Update( &ctx, (unsigned char *)inPrincipal, (CC_LONG)strlen(inPrincipal) );
	CC_SHA1_Update( &ctx, (unsigned char *)inPassword, (CC_LONG)strlen(inPassword) );
	CC_SHA1_Final( digest, &ctx );
	
	// remove illegal characters
	for ( idx = 0; idx < CC_SHA1_DIGEST_LENGTH; idx++ )
		if ( digest[idx] <= 13 )
			digest[idx] += ' ';
	
	strlcpy( outPassword, (char *)digest, CC_SHA1_DIGEST_LENGTH + 1 );
}

