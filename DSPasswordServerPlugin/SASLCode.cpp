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
#include "SASLCode.h"
#include "CPSUtilities.h"

extern int errno;

/* remove \r\n at end of the line */
void sasl_chop(char *s)
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
        *len = strlen(*result);
    
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


long getconn(const char *host, const char *port, int *outSocket)
{
    char servername[1024];
    struct sockaddr_in sin;
    int sock = 0;
    long siResult = 0;
    int rc;
	struct in_addr inetAddr;
	char *endPtr = NULL;
	struct timeval timeoutVal = { 30, 0 };
	struct addrinfo *res, *res0;
    
    if ( host==NULL || port==NULL || outSocket==NULL )
        return -1;
    
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
			rc = getaddrinfo( servername, NULL, NULL, &res0 );
			if (rc != 0) {
				syslog(LOG_INFO,"getaddrinfo");
				throw((long)-1);
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
			throw((long)-1);
		}
		
        sin.sin_family = AF_INET;
        
        /* connect */
		for ( int dontgetzero = 0; dontgetzero < 5; dontgetzero++ )
		{
			sock = socket(AF_INET, SOCK_STREAM, 0);
			if (sock < 0) {
				syslog(LOG_INFO,"socket");
				throw((long)-1);
			}
			else
			if ( sock > 0 )
				break;
        }
		
		if ( setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeoutVal, sizeof(timeoutVal) ) == -1 )
		{
			syslog(LOG_INFO,"setsockopt");
            throw((long)-1);
		}
		
        if (connect(sock, (struct sockaddr *) &sin, sizeof (sin)) < 0) {
            syslog(LOG_INFO,"connect");
            throw((long)-1);
        }
    }
    
    catch( long error )
    {
        siResult = error;
    }
    
    *outSocket = sock;
    
    return siResult;
}


