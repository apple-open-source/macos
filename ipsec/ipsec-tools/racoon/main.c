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
#include "vendorid.h"

#include <CoreFoundation/CoreFoundation.h>
#include "power_mgmt.h"
#include "preferences.h"

//#include "package_version.h"

int f_local = 0;	/* local test mode.  behave like a wall. */
int vflag = 1;		/* for print-isakmp.c */
static int dump_config = 0;	/* dump parsed config file. */
static int exec_done = 0;	/* we've already been exec'd */

#ifdef TOP_PACKAGE
static char version[] = "@(#)" TOP_PACKAGE_STRING " (" TOP_PACKAGE_URL ")";
#else /* TOP_PACKAGE */
static char version[] = "@(#) racoon / IPsec-tools";
#endif /* TOP_PACKAGE */

int main (int, char **);
static void usage (void);
static void parse (int, char **);
static void restore_params (void);
static void save_params (void);
static void saverestore_params (int);
static void cleanup_pidfile (void);
#if 0 // <rdar://problem/9286626>
int launchedbylaunchd (void);
#endif

pid_t racoon_pid = 0;
int   launchdlaunched = 0;
int print_pid = 1;	/* for racoon only */


void
usage()
{
	printf("usage: racoon [-BdDFvs%s] %s[-f (file)] [-l (file)] [-p (port)]\n",
#ifdef INET6
		"46",
#else
		"",
#endif
		""
		);
	printf("   -d: debug level, more -d will generate more debug message.\n");
	printf("   -D: started by LaunchD (implies daemon mode).\n");
	printf("   -C: dump parsed config file.\n");
	printf("   -L: include location in debug messages\n");
	printf("   -F: run in foreground, do not become daemon.\n");
	printf("   -v: be more verbose\n");
	printf("   -s: override enable auto exit\n");
#ifdef INET6
	printf("   -4: IPv4 mode.\n");
	printf("   -6: IPv6 mode.\n");
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

	/*
	 * Check IPSec plist
	 */
	prefsinit();
	ploginit();

	/* 
	 * racoon is not sandboxed on Mac OS.
	 * On embedded, racoon is sandboxed with a seatbelt-profiles entitlement.
	 */

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

#ifdef HAVE_OPENSSL
	eay_init();
#endif
	
	initlcconf();
	initrmconf();
	oakley_dhinit();
	compute_vendorids();

	parse(ac, av);

	plog(ASL_LEVEL_INFO, "***** racoon started: pid=%d  started by: %d, launchdlaunched %d\n", getpid(), getppid(), launchdlaunched);
	plog(ASL_LEVEL_INFO, "%s\n", version);
#ifdef HAVE_OPENSSL
	plog(ASL_LEVEL_INFO, "@(#)"
	    "This product linked %s (http://www.openssl.org/)"
	    "\n", eay_version());
#endif
	plog(ASL_LEVEL_INFO, "Reading configuration from \"%s\"\n", 
	    lcconf->racoon_conf);

    //%%%%% this sould probably be moved to session()
	if (pfkey_init() < 0) {
		errx(1, "failed to initialize pfkey.\n");
		/* NOTREACHED*/
	}

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
		plogresetfile(lcconf->pathinfo[LC_PATHTYPE_LOGFILE]);
		
#ifdef ENABLE_NATT
	/* Tell the kernel which port to use for UDP encapsulation */
	{
		int udp_port = PORT_ISAKMP_NATT;
		if (sysctlbyname("net.inet.ipsec.esp_port", NULL, NULL, &udp_port, sizeof(udp_port)) != 0)
			errx(1, "couldn't set net.inet.ipsec.esp_port to %d. (%s)",
				udp_port, strerror(errno));
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

	if (f_foreground)
		close(0);
	else {
		if ( !exec_done && launchdlaunched ){
			plog(ASL_LEVEL_INFO, 
				 "racoon launched by launchd.\n");
			exec_done = 1;
			if (atexit(cleanup_pidfile) < 0) {
				plog(ASL_LEVEL_ERR, 
					 "cannot register pidfile cleanup");
			}
		}else {
		
			if (exec_done) {
				if (atexit(cleanup_pidfile) < 0) {
					plog(ASL_LEVEL_ERR, 
						"cannot register pidfile cleanup");
				}
			} else {
				#define MAX_EXEC_ARGS 32
				
				char *args[MAX_EXEC_ARGS + 2]; /* 2 extra, for '-x' and NULL */
				char *env[1] = {0};	
				int	i;
				
				if (ac > MAX_EXEC_ARGS) {
					plog(ASL_LEVEL_ERR, 
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
					plog(ASL_LEVEL_ERR, 
						"cannot set uid.\n");
					exit(1);
				}
				if (setgid(0)) {
					plog(ASL_LEVEL_ERR, 
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
				plog(ASL_LEVEL_ERR, 
						"failed to exec racoon. (%s)", strerror(errno));
				exit(1);
			}
		}
	}
    
    
    /* start the session */
	session();
}

#if 0 // <rdar://problem/9286626>
int
launchedbylaunchd(){
	launch_data_t checkin_response = NULL;
    
	if ((checkin_response = launch_socket_service_check_in()) == NULL) {
		plog(LLV_ERROR, LOCATION, NULL,
			 "launch_socket_service_check_in fails.\n");
		launchdlaunched = 0;
		goto done;
	}
	if (LAUNCH_DATA_ERRNO == launch_data_get_type(checkin_response)) {
		plog(LLV_ERROR, LOCATION, NULL,
			 "launch_data_get_type fails errno %d.\n", launch_data_get_errno(checkin_response));
		launchdlaunched = 0;
		goto done;
	}
	launchdlaunched = 1;
done:
	/* clean up before we leave */
	if ( checkin_response )
		launch_data_free(checkin_response);
	return launchdlaunched;
}
#endif

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

	while ((c = getopt(ac, av, "dDLFp:P:a:f:l:vsZBCx"
#ifdef YYDEBUG
			"y"
#endif
#ifdef INET6
			"46"
#endif
			)) != -1) {
		switch (c) {
		case 'd':
			plogsetlevel(ASL_LEVEL_DEBUG);
			break;
		case 'D':
			if (f_foreground) {
				fprintf(stderr, "-D and -F are mutually exclusive\n");
				exit(1);
			}
			launchdlaunched = 1;
			break;
		case 'L':
			print_location = 1;
			break;
		case 'F':
			if (launchdlaunched) {
				fprintf(stderr, "-D and -F are mutually exclusive\n");
				exit(1);
			}
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
			fprintf(stderr, "%s: the option is disabled "
			    "in the configuration\n", pname);
			exit(1);
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

	/* 0: save, 1: restore */
	if (f) {
		lcconf->port_isakmp = s_port_isakmp;
	} else {
		s_port_isakmp = lcconf->port_isakmp;
	}
}
