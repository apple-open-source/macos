/*
 *  psauth.h
 *  AuthTest
 *
 *  Created by gbv on Mon May 20 2002.
 *  Copyright (c) 2002 __MyCompanyName__. All rights reserved.
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

void ConvertBinaryToHex( const unsigned char *inData, unsigned long inLen, char *outHexStr );
void ConvertHexToBinary( const char *inHexStr, unsigned char *outData, unsigned long *outLen );


int CheckAuthType(char* inAuthAuthorityData, char* authType);
int DoPSAuth(char* userName, char* password, char* inAuthAuthorityData);
