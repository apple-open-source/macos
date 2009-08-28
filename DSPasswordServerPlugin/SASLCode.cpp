/* 
 * Copyright (c) 2001 Carnegie Mellon University.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The name "Carnegie Mellon University" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For permission or any other legal
 *    details, please contact  
 *      Office of Technology Transfer
 *      Carnegie Mellon University
 *      5000 Forbes Avenue
 *      Pittsburgh, PA  15213-3890
 *      (412) 268-4387, fax: (412) 268-7395
 *      tech-transfer@andrew.cmu.edu
 *
 * 4. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Computing Services
 *     at Carnegie Mellon University (http://www.cmu.edu/computing/)."
 *
 * CARNEGIE MELLON UNIVERSITY DISCLAIMS ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY BE LIABLE
 * FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * (portions copyright Apple Computer, Inc.)
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

#include <fcntl.h>
#include <sys/time.h>
#include <syslog.h>
#include <poll.h>
#include "SASLCode.h"
#include "CPSUtilities.h"

extern int errno;

/* in ms */
#define CONNECT_TIMEOUT 1000

/* remove \r\n at end of the line */
void sasl_chop(char *s)
{
    char *p;

    if (s == NULL || *s == '\0')
        return;
    
    p = s + strlen(s) - 1;
    if (p[0] == '\n') {
        *p-- = '\0';
    }
    if (p >= s && p[0] == '\r') {
        *p-- = '\0';
    }
}


//-------------
int getrealm(void *context /*__attribute__((unused))*/, 
		    int id,
		    const char **availrealms,
		    const char **result)
{
    /* paranoia check */
    if (id != SASL_CB_GETREALM) return SASL_BADPARAM;
    if (!result) return SASL_BADPARAM;

    if ( availrealms ) {
        *result = *availrealms;
    }
    
    return SASL_OK;
}


int simple(void *context /*__attribute__((unused))*/,
		  int id,
		  const char **result,
		  unsigned *len)
{
    sPSContinueData *pContinue = NULL;

    //syslog(LOG_INFO, "in simple\n");

    /* paranoia check */
    if ( result == NULL || context == NULL )
        return SASL_BADPARAM;
    
	pContinue = (sPSContinueData *)context;
	
    *result = NULL;
    
    switch (id) {
        case SASL_CB_USER:
        case SASL_CB_AUTHNAME:
            //printf("please enter an authentication id: ");
            *result = pContinue->fUsername;
            break;
            
        default:
            return SASL_BADPARAM;
    }
    
    if (*result != NULL && len != NULL)
        *len = (int)strlen(*result);
    
    return SASL_OK;
}


int
getsecret(sasl_conn_t *conn,
	  void *context /*__attribute__((unused))*/,
	  int id,
	  sasl_secret_t **psecret)
{
    sPSContinueData *pContinue = NULL;
	
    //syslog(LOG_INFO, "in getsecret\n");

    /* paranoia check */
    if (! conn || ! psecret || id != SASL_CB_PASS)
        return SASL_BADPARAM;
    
	if ( context == NULL )
		return SASL_BADPARAM;
	
	pContinue = (sPSContinueData *)context;
	
    *psecret = NULL;
    
    if ( pContinue->fSASLSecret != NULL )
    	*psecret = pContinue->fSASLSecret;
    
    return SASL_OK;
}


int getconn(const char *host, const char *port, int *outSocket)
{
    char servername[1024];
    struct sockaddr_in sin;
    int sock = 0;
    int siResult = 0;
    int rc;
	struct in_addr inetAddr;
	char *endPtr = NULL;
	struct timeval timeoutVal = { 10, 0 };
	struct timeval sendTimeoutVal = { 10, 0 };
	struct addrinfo *res, *res0;
    
    if ( host==NULL || port==NULL || outSocket==NULL )
        return -1;
    
    try
    {
        strlcpy(servername, host, sizeof(servername));
        
		/* map hostname -> IP */
		rc = inet_aton(servername, &inetAddr);
        if ( rc == 1 )
		{
			sin.sin_addr.s_addr = inetAddr.s_addr;
        }
		else
		{
			rc = getaddrinfo( servername, NULL, NULL, &res0 );
			if (rc != 0) {
				syslog(LOG_INFO,"getaddrinfo");
				throw((int)-1);
			}
			
			for ( res = res0; res != NULL; res = res->ai_next )
			{
				if ( res->ai_family != AF_INET || res->ai_addrlen != sizeof(sockaddr_in) )
					continue;
				
				memcpy( &sin.sin_addr, &(((struct sockaddr_in *)(res->ai_addr))->sin_addr.s_addr), 4 );
			}
			
			freeaddrinfo( res0 );
        }
        
		/* map port -> num */
		sin.sin_port = htons(strtol(port, &endPtr, 10));
        if ((sin.sin_port == 0) || (endPtr == port)) {
			syslog(LOG_INFO, "port '%s' unknown\n", port);
			throw((int)-1);
		}
		
        sin.sin_family = AF_INET;
        
        /* connect */
		for ( int dontgetzero = 0; dontgetzero < 5; dontgetzero++ )
		{
			sock = socket(AF_INET, SOCK_STREAM, 0);
			if (sock < 0) {
				syslog(LOG_INFO,"socket");
				throw((int)-1);
			}
			else
			if ( sock > 0 )
				break;
        }
		
		if ( setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeoutVal, (socklen_t)sizeof(timeoutVal) ) == -1 )
		{
			syslog(LOG_INFO,"setsockopt SO_RCVTIMEO");
            throw((int)-1);
		}
		if ( setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &sendTimeoutVal, (socklen_t)sizeof(sendTimeoutVal) ) == -1 )
		{
			syslog(LOG_INFO, "setsockopt SO_SNDTIMEO");
			//throw((int)-1);	// not fatal
		}
		
        int flags = fcntl(sock, F_GETFL, NULL);
        flags |= O_NONBLOCK;
        fcntl(sock, F_SETFL, flags);
        errno = 0;
        if (connect(sock, (struct sockaddr *) &sin, (socklen_t)sizeof (sin)) < 0) {
            if (errno != EINPROGRESS) {
                    syslog(LOG_INFO,"connect");
                    throw((int)-1);
            }
            struct pollfd pollList;
            pollList.fd = sock;
            pollList.events = POLLSTANDARD;
            pollList.revents = 0;
            if (poll(&pollList, 1, CONNECT_TIMEOUT) < 1) {
                    throw((int)-1);
            }
            if (!(pollList.revents & POLLOUT)) {
                    throw((int)-1);
            }
            int val = 1;
            socklen_t s = sizeof(int);
            getsockopt(sock, SOL_SOCKET, SO_ERROR, (void*)(&val), &s);
            if (val) {
                    throw((int)-1);
            }
        }
        flags &= ~O_NONBLOCK;
        fcntl(sock, F_SETFL, flags);
    }
    
    catch( int error )
    {
        siResult = error;
		if ( sock > 0 ) {
			close( sock );
			sock = -1;
		}
    }
    
    *outSocket = sock;
    
    return siResult;
}




/* 
 * Copyright (c) 2001 Carnegie Mellon University.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The name "Carnegie Mellon University" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For permission or any other legal
 *    details, please contact  
 *      Office of Technology Transfer
 *      Carnegie Mellon University
 *      5000 Forbes Avenue
 *      Pittsburgh, PA  15213-3890
 *      (412) 268-4387, fax: (412) 268-7395
 *      tech-transfer@andrew.cmu.edu
 *
 * 4. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Computing Services
 *     at Carnegie Mellon University (http://www.cmu.edu/computing/)."
 *
 * CARNEGIE MELLON UNIVERSITY DISCLAIMS ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY BE LIABLE
 * FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
 

#include "SASLCode.h"

const unsigned char *COLON = (unsigned char *)":";

bool UTF8_In_8859_1(const unsigned char *base, size_t len)
{
    const unsigned char *scan, *end;
    
    end = base + len;
    for (scan = base; scan < end; ++scan) {
		if (*scan > 0xC3)
			break;			/* abort if outside 8859-1 */
		if (*scan >= 0xC0 && *scan <= 0xC3) {
			if (++scan == end || *scan < 0x80 || *scan > 0xBF)
			break;
		}
    }
    
    /* if scan >= end, then this is a 8859-1 string. */
    return (scan >= end);
}

/*
 * if the string is entirely in the 8859-1 subset of UTF-8, then translate to
 * 8859-1 prior to MD5
 */
void MD5_UTF8_8859_1(
	MD5_CTX * ctx,
	bool In_ISO_8859_1,
	const unsigned char *base,
	size_t len)
{
    const unsigned char *scan, *end;
    unsigned char cbuf;
    
    end = base + len;
    
    /* if we found a character outside 8859-1, don't alter string */
    if (!In_ISO_8859_1) {
		MD5_Update(ctx, base, len);
		return;
    }
    /* convert to 8859-1 prior to applying hash */
    do {
		for (scan = base; scan < end && *scan < 0xC0; ++scan);
		if (scan != base)
			MD5_Update(ctx, base, scan - base);
		if (scan + 1 >= end)
			break;
		cbuf = ((scan[0] & 0x3) << 6) | (scan[1] & 0x3f);
		MD5_Update(ctx, &cbuf, 1);
		base = scan + 2;
    }
    while (base < end);
}


void DigestCalcSecret(
	unsigned char *pszUserName,
	unsigned char *pszRealm,
	unsigned char *Password,
	size_t PasswordLen,
	HASH HA1)
{
    bool In_8859_1;
    MD5_CTX Md5Ctx;
    
    /* Chris Newman clarified that the following text in DIGEST-MD5 spec
       is bogus: "if name and password are both in ISO 8859-1 charset"
       We shoud use code example instead */
    
	MD5_Init(&Md5Ctx);
    
    /* We have to convert UTF-8 to ISO-8859-1 if possible */
    In_8859_1 = UTF8_In_8859_1(pszUserName, (int)strlen((char *) pszUserName));
    MD5_UTF8_8859_1(&Md5Ctx, In_8859_1,
		    pszUserName, (int)strlen((char *) pszUserName));
    
    MD5_Update(&Md5Ctx, COLON, 1);
    
    if (pszRealm != NULL && pszRealm[0] != '\0') {
		/* a NULL realm is equivalent to the empty string */
		MD5_Update(&Md5Ctx, pszRealm, strlen((char *) pszRealm));
    }      
    
    MD5_Update(&Md5Ctx, COLON, 1);
    
    /* We have to convert UTF-8 to ISO-8859-1 if possible */
    In_8859_1 = UTF8_In_8859_1(Password, PasswordLen);
    MD5_UTF8_8859_1(&Md5Ctx, In_8859_1,
		    Password, PasswordLen);
    
   	MD5_Final(HA1, &Md5Ctx);
}


void hmac_md5_init(HMAC_MD5_CTX *hmac,
			 const unsigned char *key,
			 size_t key_len)
{
	unsigned char k_ipad[65];    /* inner padding -
				* key XORd with ipad
				*/
	unsigned char k_opad[65];    /* outer padding -
				* key XORd with opad
				*/
	unsigned char tk[16];
	int i;
	/* if key is longer than 64 bytes reset it to key=MD5(key) */
	if (key_len > 64) {
	
	MD5_CTX      tctx;
	
	MD5_Init(&tctx); 
	MD5_Update(&tctx, key, key_len); 
	MD5_Final(tk, &tctx); 
	
	key = tk; 
	key_len = 16; 
	} 
	
	/*
	* the HMAC_MD5 transform looks like:
	*
	* MD5(K XOR opad, MD5(K XOR ipad, text))
	*
	* where K is an n byte key
	* ipad is the byte 0x36 repeated 64 times
	* opad is the byte 0x5c repeated 64 times
	* and text is the data being protected
	*/
	
	/* start out by storing key in pads */
	memset(k_ipad, '\0', sizeof k_ipad);
	memset(k_opad, '\0', sizeof k_opad);
	memcpy( k_ipad, key, key_len);
	memcpy( k_opad, key, key_len);
	
	/* XOR key with ipad and opad values */
	for (i=0; i<64; i++) {
	k_ipad[i] ^= 0x36;
	k_opad[i] ^= 0x5c;
	}
	
	MD5_Init(&hmac->ictx);                   /* init inner context */
	MD5_Update(&hmac->ictx, k_ipad, 64);     /* apply inner pad */
	
	MD5_Init(&hmac->octx);                   /* init outer context */
	MD5_Update(&hmac->octx, k_opad, 64);     /* apply outer pad */
	
	/* scrub the pads and key context (if used) */
	memset(&k_ipad, 0, sizeof(k_ipad));
	memset(&k_opad, 0, sizeof(k_opad));
	memset(&tk, 0, sizeof(tk));
	
	/* and we're done. */
}

/* The precalc and import routines here rely on the fact that we pad
 * the key out to 64 bytes and use that to initialize the md5
 * contexts, and that updating an md5 context with 64 bytes of data
 * leaves nothing left over; all of the interesting state is contained
 * in the state field, and none of it is left over in the count and
 * buffer fields.  So all we have to do is save the state field; we
 * can zero the others when we reload it.  Which is why the decision
 * was made to pad the key out to 64 bytes in the first place. */
void pwsf_hmac_md5_precalc(HMAC_MD5_STATE *state,
			    const unsigned char *key,
			    size_t key_len)
{
	HMAC_MD5_CTX hmac;
	//unsigned loop;
	
	hmac_md5_init(&hmac, key, key_len);
	
	/*
	for (loop = 0; loop < 4; loop++) {
		state->istate[loop] = htonl(hmac.ictx.state[loop]);
		state->ostate[loop] = htonl(hmac.octx.state[loop]);
	}
	*/
	
	state->istate[0] = htonl(hmac.ictx.A);
	state->istate[1] = htonl(hmac.ictx.B);
	state->istate[2] = htonl(hmac.ictx.C);
	state->istate[3] = htonl(hmac.ictx.D);
	
	state->ostate[0] = htonl(hmac.octx.A);
	state->ostate[1] = htonl(hmac.octx.B);
	state->ostate[2] = htonl(hmac.octx.C);
	state->ostate[3] = htonl(hmac.octx.D);
	
	memset(&hmac, 0, sizeof(hmac));
}


