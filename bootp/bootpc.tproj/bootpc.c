/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
 * bootpc.m
 * - BOOTP client used for two purposes:
 * 1) act as a BOOTP client to retrieve the IP address and other
 *    options like the subnet mask, router for dynamic host configuration
 * 2) testing the BOOTP server
 */
#import <stdlib.h>
#import <unistd.h>
#import <string.h>
#import <stdio.h>
#import <sys/types.h>
#import <sys/errno.h>
#import <sys/socket.h>
#import <ctype.h>
#import <net/if.h>
#import <net/etherdefs.h>
#import <netinet/in.h>
#import <netinet/udp.h>
#import <netinet/in_systm.h>
#import <netinet/ip.h>
#import <netinet/bootp.h>
#import <arpa/inet.h>

#import "rfc_options.h"
#import "dhcp_options.h"
#import "macnc_options.h"
#import "dhcp.h"
#import "interfaces.h"
#import "util.h"
#import "dhcplib.h"
#import <net/if_types.h>

#define USER_ERROR		1
#define UNEXPECTED_ERROR 	2
#define TIMEOUT_ERROR		3

#define USECS_PER_TICK			(100 * 1000) /* 1/10th second */
#define TICKS_PER_SEC			(USECS_PER_SEC / USECS_PER_TICK)
#define INITIAL_WAIT_SECS		4
#define MAX_WAIT_SECS			64
#define RAND_TICKS			(1 * TICKS_PER_SEC) 
					/* add a random value -1...+1 */
#define MAX_RETRIES			3
#define RECEIVE_TIMEOUT_SECS		0
#define RECEIVE_TIMEOUT_USECS		USECS_PER_TICK
#define GATHER_TIME_USECS		(2 * 1000 * 1000) /* 2 second default */
#define GATHER_TIME_TICKS		(GATHER_TIME_USECS / USECS_PER_TICK)

static u_short 		client_port = IPPORT_BOOTPC;
static boolean_t 	exit_quick = FALSE;
static u_short 		server_port = IPPORT_BOOTPS;
static u_long		gather_ticks = GATHER_TIME_TICKS;
static u_long		max_retries = MAX_RETRIES;
static boolean_t 	must_broadcast = FALSE;
static int		sockfd;
static boolean_t	testing = FALSE;
/* tags_search: these are the tags we look for: */
static dhcptag_t       	tags_search[] = { 
    dhcptag_host_name_e,
    dhcptag_subnet_mask_e, 
    dhcptag_router_e, 
};
int			n_tags_search = sizeof(tags_search) 
				        / sizeof(tags_search[0]);
/* tags_print: these are the tags we print in the response */
static dhcptag_t       	tags_print[] = { 
    dhcptag_host_name_e,
    dhcptag_subnet_mask_e, 
    dhcptag_router_e, 
    dhcptag_domain_name_server_e,
    dhcptag_domain_name_e,
    dhcptag_netinfo_server_address_e,
    dhcptag_netinfo_server_tag_e,
};
int			n_tags_print = sizeof(tags_print) 
				       / sizeof(tags_print[0]);


#define NONE	0
#define MACNC	1
#define RFC	2

int client = RFC;

struct in_addr	dest_ip;
unsigned char	rfc_magic[4] = RFC_OPTIONS_MAGIC;

extern struct ether_addr *ether_aton(const char *);

void
make_bootp_request(struct bootp * bp, 
		   u_char * hwaddr, u_char hwtype, u_char hwlen, 
		   struct in_addr ciaddr)
{
    bzero(bp, sizeof(*bp));
    
    bp->bp_op = BOOTREQUEST;
    bp->bp_htype = hwtype;
    bp->bp_hlen = hwlen;
    bp->bp_ciaddr = ciaddr; /* should normally be 0.0.0.0 */
    bp->bp_hops = 0;
    if (must_broadcast)
	bp->bp_unused = htons(0x1);
    bcopy((caddr_t)hwaddr, bp->bp_chaddr, hwlen);
    
    switch (client) {
      case RFC: {
	  bcopy(rfc_magic, bp->bp_vend, sizeof(rfc_magic));
	  bp->bp_vend[4] = dhcptag_end_e;
	  break;
      }
      case MACNC: { /* make packet look like we're a Mac NC */
	  unsigned long		client_version;
	  dhcpoa_t		options;

	  bcopy(rfc_magic, bp->bp_vend, sizeof(rfc_magic));
	  dhcpoa_init(&options, bp->bp_vend + sizeof(rfc_magic),
		      sizeof(bp->bp_vend) - sizeof(rfc_magic));
	  
#define VERSION		0
	  client_version = htonl(VERSION);
	  if ((dhcpoa_add(&options, macNCtag_client_version_e,
			  sizeof(client_version), &client_version)
	       != dhcpoa_success_e)
	      || (dhcpoa_add(&options, macNCtag_client_info_e,
			     strlen(MACNC_CLIENT_INFO), MACNC_CLIENT_INFO)
		  != dhcpoa_success_e)
	      || (dhcpoa_add(&options, dhcptag_end_e, 0, 0) 
		  != dhcpoa_success_e)) {
	      fprintf(stderr, "%s\n", dhcpoa_err(&options));
	      exit (1);
	  }
	  if (testing) {
	      dhcpol_t		parsed;
	      
	      dhcpol_init(&parsed);
	      if (dhcpol_parse_packet(&parsed, (struct dhcp *)bp, 
				      sizeof(*bp), NULL)) {
		  dhcpol_print(&parsed);
	      }
	      else
		  fprintf(stderr, "parse failed\n");
	      dhcpol_free(&parsed);
	  }
	  break;
      }
      case NONE:
      default:
	break;
	
    }
    return;
}

static void
on_alarm(int sigraised)
{
    exit(0);
    return;
}

void
wait_for_bootp_responses()
{
    u_char 		buf[2048];
    int			buf_len = sizeof(buf);
    struct sockaddr_in 	from;
    int 		fromlen;

    bzero(buf, buf_len);

    signal(SIGALRM, on_alarm);
    ualarm(gather_ticks * USECS_PER_TICK, 0);

    for(;;) {
	int 		n_recv;

	from.sin_family = AF_INET;
	fromlen = sizeof(struct sockaddr);
	
	n_recv = recvfrom(sockfd, buf, buf_len, 0,
			   (struct sockaddr *)&from, &fromlen);
	if (n_recv > 0) {
	    printf("reply from %s\n",
		   inet_ntoa(from.sin_addr));
	    dhcp_print_packet((struct dhcp *)buf, n_recv);
	    printf("\n");
	}
    }
    return;
}

void
send_packet(void * pkt, int pkt_len)
{
    struct sockaddr_in 	dst;
    int 		status;

    bzero(&dst, sizeof(dst));
    dst.sin_len = sizeof(struct sockaddr_in);
    dst.sin_family = AF_INET;
    dst.sin_port = htons(server_port);
	
    dst.sin_addr = dest_ip;
    status = sendto(sockfd, pkt, pkt_len, 0,
		    (struct sockaddr *)&dst, sizeof(struct sockaddr_in));
    if (status < 0) {
	perror("sendto");
	exit(UNEXPECTED_ERROR);
    }
    return;
}

u_char 		saved_rx_pkt[2048];
int 		saved_rx_len = 0;
dhcpol_t	saved_rx_options;

boolean_t
check_response(void * pkt, int pkt_size)
{
    boolean_t			better = FALSE;
    int				i;
    dhcpol_t			pkt_options;
    int				pkt_tag_count;

    dhcpol_init(&pkt_options);
    pkt_tag_count = 0;
    if (dhcpol_parse_packet(&pkt_options, pkt, pkt_size, NULL)) {
	for (i = 0; i < n_tags_search; i++) {
	    int len;
	    if (dhcpol_find(&pkt_options, tags_search[i], &len, NULL)) {
		pkt_tag_count++;
		if (dhcpol_find(&saved_rx_options, tags_search[i], &len, NULL) 
		    == NULL) {
		    better = TRUE;
		}
	    }
	}
    }
    dhcpol_free(&pkt_options);
    if (saved_rx_len == 0 || better) {
	saved_rx_len = pkt_size;
	bcopy(pkt, saved_rx_pkt, pkt_size);
	dhcpol_free(&saved_rx_options);
	dhcpol_parse_packet(&saved_rx_options, (struct dhcp *)saved_rx_pkt, 
			    saved_rx_len, NULL);
    }
    if (pkt_tag_count == n_tags_search)
	return (TRUE);
    return (FALSE);
}

void
print_option(FILE * file, void * option, int len, int tag)
{
    int 		i;
    int 		count;
    int			size;
    dhcptag_info_t * 	tag_info = dhcptag_info(tag);
    int			type;
    dhcptype_info_t * 	type_info;

    if (tag_info == NULL)
	return;
    type = tag_info->type;
    type_info = dhcptype_info(type);
    if (type_info == NULL)
	return;
    size = 0;
    count = 1;
    if (type_info->multiple_of != dhcptype_none_e) {
	dhcptype_info_t * base_type_info 
	    = dhcptype_info(type_info->multiple_of);
	size = base_type_info->size;
	count = len / size;
	len = size;
	type = type_info->multiple_of;
	fprintf(file, "%s_count=%d\n", tag_info->name, count);
    }
    for (i = 0; i < count; i++) {
	u_char tmp[512];
	
	if (dhcptype_to_str(tmp, option, len, type, NULL) == TRUE) {
	    fprintf(file, "%s", tag_info->name);
	    if (i > 0)
		fprintf(file, "%d", i + 1);
	    fprintf(file, "=%s\n", tmp);
	}
	option += size;
    }
    return;
}

void
echo_variables(void * pkt, int pkt_len, dhcpol_t * options)
{
    struct bootp * bp = (struct bootp *)pkt;

    if (bp->bp_yiaddr.s_addr)
	printf("ip_address=%s\n", inet_ntoa(bp->bp_yiaddr));
    else
	printf("ip_address=%s\n", inet_ntoa(bp->bp_ciaddr));

    {
	int	i;
	int 	len;
	void * 	option;

	for (i = 0; i < n_tags_print; i++) {
	    option = dhcpol_find(options, tags_print[i], &len, NULL);
	    if (option)
		print_option(stdout, option, len, tags_print[i]);
	}
    }
    if (bp->bp_siaddr.s_addr)
	printf("server_ip_address=%s\n", inet_ntoa(bp->bp_siaddr));
    if (bp->bp_sname[0])
	printf("server_name=%s\n", bp->bp_sname);
    return;
}

void
bootp_loop(u_char * hwaddr, u_char hwtype, u_char hwlen, struct in_addr ciaddr)
{
    struct timeval	current_time;
    struct sockaddr_in 	from;
    int 		fromlen;
    int			gather_tick_count = 0;
    int			retries = 0;
    struct bootp	request;
    u_char 		rxpkt[2048];
    struct bootp *	reply = (struct bootp *)rxpkt;
    struct timeval	start_time;
    struct timeval	timeout;
    int			wait_ticks = INITIAL_WAIT_SECS * TICKS_PER_SEC;
    u_long		xid;

    dhcpol_init(&saved_rx_options);

    make_bootp_request(&request, hwaddr, hwtype, hwlen, ciaddr);
    timeout.tv_sec = 0;
    timeout.tv_usec = RECEIVE_TIMEOUT_USECS;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (caddr_t)&timeout,
		   sizeof(timeout)) < 0) {
	perror("setsockopt SO_RCVTIMEO");
	exit(1);
    }
    gettimeofday(&start_time, 0);
    current_time = start_time;
    srandom(start_time.tv_usec & ~start_time.tv_sec);
    xid = random();
    for (retries = 0; retries < (max_retries + 1); retries++) {
	int 		ticks;

	request.bp_secs = htons((u_short)(current_time.tv_sec 
					  - start_time.tv_sec));
	request.bp_xid = htonl(xid);

	/* send the packet */
	send_packet(&request, sizeof(request));

	if (testing)
	    return;
	/* wait for a response */
	ticks = wait_ticks + random_range(-RAND_TICKS, RAND_TICKS);
	for (;ticks > 0;) {
	    int 		n_recv;

	    from.sin_family = AF_INET;
	    fromlen = sizeof(struct sockaddr);
	
	    n_recv = recvfrom(sockfd, rxpkt, sizeof(rxpkt), 0,
			      (struct sockaddr *)&from, &fromlen);
	    if (n_recv < 0) {
		if (errno == EAGAIN) {
		    ticks--;
		    if (saved_rx_len) {
			gather_tick_count++;
			if (gather_tick_count >= gather_ticks)
			    goto output_values;
		    }
		    continue;
		}
		perror("bootp_loop(): recvfrom");
		exit(UNEXPECTED_ERROR);
	    }
	    else if (n_recv < sizeof(struct bootp)) {
		continue;
	    }	
	    if (ntohl(reply->bp_xid) == xid
		&& (reply->bp_yiaddr.s_addr || reply->bp_ciaddr.s_addr)
		&& hwtype == reply->bp_htype
		&& hwlen == reply->bp_hlen
		&& bcmp(hwaddr, reply->bp_chaddr, hwlen) == 0) {
		/* check if this is a better response */
		if (check_response((void *)reply, n_recv))
		    goto output_values;
	    }
	}
	wait_ticks *= 2;
	if (wait_ticks >= (MAX_WAIT_SECS * TICKS_PER_SEC))
	    wait_ticks = MAX_WAIT_SECS * TICKS_PER_SEC;
	xid++;
	gettimeofday(&current_time, 0);
    }

  output_values:
    if (saved_rx_len == 0)
	exit(TIMEOUT_ERROR);

    echo_variables(saved_rx_pkt, saved_rx_len, &saved_rx_options);
    dhcpol_free(&saved_rx_options);
    saved_rx_len = 0;
    return;
}

void
usage(u_char * progname)
{
    fprintf(stderr, "useage: %s <interface> | <identifier> [options]\n"
	    "where options is one of:\n"
	    "-g <ticks> : gather response time (1 tick = 1/10th sec)\n"
	    "-r <count> : retry count\n",
	    progname);
    exit(USER_ERROR);
}

int 
main(int argc, char *argv[])
{
    char *		cp;
    struct in_addr	client_ip = { 0 };
    char *		client_ip_str = NULL;
    u_char		hwaddr[16];
    u_char		hwtype = ARPHRD_ETHER;
    u_char		hwlen = 6;
    u_char *		progname = argv[0];

    if (argc < 2)
	usage(progname);
    {
	struct ether_addr * en_p = ether_aton(argv[1]);
	if (en_p) {
	    *((struct ether_addr *)hwaddr) = *en_p;
	}
	else {
	    interface_t *	if_p;
	    interface_list_t *	list_p;

	    list_p = ifl_init(FALSE);
	    if (list_p == NULL) {
		fprintf(stderr, "no interfaces\n");
		exit(UNEXPECTED_ERROR);
	    }
	    if_p = ifl_find_name(list_p, argv[1]);
	    if (if_p == NULL || if_p->link_valid == FALSE) {
		fprintf(stderr, "link interface %s doesn't exist\n",
			argv[1]);
		exit(USER_ERROR);
	    }
	    if (if_p->link.sdl_type == IFT_ETHER) {
		hwtype = ARPHRD_ETHER;
		hwlen = if_p->link.sdl_alen;
		if (hwlen > sizeof(hwaddr)) {
		    fprintf(stderr, "interface %s has bogus len %d\n",
			    argv[1], hwlen);
		    exit(UNEXPECTED_ERROR);
		}
		bcopy(if_p->link.sdl_data + if_p->link.sdl_nlen,
		      hwaddr, hwlen);
	    }
	    else {
		fprintf(stderr, "interface %s is not ethernet\n",
			argv[1]);
		exit(USER_ERROR);
	    }
	    ifl_free(&list_p);
	}
    }

    dest_ip.s_addr = htonl(INADDR_BROADCAST);

    argc--, argv++;
    argc--, argv++;
    while (argc > 0 && *argv[0] == '-') {
	for (cp = &argv[0][1]; *cp; cp++) {
	    switch (*cp) {
	      case 'b':
		must_broadcast = TRUE;
		break;
	      case 'C': /* specify the ciaddr value */
		if (argc > 1) {
		    client_ip_str = argv[1];
		    argc--; argv++;
		}
		break;
	      case 'c': /* client port - for testing */
		if (argc > 1) {
		    client_port = atoi(argv[1]);
		    argc--; argv++;
		}
		break;
	      case 'e': /* send the request and exit */
		exit_quick = TRUE;
		break;
	      case 'g': /* gather time */
		if (argc > 1) {
		    argc--;
		    argv++;
		    gather_ticks = strtoul(*argv, NULL, NULL);
		}
		break;
	      case 'm': /* act like a Mac NC */
		client = MACNC;
		break;
	      case 'N': /* act like a generic BOOTP client */
		client = NONE;
		break;
	      case 'r': /* retry count */
		if (argc > 1) {
		    argc--;
		    argv++;
		    max_retries = strtoul(*argv, NULL, NULL);
		}
		break;
	      case 'S': /* server ip address */
		if (argc > 1) {
		    argc--;
		    argv++;
		    dest_ip.s_addr = inet_addr(*argv);
		}
		break;
	      case 's': /* server port - for testing */
		if (argc > 1) {
		    server_port = atoi(argv[1]);
		    argc--; argv++;
		}
		break;
	      case 'T': /* log and wait for all responses */
		testing = TRUE;
		break;
	      default:
		fprintf(stderr,"bootpc: unknown flag -%c ignored",*cp);
	      case 'H':
	      case 'h':
		usage(progname);
		break;
	    }
	}
	argc--, argv++;
    }
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
	perror("socket");
	exit(UNEXPECTED_ERROR);
    }
    {
	struct sockaddr_in me;
	int status;
	int opt = 1;

	bzero((char *)&me, sizeof(me));
	me.sin_family = AF_INET;
	me.sin_port = htons(client_port);
	me.sin_addr.s_addr = htonl(INADDR_ANY);
	
	status = bind(sockfd, (struct sockaddr *)&me, sizeof(me));
	if (status != 0) {
	    perror("bind");
	    exit(UNEXPECTED_ERROR);
	}
	status = setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &opt, 
			    sizeof(opt));
	if (status < 0)	{
	    perror("setsockopt SO_BROADCAST");
	    exit(UNEXPECTED_ERROR);
	}
	status = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, 
			    sizeof(opt));
	if (status < 0) {
	    perror("setsockopt SO_SOREUSEADDR");
	    exit(UNEXPECTED_ERROR);
	}
    }

    if (client_ip_str == NULL)
	client_ip.s_addr = 0;
    else {
	client_ip.s_addr = inet_addr(client_ip_str);
	if (client_ip.s_addr == -1) {
	    fprintf(stderr, "bad ip address %s\n", client_ip_str);
	    exit(USER_ERROR);
	}
    }
    if (testing) {
	bootp_loop(hwaddr, hwtype, hwlen, client_ip);
	if (exit_quick == FALSE)
	    wait_for_bootp_responses();
    }
    else {
	bootp_loop(hwaddr, hwtype, hwlen, client_ip);
    }
    exit(0);
}

