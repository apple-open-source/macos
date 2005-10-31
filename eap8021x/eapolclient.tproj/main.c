
/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All rights reserved.
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
#include <pwd.h>
#include <grp.h>
#include <sys/time.h>

#include <CoreFoundation/CFRunLoop.h>

#include <EAP8021X/LinkAddresses.h>
#include <EAP8021X/EAPClientModule.h>
#include <SystemConfiguration/SCValidation.h>
#include "EAPOLSocket.h"
#include "Supplicant.h"
#include "myCFUtil.h"
#include "mylog.h"

static SupplicantRef 		supp = NULL;


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
    return (kEAPClientModuleStatusOK);
}

#define EAPOLCLIENT_LOG_DIR	"/var/log/eapolclient"

static void
setup_log_file(const char * if_name, uid_t uid, gid_t gid)
{
    char 		filename[512];
    int			log_fd;
    struct stat		sb;

    if (stat(EAPOLCLIENT_LOG_DIR, &sb) < 0) {
	my_log(LOG_DEBUG, "%s: stat " EAPOLCLIENT_LOG_DIR " failed, %m", 
	       if_name);
	return;
    }
    if ((sb.st_mode & S_IFDIR) == 0) {
	my_log(LOG_DEBUG, "%s: " EAPOLCLIENT_LOG_DIR " is not a directory", 
	       if_name);
	return;
    }
    snprintf(filename, sizeof(filename), 
	     EAPOLCLIENT_LOG_DIR "/uid%lu-%s.log", (unsigned long)uid, 
	     if_name);
    log_fd = open(filename, O_CREAT | O_WRONLY | O_APPEND, 0600);
    if (log_fd < 0) {
	my_log(LOG_NOTICE, "%s: failed to open %s, %m", if_name,
	       filename);
    }
    else {
	my_log(LOG_NOTICE, "%s: using log file %s", if_name, filename);
	fchown(log_fd, uid, gid);
	fflush(stdout);
	fflush(stderr);
	dup2(log_fd, STDOUT_FILENO);
	dup2(log_fd, STDERR_FILENO);
    }
    return;
}

static void
init_random()
{
    struct timeval 	tv;
    
    gettimeofday(&tv, 0);
    srandom(tv.tv_usec);
    return;
}

static void
usage(char * progname)
{
    fprintf(stderr, "usage:\n"
	    "%s -i <if_name> [ -u <uid> ] [ -g <gid> ]\n",
	    progname);
    exit(EX_USAGE);
}

int
main(int argc, char * argv[1])
{
    char			ch;
    CFDictionaryRef		dict = NULL;
    char *			config_file = NULL;
    bool			d_flag = FALSE;
    int				fd;
    bool			g_flag = FALSE;
    gid_t			gid = 0;
    char *			if_name = NULL;
    LinkAddressesRef		link_addrs = NULL;
    struct sockaddr_dl *	link = NULL;
    bool			n_flag = FALSE;
    bool			u_flag = FALSE;
    uid_t			uid = 0;

    link_addrs = LinkAddresses_create();
    if (link_addrs == NULL) {
	printf("Could not build interface list\n");
	exit(EX_OSERR);
    }
    while ((ch = getopt(argc, argv, "c:di:nu:g:")) != EOF) {
	switch ((char) ch) {
	case 'c':
	    config_file = optarg;
	    break;

	case 'd':
	    d_flag = TRUE;
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
	case 'n':
	    n_flag = TRUE;
	    break;
	default:
	    usage(argv[0]);
	    break;
	}
    }
    if ((argc - optind) != 0 || if_name == NULL) {
	usage(argv[0]);
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
    if (d_flag == FALSE) {
	setup_log_file(if_name, uid, gid);
    }
    fd = eapol_socket(if_name, FALSE);
    if (fd < 0) {
	my_log(LOG_NOTICE, "eapol_socket(%s) failed", if_name);
    }
    if (g_flag) {
	if (setgid(gid) < 0) {
	    syslog(LOG_NOTICE, "setgid(%d) failed, %m", gid);
	    exit(EX_NOPERM);
	}
    }
    if (u_flag) {
	if (setuid(uid) < 0) {
	    syslog(LOG_NOTICE, "setuid(%d) failed, %m", uid);
	    exit(EX_NOPERM);
	}
    }
    if (config_file != NULL) {
	dict = (CFDictionaryRef)my_CFPropertyListCreateFromFile(config_file);
	if (isA_CFDictionary(dict) == NULL) {
	    fprintf(stderr, "contents of file %s invalid\n", config_file);
	    my_CFRelease(&dict);
	    exit (EX_CONFIG);
	}
    }
    if (S_load_modules() != kEAPClientModuleStatusOK) {
	exit(EX_SOFTWARE);
    }

    /* the Supplicant owns the file descriptor */
    supp = Supplicant_create(fd, link);
    if (supp == NULL) {
	syslog(LOG_NOTICE, "Supplicant_create failed");
	exit(EX_UNAVAILABLE);
    }
    if (n_flag) {
	Supplicant_set_no_ui(supp);
    }
    if (Supplicant_attached(supp)) {
	(void)setsid();
	(void)chdir("/");
    }
    else {
	(void)Supplicant_update_configuration(supp, dict);
	my_CFRelease(&dict);
    }
    if (d_flag) {
	Supplicant_set_debug(supp, TRUE);
	EAPOLSocketSetDebug(TRUE);
    }
    init_random();
    Supplicant_start(supp);
    
    LinkAddresses_free(&link_addrs);
    CFRunLoopRun();

    exit(EX_OK);
}
