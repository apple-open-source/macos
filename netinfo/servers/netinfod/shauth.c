/*
 *  shauth.c
 *
 *  Created by kaw on Wed Apr 16 2003.
 *  Copyright (c) 2003 Apple Computer. All rights reserved.
 *
 */

#include <stdio.h>
#include <string.h>		//used for strcpy, etc.
#include <stdlib.h>		//used for malloc
#include <openssl/sha.h>

#include "shauth.h"
#include "psauth.h"

void SHA1Encode(unsigned char *output, unsigned char *input, unsigned int len)
{
	SHA_CTX context = {};
	
	SHA1_Init(&context);
	SHA1_Update(&context, (unsigned char *)input, len);
	SHA1_Final(output, &context);
}

void CalculateShadowHash(const char *utf8Password, unsigned char outHash[20])
{
	char *password[129] = {0};
	int passLen = 0;
	
	if (utf8Password == NULL || outHash == NULL) return;
	
	if (strlen(utf8Password) < 128)
		passLen = strlen(utf8Password);
	else
		passLen = 128;
		
	memmove(password, utf8Password, passLen);
		
	SHA1Encode(outHash, (unsigned char *)password, passLen);	
}

int DoSHAuth(char* inUserName, char* inPassword, char* inGUID)
{
	int				result				= kAuthOtherError;
	unsigned char	calculatedHash[20]	= {0};
	char			storedHashHex[105]	= {0};
	unsigned char	storedHash[20]		= {0};
	FILE		   *hashFile			= NULL;
	char		   *path				= NULL;
	unsigned long	outLen				= 0;

	if ( (inUserName == NULL) || (inPassword == NULL) )
	{
		return result;
	}

	/* retrieve the stored hash */
	if (inGUID != NULL)
	{
		path = (char*)calloc(1, strlen("/var/db/shadow/hash/") + strlen(inGUID) + 1);
	}
	else
	{
		path = (char*)calloc(1, strlen("/var/db/shadow/hash/") + strlen(inUserName) + 1);
	}
	if ( path != NULL )
	{
		if (inGUID != NULL)
		{
			sprintf(path, "%s%s", "/var/db/shadow/hash/", inGUID);
		}
		else
		{
			sprintf(path, "%s%s", "/var/db/shadow/hash/", inUserName);
		}
		hashFile = fopen(path, "r");
		if (hashFile != NULL)
		{
			if ( fread((char *)storedHashHex, 1, 104, hashFile) == 104 )
			{
				ConvertHexToBinary( (char *)(storedHashHex+64), (unsigned char *)storedHash, &outLen );
				if (outLen == 20)
				{
					/* calc the SHA1 hash for the given password */
					CalculateShadowHash((const char *)inPassword, calculatedHash);
					if ( memcmp(storedHash, calculatedHash, outLen) == 0 )
					{
						result = kAuthNoError;
					}
				}
			}
			fclose(hashFile);
		}
	}

	return result;
}
