/*
 * Copyright (c) 2001-2013 Apple Inc. All rights reserved.
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
 * main.c
 */

/* 
 * Modification History
 *
 * October 26, 2001	Dieter Siegmund (dieter@apple)
 * - created
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <net/if_types.h>
#include <sysexits.h>
#include <sys/types.h>
#include <paths.h>
#include <pwd.h>
#include <grp.h>
#include <sys/time.h>

#include <CoreFoundation/CFRunLoop.h>

#include <EAP8021X/LinkAddresses.h>
#include <EAP8021X/EAPClientModule.h>
#include <SystemConfiguration/SCPreferences.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPrivate.h>
#include "EAPOLSocketPrivate.h"
#include "EAPOLSocket.h"
#include "Supplicant.h"
#include "myCFUtil.h"
#include "mylog.h"
#include "EAPOLControlPrefs.h"

extern EAPClientPluginFuncRef
md5_introspect(EAPClientPluginFuncName name);

extern EAPClientPluginFuncRef
eaptls_introspect(EAPClientPluginFuncName name);

extern EAPClientPluginFuncRef
eapttls_introspect(EAPClientPluginFuncName name);

extern EAPClientPluginFuncRef
peap_introspect(EAPClientPluginFuncName name);

extern EAPClientPluginFuncRef
eapmschapv2_introspect(EAPClientPluginFuncName name);

extern EAPClientPluginFuncRef
eapgtc_introspect(EAPClientPluginFuncName name);

extern EAPClientPluginFuncRef
eapfast_introspect(EAPClientPluginFuncName name);

extern EAPClientPluginFuncRef
eapsim_introspect(EAPClientPluginFuncName name);

extern EAPClientPluginFuncRef
eapaka_introspect(EAPClientPluginFuncName name);

typedef struct {
    EAPClientPluginFuncIntrospect *	introspect_func;
    const char *			name;
} BuiltinEAPModule;

typedef BuiltinEAPModule const * BuiltinEAPModuleRef;

static const BuiltinEAPModule	S_builtin_modules[] = {
    { md5_introspect, "md5" },
    { eaptls_introspect, "eaptls" },
    { eapttls_introspect, "eapttls" },
    { peap_introspect, "peap" },
    { eapmschapv2_introspect, "eapmschapv2" },
    { eapgtc_introspect, "eapgtc" },
    { eapfast_introspect, "eapfast" },
    { eapsim_introspect, "eapsim" },
    { eapaka_introspect, "eapaka" }
};
	
EAPClientModuleStatus
S_load_modules()
{
    int				i;
    BuiltinEAPModuleRef		scan;

    for (i = 0, scan = S_builtin_modules;
	 i < (sizeof(S_builtin_modules) / sizeof(S_builtin_modules[0]));
	 i++, scan++) {
	EAPClientModuleStatus	status;

	status = EAPClientModuleAddBuiltinModule(scan->introspect_func);
	if (status != kEAPClientModuleStatusOK) {
	    EAPLOG_FL(LOG_NOTICE, "EAPClientAddBuiltinModule(%s) failed %d",
		      scan->name, status);
	    return (status);
	}
    }
    return (kEAPClientModuleStatusOK);
}

static void
usage(char * progname)
{
    fprintf(stderr, "usage:\n"
	    "%s -i <if_name> [ -u <uid> ] [ -g <gid> ]\n",
	    progname);
    exit(EX_USAGE);
}

static void
log_then_exit(int exit_code)
{
    eapolclient_log(kLogFlagBasic, "exit");
    exit(exit_code);
    return;
}

static uint32_t
check_prefs_common(SCPreferencesRef prefs, bool log_it)
{
    uint32_t	log_flags;

    log_flags = EAPOLControlPrefsGetLogFlags();
    eapolclient_log_set_flags(log_flags, log_it);
    if (prefs == NULL) {
	return (log_flags);
    }
    EAPOLSocketSetGlobals(prefs);
    Supplicant_set_globals(prefs);
    EAPOLControlPrefsSynchronize();
    return (log_flags);
}

static void
check_prefs(SCPreferencesRef prefs)
{
    check_prefs_common(prefs, TRUE);
    return;
}

int
main(int argc, char * argv[1])
{
    char			ch;
    CFDictionaryRef		config_dict = NULL;
    char *			config_file = NULL;
    CFDictionaryRef		control_dict = NULL;
    bool			g_flag = FALSE;
    gid_t			gid = -1;
    char *			if_name = NULL;
    LinkAddressesRef		link_addrs = NULL;
    struct sockaddr_dl *	link = NULL;
    uint32_t			log_flags = 0;
    SCPreferencesRef		prefs;
    EAPOLSocketSourceRef	source;
    SupplicantRef 		supp = NULL;
    bool			u_flag = FALSE;
    uid_t			uid = -1;

    while ((ch = getopt(argc, argv, "c:g:i:lu:")) != EOF) {
	switch ((char) ch) {
	case 'c':
	    config_file = optarg;
	    break;
	case 'i':
	    if (if_name != NULL) {
		usage(argv[0]);
	    }
	    if_name = optarg;
	    break;
	case 'u':
	    if (u_flag) {
		usage(argv[0]);
	    }
	    uid = strtoul(optarg, NULL, 0);
	    u_flag = TRUE;
	    break;
	case 'g':
	    if (g_flag) {
		usage(argv[0]);
	    }
	    gid = strtoul(optarg, NULL, 0);
	    g_flag = TRUE;
	    break;
	default:
	    usage(argv[0]);
	    break;
	}
    }
    if ((argc - optind) != 0 || if_name == NULL) {
	usage(argv[0]);
    }
    if (uid == -1) {
	uid = getuid();
    }
    if (gid == -1) {
	gid = getgid();
    }
    openlog("eapolclient", LOG_CONS | LOG_PID, LOG_DAEMON);
    prefs = EAPOLControlPrefsInit(CFRunLoopGetCurrent(), check_prefs);
    log_flags = check_prefs_common(prefs, FALSE);
    if (log_flags == 0) {
	EAPLOG(LOG_NOTICE, "%s START uid %d gid %d", if_name, uid, gid);
    }
    else {
	EAPLOG(LOG_NOTICE, "%s START uid %d gid %d", if_name, uid, gid);
	EAPLOG(LOG_NOTICE, "Verbose mode enabled (LogFlags 0x%x)", log_flags);
    }
    link_addrs = LinkAddresses_create();
    if (link_addrs == NULL) {
	EAPLOG_FL(LOG_NOTICE, "Could not build interface list");
	exit(EX_OSERR);
    }
    link = LinkAddresses_lookup(link_addrs, if_name);
    if (link == NULL) {
	EAPLOG(LOG_NOTICE, "interface '%s' does not exist", if_name);
	exit(EX_CONFIG);
    }
    if (link->sdl_type != IFT_ETHER) {
	EAPLOG(LOG_NOTICE, "interface '%s' is not ethernet", if_name);
	exit(EX_CONFIG);
    }
    source = EAPOLSocketSourceCreate(if_name,
				     (const struct ether_addr *)
				     (link->sdl_data + link->sdl_nlen),
				     &control_dict);
    if (source == NULL) {
	EAPLOG_FL(LOG_NOTICE, "EAPOLSocketSourceCreate(%s) failed", if_name);
	log_then_exit(EX_UNAVAILABLE);
    }
    if (g_flag) {
	if (setgid(gid) < 0) {
	    EAPLOG_FL(LOG_NOTICE, "setgid(%d) failed, %s", gid,
		      strerror(errno));
	    log_then_exit(EX_NOPERM);
	}
    }
    if (u_flag) {
	if (setuid(uid) < 0) {
	    EAPLOG_FL(LOG_NOTICE, "setuid(%d) failed, %s", uid,
		      strerror(errno));
	    log_then_exit(EX_NOPERM);
	}
    }
    if (config_file != NULL) {
	if (control_dict != NULL) {
	    fprintf(stderr, "Ignoring -c %s\n", config_file);
	}
	else {
	    config_dict = (CFDictionaryRef)
		my_CFPropertyListCreateFromFile(config_file);
	    if (isA_CFDictionary(config_dict) == NULL) {
		fprintf(stderr,
			"contents of file %s invalid\n", config_file);
		my_CFRelease(&config_dict);
		log_then_exit(EX_CONFIG);
	    }
	}
    }
    if (config_dict == NULL && control_dict == NULL) {
	EAPLOG_FL(LOG_NOTICE, "%s: config/control dictionary missing", if_name);
	log_then_exit(EX_SOFTWARE);
    }
    if (S_load_modules() != kEAPClientModuleStatusOK) {
	log_then_exit(EX_SOFTWARE);
    }
    supp = EAPOLSocketSourceCreateSupplicant(source, control_dict);
    if (supp == NULL) {
	EAPLOG_FL(LOG_NOTICE, "EAPOLSocketSourceCreateSupplicant failed");
	EAPOLSocketSourceFree(&source);
	log_then_exit(EX_UNAVAILABLE);
    }
    if (control_dict != NULL) {
	(void)setsid();
	(void)chdir("/");
    }
    else {
	bool	should_stop = FALSE;

	(void)Supplicant_update_configuration(supp, config_dict, &should_stop);
	EAPLOG(LOG_NOTICE,
	       "Supplicant_update_configuration says we should stop - exiting");
	exit(EX_UNAVAILABLE);
    }
    my_CFRelease(&control_dict);
    my_CFRelease(&config_dict);
    Supplicant_start(supp);
    
    LinkAddresses_free(&link_addrs);
    CFRunLoopRun();

    log_then_exit(EX_OK);
    return (0);
}
