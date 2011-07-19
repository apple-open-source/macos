/*
 * Copyright (c) 2001-2010 Apple Inc. All rights reserved.
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
#include <syslog.h>
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
#include "EAPOLSocketPrivate.h"
#include "EAPOLSocket.h"
#include "Supplicant.h"
#include "myCFUtil.h"
#include "mylog.h"

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

EAPClientModuleStatus
S_load_modules()
{
    EAPClientModuleStatus status;

    status = EAPClientModuleAddBuiltinModule(md5_introspect);
    if (status != kEAPClientModuleStatusOK) {
	fprintf(stderr, "EAPClientAddBuiltinModule(md5) failed %d\n",
		status);
	return (status);
    }
    status = EAPClientModuleAddBuiltinModule(eaptls_introspect);
    if (status != kEAPClientModuleStatusOK) {
	fprintf(stderr, "EAPClientAddBuiltinModule(eaptls) failed %d\n",
		status);
	return (status);
    }
    status = EAPClientModuleAddBuiltinModule(eapttls_introspect);
    if (status != kEAPClientModuleStatusOK) {
	fprintf(stderr, "EAPClientAddBuiltinModule(eapttls) failed %d\n",
		status);
	return (status);
    }
    status = EAPClientModuleAddBuiltinModule(peap_introspect);
    if (status != kEAPClientModuleStatusOK) {
	fprintf(stderr, "EAPClientAddBuiltinModule(peap) failed %d\n",
		status);
	return (status);
    }
    status = EAPClientModuleAddBuiltinModule(eapfast_introspect);
    if (status != kEAPClientModuleStatusOK) {
	fprintf(stderr, "EAPClientAddBuiltinModule(eapfast) failed %d\n",
		status);
	return (status);
    }
    status = EAPClientModuleAddBuiltinModule(eapmschapv2_introspect);
    if (status != kEAPClientModuleStatusOK) {
	fprintf(stderr, "EAPClientAddBuiltinModule(mschapv2) failed %d\n",
		status);
	return (status);
    }
    status = EAPClientModuleAddBuiltinModule(eapgtc_introspect);
    if (status != kEAPClientModuleStatusOK) {
	fprintf(stderr, "EAPClientAddBuiltinModule(eapgtc) failed %d\n",
		status);
	return (status);
    }
    status = EAPClientModuleAddBuiltinModule(eapsim_introspect);
    if (status != kEAPClientModuleStatusOK) {
	fprintf(stderr, "EAPClientAddBuiltinModule(eapsim) failed %d\n",
		status);
	return (status);
    }
    return (kEAPClientModuleStatusOK);
}

static FILE *
setup_log_file(SCPreferencesRef prefs, 
	       const char * if_name, uint32_t * ret_log_flags)
{
    FILE *		log_file = NULL;
    CFNumberRef		log_flags;

    *ret_log_flags = 0;
    if (prefs == NULL) {
	return (NULL);
    }
    log_flags = SCPreferencesGetValue(prefs, CFSTR("LogFlags"));
    if (log_flags != NULL) {
	if (isA_CFNumber(log_flags) == NULL
	    || CFNumberGetValue(log_flags, kCFNumberIntType, 
				ret_log_flags) == FALSE) {
	    my_log(LOG_NOTICE, "com.apple.eapolclient.LogFlags invalid");
	}
	else {
	    char 		filename[512];

	    snprintf(filename, sizeof(filename), 
		     "/var/log/eapolclient.%s.log", if_name);
	    log_file = fopen(filename, "a+");
	    if (log_file == NULL) {
		my_log(LOG_NOTICE, "could not open '%s', %s\n",
		       filename, strerror(errno));
	    }
	    else {
		my_log(LOG_NOTICE, "opened log file '%s'",
		       filename);
	    }
	    if ((*ret_log_flags & kLogFlagIncludeStdoutStderr) != 0) {
		int		log_fd;

		fflush(stdout);
		fflush(stderr);

		log_fd = fileno(log_file);
		dup2(log_fd, STDOUT_FILENO);
		dup2(log_fd, STDERR_FILENO);
	    }
	}
    }
    return (log_file);
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
    eapolclient_log(kLogFlagBasic, "exit\n");
    exit(exit_code);
    return;
}

static SCPreferencesRef
open_prefs(void)
{
    SCPreferencesRef	prefs;

    prefs = SCPreferencesCreate(NULL, CFSTR("eapolclient"),
				CFSTR("com.apple.eapolclient.plist"));
    if (prefs == NULL) {
	my_log(LOG_NOTICE, "SCPreferencesCreate failed");
	return (NULL);
    }
    return (prefs);
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
    FILE *			log_file;
    uint32_t			log_flags = 0;
    SCPreferencesRef		prefs;
    EAPOLSocketSourceRef	source;
    SupplicantRef 		supp = NULL;
    bool			u_flag = FALSE;
    uid_t			uid = -1;

    link_addrs = LinkAddresses_create();
    if (link_addrs == NULL) {
	printf("Could not build interface list\n");
	exit(EX_OSERR);
    }
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
    link = LinkAddresses_lookup(link_addrs, if_name);
    if (link == NULL) {
	printf("interface '%s' does not exist\n", if_name);
	exit(EX_CONFIG);
    }
    if (link->sdl_type != IFT_ETHER) {
	printf("interface '%s' is not ethernet\n", if_name);
	exit(EX_CONFIG);
    }
    openlog("eapolclient", LOG_CONS | LOG_PID, LOG_DAEMON);

    prefs = open_prefs();
    log_file = setup_log_file(prefs, if_name, &log_flags);
    if (log_file != NULL) {
	eapolclient_log_set(log_file, log_flags);
    }
    eapolclient_log(kLogFlagBasic,
		    "start pid %d uid %d gid %d log_flags 0x%x\n",
		    getpid(), uid, gid, log_flags);
    EAPOLSocketSetGlobals(prefs);
    my_CFRelease(&prefs);
    source = EAPOLSocketSourceCreate(if_name,
				     (const struct ether_addr *)
				     (link->sdl_data + link->sdl_nlen),
				     &control_dict);
    if (source == NULL) {
	my_log(LOG_NOTICE, "EAPOLSocketSourceCreate(%s) failed", if_name);
	log_then_exit(EX_UNAVAILABLE);
    }
    if (g_flag) {
	if (setgid(gid) < 0) {
	    syslog(LOG_NOTICE, "setgid(%d) failed, %m", gid);
	    log_then_exit(EX_NOPERM);
	}
    }
    if (u_flag) {
	if (setuid(uid) < 0) {
	    syslog(LOG_NOTICE, "setuid(%d) failed, %m", uid);
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
		fprintf(stderr, "contents of file %s invalid\n", config_file);
		my_CFRelease(&config_dict);
		log_then_exit(EX_CONFIG);
	    }
	}
    }
    if (config_dict == NULL && control_dict == NULL) {
	my_log(LOG_NOTICE, "%s: config/control dictionary missing", if_name);
	log_then_exit(EX_SOFTWARE);
    }
    if (S_load_modules() != kEAPClientModuleStatusOK) {
	log_then_exit(EX_SOFTWARE);
    }
    supp = EAPOLSocketSourceCreateSupplicant(source, control_dict);
    if (supp == NULL) {
	syslog(LOG_NOTICE, "EAPOLSocketSourceCreateSupplicant failed");
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
	syslog(LOG_NOTICE,
	       "Supplicant_update_configuration says we should stop - exiting");
	exit(EX_UNAVAILABLE);
    }
    my_CFRelease(&control_dict);
    my_CFRelease(&config_dict);
    my_log(LOG_NOTICE, "%s START", if_name);
    Supplicant_start(supp);
    
    LinkAddresses_free(&link_addrs);
    CFRunLoopRun();

    log_then_exit(EX_OK);
    return (0);
}
