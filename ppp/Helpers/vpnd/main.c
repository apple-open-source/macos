/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
    This program implements a PPP central server, listening for incoming calls
    and forking/execing pppd processes to handle the incoming sessions.
    This is primarily designed to imnplement the VPN server, hence is name 'vpnd'.
    
    The architecture of this program, and in particular the plugin management, is
    largely inspired by pppd.
    
*/

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <syslog.h>
#include <netdb.h>
#include <utmp.h>
#include <paths.h>
#include <sys/queue.h>
		
#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include "../../Family/if_ppplink.h"

#include "vpnd.h"
#include "vpnoptions.h"
#include "vpnplugins.h"
#include "cf_utils.h"


// ----------------------------------------------------------------------------
//	Globals
// ----------------------------------------------------------------------------
char 		*no_ppp_msg = "This system lacks PPP kernel support\n";
static char	pid_path[MAXPATHLEN] = _PATH_VARRUN DAEMON_NAME "-";	// pid file path - the rest will be filled in later
// ----------------------------------------------------------------------------
//	Private Globals
// ----------------------------------------------------------------------------

static volatile int rcvd_sig_child = 0;
static volatile int rcvd_sig_hup = 0;
static volatile int rcvd_sig_usr1 = 0;
static volatile int terminate = 0;
static int forwarding = -1;
static FILE	*logfile = 0;
static int stdio_is_valid = 1;
static struct vpn_params *params;

static int spawn(struct vpn_params *params);
static void close_file_descriptors(void);
static void redirect_std_file(void);
static void detach(void);
static void write_pid_file(struct vpn_params *params);
static void delete_pid_file(void);
static void sig_chld(int inSignal);
static void sig_term(int inSignal);
static void sig_hup(int inSignal);
static void setup_signal_handlers(void);
static int set_forwarding(int *oldval, int newval);
static void create_log_file(char *inLogPath);
static void close_log_file(void);
void vpnlog(int nSyslogPriority, char *format_str, ...);
static void dump_params(struct vpn_params *params);

// ----------------------------------------------------------------------------
//	main
// ----------------------------------------------------------------------------
int main (int argc, char *argv[])
{
    
    int 		i;

    /*
     * Check that we are running as root.
     */
    if (geteuid() != 0) {
	fprintf(stderr, "must be root to run %s, since it is not setuid-root\n", argv[0]);
	exit(EXIT_NOT_ROOT);
    }
    

    params = (struct vpn_params*)malloc(sizeof (struct vpn_params));
    if (params == 0)
        exit(EXIT_FATAL_ERROR);
    bzero(params, sizeof(params));

    // Close all non-standard file descriptors.
    close_file_descriptors();

    /*
     * Ensure that fds 0, 1, 2 are open, to /dev/null if nowhere else.
     * This way we can close 0, 1, 2 in detach() without clobbering
     * a fd that we are using.
     */
    if ((i = open("/dev/null", O_RDWR)) >= 0) {
	while (0 <= i && i <= 2)
	    i = dup(i);
	if (i >= 0)
	    close(i);
    }

    /*
     * open syslog facility.
     */
    openlog("vpnd", LOG_PID | LOG_NDELAY, LOG_RAS);

    /*
     * read and process options.
     */
    if (process_options(params, argc, argv))
        exit(EXIT_OPTION_ERROR);
		   
    /* Check if ppp is available in the kernel */
    if (!ppp_available()) {
	vpnlog(LOG_ERR, "The PPP kernel extension could not be loaded\n");
	exit(EXIT_NO_KERNEL_SUPPORT);
    }  
    
    // if no server id option - read active server list and
    // launch a vpnd process for each
    if (params->server_id == 0) {
        close_file_descriptors();   // close all but std fd
        redirect_std_file();        // redirect std
        spawn(params);
        exit(EXIT_OK);
    }
 
    vpnlog(LOG_NOTICE, "Server '%s' starting...\n", params->server_id);

    open_dynamic_store(params); 	// open a connection to the dynamic store     
    init_address_lists();		// init the address lists

    if (process_prefs(params)) {	// prepare launch args
    	vpnlog(LOG_ERR, "Error processing prefs file\n");
        exit(EXIT_OPTION_ERROR);
    }
    if (check_conflicts(params))	// check if another server of this type already running
        exit(EXIT_OPTION_ERROR);
    
    if (kill_orphans(params))		// check for and kill orphan pppd processes running with the same
        exit(EXIT_FATAL_ERROR);		// server id left over from a crashed vpnd process.
    
    if (params->log_path)
   	create_log_file(params->log_path);
    
    if (init_plugin(params)) {
        vpnlog(LOG_ERR, "Initialization of vpnd plugin failed\n");
        exit(EXIT_FATAL_ERROR);
    }
    if (get_plugin_args(params, 0)) {		
    	vpnlog(LOG_ERR, "Error getting arguments from plugin\n");
        exit(EXIT_OPTION_ERROR);
    }

    if (params->debug)
        dump_params(params);
        
    setuid(geteuid());
    
    // OK, everything looks good. Daemonize now and redirect std fd's.
    if (params->daemonize) {
        stdio_is_valid = 0;
		close_dynamic_store(params);	// close it now, re-open after detach
        detach();
        setsid();
        redirect_std_file();
        open_dynamic_store(params);	// re-open after detach
        vpnlog(LOG_NOTICE, "Server '%s' moved to background\n", params->server_id);
    }
    
    setup_signal_handlers();
    write_pid_file(params);	/* Write out our pid like a good little daemon */
    publish_state(params);
            
    /* activate IP forwarding */
    if (set_forwarding(&forwarding, 1))
        vpnlog(LOG_ERR, "Cannot activate IP forwarding (error %d)\n", errno);

    /* now listen for connections*/
    vpnlog(LOG_NOTICE, "Listening for connections...\n");
    accept_connections(params);
        
    /* restore IP forwarding to former state */
    if (set_forwarding(0, forwarding))
        vpnlog(LOG_ERR, "Cannot reset IP forwarding (error %d)\n", errno);

    close_log_file();
    delete_pid_file();
    vpnlog(LOG_NOTICE, "Server '%s' stopped\n", params->server_id);

    exit(EXIT_OK) ;
}

//-----------------------------------------------------------------------------
// 	spawn - launch a copy of vpnd for each active server id
//-----------------------------------------------------------------------------
static int spawn(struct vpn_params *params)
{
    int             i, j, count;
    char            server[OPT_STR_LEN];
    pid_t           pidChild = 0;
    char            *args[6];
    char            *vpnd_prog = VPND_PRGM;
    char            *debug_opt = "-d";
    char            *no_detach_opt = "-x";
    char            *server_id_opt = "-i";
    CFArrayRef      active_servers;
    CFStringRef     string;
    
    // setup the common args
    server[0] = 0;
    args[0] = vpnd_prog;
    i = 1;
    if (params->debug)
        args[i++] = debug_opt;
    if (params->daemonize == 0)
        args[i++] = no_detach_opt;
    args[i++] = server_id_opt;
    args[i++] = server;
    args[i] = 0;	// teminating zero
     
    if ((active_servers = get_active_servers(params)) == 0)
        return 0;       // no active servers
    count = CFArrayGetCount(active_servers);
    
    // find and launch active server ids
    for (j = 0; j < count; j++) {
        string = CFArrayGetValueAtIndex(active_servers, j);
        if (isString(string))
            CFStringGetCString(string, server, OPT_STR_LEN, kCFStringEncodingMacRoman);
        else
            continue;   // skip it
    
        switch (pidChild = fork ()) {
            case 0:		// in child
                execve(PATH_VPND, args, (char *)0);		// launch it
                break;
            case -1:		// error
                vpnlog(LOG_ERR, "Attempt to fork new vpnd failed\n") ;
                return -1;
            default:
                vpnlog(LOG_INFO, "Launched vpnd process id '%d' for server id '%s'\n", pidChild, server);
                break;
        }
    }
    CFRelease(active_servers);
    return 0;
}

//-----------------------------------------------------------------------------
// 	change the ip forwarding setting.
//-----------------------------------------------------------------------------
static int set_forwarding(int *oldval, int newval)
{
    size_t len = sizeof(int); 
    
    if (newval != 0 && newval != 1)
        return 0;	// ignore the command
    
    return sysctlbyname("net.inet.ip.forwarding", oldval, &len, &newval, sizeof(int));
}

//-----------------------------------------------------------------------------
// 	close_file_descriptors - closes all file descriptors except std.
//-----------------------------------------------------------------------------
static void close_file_descriptors(void)
{
    // Find the true upper limit on file descriptors.
    register int	i = OPEN_MAX;
    register int	nMin = STDERR_FILENO;
    struct rlimit	lim;

    if (!getrlimit (RLIMIT_NOFILE, &lim))
        i = lim.rlim_cur;
    else
        vpnlog(LOG_ERR, "close_file_descriptors() - getrlimit() failed\n");

    // Close all file descriptors except std*.
    while (--i > nMin)
        close (i) ;
}

//-----------------------------------------------------------------------------
//	redirect_std_file - redirect standard file descriptors to /dev/null.
//-----------------------------------------------------------------------------
static void redirect_std_file(void)
{
    int	i;

    // Handle stdin
    i = open(_PATH_DEVNULL, O_RDONLY);
    if ((i != -1) && (i != STDIN_FILENO)) {
        dup2(i, STDIN_FILENO);
        close (i);
    }

    // Handle stdout and stderr
    i = open (_PATH_DEVNULL, O_WRONLY);

    if (i != STDOUT_FILENO)
        dup2(i, STDOUT_FILENO); 
    if (i != STDERR_FILENO)
        dup2(i, STDERR_FILENO);

    if ((i != -1) && (i != STDOUT_FILENO) && (i != STDERR_FILENO))
        close(i);

}


//-----------------------------------------------------------------------------
//	detach - closes all file descriptors for this process.
//-----------------------------------------------------------------------------
static void detach(void)
{
    errno = 0;
    switch (fork()) {
        case 0:		// in child
            break;
        case -1:	// error
            vpnlog(LOG_ERR, "Detach failed: errno= %d\n", errno);
            /* FALLTHRU */
        default:	// parent
            exit(errno) ;
    }
}


//-----------------------------------------------------------------------------
//	write_pid_file - writes out standard pid file at given path.
//-----------------------------------------------------------------------------
static void write_pid_file(struct vpn_params *params)
{
    FILE 	*pidfile;
    char	subtype[OPT_STR_LEN];
    
    subtype[0] = 0;
    if (params->serverSubTypeRef)
		CFStringGetCString(params->serverSubTypeRef, subtype, OPT_STR_LEN, kCFStringEncodingUTF8);
    if (subtype[0])
        strcat(pid_path, subtype);
    strcat(pid_path, ".pid");

    if ((pidfile = fopen(pid_path, "w")) != NULL) {
	fprintf(pidfile, "%d\n", getpid());
	fclose(pidfile);
    } else
	vpnlog(LOG_WARNING, "Failed to create pid file %s: %m", pid_path);
}

//-----------------------------------------------------------------------------
//	delete_pid_file 
//-----------------------------------------------------------------------------
static void delete_pid_file(void)
{
    remove(pid_path);
}

// ----------------------------------------------------------------------------
//	Log File Functions
// ----------------------------------------------------------------------------
static char *log_time_string(time_t inNow, char *inTimeString)
{
    struct tm *tmTime;
    
    if (!inTimeString)
            return 0;
    
    tmTime = localtime(&inNow);

    sprintf(inTimeString, "%04d-%02d-%02d %02d:%02d:%02d %s",
                            tmTime->tm_year + 1900, tmTime->tm_mon + 1, tmTime->tm_mday,
                            tmTime->tm_hour, tmTime->tm_min, tmTime->tm_sec,
                            tmTime->tm_zone);
    return inTimeString;
}

// ----------------------------------------------------------------------------
//	create_log_file
// ----------------------------------------------------------------------------
static void create_log_file(char *inLogPath)
{
    char	theTime[40];
	mode_t	mask;

    // Setup the log file.

	mask = umask(S_IWGRP|S_IXGRP|S_IWOTH|S_IXOTH);
	logfile = fopen(inLogPath, "a");
	mask = umask(mask);

    if (logfile == 0)
            vpnlog(LOG_ERR, "Could not open log file %s.\n", inLogPath);
    else {
        fcntl(fileno(logfile), F_SETFD, 1);

        // Add the file header only when first created.
        //if (!ftell(logfile))
        //	fprintf(logfile, "#Version: 1.0\n#Software: %s, build %s\n",
        //					inSoftware, inVersion);

        // Always add time stamp and field ID
        fprintf(logfile, "#Start-Date: %s\n"
                                        "#Fields: date time s-comment\n",
                                        log_time_string(time(NULL), theTime));
        fflush(logfile);
    }
}

// ----------------------------------------------------------------------------
//	close_log_file
// ----------------------------------------------------------------------------
static void close_log_file(void)
{
    char the_time[40];

    if (!logfile)
        return;

    fprintf (logfile, "#End-Date: %s\n", log_time_string(time(NULL), the_time)) ;
    fclose(logfile) ;
}

// ----------------------------------------------------------------------------
//	debug_log
// ----------------------------------------------------------------------------
void vpnlog(int nSyslogPriority, char *format_str, ...)
{
    va_list		args;
    time_t		tNow;
    struct tm		*tmTime;
    char		theTime[40];

    if (!params->debug && LOG_PRI(nSyslogPriority) == LOG_DEBUG)
        return;
        
    va_start(args, format_str);

    tNow = time(NULL);
    tmTime = localtime(&tNow);

    // If the facility hasn't been defined, make it LOG_DAEMON.
    //if (!(nSyslogPriority & LOG_FACMASK))
    //    nSyslogPriority |= LOG_DAEMON;

    sprintf(theTime, "%04d-%02d-%02d %02d:%02d:%02d %s\t",
                            tmTime->tm_year + 1900, tmTime->tm_mon + 1, tmTime->tm_mday,
                            tmTime->tm_hour, tmTime->tm_min, tmTime->tm_sec,
                            tmTime->tm_zone);

    if (logfile) {
        fputs(theTime, logfile);
        vfprintf(logfile, format_str, args); 
        fflush(logfile);
    };
    
	vsyslog(nSyslogPriority, format_str, args);

    // Log to stderr if the socket is valid
    //	AND ( we're debugging OR this is a high priority message)
    if (stdio_is_valid && (params->debug || (LOG_PRI(nSyslogPriority) >= LOG_WARNING))) {
        fputs(theTime, stderr);
        vfprintf(stderr, format_str, args);
        fflush(stderr);
    }
}

//-----------------------------------------------------------------------------
//	dump_params
//-----------------------------------------------------------------------------
static void dump_params(struct vpn_params *params)
{
    int	i;
	char	*subtype = 0, *servertype = "Unknown";
    
	switch (params->server_type) {
		case SERVER_TYPE_IPSEC:
			servertype = "IPSec";
			break;
		case SERVER_TYPE_PPP:
			servertype = "PPP";
			switch (params->server_subtype) {
				case PPP_TYPE_SERIAL:
					subtype = "Serial";
					break;
				case PPP_TYPE_PPPoE:
					subtype = "PPPoE";
					break;
				case PPP_TYPE_PPTP:
					subtype = "PPTP";
					break;
				case PPP_TYPE_L2TP:
					subtype = "L2TP";
					break;
				default:
					subtype = "Unknown";
					break;
			}
			break;
	}
	
    vpnlog(LOG_DEBUG, "params->daemonize = %d\n", params->daemonize);
    vpnlog(LOG_DEBUG, "params->max_sessions = %d\n", params->max_sessions);    
    vpnlog(LOG_DEBUG, "params->server_id = %s\n", params->server_id);
    vpnlog(LOG_DEBUG, "params->server_type = %s\n", servertype);
    if (subtype)
		vpnlog(LOG_DEBUG, "params->server_subtype = %s\n", subtype);
		
    vpnlog(LOG_DEBUG, "params->lb_enable = %d\n", params->lb_enable);
    if (params->lb_enable) {
		char str[32];
		//vpnlog(LOG_DEBUG, "params->lb_priority = %d\n", params->lb_priority);
		vpnlog(LOG_DEBUG, "params->lb_interface = %s\n", params->lb_interface);
		inet_ntop(AF_INET, &params->lb_cluster_address, str, sizeof(str));
		vpnlog(LOG_DEBUG, "params->lb_cluster_address = %s\n", str);
		inet_ntop(AF_INET, &params->lb_redirect_address, str, sizeof(str));
		vpnlog(LOG_DEBUG, "params->lb_redirect_address = %s\n", str);
		vpnlog(LOG_DEBUG, "params->lb_port = %d\n", ntohs(params->lb_port));
	}

    if (params->plugin_path)
		vpnlog(LOG_DEBUG, "params->plugin_path = %s\n", params->plugin_path);
    vpnlog(LOG_DEBUG, "params->log_path = %s\n", params->log_path);
    if (params->next_arg_index)
		vpnlog(LOG_DEBUG, "params->next_arg_index = %d\n", params->next_arg_index);
    
    for (i = 0; i < params->next_arg_index; i++)
        vpnlog(LOG_DEBUG, "params->exec_args[%d] = %s\n", i, params->exec_args[i]);
    
}

//-----------------------------------------------------------------------------
//	set_terminate
//-----------------------------------------------------------------------------
void set_terminate(void)
{
    terminate = 1;
}

//-----------------------------------------------------------------------------
//	got_terminate
//-----------------------------------------------------------------------------
int got_terminate(void)
{
    return terminate;
}

//-----------------------------------------------------------------------------
//	got_sig_chld
//-----------------------------------------------------------------------------
int got_sig_chld(void)
{
    if (rcvd_sig_child) {
        rcvd_sig_child = 0;
        return 1;
    }
    return 0;
}

//-----------------------------------------------------------------------------
//	got_sig_hup
//-----------------------------------------------------------------------------
int got_sig_hup(void)
{
    if (rcvd_sig_hup) {
        rcvd_sig_hup = 0;
        return 1;
    }
    return 0;
}

//-----------------------------------------------------------------------------
//	got_sig_usr1
//-----------------------------------------------------------------------------
int got_sig_usr1(void)
{
    if (rcvd_sig_usr1) {
        rcvd_sig_usr1 = 0;
        return 1;
    }
    return 0;
}



//-----------------------------------------------------------------------------
//	setup_signal_handlers
//-----------------------------------------------------------------------------
static void sig_chld(int inSignal)
{
    rcvd_sig_child = 1;
}

static void sig_term(int inSignal)
{
    vpnlog(LOG_INFO, "terminating on signal %d\n", inSignal);
    terminate = 1;
}

static void sig_hup(int inSignal)
{
    rcvd_sig_hup = 1;
}

static void sig_usr1(int inSignal)
{
    rcvd_sig_usr1 = 1;
}


static void setup_signal_handlers(void)
{
    // Setup the signal handlers.
    struct sigaction	sSigAction, sSigOldAction;

    sigemptyset (&sSigAction.sa_mask);
    // Not sure if we always want to restart system calls...
    sSigAction.sa_flags = 0;	//SA_RESTART;

    sSigAction.sa_handler = sig_term;
    sigaction(SIGTERM, &sSigAction, &sSigOldAction);

    sSigAction.sa_handler = sig_term;
    sigaction(SIGINT, &sSigAction, &sSigOldAction);

    sSigAction.sa_handler = sig_hup;
    sigaction(SIGHUP, &sSigAction, &sSigOldAction);

    sSigAction.sa_handler = sig_usr1;
    sigaction(SIGUSR1, &sSigAction, &sSigOldAction);

    sSigAction.sa_flags |= SA_NOCLDSTOP;	//%%%% do we want this ??
    sSigAction.sa_handler = sig_chld;
    sigaction(SIGCHLD, &sSigAction, &sSigOldAction);
}

//-----------------------------------------------------------------------------
// 	toggle_debug
//-----------------------------------------------------------------------------
void toggle_debug(void)
{
	if (params->debug) {
		vpnlog(LOG_DEBUG, "debugging disabled\n");
		params->debug = 0;
	} else {
		params->debug = 1;
		vpnlog(LOG_DEBUG, "debugging enabled - dumping current parameters\n");
		dump_params(params);
	}
}

//-----------------------------------------------------------------------------
// 	update_prefs
//-----------------------------------------------------------------------------
int update_prefs(void)
{
    int 		i;
    struct vpn_params 	*old_params;
	int		debug = params->debug;
    
	
	// need to update existing params structure because ptr
	// to this struct is passed to accept_connections function
    
    old_params = (struct vpn_params*)malloc(sizeof(struct vpn_params));
    if (old_params == 0) {
        vpnlog(LOG_ERR, "Could not allocate memory for old preferences");
        return -1;
    }

    // save current parameters and clear params struct
    *old_params = *params;
	bzero(params, sizeof(params));
	params->debug = old_params->debug;
	params->server_id = old_params->server_id;
	params->daemonize = old_params->daemonize;
	
	begin_address_update();
	
    if (process_prefs(params)) {
    	vpnlog(LOG_ERR, "Update preferences - Error processing prefs file\n");
        goto fail;
	}

    // check for consistency
    if (params->server_subtype != old_params->server_subtype) {
        vpnlog(LOG_ERR, "Update preferences - server subtype does not match\n");
        goto fail;
    }
    
    // get and check plugin args
    if (get_plugin_args(params, 1)) {
        vpnlog(LOG_ERR, "Update preferences - plugin arguments invalid or inconsistent\n");
        goto fail;
    }
	
	//
    // success
    //
    CFRelease(old_params->serverSubTypeRef);
    CFRelease(old_params->serverRef);
    CFRelease(old_params->serverIDRef);
    if (old_params->plugin_path)
		free(old_params->plugin_path);
    for (i = 0; i < old_params->next_arg_index; i++)
        free(old_params->exec_args[i]);
    free(old_params);
    apply_address_update();
    vpnlog(LOG_INFO, "Update of preferences succeeded - settings have been changed\n");  
    if (debug)
        dump_params(params);
    return 0;
    
fail:       
    cancel_address_update();	
	if (debug)
		dump_params(params);
	if (params->serverSubTypeRef)
		CFRelease(params->serverSubTypeRef);
	if (params->serverRef)
		CFRelease(params->serverRef);
	if (params->serverIDRef)
		CFRelease(params->serverIDRef);
    if (params->plugin_path)
		free(params->plugin_path);
    if (params->plugin_path)
		free(params->plugin_path);
	for (i = 0; i < params->next_arg_index; i++)
		free(params->exec_args[i]);
	*params = *old_params;
	free(old_params);
    
    vpnlog(LOG_ERR, "Update of preferences failed - settings left unchanged\n");
    return 0;
	
}

