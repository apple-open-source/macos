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

#define RCSID	"$Id: sys-MacOSX.c,v 1.12 2001/01/26 23:50:45 callie Exp $"

/*	$NetBSD: sys-bsd.c,v 1.1.1.3 1997/09/26 18:53:04 christos Exp $	*/

/*
 * TODO:
 */


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
#ifdef NetBSD1_2
#include <util.h>
#endif
#ifdef PPP_FILTER
#include <net/bpf.h>
#endif

#include <net/if.h>
//#include <net/ppp_defs.h>
//#include <net/if_ppp.h>
#include <net/route.h>
#include <net/if_dl.h>
#include <netinet/in.h>

#include <sys/param.h>
#include <netinet/if_ether.h>
#include <sys/ttycom.h>

#include "../../Family/PPP.kmodproj/ppp.h"
#include "../../Family/PPP.kmodproj/ppp_domain.h"
#include "../../Drivers/PPPSerial/PPPSerial.kmodproj/pppserial.h"
#include "pppd.h"
#include "fsm.h"
#include "ipcp.h"
#include "lcp.h"

#include <syslog.h>
#include <sys/un.h>
#include <pthread.h>
//#include <../ppplib/ppp_msg.h>
//#include <../pppconfd/pppconfd.h>
#define	SYSTEMCONFIGURATION_NEW_API
#include <SystemConfiguration/SystemConfiguration.h>
#if 0
#include <NetworkConfiguration/NetworkConfig.h>
#include <SystemConfiguration/SCD.h>
#include <SystemConfiguration/SCDKeys.h>
#endif

#define IP_FORMAT	"%d.%d.%d.%d"
#define IP_CH(ip)	((u_char *)(ip))
#define IP_LIST(ip)	IP_CH(ip)[0],IP_CH(ip)[1],IP_CH(ip)[2],IP_CH(ip)[3]

static const char rcsid[] = RCSID;

static int initdisc = -1;	/* Initial TTY discipline for ppp_fd */
static int initfdflags = -1;	/* Initial file descriptor flags for ppp_fd */
static int ppp_fd = -1;		/* fd which is set to PPP discipline */
static int rtm_seq;

static int restore_term;	/* 1 => we've munged the terminal */
static struct termios inittermios; /* Initial TTY termios */
static struct winsize wsinfo;	/* Initial window size info */

static int loop_slave = -1;
static int loop_master;
static char loop_name[20];

static unsigned char inbuf[512]; /* buffer for chars read from loopback */

static int sockfd;		/* socket for doing interface ioctls */
static int sockppp;		/* socket to control the ppp interface */

static fd_set in_fds;		/* set of fds that wait_input waits for */
static int max_in_fd;		/* highest fd set in in_fds */

static int if_is_up;		/* the interface is currently up */
static u_int32_t ifaddrs[2];	/* local and remote addresses we set */
static u_int32_t default_route_gateway;	/* gateway addr for default route */
static u_int32_t proxy_arp_addr;	/* remote addr for proxy arp */

/* Prototypes for procedures local to this file. */
static int dodefaultroute __P((u_int32_t, int));
static int get_ether_addr __P((u_int32_t, struct sockaddr_dl *));

SCDSessionRef		configd_session = NULL;
int			configd_session_inited = 0;

static u_char 	ppp_buf[PPP_MRU+PPP_HDRLEN+4];

// #define LOADPPPK 1
#ifdef LOADPPPK
/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long isPPPKLoaded()
{
    struct ifpppstatsreq      rq;
    int                       s;

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
        return 0;
    }

    memset (&rq, 0, sizeof (rq));
    strncpy(rq.ifr_name, "ppp0", sizeof(rq.ifr_name));
    if (ioctl(s, SIOCGPPPSTATS, &rq) < 0) {
        close(s);
        return 0;
    }

    close(s);
    return 1;
}

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
u_long loadPPPK()
{
    int pid;

    if (!isPPPKLoaded()) {

        if ((pid = fork()) < 0)
            return 1;

        if (pid == 0) {
            closeall();
            // PPP kernel extension not loaded, try load it...
            execl("/sbin/kextload", "kextload", "/System/Library/Extensions/PPPSerial.kext", (char *)0);
            exit(1);
        }

        if (waitpid(pid, 0, 0) < 0) {
            // fail
            return 1;
        }
    }
    return 0;
}
#endif

/*
 * sys_init - System-dependent initialization.
 */
void
sys_init()
{
    int 		err;
    u_long		lval;
    struct sockaddr_pppctl 	pppaddr;
     int 			flags;
       
    /* Get an internet socket for doing socket ioctl's on. */
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        fatal("Couldn't create IP socket: %m");
        
    if ((sockppp = socket(PF_PPP, SOCK_DGRAM, 0)) < 0)
        fatal("can't create PF_PPP socket: %m");

    /* need to connect to it */
    pppaddr.ppp_len = sizeof(struct sockaddr_pppctl);
    pppaddr.ppp_family = AF_PPPCTL;
    pppaddr.ppp_reserved = 0;
    if (connect(sockppp, (struct sockaddr *)&pppaddr, sizeof(struct sockaddr_pppctl)) < 0) {
        fatal("Couldn't connect to PF_PPP socket: %m");
    }

    /* set socket for non-blocking reads. */
    if ((flags = fcntl(sockppp, F_GETFL)) == -1
        || fcntl(sockppp, F_SETFL, flags | O_NONBLOCK) == -1) 
        fatal("Couldn't set ppp_sockfd to non-blocking mode: %m");

    
    FD_ZERO(&in_fds);
    max_in_fd = 0;
}

/*
 * sys_cleanup - restore any system state we modified before exiting:
 * mark the interface down, delete default route and/or proxy arp entry.
 * This should call die() because it's called from die().
 */
void
sys_cleanup()
{
    struct ifreq ifr;

    if (configd_session_inited) {
        SCDClose(&configd_session);
        configd_session_inited = 0;
    }
    
    if (if_is_up) {
	strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) >= 0
	    && ((ifr.ifr_flags & IFF_UP) != 0)) {
	    ifr.ifr_flags &= ~IFF_UP;
	    ioctl(sockfd, SIOCSIFFLAGS, &ifr);
	}
    }
    if (ifaddrs[0] != 0)
	cifaddr(0, ifaddrs[0], ifaddrs[1]);
    if (default_route_gateway)
	cifdefaultroute(0, 0, default_route_gateway);
    if (proxy_arp_addr)
	cifproxyarp(0, proxy_arp_addr);
}

/*
 * sys_close - Clean up in a child process before execing.
 */
void
sys_close()
{
    close(sockfd);
    if (loop_slave >= 0) {
	close(loop_slave);
	close(loop_master);
    }
}


void sys_change_pid()
{
    pid_t		pid = getpid();
    SCDStatus		status;

    status = SCDOpen(&configd_session, CFSTR("pppd"));
    if (status != SCD_OK) {
        warn("SCDOpen failed: %s\n", SCDError(status));
    }
    else configd_session_inited = 1;

}


/*
 * sys_check_options - check the options that the user specified
 */
int
sys_check_options()
{
#ifndef CDTRCTS
    if (crtscts == 2) {
	warn("DTR/CTS flow control is not supported on this system");
	return 0;
    }
#endif
    return 1;
}

/*
 * ppp_available - check whether the system has any ppp interfaces
 * (in fact we check whether we can do an ioctl on ppp0).
 */
int
ppp_available()
{
    int s, ok;
    struct ifreq ifr;
    extern char *no_ppp_msg;

#ifdef LOADPPPK
    loadPPPK();
#endif
    
    if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	return 1;		/* can't tell */

    strlcpy(ifr.ifr_name, "ppp0", sizeof (ifr.ifr_name));
    ok = ioctl(s, SIOCGIFFLAGS, (caddr_t) &ifr) >= 0;
    close(s);

    no_ppp_msg = "\
This system lacks kernel support for PPP.  To include PPP support\n\
in the kernel, please follow the steps detailed in the README.MacOSX\n\
file in the ppp-2.3 distribution.\n";
    return ok;
}

/*
 * establish_ppp - Turn the serial port into a ppp interface.
 */
int
establish_ppp(fd)
    int fd;
{
    int pppdisc = PPPDISC;
    int x;
    struct ifpppreq    	req;
    u_long	lval;

    if (demand) {
	/*
	 * Demand mode - prime the old ppp device to relinquish the unit.
	 */
#if 0
	if (ioctl(ppp_fd, PPPIOCXFERUNIT, 0) < 0)
	    fatal("ioctl(transfer ppp unit): %m");
#endif
    }

    if (!demand) {
	/*
	 * create a new unit first.
	 */
            bzero(&req, sizeof(struct ifpppreq));
            sprintf(req.ifr_name, "ppp0", sizeof (req.ifr_name));
            req.ifr_code = IFPPP_NEWIF;
            if (ioctl(sockfd, SIOCSIFPPP, (caddr_t) &req) < 0)
                fatal("ioctl(IFPPP_NEWIF): %m");
    
            ifunit = req.ifr_newif & 0xFFFF;
    }
    
    /*
     * Save the old line discipline of fd, and set it to PPP.
     */

    if (ioctl(fd, TIOCGETD, &initdisc) < 0)
	fatal("ioctl(TIOCGETD): %m");
    if (ioctl(fd, TIOCSETD, &pppdisc) < 0) 
	fatal("ioctl(TIOCSETD): %m");

    if (!demand) {
	/*
	 * Find out which interface we were given.
	 */
    /*        bzero(&req, sizeof(struct ifpppreq));
            sprintf(req.ifr_name, "ppp0", sizeof (req.ifr_name));
            req.ifr_code = IFPPP_NEWIF;
            if (ioctl(sockfd, SIOCSIFPPP, (caddr_t) &req) < 0)
                fatal("ioctl(IFPPP_NEWIF): %m");
    
            ifunit = req.ifr_newif & 0xFFFF;
*/
        lval = ifunit;
        ioctl(sockppp, IOC_PPP_ATTACHMUX, &lval);

    } else {
	/*
	 * Check that we got the same unit again.
	 */
	if (ioctl(fd, PPPIOCGUNIT, &x) < 0)
	    fatal("ioctl(PPPIOCGUNIT): %m");
	if (x != ifunit)
	    fatal("transfer_ppp failed: wanted unit %d, got %d", ifunit, x);

        x = TTYDISC;
        if (ioctl(loop_slave, TIOCSETD, &x) < 0)
            error("ioctl(TIOCSETD): %m");
    }

    ppp_fd = fd;

#if 0
    /*
     * Enable debug in the driver if requested.
     */
    if (kdebugflag) {
	if (ioctl(fd, PPPIOCGFLAGS, (caddr_t) &x) < 0) {
	    warn("ioctl (PPPIOCGFLAGS): %m");
	} else {
	    x |= (kdebugflag & 0xFF) * SC_DEBUG;
	    if (ioctl(fd, PPPIOCSFLAGS, (caddr_t) &x) < 0)
		warn("ioctl(PPPIOCSFLAGS): %m");
	}
    }
#endif

    /*
     * Set device for non-blocking reads.
     */
    if ((initfdflags = fcntl(fd, F_GETFL)) == -1
	|| fcntl(fd, F_SETFL, initfdflags | O_NONBLOCK) == -1) {
	warn("Couldn't set device to non-blocking mode: %m");
    }

   // return fd;
    return sockppp;
}

/*
 * restore_loop - reattach the ppp unit to the loopback.
 */
void
restore_loop()
{
    int x;

    /* Transfer the ppp interface back to the loopback. */
    if (ioctl(ppp_fd, PPPIOCXFERUNIT, 0) < 0)
	fatal("ioctl(transfer ppp unit): %m");

    /* Flush the tty output buffer so that the TIOCSETD doesn't hang.*/
    if (tcflush(loop_slave, TCIOFLUSH) < 0)
        warn("tcflush failed: %m");

    x = PPPDISC;
    if (ioctl(loop_slave, TIOCSETD, &x) < 0)
        fatal("ioctl(TIOCSETD): %m");

    /* Check that we got the same unit again. */
    if (ioctl(loop_slave, PPPIOCGUNIT, &x) < 0)
        fatal("ioctl(PPPIOCGUNIT): %m");
   if (x != ifunit)
	fatal("transfer_ppp failed: wanted unit %d, got %d", ifunit, x);
    ppp_fd = loop_slave;
}


/*
 * disestablish_ppp - Restore the serial port to normal operation.
 * This shouldn't call die() because it's called from die().
 */
void
disestablish_ppp(fd)
    int fd;
{
    struct ifpppreq    	req;
    u_long	lval;
    
    lval = ifunit;
    ioctl(sockppp, IOC_PPP_DETACHMUX, &lval);

    /* Reset non-blocking mode on fd. */
    if (initfdflags != -1 && fcntl(fd, F_SETFL, initfdflags) < 0)
	warn("Couldn't restore device fd flags: %m");
    initfdflags = -1;

    /* Restore old line discipline. */
    if (initdisc >= 0 && ioctl(fd, TIOCSETD, &initdisc) < 0)
        error("ioctl(TIOCSETD): %m");
    
    initdisc = -1;

//syslog(LOG_INFO, "disestablish_ppp, unit %d\n", ifunit, ifname);
    bzero(&req, sizeof(struct ifpppreq));
    sprintf(req.ifr_name, ifname, sizeof (req.ifr_name));
    req.ifr_code = IFPPP_DISPOSEIF;
    req.ifr_disposeif = ifunit;
    ioctl(sockfd, SIOCSIFPPP, (caddr_t) &req);

    if (fd == ppp_fd)
	ppp_fd = -1;
}

/*
 * Check whether the link seems not to be 8-bit clean.
 */
void
clean_check()
{
    int x;
    char *s;

    if (ioctl(ppp_fd, PPPIOCGFLAGS, (caddr_t) &x) == 0) {
	s = NULL;
	switch (~x & (LK_RCV_B7_0|LK_RCV_B7_1|LK_RCV_EVNP|LK_RCV_ODDP)) {
	case LK_RCV_B7_0:
	    s = "bit 7 set to 1";
	    break;
	case LK_RCV_B7_1:
	    s = "bit 7 set to 0";
	    break;
	case LK_RCV_EVNP:
	    s = "odd parity";
	    break;
	case LK_RCV_ODDP:
	    s = "even parity";
	    break;
	}
	if (s != NULL) {
	    warn("Serial link is not 8-bit clean:");
	    warn("All received characters had %s", s);
	}
    }
}

/*
 * set_up_tty: Set up the serial port on `fd' for 8 bits, no parity,
 * at the requested speed, etc.  If `local' is true, set CLOCAL
 * regardless of whether the modem option was specified.
 *
 * For *BSD, we assume that speed_t values numerically equal bits/second.
 */
void
set_up_tty(fd, local)
    int fd, local;
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

/*
 * restore_tty - restore the terminal to the saved settings.
 */
void
restore_tty(fd)
    int fd;
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

/*
 * setdtr - control the DTR line on the serial port.
 * This is called from die(), so it shouldn't call die().
 */
void
setdtr(fd, on)
int fd, on;
{
    int modembits = TIOCM_DTR;

    ioctl(fd, (on? TIOCMBIS: TIOCMBIC), &modembits);
}

/*
 * get_pty - get a pty master/slave pair and chown the slave side
 * to the uid given.  Assumes slave_name points to >= 12 bytes of space.
 */
int
get_pty(master_fdp, slave_fdp, slave_name, uid)
    int *master_fdp;
    int *slave_fdp;
    char *slave_name;
    int uid;
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


/*
 * open_ppp_loopback - open the device we use for getting
 * packets in demand mode, and connect it to a ppp interface.
 * Here we use a pty.
 */
int
open_ppp_loopback()
{
#if 0
    int flags, x;
    struct termios tios;
    int pppdisc = PPPDISC;

    if (openpty(&loop_master, &loop_slave, loop_name, NULL, NULL) < 0)
	fatal("No free pty for loopback");
    SYSDEBUG(("using %s for loopback", loop_name));

    if (tcgetattr(loop_slave, &tios) == 0) {
	tios.c_cflag &= ~(CSIZE | CSTOPB | PARENB);
	tios.c_cflag |= CS8 | CREAD;
	tios.c_iflag = IGNPAR;
	tios.c_oflag = 0;
	tios.c_lflag = 0;
	if (tcsetattr(loop_slave, TCSAFLUSH, &tios) < 0)
	    warn("couldn't set attributes on loopback: %m");
    }

    if ((flags = fcntl(loop_master, F_GETFL)) != -1) 
	if (fcntl(loop_master, F_SETFL, flags | O_NONBLOCK) == -1)
	    warn("couldn't set loopback to nonblock: %m");

    ppp_fd = loop_slave;

    if (ioctl(ppp_fd, TIOCSETD, &pppdisc) < 0)
        fatal("ioctl(TIOCSETD): %m");

    /*
     * Find out which interface we were given.
     */
    if (ioctl(ppp_fd, PPPIOCGUNIT, &ifunit) < 0)
	fatal("ioctl(PPPIOCGUNIT): %m");

    /*
     * Enable debug in the driver if requested.
     */
    if (kdebugflag) {
	if (ioctl(ppp_fd, PPPIOCGFLAGS, (caddr_t) &flags) < 0) {
	    warn("ioctl (PPPIOCGFLAGS): %m");
	} else {
	    flags |= (kdebugflag & 0xFF) * SC_DEBUG;
	    if (ioctl(ppp_fd, PPPIOCSFLAGS, (caddr_t) &flags) < 0)
		warn("ioctl(PPPIOCSFLAGS): %m");
	}
    }
    return loop_master;
#endif
return 0;
}


/*
 * output - Output PPP packet.
 */
void
output(unit, p, len)
    int unit;
    u_char *p;
    int len;
{
    if (debug) {
        // don't log lcp-echo/reply packets
        if ((*(u_short*)(p + 2) != PPP_LCP)
            || ((*(p + 3) == ECHOREQ) && (*(p + 3) == ECHOREP))) {
            dbglog("sent %P", p, len);
        }
    }
    
    bcopy(p, &ppp_buf[4], len);
    *(u_long *)(&ppp_buf[0]) = ifunit;
    if (write(sockppp, &ppp_buf[0], len + 4) < 0) {

        if (errno != EIO)
            error("write: %m");
    }
}


/*
 * wait_input - wait until there is data available,
 * for the length of time specified by *timo (indefinite
 * if timo is NULL).
 */
void
wait_input(timo)
    struct timeval *timo;
{
    fd_set ready;
    int n;

    ready = in_fds;
    n = select(max_in_fd + 1, &ready, NULL, &ready, timo);
    if (n < 0 && errno != EINTR)
	fatal("select: %m");
}


/*
 * add_fd - add an fd to the set that wait_input waits for.
 */
void add_fd(fd)
    int fd;
{
    FD_SET(fd, &in_fds);
    if (fd > max_in_fd)
	max_in_fd = fd;
}

/*
 * remove_fd - remove an fd from the set that wait_input waits for.
 */
void remove_fd(fd)
    int fd;
{
    FD_CLR(fd, &in_fds);
}

#if 0
/*
 * wait_loop_output - wait until there is data available on the
 * loopback, for the length of time specified by *timo (indefinite
 * if timo is NULL).
 */
void
wait_loop_output(timo)
    struct timeval *timo;
{
    fd_set ready;
    int n;

    FD_ZERO(&ready);
    FD_SET(loop_master, &ready);
    n = select(loop_master + 1, &ready, NULL, &ready, timo);
    if (n < 0 && errno != EINTR)
	fatal("select: %m");
}


/*
 * wait_time - wait for a given length of time or until a
 * signal is received.
 */
void
wait_time(timo)
    struct timeval *timo;
{
    int n;

    n = select(0, NULL, NULL, NULL, timo);
    if (n < 0 && errno != EINTR)
	fatal("select: %m");
}
#endif


/*
 * read_packet - get a PPP packet from the serial device.
 */
int
read_packet(buf)
    u_char *buf;
{
    int len;
        
    if ((len = read(sockppp, ppp_buf, PPP_MTU + PPP_HDRLEN + 4)) < 0) {
	if (errno == EWOULDBLOCK || errno == EINTR)
	    return -1;
	fatal("read: %m");
    }
    
    bcopy(ppp_buf + 4, buf, len - 4);
    return len - 4;
}


/*
 * get_loop_output - read characters from the loopback, form them
 * into frames, and detect when we want to bring the real link up.
 * Return value is 1 if we need to bring up the link, 0 otherwise.
 */
int
get_loop_output()
{
    int rv = 0;
    int n;

    while ((n = read(loop_master, inbuf, sizeof(inbuf))) >= 0) {
	if (loop_chars(inbuf, n))
	    rv = 1;
    }

    if (n == 0)
	fatal("eof on loopback");
    if (errno != EWOULDBLOCK)
	fatal("read from loopback: %m");

    return rv;
}


/*
 * ppp_send_config - configure the transmit characteristics of
 * the ppp interface.
 */
void
ppp_send_config(unit, mtu, asyncmap, pcomp, accomp)
    int unit, mtu;
    u_int32_t asyncmap;
    int pcomp, accomp;
{
    u_int x;
    struct ifreq ifr;
    struct ifpppreq 	ifpppr;

    strlcpy(ifr.ifr_name, ifname, sizeof (ifr.ifr_name));
    ifr.ifr_mtu = mtu;
    if (ioctl(sockfd, SIOCSIFMTU, (caddr_t) &ifr) < 0)
	fatal("ioctl(SIOCSIFMTU): %m");

    sprintf(ifpppr.ifr_name, ifname, sizeof (ifr.ifr_name));
    ifpppr.ifr_code = IFPPP_ASYNCMAP;
    ifpppr.ifr_map = asyncmap;
    if (ioctl(sockfd, SIOCSIFPPP, (caddr_t) &ifpppr) < 0)
        fatal("ioctl(IFPPP_ASYNCMAP): %m");

    ifpppr.ifr_code = IFPPP_EFLAGS;
    if (ioctl(sockfd, SIOCGIFPPP, (caddr_t) &ifpppr))
        fatal("ioctl (SIOCGIFPPP IFPPP_EFLAGS): %m");

    x = ifpppr.ifr_eflags;
    x = accomp ? x | IFF_PPP_DOES_ACCOMP: x & ~IFF_PPP_DOES_ACCOMP;
    x = pcomp ? x | IFF_PPP_DOES_PCOMP: x & ~IFF_PPP_DOES_PCOMP;
    ifpppr.ifr_eflags = x;
//    x = sync_serial ? x | SC_SYNC : x & ~SC_SYNC;
    if (ioctl(sockfd, SIOCSIFPPP, (caddr_t) &ifpppr) < 0)
        fatal("ioctl(SIOCSIFPPP IFPPP_EFLAGS): %m");

}


/*
 * ppp_set_xaccm - set the extended transmit ACCM for the interface.
 */
void
ppp_set_xaccm(unit, accm)
    int unit;
    ext_accm accm;
{
    struct ifpppreq ifpppr;

    sprintf(ifpppr.ifr_name, ifname, sizeof (ifpppr.ifr_name));
    ifpppr.ifr_code = IFPPP_XASYNCMAP;
    bcopy(&accm[0], &ifpppr.ifr_xmap[0], sizeof(ext_accm));

    if (ioctl(sockfd, SIOCSIFPPP, (caddr_t) &ifpppr) < 0)
	warn("ioctl(set extended ACCM): %m");
}


/*
 * ppp_recv_config - configure the receive-side characteristics of
 * the ppp interface.
 */
void
ppp_recv_config(unit, mru, asyncmap, pcomp, accomp)
    int unit, mru;
    u_int32_t asyncmap;
    int pcomp, accomp;
{
    int x;
    struct ifpppreq 	ifpppr;

    sprintf(ifpppr.ifr_name, ifname, sizeof (ifpppr.ifr_name));
    ifpppr.ifr_code = IFPPP_MRU;
    ifpppr.ifr_mru = mru;
    if (ioctl(sockfd, SIOCSIFPPP, (caddr_t) &ifpppr) < 0)
        fatal("ioctl(SIOCSIFPPP IFPPP_MRU): %m");

    ifpppr.ifr_code = IFPPP_RASYNCMAP;
    ifpppr.ifr_map = asyncmap;
    if (ioctl(sockfd, SIOCSIFPPP, (caddr_t) &ifpppr) < 0)
        fatal("ioctl(SIOCSIFPPP IFPPP_RASYNCMAP): %m");

    ifpppr.ifr_code = IFPPP_EFLAGS;
    if (ioctl(sockfd, SIOCGIFPPP, (caddr_t) &ifpppr))
        fatal("ioctl (SIOCGIFPPP IFPPP_EFLAGS): %m");

    x = ifpppr.ifr_eflags;
    x = accomp ? x | IFF_PPP_ACPT_ACCOMP: x & ~IFF_PPP_ACPT_ACCOMP;
    x = pcomp ? x | IFF_PPP_ACPT_PCOMP: x & ~IFF_PPP_ACPT_PCOMP;
    ifpppr.ifr_eflags = x;
    if (ioctl(sockfd, SIOCSIFPPP, (caddr_t) &ifpppr) < 0)
        fatal("ioctl(SIOCSIFPPP IFPPP_EFLAGS): %m");

}

/*
 * ccp_test - ask kernel whether a given compression method
 * is acceptable for use.  Returns 1 if the method and parameters
 * are OK, 0 if the method is known but the parameters are not OK
 * (e.g. code size should be reduced), or -1 if the method is unknown.
 */
int
ccp_test(unit, opt_ptr, opt_len, for_transmit)
    int unit, opt_len, for_transmit;
    u_char *opt_ptr;
{
#if 0
    struct ppp_option_data data;

    data.ptr = opt_ptr;
    data.length = opt_len;
    data.transmit = for_transmit;
    if (ioctl(ttyfd, PPPIOCSCOMPRESS, (caddr_t) &data) >= 0)
	return 1;
    return (errno == ENOBUFS)? 0: -1;
#endif 
return 0;
}

/*
 * ccp_flags_set - inform kernel about the current state of CCP.
 */
void
ccp_flags_set(unit, isopen, isup)
    int unit, isopen, isup;
{
    int x;

#if 0
    if (ioctl(ppp_fd, PPPIOCGFLAGS, (caddr_t) &x) < 0) {
	error("ioctl (PPPIOCGFLAGS): %m");
	return;
    }
    x = isopen? x | SC_CCP_OPEN: x &~ SC_CCP_OPEN;
    x = isup? x | SC_CCP_UP: x &~ SC_CCP_UP;
    if (ioctl(ppp_fd, PPPIOCSFLAGS, (caddr_t) &x) < 0)
	error("ioctl(PPPIOCSFLAGS): %m");
#endif
}

/*
 * ccp_fatal_error - returns 1 if decompression was disabled as a
 * result of an error detected after decompression of a packet,
 * 0 otherwise.  This is necessary because of patent nonsense.
 */
int
ccp_fatal_error(unit)
    int unit;
{
    int x;

#if 0
    if (ioctl(ppp_fd, PPPIOCGFLAGS, (caddr_t) &x) < 0) {
	error("ioctl(PPPIOCGFLAGS): %m");
	return 0;
    }
    return x & SC_DC_FERROR;
#endif 
return 0;
}

/*
 * get_idle_time - return how long the link has been idle.
 */
int
get_idle_time(u, ip)
    int u;
    struct ppp_idle *ip;
{
    return ioctl(ppp_fd, PPPIOCGIDLE, ip) >= 0;
}

/*
 * get_ppp_stats - return statistics for the link.
 */
int
get_ppp_stats(u, stats)
    int u;
    struct pppd_stats *stats;
{
    struct ifpppreq 		ifpppr;
    
    bzero (&ifpppr, sizeof (ifpppr));

    sprintf(ifpppr.ifr_name, ifname, sizeof (ifpppr.ifr_name));
    ifpppr.ifr_code = IFPPP_STATS;
    if (ioctl(sockfd, SIOCGIFPPP, (caddr_t) &ifpppr) < 0) {
	error("Couldn't get PPP statistics: %m");
	return 0;
    }

    stats->bytes_in = ifpppr.ifr_stats.ibytes;
    stats->bytes_out = ifpppr.ifr_stats.obytes;
    return 1;
}


#ifdef PPP_FILTER
/*
 * set_filters - transfer the pass and active filters to the kernel.
 */
int
set_filters(pass, active)
    struct bpf_program *pass, *active;
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

/*
 * sifvjcomp - config tcp header compression
 */
int
sifvjcomp(u, vjcomp, cidcomp, maxcid)
    int u, vjcomp, cidcomp, maxcid;
{
    u_int x;
    struct ifpppreq 	ifpppr;
    int err;
    
    bzero(&ifpppr, sizeof(struct ifpppreq));
    sprintf(ifpppr.ifr_name, "%s", ifname);
    ifpppr.ifr_code = IFPPP_IP_VJ;
    ifpppr.ifr_ip_vj.vj = vjcomp ? 1 : 0;
    ifpppr.ifr_ip_vj.cid = cidcomp ? 1 : 0;
    ifpppr.ifr_ip_vj.max_cid = maxcid;

    err = ioctl(sockfd, SIOCSIFPPP, (caddr_t) &ifpppr);
    if (err < 0) {
        printf("ioctl(SIOCSIFPPP) VJComp :  %m");
	return 0;
    }

#if 0
    if (ioctl(ppp_fd, PPPIOCGFLAGS, (caddr_t) &x) < 0) {
	error("ioctl (PPPIOCGFLAGS): %m");
	return 0;
    }
    x = vjcomp ? x | SC_COMP_TCP: x &~ SC_COMP_TCP;
    x = cidcomp? x & ~SC_NO_TCP_CCID: x | SC_NO_TCP_CCID;
    if (ioctl(ppp_fd, PPPIOCSFLAGS, (caddr_t) &x) < 0) {
	error("ioctl(PPPIOCSFLAGS): %m");
	return 0;
    }
    if (vjcomp && ioctl(ppp_fd, PPPIOCSMAXCID, (caddr_t) &maxcid) < 0) {
	error("ioctl(PPPIOCSFLAGS): %m");
	return 0;
    }
#endif
    return 1;
}

/*
 * sifup - Config the interface up and enable IP packets to pass.
 */
int
sifup(u)
    int u;
{
    struct ifreq ifr;

    strlcpy(ifr.ifr_name, ifname, sizeof (ifr.ifr_name));
    if (ioctl(sockfd, SIOCGIFFLAGS, (caddr_t) &ifr) < 0) {
	error("ioctl (SIOCGIFFLAGS): %m");
	return 0;
    }
    ifr.ifr_flags |= IFF_UP;
    if (ioctl(sockfd, SIOCSIFFLAGS, (caddr_t) &ifr) < 0) {
	error("ioctl(SIOCSIFFLAGS): %m");
	return 0;
    }
    if_is_up = 1;
    return 1;
}

/*
 * sifnpmode - Set the mode for handling packets for a given NP.
 */
int
sifnpmode(u, proto, mode)
    int u;
    int proto;
    enum NPmode mode;
{
    struct ifpppreq 	ifpppr;
    
    if (proto != PPP_IP)
        return 0;

    if (mode == NPMODE_DROP)
        return 0;

    sprintf(ifpppr.ifr_name, "%s", ifname);
    ifpppr.ifr_code = (mode == NPMODE_PASS ? IFPPP_ATTACH_IP : IFPPP_DETACH_IP);
    if (ioctl(sockfd, SIOCSIFPPP, (caddr_t) &ifpppr) < 0) {
	error("ioctl(set NP %d mode to %d): %m", proto, mode);
	return 0;
    }
    
    return 1;
}

/*
 * sifdown - Config the interface down and disable IP.
 */
int
sifdown(u)
    int u;
{
    struct ifpppreq 	ifpppr;
    struct ifreq ifr;
    int rv;

    rv = 1;
    
    sprintf(ifpppr.ifr_name, "%s", ifname);
    ifpppr.ifr_code = IFPPP_DETACH_IP;
    ioctl(sockfd, SIOCSIFPPP, (caddr_t) &ifpppr);
    /* ignore errors, because ppp_fd might have been closed by now. */

    strlcpy(ifr.ifr_name, ifname, sizeof (ifr.ifr_name));
    if (ioctl(sockfd, SIOCGIFFLAGS, (caddr_t) &ifr) < 0) {
	error("ioctl (SIOCGIFFLAGS): %m");
	rv = 0;
    } else {
	ifr.ifr_flags &= ~IFF_UP;
	if (ioctl(sockfd, SIOCSIFFLAGS, (caddr_t) &ifr) < 0) {
	    error("ioctl(SIOCSIFFLAGS): %m");
	    rv = 0;
	} else
	    if_is_up = 0;
    }
    if_is_up = 0;
    return rv;
}

/*
 * SET_SA_FAMILY - set the sa_family field of a struct sockaddr,
 * if it exists.
 */
#define SET_SA_FAMILY(addr, family)		\
    BZERO((char *) &(addr), sizeof(addr));	\
    addr.sa_family = (family); 			\
    addr.sa_len = sizeof(addr);

/*
 * sifaddr - Config the interface IP addresses and netmask.
 */
int
sifaddr(u, o, h, m)
    int u;
    u_int32_t o, h, m;
{
    struct ifaliasreq ifra;
    struct ifreq ifr;

    // we need to attach IP before....
    sifnpmode(0, PPP_IP, NPMODE_PASS);


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
    if (ioctl(sockfd, SIOCDIFADDR, (caddr_t) &ifr) < 0) {
	if (errno != EADDRNOTAVAIL)
	    warn("Couldn't remove interface address: %m");
    }
    if (ioctl(sockfd, SIOCAIFADDR, (caddr_t) &ifra) < 0) {
	if (errno != EEXIST) {
	    error("Couldn't set interface address: %m");
	    return 0;
	}
	warn("Couldn't set interface address: Address %I already exists", o);
    }
    ifaddrs[0] = o;
    ifaddrs[1] = h;
    return 1;
}

/*
 * cifaddr - Clear the interface IP addresses, and delete routes
 * through the interface if possible.
 */
int
cifaddr(u, o, h)
    int u;
    u_int32_t o, h;
{
    struct ifaliasreq ifra;

    ifaddrs[0] = 0;
    strlcpy(ifra.ifra_name, ifname, sizeof(ifra.ifra_name));
    SET_SA_FAMILY(ifra.ifra_addr, AF_INET);
    ((struct sockaddr_in *) &ifra.ifra_addr)->sin_addr.s_addr = o;
    SET_SA_FAMILY(ifra.ifra_broadaddr, AF_INET);
    ((struct sockaddr_in *) &ifra.ifra_broadaddr)->sin_addr.s_addr = h;
    BZERO(&ifra.ifra_mask, sizeof(ifra.ifra_mask));
    if (ioctl(sockfd, SIOCDIFADDR, (caddr_t) &ifra) < 0) {
	if (errno != EADDRNOTAVAIL)
	    warn("Couldn't delete interface address: %m");
	return 0;
    }
    return 1;
}

#if 0
static int remove_default_route();
static int add_default_route(u_int32_t loc, u_int32_t dst);
static int remove_dns();
static int add_dns(u_int32_t dns1, u_int32_t dns2, char * dname);
#endif 0

static int remove_route_and_dns();
static int add_route_and_dns(u_int32_t loc, u_int32_t dst,
                             u_int32_t dns1, u_int32_t dns2, char * dname);

/*
 * sifdefaultroute - assign a default route through the address given.
 */
int
sifdefaultroute(u, l, g)
    int u;
    u_int32_t l, g;
{
#if 0
    add_default_route(l, g);
    add_dns(ipcp_dns1, ipcp_dns2, ipcp_dname);
#endif 0
#if 0
    return (add_route_and_dns(l, g, ipcp_dns1, ipcp_dns2, ipcp_dname));
#endif
return 0;
//    return dodefaultroute(g, 's');
}

/*
 * cifdefaultroute - delete a default route through the address given.
 */
int
cifdefaultroute(u, l, g)
    int u;
    u_int32_t l, g;
{
#if 0
    remove_dns();
    remove_default_route();
#endif 0
    return (remove_route_and_dns());

//    return dodefaultroute(g, 'c');
}

/*
 * add the default route and DNS, using configd cache mechanism.
 */
static int
add_route_and_dns(u_int32_t loc, u_int32_t dst,
                  u_int32_t dns1, u_int32_t dns2, char * dname)
{
#if 0
    char			pkey[FOUR_CHAR_ID_STRLEN];
    struct in_addr		addr;
    CFMutableArrayRef		array = 0;
    CFMutableDictionaryRef	ipv4_dict = 0;
    SCDHandleRef 		ipv4_data = 0;
    CFStringRef			ipv4_key = 0;
//    SCDSessionRef		session = 0;
    SCDStatus			status, ipv4_status;
    CFStringRef			str;

    struct in_addr		dnsaddr[2];
    CFMutableDictionaryRef	dns_dict = 0;
    SCDHandleRef 		dns_data = 0;
    CFStringRef			dns_key = 0;
    SCDStatus 			dns_status = SCD_OK;
    int				i;

    if (dns1 == 0 && dns2 == 0) {
        return 0;
    }

    dnsaddr[0].s_addr = dns1;
    dnsaddr[1].s_addr = dns2;

    if (!configd_session_inited)
        return 0;

    /*    status = SCDOpen(&configd_session, CFSTR("pppd"));
    if (status != SCD_OK) {
        warn("SCDOpen failed: %s\n", SCDError(status));
        return 0;
    }
*/
    /* create the IP cache key and dict */
    FourCharIDToString(kNetCfgClassIPv4, pkey);
    ipv4_key = CFStringCreateWithFormat(0, 0,
                                        CFSTR(kSCDCachePrefixState
                                              kSCDCacheFormatNetworkInterfaceProtocol),
                                        ifname, pkey);
    ipv4_dict = CFDictionaryCreateMutable(0, 0,
                                          &kCFTypeDictionaryKeyCallBacks,
                                          &kCFTypeDictionaryValueCallBacks);
    /* create the DNS cache key and dict */
    FourCharIDToString(kNetCfgClassDNS, pkey);
    dns_key = CFStringCreateWithFormat(NULL, NULL,
                                       CFSTR(kSCDCachePrefixState
                                             kSCDCacheFormatNetworkInterfaceProtocol),
                                       ifname, pkey);
    dns_dict = CFDictionaryCreateMutable(0, 0,
                                         &kCFTypeDictionaryKeyCallBacks,
                                         &kCFTypeDictionaryValueCallBacks);

    /* set the ip address array */
    addr.s_addr = loc;
    array = CFArrayCreateMutable(0, 1, &kCFTypeArrayCallBacks);
    str = CFStringCreateWithFormat(0, 0, CFSTR(IP_FORMAT),
                                   IP_LIST(&addr));
    CFArrayAppendValue(array, str);
    CFRelease(str);
    CFDictionarySetValue(ipv4_dict, CFSTR(kNetCfgKeyIPv4Addresses), array);
    CFRelease(array);

    /* set the ip dest array */
    addr.s_addr = dst;
    array = CFArrayCreateMutable(NULL, 1, &kCFTypeArrayCallBacks);
    str = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT),
                                   IP_LIST(&addr));
    CFArrayAppendValue(array, str);
    CFRelease(str);
    CFDictionarySetValue(ipv4_dict, CFSTR(kNetCfgKeyIPv4DestAddresses), array);
    CFRelease(array);

    /* set the router */
    addr.s_addr = dst;
    str = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT),
                                   IP_LIST(&addr));
    CFDictionarySetValue(ipv4_dict, CFSTR(kNetCfgKeyIPv4Router), str);
    CFRelease(str);

    /* set the DNS servers */
    array = CFArrayCreateMutable(NULL, 2,
                                 &kCFTypeArrayCallBacks);
    for (i = 0; i < 2; i++) {
        if (dnsaddr[i].s_addr) {
            str = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT),
                                           IP_LIST(&dnsaddr[i]));
            CFArrayAppendValue(array, str);
            CFRelease(str);
        }
    }
    CFDictionarySetValue(dns_dict, CFSTR(kNetCfgKeyDNSServerAddresses),
                         array);
    CFRelease(array);

    /* set the DNS domain name */
    if (dname) {
        str = CFStringCreateWithBytes(NULL, dname, strlen(dname),
                                      CFStringGetSystemEncoding(), FALSE);
        CFDictionarySetValue(dns_dict, CFSTR(kNetCfgKeyDNSDomainName), str);
        CFRelease(str);
    }

    /* get system configuration cache handles */
    ipv4_data = SCDHandleInit();
    SCDHandleSetData(ipv4_data, ipv4_dict);
    CFRelease(ipv4_dict);

    dns_data = SCDHandleInit();
    SCDHandleSetData(dns_data, dns_dict);
    CFRelease(dns_dict);

    /* update atomically to avoid needless notifications */
    SCDLock(configd_session);
    SCDRemove(configd_session, ipv4_key);
    ipv4_status = SCDAdd(configd_session, ipv4_key, ipv4_data);
    (void)SCDRemove(configd_session, dns_key);
    dns_status = SCDAdd(configd_session, dns_key, dns_data);
    SCDUnlock(configd_session);
    if (ipv4_status != SCD_OK) {
        warn("SCDAdd IP %s failed: %s\n", ifname, SCDError(ipv4_status));
    }
    if (dns_status != SCD_OK) {
        warn("SCDAdd DNS %s returned: %s\n", ifname, SCDError(dns_status));
    }
    SCDHandleRelease(ipv4_data);
    CFRelease(ipv4_key);
    SCDHandleRelease(dns_data);
    CFRelease(dns_key);

    //SCDClose(&configd_session);
#endif
    return 1;
}

/*
 * remove the default route and DNS, using configd cache mechanism.
 */
static int
remove_route_and_dns()
{
#if 0
    CFStringRef			dns_key = 0;
    char			pkey[FOUR_CHAR_ID_STRLEN];
//    SCDSessionRef		session = NULL;
    SCDStatus			status;
    CFStringRef			ipv4_key = 0;

    if (!configd_session_inited)
        return 0;
/*    status = SCDOpen(&configd_session, CFSTR("pppd"));
    if (status != SCD_OK) {
        warn("SCDOpen failed: %s\n", SCDError(status));
        return 0;
    }
*/
    /* create IP cache key */
    FourCharIDToString(kNetCfgClassIPv4, pkey);
    ipv4_key = CFStringCreateWithFormat(0, 0,
                                        CFSTR(kSCDCachePrefixState
                                              kSCDCacheFormatNetworkInterfaceProtocol),
                                        ifname, pkey);
    /* create DNS cache key */
    FourCharIDToString(kNetCfgClassDNS, pkey);
    dns_key = CFStringCreateWithFormat(NULL, NULL,
                                       CFSTR(kSCDCachePrefixState
                                             kSCDCacheFormatNetworkInterfaceProtocol),
                                       ifname, pkey);
    /* update atomically to avoid needless notifications */
    SCDLock(configd_session);
    (void)SCDRemove(configd_session, ipv4_key);
    (void)SCDRemove(configd_session, dns_key);
    SCDUnlock(configd_session);

    CFRelease(dns_key);
    CFRelease(ipv4_key);

    //SCDClose(&configd_session);
#endif
    return 1;
}


#if 0
int
remove_dns()
{
    CFStringRef			dns_key = 0;
    SCDStatus 			dns_status = SCD_OK;
    char			pkey[FOUR_CHAR_ID_STRLEN];
    SCDSessionRef		session = NULL;
    SCDStatus			status;

    status = SCDOpen(&session, CFSTR("pppd"));
    if (status != SCD_OK) {
        warn("SCDOpen failed: %s\n", SCDError(status));
        return 0;
    }

    FourCharIDToString(kNetCfgClassDNS, pkey);
    dns_key = CFStringCreateWithFormat(NULL, NULL,
                                       CFSTR(kSCDCachePrefixState
                                             kSCDCacheFormatNetworkInterfaceProtocol),
                                       ifname, pkey);
    dns_status = SCDRemove(session, dns_key);
    if (dns_status != SCD_OK) {
        warn("SCDRemove DNS %s returned: %s\n", ifname, SCDError(dns_status));
    }
    CFRelease(dns_key);

    SCDClose(&session);
    return 1;
}
int
add_dns(u_int32_t dns1, u_int32_t dns2, char * dname)
{
    struct in_addr		addr[2];
    CFMutableArrayRef		array = 0;
    CFMutableDictionaryRef	dns_dict = 0;
    SCDHandleRef 		dns_data = 0;
    CFStringRef			dns_key = 0;
    SCDStatus 			dns_status = SCD_OK;
    int				i;
    char			pkey[FOUR_CHAR_ID_STRLEN];
    SCDSessionRef		session = NULL;
    SCDStatus			status;
    CFStringRef			str;

    if (dns1 == 0 && dns2 == 0) {
        return 0;
    }
    addr[0].s_addr = dns1;
    addr[1].s_addr = dns2;

    status = SCDOpen(&session, CFSTR("pppd"));
    if (status != SCD_OK) {
        warn("SCDOpen failed: %s\n", SCDError(status));
        return 0;
    }

    FourCharIDToString(kNetCfgClassDNS, pkey);
    dns_key = CFStringCreateWithFormat(NULL, NULL,
                                       CFSTR(kSCDCachePrefixState
                                             kSCDCacheFormatNetworkInterfaceProtocol),
                                       ifname, pkey);
    dns_dict = CFDictionaryCreateMutable(0, 0,
                                         &kCFTypeDictionaryKeyCallBacks,
                                         &kCFTypeDictionaryValueCallBacks);

    array = CFArrayCreateMutable(NULL, 2,
                                 &kCFTypeArrayCallBacks);
    for (i = 0; i < 2; i++) {
        if (addr[i].s_addr) {
            str = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT),
                                           IP_LIST(&addr[i]));
            CFArrayAppendValue(array, str);
            CFRelease(str);
        }
    }
    CFDictionarySetValue(dns_dict, CFSTR(kNetCfgKeyDNSServerAddresses),
                         array);
    CFRelease(array);

    if (dname) {
        str = CFStringCreateWithBytes(NULL, dname, strlen(dname),
                                      CFStringGetSystemEncoding(), FALSE);
        CFDictionarySetValue(dns_dict, CFSTR(kNetCfgKeyDNSDomainName), str);
        CFRelease(str);
    }
    dns_data = SCDHandleInit();
    SCDHandleSetData(dns_data, dns_dict);
    CFRelease(dns_dict);

    SCDLock(session);
    (void)SCDRemove(session, dns_key);
    dns_status = SCDAdd(session, dns_key, dns_data);
    SCDUnlock(session);
    if (dns_status != SCD_OK) {
        warn("SCDAdd DNS %s returned: %s\n", ifname, SCDError(dns_status));
    }
    SCDHandleRelease(dns_data);
    CFRelease(dns_key);

    SCDClose(&session);
    return 1;
}

/*
 * remove the default route, using configd cache mechanism.
 */
static int
remove_default_route()
{
    char			pkey[FOUR_CHAR_ID_STRLEN];
    CFStringRef			ipv4_key = 0;
    SCDSessionRef		session = 0;
    SCDStatus			status;

    status = SCDOpen(&session, CFSTR("pppd"));
    if (status != SCD_OK) {
        warn("SCDOpen failed: %s\n", SCDError(status));
        return 0;
    }
    /* create the cache key */
    FourCharIDToString(kNetCfgClassIPv4, pkey);
    ipv4_key = CFStringCreateWithFormat(0, 0,
                                        CFSTR(kSCDCachePrefixState
                                              kSCDCacheFormatNetworkInterfaceProtocol),
                                        ifname, pkey);
    status = SCDRemove(session, ipv4_key);
    if (status != SCD_OK) {
        warn("SCDRemove %s failed: %s\n", ifname, SCDError(status));
    }
    
    CFRelease(ipv4_key);
    SCDClose(&session);
    return 1;
}

/*
 * add the default route, using configd cache mechanism.
 */
static int
add_default_route(u_int32_t loc, u_int32_t dst)
{
    char			pkey[FOUR_CHAR_ID_STRLEN];
    struct in_addr		addr;
    CFMutableArrayRef		array = 0;
    CFMutableDictionaryRef	ipv4_dict = 0;
    SCDHandleRef 		ipv4_data = 0;
    CFStringRef			ipv4_key = 0;
    SCDSessionRef		session = 0;
    SCDStatus			status;
    CFStringRef			str;

    status = SCDOpen(&session, CFSTR("pppd"));
    if (status != SCD_OK) {
        warn("SCDOpen failed: %s\n", SCDError(status));
        return 0;
    }

    /* create the cache key */
    FourCharIDToString(kNetCfgClassIPv4, pkey);
    ipv4_key = CFStringCreateWithFormat(0, 0,
                                        CFSTR(kSCDCachePrefixState
                                              kSCDCacheFormatNetworkInterfaceProtocol),
                                        ifname, pkey);
    ipv4_dict = CFDictionaryCreateMutable(0, 0,
                                          &kCFTypeDictionaryKeyCallBacks,
                                          &kCFTypeDictionaryValueCallBacks);
    /* set the ip address array */
    addr.s_addr = loc;
    array = CFArrayCreateMutable(0, 1, &kCFTypeArrayCallBacks);
    str = CFStringCreateWithFormat(0, 0, CFSTR(IP_FORMAT),
                                   IP_LIST(&addr));
    CFArrayAppendValue(array, str);
    CFRelease(str);
    CFDictionarySetValue(ipv4_dict, CFSTR(kNetCfgKeyIPv4Addresses), array);
    CFRelease(array);

    /* set the ip dest array */
    addr.s_addr = dst;
    array = CFArrayCreateMutable(NULL, 1, &kCFTypeArrayCallBacks);
    str = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT),
                                   IP_LIST(&addr));
    CFArrayAppendValue(array, str);
    CFRelease(str);
    CFDictionarySetValue(ipv4_dict, CFSTR(kNetCfgKeyIPv4DestAddresses), array);
    CFRelease(array);

    /* set the router */
    addr.s_addr = dst;
    str = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT),
                                   IP_LIST(&addr));
    CFDictionarySetValue(ipv4_dict, CFSTR(kNetCfgKeyIPv4Router), str);
    CFRelease(str);

    /* update the system configuration cache */
    ipv4_data = SCDHandleInit();
    SCDHandleSetData(ipv4_data, ipv4_dict);
    CFRelease(ipv4_dict);

    /* update atomically to avoid needless notifications */
    SCDLock(session);
    SCDRemove(session, ipv4_key);
    status = SCDAdd(session, ipv4_key, ipv4_data);
    SCDUnlock(session);

    if (status != SCD_OK) {
        SCDLog(LOG_INFO,
               CFSTR("SCDAdd IPV4 %s returned: %s"),
               ifname, SCDError(status));
        warn("SCDAdd failed: %s\n", SCDError(status));
        return 0;
    }
    SCDHandleRelease(ipv4_data);
    CFRelease(ipv4_key);
    SCDClose(&session);
    return 1;
}
#endif
/*
 * dodefaultroute - talk to a routing socket to add/delete a default route.
 */
static int
dodefaultroute(g, cmd)
    u_int32_t g;
    int cmd;
{
    int routes;
    struct {
	struct rt_msghdr	hdr;
	struct sockaddr_in	dst;
	struct sockaddr_in	gway;
	struct sockaddr_in	mask;
    } rtmsg;

    if ((routes = socket(PF_ROUTE, SOCK_RAW, AF_INET)) < 0) {
	error("Couldn't %s default route: socket: %m",
	       cmd=='s'? "add": "delete");
	return 0;
    }

    memset(&rtmsg, 0, sizeof(rtmsg));
    rtmsg.hdr.rtm_type = cmd == 's'? RTM_ADD: RTM_DELETE;
    rtmsg.hdr.rtm_flags = RTF_UP | RTF_GATEWAY | RTF_STATIC;
    rtmsg.hdr.rtm_version = RTM_VERSION;
    rtmsg.hdr.rtm_seq = ++rtm_seq;
    rtmsg.hdr.rtm_addrs = RTA_DST | RTA_GATEWAY | RTA_NETMASK;
    rtmsg.dst.sin_len = sizeof(rtmsg.dst);
    rtmsg.dst.sin_family = AF_INET;
    rtmsg.gway.sin_len = sizeof(rtmsg.gway);
    rtmsg.gway.sin_family = AF_INET;
    rtmsg.gway.sin_addr.s_addr = g;
    rtmsg.mask.sin_len = sizeof(rtmsg.dst);
    rtmsg.mask.sin_family = AF_INET;

    rtmsg.hdr.rtm_msglen = sizeof(rtmsg);
    if (write(routes, &rtmsg, sizeof(rtmsg)) < 0) {
	error("Couldn't %s default route: %m",
	       cmd=='s'? "add": "delete");
	close(routes);
	return 0;
    }

    close(routes);
    default_route_gateway = (cmd == 's')? g: 0;
    return 1;
}

#if RTM_VERSION >= 3

/*
 * sifproxyarp - Make a proxy ARP entry for the peer.
 */
static struct {
    struct rt_msghdr		hdr;
    struct sockaddr_inarp	dst;
    struct sockaddr_dl		hwa;
    char			extra[128];
} arpmsg;

static int arpmsg_valid;

int
sifproxyarp(unit, hisaddr)
    int unit;
    u_int32_t hisaddr;
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

/*
 * cifproxyarp - Delete the proxy ARP entry for the peer.
 */
int
cifproxyarp(unit, hisaddr)
    int unit;
    u_int32_t hisaddr;
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

#else	/* RTM_VERSION */

/*
 * sifproxyarp - Make a proxy ARP entry for the peer.
 */
int
sifproxyarp(unit, hisaddr)
    int unit;
    u_int32_t hisaddr;
{
    struct arpreq arpreq;
    struct {
	struct sockaddr_dl	sdl;
	char			space[128];
    } dls;

    BZERO(&arpreq, sizeof(arpreq));

    /*
     * Get the hardware address of an interface on the same subnet
     * as our local address.
     */
    if (!get_ether_addr(hisaddr, &dls.sdl)) {
	error("Cannot determine ethernet address for proxy ARP");
	return 0;
    }

    arpreq.arp_ha.sa_len = sizeof(struct sockaddr);
    arpreq.arp_ha.sa_family = AF_UNSPEC;
    BCOPY(LLADDR(&dls.sdl), arpreq.arp_ha.sa_data, dls.sdl.sdl_alen);
    SET_SA_FAMILY(arpreq.arp_pa, AF_INET);
    ((struct sockaddr_in *) &arpreq.arp_pa)->sin_addr.s_addr = hisaddr;
    arpreq.arp_flags = ATF_PERM | ATF_PUBL;
    if (ioctl(sockfd, SIOCSARP, (caddr_t)&arpreq) < 0) {
	error("Couldn't add proxy arp entry: %m");
	return 0;
    }

    proxy_arp_addr = hisaddr;
    return 1;
}

/*
 * cifproxyarp - Delete the proxy ARP entry for the peer.
 */
int
cifproxyarp(unit, hisaddr)
    int unit;
    u_int32_t hisaddr;
{
    struct arpreq arpreq;

    BZERO(&arpreq, sizeof(arpreq));
    SET_SA_FAMILY(arpreq.arp_pa, AF_INET);
    ((struct sockaddr_in *) &arpreq.arp_pa)->sin_addr.s_addr = hisaddr;
    if (ioctl(sockfd, SIOCDARP, (caddr_t)&arpreq) < 0) {
	warn("Couldn't delete proxy arp entry: %m");
	return 0;
    }
    proxy_arp_addr = 0;
    return 1;
}
#endif	/* RTM_VERSION */


/*
 * get_ether_addr - get the hardware address of an interface on the
 * the same subnet as ipaddr.
 */
#define MAX_IFS		32

static int
get_ether_addr(ipaddr, hwaddr)
    u_int32_t ipaddr;
    struct sockaddr_dl *hwaddr;
{
    struct ifreq *ifr, *ifend, *ifp;
    u_int32_t ina, mask;
    struct sockaddr_dl *dla;
    struct ifreq ifreq;
    struct ifconf ifc;
    struct ifreq ifs[MAX_IFS];

    ifc.ifc_len = sizeof(ifs);
    ifc.ifc_req = ifs;
    if (ioctl(sockfd, SIOCGIFCONF, &ifc) < 0) {
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
	    if (ioctl(sockfd, SIOCGIFFLAGS, &ifreq) < 0)
		continue;
	    if ((ifreq.ifr_flags &
		 (IFF_UP|IFF_BROADCAST|IFF_POINTOPOINT|IFF_LOOPBACK|IFF_NOARP))
		 != (IFF_UP|IFF_BROADCAST))
		continue;
	    /*
	     * Get its netmask and check that it's on the right subnet.
	     */
	    if (ioctl(sockfd, SIOCGIFNETMASK, &ifreq) < 0)
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

/*
 * Return user specified netmask, modified by any mask we might determine
 * for address `addr' (in network byte order).
 * Here we scan through the system's list of interfaces, looking for
 * any non-point-to-point interfaces which might appear to be on the same
 * network as `addr'.  If we find any, we OR in their netmask to the
 * user-specified netmask.
 */
u_int32_t
GetMask(addr)
    u_int32_t addr;
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
    if (ioctl(sockfd, SIOCGIFCONF, &ifc) < 0) {
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
	if (ioctl(sockfd, SIOCGIFFLAGS, &ifreq) < 0)
	    continue;
	if ((ifreq.ifr_flags & (IFF_UP|IFF_POINTOPOINT|IFF_LOOPBACK))
	    != IFF_UP)
	    continue;
	/*
	 * Get its netmask and OR it into our mask.
	 */
	if (ioctl(sockfd, SIOCGIFNETMASK, &ifreq) < 0)
	    continue;
	mask |= ((struct sockaddr_in *)&ifreq.ifr_addr)->sin_addr.s_addr;
    }

    return mask;
}

/*
 * have_route_to - determine if the system has any route to
 * a given IP address.
 * For demand mode to work properly, we have to ignore routes
 * through our own interface.
 */
int have_route_to(u_int32_t addr)
{
    return -1;
}

/*
 * Use the hostid as part of the random number seed.
 */
int
get_host_seed()
{
    return gethostid();
}


#if 0
/*
 * lock - create a lock file for the named lock device
 */
#define	LOCK_PREFIX	"/var/spool/lock/LCK.."

static char *lock_file;		/* name of lock file created */

int
lock(dev)
    char *dev;
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

/*
 * unlock - remove our lockfile
 */
void
unlock()
{
    if (lock_file) {
	unlink(lock_file);
	free(lock_file);
	lock_file = NULL;
    }
}
#endif


