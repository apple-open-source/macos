/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */



#ifndef __PPP_MANAGER__
#define __PPP_MANAGER__

//#define PRINTF(x) 	printf x
#define PRINTF(x)


u_long ppp_init_all();
struct ppp *ppp_new(u_char *name, u_short unit, u_short subfamily, u_long serviceID);
int ppp_appears(u_char *name, u_short unit, u_short subfamily);
int ppp_dispose(struct ppp *ppp);
int ppp_reinit(struct ppp *ppp, u_char rearm);
struct ppp *ppp_findbyname(u_char *name, u_short unit);
struct ppp *ppp_findbyserviceID(u_long serviceID);
struct ppp *ppp_findbyref(u_long ref);
void ppp_setorder(struct ppp *ppp, u_int16_t order);
u_short ppp_findfreeunit(u_short subfam);
u_int32_t ppp_makeref(struct ppp *ppp);
u_int32_t ppp_makeifref(struct ppp *ppp);
u_long ppp_apply_options(struct ppp *ppp, struct options *opts, u_char server_mode);
int ppp_ifup(struct ppp *ppp);
int ppp_ifdown(struct ppp *ppp);
int ppp_logout();

u_int32_t ppp_getaddr(struct ppp *ppp, u_int32_t code, u_int32_t *data);

u_long ppp_cclspeed(u_short id, struct msg *msg);
u_long ppp_cclnote(u_short id, struct msg *msg);
u_long ppp_cclresult(u_short id, struct msg *msg);
u_long ppp_cclwritetext(u_short id, struct msg *msg);
u_long ppp_cclmatchtext(u_short id, struct msg *msg);
u_long ppp_readfd_data(u_long link, struct msg *msg);

#define AUTOCONNECT_IP_LOCAL_ADDRESS	0x0a100000
#define AUTOCONNECT_IP_DEST_ADDRESS	0x0a200000
#define AUTOCONNECT_IP_MASK_ADDRESS	0xFF000000

#define MAXWORDLEN	1024	/* max length of word in file (incl null) */
#define MAXARGS		1	/* max # args to a command */
#define MAXNAMELEN	256	/* max length of hostname or name for auth */
#define MAXSECRETLEN	256	/* max length of password or secret */
#define PATH_SECRETS 	"/etc/ppp/secrets"

/*
 * The following struct gives the addresses of procedures to call
 * for a particular protocol.
 */
struct protent {
    u_short protocol;				/* PPP protocol number */
    void (*init) __P((struct ppp *));		/* Initialization procedure */
    void (*input) __P((struct ppp *, u_char *pkt, int len));/* Process a received packet */
    void (*protrej) __P((struct ppp *));	/* Process a received protocol-reject */
    void (*lowerup) __P((struct ppp *));	/* Lower layer has come up */
    void (*lowerdown) __P((struct ppp *)); 	/* Lower layer has gone down */
    void (*open) __P((struct ppp *));		/* Open the protocol */
    void (*close) __P((struct ppp *, char *reason));/* Close the protocol */
    int  (*printpkt) __P((struct ppp *, u_char *pkt, int len,
                          void (*printer) __P((struct ppp *, void *, char *, ...)),
                          void *arg));		/* Print a packet in readable form */
    void (*datainput) __P((struct ppp *, u_char *pkt, int len));/* Process a received data packet */
    char *name;					/* Text name of protocol */
    char *data_name;				/* Text name of corresponding data protocol */
    void /*option_t*/ *options;			/* List of command-line options */
    void (*check_options) __P((void));		/* Check requested options, assign defaults */
    int  (*demand_conf) __P((struct ppp *));    /* Configure interface for demand-dial */
    int  (*active_pkt) __P((u_char *pkt, int len)); /* Say whether to bring up link for this pkt */

};

/* Structure representing a list of permitted IP addresses. */
struct permitted_ip {
    int		permit;		/* 1 = permit, 0 = forbid */
    u_int32_t	base;		/* match if (addr & mask) == base */
    u_int32_t	mask;		/* base and mask are in network byte order */
};

/* link state */
enum {
    link_disconnected = 0,
    link_connected,
    link_connecting,
    link_disconnecting,
    link_listening,
    link_ringing,
    link_accepting
};

/* Used for storing a sequence of words.  Usually malloced. */
struct wordlist {
    struct wordlist	*next;
    char		*word;
};

/* this struct contains all the information to control a ppp interface */
struct ppp {

    TAILQ_ENTRY(ppp) next;

    /* ----------------------------------------------------------------------
    General use
    ---------------------------------------------------------------------- */
    u_long 	serviceID;		/* service ID in the cache */
    u_short 	subfamily;		/* ref number of the driver, within APPLE_IF_FAM_PPP */
    u_int16_t 	unit;			/* ref number in the interfaces managed by this Controller */
    u_int16_t 	ifunit;			/* real ifunit number */
    u_char      name[IFNAMSIZ];		/* real ifname */
    
    u_int16_t 	phase;			/* where the link is at */
    u_char 	need_connect;		/* link needs to be connected when appears */
    u_char 	need_autoconnect;	/* link needs to be configure in dial on demand when appears */
    u_char 	need_autolisten;	/* link needs to be configure in listen mode when appears */
    u_char 	need_dispose;		/* link needs to be released */
    u_char 	need_attach;		/* link needs to be attached to real interface */
    
    u_long 	conntime; 		/* time when connected	*/
    int		log_to_fd;		/* send log messages to this fd too */
    u_int8_t	debug;			/* Debug flag */

    u_int8_t 	unsuccess;		/* # unsuccessful connection attempts */
    char 	hostname[MAXNAMELEN];	/* Our hostname */
    u_int8_t 	demand;			/* dial on demand */
    u_int8_t 	attached;		/* is this ppp attached to the ppp domain yet ? */
    u_char	disclogout;		/* disconnect on logout */
    u_char	connlogout;		/* allow connect when logout */

    /* there is 3 kind of options :
    - the default, from the database or coded by ppp
    - the negociated, obtained when connecting
    - the client option, requested by the client api, to temporarily override default options
    the connect command will apply the default or client option set,
    then the protocol will negociate some of the values.
    upon disconnection, ppp will reuse the default options set to reconfigure itself 
    in the case autolisten or autoconnect.
    default options are used as base values for clients of
    the api that want to use the default values and
    temporary override some of them.        */
    struct options 	def_options;	// default options for the connection

    /************ MUST BE GROUPED TOGETHER ********/
    u_int32_t 	outpacket_ctl; 		/* multiplex control to write packet */
    u_char 	outpacket_buf[PPP_MRU+PPP_HDRLEN]; /* buffer for outgoing packet */
    /**********************************************/
    
    int 	status;			/* last fail status */
    /* ----------------------------------------------------------------------
        Comm Part
    ---------------------------------------------------------------------- */

    struct ppp_caps	link_caps;	/* various capabilities info about the link */
    /* link_caps.support_dial = 1 if link is a dialup link
        dialup links can be manipulated with connect/disconnect ioctls
        non-dialup links need external connectors */
    CFRunLoopTimerRef	link_connectTORef; /* delay after connect timer */
    CFRunLoopTimerRef	redialTORef; 	/* redial on busy signal timer */
    u_short	redialcount;		/* nb of time to redial on busy signal */
    u_short	redialinterval;		/* nb of seconds between redials */

    /* dialup links specific */
    int 	link_ignore_disc;	 /* ignore next disconnection event */
    char	remoteaddr[256];	/* phone number or remote address to connect to */
    char      	altremoteaddr[256];     /* alternate phone number or remote address to connect to */
    u_char    	redialstate;       	/* current redialing address */
    char	listenfilter[256];	/* filter for incoming calls */
    u_long	link_idle_timer;	/* Disconnect if idle for this many seconds */
    u_long 	link_session_timer; 	/* max conn time, 0 == no limit*/
    u_int8_t 	link_state; 		/* current link state */
    
    /* non-dialup links specific */
    char	devnam[MAXPATHLEN];	/* Device name */
    int		devspeed;		/* Input/Output speed requested */
    int		speaker;		/* skeaker enabled */
    int		pulse;			/* dial modem in pulse mode */
    int		dialmode;		/* 0 = normal, 1 = blind (ignore dialtone), 2 = manual */
    char	cclname[MAXPATHLEN];	/* ccl to dial physical link */
    char	cclprgm[MAXPATHLEN];	/* ccl to dial physical link */
    u_short	chatmode;		/* use connection script or terminal window  */
    char	chatname[MAXPATHLEN];	/* script to connect */
    char	chatprgm[MAXPATHLEN];	/* program used to connect [i.e. terminal window] */
    pid_t 	cclpid;			/* pid of the current ccl engine*/
    pid_t 	chatpid;		/* pid of the current chat engine*/
    CFSocketRef	ttyref;			/* Serial port file descriptor */
    CFRunLoopSourceRef	ttyrls;		/* Runloop source tor the file descriptor */
    int 	initdisc;		/* Initial TTY discipline for ppp_fd */

    /* generic for links */
    char	lastmsg[256];		/* last msg from script or ccl */
    u_int32_t	connect_speed;		/* actual connection script */
    int		connect_delay; 		/* wait this many ms after connect script */

    /* ----------------------------------------------------------------------
        LCP Part
    ---------------------------------------------------------------------- */
    fsm 	lcp_fsm;		/* LCP fsm structure*/
    u_int16_t 	lcp_peer_mru;		/* peer MRU */
    u_int16_t	lcp_echo_interval; 	/* Interval between LCP echo-requests */
    u_int16_t	lcp_echo_fails;		/* Tolerance to unanswered echo-requests */
    u_int8_t	lcp_lax_recv;		/* accept control chars in asyncmap */
    lcp_options lcp_wantoptions;	/* Options that we want to request */
    lcp_options lcp_gotoptions;		/* Options that peer ack'd */
    lcp_options lcp_allowoptions;	/* Options we allow peer to request */
    lcp_options lcp_hisoptions;		/* Options that we ack'd */
    u_int32_t 	lcp_xmit_accm[8];	/* extended transmit ACCM */

    u_int8_t 	lcp_echos_pending;	/* Number of outstanding echo msgs */
    u_int16_t 	lcp_echo_number ;	/* ID number of next echo frame */
    u_int8_t 	lcp_echo_timer_running; /* set if a timer is running */

    CFRunLoopTimerRef lcp_echoTORef;	/* timer ref */

    u_int8_t 	lcp_nak_buffer[PPP_MRU];/* where we construct a nak packet */
    u_int8_t 	lcp_loopbackfail;

    // negociated values, for easier retrieval
    u_int16_t	lcp_mtu;
    u_int32_t	lcp_txasyncmap;
    u_int8_t	lcp_txpcomp;
    u_int8_t	lcp_txaccomp;
    u_int16_t	lcp_mru;
    u_int32_t	lcp_rcasyncmap;
    u_int8_t	lcp_rcpcomp;
    u_int8_t	lcp_rcaccomp;

    /* ----------------------------------------------------------------------
         Auth Part
    ---------------------------------------------------------------------- */

    char	our_name[MAXNAMELEN];	/* Our name for authentication purposes */
    char	user[MAXNAMELEN];	/* Username for PAP */
    char	passwd[MAXSECRETLEN];	/* Password for PAP */
    char 	peer_authname[MAXNAMELEN];    /* The name by which the peer authenticated itself to us */
    int 	auth_pending;		/* Records which authentication operations haven't completed yet */
    CFRunLoopTimerRef	auth_idleTORef;	/* inactivity timer ref */
    CFRunLoopTimerRef	auth_sessionTORef;/* max session timer ref */
   
    struct permitted_ip *addresses;	/* List of addresses which the peer may use */
    struct wordlist *noauth_addrs;	/* Wordlist giving addresses which the peer may use without authenticating itself */
    struct wordlist *extra_options;	/* Extra options to apply, from the secrets file entry for the peer */
    int 	num_np_open;		/* Number of network protocols which we have opened */
    int 	num_np_up;		/* Number of network protocols which have come up */
    int 	passwd_from_file;	/* Set if we got the contents of passwd[] from the pap-secrets file. */
    u_char 	default_auth;		/* Set if we require authentication only because we have a default route */
    u_char 	auth_required;		/* Always require authentication from peer */
    u_char 	allow_any_ip;		/* Allow peer to use any IP address */
    u_char 	explicit_remote;	/* User specified explicit remote name */
    char 	remote_name[MAXNAMELEN];/* Peer's name for authentication */

    /* ----------------------------------------------------------------------
         PAP Part
    ---------------------------------------------------------------------- */
    upap_state 	upap;			/* UPAP state */
    int 	upap_attempts;
    CFRunLoopTimerRef 	upap_reqTORef;
    CFRunLoopTimerRef 	upap_TORef;
    
    /* ----------------------------------------------------------------------
         CHAP Part
    ---------------------------------------------------------------------- */
    chap_state 	chap;			/* CHAP state */
    CFRunLoopTimerRef	chap_challengeTORef;/* chap challenge timer ref */
    CFRunLoopTimerRef	chap_rechallengeTORef;/* chap challenge timer ref */
    CFRunLoopTimerRef	chap_responseTORef;/* chap challenge timer ref */
        
    /* ----------------------------------------------------------------------
         CCP Part
    ---------------------------------------------------------------------- */

    /* ----------------------------------------------------------------------
        IPCP Part
    ---------------------------------------------------------------------- */
    fsm 	ipcp_fsm;		/* IPCP fsm structure */
    ipcp_options ipcp_wantoptions;	/* Options that we want to request */
    ipcp_options ipcp_gotoptions;	/* Options that peer ack'd */
    ipcp_options ipcp_allowoptions;	/* Options we allow peer to request */
    ipcp_options ipcp_hisoptions;	/* Options that we ack'd */
    int 	ipcp_dns1;		/* temporary change for DNS information */
    int 	ipcp_dns2;		/* until we get configd plumbing done */
    int 	ipcp_dns3;		/* until we get configd plumbing done */
    char 	ipcp_dname[256];
    u_long 	ipcp_ouraddr;		/* our current ip address */
    u_long 	ipcp_hisaddr;		/* his current ip address */

    u_int32_t 	ipcp_usermask;		/* IP netmask to set on interface */
    u_char	ipcp_disable_defaultip;/* Don't use hostname for default IP adrs */
    int 	ipcp_proxy_arp_set;	/* Have created proxy arp entry */
    u_char 	ipcp_usepeerdns;	/* Ask peer for DNS addrs */
    int 	ipcp_is_up;		/* have called np_up() */
    u_int32_t 	proxy_arp_addr;	/* remote addr for proxy arp */

    // Fix me : other parameters to move
    int		kdebugflag; // = 0;	/* Tell kernel to print debug messages */
    u_char	lockflag; // = 0;	/* Create lock file to lock the serial dev */
    int		need_holdoff;		/* Need holdoff period after link terminates */
    int		holdoff; // = 30;	/* # seconds to pause before reconnecting */
    u_char	holdoff_specified;	/* true if a holdoff value has been given */
    int 	rtm_seq;

    /*
     * PPP Data Link Layer "protocol" table.
     * One entry per supported protocol.
     * The last entry must be NULL.
     */
#define MAX_PROTO 5 	// .... fix me to be more generic
    struct protent *protocols[MAX_PROTO];
};

/* this struct contains all the information about a ppp interface driver */
struct driver {

    TAILQ_ENTRY(driver) next;

    u_short 	subfamily;		/* ref number of the driver, within APPLE_IF_FAM_PPP */
    u_char      name[IFNAMSIZ];
    struct ppp_caps	caps;	/* various capabilities info about the link */
};



void ppp_new_phase(struct ppp *ppp, u_int16_t phase);
void ppp_new_event(struct ppp *ppp, u_int32_t phase);
void ppp_output(struct ppp *ppp, u_char *p, int len);
int ppp_get_idle_time(struct ppp *ppp, struct ppp_idle *ip);



void ppp_send_config(struct ppp *, int, u_int32_t, int, int);  /* Configure i/f transmit parameters */
void ppp_set_xaccm(struct ppp *, ext_accm); /* Set extended transmit ACCM */
void ppp_recv_config(struct ppp *, int, u_int32_t, int, int); /* Configure i/f receive parameters */


int  have_route_to __P((u_int32_t)); /* Check if route to addr exists */
int  sifdefaultroute __P((struct ppp *, u_int32_t, u_int32_t));
                                /* Create default route through i/f */
int  cifdefaultroute __P((struct ppp *, u_int32_t, u_int32_t));
                                /* Delete default route through i/f */


CFSocketRef AddSocketNativeToRunLoop(int fd);
//int DelSocketRefFromRunLoop(CFSocketRef ref);
int DelSocketRef(CFSocketRef ref);
CFSocketRef CreateSocketRefWithNative(int fd);
CFRunLoopSourceRef AddSocketRefToRunLoop(CFSocketRef ref);
void DelRunLoopSource(CFRunLoopSourceRef rls);


CFRunLoopTimerRef AddTimerToRunLoop(void (*func) __P((CFRunLoopTimerRef, void *)), void *arg, u_short time);

void DelTimerFromRunLoop(CFRunLoopTimerRef *timer);

u_long close_cleanup (u_short id, struct msg *msg);
u_long ppp_readsockfd_data(struct msg *msg);

int ppp_attachip(struct ppp * ppp);
int ppp_detachip(struct ppp * ppp);
int ppp_set_ipvjcomp (struct ppp *, int, int, int);
int ppp_setipv4proxyarp(struct ppp *ppp, u_int32_t hisaddr);
int ppp_clearipv4proxyarp(struct ppp *ppp, u_int32_t hisaddr);
u_int32_t ppp_get_mask(struct ppp *ppp, u_int32_t addr);
int ppp_delroute(struct ppp *ppp, u_int32_t loc, u_int32_t dst);
int ppp_addroute(struct ppp *ppp, u_int32_t loc, u_int32_t dst, u_int32_t usermask,
                             u_int32_t dns1, u_int32_t dns2, u_char *hostname);

void ppp_autoconnect_off(struct ppp *ppp);
void ppp_autoconnect_on(struct ppp *ppp);


/* Values for do_callback and doing_callback */
#define CALLBACK_DIALIN		1	/* we are expecting the call back */
#define CALLBACK_DIALOUT	2	/* we are dialling out to call back */

enum {
    redial_none = 0,
    redial_main,
    redial_alternate
};

/*
 * Inline versions of get/put char/short/long.
 * Pointer is advanced; we assume that both arguments
 * are lvalues and will already be in registers.
 * cp MUST be u_char *.
 */
#define GETCHAR(c, cp) { \
        (c) = *(cp)++; \
}
#define PUTCHAR(c, cp) { \
        *(cp)++ = (u_char) (c); \
}


#define GETSHORT(s, cp) { \
        (s) = *(cp)++ << 8; \
        (s) |= *(cp)++; \
}
#define PUTSHORT(s, cp) { \
        *(cp)++ = (u_char) ((s) >> 8); \
        *(cp)++ = (u_char) (s); \
}

#define GETLONG(l, cp) { \
        (l) = *(cp)++ << 8; \
        (l) |= *(cp)++; (l) <<= 8; \
        (l) |= *(cp)++; (l) <<= 8; \
        (l) |= *(cp)++; \
}
#define PUTLONG(l, cp) { \
        *(cp)++ = (u_char) ((l) >> 24); \
        *(cp)++ = (u_char) ((l) >> 16); \
        *(cp)++ = (u_char) ((l) >> 8); \
        *(cp)++ = (u_char) (l); \
}

#define INCPTR(n, cp)	((cp) += (n))
#define DECPTR(n, cp)	((cp) -= (n))





/*
 * System dependent definitions for user-level 4.3BSD UNIX implementation.
 */

#define BCOPY(s, d, l)		memcpy(d, s, l)
#define BZERO(s, n)		memset(s, 0, n)

#define PRINTMSG(ppp, m, l)		{ info(ppp, "Remote message: %0.*v", l, m); }

/*
 * MAKEHEADER - Add Header fields to a packet.
 */
#define MAKEHEADER(p, t) { \
    PUTCHAR(PPP_ALLSTATIONS, p); \
    PUTCHAR(PPP_UI, p); \
    PUTSHORT(t, p); }

/*
 * Debug macros.  Slightly useful for finding bugs in pppd, not particularly
 * useful for finding out why your connection isn't being established.
 */
#ifdef DEBUGALL
#define DEBUGMAIN	1
#define DEBUGFSM	1
#define DEBUGLCP	1
#define DEBUGIPCP	1
#define DEBUGIPV6CP	1
#define DEBUGUPAP	1
#define DEBUGCHAP	1
#endif

#ifndef LOG_PPP			/* we use LOG_LOCAL2 for syslog by default */
#if defined(DEBUGMAIN) || defined(DEBUGFSM) || defined(DEBUGSYS) \
  || defined(DEBUGLCP) || defined(DEBUGIPCP) || defined(DEBUGUPAP) \
  || defined(DEBUGCHAP) || defined(DEBUG) || defined(DEBUGIPV6CP)
#define LOG_PPP LOG_LOCAL2
#else
#define LOG_PPP LOG_DAEMON
#endif
#endif /* LOG_PPP */

#ifdef DEBUGMAIN
#define MAINDEBUG(x)	if (debug) dbglog x
#else
#define MAINDEBUG(x)
#endif

#ifdef DEBUGSYS
#define SYSDEBUG(x)	if (debug) dbglog x
#else
#define SYSDEBUG(x)
#endif

#ifdef DEBUGFSM
#define FSMDEBUG(x)	if (debug) dbglog x
#else
#define FSMDEBUG(x)
#endif

#ifdef DEBUGLCP
#define LCPDEBUG(x)	if (debug) dbglog x
#else
#define LCPDEBUG(x)
#endif

#ifdef DEBUGIPCP
#define IPCPDEBUG(x)	if (debug) dbglog x
#else
#define IPCPDEBUG(x)
#endif

#ifdef DEBUGIPV6CP
#define IPV6CPDEBUG(x)  if (debug) dbglog x
#else
#define IPV6CPDEBUG(x)
#endif

#ifdef DEBUGUPAP
#define UPAPDEBUG(x)	if (debug) dbglog x
#else
#define UPAPDEBUG(x)
#endif

#ifdef DEBUGCHAP
#define CHAPDEBUG(x)	if (debug) dbglog x
#else
#define CHAPDEBUG(x)
#endif

#ifdef DEBUGIPXCP
#define IPXCPDEBUG(x)	if (debug) dbglog x
#else
#define IPXCPDEBUG(x)
#endif

#ifndef SIGTYPE
#if defined(sun) || defined(SYSV) || defined(POSIX_SOURCE)
#define SIGTYPE void
#else
#define SIGTYPE int
#endif /* defined(sun) || defined(SYSV) || defined(POSIX_SOURCE) */
#endif /* SIGTYPE */

#ifndef MIN
#define MIN(a, b)	((a) < (b)? (a): (b))
#endif
#ifndef MAX
#define MAX(a, b)	((a) > (b)? (a): (b))
#endif

#endif
