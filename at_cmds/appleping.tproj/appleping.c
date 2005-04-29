/*
 *	Copyright (c) 1988, 1989 Apple Computer, Inc. 
 *
 *	The information contained herein is subject to change without
 *	notice and  should not be  construed as a commitment by Apple
 *	Computer, Inc. Apple Computer, Inc. assumes no responsibility
 *	for any errors that may appear.
 *
 *	Confidential and Proprietary to Apple Computer, Inc.
 */

/* appleping.c: 2.0, 1.6; 7/18/89; Copyright 1988-89, Apple Computer, Inc. */

/*
 * Title:	appleping.c - Ping an AppleTalk Echo Protocol listener
 *
 * Author:	Gregory Burns, Creation Date: Jun-22-1988
 *
 *	This program is based on the InterNet Control Message Protocol (ICMP)
 *	echo program, ping.c, by Mike Muuss of the U. S. Army Ballistic Research
 *	Laboratory, December, 1983; and by UC Berkeley.  The original code is 
 *	public domainand has unlimited distribution.  
 *
 *	AppleTalk modifications are Copyright (c) Apple Computer, Inc.
 *
 */
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <stdio.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/sockio.h>
#include <sys/select.h>
#include <sys/param.h>
#include <sys/signal.h>
#include <net/if.h>

#include <netat/appletalk.h>
#include <netat/ddp.h>
#include <netat/ep.h>

#include <AppleTalk/at_proto.h>

#define MSGSTR(num,str)		str

#define	MAXWAIT		10	/* max time to wait for response, sec */

	/* Minimum packet size */
#define	MINPACKET	(DDP_X_HDR_SIZE+1+sizeof(struct timeval))
	/* Maximum packet size */
#define	MAXDATA		DDP_DATA_SIZE
	/* Default packet size */
#define	DEFDATA		(64)
int	verbose;
u_char	packet[MAXDATA+DDP_X_HDR_SIZE];
int	options;
extern	int errno;

int fd, size, flags = 0;
struct sockaddr_at local;
struct sockaddr_at remote;	/* what to ping */

int datalen;			/* How much data */

char *hostname;
char namebuf[64];

int npackets;
int ntransmitted = 0;		/* sequence # for outbound packets = #sent */
int ident;

int nreceived = 0;		/* # of packets we got back */
int timing = 0;
int tmin = 999999999;
int tmax = 0;
int tsum = 0;			/* sum of all times, for doing average */
int hopmin = 999999999;
int hopmax = 0;
int hopsum = 0;			/* sum of all hop counts, for doing average */
static void finish(int), catcher(int);
static void pinger(void);
static void pr_pack(char *buf, int cc);
static void tvsub(register struct timeval *out, register struct timeval *in);
static void finish(int sig);

/*
 * 			M A I N
 */
int
main(argc, argv)
char *argv[];
{
	char **av = argv;
	int i[2];

	setbuf(stdout, 0);

	argc--, av++;
	while (argc > 0 && *av[0] == '-') {
		while (*++av[0]) switch (*av[0]) {
			case 'v':
				verbose++;
				break;
		}
		argc--, av++;
	}
	if (argc < 1)  {
		printf(MSGSTR(M_AP_USAGE, \
			"Usage:  appleping net.node [data size] [npackets]\n"\
		   		"or:  appleping name:type@zone [data size] [npackets]\n"\
		   	"\n"\
		   	"examples:  appleping 'John Doe:Macintosh SE@EndZone' \n"\
				"or:  appleping 6b16.54 \n"));
		exit(1);
	}
		/* check to see if atalk is loaded */
        {
                int error = 0;
                error = checkATStack();
/*              printf ("returned from checkATStack with error = %d\n", error);
*/
                switch (error) {
                        case NOTLOADED:
                                fprintf(stderr, MSGSTR(M_NOT_LOADED,
                                "The AppleTalk stack is not Loaded\n"));
                                break;
                        case LOADED:
                                fprintf(stderr, MSGSTR(M_NOT_LOADED,
                                "The AppleTalk stack is not Running\n"));
                                break;

                        case RUNNING:
                                error =0;
                                break;
                        default:
                                fprintf(stderr, MSGSTR(M_NOT_LOADED,
                                "Other error with The AppleTalk stack\n"));
                }
                if (error != 0)
                        exit(1);
	}

	if (strchr(av[0], ':') != NULL) {
		at_nbptuple_t	tuple;
		at_entity_t	entity;
		int		got;

		if (nbp_parse_entity(&entity, av[0]) < 0) {
			perror(MSGSTR(M_AP_NBP, "appleping: nbp_parse_entity"));
			exit(1);
		}

		if ((got = nbp_lookup(&entity, &tuple, 1, NULL)) < 0) {
/*			perror(MSGSTR(M_AP_LOOK, "appleping: nbp_lookup"));*/
			perror("appleping: nbp_lookup");
			exit(1);
		}

		if (got == 0) {
			fprintf(stderr, MSGSTR(M_AP_FIND,
				"appleping: Can't find %s\n"), av[0]);
			exit(1);
		}

		remote.sat_addr.s_net = tuple.enu_addr.net;
		remote.sat_addr.s_node = tuple.enu_addr.node;

		printf(MSGSTR(M_PING1, "Pinging %04x.%02x.04\n"), 
			remote.sat_addr.s_net, remote.sat_addr.s_node);
	} else {
		sscanf(av[0], "%x.%x", &i[0], &i[1]);

		remote.sat_addr.s_net = i[0];
		remote.sat_addr.s_node = i[1];
	}

	strcpy(namebuf, av[0]);
	hostname = namebuf;

	if (argc >= 2)
		datalen = atoi(av[1]);
	else
		datalen = DEFDATA;
	datalen -= DDP_X_HDR_SIZE;
	if (datalen > MAXDATA) 
		datalen = MAXDATA;
	if (datalen < 1) 
		datalen = 1;
	if (datalen > sizeof(struct timeval))
		timing = 1;
	if (argc > 2)
		npackets = atoi(av[2]);

	ident = getpid() & 0xFFFF;

	if ((fd = socket(AF_APPLETALK, SOCK_RAW, DDP_ECHO)) < 0) {
		perror("socket failed");
		exit(5);
		}
	size = sizeof(remote);
	remote.sat_len = size;
	remote.sat_family = AF_APPLETALK;
	remote.sat_port = EP_SOCKET;
	/* *** setsockopt no checksum(?) *** */

	local.sat_len = size;
	local.sat_family = AF_APPLETALK;
	local.sat_port = 0;
	if (0 > (bind(fd, &local, size))) {
		perror("bind failed");
		close(fd);
		exit(6);
	}

	printf(MSGSTR(M_PING2,
		"Pinging %s: %d data bytes\n"), hostname, datalen+DDP_X_HDR_SIZE);

	signal(SIGINT, finish);
	signal (SIGALRM, catcher);

	catcher(0);	/* start things going */

	for (;;) {
		int len = sizeof (packet);
		int cc;

		size = sizeof(remote);
		if ((cc = 
		     recvfrom(fd, packet, len,
			      flags, NULL, &size)) < 0) {
			perror("recvfrom failed");
			close(fd);
			exit(1);
		}

		pr_pack(packet, cc);
		if (npackets && nreceived >= npackets)
			finish(0);
	}
	/*NOTREACHED*/
}


/*
 * 			C A T C H E R
 * 
 * This routine causes another PING to be transmitted, and then
 * schedules another SIGALRM for 1 second from now.
 * 
 * Bug -
 * 	Our sense of time will slowly skew (ie, packets will not be launched
 * 	exactly at 1-second intervals).  This does not affect the quality
 *	of the delay and loss statistics.
 */
static void catcher(int sig)
{
	int waittime;

	pinger();
	if (npackets == 0 || ntransmitted < npackets)
		alarm(1);
	else {
		if (nreceived) {
			waittime = 2 * tmax / 1000;
			if (waittime == 0)
				waittime = 1;
		} else
			waittime = MAXWAIT;
		signal(SIGALRM, finish);
		alarm(waittime);
	}
}


/*
 * 			P I N G E R
 * 
 * Compose and transmit an AppleTalk Echo Protocol request packet.  The first
 * byte conatins the echo command, followed by an optional 8 byte "timeval"
 * struct in network byte-order, to compute the round-trip time.
 */

static void
pinger()
{
	static at_ddp_t outpack;
	int i, cc;
	register struct timeval *tp = (struct timeval *) &outpack.data[1];
	register u_char *datap = (u_char *) &outpack.data[sizeof(struct timeval) + 1];

	cc = datalen;

	if (timing) {
		gettimeofday(tp, 0);
		for (i = sizeof(struct timeval) + 1; i < datalen; i++)
			*datap++ = i;
	}

	outpack.data[0] = EP_REQUEST; 

	size = sizeof(local);
	i = sendto(fd, &outpack.data, cc, flags, (struct sockaddr *)&remote, 
		   size);
	if (i < 0 || i != cc)  {
		if (i < 0) {
			perror(MSGSTR(M_AP_WRITE, "appleping: write"));
			exit(1);
		}
		printf(MSGSTR(M_AP_WROTE, "appleping: wrote %s %d chars, ret=%d\n"),
			hostname, cc, i);
		fflush(stdout);
	}
	else
		ntransmitted++;
}


/*
 *			P R _ P A C K
 *
 * Print out the packet, computing statistics.
 */

static void
pr_pack(buf, cc)
char *buf;
int cc;
{
	at_ddp_t *ddp;
	struct timeval tv;
	struct timeval *tp;
	int triptime;

	gettimeofday(&tv, 0);

	ddp = (at_ddp_t *) buf;
	tp = (struct timeval *) &ddp->data[1];
	printf(MSGSTR(M_BYTES_FROM, "%d bytes from %04x.%02x.%02x: "),
		cc, NET_VALUE(ddp->src_net), ddp->src_node & 0xff, ddp->src_socket & 0xff);
	hopsum += ddp->hopcount;
	if ((int) ddp->hopcount < hopmin)
		hopmin = ddp->hopcount;
	if ((int) ddp->hopcount > hopmax)
		hopmax = ddp->hopcount;
	printf(MSGSTR(M_HOP, "hop=%2d, "), ddp->hopcount & 0xf);
	if (timing) {
		tvsub(&tv, tp);
		triptime = tv.tv_sec*1000+(tv.tv_usec/1000);
		printf(MSGSTR(M_TIME_MS, "time=%4d. ms\n"), triptime);
		tsum += triptime;
		if (triptime < tmin)
			tmin = triptime;
		if (triptime > tmax)
			tmax = triptime;
	} else
		putchar('\n');
	nreceived++;
}


/*
 * 			T V S U B
 * 
 * Subtract 2 timeval structs:  out = out - in.
 * 
 * Out is assumed to be >= in.
 */

static void
tvsub(out, in)
register struct timeval *out, *in;
{
	if ((out->tv_usec -= in->tv_usec) < 0)   {
		out->tv_sec--;
		out->tv_usec += 1000000;
	}
	out->tv_sec -= in->tv_sec;
}


/*
 *			F I N I S H
 *
 * Print out statistics, and give up.
 * Heavily buffered STDIO is used here, so that all the statistics
 * will be written with 1 sys-write call.  This is nice when more
 * than one copy of the program is running on a terminal;  it prevents
 * the statistics output from becomming intermingled.
 */
static void finish(int sig)
{
	printf(MSGSTR(M_AT_STAT, "\n----%s ApplePing Statistics----\n"), hostname);
	printf(MSGSTR(M_PKTS_XMIT, "%d packets transmitted, "), ntransmitted);
	printf(MSGSTR(M_PKTS_RECD, "%d packets received, "), nreceived);
	if (ntransmitted)
	    printf(MSGSTR(M_PKT_LOSS, "%d%% packet loss"),
		(int) (((ntransmitted-nreceived)*100) / ntransmitted));
	printf("\n");
	if (nreceived && timing)
	    printf(MSGSTR(M_RTT, "round-trip (ms)  min/avg/max = %d/%d/%d\n"),
		tmin,
		tsum / nreceived,
		tmax);
	if (nreceived)
	    printf(MSGSTR(M_HOPCNT, "hop-count        min/avg/max = %d/%d/%d\n"),
		hopmin,
		hopsum / nreceived,
		hopmax);
	fflush(stdout);
	exit(0);
}
