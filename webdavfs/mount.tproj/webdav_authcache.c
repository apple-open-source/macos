/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All rights reserved.
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

#include "webdavd.h"

#include <sys/types.h>
#include <pthread.h>
#include "webdav_authcache.h"
#include "webdav_network.h"

/*****************************************************************************/

/* define authcache_head structure */
LIST_HEAD(authcache_head, authcache_entry);

struct authcache_entry
{
	LIST_ENTRY(authcache_entry) entries;
	uid_t uid;
	CFHTTPAuthenticationRef auth;
	CFStringRef username;
	CFStringRef password;
	CFStringRef domain;			/* can be NULL if there is no account domain */
	u_int32_t authflags;		/* The options for this authorization */
};

/* authFlags */
enum
{
	/* No flags */
	kAuthNone					= 0x00000000,

	kCredentialsFromMount		= 0x00000001,	/* Credentials passed at mount time */
	
	/* Set if mount credentials should not be used (they were tried and didn't work) */
	kNoMountCredentials			= 0x00000002,
	
	/* Set once the credentials are successfully used for a transaction */
	kCredentialsValid			= 0x00000004	
};

/*****************************************************************************/

static pthread_mutex_t authcache_lock;					/* lock for authcache */
static u_int32_t authcache_generation = 1;				/* generation count of authcache (never zero)*/
static struct authcache_head authcache_list;			/* the list of authcache_entry structs for the server */
static struct authcache_entry *authcache_proxy_entry = NULL;	/* the authcache_entry for the proxy server, or NULL */
static CFStringRef mount_username = NULL;
static CFStringRef mount_password = NULL;
static CFStringRef mount_proxy_username = NULL;
static CFStringRef mount_proxy_password = NULL;
static CFStringRef mount_domain = NULL;

/*****************************************************************************/

// static
char *CopyCFStringToCString(CFStringRef theString);

static
void ReleaseCredentials(struct authcache_entry *entry_ptr);

/*****************************************************************************/

static
void RemoveAuthentication(struct authcache_entry *entry_ptr)
{
	/* authcache_proxy_entry is never in the authcache_list */
	if ( entry_ptr != authcache_proxy_entry )
	{
		LIST_REMOVE(entry_ptr, entries);
	}
	else
	{
		authcache_proxy_entry = NULL;
	}
	++authcache_generation;
	if ( authcache_generation == 0 )
	{
		++authcache_generation;
	}
	
	if ( entry_ptr->auth != NULL )
	{
		CFRelease(entry_ptr->auth);
		entry_ptr->auth = NULL;
	}
	
	ReleaseCredentials(entry_ptr);
	
	free(entry_ptr);
}

/*****************************************************************************/

static
void ReleaseCredentials(
	struct authcache_entry *entry_ptr)
{
	if (entry_ptr->username != NULL)
	{
		CFRelease(entry_ptr->username);
		entry_ptr->username = NULL;
	}
	if (entry_ptr->password != NULL)
	{
		CFRelease(entry_ptr->password);
		entry_ptr->password = NULL;
	}
	if (entry_ptr->domain != NULL)
	{
		CFRelease(entry_ptr->domain);
		entry_ptr->domain = NULL;
	}
}

/*****************************************************************************/

static
void SetCredentials(
	struct authcache_entry *entry_ptr,
	CFStringRef new_username,
	CFStringRef new_password,
	CFStringRef new_domain)
{
	entry_ptr->username = new_username;
	entry_ptr->password = new_password;
	entry_ptr->domain = new_domain;
}

/*****************************************************************************/

// static
char *CopyCFStringToCString(CFStringRef theString)
{
	char *cstring;
	CFIndex usedBufLen;
	CFIndex converted;
	CFRange range;
	
	range = CFRangeMake(0, CFStringGetLength(theString));
	converted = CFStringGetBytes(theString, range, kCFStringEncodingUTF8, 0, false, NULL, 0, &usedBufLen);
	cstring = malloc(usedBufLen + 1);
	if ( cstring != NULL )
	{
		converted = CFStringGetBytes(theString, range, kCFStringEncodingUTF8, 0, false, (UInt8 *)cstring, usedBufLen, &usedBufLen);
		cstring[usedBufLen] = '\0';
	}
	return ( cstring );
}

/*****************************************************************************/

static
int CopyMountCredentials(
	CFHTTPAuthenticationRef auth,
	CFStringRef *username,
	CFStringRef *password,
	CFStringRef *domain,
	int *secureAuth)				/* <- TRUE if auth is sent securely */
{
	int result;
    CFStringRef method;
	
	/* determine if this authentication is secure */
	if ( gSecureConnection )
	{
		/* the connection is secure so the authentication is secure */
		*secureAuth = TRUE;
	}
	else
	{
		/* the connection is not secure, so secure means "not Basic authentication" */
		method = CFHTTPAuthenticationCopyMethod(auth);
		if ( method != NULL )
		{
			*secureAuth = !CFEqual(method, CFSTR("Basic"));
			CFRelease(method);
		}
		else
		{
			*secureAuth = FALSE;
		}
	}

	/* make sure we aren't using Basic over an unsecure connnection if gSecureServerAuth is TRUE */
	if ( (gSecureServerAuth == TRUE) && (*secureAuth == FALSE) ) {
		syslog(LOG_ERR, "Mount failed, Authentication method (Basic) too weak");
		result = EAUTH;
		goto SecureServerAuthRequired;
	}
	
	if ( mount_username != NULL )
	{
		CFRetain(mount_username);
		*username = mount_username;
		
		if ( mount_password != NULL )
		{
			CFRetain(mount_password);
		}
		*password = mount_password;
		
		if ( mount_domain != NULL )
		{
			CFRetain(mount_domain);
		}
		*domain = mount_domain;
		
		result = 0;
	}
	else
	{
		result = 1;
	}

SecureServerAuthRequired:
	
	return ( result );
}

/*****************************************************************************/
	
static
int CopyMountProxyCredentials(CFHTTPAuthenticationRef auth,
							  CFStringRef *username,
							  CFStringRef *password,
							  CFStringRef *domain,
							  int *secureAuth)		/* <- TRUE if auth is sent securely */
{
#pragma unused(domain)	
	int result = 1;
    CFStringRef method;
	
	if (auth == NULL) {
		syslog(LOG_ERR, "auth object arg is NULL");
		return result;
	}
	if (username == NULL) {
		syslog(LOG_ERR, "user arg is NULL");
		return result;
	}
	if (password == NULL) {
		syslog(LOG_ERR, "credential arg is NULL");
		return result;
	}
	if (domain == NULL) {
		syslog(LOG_ERR, "domain arg is NULL");
		return result;
	}
	if (secureAuth == NULL) {
		syslog(LOG_ERR, "secureAuth object is NULL");
		return result;
	}	
	
	/* determine if this authentication is secure */
	if ( gSecureConnection )
	{
		/* the connection is secure so the authentication is secure */
		*secureAuth = TRUE;
	}
	else
	{
		/* the connection is not secure, so secure means "not Basic authentication" */
		method = CFHTTPAuthenticationCopyMethod(auth);
		if ( method != NULL )
		{
			*secureAuth = !CFEqual(method, CFSTR("Basic"));
			CFRelease(method);
		}
		else
		{
			*secureAuth = FALSE;
		}
	}	
	
	if ( mount_proxy_username != NULL )
	{
		CFRetain(mount_proxy_username);
		*username = mount_proxy_username;
			
		if ( mount_proxy_password != NULL )
		{
			CFRetain(mount_proxy_password);
		}
		*password = mount_proxy_password;
		result = 0;
	}
		
	return ( result );
}
	
/*****************************************************************************/

static
int AddServerCredentials(
	struct authcache_entry *entry_ptr,
	CFHTTPMessageRef request)
{
	int result;
	/* locals for getting new values */
	CFStringRef username;
	CFStringRef password;
	CFStringRef domain;
	int secureAuth;
	
	username = password = domain = NULL;
	result = EACCES;

	if ( CFHTTPAuthenticationRequiresUserNameAndPassword(entry_ptr->auth) )
	{
		/* invalidate credential sources already tried */
		if (entry_ptr->authflags & kCredentialsFromMount)
		{
			entry_ptr->authflags |= kNoMountCredentials;
		}
	
		/* if we haven't tried the mount credentials, try them now */	
		if ( (entry_ptr->authflags & kNoMountCredentials) == 0 )
		{
			result = CopyMountCredentials(entry_ptr->auth, &username, &password, &domain, &secureAuth);
			if ( result == 0 )
			{
				ReleaseCredentials(entry_ptr);
				SetCredentials(entry_ptr, username, password, domain);
				entry_ptr->authflags |= kCredentialsFromMount;
			}
			else
			{
				/* there are no mount credentials */
				syslog(LOG_DEBUG, "AddServerCred: no mount creds, req %p", request);
				entry_ptr->authflags |= kNoMountCredentials;
			}
		}
			
		if ( (result == 0) && !(secureAuth) )
		{
			CFURLRef url;
			CFStringRef urlString;
			char *urlStr;
			
			urlStr = NULL;
			
			url = CFHTTPMessageCopyRequestURL(request);
			if ( url != NULL )
			{
				urlString = CFURLGetString(url);
				if ( urlString != NULL )
				{
					urlStr = CopyCFStringToCString(urlString);
				}
				else
				{
					urlStr = NULL;
				}
				CFRelease(url);
			}
			
			syslog(LOG_ERR | LOG_AUTHPRIV, "WebDAV FS authentication credentials are being sent insecurely to: %s", (urlStr ? urlStr : ""));
			
			if ( urlStr != NULL )
			{
				free(urlStr);
			}
		}
	}
	else
	{
		result = 0;
	}
	return ( result );
}

/*****************************************************************************/

static
int AddProxyCredentials(struct authcache_entry *entry_ptr, CFHTTPMessageRef request)
{
	int result;
	/* locals for getting new values */
	CFStringRef username;
	CFStringRef password;
	CFStringRef domain;
	int secureAuth;

	if ( CFHTTPAuthenticationRequiresUserNameAndPassword(entry_ptr->auth) )
	{
		username = password = domain = NULL;
		result = EACCES;
		
		/* invalidate credential sources already tried */
		if (entry_ptr->authflags & kCredentialsFromMount)
		{
			entry_ptr->authflags |= kNoMountCredentials;
		}
		
		/* if we haven't tried the mount credentials, try them now */	
		if ( (entry_ptr->authflags & kNoMountCredentials) == 0 )
		{
			if ( CopyMountProxyCredentials(entry_ptr->auth, &username, &password, &domain, &secureAuth) == 0 )
			{
				ReleaseCredentials(entry_ptr);
				SetCredentials(entry_ptr, username, password, domain);
				entry_ptr->authflags |= kCredentialsFromMount;
				result = 0;
			}
			else
			{
				/* there are no mount credentials */
				syslog(LOG_DEBUG, "AddProxyCredentials: no mount creds, req %p", request);
				entry_ptr->authflags |= kNoMountCredentials;
			}
		}
	
		if ( (result == 0) && !(secureAuth) )
		{
			CFURLRef url;
			CFStringRef urlString;
			char *urlStr;
		
			urlStr = NULL;
		
			url = CFHTTPMessageCopyRequestURL(request);
			if ( url != NULL )
			{
				urlString = CFURLGetString(url);
				if ( urlString != NULL )
				{
					urlStr = CopyCFStringToCString(urlString);
				}
				else
				{
					urlStr = NULL;
				}
				CFRelease(url);
			}
		
			syslog(LOG_ERR | LOG_AUTHPRIV, "WebDAV FS authentication proxy credentials are being sent insecurely to: %s", (urlStr ? urlStr : ""));
		
			if ( urlStr != NULL )
			{
				free(urlStr);
			}
		}
	}
	else {
		result = 0;
	}

	return ( result );
}

/*****************************************************************************/

static
struct authcache_entry *CreateProxyAuthenticationFromResponse(
	uid_t uid,							/* -> uid of the user making the request */
	CFHTTPMessageRef request,			/* -> the request message to apply authentication to */
	CFHTTPMessageRef response)			/* -> the response message  */
{
	struct authcache_entry *entry_ptr;
	int result;
	
	entry_ptr = calloc(sizeof(struct authcache_entry), 1);
	require(entry_ptr != NULL, calloc);
	
	entry_ptr->uid = uid;
	entry_ptr->auth = CFHTTPAuthenticationCreateFromResponse(kCFAllocatorDefault, response);
	require(entry_ptr->auth != NULL, CFHTTPAuthenticationCreateFromResponse);
	
	require(CFHTTPAuthenticationIsValid(entry_ptr->auth, NULL), CFHTTPAuthenticationIsValid);
	
	result = AddProxyCredentials(entry_ptr, request);
	require_noerr_quiet(result, AddProxyCredentials);
		
	authcache_proxy_entry = entry_ptr;
	++authcache_generation;
	if ( authcache_generation == 0 )
	{
		++authcache_generation;
	}
	
	return ( entry_ptr );

AddProxyCredentials:
CFHTTPAuthenticationIsValid:

	CFRelease(entry_ptr->auth);

CFHTTPAuthenticationCreateFromResponse:

	free(entry_ptr);
	
calloc:

	return ( NULL );
}

/*****************************************************************************/

static
struct authcache_entry *CreateAuthenticationFromResponse(
	uid_t uid,							/* -> uid of the user making the request */
	CFHTTPMessageRef request,			/* -> the request message to apply authentication to */
	CFHTTPMessageRef response,			/* -> the response message  */
	int *result,						/* -> result of this function (errno) */
	int isProxy)						/* -> if TRUE, create authcache_proxy_entry */
{
	struct authcache_entry *entry_ptr;
	*result = 0;
	
	entry_ptr = calloc(1, sizeof(struct authcache_entry));
	require_action(entry_ptr != NULL, calloc_err, *result = ENOMEM);
	
	entry_ptr->uid = uid;
	entry_ptr->auth = CFHTTPAuthenticationCreateFromResponse(kCFAllocatorDefault, response);
	require_action(entry_ptr->auth != NULL, CFHTTPAuthenticationCreateFromResponse, *result = EIO);
	
	require_action(CFHTTPAuthenticationIsValid(entry_ptr->auth, NULL), CFHTTPAuthenticationIsValid, *result = EIO);
	
	if ( !isProxy )
	{
		*result = AddServerCredentials(entry_ptr, request);
		require_noerr_quiet(*result, AddServerCredentials);
		
		LIST_INSERT_HEAD(&authcache_list, entry_ptr, entries);
	}
	else
	{
		*result = AddProxyCredentials(entry_ptr, request);
		require_noerr_quiet(*result, AddProxyCredentials);		
	}
	
	++authcache_generation;
	if ( authcache_generation == 0 )
	{
		++authcache_generation;
	}
	
	return ( entry_ptr );
	
AddProxyCredentials:
AddServerCredentials:
CFHTTPAuthenticationIsValid:
	
	CFRelease(entry_ptr->auth);
	
CFHTTPAuthenticationCreateFromResponse:
	
	free(entry_ptr);
	
calloc_err:
	
	return ( NULL );
}

/*****************************************************************************/

static
struct authcache_entry *FindAuthenticationForRequest(
	uid_t uid,							/* -> uid of the user making the request */
	CFHTTPMessageRef request)			/* -> the request message to apply authentication to */
{
	struct authcache_entry *entry_ptr;
	
	/* see if we have an authentication we can use */
	LIST_FOREACH(entry_ptr, &authcache_list, entries)
	{
		/*
		 * If this authentication is for the current user or root user,
		 * and it applies to this request, then break.
		 */
		if ( (entry_ptr->uid == uid) || (0 == uid) )
		{
			if ( CFHTTPAuthenticationAppliesToRequest(entry_ptr->auth, request) )
			{
				break;
			}
		}
	}
	return ( entry_ptr );
}

/*****************************************************************************/

static int ApplyCredentialsToRequest(struct authcache_entry *entry_ptr, CFHTTPMessageRef request)
{
	int result;

	if ( entry_ptr->domain != NULL ) {
		CFMutableDictionaryRef dict;
		
		dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 3, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		require_action(dict != NULL, CFDictionaryCreateMutable, result = FALSE);
		
		if ( entry_ptr->username != NULL ) {
			CFDictionaryAddValue(dict, kCFHTTPAuthenticationUsername, entry_ptr->username);
		}
		
		if ( entry_ptr->password != NULL ) {
			CFDictionaryAddValue(dict, kCFHTTPAuthenticationPassword, entry_ptr->password);
		}
		
		if ( entry_ptr->domain != NULL ) {
			CFDictionaryAddValue(dict, kCFHTTPAuthenticationAccountDomain, entry_ptr->domain);
		}
		
		result = CFHTTPMessageApplyCredentialDictionary(request, entry_ptr->auth, dict, NULL);
		
		CFRelease(dict);

	}
	else {
		result = CFHTTPMessageApplyCredentials(request, entry_ptr->auth, entry_ptr->username, entry_ptr->password,  NULL);
	}

CFDictionaryCreateMutable:
	
	return ( result );
}

/*****************************************************************************/

static
int AddExistingAuthentications(
		uid_t uid,				/* -> uid of the user making the request */
	CFHTTPMessageRef request)	/* -> the request message to apply authentication to */
{
	struct authcache_entry *entry_ptr;
	
	entry_ptr = FindAuthenticationForRequest(uid, request);
	if ( entry_ptr != NULL )
	{	
		/* try to apply valid entry to the request */
		if ( CFHTTPAuthenticationIsValid(entry_ptr->auth, NULL) )
		{
			if ( !ApplyCredentialsToRequest(entry_ptr, request) )
			{
				/*
				 * Remove the unusable entry and do nothing -- we'll get a 401 when this request goes to the server
				 * which should allow us to create a new entry.
				 */
				RemoveAuthentication(entry_ptr);
			}
		}
		else
		{
			/*
			 * Remove the unusable entry and do nothing -- we'll get a 401 when this request goes to the server
			 * which should allow us to create a new entry.
			 */
			RemoveAuthentication(entry_ptr);
		}
	}
	
	if ( authcache_proxy_entry != NULL )
	{
		/* try to apply valid entry to the request */
		if ( !CFHTTPAuthenticationIsValid(authcache_proxy_entry->auth, NULL) || !ApplyCredentialsToRequest(authcache_proxy_entry, request) )
		{
			/*
			 * Remove the unusable entry and do nothing -- we'll get a 407 when this request goes to the server
			 * which should allow us to create a new entry.
			 */
			RemoveAuthentication(authcache_proxy_entry);
		}
	}
	
	return ( 0 );
}

/*****************************************************************************/

/*
 * DoServerAuthentication
 *
 * Handles authentication challenge (401) from the server
 * Adds a new authcache_entry, or update an exiting authcache_entry for a server.
 */
 static
int DoServerAuthentication(
	uid_t uid,							/* -> uid of the user making the request */
	CFHTTPMessageRef request,			/* -> the request message to apply authentication to */
	CFHTTPMessageRef response)			/* -> the response containing the challenge, or NULL if no challenge */
{
	struct authcache_entry *entry_ptr;
	int result = 0;

	/* see if we already have an authcache_entry */
	entry_ptr = FindAuthenticationForRequest(uid, request);
	
	/* if we have one, we need to try to update it and use it */
	if ( entry_ptr != NULL )
	{
		// Clear valid flag since we just got a 401 for this auth_entry
		entry_ptr->authflags &= ~kCredentialsValid;	
	
		/* ensure the CFHTTPAuthenticationRef is valid */
		if ( CFHTTPAuthenticationIsValid(entry_ptr->auth, NULL) )
		{
			result = 0;
		}
		else
		{
			/* It's invalid so release the old one and try to create a new one */
			CFRelease(entry_ptr->auth);
			entry_ptr->auth = CFHTTPAuthenticationCreateFromResponse(kCFAllocatorDefault, response);
			if ( entry_ptr->auth != NULL )
			{
				if ( CFHTTPAuthenticationIsValid(entry_ptr->auth, NULL) )
				{
					result = AddServerCredentials(entry_ptr, request);
				}
				else
				{
					result = EACCES;
				}
			}
			else
			{
				result = EACCES;
			}
		}
		if ( result != 0 )
		{
			RemoveAuthentication(entry_ptr);
		}
	}
	else
	{
		/* create a new authcache_entry */
		entry_ptr = CreateAuthenticationFromResponse(uid, request, response, &result, FALSE);
	}
	
	return ( result );
}
 
/*****************************************************************************/

/*
 * AddProxyAuthentication
 *
 * Add a new authcache_entry, or update an exiting authcache_entry for a proxy.
 */
static
int AddProxyAuthentication(
	uid_t uid,							/* -> uid of the user making the request */
	CFHTTPMessageRef request,			/* -> the request message to apply authentication to */
	CFHTTPMessageRef response)			/* -> the response containing the challenge, or NULL if no challenge */
{
	int result;
	
	/* if we have an entry for the proxy, we need to try to update it and use it */
	if ( authcache_proxy_entry != NULL )
	{
		authcache_proxy_entry->authflags &= ~kCredentialsValid;
		
		/* ensure the CFHTTPAuthenticationRef is valid */
		if ( CFHTTPAuthenticationIsValid(authcache_proxy_entry->auth, NULL) )
		{
			result = 0;
		}
		else
		{
			/* It's invalid so release the old and try to create a new one */
			CFRelease(authcache_proxy_entry->auth);
			authcache_proxy_entry->auth = CFHTTPAuthenticationCreateFromResponse(kCFAllocatorDefault, response);
			if ( authcache_proxy_entry->auth != NULL )
			{
				if ( CFHTTPAuthenticationIsValid(authcache_proxy_entry->auth, NULL) )
				{
					result = AddProxyCredentials(authcache_proxy_entry, request);
				}
				else
				{
					result = EACCES;
				}
			}
			else
			{
				result = EACCES;
			}
		}
		if ( result != 0 )
		{
			RemoveAuthentication(authcache_proxy_entry);
		}
	}
	else
	{
		/* create a new authcache_entry for the proxy */
		authcache_proxy_entry = CreateProxyAuthenticationFromResponse(uid, request, response);
		if ( authcache_proxy_entry != NULL )
		{
			result = 0;
		}
		else
		{
			result = EACCES;
		}
	}
	
	return ( result );
}

/*****************************************************************************/

int authcache_apply(
	uid_t uid,							/* -> uid of the user making the request */
	CFHTTPMessageRef request,			/* -> the request message to apply authentication to */
	UInt32 statusCode,					/* -> the status code (401, 407), or 0 if no challenge */
	CFHTTPMessageRef response,			/* -> the response containing the challenge, or NULL if no challenge */
	UInt32 *generation)					/* <- the generation count of the cache entry */
{
	int result, result2;
		
	/* lock the Authcache */
	result = pthread_mutex_lock(&authcache_lock);
	require_noerr_action(result, pthread_mutex_lock, webdav_kill(-1));

	switch (statusCode)
	{
	case 0:
		/* no challenge */
		result = 0;
		break;
		
	case 401:
		/* server challenge -- add server authentication */
		
		/* only add server authentication if the uid is the mount's user or root user */
		if ( (gProcessUID == uid) || (0 == uid) )
		{
			result = DoServerAuthentication(uid, request, response);
		}
		else
		{
			result = EACCES;
		}
		break;
		
	case 407:
		/* proxy challenge -- add proxy authentication */
		
		/* only add proxy authentication if the uid is the mount's user or root user */
		if ( (gProcessUID == uid) || (0 == uid) )
		{
			result = AddProxyAuthentication(uid, request, response);
		}
		else
		{
			result = EACCES;
		}
		break;
		
	default:
		/* should never happen */
		result = EACCES;
		break;
	}
	
	/* only apply existing authentications if the uid is the mount's user or root user */
	if ( (result == 0) && ((gProcessUID == uid) || (0 == uid)) )
	{
		result = AddExistingAuthentications(uid, request);
	}
	
	/* return the current authcache_generation */
	*generation = authcache_generation;

	/* unlock the Authcache */
	result2 = pthread_mutex_unlock(&authcache_lock);
	require_noerr_action(result2, pthread_mutex_unlock, result = result2; webdav_kill(-1));

pthread_mutex_unlock:
pthread_mutex_lock:

	return ( result );
}

/*****************************************************************************/

int authcache_valid(
	uid_t uid,							/* -> uid of the user making the request */
	CFHTTPMessageRef request,			/* -> the message of the successful request */
	UInt32 generation)					/* -> the generation count of the cache entry */
{
	int result, result2;
	
	/* only validate authentications if the uid is the mount's user or root user */
	require_quiet(((gProcessUID == uid) || (0 == uid)), not_owner_uid);

	/* lock the Authcache */
	result = pthread_mutex_lock(&authcache_lock);
	require_noerr_action(result, pthread_mutex_lock, webdav_kill(-1));

	if ( generation == authcache_generation )
	{
		struct authcache_entry *entry_ptr;
		
		/* see if we have an authcache_entry */
		entry_ptr = FindAuthenticationForRequest(uid, request);
		if ( entry_ptr != NULL )
		{
			/* mark this authentication valid */
			entry_ptr->authflags |= kCredentialsValid;			
		}		
		
		if ( authcache_proxy_entry != NULL )
		{
			/* mark this authentication valid */
			authcache_proxy_entry->authflags |= kCredentialsValid;
		}
	}
	
	/* unlock the Authcache */
	result2 = pthread_mutex_unlock(&authcache_lock);
	require_noerr_action(result2, pthread_mutex_unlock, result = result2; webdav_kill(-1));

pthread_mutex_unlock:
pthread_mutex_lock:
not_owner_uid:

	return ( 0 );
}

/*****************************************************************************/

int authcache_proxy_invalidate(void)
{
	int result, result2;
	
	/* lock the Authcache */
	result = pthread_mutex_lock(&authcache_lock);
	require_noerr_action(result, pthread_mutex_lock, webdav_kill(-1));
	
	/* called when proxy settings change -- remove any proxy authentications */
	if ( authcache_proxy_entry != NULL )
	{
		RemoveAuthentication(authcache_proxy_entry);
	}

	/* unlock the Authcache */
	result2 = pthread_mutex_unlock(&authcache_lock);
	require_noerr_action(result2, pthread_mutex_unlock, result = result2; webdav_kill(-1));

pthread_mutex_unlock:
pthread_mutex_lock:

	return ( result );
}

/*****************************************************************************/

int authcache_init(
	char *username,				/* -> username to attempt to use on first server challenge, or NULL */
	char *password,				/* -> password to attempt to use on first server challenge, or NULL */
	char *proxy_username,		/* -> username to attempt to use on first proxy server challenge, or NULL */
	char *proxy_password,		/* -> password to attempt to use on first proxy server challenge, or NULL */
	char *domain)				/* -> account domain to attempt to use on first server challenge, or NULL */
{
	int result;
	pthread_mutexattr_t mutexattr;
	
	/* set up the lock on the list */
	result = pthread_mutexattr_init(&mutexattr);
	require_noerr(result, pthread_mutexattr_init);
	
	result = pthread_mutex_init(&authcache_lock, &mutexattr);
	require_noerr(result, pthread_mutex_init);
	
	LIST_INIT(&authcache_list);
	authcache_generation = 1;
	
	result = 0;
	
	if ( username != NULL && password != NULL && username[0] != '\0')
	{
		mount_username = CFStringCreateWithCString(kCFAllocatorDefault, username, kCFStringEncodingUTF8);
		require_action(mount_username != NULL, CFStringCreateWithCString, result = ENOMEM);
		
		mount_password = CFStringCreateWithCString(kCFAllocatorDefault, password, kCFStringEncodingUTF8);
		require_action(mount_password != NULL, CFStringCreateWithCString, result = ENOMEM);
	}

	if ( proxy_username != NULL && proxy_password != NULL && proxy_username[0] != '\0')
	{
		mount_proxy_username = CFStringCreateWithCString(kCFAllocatorDefault, proxy_username, kCFStringEncodingUTF8);
		require_action(mount_proxy_username != NULL, CFStringCreateWithCString, result = ENOMEM);
		
		mount_proxy_password = CFStringCreateWithCString(kCFAllocatorDefault, proxy_password, kCFStringEncodingUTF8);
		require_action(mount_proxy_password != NULL, CFStringCreateWithCString, result = ENOMEM);
	}	
	
	if ( domain != NULL && domain[0] != '\0' )
	{
		mount_domain = CFStringCreateWithCString(kCFAllocatorDefault, domain, kCFStringEncodingUTF8);
		require_action(mount_domain != NULL, CFStringCreateWithCString, result = ENOMEM);
	}

CFStringCreateWithCString:
pthread_mutex_init:
pthread_mutexattr_init:

	return ( result );
}

/*****************************************************************************/
