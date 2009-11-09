/* $Id: main.c,v 1.14.2.3 2005/11/06 17:18:26 monas Exp $ */

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sysctl.h>

#include <netinet/in.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <paths.h>
#include <err.h>
#include <launch.h>

/*
 * If we're using a debugging malloc library, this may define our
 * wrapper stubs.
 */
#define	RACOON_MAIN_PROGRAM
#include "gcmalloc.h"

#include "var.h"
#include "misc.h"
#include "vmbuf.h"
#include "plog.h"
#include "debug.h"

#include "cfparse_proto.h"
#include "isakmp_var.h"
#ifdef ENABLE_HYBRID
#include <resolv.h>
#include "isakmp.h"
#include "isakmp_xauth.h"
#include "isakmp_cfg.h"
#endif
#include "remoteconf.h"
#include "localconf.h"
#include "session.h"
#include "oakley.h"
#include "pfkey.h"
#include "policy.h"
#include "crypto_openssl.h"
#include "backupsa.h"
#include "vendorid.h"

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#endif

//#include "package_version.h"

int f_local = 0;	/* local test mode.  behave like a wall. */
int vflag = 1;		/* for print-isakmp.c */
static int loading_sa = 0;	/* install sa when racoon boots up. */
static int dump_config = 0;	/* dump parsed config file. */
static int exec_done = 0;	/* we've already been exec'd */

#ifdef TOP_PACKAGE
static char version[] = "@(#)" TOP_PACKAGE_STRING " (" TOP_PACKAGE_URL ")";
#else /* TOP_PACKAGE */
static char version[] = "@(#) racoon / IPsec-tools";
#endif /* TOP_PACKAGE */

int main __P((int, char **));
static void usage __P((void));
static void parse __P((int, char **));
static void restore_params __P((void));
static void save_params __P((void));
static void saverestore_params __P((int));
static void cleanup_pidfile __P((void));
static int launchedbylaunchd(void);

pid_t racoon_pid = 0;
int print_pid = 1;	/* for racoon only */

void
usage()
{
	printf("usage: racoon [-BdFvs%s] %s[-f (file)] [-l (file)] [-p (port)]\n",
#ifdef INET6
		"46",
#else
		"",
#endif
#ifdef ENABLE_ADMINPORT
		"[-a (port)] "
#else
		""
#endif
		);
	printf("   -B: install SA to the kernel from the file "
		"specified by the configuration file.\n");
	printf("   -d: debug level, more -d will generate more debug message.\n");
	printf("   -C: dump parsed config file.\n");
	printf("   -L: include location in debug messages\n");
	printf("   -F: run in foreground, do not become daemon.\n");
	printf("   -v: be more verbose\n");
	printf("   -s: override enable auto exit\n");
#ifdef INET6
	printf("   -4: IPv4 mode.\n");
	printf("   -6: IPv6 mode.\n");
#endif
#ifdef ENABLE_ADMINPORT
	printf("   -a: port number for admin port.\n");
#endif
	printf("   -f: pathname for configuration file.\n");
	printf("   -l: pathname for log file.\n");
	printf("   -p: port number for isakmp (default: %d).\n", PORT_ISAKMP);
	printf("   -P: port number for NAT-T (default: %d).\n", PORT_ISAKMP_NATT);
	exit(1);
}

int
main(ac, av)
	int ac;
	char **av;
{
	int error;
	char				logFileStr[MAXPATHLEN+1];

	if (geteuid() != 0) {
		errx(1, "must be root to invoke this program.");
		/* NOTREACHED*/
	}

	/*
	 * Don't let anyone read files I write.  Although some files (such as
	 * the PID file) can be other readable, we dare to use the global mask,
	 * because racoon uses fopen(3), which can't specify the permission
	 * at the creation time.
	 */
	umask(077);
	if (umask(077) != 077) {
		errx(1, "could not set umask");
		/* NOTREACHED*/
	}

#ifdef DEBUG_RECORD_MALLOCATION
	DRM_init();
#endif

	logFileStr[0] = 0;

	eay_init();
	initlcconf();
	initrmconf();
	oakley_dhinit();
	compute_vendorids();

	parse(ac, av);
	
	#ifdef __APPLE__
	/*
	 * Check IPSec plist
	 */
	{
		SCPreferencesRef 	prefs = NULL;
		CFPropertyListRef	globals;
		CFStringRef			logFileRef;
		CFNumberRef			debugLevelRef;
		
		int					level = 0;
		
		logFileStr[0] = 0;
		   
	    if ((prefs = SCPreferencesCreate(0, CFSTR("racoon"), CFSTR("com.apple.ipsec.plist"))) == NULL)
			goto skip;
		globals = SCPreferencesGetValue(prefs, CFSTR("Global"));
		if (!globals || (CFGetTypeID(globals) != CFDictionaryGetTypeID()))
			goto skip;
		debugLevelRef = CFDictionaryGetValue(globals, CFSTR("DebugLevel"));
		if (!debugLevelRef || (CFGetTypeID(debugLevelRef) != CFNumberGetTypeID()))
			goto skip;
		CFNumberGetValue(debugLevelRef, kCFNumberSInt32Type, &level);
		switch (level)
		{
			case 0:
				loglevel = 5;
				goto skip;
				break;
			case 1:
				loglevel = 6;
				break;
			case 2:
				loglevel = 7;
				break;
			default:
				break; /* invalid - ignore */
		}
		
		logFileRef = CFDictionaryGetValue(globals, CFSTR("DebugLogfile"));
		if (!logFileRef	|| (CFGetTypeID(logFileRef) != CFStringGetTypeID())) {	
			goto skip;
		}
		CFStringGetCString(logFileRef, logFileStr, MAXPATHLEN, kCFStringEncodingMacRoman);
skip:
		if (prefs)
			CFRelease(prefs);
	}
	
	if (logFileStr[0])
			plogset(logFileStr);
	else	
#endif /* __APPLE__ */
		if (lcconf->logfile_param)
			plogset(lcconf->logfile_param);			

	ploginit();

	plog(LLV_INFO, LOCATION, NULL, "***** racoon started: pid=%d  started by: %d\n", getpid(), getppid());
	plog(LLV_INFO, LOCATION, NULL, "%s\n", version);
	plog(LLV_INFO, LOCATION, NULL, "@(#)"
	    "This product linked %s (http://www.openssl.org/)"
	    "\n", eay_version());
	plog(LLV_INFO, LOCATION, NULL, "Reading configuration from \"%s\"\n", 
	    lcconf->racoon_conf);

	if (pfkey_init() < 0) {
		errx(1, "something error happened "
			"while pfkey initializing.");
		/* NOTREACHED*/
	}

#ifdef ENABLE_HYBRID
	if (isakmp_cfg_init(ISAKMP_CFG_INIT_COLD))
		errx(1, "could not initialize ISAKMP mode config structures");
#endif

#ifdef HAVE_LIBLDAP
	if (xauth_ldap_init() != 0)
		errx(1, "could not initialize libldap");
#endif

	/*
	 * in order to prefer the parameters by command line,
	 * saving some parameters before parsing configuration file.
	 */
	save_params();
	error = cfparse();
	if (error != 0)
		errx(1, "failed to parse configuration file.");
	restore_params();
	
	if (lcconf->logfile_param == NULL && logFileStr[0] == 0)
		plogreset(lcconf->pathinfo[LC_PATHTYPE_LOGFILE]);
		
#ifdef ENABLE_NATT
	/* Tell the kernel which port to use for UDP encapsulation */
	{
		int udp_port = PORT_ISAKMP_NATT;
		if (sysctlbyname("net.inet.ipsec.esp_port", NULL, NULL, &udp_port, sizeof(udp_port)) != 0)
			errx(1, "couldn't set net.inet.ipsec.esp_port to %d. (%s)",
				udp_port, strerror(errno));
	}
#endif

#ifdef HAVE_LIBRADIUS
	if (xauth_radius_init() != 0) {
		errx(1, "could not initialize libradius");
		/* NOTREACHED*/
	}
#endif

#ifdef ENABLE_HYBRID
	if(isakmp_cfg_config.network4 && isakmp_cfg_config.pool_size == 0)
		if ((error = isakmp_cfg_resize_pool(ISAKMP_CFG_MAX_CNX)) != 0)
			return error;
#endif

	if (dump_config)
		dumprmconf ();

	/*
	 * install SAs from the specified file.  If the file is not specified
	 * by the configuration file, racoon will exit.
	 */
	if (loading_sa && !f_local) {
		if (backupsa_from_file() != 0)
			errx(1, "something error happened "
				"SA recovering.");
	}

	if (f_foreground)
		close(0);
	else {
		if ( !exec_done && launchedbylaunchd() ){
			plog(LLV_INFO, LOCATION, NULL,
				 "racoon launched by launchd.\n");
			exec_done = 1;
			if (atexit(cleanup_pidfile) < 0) {
				plog(LLV_ERROR, LOCATION, NULL,
					 "cannot register pidfile cleanup");
			}
		}else {
		
			if (exec_done) {
				if (atexit(cleanup_pidfile) < 0) {
					plog(LLV_ERROR, LOCATION, NULL,
						"cannot register pidfile cleanup");
				}
			} else {
				#define MAX_EXEC_ARGS 32
				
				char *args[MAX_EXEC_ARGS + 2]; /* 2 extra, for '-x' and NULL */
				char *env[1] = {0};	
				int	i;
				
				if (ac > MAX_EXEC_ARGS) {
					plog(LLV_ERROR, LOCATION, NULL,
						"too many arguments.\n");
					exit(1);
				}
				
				if (daemon(0, 0) < 0) {
					errx(1, "failed to be daemon. (%s)",
						strerror(errno));
				}
				
				/* Radar 5129006 - Prevent non-root user from killing racoon
				 * when launched by setuid process
				 */
				if (setuid(0)) {
					plog(LLV_ERROR, LOCATION, NULL,
						"cannot set uid.\n");
					exit(1);
				}
				if (setgid(0)) {
					plog(LLV_ERROR, LOCATION, NULL,
						"cannot set gid.\n");
					exit(1);
				}
				
				/* setup args to re-exec - for CoreFoundation issues */
				args[0] = PATHRACOON;	
				for (i = 1; i < ac; i++)
					args[i] = *(av + i);
				args[ac] = "-x";		/* tells racoon its been exec'd */
				args[ac+1] = 0;
				
				execve(PATHRACOON, args, env);
				plog(LLV_ERROR, LOCATION, NULL,
						"failed to exec racoon. (%s)", strerror(errno));
				exit(1);
			}
		}
	}

	session();
	
	exit(0);
}


static int
launchedbylaunchd(){
	int             launchdlaunched = 1;
	launch_data_t checkin_response = NULL;
	launch_data_t checkin_request = NULL;
	
	/* check in with launchd */
	if ((checkin_request = launch_data_new_string(LAUNCH_KEY_CHECKIN)) == NULL) {
		plog(LLV_ERROR, LOCATION, NULL,
			 "launch_data_new_string fails.\n");
		launchdlaunched = 0;
		goto done;
	}
	if ((checkin_response = launch_msg(checkin_request)) == NULL) {
		plog(LLV_ERROR, LOCATION, NULL,
			 "launch_msg fails.\n");
		launchdlaunched = 0;
		goto done;
	}
	if (LAUNCH_DATA_ERRNO == launch_data_get_type(checkin_response)) {
		plog(LLV_ERROR, LOCATION, NULL,
			 "launch_data_get_type fails errno %d.\n", launch_data_get_errno(checkin_response));
		launchdlaunched = 0;
		goto done;
	}
	
done:
	/* clean up before we leave */
	if ( checkin_request )
		launch_data_free(checkin_request);
	if ( checkin_response )
		launch_data_free(checkin_response);
	return launchdlaunched;
}

static void
cleanup_pidfile()
{
	char pid_file[MAXPATHLEN];
	pid_t p = getpid();

	/* if it's not child process, clean everything */
	if (racoon_pid == p) {
		if (lcconf->pathinfo[LC_PATHTYPE_PIDFILE] == NULL) 
			strlcpy(pid_file, _PATH_VARRUN "racoon.pid", sizeof(pid_file));
		else if (lcconf->pathinfo[LC_PATHTYPE_PIDFILE][0] == '/') 
			strlcpy(pid_file, lcconf->pathinfo[LC_PATHTYPE_PIDFILE], sizeof(pid_file));
		else {
			strlcat(pid_file, _PATH_VARRUN, sizeof(pid_file));
			strlcat(pid_file, lcconf->pathinfo[LC_PATHTYPE_PIDFILE], sizeof(pid_file));
		}
		(void) unlink(pid_file);
	}
}


static void
parse(ac, av)
	int ac;
	char **av;
{
	extern char *optarg;
	extern int optind;
	int c;
#ifdef YYDEBUG
	extern int yydebug;
#endif

	pname = strrchr(*av, '/');
	if (pname)
		pname++;
	else
		pname = *av;

#if 0	/* for debugging */
	loglevel += 2;
	plogset("/tmp/racoon.log");
#endif

	while ((c = getopt(ac, av, "dLFp:P:a:f:l:vsZBCx"
#ifdef YYDEBUG
			"y"
#endif
#ifdef INET6
			"46"
#endif
			)) != -1) {
		switch (c) {
		case 'd':
			loglevel++;
			break;
		case 'L':
			print_location = 1;
			break;
		case 'F':
			printf("Foreground mode.\n");
			f_foreground = 1;
			break;
		case 'p':
			lcconf->port_isakmp = atoi(optarg);
			break;
		case 'P':
			lcconf->port_isakmp_natt = atoi(optarg);
			break;
		case 'a':
#ifdef ENABLE_ADMINPORT
			lcconf->port_admin = atoi(optarg);
			break;
#else
			fprintf(stderr, "%s: the option is disabled "
			    "in the configuration\n", pname);
			exit(1);
#endif
		case 'f':
			lcconf->racoon_conf = optarg;
			break;
		case 'l':
			lcconf->logfile_param = optarg;
			break;
		case 'v':
			vflag++;
			break;
		case 's':
			lcconf->auto_exit_state &= ~LC_AUTOEXITSTATE_CLIENT;	/* override default auto exit state */
			break;
		case 'x':
			exec_done = 1;
			break;
		case 'Z':
			/*
			 * only local test.
			 * To specify -Z option and to choice a appropriate
			 * port number for ISAKMP, you can launch some racoons
			 * on the local host for debug.
			 * pk_sendadd() on initiator side is always failed
			 * even if this flag is used.  Because there is same
			 * spi in the SAD which is inserted by pk_sendgetspi()
			 * on responder side.
			 */
			printf("Local test mode.\n");
			f_local = 1;
			break;
#ifdef YYDEBUG
		case 'y':
			yydebug = 1;
			break;
#endif
#ifdef INET6
		case '4':
			lcconf->default_af = AF_INET;
			break;
		case '6':
			lcconf->default_af = AF_INET6;
			break;
#endif
		case 'B':
			loading_sa++;
			break;
		case 'C':
			dump_config++;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	ac -= optind;
	av += optind;

	if (ac != 0) {
		usage();
		/* NOTREACHED */
	}

	return;
}

static void
restore_params()
{
	saverestore_params(1);
}

static void
save_params()
{
	saverestore_params(0);
}

static void
saverestore_params(f)
	int f;
{
	static u_int16_t s_port_isakmp;
#ifdef ENABLE_ADMINPORT
	static u_int16_t s_port_admin;
#endif

	/* 0: save, 1: restore */
	if (f) {
		lcconf->port_isakmp = s_port_isakmp;
#ifdef ENABLE_ADMINPORT
		lcconf->port_admin = s_port_admin;
#endif
	} else {
		s_port_isakmp = lcconf->port_isakmp;
#ifdef ENABLE_ADMINPORT
		s_port_admin = lcconf->port_admin;
#endif
	}
}
