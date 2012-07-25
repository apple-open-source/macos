#include "portable.h"
#include "slap.h"
/*
 *  psauth.h
 *  AuthTest
 *
 *  Created by gbv on Mon May 20 2002.
 *  Copyright (c) 2003 Apple Computer, Inc., All rights reserved.
 *
 */

enum {
    kAuthNoError = 0,
    kAuthSASLError = 1,
    kAuthOtherError = 2,
    kAuthKeyError = 3,
    kAuthenticationError = 4
};

#define PASSWORD_SERVER_AUTH_TYPE "ApplePasswordServer"
#define BASIC_AUTH_TYPE "basic"
#define SHADOWHASH_AUTH_TYPE "ShadowHash"

int CheckAuthType(char* inAuthAuthorityData, char* authType);
int DoPSAuth(char* userName, char* password, char* inAuthAuthorityData, Connection *conn, const char *dn);

