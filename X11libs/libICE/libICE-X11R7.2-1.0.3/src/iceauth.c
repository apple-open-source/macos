/* $Xorg: iceauth.c,v 1.4 2001/02/09 02:03:26 xorgcvs Exp $ */
/******************************************************************************


Copyright 1993, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.

Author: Ralph Mor, X Consortium
******************************************************************************/
/* $XFree86: xc/lib/ICE/iceauth.c,v 3.5 2001/12/14 19:53:36 dawes Exp $ */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <X11/ICE/ICElib.h>
#include "ICElibint.h"
#include <X11/ICE/ICEutil.h>

#include <time.h>
#define Time_t time_t

static int binaryEqual ();

static int was_called_state;

/*
 * MIT-MAGIC-COOKIE-1 is a sample authentication method implemented by
 * the SI.  It is not part of standard ICElib.
 */


char *
IceGenerateMagicCookie (len)

int len;

{
    char    *auth;
    long    ldata[2];
    int	    seed;
    int	    value;
    int	    i;
    
    if ((auth = (char *) malloc (len + 1)) == NULL)
	return (NULL);

#ifdef ITIMER_REAL
    {
	struct timeval  now;
	X_GETTIMEOFDAY (&now);
	ldata[0] = now.tv_sec;
	ldata[1] = now.tv_usec;
    }
#else
    {
#ifndef __UNIXOS2__
	long    time ();
#endif
	ldata[0] = time ((long *) 0);
	ldata[1] = getpid ();
    }
#endif
    seed = (ldata[0]) + (ldata[1] << 16);
    srand (seed);
    for (i = 0; i < len; i++)
    {
	value = rand ();
	auth[i] = value & 0xff;
    }
    auth[len] = '\0';

    return (auth);
}



IcePoAuthStatus
_IcePoMagicCookie1Proc (iceConn, authStatePtr, cleanUp, swap,
    authDataLen, authData, replyDataLenRet, replyDataRet, errorStringRet)

IceConn		iceConn;
IcePointer	*authStatePtr;
Bool 		cleanUp;
Bool		swap;
int     	authDataLen;
IcePointer	authData;
int 		*replyDataLenRet;
IcePointer	*replyDataRet;
char    	**errorStringRet;

{
    if (cleanUp)
    {
	/*
	 * We didn't allocate any state.  We're done.
	 */

	return (IcePoAuthDoneCleanup);
    }

    *errorStringRet = NULL;

    if (*authStatePtr == NULL)
    {
	/*
	 * This is the first time we're being called.  Search the
	 * authentication data for the first occurence of
	 * MIT-MAGIC-COOKIE-1 that matches iceConn->connection_string.
	 */

	unsigned short  length;
	char		*data;

	_IceGetPoAuthData ("ICE", iceConn->connection_string,
	    "MIT-MAGIC-COOKIE-1", &length, &data);

	if (!data)
	{
	    char *tempstr =
		"Could not find correct MIT-MAGIC-COOKIE-1 authentication";

	    *errorStringRet = (char *) malloc (strlen (tempstr) + 1);
	    if (*errorStringRet)
		strcpy (*errorStringRet, tempstr);

	    return (IcePoAuthFailed);
	}
	else
	{
	    *authStatePtr = (IcePointer) &was_called_state;

	    *replyDataLenRet = length;
	    *replyDataRet = data;

	    return (IcePoAuthHaveReply);
	}
    }
    else
    {
	/*
	 * We should never get here for MIT-MAGIC-COOKIE-1 since it is
	 * a single pass authentication method.
	 */

	char *tempstr = "MIT-MAGIC-COOKIE-1 authentication internal error";

	*errorStringRet = (char *) malloc (strlen (tempstr) + 1);
	if (*errorStringRet)
	    strcpy (*errorStringRet, tempstr);

	return (IcePoAuthFailed);
    }
}



IcePaAuthStatus
_IcePaMagicCookie1Proc (iceConn, authStatePtr, swap,
    authDataLen, authData, replyDataLenRet, replyDataRet, errorStringRet)

IceConn		iceConn;
IcePointer	*authStatePtr;
Bool		swap;
int     	authDataLen;
IcePointer	authData;
int 		*replyDataLenRet;
IcePointer	*replyDataRet;
char    	**errorStringRet;

{
    *errorStringRet = NULL;
    *replyDataLenRet = 0;
    *replyDataRet = NULL;

    if (*authStatePtr == NULL)
    {
	/*
	 * This is the first time we're being called.  We don't have
	 * any data to pass to the other client.
	 */

	*authStatePtr = (IcePointer) &was_called_state;

	return (IcePaAuthContinue);
    }
    else
    {
	/*
	 * Search the authentication data for the first occurence of
	 * MIT-MAGIC-COOKIE-1 that matches iceConn->connection_string.
	 */

	unsigned short  length;
	char		*data;

	_IceGetPaAuthData ("ICE", iceConn->connection_string,
	    "MIT-MAGIC-COOKIE-1", &length, &data);

	if (data)
	{
	    IcePaAuthStatus stat;

	    if (authDataLen == length &&
	        binaryEqual ((char *) authData, data, authDataLen))
	    {
		stat = IcePaAuthAccepted;
	    }
	    else
	    {
		char *tempstr = "MIT-MAGIC-COOKIE-1 authentication rejected";

		*errorStringRet = (char *) malloc (strlen (tempstr) + 1);
		if (*errorStringRet)
		    strcpy (*errorStringRet, tempstr);

		stat = IcePaAuthRejected;
	    }

	    free (data);
	    return (stat);
	}
	else
	{
	    /*
	     * We should never get here because in the ConnectionReply
	     * we should have passed all the valid methods.  So we should
	     * always find a valid entry.
	     */

	    char *tempstr =
		"MIT-MAGIC-COOKIE-1 authentication internal error";

	    *errorStringRet = (char *) malloc (strlen (tempstr) + 1);
	    if (*errorStringRet)
		strcpy (*errorStringRet, tempstr);

	    return (IcePaAuthFailed);
	}
    }
}



/*
 * local routines
 */

static int
binaryEqual (a, b, len)

register char		*a, *b;
register unsigned	len;

{
    while (len--)
	if (*a++ != *b++)
	    return 0;
    return 1;
}
