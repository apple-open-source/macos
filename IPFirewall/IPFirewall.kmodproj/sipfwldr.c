/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All rights reserved.
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
 * sipfwldr.c
 *
 * by Vincent Lubet
 * 
 * Global PF_NDRV socket filter that installs the IP firewall socket filter before the Shared
 * IP socket filter.
 * The idea is to intercept the sobind call and look for the Shared IP socket filter
 * It's too early to intercept socreate because the Shared IP socket filter is not
 * not yet installed.
 */

#include <mach/mach_types.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>
#include <net/kext_net.h>
#include <machine/spl.h>

#include "sipfw.h"

extern struct NFDescriptor *find_nke(unsigned int handle);
extern int nke_insert(struct socket *so, struct so_nke *np);

static int sipfwldr_sobind(struct socket *so, struct sockaddr *sa, struct kextcb *kp);
static int sipfwldr_socreate(struct socket *so, struct protosw *proto, struct kextcb *kp);
static int sipfwldr_sofree(struct socket *so, struct kextcb *kp);

static TAILQ_HEAD(sipfwldr_so_head, sipfwldr_so_entry) sipfwldr_so_head = TAILQ_HEAD_INITIALIZER(sipfwldr_so_head);

struct sipfwldr_so_entry {
    TAILQ_ENTRY(sipfwldr_so_entry) 	link;
    struct socket			*so;
};

static int ipfwldr_use_count = 0;
static int sipfwldr_inited = 0;

SYSCTL_DECL(_net_inet_ip_fw_classic);
SYSCTL_INT(_net_inet_ip_fw_classic, OID_AUTO, ldrusecount, CTLFLAG_RW, &ipfwldr_use_count, 0, "");


static struct sockif sipfwldr_sockif = {
    NULL,		/* soabort */
    NULL,		/* soaccept */
    sipfwldr_sobind,	/* sobind */
    NULL,		/* soclose */
    NULL,		/* soconnect */
    NULL,		/* soconnect2 */
    NULL,		/* soset/getopt */
    sipfwldr_socreate,	/* socreate */
    NULL,		/* sodisconnect */
    sipfwldr_sofree,	/* sofree */
    NULL,		/* sogetopt */
    NULL,		/* sohasoutofband */
    NULL,		/* solisten */
    NULL,		/* soreceive */
    NULL,		/* sorflush */
    NULL,		/* sosend */
    NULL,		/* sosetopt */
    NULL,		/* soshutdown */
    NULL,		/* socantrcvmore */
    NULL,		/* socantsendmore */
    NULL,		/* soisconnected */
    NULL,		/* soisconnecting */
    NULL,		/* soisdisconnected */
    NULL,		/* soisdisconnecting */
    NULL,		/* sonewconn1 */
    NULL,		/* soqinsque */
    NULL,		/* soqremque */
    NULL,		/* soreserve */
    NULL,		/* sowakeup */
};

static struct sockutil sipfwldr_sockutil = {
    NULL, /* sb_lock */
    NULL, /* sbappend */
    NULL, /* sbappendaddr */
    NULL, /* sbappendcontrol */
    NULL, /* sbappendrecord */
    NULL, /* sbcompress */
    NULL, /* sbdrop */
    NULL, /* sbdroprecord */
    NULL, /* sbflush */
    NULL, /* sbinsertoob */
    NULL, /* sbrelease */
    NULL, /* sbreserve */
    NULL, /* sbwait */
};

static struct NFDescriptor sipfwldr_nfd =
{
    {NULL, NULL},
    {NULL, NULL},
    SIPFWLDR_NFHANDLE,
    NFD_GLOBAL|NFD_VISIBLE,	/* only if we want global filtering */
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    &sipfwldr_sockif,
    &sipfwldr_sockutil
};


int sipfwldr_sobind(struct socket *so, struct sockaddr *sa, struct kextcb *kp)
{
    struct NFDescriptor *sipfw_nfd;
    int error;

    printf("sipfwldr_sobind: so=%x sa=%x kp=%x\n", so, sa, kp);

    sipfw_nfd = find_nke(SIPFW_NFHANDLE);
    if (sipfw_nfd) {
        struct so_nke sipfw_nke;

        sipfw_nke.nke_handle = SIPFW_NFHANDLE;
        sipfw_nke.nke_where  = 0;
        sipfw_nke.nke_flags  = NFF_BEFORE;

        if ((error = nke_insert(so, &sipfw_nke))) {
            printf("sipfwldr_socreate nke_insert failed %d\n", error);
        }
    } else
        printf("sipfw nfd not found\n");

    return 0;
}

int sipfwldr_socreate(struct socket *so, struct protosw *proto, struct kextcb *kp)
{
    int							err = 0;
    struct sipfwldr_so_entry	*entry;
    
    printf("sipfwldr_socreate: so=%x protp=%x kp=%x\n", so, proto, kp);

    if ((entry = _MALLOC(sizeof (struct sipfwldr_so_entry), M_TEMP, M_WAITOK)) == NULL) {
        err = ENOBUFS;
        goto done;
    }
    entry->so = so;
    TAILQ_INSERT_TAIL(&sipfwldr_so_head, entry, link);
    ipfwldr_use_count++;
done:
    return 0;
}

int sipfwldr_sofree(struct socket *so, struct kextcb *kp)
{
    int							err = 0;
    struct sipfwldr_so_entry	*entry;

    printf("sipfwldr_sofree: so=%x kp=%x\n", so, kp);
    
    for (entry = sipfwldr_so_head.tqh_first; entry != NULL; entry = entry->link.tqe_next) {
        if (entry->so == so) {
            TAILQ_REMOVE(&sipfwldr_so_head, entry, link);
            FREE(entry, M_TEMP);
            ipfwldr_use_count--;
        }
    }
    return err;
}

kern_return_t sipfwldr_load()
{
    kern_return_t	err = KERN_FAILURE;
    struct protosw *pp;
    spl_t spl;

    spl = splnet();

    if ((pp = pffindproto(PF_NDRV, 0, SOCK_RAW)) == NULL)
    {
        goto done;
    }
    if (register_sockfilter(&sipfwldr_nfd, NULL, pp, 0))
    {
        goto done;
    }
    sysctl_register_oid(&sysctl__net_inet_ip_fw_classic_ldrusecount);
    sipfwldr_inited = 1;
    err = KERN_SUCCESS;
done:
    splx(spl);

    return err;
}

kern_return_t sipfwldr_unload()
{
    kern_return_t	err = KERN_FAILURE;
    struct protosw *pp;
    spl_t spl;

    spl = splnet();

    sysctl_unregister_oid(&sysctl__net_inet_ip_fw_classic_ldrusecount);

    if ((pp = pffindproto(PF_NDRV, 0, SOCK_RAW)) == NULL)
    {
        goto done;
    }
    if (unregister_sockfilter(&sipfwldr_nfd, pp, 0))
    {
        goto done;
    }
    err = KERN_SUCCESS;
    sipfwldr_inited = 0;
done:
    splx(spl);

    return err;
}



