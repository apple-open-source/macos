/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 2000
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Support for splitting captures into multiple files with a maximum
 * file size:
 *
 * Copyright (c) 2001
 *	Seth Webster <swebster@sst.ll.mit.edu>
 */

#ifndef lint
static const char copyright[] _U_ =
    "@(#) Copyright (c) 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 2000\n\
The Regents of the University of California.  All rights reserved.\n";
static const char rcsid[] _U_ =
    "@(#) $Header: /tcpdump/master/tcpdump/tcpdump.c,v 1.283 2008-09-25 21:45:50 guy Exp $ (LBL)";
#endif

/*
 * tcpdump - monitor tcp/ip traffic on an ethernet.
 *
 * First written in 1987 by Van Jacobson, Lawrence Berkeley Laboratory.
 * Mercilessly hacked and occasionally improved since then via the
 * combined efforts of Van, Steve McCanne and Craig Leres of LBL.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tcpdump-stdinc.h>

#ifdef WIN32
#include "getopt.h"
#include "w32_fzs.h"
extern int strcasecmp (const char *__s1, const char *__s2);
extern int SIZE_BUF;
#define off_t long
#define uint UINT
#endif /* WIN32 */

#ifdef HAVE_SMI_H
#include <smi.h>
#endif

#ifdef __APPLE__
#define __APPLE_PCAP_NG_API
#include <sys/ioctl.h>
#include "pktmetadatafilter.h"
#endif /* __APPLE__ */
#include <pcap.h>
#ifdef DLT_PKTAP
#include <net/pktap.h>
#include <net/iptap.h>
#endif
#ifdef DLT_PCAPNG
#include <pcap/pcap-ng.h>
#include <pcap/pcap-util.h>
#endif
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#ifndef WIN32
#include <sys/wait.h>
#include <sys/resource.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#endif /* WIN32 */


#include "netdissect.h"
#include "interface.h"
#include "addrtoname.h"
#include "machdep.h"
#include "setsignal.h"
#include "gmt2local.h"
#include "pcap-missing.h"

#ifndef NAME_MAX
#define NAME_MAX 255
#endif

#ifdef SIGINFO
#define SIGNAL_REQ_INFO SIGINFO
#elif SIGUSR1
#define SIGNAL_REQ_INFO SIGUSR1
#endif

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

netdissect_options Gndo;
netdissect_options *gndo = &Gndo;

static int dflag;			/* print filter code */
static int Lflag;			/* list available data link types and exit */
#ifdef HAVE_PCAP_SET_TSTAMP_TYPE
static int Jflag;			/* list available time stamp types */
#endif
static char *zflag = NULL;		/* compress each savefile using a specified command (like gzip or bzip2) */

static int infodelay;
static int infoprint;

char *filter_src_buf = NULL;

char *program_name;

int32_t thiszone;		/* seconds offset from gmt to local time */

/* Forwards */
static RETSIGTYPE cleanup(int);
static RETSIGTYPE child_cleanup(int);
static void usage(void) __attribute__((noreturn));
static void show_dlts_and_exit(const char *device, pcap_t *pd) __attribute__((noreturn));

static void print_callback(u_char *, const struct pcap_pkthdr *, const u_char *);
static void ndo_default_print(netdissect_options *, const u_char *, u_int);
static void dump_packet(u_char *, const struct pcap_pkthdr *, const u_char *);
static void droproot(const char *, const char *);
static void ndo_error(netdissect_options *ndo, const char *fmt, ...)
     __attribute__ ((noreturn, format (printf, 2, 3)));
static void ndo_warning(netdissect_options *ndo, const char *fmt, ...);

#ifdef __APPLE__

node_t *pkt_meta_data_expression = NULL;

char *open_special_device(char *);
int pktap_filter_packet(pcap_t *, struct pcap_if_info *, const struct pcap_pkthdr *, const u_char *);

#endif /* __APPLE__ */

#ifdef SIGNAL_REQ_INFO
RETSIGTYPE requestinfo(int);
#endif

#if defined(USE_WIN32_MM_TIMER)
  #include <MMsystem.h>
  static UINT timer_id;
  static void CALLBACK verbose_stats_dump(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);
#elif defined(HAVE_ALARM)
  static void verbose_stats_dump(int sig);
#endif

static void info(int);
static u_long packets_captured;
static u_long max_packet_cnt = -1;
u_int packets_mtdt_fltr_drop = 0; /* Drops by metadata filter */

struct printer {
        if_printer f;
	int type;
};


struct ndo_printer {
        if_ndo_printer f;
	int type;
};


static struct printer printers[] = {
	{ arcnet_if_print,	DLT_ARCNET },
#ifdef DLT_ARCNET_LINUX
	{ arcnet_linux_if_print, DLT_ARCNET_LINUX },
#endif
	{ token_if_print,	DLT_IEEE802 },
#ifdef DLT_LANE8023
	{ lane_if_print,        DLT_LANE8023 },
#endif
#ifdef DLT_CIP
	{ cip_if_print,         DLT_CIP },
#endif
#ifdef DLT_ATM_CLIP
	{ cip_if_print,		DLT_ATM_CLIP },
#endif
	{ sl_if_print,		DLT_SLIP },
#ifdef DLT_SLIP_BSDOS
	{ sl_bsdos_if_print,	DLT_SLIP_BSDOS },
#endif
	{ ppp_if_print,		DLT_PPP },
#ifdef DLT_PPP_WITHDIRECTION
	{ ppp_if_print,		DLT_PPP_WITHDIRECTION },
#endif
#ifdef DLT_PPP_BSDOS
	{ ppp_bsdos_if_print,	DLT_PPP_BSDOS },
#endif
	{ fddi_if_print,	DLT_FDDI },
	{ null_if_print,	DLT_NULL },
#ifdef DLT_LOOP
	{ null_if_print,	DLT_LOOP },
#endif
	{ raw_if_print,		DLT_RAW },
	{ atm_if_print,		DLT_ATM_RFC1483 },
#ifdef DLT_C_HDLC
	{ chdlc_if_print,	DLT_C_HDLC },
#endif
#ifdef DLT_HDLC
	{ chdlc_if_print,	DLT_HDLC },
#endif
#ifdef DLT_PPP_SERIAL
	{ ppp_hdlc_if_print,	DLT_PPP_SERIAL },
#endif
#ifdef DLT_PPP_ETHER
	{ pppoe_if_print,	DLT_PPP_ETHER },
#endif
#ifdef DLT_LINUX_SLL
	{ sll_if_print,		DLT_LINUX_SLL },
#endif
#ifdef DLT_IEEE802_11
	{ ieee802_11_if_print,	DLT_IEEE802_11},
#endif
#ifdef DLT_LTALK
	{ ltalk_if_print,	DLT_LTALK },
#endif
#if defined(DLT_PFLOG) && defined(HAVE_NET_PFVAR_H)
	{ pflog_if_print,	DLT_PFLOG },
#endif
#ifdef DLT_FR
	{ fr_if_print,		DLT_FR },
#endif
#ifdef DLT_FRELAY
	{ fr_if_print,		DLT_FRELAY },
#endif
#ifdef DLT_SUNATM
	{ sunatm_if_print,	DLT_SUNATM },
#endif
#ifdef DLT_IP_OVER_FC
	{ ipfc_if_print,	DLT_IP_OVER_FC },
#endif
#ifdef DLT_PRISM_HEADER
	{ prism_if_print,	DLT_PRISM_HEADER },
#endif
#ifdef DLT_IEEE802_11_RADIO
	{ ieee802_11_radio_if_print,	DLT_IEEE802_11_RADIO },
#endif
#ifdef DLT_ENC
	{ enc_if_print,		DLT_ENC },
#endif
#ifdef DLT_SYMANTEC_FIREWALL
	{ symantec_if_print,	DLT_SYMANTEC_FIREWALL },
#endif
#ifdef DLT_APPLE_IP_OVER_IEEE1394
	{ ap1394_if_print,	DLT_APPLE_IP_OVER_IEEE1394 },
#endif
#ifdef DLT_IEEE802_11_RADIO_AVS
	{ ieee802_11_radio_avs_if_print,	DLT_IEEE802_11_RADIO_AVS },
#endif
#ifdef DLT_JUNIPER_ATM1
	{ juniper_atm1_print,	DLT_JUNIPER_ATM1 },
#endif
#ifdef DLT_JUNIPER_ATM2
	{ juniper_atm2_print,	DLT_JUNIPER_ATM2 },
#endif
#ifdef DLT_JUNIPER_MFR
	{ juniper_mfr_print,	DLT_JUNIPER_MFR },
#endif
#ifdef DLT_JUNIPER_MLFR
	{ juniper_mlfr_print,	DLT_JUNIPER_MLFR },
#endif
#ifdef DLT_JUNIPER_MLPPP
	{ juniper_mlppp_print,	DLT_JUNIPER_MLPPP },
#endif
#ifdef DLT_JUNIPER_PPPOE
	{ juniper_pppoe_print,	DLT_JUNIPER_PPPOE },
#endif
#ifdef DLT_JUNIPER_PPPOE_ATM
	{ juniper_pppoe_atm_print, DLT_JUNIPER_PPPOE_ATM },
#endif
#ifdef DLT_JUNIPER_GGSN
	{ juniper_ggsn_print,	DLT_JUNIPER_GGSN },
#endif
#ifdef DLT_JUNIPER_ES
	{ juniper_es_print,	DLT_JUNIPER_ES },
#endif
#ifdef DLT_JUNIPER_MONITOR
	{ juniper_monitor_print, DLT_JUNIPER_MONITOR },
#endif
#ifdef DLT_JUNIPER_SERVICES
	{ juniper_services_print, DLT_JUNIPER_SERVICES },
#endif
#ifdef DLT_JUNIPER_ETHER
	{ juniper_ether_print,	DLT_JUNIPER_ETHER },
#endif
#ifdef DLT_JUNIPER_PPP
	{ juniper_ppp_print,	DLT_JUNIPER_PPP },
#endif
#ifdef DLT_JUNIPER_FRELAY
	{ juniper_frelay_print,	DLT_JUNIPER_FRELAY },
#endif
#ifdef DLT_JUNIPER_CHDLC
	{ juniper_chdlc_print,	DLT_JUNIPER_CHDLC },
#endif
#ifdef DLT_MFR
	{ mfr_if_print,		DLT_MFR },
#endif
#if defined(DLT_BLUETOOTH_HCI_H4_WITH_PHDR) && defined(HAVE_PCAP_BLUETOOTH_H)
	{ bt_if_print,		DLT_BLUETOOTH_HCI_H4_WITH_PHDR},
#endif
#ifdef HAVE_PCAP_USB_H
#ifdef DLT_USB_LINUX
	{ usb_linux_48_byte_print, DLT_USB_LINUX},
#endif /* DLT_USB_LINUX */
#ifdef DLT_USB_LINUX_MMAPPED
	{ usb_linux_64_byte_print, DLT_USB_LINUX_MMAPPED},
#endif /* DLT_USB_LINUX_MMAPPED */
#endif /* HAVE_PCAP_USB_H */
#ifdef DLT_IPV4
	{ raw_if_print,		DLT_IPV4 },
#endif
#ifdef DLT_IPV6
	{ raw_if_print,		DLT_IPV6 },
#endif
	{ NULL,			0 },
};

static struct ndo_printer ndo_printers[] = {
	{ ether_if_print,	DLT_EN10MB },
#ifdef DLT_IPNET
	{ ipnet_if_print,	DLT_IPNET },
#endif
#ifdef DLT_IEEE802_15_4
	{ ieee802_15_4_if_print, DLT_IEEE802_15_4 },
#endif
#ifdef DLT_IEEE802_15_4_NOFCS
	{ ieee802_15_4_if_print, DLT_IEEE802_15_4_NOFCS },
#endif
#ifdef DLT_PPI
	{ ppi_if_print,		DLT_PPI },
#endif
#ifdef DLT_NETANALYZER
	{ netanalyzer_if_print, DLT_NETANALYZER },
#endif
#ifdef DLT_NETANALYZER_TRANSPARENT
	{ netanalyzer_transparent_if_print, DLT_NETANALYZER_TRANSPARENT },
#endif
#ifdef DLT_PKTAP
	{ pktap_if_print,		DLT_PKTAP },
#endif
#ifdef DLT_PCAPNG
	{ pcapng_print,		DLT_PCAPNG },
#endif
	{ NULL,			0 },
};

if_printer
lookup_printer(int type)
{
	struct printer *p;

	for (p = printers; p->f; ++p)
		if (type == p->type)
			return p->f;

	return NULL;
	/* NOTREACHED */
}

if_ndo_printer
lookup_ndo_printer(int type)
{
	struct ndo_printer *p;

	for (p = ndo_printers; p->f; ++p)
		if (type == p->type)
			return p->f;

	return NULL;
	/* NOTREACHED */
}

/* packet capture handle to read packets from device or file */
static pcap_t *pd;

static int supports_monitor_mode;

extern int optind;
extern int opterr;
extern char *optarg;

struct print_info;
typedef int (*print_handler_func_t)(struct print_info *, const struct pcap_pkthdr *,
									const u_char *);
struct print_info {
	netdissect_options *ndo;
	union {
		if_printer     printer;
		if_ndo_printer ndo_printer;
	} p;
	int ndo_type;
	pcap_t *pcap;
	print_handler_func_t printer_func;
};

static int print_packet(struct print_info *, const struct pcap_pkthdr *, const u_char *);
static int print_pcap_ng_block(struct print_info *, const struct pcap_pkthdr *, const u_char *);
#ifdef __APPLE__
static int print_pktap_packet(struct print_info *, const struct pcap_pkthdr *, const u_char *);
#endif

struct dump_info;
typedef int (*dump_handler_func_t)(struct dump_info *, const struct pcap_pkthdr *,
							 const u_char *);
struct dump_info {
	char	*WFileName;
	char	*CurrentFileName;
	pcap_t	*pcap;
	pcap_dumper_t *dumper;
	dump_handler_func_t dumper_func;
};

int handle_pcap_dump(struct dump_info *, const struct pcap_pkthdr *, const u_char *);
int handle_bpf_exthdr_dump(struct dump_info *, const struct pcap_pkthdr *, const u_char *);
int handle_pcap_ng_dump(struct dump_info *, const struct pcap_pkthdr *, const u_char *);
int handle_pktap_dump(struct dump_info *, const struct pcap_pkthdr *, const u_char *);


#ifdef HAVE_PCAP_SET_TSTAMP_TYPE
static void
show_tstamp_types_and_exit(const char *device, pcap_t *pd)
{
	int n_tstamp_types;
	int *tstamp_types = 0;
	const char *tstamp_type_name;
	int i;

	n_tstamp_types = pcap_list_tstamp_types(pd, &tstamp_types);
	if (n_tstamp_types < 0)
		error("%s", pcap_geterr(pd));

	if (n_tstamp_types == 0) {
		fprintf(stderr, "Time stamp type cannot be set for %s\n",
		    device);
		exit(0);
	}
	fprintf(stderr, "Time stamp types for %s (use option -j to set):\n",
	    device);
	for (i = 0; i < n_tstamp_types; i++) {
		tstamp_type_name = pcap_tstamp_type_val_to_name(tstamp_types[i]);
		if (tstamp_type_name != NULL) {
			(void) fprintf(stderr, "  %s (%s)\n", tstamp_type_name,
			    pcap_tstamp_type_val_to_description(tstamp_types[i]));
		} else {
			(void) fprintf(stderr, "  %d\n", tstamp_types[i]);
		}
	}
	pcap_free_tstamp_types(tstamp_types);
	exit(0);
}
#endif

static void
show_dlts_and_exit(const char *device, pcap_t *pd)
{
	int n_dlts;
	int *dlts = 0;
	const char *dlt_name;

	n_dlts = pcap_list_datalinks(pd, &dlts);
	if (n_dlts < 0)
		error("%s", pcap_geterr(pd));
	else if (n_dlts == 0 || !dlts)
		error("No data link types.");

	/*
	 * If the interface is known to support monitor mode, indicate
	 * whether these are the data link types available when not in
	 * monitor mode, if -I wasn't specified, or when in monitor mode,
	 * when -I was specified (the link-layer types available in
	 * monitor mode might be different from the ones available when
	 * not in monitor mode).
	 */
	if (supports_monitor_mode)
		(void) fprintf(stderr, "Data link types for %s %s (use option -y to set):\n",
		    device,
		    Iflag ? "when in monitor mode" : "when not in monitor mode");
	else
		(void) fprintf(stderr, "Data link types for %s (use option -y to set):\n",
		    device);

	while (--n_dlts >= 0) {
		dlt_name = pcap_datalink_val_to_name(dlts[n_dlts]);
		if (dlt_name != NULL) {
			(void) fprintf(stderr, "  %s (%s)", dlt_name,
			    pcap_datalink_val_to_description(dlts[n_dlts]));

			/*
			 * OK, does tcpdump handle that type?
			 */
			if (lookup_printer(dlts[n_dlts]) == NULL
                            && lookup_ndo_printer(dlts[n_dlts]) == NULL)
				(void) fprintf(stderr, " (printing not supported)");
			fprintf(stderr, "\n");
		} else {
			(void) fprintf(stderr, "  DLT %d (printing not supported)\n",
			    dlts[n_dlts]);
		}
	}
	pcap_free_datalinks(dlts);
	exit(0);
}

/*
 * Set up flags that might or might not be supported depending on the
 * version of libpcap we're using.
 */
#if defined(HAVE_PCAP_CREATE) || defined(WIN32)
#define B_FLAG		"B:"
#define B_FLAG_USAGE	" [ -B size ]"
#else /* defined(HAVE_PCAP_CREATE) || defined(WIN32) */
#define B_FLAG
#define B_FLAG_USAGE
#endif /* defined(HAVE_PCAP_CREATE) || defined(WIN32) */

#ifdef __APPLE__
#define g_FLAG		"g"
#define Q_FLAG		"Q:"
#else
#define g_FLAG
#define Q_FLAG
#endif

#ifdef HAVE_PCAP_CREATE
#define I_FLAG		"I"
#else /* HAVE_PCAP_CREATE */
#define I_FLAG
#endif /* HAVE_PCAP_CREATE */

#ifdef HAVE_PCAP_SET_TSTAMP_TYPE
#define j_FLAG		"j:"
#define j_FLAG_USAGE	" [ -j tstamptype ]"
#define J_FLAG		"J"
#else /* PCAP_ERROR_TSTAMP_TYPE_NOTSUP */
#define j_FLAG
#define j_FLAG_USAGE
#define J_FLAG
#endif /* PCAP_ERROR_TSTAMP_TYPE_NOTSUP */

#ifdef HAVE_PCAP_FINDALLDEVS
#ifndef HAVE_PCAP_IF_T
#undef HAVE_PCAP_FINDALLDEVS
#endif
#endif

#ifdef HAVE_PCAP_FINDALLDEVS
#define D_FLAG	"D"
#else
#define D_FLAG
#endif

#ifdef HAVE_PCAP_DUMP_FLUSH
#define U_FLAG	"U"
#else
#define U_FLAG
#endif

#ifndef WIN32
/* Drop root privileges and chroot if necessary */
static void
droproot(const char *username, const char *chroot_dir)
{
	struct passwd *pw = NULL;

	if (chroot_dir && !username) {
		fprintf(stderr, "tcpdump: Chroot without dropping root is insecure\n");
		exit(1);
	}
	
	pw = getpwnam(username);
	if (pw) {
		if (chroot_dir) {
			if (chroot(chroot_dir) != 0 || chdir ("/") != 0) {
				fprintf(stderr, "tcpdump: Couldn't chroot/chdir to '%.64s': %s\n",
				    chroot_dir, pcap_strerror(errno));
				exit(1);
			}
		}
		if (initgroups(pw->pw_name, pw->pw_gid) != 0 ||
		    setgid(pw->pw_gid) != 0 || setuid(pw->pw_uid) != 0) {
			fprintf(stderr, "tcpdump: Couldn't change to '%.32s' uid=%lu gid=%lu: %s\n",
			    username, 
			    (unsigned long)pw->pw_uid,
			    (unsigned long)pw->pw_gid,
			    pcap_strerror(errno));
			exit(1);
		}
	}
	else {
		fprintf(stderr, "tcpdump: Couldn't find user '%.32s'\n",
		    username);
		exit(1);
	}
}
#endif /* WIN32 */

static int
getWflagChars(int x)
{
	int c = 0;

	x -= 1;
	while (x > 0) {
		c += 1;
		x /= 10;
	}

	return c;
}


static void
MakeFilename(char *buffer, char *orig_name, int cnt, int max_chars)
{
        char *filename = malloc(NAME_MAX + 1);

        /* Process with strftime if Gflag is set. */
        if (Gflag != 0) {
          struct tm *local_tm;

          /* Convert Gflag_time to a usable format */
          if ((local_tm = localtime(&Gflag_time)) == NULL) {
                  error("MakeTimedFilename: localtime");
          }

          /* There's no good way to detect an error in strftime since a return
           * value of 0 isn't necessarily failure.
           */
          strftime(filename, NAME_MAX, orig_name, local_tm);
        } else {
          strncpy(filename, orig_name, NAME_MAX);
        }

	if (cnt == 0 && max_chars == 0)
		strncpy(buffer, filename, NAME_MAX + 1);
	else
		if (snprintf(buffer, NAME_MAX + 1, "%s%0*d", filename, max_chars, cnt) > NAME_MAX)
                  /* Report an error if the filename is too large */
                  error("too many output files or filename is too long (> %d)", NAME_MAX);
        free(filename);
}

static int tcpdump_printf(netdissect_options *ndo _U_,
			  const char *fmt, ...)
{
  
  va_list args;
  int ret;

  va_start(args, fmt);
  ret=vfprintf(stdout, fmt, args);
  va_end(args);

  return ret;
}

int
main(int argc, char **argv)
{
	register int op, i;
	bpf_u_int32 localnet, netmask;
	register char *cp, *infile, *device, *RFileName, *WFileName;
	pcap_handler callback;
	int type;
	struct bpf_program fcode;
#ifndef WIN32
	RETSIGTYPE (*oldhandler)(int);
#endif
	struct print_info printinfo;
	struct dump_info dumpinfo;
	u_char *pcap_userdata;
	char ebuf[PCAP_ERRBUF_SIZE];
	char *username = NULL;
	char *chroot_dir = NULL;
#ifdef HAVE_PCAP_FINDALLDEVS
	pcap_if_t *devpointer;
	int devnum;
#endif
	int status;
#ifdef WIN32
	if(wsockinit() != 0) return 1;
#endif /* WIN32 */
#ifdef __APPLE__
	int on = 1;
	int no_loopkupnet_warning = 0;
#endif

	jflag=-1;	/* not set */
        gndo->ndo_Oflag=1;
	gndo->ndo_Rflag=1;
	gndo->ndo_dlt=-1;
	gndo->ndo_default_print=ndo_default_print;
	gndo->ndo_printf=tcpdump_printf;
	gndo->ndo_error=ndo_error;
	gndo->ndo_warning=ndo_warning;
	gndo->ndo_snaplen = DEFAULT_SNAPLEN;
  
	device = NULL;
	infile = NULL;
	RFileName = NULL;
	WFileName = NULL;
	if ((cp = strrchr(argv[0], '/')) != NULL)
		program_name = cp + 1;
	else
		program_name = argv[0];

	if (abort_on_misalignment(ebuf, sizeof(ebuf)) < 0)
		error("%s", ebuf);

#ifdef LIBSMI
	smiInit("tcpdump");
#endif

	while (
	    (op = getopt(argc, argv, "@1aAb" B_FLAG "c:C:d" D_FLAG "eE:fF:" g_FLAG " G:hHi:" I_FLAG j_FLAG J_FLAG "kKlLm:M:nNOpPq" Q_FLAG "r:Rs:StT:u" U_FLAG "vVw:W:xXy:Yz:Z:")) != -1)
		switch (op) {

		case 'a':
			/* compatibility for old -a */
			break;

		case 'A':
			++Aflag;
			break;

		case 'b':
			++bflag;
			break;

#if defined(HAVE_PCAP_CREATE) || defined(WIN32)
		case 'B':
			Bflag = atoi(optarg)*1024;
			if (Bflag <= 0)
				error("invalid packet buffer size %s", optarg);
			break;
#endif /* defined(HAVE_PCAP_CREATE) || defined(WIN32) */

		case 'c':
			max_packet_cnt = strtoul(optarg, NULL, 0);
			break;

		case 'C':
			Cflag = atoi(optarg) * 1000000;
			if (Cflag < 0)
				error("invalid file size %s", optarg);
			break;

		case 'd':
			++dflag;
			break;

#ifdef HAVE_PCAP_FINDALLDEVS
		case 'D':
			if (pcap_findalldevs(&devpointer, ebuf) < 0)
				error("%s", ebuf);
			else {
				for (i = 0; devpointer != 0; i++) {
					printf("%d.%s", i+1, devpointer->name);
					if (devpointer->description != NULL)
						printf(" (%s)", devpointer->description);
					printf("\n");
					devpointer = devpointer->next;
				}
			}
			return 0;
#endif /* HAVE_PCAP_FINDALLDEVS */

		case 'e':
			++eflag;
			break;

		case 'E':
#ifndef HAVE_LIBCRYPTO
			warning("crypto code not compiled in");
#endif
			gndo->ndo_espsecret = optarg;
			break;

		case 'f':
			++fflag;
			break;

		case 'F':
			infile = optarg;
			break;

#ifdef __APPLE__
		case 'g':
			gflag++;
			break;
#endif

		case 'G':
			Gflag = atoi(optarg);
			if (Gflag < 0)
				error("invalid number of seconds %s", optarg);

                        /* We will create one file initially. */
                        Gflag_count = 0;

			/* Grab the current time for rotation use. */
			if ((Gflag_time = time(NULL)) == (time_t)-1) {
				error("main: can't get current time: %s",
				    pcap_strerror(errno));
			}
			break;

		case 'h':
			usage();
			break;

		case 'H':
			++Hflag;
			break;

		case 'i':
			if (optarg[0] == '0' && optarg[1] == 0)
				error("Invalid adapter index");
			
#ifdef HAVE_PCAP_FINDALLDEVS
			/*
			 * If the argument is a number, treat it as
			 * an index into the list of adapters, as
			 * printed by "tcpdump -D".
			 *
			 * This should be OK on UNIX systems, as interfaces
			 * shouldn't have names that begin with digits.
			 * It can be useful on Windows, where more than
			 * one interface can have the same name.
			 */
			if ((devnum = atoi(optarg)) != 0) {
				if (devnum < 0)
					error("Invalid adapter index");

				if (pcap_findalldevs(&devpointer, ebuf) < 0)
					error("%s", ebuf);
				else {
					/*
					 * Look for the devnum-th entry
					 * in the list of devices
					 * (1-based).
					 */
					for (i = 0;
					    i < devnum-1 && devpointer != NULL;
					    i++, devpointer = devpointer->next)
						;
					if (devpointer == NULL)
						error("Invalid adapter index");
				}
				device = devpointer->name;
				break;
			}
#endif /* HAVE_PCAP_FINDALLDEVS */
			device = optarg;
			break;

#ifdef HAVE_PCAP_CREATE
		case 'I':
			++Iflag;
			break;
#endif /* HAVE_PCAP_CREATE */

#ifdef HAVE_PCAP_SET_TSTAMP_TYPE
		case 'j':
			jflag = pcap_tstamp_type_name_to_val(optarg);
			if (jflag < 0)
				error("invalid time stamp type %s", optarg);
			break;

		case 'J':
			Jflag++;
			break;
#endif

		case 'L':
			Lflag++;
			break;
		
		case 'l':
#ifdef WIN32
			/*
			 * _IOLBF is the same as _IOFBF in Microsoft's C
			 * libraries; the only alternative they offer
			 * is _IONBF.
			 *
			 * XXX - this should really be checking for MSVC++,
			 * not WIN32, if, for example, MinGW has its own
			 * C library that is more UNIX-compatible.
			 */
			setvbuf(stdout, NULL, _IONBF, 0);
#else /* WIN32 */
#ifdef HAVE_SETLINEBUF
			setlinebuf(stdout);
#else
			setvbuf(stdout, NULL, _IOLBF, 0);
#endif
#endif /* WIN32 */
			break;
#ifdef __APPLE__
		case 'k': {
			const char *kstr;
			int ch;
			int val = 0;
			
			if (optind >= argc || argv[optind][0] == '-') {
				kflag = PRMD_ALL;
				break;
			}
			kstr = argv[optind];
			
			while ((ch = *kstr++) != 0) {
				if (ch == 'I')
					val |= PRMD_IF;
				else if (ch == 'P')
					val |= PRMD_PID;
				else if (ch == 'N')
					val |= PRMD_PNAME;
				else if (ch == 'S')
					val |= PRMD_SVC;
				else if (ch == 'D')
					val |= PRMD_DIR;
				else if (ch == 'C')
					val |= PRMD_COMMENT;
				else {
					/*
					 * Was most likely parsing a filter expression 
					 * if we do not recognize the character
					 */
					if (val == 0)
						break;
					error("Invalid flag for option '-k'");
				}
			}
			if (val == 0)
				kflag = PRMD_ALL;
			else {
				kflag = val;
				optind++;
			}
			break;
		}
#endif
		case 'K':
			++Kflag;
			break;

		case 'm':
#ifdef LIBSMI
			if (smiLoadModule(optarg) == 0) {
				error("could not load MIB module %s", optarg);
			}
			sflag = 1;
#else
			(void)fprintf(stderr, "%s: ignoring option `-m %s' ",
				      program_name, optarg);
			(void)fprintf(stderr, "(no libsmi support)\n");
#endif
			break;

		case 'M':
			/* TCP-MD5 shared secret */
#ifndef HAVE_LIBCRYPTO
			warning("crypto code not compiled in");
#endif
			sigsecret = optarg;
			break;

		case 'n':
			++nflag;
			break;

		case 'N':
			++Nflag;
			break;

		case 'O':
			Oflag = 0;
			break;

#ifdef __APPLE__
		case 'P':
			++Pflag;
#endif

		case 'p':
			++pflag;
			break;

		case 'q':
			++qflag;
			++suppress_default_print;
			break;

#ifdef __APPLE__
		case 'Q': {
			pkt_meta_data_expression = parse_expression(optarg);
			if (pkt_meta_data_expression == NULL)
				error("invalid expression \"%s\"", optarg);
			break;
		}				
#endif
		case 'r':
			RFileName = optarg;
			break;

		case 'R':
			Rflag = 0;
			break;

		case 's': {
			char *end;

			gndo->ndo_snaplen = strtol(optarg, &end, 0);
			if (optarg == end || *end != '\0'
			    || gndo->ndo_snaplen < 0 || gndo->ndo_snaplen > MAXIMUM_SNAPLEN)
				error("invalid snaplen %s", optarg);
			else if (gndo->ndo_snaplen == 0)
				gndo->ndo_snaplen = MAXIMUM_SNAPLEN;
			break;
		}

		case 'S':
			++Sflag;
			break;

		case 't': {
			unsigned long mode;
			const char *ptr;
			char *endptr;
			
			if (optind >= argc || argv[optind][0] == '-') {
				++tflag;
				break;
			}
			ptr = argv[optind];
			mode = strtoul(ptr, &endptr, 0);
			if (*endptr != 0) {
				++tflag;
				break;
			}
			tflag = 1;
			switch (mode) {
				case 0:
					t0flag++;
					break;
				case 1:
					t1flag++;
					break;
				case 2:
					t2flag++;
					break;
				case 3:
					t3flag++;
					break;
				case 4:
					t4flag++;
					break;
				case 5:
					t5flag++;
					break;
				default:
					error("invalid timestamp mode %s", ptr);
					break;
			}
			optind++;
			break;
		}
		case 'T':
			if (strcasecmp(optarg, "vat") == 0)
				packettype = PT_VAT;
			else if (strcasecmp(optarg, "wb") == 0)
				packettype = PT_WB;
			else if (strcasecmp(optarg, "rpc") == 0)
				packettype = PT_RPC;
			else if (strcasecmp(optarg, "rtp") == 0)
				packettype = PT_RTP;
			else if (strcasecmp(optarg, "rtcp") == 0)
				packettype = PT_RTCP;
			else if (strcasecmp(optarg, "snmp") == 0)
				packettype = PT_SNMP;
			else if (strcasecmp(optarg, "cnfp") == 0)
				packettype = PT_CNFP;
			else if (strcasecmp(optarg, "tftp") == 0)
				packettype = PT_TFTP;
			else if (strcasecmp(optarg, "aodv") == 0)
				packettype = PT_AODV;
			else if (strcasecmp(optarg, "carp") == 0)
				packettype = PT_CARP;
			else
				error("unknown packet type `%s'", optarg);
			break;

		case 'u':
			++uflag;
			break;

#ifdef HAVE_PCAP_DUMP_FLUSH
		case 'U':
			++Uflag;
			break;
#endif

		case 'v':
			++vflag;
			break;

		case 'V':
			++Vflag;
			break;
			
		case 'w':
			WFileName = optarg;
			break;

		case 'W':
			Wflag = atoi(optarg);
			if (Wflag < 0) 
				error("invalid number of output files %s", optarg);
			WflagChars = getWflagChars(Wflag);
			break;

		case 'x':
			++xflag;
			++suppress_default_print;
			break;

		case 'X':
			++Xflag;
			++suppress_default_print;
			break;

		case 'y':
			gndo->ndo_dltname = optarg;
			gndo->ndo_dlt =
			  pcap_datalink_name_to_val(gndo->ndo_dltname);
			if (gndo->ndo_dlt < 0)
				error("invalid data link type %s", gndo->ndo_dltname);
			break;

#if defined(HAVE_PCAP_DEBUG) || defined(HAVE_YYDEBUG)
		case 'Y':
			{
			/* Undocumented flag */
#ifdef HAVE_PCAP_DEBUG
			extern int pcap_debug;
			pcap_debug = 1;
#else
			extern int yydebug;
			yydebug = 1;
#endif
			}
			break;
#endif
		case 'z':
			if (optarg) {
				zflag = strdup(optarg);
			} else {
				usage();
				/* NOTREACHED */
			}
			break;

		case 'Z':
			if (optarg) {
				username = strdup(optarg);
			}
			else {
				usage();
				/* NOTREACHED */
			}
			break;

		default:
			usage();
			/* NOTREACHED */
		}

	switch (tflag) {

	case 0: /* Default */
	case 4: /* Default + Date*/
		thiszone = gmt2local(0);
		break;

	case 1: /* No time stamp */
	case 2: /* Unix timeval style */
	case 3: /* Microseconds since previous packet */
	case 5: /* Microseconds since first packet */
		break;

	default: /* Not supported */
		error("only -t, -tt, -ttt, -tttt and -ttttt are supported");
		break;
	}

	if (t0flag || t4flag)
		thiszone = gmt2local(0);
				
#ifdef WITH_CHROOT
	/* if run as root, prepare for chrooting */
	if (getuid() == 0 || geteuid() == 0) {
		/* future extensibility for cmd-line arguments */
		if (!chroot_dir)
			chroot_dir = WITH_CHROOT;
	}
#endif

#ifdef WITH_USER
	/* if run as root, prepare for dropping root privileges */
	if (getuid() == 0 || geteuid() == 0) {
		/* Run with '-Z root' to restore old behaviour */ 
		if (!username)
			username = WITH_USER;
	}
#endif

	if (RFileName != NULL) {
		int dlt;
		const char *dlt_name;

#ifndef WIN32
		/*
		 * We don't need network access, so relinquish any set-UID
		 * or set-GID privileges we have (if any).
		 *
		 * We do *not* want set-UID privileges when opening a
		 * trace file, as that might let the user read other
		 * people's trace files (especially if we're set-UID
		 * root).
		 */
		if (setgid(getgid()) != 0 || setuid(getuid()) != 0 )
			fprintf(stderr, "Warning: setgid/setuid failed !\n");
#endif /* WIN32 */
#if defined(__APPLE__) && defined(DLT_PCAPNG)
		pd = pcap_ng_open_offline(RFileName, ebuf);
		if (pd != NULL) {
			fprintf(stderr, "reading from PCAP-NG file %s\n",
					RFileName);
			/*
			 * The output file is also a pcap-ng file
			 */
			Pflag++;
		} else
#endif /* __APPLE__ && DLT_PCAPNG */
        {
			pd = pcap_open_offline(RFileName, ebuf);
			if (pd == NULL)
				error("%s", ebuf);
			dlt = pcap_datalink(pd);
			dlt_name = pcap_datalink_val_to_name(dlt);
			if (dlt_name == NULL) {
				fprintf(stderr, "reading from file %s, link-type %u\n",
					RFileName, dlt);
			} else {
				fprintf(stderr,
					"reading from file %s, link-type %s (%s)\n",
					RFileName, dlt_name,
					pcap_datalink_val_to_description(dlt));
			}
		}
		localnet = 0;
		netmask = 0;
		if (fflag != 0)
			error("-f and -r options are incompatible");
	} else {
#if defined(__APPLE__) && defined(DLT_PKTAP)
		/*
		 * By default use a pktap interface to tap on multiple physical interfaces
		 * The special device name "all" means to tap on all interfaces
		 */
		if (device == NULL)
			device = PKTAP_IFNAME;
		if (strncmp(device, PKTAP_IFNAME, strlen(PKTAP_IFNAME)) == 0 ||
			strncmp(device, IPTAP_IFNAME, strlen(IPTAP_IFNAME)) == 0 ||
			strcmp(device, "any") == 0 ||
			strcmp(device, "all") == 0) {

			/* No need to attempt to use promiscuous mode */
			gndo->ndo_pflag = 1;

			/* Suppress useless warnings */
			no_loopkupnet_warning = 1;

            /* By default use PKTAP data link type */
            if (gndo->ndo_dltname == NULL) {
                gndo->ndo_dltname = "PKTAP";
                gndo->ndo_dlt = pcap_datalink_name_to_val(gndo->ndo_dltname);
                if (gndo->ndo_dlt < 0)
                    error("cannot use data link type %s", gndo->ndo_dltname);
            }
            /* Force PCAP-NG if capturing with metadata */
            if (gndo->ndo_dlt == DLT_PKTAP)
                Pflag++;
        }
#else /* __APPLE__ && DLT_PKTAP */
		if (device == NULL) {
			device = pcap_lookupdev(ebuf);
			if (device == NULL)
				error("%s", ebuf);
		}
#endif /* __APPLE__ && DLT_PCAPNG */
#ifdef WIN32
		if(strlen(device) == 1)	//we assume that an ASCII string is always longer than 1 char
		{						//a Unicode string has a \0 as second byte (so strlen() is 1)
			fprintf(stderr, "%s: listening on %ws\n", program_name, device);
		}
		else
		{
			fprintf(stderr, "%s: listening on %s\n", program_name, device);
		}

		fflush(stderr);	
#endif /* WIN32 */
#ifdef HAVE_PCAP_CREATE
		pd = pcap_create(device, ebuf);
		if (pd == NULL)
			error("%s", ebuf);
#ifdef HAVE_PCAP_SET_TSTAMP_TYPE
		if (Jflag)
			show_tstamp_types_and_exit(device, pd);
#endif
		/*
		 * Is this an interface that supports monitor mode?
		 */
		if (pcap_can_set_rfmon(pd) == 1)
			supports_monitor_mode = 1;
		else
			supports_monitor_mode = 0;
		status = pcap_set_snaplen(pd, gndo->ndo_snaplen);
		if (status != 0)
			error("%s: Can't set snapshot length: %s",
			    device, pcap_statustostr(status));
		status = pcap_set_promisc(pd, !pflag);
		if (status != 0)
			error("%s: Can't set promiscuous mode: %s",
			    device, pcap_statustostr(status));
		if (Iflag) {
			status = pcap_set_rfmon(pd, 1);
			if (status != 0)
				error("%s: Can't set monitor mode: %s",
				    device, pcap_statustostr(status));
		}
		status = pcap_set_timeout(pd, 1000);
		if (status != 0)
			error("%s: pcap_set_timeout failed: %s",
			    device, pcap_statustostr(status));
		if (Bflag != 0) {
			status = pcap_set_buffer_size(pd, Bflag);
			if (status != 0)
				error("%s: Can't set buffer size: %s",
				    device, pcap_statustostr(status));
		}
#ifdef HAVE_PCAP_SET_TSTAMP_TYPE
                if (jflag != -1) {
			status = pcap_set_tstamp_type(pd, jflag);
			if (status < 0)
				error("%s: Can't set time stamp type: %s",
			    	    device, pcap_statustostr(status));
		}
#endif
		status = pcap_activate(pd);
		if (status < 0) {
			/*
			 * pcap_activate() failed.
			 */
			cp = pcap_geterr(pd);
			if (status == PCAP_ERROR)
				error("%s", cp);
			else if ((status == PCAP_ERROR_NO_SUCH_DEVICE ||
			          status == PCAP_ERROR_PERM_DENIED) &&
			         *cp != '\0')
				error("%s: %s\n(%s)", device,
				    pcap_statustostr(status), cp);
			else
				error("%s: %s", device,
				    pcap_statustostr(status));
		} else if (status > 0) {
			/*
			 * pcap_activate() succeeded, but it's warning us
			 * of a problem it had.
			 */
			cp = pcap_geterr(pd);
			if (status == PCAP_WARNING)
				warning("%s", cp);
			else if (status == PCAP_WARNING_PROMISC_NOTSUP &&
			         *cp != '\0')
				warning("%s: %s\n(%s)", device,
				    pcap_statustostr(status), cp);
			else
				warning("%s: %s", device,
				    pcap_statustostr(status));
		}
#else
		*ebuf = '\0';
		pd = pcap_open_live(device, gndo->ndo_snaplen, !pflag, 1000, ebuf);
		if (pd == NULL)
			error("%s", ebuf);
		else if (*ebuf)
			warning("%s", ebuf);
#endif /* HAVE_PCAP_CREATE */
		
		/*
		 * Let user own process after socket has been opened.
		 */
#ifndef WIN32
		if (setgid(getgid()) != 0 || setuid(getuid()) != 0)
			fprintf(stderr, "Warning: setgid/setuid failed !\n");
#endif /* WIN32 */
#if !defined(HAVE_PCAP_CREATE) && defined(WIN32)
		if(Bflag != 0)
			if(pcap_setbuff(pd, Bflag)==-1){
				error("%s", pcap_geterr(pd));
			}
#endif /* !defined(HAVE_PCAP_CREATE) && defined(WIN32) */
		if (Lflag)
			show_dlts_and_exit(device, pd);
		if (gndo->ndo_dlt >= 0) {
#ifdef HAVE_PCAP_SET_DATALINK
			if (pcap_set_datalink(pd, gndo->ndo_dlt) < 0)
				error("%s", pcap_geterr(pd));
#else
			/*
			 * We don't actually support changing the
			 * data link type, so we only let them
			 * set it to what it already is.
			 */
			if (gndo->ndo_dlt != pcap_datalink(pd)) {
				error("%s is not one of the DLTs supported by this device\n",
				      gndo->ndo_dltname);
			}
#endif
			(void)fprintf(stderr, "%s: data link type %s\n",
				      program_name, gndo->ndo_dltname);
			(void)fflush(stderr);
		}
#ifdef __APPLE__
#ifdef DLT_PKTAP
		/*
		 * Use packet metadata from one source only: DLT_PKTAP
		 * supersedes the BPF extended header mechamisn
		 */
		if (pcap_datalink(pd) != DLT_PKTAP)
#endif /* DLT_PKTAP */
			if ((kflag || Pflag) && pcap_apple_set_exthdr(pd, on) == -1)
				warning("%s", pcap_geterr(pd));
#endif /* __APPLE__ */
		i = pcap_snapshot(pd);
		if (gndo->ndo_snaplen < i) {
			warning("snaplen raised from %d to %d", gndo->ndo_snaplen, i);
			gndo->ndo_snaplen = i;
		}
		if (pcap_lookupnet(device, &localnet, &netmask, ebuf) < 0) {
			localnet = 0;
			netmask = 0;
#if __APPLE__
			if (!no_loopkupnet_warning)
				warning("%s", ebuf);
#else
			warning("%s", ebuf);
#endif /* __APPLE__ */
		}
	}
	if (infile)
		filter_src_buf = read_infile(infile);
	else
		filter_src_buf = copy_argv(&argv[optind]);

#if defined(DLT_PCAPNG) && defined(DLT_PKTAP)
	/*
	 * For PKTAP and PCAPNG the filter expression is compiled
	 * whenever a new interface is discovered
	 */
	type = pcap_datalink(pd);
	if (type == DLT_PCAPNG || type == DLT_PKTAP) {
		if (pcap_set_filter_info(pd, filter_src_buf, Oflag, netmask) < 0)
			error("%s", pcap_geterr(pd));
	} else
#endif /* DLT_PCAPNG && DLT_PKTAP */
		if (pcap_compile(pd, &fcode, filter_src_buf, Oflag, netmask) < 0)
			error("%s", pcap_geterr(pd));

	if (dflag) {
		bpf_dump(&fcode, dflag);
		pcap_close(pd);
		exit(0);
	}
	init_addrtoname(localnet, netmask);
        init_checksum();

#ifndef WIN32	
	(void)setsignal(SIGPIPE, cleanup);
	(void)setsignal(SIGTERM, cleanup);
	(void)setsignal(SIGINT, cleanup);
#endif /* WIN32 */
#ifdef __APPLE__
    /*
     * The default action for SIGQUIT is to create a core dump and that is
     * generating a lot of useless crash tracer reports.
     */
	(void)setsignal(SIGQUIT, cleanup);
	(void)setsignal(SIGABRT, cleanup);
#endif /* __APPLE__ */
#if defined(HAVE_FORK) || defined(HAVE_VFORK)
	(void)setsignal(SIGCHLD, child_cleanup);
#endif
	/* Cooperate with nohup(1) */
#ifndef WIN32	
	if ((oldhandler = setsignal(SIGHUP, cleanup)) != SIG_DFL)
		(void)setsignal(SIGHUP, oldhandler);
#endif /* WIN32 */
    
#ifndef WIN32
	/*
	 * If a user name was specified with "-Z", attempt to switch to
	 * that user's UID.  This would probably be used with sudo,
	 * to allow tcpdump to be run in a special restricted
	 * account (if you just want to allow users to open capture
	 * devices, and can't just give users that permission,
	 * you'd make tcpdump set-UID or set-GID).
	 *
	 * Tcpdump doesn't necessarily write only to one savefile;
	 * the general only way to allow a -Z instance to write to
	 * savefiles as the user under whose UID it's run, rather
	 * than as the user specified with -Z, would thus be to switch
	 * to the original user ID before opening a capture file and
	 * then switch back to the -Z user ID after opening the savefile.
	 * Switching to the -Z user ID only after opening the first
	 * savefile doesn't handle the general case.
	 */
	if (getuid() == 0 || geteuid() == 0) {
		if (username || chroot_dir)
			droproot(username, chroot_dir);
	}
#endif /* WIN32 */

#if defined(DLT_PCAPNG) && defined(DLT_PKTAP)
	type = pcap_datalink(pd);
	if (type != DLT_PCAPNG && type != DLT_PKTAP)
#endif /* DLT_PCAPNG && DLT_PKTAP */
		if (pcap_setfilter(pd, &fcode) < 0)
			error("%s", pcap_geterr(pd));

	if (WFileName) {
		pcap_dumper_t *p;
		/* Do not exceed the default NAME_MAX for files. */
		dumpinfo.CurrentFileName = (char *)malloc(NAME_MAX + 1);

		if (dumpinfo.CurrentFileName == NULL)
			error("malloc of dumpinfo.CurrentFileName");

		/* We do not need numbering for dumpfiles if Cflag isn't set. */
		if (Cflag != 0)
		  MakeFilename(dumpinfo.CurrentFileName, WFileName, 0, WflagChars);
		else
		  MakeFilename(dumpinfo.CurrentFileName, WFileName, 0, 0);

#ifdef __APPLE__
		if (Pflag)
			p = pcap_ng_dump_open(pd, dumpinfo.CurrentFileName);
		else
#endif /* __APPLE__ */
			p = pcap_dump_open(pd, dumpinfo.CurrentFileName);
		if (p == NULL)
			error("%s", pcap_geterr(pd));

		if (Cflag != 0 || Gflag != 0)
			dumpinfo.WFileName = WFileName;

		callback = dump_packet;
		dumpinfo.pcap = pd;
		dumpinfo.dumper = p;
#if defined(DLT_PCAPNG) && defined(DLT_PKTAP)
		if (type == DLT_PKTAP)
			dumpinfo.dumper_func = handle_pktap_dump;
		else if (type == DLT_PCAPNG)
			dumpinfo.dumper_func = handle_pcap_ng_dump;
		else
#endif /* DLT_PCAPNG && DLT_PKTAP */
#ifdef __APPLE__
		if (Pflag)
			dumpinfo.dumper_func = handle_bpf_exthdr_dump;
		else
#endif /* __APPLE__ */
			dumpinfo.dumper_func = handle_pcap_dump;
		pcap_userdata = (u_char *)&dumpinfo;

#ifdef HAVE_PCAP_DUMP_FLUSH
		if (Uflag)
			pcap_dump_flush(p);
#endif
	} else {
		type = pcap_datalink(pd);
		callback = print_callback;
		printinfo.ndo_type = 1;
		printinfo.ndo = gndo;
		printinfo.p.ndo_printer = lookup_ndo_printer(type);
		if (printinfo.p.ndo_printer == NULL) {
			printinfo.p.printer = lookup_printer(type);
			printinfo.ndo_type = 0;
			if (printinfo.p.printer == NULL) {
				gndo->ndo_dltname = pcap_datalink_val_to_name(type);
				if (gndo->ndo_dltname != NULL)
					error("packet printing is not supported for link type %s: use -w",
						  gndo->ndo_dltname);
				else
					error("packet printing is not supported for link type %d: use -w", type);
			}
		}
		printinfo.pcap = pd;
#ifdef DLT_PCAPNG
		if (type == DLT_PCAPNG)
			printinfo.printer_func = print_pcap_ng_block;
		else
#endif /* DLT_PCAPNG */
#ifdef DLT_PKTAP
		if (type == DLT_PKTAP)
			printinfo.printer_func = print_pktap_packet;
		else
#endif /* DLT_PKTAP */
			printinfo.printer_func = print_packet;
		pcap_userdata = (u_char *)&printinfo;
		gndo->ndo_pcap = pd;
	}

#ifdef SIGNAL_REQ_INFO
	/*
	 * We can't get statistics when reading from a file rather
	 * than capturing from a device.
	 */
	if (RFileName == NULL)
		(void)setsignal(SIGNAL_REQ_INFO, requestinfo);
#endif

	if (vflag > 0 && WFileName) {
		/*
		 * When capturing to a file, "-v" means tcpdump should,
		 * every 10 seconds, "v"erbosely report the number of
		 * packets captured.
		 */
#ifdef USE_WIN32_MM_TIMER
		/* call verbose_stats_dump() each 1000 +/-100msec */
		timer_id = timeSetEvent(1000, 100, verbose_stats_dump, 0, TIME_PERIODIC);
		setvbuf(stderr, NULL, _IONBF, 0);
#elif defined(HAVE_ALARM)
		(void)setsignal(SIGALRM, verbose_stats_dump);
		alarm(1);
#endif
	}

#ifndef WIN32
	if (RFileName == NULL) {
		int dlt;
		const char *dlt_name;

		if (!vflag && !WFileName) {
			(void)fprintf(stderr,
			    "%s: verbose output suppressed, use -v or -vv for full protocol decode\n",
			    program_name);
		} else
			(void)fprintf(stderr, "%s: ", program_name);
		dlt = pcap_datalink(pd);
		dlt_name = pcap_datalink_val_to_name(dlt);
		if (dlt_name == NULL) {
			(void)fprintf(stderr, "listening on %s, link-type %u, capture size %u bytes\n",
			    device, dlt, gndo->ndo_snaplen);
		} else {
			(void)fprintf(stderr, "listening on %s, link-type %s (%s), capture size %u bytes\n",
			    device, dlt_name,
			    pcap_datalink_val_to_description(dlt), gndo->ndo_snaplen);
		}
		(void)fflush(stderr);
	}
#endif /* WIN32 */
	status = pcap_loop(pd, -1, callback, pcap_userdata);
	if (WFileName == NULL) {
		/*
		 * We're printing packets.  Flush the printed output,
		 * so it doesn't get intermingled with error output.
		 */
		if (status == -2) {
			/*
			 * We got interrupted, so perhaps we didn't
			 * manage to finish a line we were printing.
			 * Print an extra newline, just in case.
			 */
			putchar('\n');
		}
		(void)fflush(stdout);
	} else {
		if (Pflag)
			pcap_ng_dump_close(dumpinfo.dumper);
		else
			pcap_dump_close(dumpinfo.dumper);
	}
	if (status == -1) {
		/*
		 * Error.  Report it.
		 */
		(void)fprintf(stderr, "%s: pcap_loop: %s\n",
		    program_name, pcap_geterr(pd));
	}
	if (RFileName == NULL) {
		/*
		 * We're doing a live capture.  Report the capture
		 * statistics.
		 */
		info(1);
	}
	pcap_close(pd);
	exit(status == -1 ? 1 : 0);
}

/* make a clean exit on interrupts */
static RETSIGTYPE
cleanup(int signo _U_)
{
#ifdef USE_WIN32_MM_TIMER
	if (timer_id)
		timeKillEvent(timer_id);
	timer_id = 0;
#elif defined(HAVE_ALARM)
	alarm(0);
#endif

#ifdef HAVE_PCAP_BREAKLOOP
	/*
	 * We have "pcap_breakloop()"; use it, so that we do as little
	 * as possible in the signal handler (it's probably not safe
	 * to do anything with standard I/O streams in a signal handler -
	 * the ANSI C standard doesn't say it is).
	 */
	pcap_breakloop(pd);
#else
	/*
	 * We don't have "pcap_breakloop()"; this isn't safe, but
	 * it's the best we can do.  Print the summary if we're
	 * not reading from a savefile - i.e., if we're doing a
	 * live capture - and exit.
	 */
	if (pd != NULL && pcap_file(pd) == NULL) {
		/*
		 * We got interrupted, so perhaps we didn't
		 * manage to finish a line we were printing.
		 * Print an extra newline, just in case.
		 */
		putchar('\n');
		(void)fflush(stdout);
		info(1);
	}
	exit(0);
#endif
}

/*
  On windows, we do not use a fork, so we do not care less about
  waiting a child processes to die
 */
#if defined(HAVE_FORK) || defined(HAVE_VFORK)
static RETSIGTYPE
child_cleanup(int signo _U_)
{
  wait(NULL);
}
#endif /* HAVE_FORK && HAVE_VFORK */

static void
info(register int verbose)
{
	struct pcap_stat stat;

	/*
	 * Older versions of libpcap didn't set ps_ifdrop on some
	 * platforms; initialize it to 0 to handle that.
	 */
	stat.ps_ifdrop = 0;
	if (pcap_stats(pd, &stat) < 0) {
		(void)fprintf(stderr, "pcap_stats: %s\n", pcap_geterr(pd));
		infoprint = 0;
		return;
	}

	if (!verbose)
		fprintf(stderr, "%s: ", program_name);

	(void)fprintf(stderr, "%lu packet%s captured", packets_captured,
	    PLURAL_SUFFIX(packets_captured));
	if (!verbose)
		fputs(", ", stderr);
	else
		putc('\n', stderr);
	(void)fprintf(stderr, "%u packet%s received by filter", stat.ps_recv,
	    PLURAL_SUFFIX(stat.ps_recv));
	if (!verbose)
		fputs(", ", stderr);
	else
		putc('\n', stderr);
	(void)fprintf(stderr, "%u packet%s dropped by kernel", stat.ps_drop,
	    PLURAL_SUFFIX(stat.ps_drop));
	if (packets_mtdt_fltr_drop != 0) {
		if (!verbose)
			fputs(", ", stderr);
		else
			putc('\n', stderr);
		(void)fprintf(stderr, "%u drop%s by metadata filter", packets_mtdt_fltr_drop,
					  PLURAL_SUFFIX(packets_mtdt_fltr_drop));
	}
	if (stat.ps_ifdrop != 0) {
		if (!verbose)
			fputs(", ", stderr);
		else
			putc('\n', stderr);
		(void)fprintf(stderr, "%u packet%s dropped by interface\n",
		    stat.ps_ifdrop, PLURAL_SUFFIX(stat.ps_ifdrop));
	} else
		putc('\n', stderr);
	infoprint = 0;
}

#if defined(HAVE_FORK) || defined(HAVE_VFORK)
static void
compress_savefile(const char *filename)
{
# ifdef HAVE_FORK
	if (fork())
# else
	if (vfork())
# endif
		return;
	/*
	 * Set to lowest priority so that this doesn't disturb the capture
	 */
#ifdef NZERO
	setpriority(PRIO_PROCESS, 0, NZERO - 1);
#else
	setpriority(PRIO_PROCESS, 0, 19);
#endif
	if (execlp(zflag, zflag, filename, (char *)NULL) == -1)
		fprintf(stderr,
			"compress_savefile:execlp(%s, %s): %s\n",
			zflag,
			filename,
			strerror(errno));
# ifdef HAVE_FORK
	exit(1);
# else
	_exit(1);
# endif
}
#else  /* HAVE_FORK && HAVE_VFORK */
static void
compress_savefile(const char *filename)
{
	fprintf(stderr,
		"compress_savefile failed. Functionality not implemented under your system\n");
}
#endif /* HAVE_FORK && HAVE_VFORK */

static void
dump_packet(u_char *user, const struct pcap_pkthdr *h, const u_char *sp)
{
	struct dump_info *dump_info;

	++infodelay;

	dump_info = (struct dump_info *)user;
	
	if (dump_info->WFileName && (Cflag != 0 || Gflag != 0)) {
		/*
		 * XXX - this won't force the file to rotate on the specified time
		 * boundary, but it will rotate on the first packet received after the
		 * specified Gflag number of seconds. Note: if a Gflag time boundary
		 * and a Cflag size boundary coincide, the time rotation will occur
		 * first thereby cancelling the Cflag boundary (since the file should
		 * be 0).
		 */
		if (Gflag != 0) {
			/* Check if it is time to rotate */
			time_t t;
			
			/* Get the current time */
			if ((t = time(NULL)) == (time_t)-1) {
				error("dump_and_trunc_packet: can't get current_time: %s",
					  pcap_strerror(errno));
			}
			
			
			/* If the time is greater than the specified window, rotate */
			if (t - Gflag_time >= Gflag) {
				/* Update the Gflag_time */
				Gflag_time = t;
				/* Update Gflag_count */
				Gflag_count++;
				/*
				 * Close the current file and open a new one.
				 */
				if (Pflag)
					pcap_ng_dump_close(dump_info->dumper);
				else
					pcap_dump_close(dump_info->dumper);
				
				/*
				 * Compress the file we just closed, if the user asked for it
				 */
				if (zflag != NULL)
					compress_savefile(dump_info->CurrentFileName);
				
				/*
				 * Check to see if we've exceeded the Wflag (when
				 * not using Cflag).
				 */
				if (Cflag == 0 && Wflag > 0 && Gflag_count >= Wflag) {
					(void)fprintf(stderr, "Maximum file limit reached: %d\n",
								  Wflag);
					exit(0);
					/* NOTREACHED */
				}
				if (dump_info->CurrentFileName != NULL)
					free(dump_info->CurrentFileName);
				/* Allocate space for max filename + \0. */
				dump_info->CurrentFileName = (char *)malloc(NAME_MAX + 1);
				if (dump_info->CurrentFileName == NULL)
					error("dump_packet_and_trunc: malloc");
				/*
				 * This is always the first file in the Cflag
				 * rotation: e.g. 0
				 * We also don't need numbering if Cflag is not set.
				 */
				if (Cflag != 0)
					MakeFilename(dump_info->CurrentFileName, dump_info->WFileName, 0,
								 WflagChars);
				else
					MakeFilename(dump_info->CurrentFileName, dump_info->WFileName, 0, 0);
				
				if (Pflag)
					dump_info->dumper = pcap_ng_dump_open(dump_info->pcap, dump_info->CurrentFileName);
				else
					dump_info->dumper = pcap_dump_open(dump_info->pcap, dump_info->CurrentFileName);
				if (dump_info->dumper == NULL)
					error("%s", pcap_geterr(pd));
			}
		}
		
		/*
		 * XXX - this won't prevent capture files from getting
		 * larger than Cflag - the last packet written to the
		 * file could put it over Cflag.
		 */
		if (Cflag != 0 && pcap_dump_ftell(dump_info->dumper) > Cflag) {
			/*
			 * Close the current file and open a new one.
			 */
			if (Pflag)
				pcap_ng_dump_close(dump_info->dumper);
			else
				pcap_dump_close(dump_info->dumper);
			
			/*
			 * Compress the file we just closed, if the user asked for it
			 */
			if (zflag != NULL)
				compress_savefile(dump_info->CurrentFileName);
			
			Cflag_count++;
			if (Wflag > 0) {
				if (Cflag_count >= Wflag)
					Cflag_count = 0;
			}
			if (dump_info->CurrentFileName != NULL)
				free(dump_info->CurrentFileName);
			dump_info->CurrentFileName = (char *)malloc(NAME_MAX + 1);
			if (dump_info->CurrentFileName == NULL)
				error("dump_packet_and_trunc: malloc");
			MakeFilename(dump_info->CurrentFileName, dump_info->WFileName, Cflag_count, WflagChars);
			if (Pflag)
				dump_info->dumper = pcap_ng_dump_open(dump_info->pcap, dump_info->CurrentFileName);
			else
				dump_info->dumper = pcap_dump_open(dump_info->pcap, dump_info->CurrentFileName);
			if (dump_info->dumper == NULL)
				error("%s", pcap_geterr(pd));
		}
	}
	
	if (dump_info->dumper_func(dump_info, h, sp) == 1)
		packets_captured++;

#ifdef HAVE_PCAP_DUMP_FLUSH
	if (Uflag)
		pcap_dump_flush(dump_info->dumper);
#endif

	--infodelay;
	if (infoprint)
		info(0);
		
 	if (packets_captured >= max_packet_cnt)
		pcap_breakloop(dump_info->pcap);
}

char *
svc2str(uint32_t svc)
{
	static char svcstr[10];
	
	switch (svc) {
		case SO_TC_BK_SYS:
			return "BK_SYS";
		case SO_TC_BK:
			return "BK";
		case SO_TC_BE:
			return "BE";
		case SO_TC_RD:
			return "RD";
		case SO_TC_OAM:
			return "OAM";
		case SO_TC_AV:
			return "AV";
		case SO_TC_RV:
			return "RV";
		case SO_TC_VI:
			return "VI";
		case SO_TC_VO:
			return "VO";
		case SO_TC_CTL:
			return "CTL";
		default:
			snprintf(svcstr, sizeof(svcstr), "%u", svc);
			return svcstr;
	}
}

int
handle_pcap_dump(struct dump_info *dump_info, const struct pcap_pkthdr *h,
				   const u_char *sp)
{
	pcap_dump((u_char *)dump_info->dumper, h, sp);
	
	return (1);
}

int
handle_bpf_exthdr_dump(struct dump_info *dump_info, const struct pcap_pkthdr *h,
					const u_char *sp)
{
	pcap_ng_dump((u_char *)dump_info->dumper, h, sp);

	return (1);
}

int
handle_pcap_ng_dump(struct dump_info *dump_info, const struct pcap_pkthdr *h,
					const u_char *sp)
{
	static pcapng_block_t block = NULL;
	struct pcap_if_info *if_info = NULL;
	uint32_t if_id;
	u_short pack_flags_code = 0;
	u_char *pkt_data;
	struct pcap_proc_info *proc_info = NULL;
	struct pcap_proc_info *e_proc_info = NULL;
	uint32_t pkt_svc = -1;
	uint32_t packet_flags = 0;
	struct pcapng_option_info option_info;
	int result = 0;
	
	if (block == NULL) {
		block = pcap_ng_block_alloc(65536);
		if (block == NULL) {
			error("%s: pcap_ng_block_alloc() no memory", __func__);
		}
	}
	
	if (pcap_ng_block_init_with_raw_block(block, dump_info->pcap, (u_char *)sp)) {
		warning("%s: pcap_ng_block_init_with_raw_block() ", __func__);
		goto done;
	}
	
	switch (pcap_ng_block_get_type(block)) {
		case PCAPNG_BT_SHB: {
			pcap_clear_if_infos(dump_info->pcap);
			
			pcap_ng_dump_block(dump_info->dumper, block);
			
			goto done;
		}
		case PCAPNG_BT_IDB: {
			struct pcapng_interface_description_fields *idbp =
			pcap_ng_get_interface_description_fields(block);
			const char *ifname = "";
			
			if (pcap_ng_block_get_option(block, PCAPNG_IF_NAME, &option_info) == 1)
				ifname = (const char *)option_info.value;
			
			if_info = pcap_add_if_info(dump_info->pcap, ifname, -1, idbp->linktype, idbp->snaplen);
			if (if_info == NULL)
				error("%s: cannot allocate memory", __func__);
			
			pcap_ng_dump_block(dump_info->dumper, block);
			
			goto done;
		}
		case PCAPNG_BT_PIB: {
			struct pcapng_process_information_fields *pibp =
			pcap_ng_get_process_information_fields(block);
			const char *procname = "";
			
			if (pcap_ng_block_get_option(block, PCAPNG_PIB_NAME, &option_info) == 1)
				procname = option_info.value;
			
			(void) pcap_add_proc_info(dump_info->pcap, pibp->process_id, procname);
			
			pcap_ng_dump_block(dump_info->dumper, block);
			
			goto done;
		}
		case PCAPNG_BT_EPB: {
			struct pcapng_enhanced_packet_fields *epbp = pcap_ng_get_enhanced_packet_fields(block);
			
			if (pcap_ng_block_get_option(block, PCAPNG_EPB_PIB_INDEX, &option_info) == 1) {
				uint32_t pibindex;
				
				if (option_info.length != 4) {
					error("%s: pib index option length %u != 4", __func__, option_info.length);
					abort();
				}
				pibindex = *(uint32_t *)(option_info.value);
				if (pcap_is_swapped(dump_info->pcap))
					pibindex = SWAPLONG(pibindex);
				
				proc_info = pcap_find_proc_info_by_index(dump_info->pcap, pibindex);
			}
			if (pcap_ng_block_get_option(block, PCAPNG_EPB_E_PIB_INDEX, &option_info) == 1) {
				uint32_t pibindex;
				
				if (option_info.length != 4) {
					error("%s: e_pib index option length %u != 4", __func__, option_info.length);
					abort();
				}
				pibindex = *(uint32_t *)(option_info.value);
				if (pcap_is_swapped(dump_info->pcap))
					pibindex = SWAPLONG(pibindex);
				
				e_proc_info = pcap_find_proc_info_by_index(dump_info->pcap, pibindex);
			}
			if (pcap_ng_block_get_option(block, PCAPNG_EPB_SVC, &option_info) == 1) {
				if (option_info.length != 4) {
					error("%s: svc option length %u != 4", __func__, option_info.length);
					abort();
				}
				pkt_svc = *(uint32_t *)(option_info.value);
				if (pcap_is_swapped(dump_info->pcap))
					pkt_svc = SWAPLONG(pkt_svc);
			}
			
			if_id = epbp->interface_id;
			
			pack_flags_code = PCAPNG_EPB_FLAGS;
			
			break;
		}
		case PCAPNG_BT_SPB: {
			
			if_id = 0;
			
			pack_flags_code = PCAPNG_PACK_FLAGS;
			
			break;
		}
		case PCAPNG_BT_PB: {
			struct pcapng_packet_fields *pbp = pcap_ng_get_packet_fields(block);
			
			if_id = pbp->interface_id;
			
			break;
		}
		default:
			goto done;
	}
	
	/*
	 * Here we have a packet
	 */
	pkt_data = pcap_ng_block_packet_get_data_ptr(block);
	
	if_info = pcap_find_if_info_by_id(dump_info->pcap, if_id);
	if (if_info == NULL) {
		error("%s: unknown interface id %u", __func__, if_id);
		abort();
	}
	
	/*
	 * Evaluate the per-interface BPF filter expression
	 */
	if (if_info->if_filter_program.bf_insns != NULL &&
		pcap_offline_filter(&if_info->if_filter_program, h, pkt_data) == 0)
		goto done;
	
	if (pcap_ng_block_get_option(block, pack_flags_code, &option_info) == 1) {
		if (option_info.length != 4) {
			error("%s: pack_flags option length %u != 4", __func__, option_info.length);
			abort();
		}
		bcopy(option_info.value, &packet_flags, sizeof(packet_flags));
		
		if (pcap_is_swapped(dump_info->pcap))
			packet_flags = SWAPLONG(packet_flags);
	}
	
	/*
	 * Evaluate the packet metadata expression
	 */
	if (pkt_meta_data_expression != NULL) {
		struct pkt_meta_data pmd;
		
		pmd.itf = &if_info->if_name[0];
		pmd.proc = (proc_info != NULL) ? proc_info->proc_name : "";
		pmd.eproc = (e_proc_info != NULL) ? e_proc_info->proc_name : "";
		pmd.pid = (proc_info != NULL) ? proc_info->proc_pid : -1;
		pmd.epid = (e_proc_info != NULL) ? e_proc_info->proc_pid : -1;
		pmd.svc = (pkt_svc != -1) ? svc2str(pkt_svc) : "";
		pmd.dir =  (packet_flags & 3) == 2 ? "out" :
		(packet_flags & 3) == 1 ? "in" : "";
		
		if (evaluate_expression(pkt_meta_data_expression, &pmd) == 0) {
			packets_mtdt_fltr_drop++;
			goto done;
		}
	}
	
	pcap_ng_dump_block(dump_info->dumper, block);

	result = 1;

done:
	return (result);
}

int
handle_pktap_dump(struct dump_info *dump_info, const struct pcap_pkthdr *h,
				  const u_char *sp)
{
	if (pktap_filter_packet(dump_info->pcap, NULL, h, sp) == 0)
		return (0);
	
	if (pcap_ng_dump_pktap(dump_info->pcap, dump_info->dumper, h, sp) == 0)
		return (0);
	
	return (1);
}

static void
print_callback(u_char *user, const struct pcap_pkthdr *h, const u_char *sp)
{
	struct print_info *print_info = (struct print_info *)user;
	
	++infodelay;
	
	print_info->printer_func(print_info, h, sp);

	--infodelay;
	if (infoprint)
		info(0);
	
	if (packets_captured >= max_packet_cnt)
		pcap_breakloop(print_info->pcap);
}

static void
print_raw_packet_data(const struct pcap_pkthdr *h, const u_char *sp, u_int hdrlen)
{
	if (Xflag) {
		/*
		 * Print the raw packet data in hex and ASCII.
		 */
		if (Xflag > 1) {
			/*
			 * Include the link-layer header.
			 */
			hex_and_ascii_print("\n\t", sp, h->caplen);
		} else {
			/*
			 * Don't include the link-layer header - and if
			 * we have nothing past the link-layer header,
			 * print nothing.
			 */
			if (h->caplen > hdrlen)
				hex_and_ascii_print("\n\t", sp + hdrlen,
									h->caplen - hdrlen);
		}
	} else if (xflag) {
		/*
		 * Print the raw packet data in hex.
		 */
		if (xflag > 1) {
			/*
			 * Include the link-layer header.
			 */
			hex_print("\n\t", sp, h->caplen);
		} else {
			/*
			 * Don't include the link-layer header - and if
			 * we have nothing past the link-layer header,
			 * print nothing.
			 */
			if (h->caplen > hdrlen)
				hex_print("\n\t", sp + hdrlen,
						  h->caplen - hdrlen);
		}
	} else if (Aflag) {
		/*
		 * Print the raw packet data in ASCII.
		 */
		if (Aflag > 1) {
			/*
			 * Include the link-layer header.
			 */
			ascii_print(sp, h->caplen);
		} else {
			/*
			 * Don't include the link-layer header - and if
			 * we have nothing past the link-layer header,
			 * print nothing.
			 */
			if (h->caplen > hdrlen)
				ascii_print(sp + hdrlen, h->caplen - hdrlen);
		}
	}
}

static int
print_packet(struct print_info *print_info, const struct pcap_pkthdr *h, const u_char *sp)
{
	u_int hdrlen;
	
	packets_captured++;
	
	if (Vflag)
		printf("%5lu ", packets_captured);
	
	ts_print(&h->ts);
#ifdef __APPLE__
	if (kflag && h->comment[0])
		printf("%s ", h->comment);
#endif
	
	/*
	 * Some printers want to check that they're not walking off the
	 * end of the packet.
	 * Rather than pass it all the way down, we set this global.
	 */
	snapend = sp + h->caplen;
	
	if(print_info->ndo_type) {
		hdrlen = (*print_info->p.ndo_printer)(print_info->ndo, h, sp);
	} else {
		hdrlen = (*print_info->p.printer)(h, sp);
	}
    
	print_raw_packet_data(h, sp, hdrlen);

	putchar('\n');
	
	return (1);
}

#ifdef __APPLE__
static int
print_pktap_packet(struct print_info *print_info, const struct pcap_pkthdr *h, const u_char *sp)
{
	if (pktap_filter_packet(print_info->pcap, NULL, h, sp) == 0)
		return (0);
	
	print_packet(print_info, h, sp);
	
	return (1);
}

#ifdef DLT_PCAPNG

static int
print_pcap_ng_block(struct print_info *print_info, const struct pcap_pkthdr *h, const u_char *sp)
{
	pcapng_block_t block;
	struct pcap_if_info *if_info = NULL;
	uint32_t if_id;
	if_ndo_printer ndo_printer;
	if_printer printer;
	u_short pack_flags_code = 0;
	u_char *pkt_data;
	netdissect_options *ndo = print_info->ndo;
	u_int hdrlen = 0;
	struct pcap_proc_info *proc_info = NULL;
	struct pcap_proc_info *e_proc_info = NULL;
	uint32_t pkt_svc = -1;
	uint32_t packet_flags = 0;
    struct pcapng_option_info option_info;
	int result = 0;

	block = pcap_ng_block_alloc_with_raw_block(print_info->pcap, (u_char *)sp);
	if (block == NULL) {
		warning("%s: unknown PCAP-NG block type", __func__);
		goto done;
	}
	
	switch (pcap_ng_block_get_type(block)) {
		case PCAPNG_BT_SHB: {
			struct pcapng_section_header_fields *shbp = pcap_ng_get_section_header_fields(block);
			
			pcap_clear_if_infos(print_info->pcap);
			if (vflag) {
				printf("Section Header Block version %u.%u",
					   shbp->major_version, shbp->minor_version);
				if (shbp->section_length == (u_int64_t)-1)
					printf(", section_length: -1");
				else
					printf(", section_length: %llu", shbp->section_length);

				if (pcap_ng_block_get_option(block, PCAPNG_SHB_HARDWARE, &option_info) == 1)
					printf(", hardware: %s", (const char *)option_info.value);
				
				if (pcap_ng_block_get_option(block, PCAPNG_SHB_OS, &option_info) == 1)
					printf(", OS: %s", (const char *)option_info.value);

				if (pcap_ng_block_get_option(block, PCAPNG_SHB_USERAPPL, &option_info) == 1)
					printf(", app: %s", (const char *)option_info.value);
			
				printf("\n");
			}
			goto done;
		}
		case PCAPNG_BT_IDB: {
			struct pcapng_interface_description_fields *idbp =
				pcap_ng_get_interface_description_fields(block);
			const char *ifname = "";
			
            if (pcap_ng_block_get_option(block, PCAPNG_IF_NAME, &option_info) == 1)
                ifname = (const char *)option_info.value;
            
			if_info = pcap_add_if_info(print_info->pcap, ifname, -1, idbp->linktype, idbp->snaplen);
			if (if_info == NULL)
				error("%s: cannot allocate memory", __func__);
			
			if (vflag)
				printf("Interface Description Block id: %d name: %s linktype: %u snaplen: %u\n",
					   if_info->if_id, if_info->if_name, if_info->if_linktype,
					   if_info->if_snaplen);
			
			goto done;
		}
		case PCAPNG_BT_PIB: {
			struct pcapng_process_information_fields *pibp =
				pcap_ng_get_process_information_fields(block);
			const char *procname = "";

            if (pcap_ng_block_get_option(block, PCAPNG_PIB_NAME, &option_info) == 1)
                procname = option_info.value;

			proc_info = pcap_add_proc_info(print_info->pcap, pibp->process_id, procname);
			
			if (vflag)
				printf("Process Information Block pid: %u proc_name: %s\n",
					   proc_info->proc_pid, proc_info->proc_name);
			goto done;
		}
		case PCAPNG_BT_EPB: {
			struct pcapng_enhanced_packet_fields *epbp = pcap_ng_get_enhanced_packet_fields(block);

            if (pcap_ng_block_get_option(block, PCAPNG_EPB_PIB_INDEX, &option_info) == 1) {
				uint32_t pibindex;

                if (option_info.length != 4) {
                    error("%s: pib index option length %u != 4", __func__, option_info.length);
                    abort();
                }
				pibindex = *(uint32_t *)(option_info.value);
                if (pcap_is_swapped(print_info->pcap))
					pibindex = SWAPLONG(pibindex);
				
				proc_info = pcap_find_proc_info_by_index(print_info->pcap, pibindex);
			}
            if (pcap_ng_block_get_option(block, PCAPNG_EPB_E_PIB_INDEX, &option_info) == 1) {
				uint32_t pibindex;
				
                if (option_info.length != 4) {
                    error("%s: e_pib index option length %u != 4", __func__, option_info.length);
                    abort();
                }
				pibindex = *(uint32_t *)(option_info.value);
                if (pcap_is_swapped(print_info->pcap))
					pibindex = SWAPLONG(pibindex);
				
				e_proc_info = pcap_find_proc_info_by_index(print_info->pcap, pibindex);
			}
            if (pcap_ng_block_get_option(block, PCAPNG_EPB_SVC, &option_info) == 1) {
                if (option_info.length != 4) {
                    error("%s: svc option length %u != 4", __func__, option_info.length);
                    abort();
                }
				pkt_svc = *(uint32_t *)(option_info.value);
				if (pcap_is_swapped(print_info->pcap))
					pkt_svc = SWAPLONG(pkt_svc);
			}

			if_id = epbp->interface_id;
			
			pack_flags_code = PCAPNG_EPB_FLAGS;
			
			break;
		}
		case PCAPNG_BT_SPB: {
			
			if_id = 0;
			
			pack_flags_code = PCAPNG_PACK_FLAGS;
			
			break;
		}
		case PCAPNG_BT_PB: {
			struct pcapng_packet_fields *pbp = pcap_ng_get_packet_fields(block);
			
			if_id = pbp->interface_id;
			
			break;
		}
		default:
			goto done;
	}
	
	/*
	 * Here we have a packet
	 */
	pkt_data = pcap_ng_block_packet_get_data_ptr(block);
	
	if_info = pcap_find_if_info_by_id(print_info->pcap, if_id);
	if (if_info == NULL) {
		error("%s: unknown interface id %u", __func__, if_id);
		abort();
	}
	
	/*
	 * Evaluate the per-interface BPF filter expression
	 */
	if (if_info->if_filter_program.bf_insns != NULL &&
		pcap_offline_filter(&if_info->if_filter_program, h, pkt_data) == 0)
		goto done;
	
    if (pcap_ng_block_get_option(block, pack_flags_code, &option_info) == 1) {
		if (option_info.length != 4) {
			error("%s: pack_flags option length %u != 4", __func__, option_info.length);
			abort();
		}
        bcopy(option_info.value, &packet_flags, sizeof(packet_flags));

		if (pcap_is_swapped(print_info->pcap))
			packet_flags = SWAPLONG(packet_flags);
	}
	
	/*
	 * Evaluate the packet metadata expression
	 */
	if (pkt_meta_data_expression != NULL) {
		struct pkt_meta_data pmd;
		
		pmd.itf = &if_info->if_name[0];
		pmd.proc = (proc_info != NULL) ? proc_info->proc_name : "";
		pmd.eproc = (e_proc_info != NULL) ? e_proc_info->proc_name : "";
		pmd.pid = (proc_info != NULL) ? proc_info->proc_pid : -1;
		pmd.epid = (e_proc_info != NULL) ? e_proc_info->proc_pid : -1;
		pmd.svc = (pkt_svc != -1) ? svc2str(pkt_svc) : "";
		pmd.dir =  (packet_flags & 3) == 2 ? "out" :
			(packet_flags & 3) == 1 ? "in" : "";
		
		if (evaluate_expression(pkt_meta_data_expression, &pmd) == 0) {
			packets_mtdt_fltr_drop++;
			goto done;
		}
	}

	packets_captured++;
	
	if (Vflag)
		printf("%5lu ", packets_captured);
	
	ts_print(&h->ts);

	/*
	 * Packet metadata
	 */
	if (kflag != PRMD_NONE) {
		const char *prsep = "";

		ND_PRINT((ndo, "("));
		
		/*
		 * Interface name
		 */
		if (kflag & PRMD_IF) {
			if (if_info->if_name && if_info->if_name[0] != 0)
				ND_PRINT((ndo, "%s", if_info->if_name));
			else
				ND_PRINT((ndo, "%d", if_id));
			prsep = ", ";
		}
		
		/*
		 * Process name and/or process ID
		 */
		if (proc_info != NULL) {
			switch ((kflag & (PRMD_PNAME |PRMD_PID))) {
				case (PRMD_PNAME | PRMD_PID):
					ND_PRINT((ndo, "%sproc %s:%u",
							  prsep,
							  proc_info->proc_name ,
							  proc_info->proc_pid));
					prsep = ", ";
					if (e_proc_info != NULL) {
						ND_PRINT((ndo, "%seproc %s:%u",
								  prsep,
								  e_proc_info->proc_name ,
								  e_proc_info->proc_pid));
						prsep = ", ";
					}
					break;
				case PRMD_PNAME:
					ND_PRINT((ndo, "%sproc %s",
							  prsep,
							  proc_info->proc_name));
					prsep = ", ";
					if (e_proc_info != NULL) {
						ND_PRINT((ndo, "%seproc %s",
								  prsep,
								  e_proc_info->proc_name));
					}
					break;
					
				case PRMD_PID:
					ND_PRINT((ndo, "%sproc %u",
							  prsep,
							  proc_info->proc_pid));
					prsep = ", ";
					if (e_proc_info != NULL) {
						ND_PRINT((ndo, "%seproc %u",
								  prsep,
								  e_proc_info->proc_pid));
					}
					break;
					
				default:
					break;
			}
		}
		
		/*
		 * Service class
		 */
		if ((kflag & PRMD_SVC) && pkt_svc != -1) {
			ND_PRINT((ndo, "%ssvc %s",
					  prsep,
					  svc2str(pkt_svc)));
			prsep = ", ";
		}
		
		/*
		 * Direction
		 */
		if ((kflag & PRMD_DIR) && (packet_flags & 3)) {
			if ((packet_flags & 2) == 2)
				ND_PRINT((ndo, "%sout",
						  prsep));
			else if ((packet_flags & 1) == 1)
				ND_PRINT((ndo, "%sin",
						  prsep));
			prsep = ", ";
		}
		
		/*
		 * Comment
		 */
		if (kflag & PRMD_COMMENT) {
            if (pcap_ng_block_get_option(block, PCAPNG_OPT_COMMENT, &option_info) == 1) {
				if (option_info.value != NULL) {
					const char *str_comment = (const char *)option_info.value;
					
					if (str_comment && *str_comment != 0) {
						ND_PRINT((ndo, "%s%s",
								  prsep,
								  str_comment));
						prsep = ", ";
					}
				}
			}
        }
		
		ND_PRINT((ndo, ") "));
	}
	
	/*
	 * Some printers want to check that they're not walking off the
	 * end of the packet.
	 * Rather than pass it all the way down, we set this global.
	 */
	snapend = pkt_data + h->caplen;

	if ((printer = lookup_printer(if_info->if_linktype)) != NULL) {
		hdrlen = printer(h, pkt_data);
	} else if ((ndo_printer = lookup_ndo_printer(if_info->if_linktype)) != NULL) {
		hdrlen = ndo_printer(ndo, h, pkt_data);
	} else {
		if (!ndo->ndo_suppress_default_print)
			ndo->ndo_default_print(ndo, pkt_data, h->caplen);
	}
	print_raw_packet_data(h, pkt_data, hdrlen);

	putchar('\n');

	result = 1;
	
done:
	if (block != NULL)
		pcap_ng_free_block(block);

	return (result);
}

#endif /* DLT_PCAPNG */
#endif /* __APPLE__ */

#ifdef WIN32
	/*
	 * XXX - there should really be libpcap calls to get the version
	 * number as a string (the string would be generated from #defines
	 * at run time, so that it's not generated from string constants
	 * in the library, as, on many UNIX systems, those constants would
	 * be statically linked into the application executable image, and
	 * would thus reflect the version of libpcap on the system on
	 * which the application was *linked*, not the system on which it's
	 * *running*.
	 *
	 * That routine should be documented, unlike the "version[]"
	 * string, so that UNIX vendors providing their own libpcaps
	 * don't omit it (as a couple of vendors have...).
	 *
	 * Packet.dll should perhaps also export a routine to return the
	 * version number of the Packet.dll code, to supply the
	 * "Wpcap_version" information on Windows.
	 */
	char WDversion[]="current-cvs.tcpdump.org";
#if !defined(HAVE_GENERATED_VERSION)
	char version[]="current-cvs.tcpdump.org";
#endif
	char pcap_version[]="current-cvs.tcpdump.org";
	char Wpcap_version[]="3.1";
#endif

/*
 * By default, print the specified data out in hex and ASCII.
 */
static void
ndo_default_print(netdissect_options *ndo _U_, const u_char *bp, u_int length)
{
	hex_and_ascii_print("\n\t", bp, length); /* pass on lf and identation string */
}

void
default_print(const u_char *bp, u_int length)
{
	ndo_default_print(gndo, bp, length);
}

#ifdef SIGNAL_REQ_INFO
RETSIGTYPE requestinfo(int signo _U_)
{
	if (infodelay)
		++infoprint;
	else
		info(0);
}
#endif

/*
 * Called once each second in verbose mode while dumping to file
 */
#ifdef USE_WIN32_MM_TIMER
void CALLBACK verbose_stats_dump (UINT timer_id _U_, UINT msg _U_, DWORD_PTR arg _U_,
				  DWORD_PTR dw1 _U_, DWORD_PTR dw2 _U_)
{
	struct pcap_stat stat;

	if (infodelay == 0 && pcap_stats(pd, &stat) >= 0)
		fprintf(stderr, "Got %u\r", packets_captured);
}
#elif defined(HAVE_ALARM)
static void verbose_stats_dump(int sig _U_)
{
	struct pcap_stat stat;

	if (infodelay == 0 && pcap_stats(pd, &stat) >= 0)
		fprintf(stderr, "Got %lu\r", packets_captured);
	alarm(1);
}
#endif

static void
usage(void)
{
	extern char version[];
#ifndef HAVE_PCAP_LIB_VERSION
#if defined(WIN32) || defined(HAVE_PCAP_VERSION)
	extern char pcap_version[];
#else /* defined(WIN32) || defined(HAVE_PCAP_VERSION) */
	static char pcap_version[] = "unknown";
#endif /* defined(WIN32) || defined(HAVE_PCAP_VERSION) */
#endif /* HAVE_PCAP_LIB_VERSION */

#ifdef HAVE_PCAP_LIB_VERSION
#ifdef WIN32
	(void)fprintf(stderr, "%s version %s, based on tcpdump version %s\n", program_name, WDversion, version);
#else /* WIN32 */
	(void)fprintf(stderr, "%s version %s\n", program_name, version);
#endif /* WIN32 */
	(void)fprintf(stderr, "%s\n",pcap_lib_version());
#else /* HAVE_PCAP_LIB_VERSION */
#ifdef WIN32
	(void)fprintf(stderr, "%s version %s, based on tcpdump version %s\n", program_name, WDversion, version);
	(void)fprintf(stderr, "WinPcap version %s, based on libpcap version %s\n",Wpcap_version, pcap_version);
#else /* WIN32 */
	(void)fprintf(stderr, "%s version %s\n", program_name, version);
	(void)fprintf(stderr, "libpcap version %s\n", pcap_version);
#endif /* WIN32 */
#endif /* HAVE_PCAP_LIB_VERSION */
	(void)fprintf(stderr,
"Usage: %s [-aAbd" D_FLAG "efhH" g_FLAG I_FLAG J_FLAG "kKlLnNOpPq" Q_FLAG "RStu" U_FLAG "vxX]" B_FLAG_USAGE " [ -c count ]\n", program_name);
	(void)fprintf(stderr,
"\t\t[ -C file_size ] [ -E algo:secret ] [ -F file ] [ -G seconds ]\n");
	(void)fprintf(stderr,
"\t\t[ -i interface ]" j_FLAG_USAGE " [ -M secret ]\n");
#if __APPLE__
	(void)fprintf(stderr,
"\t\t[ -Q metadata-filter-expression ]\n");
#endif /* __APPLE__ */
	(void)fprintf(stderr,
"\t\t[ -r file ] [ -s snaplen ] [ -T type ] [ -w file ]\n");
	(void)fprintf(stderr,
"\t\t[ -W filecount ] [ -y datalinktype ] [ -z command ]\n");
	(void)fprintf(stderr,
"\t\t[ -Z user ] [ expression ]\n");
	exit(1);
}

/* VARARGS */
static void
ndo_error(netdissect_options *ndo _U_, const char *fmt, ...)
{
	va_list ap;

	(void)fprintf(stderr, "%s: ", program_name);
	va_start(ap, fmt);
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	if (*fmt) {
		fmt += strlen(fmt);
		if (fmt[-1] != '\n')
			(void)fputc('\n', stderr);
	}
	exit(1);
	/* NOTREACHED */
}

/* VARARGS */
static void
ndo_warning(netdissect_options *ndo _U_, const char *fmt, ...)
{
	va_list ap;

	(void)fprintf(stderr, "%s: WARNING: ", program_name);
	va_start(ap, fmt);
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	if (*fmt) {
		fmt += strlen(fmt);
		if (fmt[-1] != '\n')
			(void)fputc('\n', stderr);
	}
}
