/*
 * Copyright (c) 1999, 2000 Apple Computer, Inc. All rights reserved.
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

/*
 * bsdpd.m
 * - Boot Server Discovery Protocol (BSDP) server
 */

/*
 * Modification History
 *
 * Dieter Siegmund (dieter@apple.com)		November 24, 1999
 * - created
 */

#import <unistd.h>
#import <stdlib.h>
#import <stdio.h>
#import <sys/stat.h>
#import <sys/socket.h>
#import <sys/ioctl.h>
#import <sys/file.h>
#import <sys/time.h>
#import <sys/types.h>
#import <net/if.h>
#import <netinet/in.h>
#import <netinet/in_systm.h>
#import <netinet/ip.h>
#import <netinet/udp.h>
#import <netinet/bootp.h>
#import <netinet/if_ether.h>
#import <syslog.h>
#import <arpa/inet.h>
#import <net/if_arp.h>
#import <mach/boolean.h>
#import <sys/errno.h>
#import <limits.h>
#import <pwd.h>

#import "afp.h"
#import "dhcp.h"
#import "bsdp.h"
#import "subnetDescr.h"
#import "dhcp_options.h"
#import "host_identifier.h"
#import "interfaces.h"
#import "dhcpd.h"
#import "globals.h"
#import "bootp_transmit.h"
#import "netinfo.h"
#import "bsdpd.h"
#import "NIDomain.h"
#import "bootpd.h"
#import "macNC.h"
#import "macnc_options.h"

#import "NICache.h"
#import "NICachePrivate.h"

#import "AFPUsers.h"

typedef struct {
    PLCache_t		list;
} BSDPClients_t;

/* DEFAULT_IMAGE_ID: XXX This should be dynamic */
#define DEFAULT_IMAGE_ID	1
#define BSDP_BOOT_IMAGE_LIST_DIR	"/private/tftpboot"
#define BSDP_BOOT_IMAGE_LIST_PATH	BSDP_BOOT_IMAGE_LIST_DIR "/bsdplist"

static BSDPClients_t	S_clients;
static AFPUsers_t	S_afp_users;

#define AFP_UID_START	100
static int		S_afp_uid_start = AFP_UID_START;

void
BSDPClients_free(BSDPClients_t * clients)
{
    PLCache_free(&clients->list);
    bzero(clients, sizeof(*clients));
}

#define BSDP_CLIENTS_FILE	"/var/db/bsdpd_clients"
boolean_t
BSDPClients_init(BSDPClients_t * clients)
{
    bzero(clients, sizeof(*clients));
    PLCache_init(&clients->list);
#define ARBITRARILY_LARGE_NUMBER	(100 * 1024 * 1024)
    PLCache_set_max(&clients->list, ARBITRARILY_LARGE_NUMBER);

    if (PLCache_read(&clients->list, BSDP_CLIENTS_FILE) == FALSE) {
	goto failed;
    }
    return (TRUE);
 failed:
    BSDPClients_free(clients);
    return (FALSE);
}

boolean_t
bsdp_init()
{
    static boolean_t 	first = TRUE;

    if (first == TRUE) {
	if (netboot_gid == 0) {
	    syslog(LOG_INFO, "NetBoot: netboot_gid not set");
	    return (FALSE);
	}
	if (BSDPClients_init(&S_clients) == FALSE) {
	    return (FALSE);
	}
	if (AFPUsers_init(&S_afp_users, ni_local) == FALSE) {
	    return (FALSE);
	}
	AFPUsers_create(&S_afp_users, netboot_gid, S_afp_uid_start, 
			afp_users_max);
	first = FALSE;
    }
    else {
	BSDPClients_t 	new_clients;
	AFPUsers_t	new_users;

	syslog(LOG_INFO, "NetBoot: re-reading client list");
	if (BSDPClients_init(&new_clients) == TRUE) {
	    BSDPClients_free(&S_clients);
	    S_clients = new_clients;
	}
	if (AFPUsers_init(&new_users, ni_local) == TRUE) {
	    AFPUsers_create(&new_users, netboot_gid, 
			    S_afp_uid_start, afp_users_max);
	    AFPUsers_free(&S_afp_users);
	    S_afp_users = new_users;
	}
    }
    return (TRUE);
}

static boolean_t
S_client_number_in_use(int number)
{
    PLCacheEntry_t * 	scan;

    for (scan = S_clients.list.head; scan; scan = scan->next) {
	int			number_index;
	ni_namelist *		number_nl_p;
	int			val;

	number_index = ni_proplist_match(scan->pl, NIPROP_NETBOOT_NUMBER, NULL);
	if (number_index == NI_INDEX_NULL)
	    continue; /* this can't happen */
	number_nl_p = &scan->pl.nipl_val[number_index].nip_val;
	val = strtol(number_nl_p->ninl_val[0], NULL, NULL);
	if (val == number) {
	    return (TRUE);
	}
    }
    return (FALSE);
}

static int
S_next_client_number()
{
    int i = 1;

    while (1) {
	if (S_client_number_in_use(i) == FALSE)
	    return (i);
	i++;
    }
    return (0); /* NOT REACHED */
}

static boolean_t
S_commit_mods(PLCacheEntry_t * entry)
{
    return (PLCache_write(&S_clients.list, BSDP_CLIENTS_FILE));
}

static boolean_t
S_client_update(PLCacheEntry_t * entry, struct dhcp * reply, 
		char * idstr, struct in_addr server_ip,
		bsdp_image_id_t image_id,
		dhcpoa_t * options)
{
    u_char *		afp_user;
    u_char *		hostname;
    boolean_t		modified = FALSE;
    u_char 		passwd[AFP_PASSWORD_LEN + 1];
    PLCacheEntry_t *	user_entry;
    unsigned long	password;
    uid_t		uid;
    u_char *		val;


    /* XXX */
    /* get the type of image i.e. Classic, NFS */
    /* dispatch to appropriate handler for the type of image */
    /* XXX */

    hostname = ni_valforprop(&entry->pl, NIPROP_NAME);
    val = ni_valforprop(&entry->pl, NIPROP_NETBOOT_NUMBER);
    afp_user = ni_valforprop(&entry->pl, NIPROP_NETBOOT_AFP_USER);
    if (hostname == NULL || val == NULL
	|| afp_user == NULL ) {
	syslog(LOG_INFO, "NetBoot: %s missing " NIPROP_NAME 
	       ", " NIPROP_NETBOOT_NUMBER
	       ", or " NIPROP_NETBOOT_AFP_USER, idstr);
	return (FALSE);
    }
    user_entry = PLCache_lookup_prop(&S_afp_users.list, 
				     NIPROP_NAME, afp_user);
    if (user_entry == NULL) {
	syslog(LOG_INFO, "NetBoot: failed to locate entry for %s",
	       afp_user);
	return (FALSE);
    }
    password = random();
    uid = strtoul(ni_valforprop(&user_entry->pl, NIPROP_UID), NULL, NULL);

    sprintf(passwd, "%08lx", password);
    if (AFPUsers_set_password(&S_afp_users, user_entry, passwd)
	== FALSE) {
	syslog(LOG_INFO, "NetBoot: failed to set password for %s",
	       hostname);
	return (FALSE);
    }
    if (macNC_allocate(reply, hostname, server_ip, strtol(val, NULL, NULL),
		       options, uid, afp_user, passwd) == FALSE) {
	return (FALSE);
    }
    {
	char	buf[32];
	sprintf(buf, "0x%x", image_id);
	ni_set_prop(&entry->pl, NIPROP_NETBOOT_IMAGE_ID, buf, &modified);
    }

    if (modified && S_commit_mods(entry) == FALSE) {
	return (FALSE);
    }
    return (TRUE);
}

static PLCacheEntry_t *
S_next_afp_user(u_char * * afp_user)
{
    PLCacheEntry_t * scan;

    for (scan = S_afp_users.list.head; scan; scan = scan->next) {
	int		name_index;
	ni_namelist *	nl_p;

	name_index = ni_proplist_match(scan->pl, NIPROP_NAME, NULL);
	if (name_index == NI_INDEX_NULL)
	    continue;
	nl_p = &scan->pl.nipl_val[name_index].nip_val;
	if (nl_p->ninl_len == 0)
	    continue;

	if (PLCache_lookup_prop(&S_clients.list, NIPROP_NETBOOT_AFP_USER,
				nl_p->ninl_val[0]) == NULL) {
	    *afp_user = nl_p->ninl_val[0];
	    return (scan);
	}
    }
    return (NULL);
}

static boolean_t
S_client_create(struct dhcp * reply, char * idstr, 
		char * arch, char * sysid, 
		struct in_addr server_ip,
		bsdp_image_id_t image_id, dhcpoa_t * options)
{
    u_char *		afp_user = NULL;
    ni_id		child = {0, 0};
    char		hostname[256];
    int			num;
    u_char 		passwd[AFP_PASSWORD_LEN + 1];
    unsigned long	password;
    ni_proplist		pl;
    PLCacheEntry_t *	user_entry;
    uid_t		uid;

    /* XXX
     * get the type of image i.e. Classic, NFS
     * dispatch to appropriate handler for the type of image
     */

    num = S_next_client_number();
    user_entry = S_next_afp_user(&afp_user);
    if (user_entry == NULL) {
	syslog(LOG_INFO, "NetBoot AFP login capacity of %d reached",
	       afp_users_max);
	return (FALSE);
    }
    password = random();
    uid = strtoul(ni_valforprop(&user_entry->pl, NIPROP_UID), NULL, NULL);
    NI_INIT(&pl);
    sprintf(hostname, "bsdp%03d", num);
    ni_proplist_addprop(&pl, NIPROP_NAME, (ni_name)hostname);
    ni_proplist_addprop(&pl, NIPROP_IDENTIFIER, (ni_name)idstr);
    ni_proplist_addprop(&pl, NIPROP_NETBOOT_AFP_USER, afp_user);
    ni_proplist_addprop(&pl, NIPROP_NETBOOT_ARCH, arch);
    ni_proplist_addprop(&pl, NIPROP_NETBOOT_SYSID, sysid);
    {
	char buf[64];

	sprintf(buf, "0x%x", image_id);
	ni_proplist_addprop(&pl, NIPROP_NETBOOT_IMAGE_ID, (ni_name)buf);
	sprintf(buf, "%d", num);
	ni_proplist_addprop(&pl, NIPROP_NETBOOT_NUMBER, (ni_name)buf);
    }
    sprintf(passwd, "%08lx", password);
    if (AFPUsers_set_password(&S_afp_users, user_entry, passwd)
	== FALSE) {
	syslog(LOG_INFO, "NetBoot: failed to set password for %s",
	       hostname);
	return (FALSE);
    }

    if (macNC_allocate(reply, hostname, server_ip, num, 
		       options, uid, afp_user, passwd) == FALSE) {
	goto error;
    }

    PLCache_add(&S_clients.list, PLCacheEntry_create(child, pl));
    PLCache_write(&S_clients.list, BSDP_CLIENTS_FILE);

    ni_proplist_free(&pl);
    return (TRUE);
 error:
    ni_proplist_free(&pl);
    return (FALSE);

}

static boolean_t
S_client_remove(PLCacheEntry_t * * entry)
{
    PLCacheEntry_t *	ent = *entry;

    PLCache_remove(&S_clients.list, ent);
    PLCacheEntry_free(ent);
    *entry = NULL;
    PLCache_write(&S_clients.list, BSDP_CLIENTS_FILE);
    return (TRUE);
}

static struct dhcp * 
make_bsdp_reply(struct dhcp * reply, int pkt_size, 
		struct in_addr server_id, dhcp_msgtype_t msg, 
		struct dhcp * request, dhcpoa_t * options)
{
    struct dhcp * r;

    r = make_dhcp_reply(reply, pkt_size, server_id, msg, request, options);
    if (r == NULL)
	return (NULL);
    if (dhcpoa_add(options, 
		   dhcptag_vendor_class_identifier_e, 
		   strlen(BSDP_VENDOR_CLASS_ID),
		   BSDP_VENDOR_CLASS_ID) != dhcpoa_success_e) {
	syslog(LOG_INFO, "NetBoot: add class id failed, %s",
	       dhcpoa_err(options));
	return (NULL);
    }
    return (r);
}


static __inline__ boolean_t
S_prop_u_int32(ni_proplist * pl_p, u_char * prop, u_int32_t * retval)
{
    ni_name str = ni_valforprop(pl_p, prop);

    if (str == NULL)
	return (FALSE);
    *retval = strtoul(str, 0, 0);
    if (*retval == ULONG_MAX && errno == ERANGE) {
	return (FALSE);
    }
    return (TRUE);
}

#define TXBUF_SIZE	2048
static char	txbuf[TXBUF_SIZE];

void
bsdp_request(dhcp_msgtype_t dhcpmsg, interface_t * if_p,
	     u_char * rxpkt, int n, dhcpol_t * rq_options, 
	     struct in_addr * dstaddr_p, struct timeval * time_in_p)
{
    u_char		arch[256];
    char		bsdp_buf[DHCP_OPTION_SIZE_MAX];
    dhcpoa_t		bsdp_options;
    void *		classid;
    int			classid_len;
    PLCacheEntry_t *	entry;
    u_char *		idstr = NULL;
    dhcpoa_t		options;
    u_char		err[256];
    struct dhcp *	rq = (struct dhcp *)rxpkt;
    u_int16_t		reply_port = IPPORT_BOOTPC;
    struct dhcp *	reply = NULL;
    dhcpol_t		rq_vsopt;
    u_char		sysid[256];
    u_int16_t *		vers;

    if (dhcpmsg != dhcp_msgtype_discover_e
	&& dhcpmsg != dhcp_msgtype_inform_e) {
	return;
    }
    classid = dhcpol_find(rq_options, dhcptag_vendor_class_identifier_e,
			  &classid_len, NULL);
    if (classid == NULL 
	|| bsdp_parse_class_id(classid, classid_len, arch, sysid) == FALSE) {
	return; /* not BSDP */
    }
    if (strcmp(arch, "ppc")) {
	return; /* unsupported architecture */
    }

    /* parse the vendor-specific option area */
    if (dhcpol_parse_vendor(&rq_vsopt, rq_options, err) == FALSE) {
	if (!quiet) {
	    syslog(LOG_INFO, 
		   "NetBoot: parse vendor specific options failed, %s", err);
	}
	goto no_reply;
    }

    /* check the client version */
    vers = (u_int16_t *)dhcpol_find(&rq_vsopt, bsdptag_version_e, NULL, NULL);
    if (vers == NULL) {
	if (!quiet) {
	    syslog(LOG_INFO, "NetBoot: BSDP version missing");
	}
	goto no_reply;
    }
    if (ntohs(*vers) != BSDP_VERSION) {
	if (!quiet) {
	    syslog(LOG_INFO, "NetBoot: unsupported BSDP version %d",
		   ntohs(*vers));
	}
	goto no_reply;
    }

    idstr = identifierToString(rq->dp_htype, rq->dp_chaddr, rq->dp_hlen);
    if (!quiet) {
	syslog(LOG_INFO, "BSDP %s [%s] %s arch=%s sysid=%s", 
	       dhcp_msgtype_names(dhcpmsg), 
	       if_name(if_p), idstr, arch, sysid);
    }
    entry = PLCache_lookup_identifier(&S_clients.list, idstr,
				      NULL, NULL, NULL, NULL);
    switch (dhcpmsg) {
      case dhcp_msgtype_discover_e: { /* DISCOVER */
	  u_int32_t	image_id;

	  if (strchr(testing_control, 'd')) {
	      printf("NetBoot: Ignoring DISCOVER\n");
	      goto no_reply;
	  }

	  if (entry == NULL) {
	      goto no_reply;
	  }
	  /* reply with a BSDP OFFER packet */
	  reply = make_bsdp_reply((struct dhcp *)txbuf, sizeof(txbuf),
				  if_inet_addr(if_p), dhcp_msgtype_offer_e,
				  rq, &options);
	  if (reply == NULL) {
	      goto no_reply;
	  }
	  /* client has no IP address yet */
	  reply->dp_ciaddr.s_addr = 0;
	  reply->dp_yiaddr.s_addr = 0;

	  /* set the selected image id property */
	  dhcpoa_init(&bsdp_options, bsdp_buf, sizeof(bsdp_buf));
	  if (S_prop_u_int32(&entry->pl, NIPROP_NETBOOT_IMAGE_ID, &image_id) 
	      == FALSE) {
	      syslog(LOG_INFO, "NetBoot: [%s] image id invalid for %s", idstr);
	      goto no_reply;
	  }
	  image_id = htonl(image_id); /* put into network order */
	  if (dhcpoa_add(&bsdp_options, 
			 bsdptag_selected_boot_image_e,
			 sizeof(image_id), &image_id)
	      != dhcpoa_success_e) {
	      syslog(LOG_INFO, 
		     "NetBoot: [%s] add selected image id failed, %s", idstr,
		     dhcpoa_err(&bsdp_options));
	      goto no_reply;
	  }

	  if (S_client_update(entry, reply, idstr, if_inet_addr(if_p),
			      image_id, &options) == FALSE) {
	      /* send an ACK[FAILED] XXX */

	      goto no_reply; /* XXX for now */
	  }

	  if (dhcpoa_add(&bsdp_options, dhcptag_end_e, 0, 0) 
	      != dhcpoa_success_e) {
	      syslog(LOG_INFO, 
		     "NetBoot: [%s] add BSDP end tag failed, %s",
		     dhcpoa_err(&bsdp_options));
	      goto no_reply;
	  }
	  /* add the BSDP options to the packet */
	  if (dhcpoa_add(&options, dhcptag_vendor_specific_e,
			 dhcpoa_used(&bsdp_options), &bsdp_buf)
	      != dhcpoa_success_e) {
	      syslog(LOG_INFO, "NetBoot: [%s] add vendor specific failed, %s",
		     idstr, dhcpoa_err(&options));
	      goto no_reply;
	  }
	  if (dhcpoa_add(&options, dhcptag_end_e, 0, NULL)
	      != dhcpoa_success_e) {
	      syslog(LOG_INFO, "NetBoot: [%s] add dhcp options end failed, %s",
		     idstr, dhcpoa_err(&bsdp_options));
	      goto no_reply;
	  }
	  { /* send a reply */
	      int 		size;
	      
	      reply->dp_siaddr = if_inet_addr(if_p);
	      strcpy(reply->dp_sname, server_name);

	      size = sizeof(struct dhcp) + sizeof(rfc_magic) +
		  dhcpoa_used(&options);
	      if (size < sizeof(struct bootp)) {
		  /* pad out to BOOTP-sized packet */
		  size = sizeof(struct bootp);
	      }
	      if (sendreply(if_p, (struct bootp *)reply, size, 
			    FALSE, &reply->dp_yiaddr)) {
		  if (!quiet) {
		      syslog(LOG_INFO, "BSDP OFFER sent [%s] pktsize %d",
			     idstr, size);
		  }
	      }
	  }
	  break;
      } /* DISCOVER */
    
      case dhcp_msgtype_inform_e: { /* INFORM */
	  unsigned char		msgtype;
	  u_int16_t *		port;
	  void *		ptr;

	  port = (u_int16_t *)dhcpol_find(&rq_vsopt, bsdptag_reply_port_e, 
					NULL, NULL);
	  if (port) { /* client wants reply on alternate port */
	      reply_port = ntohs(*port);
	      if (reply_port >= IPPORT_RESERVED)
		  goto no_reply; /* client must be on privileged port */
	  }

	  if (rq->dp_ciaddr.s_addr == 0) {
	      if (!quiet) {
		  syslog(LOG_INFO, "NetBoot: [%s] INFORM with no IP address",
			 idstr);
	      }
	      goto no_reply;
	  }
	  ptr = dhcpol_find(&rq_vsopt, bsdptag_message_type_e, NULL, NULL);
	  if (ptr == NULL) {
	      if (!quiet) {
		  syslog(LOG_INFO, "NetBoot: [%s] BSDP message type missing",
			 idstr);
	      }
	      goto no_reply;
	  }
	  msgtype = *(unsigned char *)ptr;

	  /* ready the vendor-specific option area to hold bsdp options */
	  dhcpoa_init(&bsdp_options, bsdp_buf, sizeof(bsdp_buf));

	  switch (msgtype) {
	    case bsdp_msgtype_list_e: {
		bsdp_priority_t	priority;
		u_int32_t	default_image_id = htonl(DEFAULT_IMAGE_ID); /* XXX */
		u_int32_t	image_id;

		/* reply with an ACK[LIST] packet */
		reply = make_bsdp_reply((struct dhcp *)txbuf, sizeof(txbuf),
					if_inet_addr(if_p), dhcp_msgtype_ack_e,
					rq, &options);
		if (reply == NULL)
		    goto no_reply;
		/* formulate the BSDP options */
		if (dhcpoa_add(&bsdp_options, bsdptag_message_type_e,
			       sizeof(msgtype), &msgtype) 
		    != dhcpoa_success_e) {
		    syslog(LOG_INFO, "NetBoot: [%s] add message type failed, %s",
			   idstr, dhcpoa_err(&bsdp_options));
		    goto no_reply;
		}
		if (PLCache_count(&S_clients.list) > server_priority) {
		    priority = 0;
		}
		else {
		    priority = htons(server_priority - 
				     PLCache_count(&S_clients.list));
		}
		if (dhcpoa_add(&bsdp_options, bsdptag_server_priority_e,
			       sizeof(priority), &priority) 
		    != dhcpoa_success_e) {
		    syslog(LOG_INFO, "NetBoot: [%s] add priority failed, %s", 
			   idstr, dhcpoa_err(&bsdp_options));
		    goto no_reply;
		}
		if (dhcpoa_add(&bsdp_options, bsdptag_default_boot_image_e,
			       sizeof(default_image_id), &default_image_id)
		    != dhcpoa_success_e) {
		    syslog(LOG_INFO, 
			   "NetBoot: [%s] add default image id failed, %s",
			   idstr, dhcpoa_err(&bsdp_options));
		    goto no_reply;
		}
		if (entry) {
		    if (strchr(testing_control, 'e')) {
			printf("NetBoot: Not supplying selected image id\n");
		    }
		    else {
			if (S_prop_u_int32(&entry->pl, 
					   NIPROP_NETBOOT_IMAGE_ID, 
					   &image_id) == FALSE) {
			    syslog(LOG_INFO, 
				   "NetBoot: [%s] image id invalid for %s", 
				   idstr);
			    goto no_reply;
			}
			if (dhcpoa_add(&bsdp_options, 
				       bsdptag_selected_boot_image_e,
				       sizeof(image_id), &image_id)
			    != dhcpoa_success_e) {
			    syslog(LOG_INFO, 
				   "NetBoot: [%s] add selected"
				   " image id failed, %s",
				   idstr, dhcpoa_err(&bsdp_options));
			    goto no_reply;
			}
		    }
		}
		if (dhcpoa_add(&bsdp_options, 
			       bsdptag_boot_image_list_e,
			       strlen(BSDP_BOOT_IMAGE_LIST_PATH),
			       BSDP_BOOT_IMAGE_LIST_PATH)
		    != dhcpoa_success_e) {
		    syslog(LOG_INFO, 
			   "NetBoot: [%s] add boot image list failed, %s",
			   idstr, dhcpoa_err(&bsdp_options));
		    goto no_reply;
		}
		if (dhcpoa_add(&bsdp_options, dhcptag_end_e, 0, NULL)
		    != dhcpoa_success_e) {
		    syslog(LOG_INFO, 
			   "NetBoot: [%s] add bsdp options end failed, %s",
			   idstr, dhcpoa_err(&bsdp_options));
		    goto no_reply;
		}

		/* add the BSDP options to the packet */
		if (dhcpoa_add(&options, dhcptag_vendor_specific_e,
			       dhcpoa_used(&bsdp_options), &bsdp_buf)
		    != dhcpoa_success_e) {
		    syslog(LOG_INFO, 
			   "NetBoot: [%s] add vendor specific failed, %s",
			   idstr, dhcpoa_err(&options));
		    goto no_reply;
		}
		if (dhcpoa_add(&options, dhcptag_end_e, 0, NULL)
		    != dhcpoa_success_e) {
		    syslog(LOG_INFO, 
			   "NetBoot: [%s] add dhcp options end failed, %s",
			   idstr, dhcpoa_err(&bsdp_options));
		    goto no_reply;
		}
		break;
	    }
	    case bsdp_msgtype_select_e: {
		bsdp_image_id_t	image_id;
		u_int32_t *	selected_image_id;
		struct in_addr *server_id;

		server_id = (struct in_addr *)
		    dhcpol_find(&rq_vsopt, bsdptag_server_identifier_e, 
				NULL, NULL);
		if (server_id == NULL) {
		    if (!quiet) {
			syslog(LOG_INFO, 
			       "NetBoot: [%s] INFORM[SELECT] missing server id",
			       idstr);
		    }
		    goto no_reply;
		}
		if (server_id->s_addr != if_inet_addr(if_p).s_addr) {
		    if (debug)
			printf("client selected %s\n", inet_ntoa(*server_id));
		    if (entry) {
			/* we have a binding, delete or mark for deletion */
			(void)S_client_remove(&entry);
		    }
		    goto no_reply;
		}
		selected_image_id = (u_int32_t *)
		    dhcpol_find(&rq_vsopt, bsdptag_selected_boot_image_e,
				NULL, NULL);
		if (selected_image_id == NULL) {
		    if (!quiet) {
			syslog(LOG_INFO,
			       "NetBoot: [%s] INFORM[SELECT] missing "
			       "selected boot image", idstr);
		    }
		    goto no_reply;
		}
		image_id = ntohl(*selected_image_id);
		/* reply with an ACK[SELECT] packet */
		reply = make_bsdp_reply((struct dhcp *)txbuf, sizeof(txbuf),
					if_inet_addr(if_p), dhcp_msgtype_ack_e,
					rq, &options);
		if (reply == NULL)
		    goto no_reply;

		/* formulate the BSDP options */
		if (dhcpoa_add(&bsdp_options, bsdptag_message_type_e,
			       sizeof(msgtype), &msgtype) 
		    != dhcpoa_success_e) {
		    syslog(LOG_INFO, "NetBoot: [%s] add message type failed, %s",
			   idstr, dhcpoa_err(&bsdp_options));
		    goto no_reply;
		}
		if (dhcpoa_add(&bsdp_options, bsdptag_selected_boot_image_e,
			       sizeof(*selected_image_id), selected_image_id)
		    != dhcpoa_success_e) {
		    syslog(LOG_INFO, 
			   "NetBoot: [%s] add selected image id failed, %s",
			   idstr, dhcpoa_err(&bsdp_options));
		    goto no_reply;
		}
		if (entry) {
		    if (S_client_update(entry, reply, idstr, 
					if_inet_addr(if_p),
					image_id, &options)
			== FALSE) {
			/* send an ACK[FAILED] */
			goto no_reply; /* XXX for now */
		    }
		}
		else {
		    if (S_client_create(reply, idstr, arch, sysid, 
					if_inet_addr(if_p),
					image_id, &options) == FALSE) {
			/* send an ACK[FAILED] */
			goto no_reply; /* XXX for now */
		    }
		}
		if (dhcpoa_add(&bsdp_options, dhcptag_end_e, 0, NULL)
		    != dhcpoa_success_e) {
		    syslog(LOG_INFO, 
			   "NetBoot: [%s] add bsdp options end failed, %s",
			   idstr, dhcpoa_err(&bsdp_options));
		    goto no_reply;
		}

		/* add the BSDP options to the packet */
		if (dhcpoa_add(&options, dhcptag_vendor_specific_e,
			       dhcpoa_used(&bsdp_options), &bsdp_buf)
		    != dhcpoa_success_e) {
		    syslog(LOG_INFO, 
			   "NetBoot: [%s] add vendor specific failed, %s",
			   idstr, dhcpoa_err(&options));
		    goto no_reply;
		}
		if (dhcpoa_add(&options, dhcptag_end_e, 0, NULL)
		    != dhcpoa_success_e) {
		    syslog(LOG_INFO, 
			   "NetBoot: [%s] add dhcp options end failed, %s",
			   idstr, dhcpoa_err(&bsdp_options));
		    goto no_reply;
		}
		break;
	    }
	    default: {
		/* invalid request */
		if (!quiet) {
		    syslog(LOG_INFO, "NetBoot: [%s] invalid BSDP message %d",
			   idstr, msgtype);
		}
		goto no_reply;
		break;
	    }
	  }
	  { /* send a reply */
	      int size;
	      
	      reply->dp_siaddr = if_inet_addr(if_p);
	      strcpy(reply->dp_sname, server_name);

	      size = sizeof(struct dhcp) + sizeof(rfc_magic) +
		  dhcpoa_used(&options);
	      if (size < sizeof(struct bootp)) {
		  /* pad out to BOOTP-sized packet */
		  size = sizeof(struct bootp);
	      }
	      if (bootp_transmit(bootp_socket, transmit_buffer, if_name(if_p),
				 rq->dp_htype, NULL, 0, 
				 rq->dp_ciaddr, if_inet_addr(if_p),
				 reply_port, IPPORT_BOOTPS,
				 reply, size) < 0) {
		  syslog(LOG_INFO, "send failed, %m");
	      }
	      else {
		  if (debug && verbose) {
		      printf("\n=================== Server Reply ===="
			     "=================\n");
		      dhcp_print_packet(reply, size);
		  }
		  if (!quiet) {
		      syslog(LOG_INFO, "NetBoot: [%s] BSDP ACK[%s] sent %s "
			     "pktsize %d", idstr, 
			     bsdp_msgtype_names(msgtype),
			     inet_ntoa(rq->dp_ciaddr), size);
		  }
	      }
	  }
	  break;
      } /* INFORM */
      default: {
	  /* invalid request */
	  if (!quiet) {
	      syslog(LOG_INFO, "NetBoot: [%s] DHCP message %s not supported",
		     idstr, dhcp_msgtype_names(dhcpmsg));
	  }
	  goto no_reply;
	  break;
      }
    }

 no_reply:
    if (idstr)
	free(idstr);
    dhcpol_free(&rq_vsopt);
    return;
}


struct dhcp * 
make_bsdp_bootp_reply(struct dhcp * reply, int pkt_size, 
		      struct dhcp * request, dhcpoa_t * options)
{
    *reply = *request;
    reply->dp_hops = 0;
    reply->dp_secs = 0;
    reply->dp_op = BOOTREPLY;
    bcopy(rfc_magic, reply->dp_options, sizeof(rfc_magic));
    dhcpoa_init(options, reply->dp_options + sizeof(rfc_magic),
		DHCP_MIN_OPTIONS_SIZE - sizeof(rfc_magic));
    return (reply);
}

boolean_t
old_netboot_request(interface_t * if_p,
		    u_char * rxpkt, int n, dhcpol_t * rq_options, 
		    struct in_addr * dstaddr_p, struct timeval * time_in_p)
{
    PLCacheEntry_t *	bsdp_entry = NULL;
    struct in_addr	iaddr = {0};
    u_char *		idstr = NULL;
    dhcpoa_t		options;
    struct dhcp *	rq = (struct dhcp *)rxpkt;
    struct dhcp *	reply = NULL;
    id			subnet = nil;
    u_int32_t		version = 0;

    if (macNC_get_client_info((struct dhcp *)rxpkt, n, 
			      rq_options, NULL) == FALSE) {
	return (FALSE);
    }
    idstr = identifierToString(rq->dp_htype, rq->dp_chaddr, rq->dp_hlen);
    if (idstr == NULL) {
	return (FALSE);
    }
    if (!quiet) {
	syslog(LOG_INFO, "NetBoot[BOOTP]: [%s] %s",
	       if_name(if_p), idstr);
    }
    if (dhcp_bootp_allocate(idstr, idstr, rq, if_p, time_in_p, &iaddr,
			    &subnet) == FALSE) {
	/* no client binding available */
	goto no_reply;
    }
    bsdp_entry = PLCache_lookup_identifier(&S_clients.list, idstr,
					   NULL, NULL, NULL, NULL);

    reply = make_bsdp_bootp_reply((struct dhcp *)txbuf, sizeof(txbuf),
				  rq, &options);
    if (reply == NULL)
	goto no_reply;

    reply->dp_yiaddr = iaddr;
    reply->dp_siaddr = if_inet_addr(if_p); /* XXX */
    strcpy(reply->dp_sname, server_name);

    /* add the client-specified parameters */
    (void)add_subnet_options(NULL, NULL, iaddr, if_p, &options, NULL, 0);

    if (bsdp_entry) {
	if (S_client_update(bsdp_entry, reply, idstr, if_inet_addr(if_p),
			    DEFAULT_IMAGE_ID, &options) 
	    == FALSE) {
	    goto no_reply;
	}
    }
    else {
	if (S_client_create(reply, idstr, "ppc", "unknown", 
			    if_inet_addr(if_p), DEFAULT_IMAGE_ID, 
			    &options) == FALSE) {
	    goto no_reply;
	}
    }
    if (dhcpoa_add(&options, macNCtag_server_version_e, 
		   sizeof(version), &version) != dhcpoa_success_e) {
	goto no_reply;
    }
    if (dhcpoa_add(&options, dhcptag_end_e, 0, NULL)
	!= dhcpoa_success_e) {
	syslog(LOG_INFO, 
	       ": [%s] add dhcp options end failed, %s",
	       idstr, dhcpoa_err(&options));
	goto no_reply;
    }
    { /* send a reply */
	int size;
	
	reply->dp_siaddr = if_inet_addr(if_p);
	strcpy(reply->dp_sname, server_name);
	
	size = sizeof(struct dhcp) + sizeof(rfc_magic) +
	    dhcpoa_used(&options);
	if (size < sizeof(struct bootp)) {
	    /* pad out to BOOTP-sized packet */
	    size = sizeof(struct bootp);
	}
	if (sendreply(if_p, (struct bootp *)reply, size, 
		      FALSE, &iaddr)) {
	    if (!quiet) {
		syslog(LOG_INFO, "NetBoot[BOOTP]: reply sent %s pktsize %d",
		       inet_ntoa(iaddr), size);
	    }
	}
    }


 no_reply:
    if (idstr)
	free(idstr);
    return (TRUE);
}

