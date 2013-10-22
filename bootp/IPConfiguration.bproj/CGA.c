/*
 * Copyright (c) 2013 Apple Inc. All rights reserved.
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

/*
 * CGA.c
 * - CGA (Cryptographically Generated Addresses) support routines
 */

/* 
 * Modification History
 *
 * April 11, 2013 	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 */
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <SystemConfiguration/SCValidation.h>
#include "symbol_scope.h"
#include "CGA.h"
#include "RSAKey.h"
#include "ipconfigd_globals.h"
#include "mylog.h"
#include "util.h"
#include "cfutil.h"
#include "globals.h"
#include "HostUUID.h"

#define CGA_KEYS_FILE	IPCONFIGURATION_PRIVATE_DIR "/CGAKeys.plist"
#define CGA_FILE	IPCONFIGURATION_PRIVATE_DIR "/CGA.plist"

/*
 * CGA.plist schema
 */
#define kHostUUID		CFSTR("HostUUID")		/* data */
#define kGlobalModifier		CFSTR("GlobalModifier")		/* dict */
#define kLinkLocalModifiers	CFSTR("LinkLocalModifiers")	/* dict[dict] */

/*
 * CGAKeys.plist schema
 */
#define kPrivateKey		CFSTR("PrivateKey")		/* data */
#define kPublicKey		CFSTR("PublicKey")		/* data */


#define kCGASecurityLevelZero	(0)

/*
 * modifier dictionary schema
 */
#define kModifier		CFSTR("Modifier")
#define kSecurityLevel		CFSTR("SecurityLevel")
#define kCreationDate		CFSTR("CreationDate")

#define knet_inet6_send_cga_parameters	"net.inet6.send.cga_parameters"
#define knet_inet6_send_opmode		"net.inet6.send.opmode"

#define CGA_PARAMETERS_MAX_SIZE				\
    (sizeof(struct in6_cga_prepare)			\
     + (2 * (sizeof(uint16_t) + IN6_CGA_KEY_MAXSIZE)))

#define SECS_PER_HOUR				(3600)
#define LINKLOCAL_MODIFIER_EXPIRATION_SECONDS	(SECS_PER_HOUR * 24)

STATIC CFDictionaryRef		S_GlobalModifier;
STATIC CFMutableDictionaryRef	S_LinkLocalModifiers;

STATIC CFDataRef
CGAModifierDictGetModifier(CFDictionaryRef dict, uint8_t * security_level);

STATIC CFDataRef
my_CFDataCreateWithRandomBytes(CFIndex size)
{
    CFMutableDataRef	data;

    data = CFDataCreateMutable(NULL, size);
    CFDataSetLength(data, size);
    fill_with_random(CFDataGetMutableBytePtr(data), size);
    return (data);
}

STATIC bool
linklocal_modifier_has_expired(CFDictionaryRef dict, CFDateRef now)
{
    bool	has_expired = TRUE;
    CFDataRef	modifier;
    uint8_t	security_level;

    modifier = CGAModifierDictGetModifier(dict, &security_level);
    if (modifier != NULL) {
	CFDateRef	creation_date;
	
	creation_date = CFDictionaryGetValue(dict, kCreationDate);
	if (isA_CFDate(creation_date) != NULL
	    && (CFDateGetTimeIntervalSinceDate(now, creation_date)
		< LINKLOCAL_MODIFIER_EXPIRATION_SECONDS)) {
	    has_expired = FALSE;
	}
    }
    return (has_expired);
}

STATIC void
remove_old_linklocal_modifiers(CFMutableDictionaryRef modifiers)
{
    int			count;

    count = CFDictionaryGetCount(modifiers);
    if (count > 0) {
	int		i;
	const void *	keys[count];
	CFDateRef	now;
	const void *	values[count];

	now = CFDateCreate(NULL, CFAbsoluteTimeGetCurrent());
	CFDictionaryGetKeysAndValues(modifiers, keys, values);
	for (i = 0; i < count; i++) {
	    CFDictionaryRef	dict = (CFDictionaryRef)values[i];

	    if (linklocal_modifier_has_expired(dict, now)) {
		CFStringRef	key = (CFStringRef)keys[i];

		CFDictionaryRemoveValue(modifiers, key);
	    }
	}
	CFRelease(now);
    }
    return;
}

STATIC void
cga_prepare_set(struct in6_cga_prepare * cga_prep,
		CFDataRef modifier, uint8_t security_level)
{
    CFDataGetBytes(modifier,
		   CFRangeMake(0, CFDataGetLength(modifier)),
		   cga_prep->cga_modifier.octets);
    cga_prep->cga_security_level = security_level;
    bzero(cga_prep->reserved_A, sizeof(cga_prep->reserved_A));
    return;
}

STATIC bool
cga_parameters_set(CFDataRef priv, CFDataRef pub, 
		   CFDataRef modifier, uint8_t security_level)
{
    UInt8 			buf[CGA_PARAMETERS_MAX_SIZE];
    uint16_t			key_len;
    UInt8 *			offset;

    offset = buf;

    /* cga_prepare */
    cga_prepare_set((struct in6_cga_prepare *)offset, modifier, security_level);
    offset += sizeof(struct in6_cga_prepare);

    /* private key length (uint16_t) */
    key_len = CFDataGetLength(priv);
    bcopy(&key_len, offset, sizeof(key_len));
    offset += sizeof(key_len);

    /* private key */
    CFDataGetBytes(priv, CFRangeMake(0, key_len), offset);
    offset += key_len;
    
    /* public key length (uint16_t) */
    key_len = CFDataGetLength(pub);
    bcopy(&key_len, offset, sizeof(key_len));
    offset += sizeof(key_len);

    /* public key */
    CFDataGetBytes(pub, CFRangeMake(0, key_len), offset);
    offset += key_len;

    if (sysctlbyname(knet_inet6_send_cga_parameters,
		     NULL, NULL, buf, (offset - buf)) != 0) {
	my_log_fl(LOG_ERR, "sysctl(%s) failed, %s",
		  knet_inet6_send_cga_parameters,
		  strerror(errno));
	return (FALSE);
    }
    return (TRUE);
}

STATIC bool
cga_is_enabled(void)
{
    int		enabled;
    size_t	enabled_size = sizeof(enabled);

    if (sysctlbyname(knet_inet6_send_opmode,
		     &enabled, &enabled_size, NULL, 0) != 0) {
	my_log_fl(LOG_ERR, "sysctl(%s) failed, %s",
		  knet_inet6_send_opmode,
		  strerror(errno));
	enabled = 0;
    }
    return (enabled != 0);
}


STATIC CFDataRef
CGAModifierDictGetModifier(CFDictionaryRef dict, uint8_t * security_level)
{
    CFDataRef	modifier = NULL;

    *security_level = 0;
    if (isA_CFDictionary(dict) != NULL) {
	modifier = CFDictionaryGetValue(dict, kModifier);
	modifier = isA_CFData(modifier);
	if (modifier != NULL) {
	    if (CFDataGetLength(modifier) != IN6_CGA_MODIFIER_LENGTH) { 
		modifier = NULL;
	    }
	    else {
		CFNumberRef	level;

		level = CFDictionaryGetValue(dict, kSecurityLevel);
		if (isA_CFNumber(level) != NULL) {
		    CFNumberGetValue(level, kCFNumberSInt8Type, security_level);
		}
	    }
	}
    }
    return (modifier);
}

STATIC CFDictionaryRef
modifier_dict_create(CFDataRef modifier, CFNumberRef sec_level,
		     CFDateRef now)
{
    const void *	keys[] = {
	kModifier,
	kSecurityLevel,
	kCreationDate
    };
    const void *	values[] = {
	modifier,
	sec_level,
	now
    };

    return (CFDictionaryCreate(NULL, keys, values,
			       sizeof(keys) / sizeof(keys[0]),
			       &kCFTypeDictionaryKeyCallBacks,
			       &kCFTypeDictionaryValueCallBacks));
}


STATIC CFDictionaryRef
CGAModifierDictCreate(CFDataRef modifier, uint8_t sec_level)
{
    CFDictionaryRef	dict;
    CFDateRef 		now;
    CFNumberRef 	sec_level_cf;

    now = CFDateCreate(NULL, CFAbsoluteTimeGetCurrent());
    sec_level_cf = CFNumberCreate(NULL, kCFNumberSInt8Type, &sec_level);
    dict = modifier_dict_create(modifier, sec_level_cf, now);
    CFRelease(now);
    CFRelease(sec_level_cf);
    return (dict);
}

STATIC CFDictionaryRef
cga_dict_create(CFDataRef host_uuid, CFDictionaryRef global_modifier,
		CFDictionaryRef linklocal_modifiers)
{
    CFIndex		count = 2;
    const void *	keys[] = {
	kHostUUID,
	kGlobalModifier,
	kLinkLocalModifiers

    };
    const void *	values[] = {
	host_uuid,
	global_modifier,
	linklocal_modifiers
    };
    if (linklocal_modifiers != NULL) {
	count++;
    }
    return (CFDictionaryCreate(NULL, keys, values, count,
			       &kCFTypeDictionaryKeyCallBacks,
			       &kCFTypeDictionaryValueCallBacks));
}

STATIC bool
CGAWrite(CFDataRef host_uuid, CFDictionaryRef global_modifier,
	 CFDictionaryRef linklocal_modifiers)
{
    CFDictionaryRef	dict;
    bool		success = TRUE;
    
    dict = cga_dict_create(host_uuid, global_modifier, linklocal_modifiers);
    if (my_CFPropertyListWriteFile(dict, CGA_FILE, 0644) < 0) {
	/*
	 * An ENOENT error is expected on a read-only filesystem.  All 
	 * other errors should be reported as LOG_ERR.
	 */
	my_log((errno == ENOENT) ? LOG_DEBUG : LOG_ERR,
	       "CGAParameters: failed to write %s, %s",
	       CGA_FILE, strerror(errno));
	success = FALSE;
    }
    CFRelease(dict);
    return (success);
}

STATIC CFDictionaryRef
cga_keys_dict_create(CFDataRef priv, CFDataRef pub)
{
    const void *	keys[] = {
	kPrivateKey,
	kPublicKey
    };
    const void *	values[] = {
	priv,
	pub
    };
    return (CFDictionaryCreate(NULL, keys, values,
			       sizeof(keys) / sizeof(keys[0]),
			       &kCFTypeDictionaryKeyCallBacks,
			       &kCFTypeDictionaryValueCallBacks));
}

STATIC bool
CGAKeysWrite(CFDataRef priv, CFDataRef pub)
{
    CFDictionaryRef	dict;
    bool		success = TRUE;

    dict = cga_keys_dict_create(priv, pub);
    if (my_CFPropertyListWriteFile(dict, CGA_KEYS_FILE, 0600) < 0) {
	/*
	 * An ENOENT error is expected on a read-only filesystem.  All 
	 * other errors should be reported as LOG_ERR.
	 */
	my_log((errno == ENOENT) ? LOG_DEBUG : LOG_ERR,
	       "CGAParameters: failed to write %s, %s",
	       CGA_KEYS_FILE, strerror(errno));
	success = FALSE;
    }
    CFRelease(dict);
    return (success);
}

STATIC CFDictionaryRef
CGAParametersCreate(CFDataRef host_uuid)
{
    CFDictionaryRef	global_modifier = NULL;
    CFDataRef		modifier = NULL;
    CFDataRef		pub = NULL;
    CFDataRef		priv = NULL;
    uint8_t		security_level;
    bool		success = FALSE;

    /* generate key pair */
    priv = RSAKeyPairGenerate(1024, &pub);
    if (priv == NULL) {
	goto done;
    }

    /* generate global modifier */
    security_level = kCGASecurityLevelZero;
    modifier = my_CFDataCreateWithRandomBytes(IN6_CGA_MODIFIER_LENGTH);
    global_modifier = CGAModifierDictCreate(modifier, security_level);

    /* save the information */
    if (CGAWrite(host_uuid, global_modifier, NULL)
	&& CGAKeysWrite(priv, pub)) {
	/* if we were able to save, push parameters to the kernel */
	success = cga_parameters_set(priv, pub, modifier, security_level);
    }

 done:
    if (success == FALSE) {
	my_CFRelease(&global_modifier);
    }
    my_CFRelease(&pub);
    my_CFRelease(&priv);
    my_CFRelease(&modifier);
    return (global_modifier);
}

STATIC bool
CGAParametersLoad(CFDataRef host_uuid)
{
    CFDictionaryRef	keys_info = NULL;
    CFDictionaryRef	global_modifier = NULL;
    CFDictionaryRef	linklocal_modifiers = NULL;
    CFDictionaryRef	parameters = NULL;
    CFDataRef		modifier = NULL;
    CFDataRef		plist_host_uuid;
    CFDataRef		priv;
    CFDataRef		pub;
    uint8_t		security_level;

    /* load CGA persistent information */
    parameters = my_CFPropertyListCreateFromFile(CGA_FILE);
    if (isA_CFDictionary(parameters) == NULL) {
	if (parameters != NULL) {
	    my_log_fl(LOG_ERR, "%s is not a dictionary", CGA_FILE);
	}
	goto done;
    }

    /* make sure that HostUUID matches */
    plist_host_uuid = CFDictionaryGetValue(parameters, kHostUUID);
    if (isA_CFData(plist_host_uuid) == NULL
	|| !CFEqual(plist_host_uuid, host_uuid)) {
	my_log_fl(LOG_ERR, "%@ missing/invalid", kHostUUID);
	goto done;
    }

    /* global modifier */
    global_modifier = CFDictionaryGetValue(parameters, kGlobalModifier);
    if (global_modifier != NULL) {
	modifier = CGAModifierDictGetModifier(global_modifier,
					      &security_level);
    }
    if (modifier == NULL) {
	global_modifier = NULL;
	my_log_fl(LOG_ERR, "%@ missing/invalid", kGlobalModifier);
	goto done;
    }

    /* load CGA keys */
    keys_info = my_CFPropertyListCreateFromFile(CGA_KEYS_FILE);
    if (isA_CFDictionary(keys_info) == NULL) {
	if (keys_info != NULL) {
	    my_log_fl(LOG_ERR, "%s is not a dictionary", CGA_KEYS_FILE);
	}
	goto done;
    }

    /* private key */
    priv = CFDictionaryGetValue(keys_info, kPrivateKey);
    if (isA_CFData(priv) == NULL) {
	my_log_fl(LOG_ERR, "%@ missing/invalid", kPrivateKey);
	goto done;
    }

    /* public key */
    pub = CFDictionaryGetValue(keys_info, kPublicKey);
    if (isA_CFData(pub) == NULL) {
	my_log_fl(LOG_ERR, "%@ missing/invalid", kPublicKey);
	goto done;
    }

    /* set CGA parameters in the kernel */
    if (cga_parameters_set(priv, pub, modifier, security_level) == FALSE) {
	my_log_fl(LOG_ERR, "cga_parameters_set failed");
	goto failed;
    }

    /* linklocal modifiers */
    linklocal_modifiers
	= CFDictionaryGetValue(parameters, kLinkLocalModifiers);
    if (linklocal_modifiers != NULL
	&& isA_CFDictionary(linklocal_modifiers) == NULL) {
	my_log_fl(LOG_ERR, "%s is not a dictionary", kLinkLocalModifiers);
	linklocal_modifiers = NULL;
    }

 done:
    if (global_modifier != NULL) {
	S_GlobalModifier = CFRetain(global_modifier);
    }
    else {
	global_modifier = CGAParametersCreate(host_uuid);
	if (global_modifier == NULL) {
	    goto failed;
	}
	S_GlobalModifier = global_modifier;
    }
    if (linklocal_modifiers != NULL) {
	/* make a copy of the existing one */
	S_LinkLocalModifiers 
	    = CFDictionaryCreateMutableCopy(NULL, 0, linklocal_modifiers);
	remove_old_linklocal_modifiers(S_LinkLocalModifiers);
    }
    else {
	/* create an empty modifiers dictionary */
	S_LinkLocalModifiers 
	    = CFDictionaryCreateMutable(NULL, 0,
					&kCFTypeDictionaryKeyCallBacks,
					&kCFTypeDictionaryValueCallBacks);
    }

 failed:
    my_CFRelease(&keys_info);
    my_CFRelease(&parameters);
    return (S_GlobalModifier != NULL);
}

PRIVATE_EXTERN void
CGAPrepareSetForInterface(const char * ifname,
			  struct in6_cga_prepare * cga_prep)
{
    CFDictionaryRef	dict;
    CFStringRef		ifname_cf;
    CFDataRef		modifier = NULL;
    uint8_t		security_level;

    if (S_LinkLocalModifiers == NULL) {
	my_log_fl(LOG_ERR, "S_LinkLocalModifiers is NULL");
	return;
    }
    ifname_cf = CFStringCreateWithCString(NULL, ifname, kCFStringEncodingASCII);
    dict = CFDictionaryGetValue(S_LinkLocalModifiers, ifname_cf);
    if (isA_CFDictionary(dict) != NULL) {
	modifier = CGAModifierDictGetModifier(dict, &security_level);
    }
    if (modifier == NULL) {
	security_level = kCGASecurityLevelZero;
	modifier = my_CFDataCreateWithRandomBytes(IN6_CGA_MODIFIER_LENGTH);
	dict = CGAModifierDictCreate(modifier, security_level);
	    CFDictionarySetValue(S_LinkLocalModifiers, ifname_cf, dict);
	CFRelease(modifier);
	CGAWrite(HostUUIDGet(), S_GlobalModifier, S_LinkLocalModifiers);
    }
    CFRelease(ifname_cf);
    cga_prepare_set(cga_prep, modifier, security_level);
    return;
}

PRIVATE_EXTERN bool
CGAIsEnabled(void)
{
    return (S_LinkLocalModifiers != NULL);
}

PRIVATE_EXTERN void
CGAInit(void)
{
    CFDataRef		host_uuid;

    if (G_is_netboot || cga_is_enabled() == FALSE) {
	return;
    }
    host_uuid = HostUUIDGet();
    if (host_uuid == NULL) {
	my_log_fl(LOG_ERR, "Failed to get HostUUID");
	return;
    }
    if (CGAParametersLoad(host_uuid) == FALSE) {
	return;
    }
    return;
    
}

#ifdef TEST_CGA
boolean_t G_is_netboot;

int
main()
{
    ipconfigd_create_paths();
    CGAInit();
}

#endif /* TEST_CGA */
