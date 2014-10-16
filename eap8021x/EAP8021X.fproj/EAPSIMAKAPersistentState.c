/*
 * Copyright (c) 2012-2014 Apple Inc. All rights reserved.
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
 * EAPSIMAKAPersistentState.c
 * - routines to load and save persistent state 
 * - most information is stored in preferences, the master key is stored
 *   in the keychain
 */

/* 
 * Modification History
 *
 * October 19, 2012	Dieter Siegmund (dieter@apple)
 * - created
 */

#include <CoreFoundation/CFPreferences.h>
#include <notify.h>
#include <SystemConfiguration/SCValidation.h>

#include "symbol_scope.h"
#include "myCFUtil.h"
#include "EAP.h"
#include "EAPKeychainUtil.h"
#include "EAPLog.h"
#include "EAPSIMAKAPersistentState.h"

#define kEAPSIMAKAPrefsChangedNotification		\
    "com.apple.network.eapclient.eapsimaka.prefs"

typedef Boolean
(*IMSIMatchFuncRef)(CFStringRef imsi, CFDictionaryRef info,
		    const void * context);
    
STATIC void
IMSIListRemoveMatches(EAPType type, IMSIMatchFuncRef iter,
		      const void * context);

STATIC Boolean
prefs_did_change(uint32_t * gen_p)
{
    Boolean		changed;
    int			check = 0;
    uint32_t		notify_status;
    STATIC uint32_t	S_generation;
    STATIC int		S_token;
    STATIC Boolean	S_token_valid = FALSE;

    changed = FALSE;
    if (!S_token_valid) {
	notify_status = 
	    notify_register_check(kEAPSIMAKAPrefsChangedNotification,
				  &S_token);
	if (notify_status != NOTIFY_STATUS_OK) {
	    EAPLOG_FL(LOG_NOTICE,
		      "notify_register_check returned %d",
		      notify_status);
	    goto done;
	}
	S_token_valid = TRUE;
    }
    notify_status = notify_check(S_token, &check);
    if (notify_status != NOTIFY_STATUS_OK) {
	EAPLOG_FL(LOG_NOTICE, "notify_check returned %d",
		  notify_status);
	goto done;
    }
    if (check != 0) {
	S_generation++;
    }
    if (*gen_p != S_generation) {
	changed = TRUE;
    }
    *gen_p = S_generation;

 done:
    return (changed);
}

STATIC void
prefs_notify_change(void)
{
    uint32_t	status;

    status = notify_post(kEAPSIMAKAPrefsChangedNotification);
    if (status != NOTIFY_STATUS_OK) {
	EAPLOG_FL(LOG_NOTICE, "notify_post returned %d", status);
    }
    return;
}

/*
 * MasterKey
 * - retrieve, store, remove master key from keychain
 */
STATIC CFStringRef
MasterKeyItemIDCreate(CFStringRef proto, CFStringRef imsi)
{
    return CFStringCreateWithFormat(NULL,
				    NULL,
				    CFSTR("com.apple.network.%@.master-key.%@"),
				    proto,
				    imsi);
}

STATIC CFDataRef
MasterKeyCopyFromKeychain(CFStringRef proto, CFStringRef imsi)
{
    CFDataRef	data = NULL;
    CFStringRef	key;
    OSStatus	result;

    key = MasterKeyItemIDCreate(proto, imsi);
    result = EAPSecKeychainPasswordItemCopy(NULL, key, &data);
    CFRelease(key);
    if (result != noErr && result != errSecItemNotFound) {
	EAPLOG_FL(LOG_NOTICE, "Failed to read a keychain item: %d",
		  (int)result);
	return NULL;
    }
    return (data);
}

STATIC void
MasterKeySaveToKeychain(CFStringRef proto, CFStringRef imsi, CFDataRef data)
{
    CFStringRef	key;
    OSStatus	result;

    key = MasterKeyItemIDCreate(proto, imsi);
    result = EAPSecKeychainPasswordItemSet(NULL, key, data);
    if (result != noErr) {
	result = EAPSecKeychainPasswordItemCreateWithAccess(NULL,
							    NULL,
							    key,
							    NULL,
							    NULL,
							    NULL,
							    data);
    }
    CFRelease(key);
    if (result != noErr) {
	EAPLOG_FL(LOG_NOTICE, "Failed to update/create a keychain item: %d",
		  (int)result);
    }
    return;
}

STATIC void
MasterKeyRemoveFromKeychain(CFStringRef proto, CFStringRef imsi)
{
    CFStringRef	key;

    key = MasterKeyItemIDCreate(proto, imsi);
    EAPSecKeychainPasswordItemRemove(NULL, key);
    CFRelease(key);
    return;
}

/*
 * ProtoInfo
 * - constant information specific to particular protocol (EAP-SIM, EAP-AKA)
 *   	appID:		locates preferences
 *   	proto:		locates keychain items
 *   	generation:	multiplexes single notification to detect changes
 */

#define kEAPSIMAppIDStr	"com.apple.network.eapclient.eapsim"
#define kEAPAKAAppIDStr "com.apple.network.eapclient.eapaka"

typedef struct {
    const CFStringRef		appID;
    const CFStringRef		proto;
    uint32_t			generation;
} ProtoInfo, * ProtoInfoRef;

STATIC ProtoInfo	S_eapsim_info = {
    CFSTR(kEAPSIMAppIDStr), CFSTR("eapsim"), 0,
};

STATIC ProtoInfo	S_eapaka_info = {
    CFSTR(kEAPAKAAppIDStr), CFSTR("eapaka"), 0,
};

STATIC ProtoInfoRef
ProtoInfoForType(EAPType type)
{
    ProtoInfoRef	info;

    switch (type) {
    case kEAPTypeEAPSIM:
	info = &S_eapsim_info;
	break;
    case kEAPTypeEAPAKA:
	info = &S_eapaka_info;
	break;
    default:
	EAPLOG_FL(LOG_NOTICE, "unrecognized type %d", type);
	info = NULL;
	break;
    }
    return (info);
}

STATIC void
ProtoInfoChangedCheck(ProtoInfoRef proto_info)
{
    if (prefs_did_change(&proto_info->generation)) {
	CFPreferencesSynchronize(proto_info->appID,
				 kCFPreferencesCurrentUser,
				 kCFPreferencesAnyHost);
    }
    return;
}

STATIC void
ProtoInfoNotifyChange(ProtoInfoRef proto_info)
{
    prefs_notify_change();
    return;
}

/**
 ** EAPSIMAKAPersistentState
 **/

STATIC CFStringRef kPrefsPseudonym = CFSTR("Pseudonym"); /* string */
STATIC CFStringRef kPrefsReauthCounter = CFSTR("ReauthCounter"); /* number */
STATIC CFStringRef kPrefsReauthID = CFSTR("ReauthID"); 	/* string */
STATIC CFStringRef kPrefsSSID = CFSTR("SSID"); 		/* string */

struct EAPSIMAKAPersistentState {
    EAPType			type;
    EAPSIMAKAAttributeType 	identity_type;
    CFStringRef			imsi;
    uint16_t			counter;
    CFStringRef			pseudonym;
    CFStringRef			reauth_id;
    ProtoInfoRef		proto_info;
    int				master_key_size;
    uint8_t			master_key[1];
};

INLINE unsigned int
EAPSIMAKAPersistentStateComputeSize(unsigned int n)
{
    return ((int)offsetof(struct EAPSIMAKAPersistentState, master_key[n]));
}

PRIVATE_EXTERN uint8_t *
EAPSIMAKAPersistentStateGetMasterKey(EAPSIMAKAPersistentStateRef persist)
{
    return (persist->master_key);
}

PRIVATE_EXTERN int
EAPSIMAKAPersistentStateGetMasterKeySize(EAPSIMAKAPersistentStateRef persist)
{
    return (persist->master_key_size);
}

PRIVATE_EXTERN CFStringRef
EAPSIMAKAPersistentStateGetIMSI(EAPSIMAKAPersistentStateRef persist)
{
    return (persist->imsi);
}

PRIVATE_EXTERN CFStringRef
EAPSIMAKAPersistentStateGetPseudonym(EAPSIMAKAPersistentStateRef persist)
{
    return (persist->pseudonym);
}

PRIVATE_EXTERN void
EAPSIMAKAPersistentStateSetPseudonym(EAPSIMAKAPersistentStateRef persist,
				     CFStringRef pseudonym)
{
    if (persist->identity_type == kAT_PERMANENT_ID_REQ) {
	/* we don't need to remember this */
	return;
    }
    my_FieldSetRetainedCFType(&persist->pseudonym, pseudonym);
    return;
}

PRIVATE_EXTERN CFStringRef
EAPSIMAKAPersistentStateGetReauthID(EAPSIMAKAPersistentStateRef persist)
{
    return (persist->reauth_id);
}

PRIVATE_EXTERN void
EAPSIMAKAPersistentStateSetReauthID(EAPSIMAKAPersistentStateRef persist,
				    CFStringRef reauth_id)
{
    if (persist->identity_type == kAT_ANY_ID_REQ) {
	/* only save this if we want any identity */
	my_FieldSetRetainedCFType(&persist->reauth_id, reauth_id);
    }
    return;
}

PRIVATE_EXTERN uint16_t
EAPSIMAKAPersistentStateGetCounter(EAPSIMAKAPersistentStateRef persist)
{
    return (persist->counter);
}

PRIVATE_EXTERN void
EAPSIMAKAPersistentStateSetCounter(EAPSIMAKAPersistentStateRef persist,
				   uint16_t counter)
{
    persist->counter = counter;
    return;
}

PRIVATE_EXTERN EAPSIMAKAPersistentStateRef
EAPSIMAKAPersistentStateCreate(EAPType type, int master_key_size, 
			       CFStringRef imsi,
			       EAPSIMAKAAttributeType identity_type)
{
    EAPSIMAKAPersistentStateRef	persist = NULL;
    ProtoInfoRef		proto_info;

    if (imsi == NULL) {
	EAPLOG_FL(LOG_NOTICE, "imsi is NULL");
	goto done;
    }
    proto_info = ProtoInfoForType(type);
    if (proto_info == NULL) {
	goto done;
    }
    persist = (EAPSIMAKAPersistentStateRef)
	malloc(EAPSIMAKAPersistentStateComputeSize(master_key_size));
    bzero(persist, sizeof(*persist));
    persist->type = type;
    persist->imsi = CFRetain(imsi);
    persist->proto_info = proto_info;
    persist->master_key_size = master_key_size;
    persist->identity_type = identity_type;

    /* retrieve stored information if it's required */
    if (identity_type != kAT_PERMANENT_ID_REQ) {
	CFPropertyListRef	info;
	CFStringRef		pseudonym = NULL;

	ProtoInfoChangedCheck(proto_info);
	info = CFPreferencesCopyValue(imsi,
				      proto_info->appID,
				      kCFPreferencesCurrentUser,
				      kCFPreferencesAnyHost);
	if (isA_CFDictionary(info) != NULL) {
	    CFDictionaryRef	dict = (CFDictionaryRef)info;
	    
	    pseudonym
		= isA_CFString(CFDictionaryGetValue(dict, kPrefsPseudonym));
	    if (pseudonym == NULL) {
		/* try the old value */
		pseudonym
		    = isA_CFString(CFDictionaryGetValue(dict,
							CFSTR("PseudonymID")));
	    }
	    if (identity_type == kAT_ANY_ID_REQ) {
		CFNumberRef		counter;
		CFDataRef		master_key;
		CFStringRef		reauth_id;

		/* grab the reauth ID information */
		counter 
		    = isA_CFNumber(CFDictionaryGetValue(dict,
							kPrefsReauthCounter));
		reauth_id =
		    isA_CFString(CFDictionaryGetValue(dict, kPrefsReauthID));
		master_key 
		    = MasterKeyCopyFromKeychain(proto_info->proto, imsi);
		if (counter != NULL 
		    && reauth_id != NULL
		    && master_key != NULL 
		    && CFDataGetLength(master_key) == master_key_size) {
		    uint16_t	val;

		    CFDataGetBytes(master_key,
				   CFRangeMake(0, master_key_size),
				   persist->master_key);
		    CFNumberGetValue(counter, kCFNumberSInt16Type,
				     (void *)&val);
		    EAPSIMAKAPersistentStateSetCounter(persist, val);
		    EAPSIMAKAPersistentStateSetReauthID(persist, reauth_id);
		}
		my_CFRelease(&master_key);
	    }
	}
	else if (isA_CFString(info) != NULL) {
	    pseudonym = (CFStringRef)info;
	}
	if (pseudonym != NULL) {
	    EAPSIMAKAPersistentStateSetPseudonym(persist, pseudonym);
	}
	my_CFRelease(&info);
    }
 done:
    return (persist);
}


STATIC Boolean
IMSIDoesNotMatch(CFStringRef imsi, CFDictionaryRef info,
		 const void * context)
{
    CFStringRef		imsi_to_check = (CFStringRef)context;

    return (my_CFEqual(imsi, imsi_to_check) == FALSE);
}
    

PRIVATE_EXTERN void
EAPSIMAKAPersistentStateSave(EAPSIMAKAPersistentStateRef persist,
			     Boolean master_key_valid,
			     CFStringRef ssid)
{
    CFMutableDictionaryRef	info = NULL;

    if (persist->identity_type == kAT_PERMANENT_ID_REQ) {
	/* don't need to store information */
	return;
    }

    /* remove any entry that does not match this IMSI */
    IMSIListRemoveMatches(persist->type, IMSIDoesNotMatch, persist->imsi);

    /* Pseudonym */
    if (EAPSIMAKAPersistentStateGetPseudonym(persist) != NULL) {
	info = CFDictionaryCreateMutable(NULL, 0,
					 &kCFTypeDictionaryKeyCallBacks,
					 &kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(info, kPrefsPseudonym,
			     EAPSIMAKAPersistentStateGetPseudonym(persist));
    }

    if (EAPSIMAKAPersistentStateGetReauthID(persist) != NULL
	&& master_key_valid) {
	CFNumberRef	counter;
	CFDataRef	master_key;
	
	if (info == NULL) {
	    info = CFDictionaryCreateMutable(NULL, 0,
					     &kCFTypeDictionaryKeyCallBacks,
					     &kCFTypeDictionaryValueCallBacks);
	}
	/* Reauth Counter */
	counter = CFNumberCreate(NULL, kCFNumberSInt16Type,
				 (const void *)&persist->counter);
	CFDictionarySetValue(info, kPrefsReauthCounter, counter);
	CFRelease(counter);

	/* Reauth ID */
	CFDictionarySetValue(info, kPrefsReauthID,
			     EAPSIMAKAPersistentStateGetReauthID(persist));

	/* Master Key */
	master_key 
	    = CFDataCreate(NULL,
			   EAPSIMAKAPersistentStateGetMasterKey(persist),
			   EAPSIMAKAPersistentStateGetMasterKeySize(persist));
	MasterKeySaveToKeychain(persist->proto_info->proto, persist->imsi,
				master_key);
	CFRelease(master_key);
    }

    /* SSID */
    if (info != NULL && ssid != NULL) {
	CFDictionarySetValue(info, kPrefsSSID, ssid);
    }
    ProtoInfoChangedCheck(persist->proto_info);
    CFPreferencesSetValue(persist->imsi, info,
			  persist->proto_info->appID,
			  kCFPreferencesCurrentUser,
			  kCFPreferencesAnyHost);
    my_CFRelease(&info);    
    CFPreferencesSynchronize(persist->proto_info->appID,
			     kCFPreferencesCurrentUser,
			     kCFPreferencesAnyHost);
    ProtoInfoNotifyChange(persist->proto_info);
    return;

}


PRIVATE_EXTERN void
EAPSIMAKAPersistentStateRelease(EAPSIMAKAPersistentStateRef persist)
{
    my_CFRelease(&persist->imsi);
    EAPSIMAKAPersistentStateSetReauthID(persist, NULL);
    EAPSIMAKAPersistentStateSetPseudonym(persist, NULL);
    free(persist);
    return;
}
    
STATIC void
IMSIListRemoveMatches(EAPType type, IMSIMatchFuncRef iter,
		      const void * context)
{
    Boolean		changed = FALSE;
    int			i;
    CFArrayRef		imsi_list;
    ProtoInfoRef	proto_info;

    proto_info = ProtoInfoForType(type);
    if (proto_info == NULL) {
	return;
    }
    ProtoInfoChangedCheck(proto_info);
    imsi_list = CFPreferencesCopyKeyList(proto_info->appID,
					 kCFPreferencesCurrentUser,
					 kCFPreferencesAnyHost);
    if (imsi_list == NULL) {
	return;
    }
    for (i = 0; i < CFArrayGetCount(imsi_list); i++) {
	CFStringRef		imsi = CFArrayGetValueAtIndex(imsi_list, i);
	CFPropertyListRef	prefs;

	prefs = CFPreferencesCopyValue(imsi,
				       proto_info->appID,
				       kCFPreferencesCurrentUser,
				       kCFPreferencesAnyHost);
	if (prefs == NULL) {
	    continue;
	}
	if (isA_CFDictionary(prefs) != NULL) {
	    CFDictionaryRef 	info = (CFDictionaryRef)prefs;

	    if ((*iter)(imsi, info, context)) {
		CFPreferencesSetValue(imsi, NULL,
				      proto_info->appID,
				      kCFPreferencesCurrentUser,
				      kCFPreferencesAnyHost);
		CFPreferencesSynchronize(proto_info->appID,
					 kCFPreferencesCurrentUser,
					 kCFPreferencesAnyHost);
		MasterKeyRemoveFromKeychain(proto_info->proto, imsi);
		changed = TRUE;
	    }
	}
	CFRelease(prefs);
    }
    if (changed) {
	ProtoInfoNotifyChange(proto_info);
    }
    CFRelease(imsi_list);
    return;
}

STATIC Boolean
IMSIMatchesSSID(CFStringRef imsi, CFDictionaryRef info,
		const void * context)
{
    Boolean		match = FALSE;
    CFStringRef 	saved_ssid;
    CFStringRef		ssid = (CFStringRef)context;
	    
    saved_ssid = CFDictionaryGetValue(info, kPrefsSSID);
    if (my_CFEqual(ssid, saved_ssid)) {
	match = TRUE;
    }
    return (match);
}

PRIVATE_EXTERN void
EAPSIMAKAPersistentStateForgetSSID(CFStringRef ssid)
{
    IMSIListRemoveMatches(kEAPTypeEAPSIM, IMSIMatchesSSID, ssid);
    IMSIListRemoveMatches(kEAPTypeEAPAKA, IMSIMatchesSSID, ssid);
    return;
}

#ifdef TEST_EAPSIMAKA_PERSISTENT_STATE
#define USE_SYSTEMCONFIGURATION_PRIVATE_HEADERS 1
#include <SystemConfiguration/SCPrivate.h>
#include <string.h>
#include <sysexits.h>
#include <CommonCrypto/CommonDigest.h>
#include "printdata.h"

STATIC void 
usage(const char * progname)
{
    printf("usage: %s \"sim\" | \"aka\" <command> <parameters>\n"
	   "    <command> is one of \"get\", \"set\", or \"remove\"\n",
	   progname);
    exit(EX_USAGE);
}

STATIC void
EAPSIMAKAPersistentStatePrint(EAPSIMAKAPersistentStateRef persist)
{
    if (persist->pseudonym != NULL) {
	SCPrint(TRUE, stdout, CFSTR("Pseudonym: %@\n"), persist->pseudonym);
    }

    if (persist->reauth_id != NULL) {
	SCPrint(TRUE, stdout, CFSTR("Reauth ID: %@\n"), persist->reauth_id);
	if (persist->master_key_size != 0) {
	    printf("Master Key: ");
	    print_bytes(persist->master_key, persist->master_key_size);
	    printf("\n");
	}
	SCPrint(TRUE, stdout, CFSTR("Reauth counter: %d\n"),
		persist->counter);
    }
    return;
}

STATIC void
handle_get(const char * progname, EAPType type, int argc, char * argv[])
{
    CFStringRef			imsi;
    EAPSIMAKAPersistentStateRef	persist;

    if (argc < 1) {
	fprintf(stderr, "%s %s get <IMSI>\n",
		progname, (type == kEAPTypeEAPSIM) ? "sim" : "aka");
	exit(EX_USAGE);
    }
    imsi = CFStringCreateWithCString(NULL, argv[0], kCFStringEncodingUTF8);
    persist = EAPSIMAKAPersistentStateCreate(type, 
					     CC_SHA1_DIGEST_LENGTH,
					     imsi,
					     kAT_ANY_ID_REQ);
    CFRelease(imsi);
    EAPSIMAKAPersistentStatePrint(persist);
    EAPSIMAKAPersistentStateRelease(persist);
    return;
}

STATIC void
set_usage(const char * progname, EAPType type)
{
    fprintf(stderr, "usage: %s %s set <IMSI> "
	    "[ -c <counter> -r <reauth_id> ] "
	    "[ -p <pseudonym> ] [ -s <ssid> ]\n",
	    progname, (type == kEAPTypeEAPSIM) ? "sim" : "aka");
    exit(EX_USAGE);
    return;
}

STATIC void
handle_set(const char * progname, EAPType type, int argc, char * argv[])
{
    int				ch;
    Boolean			counter_set = FALSE;
    CFStringRef			imsi;
    EAPSIMAKAPersistentStateRef	persist;
    CFStringRef			pseudonym = NULL;
    CFStringRef			reauth_id = NULL;
    CFStringRef			ssid = NULL;

    if (argc < 1) {
	set_usage(progname, type);
    }
    imsi = CFStringCreateWithCString(NULL, argv[0], kCFStringEncodingUTF8);
    persist = EAPSIMAKAPersistentStateCreate(type, 
					     CC_SHA1_DIGEST_LENGTH,
					     imsi,
					     kAT_ANY_ID_REQ);
    CFRelease(imsi);
    while ((ch = getopt(argc, argv, "c:p:r:s:")) != EOF) {
	switch (ch) {
	case 'c':
	    if (counter_set) {
		fprintf(stderr, "-c specified multiple times\n");
		set_usage(progname, type);
		exit(EX_USAGE);
	    }
	    counter_set = TRUE;
	    EAPSIMAKAPersistentStateSetCounter(persist,
					       strtoul(optarg, NULL, 0));
	    break;
	case 'p':
	    if (pseudonym != NULL) {
		fprintf(stderr, "-p specified multiple times\n");
		set_usage(progname, type);
	    }
	    pseudonym = CFStringCreateWithCString(NULL, optarg,
						  kCFStringEncodingUTF8);
	    EAPSIMAKAPersistentStateSetPseudonym(persist, pseudonym);
	    break;
	case 'r':
	    if (reauth_id != NULL) {
		fprintf(stderr, "-r specified multiple times\n");
		set_usage(progname, type);
	    }
	    reauth_id = CFStringCreateWithCString(NULL, optarg,
						  kCFStringEncodingUTF8);
	    EAPSIMAKAPersistentStateSetReauthID(persist, reauth_id);
	    break;
	case 's':
	    if (ssid != NULL) {
		fprintf(stderr, "-s specified multiple times\n");
		set_usage(progname, type);
	    }
	    ssid = CFStringCreateWithCString(NULL, optarg,
					     kCFStringEncodingUTF8);
	    break;
	default:
	    set_usage(progname, type);
	    break;
	}
    }
    if (reauth_id != NULL || counter_set) {
	int		count;
	int		i;
	uint8_t *	scan;

	if (reauth_id == NULL || counter_set == FALSE) {
	    fprintf(stderr, "-c and -r must both be specified\n");
	    set_usage(progname, type);
	}
	count = EAPSIMAKAPersistentStateGetMasterKeySize(persist);
	scan = EAPSIMAKAPersistentStateGetMasterKey(persist);
	for (i = 0; i < count; i++, scan++) {
	    *scan = '0' + i;
	}
    }
    if (pseudonym == NULL && reauth_id == NULL) {
	set_usage(progname, type);
    }
    EAPSIMAKAPersistentStateSave(persist, (reauth_id != NULL), ssid);
    EAPSIMAKAPersistentStateRelease(persist);
    return;
}

STATIC void
handle_remove(const char * progname, EAPType type, int argc, char * argv[])
{
    CFStringRef			imsi;
    EAPSIMAKAPersistentStateRef	persist;
    CFStringRef			ssid;

    if (argc < 1) {
	fprintf(stderr, "%s %s remove <SSID>\n",
		progname, (type == kEAPTypeEAPSIM) ? "sim" : "aka");
	exit(EX_USAGE);
    }
    ssid = CFStringCreateWithCString(NULL, argv[0], kCFStringEncodingUTF8);
    IMSIListRemoveMatches(type, IMSIMatchesSSID, ssid);
    CFRelease(ssid);
    return;
}

int 
main(int argc, char * argv[])
{
    const char *		command;
    EAPType			eap_type;
    CFStringRef			imsi = NULL;
    const char *		progname = argv[0];
    CFStringRef			ssid = NULL;
    const char *		type;

    if (argc < 3) {
	usage(progname);
    }
    type = argv[1];
    if (strcasecmp(type, "sim") == 0) {
	eap_type = kEAPTypeEAPSIM;
    }
    else if (strcasecmp(type, "aka") == 0) {
	eap_type = kEAPTypeEAPAKA;
    }
    else {
	usage(progname);
    }
    command = argv[2];
    argc -= 3;
    argv += 3;
    if (strcmp(command, "get") == 0) {
	handle_get(progname, eap_type, argc, argv);
    }
    else if (strcmp(command, "set") == 0) {
	handle_set(progname, eap_type, argc, argv);
    }
    else if (strcmp(command, "remove") == 0) {
	handle_remove(progname, eap_type, argc, argv);
    }
    else {
	usage(progname);
    }
    exit(0);
    return (0);
}

#endif /* TEST_EAPSIMAKA_PERSISTENT_STATE */
