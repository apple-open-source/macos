/*
 * Copyright (c) 2001-2002 Apple Inc. All rights reserved.
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
 *
 */

/*
 * LinkAddresses.c
 * - get the list of link-level addresses 
 */
/*
 * Modification History
 *
 * November 1, 2001		Dieter Siegmund (dieter@apple.com)
 * - created
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <sys/socket.h>
#include <sys/filio.h>
#include <errno.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/route.h>
#include <string.h>

#include "LinkAddresses.h"	

struct LinkAddresses_s {
    struct sockaddr_dl * *	list;
    int				count;
};

void
sockaddr_dl_print(struct sockaddr_dl * dl_p)
{
    int i;

    printf("link: len %d index %d family %d type 0x%x nlen %d alen %d"
	   " slen %d addr ", dl_p->sdl_len, 
	   dl_p->sdl_index,  dl_p->sdl_family, dl_p->sdl_type,
	   dl_p->sdl_nlen, dl_p->sdl_alen, dl_p->sdl_slen);
    for (i = 0; i < dl_p->sdl_alen; i++) 
	printf("%s%x", i ? ":" : "", 
	       ((unsigned char *)dl_p->sdl_data + dl_p->sdl_nlen)[i]);
    printf("\n");
}

#ifdef TEST_LINKADDRESSES
static void
LinkAddresses_print(LinkAddressesRef link_addrs)
{
    int i;
    for (i = 0; i < link_addrs->count; i++) {
	struct sockaddr_dl * sdl = link_addrs->list[i];
	printf("%.*s ", sdl->sdl_nlen, sdl->sdl_data);
	sockaddr_dl_print(sdl);
    }
}
#endif TEST_LINKADDRESSES

struct sockaddr_dl *
LinkAddresses_lookup(LinkAddressesRef list, char * ifname)
{
    int i;
    int len = strlen(ifname);

    for (i = 0; i < list->count; i++) {
	struct sockaddr_dl * sdl = list->list[i];

	if (strncmp(sdl->sdl_data, ifname, sdl->sdl_nlen) == 0
	    && len == sdl->sdl_nlen) {
	    return (sdl);
	}
    }
    return (NULL);
}

int
LinkAddresses_count(LinkAddressesRef list)
{
    return (list->count);
}

struct sockaddr_dl *
LinkAddresses_element(LinkAddressesRef list, int i)
{
    if (i >= 0 && i < list->count) {
	return (list->list[i]);
    }
    return (NULL);
}

void
LinkAddresses_free(LinkAddressesRef * list_p)
{
    int i;
    LinkAddressesRef list = *list_p;

    if (list) {
	for (i = 0; i < list->count; i++) {
	    free(list->list[i]);
	    list->list[i] = NULL;
	}
	free(list->list);
	list->list = NULL;
	list->count = 0;
	free(list);
    }
    *list_p = NULL;
    return;
}

LinkAddressesRef
LinkAddresses_create()
{
    char *			buf = NULL;
    size_t			buf_len = 0;
    int				mib[6];
    int				offset = 0;
    struct sockaddr_dl * *	list = NULL;
    int				list_size = 10;
    int				list_count = 0;
    LinkAddressesRef		ret_list = NULL;

    mib[0] = CTL_NET;
    mib[1] = PF_ROUTE;
    mib[2] = 0;
    mib[3] = AF_LINK;
    mib[4] = NET_RT_IFLIST;
    mib[5] = 0;

    if (sysctl(mib, 6, NULL, &buf_len, NULL, 0) < 0) {
	fprintf(stderr, "sysctl() size failed: %s", strerror(errno));
	goto failed;
    }
    buf = malloc(buf_len);
    if (sysctl(mib, 6, buf, &buf_len, NULL, 0) < 0) {
	fprintf(stderr, "sysctl() failed: %s", strerror(errno));
	goto failed;
    }
    list = (struct sockaddr_dl * *)malloc(sizeof(*list) * list_size);
    if (list == NULL) {
	goto failed;
    }
    offset = 0;
    while (offset < buf_len) {
	struct if_msghdr * 	ifm;
	struct sockaddr_dl *	sdl;
	struct sockaddr_dl * 	new_p;

	ifm = (struct if_msghdr *)&buf[offset];

	switch (ifm->ifm_type) {
	case RTM_IFINFO:
	    /* get interface name */
	    sdl = (struct sockaddr_dl *)(ifm + 1);
	    if (list_count == list_size) {
		list_size *= 2;
		list = realloc(list, sizeof(*list) * list_size);
		if (list == NULL) {
		    goto failed;
		}
	    }
	    new_p = (void *)malloc(sdl->sdl_len);
	    if (new_p == NULL) {
		break;
	    }
	    bcopy(sdl, new_p, sdl->sdl_len);
	    list[list_count++] = new_p;
	    break;

	default:
	    break;
	}
	/* advance to next address/interface */
	offset += ifm->ifm_msglen;
    }
    if (list_count == 0) {
	free(list);
	list = NULL;
    }
    else if (list_count < list_size) {
	list = realloc(list, sizeof(*list) * list_count);
    }
    if (list) {
	ret_list = (void *)malloc(sizeof(*ret_list));
	if (ret_list) {
	    ret_list->list = list;
	    ret_list->count = list_count;
	}
	else {
	    free(list);
	    list = NULL;
	}
    }
 failed:
    if (buf) {
	free(buf);
    }
    return (ret_list);
}

#ifdef TEST_LINKADDRESSES
int
main()
{
    LinkAddressesRef	link_addrs;
    
    link_addrs = LinkAddresses_create();
    if (link_addrs) {
	LinkAddresses_print(link_addrs);
	LinkAddresses_free(&link_addrs);
    }
    exit(0);
}
#endif TEST_LINKADDRESSES
