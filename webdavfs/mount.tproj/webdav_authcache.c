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
	u_int32_t authflags;		/* The keychain options for this authorization */
	u_int32_t generation;		/* the generation of authcache_entry */
};

/* authFlags */
enum
{
	/* No flags */
	kAuthNone					= 0x00000000,
	
	/* The credentials came from one of these sources */
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
static struct authcache_head authcache_list;			/* the list of authcache_entry for the server */
static u_int32_t authcache_generation = 1;				/* generation count of authcache_list (never zero)*/
static struct authcache_entry *authcache_proxy_entry = NULL;	/* the authcache_entry for the proxy server, or NULL */
static CFStringRef mount_username = NULL;
static CFStringRef mount_password = NULL;
static CFStringRef mount_domain = NULL;

/*****************************************************************************/

static
OSStatus KeychainItemCopyAccountPassword(
	SecKeychainItemRef itemRef,
	CFStringRef *username,
	CFStringRef *password,
	CFStringRef *domain);

/*****************************************************************************/

static
void LoginFailedWarning(void)
{
	SInt32 error;
	CFURLRef localizationPath;
	CFOptionFlags responseFlags;
	CFMutableDictionaryRef dictionary;
	CFUserNotificationRef userNotification;

	dictionary = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
		&kCFTypeDictionaryValueCallBacks);
	require(dictionary != NULL, CFDictionaryCreateMutable);

	localizationPath = CFURLCreateWithFileSystemPath(NULL, CFSTR(WEBDAV_LOCALIZATION_BUNDLE),
		kCFURLPOSIXPathStyle, TRUE);
	require(localizationPath != NULL, CFURLCreateWithFileSystemPath);

	CFDictionaryAddValue(dictionary, kCFUserNotificationLocalizationURLKey, localizationPath);
	
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
	CFRelease(localizationPath);
CFURLCreateWithFileSystemPath:
	CFRelease(dictionary);
CFDictionaryCreateMutable:

	return;
}

/*****************************************************************************/

static
int CopyCredentialsFromUserNotification(
	CFHTTPAuthenticationRef auth,	/* -> the authentication to get credentials for */
	CFHTTPMessageRef request,		/* -> the request message that was challenged */
	int badlogin,					/* -> if TRUE, the previous credentials retrieved from the user were not valid */
	CFStringRef *username,			/* <-> input: the previous username entered, or NULL; output: the username */
	CFStringRef *password,			/* <-> input: the previous password entered, or NULL; output: the password */
	CFStringRef *domain,			/* <-> input: the previous domain entered, or NULL; output: the domain, or NULL if authentication doesn't use domains */
	int *addtokeychain)				/* <- TRUE if the user wants these credentials added to their keychain */
/* IMPORTANT: if username, password, or domain values are passed in, webdav_get_authentication() releases them */
{
	int result;
    CFStringRef method;
	int secure;
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
		secure = TRUE;
	}
	else
	{
		/* the connection is not secure, so secure means "not Basic authentication" */
		method = CFHTTPAuthenticationCopyMethod(auth);
		if ( method != NULL )
		{
			secure = (CFStringCompare(method, CFSTR("Basic"), kCFCompareCaseInsensitive) != kCFCompareEqualTo);
			CFRelease(method);
		}
		else
		{
			secure = FALSE;
		}
	}
	
	/* get the url and realm strings */
	url = CFHTTPMessageCopyRequestURL(request);
	require(url != NULL, CFHTTPMessageCopyRequestURL);

	urlString = CFURLGetString(url);
	require(urlString != NULL, CFURLGetString);

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
	a[2] = secure ? CFSTR("WEBDAV_AUTH_MSG_SECURE_PARAMETER_KEY") : CFSTR("WEBDAV_AUTH_MSG_INSECURE_PARAMETER_KEY");
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

	/* if the UNC notification did not time out and the user clicked OK (default), then get their response */
	if ( (CFUserNotificationReceiveResponse(userNotification, WEBDAV_AUTHENTICATION_TIMEOUT, &responseFlags) == 0) &&
		((responseFlags & 3) == kCFUserNotificationDefaultResponse) )
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
	else
	{
        result = EACCES;
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
CFURLGetString:
	CFRelease(url);
CFHTTPMessageCopyRequestURL:
	
	return ( result );
}

/*****************************************************************************/

static
void RemoveAuthentication(struct authcache_entry *entry_ptr)
{
	/* all but the authcache_proxy_entry are in the authcache_list */
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
	}
	
	if ( entry_ptr->username != NULL )
	{
		CFRelease(entry_ptr->username);
	}
	
	if ( entry_ptr->password != NULL )
	{
		CFRelease(entry_ptr->password);
	}
	
	if ( entry_ptr->domain != NULL )
	{
		CFRelease(entry_ptr->domain);
	}
	
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
		converted = CFStringGetBytes(theString, range, kCFStringEncodingUTF8, 0, false, cstring, usedBufLen, &usedBufLen);
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
	CFStringRef *username,
	CFStringRef *password,
	CFStringRef *domain)
{
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
		
		return ( 0 );
	}
	else
	{
		return ( 1 );
	}
}

/*****************************************************************************/

static
int CopyCredentialsFromKeychain(
	CFHTTPAuthenticationRef auth,
	CFHTTPMessageRef request,
	CFStringRef *username,
	CFStringRef *password,
	CFStringRef *domain,
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
	
	serverName = NULL;
	realmStr = NULL;
	
	/* get the URL */
	messageURL = CFHTTPMessageCopyRequestURL(request);
	
	/* get the protocol type */
	theString = CFURLCopyScheme(messageURL);
	if ( CFEqual(theString, CFSTR("http")) )
	{
		protocol = kSecProtocolTypeHTTP;
	}
	else if ( CFEqual(theString, CFSTR("https")) )
	{
		protocol = kSecProtocolTypeHTTPS;
	}
	else
	{
		protocol = 0;
	}
	CFRelease(theString);
	
	if ( isProxy )
	{
		int httpProxyEnabled;
		char *httpProxyServer;
		int httpProxyPort;
		int httpsProxyEnabled;
		char *httpsProxyServer;
		int httpsProxyPort;
		
		authenticationType = 0;
		
		/* get the server name and port number for the proxy */
		require_action(protocol != 0, unknown_proxy_type, result = 1);
		
		result = network_get_proxy_settings(&httpProxyEnabled, &httpProxyServer, &httpProxyPort,
			&httpsProxyEnabled, &httpsProxyServer, &httpsProxyPort);
		require_noerr_quiet(result, network_get_proxy_settings);
		
		if ( protocol == kSecProtocolTypeHTTP )
		{
			serverName = httpProxyServer;
			portNumber = httpProxyPort;
		}
		else
		{
			serverName = httpsProxyServer;
			portNumber = httpsProxyPort;
		}
	}
	else
	{
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
		
		/* get the server name and port number for the server */
		theString = CFURLCopyHostName(messageURL);
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
	
		/* get the realm */
		theString = CFHTTPAuthenticationCopyRealm(auth);
		if ( theString != NULL )
		{
			realmStr = CopyCFStringToCString(theString);
			CFRelease(theString);
		}
	}
	
	result = SecKeychainFindInternetPassword(NULL,
		strlen(serverName), serverName,				/* serverName */
		(realmStr != NULL) ? strlen(realmStr) : 0, realmStr, /* securityDomain */
		0, NULL,									/* no accountName */
		0, NULL,									/* path */
		portNumber,									/* port */
		protocol,									/* protocol */
		authenticationType,							/* authType */
		0, NULL,									/* no password */
		&itemRef);
	if ( result == noErr )
	{
		result = KeychainItemCopyAccountPassword(itemRef, username, password, domain);
		CFRelease(itemRef);
	}

network_get_proxy_settings:
unknown_proxy_type:

	if ( serverName != NULL )
	{
		free(serverName);
	}
	if ( realmStr != NULL )
	{
		free(realmStr);
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

	
	serverName = NULL;
	realmStr = NULL;
	username = NULL;
	password = NULL;
	
	/* get the URL */
	messageURL = CFHTTPMessageCopyRequestURL(request);
	
	/* get the protocol type */
	theString = CFURLCopyScheme(messageURL);
	if ( CFEqual(theString, CFSTR("http")) )
	{
		protocol = kSecProtocolTypeHTTP;
	}
	else if ( CFEqual(theString, CFSTR("https")) )
	{
		protocol = kSecProtocolTypeHTTPS;
	}
	else
	{
		protocol = 0;
	}
	CFRelease(theString);
	
	if ( isProxy )
	{
		int httpProxyEnabled;
		char *httpProxyServer;
		int httpProxyPort;
		int httpsProxyEnabled;
		char *httpsProxyServer;
		int httpsProxyPort;
		
		authenticationType = 0;
		
		/* get the server name and port number for the proxy */
		require_action(protocol != 0, unknown_proxy_type, result = 1);
		
		result = network_get_proxy_settings(&httpProxyEnabled, &httpProxyServer, &httpProxyPort,
			&httpsProxyEnabled, &httpsProxyServer, &httpsProxyPort);
		require_noerr_quiet(result, network_get_proxy_settings);
		
		if ( protocol == kSecProtocolTypeHTTP )
		{
			serverName = httpProxyServer;
			portNumber = httpProxyPort;
		}
		else
		{
			serverName = httpsProxyServer;
			portNumber = httpsProxyPort;
		}
	}
	else
	{
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
		
		/* get the server name and port number for the server */
		theString = CFURLCopyHostName(messageURL);
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
		
		/* get the realm */
		theString = CFHTTPAuthenticationCopyRealm(entry_ptr->auth);
		if ( theString != NULL )
		{
			realmStr = CopyCFStringToCString(theString);
			CFRelease(theString);
		}
	}
	
	username = CopyCFStringToCString(entry_ptr->username);
	password = CopyCFStringToCString(entry_ptr->password);
	if ( entry_ptr->domain != NULL )
	{
		/*
		 * If there's a domain, then we have to combine the domain and username into a single string in the format:
		 *    domain "\" username
		 */
		char *domain;
		char *temp;
		
		domain = CopyCFStringToCString(entry_ptr->domain);
		temp = malloc(strlen(domain) + strlen(username) + 2);
		strcpy(temp, domain);
		free(domain);
		strcat(temp, "\\");
		strcat(temp, username);
		free(username);
		username = temp;
	}
	
	result = SecKeychainFindInternetPassword(NULL,
		strlen(serverName), serverName,				/* serverName */
		(realmStr != NULL) ? strlen(realmStr) : 0, realmStr, /* securityDomain */
		0, NULL,									/* no accountName */
		0, NULL,									/* path */
		portNumber,									/* port */
		protocol,									/* protocol */
		authenticationType,							/* authenticationType */
		0, NULL,									/* no password */
		&itemRef);
	if ( result == noErr )
	{
		/* update the current item */
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
		/* otherwise, add new InternetPassword */
		result = SecKeychainAddInternetPassword(NULL,
			strlen(serverName), serverName,			/* serverName */
			(realmStr != NULL) ? strlen(realmStr) : 0, realmStr, /* securityDomain */
			strlen(username), username,				/* accountName */
			0, NULL,								/* path */
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
	entry_ptr->authflags = kCredentialsFromKeychain;

SecKeychainAddInternetPassword:
SecKeychainItemModifyContent:
network_get_proxy_settings:
unknown_proxy_type:

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
	
	if ( CFHTTPAuthenticationRequiresUserNameAndPassword(entry_ptr->auth) )
	{
		username = password = domain = NULL;
		
		/* invalidate credential sources already tried */
		if (entry_ptr->authflags & kCredentialsFromMount)
		{
			entry_ptr->authflags |= kNoMountCredentials;
		}
		else if (entry_ptr->authflags & kCredentialsFromKeychain)
		{
			entry_ptr->authflags |= kNoKeychainCredentials;
		}
		entry_ptr->authflags &= ~kAuthHasCredentials;
		
		if ( !(entry_ptr->authflags & kNoMountCredentials) &&
			(CopyMountCredentials(&username, &password, &domain) == 0) )
		{
			ReleaseCredentials(entry_ptr);
			SetCredentials(entry_ptr, username, password, domain);
			entry_ptr->authflags |= kCredentialsFromMount;
			result = 0;
		}
		else if ( !(entry_ptr->authflags & kNoKeychainCredentials) &&
			(CopyCredentialsFromKeychain(entry_ptr->auth, request, &username, &password, &domain, FALSE) == 0) ) 
		{
			ReleaseCredentials(entry_ptr);
			SetCredentials(entry_ptr, username, password, domain);
			entry_ptr->authflags |= kCredentialsFromKeychain;
			result = 0;
		}
		else
		{
			int addtokeychain;
			
			/* put the last username, password, and domain used into the dialog */
			username = entry_ptr->username;
			password = entry_ptr->password;
			domain = entry_ptr->domain;
			if ( CopyCredentialsFromUserNotification(entry_ptr->auth, request,
				((entry_ptr->authflags & kCredentialsFromUI) != 0),
				&username, &password, &domain, &addtokeychain) == 0 )
			{
				ReleaseCredentials(entry_ptr);
				SetCredentials(entry_ptr, username, password, domain);
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
int AddProxyCredentials(
	struct authcache_entry *entry_ptr,
	CFHTTPMessageRef request)
{
	int result;
	/* locals for getting new values */
	CFStringRef username;
	CFStringRef password;
	CFStringRef domain;
	
	if ( CFHTTPAuthenticationRequiresUserNameAndPassword(entry_ptr->auth) )
	{
		username = password = domain = NULL;
		
		/* invalidate credential sources already tried */
		if (entry_ptr->authflags & kCredentialsFromKeychain)
		{
			entry_ptr->authflags |= kNoKeychainCredentials;
		}
		entry_ptr->authflags &= ~kAuthHasCredentials;
		
		if ( !(entry_ptr->authflags & kNoKeychainCredentials) &&
			(CopyCredentialsFromKeychain(entry_ptr->auth, request, &username, &password, &domain, TRUE) == 0) ) 
		{
			ReleaseCredentials(entry_ptr);
			SetCredentials(entry_ptr, username, password, domain);
			entry_ptr->authflags |= kCredentialsFromKeychain;
			result = 0;
		}
		else
		{
			int addtokeychain;
			
			/* put the last username, password, and domain used into the dialog */
			username = entry_ptr->username;
			password = entry_ptr->password;
			domain = entry_ptr->domain;
			if ( CopyCredentialsFromUserNotification(entry_ptr->auth, request,
				((entry_ptr->authflags & kCredentialsFromUI) != 0),
				&username, &password, &domain, &addtokeychain) == 0 )
			{
				ReleaseCredentials(entry_ptr);
				SetCredentials(entry_ptr, username, password, domain);
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
struct authcache_entry *CreateAuthenticationFromResponse(
	uid_t uid,							/* -> uid of the user making the request */
	CFHTTPMessageRef request,			/* -> the request message to apply authentication to */
	CFHTTPMessageRef response,			/* -> the response message  */
	int isProxy)						/* -> if TRUE, create authcache_proxy_entry */
{
	struct authcache_entry *entry_ptr;
	int result;
	
	entry_ptr = calloc(sizeof(struct authcache_entry), 1);
	require(entry_ptr != NULL, calloc);
	
	entry_ptr->uid = uid;
	entry_ptr->auth = CFHTTPAuthenticationCreateFromResponse(kCFAllocatorDefault, response);
	require(entry_ptr->auth != NULL, CFHTTPAuthenticationCreateFromResponse);
	
	require(CFHTTPAuthenticationIsValid(entry_ptr->auth, NULL), CFHTTPAuthenticationIsValid);
	
	if ( !isProxy )
	{
		result = AddServerCredentials(entry_ptr, request);
		require_noerr_quiet(result, AddServerCredentials);
		
		LIST_INSERT_HEAD(&authcache_list, entry_ptr, entries);
		++authcache_generation;
		if ( authcache_generation == 0 )
		{
			++authcache_generation;
		}
	}
	else
	{
		result = AddProxyCredentials(entry_ptr, request);
		require_noerr_quiet(result, AddProxyCredentials);
		
		authcache_proxy_entry = entry_ptr;
		++authcache_generation;
		if ( authcache_generation == 0 )
		{
			++authcache_generation;
		}
	}
	
	return ( entry_ptr );

AddProxyCredentials:
AddServerCredentials:
CFHTTPAuthenticationIsValid:

	CFRelease(entry_ptr->auth);

CFHTTPAuthenticationCreateFromResponse:

	free(entry_ptr);
	
calloc:

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

static
int ApplyCredentialsToRequest(
	struct authcache_entry *entry_ptr,
	CFHTTPMessageRef request)
{
	int result;
	CFMutableDictionaryRef dict;
	
	dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 3, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	require_action(dict != NULL, CFDictionaryCreateMutable, result = FALSE);
	
	if ( entry_ptr->username != NULL )
	{
		CFDictionaryAddValue(dict, kCFHTTPAuthenticationUsername, entry_ptr->username);
	}
	
	if ( entry_ptr->password != NULL )
	{
		CFDictionaryAddValue(dict, kCFHTTPAuthenticationPassword, entry_ptr->password);
	}
	
	if ( entry_ptr->domain != NULL )
	{
		CFDictionaryAddValue(dict, kCFHTTPAuthenticationAccountDomain, entry_ptr->domain);
	}
	
	result = CFHTTPMessageApplyCredentialDictionary(request, entry_ptr->auth, dict, NULL);
	
	CFRelease(dict);

CFDictionaryCreateMutable:
	
	return ( result );
}

/*****************************************************************************/

static
int AddExistingAuthentications(
	uid_t uid,							/* -> uid of the user making the request */
	CFHTTPMessageRef request)			/* -> the request message to apply authentication to */
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

static
int AddServerAuthentication(
	uid_t uid,							/* -> uid of the user making the request */
	CFHTTPMessageRef request,			/* -> the request message to apply authentication to */
	CFHTTPMessageRef response)			/* -> the response containing the challenge, or NULL if no challenge */
{
	int result;
	struct authcache_entry *entry_ptr;

	/* see if we already have a authcache_entry */
	entry_ptr = FindAuthenticationForRequest(uid, request);
	
	/* if we have one, we need to try to update it and use it */
	if ( entry_ptr != NULL )
	{
		entry_ptr->authflags &= ~kCredentialsValid;

		/* ensure the CFHTTPAuthenticationRef is valid */
		if ( CFHTTPAuthenticationIsValid(entry_ptr->auth, NULL) )
		{
			result = 0;
		}
		else
		{
			/* It's invalid so release the old and try to create a new one */
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
		
		if ( result == 0 )
		{
			if ( !ApplyCredentialsToRequest(entry_ptr, request) )
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
		entry_ptr = CreateAuthenticationFromResponse(uid, request, response, FALSE);
		if ( entry_ptr != NULL )
		{
			if ( ApplyCredentialsToRequest(entry_ptr, request) )
			{
				result = 0;
			}
			else
			{
				RemoveAuthentication(entry_ptr);
				result = EACCES;
			}
		}
		else
		{
			result = EACCES;
		}
	}
	
	return ( result );
}

/*****************************************************************************/

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
		
		if ( result == 0 )
		{
			if ( !ApplyCredentialsToRequest(authcache_proxy_entry, request) )
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
		authcache_proxy_entry = CreateAuthenticationFromResponse(uid, request, response, TRUE);
		if ( authcache_proxy_entry != NULL )
		{
			if ( ApplyCredentialsToRequest(authcache_proxy_entry, request) )
			{
				result = 0;
			}
			else
			{
				RemoveAuthentication(authcache_proxy_entry);
				result = EACCES;
			}
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
		/* no challenge -- add existing authentications */
		
		/* only apply existing authentications if the uid is the mount's user or root user */
		if ( (gProcessUID == uid) || (0 == uid) )
		{
			result = AddExistingAuthentications(uid, request);
		}
		else
		{
			result = 0;
		}
		break;
		
	case 401:
		/* server challenge -- add server authentication */
		
		/* only add server authentication if the uid is the mount's user or root user */
		if ( (gProcessUID == uid) || (0 == uid) )
		{
			result = AddServerAuthentication(uid, request, response);
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

		/* see if we have a authcache_entry */
		entry_ptr = FindAuthenticationForRequest(uid, request);
		if ( entry_ptr != NULL )
		{
			/* mark this authentication valid */
			entry_ptr->authflags |= kCredentialsValid;
			
			if ( entry_ptr->authflags & kAddCredentialsToKeychain )
			{
				entry_ptr->authflags &= ~kAddCredentialsToKeychain;
				result = SaveCredentialsToKeychain(entry_ptr, request, FALSE);
			}
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
