/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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

#include <stdlib.h>
#include <string.h>
#include "dsutil.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <CoreFoundation/CoreFoundation.h>
#include <DirectoryService/DirServicesConst.h>
#include "BSDDebugLog.h"

static void
_remove_key(CFMutableDictionaryRef dict, char *key)
{
	CFStringRef			keyRef = NULL;

	if (dict == NULL) return;
	if (key == NULL) return;

	keyRef = CFStringCreateWithCString( NULL, key, kCFStringEncodingUTF8 );

	if (keyRef == NULL) return;

	CFDictionaryRemoveValue( dict, keyRef );

	CFRelease( keyRef );
}

static CFStringRef
_value_for_key(CFMutableDictionaryRef dict, char *key)
{
	CFStringRef			valueRef = NULL;
	CFStringRef			keyRef = NULL;

	if (dict == NULL) return NULL;
	if (key == NULL) return NULL;

	keyRef = CFStringCreateWithCString( NULL, key, kCFStringEncodingUTF8 );

	if (keyRef == NULL) return NULL;

	valueRef = (CFStringRef)CFDictionaryGetValue( dict, keyRef );

	CFRelease( keyRef );
	
	return valueRef;
}

static void
_set_values_for_key(CFMutableDictionaryRef dict, char **values, char *key)
{
	CFMutableArrayRef	valuesRef = NULL;
	CFStringRef			keyRef = NULL;
	CFStringRef			tempRef = NULL;
	int					i;

	if (dict == NULL) return;
	if (values == NULL) return;
	if (key == NULL) return;
	if (values[0] == NULL) return;
	
	keyRef = CFStringCreateWithCString( NULL, key, kCFStringEncodingUTF8 );

	if (keyRef == NULL) return;

	valuesRef = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
	
	for (i = 0; values[i] != NULL; i++)
	{
		tempRef = CFStringCreateWithCString( NULL, values[i], kCFStringEncodingUTF8 );
		
		if ( tempRef )
		{
			CFArrayAppendValue( valuesRef, tempRef );
			CFRelease( tempRef );
		}
	}

	CFDictionarySetValue( dict, keyRef, valuesRef );

	CFRelease( keyRef );
	CFRelease( valuesRef );
}

static void
_add_value_for_key(CFMutableDictionaryRef dict, char *value, char *key)
{
	CFStringRef			valueRef = NULL;
	CFStringRef			keyRef = NULL;

	if (dict == NULL) return;
	if (value == NULL) return;
	if (key == NULL) return;

	keyRef = CFStringCreateWithCString( NULL, key, kCFStringEncodingUTF8 );

	if (keyRef == NULL) return;

	valueRef = CFStringCreateWithCString( NULL, value, kCFStringEncodingUTF8 );
	
	if (valueRef == NULL)
		valueRef = CFStringCreateWithCString( NULL, "", kCFStringEncodingUTF8 );	

	CFPropertyListRef	prevResultRef = (CFPropertyListRef)CFDictionaryGetValue( dict, keyRef );
	
	if ( prevResultRef && CFGetTypeID( prevResultRef ) == CFArrayGetTypeID() )
	{
		CFArrayAppendValue( (CFMutableArrayRef)prevResultRef, keyRef );	
	}
	else if ( prevResultRef && CFGetTypeID( prevResultRef ) == CFStringGetTypeID() )
	{
		CFMutableArrayRef newValuesRef = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
		
		CFArrayAppendValue( newValuesRef, prevResultRef );		// there was already one, make sure to include it
		CFArrayAppendValue( newValuesRef, keyRef );	
		CFDictionarySetValue( dict, keyRef, newValuesRef );
		CFRelease( newValuesRef );
	}
	else if ( !prevResultRef )
		CFDictionarySetValue( dict, keyRef, valueRef );

	CFRelease( keyRef );
	CFRelease( valueRef );
}

static void
_add_values_for_key(CFMutableDictionaryRef dict, char **values, char *key)
{
	CFMutableArrayRef	valuesRef = NULL;
	CFStringRef			keyRef = NULL;
	CFStringRef			tempRef = NULL;
	int					i;

	if (dict == NULL) return;
	if (values == NULL) return;
	if (key == NULL) return;
	if (values[0] == NULL) return;
	
	keyRef = CFStringCreateWithCString( NULL, key, kCFStringEncodingUTF8 );

	if (keyRef == NULL) return;

	CFPropertyListRef	resultRef = (CFPropertyListRef)CFDictionaryGetValue( dict, keyRef );
	
	if ( resultRef && CFGetTypeID( resultRef ) == CFArrayGetTypeID() )
		valuesRef = (CFMutableArrayRef)resultRef;
		
	if ( !valuesRef )
	{
		valuesRef = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
		
		if ( resultRef && CFGetTypeID( resultRef ) == CFStringGetTypeID() )
			CFArrayAppendValue( valuesRef, resultRef );		// there was only one, make sure to include it
	}
	else
		CFRetain( valuesRef );	// for later release
		
	for (i = 0; values[i] != NULL; i++)
	{
		tempRef = CFStringCreateWithCString( NULL, values[i], kCFStringEncodingUTF8 );
		
		if ( tempRef )
		{
			CFArrayAppendValue( valuesRef, tempRef );
			CFRelease( tempRef );
		}
	}

	CFDictionarySetValue( dict, keyRef, valuesRef );

	CFRelease( keyRef );
	CFRelease( valuesRef );
}

static void
_set_value_for_key(CFMutableDictionaryRef dict, char *value, char *key)
{
	CFStringRef			valueRef = NULL;
	CFStringRef			keyRef = NULL;

	if (dict == NULL) return;
//	if (value == NULL) return;
	if (key == NULL) return;

	keyRef = CFStringCreateWithCString( NULL, key, kCFStringEncodingUTF8 );

	if (keyRef == NULL) return;

	if ( value )
		valueRef = CFStringCreateWithCString( NULL, value, kCFStringEncodingUTF8 );
	
	if (valueRef == NULL)
		valueRef = CFStringCreateWithCString( NULL, "", kCFStringEncodingUTF8 );	

	CFDictionarySetValue( dict, keyRef, valueRef );

	CFRelease( keyRef );
	CFRelease( valueRef );
}

static void
_set_value_for_native_key(CFMutableDictionaryRef dict, char *value, char *key)
{
	char	modKey[1024] = {0,};
	
	while (isspace(*key)) key++;
	
	if ( key[0] != '\0' )
	{
		snprintf( modKey, sizeof(modKey), "%s%s", kDSNativeAttrTypePrefix, key );
		_set_value_for_key( dict, value, modKey );
	}
}

char **
ff_tokens_from_line(const char *data, const char *sep, int skip_comments)
{
	char **tokens = NULL;
	const char *p;
	int i, j, len;
	char buf[4096];
	int scanning;

	if (data == NULL) return NULL;
	if (sep == NULL)
	{
		tokens = appendString((char *)data, tokens);
		return tokens;
	}

	len = strlen(sep);

	p = data;

	while (p[0] != '\0')
	{
		/* skip leading white space */
		while ((p[0] == ' ') || (p[0] == '\t') || (p[0] == '\n')) p++;

		/* stop adding tokens at a # if skip_comments is set */
		if ((skip_comments != 0) && (p[0] == '#')) break;

		/* check for end of line */
		if (p[0] == '\0') break;

		/* copy data */
		i = 0;
		scanning = 1;
		for (j = 0; (j < len) && (scanning == 1); j++)
		{
			if (p[0] == sep[j] || (p[0] == '\0')) scanning = 0;
		}

		while (scanning == 1)
		{
			buf[i++] = p[0];
			p++;
			for (j = 0; (j < len) && (scanning == 1); j++)
			{
				if (p[0] == sep[j] || (p[0] == '\0')) scanning = 0;
			}
		}
	
		/* back over trailing whitespace */
		i--;
		while ((buf[i] == ' ') || (buf[i] == '\t') || (buf[i] == '\n')) i--;
		buf[++i] = '\0';
	
		tokens = appendString(buf, tokens);

		/* check for end of line */
		if (p[0] == '\0') break;

		/* skip separator */
		scanning = 1;
		for (j = 0; (j < len) && (scanning == 1); j++)
		{
			if (p[0] == sep[j])
			{
				p++;
				scanning = 0;
			}
		}

		if ((scanning == 0) && p[0] == '\0')
		{
			/* line ended at a separator - add a null member */
			tokens = appendString("", tokens);
			return tokens;
		}
	}
	return tokens;
}

char **
ff_netgroup_tokens_from_line(const char *data)
{
	char **tokens = NULL;
	const char *p;
	int i, j, len;
	char buf[4096], sep[3];
	int scanning, paren;

	if (data == NULL) return NULL;
	strcpy(sep," \t");
	len = 2;

	p = data;

	while (p[0] != '\0')
	{
		/* skip leading white space */
		while ((p[0] == ' ') || (p[0] == '\t') || (p[0] == '\n')) p++;

		/* check for end of line */
		if (p[0] == '\0') break;

		/* copy data */
		i = 0;
		scanning = 1;
		for (j = 0; (j < len) && (scanning == 1); j++)
		{
			if (p[0] == sep[j] || (p[0] == '\0')) scanning = 0;
		}

		paren = 0;
		if (p[0] == '(')
		{
			paren = 1;
			p++;
		}

		while (scanning == 1)
		{
			if (p[0] == '\0') return NULL;
			buf[i++] = p[0];
			p++;
			if (paren == 1)
			{
				if (p[0] == ')') scanning = 0;
			}
			else
			{
				for (j = 0; (j < len) && (scanning == 1); j++)
				{
					if ((p[0] == sep[j]) || (p[0] == '\0')) scanning = 0;
				}					
			}
		}

		if (paren == 1)
		{
			paren = 0;
			if (p[0] == ')') p++;
		}

		/* back over trailing whitespace */
		i--;
		while ((buf[i] == ' ') || (buf[i] == '\t') || (buf[i] == '\n')) i--;
		buf[++i] = '\0';
	
		tokens = appendString(buf, tokens);

		/* check for end of line */
		if (p[0] == '\0') break;

		/* skip separator */
		scanning = 1;
		for (j = 0; (j < len) && scanning; j++)
		{
			if (p[0] == sep[j])
			{
				p++;
				scanning = 0;
			}
		}
	}

	return tokens;
}

static CFMutableDictionaryRef
ff_parse_magic_cookie(char **tokens)
{
	freeList(tokens);
	tokens = NULL;
	return NULL;
}

CFMutableDictionaryRef
ff_parse_user_A(char *data)
{
	CFMutableDictionaryRef itemRef = NULL;
	char **tokens;

	if (data == NULL) return NULL;

	tokens = ff_tokens_from_line(data, ":", 0);
	if (listLength(tokens) == 0)
	{
		freeList(tokens);
		return NULL;
	}

	if (tokens[0][0] == '+')
	{
		return ff_parse_magic_cookie(tokens);
	}

	if (listLength(tokens) != 10)
	{
		freeList(tokens);
		return NULL;
	}

	itemRef = CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

	CFDictionarySetValue( itemRef, CFSTR(kDSNAttrRecordType), CFSTR(kDSStdRecordTypeUsers) );

	_set_value_for_key(itemRef, tokens[0], kDSNAttrRecordName);
	_set_value_for_key(itemRef, tokens[1], kDS1AttrPassword);
//	_set_value_for_key(itemRef, tokens[1], kDS1AttrPasswordPlus);		// also set the password plus to the same crypt password
	_set_value_for_key(itemRef, tokens[2], kDS1AttrUniqueID);
	_set_value_for_key(itemRef, tokens[3], kDS1AttrPrimaryGroupID);
//	_set_value_for_key(itemRef, tokens[4], "dsAttrTypeNative:class");
	_set_value_for_key(itemRef, tokens[5], kDS1AttrChange);
	_set_value_for_key(itemRef, tokens[6], kDS1AttrExpire);
	_set_value_for_key(itemRef, tokens[7], kDS1AttrDistinguishedName);
	_set_value_for_key(itemRef, tokens[8], kDS1AttrNFSHomeDirectory);
	
	if ( tokens[9] != '\0' )
		_set_value_for_key(itemRef, tokens[9], kDS1AttrUserShell);
	else
		_set_value_for_key(itemRef, "/bin/sh", kDS1AttrUserShell);	// according to man(5) passwd, if its null, use the bourne shell
		
	freeList(tokens);
	tokens = NULL;

	return itemRef;
}

CFMutableDictionaryRef
ff_parse_user(char *data)
{
	/* For compatibility with YP, support 4.3 style passwd files. */
	
	CFMutableDictionaryRef itemRef;
	char **tokens;

	if (data == NULL) return NULL;

	tokens = ff_tokens_from_line(data, ":", 0);
	if (listLength(tokens) == 0)
	{
		freeList(tokens);
		return NULL;
	}

	if (tokens[0][0] == '+')
	{
		return ff_parse_magic_cookie(tokens);
	}

	if (listLength(tokens) != 7)
	{
		freeList(tokens);
		return NULL;
	}

	itemRef = CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

	CFDictionarySetValue( itemRef, CFSTR(kDSNAttrRecordType), CFSTR(kDSStdRecordTypeUsers) );
	
	_set_value_for_key(itemRef, tokens[0], kDSNAttrRecordName);
	_set_value_for_key(itemRef, tokens[1], kDS1AttrPassword);
//	_set_value_for_key(itemRef, tokens[1], kDS1AttrPasswordPlus);		// also set the password plus to the same crypt password
	_set_value_for_key(itemRef, tokens[2], kDS1AttrUniqueID);
	_set_value_for_key(itemRef, tokens[3], kDS1AttrPrimaryGroupID);
	_set_value_for_key(itemRef, tokens[4], kDS1AttrDistinguishedName);
	_set_value_for_key(itemRef, tokens[5], kDS1AttrNFSHomeDirectory);
	_set_value_for_key(itemRef, tokens[6], kDS1AttrUserShell);

	freeList(tokens);
	tokens = NULL;

	return itemRef;
}

CFMutableDictionaryRef
ff_parse_group(char *data)
{
	CFMutableDictionaryRef itemRef;
	char **users;
	char **tokens;

	if (data == NULL) return NULL;

	tokens = ff_tokens_from_line(data, ":", 0);
	if (listLength(tokens) == 0)
	{
		freeList(tokens);
		return NULL;
	}

	if (tokens[0][0] == '+')
	{
		return ff_parse_magic_cookie(tokens);
	}

	if (listLength(tokens) < 3)
	{
		freeList(tokens);
		return NULL;
	}

	itemRef = CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

	CFDictionarySetValue( itemRef, CFSTR(kDSNAttrRecordType), CFSTR(kDSStdRecordTypeGroups) );
	
	_set_value_for_key(itemRef, tokens[0], kDSNAttrRecordName);
	_set_value_for_key(itemRef, tokens[1], kDS1AttrPassword);
//	_set_value_for_key(itemRef, tokens[1], kDS1AttrPasswordPlus);		// also set the password plus to the same crypt password
	_set_value_for_key(itemRef, tokens[2], kDS1AttrPrimaryGroupID);

	if (listLength(tokens) < 4)
	{
		_set_value_for_key(itemRef, "", kDSNAttrGroupMembership);
	}
	else
	{
		users = ff_tokens_from_line(tokens[3], ",", 0);
		_set_values_for_key(itemRef, users, kDSNAttrGroupMembership);
		freeList(users);
		users = NULL;
	}

	freeList(tokens);
	tokens = NULL;

	return itemRef;
}

CFMutableDictionaryRef
ff_parse_host(char *data)
{
	CFMutableDictionaryRef itemRef;
	char **tokens;
	int len, af;
	struct in_addr a4;
	struct in6_addr a6;
	char paddr[64];
	void *saddr = NULL;

	if (data == NULL) return NULL;

	tokens = ff_tokens_from_line(data, " \t", 1);
	len = listLength(tokens);
	if (len < 2)
	{
		freeList(tokens);
		return NULL;
	}

	af = AF_UNSPEC;
	if (inet_aton(tokens[0], &a4) == 1)
	{
		af = AF_INET;
		saddr = &a4;
	}
	else if (inet_pton(AF_INET6, tokens[0], &a6) == 1)
	{
		af = AF_INET6;
		saddr = &a6;
	}

	if (af == AF_UNSPEC)
	{
		freeList(tokens);
		return NULL;
	}

	/* We use inet_pton to convert to a canonical form */
	if (inet_ntop(af, saddr, paddr, 64) == NULL)
	{
		freeList(tokens);
		return NULL;
	}

	itemRef = CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

	CFDictionarySetValue( itemRef, CFSTR(kDSNAttrRecordType), CFSTR(kDSStdRecordTypeHosts) );
	
	if (af == AF_INET) _set_value_for_key(itemRef, paddr, kDSNAttrIPAddress);
	else _set_value_for_key(itemRef, paddr, "dsAttrTypeNative:ipv6_address");

	_set_values_for_key(itemRef, tokens+1, kDSNAttrRecordName);

	freeList(tokens);
	tokens = NULL;

	return itemRef;
}

static CFMutableDictionaryRef
ff_parse_nna(char *data, char *aKey)
{
	CFMutableDictionaryRef itemRef;
	char **tokens;
	int len;

	if (data == NULL) return NULL;

	tokens = ff_tokens_from_line(data, " \t", 1);
	len = listLength(tokens);
	if (len < 2)
	{
		freeList(tokens);
		return NULL;
	}

	itemRef = CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

	_set_value_for_key(itemRef, tokens[0], kDSNAttrRecordName);
	_set_value_for_key(itemRef, tokens[1], aKey);
	
	_add_values_for_key(itemRef, tokens+2, kDSNAttrRecordName);

	freeList(tokens);
	tokens = NULL;

	return itemRef;
}

CFMutableDictionaryRef
ff_parse_network(char *data)
{
	CFMutableDictionaryRef itemRef = ff_parse_nna(data, "dsAttrTypeNative:address");
	
	if ( itemRef )
		CFDictionarySetValue( itemRef, CFSTR(kDSNAttrRecordType), CFSTR("dsRecTypeNative:address") );
	else
		DBGLOG( "ff_parse_network is returning NULL since ff_parse_nna returned no results\n" );
		
	return itemRef;
}

CFMutableDictionaryRef
ff_parse_service(char *data)
{
	CFMutableDictionaryRef itemRef;
	char 	*port;
	char 	*proto;
	char	tempBuf[1024];
	char 	*pp = tempBuf;
	CFIndex	ppLen = sizeof(tempBuf);

	itemRef = ff_parse_nna(data, "dsAttrTypeNative:protport");
	if (itemRef == NULL) return NULL;

	CFStringRef ppRef = _value_for_key(itemRef, "dsAttrTypeNative:protport");
	if (ppRef == NULL)
	{
		CFRelease(itemRef);
		return NULL;
	}
	
	if ( ::CFStringGetMaximumSizeForEncoding( CFStringGetLength(ppRef), kCFStringEncodingUTF8) +1 > (CFIndex)sizeof(tempBuf) )
	{
		ppLen = ::CFStringGetMaximumSizeForEncoding( CFStringGetLength(ppRef), kCFStringEncodingUTF8) +1;
		pp = (char*)malloc( ppLen );
	}
		
	::CFStringGetCString( ppRef, pp, ppLen, kCFStringEncodingUTF8 );
	
	port = prefix(pp, '/');
	if (port == NULL)
	{
		free(pp);
		CFRelease(itemRef);
		return NULL;
	}

	proto = postfix(pp, '/');
	
	if ( pp != tempBuf )
		free(pp);
		
	if (proto == NULL)
	{
		freeString(port);
		port = NULL;
		CFRelease(itemRef);
		return NULL;
	}

	CFDictionarySetValue( itemRef, CFSTR(kDSNAttrRecordType), CFSTR(kDSStdRecordTypeServices) );
	
	_set_value_for_key(itemRef, port, kDS1AttrPort);
//	_set_value_for_key(itemRef, port, "dsAttrTypeNative:port");
	_set_value_for_key(itemRef, proto, kDSNAttrProtocols);
	_set_value_for_key(itemRef, proto, "dsAttrTypeNative:protocol");
	freeString(port);
	port = NULL;
	freeString(proto);
	proto = NULL;

	_remove_key(itemRef, "dsAttrTypeNative:protport");

	return itemRef;
}

CFMutableDictionaryRef
ff_parse_protocol(char *data)
{
	CFMutableDictionaryRef itemRef = ff_parse_nna(data, "dsAttrTypeNative:number");
	
	CFDictionarySetValue( itemRef, CFSTR(kDSNAttrRecordType), CFSTR(kDSStdRecordTypeProtocols) );
	
	return itemRef;
}

CFMutableDictionaryRef
ff_parse_rpc(char *data)
{
	CFMutableDictionaryRef itemRef = ff_parse_nna(data, "dsAttrTypeNative:number");
	
	CFDictionarySetValue( itemRef, CFSTR(kDSNAttrRecordType), CFSTR(kDSStdRecordTypeRPC) );
	
	return itemRef;
}

CFMutableDictionaryRef
ff_parse_mount(char *data)
{
	CFMutableDictionaryRef itemRef;
	char **val;
	char **tokens;

	if (data == NULL) return NULL;

	tokens = ff_tokens_from_line(data, " \t", 0);
	if (listLength(tokens) < 6)
	{
		freeList(tokens);
		return NULL;
	}

	itemRef = CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

	CFDictionarySetValue( itemRef, CFSTR(kDSNAttrRecordType), CFSTR(kDSStdRecordTypeMounts) );

	_set_value_for_key(itemRef, tokens[0], kDSNAttrRecordName);
	_set_value_for_key(itemRef, tokens[1], kDS1AttrVFSLinkDir);
	_set_value_for_key(itemRef, tokens[2], kDS1AttrVFSType);

	val = ff_tokens_from_line(tokens[3], ",", 0);
	_set_values_for_key(itemRef, val, kDSNAttrVFSOpts);

	freeList(val);
	val = NULL;

	_set_value_for_key(itemRef, tokens[4], kDS1AttrVFSDumpFreq);
	_set_value_for_key(itemRef, tokens[5], kDS1AttrVFSPassNo);
	
	// make up a url for this as well since clients can browse for nfs..

	freeList(tokens);
	tokens = NULL;

	return itemRef;
}

static CFMutableDictionaryRef
ff_parse_pb(char *data, char c)
{
	char **options;
	char **opt;
	char t[2];
	int i, len;
	CFMutableDictionaryRef itemRef;

	if (data == NULL) return NULL;

	itemRef = CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

	t[0] = c;
	t[1] = '\0';
	options = explode(data, t);

	len = listLength(options);
	if (len < 1)
	{
		freeList(options);
		return NULL;
	}

	_set_value_for_key(itemRef, options[0], kDSNAttrRecordName);

	for (i = 1; i < len; i++)
	{
		opt = explode(options[i], "=");
		if (listLength(opt) == 2) _set_value_for_native_key(itemRef, opt[1], opt[0]);
		else if ( listLength(opt) == 1 ) _set_value_for_native_key(itemRef, NULL, opt[0]);	// set the key with an empty value ("")
		freeList(opt);
		opt = NULL;
	}

	freeList(options);
	options = NULL;

	return itemRef;
}

CFMutableDictionaryRef
ff_parse_printer(char *data)
{
	CFMutableDictionaryRef itemRef = ff_parse_pb(data, ':');
	
	CFDictionarySetValue( itemRef, CFSTR(kDSNAttrRecordType), CFSTR(kDSStdRecordTypePrintService) );
	
	return itemRef;
}

CFMutableDictionaryRef
ff_parse_bootparam(char *data)
{
	CFMutableDictionaryRef itemRef = ff_parse_pb(data, '\t');
	
	CFDictionarySetValue( itemRef, CFSTR(kDSNAttrRecordType), CFSTR(kDSNAttrBootParams) );
	
	return itemRef;
}

CFMutableDictionaryRef
ff_parse_bootp(char *data)
{
	CFMutableDictionaryRef itemRef = ff_parse_pb(data, ':');
	
	CFDictionarySetValue( itemRef, CFSTR(kDSNAttrRecordType), CFSTR(kDSStdRecordTypeBootp) );
/*	char **tokens;

	if (data == NULL) return NULL;

//	tokens = ff_tokens_from_line(data, " \t", 0);
	if (listLength(tokens) < 5)
	{
		freeList(tokens);
		return NULL;
	}

	itemRef = CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

	CFDictionarySetValue( itemRef, CFSTR(kDSNAttrRecordType), CFSTR(kDSStdRecordTypeBootp) );
	
	_set_value_for_key(itemRef, tokens[0], kDSNAttrRecordName);
	_set_value_for_key(itemRef, tokens[1], "dsAttrTypeNative:htype");
	_set_value_for_key(itemRef, tokens[2], kDS1AttrENetAddress);
	_set_value_for_key(itemRef, tokens[3], kDSNAttrIPAddress);
	_set_value_for_key(itemRef, tokens[4], "dsAttrTypeNative:bootfile");

	freeList(tokens);
	tokens = NULL;
*/
	return itemRef;
}

CFMutableDictionaryRef
ff_parse_alias(char *data)
{
	CFMutableDictionaryRef itemRef;
	char **members;
	char **tokens;

	if (data == NULL) return NULL;

	tokens = ff_tokens_from_line(data, ":", 0);
	if (listLength(tokens) < 2)
	{
		freeList(tokens);
		return NULL;
	}

	itemRef = CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

	CFDictionarySetValue( itemRef, CFSTR(kDSNAttrRecordType), CFSTR(kDSNAttrRecordAlias) );
	
	_set_value_for_key(itemRef, tokens[0], kDSNAttrRecordName);

	members = ff_tokens_from_line(tokens[1], ",", 0);
	_set_values_for_key(itemRef, members, "dsAttrTypeNative:members");

	freeList(members);
	members = NULL;

	freeList(tokens);
	tokens = NULL;

	return itemRef;
}

CFMutableDictionaryRef
ff_parse_ethernet(char *data)
{
	CFMutableDictionaryRef itemRef;
	char **tokens;

	if (data == NULL) return NULL;

	tokens = ff_tokens_from_line(data, " \t", 1);
	if (listLength(tokens) < 2)
	{
		freeList(tokens);
		return NULL;
	}

	itemRef = CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

	CFDictionarySetValue( itemRef, CFSTR(kDSNAttrRecordType), CFSTR(kDSStdRecordTypeEthernets) );
	
	_set_value_for_key(itemRef, tokens[0], kDS1AttrENetAddress);
	_set_value_for_key(itemRef, tokens[1], kDSNAttrRecordName);

	freeList(tokens);
	tokens = NULL;

	return itemRef;
}

CFMutableDictionaryRef
ff_parse_netgroup(char *data)
{
	CFMutableDictionaryRef itemRef;
	char **val;
	char **tokens;
	int i, len;

	if (data == NULL) return NULL;

	tokens = ff_netgroup_tokens_from_line(data);
	if (tokens == NULL) return NULL;

	len = listLength(tokens);
	if (len < 1)
	{
		freeList(tokens);
		return NULL;
	}

	itemRef = CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

	CFDictionarySetValue( itemRef, CFSTR(kDSNAttrRecordType), CFSTR(kDSStdRecordTypeNetGroups) );
	
	_set_value_for_key(itemRef, tokens[0], kDSNAttrRecordName);

	for (i = 1; i < len; i++)
	{
		val = ff_tokens_from_line(tokens[i], ",", 0);
		if (listLength(val) == 1)
		{
			_add_value_for_key(itemRef, val[0], kDSNAttrNetGroups);
			freeList(val);
			val = NULL;
			continue;
		}

		if (listLength(val) != 3)
		{
			CFRelease(itemRef);
			freeList(tokens);
			tokens = NULL;
			freeList(val);
			val = NULL;
			return NULL;
		}

		if (val[0][0] != '\0') _add_value_for_key(itemRef, val[0], kDSStdRecordTypeHosts);
		if (val[1][0] != '\0') _add_value_for_key(itemRef, val[1], kDSStdRecordTypeUsers);
		if (val[2][0] != '\0') _add_value_for_key(itemRef, val[2], "dsAttrTypeNative:domains");

		freeList(val);
		val = NULL;
	}

	freeList(tokens);
	tokens = NULL;

	return itemRef;
}
