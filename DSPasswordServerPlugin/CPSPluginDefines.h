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
 *  CPSPluginDefines.h
 *  PasswordServerPlugin
 */
 
#ifndef __CPSPLUGINDEFINES__
#define __CPSPLUGINDEFINES__


#ifdef __cplusplus
#include <PasswordServer/CReplicaFile.h>
extern "C" {
#endif

	#include <stdio.h>
	#include <openssl/rc5.h>
	#include <openssl/cast.h>
	#include <netdb.h>
	#include "sasl.h"
	#include "AuthFile.h"
	#include "key.h"
#ifdef __cplusplus
};
#else
typedef void CReplicaFile;
#endif


#define kDHX_SASL_Name						"DHX"
#define kAuthNative_Priority				"DIGEST-MD5 CRAM-MD5 DHX TWOWAYRANDOM"
#define kPasswordServerPrefixStr			"/PasswordServer/"
#define kSASLListPrefix						"(SASL "
#define kEmptyPasswordAltStr				"<1-empty-insecure-1>"
#define kPasswordServerPortStr				"3659"
#define kMaxUserNameLength					255

typedef struct AuthInfo {
    char username[kMaxUserNameLength + 1];
    char *password;
    long passwordLen;
    Boolean successfulAuth;
	bool methodCanSetPassword;
} AuthInfo;

typedef struct sPSServerEntry {
	int fd;
	bool lastContact;
	bool ipFromNode;
	char ip[64];
	char port[12];
	char dns[256];
	char id[34];
} sPSServerEntry;

// Context data structure
typedef struct sPSContextData {
	char *psName;										// domain or ip address of passwordserver
	char psPort[10];									// port # of the password server
	unsigned long offset;										// offset for GetDirNodeInfo data extraction
	char localaddr[NI_MAXHOST + NI_MAXSERV + 1];
	char remoteaddr[NI_MAXHOST + NI_MAXSERV + 1];
    
    sasl_conn_t *conn;
    FILE *serverIn, *serverOut;
    int fd;
	sasl_callback_t callbacks[5];
    
	char *rsaPublicKeyStr;
    Key *rsaPublicKey;
	char rsaPublicKeyHash[34];
	
    AuthMethName *mech;
    int mechCount;
    
    AuthInfo last;						// information for the current authorization
    AuthInfo nao;						// information for the last authorization that was not "auth-only"
    
	CReplicaFile *replicaFile;
	CFMutableArrayRef serverList;
	sPSServerEntry serverProvidedFromNode;
	bool providedNodeOnlyOrFail;
	RC5_32_KEY rc5Key;
	bool madeFirstContact;
	char *syncFilePath;
	unsigned long pushByteCount;
	unsigned char psIV[10];
	bool castKeySet;
	CAST_KEY castKey;
	unsigned char castIV[10];
	unsigned char castReceiveIV[10];
} sPSContextData;

typedef struct sPSContinueData {
	unsigned long		fAuthPass;
	unsigned char *		fData;
	unsigned long 		fDataLen;
    sasl_secret_t *		fSASLSecret;
	char				fUsername[kMaxUserNameLength + 1];
	unsigned long 		fDataPos;
} sPSContinueData;



#endif


