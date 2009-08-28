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

#ifndef __KERBEROS_INTERFACE__
#define __KERBEROS_INTERFACE__

#include <PasswordServer/AuthFile.h>

#define kKAdminLocalFilePath		"/usr/sbin/kadmin.local"
#define kKDBUtilLocalFilePath		"/usr/sbin/kdb5_util"
#define kKAdminUtilFilePath			"/usr/sbin/kadmin_util"
#define kKAdminArgsSlushFactor		512

#ifdef __cplusplus
extern "C" {
#endif

bool pwsf_AllKerberosToolsInstalled( void );
int pwsf_AddPrincipal(const char* userName, const char* password, char* outRealmName, size_t maxRealmName);
int pwsf_AddPrincipalWithBuffer(const char* userName, const char* password, char* outRealmName, size_t maxRealmName, char **inOutBuff, size_t *inOutBuffLen);
int pwsf_AddPrincipalToLocalRealm(const char* userName, const char* password, const char* inRealmName);
void pwsf_ChangePassword(const char* principalName, const char* password);
void pwsf_ChangePasswordInLocalRealm(const char* principalName, const char *realmName, const char* password);
void pwsf_DeletePrincipal(const char* principalName);
void pwsf_DeletePrincipalInLocalRealm(const char* principalName, const char *realmName);
void pwsf_SetCertHash( const char *certHash, const char *principalName );
void pwsf_SetCertHashInLocalRealm( const char *certHash, const char *principalName, const char *realmName );
void pwsf_ModifyPrincipalWithBuffer(char* principalName, PWAccessFeatures* access, UInt32 oldDuration, char **inOutBuff, size_t *inOutBuffLen);
void pwsf_ModifyPrincipalInLocalRealm(char* principalName, const char *realmName, PWAccessFeatures* access, UInt32 oldDuration, char **inOutBuff, size_t *inOutBuffLen);
bool pwsf_ScanForRealm( const char *inKAdminText, char *outRealm, size_t inRealmMaxSize );
int pwsf_SetPrincipalAdministratorState( const char *inPrincipal, bool inAdmin, bool inSignalHUP );
void pwsf_GeneratePasswordForPrincipal( const char *inPassword, const char *inPrincipal, char *outPassword );

#ifdef __cplusplus
};
#endif

#endif
