/*
 * sys-bsd.c - System-dependent procedures for setting up
 * PPP interfaces on bsd-4.4-ish systems (including 386BSD, NetBSD, etc.)
 *
 * Copyright (c) 1989 Carnegie Mellon University.
 * Copyright (c) 1995 The Australian National University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by Carnegie Mellon University and The Australian National University.
 * The names of the Universities may not be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#define RCSID	"$Id: sys-MacOSX.c,v 1.22 2001/09/07 00:26:02 callie Exp $"

/* -----------------------------------------------------------------------------
  Includes
----------------------------------------------------------------------------- */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/param.h>
#ifdef PPP_FILTER
#include <net/bpf.h>
#endif

#include <net/if.h>

#include <mach-o/dyld.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <dirent.h>
#include <unistd.h>
#include <NSSystemDirectories.h>

#ifdef	USE_SYSTEMCONFIGURATION_PUBLIC_APIS
#include <SystemConfiguration/SystemConfiguration.h>
#else	/* USE_SYSTEMCONFIGURATION_PUBLIC_APIS */
#include <SystemConfiguration/v1Compatibility.h>
#include <SystemConfiguration/SCSchemaDefinitions.h>
#endif	/* USE_SYSTEMCONFIGURATION_PUBLIC_APIS */

#include <CoreFoundation/CFBundle.h>
#include <net/ppp_defs.h>
#include <net/ppp_domain.h>
#include <net/ppp_msg.h>
#include <net/ppp_privmsg.h>
#include <net/if_ppp.h>
#include <net/route.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <sys/param.h>
#include <netinet/if_ether.h>

#include "pppd.h"
#include "fsm.h"
#include "ipcp.h"
#include "lcp.h"

#include <syslog.h>
#include <sys/un.h>
#include <pthread.h>


/* -----------------------------------------------------------------------------
 Definitions
----------------------------------------------------------------------------- */

#define IP_FORMAT	"%d.%d.%d.%d"
#define IP_CH(ip)	((u_char *)(ip))
#define IP_LIST(ip)	IP_CH(ip)[0],IP_CH(ip)[1],IP_CH(ip)[2],IP_CH(ip)[3]

#define PPP_NKE_PATH 	"/System/Library/Extensions/PPP.kext"

/* We can get an EIO error on an ioctl if the modem has hung up */
#define ok_error(num) ((num)==EIO)

#ifndef N_SYNC_PPP
#define N_SYNC_PPP 14
#endif

/* -----------------------------------------------------------------------------
 Forward declarations
----------------------------------------------------------------------------- */

static int get_flags (int fd);
static void set_flags (int fd, int flags);
static int translate_speed (int bps);
static int baud_rate_of (int speed);
static void close_route_table (void);
static int open_route_table (void);
static int read_route_table (struct rtentry *rt);
static int defaultroute_exists (struct rtentry *rt);
//static int get_ether_addr (u_int32_t ipaddr, struct sockaddr *hwaddr,
//			   char *name, int namelen);
static void decode_version (char *buf, int *version, int *mod, int *patch);
static int set_kdebugflag(int level);
static int ppp_registered(void);
static int make_ppp_unit(void);
/* Prototypes for procedures local to this file. */
static int get_ether_addr __P((u_int32_t, struct sockaddr_dl *));
static int connect_pfppp();
static void sys_pidchange(void *arg, int pid);
static void sys_phasechange(void *arg, int phase);
int sys_getconsoleuser(uid_t *uid);
static void sys_exitnotify(void *arg, int exitcode);
static int publish_keyentry(CFStringRef key, CFStringRef entry, CFTypeRef value);
static int publish_numentry(CFStringRef entry, int val);
static int publish_strentry(CFStringRef entry, char *str, int encoding);
static int unpublish_keyentry(CFStringRef key, CFStringRef entry);
static int unpublish_entity(CFStringRef entity);
static int unpublish_entry(CFStringRef entry);
static void sys_eventnotify(void *param, int code);

/* -----------------------------------------------------------------------------
 Globals
----------------------------------------------------------------------------- */
static const char 	rcsid[] = RCSID;

static int 		ttydisc = TTYDISC;	/* The default tty discipline */
static int 		pppdisc = PPPDISC;	/* The PPP sync or async discipline */

static int 		initfdflags = -1;	/* Initial file descriptor flags for ppp_fd */
static int 		ppp_fd = -1;		/* fd which is set to PPP discipline */
static int		rtm_seq;

static int 		restore_term;		/* 1 => we've munged the terminal */
static struct termios 	inittermios; 		/* Initial TTY termios */
static struct winsize 	wsinfo;			/* Initial window size info */

static int 		ip_sockfd;		/* socket for doing interface ioctls */
static int 		csockfd = -1;		/* socket for doing configd ioctls */

static fd_set 		in_fds;			/* set of fds that wait_input waits for */
static fd_set 		ready_fds;		/* set of fds currently ready (out of select) */
static int 		max_in_fd;		/* highest fd set in in_fds */

static int 		if_is_up;		/* the interface is currently up */
static u_int32_t 	ifaddrs[2];		/* local and remote addresses we set */
static u_int32_t 	default_route_gateway;	/* gateway addr for default route */
static u_int32_t 	proxy_arp_addr;		/* remote addr for proxy arp */
static SCDSessionRef	cfgCache = 0;		/* configd session */
static CFStringRef	serviceidRef = 0;	/* service id ref */

extern u_char		inpacket_buf[];		/* borrowed from main.c */

static int		looped;			/* 1 if using loop */
static int 		ppp_sockfd = -1;	/* fd for PF_PPP socket */
static char 		*serviceid = NULL; 	/* configuration service ID to publish */
static bool		tempserviceid = 0;	/* use a temporary created serviceid ? */
static bool 		noload = 0;		/* don't load the kernel extension */
static bool 		forcedetach = 0;	/* force detach from terminal, regardless of nodetach/updetach settings */

option_t sys_options[] = {
    { "serviceid", o_string, &serviceid,
      "Service ID to publish"},
    { "nopppload", o_bool, &noload,
      "Don't try to load PPP NKE", 1},
    { "forcedetach", o_bool, &forcedetach,
      "Force detach from terminal", 1},
    { NULL }
};

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void closeall()
{
    int i;

    for (i = getdtablesize() - 1; i >= 0; i--) close(i);
    open("/dev/null", O_RDWR, 0);
    dup(0);
    dup(0);
    return;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long load_kext(char *kext)
{
    int pid;

    if ((pid = fork()) < 0)
        return 1;

    if (pid == 0) {
        closeall();
        // PPP kernel extension not loaded, try load it...
        execl("/sbin/kextload", "kextload", kext, (char *)0);
        exit(1);
    }

    while (waitpid(pid, 0, 0) < 0) {
        if (errno == EINTR)
            continue;
       return 1;
    }
    return 0;
}

/* -----------------------------------------------------------------------------
preinitialize options, called before sysinit
----------------------------------------------------------------------------- */
void sys_install_options()
{
    add_options(sys_options);
}

/* -----------------------------------------------------------------------------
System-dependent initialization
----------------------------------------------------------------------------- */
void sys_init()
{
    int 		err, flags;
    struct sockaddr_un	adr;
    struct sockaddr_ppp 	pppaddr;
    SCDStatus		status;
    char 		str[16];

    openlog("pppd", LOG_PID | LOG_NDELAY, LOG_PPP);
    setlogmask(LOG_UPTO(LOG_INFO));
    if (debug)
	setlogmask(LOG_UPTO(LOG_DEBUG));

    // open a socket to the PF_PPP protocol
    ppp_sockfd = connect_pfppp();
    if (ppp_sockfd < 0)
        fatal("Couldn't open PF_PPP: %m");
                
    flags = fcntl(ppp_sockfd, F_GETFL);
    if (flags == -1
        || fcntl(ppp_sockfd, F_SETFL, flags | O_NONBLOCK) == -1)
        warn("Couldn't set PF_PPP to nonblock: %m");

    // Get an internet socket for doing socket ioctls
    ip_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (ip_sockfd < 0)
	fatal("Couldn't create IP socket: %m(%d)", errno);
        
#if 1
    csockfd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (csockfd < 0)
        fatal("Couldn't create PPPController local socket: %m");

    bzero(&adr, sizeof(adr));
    adr.sun_family = AF_LOCAL;
    strcpy(adr.sun_path, PPP_PATH);

    if (err = connect(csockfd,  (struct sockaddr *)&adr, sizeof(adr)) < 0) {
        warn("Couldn't connect to PPPController \n");
        csockfd = -1;
    }
#endif

    // serviceid is required if we want pppd to publish information into the cache
#if 1
    if (!serviceid) {
        sprintf(str, "ppp-%d", getpid());
        if (serviceid = malloc(strlen(str) + 1))
            strcpy(serviceid, str);
        else 
            fatal("Couldn't allocate memory to create temporary service id: %m(%d)", errno);
        tempserviceid = 1;
    }        
#endif
    serviceidRef = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s"), serviceid);
    if (!serviceidRef) 
        fatal("Couldn't allocate memory to create service id reference: %m(%d)", errno);


    // if force detach is specified, remove nodetach option.
    // we don't want to block configd with bad config files
    if (forcedetach)
        nodetach = updetach = 0;
        
    //sys_pidchange(0, getpid());
    sys_reinit();
    //publish_entry(CFSTR("Status"), CFSTR("Initializing"));
    publish_numentry(CFSTR("Status"), phase);
    //unpublish_entry(CFSTR("LastCause"));
    //add_notifier(&pidchange, sys_pidchange, 0);
    add_notifier(&phasechange, sys_phasechange, 0);
    add_notifier(&exitnotify, sys_exitnotify, 0);
    
    // only send event if started siwth serviceid option
    // PPPController knows only about serviceid based connections
    if (!tempserviceid) {
        add_notifier(&ip_up_notify, sys_eventnotify, (void*)PPP_EVT_IPCP_UP);
        add_notifier(&ip_down_notify, sys_eventnotify, (void*)PPP_EVT_IPCP_DOWN);
        add_notifier(&lcp_up_notify, sys_eventnotify, (void*)PPP_EVT_LCP_UP);
        add_notifier(&lcp_down_notify, sys_eventnotify, (void*)PPP_EVT_LCP_DOWN);
        add_notifier(&lcp_lowerup_notify, sys_eventnotify, (void*)PPP_EVT_LOWERLAYER_UP);
        add_notifier(&lcp_lowerdown_notify, sys_eventnotify, (void*)PPP_EVT_LOWERLAYER_DOWN);
        add_notifier(&auth_start_notify, sys_eventnotify, (void*)PPP_EVT_AUTH_STARTED);
        add_notifier(&auth_withpeer_fail_notify, sys_eventnotify, (void*)PPP_EVT_AUTH_FAILED);
        add_notifier(&auth_withpeer_success_notify, sys_eventnotify, (void*)PPP_EVT_AUTH_SUCCEDED);
        add_notifier(&connectscript_started_notify, sys_eventnotify, (void*)PPP_EVT_CONNSCRIPT_STARTED);
        add_notifier(&connectscript_finished_notify, sys_eventnotify, (void*)PPP_EVT_CONNSCRIPT_FINISHED);
        add_notifier(&terminalscript_started_notify, sys_eventnotify, (void*)PPP_EVT_TERMSCRIPT_STARTED);
        add_notifier(&terminalscript_finished_notify, sys_eventnotify, (void*)PPP_EVT_TERMSCRIPT_FINISHED);
        add_notifier(&connect_started_notify, sys_eventnotify, (void*)PPP_EVT_CONN_STARTED);
        add_notifier(&connect_success_notify, sys_eventnotify, (void*)PPP_EVT_CONN_SUCCEDED);
        add_notifier(&connect_fail_notify, sys_eventnotify, (void*)PPP_EVT_CONN_FAILED);
        add_notifier(&disconnect_started_notify, sys_eventnotify, (void*)PPP_EVT_DISC_STARTED);
        add_notifier(&disconnect_done_notify, sys_eventnotify, (void*)PPP_EVT_DISC_FINISHED);
    }

    FD_ZERO(&in_fds);
    FD_ZERO(&ready_fds);
    max_in_fd = 0;
}

/* ----------------------------------------------------------------------------- 
 sys_cleanup - restore any system state we modified before exiting:
 mark the interface down, delete default route and/or proxy arp entry.
 This should call die() because it's called from die()
----------------------------------------------------------------------------- */
void sys_cleanup()
{
    struct ifreq ifr;

    if (if_is_up) {
	strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	if (ioctl(ip_sockfd, SIOCGIFFLAGS, &ifr) >= 0
	    && ((ifr.ifr_flags & IFF_UP) != 0)) {
	    ifr.ifr_flags &= ~IFF_UP;
	    ioctl(ip_sockfd, SIOCSIFFLAGS, &ifr);
	}
    }
    if (ifaddrs[0] != 0)
	cifaddr(0, ifaddrs[0], ifaddrs[1]);
    if (default_route_gateway)
	cifdefaultroute(0, 0, default_route_gateway);
    if (proxy_arp_addr)
	cifproxyarp(0, proxy_arp_addr);
}

/* ----------------------------------------------------------------------------- 
----------------------------------------------------------------------------- */
void sys_close()
{
    close(ip_sockfd);
}

/* ----------------------------------------------------------------------------- 
 Functions to read and set the flags value in the device driver
----------------------------------------------------------------------------- */
static int get_flags (int fd)
{    
    int flags;

    if (ioctl(fd, PPPIOCGFLAGS, (caddr_t) &flags) < 0) {
	if ( ok_error (errno) )
	    flags = 0;
	else
	    fatal("ioctl(PPPIOCGFLAGS): %m");
    }

    SYSDEBUG ((LOG_DEBUG, "get flags = %x\n", flags));
    return flags;
}

/* ----------------------------------------------------------------------------- 
----------------------------------------------------------------------------- */
static void set_flags (int fd, int flags)
{    
    SYSDEBUG ((LOG_DEBUG, "set flags = %x\n", flags));

    if (ioctl(fd, PPPIOCSFLAGS, (caddr_t) &flags) < 0) {
	if (! ok_error (errno) )
	    fatal("ioctl(PPPIOCSFLAGS, %x): %m", flags, errno);
    }
}

/* ----------------------------------------------------------------------------- 
we installed the notifier with the event as the parameter
----------------------------------------------------------------------------- */
void sys_eventnotify(void *param, int code)
{
    struct ppp_msg_hdr	*msg;
    int 		servlen = strlen(serviceid);
    int 		totlen = sizeof(struct ppp_msg_hdr) + servlen + 8;
    u_char 		*p;
    
    if (csockfd == -1)
        return;
        
    p = malloc(totlen);
    if (!p) {
	warn("no memory to send event to PPPController");
        return;
    }
        
    bzero(p, totlen);
    msg = (struct ppp_msg_hdr *)p;
    msg->m_type = PPPD_EVENT;
    msg->m_len = servlen + 8;
    
    bcopy((u_char*)&param, p + sizeof(struct ppp_msg_hdr), 4);
    bcopy((u_char*)&code, p + sizeof(struct ppp_msg_hdr) + 4, 4);
    bcopy(serviceid, p + sizeof(struct ppp_msg_hdr) + 8, servlen);
    
    if (write(csockfd, p, totlen) != totlen) {
	warn("can't talk to PPPController : %m");
    }
    free(p);
}


/* ----------------------------------------------------------------------------- 
check the options that the user specified
----------------------------------------------------------------------------- */
int sys_check_options()
{
#ifndef CDTRCTS
    if (crtscts == 2) {
	warn("DTR/CTS flow control is not supported on this system");
	return 0;
    }
#endif
    return 1;
}

/* ----------------------------------------------------------------------------- 
check if the kernel supports PPP
----------------------------------------------------------------------------- */
int ppp_available()
{
    int 	s;
    extern char *no_ppp_msg;
    
    no_ppp_msg = "\
Mac OS X lacks kernel support for PPP.  \n\
To include PPP support in the kernel, please follow \n\
the steps detailed in the README.MacOSX file.\n";

    // open to socket to the PF_PPP family
    // if that works, the kernel extension is loaded.
    if ((s = socket(PF_PPP, SOCK_RAW, PPPPROTO_CTL)) < 0) {
    
        if (!noload && !load_kext(PPP_NKE_PATH))
            s = socket(PF_PPP, SOCK_RAW, PPPPROTO_CTL);
            
        if (s < 0)
            return 0;
    }
    
    // could be smarter and get the version of the ppp family, 
    // using get option or ioctl

    close(s);

    return 1;
}

/* ----------------------------------------------------------------------------- 
new_fd is the fd of a tty
----------------------------------------------------------------------------- */
static void set_ppp_fd (int new_fd)
{
    SYSDEBUG ((LOG_DEBUG, "setting ppp_fd to %d\n", new_fd));
    ppp_fd = new_fd;
}

/* ----------------------------------------------------------------------------- 
----------------------------------------------------------------------------- */
static int still_ppp(void)
{

    return !hungup && ppp_fd >= 0;
}

/* ----------------------------------------------------------------------------- 
Define the debugging level for the kernel
----------------------------------------------------------------------------- */
static int set_kdebugflag (int requested_level)
{
   if (ifunit < 0)
	return 1;
    if (ioctl(ppp_sockfd, PPPIOCSDEBUG, &requested_level) < 0) {
	if ( ! ok_error (errno) )
	    error("ioctl(PPPIOCSDEBUG): %m");
	return (0);
    }
    SYSDEBUG ((LOG_INFO, "set kernel debugging level to %d",
		requested_level));
    return (1);
}

/* ----------------------------------------------------------------------------- 
make a new ppp unit for ppp_sockfd
----------------------------------------------------------------------------- */
static int make_ppp_unit()
{
    struct sockaddr_ppp 	pppaddr;
    int x;

    ifunit = req_unit;
    x = ioctl(ppp_sockfd, PPPIOCNEWUNIT, &ifunit);
    if (x < 0 && req_unit >= 0 && errno == EEXIST) {
        warn("Couldn't allocate PPP unit %d as it is already in use");
        ifunit = -1;
        x = ioctl(ppp_sockfd, PPPIOCNEWUNIT, &ifunit);
    }
    if (x < 0)
        error("Couldn't create new ppp unit: %m");

    return x;
}

/* ----------------------------------------------------------------------------- 
coen a socket to the PF_PPP protocol
----------------------------------------------------------------------------- */
int connect_pfppp()
{
    int 			fd = -1;
    struct sockaddr_ppp 	pppaddr;
    
    // open a PF_PPP socket
    fd = socket(PF_PPP, SOCK_RAW, PPPPROTO_CTL);
    if (fd < 0) {
        error("Couldn't open PF_PPP: %m");
        return -1;
    }
    // need to connect to the PPP protocol
    pppaddr.ppp_len = sizeof(struct sockaddr_ppp);
    pppaddr.ppp_family = AF_PPP;
    pppaddr.ppp_proto = PPPPROTO_CTL;
    pppaddr.ppp_cookie = 0;
    if (connect(fd, (struct sockaddr *)&pppaddr, sizeof(struct sockaddr_ppp)) < 0) {
        error("Couldn't connect to PF_PPP: %m");
        close(fd);
        return -1;
    }
    return fd;
}


/* ----------------------------------------------------------------------------- 
Turn the serial port into a ppp interface
----------------------------------------------------------------------------- */
int establish_ppp_tty (int tty_fd)
{
    /* Ensure that the tty device is in exclusive mode.  */
    if (ioctl(tty_fd, TIOCEXCL, 0) < 0) {
        if ( ! ok_error ( errno ))
            warn("Couldn't make tty exclusive: %m");
    }
    
    // Set the current tty to the PPP discpline
    pppdisc = sync_serial ? N_SYNC_PPP: PPPDISC;
    if (ioctl(tty_fd, TIOCSETD, &pppdisc) < 0) {
        if ( ! ok_error (errno) ) {
            error("Couldn't set tty to PPP discipline: %m");
            return -1;
        }
    }

    return 0;
}

/* -----------------------------------------------------------------------------
Restore the serial port to normal operation.
This shouldn't call die() because it's called from die()
----------------------------------------------------------------------------- */
void disestablish_ppp_tty(int tty_fd)
{
    int 	x;
        
        // Flush the tty output buffer so that the TIOCSETD doesn't hang.
	//if (tcflush(tty_fd, TCIOFLUSH) < 0)
	//    warn("tcflush failed: %m");
    
        // Restore the previous line discipline
    if (ioctl(tty_fd, TIOCSETD, &ttydisc) < 0) {
        if ( ! ok_error (errno))
            error("ioctl(TIOCSETD, TTYDISC): %m");
    }
    
    if (ioctl(tty_fd, TIOCNXCL, 0) < 0) {
        if ( ! ok_error (errno))
            warn("ioctl(TIOCNXCL): %m(%d)", errno);
    }
}

/* ----------------------------------------------------------------------------- 
perform
----------------------------------------------------------------------------- */
int establish_ppp (int fd)
{
    int 		x, flags, s = -1, link = 0, ret;
    struct sockaddr_ppp pppaddr;

    // use the hook to establish the ppp connection. establishment mechanism depends on the device	
    if (dev_establish_ppp_hook)
        if (ret = (*dev_establish_ppp_hook)(fd))
            return ret;
        
    // when the device is setup as a ppp link, then do the generic link work
    
    // Open another instance of the ppp socket and connect the link to it
    if (ioctl(fd, PPPIOCGCHAN, &link) == -1) {
        error("Couldn't get link number: %m");
        goto err;
    }

    dbglog("using link %d", link);
        
    // open a socket to the PPP protocol
    s = connect_pfppp();
    if (s < 0) {
        error("Couldn't reopen PF_PPP: %m");
        goto err;
    }
    
    if (ioctl(s, PPPIOCATTCHAN, &link) < 0) {
        error("Couldn't attach to the ppp link %d: %m", link);
        goto err_close;
    }

    flags = fcntl(s, F_GETFL);
    if (flags == -1 || fcntl(s, F_SETFL, flags | O_NONBLOCK) == -1)
        warn("Couldn't set ppp socket link to nonblock: %m");
    set_ppp_fd(s);

    if (!looped)
        ifunit = -1;
    if (!looped && !multilink) {
        // Create a new PPP unit.
        if (make_ppp_unit() < 0)
            goto err_close;
    }

    if (looped) {
        set_flags(ppp_sockfd, get_flags(ppp_sockfd) & ~SC_LOOP_TRAFFIC);
        looped = 0;
    }

    if (!multilink) {
        add_fd(ppp_sockfd);
        if (ioctl(s, PPPIOCCONNECT, &ifunit) < 0) {
            error("Couldn't attach to PPP unit %d: %m", ifunit);
            goto err_close;
        }
    }

    // Enable debug in the driver if requested.
    set_kdebugflag (kdebugflag);

    set_flags(ppp_fd, get_flags(ppp_fd) & ~(SC_RCV_B7_0 | SC_RCV_B7_1 | SC_RCV_EVNP | SC_RCV_ODDP));

    return ppp_fd;

 err_close:
    close(s);
    
 err:
    // use the hook to disestablish the ppp connection	
    if (dev_disestablish_ppp_hook)
        (*dev_disestablish_ppp_hook)(fd);

    return -1;
}

/* -----------------------------------------------------------------------------
Restore the serial port to normal operation.
This shouldn't call die() because it's called from die()
----------------------------------------------------------------------------- */
void disestablish_ppp(int fd)
{
    int 	x;
    
    if (!hungup) {
        
        // use the hook to disestablish the ppp connection	
        if (dev_disestablish_ppp_hook)
            (*dev_disestablish_ppp_hook)(fd);

	/* Reset non-blocking mode on fd. */
	if (initfdflags != -1 && fcntl(fd, F_SETFL, initfdflags) < 0) {
	    if ( ! ok_error (errno))
		warn("Couldn't restore device fd flags: %m");
	}
    }

    initfdflags = -1;
    close(ppp_fd);
    ppp_fd = -1;
    if (!looped && ifunit >= 0 && ioctl(ppp_sockfd, PPPIOCDETACH, &x) < 0)
        error("Couldn't release PPP unit ppp_sockfd %d: %m", ppp_sockfd);
    if (!multilink)
        remove_fd(ppp_sockfd);
}

/* -----------------------------------------------------------------------------
Check whether the link seems not to be 8-bit clean
----------------------------------------------------------------------------- */
void clean_check()
{
    int x;
    char *s;

    if (!still_ppp())
        return;

    if (ioctl(ppp_fd, PPPIOCGFLAGS, (caddr_t) &x) == 0) {
	s = NULL;
	switch (~x & (SC_RCV_B7_0|SC_RCV_B7_1|SC_RCV_EVNP|SC_RCV_ODDP)) {
	case SC_RCV_B7_0:
	    s = "bit 7 set to 1";
	    break;
	case SC_RCV_B7_1:
	    s = "bit 7 set to 0";
	    break;
	case SC_RCV_EVNP:
	    s = "odd parity";
	    break;
	case SC_RCV_ODDP:
	    s = "even parity";
	    break;
	}
	if (s != NULL) {
	    warn("Serial link is not 8-bit clean:");
	    warn("All received characters had %s", s);
	}
    }
}

/* -----------------------------------------------------------------------------
Set up the serial port on `fd' for 8 bits, no parity,
 * at the requested speed, etc.  If `local' is true, set CLOCAL
 * regardless of whether the modem option was specified.
 *
 * For *BSD, we assume that speed_t values numerically equal bits/second
----------------------------------------------------------------------------- */
void set_up_tty(int fd, int local)
{
    struct termios tios;

    if (tcgetattr(fd, &tios) < 0)
	fatal("tcgetattr: %m");

    if (!restore_term) {
	inittermios = tios;
	ioctl(fd, TIOCGWINSZ, &wsinfo);
    }

    tios.c_cflag &= ~(CSIZE | CSTOPB | PARENB | CLOCAL);
    if (crtscts > 0 && !local) {
        if (crtscts == 2) {
#ifdef CDTRCTS
            tios.c_cflag |= CDTRCTS;
#endif
	} else
	    tios.c_cflag |= CRTSCTS;
    } else if (crtscts < 0) {
	tios.c_cflag &= ~CRTSCTS;
#ifdef CDTRCTS
	tios.c_cflag &= ~CDTRCTS;
#endif
    }

    tios.c_cflag |= CS8 | CREAD | HUPCL;
    if (local || !modem)
	tios.c_cflag |= CLOCAL;
    tios.c_iflag = IGNBRK | IGNPAR;
    tios.c_oflag = 0;
    tios.c_lflag = 0;
    tios.c_cc[VMIN] = 1;
    tios.c_cc[VTIME] = 0;

    if (crtscts == -2) {
	tios.c_iflag |= IXON | IXOFF;
	tios.c_cc[VSTOP] = 0x13;	/* DC3 = XOFF = ^S */
	tios.c_cc[VSTART] = 0x11;	/* DC1 = XON  = ^Q */
    }

    if (inspeed) {
	cfsetospeed(&tios, inspeed);
	cfsetispeed(&tios, inspeed);
    } else {
	inspeed = cfgetospeed(&tios);
	/*
	 * We can't proceed if the serial port speed is 0,
	 * since that implies that the serial port is disabled.
	 */
	if (inspeed == 0)
	    fatal("Baud rate for %s is 0; need explicit baud rate", devnam);
    }
    baud_rate = inspeed;

    if (tcsetattr(fd, TCSAFLUSH, &tios) < 0)
	fatal("tcsetattr: %m");

    restore_term = 1;
}


/* -----------------------------------------------------------------------------
Set up the serial port
 * If `local' is true, set CLOCAL
 * regardless of whether the modem option was specified.
 * other parameter may have been changed by the CCL
----------------------------------------------------------------------------- */
void set_up_tty_local(int fd, int local)
{
    struct termios tios;

    if (tcgetattr(fd, &tios) < 0)
	fatal("tcgetattr: %m");

    tios.c_cflag &= ~CLOCAL;
    if (local || !modem)
	tios.c_cflag |= CLOCAL;

    if (tcsetattr(fd, TCSAFLUSH, &tios) < 0)
	fatal("tcsetattr: %m");
}

/* -----------------------------------------------------------------------------
restore the terminal to the saved settings 
----------------------------------------------------------------------------- */
void restore_tty(int fd)
{
    if (restore_term) {
	if (!default_device) {
	    /*
	     * Turn off echoing, because otherwise we can get into
	     * a loop with the tty and the modem echoing to each other.
	     * We presume we are the sole user of this tty device, so
	     * when we close it, it will revert to its defaults anyway.
	     */
	    inittermios.c_lflag &= ~(ECHO | ECHONL);
	}
	if (tcsetattr(fd, TCSAFLUSH, &inittermios) < 0)
	    if (errno != ENXIO)
		warn("tcsetattr: %m");
	ioctl(fd, TIOCSWINSZ, &wsinfo);
	restore_term = 0;
    }
}

/* -----------------------------------------------------------------------------
control the DTR line on the serial port.
 * This is called from die(), so it shouldn't call die()
----------------------------------------------------------------------------- */
void setdtr(int fd, int on)
{
    int modembits = TIOCM_DTR;

    ioctl(fd, (on? TIOCMBIS: TIOCMBIC), &modembits);
}

/* -----------------------------------------------------------------------------
get a pty master/slave pair and chown the slave side
 * to the uid given.  Assumes slave_name points to >= 12 bytes of space
----------------------------------------------------------------------------- */
int get_pty(int *master_fdp, int *slave_fdp, char *slave_name, int uid)
{
    struct termios tios;

    if (openpty(master_fdp, slave_fdp, slave_name, NULL, NULL) < 0)
	return 0;

    fchown(*slave_fdp, uid, -1);
    fchmod(*slave_fdp, S_IRUSR | S_IWUSR);
    if (tcgetattr(*slave_fdp, &tios) == 0) {
	tios.c_cflag &= ~(CSIZE | CSTOPB | PARENB);
	tios.c_cflag |= CS8 | CREAD;
	tios.c_iflag  = IGNPAR | CLOCAL;
	tios.c_oflag  = 0;
	tios.c_lflag  = 0;
	if (tcsetattr(*slave_fdp, TCSAFLUSH, &tios) < 0)
	    warn("couldn't set attributes on pty: %m");
    } else
	warn("couldn't get attributes on pty: %m");

    return 1;
}

/* -----------------------------------------------------------------------------
create the ppp interface and configure it in loopback mode.
----------------------------------------------------------------------------- */
int open_ppp_loopback()
{
    looped = 1;
    /* allocate ourselves a ppp unit */
    if (make_ppp_unit() < 0)
        die(1);
    set_flags(ppp_sockfd, SC_LOOP_TRAFFIC);
    set_kdebugflag(kdebugflag);
    ppp_fd = -1;
    return ppp_sockfd;
}

/* -----------------------------------------------------------------------------
reconfigure the interface in loopback mode again
----------------------------------------------------------------------------- */
void restore_loop()
{
    looped = 1;
    set_flags(ppp_sockfd, get_flags(ppp_sockfd) | SC_LOOP_TRAFFIC);
}


/* -----------------------------------------------------------------------------
Output PPP packet
----------------------------------------------------------------------------- */
void output(int unit, u_char *p, int len)
{
    if (debug) {
        // don't log lcp-echo/reply packets
        //if ((*(u_short*)(p + 2) != PPP_LCP)
        //    || ((*(p + 3) == ECHOREQ) && (*(p + 3) == ECHOREP))) {
            dbglog("sent %P", p, len);
            //info("sent %P", p, len);
        //}
    }
    
    // don't write FF03
    len -= 2;
    p += 2;

    if (write(ppp_fd, p, len) < 0) {
	if (errno != EIO)
	    error("write: %m");
    }
}

/* -----------------------------------------------------------------------------
wait until there is data available, for the length of time specified by *timo
(indefinite if timo is NULL)
----------------------------------------------------------------------------- */
void wait_input(struct timeval *timo)
{
    int n;

    ready_fds = in_fds;
    n = select(max_in_fd + 1, &ready_fds, NULL, NULL, timo);
   if (n < 0 && errno != EINTR)
	fatal("select: %m");

}

/* -----------------------------------------------------------------------------
wait on fd until there is data available, for the delay in milliseconds
return 0 if timeout expires, < 0 if error, otherwise > 0
----------------------------------------------------------------------------- */
int wait_input_fd(int fd, int delay)
{
    fd_set 		ready;
    int 		n;
    struct timeval 	t;
    char 		c;

    t.tv_sec = delay / 1000;
    t.tv_usec = delay % 1000;

    FD_ZERO(&ready);
    FD_SET(fd, &ready);

    do {
        n = select(fd + 1, &ready, NULL, &ready, &t);
    } while (n < 0 && errno == EINTR);
    
    // Fix me : we swallow the first byte, which is bad...
    if (n > 0)
        n = read(fd, &c, 1);

    return n;
}


/* -----------------------------------------------------------------------------
add an fd to the set that wait_input waits for
----------------------------------------------------------------------------- */
void add_fd(int fd)
{
    FD_SET(fd, &in_fds);
    if (fd > max_in_fd)
	max_in_fd = fd;
}

/* -----------------------------------------------------------------------------
remove an fd from the set that wait_input waits for
----------------------------------------------------------------------------- */
void remove_fd(int fd)
{
    FD_CLR(fd, &in_fds);
}

/* -----------------------------------------------------------------------------
return 1 is fd is set (i.e. select returned with this file descriptor set)
----------------------------------------------------------------------------- */
bool is_ready_fd(int fd)
{

    return FD_ISSET(fd, &ready_fds);
}

/* -----------------------------------------------------------------------------
get a PPP packet from the serial device
----------------------------------------------------------------------------- */
int read_packet(u_char *buf)
{
    int len = -1;
    
    // FF03 are not read
    *buf++ = PPP_ALLSTATIONS;
    *buf++ = PPP_UI;

    // read first the socket attached to the link
    if (ppp_fd >= 0) {
        if ((len = read(ppp_fd, buf, PPP_MRU + PPP_HDRLEN - 2)) < 0) {
            if (errno != EWOULDBLOCK && errno != EINTR)
                error("read from socket link: %m");
        }
    }
    
    // then, if nothing, link the socket attached to the bundle
    if (len < 0 && ifunit >= 0) {
        if ((len = read(ppp_sockfd, buf, PPP_MRU + PPP_HDRLEN - 2)) < 0) {
            if (errno != EWOULDBLOCK && errno != EINTR)
                error("read from socket bundle: %m");
        }
    }
    return (len <= 0 ? len : len + 2);
}

/* -----------------------------------------------------------------------------
 read characters from the loopback, form them
 * into frames, and detect when we want to bring the real link up.
 * Return value is 1 if we need to bring up the link, 0 otherwise
----------------------------------------------------------------------------- */
int get_loop_output()
{
    int rv = 0;
    int n;

    while ((n = read_packet(inpacket_buf)) > 0)
        if (loop_frame(inpacket_buf, n))
            rv = 1;
    return rv;
}

/* -----------------------------------------------------------------------------
configure the transmit characteristics of the ppp interface
 ----------------------------------------------------------------------------- */
void ppp_send_config(int unit,int  mtu, u_int32_t asyncmap, int pcomp, int accomp)
{
    u_int x;
    struct ifreq ifr;

    strlcpy(ifr.ifr_name, ifname, sizeof (ifr.ifr_name));
    ifr.ifr_mtu = mtu;
    if (ioctl(ip_sockfd, SIOCSIFMTU, (caddr_t) &ifr) < 0)
	fatal("ioctl(SIOCSIFMTU): %m");

    if (!still_ppp())
	return;

    if (ioctl(ppp_fd, PPPIOCSASYNCMAP, (caddr_t) &asyncmap) < 0)
	fatal("ioctl(PPPIOCSASYNCMAP): %m");

    if (ioctl(ppp_fd, PPPIOCGFLAGS, (caddr_t) &x) < 0)
	fatal("ioctl (PPPIOCGFLAGS): %m");
    x = pcomp? x | SC_COMP_PROT: x &~ SC_COMP_PROT;
    x = accomp? x | SC_COMP_AC: x &~ SC_COMP_AC;
    x = sync_serial ? x | SC_SYNC : x & ~SC_SYNC;
    if (ioctl(ppp_fd, PPPIOCSFLAGS, (caddr_t) &x) < 0)
	fatal("ioctl(PPPIOCSFLAGS): %m");
        
    publish_numentry(kSCPropNetPPPLCPCompressionPField, pcomp);
    publish_numentry(kSCPropNetPPPLCPCompressionACField, accomp);
    publish_numentry(kSCPropNetPPPLCPMTU, mtu);
    publish_numentry(kSCPropNetPPPLCPTransmitACCM, asyncmap);
}


/* -----------------------------------------------------------------------------
set the extended transmit ACCM for the interface
----------------------------------------------------------------------------- */
void ppp_set_xaccm(int unit, ext_accm accm)
{
    if (!still_ppp())
	return;
    if (ioctl(ppp_fd, PPPIOCSXASYNCMAP, accm) < 0 && errno != ENOTTY)
	warn("ioctl(set extended ACCM): %m");
}


/* -----------------------------------------------------------------------------
configure the receive-side characteristics of the ppp interface.
----------------------------------------------------------------------------- */
void ppp_recv_config(int unit, int mru, u_int32_t asyncmap, int pcomp, int accomp)
{
    int x;

    if (!still_ppp())
	return;

    if (ioctl(ppp_fd, PPPIOCSMRU, (caddr_t) &mru) < 0)
	fatal("ioctl(PPPIOCSMRU): %m");
    if (ioctl(ppp_fd, PPPIOCSRASYNCMAP, (caddr_t) &asyncmap) < 0)
	fatal("ioctl(PPPIOCSRASYNCMAP): %m");
    if (ioctl(ppp_fd, PPPIOCGFLAGS, (caddr_t) &x) < 0)
	fatal("ioctl (PPPIOCGFLAGS): %m");
    x = !accomp? x | SC_REJ_COMP_AC: x &~ SC_REJ_COMP_AC;
    if (ioctl(ppp_fd, PPPIOCSFLAGS, (caddr_t) &x) < 0)
	fatal("ioctl(PPPIOCSFLAGS): %m");
    publish_numentry(kSCPropNetPPPLCPMRU, mru);
    publish_numentry(kSCPropNetPPPLCPReceiveACCM, asyncmap);
}

/* -----------------------------------------------------------------------------
ask kernel whether a given compression method
 * is acceptable for use.  Returns 1 if the method and parameters
 * are OK, 0 if the method is known but the parameters are not OK
 * (e.g. code size should be reduced), or -1 if the method is unknown
 ----------------------------------------------------------------------------- */
int ccp_test(int unit, u_char * opt_ptr, int opt_len, int for_transmit)
{
    struct ppp_option_data data;

    data.ptr = opt_ptr;
    data.length = opt_len;
    data.transmit = for_transmit;
    if (ioctl(ppp_fd/*ttyfd*/, PPPIOCSCOMPRESS, (caddr_t) &data) >= 0)
	return 1;
    return (errno == ENOBUFS)? 0: -1;
}

/* -----------------------------------------------------------------------------
inform kernel about the current state of CCP
----------------------------------------------------------------------------- */
void ccp_flags_set(int unit, int isopen, int isup)
{
    int x;

    if (!still_ppp())
	return;

    if (ioctl(ppp_fd, PPPIOCGFLAGS, (caddr_t) &x) < 0) {
	error("ioctl (PPPIOCGFLAGS): %m");
	return;
    }
    x = isopen? x | SC_CCP_OPEN: x &~ SC_CCP_OPEN;
    x = isup? x | SC_CCP_UP: x &~ SC_CCP_UP;
    if (ioctl(ppp_fd, PPPIOCSFLAGS, (caddr_t) &x) < 0)
	error("ioctl(PPPIOCSFLAGS): %m");
}

/* -----------------------------------------------------------------------------
returns 1 if decompression was disabled as a
 * result of an error detected after decompression of a packet,
 * 0 otherwise.  This is necessary because of patent nonsense
 ----------------------------------------------------------------------------- */
int ccp_fatal_error(int unit)
{
    int x;

    if (ioctl(ppp_fd, PPPIOCGFLAGS, (caddr_t) &x) < 0) {
	error("ioctl(PPPIOCGFLAGS): %m");
	return 0;
    }
    return x & SC_DC_FERROR;
}

/* -----------------------------------------------------------------------------
return how long the link has been idle
----------------------------------------------------------------------------- */
int get_idle_time(int u, struct ppp_idle *ip)
{
    return ioctl(ppp_sockfd, PPPIOCGIDLE, ip) >= 0;
}

/* -----------------------------------------------------------------------------
return statistics for the link
----------------------------------------------------------------------------- */
int get_ppp_stats(int u, struct pppd_stats *stats)
{
    struct ifpppstatsreq req;

    memset (&req, 0, sizeof (req));
    strlcpy(req.ifr_name, ifname, sizeof(req.ifr_name));
    if (ioctl(ip_sockfd, SIOCGPPPSTATS, &req) < 0) {
	error("Couldn't get PPP statistics: %m");
	return 0;
    }
    stats->bytes_in = req.stats.p.ppp_ibytes;
    stats->bytes_out = req.stats.p.ppp_obytes;
    return 1;
}


#ifdef PPP_FILTER
/* -----------------------------------------------------------------------------
transfer the pass and active filters to the kernel
----------------------------------------------------------------------------- */
int set_filters(struct bpf_program *pass, struct bpf_program *active)
{
    int ret = 1;

    if (pass->bf_len > 0) {
	if (ioctl(ppp_fd, PPPIOCSPASS, pass) < 0) {
	    error("Couldn't set pass-filter in kernel: %m");
	    ret = 0;
	}
    }
    if (active->bf_len > 0) {
	if (ioctl(ppp_fd, PPPIOCSACTIVE, active) < 0) {
	    error("Couldn't set active-filter in kernel: %m");
	    ret = 0;
	}
    }
    return ret;
}
#endif

/* -----------------------------------------------------------------------------
config tcp header compression
----------------------------------------------------------------------------- */
int sifvjcomp(int u, int vjcomp, int cidcomp, int maxcid)
{
    u_int x;

    if (ioctl(ppp_sockfd, PPPIOCGFLAGS, (caddr_t) &x) < 0) {
	error("ioctl (PPPIOCGFLAGS): %m");
	return 0;
    }
    x = vjcomp ? x | SC_COMP_TCP: x &~ SC_COMP_TCP;
    x = cidcomp? x & ~SC_NO_TCP_CCID: x | SC_NO_TCP_CCID;
    if (ioctl(ppp_sockfd, PPPIOCSFLAGS, (caddr_t) &x) < 0) {
	error("ioctl(PPPIOCSFLAGS): %m");
	return 0;
    }
    if (vjcomp && ioctl(ppp_sockfd, PPPIOCSMAXCID, (caddr_t) &maxcid) < 0) {
	error("ioctl(PPPIOCSMAXCID): %m");
	return 0;
    }
    publish_numentry(kSCPropNetPPPIPCPCompressionVJ, vjcomp);
    return 1;
}

/* -----------------------------------------------------------------------------
Config the interface up and enable IP packets to pass
----------------------------------------------------------------------------- */
int sifup(int u)
{
    struct ifreq ifr;

    strlcpy(ifr.ifr_name, ifname, sizeof (ifr.ifr_name));
    if (ioctl(ip_sockfd, SIOCGIFFLAGS, (caddr_t) &ifr) < 0) {
	error("ioctl (SIOCGIFFLAGS): %m");
	return 0;
    }
    ifr.ifr_flags |= IFF_UP;
    if (ioctl(ip_sockfd, SIOCSIFFLAGS, (caddr_t) &ifr) < 0) {
	error("ioctl(SIOCSIFFLAGS): %m");
	return 0;
    }
    if_is_up = 1;
    return 1;
}

/* -----------------------------------------------------------------------------
Set the mode for handling packets for a given NP
----------------------------------------------------------------------------- */
int sifnpmode(int u, int proto, enum NPmode mode)
{
    struct npioctl npi;

    npi.protocol = proto;
    npi.mode = mode;
    if (ioctl(ppp_sockfd, PPPIOCSNPMODE, &npi) < 0) {
	error("ioctl(set NP %d mode to %d): %m", proto, mode);
	return 0;
    }
    return 1;
}

/* -----------------------------------------------------------------------------
Config the interface down and disable IP
----------------------------------------------------------------------------- */
int sifdown(int u)
{
    struct ifreq ifr;
    int rv;
    struct npioctl npi;

    rv = 1;
    npi.protocol = PPP_IP;
    npi.mode = NPMODE_ERROR;
    ioctl(ppp_sockfd, PPPIOCSNPMODE, (caddr_t) &npi);
    /* ignore errors, because ppp_fd might have been closed by now. */

    strlcpy(ifr.ifr_name, ifname, sizeof (ifr.ifr_name));
    if (ioctl(ip_sockfd, SIOCGIFFLAGS, (caddr_t) &ifr) < 0) {
	error("ioctl (SIOCGIFFLAGS): %m");
	rv = 0;
    } else {
	ifr.ifr_flags &= ~IFF_UP;
	if (ioctl(ip_sockfd, SIOCSIFFLAGS, (caddr_t) &ifr) < 0) {
	    error("ioctl(SIOCSIFFLAGS): %m");
	    rv = 0;
	} else
	    if_is_up = 0;
    }
    return rv;
}

/* -----------------------------------------------------------------------------
set the sa_family field of a struct sockaddr, if it exists.
----------------------------------------------------------------------------- */
#define SET_SA_FAMILY(addr, family)		\
    BZERO((char *) &(addr), sizeof(addr));	\
    addr.sa_family = (family); 			\
    addr.sa_len = sizeof(addr);

/* -----------------------------------------------------------------------------
Config the interface IP addresses and netmask
----------------------------------------------------------------------------- */
int sifaddr(int u, u_int32_t o, u_int32_t h, u_int32_t m)
{
    struct ifaliasreq ifra;
    struct ifreq ifr;

    strlcpy(ifra.ifra_name, ifname, sizeof(ifra.ifra_name));
    SET_SA_FAMILY(ifra.ifra_addr, AF_INET);
    ((struct sockaddr_in *) &ifra.ifra_addr)->sin_addr.s_addr = o;
    SET_SA_FAMILY(ifra.ifra_broadaddr, AF_INET);
    ((struct sockaddr_in *) &ifra.ifra_broadaddr)->sin_addr.s_addr = h;
    if (m != 0) {
	SET_SA_FAMILY(ifra.ifra_mask, AF_INET);
	((struct sockaddr_in *) &ifra.ifra_mask)->sin_addr.s_addr = m;
    } else
	BZERO(&ifra.ifra_mask, sizeof(ifra.ifra_mask));
    BZERO(&ifr, sizeof(ifr));
    strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
    if (ioctl(ip_sockfd, SIOCDIFADDR, (caddr_t) &ifr) < 0) {
	if (errno != EADDRNOTAVAIL)
	    warn("Couldn't remove interface address: %m");
    }
    if (ioctl(ip_sockfd, SIOCAIFADDR, (caddr_t) &ifra) < 0) {
	if (errno != EEXIST) {
	    error("Couldn't set interface address: %m");
	    return 0;
	}
	warn("Couldn't set interface address: Address %I already exists", o);
    }
    ifaddrs[0] = o;
    ifaddrs[1] = h;
    
    publish_stateaddr(o, h, m);
    return 1;
}

/* -----------------------------------------------------------------------------
Clear the interface IP addresses, and delete routes
 * through the interface if possible
 ----------------------------------------------------------------------------- */
int cifaddr(int u, u_int32_t o, u_int32_t h)
{
    struct ifaliasreq ifra;

    unpublish_entity(kSCEntNetIPv4);

    ifaddrs[0] = 0;
    strlcpy(ifra.ifra_name, ifname, sizeof(ifra.ifra_name));
    SET_SA_FAMILY(ifra.ifra_addr, AF_INET);
    ((struct sockaddr_in *) &ifra.ifra_addr)->sin_addr.s_addr = o;
    SET_SA_FAMILY(ifra.ifra_broadaddr, AF_INET);
    ((struct sockaddr_in *) &ifra.ifra_broadaddr)->sin_addr.s_addr = h;
    BZERO(&ifra.ifra_mask, sizeof(ifra.ifra_mask));
    if (ioctl(ip_sockfd, SIOCDIFADDR, (caddr_t) &ifra) < 0) {
	if (errno != EADDRNOTAVAIL)
	    warn("Couldn't delete interface address: %m");
	return 0;
    }
    return 1;
}

/* -----------------------------------------------------------------------------
assign a default route through the address given 
----------------------------------------------------------------------------- */
int sifdefaultroute(int u, u_int32_t l, u_int32_t g)
{
    CFNumberRef		num;
    CFStringRef		key;
    int			ret = ENOMEM, val = 1;
    
    // ppp daemons without services are not published in the cache
    if (cfgCache == NULL)
        return 0;
    
    key = SCDKeyCreateNetworkServiceEntity(kSCCacheDomainState, serviceidRef, kSCEntNetIPv4);
    if (key) {
        num = CFNumberCreate(NULL, kCFNumberIntType, &val);
        if (num) {
            ret = publish_keyentry(key, CFSTR("OverridePrimary"), num);
            CFRelease(num);
            ret = 0;
        }
        CFRelease(key);
    }
    return ret;
}

/* -----------------------------------------------------------------------------
delete a default route through the address given
----------------------------------------------------------------------------- */
int cifdefaultroute(int u, u_int32_t l, u_int32_t g)
{
    CFStringRef		key;
    int			ret = ENOMEM;

    key = SCDKeyCreateNetworkServiceEntity(kSCCacheDomainState, serviceidRef, kSCEntNetIPv4);
    if (key) {
        ret = unpublish_keyentry(key, CFSTR("OverridePrimary"));
        CFRelease(key);
        ret = 0;
    }
    return ret;
}

/* -----------------------------------------------------------------------------
publish ip addresses using configd cache mechanism
use new state information model
----------------------------------------------------------------------------- */
int publish_stateaddr(u_int32_t o, u_int32_t h, u_int32_t m)
{
    struct in_addr		addr;
    CFMutableArrayRef		array;
    CFStringRef			ifnam = 0;
    CFMutableDictionaryRef	ipv4_dict = 0;
    SCDHandleRef 		ipv4_data = 0;
    CFStringRef			ipv4_key = 0;
    SCDStatus			status;
    CFStringRef			str;

    // ppp daemons without services are not published in the cache
    if (cfgCache == NULL)
        return 0;
            
    // then publish configd cache information
    ifnam = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s"), ifname);       

    /* create the IP cache key and dict */
    ipv4_key = SCDKeyCreateNetworkServiceEntity(kSCCacheDomainState, serviceidRef, kSCEntNetIPv4);
    ipv4_dict = CFDictionaryCreateMutable(0, 0,
                                          &kCFTypeDictionaryKeyCallBacks,
                                          &kCFTypeDictionaryValueCallBacks);

    /* set the ip address array */
    addr.s_addr = o;
    array = CFArrayCreateMutable(0, 1, &kCFTypeArrayCallBacks);
    str = CFStringCreateWithFormat(0, 0, CFSTR(IP_FORMAT), IP_LIST(&addr));
    CFArrayAppendValue(array, str);
    CFRelease(str);
    CFDictionarySetValue(ipv4_dict, kSCPropNetIPv4Addresses, array);
    CFRelease(array);

    /* set the ip dest array */
    addr.s_addr = h;
    array = CFArrayCreateMutable(NULL, 1, &kCFTypeArrayCallBacks);
    str = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT), 
				   IP_LIST(&addr));
    CFArrayAppendValue(array, str);
    CFRelease(str);
    CFDictionarySetValue(ipv4_dict, kSCPropNetIPv4DestAddresses, array);
    CFRelease(array);

    /* set the router */
    addr.s_addr = h;
    str = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT), IP_LIST(&addr));
    CFDictionarySetValue(ipv4_dict, kSCPropNetIPv4Router, str);
    CFRelease(str);

    /* add the interface name */
    CFDictionarySetValue(ipv4_dict, CFSTR("InterfaceName"), ifnam);

    /* get system configuration cache handles */
    ipv4_data = SCDHandleInit();
    SCDHandleSetData(ipv4_data, ipv4_dict);
    CFRelease(ipv4_dict);

    /* update atomically to avoid needless notifications */
    SCDLock(cfgCache);
    SCDRemove(cfgCache, ipv4_key);

    status = SCDAdd(cfgCache, ipv4_key, ipv4_data);
    if (status != SCD_OK)
        warn("SCDAdd IP %s failed: %s\n", ifname, SCDError(status));

    SCDUnlock(cfgCache);
    SCDHandleRelease(ipv4_data);
    CFRelease(ipv4_key);
    CFRelease(ifnam);
    return 1;
}

/* -----------------------------------------------------------------------------
 add the default route and DNS, using configd cache mechanism.
----------------------------------------------------------------------------- */
int publish_dns(u_int32_t dns1, u_int32_t dns2)
{
    int				i;
    struct in_addr		addr, dnsaddr[2];
    CFMutableArrayRef		array;
    CFMutableDictionaryRef	dns_dict = 0;
    SCDHandleRef 		dns_data = 0;
    CFStringRef			dns_key = 0;
    SCDStatus			status;
    CFStringRef			str;

    // ppp daemons without services are not published in the cache
    if (cfgCache == NULL)
        return 0;

    dnsaddr[0].s_addr = dns1;
    dnsaddr[1].s_addr = dns2;

    dns_key = SCDKeyCreateNetworkServiceEntity(kSCCacheDomainState, serviceidRef, kSCEntNetDNS);

    dns_dict = CFDictionaryCreateMutable(0, 0,
                                            &kCFTypeDictionaryKeyCallBacks,
                                            &kCFTypeDictionaryValueCallBacks);
    
    /* set the DNS servers */
    array = CFArrayCreateMutable(NULL, 2, &kCFTypeArrayCallBacks);
    for (i = 0; i < 2; i++) {
        if (dnsaddr[i].s_addr) {
            str = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT), 
                                            IP_LIST(&dnsaddr[i]));
            CFArrayAppendValue(array, str);
            CFRelease(str);
        }
    }
    CFDictionarySetValue(dns_dict, kSCPropNetDNSServerAddresses, array);
    CFRelease(array);

    dns_data = SCDHandleInit();
    SCDHandleSetData(dns_data, dns_dict);
    CFRelease(dns_dict);

    /* update atomically to avoid needless notifications */
    SCDLock(cfgCache);
    SCDRemove(cfgCache, dns_key);
    status = SCDAdd(cfgCache, dns_key, dns_data);
    if (status != SCD_OK)
        warn("SCDAdd DNS %s returned: %s\n", ifname, SCDError(status));
    SCDUnlock(cfgCache);
    
    SCDHandleRelease(dns_data);
    CFRelease(dns_key);
    return 1;
}

/* -----------------------------------------------------------------------------
 remove the DNS, using configd cache mechanism.
----------------------------------------------------------------------------- */
int unpublish_dns()
{
    CFStringRef		dns_key;

    // ppp daemons without services are not published in the cache
    if (cfgCache == NULL)
        return 0;

    /* create DNS cache key */
    dns_key = SCDKeyCreateNetworkServiceEntity(kSCCacheDomainState, serviceidRef, kSCEntNetDNS);

    /* update atomically to avoid needless notifications */
    SCDLock(cfgCache);
    SCDRemove(cfgCache, dns_key);
    SCDUnlock(cfgCache);

    CFRelease(dns_key);
    return 1;
}

/* -----------------------------------------------------------------------------
set dns information
----------------------------------------------------------------------------- */
int sifdns(u_int32_t dns1, u_int32_t dns2)
{
    return publish_dns(dns1, dns2);
}

/* -----------------------------------------------------------------------------
clear dns information
----------------------------------------------------------------------------- */
int cifdns(u_int32_t dns1, u_int32_t dns2)
{
    return unpublish_entity(kSCEntNetDNS);
}


static struct {
    struct rt_msghdr		hdr;
    struct sockaddr_inarp	dst;
    struct sockaddr_dl		hwa;
    char			extra[128];
} arpmsg;

static int arpmsg_valid;

/* -----------------------------------------------------------------------------
Make a proxy ARP entry for the peer
----------------------------------------------------------------------------- */
int sifproxyarp(int unit, u_int32_t hisaddr)
{
    int routes;

    /*
     * Get the hardware address of an interface on the same subnet
     * as our local address.
     */
    memset(&arpmsg, 0, sizeof(arpmsg));
    if (!get_ether_addr(hisaddr, &arpmsg.hwa)) {
	error("Cannot determine ethernet address for proxy ARP");
	return 0;
    }

    if ((routes = socket(PF_ROUTE, SOCK_RAW, AF_INET)) < 0) {
	error("Couldn't add proxy arp entry: socket: %m");
	return 0;
    }

    arpmsg.hdr.rtm_type = RTM_ADD;
    arpmsg.hdr.rtm_flags = RTF_ANNOUNCE | RTF_HOST | RTF_STATIC;
    arpmsg.hdr.rtm_version = RTM_VERSION;
    arpmsg.hdr.rtm_seq = ++rtm_seq;
    arpmsg.hdr.rtm_addrs = RTA_DST | RTA_GATEWAY;
    arpmsg.hdr.rtm_inits = RTV_EXPIRE;
    arpmsg.dst.sin_len = sizeof(struct sockaddr_inarp);
    arpmsg.dst.sin_family = AF_INET;
    arpmsg.dst.sin_addr.s_addr = hisaddr;
    arpmsg.dst.sin_other = SIN_PROXY;

    arpmsg.hdr.rtm_msglen = (char *) &arpmsg.hwa - (char *) &arpmsg
	+ arpmsg.hwa.sdl_len;
    if (write(routes, &arpmsg, arpmsg.hdr.rtm_msglen) < 0) {
	error("Couldn't add proxy arp entry: %m");
	close(routes);
	return 0;
    }

    close(routes);
    arpmsg_valid = 1;
    proxy_arp_addr = hisaddr;
    return 1;
}

/* -----------------------------------------------------------------------------
Delete the proxy ARP entry for the peer
----------------------------------------------------------------------------- */
int cifproxyarp(int unit, u_int32_t hisaddr)
{
    int routes;

    if (!arpmsg_valid)
	return 0;
    arpmsg_valid = 0;

    arpmsg.hdr.rtm_type = RTM_DELETE;
    arpmsg.hdr.rtm_seq = ++rtm_seq;

    if ((routes = socket(PF_ROUTE, SOCK_RAW, AF_INET)) < 0) {
	error("Couldn't delete proxy arp entry: socket: %m");
	return 0;
    }

    if (write(routes, &arpmsg, arpmsg.hdr.rtm_msglen) < 0) {
	error("Couldn't delete proxy arp entry: %m");
	close(routes);
	return 0;
    }

    close(routes);
    proxy_arp_addr = 0;
    return 1;
}

#define MAX_IFS		32

/* -----------------------------------------------------------------------------
get the hardware address of an interface on the
 * the same subnet as ipaddr.
----------------------------------------------------------------------------- */
static int get_ether_addr(u_int32_t ipaddr, struct sockaddr_dl *hwaddr)
{
    struct ifreq *ifr, *ifend, *ifp;
    u_int32_t ina, mask;
    struct sockaddr_dl *dla;
    struct ifreq ifreq;
    struct ifconf ifc;
    struct ifreq ifs[MAX_IFS];

    ifc.ifc_len = sizeof(ifs);
    ifc.ifc_req = ifs;
    if (ioctl(ip_sockfd, SIOCGIFCONF, &ifc) < 0) {
	error("ioctl(SIOCGIFCONF): %m");
	return 0;
    }

    /*
     * Scan through looking for an interface with an Internet
     * address on the same subnet as `ipaddr'.
     */
    ifend = (struct ifreq *) (ifc.ifc_buf + ifc.ifc_len);
    for (ifr = ifc.ifc_req; ifr < ifend; ifr = (struct ifreq *)
	 	((char *)&ifr->ifr_addr + ifr->ifr_addr.sa_len)) {
	if (ifr->ifr_addr.sa_family == AF_INET) {
	    ina = ((struct sockaddr_in *) &ifr->ifr_addr)->sin_addr.s_addr;
	    strlcpy(ifreq.ifr_name, ifr->ifr_name, sizeof(ifreq.ifr_name));
	    /*
	     * Check that the interface is up, and not point-to-point
	     * or loopback.
	     */
	    if (ioctl(ip_sockfd, SIOCGIFFLAGS, &ifreq) < 0)
		continue;
	    if ((ifreq.ifr_flags &
		 (IFF_UP|IFF_BROADCAST|IFF_POINTOPOINT|IFF_LOOPBACK|IFF_NOARP))
		 != (IFF_UP|IFF_BROADCAST))
		continue;
	    /*
	     * Get its netmask and check that it's on the right subnet.
	     */
	    if (ioctl(ip_sockfd, SIOCGIFNETMASK, &ifreq) < 0)
		continue;
	    mask = ((struct sockaddr_in *) &ifreq.ifr_addr)->sin_addr.s_addr;
	    if ((ipaddr & mask) != (ina & mask))
		continue;

	    break;
	}
    }

    if (ifr >= ifend)
	return 0;
    info("found interface %s for proxy arp", ifr->ifr_name);

    /*
     * Now scan through again looking for a link-level address
     * for this interface.
     */
    ifp = ifr;
    for (ifr = ifc.ifc_req; ifr < ifend; ) {
	if (strcmp(ifp->ifr_name, ifr->ifr_name) == 0
	    && ifr->ifr_addr.sa_family == AF_LINK) {
	    /*
	     * Found the link-level address - copy it out
	     */
	    dla = (struct sockaddr_dl *) &ifr->ifr_addr;
	    BCOPY(dla, hwaddr, dla->sdl_len);
	    return 1;
	}
	ifr = (struct ifreq *) ((char *)&ifr->ifr_addr + ifr->ifr_addr.sa_len);
    }

    return 0;
}

/* -----------------------------------------------------------------------------
 * Return user specified netmask, modified by any mask we might determine
 * for address `addr' (in network byte order).
 * Here we scan through the system's list of interfaces, looking for
 * any non-point-to-point interfaces which might appear to be on the same
 * network as `addr'.  If we find any, we OR in their netmask to the
 * user-specified netmask.
----------------------------------------------------------------------------- */
u_int32_t GetMask(u_int32_t addr)
{
    u_int32_t mask, nmask, ina;
    struct ifreq *ifr, *ifend, ifreq;
    struct ifconf ifc;
    struct ifreq ifs[MAX_IFS];

    addr = ntohl(addr);
    if (IN_CLASSA(addr))	/* determine network mask for address class */
	nmask = IN_CLASSA_NET;
    else if (IN_CLASSB(addr))
	nmask = IN_CLASSB_NET;
    else
	nmask = IN_CLASSC_NET;
    /* class D nets are disallowed by bad_ip_adrs */
    mask = netmask | htonl(nmask);

    /*
     * Scan through the system's network interfaces.
     */
    ifc.ifc_len = sizeof(ifs);
    ifc.ifc_req = ifs;
    if (ioctl(ip_sockfd, SIOCGIFCONF, &ifc) < 0) {
	warn("ioctl(SIOCGIFCONF): %m");
	return mask;
    }
    ifend = (struct ifreq *) (ifc.ifc_buf + ifc.ifc_len);
    for (ifr = ifc.ifc_req; ifr < ifend; ifr = (struct ifreq *)
	 	((char *)&ifr->ifr_addr + ifr->ifr_addr.sa_len)) {
	/*
	 * Check the interface's internet address.
	 */
	if (ifr->ifr_addr.sa_family != AF_INET)
	    continue;
	ina = ((struct sockaddr_in *) &ifr->ifr_addr)->sin_addr.s_addr;
	if ((ntohl(ina) & nmask) != (addr & nmask))
	    continue;
	/*
	 * Check that the interface is up, and not point-to-point or loopback.
	 */
	strlcpy(ifreq.ifr_name, ifr->ifr_name, sizeof(ifreq.ifr_name));
	if (ioctl(ip_sockfd, SIOCGIFFLAGS, &ifreq) < 0)
	    continue;
	if ((ifreq.ifr_flags & (IFF_UP|IFF_POINTOPOINT|IFF_LOOPBACK))
	    != IFF_UP)
	    continue;
	/*
	 * Get its netmask and OR it into our mask.
	 */
	if (ioctl(ip_sockfd, SIOCGIFNETMASK, &ifreq) < 0)
	    continue;
	mask |= ((struct sockaddr_in *)&ifreq.ifr_addr)->sin_addr.s_addr;
    }

    return mask;
}

/* -----------------------------------------------------------------------------
determine if the system has any route to
 * a given IP address.
 * For demand mode to work properly, we have to ignore routes
 * through our own interface.
 ----------------------------------------------------------------------------- */
int have_route_to(u_int32_t addr)
{
    return -1;
}

/* -----------------------------------------------------------------------------
Use the hostid as part of the random number seed
----------------------------------------------------------------------------- */
int get_host_seed()
{
    return gethostid();
}


#if 0

#define	LOCK_PREFIX	"/var/spool/lock/LCK.."

static char *lock_file;		/* name of lock file created */

/* -----------------------------------------------------------------------------
create a lock file for the named lock device
----------------------------------------------------------------------------- */
int lock(char *dev)
{
    char hdb_lock_buffer[12];
    int fd, pid, n;
    char *p;
    size_t l;

    if ((p = strrchr(dev, '/')) != NULL)
	dev = p + 1;
    l = strlen(LOCK_PREFIX) + strlen(dev) + 1;
    lock_file = malloc(l);
    if (lock_file == NULL)
	novm("lock file name");
    slprintf(lock_file, l, "%s%s", LOCK_PREFIX, dev);

    while ((fd = open(lock_file, O_EXCL | O_CREAT | O_RDWR, 0644)) < 0) {
	if (errno == EEXIST
	    && (fd = open(lock_file, O_RDONLY, 0)) >= 0) {
	    /* Read the lock file to find out who has the device locked */
	    n = read(fd, hdb_lock_buffer, 11);
	    if (n <= 0) {
		error("Can't read pid from lock file %s", lock_file);
		close(fd);
	    } else {
		hdb_lock_buffer[n] = 0;
		pid = atoi(hdb_lock_buffer);
		if (kill(pid, 0) == -1 && errno == ESRCH) {
		    /* pid no longer exists - remove the lock file */
		    if (unlink(lock_file) == 0) {
			close(fd);
			notice("Removed stale lock on %s (pid %d)",
			       dev, pid);
			continue;
		    } else
			warn("Couldn't remove stale lock on %s",
			       dev);
		} else
		    notice("Device %s is locked by pid %d",
			   dev, pid);
	    }
	    close(fd);
	} else
	    error("Can't create lock file %s: %m", lock_file);
	free(lock_file);
	lock_file = NULL;
	return -1;
    }

    slprintf(hdb_lock_buffer, sizeof(hdb_lock_buffer), "%10d\n", getpid());
    write(fd, hdb_lock_buffer, 11);

    close(fd);
    return 0;
}

/* -----------------------------------------------------------------------------
remove our lockfile
----------------------------------------------------------------------------- */
void unlock()
{
    if (lock_file) {
	unlink(lock_file);
	free(lock_file);
	lock_file = NULL;
    }
}
#endif

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int sys_loadplugin(char *arg)
{
    char		path[MAXPATHLEN];
    int 		ret = -1;
    CFBundleRef		bundle;
    CFURLRef		url;
    int 		(*start)(CFBundleRef);


    if (arg[0] == '/') {
        strcpy(path, arg);
    }
    else {
        strcpy(path, "/System/Library/Extensions/");
        strcat(path, arg);
    } 

    url = CFURLCreateFromFileSystemRepresentation(NULL, path, strlen(path), TRUE);
    if (url) {        
        bundle =  CFBundleCreate(NULL, url);
        if (bundle) {

            if (CFBundleLoadExecutable(bundle)
                && (start = CFBundleGetFunctionPointerForName(bundle, CFSTR("start")))) {
            
                ret = (*start)(bundle);
            }
                        
            CFRelease(bundle);
        }
        CFRelease(url);
    }
    return ret;
}

#if 0
/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
CFTypeRef getEntity()
{
    CFTypeRef		data = NULL;
    CFStringRef		key;
    SCDHandleRef	handle = NULL;

    key = SCDKeyCreate(CFSTR("%@/%@/%s/%s"), kSCCacheDomainState, kSCCompNetwork, "PPP", serviceid);
    if (key) {
        if (SCDGet(cfgCache, key, &handle) == SCD_OK) {
            data  = SCDHandleGetData(handle);
            if (data) {
                if (CFGetTypeID(data) == CFDictionaryGetTypeID())	
                    data = (CFTypeRef)CFDictionaryCreateMutableCopy(NULL, 0, data);
                else if (CFGetTypeID(data) == CFStringGetTypeID())
                    data = (CFTypeRef)CFStringCreateCopy(NULL, data);
            }
            SCDHandleRelease(handle);
        }
        CFRelease(key);
    }
    return data;
}
#endif

/* -----------------------------------------------------------------------------
publish a dictionnary entry in the cache
----------------------------------------------------------------------------- */
int publish_keyentry(CFStringRef key, CFStringRef entry, CFTypeRef value)
{
    CFDictionaryRef		dict;
    CFMutableDictionaryRef	newDict = 0;
    SCDHandleRef 		handle = 0;
    SCDStatus			status;

    // ppp daemons without services are not published in the cache
    if (cfgCache == NULL)
        return 0;
            
    status = SCDGet(cfgCache, key, &handle);
    if (status == SCD_OK) {
        if (dict = SCDHandleGetData(handle)) 
            newDict = CFDictionaryCreateMutableCopy(0, 0, dict);
    }
    else {
        if (handle = SCDHandleInit())
            newDict = CFDictionaryCreateMutable(0, 0,
                                          &kCFTypeDictionaryKeyCallBacks,
                                          &kCFTypeDictionaryValueCallBacks);
    }
    
    if (newDict == NULL)
        goto done;

    CFDictionarySetValue(newDict,  entry, value);
    SCDHandleSetData(handle, newDict);
    status = SCDSet(cfgCache, key, handle);
    if (status != SCD_OK) {
        warn("publish_entry SCDSet() failed: %s\n", SCDError(status));
    }

done:
    if (handle) SCDHandleRelease(handle);
    if (newDict) CFRelease(newDict);
    return 0;
}

/* -----------------------------------------------------------------------------
publish a dictionnary entry in the cache
----------------------------------------------------------------------------- */
int publish_numentry(CFStringRef entry, int val)
{
    int			ret = ENOMEM;
    CFNumberRef		num;
    CFStringRef		key;

    // ppp daemons without services are not published in the cache
    if (cfgCache == NULL)
        return 0;
    
    key = SCDKeyCreateNetworkServiceEntity(kSCCacheDomainState, serviceidRef, kSCEntNetPPP);
    if (key) {
        num = CFNumberCreate(NULL, kCFNumberIntType, &val);
        if (num) {
            ret = publish_keyentry(key, entry, num);
            CFRelease(num);
            ret = 0;
        }
        CFRelease(key);
    }
    return ret;
}

/* -----------------------------------------------------------------------------
publish a dictionnary entry in the cache
----------------------------------------------------------------------------- */
int publish_strentry(CFStringRef entry, char *str, int encoding)
{
    int			ret = ENOMEM;
    CFStringRef 	ref;
    CFStringRef		key;
    
    // ppp daemons without services are not published in the cache
    if (cfgCache == NULL)
        return 0;

    key = SCDKeyCreateNetworkServiceEntity(kSCCacheDomainState, serviceidRef, kSCEntNetPPP);
    if (key) {
        ref = CFStringCreateWithCString(NULL, str, encoding);
        if (ref) {
            ret = publish_keyentry(key, entry, ref);
            CFRelease(ref);
            ret = 0;
        }
        CFRelease(key);
    }
    return ret;
}

/* -----------------------------------------------------------------------------
unpublish a disctionnary entry from the cache
----------------------------------------------------------------------------- */
int unpublish_keyentry(CFStringRef key, CFStringRef entry)
{
    CFDictionaryRef		dict;
    CFMutableDictionaryRef	newDict;
    SCDHandleRef 		handle = 0;
    SCDStatus			status;
    CFStringRef			str;

    // ppp daemons without services are not published in the cache
    if (cfgCache == NULL)
        return 0;
        
    status = SCDGet(cfgCache, key, &handle);
    if (status != SCD_OK) {
        //warn("unpublish_keyentry SCDGet() failed: %s\n", SCDError(status));
        return 0;
    }
    
    dict = SCDHandleGetData(handle);
    newDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);

    CFDictionaryRemoveValue(newDict, entry);

    /* update status */
    SCDHandleSetData(handle, newDict);
    status = SCDSet(cfgCache, key, handle);
    if (status != SCD_OK) {
        warn("unpublish_keyentry SCDSet() failed: %s\n", SCDError(status));
    }

    SCDHandleRelease(handle);
    CFRelease(newDict);
    return 0;
}

/* -----------------------------------------------------------------------------
unpublish a complete dictionnary from the cache
----------------------------------------------------------------------------- */
int unpublish_entity(CFStringRef entity)
{
    int			ret = ENOMEM;
    SCDStatus		status;
    CFStringRef		key;

    if (cfgCache == NULL)
        return 0;
            
    key = SCDKeyCreateNetworkServiceEntity(kSCCacheDomainState, serviceidRef, entity);
    if (key) {
        status = SCDRemove(cfgCache, key);
        if (status != SCD_OK) {
            //warn("unpublish_entity SCDRemove() failed: %s\n", SCDError(status));
            ret = 1;
        }
        else 
            ret = 0;
        CFRelease(key);
    }
    return ret;
}

/* -----------------------------------------------------------------------------
publish a dictionnary entry in the cache
----------------------------------------------------------------------------- */
int unpublish_entry(CFStringRef entry)
{
    int			ret = ENOMEM;
    CFStringRef		key;
    
    key = SCDKeyCreateNetworkServiceEntity(kSCCacheDomainState, serviceidRef, kSCEntNetPPP);
    if (key) {
        ret = unpublish_keyentry(key, entry);
        CFRelease(key);
        ret = 0;
    }
    return ret;
}

/* -----------------------------------------------------------------------------
get the current logged in user
----------------------------------------------------------------------------- */
int sys_getconsoleuser(uid_t *uid)
{
    char 	str[32];
    gid_t	gid;

    status = SCDConsoleUserGet(str, sizeof(str), uid, &gid);
    if (status == SCD_OK)
        return 0;
        
    return -1;
}

/* -----------------------------------------------------------------------------
our new phase hook
----------------------------------------------------------------------------- */
void sys_phasechange(void *arg, int p)
{
    struct timeval 	t;

    // publish DEAD status when we exit only, to avoid race conditions
    if (p != PHASE_DEAD)
        publish_numentry(CFSTR("Status"), p);
        
    if (p == PHASE_ESTABLISH) {
        gettimeofday(&t, NULL);    
        publish_numentry(CFSTR("ConnectTime"), t.tv_sec);
    }

    if (p == PHASE_RUNNING || p == PHASE_DORMANT) {
        publish_strentry(CFSTR("InterfaceName"), ifname, kCFStringEncodingMacRoman);
        //publish_numentry(CFSTR("LastCause"), status);
        if (p == PHASE_DORMANT)
            sys_eventnotify((void*)PPP_EVT_DISCONNECTED, status);
    }

    if (p == PHASE_TERMINATE) {
        unpublish_entry(CFSTR("InterfaceName"));
        unpublish_entry(CFSTR("ConnectTime"));
    }
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void sys_publish_status(u_long status)
{
  publish_numentry(CFSTR("LastCause"), status);
}

/* -----------------------------------------------------------------------------
our pid has changed, reinit things
----------------------------------------------------------------------------- */
void sys_reinit()
{
    SCDStatus		status;
    
    // reopen opens now our session to the cache
    // serviceid paramater is required for publishing information into the cache.
    // a user could be willin to publish everything itself, without pppd help
    if (serviceid == NULL)
        return;
        
    cfgCache = 0;
    status = SCDOpen(&cfgCache, CFSTR("pppd"));
    if (status != SCD_OK)
        fatal("SCDOpen failed: %s", SCDError(status));
    
    publish_numentry(CFSTR("pid"), getpid());
}

/* ----------------------------------------------------------------------------- 
we are exiting
----------------------------------------------------------------------------- */
void sys_exitnotify(void *arg, int exitcode)
{
    
    // unpublish the various info about the connection
    unpublish_entry(kSCPropNetPPPLCPCompressionPField);
    unpublish_entry(kSCPropNetPPPLCPCompressionACField);
    unpublish_entry(kSCPropNetPPPLCPMTU);
    unpublish_entry(kSCPropNetPPPLCPMRU);
    unpublish_entry(kSCPropNetPPPLCPReceiveACCM);
    unpublish_entry(kSCPropNetPPPLCPTransmitACCM);
    unpublish_entry(kSCPropNetPPPIPCPCompressionVJ);
    unpublish_entry(CFSTR("pid"));
    unpublish_entry(CFSTR("ConnectTime"));
    unpublish_entity(kSCEntNetDNS);
    unpublish_entry(CFSTR("InterfaceName"));

    /* publish status at the end to avoid race conditions with the controller */
    publish_numentry(CFSTR("LastCause"), exitcode);
    publish_numentry(CFSTR("Status"), PHASE_DEAD);
    sys_eventnotify((void*)PPP_EVT_DISCONNECTED, exitcode);

    if (tempserviceid) {
    	unpublish_entity(kSCEntNetPPP);
    }
    
    if (cfgCache) {
        SCDClose(&cfgCache);
        cfgCache = 0;
    }

    if (csockfd != -1) {
        close(csockfd);
        csockfd = -1;
    }
}
