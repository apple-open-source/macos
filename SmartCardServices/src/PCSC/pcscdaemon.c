/*
 *  Copyright (c) 2000-2007 Apple Inc. All Rights Reserved.
 * 
 *  @APPLE_LICENSE_HEADER_START@
 *  
 *  This file contains Original Code and/or Modifications of Original Code
 *  as defined in and that are subject to the Apple Public Source License
 *  Version 2.0 (the 'License'). You may not use this file except in
 *  compliance with the License. Please obtain a copy of the License at
 *  http://www.opensource.apple.com/apsl/ and read it before using this
 *  file.
 *  
 *  The Original Code and all software distributed under the License are
 *  distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 *  Please see the License for the specific language governing rights and
 *  limitations under the License.
 *  
 *  @APPLE_LICENSE_HEADER_END@
 */

/*
 *  pcscdaemon.c
 *  SmartCardServices
 */

/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 1999-2005
 *  David Corcoran <corcoran@linuxnet.com>
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id: pcscdaemon.c 2377 2007-02-05 13:13:56Z rousseau $
 */

/**
 * @file
 * @brief This is the main pcscd daemon.
 *
 * The function \c main() starts up the communication environment.\n
 * Then an endless loop is calld to look for Client connections. For each
 * Client connection a call to \c CreateContextThread() is done.
 */

#include "config.h"
#include <time.h>
#include <syslog.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#include "wintypes.h"
#include "pcsclite.h"
#include "debuglog.h"
#include "winscard_msg.h"
#include "winscard_svc.h"
#include "sys_generic.h"
#include "thread_generic.h"
#include "hotplug.h"
#include "readerfactory.h"
#include "configfile.h"
#include "powermgt_generic.h"

#include <security_utilities/debugging.h>

char AraKiri = 0;
int respawn = 0;
static char Init = 1;
int HPForceReaderPolling = 0;

char **globalArgv;

/*
 * Some internal functions
 */
void SVCServiceRunLoop(void);
void SVCClientCleanup(psharedSegmentMsg);
void at_exit(void);
void clean_temp_files(void);
void signal_reload(int sig);
void signal_respawn(int sig);
void signal_trap(int);
void print_version (void);
void print_usage (char const * const);
int ProcessHotplugRequest();
void tryRespawn();

PCSCLITE_MUTEX usbNotifierMutex;

#ifdef USE_RUN_PID
pid_t GetDaemonPid(void);
pid_t GetDaemonPid(void)
{
	FILE *f;
	pid_t pid;

	/* pids are only 15 bits but 4294967296
	 * (32 bits in case of a new system use it) is on 10 bytes
	 */
	if ((f = fopen(USE_RUN_PID, "rb")) != NULL)
	{
#define PID_ASCII_SIZE 11
		char pid_ascii[PID_ASCII_SIZE];

		fgets(pid_ascii, PID_ASCII_SIZE, f);
		fclose(f);

		pid = atoi(pid_ascii);
	}
	else
	{
		Log2(PCSC_LOG_CRITICAL, "Can't open " USE_RUN_PID ": %s",
			strerror(errno));
		return -1;
	}

	return pid;
} /* GetDaemonPid */
#endif

int SendHotplugSignal(void)
{
#ifdef USE_RUN_PID
	pid_t pid;

	pid = GetDaemonPid();

	if (pid != -1)
	{
		Log2(PCSC_LOG_INFO, "Send hotplug signal to pcscd (pid=%d)", pid);
		if (kill(pid, SIGUSR1) < 0)
		{
			Log3(PCSC_LOG_CRITICAL, "Can't signal pcscd (pid=%d): %s",
				pid, strerror(errno));
			return EXIT_FAILURE ;
		}
	}
#endif

	return EXIT_SUCCESS;
} /* SendHotplugSignal */

int ProcessHotplugRequest()
{
#ifdef USE_RUN_PID

	/* read the pid file to get the old pid and test if the old pcscd is
	 * still running
	 */
	if (GetDaemonPid() != -1)
		return SendHotplugSignal();

	Log1(PCSC_LOG_CRITICAL, "file " USE_RUN_PID " does not exist");
	Log1(PCSC_LOG_CRITICAL,	"Perhaps pcscd is not running?");
#else
	struct stat tmpStat;
	if (SYS_Stat(PCSCLITE_CSOCK_NAME, &tmpStat) == 0)	// socket file exists, so maybe pcscd is running
		return SendHotplugSignal();
	Log1(PCSC_LOG_CRITICAL, "pcscd was not configured with --enable-runpid=FILE");
#endif
	Log1(PCSC_LOG_CRITICAL, "Hotplug failed");
	return EXIT_FAILURE;
}

/*
 * Cleans up messages still on the queue when a client dies
 */
void SVCClientCleanup(psharedSegmentMsg msgStruct)
{
	/*
	 * May be implemented in future releases
	 */
}

/**
 * @brief The Server's Message Queue Listener function.
 *
 * An endless loop calls the function \c SHMProcessEventsServer() to check for
 * messages sent by clients.
 * If the message is valid, \c CreateContextThread() is called to serve this
 * request.
 */
void SVCServiceRunLoop(void)
{
	int rsp;
	LONG rv;
	DWORD dwClientID;	/* Connection ID used to reference the Client */

	rsp = 0;
	rv = 0;

	/*
	 * Initialize the comm structure
	 */
	rsp = SHMInitializeCommonSegment();

	if (rsp == -1)
	{
		Log1(PCSC_LOG_CRITICAL, "Error initializing pcscd.");
		exit(-1);
	}

	/*
	 * Initialize the contexts structure
	 */
	rv = ContextsInitialize();

	if (rv == -1)
	{
		Log1(PCSC_LOG_CRITICAL, "Error initializing pcscd.");
		exit(-1);
	}

	/*
	 * Solaris sends a SIGALRM and it is annoying
	 */

	signal(SIGALRM, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);	/* needed for Solaris. The signal is sent
				 * when the shell is existed */

	/*
	 * This function always returns zero
	 */
	rsp = SYS_MutexInit(&usbNotifierMutex);

	/*
	 * Set up the search for USB/PCMCIA devices
	 */
	HPSearchHotPluggables();
	HPRegisterForHotplugEvents();

	/*
	 * Set up the power management callback routine
	 */
//	PMRegisterForPowerEvents();

	while (1)
	{
		switch (rsp = SHMProcessEventsServer(&dwClientID, 0))
		{

		case 0:
			Log2(PCSC_LOG_DEBUG, "A new context thread creation is requested: %d", dwClientID);
			rv = CreateContextThread(&dwClientID);

 			if (rv != SCARD_S_SUCCESS)
			{
				Log1(PCSC_LOG_ERROR, "Problem during the context thread creation");
				AraKiri = 1;
			}

			break;

		case 2:
			/*
			 * timeout in SHMProcessEventsServer(): do nothing
			 * this is used to catch the Ctrl-C signal at some time when
			 * nothing else happens
			 */
			break;

		case -1:
			Log1(PCSC_LOG_ERROR, "Error in SHMProcessEventsServer");
			break;

		case -2:
			/* Nothing to do in case of a syscall interrupted
			 * It happens when SIGUSR1 (reload) or SIGINT (Ctrl-C) is received
			 * We just try again */
			break;

		default:
			Log2(PCSC_LOG_ERROR, "SHMProcessEventsServer unknown retval: %d",
				rsp);
			break;
		}

		if (AraKiri)
		{
			/* stop the hotpug thread and waits its exit */
			Log1(PCSC_LOG_ERROR, "Preparing to exit...");
			HPStopHotPluggables();
			SYS_Sleep(1);

			/* now stop all the drivers */
			int shouldExit = !respawn;
			RFCleanupReaders(shouldExit);
		}
		if (respawn)
		{
			HPCancelHotPluggables();
			HPJoinHotPluggables();
			clean_temp_files();
			tryRespawn();
		}
	}
}

int main(int argc, char **argv)
{
	int rv;
	char setToForeground;
	char HotPlug;
	char *newReaderConfig;
	struct stat fStatBuf;
	int opt;
#ifdef HAVE_GETOPT_LONG
	int option_index = 0;
	static struct option long_options[] = {
		{"config", 1, 0, 'c'},
		{"foreground", 0, 0, 'f'},
		{"help", 0, 0, 'h'},
		{"version", 0, 0, 'v'},
		{"apdu", 0, 0, 'a'},
		{"debug", 0, 0, 'd'},
		{"info", 0, 0, 0},
		{"error", 0, 0, 'e'},
		{"critical", 0, 0, 'C'},
		{"hotplug", 0, 0, 'H'},
		{"force-reader-polling", optional_argument, 0, 0},
		{0, 0, 0, 0}
	};
#endif
#define OPT_STRING "c:fdhvaeCH"

	rv = 0;
	newReaderConfig = NULL;
	setToForeground = 0;
	HotPlug = 0;
	globalArgv = argv;
	
	/*
	 * test the version
	 */
	if (strcmp(PCSCLITE_VERSION_NUMBER, VERSION) != 0)
	{
		printf("BUILD ERROR: The release version number PCSCLITE_VERSION_NUMBER\n");
		printf("  in pcsclite.h (%s) does not match the release version number\n",
			PCSCLITE_VERSION_NUMBER);
		printf("  generated in config.h (%s) (see configure.in).\n", VERSION);

		return EXIT_FAILURE;
	}

	/*
	 * By default we create a daemon (not connected to any output)
	 * The log will go to wherever securityd log output goes.
	 */
	DebugLogSetLogType(DEBUGLOG_NO_DEBUG);

	/*
	 * Handle any command line arguments
	 */
#ifdef  HAVE_GETOPT_LONG
	while ((opt = getopt_long (argc, argv, OPT_STRING, long_options, &option_index)) != -1) {
#else
	while ((opt = getopt (argc, argv, OPT_STRING)) != -1) {
#endif
		switch (opt) {
#ifdef  HAVE_GETOPT_LONG
			case 0:
				if (strcmp(long_options[option_index].name,
					"force-reader-polling") == 0)
					HPForceReaderPolling = optarg ? abs(atoi(optarg)) : 1;
				break;
#endif
			case 'c':
				Log2(PCSC_LOG_INFO, "using new config file: %s", optarg);
				newReaderConfig = optarg;
				break;

			case 'f':
				setToForeground = 1;
				/* debug to stderr instead of default syslog */
				Log1(PCSC_LOG_INFO,
					"pcscd set to foreground with debug send to stderr");
				break;

			case 'd':
				DebugLogSetLogType(DEBUGLOG_STDERR_DEBUG);
				DebugLogSetLevel(PCSC_LOG_DEBUG);
				break;

			case 'e':
				DebugLogSetLogType(DEBUGLOG_STDERR_DEBUG);
				DebugLogSetLevel(PCSC_LOG_ERROR);
				break;

			case 'C':
				DebugLogSetLogType(DEBUGLOG_STDERR_DEBUG);
				DebugLogSetLevel(PCSC_LOG_CRITICAL);
				break;

			case 'h':
				print_usage (argv[0]);
				return EXIT_SUCCESS;

			case 'v':
				print_version ();
				return EXIT_SUCCESS;

			case 'a':
				DebugLogSetCategory(DEBUG_CATEGORY_APDU);
				break;

			case 'H':
				/* debug to stderr instead of default syslog */
				DebugLogSetLogType(DEBUGLOG_STDERR_DEBUG);
				HotPlug = 1;
				break;

			default:
				print_usage (argv[0]);
				return EXIT_FAILURE;
		}

	}

	if (argv[optind])
	{
		printf("Unknown option: %s\n\n", argv[optind]);
		print_usage(argv[0]);
		return EXIT_SUCCESS;
	}

	/*
		If this run of pcscd has the hotplug option, just send a signal to the
		running one and exit
	*/
	
	if (HotPlug)
		return ProcessHotplugRequest();

	/*
	 * test the presence of /var/run/pcsc.comm
	 */

	rv = SYS_Stat(PCSCLITE_CSOCK_NAME, &fStatBuf);

	if (rv == 0)
	{
#ifdef USE_RUN_PID
		pid_t pid;

		/* read the pid file to get the old pid and test if the old pcscd is
		 * still running
		 */
		pid = GetDaemonPid();

		if (pid != -1)
		{
			if (kill(pid, 0) == 0)
			{
				Log2(PCSC_LOG_CRITICAL,
					"Another pcscd (pid: %d) seems to be running.", pid);
				Log1(PCSC_LOG_CRITICAL,
					"Remove " USE_RUN_PID " if pcscd is not running to clear this message.");
				return EXIT_FAILURE;
			}
			else
				/* the old pcscd is dead. Do some cleanup */
				clean_temp_files();
		}
#else
		{
			Log1(PCSC_LOG_CRITICAL,
				"file " PCSCLITE_CSOCK_NAME " already exists.");
			Log1(PCSC_LOG_CRITICAL,
				"Maybe another pcscd is running?");
			Log1(PCSC_LOG_CRITICAL,
				"Remove " PCSCLITE_CSOCK_NAME "if pcscd is not running to clear this message.");
			return EXIT_FAILURE;
		}
#endif
	}

	/*
	 * If this is set to one the user has asked it not to fork
	 */
	if (!setToForeground)
	{
		if (SYS_Daemon(0, 0))
			Log2(PCSC_LOG_CRITICAL, "SYS_Daemon() failed: %s",
				strerror(errno));
	}

	/*
	 * cleanly remove /tmp/pcsc when exiting
	 */
	signal(SIGQUIT, signal_trap);
	signal(SIGTERM, signal_trap);
	signal(SIGINT, signal_trap);
	signal(SIGHUP, signal_trap);

#ifdef USE_RUN_PID
	/*
	 * Record our pid to make it easier
	 * to kill the correct pcscd
	 */
	{
		FILE *f;

		if ((f = fopen(USE_RUN_PID, "wb")) != NULL)
		{
			fprintf(f, "%u\n", (unsigned) getpid());
			fclose(f);
		}
	}
#endif

	/*
	 * If PCSCLITE_IPC_DIR does not exist then create it
	 */
	rv = SYS_Stat(PCSCLITE_IPC_DIR, &fStatBuf);
	if (rv < 0)
	{
		rv = SYS_Mkdir(PCSCLITE_IPC_DIR, S_ISVTX | S_IRWXO | S_IRWXG | S_IRWXU);
		if (rv != 0)
		{
			Log2(PCSC_LOG_CRITICAL,
				"cannot create " PCSCLITE_IPC_DIR ": %s", strerror(errno));
			return EXIT_FAILURE;
		}
	}

	/* cleanly remove /var/run/pcsc.* files when exiting */
	if (atexit(at_exit))
		Log2(PCSC_LOG_CRITICAL, "atexit() failed: %s", strerror(errno));

	/*
	 * Allocate memory for reader structures
	 */
	RFAllocateReaderSpace();

	/*
		Grab the information from the reader.conf. If a file has been specified
		and there is any error, consider it fatal. If no file was explicitly
		specified, ignore if file not present.

		 DBUpdateReaders returns:
		 
		 1	if config file can't be opened
		 -1	if config file is broken
		 0	if all good
	 
		We skip this step if running in 64 bit mode, as serial readers are considered
		legacy code.
	*/

	rv = RFStartSerialReaders(newReaderConfig?newReaderConfig:PCSCLITE_READER_CONFIG);
	if (rv == -1)
	{
		Log3(PCSC_LOG_CRITICAL, "invalid file %s: %s", newReaderConfig,
				strerror(errno));
		at_exit();
	}
	else
	if ((rv == 1) && newReaderConfig)
	{
		Log3(PCSC_LOG_CRITICAL, "file %s can't be opened: %s", 
				 newReaderConfig, strerror(errno));
		at_exit();
	}

	/*
	 * Set the default globals
	 */
	g_rgSCardT0Pci.dwProtocol = SCARD_PROTOCOL_T0;
	g_rgSCardT1Pci.dwProtocol = SCARD_PROTOCOL_T1;
	g_rgSCardRawPci.dwProtocol = SCARD_PROTOCOL_RAW;

	Log1(PCSC_LOG_INFO, "pcsc-lite " VERSION " daemon ready.");

	/*
	 * post initialistion
	 */
	Init = 0;

	/*
	 * signal_trap() does just set a global variable used by the main loop
	 */
	signal(SIGQUIT, signal_trap);
	signal(SIGTERM, signal_trap);
	signal(SIGINT, signal_trap);
	signal(SIGHUP, signal_trap);

	signal(SIGUSR1, signal_reload);
	signal(SIGUSR2, signal_respawn);

	SVCServiceRunLoop();

	Log1(PCSC_LOG_ERROR, "SVCServiceRunLoop returned");
	return EXIT_FAILURE;
}

void at_exit(void)
{
	Log1(PCSC_LOG_INFO, "cleaning " PCSCLITE_IPC_DIR);

	clean_temp_files();

	SYS_Exit(EXIT_SUCCESS);
}

void clean_temp_files(void)
{
	int rv;

	rv = SYS_Unlink(PCSCLITE_CSOCK_NAME);
	if (rv != 0)
		Log2(PCSC_LOG_ERROR, "Cannot unlink " PCSCLITE_CSOCK_NAME ": %s",
			strerror(errno));

#ifdef USE_RUN_PID
	rv = SYS_Unlink(USE_RUN_PID);
	if (rv != 0)
		Log2(PCSC_LOG_ERROR, "Cannot unlink " USE_RUN_PID ": %s",
			strerror(errno));
#endif
}

void signal_reload(int sig)
{
	static int rescan_ongoing = 0;

	if (AraKiri)
		return;

	Log1(PCSC_LOG_INFO, "Reload serial configuration");
	if (rescan_ongoing)
	{
		Log1(PCSC_LOG_INFO, "Rescan already ongoing");
		return;
	}

	rescan_ongoing = 0;

	HPReCheckSerialReaders();

	rescan_ongoing = 0;
	Log1(PCSC_LOG_INFO, "End reload serial configuration");
} /* signal_reload */

void signal_trap(int sig)
{
	/* the signal handler is called several times for the same Ctrl-C */
	if (AraKiri == 0)
	{
		Log1(PCSC_LOG_INFO, "Preparing for suicide");
		AraKiri = 1;

		/* if still in the init/loading phase the AraKiri will not be
		 * seen by the main event loop
		 */
		if (Init)
		{
			Log1(PCSC_LOG_INFO, "Suicide during init");
			at_exit();
		}
	}
}

void signal_respawn(int sig)
{
	Log1(PCSC_LOG_INFO, "Got signal to respawn in 32 bit mode");
	AraKiri = 1;
	respawn = 1;
}

#if MAX_OS_X_VERSION_MIN_REQUIRED <= MAX_OS_X_VERSION_10_5
	#include <spawn.h>
	#include <err.h>
	#include <CoreFoundation/CFBundle.h>
	#include <CoreFoundation/CFNumber.h>
#endif
	
extern char **environ;

void tryRespawn()
{
#if MAX_OS_X_VERSION_MIN_REQUIRED <= MAX_OS_X_VERSION_10_5
	/* now try respawn */
	static cpu_type_t only32cpu[] = { CPU_TYPE_I386 };
	const size_t only32cpuSize = (sizeof(only32cpu) / sizeof(cpu_type_t));
	
	int rx;
	posix_spawnattr_t attr;
	if ((rx = posix_spawnattr_init(&attr)) != 0) 
		errc(1, rx, "posix_spawnattr_init");
	
	if ((rx = posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETEXEC)) != 0) 
		errc(1, rx, "posix_spawnattr_setflags");
	
	size_t copied = 0;
	if ((rx = posix_spawnattr_setbinpref_np(&attr, only32cpuSize, only32cpu, &copied)) != 0) 
		errc(1, rx, "posix_spawnattr_setbinpref_np");
	
	if (copied != only32cpuSize)
		errx(1, "posix_spawnattr_setbinpref_np only copied %d of %d", (int)copied, only32cpuSize);
	
	pid_t pid = 0;
    rx = posix_spawn(&pid, globalArgv[0], NULL, &attr, globalArgv, environ);
	errc(1, rx, "posix_spawn: %s", globalArgv[0]);
#else
	/* we shouldn't get here, but if we do, we are in no state to continue */
	Log1(PCSC_LOG_INFO, "Unexpected call to tryRespawn");
	at_exit();
#endif
}	
	
void print_version (void)
{
	printf("%s version %s.\n",  PACKAGE, VERSION);
	printf("Copyright (C) 1999-2002 by David Corcoran <corcoran@linuxnet.com>.\n");
	printf("Copyright (C) 2001-2005 by Ludovic Rousseau <ludovic.rousseau@free.fr>.\n");
	printf("Copyright (C) 2003-2004 by Damien Sauveron <sauveron@labri.fr>.\n");
	printf("Portions Copyright (C) 2000-2007 by Apple Inc.\n");
	printf("Report bugs to <sclinux@linuxnet.com>.\n");
}

void print_usage (char const * const progname)
{
	printf("Usage: %s options\n", progname);
	printf("Options:\n");
#ifdef HAVE_GETOPT_LONG
	printf("  -a, --apdu		log APDU commands and results\n");
	printf("  -c, --config		path to reader.conf\n");
	printf("  -f, --foreground	run in foreground (no daemon),\n");
	printf("			send logs to stderr instead of syslog\n");
	printf("  -h, --help		display usage information\n");
	printf("  -H, --hotplug		ask the daemon to rescan the available readers\n");
	printf("  -v, --version		display the program version number\n");
	printf("  -d, --debug	 	display lower level debug messages\n");
	printf("      --info	 	display info level debug messages (default level)\n");
	printf("  -e  --error	 	display error level debug messages\n");
	printf("  -C  --critical 	display critical only level debug messages\n");
	printf("  --force-reader-polling ignore the IFD_GENERATE_HOTPLUG reader capability\n");
#else
	printf("  -a    log APDU commands and results\n");
	printf("  -c 	path to reader.conf\n");
	printf("  -f	run in foreground (no daemon), send logs to stderr instead of syslog\n");
	printf("  -d 	display debug messages. Output may be:\n");
	printf("  -h 	display usage information\n");
	printf("  -H	ask the daemon to rescan the avaiable readers\n");
	printf("  -v 	display the program version number\n");
#endif
}

