/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
/*		@(#)webdav_authcache.c		*
 *		(c) 2000   Apple Computer, Inc.	 All Rights Reserved
 *
 *
 *		webdav_authcache.c -- WebDAV in memory authorization cache
 *
 *		MODIFICATION HISTORY:
 *				10-MAR-2000		Clark Warner	  File Creation
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/syslog.h>
#include <pthread.h>
#include <Security/SecKey.h>
#include <Security/SecKeychain.h>
#include <Security/SecKeychainItem.h>
#include <Security/SecKeychainSearch.h>
#include "webdav_authentication.h"

/*
 * DEBUG (which defines the state of DEBUG_ASSERT_PRODUCTION_CODE),
 * DEBUG_ASSERT_COMPONENT_NAME_STRING and DEBUG_ASSERT_MESSAGE must be
 * defined before including AssertMacros.h
 */
#define DEBUG_ASSERT_COMPONENT_NAME_STRING "webdavfs"
#define DEBUG_ASSERT_MESSAGE(componentNameString, \
	assertionString, \
	exceptionLabelString, \
	errorString, \
	fileName, \
	lineNumber, \
	errorCode) \
	WebDAVDebugAssert(componentNameString, \
	assertionString, \
	exceptionLabelString, \
	errorString, \
	fileName, \
	lineNumber, \
	errorCode)

#include <AssertMacros.h>

#include "fetch.h"
#include "digcalc.h"
#include "webdavd.h"
#include "webdav_authcache.h"

/*****************************************************************************/

#ifdef DEBUG

/* WebDAVDebugAssert prototype*/
static void
WebDAVDebugAssert(const char * componentNameString,
	const char * assertionString, const char * exceptionLabelString,
	const char * errorString, const char * fileName, long lineNumber,
	int errorCode);

/*
 * WebDAVDebugAssert is called to display assert messages in DEBUG builds
 */
static void
WebDAVDebugAssert(const char * componentNameString,
	const char * assertionString, const char * exceptionLabelString,
	const char * errorString, const char * fileName, long lineNumber,
	int errorCode)
{
	if ( (assertionString != NULL) && (*assertionString != '\0') )
		syslog(LOG_INFO, "Assertion failed: %s: %s", componentNameString, assertionString);
	else
		syslog(LOG_INFO, "Check failed: %s:", componentNameString);
	if ( exceptionLabelString != NULL )
		syslog(LOG_INFO, "    %s", exceptionLabelString);
	if ( errorString != NULL )
		syslog(LOG_INFO, "    %s", errorString);
	if ( fileName != NULL )
		syslog(LOG_INFO, "    file: %s", fileName);
	if ( lineNumber != 0 )
		syslog(LOG_INFO, "    line: %ld", lineNumber);
	if ( errorCode != 0 )
		syslog(LOG_INFO, "    error: %d", errorCode);
}

#endif /* DEBUG */

/*****************************************************************************/

/* local structures */

/*
 * Constant_strlen is used to get length of constant strings instead of strlen
 * so that the compiler can determine the length instead of runtime code.
 */
#define Constant_strlen(s) (sizeof(s) - 1)

/*
 * The URIRec struct holds a single URI an authentication.
 * The string lengths are precalculated to speed up comparisons.
 */
struct URIRec
{
	struct URIRec *next;	/* next URIRec in list */
	char *server;			/* the URI's server string (rfc 2396, section 3.2.2) */
	size_t serverLen;		/* length of server string */
	char *absPath;			/* the URI's abs_path string (rfc 2396, section 3.) */
	size_t absPathLen;		/* length of absPath string */
};
typedef struct URIRec URIRec;

/*
 * Declare typedef for WebdavAuthcacheElement here since
 * MakeAuthHeaderProcPtr needs it.
 */
typedef struct WebdavAuthcacheElement WebdavAuthcacheElement;

/*
 * A MakeAuthHeaderProc function knows how to create an authentication header
 * from the requestData and scheme-specific authData. The authentication
 * header is returned in requestData->authorization.
 */
typedef void (*MakeAuthHeaderProcPtr)(WebdavAuthcacheRetrieveRec *retrieveRec,
	WebdavAuthcacheElement *elem);
#define CallMakeAuthHeaderProc(userRoutine, retrieveRec, elem) \
	(*(userRoutine))((retrieveRec), (elem))

/*
 * A FreeAuthDataProc function knows how to free the scheme-specific authData.
 */
typedef void (*FreeAuthDataProcPtr)(void *authData);
#define CallFreeAuthDataProc(userRoutine, authData) \
	(*(userRoutine))((authData))

/*
 * The WebdavAuthcacheElement struct holds the authentication information
 * for user's authentication to a domain. The domain is specified by the list
 * of URI stored in the linked list domainHead. If uriCount is zero and
 * domainHead is NULL, then there was no domain specified in the
 * authentication challenge.
 */
struct WebdavAuthcacheElement
{
	struct WebdavAuthcacheElement *next; /* next element in list */
	uid_t uid;				/* user ID */
	int isProxy;			/* if TRUE, this element is for a proxy */
	char *realmStr;			/* A pointer to a case-sensitive */
							/* C string containing the realm-value */
							/* string for this authentication. */
							/* This string, along with the user */
							/* ID defines the protection space */
							/* for this authentication. */
							/* If this field is NULL, then this */
							/* element needs to be updated */
							/* before it can be */
							/* used for authentication. */
							/* (rfc 2617, section 1.2) */
	ChallengeSecurityLevelType scheme; /* the scheme */
	char *username;			/* A pointer to a C string */
							/* containing the username */
	char *password;			/* A pointer to a C string */
							/* containing the password */
	unsigned long uriCount; /* number of URIRec in domainHead */
							/* list; 0 = no specified domain */
	URIRec *domainHead;		/* head URIRec of domain list; */
							/* NULL = no specified domain */
	MakeAuthHeaderProcPtr makeProcPtr; /* scheme-specific MakeAuthHeader */
							/* function */
	FreeAuthDataProcPtr freeProcPtr; /* scheme-specific FreeAuthData */
							/* function */
	void *authData;			/* scheme-specific cached */
							/* authentication data */
	AuthFlagsType authflags; /* The keychain options for this authorization */
};

/*
 * The WebdavAuthcacheHeader struct holds the list of cached authentication
 * information.	 If count is zero, then there are no WebdavAuthcacheElements
 * in the list.
 */
struct WebdavAuthcacheHeader
{
	pthread_mutex_t lock;	/* lock for WebdavAuthcacheHeader and */
							/* related structs */
	unsigned long count;	/* number of WebdavAuthcacheElements in */
							/* list; 0 = none */
	WebdavAuthcacheElement *head; /* head WebdavAuthcacheElement of list; */
							/* NULL = empty */
	int proxyElementCount;	/* a reference count: if non-zero, a */
							/* proxy element has been */
							/* inserted into the authcache */
	char *cnonce;			/* client nonce string for this server connection */
	unsigned long generation;	/* generation count of cache (never zero)*/
};
typedef struct WebdavAuthcacheHeader WebdavAuthcacheHeader;

/*****************************************************************************/

/* non scheme-specific authcache queue routines */
static void GetNextNextWebdavAuthcacheElement(WebdavAuthcacheElement **elem);
static void EnqueueWebdavAuthcacheElement(WebdavAuthcacheElement *elem);
static WebdavAuthcacheElement * DequeueWebdavAuthcacheElement(uid_t uid,
	int isProxy, char *realmStr);
static void FreeWebdavAuthcacheElement(WebdavAuthcacheElement *elem);
static int InsertPlaceholder(WebdavAuthcacheInsertRec *insertRec);
static OSStatus KeychainItemCopyAccountPassword(SecKeychainItemRef itemRef, char *user, char *pass);

/*****************************************************************************/

/* globals */

static int gAuthcacheInitialized = 0;
static WebdavAuthcacheHeader gAuthcacheHeader;

/*****************************************************************************/
/* parsing routines */
/*****************************************************************************/

/* parsing function prototypes */

static char * ParseChallenge(char *params, char **directive, char **value,
	int *error);

/*****************************************************************************/

/*
 * ParseChallenge parses the params challenge string. If an auth-scheme is
 * found, it is returned as the directive in a newly allocated buffer and
 * value is set to NULL. If an auth-param is found, the auth-param directive
 * is returned as the directive in a newly allocated buffer and the
 * auth-param value is returned as the value in a newly allocated buffer.
 *	
 * The rules for challenge and auth-param are (rfc 2617, section 1.2):
 *	challenge	= auth-scheme 1*SP 1#auth-param
 *	auth-scheme = token
 *	auth-param	= token "=" ( token | quoted-string )
 */
static char * ParseChallenge(char *params, char **directive, char **value,
	int *error)
{
	char *token;
	
	/* set outputs to NULL */
	*directive = *value = NULL;
	*error = 0;
	
	/* find first non-LWS character */
	params = SkipLWS(params);
	
	/* anything left? */
	if ( *params != '\0' )
	{
		/* found start of the token */
		token = params;
		
		/* find the end of the token */
		params = SkipToken(params);
		
		/*
		 * Make sure we didn't run out of params string,  and that
		 * the token isn't zero length
		 */
		require_action((*params != '\0') && (params != token),
			malformedDirectiveName, *error = EINVAL);
		
		/* allocate space for the directive string */
		*directive = malloc((size_t)(params - token + 1));
		require_action(*directive != NULL, malloc_directive,
			*error = ENOMEM);
		
		/* copy the token to directive string and terminate it */
		strncpy(*directive, token, (size_t)(params - token));
		(*directive)[params - token] = '\x00';
		
		/* is the token an auth-scheme or a auth-param? */
		if ( *params == '=')
		{
			/* it's an auth-param */
			
			/* skip over the '=' */
			++params;
			
			/* is value a token or a quoted-string? */
			if ( *params == '\"' )
			{
				/* it's a quoted-string */
				
				/* skip over quote */
				++params;
				token = params;
				/* find '\"' marking the end of the quoted-string */
				params = SkipQuotedString(params);
				
				/*
				 * make sure we didn't run out of params string or end up
				 * with zero length string
				 */
				require_action(*params, malformedValueQuotedString,
					*error = EINVAL);
				
				/* allocate space for value string */
				*value = malloc((size_t)(params - token + 1));
				require_action(*value != NULL, malloc_value,
					*error = ENOMEM);
				
				/* copy the token to value string */
				strncpy(*value, token, (size_t)(params - token));
				(*value)[params - token] = '\x00';
				
				/* skip over '\"' */
				++params;
			}
			else
			{
				/* it's a token */
				
				/* mark start of the value token */
				token = params;
				
				/* find the end of the value token */
				params = SkipToken(params);
				
				/* allocate space for value string */
				*value = malloc((size_t)(params - token + 1));
				require_action(*value != NULL, malloc_value,
					*error = ENOMEM);
				
				/* copy the token to value string */
				strncpy(*value, token, (size_t)(params - token));
				(*value)[params - token] = '\x00';
			}
			
			/* skip over LWS (if any) */
			params = SkipLWS(params);
			
			/* if there's any string left after the LWS... */
			if ( *params != '\0' )
			{
				/* we should have found a comma */
				require_action(*params == ',', missingCommaSeparator,
					*error = EINVAL);
				
				/* skip over one or more commas */
				while ( *params == ',' )
				{
					++params;
				}
			}
			
			/*
			 * params is now pointing at first character after comma
			 * delimiter, or at end of string
			 */
		}
		else
		{
			/* it's an auth-scheme */
			
			/* skip over LWS leaving params pointing at first auth-param */
			params = SkipLWS(params);
		}
	}
	
	return ( params );
	
	/**********************/
	
missingCommaSeparator:
	free(*value);
malloc_value:
malformedValueQuotedString:
	/* free directive memory already allocated */
	free(*directive);

malloc_directive:
malformedDirectiveName:
	/* burn up rest of string */
	while ( *params != '\0' )
	{
		++params;
	}
	
	syslog(LOG_ERR, "ParseChallenge: %s", strerror(*error));
	return ( params );
}

/*****************************************************************************/

/*
 * ParseQOPs parses qop-values from the qop-options value params string.
 *
 * The rules for qop-options is (rfc 2617, section 3.2.1):
 *	qop-options = "qop" "=" <"> 1#qop-value <">
 *	qop-value	= "auth" | "auth-int" | token
 *
 * Since the params string has already had the quotes stripped from it,
 * this routine only needs to handle 1#qop-value to get the qop-value.
 */
static char * ParseQOPs(char *params, char **qopValue, int *error)
{
	char *token;
	
	/* set outputs to NULL */
	*qopValue = NULL;
	*error = 0;
	
	/* find first non-LWS character */
	params = SkipLWS(params);
	
	/* anything left? */
	if ( *params != '\0' )
	{
		/* mark start of the qopValue token */
		token = params;
		
		/* find the end of the qopValue token */
		params = SkipToken(params);
		
		/* make sure we didn't end up with zero length token */
		require_action(params != token, malformedValueToken,
			*error = EINVAL);
		
		/* allocate space for qopValue string */
		*qopValue = malloc((size_t)(params - token + 1));
		require_action(*qopValue != NULL, malloc_qopValue,
			*error = ENOMEM);
		
		/* copy the token to qopValue string */
		strncpy(*qopValue, token, (size_t)(params - token));
		(*qopValue)[params - token] = '\x00';
		
		/* skip over LWS (if any) */
		params = SkipLWS(params);
		
		/* if there's any string left after the LWS... */
		if ( *params != '\0' )
		{
			/* we should have found a comma */
			require_action(*params == ',', missingCommaSeparator,
				*error = EINVAL);
			
			/* skip over one or more commas */
			while ( *params == ',' )
			{
				++params;
			}
		}
		
		/*
		 * params is now pointing at first character after comma
		 * delimiter, or at end of string
		 */
	}
	
	return ( params );
	
	/**********************/
	
missingCommaSeparator:
	free(*qopValue);
malloc_qopValue:
malformedValueToken:
	/* burn up rest of string */
	while ( *params != '\0' )
	{
		++params;
	}
	
	syslog(LOG_ERR, "ParseQOPs: %s", strerror(*error));
	return ( params );
}

/*****************************************************************************/
/* Scheme specific constants, data types and routines */
/*****************************************************************************/

/*
 * The authData structure for the Digest scheme
 */
struct AuthDataDigest
{
	char *nonce;			/* the server-specified nouce data string */
							/* (rfc 2617, section 3.2.1) */
	char *opaque;			/* the server-specified opaque data string */
							/* NULL = no opaque string */
							/* (rfc 2617, section 3.2.1) */
	char *algorithm;		/* the server-specified algorithm string */
							/* NULL = no algorithm */
							/* (rfc 2617, section 3.2.1) */
	char *uriList;			/* the URI list that define the domain in */
							/* the form of: URI ( 1*SP URI ) */
							/* (rfc 2617, section 3.2.1) */
	int stale;				/* TRUE if stale directive is "true" */
	HASHHEX HA1;			/* H(A1) digest string which can be */
							/* precalculated since we only support */
							/* the "MD5" algorithm */
							/* (rfc 2617, section 3.2.2.2) */
	/* other fields will be needed if optional qop directives are used */
	char *qop;				/* the qop we're using ("auth" - we don't support" auth-int") */
							/* NULL = no qop */
							/* (rfc 2617, section 3.2.1) */
	u_int32_t nonceCount;	/* the client nonce-count */
							/* initialized to 1 each time the nonce is set */
							/* and is incremented after each time the nonce is sent */
							/* (rfc 2617, section 3.2.2 ) */
};
typedef struct AuthDataDigest AuthDataDigest;

/*
 * The authData structure for the Basic scheme
 */
struct AuthDataBasic
{
	char *credentialsStr;	/* the basic-credentials base64 string */
							/* (rfc 2617, section 2) */
};
typedef struct AuthDataBasic AuthDataBasic;

/*
 * scheme-specific prototypes
 */
 
/* Digest authentication scheme specific */
static AuthDataDigest * AllocateAuthDataDigest(void);
static void FreeAuthDataDigest(void *authData);
static int ParseAuthParmsDigest(char *authParam, char **realmStr,
	AuthDataDigest *authData);
static void MakeAuthHeaderDigest(WebdavAuthcacheRetrieveRec *retrieveRec,
	WebdavAuthcacheElement *elem);
static int EvaluateDigest(WebdavAuthcacheEvaluateRec *evaluateRec,
	char *authParam);
char *GetURI(char *params, char **uri, int *error);
static int AddURIToURIRec(char *uri, URIRec *theURIRec);
static int InsertDigest(WebdavAuthcacheInsertRec *insertRec, char *authParam);

/* Basic authentication scheme specific */
static AuthDataBasic * AllocateAuthDataBasic(void);
static void FreeAuthDataBasic(void *authData);
static int ParseAuthParmsBasic(char *authParam, char **realmStr);
static void MakeAuthHeaderBasic(WebdavAuthcacheRetrieveRec *retrieveRec,
	WebdavAuthcacheElement *elem);
static int EvaluateBasic(WebdavAuthcacheEvaluateRec *evaluateRec,
	char *authParam);
static int InsertBasic(WebdavAuthcacheInsertRec *insertRec, char *authParam);

/*****************************************************************************/
/*****************************************************************************/

/*
 * AllocateAuthDataDigest allocates a cleared AuthDataDigest record.
 */
static AuthDataDigest * AllocateAuthDataDigest(void)
{
	AuthDataDigest *authData;
	
	authData = calloc(sizeof(AuthDataDigest), 1);
	if (authData == NULL)
	{
		syslog(LOG_ERR, "AllocateAuthDataDigest: %s", strerror(errno));
	}
	return ( authData );
}

/*****************************************************************************/

/*
 * FreeAuthDataDigest frees all memory alllocted for a AuthDataDigest record.
 */
static void FreeAuthDataDigest(void *authData)
{
	AuthDataDigest *digestAuthData;
	
	digestAuthData = authData;
		
		if ( digestAuthData->uriList != NULL )
		{
			free(digestAuthData->uriList);
		}

		if ( digestAuthData->nonce != NULL )
		{
			free(digestAuthData->nonce);
		}

		if ( digestAuthData->opaque != NULL )
		{
			free(digestAuthData->opaque);
		}

		if ( digestAuthData->algorithm != NULL )
		{
			free(digestAuthData->algorithm);
		}
		if ( digestAuthData->qop != NULL )
		{
			free(digestAuthData->qop);
		}
	
	free(digestAuthData);
}

/*****************************************************************************/

/*
 * ParseAuthParmsDigest parses and validates the auth-param section of the
 * Digest scheme's challenge. If the challenge is valid, the realm string is
 * returned, the authData struct is filled in, and 0 is returned.
 */
static int ParseAuthParmsDigest(char *authParam, char **realmStr,
	AuthDataDigest *authData)
{
	int error;
	int directiveLength;
	char *directive;
	char *value;
	
	/* default error */
	error = EINVAL;
	
	authData->stale = FALSE;
	
	/* parse until end of string */
	while ( *authParam != '\0' )
	{
		/* get next directive and value */
		authParam = ParseChallenge(authParam, &directive, &value, &error);
		require_noerr_quiet(error, ParseChallenge);
		
		directiveLength = strlen(directive);
		if ( (Constant_strlen("realm") == directiveLength) &&
			(strncasecmp(directive, "realm", (size_t)directiveLength) == 0) )
		{
			/* return the realmStr */
			*realmStr = value;
		}
		else if ( (Constant_strlen("domain") == directiveLength) &&
			(strncasecmp(directive, "domain", (size_t)directiveLength) == 0) )
		{
			/* return the uri list string */
			authData->uriList = value;
		}
		else if ( (Constant_strlen("nonce") == directiveLength) &&
			(strncasecmp(directive, "nonce", (size_t)directiveLength) == 0) )
		{
			/* return the nonce string */
			authData->nonce = value;
			authData->nonceCount = 0;
		}
		else if ( (Constant_strlen("opaque") == directiveLength) &&
			(strncasecmp(directive, "opaque", (size_t)directiveLength) == 0) )
		{
			/* return the opaque string */
			authData->opaque = value;
		}
		else if ( (Constant_strlen("stale") == directiveLength) &&
			(strncasecmp(directive, "stale", (size_t)directiveLength) == 0) )
		{
			/* is stale directive "true" or something else? */
			authData->stale = strncasecmp(value, "true", Constant_strlen("true")) == 0;
			free (value);
		}
		else if ( (Constant_strlen("algorithm") == directiveLength) &&
			(strncasecmp(directive, "algorithm", (size_t)directiveLength) == 0) )
		{
			/*
			 * We only support MD5 -- reject challenge quietly if it's anything
			 * else and hopefully another challenge can be used.
			 */
			require_action_quiet( (Constant_strlen("MD5") == strlen(value)) &&
				(strncasecmp(value, "MD5", Constant_strlen("MD5")) == 0),
				unsupportedAlgorithm,
				free(directive); free(value); error = EINVAL);
				
			authData->algorithm = value;
		}
		else if ( (Constant_strlen("qop") == directiveLength) &&
			(strncasecmp(directive, "qop", (size_t)directiveLength) == 0) )
		{
			char *qopValueList;
			char *qopValue;
			
			authData->qop = NULL;	/* in case we don't find a qop-value we support */

			qopValueList = value;			
			while ( *qopValueList != '\0' )
			{
				/* get next qopValue */
				qopValueList = ParseQOPs(qopValueList, &qopValue, &error);
				if ( error != 0 )
				{
					break;
				}
				
				/* we only support the "auth" qop-value */
				if ( (Constant_strlen("auth") == strlen(qopValue)) &&
					(strncasecmp(qopValue, "auth", Constant_strlen("auth")) == 0) )
				{
					/* found it so save it */
					authData->qop = qopValue;
					break;
				}
				else
				{
					/* free the qopValue string and continue */
					free(qopValue);
				}
			}
			
			/* free the value string */
			free(value);
		}
		else
		{
			/* unrecognized directive -- ignore it */
			free(value);
		}
		
		/* done with this directive string */
		free(directive);
	}
	
	/* the required directives are realm and nonce */
	require_action((*realmStr != NULL) && (authData->nonce != NULL),
		missingDirectives, error = EINVAL);
	
	error = 0;	/* no errors */
	
	return ( error );

missingDirectives:
unsupportedAlgorithm:
	syslog(LOG_ERR, "ParseAuthParmsDigest: %s", strerror(error));
ParseChallenge:
	return ( error );
}

/*****************************************************************************/

/*
 * MakeAuthHeaderDigest adds the Digest credentials from the
 * WebdavAuthcacheElement parameter to the retrieveRec->authorization string
 * (creating the string if needed).
 */
static void MakeAuthHeaderDigest(WebdavAuthcacheRetrieveRec *retrieveRec,
	WebdavAuthcacheElement *elem)
{
	int error;
	AuthDataDigest *authData;
	char *credentialsStr;
	unsigned int credentialsLength;
	char *requestDigestStr;
	char *existingAuthorization;
	char *uriString;
	char nonceCountStr[9];
	
	error = 0;
	authData = (AuthDataDigest*)elem->authData;
	
	/*
	 * Build the uri that will match the request-uri in the request-line.
	 * (i.e., add the query if there is one)
	 */
	uriString = malloc(strlen(retrieveRec->uri) +
		((retrieveRec->query != NULL) ? strlen(retrieveRec->query) : 0) + 1 );
	require_action(uriString != NULL, malloc_uriString, error = ENOMEM);
	strcpy(uriString, retrieveRec->uri);
	if ( retrieveRec->query != NULL )
	{
		strcat(uriString, retrieveRec->query);
	}
	
	/* determine length of credentials string */
	if ( elem->isProxy )
	{
		/* add count for terminator here */
		credentialsLength = sizeof("Proxy-Authorization: Digest\r\n");
	}
	else
	{
		/* add count for terminator here */
		credentialsLength = sizeof("Authorization: Digest\r\n");
	}
	credentialsLength += (Constant_strlen(" username=\"\"") + strlen(elem->username));
	credentialsLength += (Constant_strlen(", realm=\"\"") + strlen(elem->realmStr));
	credentialsLength += (Constant_strlen(", nonce=\"\"") + strlen(authData->nonce));
	credentialsLength += (Constant_strlen(", uri=\"\"") + strlen(uriString));
	credentialsLength += (Constant_strlen(", response=\"\"") + HASHHEXLEN);
	if ( authData->algorithm != NULL )
	{
		credentialsLength += (Constant_strlen(", algorithm=\"\"") +
			strlen(authData->algorithm));
	}
	if ( authData->opaque != NULL )
	{
		credentialsLength += (Constant_strlen(", opaque=\"\"") +
			strlen(authData->opaque));
	}
	if ( authData->qop != NULL )
	{
		credentialsLength += (Constant_strlen(", qop=\"\"") +
			strlen(authData->qop) +
			Constant_strlen(", cnonce=\"\"") +
			strlen(gAuthcacheHeader.cnonce) +
			Constant_strlen(", nc=\"\"") +
			8); /* nc-value is always 8 characters */
	}
	
	/* allocate memory for credentials string */
	credentialsStr = malloc(credentialsLength);
	require_action(credentialsStr != NULL, malloc_credentialsStr,
		error = ENOMEM);
	
	/* get the request-digest string */
	requestDigestStr = malloc(sizeof(HASHHEX));
	require_action(requestDigestStr != NULL, malloc_requestDigestStr,
		error = ENOMEM);
	if ( authData->qop == NULL )
	{
		DigestCalcResponse(authData->HA1, authData->nonce, "", "", "",
			retrieveRec->method, uriString, NULL, requestDigestStr);
	}
	else
	{
		/* increment the nonce-count */
		++authData->nonceCount;
		/* and then create nonceCountStr from authData->nonceCount */
		snprintf(nonceCountStr, sizeof(nonceCountStr), "%.8lx",
			(long unsigned int)authData->nonceCount);
		DigestCalcResponse(authData->HA1, authData->nonce, nonceCountStr,
			gAuthcacheHeader.cnonce, authData->qop,
			retrieveRec->method, uriString, NULL, requestDigestStr);
	}
	
	/* build the credentials string */
	strcpy(credentialsStr, (elem->isProxy ?
		"Proxy-Authorization: Digest" : "Authorization: Digest"));
	strcat(credentialsStr, " username=\"");
	strcat(credentialsStr, elem->username);
	strcat(credentialsStr, "\", realm=\"");
	strcat(credentialsStr, elem->realmStr);
	strcat(credentialsStr, "\", nonce=\"");
	strcat(credentialsStr, authData->nonce);
	strcat(credentialsStr, "\", uri=\"");
	strcat(credentialsStr, uriString);
	strcat(credentialsStr, "\", response=\"");
	strcat(credentialsStr, requestDigestStr);
	strcat(credentialsStr, "\"");
	if ( authData->algorithm != NULL )
	{
		strcat(credentialsStr, ", algorithm=\"");
		strcat(credentialsStr, authData->algorithm);
		strcat(credentialsStr, "\"");
	}
	if ( authData->opaque != NULL )
	{
		strcat(credentialsStr, ", opaque=\"");
		strcat(credentialsStr, authData->opaque);
		strcat(credentialsStr, "\"");
	}
	if	(authData->qop != NULL )
	{
		strcat(credentialsStr, ", qop=\"");
		strcat(credentialsStr, authData->qop);
		/*
		 * The nonce-count is not quoted because some proxies don't
		 * handle a quoted-string for the nonce-count value.
		 */
		strcat(credentialsStr, "\", nc=");
		strcat(credentialsStr, nonceCountStr);
		strcat(credentialsStr, ", cnonce=\"");
		strcat(credentialsStr, gAuthcacheHeader.cnonce);
		strcat(credentialsStr, "\"");
	}
	strcat(credentialsStr, "\r\n");
	
	if ( retrieveRec->authorization == NULL )
	{
		/* this is the first authorization header we're adding */
		retrieveRec->authorization = credentialsStr;
	}
	else
	{
		/*
		 * Allocate a buffer big enough for existing authorization header
		 * string and the one we're adding. Then copy both strings into it.
		 */
		existingAuthorization = retrieveRec->authorization;
		retrieveRec->authorization =
			malloc(strlen(existingAuthorization) +
			credentialsLength);
		require_action(retrieveRec->authorization != NULL, malloc_authorization,
			retrieveRec->authorization = existingAuthorization; free (credentialsStr));
		
		strcpy(retrieveRec->authorization, existingAuthorization);
		strcat(retrieveRec->authorization, credentialsStr);
		free(credentialsStr);
		free(existingAuthorization);
	}
	
malloc_authorization:
	free(requestDigestStr);
malloc_requestDigestStr:
malloc_credentialsStr:
	free(uriString);
malloc_uriString:
	if ( error )
	{
		syslog(LOG_ERR, "MakeAuthHeaderDigest: %s", strerror(error));
	}
	return;
}

/*****************************************************************************/

/*
 * GetURI parses the next URI from params, the list of URI that define a
 * challenge's domain. GetURI returns a pointer further into the params string
 * (possibly the end of the string).
 */
char *GetURI(char *params, char **uri, int *error)
{
	char *stringStart;
	
	/* set outputs */
	*uri = NULL;
	*error = 0;
		
	/* anything to parse? */
	if ( *params != '\0' )
	{
		/* keep the start of the URI string */
		stringStart = params;
		
		/* find the end of the URI */
		while ( *params != '\0' )
		{
			if ( *params != ' ' )
			{
				/* skip non-SP characters */
				++params;
				continue;
			}
			
			/* found the end of the non-SP run */
			break;
		}
		
		/* allocate space for the uri string */
		*uri = malloc((size_t)(params - stringStart + 1));
		require_action(*uri != NULL, malloc_uri, *error = ENOMEM);

		/* copy the string at stringStart to uri string and terminate it */
		strncpy(*uri, stringStart, (size_t)(params - stringStart));
		(*uri)[params - stringStart] = '\0';
	
		/* skip over SP (if any) between URI (if any more) */
		while ( *params != '\0' )
		{
			if ( *params == ' ' )
			{
				/* skip SP characters */
				++params;
				continue;
			}
			
			/* found the end of the SP run */
			break;
		}
		
		/*
		 * params is now pointing at first character of the next URI,
		 * or at end of string
		 */
	}
		 
malloc_uri:
	if ( *error )
	{
		syslog(LOG_ERR, "GetURI: %s", strerror(*error));
	}
	return ( params );
}

/*****************************************************************************/

static void FreeURIRec(URIRec *theURIRec)
{
	if ( theURIRec->server )
	{
		free(theURIRec->server);
	}
	if ( theURIRec->absPath )
	{
		free(theURIRec->absPath);
	}
	free(theURIRec);
}

/*****************************************************************************/

/*
 * AddURIToURIRec adds the server and abs_path strings from the uri parameter
 * to theURIRec parameter.
 *
 * The uri parameter can either be an absoluteURI or an abs_path
 * (rfc 2396, section 3). If the uri parameter is an abs_path, then the server
 * string is assumed to be the same as the global dest_server. If the uri
 * parameter is an absoluteURI, then the server string (without the port
 * number) and the abs_path are parsed from absoluteURI string.
 */
static int AddURIToURIRec(char *uri, URIRec *theURIRec)
{
	int error;
	
	theURIRec->server = theURIRec->absPath = NULL;
	
	/*
	 * Is this an abs_path (or an empty string), or is it an absoluteURI?
	 * abs_path starts with '/'; absoluteURI does not
	 */
	if ( *uri == '/' || *uri == '\0' )
	{
		/* uri is an abs_path (or an empty string) */
		
		/* server is dest_server */
		theURIRec->server = malloc(strlen(dest_server) + 1);
		require_action(theURIRec->server != NULL, malloc_theURIRec_server,
			error = ENOMEM);
		
		strcpy(theURIRec->server, dest_server);
		
		/* is uri an empty string? */
		if ( *uri != '\0' )
		{
			/* absPath is the uri -- save it with the % encoding removed */
			theURIRec->absPath = percent_decode(uri);
		}
		else
		{
			/* an empty abs_path is equivalent to an abs_path of "/" */
			theURIRec->absPath = malloc(2);
			if ( theURIRec->absPath )
			{
				theURIRec->absPath[0] = '/';
				theURIRec->absPath[1] = '\0';
			}
		}
		require_action(theURIRec->absPath != NULL, malloc_theURIRec_absPath,
			error = ENOMEM);
	}
	else
	{
		/* uri is an absoluteURI */
		char *bytes;
		char *server;
	  
		/* skip over the URI scheme to the authority */
		bytes = uri;
		while ( *bytes != '\0' )
		{
			if ( *bytes == '/' && bytes[1] == '/' )
			{
				/* found end of URI scheme - skip over it and break */
				bytes += 2;
				break;
			}
			++bytes;
		}
		/* there better be some string left */
		require_action(*bytes != '\0', invalidAbsoluteURI, error = EINVAL);
		
		/* save start of server string */
		server = bytes;
		/*
		 * Find end of server string ignoring the port (if any).
		 * That will either be the end of the string,
		 * a ':' character, or a '/' character.
		 */
		while ( *bytes != '\0' )
		{
			if ( (*bytes == ':') || (*bytes == '/') )
			{
				/* found end of server string - break */
				break;
			}
			++bytes;
		}
		/* copy the server string */
		theURIRec->server = malloc((size_t)(bytes - server + 1));
		require_action(theURIRec->server != NULL, malloc_theURIRec_server,
			error = ENOMEM);
		
		/* copy and terminate it */
		strncpy(theURIRec->server, server, (size_t)(bytes - server));
		theURIRec->server[bytes - server] = '\0';
		
		/* was there a port? */
		if ( *bytes == ':' )
		{
			/* skip over the port */
			while ( *bytes != '\0' )
			{
				if ( *bytes == '/' )
				{
					/* found end of server string - break */
					break;
				}
				++bytes;
			}
		}
		
		/* is bytes an empty string? */
		if ( *bytes != '\0' )
		{
			/* absPath is bytes -- save it with the % encoding removed */
			theURIRec->absPath = percent_decode(bytes);
		}
		else
		{
			/* an empty abs_path is equivalent to an abs_path of "/" */
			theURIRec->absPath = malloc(2);
			if ( theURIRec->absPath )
			{
				theURIRec->absPath[0] = '/';
				theURIRec->absPath[1] = '\0';
			}
		}
		require_action(theURIRec->absPath != NULL, malloc_theURIRec_absPath,
			error = ENOMEM);
	}
	
	theURIRec->serverLen = strlen(theURIRec->server);
	theURIRec->absPathLen = strlen(theURIRec->absPath);
	
	return ( 0 );
	
	/**********************/
	
	/* Error cleanup */
	
malloc_theURIRec_server:
malloc_theURIRec_absPath:
invalidAbsoluteURI:
	syslog(LOG_ERR, "AddURIToURIRec: %s", strerror(error));
	return ( error );
}

/*****************************************************************************/

static int UpdateElementDigest(WebdavAuthcacheElement *elem)
{
	int error;
	AuthDataDigest *authData;
	char *params;
	URIRec *theURIRec;
	char *uri;
	URIRec	*domain;
	URIRec	*nextDomain;
	
	authData = elem->authData;
	error = 0;
	
	/* calculate (or recalculate) HA1 */
	DigestCalcHA1( authData->algorithm == NULL ? "" : authData->algorithm,
		elem->username, elem->realmStr, elem->password, "", "", authData->HA1);
	
	/* free the existing domain list's URIs and URIRecs (if any) */
	domain = elem->domainHead;
	elem->domainHead = NULL;
	while ( domain != NULL )
	{
		nextDomain = domain->next;
		FreeURIRec(domain);
		domain = nextDomain;
	}
	
	/* check for uriList and add it to the element's domain list if needed */
	if ( !elem->isProxy && (authData->uriList != NULL) )
	{
		/* add authdata->uriList to elem->domain */
		params = authData->uriList;
		while ( *params != '\0' )
		{
			/* allocate space for another URIRec */
			theURIRec = malloc(sizeof(URIRec));
			require_action(theURIRec != NULL, malloc_theURIRec, error = ENOMEM);
			
			/* get the next URI string from the list */
			params = GetURI(params, &uri, &error);
			require_noerr_action_quiet(error, GetURI, free(theURIRec));
			
			/* add it to the URIRec */
			error = AddURIToURIRec(uri, theURIRec);
			require_noerr_quiet(error, AddURIToURIRec);
			
			free(uri); /* free the uri */
			
			/* add the URIRec to the domain list */
			theURIRec->next = elem->domainHead;
			elem->domainHead = theURIRec;
			++elem->uriCount;
		}
	}
	
	return ( error );

	/**********************/
	
	/* Error cleanup */
	
AddURIToURIRec:
	free(uri);
GetURI:
	FreeURIRec(theURIRec);
	return ( error );
	
malloc_theURIRec:
	syslog(LOG_ERR, "UpdateElementDigest: %s", strerror(error));
	return ( error );
}

/*****************************************************************************/

/*
 * EvaluateDigest handles evaluate requests for the Digest scheme.
 */
static int EvaluateDigest(WebdavAuthcacheEvaluateRec *evaluateRec,
	char *authParam)
{
	int error;
	WebdavAuthcacheElement *elem;
	AuthDataDigest *authData;
	int foundPlaceHolder, foundElementToUpdate;
	
	/* allocate an AuthDataDigest structure */
	authData = AllocateAuthDataDigest();
	require_action_quiet(authData != NULL, AllocateAuthDataDigest, error = ENOMEM);
	
	/*
	 * can we handle this Digest challenge? If so, get the realmStr
	 * and authData.
	 */
	error = ParseAuthParmsDigest(authParam, &evaluateRec->realmStr, authData);
	require_noerr_quiet(error, ParseAuthParmsDigest);
	
	/* check for a placeholder or stale element */
	elem = NULL;	/* start with head of queue */
	foundPlaceHolder = foundElementToUpdate = FALSE;
	while ( TRUE )
	{
		GetNextNextWebdavAuthcacheElement(&elem);
		if ( elem == NULL )
		{
			/* no more elements in list */
			break;
		}
		
		/* if this is an element for this user and server/proxy */
		if ( (evaluateRec->uid == elem->uid) &&
			(evaluateRec->isProxy == elem->isProxy) )
		{
			if ( elem->realmStr == NULL )
			{
				/* found a placeholder so no UI is needed */
				foundPlaceHolder = TRUE;
				break;
			}
			else if ( strcmp(evaluateRec->realmStr, elem->realmStr) == 0 )
			{
				/* found element with matching realm but a different nonce string */
				foundElementToUpdate = TRUE;
				break;
			}
		}
	}

	/* do we need to update? */
	if ( authData->stale )
	{
		/* we should always have an element to update */
		require_action(foundElementToUpdate, elementToUpdateNotFound, error = EINVAL);
		
		/* free the old AuthDataDigest */
		FreeAuthDataDigest((AuthDataDigest *)elem->authData);
		
		/* replace it with the new AuthDataDigest */
		(AuthDataDigest *)elem->authData = authData;
		
		/* update the element with the new authData */
		error = UpdateElementDigest(elem);
		
		/* and indicate that we updated an existing element */
		evaluateRec->updated = TRUE;
	}
	else
	{
		/* done with authData */
		FreeAuthDataDigest(authData);
		
		if ( foundPlaceHolder )
		{
			/* found a placeholder so no UI is needed */
			evaluateRec->uiNotNeeded = TRUE;
		}
	}
	
	return ( 0 );
	
	/**********************/
	
	/* Error cleanup */
	
elementToUpdateNotFound:
	syslog(LOG_ERR, "EvaluateDigest: %s", strerror(error));
ParseAuthParmsDigest:
	FreeAuthDataDigest(authData);
AllocateAuthDataDigest:
	return ( error );
}

/*****************************************************************************/

/*
 * InsertDigest handles insert request for the Digest scheme.
 */
static int InsertDigest(WebdavAuthcacheInsertRec *insertRec, char *authParam)
{
	int error;
	char *realmStr;
	WebdavAuthcacheElement *elem;
	int elemInCache;
	AuthDataDigest *authData;
	
	/* allocate an AuthDataDigest structure */
	authData = AllocateAuthDataDigest();
	require_action_quiet(authData != NULL, AllocateAuthDataDigest, error = ENOMEM);
	
	/*
	 * can we handle this Digest challenge? If so, get the realmStr
	 * and authData.
	 */
	error = ParseAuthParmsDigest(authParam, &realmStr, authData);
	require_noerr_quiet(error, ParseAuthParmsDigest);
		
	/* is there an authentication already in the cache? */
	elem = NULL;	/* start with head of queue */
	while ( TRUE )
	{
		GetNextNextWebdavAuthcacheElement(&elem);
		if ( elem == NULL )
		{
			/* no more elements in list */
			break;
		}
		
		if ( (insertRec->uid == elem->uid) &&
			 (insertRec->isProxy == elem->isProxy) )
		{
			if ( elem->realmStr == NULL )
			{
				/* found a placeholder element -- use it */
				break;
			}
			/* make sure we don't insert a duplicate element */
			require_action(strcmp(realmStr, elem->realmStr) != 0,
				DuplicateDigestAuthcacheElement, error = 0);
		}
	}
	
	if ( elem == NULL )
	{
		/* no placeholder */
		elemInCache = FALSE;
		
		/* it's not there -- allocate it */
		elem = calloc(sizeof(WebdavAuthcacheElement), 1);
		require_action(elem != NULL, calloc_elem, error = ENOMEM);
		
		/* add uid and isProxy */
		elem->uid = insertRec->uid;
		elem->isProxy = insertRec->isProxy;
		
		/*
		 * add copies of username and password to
		 * WebdavAuthcacheElement
		 */
		elem->username = malloc(strlen(insertRec->username) + 1);
		require_action(elem->username != NULL, malloc_elem_username,
			error = ENOMEM);
		
		strcpy(elem->username, insertRec->username);
		
		elem->password = malloc(strlen(insertRec->password) + 1);
		require_action(elem->password != NULL, malloc_elem_password,
			error = ENOMEM);
		
		strcpy(elem->password, insertRec->password);
		/* now, it's got everything a placeholder element would have */
	}
	else
	{
		/* found a placeholder */
		elemInCache = TRUE;
	}
	
	/* initialize most other fields in element */
	elem->realmStr = realmStr;
	elem->scheme = kChallengeSecurityLevelDigest;
	elem->makeProcPtr = MakeAuthHeaderDigest;
	elem->freeProcPtr = FreeAuthDataDigest;
	elem->authData = authData;
	elem->authflags = ((insertRec->uid != 0) && (insertRec->uid != 1)) ?
		insertRec->authflags : kAuthNone;
	
	/* initialize the element with the authData */
	error = UpdateElementDigest(elem);
	
	/* add elem to the cache if it isn't already there */
	if ( !elemInCache )
	{
		EnqueueWebdavAuthcacheElement(elem);
	}
		
	return ( 0 );
	
	/**********************/
	
	/* Error cleanup */
	
malloc_elem_password:
	if ( !elemInCache )
	{
		free(elem->username);
	}
malloc_elem_username:
	if ( !elemInCache )
	{
		free(elem);
	}
calloc_elem:
	syslog(LOG_ERR, "InsertDigest: %s", strerror(error));
DuplicateDigestAuthcacheElement:
	free(realmStr);
ParseAuthParmsDigest:
	FreeAuthDataDigest(authData);
AllocateAuthDataDigest:
	return ( error );
}

/*****************************************************************************/
/*****************************************************************************/

/*
 * AllocateAuthDataBasic allocates a cleared AuthDataBasic record.
 */
static AuthDataBasic * AllocateAuthDataBasic(void)
{
	AuthDataBasic *authData;
	
	authData = calloc(sizeof(AuthDataBasic), 1);
	if (authData == NULL)
	{
		syslog(LOG_ERR, "AllocateAuthDataBasic: %s", strerror(errno));
	}
	return ( authData );
}

/*****************************************************************************/

/*
 * FreeAuthDataBasic frees all memory alllocted for a AuthDataBasic record.
 */
static void FreeAuthDataBasic(void *authData)
{
	AuthDataBasic *basicAuthData;
	
	basicAuthData = authData;
	if ( basicAuthData->credentialsStr != NULL )
	{
		free(basicAuthData->credentialsStr);
	}
	free(basicAuthData);
}

/*****************************************************************************/

/*
 * ParseAuthParmsBasic parses and validates the auth-param section of the
 * Basic scheme's challenge. If the challenge is valid, the realm string is
 * returned and 0 is returned.
 */
static int ParseAuthParmsBasic(char *authParam, char **realmStr)
{
	int error;
	int directiveLength;
	char *directive;
	char *value;
	
	/* default error */
	error = EINVAL;
	
	/* parse until end of string */
	while ( *authParam != '\0' )
	{
		/* get next directive and value */
		authParam = ParseChallenge(authParam, &directive, &value, &error);
		require_noerr_quiet(error, ParseChallenge);
		
		/* see if it is the realm directive */
		directiveLength = strlen(directive);
				
		/* Basic allows only the realm directive */
		require_action((Constant_strlen("realm") == directiveLength) &&
			(strncasecmp(directive, "realm", (size_t)directiveLength) == 0),
			unsupportedDirective,
			free(directive); free(value); error = EINVAL);
		
		/* done with the directive string */
		free(directive);
		
		/* return the realmStr */
		*realmStr = value;
		
		/* the Realm directive is required by Basic */
		error = 0;
	}
	check_noerr_string(error, "missing Realm directive");

unsupportedDirective:
	if ( error )
	{
		syslog(LOG_ERR, "ParseAuthParmsBasic: %s", strerror(error));
	}
ParseChallenge:
	return ( error );
}

/*****************************************************************************/

/*
 * MakeAuthHeaderBasic adds the Basic credentials from the WebdavAuthcacheElement
 * parameter to the retrieveRec->authorization string (creating the string if
 * needed).
 */
static void MakeAuthHeaderBasic(WebdavAuthcacheRetrieveRec *retrieveRec,
	WebdavAuthcacheElement *elem)
{
	AuthDataBasic *authData;
	char *existingAuthorization;
	
	authData = (AuthDataBasic*)elem->authData;
	if ( retrieveRec->authorization == NULL )
	{
		/* this is the first authorization header we're adding */
		retrieveRec->authorization =
					malloc(strlen(authData->credentialsStr) + 1);
		require(retrieveRec->authorization != NULL, malloc_authorization);
		
		strcpy(retrieveRec->authorization, authData->credentialsStr);
	}
	else
	{
		/*
		 * Allocate a buffer big enough for existing authorization header
		 * string and the one we're adding. Then copy both strings into
				 * it.
		 */
		existingAuthorization = retrieveRec->authorization;
		retrieveRec->authorization = malloc(
			strlen(existingAuthorization) +
			strlen(authData->credentialsStr) + 1);
		require(retrieveRec->authorization != NULL, malloc_authorization);
		
		strcpy(retrieveRec->authorization, existingAuthorization);
		strcat(retrieveRec->authorization, authData->credentialsStr);
		free (existingAuthorization);
	}
	
	return;
	
malloc_authorization:
	syslog(LOG_ERR, "MakeAuthHeaderBasic: %s", strerror(errno));
	return;
}

/*****************************************************************************/

/*
 * EvaluateBasic handles evaluate requests for the Basic scheme.
 */
static int EvaluateBasic(WebdavAuthcacheEvaluateRec *evaluateRec,
	char *authParam)
{
	int error;
	WebdavAuthcacheElement *elem;
	
	/* can we handle this Basic challenge? If so, get the realmStr. */
	error = ParseAuthParmsBasic(authParam, &evaluateRec->realmStr);
	require_noerr_quiet(error, ParseAuthParmsBasic);
	
	/* we can never update with the Basic scheme */
	evaluateRec->updated = FALSE;
	
	/* check for a placeholder element */
	elem = NULL;	/* start with head of queue */
	while ( TRUE )
	{
		GetNextNextWebdavAuthcacheElement(&elem);
		if ( elem == NULL )
		{
			/* no more elements in list */
			break;
		}
		
		if ( (evaluateRec->uid == elem->uid) &&
			(evaluateRec->isProxy == elem->isProxy) &&
			(elem->realmStr == NULL) )
		{
			/* found a placeholder so no UI is needed */
			evaluateRec->uiNotNeeded = TRUE;
			break;
		}
	}
	
ParseAuthParmsBasic:
	return ( error );
}

/*****************************************************************************/

/*
 * InsertBasic handles insert request for the Basic scheme.
 */
static int InsertBasic(WebdavAuthcacheInsertRec *insertRec, char *authParam)
{
	int error;
	char *realmStr;
	WebdavAuthcacheElement *elem;
	int elemInCache;
	AuthDataBasic *authData;
	char *userPass;
	char *basicCredentials;
	
	/* can we handle this Basic challenge? If so, get the realmStr. */
	error = ParseAuthParmsBasic(authParam, &realmStr);
	require_noerr_quiet(error, ParseAuthParmsBasic);
	
	/* is there an authentication already in the cache? */
	elem = NULL;	/* start with head of queue */
	while ( TRUE )
	{
		GetNextNextWebdavAuthcacheElement(&elem);
		if ( elem == NULL )
		{
			/* no more elements in list */
			break;
		}
		
		if ( (insertRec->uid == elem->uid) &&
			 (insertRec->isProxy == elem->isProxy) )
		{
			if ( elem->realmStr == NULL )
			{
				/* found a placeholder element -- use it */
				break;
			}
			/* make sure we don't insert a duplicate element */
			require_action(strcmp(realmStr, elem->realmStr) != 0,
				DuplicateBasicAuthcacheElement, error = 0);
		}
	}
	
	if ( elem == NULL )
	{
		/* no placeholder */
		elemInCache = FALSE;
		
		/* it's not there -- allocate it */
		elem = calloc(sizeof(WebdavAuthcacheElement), 1);
		require_action(elem != NULL, calloc_elem, error = ENOMEM);
		
		/* add uid and isProxy */
		elem->uid = insertRec->uid;
		elem->isProxy = insertRec->isProxy;
		
		/*
		 * add copies of username and password to
		 * WebdavAuthcacheElement
		 */
		elem->username = malloc(strlen(insertRec->username) + 1);
		require_action(elem->username != NULL, malloc_elem_username,
			error = ENOMEM);
		
		strcpy(elem->username, insertRec->username);
		
		elem->password = malloc(strlen(insertRec->password) + 1);
		require_action(elem->password != NULL, malloc_elem_password,
			error = ENOMEM);
		
		strcpy(elem->password, insertRec->password);
		/* now, it's got everything a placeholder element would have */
	}
	else
	{
		/* found a placeholder */
		elemInCache = TRUE;
	}
	
	/* allocate authData for Basic */
	authData = AllocateAuthDataBasic();
	require_action_quiet(authData != NULL, AllocateAuthDataBasic, error = ENOMEM);
	
	/* convert userName and password to basic-credentials */
	userPass = malloc(strlen(elem->username) +
		strlen(elem->password) + 2);
	require_action(userPass != NULL, malloc_userPass, error = ENOMEM);
	
	strcpy(userPass, elem->username);
	strcat(userPass, ":");
	strcat(userPass, elem->password);
	basicCredentials = to_base64(userPass, strlen(userPass));
	require_action(basicCredentials != NULL, to_base64, error = ENOMEM);
	
	/* create the credentials */
	authData->credentialsStr = 
		malloc((insertRec->isProxy ?
		sizeof("Proxy-Authorization: Basic \r\n") :
		sizeof("Authorization: Basic \r\n"))
		+ strlen(basicCredentials));
	require_action(authData->credentialsStr != NULL, malloc_credentialsStr,
		error = ENOMEM);
	
	strcpy(authData->credentialsStr, (insertRec->isProxy ?
		"Proxy-Authorization: Basic " : "Authorization: Basic "));
	strcat(authData->credentialsStr, basicCredentials);
	strcat(authData->credentialsStr, "\r\n");
	
	/* initialize most other fields */
	elem->realmStr = realmStr;
	elem->scheme = kChallengeSecurityLevelBasic;
	elem->uriCount = 0;
	elem->domainHead = NULL;
	elem->makeProcPtr = MakeAuthHeaderBasic;
	elem->freeProcPtr = FreeAuthDataBasic;
	elem->authData = authData;
	elem->authflags = ((insertRec->uid != 0) && (insertRec->uid != 1)) ?
		insertRec->authflags : kAuthNone;
	
	/* add elem to the cache if it isn't already there */
	if ( !elemInCache )
	{
		EnqueueWebdavAuthcacheElement(elem);
	}
	
	/* free up the temporary variables */
	free(userPass);
	free(basicCredentials);
	
	return ( 0 );
	
	/**********************/
	
	/* Error cleanup */
	
malloc_credentialsStr:
	free(basicCredentials);
to_base64:	
	free(userPass);
malloc_userPass:
	FreeAuthDataBasic(authData);
AllocateAuthDataBasic:
	if ( !elemInCache )
	{
		free(elem->password);
	}
malloc_elem_password:
	if ( !elemInCache )
	{
		free(elem->username);
	}
malloc_elem_username:
	if ( !elemInCache )
	{
		free(elem);
	}
calloc_elem:
	syslog(LOG_ERR, "InsertBasic: %s", strerror(error));
DuplicateBasicAuthcacheElement:
	free(realmStr);
ParseAuthParmsBasic:
	return ( error );
}

/*****************************************************************************/
/*****************************************************************************/

/*
 * GetNextChallenge parses a challenge (if any) from the params
 * string. The result is a pointer to the next challenge (if any) or the end
 * of the params string. If level is returned non-zero, then level indicates
 * the authentication scheme for the parsed challenge and authParam contains a
 * pointer to the authParam string for the challenge. If level is returned zero,
 * then the authParam parameter will be returned NULL. The caller is responsible
 * for freeing the authParam string if it is returned.
 *
 * This code assumes that the input, params, points to a the scheme name in a
 * challenge. It returns a pointer to the scheme name of the next challenge, or
 * a pointer to the end of the string.
 */
static char * GetNextChallenge(char *params,
	ChallengeSecurityLevelType *level, char **authParam, int *error)
{
	char *directive;
	char *value;
	char *authParamStart;
	char *previousParams;
	int schemeLength;
	
	*level = 0;
	*authParam = NULL;
		
	/* get auth-scheme */
	params = ParseChallenge(params, &directive, &value, error);
	require_noerr_quiet(*error, ParseChallenge);
	
	/* make sure we got an auth-scheme and not an auth-param */
	require_action(value == NULL, noSchemeName, free(directive); *error = EINVAL);
	
	/* determine the scheme */
	schemeLength = strlen(directive);
	if ( (Constant_strlen("Basic") == schemeLength) &&
	(strncasecmp(directive, "Basic", (size_t)schemeLength) == 0) )
	{
		/* use the "Basic" authentication scheme */
		*level = kChallengeSecurityLevelBasic;
	}
	else if ( (Constant_strlen("Digest") == schemeLength) &&
	(strncasecmp(directive, "Digest", (size_t)schemeLength) == 0) )
	{
		/* use the "Digest" authentication scheme */
		*level = kChallengeSecurityLevelDigest;
	}
	
	/* done with auth-scheme string */
	free(directive);
	
	/* authParamStart = start of 1#auth-param */
	authParamStart = params;
	
	/* parse until end of string or until we find another auth-scheme */
	while ( *params != '\0' )
	{
		/* save before parsing next chunk */
		previousParams = params;
		
		/* get next directive and value */
		params = ParseChallenge(params, &directive, &value, error);
		require_noerr_quiet(*error, ParseChallenge);
		
		/* is it an auth-scheme or an auth-param? */
		if ( value != NULL )
		{
			/* auth-param -- free both strings and continue */
			free(value);
			free(directive);
		}
		else
		{
			/*
			 * auth-scheme -- free the directive string,
			 * back up to previous params, and break
			 */
			free(directive);
			params = previousParams;
			break;
		}
	}
	
	if ( *level != 0 )
	{
		/* allocate space for the auth-param string */
		*authParam = malloc((size_t)(params - authParamStart + 1));
		require_action(*authParam != NULL, malloc_authParam, *error = ENOMEM);
	
		/* copy the token to auth-param string and terminate it */
		strncpy(*authParam, authParamStart, (size_t)(params - authParamStart));
		(*authParam)[params - authParamStart] = '\x00';
	}
	
	return ( params );
	
malloc_authParam:
noSchemeName:
	syslog(LOG_ERR, "GetNextChallenge: %s", strerror(*error));
ParseChallenge:
	return ( params );
}

/*****************************************************************************/

/*
 * GetNextNextWebdavAuthcacheElement is used to iterate through the
 * WebdavAuthcacheElements in the authcache. Passed NULL, it returns the
 * first element. Passed a previous element, it returns the next element.
 */
static void GetNextNextWebdavAuthcacheElement(WebdavAuthcacheElement **elem)
{
	/* new search? */
	if ( *elem == NULL )
	{
		/* then return first element */
		*elem = gAuthcacheHeader.head;
	}
	else
	{
		/* else return next element */
		*elem = (*elem)->next;
	}
}

/*****************************************************************************/

/*
 * EnqueueWebdavAuthcacheElement adds a WebdavAuthcacheElement to the
 * authcache.
 */
static void EnqueueWebdavAuthcacheElement(WebdavAuthcacheElement *elem)
{
	elem->next = gAuthcacheHeader.head;
	gAuthcacheHeader.head = elem;
	++gAuthcacheHeader.count;
}

/*****************************************************************************/

/*
 * DequeueWebdavAuthcacheElement finds a WebdavAuthcacheElement that matches
 * the uid, isProxy, and realmStr parameters, and then removes it from the
 * authcache. The matching element is returned.
 */
static WebdavAuthcacheElement * DequeueWebdavAuthcacheElement(uid_t uid,
	int isProxy, char *realmStr)
{
	WebdavAuthcacheElement *elem;
	WebdavAuthcacheElement *prevElem;
	
	/* find the element */
	prevElem = NULL;
	elem = gAuthcacheHeader.head;
	while ( elem != NULL )
	{
		/* is this a match? */
		if ( (uid == elem->uid) &&
			 (isProxy == elem->isProxy) )
		{
			if ( elem->realmStr != NULL )
			{
				if ( strcmp(realmStr, elem->realmStr) == 0 )
				{
					/* found it */
					break;
				}
			}
		}
		prevElem = elem;
		elem = elem->next;
	}
	require(elem != NULL, elementNotFound);
	
	/* and remove it from the linked list */
	if ( prevElem == NULL )
	{
		/* it was the head of the list */
		gAuthcacheHeader.head = elem->next;
	}
	else
	{
		/* not the head of the list */
		prevElem->next = elem->next;
	}
	--gAuthcacheHeader.count;

elementNotFound:	
	return ( elem );
}

/*****************************************************************************/

/*
 * FreeWebdavAuthcacheElement frees the memory associated with a
 * WebdavAuthcacheElement record.
 */
static void FreeWebdavAuthcacheElement(WebdavAuthcacheElement *elem)
{
	URIRec	*domain;
	URIRec	*nextDomain;
	
	/* free the realmStr */
	if ( elem->realmStr != NULL )
	{
		free(elem->realmStr);
	}
	
	/* free the username */
	if ( elem->username != NULL )
	{
		free(elem->username);
	}
	
	/* free the password */
	if ( elem->password != NULL )
	{
		free(elem->password);
	}
	
	/* free the domain list's URIs and URIRecs */
	domain = elem->domainHead;
	elem->domainHead = NULL;
	while ( domain != NULL )
	{
		nextDomain = domain->next;
		FreeURIRec(domain);
		domain = nextDomain;
	}
		
	/* free the element's authData */
	if ( elem->authData != NULL )
	{
		CallFreeAuthDataProc(elem->freeProcPtr, elem->authData);
	}
	
	/* and free the element */
	free(elem);
}

/*****************************************************************************/

/*
 * InsertPlaceholder inserts a placeholder WebdavAuthcacheInsertRec into
 * the authcache. A placeholder element has only the uid, iProxy, username,
 * and password fields initialized, but since the authentication scheme isn't
 * yet known, the other fields are left 0 or NULL. A placeholder in the
 * authcache is identified a NULL realmStr field.
 */
static int InsertPlaceholder(WebdavAuthcacheInsertRec *insertRec)
{
	int error;
	WebdavAuthcacheElement *elem;
	
	/*
	 * check for a placeholder element
	 * if one is already there, we have a problem.
	 */
	elem = NULL;	/* start with head of queue */
	while ( TRUE )
	{
		GetNextNextWebdavAuthcacheElement(&elem);
		if ( elem == NULL )
		{
			/* no more elements in list */
			break;
		}
		
		if ( (insertRec->uid == elem->uid) &&
			(insertRec->isProxy == elem->isProxy) )
		{
			require_action(elem->realmStr != NULL,
				DuplicateBasicAuthcacheElement, error = EINVAL);
		}
	}
	
	/* good, it's not there -- add it */
	elem = calloc(sizeof(WebdavAuthcacheElement), 1);
	require_action(elem != NULL, calloc_elem, error = ENOMEM);
	
	/* add copies of username and password to WebdavAuthcacheElement */
	elem->username = malloc(strlen(insertRec->username) + 1);
	require_action(elem->username != NULL, malloc_elem_username,
		error = ENOMEM);
	
	strcpy(elem->username, insertRec->username);
	
	elem->password = malloc(strlen(insertRec->password) + 1);
	require_action(elem->password != NULL, malloc_elem_password,
		error = ENOMEM);
	
	strcpy(elem->password, insertRec->password);
	
	/* initialize the rest of elem and insert it into the cache */
	elem->uid = insertRec->uid;
	elem->isProxy = insertRec->isProxy;
	elem->realmStr = NULL;
	elem->scheme = 0xffffffff; /* invalid */
	elem->uriCount = 0;
	elem->domainHead = NULL;
	elem->makeProcPtr = NULL;
	elem->freeProcPtr = NULL;
	elem->authData = NULL;
	elem->authflags = kAuthFromPlaceholder;
	EnqueueWebdavAuthcacheElement(elem);
	
	return ( 0 );
	
	/**********************/
	
	/* Error cleanup */
	
malloc_elem_password:
	free(elem->username);
malloc_elem_username:
	free(elem);
calloc_elem:
DuplicateBasicAuthcacheElement:
	syslog(LOG_ERR, "InsertPlaceholder: %s", strerror(error));
	return ( error );
}

/*****************************************************************************/

/*
 * HasMatchingURI returns TRUE if the uri parameter is in the protection space
 * of an URI in the WebdavAuthcacheElement's domain list.
 */
static int HasMatchingURI(char *uri, WebdavAuthcacheElement *elem)
{
	int result;
	int error;
	URIRec inputURIRec;
	URIRec *domain;
		
	/* put URI in easy to compare format */
	error = AddURIToURIRec(uri, &inputURIRec);
	require_noerr_quiet(error, AddURIToURIRec);
	
	result = FALSE;
	domain = elem->domainHead;
	while ( domain != NULL )
	{
		/*
		 * quick check of lengths:
		 * the abs_path must be the same length or longer and
		 * the server must be the same.
		 */
		if ( (inputURIRec.absPathLen >= domain->absPathLen) &&
			(inputURIRec.serverLen == domain->serverLen) )
		{
			if ( (strncasecmp(inputURIRec.server, domain->server,
				inputURIRec.serverLen) == 0) &&
				(strncmp(inputURIRec.absPath, domain->absPath,
				domain->absPathLen) == 0) )
			{
				result = TRUE;
				break;
			}
		}
		domain = domain->next;
	}
	
	free(inputURIRec.server);
	free(inputURIRec.absPath);
	
	return ( result );
	
AddURIToURIRec:
	return ( FALSE );
}

/*****************************************************************************/

static OSStatus KeychainItemCopyAccountPassword(SecKeychainItemRef itemRef, char *user, char *pass)
 {
	OSStatus					result;
	SecKeychainAttribute		attr;
	SecKeychainAttributeList	attrList;
	UInt32						length; 
	void						*outData;
	
	/* the attribute we want is the account name */
	attr.tag = kSecAccountItemAttr;
	attr.length = 0;
	attr.data = NULL;
	
	attrList.count = 1;
	attrList.attr = &attr;
	
	result = SecKeychainItemCopyContent(itemRef, NULL, &attrList, &length, &outData);
	if ( result == noErr )
	{
		/* attr.data is the account (username) and outdata is the password */
		
		/* make sure they'll fit in our buffers */
		if ( (attr.length < WEBDAV_MAX_USERNAME_LEN) && (length < WEBDAV_MAX_PASSWORD_LEN) )
		{
			/* copy the username and password */
			memcpy(user, attr.data, attr.length);
			memcpy(pass, outData, length);
		}
		else
		{
			result = errSecDataTooLarge; /* as good as anything */
		}
		(void) SecKeychainItemFreeContent(&attrList, outData);
	}
	return ( result );
 }

/*****************************************************************************/
/* External functions */
/*****************************************************************************/

/*
 * webdav_authcache_init initializes gAuthcacheHeader. It must be called before
 * any other webdav_authcache function.
 *	
 * Result
 *	0		The authentication cache was initialized.
 *	nonzero The authentication cache could not be initialized.
 */
int webdav_authcache_init(void)
{
	int error;
	pthread_mutexattr_t mutexattr;
	time_t	timeStamp;
	pid_t	privateKey;
	char	buf[18]; /* 8 + 8 + 1 + string terminator */
   
	/* if already initialized, no error - just a warning */
	require_action(gAuthcacheInitialized == 0, AlreadyInitialized, error = 0);
	
	/* initialize the WebdavAuthcacheElements list */
	gAuthcacheHeader.count = 0;
	gAuthcacheHeader.head = NULL;
	gAuthcacheHeader.proxyElementCount = 0;
	gAuthcacheHeader.cnonce = NULL;
	gAuthcacheHeader.generation = 1;
	
	/* set up the lock on the list */
	error = pthread_mutexattr_init(&mutexattr);
	require_noerr(error, pthread_mutexattr_init);
	
	error = pthread_mutex_init(&(gAuthcacheHeader.lock), &mutexattr);
	require_noerr(error, pthread_mutex_init);
	
	/*
	 * create a cnonce string (rfc 2617, sections 3.2.1 and 3.2.2)
	 */
	timeStamp = time(NULL); /* get the time-stamp */
	privateKey = getpid();	/* get the private-key */
	/* format as time-stamp:privateKey */
	snprintf(buf, sizeof(buf), "%.8lx:%.8lx", (long unsigned int)timeStamp,
		(long unsigned int)privateKey);
	/* convert to base64 */
	gAuthcacheHeader.cnonce = to_base64(buf, strlen(buf));
	
	/* set the initialized flag */
	gAuthcacheInitialized = 1;

pthread_mutex_init:
pthread_mutexattr_init:
AlreadyInitialized:

	if ( error != 0 )
	{
		syslog(LOG_ERR, "webdav_authcache_init: %s", strerror(error));
	}
	
	return (error);
}

/*****************************************************************************/

/*
 * webdav_authcache_evaluate evaluates a challenge (server or proxy) to
 * determine if it is supported. If the challenge is supported, the following
 * are returned:
 *	* the security level of the challenge's authentication scheme.
 *	* a boolean, updated, which indicates if an existing element in the
 *	  authcache was updated using the challenge.
 *	* a boolean, uiNotNeeded, which indicates a placeholder element was
 *	  found that contains the uid, iProxy, username, and password fields
 *	  for the challenge.
 *	* a realm string to display to the user when asking for the username
 *	  and password.
 *	
 * Result
 *	0		The challenge is supported.
 *	nonzero The challenge is unsupported.
 */
int webdav_authcache_evaluate(WebdavAuthcacheEvaluateRec *evaluateRec)
{
	int error, error2;
	char *authParam;
	char *params;
	ChallengeSecurityLevelType level;
	
	/* lock the Authcache */
	error = pthread_mutex_lock(&(gAuthcacheHeader.lock));
	if ( error )
	{
		syslog(LOG_ERR, "webdav_authcache_evaluate: pthread_mutex_lock(): %s", strerror(error));
		webdav_kill(-1);	/* tell the main select loop to force unmount */
	}
	require_noerr(error, pthread_mutex_lock);
	
	/* default return values */
	evaluateRec->level = 0;
	evaluateRec->updated = FALSE;
	evaluateRec->uiNotNeeded = FALSE;
	evaluateRec->realmStr = NULL;
	
	params = evaluateRec->challenge;
	
	/* parse the challenge(s) in the challenge string */
	while ( *params != '\0' )
	{
		params = GetNextChallenge(params, &level, &authParam, &error);
		require_noerr(error, GetNextChallenge);
		
		if ( level != 0 )
		{
			if ( level == kChallengeSecurityLevelBasic )
			{
				/* use the "Basic" authentication scheme */
				error = EvaluateBasic(evaluateRec, authParam);
			}
			else if ( level == kChallengeSecurityLevelDigest )
			{
				/* use the "Digest" authentication scheme */
				error = EvaluateDigest(evaluateRec, authParam);
			}
			
			/* are we accepting this challenge? */
			if ( error == 0 )
			{
				/* is it better than any other we've accepted? */
				if ( level > evaluateRec->level )
				{
					evaluateRec->level = level;
				}
			}
			/* we need to free authParam if the level != 0 */
			free(authParam);
		}
	}
	
 GetNextChallenge:
   /* was any challenge accepted? */
	if ( evaluateRec->level == 0 )
	{
		/* nope -- no supported schemes */
		debug_string("unsupported scheme");
		error = EACCES;
	}
	else
	{
		error = 0;
	}
	
	/* unlock the Authcache */
	error2 = pthread_mutex_unlock(&(gAuthcacheHeader.lock));
	if ( error2 != 0 )
	{
		syslog(LOG_ERR, "webdav_authcache_evaluate: pthread_mutex_unlock(): %s", strerror(error2));
		webdav_kill(-1);	/* tell the main select loop to force unmount */
		error = error2;
	}

pthread_mutex_lock:
	if ( error != 0 )
	{
		syslog(LOG_ERR, "webdav_authcache_evaluate: %s", strerror(error));
	}
	return ( error );
}

/*****************************************************************************/

/*
 * webdav_authcache_insert attempts to add an authentication for the given
 * challenge (server or proxy), username, and password to the authentication
 * cache. If the challenge input is NULL, then a placeholder element containing
 * the uid, iProxy, username, and password fields is added to the authcache.
 *	
 * Result
 *	0		The authentication was added to the authentication cache.
 *	nonzero The authentication could not be added to the authentication
 *			cache.
 */
int webdav_authcache_insert(WebdavAuthcacheInsertRec *insertRec, int getLock)
{
	int error, error2;
	char *authParam;
	char *params;
	ChallengeSecurityLevelType level;
	
	/* lock the cache if needed */
	if ( getLock )
	{
		error = pthread_mutex_lock(&(gAuthcacheHeader.lock));
		if ( error )
		{
			syslog(LOG_ERR, "webdav_authcache_insert: pthread_mutex_lock(): %s", strerror(error));
			webdav_kill(-1);	/* tell the main select loop to force unmount */
		}
		require_noerr(error, pthread_mutex_lock);
	}
	
	if ( insertRec->challenge != NULL )
	{
		params = insertRec->challenge;
		
		/* parse the challenge(s) in the challenge string */
		while ( *params != '\0' )
		{
			params = GetNextChallenge(params, &level, &authParam, &error);
			require_noerr(error, GetNextChallenge);
			
			/* is this the challenge to insert? */
			if ( level == insertRec->level )
			{
				if ( level == kChallengeSecurityLevelBasic )
				{
					/* use the "Basic" authentication scheme */
					error = InsertBasic(insertRec, authParam);
				}
				else if ( level == kChallengeSecurityLevelDigest )
				{
					/* use the "Digest" authentication scheme */
					error = InsertDigest(insertRec, authParam);
				}
				free(authParam);
				break;
			}
			else
			{
				/* not this one - free authParam and loop */
				free(authParam);
			}
		}
		
GetNextChallenge:
		;
	}
	else
	{
		/*
		 * There is no challenge yet, but we need to create a placeholder
		 * entry to store the username and password we're being passed.
		 */
		error = InsertPlaceholder(insertRec);
	}
		
	/* increment proxyElementCount if a proxy element was just inserted */
	if ( error == 0 )
	{
		++gAuthcacheHeader.generation;
		if ( gAuthcacheHeader.generation == 0 )
		{
			gAuthcacheHeader.generation = 1;
		}
		if ( insertRec->isProxy )
		{
			verify((++gAuthcacheHeader.proxyElementCount) > 0);
		}
	}

	if ( getLock )
	{
		error2 = pthread_mutex_unlock(&(gAuthcacheHeader.lock));
		if ( error2 != 0 )
		{
			syslog(LOG_ERR, "webdav_authcache_insert: pthread_mutex_unlock(): %s", strerror(error2));
			webdav_kill(-1);	/* tell the main select loop to force unmount */
			error = error2;
		}
	}
	
pthread_mutex_lock:
	
	if ( error != 0 )
	{
		syslog(LOG_ERR, "webdav_authcache_insert failed: %s", strerror(error));
	}
	
	return ( error );
}

/*****************************************************************************/

/*
 * webdav_authcache_retrieve attempts to create the Authorization Request
 * credentials string using the data passed in retrieveRec and cached
 * authentication data found in the authentication cache. Both server and
 * proxy (if any) credentials are returned in the authorization string.
 *	
 * Result
 *	0		The credentials string was created.
 *	nonzero The credentials string could not be created.
 */
int webdav_authcache_retrieve(WebdavAuthcacheRetrieveRec *retrieveRec)
{
	int error, error2;
	WebdavAuthcacheElement *elem;
	int foundServer, foundProxy;

	/* lock the cache */
	error = pthread_mutex_lock(&(gAuthcacheHeader.lock));
	if ( error )
	{
		syslog(LOG_ERR, "webdav_authcache_retrieve: pthread_mutex_lock(): %s", strerror(error));
		webdav_kill(-1);	/* tell the main select loop to force unmount */
	}
	require_noerr(error, pthread_mutex_lock);
	
	/* return the cache generation count */
	retrieveRec->generation = gAuthcacheHeader.generation;
	
	/* find the cache element */
	foundServer = FALSE; /* haven't found server */
	/* look for proxy only if a proxy element has been inserted */
	foundProxy = (gAuthcacheHeader.proxyElementCount == 0);
	retrieveRec->authorization = NULL; /* no string yet */
	elem = NULL;
	/* done if we've found both a server and proxy element (if any) */
	while ( (foundServer == FALSE) || (foundProxy == FALSE) )
	{
		GetNextNextWebdavAuthcacheElement(&elem);
		if ( elem == NULL )
		{
			/* done - no more auth cache elements */
			break;
		}
		
				/* match on uid but skip placeholders */
		if ( (retrieveRec->uid == elem->uid) && (elem->realmStr != NULL) )
		{
			/*
			 * If the element has no uri, then the protection space is
			 * the entire realm. Otherwise, we have to see if
			 * retrieveRec->uri is covered by elem's domain.  
			 */
			if ( (elem->domainHead == NULL) ||
				HasMatchingURI(retrieveRec->uri, elem) )
			{
				if ( elem->isProxy )
				{
					/* should be FALSE */
					check(foundProxy == FALSE);
					foundProxy = TRUE;
				}
				else
				{
					/* should be FALSE */
					check(foundServer == FALSE); 
					foundServer = TRUE;
				}
				
				/*
				 * call the appropriate routine to make the
				 * auth header
				 */
				/* NULL would be bad */
				check(elem->makeProcPtr != NULL);
				CallMakeAuthHeaderProc(elem->makeProcPtr,
					retrieveRec, elem);
				require_action(retrieveRec->authorization != NULL,
					CallMakeAuthHeaderProc, error = EACCES);
				retrieveRec->authflags = elem->authflags;
			}
		}
	}

CallMakeAuthHeaderProc:
	error2 = pthread_mutex_unlock(&(gAuthcacheHeader.lock));
	if ( error2 != 0 )
	{
		syslog(LOG_ERR, "webdav_authcache_retrieve: pthread_mutex_unlock(): %s", strerror(error2));
		webdav_kill(-1);	/* tell the main select loop to force unmount */
		error = error2;
	}
	
pthread_mutex_lock: 
	if ( error != 0 )
	{
		syslog(LOG_ERR, "webdav_authcache_retrieve: %s", strerror(error));
	}
	return ( error );
}

/*****************************************************************************/

/*
 * webdav_authcache_remove attempts to remove a matching authentication from
 * the authentication cache.
 *	
 * Result
 *	0		The authentication was removed from the authentication cache.
 *	nonzero The authentication could not be removed from the authentication
 *			cache.
 */
int webdav_authcache_remove(WebdavAuthcacheRemoveRec *removeRec, int getLock)
{
	int error, error2;
	WebdavAuthcacheElement *elem;

	error = 0;
	
	/* lock the cache if needed */
	if ( getLock )
	{
		error = pthread_mutex_lock(&(gAuthcacheHeader.lock));
		if ( error )
		{
			syslog(LOG_ERR, "webdav_authcache_remove: pthread_mutex_lock(): %s", strerror(error));
			webdav_kill(-1);	/* tell the main select loop to force unmount */
		}
		require_noerr(error, pthread_mutex_lock);
	}
	
	/* find and delink the element from the cache */
	elem = DequeueWebdavAuthcacheElement(removeRec->uid, removeRec->isProxy,
		removeRec->realmStr);
	require_action_quiet(elem != NULL, DequeueWebdavAuthcacheElement,
			error = ENOENT);
	
	/* free the element and anything it points to */
	FreeWebdavAuthcacheElement(elem);
		
	/*	If we removed a proxy element, decrement proxyElementCount */
	if ( removeRec->isProxy )
	{
		/* decrement proxyElementCount */
		verify((--gAuthcacheHeader.proxyElementCount) >= 0);
	}

DequeueWebdavAuthcacheElement:

	if ( error == 0 )
	{
		++gAuthcacheHeader.generation;
		if ( gAuthcacheHeader.generation == 0 )
		{
			gAuthcacheHeader.generation = 1;
		}
	}
	
	if ( getLock )
	{
		error2 = pthread_mutex_unlock(&(gAuthcacheHeader.lock));
		if ( error2 != 0 )
		{
			syslog(LOG_ERR, "webdav_authcache_remove: pthread_mutex_unlock(): %s", strerror(error2));
			webdav_kill(-1);	/* tell the main select loop to force unmount */
			error = error2;
		}
	}
	
pthread_mutex_lock: 
	return ( error );
}

/*****************************************************************************/

int webdav_authcache_update(WebdavAuthcacheUpdateRec *updateRec)
{
	int error, error2;
	char user[WEBDAV_MAX_USERNAME_LEN];
	char pass[WEBDAV_MAX_PASSWORD_LEN];
	int addtokeychain;
	AuthFlagsType newAuthflags;
		
	error = pthread_mutex_lock(&(gAuthcacheHeader.lock));
	if ( error )
	{
		syslog(LOG_ERR, "webdav_authcache_update: pthread_mutex_lock(): %s", strerror(error));
		webdav_kill(-1);	/* tell the main select loop to force unmount */
	}
	require_noerr(error, pthread_mutex_lock);
	
	/* If the cache has already changed, don't prompt -- just try the new credentials */
	require_quiet((updateRec->generation == gAuthcacheHeader.generation) ||
				(updateRec->generation == 0), updateComplete);
	
	/* make sure we are updating with at least kChallengeSecurityLevelBasic */
	require_action(updateRec->level != 0, auth_level_zero, error = EACCES);
	
	newAuthflags = kAuthNone;
	
	bzero(user, WEBDAV_MAX_USERNAME_LEN);
	bzero(pass, WEBDAV_MAX_PASSWORD_LEN);
	if (!updateRec->uiNotNeeded)
	{
		char *allocatedURL = NULL;
		char *displayURL;
		int promptUser;
		WebdavAuthcacheElement *elem;
		
		/*
		 * If updateRec->authflags shows last auth wasn't from keychain,
		 * try and get it from there. Then, if we got it from the keychain, set
		 * newAuthflags to kAuthFromKeychain.
		 * 
		 * Otherwise, prompt user and if user wants it added to the keychain,
		 * set newAuthflags to kAuthAddToKeychain.
		*/
		promptUser = TRUE;
		if ( (updateRec->authflags & kAuthFromKeychain) == 0 )
		{
			OSStatus result;
			char *serverName;
			char *path;
			SecKeychainItemRef itemRef;
			
			serverName = (updateRec->isProxy ? proxy_server : dest_server);
			path = (updateRec->isProxy ? NULL : dest_path);
			
			/* is there an existing keychain item? */
			if ( updateRec->isProxy )
			{
				result = SecKeychainFindInternetPassword(NULL,
					strlen(serverName), serverName,				/* serverName */
					0, NULL,									/* securityDomain */
					0, NULL,									/* no accountName */
					0, NULL,									/* path */
					proxy_port,									/* port */
					kSecProtocolTypeHTTPProxy,					/* protocol */
					0,											/* authType */
					0, NULL,									/* no password */
					&itemRef);
			}
			else
			{
				result = SecKeychainFindInternetPassword(NULL,
					strlen(serverName), serverName,				/* serverName */
					strlen(updateRec->realmStr), updateRec->realmStr, /* securityDomain */
					0, NULL,									/* no accountName */
					(path == NULL) ? 0 : strlen(path), path,	/* path */
					dest_port,									/* port */
					kSecProtocolTypeHTTP,						/* protocol */
					(updateRec->level == kChallengeSecurityLevelDigest) ?
						kSecAuthenticationTypeHTTPDigest :
						kSecAuthenticationTypeDefault,			/* authType */
					0, NULL,									/* no password */
					&itemRef);
			}
			
			if ( result == noErr )
			{
				if ( KeychainItemCopyAccountPassword(itemRef, user, pass) == noErr )
				{
					promptUser = FALSE;
					/* indicate where the authentication came from. It isn't provisional because it was good at one time. */
					newAuthflags = kAuthFromKeychain;
				}
				CFRelease(itemRef);
			}
		}
		
		if ( promptUser )
		{
			if ( !updateRec->isProxy )
			{
				/* 401 Unauthorized error */
				if ( reconstruct_url(http_hostname, updateRec->uri, &allocatedURL) == 0 )
				{
					/* use allocatedURL for displayURL */
					displayURL = allocatedURL;
				}
				else
				{
					/* this shouldn't happen, but if it does, use http_remote_request */
					allocatedURL = NULL;
					displayURL = updateRec->uri;
				}
			}
			else
			{
				/* must be 407 Proxy Authentication Required error */
				/* use proxy_server for displayURL */
				displayURL = proxy_server;
			}
			
			/* If there is a cache element for this uid/server/realm, get the username from
			 * it and pass it to webdav_get_authentication(). If not, then pass current username
			 * from system.
			 */

			/* find the element */
			elem = gAuthcacheHeader.head;
			while ( elem != NULL )
			{
				/* is this a match? */
				if ( (updateRec->uid == elem->uid) &&
					(updateRec->isProxy == elem->isProxy) )
				{
					if ( elem->realmStr != NULL )
					{
						if ( strcmp(updateRec->realmStr, elem->realmStr) == 0 )
						{
							/* found it */
							break;
						}
					}
				}
				elem = elem->next;
			}
			
			if ( elem != NULL )
			{
				strcpy(user, elem->username);
			}
			else
			{
				char *loginname;
				
				loginname = getlogin();
				if (loginname != NULL )
				{
					strcpy(user, loginname);
				}
			}

			if ( gSuppressAllUI )
			{
				/* we're suppressing UI so just return EACCES */
				error = EACCES;
			}
			else
			{
				/* put up prompt for updateRec->level */
				error = webdav_get_authentication(user, sizeof(user), pass, sizeof(pass),
					(const char *)displayURL, (const char *)updateRec->realmStr, updateRec->level,
					&addtokeychain, ((elem != NULL) ? ((elem->authflags & kAuthProvisional) != 0) : FALSE) );
				
				if ( error == 0 )
				{
					/* indicate where the authentication came from and that it is provisional */
					newAuthflags = kAuthFromUI | kAuthProvisional;
				}
				else
				{
					/* for any error response, set error to EACCES */
					error = EACCES;
				}
			}
			
			if ( addtokeychain )
			{
				newAuthflags |= kAuthAddToKeychain;
			}
			
			/* free allocatedURL if allocated */
			if ( allocatedURL != NULL )
			{
				free(allocatedURL);
			}
		}
	}
	
	require_noerr(error, webdav_get_authentication);
	
	/* insert authcache entry */
	WebdavAuthcacheRemoveRec auth_rem =
	{
		updateRec->uid, updateRec->isProxy, updateRec->realmStr
	};
	WebdavAuthcacheInsertRec auth_insert =
	{
		updateRec->uid, updateRec->challenge, updateRec->level, updateRec->isProxy,
			user, pass, newAuthflags
	};

	/*
	 * Whatever authorization we had before is no longer valid
	 * so remove it from the cache. If getting rid of it
	 * doesn't work, ignore it.
	 */
	(void)webdav_authcache_remove(&auth_rem, FALSE);
	if (updateRec->uid != 0)
	{
		auth_rem.uid = 0;
		(void)webdav_authcache_remove(&auth_rem, FALSE);
	}
	if (updateRec->uid != 1)
	{
		auth_rem.uid = 1;
		(void)webdav_authcache_remove(&auth_rem, FALSE);
	}

	error = webdav_authcache_insert(&auth_insert, FALSE);
	if (!error)
	{
		/* if not "root", make an entry for root */
		if (updateRec->uid != 0)
		{
			auth_insert.uid = 0;
			(void)webdav_authcache_insert(&auth_insert, FALSE);
		}
		/* if not "daemon", make an entry for daemon */
		if (updateRec->uid != 1)
		{
			auth_insert.uid = 1;
			(void)webdav_authcache_insert(&auth_insert, FALSE);
		}
	}
	bzero(user, WEBDAV_MAX_USERNAME_LEN);
	bzero(pass, WEBDAV_MAX_PASSWORD_LEN);
	
webdav_get_authentication:
auth_level_zero:
updateComplete:
	
	error2 = pthread_mutex_unlock(&(gAuthcacheHeader.lock));
	if ( error2 != 0 )
	{
		syslog(LOG_ERR, "webdav_authcache_update: pthread_mutex_unlock(): %s", strerror(error2));
		webdav_kill(-1);	/* tell the main select loop to force unmount */
		error = error2;
	}
	
pthread_mutex_lock:

	return ( error );
}

/*****************************************************************************/

int webdav_authcache_keychain(WebdavAuthcacheKeychainRec *keychainRec)
{
	int error, error2;
	WebdavAuthcacheElement *elem;
	int foundServer, foundProxy;
	
	error = pthread_mutex_lock(&(gAuthcacheHeader.lock));
	if ( error )
	{
		syslog(LOG_ERR, "webdav_authcache_keychain: pthread_mutex_lock(): %s", strerror(error));
		webdav_kill(-1);	/* tell the main select loop to force unmount */
	}
	require_noerr(error, pthread_mutex_lock);
	
	/* If the generation has changed, do nothing.
	 * If addtokeychain is still set, the next request that works will call
	 * webdav_authcache_keychain() again with a different generation.
	 */
	require_quiet(keychainRec->generation == gAuthcacheHeader.generation, generationChanged);
	
	/* find the cache element(s) which might need to be added to the keychain */
	foundServer = FALSE; /* haven't found server */
	/* look for proxy only if a proxy element has been inserted */
	foundProxy = (gAuthcacheHeader.proxyElementCount == 0);
	elem = NULL;
	/* done if we've found both a server and proxy element (if any) */
	while ( (foundServer == FALSE) || (foundProxy == FALSE) )
	{
		GetNextNextWebdavAuthcacheElement(&elem);
		if ( elem == NULL )
		{
			/* done - no more auth cache elements */
			break;
		}
		
		/* match on uid but skip placeholders */
		if ( (keychainRec->uid == elem->uid) && (elem->realmStr != NULL) )
		{
			/*
			 * If the element has no uri, then the protection space is
			 * the entire realm. Otherwise, we have to see if
			 * retrieveRec->uri is covered by elem's domain.  
			 */
			if ( (elem->domainHead == NULL) ||
				HasMatchingURI(keychainRec->uri, elem) )
			{
				char *serverName;
				char *path;
				SecKeychainItemRef itemRef;
				OSStatus result;
				
				if ( elem->isProxy )
				{
					/* should be FALSE */
					check(foundProxy == FALSE);
					foundProxy = TRUE;
				}
				else
				{
					/* should be FALSE */
					check(foundServer == FALSE); 
					foundServer = TRUE;
				}
				
				/* mark it as a known good authentication */
				elem->authflags &= ~kAuthProvisional;
				
				if (elem->authflags & kAuthAddToKeychain)
				{
					serverName = (elem->isProxy ? proxy_server : dest_server);
					path = (elem->isProxy ? NULL : dest_path);
					
					/* is there an existing keychain item? */
					if ( elem->isProxy )
					{
						result = SecKeychainFindInternetPassword(NULL,
						strlen(serverName), serverName,				/* serverName */
						0, NULL,									/* securityDomain */
						0, NULL,									/* accountName */
						0, NULL,									/* path */
						proxy_port,									/* port */
						kSecProtocolTypeHTTPProxy,					/* protocol */
						0,											/* authType */
						NULL, NULL,									/* don't want password */
						&itemRef);
					}
					else
					{
						result = SecKeychainFindInternetPassword(NULL,
						strlen(serverName), serverName,				/* serverName */
						strlen(elem->realmStr), elem->realmStr,		/* securityDomain */
						strlen(elem->username), elem->username,		/* accountName */
						(path == NULL) ? 0 : strlen(path), path,	/* path */
						dest_port,									/* port */
						kSecProtocolTypeHTTP,						/* protocol */
						(elem->scheme == kChallengeSecurityLevelDigest) ?
							kSecAuthenticationTypeHTTPDigest :
							kSecAuthenticationTypeDefault,			/* authType */
						NULL, NULL,									/* don't want password */
						&itemRef);
					}
					
					if ( result == noErr )
					{
						/* update the password in it */
						result = SecKeychainItemModifyContent(itemRef, NULL, strlen(elem->password), (void *)elem->password);
						CFRelease(itemRef);
					}
					else
					{
						/* otherwise, add new InternetPassword */
						result = SecKeychainAddInternetPassword(NULL,
							strlen(serverName), serverName,			/* serverName */
							strlen(elem->realmStr), elem->realmStr, /* securityDomain */
							strlen(elem->username), elem->username,	/* accountName */
							(path == NULL) ? 0 : strlen(path), path, /* path */
							elem->isProxy ? proxy_port : dest_port,	/* port */
							elem->isProxy ? kSecProtocolTypeHTTPProxy : kSecProtocolTypeHTTP, /* protocol */
							(elem->scheme == kChallengeSecurityLevelDigest) ?
								kSecAuthenticationTypeHTTPDigest :
								kSecAuthenticationTypeDefault,		/* authType */
							strlen(elem->password), elem->password,	/* password */
							&itemRef);
						CFRelease(itemRef);
					}
					
					/* if it's now in the keychain, then future retrieves need to indicate that */
					if ( result == noErr )
					{
						/* indicate where the authentication came from */
						elem->authflags = kAuthFromKeychain;
					}
				}
			}
		}
	}
	
generationChanged:

	error2 = pthread_mutex_unlock(&(gAuthcacheHeader.lock));
	if ( error2 != 0 )
	{
		syslog(LOG_ERR, "webdav_authcache_keychain: pthread_mutex_unlock(): %s", strerror(error2));
		webdav_kill(-1);	/* tell the main select loop to force unmount */
		error = error2;
	}
	
pthread_mutex_lock:

	return ( error );
}

/*****************************************************************************/
