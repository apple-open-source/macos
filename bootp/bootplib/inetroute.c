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
 * inetroute.c
 * - get a list of internet network routes
 */

/*
 * Modification History
 *
 * Dieter Siegmund (dieter@apple.com) Tue Jul 14 11:33:50 PDT 1998
 * - created
 */
#import <sys/param.h>
#import <sys/socket.h>
#import <sys/mbuf.h>
#import <net/if.h>
#import <net/if_dl.h>
#import <net/if_types.h>
#import <net/route.h>
#import <netinet/in.h>
#import <sys/sysctl.h>
#import <netdb.h>
#import <stdio.h>
#import <stdlib.h>
#import <string.h>
#import <unistd.h>
#import <sys/cdefs.h>
#import <sys/types.h>
#import <sys/socket.h>
#import <arpa/inet.h> /* has inet_ntoa, etc. */

#import "inetroute.h"
#import "util.h"

#define INDEX_NONE	-1

inetroute_list_t *
inetroute_list_init()
{
    char *		buf = NULL;
    char *		lim;
    inetroute_list_t *	list_p = NULL;
    int			list_size = 2;
    int 		mib[6];
    size_t 		needed;
    char * 		next;
    struct rt_msghdr *	rtm;

    mib[0] = CTL_NET;
    mib[1] = PF_ROUTE;
    mib[2] = 0;
    mib[3] = 0;
    mib[4] = NET_RT_DUMP;
    mib[5] = 0;
    if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0) { 
	perror("route-sysctl-estimate"); 
	goto err;
    }
    if ((buf = malloc(needed)) == 0) {
	goto err;
    }
    if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0) {
	perror("sysctl of routing table");
	goto err;
    }
    list_p = (inetroute_list_t *)malloc(sizeof(*list_p));
    list_p->def_index = INDEX_NONE;
    if (list_p == NULL)
	goto err;
    list_p->count = 0;
    list_p->list = (inetroute_t *)malloc(sizeof(*(list_p->list)) * list_size);
    if (list_p->list == NULL)
	goto err;
    lim = buf + needed;
    for (next = buf; next < lim; next += rtm->rtm_msglen) {
	void *		  addrs;
	struct sockaddr * dst = NULL;
	struct sockaddr * gateway = NULL;
	struct sockaddr * mask = NULL;

	rtm = (struct rt_msghdr *)next;
	addrs = (void *)(&rtm[1]);
	if (rtm->rtm_addrs & RTA_DST) {
	    dst = (struct sockaddr *)addrs;
	    addrs += MAX(sizeof(*dst), dst->sa_len);
	}
	if (rtm->rtm_addrs & RTA_GATEWAY) {
	    gateway = (struct sockaddr *)addrs;
	    addrs += MAX(sizeof(*dst), gateway->sa_len);
	}
	if (rtm->rtm_addrs & RTA_NETMASK) {
	    mask = (struct sockaddr *)addrs;
	}

	if (dst && dst->sa_family == AF_INET 
	    && gateway 
	    && mask
	    && !(rtm->rtm_flags & RTF_HOST)) {
	    struct sockaddr_in * dst_p = (struct sockaddr_in *)dst;
	    struct sockaddr_in * mask_p = (struct sockaddr_in *)mask;
	    inetroute_t * entry;
	    if (list_p->count == list_size) {
		list_size *= 2;
		list_p->list = (inetroute_t *) 
		    realloc(list_p->list, sizeof(*(list_p->list)) * list_size);
		if (list_p->list == NULL)
		    goto err;
	    }
	    entry = list_p->list + list_p->count;
	    bzero(entry, sizeof(*entry));
	    entry->dest = dst_p->sin_addr;
	    entry->mask = mask_p->sin_addr;
	    if (dst_p->sin_addr.s_addr == htonl(INADDR_ANY)) {
		list_p->def_index = list_p->count;
	    }
	    if (gateway->sa_family == AF_LINK) {
		struct sockaddr_dl * sdl = (struct sockaddr_dl *)gateway;
		entry->gateway.link = *sdl;
	    }
	    else {
		struct sockaddr_in * in_p = (struct sockaddr_in *)gateway;
		entry->gateway.inet = *in_p;
	    }
	    list_p->count++;
	}
    }
    free(buf);
    return (list_p);
  err:
    if (buf)
	free(buf);
    inetroute_list_free(&list_p);
    return (NULL);
}

void
inetroute_list_free(inetroute_list_t * * list)
{
    if (list != NULL && *list != NULL) {
	if ((*list)->list)
	    free((*list)->list);
	(*list)->list = NULL;
	free(*list);
	*list = NULL;
    }
}

struct in_addr *
inetroute_default(inetroute_list_t * list_p)
{
    if (list_p->def_index != INDEX_NONE) {
	return (&(list_p->list[list_p->def_index].gateway.inet.sin_addr));
    }
    return (NULL);
}

void
inetroute_list_print(inetroute_list_t * list_p)
{
    int i;

    for (i = 0; i < list_p->count; i++) {
	inetroute_t * entry = list_p->list + i;
	if (i == list_p->def_index) {
	    printf("default: %s\n", inet_ntoa(entry->gateway.inet.sin_addr));
	}
	else {
	    printf("%s ==> link %d\n", 
		   inet_nettoa(entry->dest, entry->mask),
		   entry->gateway.link.sdl_index);
	}
    }
}

#ifdef TESTING
main()
{
    inetroute_list_t *	list_p;
    struct in_addr *	def;

    list_p = inetroute_list_init();
    if (list_p == NULL)
	exit(0);
    inetroute_list_print(list_p);
    def = inetroute_default(list_p);
    if (def) {
	printf("default route: %s\n", inet_ntoa(*def));
    }
    inetroute_list_free(&list_p);
    exit(0);
}
#endif
