#ifndef _WEBDAV_AUTHCACHE_H_INCLUDE
#define _WEBDAV_AUTHCACHE_H_INCLUDE


/* webdav_authcache.h created by warner_c on Fri 10-Mar-2000 */

/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * "Portions Copyright (c) 2000 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').	You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 *
 * @APPLE_LICENSE_HEADER_END@
 */
/*		@(#)webdav_authchache.h		 *
 *		(c) 2000   Apple Computer, Inc.	 All Rights Reserved
 *
 *
 *		webdav_authcache.h -- Headers for WebDAV in memory authorization cache
 *
 *		MODIFICATION HISTORY:
 *				10-Mar-2000		Clark Warner	  File Creation
 */

/******************************************************************************/

/* definitions */

typedef unsigned long ChallengeSecurityLevelType;
enum
{
	/* Basic authentication is always the lowest security level */
	kChallengeSecurityLevelBasic = 1,
	/*
	 * add other authentication methods in the order of authentication
	 * security level (is this order subjective? yes it is)
	 */
	kChallengeSecurityLevelDigest
};

/******************************************************************************/

/* structures */

/*
 * The WebdavAuthcacheEvaluateRec struct is passed to WebdavAuthcacheEvaluate
 * to determine if a challenge can be accepted.
 *	
 * Field descriptions:
 *	uid			in:		The user ID
 *	challenge	in:		A C string containing the challenge
 *						(rfc 2617, section 1.2)
 *	isProxy		in:		true if proxy challenge;
 *						false if server challenge
 *	level		out:	The security level of the challenge
 *	updated		out:	true if an authentication already exists in the
 *						authentication cache for the specified realm and
 *						the challenge was used to update the
 *						authentication (i.e., a digest-challenge with
 *						"stale=TRUE"); false if this challenge should be
 *						inserted.
 *	uiNotNeeded out:	true if a placeholder authentication already
 *						exists in the cache, and no user and password
 *						are needed.
 *	realmStr	out:	A pointer to a C string containing the
 *						realm-value string for this authentication. This
 *						string, along with the user ID defines the
 *						protection space for this authentication. This
 *						string should be displayed when asking for the
 *						username and password. The caller is responsible
 *						for disposing of this string when it is no
 *						longer needed.
 *						(rfc 1617, section 1.2)
 *						(rfc 1617, section 3.2.1)
 */
struct WebdavAuthcacheEvaluateRec
{
	uid_t				uid;
	char				*challenge;
	int					isProxy;
	ChallengeSecurityLevelType	level;
	int					updated;
	int					uiNotNeeded;
	char				*realmStr;
};
typedef struct WebdavAuthcacheEvaluateRec WebdavAuthcacheEvaluateRec;

/*
 * The WebdavAuthcacheInsertRec struct is passed to WebdavAuthcacheInsert
 * to add a Authorization or Proxy-Authorization request to the
 * authentication cache.
 *
 * Field descriptions:
 *	uid			in: 	The user ID
 *	challenge	in:		A C string containing the challenge
 *						(rfc 2617, section 1.2)
 *	level		in:		The level of the challenge to insert
 *	isProxy		in:		true if proxy challenge;
 *						false if server challenge
 *	username	in:		A C string containing the username
 *	password	in:		A C string containing the password
 */
struct WebdavAuthcacheInsertRec
{
	uid_t	uid;
	char	*challenge;
	ChallengeSecurityLevelType level;
	int		isProxy;
	char	*username;
	char	*password;
};
typedef struct WebdavAuthcacheInsertRec WebdavAuthcacheInsertRec;

/*
 * The WebdavAuthcacheRetrieveRec struct is passed to WebdavAuthcacheRetrieve
 * to get the Authorization and/or Proxy-Authorization request header string.
 *
 * Field descriptions:
 *	uid			in:		The user ID
 *	uri			in:		The request URI
 *						(rfc 2616, section 5.1.2)
 *	query		in:		The request URI query string, or NULL
 *	method		in:		The method (i.e., GET, PUT, etc.)
 *						(rfc 2616, section 5.1.1)
 *	authorization out:	If not NULL, a pointer to a C string containing
 *						the Authorization and/or Proxy-Authorization
 *						string(s)
 *						(rfc 2617, section 3.2.2)
 */
struct WebdavAuthcacheRetrieveRec
{
	uid_t	uid;
	char	*uri;
	char	*query;
	char	*method;
	char	*authorization;
};
typedef struct WebdavAuthcacheRetrieveRec WebdavAuthcacheRetrieveRec;

/*
 * The WebdavAuthcacheRemoveRec struct is passed to WebdavAuthcacheRemove
 * to remove a authentication from the authentication cache when updated is
 * returned as true, but the retry failed. For example, a request is sent, the
 * response from the server includes a challenge, the challenge is passed to
 * webdav_authcache_evaluate which returns with updated = true, the request is
 * retried but fails, THEN, WebdavAuthcacheRemove should be called to remove
 * the authentication from the cache.
 *	
 * Field descriptions:
 *	uid			in:		The user ID
 *	isProxy		in:		true if proxy authentication;
 *						false if server authentication
 *	realmStr	in:		A pointer to a C string containing the
 *						realm-value string for this authentication. This
 *						string, along with the user ID defines the
 *						protection space for this authentication.
 *						(rfc 1617, section 3.2.1)
 */
struct WebdavAuthcacheRemoveRec
{
	uid_t	uid;
	int		isProxy;
	char	*realmStr;
};
typedef struct WebdavAuthcacheRemoveRec WebdavAuthcacheRemoveRec;

/******************************************************************************/

/* functions */

/*
 * webdav_authcache_init initializes gAuthcacheHeader. It must be called before
 * any other webdav_authcache function.
 *	
 * Result
 *	0	The authentication cache was initialized.
 *	nonzero The authentication cache could not be initialized.
 */
extern int webdav_authcache_init(void);

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
extern int webdav_authcache_evaluate(WebdavAuthcacheEvaluateRec *evaluateRec);

/*
 * webdav_authcache_insert attempts to add an authentication for the given
 * challenge (server or proxy), username, and password to the authentication
 * cache. If the challenge input is NULL, then a placeholder element containing
 * the uid, iProxy, username, and password fields is added to the authcache.
 *	
 * Result
 *	0		The authentication was added to the authentication cache.
 *	nonzero	The authentication could not be added to the authentication
 *			cache.
 */
extern int webdav_authcache_insert(WebdavAuthcacheInsertRec *insertRec);

/*
 * webdav_authcache_retrieve attempts to create the Authorization Request
 * credentials string using the data passed in retrieveRec and cached
 * authentication data found in the authentication cache. Both server and
 * proxy (if any) credentials are returned in the authorization string.
 *	
 * Result
 *	0		The credentials string was created.
 *	nonzero	The credentials string could not be created.
 */
extern int webdav_authcache_retrieve(WebdavAuthcacheRetrieveRec *retrieveRec);

/*
 * webdav_authcache_remove attempts to remove a matching authentication from
 * the authentication cache.
 *	
 * Result
 *	0		The authentication was removed from the authentication cache.
 *	nonzero	The authentication could not be removed from the authentication
 *			cache.
 */
extern int webdav_authcache_remove(WebdavAuthcacheRemoveRec *removeRec);

/*****************************************************************************/

#endif
