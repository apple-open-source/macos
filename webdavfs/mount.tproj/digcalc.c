/*
 *	digcalc.c
 *	webdavfs
 *
 *	Created by lutherj on Mon Oct 01 2001.
 *	Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 */

/*
 * Copyright (C) The Internet Society (1999).  All Rights Reserved.
 * 
 * This document and translations of it may be copied and furnished to
 * others, and derivative works that comment on or otherwise explain it
 * or assist in its implementation may be prepared, copied, published
 * and distributed, in whole or in part, without restriction of any
 * kind, provided that the above copyright notice and this paragraph are
 * included on all such copies and derivative works.  However, this
 * document itself may not be modified in any way, such as by removing
 * the copyright notice or references to the Internet Society or other
 * Internet organizations, except as needed for the purpose of
 * developing Internet standards in which case the procedures for
 * copyrights defined in the Internet Standards process must be
 * followed, or as required to translate it into languages other than
 * English.
 * 
 * The limited permissions granted above are perpetual and will not be
 * revoked by the Internet Society or its successors or assigns.
 * 
 * This document and the information contained herein is provided on an
 * "AS IS" basis and THE INTERNET SOCIETY AND THE INTERNET ENGINEERING
 * TASK FORCE DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO ANY WARRANTY THAT THE USE OF THE INFORMATION
 * HEREIN WILL NOT INFRINGE ANY RIGHTS OR ANY IMPLIED WARRANTIES OF
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <openssl/md5.h>

#include <string.h>
#include "digcalc.h"

/*****************************************************************************/

/*
 * webdavfs doesn't support the "auth-int" "quality of protection" directive or the
 * "MD5-sess" algorithm value yet, so we might as well skip of the code
 * that checks for those. If in the future, these are supported, the code
 * should probably be changed so that string compares aren't needed to
 * determine the qop or algorithm values.
 */
#define AUTH_INT_SUPPORT 0
#define MD5_SESS_SUPPORT 0

/*****************************************************************************/

static void CvtHex(IN HASH Bin, OUT HASHHEX Hex)
{
	unsigned short i;
	unsigned char j;

	for (i = 0; i < HASHLEN; i++)
	{
		j = (Bin[i] >> 4) & 0xf;
		if (j <= 9)
		{
			Hex[i * 2] = (j + '0');
		}
		else
		{
			Hex[i * 2] = (j + 'a' - 10);
		}
		j = Bin[i] & 0xf;
		if (j <= 9)
		{
			Hex[i * 2 + 1] = (j + '0');
		}
		else
		{
			Hex[i * 2 + 1] = (j + 'a' - 10);
		}
	}
	Hex[HASHHEXLEN] = '\0';
}

/*****************************************************************************/

/*
 * calculate H(A1) as per spec
 */
void DigestCalcHA1(
	IN char *pszAlg,
	IN char *pszUserName,
	IN char *pszRealm,
	IN char *pszPassword,
	IN char *pszNonce,
	IN char *pszCNonce,
	OUT HASHHEX SessionKey)
{
	MD5_CTX Md5Ctx;
	HASH HA1;

	MD5_Init(&Md5Ctx);
	MD5_Update(&Md5Ctx, (unsigned char *)pszUserName, strlen(pszUserName));
	MD5_Update(&Md5Ctx, (unsigned char *)":", 1);
	MD5_Update(&Md5Ctx, (unsigned char *)pszRealm, strlen(pszRealm));
	MD5_Update(&Md5Ctx, (unsigned char *)":", 1);
	MD5_Update(&Md5Ctx, (unsigned char *)pszPassword, strlen(pszPassword));
	MD5_Final((unsigned char *)HA1, &Md5Ctx);
#if MD5_SESS_SUPPORT
	if ((strlen(pszAlg) == strlen("md5-sess")) &&
			(strncasecmp(pszAlg, "md5-sess", strlen("md5-sess")) == 0))
	{
		MD5_Init(&Md5Ctx);
		MD5_Update(&Md5Ctx, (unsigned char *)HA1, HASHLEN);
		MD5_Update(&Md5Ctx, (unsigned char *)":", 1);
		MD5_Update(&Md5Ctx, (unsigned char *)pszNonce,
					strlen(pszNonce));
		MD5_Update(&Md5Ctx, (unsigned char *)":", 1);
		MD5_Update(&Md5Ctx, (unsigned char *)pszCNonce,
					strlen(pszCNonce));
		MD5_Final((unsigned char *)HA1, &Md5Ctx);
	}
#endif /* MD5_SESS_SUPPORT */
	CvtHex(HA1, SessionKey);
}

/*****************************************************************************/

/*
 * calculate request-digest/response-digest as per HTTP Digest spec
 */
void DigestCalcResponse(
	IN HASHHEX HA1,			/* H(A1) */
	IN char *pszNonce,		/* nonce from server */
	IN char *pszNonceCount, /* 8 hex digits */
	IN char *pszCNonce,		/* client nonce */
	IN char *pszQop,		/* qop-value: "", "auth", "auth-int" */
	IN char *pszMethod,		/* method from the request */
	IN char *pszDigestUri,	/* requested URL */
	IN HASHHEX HEntity,		/* H(entity body) if qop="auth-int" */
	OUT HASHHEX Response	/* request-digest or response-digest */
	)
{
	MD5_CTX Md5Ctx;
	HASH HA2;
	HASH RespHash;
	HASHHEX HA2Hex;

	/* calculate H(A2) */
	MD5_Init(&Md5Ctx);
	MD5_Update(&Md5Ctx, (unsigned char *)pszMethod, strlen(pszMethod));
	MD5_Update(&Md5Ctx, (unsigned char *)":", 1);
	MD5_Update(&Md5Ctx, (unsigned char *)pszDigestUri,
			strlen(pszDigestUri));
#if AUTH_INT_SUPPORT
	if ((strlen(pszQop) == strlen("auth-int")) &&
			(strncasecmp(pszQop, "auth-int", strlen("auth-int")) == 0))
	{
		MD5_Update(&Md5Ctx, (unsigned char *)":", 1);
		MD5_Update(&Md5Ctx, (unsigned char *)HEntity, HASHHEXLEN);
	}
#endif /* AUTH_INT_SUPPORT */
	MD5_Final((unsigned char *)HA2, &Md5Ctx);
	CvtHex(HA2, HA2Hex);

	/* calculate response */
	MD5_Init(&Md5Ctx);
	MD5_Update(&Md5Ctx, (unsigned char *)HA1, HASHHEXLEN);
	MD5_Update(&Md5Ctx, (unsigned char *)":", 1);
	MD5_Update(&Md5Ctx, (unsigned char *)pszNonce, strlen(pszNonce));
	MD5_Update(&Md5Ctx, (unsigned char *)":", 1);
	if (*pszQop)
	{
		MD5_Update(&Md5Ctx, (unsigned char *)pszNonceCount,
					strlen(pszNonceCount));
		MD5_Update(&Md5Ctx, (unsigned char *)":", 1);
		MD5_Update(&Md5Ctx, (unsigned char *)pszCNonce,
					strlen(pszCNonce));
		MD5_Update(&Md5Ctx, (unsigned char *)":", 1);
		MD5_Update(&Md5Ctx, (unsigned char *)pszQop, strlen(pszQop));
		MD5_Update(&Md5Ctx, (unsigned char *)":", 1);
	}
	MD5_Update(&Md5Ctx, (unsigned char *)HA2Hex, HASHHEXLEN);
	MD5_Final((unsigned char *)RespHash, &Md5Ctx);
	CvtHex(RespHash, Response);
}

/*****************************************************************************/
