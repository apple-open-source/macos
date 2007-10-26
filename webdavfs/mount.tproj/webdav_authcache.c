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
#include <Security/SecKey.h>
#include <Security/SecKeychain.h>
#include <Security/SecKeychainItem.h>
#include <Security/SecKeychainSearch.h>
#include <pthread.h>
#include "webdav_authcache.h"
#include "webdav_network.h"

/*****************************************************************************/

struct authcache_entry
{
	uid_t uid;
	CFHTTPAuthenticationRef auth;
	CFStringRef username;
	CFStringRef password;
	CFStringRef domain;			/* can be NULL if there is no account domain */
	u_int32_t authflags;		/* The keychain options for this authorization */
	u_int32_t generation;		/* the generation of authcache_entry */
};

/* authFlags */
enum
{
	/* No flags */
	kAuthNone					= 0x00000000,
	
	/* The credentials came from one of these sources -- only one bit will be set at any time */
	kCredentialsFromMount		= 0x00000001,	/* Credentials passed at mount time */
	kCredentialsFromKeychain	= 0x00000002,	/* Credentials retrieved from keychain */
	kCredentialsFromUI			= 0x00000004,	/* Credentials retrieved from UI */
	/* a mask for determining if the auth has credentials */
	kAuthHasCredentials			= (kCredentialsFromMount | kCredentialsFromKeychain | kCredentialsFromUI),
	
	/* Set if mount credentials should not be used (they were tried and didn't work) */
	kNoMountCredentials			= 0x00000008,
	/* Set if keychain credentials should not be used (they were tried and didn't work) */
	kNoKeychainCredentials		= 0x00000010,
	
	/* Set once the credentials are successfully used for a transaction */
	kCredentialsValid			= 0x00000020,
	
	/* Set if valid credentials from UI should be added to the keychain */
	kAddCredentialsToKeychain	= 0x00000040
};

/*****************************************************************************/

static pthread_mutex_t authcache_lock;					/* lock for authcache */
static u_int32_t authcache_generation = 1;				/* generation count of authcache (never zero)*/
static struct authcache_entry *authcache_server_entry = NULL;	/* the authcache_entry for the http server */
static struct authcache_entry *authcache_proxy_entry = NULL;	/* the authcache_entry for the proxy server, or NULL */
static CFStringRef mount_username = NULL;
static CFStringRef mount_password = NULL;
static CFStringRef mount_domain = NULL;

/* Authentication states of a mount */
enum AuthCache_State
{
	UNDEFINED_GUEST = 0,	/* (initial) authcache_server_entry is NULL */
	TRY_MOUNT_CRED = 1,		/* (transient) authenticating with webdav_mount credentials ("-a" option) */
	TRY_KEYCHAIN_CRED = 2,	/* (transient) authenticating with credentials found in the keychain */
	TRY_UI_CRED = 3,		/* (transient) authenticating with credentials from User Notification (auth dialog) */
	AUTHENTICATED_USER = 4,	/* (final) Authenticated mount */
	GUEST_USER = 5			/* (final) Guest mount.  Authentication was cancelled by user action (from auth dialog) */
};

/* Current authentication state of the mount */
static enum AuthCache_State authcache_state = UNDEFINED_GUEST;

/* Once the mount is authenticated, how many times to retry */
/* a request that is being returned by the server with */
/* 401 status.  The request will be retried to handle cases */
/* such a stale nonce with no "stale" directive.  It's also */
/* possible that the user doesn't have the right permissions. */
/* Either case, this controls how many times a request will */
/* be retried AFTER the mount has been successfully authenticated */
enum {MAX_AUTHENTICATED_USER_RETRIES = 4};

/*****************************************************************************/

static
OSStatus KeychainItemCopyAccountPassword(
	SecKeychainItemRef itemRef,
	CFStringRef *username,
	CFStringRef *password,
	CFStringRef *domain);

static
char *CopyCFStringToCString(CFStringRef theString);

static
void ReleaseCredentials(struct authcache_entry *entry_ptr);

/*****************************************************************************/

static
void LoginFailedWarning(void)
{
	SInt32 error;
	CFURLRef localizationPath;
	CFURLRef iconPath;
	CFOptionFlags responseFlags;
	CFMutableDictionaryRef dictionary;
	CFUserNotificationRef userNotification;

	dictionary = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
		&kCFTypeDictionaryValueCallBacks);
	require(dictionary != NULL, CFDictionaryCreateMutable);

	localizationPath = CFURLCreateWithFileSystemPath(NULL, CFSTR(WEBDAV_LOCALIZATION_BUNDLE),
		kCFURLPOSIXPathStyle, TRUE);
	require(localizationPath != NULL, CFURLCreateWithFileSystemPath_localization);

	CFDictionaryAddValue(dictionary, kCFUserNotificationLocalizationURLKey, localizationPath);
	
	iconPath = CFURLCreateWithFileSystemPath(NULL, CFSTR(WEBDAV_SERVER_ICON_PATH),
		kCFURLPOSIXPathStyle, TRUE);
	require(iconPath != NULL, CFURLCreateWithFileSystemPath_Icon);
	
	CFDictionaryAddValue(dictionary, kCFUserNotificationIconURLKey, iconPath);

	CFDictionaryAddValue(dictionary, kCFUserNotificationAlertHeaderKey, CFSTR("WEBDAV_LOGIN_FAILED_HEADER_KEY"));
	CFDictionaryAddValue(dictionary, kCFUserNotificationAlertMessageKey, CFSTR("WEBDAV_LOGIN_FAILED_MSG_KEY"));
	CFDictionaryAddValue(dictionary, kCFUserNotificationDefaultButtonTitleKey, CFSTR("WEBDAV_LOGIN_FAILED_OK_KEY"));
		
	userNotification = CFUserNotificationCreate(NULL, WEBDAV_AUTHENTICATION_TIMEOUT,
		kCFUserNotificationStopAlertLevel, &error, dictionary);
	require(userNotification != NULL, CFUserNotificationCreate);

	CFUserNotificationReceiveResponse(userNotification, WEBDAV_AUTHENTICATION_TIMEOUT,
		&responseFlags);

	CFRelease(userNotification);
CFUserNotificationCreate:
	CFRelease(iconPath);
CFURLCreateWithFileSystemPath_Icon:
	CFRelease(localizationPath);
CFURLCreateWithFileSystemPath_localization:
	CFRelease(dictionary);
CFDictionaryCreateMutable:

	return;
}

/*****************************************************************************/

/* Return Values
 *	0			Success, credentials were obtained successfully
 *	ECANCELED	User cancelled the notification.
 *	EACCES		Notification timed out.
 *	ENOMEM		Something unexpected happened.
 */
static
int CopyCredentialsFromUserNotification(
	CFHTTPAuthenticationRef auth,	/* -> the authentication to get credentials for */
	CFHTTPMessageRef request,		/* -> the request message that was challenged */
	int badlogin,					/* -> if TRUE, the previous credentials retrieved from the user were not valid */
	int isProxy,					/* -> TRUE if getting proxy credentials */
	CFStringRef *username,			/* <-> input: the previous username entered, or NULL; output: the username */
	CFStringRef *password,			/* <-> input: the previous password entered, or NULL; output: the password */
	CFStringRef *domain,			/* <-> input: the previous domain entered, or NULL; output: the domain, or NULL if authentication doesn't use domains */
	int *addtokeychain,				/* <- TRUE if the user wants these credentials added to their keychain */
	int *secureAuth)				/* <- TRUE if auth is sent securely */
/* IMPORTANT: if username, password, or domain values are passed in, webdav_get_authentication() releases them */
{
	int result;
    CFStringRef method;
	int useDomain;
	int index;
    CFTypeRef a[3];
    CFArrayRef array;
	SInt32 error;
	CFOptionFlags responseFlags;
	CFURLRef url;
	CFStringRef urlString;
	CFStringRef realmString;
	CFMutableDictionaryRef dictionary;
	CFURLRef localizationPath;
	CFURLRef iconPath;
	CFUserNotificationRef userNotification;
	
	result = ENOMEM;	/* returned if something unexpected happens */
	*secureAuth = FALSE;
	
	/* are we asking again because the name and password didn't work? */
	if ( badlogin )
	{
		/* tell them it didn't work */
		LoginFailedWarning();
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
	
	/* get the url and realm strings */
	if ( isProxy )
	{
		int httpProxyEnabled;
		char *httpProxyServer;
		int httpProxyPort;
		int httpsProxyEnabled;
		char *httpsProxyServer;
		int httpsProxyPort;
		
		url = NULL;
		
		result = network_get_proxy_settings(&httpProxyEnabled, &httpProxyServer, &httpProxyPort,
			&httpsProxyEnabled, &httpsProxyServer, &httpsProxyPort);
		require_noerr_quiet(result, network_get_proxy_settings);
		
		if ( gSecureConnection )
		{
			urlString = CFStringCreateWithCString(kCFAllocatorDefault, httpsProxyServer, kCFStringEncodingUTF8);
		}
		else
		{
			urlString = CFStringCreateWithCString(kCFAllocatorDefault, httpProxyServer, kCFStringEncodingUTF8);
		}
		
		free(httpProxyServer);
		free(httpsProxyServer);
		
		require(urlString != NULL, CFStringCreateWithCString);
	}
	else
	{
		url = CFHTTPMessageCopyRequestURL(request);
		require(url != NULL, CFHTTPMessageCopyRequestURL);
		
		urlString = CFURLGetString(url);
		require(urlString != NULL, CFURLGetString);
		
		/* make sure we aren't using Basic over an unsecure connnection if gSecureServerAuth is TRUE */
		require_action_quiet(!gSecureServerAuth || (*secureAuth), SecureServerAuthRequired, result = EACCES);
	}

	realmString = CFHTTPAuthenticationCopyRealm(auth);

	/* does this authentication method require a domain? */
	useDomain = CFHTTPAuthenticationRequiresAccountDomain(auth);
	
	/* create the dictionary */
	dictionary = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
		&kCFTypeDictionaryValueCallBacks);
	require(dictionary != NULL, CFDictionaryCreateMutable);
	
	localizationPath = CFURLCreateWithFileSystemPath(NULL, CFSTR(WEBDAV_LOCALIZATION_BUNDLE),
		kCFURLPOSIXPathStyle, TRUE);
	require(localizationPath != NULL, CFURLCreateWithFileSystemPath_localization);
	
	CFDictionaryAddValue(dictionary, kCFUserNotificationLocalizationURLKey, localizationPath);
	
	iconPath = CFURLCreateWithFileSystemPath(NULL, CFSTR(WEBDAV_SERVER_ICON_PATH),
		kCFURLPOSIXPathStyle, TRUE);
	require(iconPath != NULL, CFURLCreateWithFileSystemPath_Icon);
	
	CFDictionaryAddValue(dictionary, kCFUserNotificationIconURLKey, iconPath);
	
	CFDictionaryAddValue(dictionary, kCFUserNotificationAlertHeaderKey, CFSTR("WEBDAV_AUTH_HEADER_KEY"));
	/* In the future, there will be a symbolic string constant for "AlertMessageWithParameters". */
	CFDictionaryAddValue(dictionary, CFSTR("AlertMessageWithParameters"),
		useDomain ?  CFSTR("WEBDAV_AUTH_DOMAIN_MSG_WITH_PARAMETERS_KEY") : CFSTR("WEBDAV_AUTH_MSG_WITH_PARAMETERS_KEY"));
	
    a[0] = urlString;
	a[1] = (realmString != NULL) ? realmString : CFSTR("");
	a[2] = *secureAuth ? CFSTR("WEBDAV_AUTH_MSG_SECURE_PARAMETER_KEY") : CFSTR("WEBDAV_AUTH_MSG_INSECURE_PARAMETER_KEY");
    array = CFArrayCreate(NULL, a, 3, &kCFTypeArrayCallBacks);
    require(array != NULL, CFArrayCreate_AlertMessageParameter);
    
	/* In the future, there will be a symbolic string constant for "AlertMessageParameter". */
	CFDictionaryAddValue(dictionary, CFSTR("AlertMessageParameter"), array);
    CFRelease(array);
	
	index = 0;
	if ( useDomain )
	{
		a[index++] = CFSTR("WEBDAV_AUTH_DOMAIN_KEY");
	}
	a[index++] = CFSTR("WEBDAV_AUTH_USERNAME_KEY");
	a[index++] = CFSTR("WEBDAV_AUTH_PASSWORD_KEY");
    array = CFArrayCreate(NULL, a, index, &kCFTypeArrayCallBacks);
    require(array != NULL, CFArrayCreate_TextFieldTitles);
	
    CFDictionaryAddValue(dictionary, kCFUserNotificationTextFieldTitlesKey, array);
    CFRelease(array);
	
	index = 0;
	if ( useDomain )
	{
		a[index++] = (*domain != NULL) ? *domain : CFSTR("");
	}
	a[index++] = (*username != NULL) ? *username : CFSTR("");
	a[index++] = (*password != NULL) ? *password : CFSTR("");
    array = CFArrayCreate(NULL, a, index, &kCFTypeArrayCallBacks);
	*username = NULL;
	*password = NULL;
	*domain = NULL;
    require(array != NULL, CFArrayCreate_TextFieldValues);
	
	CFDictionaryAddValue(dictionary, kCFUserNotificationTextFieldValuesKey, array);
	CFRelease(array);

	CFDictionaryAddValue(dictionary, kCFUserNotificationCheckBoxTitlesKey, CFSTR("WEBDAV_AUTH_KEYCHAIN_KEY"));
	CFDictionaryAddValue(dictionary, kCFUserNotificationDefaultButtonTitleKey, CFSTR("WEBDAV_AUTH_OK_KEY"));
	CFDictionaryAddValue(dictionary, kCFUserNotificationAlternateButtonTitleKey, CFSTR("WEBDAV_AUTH_CANCEL_KEY"));

	userNotification = CFUserNotificationCreate(NULL, WEBDAV_AUTHENTICATION_TIMEOUT,
		kCFUserNotificationPlainAlertLevel | CFUserNotificationSecureTextField(useDomain ? 2 : 1), &error, dictionary);
	require(userNotification != NULL, CFUserNotificationCreate);

	/* if the UNC notification did not time out */
	if ( CFUserNotificationReceiveResponse(userNotification, WEBDAV_AUTHENTICATION_TIMEOUT, &responseFlags) == 0) 
	{
		/* and the user clicked OK (default), get the user's response */
		if ( (responseFlags & 3) == kCFUserNotificationDefaultResponse)
		{
			/* get the user's input */
			index = 0;
			if ( useDomain )
			{
				*domain = CFRetain(CFUserNotificationGetResponseValue(userNotification, kCFUserNotificationTextFieldValuesKey, index++));
			}
			*username = CFRetain(CFUserNotificationGetResponseValue(userNotification, kCFUserNotificationTextFieldValuesKey, index++));
			*password = CFRetain(CFUserNotificationGetResponseValue(userNotification, kCFUserNotificationTextFieldValuesKey, index++));
			*addtokeychain = ((responseFlags & CFUserNotificationCheckBoxChecked(0)) != 0);
			result = 0;
		}
		else if ( (responseFlags & 3) == kCFUserNotificationAlternateResponse)
		{
			/* the user hit the Cancel button */
			result = ECANCELED;
			syslog(LOG_DEBUG, "User auth notification cancelled by user");
		}
		else
		{
			/* Timedout, no button pressed */
			result = EACCES;
			syslog(LOG_DEBUG, "User auth notification timed out");
		}
	}
	else
	{
		/* timed out, no button pressed */
        result = EACCES;
		syslog(LOG_DEBUG, "User auth notification timed out");
	}

	/*
	 * release everything we copied or created
	 */
	CFRelease(userNotification);
CFUserNotificationCreate:
CFArrayCreate_TextFieldValues:
CFArrayCreate_TextFieldTitles:
CFArrayCreate_AlertMessageParameter:
	CFRelease(iconPath);
CFURLCreateWithFileSystemPath_Icon:
	CFRelease(localizationPath);
CFURLCreateWithFileSystemPath_localization:
	CFRelease(dictionary);
CFDictionaryCreateMutable:
	if ( realmString != NULL )
	{
		CFRelease(realmString);
	}
SecureServerAuthRequired:
CFURLGetString:
	if ( url != NULL )
	{
		CFRelease(url);
	}
CFStringCreateWithCString:
CFHTTPMessageCopyRequestURL:
network_get_proxy_settings:
	
	return ( result );
}

/*****************************************************************************/

static
void RemoveAuthentication(struct authcache_entry *entry_ptr)
{
	/* only valid authcache_proxy_entry are in the authcache_list */
	if ( entry_ptr != authcache_proxy_entry )
	{
		authcache_server_entry = NULL;
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

static
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
OSStatus KeychainItemCopyAccountPassword(
	SecKeychainItemRef itemRef,
	CFStringRef *username,
	CFStringRef *password,
	CFStringRef *domain)
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
		*username = CFStringCreateWithBytes(kCFAllocatorDefault, attr.data, attr.length, kCFStringEncodingUTF8, false);
		*password = CFStringCreateWithBytes(kCFAllocatorDefault, outData, length, kCFStringEncodingUTF8, false);
		*domain = NULL; /* no domain in keychain items */
		(void) SecKeychainItemFreeContent(&attrList, outData);
	}
	return ( result );
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
	require_action_quiet(!gSecureServerAuth || (*secureAuth), SecureServerAuthRequired, result = EACCES);
	
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
char *CopyComponentPathToCString(CFURLRef url)
{
	char *result;
	CFRange range;
	
	CFIndex bufferLength;
	UInt8 *buffer;
	
	result = NULL;
	buffer = NULL;
	
	/* get the buffer length */
	bufferLength = CFURLGetBytes(url, NULL, 0);
	require(bufferLength != 0, CFURLGetBytes_length);
	
	/* allocate a buffer for the URL's bytes */
	buffer = malloc(bufferLength);
	require(buffer != NULL, malloc_buffer);
	
	/* get the bytes */
	require(CFURLGetBytes(url, buffer, bufferLength) == bufferLength, CFURLGetBytes);
	
	/* get the range of kCFURLComponentPath */
	range = CFURLGetByteRangeForComponent(url, kCFURLComponentPath, NULL);
	require(range.location != kCFNotFound, CFURLGetByteRangeForComponent);
	
	/* allocate result buffer */
	result = malloc(range.length + 1);
	require(result != NULL, malloc_result);
	
	/* copy the component path string */
	strncpy(result, (char *)&buffer[range.location], range.length);
	result[range.length] = '\0';

malloc_result:
CFURLGetByteRangeForComponent:
CFURLGetBytes:

	free (buffer);

malloc_buffer:
CFURLGetBytes_length:

	return ( result );
}

/*****************************************************************************/

static
int CopyCredentialsFromKeychain(
	CFHTTPAuthenticationRef auth,
	CFHTTPMessageRef request,
	CFStringRef *username,
	CFStringRef *password,
	CFStringRef *domain,
	int isProxy,
	int *secureAuth)				/* <- TRUE if auth is sent securely */
{
	OSStatus result;
	CFURLRef messageURL;
	CFStringRef theString;
	SecProtocolType protocol;
	SecAuthenticationType authenticationType;
	SecKeychainItemRef itemRef;
	int portNumber;
	char *serverName;
	char *realmStr;
	char *path;
	
	result = 0;
	serverName = NULL;
	realmStr = NULL;
	path = NULL;
	
	/* get the URL */
	messageURL = CFHTTPMessageCopyRequestURL(request);
	require_action(messageURL != NULL, CFHTTPMessageCopyRequestURL, result = 1);
	
	/* get the protocol type */
	theString = CFURLCopyScheme(messageURL);
	if ( CFEqual(theString, CFSTR("http")) )
	{
		if ( isProxy )
		{
			protocol = kSecProtocolTypeHTTPProxy;
		}
		else
		{
			protocol = kSecProtocolTypeHTTP;
		}
	}
	else if ( CFEqual(theString, CFSTR("https")) )
	{
		if ( isProxy )
		{
			protocol = kSecProtocolTypeHTTPSProxy;
		}
		else
		{
			protocol = kSecProtocolTypeHTTPS;
		}
	}
	else
	{
		protocol = 0;
	}
	CFRelease(theString);
	require_action(protocol != 0, unknown_protocol, result = 1);
	
	/* get the authentication method */
	theString = CFHTTPAuthenticationCopyMethod(auth);
	if ( CFEqual(theString, CFSTR("Basic")) )
	{
		authenticationType = kSecAuthenticationTypeHTTPBasic;
	}
	else if ( CFEqual(theString, CFSTR("Digest")) )
	{
		authenticationType = kSecAuthenticationTypeHTTPDigest;
	}
	else if ( CFEqual(theString, CFSTR("NTLM")) )
	{
		authenticationType = kSecAuthenticationTypeNTLM;
	}
	else
	{
		authenticationType = kSecAuthenticationTypeDefault;
	}
	CFRelease(theString);
	
	*secureAuth = (protocol == kSecProtocolTypeHTTPSProxy) ||
					(protocol == kSecProtocolTypeHTTPS) ||
					(authenticationType != kSecAuthenticationTypeHTTPBasic);
	
	if ( isProxy )
	{
		/* Proxy: Get the serverName and portNumber */
		int httpProxyEnabled;
		char *httpProxyServer;
		int httpProxyPort;
		int httpsProxyEnabled;
		char *httpsProxyServer;
		int httpsProxyPort;
		
		result = network_get_proxy_settings(&httpProxyEnabled, &httpProxyServer, &httpProxyPort,
			&httpsProxyEnabled, &httpsProxyServer, &httpsProxyPort);
		require_noerr_quiet(result, network_get_proxy_settings);
		
		if ( protocol == kSecProtocolTypeHTTPProxy )
		{
			free(httpsProxyServer);
			serverName = httpProxyServer;
			portNumber = httpProxyPort;
		}
		else
		{
			free(httpProxyServer);
			serverName = httpsProxyServer;
			portNumber = httpsProxyPort;
		}
		result = SecKeychainFindInternetPassword(NULL,	/* default keychain */
			strlen(serverName), serverName,				/* serverName */
			0, NULL,									/* no securityDomain */
			0, NULL,									/* no accountName */
			0, NULL,									/* no path */
			portNumber,									/* port */
			protocol,									/* protocol */
			0,											/* no authenticationType */
			0, NULL,									/* no password */
			&itemRef);
	}
	else
	{
		/* Server: Get the path, serverName, portNumber, and realmStr */
		
		/* make sure we aren't using Basic over an unsecure connnection if gSecureServerAuth is TRUE */
		require_action_quiet(!gSecureServerAuth || (*secureAuth), SecureServerAuthRequired, result = EACCES);
			
		/* get the path of the base URL (used because it needs to be unique for a mount point) */
		path = CopyComponentPathToCString(gBaseURL);
		
		/* get the server name and port number for the server */
		theString = CFURLCopyHostName(messageURL);
		require(theString != NULL, CFURLCopyHostName);
		
		serverName = CopyCFStringToCString(theString);
		CFRelease(theString);
		require(serverName != NULL, CopyCFStringToCString);
		
		/* get the realm (securityDomain) */
		theString = CFHTTPAuthenticationCopyRealm(auth);
		if ( theString != NULL )
		{
			realmStr = CopyCFStringToCString(theString);
			CFRelease(theString);
		}
		
		portNumber = CFURLGetPortNumber(messageURL);
		if ( portNumber == -1 )
		{
			if ( protocol == kSecProtocolTypeHTTP )
			{
				portNumber = kHttpDefaultPort;
			}
			else if ( protocol == kSecProtocolTypeHTTPS )
			{
				portNumber = kHttpsDefaultPort;
			}
		}
		
		result = SecKeychainFindInternetPassword(NULL,
			strlen(serverName), serverName,						/* serverName */
			(realmStr != NULL) ? strlen(realmStr) : 0, realmStr, /* securityDomain */
			0, NULL,											/* no accountName */
			(path != NULL) ? strlen(path) : 0, path,			/* path */
			portNumber,											/* port */
			protocol,											/* protocol */
			authenticationType,									/* authenticationType */
			0, NULL,											/* no password */
			&itemRef);
	}
		
	if ( result == noErr )
	{
		result = KeychainItemCopyAccountPassword(itemRef, username, password, domain);
		CFRelease(itemRef);
	}

network_get_proxy_settings:
CopyCFStringToCString:
CFURLCopyHostName:
SecureServerAuthRequired:
unknown_protocol:
CFHTTPMessageCopyRequestURL:

	if ( serverName != NULL )
	{
		free(serverName);
	}
	if ( realmStr != NULL )
	{
		free(realmStr);
	}
	if ( path != NULL )
	{
		free(path);
	}
	if ( messageURL != NULL )
	{
		CFRelease(messageURL);
	}
	
	return ( result );
}

/*****************************************************************************/

static
int SaveCredentialsToKeychain(
	struct authcache_entry *entry_ptr,
	CFHTTPMessageRef request,
	int isProxy)
{
	OSStatus result;
	CFURLRef messageURL;
	CFStringRef theString;
	SecProtocolType protocol;
	SecAuthenticationType authenticationType;
	SecKeychainItemRef itemRef;
	int portNumber;
	char *serverName;
	char *realmStr;
	char *username;
	char *password;
	char *path;

	serverName = NULL;
	realmStr = NULL;
	username = NULL;
	password = NULL;
	path = NULL;
	
	/* get the URL */
	messageURL = CFHTTPMessageCopyRequestURL(request);
	require_action(messageURL != NULL, CFHTTPMessageCopyRequestURL, result = 1);
	
	/* get the realm (securityDomain) */
	theString = CFHTTPAuthenticationCopyRealm(entry_ptr->auth);
	if ( theString != NULL )
	{
		realmStr = CopyCFStringToCString(theString);
		CFRelease(theString);
	}
	
	/* get the accountName */
	username = CopyCFStringToCString(entry_ptr->username);
	require_action(username != NULL, CopyCFStringToCString, result = 1);
	
	/*
	 * If there's a domain and it isn't an empty string, then we have to combine
	 * the domain and username into a single string in the format:
	 *        domain "\" username
	 */
	if ( (entry_ptr->domain != NULL) && (CFStringGetLength(entry_ptr->domain) != 0) )
	{
		char *domain;
		char *temp;
		
		domain = CopyCFStringToCString(entry_ptr->domain);
		require_action(domain != NULL, CopyCFStringToCString, result = 1);
		
		temp = malloc(strlen(domain) + strlen(username) + 2);
		require_action(temp != NULL, malloc, result = 1);
		
		strcpy(temp, domain);
		free(domain);
		strcat(temp, "\\");
		strcat(temp, username);
		free(username);
		username = temp;
	}
	
	/* get the protocol type */
	theString = CFURLCopyScheme(messageURL);
	if ( CFEqual(theString, CFSTR("http")) )
	{
		if ( isProxy )
		{
			protocol = kSecProtocolTypeHTTPProxy;
		}
		else
		{
			protocol = kSecProtocolTypeHTTP;
		}
	}
	else if ( CFEqual(theString, CFSTR("https")) )
	{
		if ( isProxy )
		{
			protocol = kSecProtocolTypeHTTPSProxy;
		}
		else
		{
			protocol = kSecProtocolTypeHTTPS;
		}
	}
	else
	{
		protocol = 0;
	}
	CFRelease(theString);
	require_action(protocol != 0, unknown_protocol, result = 1);
	
	/* get the authentication method */
	theString = CFHTTPAuthenticationCopyMethod(entry_ptr->auth);
	if ( CFEqual(theString, CFSTR("Basic")) )
	{
		authenticationType = kSecAuthenticationTypeHTTPBasic;
	}
	else if ( CFEqual(theString, CFSTR("Digest")) )
	{
		authenticationType = kSecAuthenticationTypeHTTPDigest;
	}
	else if ( CFEqual(theString, CFSTR("NTLM")) )
	{
		authenticationType = kSecAuthenticationTypeNTLM;
	}
	else
	{
		authenticationType = kSecAuthenticationTypeDefault;
	}
	CFRelease(theString);
	
	/* get the password */
	password = CopyCFStringToCString(entry_ptr->password);
	require_action(username != NULL, CopyCFStringToCString, result = 1);
	
	if ( isProxy )
	{
		/* Proxy: Get the serverName and portNumber */
		int httpProxyEnabled;
		char *httpProxyServer;
		int httpProxyPort;
		int httpsProxyEnabled;
		char *httpsProxyServer;
		int httpsProxyPort;
		
		result = network_get_proxy_settings(&httpProxyEnabled, &httpProxyServer, &httpProxyPort,
			&httpsProxyEnabled, &httpsProxyServer, &httpsProxyPort);
		require_noerr_action_quiet(result, network_get_proxy_settings, result = 1);
		
		if ( protocol == kSecProtocolTypeHTTPProxy )
		{
			free(httpsProxyServer);
			serverName = httpProxyServer;
			portNumber = httpProxyPort;
		}
		else
		{
			free(httpProxyServer);
			serverName = httpsProxyServer;
			portNumber = httpsProxyPort;
		}
		
		/* find existing keychain item (if any) */
		result = SecKeychainFindInternetPassword(NULL,	/* default keychain */
			strlen(serverName), serverName,				/* serverName */
			0, NULL,									/* no securityDomain */
			0, NULL,									/* no accountName */
			0, NULL,									/* no path */
			portNumber,									/* port */
			protocol,									/* protocol */
			0,											/* no authenticationType */
			0, NULL,									/* no password */
			&itemRef);
	}
	else
	{
		/* Server: Get the path, serverName, and portNumber */
		
		/* get the path of the base URL (used because it needs to be unique for a mount point) */
		path = CopyComponentPathToCString(gBaseURL);
		
		/* get the server name and port number for the server */
		theString = CFURLCopyHostName(messageURL);
		require_action(theString != NULL, CFURLCopyHostName, result = 1);
		
		serverName = CopyCFStringToCString(theString);
		CFRelease(theString);
		
		portNumber = CFURLGetPortNumber(messageURL);
		if ( portNumber == -1 )
		{
			if ( protocol == kSecProtocolTypeHTTP )
			{
				portNumber = kHttpDefaultPort;
			}
			else if ( protocol == kSecProtocolTypeHTTPS )
			{
				portNumber = kHttpsDefaultPort;
			}
		}
		
		/* find existing keychain item (if any) */
		result = SecKeychainFindInternetPassword(NULL,	/* default keychain */
			strlen(serverName), serverName,				/* serverName */
			(realmStr != NULL) ? strlen(realmStr) : 0, realmStr,	/* securityDomain */
			strlen(username), username,					/* update the correct accountName */
			(path != NULL) ? strlen(path) : 0, path,	/* path */
			portNumber,									/* port */
			protocol,									/* protocol */
			authenticationType,							/* authenticationType */
			0, NULL,									/* no password */
			&itemRef);
	}
	
	if ( result == noErr )
	{
		/* update the existing item's accountName and password */
		SecKeychainAttribute attr;
		SecKeychainAttributeList attrList;
		
		/* the attribute we want is the account name */
		attr.tag = kSecAccountItemAttr;
		attr.length = strlen(username);
		attr.data = username;
		
		attrList.count = 1;
		attrList.attr = &attr;
		
		result = SecKeychainItemModifyContent(itemRef, &attrList, strlen(password), (void *)password);
		CFRelease(itemRef); /* done with itemRef either way */
		
		require_noerr(result, SecKeychainItemModifyContent);
	}
	else
	{
		/* otherwise, add new InternetPassword item */
		result = SecKeychainAddInternetPassword(NULL,
			strlen(serverName), serverName,			/* serverName */
			(realmStr != NULL) ? strlen(realmStr) : 0, realmStr, /* securityDomain */
			strlen(username), username,				/* accountName */
			(path != NULL) ? strlen(path) : 0, path, /* path */
			portNumber,								/* port */
			protocol,								/* protocol */
			authenticationType,						/* authenticationType */
			strlen(password), password,				/* password */
			&itemRef);
		require_noerr(result, SecKeychainAddInternetPassword);
		
		CFRelease(itemRef); /* we got itemRef so release it */
	}
	
	/* if it's now in the keychain, then future retrieves need to indicate that */
	/* indicate where the authentication came from */
	entry_ptr->authflags &= ~kAuthHasCredentials;
	entry_ptr->authflags |= kCredentialsFromKeychain;

SecKeychainAddInternetPassword:
SecKeychainItemModifyContent:
network_get_proxy_settings:
CopyCFStringToCString:
malloc:
CFURLCopyHostName:
unknown_protocol:
CFHTTPMessageCopyRequestURL:

	if ( serverName != NULL )
	{
		free(serverName);
	}
	if ( realmStr != NULL )
	{
		free(realmStr);
	}
	if ( username != NULL )
	{
		free(username);
	}
	if ( password != NULL )
	{
		free(password);
	}
	if ( path != NULL )
	{
		free(path);
	}
	if ( messageURL != NULL )
	{
		CFRelease(messageURL);
	}

	return ( result );
}

/*****************************************************************************/

static
int AddServerCredentials(CFHTTPMessageRef request)
{
	int result;
	/* locals for getting new values */
	CFStringRef username;
	CFStringRef password;
	CFStringRef domain;
	int secureAuth;
	
	username = password = domain = NULL;
	result = EACCES;

	if ( authcache_state == UNDEFINED_GUEST )
	{
		/* Try mount credentials (credentials passed to mount_webdav via the "-a" option) */
		if ( CopyMountCredentials(authcache_server_entry->auth, &username, &password, &domain, &secureAuth) == 0 )
		{
			syslog(LOG_DEBUG, "AddServerCred:UNDEFINED_GUEST: -> TRY_MOUNT_CRED, req %p", request);
				
			SetCredentials(authcache_server_entry, username, password, domain);
			authcache_state = TRY_MOUNT_CRED;
			++authcache_generation;
			if ( authcache_generation == 0 )
			{
				++authcache_generation;
			}
			result = 0;
		}
		else
		{
			syslog(LOG_DEBUG, "AddServerCred:UNDEFINED_GUEST: no mount creds, req %p", request);
		}
	}
			
	if ( (result != 0) && ( (authcache_state == UNDEFINED_GUEST) || (authcache_state == TRY_MOUNT_CRED)) )
	{
		/* try the keychain in theses states */
		if ( CopyCredentialsFromKeychain(authcache_server_entry->auth, request, &username, &password, &domain, FALSE, &secureAuth) == 0 )
		{
			syslog(LOG_DEBUG, "AddServerCred: state %d -> TRY_KEYCHAIN_CRED, req %p", authcache_state, request);
			ReleaseCredentials(authcache_server_entry);
			SetCredentials(authcache_server_entry, username, password, domain);
			authcache_state = TRY_KEYCHAIN_CRED;
			++authcache_generation;
			if ( authcache_generation == 0 )
			{
				++authcache_generation;
			}
			result = 0;
		}
		else
		{
			syslog(LOG_DEBUG, "AddServerCred: state %d, no keychain creds, req %p", authcache_state, request);
		}
	}
	
	/* try asking the user for credentials */
	if (result != 0)
	{
		int addtokeychain;
			
		/* put the last username, password, and domain used into the dialog */
		username = authcache_server_entry->username;
		password = authcache_server_entry->password;
		domain = authcache_server_entry->domain;
		
		if (authcache_state != TRY_UI_CRED)
		{
			syslog(LOG_DEBUG, "AddServerCred: state %d -> TRY_UI_CRED, req %p", authcache_state, request);
		}
		else
		{
			syslog(LOG_DEBUG, "AddServerCred:TRY_UI_CRED: prompting user for creds, req %p", request);
		}

		result = CopyCredentialsFromUserNotification(authcache_server_entry->auth, request,
					(authcache_state == TRY_UI_CRED), FALSE,
					&username, &password, &domain, &addtokeychain, &secureAuth);
			
		authcache_state = TRY_UI_CRED;
		++authcache_generation;
		if ( authcache_generation == 0 )
		{
			++authcache_generation;
		}
			
		if (result == 0)
		{
			syslog(LOG_DEBUG, "AddServerCred:TRY_UI_CRED: Got creds, retrying req %p", request);
			ReleaseCredentials(authcache_server_entry);
			SetCredentials(authcache_server_entry, username, password, domain);
			if ( addtokeychain )
			{
				authcache_server_entry->authflags |= kAddCredentialsToKeychain;
			}			
		}
		else if (result == ECANCELED)
		{
			/* The user hit the "Cancel" button.  The webDAV mount now becomes a GUEST USER mount */
			syslog(LOG_NOTICE, "Athentication cancelled by user action, mounting as guest user");
			authcache_state = GUEST_USER;
			++authcache_generation;
			if ( authcache_generation == 0 )
			{
				++authcache_generation;
			}
		
			ReleaseCredentials(authcache_server_entry);
			
			/* Free up the authcache_server_entry, we don't need it anymore */
			CFRelease(authcache_server_entry->auth);
			free (authcache_server_entry);
			authcache_server_entry = NULL;
			result = EACCES;
		}
		else
		{
			/* auth dialog timed out or an allocation failed */
			syslog(LOG_DEBUG, "AddServerCred:TRY_UI_CRED: UI timed out, returning EACCES, req %p", request);
			ReleaseCredentials(authcache_server_entry);
			result = EACCES;
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

	return ( result );
}

/*****************************************************************************/

static
int AddProxyCredentials(
	struct authcache_entry *entry_ptr,
	CFHTTPMessageRef request)
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
		if (entry_ptr->authflags & kCredentialsFromKeychain)
		{
			entry_ptr->authflags |= kNoKeychainCredentials;
		}
		
		/* if we haven't tried the keychain credentials, try them now */
		if ( (entry_ptr->authflags & kNoKeychainCredentials) == 0 )
		{
			if ( CopyCredentialsFromKeychain(entry_ptr->auth, request, &username, &password, &domain, TRUE, &secureAuth) == 0 ) 
			{
				ReleaseCredentials(entry_ptr);
				SetCredentials(entry_ptr, username, password, domain);
				entry_ptr->authflags &= ~kAuthHasCredentials;
				entry_ptr->authflags |= kCredentialsFromKeychain;
				result = 0;
			}
			else
			{
				/* there are no keychain credentials */
				entry_ptr->authflags |= kNoKeychainCredentials;
				result = EACCES;
			}
		}
		
		/* if we don't have credentials, try asking the user for them */
		if ( result != 0 )
		{
			int addtokeychain;
			
			/* put the last username, password, and domain used into the dialog */
			username = entry_ptr->username;
			password = entry_ptr->password;
			domain = entry_ptr->domain;
			if ( CopyCredentialsFromUserNotification(entry_ptr->auth, request,
				((entry_ptr->authflags & kCredentialsFromUI) != 0), TRUE,
				&username, &password, &domain, &addtokeychain, &secureAuth) == 0 )
			{
				ReleaseCredentials(entry_ptr);
				SetCredentials(entry_ptr, username, password, domain);
				entry_ptr->authflags &= ~kAuthHasCredentials;
				entry_ptr->authflags |= kCredentialsFromUI;
				if ( addtokeychain )
				{
					entry_ptr->authflags |= kAddCredentialsToKeychain;
				}
				result = 0;
			}
			else
			{
				ReleaseCredentials(entry_ptr);
				result = EACCES;
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
	CFHTTPMessageRef request)	/* -> the request message to apply authentication to */
{
	switch (authcache_state)
	{
		case UNDEFINED_GUEST:
			/* Check for a valid auth object (server could be negotiating an auth method) */
			if (authcache_server_entry != NULL)
			{
				if (CFHTTPAuthenticationIsValid(authcache_server_entry->auth, NULL))
				{
					syslog(LOG_DEBUG, "AddExistingAuth:UNDEFINED_GUEST: applying creds, req %p", request);
					ApplyCredentialsToRequest(authcache_server_entry, request);
				}
				else
				{
					syslog(LOG_DEBUG, "AddExistingAuth:UNDEFINED_GUEST: auth obj not valid, req %p", request);
				}
			}
			break;
		
		case TRY_MOUNT_CRED:
		case TRY_KEYCHAIN_CRED:
		case TRY_UI_CRED:
		case AUTHENTICATED_USER:
				if (CFHTTPAuthenticationIsValid(authcache_server_entry->auth, NULL))
				{
					ApplyCredentialsToRequest(authcache_server_entry, request);
				}
				else
				{
					syslog(LOG_DEBUG, "AddExistingAuth: state %d,  auth obj not valid, req %p", authcache_state, request);
				}
			break;
			
		case GUEST_USER:
			/* not an authenticated mount, no auth to apply */
			break;
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
 */
 static
int DoServerAuthentication(
	uid_t uid,							/* -> uid of the user making the request */
	CFHTTPMessageRef request,			/* -> the request message to apply authentication to */
	CFHTTPMessageRef response,			/* -> the response containing the challenge, or NULL if no challenge */
	AuthRequestContext *ctx)			/* <- the auth context for this request */
{
	CFHTTPAuthenticationRef auth;
	int result = 0;
	
	switch (authcache_state) {
	
		case UNDEFINED_GUEST:
			/* Create the authcache_server_entry if not already done */
			if ( authcache_server_entry == NULL)
			{
				syslog(LOG_DEBUG, "doServerAuth:UNDEFINED_GUEST: creating authcache_server_entry, req %p", request);
				
				/* Allocate */
				authcache_server_entry = calloc(sizeof(struct authcache_entry), 1);
				require(authcache_server_entry != NULL, calloc_entry);
			
				/* Initialize it */
				authcache_server_entry->uid = uid;
				authcache_server_entry->auth = CFHTTPAuthenticationCreateFromResponse(kCFAllocatorDefault, response);
				require(authcache_server_entry->auth != NULL, CFHTTPAuthenticationCreateFromResponse);
				require(CFHTTPAuthenticationIsValid(authcache_server_entry->auth, NULL), CFHTTPAuthenticationIsValid);
				
				/* Bump the generation since authcache_server_entry state changed */
				++authcache_generation;
				if ( authcache_generation == 0 )
					++authcache_generation;
					
				syslog(LOG_DEBUG, "doServerAuth:UNDEFINED_GUEST: authcache_server_entry created, req %p", request);
			}
			else
			{
				/* Check if response is stale */
				if (ctx->generation != authcache_generation)
				{
					/* stale response */
					syslog(LOG_DEBUG, "doServerAuth:UNDEFINED_GUEST: stale generation, req %p", request);
					result = 0;
					goto done;
				}
				
				/* Check if our auth object applies to this request. */
				if ( !CFHTTPAuthenticationAppliesToRequest(authcache_server_entry->auth, request) )
				{
					syslog(LOG_DEBUG, "doServerAuth:UNDEFINED_GUEST: auth doesn't apply to req %p", request);
					result = EACCES;
					goto done;
				}
			
				/* Make sure our auth object is still valid */
				if ( !CFHTTPAuthenticationIsValid(authcache_server_entry->auth, NULL) )
				{
					syslog(LOG_DEBUG, "doServerAuth:UNDEFINED_GUEST: authcache_server_entry not valid, req %p", request);
					
					/* Try and create a new auth object */
					auth = CFHTTPAuthenticationCreateFromResponse(kCFAllocatorDefault, response);
					if (auth == NULL) {
						syslog(LOG_DEBUG, "doServerAuth:UNDEFINED_GUEST: failed to create auth obj from response, req %p", request);
						result = EACCES;
						goto done;
					}
				
					if ( !CFHTTPAuthenticationIsValid(auth, NULL) )
					{
						syslog(LOG_DEBUG, "doServerAuth:UNDEFINED_GUEST: new auth obj not valid, req %p", request);
						CFRelease(auth);
						result = EACCES;
						goto done;
					}
			
					CFRelease(authcache_server_entry->auth);
					authcache_server_entry->auth = auth;
					
					/* bump the generation since the auth object just changed */
					++authcache_generation;
					if ( authcache_generation == 0 )
						++authcache_generation;
				}
			}
				
			/* Get credentials if needed */
			if ( CFHTTPAuthenticationRequiresUserNameAndPassword(authcache_server_entry->auth) )
			{
				syslog(LOG_DEBUG, "doServerAuth:UNDEFINED_GUEST: Adding Server Credentials, req %p", request);
				result = AddServerCredentials(request);
			}
			else
			{
				/* Credentials are not needed at this point, the server probably wants */
				/* to negotiate an auth method first */
				
				syslog(LOG_DEBUG, "doServerAuth:UNDEFINED_GUEST: Crendentials not needed, req %p", request);
				result = 0;
			}
			break;
				
		case TRY_MOUNT_CRED:
		case TRY_KEYCHAIN_CRED:
		case TRY_UI_CRED:
			/* Check if response is stale */
			if (ctx->generation != authcache_generation)
			{
				/* stale response */
				syslog(LOG_DEBUG, "webdav_fsagent:doServerAuth: state %d, stale generation, req %p", authcache_state, request);
				result = 0;
				goto done;
			}
			
			/* Check if our auth object applies to this request. */
			if ( !CFHTTPAuthenticationAppliesToRequest(authcache_server_entry->auth, request) )
			{
				syslog(LOG_DEBUG, "doServerAuth: state %d, auth doesn't apply to req %p", authcache_state, request);
				result = EACCES;
				goto done;
			}
			
			/* Make sure our auth object is still valid */
			if ( !CFHTTPAuthenticationIsValid(authcache_server_entry->auth, NULL) )
			{
				syslog(LOG_DEBUG, "doServerAuth: state %d, auth not valid, req %p", authcache_state, request);
				
				/* Try and create a new auth object */
				auth = CFHTTPAuthenticationCreateFromResponse(kCFAllocatorDefault, response);
				if (auth == NULL) {
					syslog(LOG_DEBUG, "doServerAuth: state %d, failed to create auth obj from response, req %p",
							authcache_state, request);
					result = EACCES;
					goto done;
				}
				
				if ( !CFHTTPAuthenticationIsValid(auth, NULL) )
				{
					syslog(LOG_DEBUG, "doServerAuth: state %d, new auth obj not valid, req %p", authcache_state, request);
					CFRelease(auth);
					result = EACCES;
					goto done;
				}
			
				CFRelease(authcache_server_entry->auth);
				authcache_server_entry->auth = auth;
			
				/* Bump generation since auth object changed */
				++authcache_generation;
				if ( authcache_generation == 0 )
					++authcache_generation;					
			}
			
			result = AddServerCredentials(request);
			break;
		case AUTHENTICATED_USER:
			/* Check if response is stale */
			if (ctx->generation != authcache_generation)
			{
				/* stale response */
				syslog(LOG_DEBUG, "doServerAuth:AUTHENTICATED_USER: stale generation, req %p", request);
				result = 0;
				goto done;
			}
			
			/* Check if our auth object applies to this request. */
			if ( !CFHTTPAuthenticationAppliesToRequest(authcache_server_entry->auth, request) )
			{
				syslog(LOG_DEBUG, "doServerAuth:AUTHENTICATED_USER: auth doesn't apply to req %p", request);
				result = EACCES;
				goto done;
			}
			
			/* Make sure our auth object is still valid */
			if ( !CFHTTPAuthenticationIsValid(authcache_server_entry->auth, NULL) )
			{
				syslog(LOG_DEBUG, "doServerAuth:AUTHENTICATED_USER: authcache_server_entry not valid, req %p", request);
				
				/* We don't have permission to access the resource.  Or there could be other reasons for the 401,  */
				/* such as a stale nonce and the server doesn't support the 'stale' directive. */
				/* Need to update our auth object in any case. */
				auth = CFHTTPAuthenticationCreateFromResponse(kCFAllocatorDefault, response);
				if (auth == NULL) {
					syslog(LOG_DEBUG, "doServerAuth:AUTHENTICATED_USER: failed to create auth obj from response, req %p", request);
					/* can only try again on the next request */
					result = EACCES;
					goto done;
				}
				
				if ( !CFHTTPAuthenticationIsValid(auth, NULL) )
				{
					syslog(LOG_DEBUG, "webdavfs_agent:doServerAuth:AUTHENTICATED_USER: new auth obj not valid, req %p", request);
					CFRelease(auth);
					result = EACCES;
					goto done;
				}
			
				CFRelease(authcache_server_entry->auth);
				authcache_server_entry->auth = auth;
			
				/* Bump generation since auth object changed */
				++authcache_generation;
				if ( authcache_generation == 0 )
					++authcache_generation;
			}
			
			if (ctx->count < MAX_AUTHENTICATED_USER_RETRIES)
			{
				/* can retry the request */
				ctx->count++;
				result = 0;
			}
			else
			{
				syslog(LOG_DEBUG, "doServerAuth:AUTHENTICATED_USER: to many auth retries for req %p", request);
				result = EACCES;
			}
			break;

		case GUEST_USER:
				/* Guest user is not authorized */
				syslog(LOG_DEBUG, "doServerAuth:GUEST: not authorized, req %p", request);
			result = EACCES;
			break;
	}

done:
	return (result);
	
CFHTTPAuthenticationIsValid:
	CFRelease(authcache_server_entry->auth);
	
CFHTTPAuthenticationCreateFromResponse:
	free (authcache_server_entry);
	authcache_server_entry = NULL;
	
calloc_entry:
	result = EACCES;
	return (result);
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
	AuthRequestContext *ctx)			/* <- the auth context for this request */
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
			result = DoServerAuthentication(uid, request, response, ctx);
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
		result = AddExistingAuthentications(request);
	}
	
	/* return the current authcache_generation */
	ctx->generation = authcache_generation;

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
	AuthRequestContext *ctx)			/* -> the auth context for this request */
{
	int result, result2;
	
	/* only validate authentications if the uid is the mount's user or root user */
	require_quiet(((gProcessUID == uid) || (0 == uid)), not_owner_uid);

	/* lock the Authcache */
	result = pthread_mutex_lock(&authcache_lock);
	require_noerr_action(result, pthread_mutex_lock, webdav_kill(-1));

	if ( ctx->generation == authcache_generation )
	{
		switch (authcache_state)
		{
			case UNDEFINED_GUEST:
			case AUTHENTICATED_USER:
			case GUEST_USER:
				/* nothing to do */
				break;
				
			case TRY_MOUNT_CRED:
			case TRY_KEYCHAIN_CRED:
			case TRY_UI_CRED:
				/* This mount is now authenticated  */
				syslog(LOG_NOTICE, "mounting as authenticated user");

				authcache_state = AUTHENTICATED_USER;
			
				/* bump generation since the state is changing */
				++authcache_generation;
				if ( authcache_generation == 0 )
				{
					++authcache_generation;
				}
			
				/* update keychain if needed */
				if ( authcache_server_entry->authflags & kAddCredentialsToKeychain )
				{
					authcache_server_entry->authflags &= ~kAddCredentialsToKeychain;
					result = SaveCredentialsToKeychain(authcache_server_entry, request, FALSE);
				}
				break;
		}
	
		if ( authcache_proxy_entry != NULL )
		{
			/* mark this authentication valid */
			authcache_proxy_entry->authflags |= kCredentialsValid;
			
			if ( authcache_proxy_entry->authflags & kAddCredentialsToKeychain )
			{
				authcache_proxy_entry->authflags &= ~kAddCredentialsToKeychain;
				result = SaveCredentialsToKeychain(authcache_proxy_entry, request, TRUE);
			}
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
	char *domain)				/* -> account domain to attempt to use on first server challenge, or NULL */
{
	int result;
	pthread_mutexattr_t mutexattr;
	
	/* set up the lock on the list */
	result = pthread_mutexattr_init(&mutexattr);
	require_noerr(result, pthread_mutexattr_init);
	
	result = pthread_mutex_init(&authcache_lock, &mutexattr);
	require_noerr(result, pthread_mutex_init);
	
	authcache_generation = 1;
	
	result = 0;
	
	if ( username != NULL && password != NULL && username[0] != '\0')
	{
		mount_username = CFStringCreateWithCString(kCFAllocatorDefault, username, kCFStringEncodingUTF8);
		require_action(mount_username != NULL, CFStringCreateWithCString, result = ENOMEM);
		
		mount_password = CFStringCreateWithCString(kCFAllocatorDefault, password, kCFStringEncodingUTF8);
		require_action(mount_password != NULL, CFStringCreateWithCString, result = ENOMEM);
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
