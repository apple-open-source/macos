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
#include "CPSPlugin.h"

#if 0
#define DEBUGLOG(A,args...)		CShared::LogIt( 0x0F, (A), ##args )
#else
#define DEBUGLOG(A,args...)		
#endif


/* remove \r\n at end of the line */
void chop(char *s)
{
    char *p;

    if (s==NULL)
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
    /* paranoia check */
    if (! result)
        return SASL_BADPARAM;
    
    *result = NULL;
    
    switch (id) {
        case SASL_CB_USER:
        case SASL_CB_AUTHNAME:
            //printf("please enter an authentication id: ");
            *result = ((sPSContextData *)context)->last.username;
            break;
            
        default:
            return SASL_BADPARAM;
    }
    
    if (*result != NULL && len != NULL)
        *len = strlen(*result);
    
    return SASL_OK;
}


int
getsecret(sasl_conn_t *conn,
	  void *context /*__attribute__((unused))*/,
	  int id,
	  sasl_secret_t **psecret)
{
    size_t len = 0;
    sasl_secret_t *xsec = NULL;
    
    /* paranoia check */
    if (! conn || ! psecret || id != SASL_CB_PASS)
        return SASL_BADPARAM;
    
    *psecret = NULL;
    
    if (((sPSContextData *)context)->last.password != NULL)
    {
        len = ((sPSContextData *)context)->last.passwordLen;
        
        xsec = (sasl_secret_t *) malloc(sizeof(sasl_secret_t) + len + 1);
    	if (xsec == NULL)
            return SASL_NOMEM;
        
        xsec->len = len;
        memcpy( xsec->data, ((sPSContextData *)context)->last.password, len );
        xsec->data[len] = '\0';
    }
    
    *psecret = xsec;
    return SASL_OK;
}


sInt32 getconn(const char *host, const char *port, int *outSocket)
{
    char servername[1024];
    struct sockaddr_in sin;
    struct hostent *hp;
    int sock = 0;
    sInt32 siResult = eDSNoErr;
    int rc;
	struct in_addr inetAddr;
	char *endPtr = NULL;
	
    if ( host==NULL || port==NULL || outSocket==NULL )
        return eParameterError;
    
    try
    {
        strncpy(servername, host, sizeof(servername) - 1);
        servername[sizeof(servername) - 1] = '\0';
        
		/* map hostname -> IP */
        rc = inet_aton(servername, &inetAddr);
        if ( rc == 1 )
		{
			sin.sin_addr.s_addr = inetAddr.s_addr;
        }
		else
		{
			if ((hp = gethostbyname(servername)) == NULL) {
				DEBUGLOG("gethostbyname");
				throw((sInt32)eDSServiceUnavailable);
			}
			memcpy(&sin.sin_addr, hp->h_addr, hp->h_length);
        }
        
		/* map port -> num */
		sin.sin_port = htons(strtol(port, &endPtr, 10));
        if ((sin.sin_port == 0) || (endPtr == port)) {
			DEBUGLOG( "port '%s' unknown\n", port);
			throw((sInt32)eParameterError);
		}
		
        sin.sin_family = AF_INET;
        
        /* connect */
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            DEBUGLOG("socket");
            throw((sInt32)eDSServiceUnavailable);
        }
        
        if (connect(sock, (struct sockaddr *) &sin, sizeof (sin)) < 0) {
            DEBUGLOG("connect");
            throw((sInt32)eDSServiceUnavailable);
        }
    }
    
    catch( sInt32 error )
    {
        siResult = error;
    }
    
    *outSocket = sock;
    
    return siResult;
}


