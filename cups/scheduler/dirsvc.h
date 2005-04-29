/*
 * "$Id: dirsvc.h,v 1.7 2005/02/16 17:58:01 jlovell Exp $"
 *
 *   Directory services definitions for the Common UNIX Printing System
 *   (CUPS) scheduler.
 *
 *   Copyright 1997-2005 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights are outlined in the file
 *   "LICENSE.txt" which should have been included with this file.  If this
 *   file is missing or damaged please contact Easy Software Products
 *   at:
 *
 *       Attn: CUPS Licensing Information
 *       Easy Software Products
 *       44141 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 */

/*
 * Include necessary headers...
 */

#ifdef HAVE_LIBSLP
#  include <slp.h>
#endif /* HAVE_LIBSLP */

#ifdef HAVE_DNSSD
#  include <dns_sd.h>
#endif /* HAVE_DNSSD */

/*
 * Browse protocols...
 */

#define BROWSE_CUPS	1		/* CUPS */
#define	BROWSE_SLP	2		/* SLPv2 */
#define BROWSE_LDAP	4		/* LDAP (not supported yet) */
#define BROWSE_DNSSD	8		/* DNS Service Discovery (aka Bonjour) */
#define BROWSE_ALL	15		/* All protocols */


/*
 * Browse address...
 */

typedef struct
{
  char			iface[32];	/* Destination interface */
  struct sockaddr_in	to;		/* Destination address */
} dirsvc_addr_t;


/*
 * Relay structure...
 */

typedef struct
{
  authmask_t		from;		/* Source address/name mask */
  struct sockaddr_in	to;		/* Destination address */
} dirsvc_relay_t;


/*
 * Polling structure...
 */

typedef struct
{
  char			hostname[16];	/* Hostname (actually, IP address) */
  int			port;		/* Port number */
  int			pid;		/* Current poll server PID */
} dirsvc_poll_t;


/*
 * Globals...
 */

VAR int			Browsing	VALUE(TRUE),
					/* Whether or not browsing is enabled */
			BrowseLocalProtocols	VALUE(BROWSE_ALL),
					/* Register protocols to support */
			BrowseRemoteProtocols	VALUE(BROWSE_ALL),
					/* Listen protocols to support */
			BrowseShortNames VALUE(TRUE),
					/* Short names for remote printers? */
			BrowseSocket	VALUE(-1),
					/* Socket for browsing */
			BrowsePort	VALUE(IPP_PORT),
					/* Port number for broadcasts */
			BrowseInterval	VALUE(DEFAULT_INTERVAL),
					/* Broadcast interval in seconds */
			BrowseTimeout	VALUE(DEFAULT_TIMEOUT),
					/* Time out for printers in seconds */
			NumBrowsers	VALUE(0);
					/* Number of broadcast addresses */
VAR dirsvc_addr_t	*Browsers	VALUE(NULL);
					/* Broadcast addresses */
VAR location_t		*BrowseACL	VALUE(NULL);
					/* Browser access control list */
VAR printer_t		*BrowseNext	VALUE(NULL);
					/* Next class/printer to broadcast */
VAR int			NumRelays	VALUE(0);
					/* Number of broadcast relays */
VAR dirsvc_relay_t	*Relays		VALUE(NULL);
					/* Broadcast relays */
VAR int			NumPolled	VALUE(0);
					/* Number of polled servers */
VAR dirsvc_poll_t	*Polled		VALUE(NULL);
					/* Polled servers */
VAR int			PollPipe	VALUE(0);
					/* Status pipe for pollers */

#ifdef HAVE_LIBSLP
VAR SLPHandle		BrowseSLPHandle	VALUE(NULL);
					/* SLP API handle */
VAR time_t		BrowseSLPRefresh VALUE(0);
					/* Next SLP refresh time */
#endif /* HAVE_LIBSLP */

#ifdef HAVE_DNSSD
VAR DNSServiceRef	BrowseDNSSDRef	VALUE(NULL);
					/* Browse reference */
VAR int			BrowseDNSSDfd	VALUE(-1);
					/* Browse descriptor */

typedef struct dnssd_resolve {
  DNSServiceRef		sdRef;		/* Reference to pending resolve */
  int			fd;		/* sdRef descriptor */
  char			*service_name;	/* Service name */
  struct dnssd_resolve*	next;		/* Next pending resolve in list */
} dnssd_resolve_t;

VAR dnssd_resolve_t	*DNSSDPendingResolves	VALUE(NULL);
					/* Pending dnssd resolves */

#endif /* HAVE_DNSSD HAVE_DNSSD */

/*
 * Prototypes...
 */

extern void	SendBrowseList(void);
extern void	StartBrowsing(void);
extern void	StartPolling(void);
extern void	StopBrowsing(void);
extern void	StopPolling(void);
extern void	UpdateCUPSBrowse(void);
extern void	UpdatePolling(void);
extern void	UpdateSLPBrowse(void);
extern void	BrowseRegisterPrinter(printer_t *p);
extern void	BrowseDeregisterPrinter(printer_t *p, int delete);


/*
 * End of "$Id: dirsvc.h,v 1.7 2005/02/16 17:58:01 jlovell Exp $".
 */
