
/*
 * bsdpc.c
 * - BSDP client
 */
/* 
 * Modification History
 *
 * December 16, 1999 	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 */

#import <stdlib.h>
#import <unistd.h>
#import <string.h>
#import <stdio.h>
#import <sys/types.h>
#import <sys/errno.h>
#import <sys/socket.h>
#import <sys/fcntl.h>
#import <ctype.h>
#import <net/if.h>
#import <net/etherdefs.h>
#import <netinet/in.h>
#import <netinet/udp.h>
#import <netinet/in_systm.h>
#import <netinet/ip.h>
#import <netinet/bootp.h>
#import <arpa/inet.h>

#import "interfaces.h"
#import "rfc_options.h"
#import "dhcp_options.h"
#import "bsdp.h"
#import "util.h"
#import "dhcplib.h"

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

static u_short 			client_port;
static boolean_t 		exit_quick = FALSE;
static u_short 			server_port = IPPORT_BOOTPS;
static u_long			gather_ticks = GATHER_TIME_TICKS;
static u_long			max_retries = MAX_RETRIES;
static int			sockfd;
static boolean_t		testing = FALSE;
static interface_list_t *	interfaces = NULL;

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

struct in_addr	dest_ip;
unsigned char	rfc_magic[4] = RFC_OPTIONS_MAGIC;

extern struct ether_addr *ether_aton(char *);

static u_char dhcp_params[] = {
#if 0
    dhcptag_subnet_mask_e, 
    dhcptag_router_e,
    dhcptag_host_name_e,
    dhcptag_domain_name_server_e,
    dhcptag_domain_name_e,
    dhcptag_netinfo_server_address_e,
    dhcptag_netinfo_server_tag_e,
#endif 0
    dhcptag_vendor_class_identifier_e,
    dhcptag_vendor_specific_e,
};
int	n_dhcp_params = sizeof(dhcp_params) / sizeof(dhcp_params[0]);


typedef struct {
    u_int32_t		signature;
    bsdp_image_id_t	image_id;
    char *		description;
    char *		graphic;
} bsdp_image_info_t;

#define BSDP_IMAGE_INFO_SIG	((u_int32_t)0x12345678)

typedef struct {
    struct in_addr	server_ip;
    bsdp_priority_t	server_priority;
    bsdp_image_id_t	default_image_id;
    bsdp_image_id_t	selected_image_id;
    boolean_t		selected_image_id_supplied;
    char		list_path[DHCP_OPTION_SIZE_MAX + 1];
} bsdp_list_response_t;

typedef struct {
    bsdp_image_info_t *	image_info;
    ptrlist_t		servers;
} bsdp_image_selection_t;

typedef struct {
    dynarray_t		images;
    dynarray_t		servers;
    bsdp_list_response_t*current;
} bsdp_image_menu_t;

#import "tftplib.h"

#define END_TAG_LENGTH(a)	(strlen(a) + 2)
#define IMAGE_TAG		"image"
#define ID_TAG			"id"
#define GRAPHIC_TAG		"graphic"
#define DESCRIPTION_TAG		"description"

static __inline__ void
S_image_info_free_elements(bsdp_image_info_t * info)
{
    if (info->description) {
	free(info->description);
	info->description = NULL;
    }
    if (info->graphic) {
	free(info->graphic);
	info->graphic = NULL;
    }
    return;
}

void
bsdp_image_info_free(void * info_p)
{
    bsdp_image_info_t * info = (bsdp_image_info_t *)info_p;
    if (info->signature != BSDP_IMAGE_INFO_SIG) {
	fprintf(stderr, "bsdp_image_info_free: arg %x not bsdp_image_info_t\n",
		(u_int)info);
	return;
    }
    S_image_info_free_elements(info);
    free(info);
    return;
}

bsdp_image_info_t *
bsdp_image_info_create(bsdp_image_id_t image_id, 
		       char * description, char * graphic)
{
    bsdp_image_info_t * image_info = malloc(sizeof(*image_info));

    if (image_info == NULL)
	return (NULL);
    bzero(image_info, sizeof(*image_info));
    image_info->signature = BSDP_IMAGE_INFO_SIG;
    image_info->image_id = image_id;
    if (description) {
	image_info->description = strdup(description);
    }
    if (graphic) {
	image_info->graphic = strdup(graphic);
    }
    return (image_info);
}

void *
bsdp_image_info_copy(void * src)
{
    bsdp_image_info_t * source = (bsdp_image_info_t *)src;

    if (source->signature != BSDP_IMAGE_INFO_SIG) {
	fprintf(stderr, "bsdp_image_info_copy: arg %x not bsdp_image_info_t\n",
		(u_int)src);
	return (NULL);
    }
    return(bsdp_image_info_create(source->image_id, source->description, 
				  source->graphic));
}


boolean_t
image_list_parse(char * buf_start, size_t buf_len, dynarray_t * list_p)
{
    char * 	buf_end = buf_start + buf_len;
    char * 	scan;

    dynarray_init(list_p, bsdp_image_info_free, bsdp_image_info_copy);
    for (scan = buf_start; scan < buf_end; ) {
	bsdp_image_info_t *	info;
	bsdp_image_id_t		image_id = 0;
	char *			description = NULL;
	char *			graphic = NULL;
	char * 			image_start;
	char * 			image_end;
	char *			start;
	char * 			end;
	int			len;

	image_start = tagtext_get(scan, buf_end, IMAGE_TAG, &image_end);
	if (image_start == NULL) {
	    break; /* no more image tags */
	}
	/* get the text description */
	start = tagtext_get(image_start, image_end, DESCRIPTION_TAG, &end);
	if (start == NULL)
	    goto cleanup_fail;
	len = end - start;

	description = malloc(len + 1);
	if (description == NULL)
	    goto cleanup_fail;
	bcopy(start, description, len);
	description[len] = '\0';
	
	/* get the image id */
	start = tagtext_get(image_start, image_end, ID_TAG, &end);
	if (start == NULL)
	    goto cleanup_fail;
	image_id = strtoul(start, NULL, NULL);

	/* get the graphic */
	start = tagtext_get(image_start, image_end, GRAPHIC_TAG, &end);
	if (start == NULL) {
	    goto cleanup_fail;
	}
	else {
	    len = end - start;
	    graphic = malloc(len + 1);
	    if (graphic == NULL)
		goto cleanup_fail;
	    bcopy(start, graphic, len);
	    graphic[len] = '\0';
	}
	scan = image_end + END_TAG_LENGTH(IMAGE_TAG);
	info = bsdp_image_info_create(image_id, description, graphic);
	if (info) {
	    if (description)
		free(description);
	    if (graphic)
		free(graphic);
	    dynarray_add(list_p, info);
	    continue;
	}
    cleanup_fail:
	if (description)
	    free(description);
	if (graphic)
	    free(graphic);
	goto failed;
    }
    return (TRUE);
 failed:
    dynarray_free(list_p);
    return (FALSE);
}

boolean_t
image_list_get(struct in_addr server, char * path, dynarray_t * list_p)
{
    void *	data = NULL;
    size_t	data_len;
    int		fd = -1;
    off_t	len;
    char * 	local_filename = NULL;
    boolean_t	ret = FALSE;
    struct stat sb;

    local_filename = tftp_get(inet_ntoa(server), path, &len, 5);
    if (local_filename == NULL || len == 0) {
	fprintf(stderr, "download of %s:%s failed\n", 
		inet_ntoa(server), path);
	goto done;
    }
    
    if (stat(local_filename, &sb) < 0)
	goto done;
    data_len = sb.st_size;
    if (data_len == 0)
	goto done;

    data = malloc(data_len);
    if (data == NULL)
	goto done;

    fd = open(local_filename, O_RDONLY);
    if (fd < 0)
	goto done;

    if (read(fd, data, data_len) != data_len) {
#ifdef DEBUG
	fprintf(stderr, "read %s failed, %s\n", local_filename, 
		strerror(errno));
#endif DEBUG
	goto done;
    }
    if (image_list_parse(data, data_len, list_p) == FALSE
	|| dynarray_count(list_p) == 0) {
	goto done;
    }
    ret = TRUE;

 done:
    if (local_filename) {
	unlink(local_filename);
	free(local_filename);
    }
    if (data) {
	free(data);
    }
    if (fd >= 0)
	close(fd);
    if (ret == FALSE) {
	dynarray_free(list_p);
    }
    return (ret);
}

void
image_list_print(dynarray_t * list_p)
{
    int 		i;

    for (i = 0; i < dynarray_count(list_p); i++) {
	bsdp_image_info_t *	info_p;

	info_p = (bsdp_image_info_t *)dynarray_element(list_p, i);
	printf("image_id %x description '%s' graphic '%s'\n",
	       info_p->image_id, info_p->description, info_p->graphic);
    }
}

static struct dhcp * 
make_bsdp_request(struct dhcp * request, int pkt_size,
		  dhcp_msgtype_t msg, u_char * hwaddr, u_char hwtype, 
		  u_char hwlen, dhcpoa_t * options_p)
{
    bzero(request, pkt_size);
    request->dp_op = BOOTREQUEST;
    request->dp_htype = hwtype;
    request->dp_hlen = hwlen;
    bcopy(hwaddr, request->dp_chaddr, hwlen);
    bcopy(rfc_magic, request->dp_options, sizeof(rfc_magic));
    dhcpoa_init(options_p, request->dp_options + sizeof(rfc_magic),
		DHCP_MIN_OPTIONS_SIZE - sizeof(rfc_magic));
    
    /* make the request a dhcp message */
    if (dhcpoa_add_dhcpmsg(options_p, msg) != dhcpoa_success_e) {
	fprintf(stderr,
	       "make_bsdp_request: couldn't add dhcp message tag %d, %s", msg,
	       dhcpoa_err(options_p));
	goto err;
    }

    /* add the list of required parameters */
    if (dhcpoa_add(options_p, dhcptag_parameter_request_list_e,
		   sizeof(dhcp_params), dhcp_params)
	!= dhcpoa_success_e) {
	fprintf(stderr, "make_bsdp_request: "
	       "couldn't add parameter request list, %s",
	       dhcpoa_err(options_p));
	goto err;
    }
    if (dhcpoa_add(options_p, 
		   dhcptag_vendor_class_identifier_e, 
		   strlen(BSDP_VENDOR_CLASS_ID),
		   BSDP_VENDOR_CLASS_ID) != dhcpoa_success_e) {
	fprintf(stderr, "make_bsdp_request: add class id failed, %s",
		dhcpoa_err(options_p));
	return (NULL);
    }

    return (request);
  err:
    return (NULL);
}

static void
on_alarm(int sigraised)
{
    exit(0);
    return;
}

void
wait_for_responses()
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

boolean_t
send_packet(void * pkt, int pkt_len, struct in_addr iaddr)
{
    struct sockaddr_in 	dst;
    int 		status;

    bzero(&dst, sizeof(dst));
    dst.sin_len = sizeof(struct sockaddr_in);
    dst.sin_family = AF_INET;
    dst.sin_port = htons(server_port);
    dst.sin_addr = iaddr;

    status = sendto(sockfd, pkt, pkt_len, 0,
		    (struct sockaddr *)&dst, sizeof(struct sockaddr_in));
    if (status < 0) {
	perror("sendto");
	return (FALSE);
    }
    return (TRUE);
}

u_char		rxbuf[2048];
int		rxbuf_size;

#if 0
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
#endif 0

void
print_list_response(bsdp_list_response_t * response)
{
    printf("Server %s Priority %d Default %x",
	   inet_ntoa(response->server_ip),
	   response->server_priority,
	   response->default_image_id);
    if (response->selected_image_id_supplied)
	printf(" Selected %x", response->selected_image_id);
    printf(" Path %s\n", response->list_path);
}

bsdp_list_response_t *
process_list_ack(struct dhcp * reply, int n)
{
    bsdp_list_response_t * 	ack = NULL;
    dhcpol_t			bsdp_options;
    void *			opt;
    int				opt_len;
    dhcpol_t			options;

    dhcpol_init(&options);
    dhcpol_init(&bsdp_options);

    if (dhcpol_parse_packet(&options, reply, n, NULL) == FALSE) {
	return (FALSE);
    }

    ack = malloc(sizeof(*ack));
    if (ack == NULL)
	goto failed;
    opt = dhcpol_find(&options, dhcptag_dhcp_message_type_e, NULL, NULL);
    if (opt == NULL || *((unsigned char *)opt) != dhcp_msgtype_ack_e) {
	goto failed; /* response must be a DHCP ack */
    }
    opt = dhcpol_find(&options, dhcptag_vendor_class_identifier_e,
		      &opt_len, NULL);
    if (opt == NULL
	|| opt_len != strlen(BSDP_VENDOR_CLASS_ID)
	|| bcmp(opt, BSDP_VENDOR_CLASS_ID, opt_len)) {
	goto failed; /* not BSDP */
    }
    if (dhcpol_parse_vendor(&bsdp_options, &options, NULL) == FALSE) {
	goto failed; /* no vendor info */
    }
    opt = dhcpol_find(&bsdp_options, bsdptag_message_type_e,
		      &opt_len, NULL);
    if (opt == NULL || opt_len != 1
	|| *((unsigned char *)opt) != bsdp_msgtype_list_e) {
	goto failed; /* not an ACK[LIST] message */
    }

    /* get the server identifier */
    opt = dhcpol_find(&options, dhcptag_server_identifier_e,
		      &opt_len, NULL);
    if (opt == NULL || opt_len != sizeof(ack->server_ip)) {
	goto failed;
    }

    bzero(ack, sizeof(*ack));

    ack->server_ip = *(struct in_addr *)opt;

    /* get the boot image list */
    opt = dhcpol_find(&bsdp_options, bsdptag_boot_image_list_e, &opt_len,
		      NULL);
    if (opt == NULL)
	goto failed;
    bcopy(opt, ack->list_path, opt_len);
    ack->list_path[opt_len] = '\0';

    /* get the server priority */
    opt = dhcpol_find(&bsdp_options, bsdptag_server_priority_e, &opt_len,
		      NULL);
    if (opt == NULL || opt_len != sizeof(ack->server_priority))
	goto failed; /* priority is missing */
    ack->server_priority = ntohs(*((bsdp_priority_t *)opt));
    
    /* get the default boot image */
    opt = dhcpol_find(&bsdp_options, bsdptag_default_boot_image_e, &opt_len,
		      NULL);
    if (opt == NULL || opt_len != sizeof(ack->default_image_id))
	goto failed; /* priority is missing */
    ack->default_image_id = ntohl(*((bsdp_image_id_t *)opt));

    /* get the selected boot image (may or may not be present) */
    opt = dhcpol_find(&bsdp_options, bsdptag_selected_boot_image_e, &opt_len,
		      NULL);
    if (opt && opt_len == sizeof(ack->selected_image_id)) {
	ack->selected_image_id_supplied = TRUE;
	ack->selected_image_id = ntohl(*((bsdp_image_id_t *)opt));
    }
    dhcpol_free(&options);
    dhcpol_free(&bsdp_options);
    return (ack);

 failed:
    if (ack)
	free(ack);
    dhcpol_free(&options);
    dhcpol_free(&bsdp_options);
    return (NULL);
}

void
list_response_add(dynarray_t * list_p, bsdp_list_response_t * ack)
{
    int 			i;
    bsdp_list_response_t *	scan;

    for (i = 0; i < dynarray_count(list_p); i++) {
	scan = dynarray_element(list_p, i);
	if (scan->server_ip.s_addr == ack->server_ip.s_addr) {
	    /* replace with the latest response */
	    *scan = *ack;
	    free(ack);
	    return;
	}
    }
    dynarray_add(list_p, ack);
    return;
}

static u_long
next_xid()
{
    static boolean_t	inited = FALSE;
    static u_long	xid;

    if (inited == FALSE) {
	struct timeval	start_time;
	gettimeofday(&start_time, 0);
	srandom(start_time.tv_usec & ~start_time.tv_sec);
	xid = random();
	inited = TRUE;
    }
    else {
	xid++;
    }
    return (xid);
}

boolean_t
bsdp_list(interface_t * if_p, dynarray_t * list_p)
{
    char		bsdp_buf[DHCP_OPTION_SIZE_MAX];
    dhcpoa_t		bsdp_options;
    u_char *		buf = NULL;
    int			bufsize;
    struct timeval	current_time;
    dhcpoa_t		options;
    struct sockaddr_in 	from;
    int 		fromlen;
    int			gather_tick_count = 0;
    unsigned char	msgtype;
    int			retries = 0;
    struct dhcp *	request;
    int 		request_size = 0;
    struct timeval	start_time;
    struct timeval	timeout;
    int			wait_ticks = INITIAL_WAIT_SECS * TICKS_PER_SEC;

    dynarray_init(list_p, free, NULL);

    bufsize = sizeof(struct dhcp) + DHCP_MIN_OPTIONS_SIZE;
    buf = malloc(bufsize);
    if (buf == NULL)
	goto failed;

    request = make_bsdp_request((struct dhcp *)buf, bufsize,
				dhcp_msgtype_inform_e, if_link_address(if_p), 
				if_link_arptype(if_p), if_link_length(if_p), 
				&options);
    if (request == NULL)
	goto failed;

    request->dp_ciaddr = if_inet_addr(if_p);
    dhcpoa_init(&bsdp_options, bsdp_buf, sizeof(bsdp_buf));
    msgtype = bsdp_msgtype_list_e;
    if (dhcpoa_add(&bsdp_options, bsdptag_message_type_e,
		   sizeof(msgtype), &msgtype) 
	!= dhcpoa_success_e) {
	fprintf(stderr, "bsdpc: add message type failed, %s",
		dhcpoa_err(&bsdp_options));
	goto failed;
    }
    {
	u_int16_t		port = htons(client_port);
	bsdp_version_t		version = htons(BSDP_VERSION);

	if (dhcpoa_add(&bsdp_options, bsdptag_version_e, sizeof(version),
		       &version) != dhcpoa_success_e) {
	    fprintf(stderr, "bsdpc: add version failed, %s",
		    dhcpoa_err(&bsdp_options));
	    goto failed;
	}
	if (dhcpoa_add(&bsdp_options, bsdptag_reply_port_e, sizeof(port), 
		       &port) != dhcpoa_success_e) {
	    fprintf(stderr, "bsdpc: add reply port failed, %s",
		    dhcpoa_err(&bsdp_options));
	    goto failed;
	}
    }
    if (dhcpoa_add(&bsdp_options, dhcptag_end_e, 0, NULL)
	!= dhcpoa_success_e) {
	fprintf(stderr, "bsdpc: add bsdp options end failed, %s",
		dhcpoa_err(&bsdp_options));
	goto failed;
    }
    if (dhcpoa_add(&options, dhcptag_vendor_specific_e,
		   dhcpoa_used(&bsdp_options), &bsdp_buf)
	!= dhcpoa_success_e) {
	fprintf(stderr, "bsdpc: add vendor specific failed, %s",
	       dhcpoa_err(&options));
	goto failed;
    }
    if (dhcpoa_add(&options, dhcptag_end_e, 0, NULL)
	!= dhcpoa_success_e) {
	fprintf(stderr, "bsdpc: add dhcp options end failed, %s",
		dhcpoa_err(&bsdp_options));
	goto failed;
    }
    request_size = sizeof(*request) + sizeof(rfc_magic) 
	+ dhcpoa_used(&options);
    if (request_size < sizeof(struct bootp)) {
	/* pad out to BOOTP-sized packet */
	request_size = sizeof(struct bootp);
    }

    timeout.tv_sec = 0;
    timeout.tv_usec = RECEIVE_TIMEOUT_USECS;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (caddr_t)&timeout,
		   sizeof(timeout)) < 0) {
	perror("setsockopt SO_RCVTIMEO");
	goto failed;
    }
    gettimeofday(&start_time, 0);
    current_time = start_time;
    for (retries = 0; retries < (max_retries + 1); retries++) {
	int 		ticks;
	u_long		xid = next_xid();

	request->dp_secs = htons((u_short)(current_time.tv_sec 
					   - start_time.tv_sec));
	request->dp_xid = htonl(xid);

	/* send the packet */
	if (send_packet(request, request_size, if_inet_broadcast(if_p))
	    == FALSE)
	    goto failed;

	if (testing) {
	    goto failed;
	}
	/* wait for a response */
	ticks = wait_ticks + random_range(-RAND_TICKS, RAND_TICKS);
	for (;ticks > 0;) {
	    bsdp_list_response_t *	ack;
	    int 			n_recv;
	    struct dhcp * 		reply = (struct dhcp *)rxbuf;

	    from.sin_family = AF_INET;
	    fromlen = sizeof(struct sockaddr);
	
	    n_recv = recvfrom(sockfd, rxbuf, sizeof(rxbuf), 0,
			      (struct sockaddr *)&from, &fromlen);
	    if (n_recv < 0) {
		if (errno == EAGAIN) {
		    ticks--;
		    if (dynarray_count(list_p) > 0) {
			gather_tick_count++;
			if (gather_tick_count >= gather_ticks)
			    goto done;
		    }
		    continue;
		}
		perror("bsdp_list(): recvfrom");
		goto failed;
	    }
	    if (n_recv < sizeof(struct bootp)) {
		continue;
	    }

	    if (dhcp_packet_match((struct bootp *)reply, xid, 
				  if_link_arptype(if_p),
				  if_link_address(if_p),
				  if_link_length(if_p)) == FALSE
		|| reply->dp_ciaddr.s_addr != if_inet_addr(if_p).s_addr) {
		continue;
	    }
	    ack = process_list_ack(reply, n_recv);
	    if (ack) {
		list_response_add(list_p, ack);
	    }
	}
	wait_ticks *= 2;
	if (wait_ticks >= (MAX_WAIT_SECS * TICKS_PER_SEC))
	    wait_ticks = MAX_WAIT_SECS * TICKS_PER_SEC;
	gettimeofday(&current_time, 0);
    }
 done:
    if (dynarray_count(list_p) == 0)
	goto failed;
    if (buf) {
	free(buf);
	buf = NULL;
    }
    return (TRUE);

 failed:
    if (buf) {
	free(buf);
	buf = NULL;
    }
    dynarray_free(list_p);
    return (FALSE);
}

boolean_t
process_select_ack(struct dhcp * reply, int n, struct in_addr server_ip,
		   bsdp_image_id_t image_id, boolean_t * success)
{
    dhcpol_t		bsdp_options;
    void *		opt;
    int			opt_len;
    dhcpol_t		options;

    dhcpol_init(&options);
    dhcpol_init(&bsdp_options);

    if (dhcpol_parse_packet(&options, reply, n, NULL) == FALSE) {
	return (FALSE);
    }

    opt = dhcpol_find(&options, dhcptag_dhcp_message_type_e, NULL, NULL);
    if (opt == NULL || *((unsigned char *)opt) != dhcp_msgtype_ack_e) {
	goto failed; /* response must be a DHCP ack */
    }
    opt = dhcpol_find(&options, dhcptag_server_identifier_e,
		      &opt_len, NULL);
    if (opt == NULL)
	goto failed; /* response must have a server identifier */
    if (((struct in_addr *)opt)->s_addr != server_ip.s_addr)
	goto failed; /* not the right server */

    opt = dhcpol_find(&options, dhcptag_vendor_class_identifier_e,
		      &opt_len, NULL);
    if (opt == NULL
	|| opt_len != strlen(BSDP_VENDOR_CLASS_ID)
	|| bcmp(opt, BSDP_VENDOR_CLASS_ID, opt_len)) {
	goto failed; /* not BSDP */
    }
    if (dhcpol_parse_vendor(&bsdp_options, &options, NULL) == FALSE) {
	goto failed; /* no vendor info */
    }
    opt = dhcpol_find(&bsdp_options, bsdptag_message_type_e,
		      &opt_len, NULL);
    if (opt == NULL || opt_len != 1) {
	goto failed;
    }
    {
	unsigned char msg = *((unsigned char *)opt);
	if (msg == bsdp_msgtype_select_e) {
	    *success = TRUE;
	}
	else if (msg == bsdp_msgtype_failed_e) {
	    *success = FALSE;
	}
	else {
	    goto failed;
	}
    }
    dhcpol_free(&options);
    dhcpol_free(&bsdp_options);
    return (TRUE);

 failed:
    dhcpol_free(&options);
    dhcpol_free(&bsdp_options);
    return (FALSE);
}

boolean_t
bsdp_select(interface_t * if_p, struct in_addr server_ip, 
	    bsdp_image_id_t image_id)
{
    char		bsdp_buf[DHCP_OPTION_SIZE_MAX];
    dhcpoa_t		bsdp_options;
    u_char *		buf = NULL;
    int			bufsize;
    struct timeval	current_time;
    dhcpoa_t		options;
    struct sockaddr_in 	from;
    int 		fromlen;
    unsigned char	msgtype;
    int			retries = 0;
    struct dhcp *	request;
    int 		request_size = 0;
    boolean_t		ret = FALSE;
    struct timeval	start_time;
    struct timeval	timeout;
    int			wait_ticks = INITIAL_WAIT_SECS * TICKS_PER_SEC;

    bufsize = sizeof(struct dhcp) + DHCP_MIN_OPTIONS_SIZE;
    buf = malloc(bufsize);
    if (buf == NULL)
	goto done;

    request = make_bsdp_request((struct dhcp *)buf, bufsize,
				dhcp_msgtype_inform_e, if_link_address(if_p), 
				if_link_arptype(if_p), if_link_length(if_p), 
				&options);
    if (request == NULL)
	goto done;

    request->dp_ciaddr = if_inet_addr(if_p);
    dhcpoa_init(&bsdp_options, bsdp_buf, sizeof(bsdp_buf));
    msgtype = bsdp_msgtype_select_e;
    if (dhcpoa_add(&bsdp_options, bsdptag_message_type_e,
		   sizeof(msgtype), &msgtype) 
	!= dhcpoa_success_e) {
	fprintf(stderr, "bsdpc: add message type failed, %s",
		dhcpoa_err(&bsdp_options));
	goto done;
    }
    {
	u_int16_t		port = htons(client_port);
	bsdp_version_t		version = htons(BSDP_VERSION);
	u_int32_t		selection = htonl(image_id);

	if (dhcpoa_add(&bsdp_options, bsdptag_version_e, sizeof(version),
		       &version) != dhcpoa_success_e) {
	    fprintf(stderr, "bsdpc: add version failed, %s",
		    dhcpoa_err(&bsdp_options));
	    goto done;
	}
	if (dhcpoa_add(&bsdp_options, bsdptag_reply_port_e, sizeof(port), 
		       &port) != dhcpoa_success_e) {
	    fprintf(stderr, "bsdpc: add reply port failed, %s",
		    dhcpoa_err(&bsdp_options));
	    goto done;
	}
	if (dhcpoa_add(&bsdp_options, bsdptag_selected_boot_image_e,
		       sizeof(selection), &selection) != dhcpoa_success_e) {
	    fprintf(stderr, "bsdpc: add selected image failed, %s",
		    dhcpoa_err(&bsdp_options));
	    goto done;
	}
    }
    if (dhcpoa_add(&bsdp_options, bsdptag_server_identifier_e,
		   sizeof(server_ip), &server_ip) != dhcpoa_success_e) {
	fprintf(stderr, "bsdpc: add server identifier failed, %s",
		dhcpoa_err(&bsdp_options));
	goto done;
    }
    if (dhcpoa_add(&bsdp_options, dhcptag_end_e, 0, NULL)
	!= dhcpoa_success_e) {
	fprintf(stderr, "bsdpc: add bsdp options end failed, %s",
		dhcpoa_err(&bsdp_options));
	goto done;
    }
    if (dhcpoa_add(&options, dhcptag_vendor_specific_e,
		   dhcpoa_used(&bsdp_options), &bsdp_buf)
	!= dhcpoa_success_e) {
	fprintf(stderr, "bsdpc: add vendor specific failed, %s",
	       dhcpoa_err(&options));
	goto done;
    }
    if (dhcpoa_add(&options, dhcptag_end_e, 0, NULL)
	!= dhcpoa_success_e) {
	fprintf(stderr, "bsdpc: add dhcp options end failed, %s",
		dhcpoa_err(&bsdp_options));
	goto done;
    }
    request_size = sizeof(*request) + sizeof(rfc_magic) 
	+ dhcpoa_used(&options);
    if (request_size < sizeof(struct bootp)) {
	/* pad out to BOOTP-sized packet */
	request_size = sizeof(struct bootp);
    }

    timeout.tv_sec = 0;
    timeout.tv_usec = RECEIVE_TIMEOUT_USECS;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (caddr_t)&timeout,
		   sizeof(timeout)) < 0) {
	perror("setsockopt SO_RCVTIMEO");
	goto done;
    }
    gettimeofday(&start_time, 0);
    current_time = start_time;
    for (retries = 0; retries < (max_retries + 1); retries++) {
	int 		ticks;
	u_long		xid = next_xid();

	request->dp_secs = htons((u_short)(current_time.tv_sec 
					   - start_time.tv_sec));
	request->dp_xid = htonl(xid);

	/* send the packet */
	if (send_packet(request, request_size, if_inet_broadcast(if_p))
	    == FALSE)
	    goto done;

	/* wait for a response */
	ticks = wait_ticks + random_range(-RAND_TICKS, RAND_TICKS);
	for (;ticks > 0;) {
	    int 			n_recv;
	    struct dhcp * 		reply = (struct dhcp *)rxbuf;

	    from.sin_family = AF_INET;
	    fromlen = sizeof(struct sockaddr);
	
	    n_recv = recvfrom(sockfd, rxbuf, sizeof(rxbuf), 0,
			      (struct sockaddr *)&from, &fromlen);
	    if (n_recv < 0) {
		if (errno == EAGAIN) {
		    ticks--;
		    continue;
		}
		perror("bsdp_list(): recvfrom");
		goto done;
	    }
	    if (n_recv < sizeof(struct bootp)) {
		continue;
	    }

	    if (dhcp_packet_match((struct bootp *)reply, xid, 
				  if_link_arptype(if_p),
				  if_link_address(if_p),
				  if_link_length(if_p)) == FALSE
		|| reply->dp_ciaddr.s_addr != if_inet_addr(if_p).s_addr) {
		continue;
	    }
	    {
		boolean_t confirmation = FALSE;

		if (process_select_ack(reply, n_recv, server_ip, image_id,
				       &confirmation)) {
		    ret = confirmation;
		    goto done;
		}
	    }
	}
	wait_ticks *= 2;
	if (wait_ticks >= (MAX_WAIT_SECS * TICKS_PER_SEC))
	    wait_ticks = MAX_WAIT_SECS * TICKS_PER_SEC;
	gettimeofday(&current_time, 0);
    }
 done:
    if (buf) {
	free(buf);
	buf = NULL;
    }
    return (ret);
}

bsdp_image_selection_t * 
image_selection_find(dynarray_t * list_p, bsdp_image_id_t image_id)
     /* list_p is an array of bsdp_image_selection_t's */
{
    int 			i;


    for (i = 0; i < dynarray_count(list_p); i++) {
	bsdp_image_selection_t * selection;

	selection = dynarray_element(list_p, i);
	if (selection->image_info->image_id == image_id)
	    return (selection);
    }
    return (NULL);
}

void
bsdp_image_selection_free(void * s)
{
    bsdp_image_selection_t * selection = (bsdp_image_selection_t *)s;

    bsdp_image_info_free(selection->image_info);
    ptrlist_free(&selection->servers);
    bzero(selection, sizeof(*selection));
    return;
}

/* 
 * Function: image_selection_add
 * Args:
 *   selections:	array of bsdp_image_selection_t's,
 *   images:	 	array of bsdp_image_info_t's
 */
boolean_t
image_selection_add(dynarray_t * selections, bsdp_list_response_t * response, 
		    dynarray_t * images)
{
    int 			i;

    for (i = 0; i < dynarray_count(images); i++) {
	bsdp_image_info_t *	 image_info = dynarray_element(images, i);
	bsdp_image_selection_t * selection;

	selection = image_selection_find(selections, image_info->image_id);
	if (selection) {
	    /* image item already exists, add this server to the list */
	    if (ptrlist_add(&selection->servers, response) == FALSE) {
		fprintf(stderr, "could add server %s to list for image %d\n", 
			inet_ntoa(response->server_ip),	image_info->image_id);
		return (FALSE);
	    }
	}
	else {
	    /* create a new item */
	    bsdp_image_selection_t *	item = NULL;

	    item = malloc(sizeof(*item));
	    if (item == NULL)
		goto cleanup_fail;

	    bzero(item, sizeof(*item));
	    ptrlist_init(&item->servers);
	    item->image_info = bsdp_image_info_copy(image_info);
	    if (item->image_info == NULL) {
		fprintf(stderr, "could add image %d to list\n", 
			image_info->image_id);
		goto cleanup_fail;
	    }
	    if (ptrlist_add(&item->servers, response) == FALSE) {
		fprintf(stderr, "could add server %s to list for image %d\n", 
			inet_ntoa(response->server_ip),	image_info->image_id);
		goto cleanup_fail;
	    }
	    dynarray_add(selections, item);
	    continue;

	cleanup_fail:
	    if (item) {
		ptrlist_free(&item->servers);
		if (item->image_info)
		    free(item->image_info);
		free(item);
		return (FALSE);
	    }
	}
    }
    return (TRUE);
}

int
menu_num_items(bsdp_image_menu_t * menu)
{
    return (dynarray_count(&menu->images));
}

void
menu_display(bsdp_image_menu_t * menu)
{
    int i;

    printf("\nThe following images are available:\n");
    for (i = 0; i < dynarray_count(&menu->images); i++) {
#ifdef DEBUG
	int			 j;
#endif DEBUG
	bsdp_image_selection_t * item = dynarray_element(&menu->images, i);
	
	printf("%d. %s", i + 1, item->image_info->description);
	if (menu->current 
	    && (menu->current->selected_image_id 
		== item->image_info->image_id)) {
	    printf(" [Server %s Image %d]", 
		   inet_ntoa(menu->current->server_ip),
		   item->image_info->image_id);
	}
#ifdef DEBUG
	printf(" (");
	for (j = 0; j < ptrlist_count(&item->servers); j++) {
	    bsdp_list_response_t * r;
	    r = ptrlist_element(&item->servers, j);
	    printf("%s%s", (j != 0) ? ", " : " (",
		   inet_ntoa(r->server_ip));
	}
	printf(")");
#endif DEBUG
	printf("\n");
    }
}

void
menu_prompt()
{
    printf("Please choose an image (type 'q' to quit) [q]: ");
}

void
menu_free(bsdp_image_menu_t * menu)
{
}


boolean_t
menu_create(interface_t * if_p, bsdp_image_menu_t * menu)
{
    int i;

    dynarray_init(&menu->images, bsdp_image_selection_free, NULL);
    menu->current = NULL;

    if (bsdp_list(if_p, &menu->servers)) {
#ifdef DEBUG
	printf("Number of responses: %d\n", dynarray_count(&menu->servers));
#endif DEBUG
	for (i = 0; i < dynarray_count(&menu->servers); i++) {
	    dynarray_t			image_list;
	    bsdp_list_response_t *	r;
	    
	    r = dynarray_element(&menu->servers, i);
	    if (r->selected_image_id_supplied && menu->current == NULL)
		menu->current = r;
#ifdef DEBUG
	    printf("%d: ", i + 1);
	    print_list_response(r);
#endif DEBUG
	    if (image_list_get(r->server_ip, r->list_path,
			       &image_list) == FALSE) {
		fprintf(stderr, "Couldn't get %s from %s\n",
			r->list_path, inet_ntoa(r->server_ip));
	    }
	    else {
#ifdef DEBUG 
		image_list_print(&image_list);
#endif DEBUG
		if (image_selection_add(&menu->images, r, &image_list)
		    == FALSE) {
		    fprintf(stderr, "Failed to add selection item(s)"
			    " from server %s\n", inet_ntoa(r->server_ip));
		}
		dynarray_free(&image_list);
	    }
	}
	return (TRUE);
    }
    return (FALSE);
}

boolean_t
menu_select(interface_t * if_p, bsdp_image_menu_t * menu, int which)
{
    int				i;
    bsdp_image_selection_t *	image;
    bsdp_list_response_t *	best_response = NULL;

    image = dynarray_element(&menu->images, which - 1);
    if (image == NULL)
	return (FALSE);

    for (i = 0; i < ptrlist_count(&image->servers); i++) {
	bsdp_list_response_t *	response;
	response = ptrlist_element(&image->servers, i);
	if (best_response == NULL 
	    || response->server_priority > best_response->server_priority)
	    best_response = response;
    }
    if (best_response == NULL)
	return (FALSE);
    return (bsdp_select(if_p, best_response->server_ip, 
			image->image_info->image_id));
}

void
usage(u_char * progname)
{
    fprintf(stderr, "useage: %s -i <interface> [options]\n"
	    "options:\n"
	    "-g <ticks> : gather response time (1 tick = 1/10th sec)\n"
	    "-r <count> : retry count\n",
	    progname);
    exit(USER_ERROR);
}

int 
main(int argc, char *argv[])
{
    char		ch;
    interface_t * 	if_p = NULL;
    u_char *		progname = argv[0];

    if (argc < 2)
	usage(progname);
    interfaces = ifl_init(FALSE);
    if (interfaces == NULL) {
	fprintf(stderr, "no interfaces\n");
	exit(UNEXPECTED_ERROR);
    }

    dest_ip.s_addr = htonl(INADDR_BROADCAST);

    while ((ch =  getopt(argc, argv, "eghHi::r:S:T")) != EOF) {
	switch (ch) {
	case 'e':
	    exit_quick = TRUE;
	    break;
	case 'g': /* gather time */
	    gather_ticks = strtoul(optarg, NULL, NULL);
	    break;
	case 'H':
	case 'h':
	    usage(progname);
	    break;
	case 'i': {
	    if_p = ifl_find_name(interfaces, optarg);
	    if (if_p == NULL || if_p->link_valid == FALSE
		|| if_inet_valid(if_p) == FALSE) {
		fprintf(stderr, "interface %s not configured\n",
			optarg);
		exit(USER_ERROR);
	    }
	    break;
	}
	case 'r': /* retry count */
	    max_retries = strtoul(optarg, NULL, NULL);
	    break;
	case 'S': /* server ip address */
	    dest_ip.s_addr = inet_addr(optarg);
	    break;
	case 'T': /* log and wait for all responses */
	    testing = TRUE;
	    break;
	}
    }
    if (if_p == NULL) {
	fprintf(stderr, "no interface specified");
	usage(progname);
    }

    if (dl_to_arp_hwtype(if_p->link.sdl_type) == -1) {
	fprintf(stderr, "unsupported interface type");
	exit(UNEXPECTED_ERROR);
    }

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
	perror("socket");
	exit(UNEXPECTED_ERROR);
    }
    {
	struct sockaddr_in 	me;
	int		   	me_len;
	int 			status;
	int 			opt;

	bzero((char *)&me, sizeof(me));
	me.sin_family = AF_INET;
#ifdef MOSX
	opt = IP_PORTRANGE_LOW;

	/* get a privileged port */
	status = setsockopt(sockfd, IPPROTO_IP, IP_PORTRANGE, &opt, 
			    sizeof(opt));
	if (status < 0) {
	    perror("setsockopt IPPROTO_IP IP_PORTRANGE");
	    exit(UNEXPECTED_ERROR);
	}
#else MOSX
	me.sin_port = 975;
#endif MOSX

	status = bind(sockfd, (struct sockaddr *)&me, sizeof(me));
	if (status != 0) {
	    perror("bind");
	    exit(UNEXPECTED_ERROR);
	}
	me_len = sizeof(me);
	if (getsockname(sockfd, (struct sockaddr *)&me,  &me_len) < 0) {
	    perror("getsockname");
	    exit(UNEXPECTED_ERROR);
	}
	client_port = ntohs(me.sin_port);
	opt = 1;
	status = setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &opt, 
			    sizeof(opt));
	if (status < 0)	{
	    perror("setsockopt SO_BROADCAST");
	    exit(UNEXPECTED_ERROR);
	}
    }

    if (testing) {
	dynarray_t list;

	bsdp_list(if_p, &list);
	if (exit_quick == FALSE)
	    wait_for_responses();
    }
    else {
	bsdp_image_menu_t	menu;

	printf("Discovering NetBoot 2.0 images...\n");
	fflush(stdout);
	if (menu_create(if_p, &menu)) {
	    int i = 0;

	    if (menu_num_items(&menu) == 0) {
		fprintf(stderr, "no images available\n");
		goto done;
	    }
	    while (1) {
		char	choice[128];

		if (i == 0) {
		    menu_display(&menu);
		    i = 4;
		}
		menu_prompt();
		if (fgets(choice, sizeof(choice), stdin) == choice) {
		    if (*choice == '\n' || *choice == 'q' || *choice == 'Q')
			break;
		    if (!isdigit(*choice)) {
			fprintf(stderr, "Invalid response\n");
		    }
		    else {
			int	which;
			which = strtoul(choice, 0, 0);
			if (which == ULONG_MAX) {
			    fprintf(stderr, "Invalid response\n");
			}
			else if (which >=1 && which <= menu_num_items(&menu)) {
			    if (menu_select(if_p, &menu, which) == FALSE) {
				fprintf(stderr, 
					"NetBoot image selection failed\n");
			    }
			    break;
			}

		    }
		}
		else { 
		    break;
		}
		i--;
	    }
	done:
	    menu_free(&menu);
	}
    }
    exit(0);
}
