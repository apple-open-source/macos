/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
/* Copyright (C) 1999 Apple Computer, Inc. */

/*
 * NKE management domain - allows control connections to
 *  an NKE, to read/write data, read/set status.
 *
 * Justin C. Walker, 990319
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/syslog.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <net/kext_net.h>
#include "NKEMgr.h"
#include "NKEMgrvar.h"

#include <mach/vm_types.h>
#include <mach/kmod.h>

#include <machine/spl.h>
#include <kern/thread.h>

/*TAILQ_HEAD(nf_list, NFDescriptor);*/
extern struct nf_list nf_list;
struct nf_list dlnke_list;
extern struct domain NKEdomain;
extern struct NFDescriptor *find_nke(int);
struct NFDescriptor *find_dlnke(int);

/*
 * Defs for DataLink NKE support
 */
extern void dln_input(), dln_init();
extern int dln_output();

int dln_abort(struct socket *),
    dln_attach(struct socket *, int, struct proc *),
    dln_detach(struct socket *),
    dln_connect(struct socket *, struct sockaddr *, struct proc *),
    dln_disconnect(struct socket *), dln_shutdown(struct socket *),
    dln_peeraddr(struct socket *, struct sockaddr **);

struct pr_usrreqs dln_usrreqs =
{	dln_abort, pru_accept_notsupp, dln_attach, pru_bind_notsupp,
	dln_connect, pru_connect2_notsupp, pru_control_notsupp, dln_detach,
	pru_disconnect_notsupp, pru_listen_notsupp, dln_peeraddr,
	pru_rcvd_notsupp, pru_rcvoob_notsupp, pru_send_notsupp,
	pru_sense_null, dln_shutdown, pru_sockaddr_notsupp,
	sosend, soreceive, sopoll
};

struct protosw dlinkNKE =
{	SOCK_RAW, &NKEdomain, NKEPROTO_DLINK, PR_ATOMIC|PR_CONNREQUIRED,
	dln_input, dln_output, NULL, NULL,
	NULL, dln_init,
	NULL, NULL, NULL, NULL, &dln_usrreqs
};

/*
 * Defs for Socket NKE support
 */
extern void skn_init();
extern int skn_output();

int skn_abort(struct socket *),
    skn_attach(struct socket *, int, struct proc *),
    skn_detach(struct socket *),
    skn_connect(struct socket *, struct sockaddr *, struct proc *),
    skn_disconnect(struct socket *), skn_shutdown(struct socket *),
    skn_peeraddr(struct socket *, struct sockaddr **);

struct pr_usrreqs skn_usrreqs =
{	skn_abort, pru_accept_notsupp, skn_attach, pru_bind_notsupp,
	skn_connect, pru_connect2_notsupp, pru_control_notsupp, skn_detach,
	pru_disconnect_notsupp, pru_listen_notsupp, skn_peeraddr,
	pru_rcvd_notsupp, pru_rcvoob_notsupp, pru_send_notsupp,
	pru_sense_null, skn_shutdown, pru_sockaddr_notsupp,
	sosend, soreceive, sopoll
};

struct protosw socketNKE =
{	SOCK_RAW, &NKEdomain, NKEPROTO_SOCKET, PR_ATOMIC|PR_CONNREQUIRED,
	NULL, skn_output, NULL, NULL,
	NULL, skn_init,
	NULL, NULL, NULL, NULL, &skn_usrreqs
};

extern int NKEMgr_start(void);

struct domain NKEdomain =
{	PF_NKE, "NKE", NULL,
	NULL, NULL, NULL, NULL, NULL,
	0, 0, 0, 0
};

/*
 * Install the protosw's for the NKE manager.  Invoked at
 *  extension load time
 */
int
NKEMgr_start(void)
{
    int retval;
    int	funnel_state;

    funnel_state = thread_funnel_set(network_flock, TRUE);

    net_add_domain(&NKEdomain);
    if ((retval = net_add_proto(&socketNKE, &NKEdomain)) == 0)
    {
        if ((retval = net_add_proto(&dlinkNKE, &NKEdomain)) == 0) {
            thread_funnel_set(network_flock, funnel_state);
            return(KERN_SUCCESS);
        }
        net_del_proto(socketNKE.pr_type, socketNKE.pr_protocol,
                      &NKEdomain);
        net_del_domain(&NKEdomain);
    } else
        net_del_domain(&NKEdomain);
    log(LOG_WARNING, "Can't install NKE manager (%d)\n", retval);
    thread_funnel_set(network_flock, funnel_state);
    return(retval);
}

/* Persistence of memory... */
int
NKEMgr_stop(kmod_info_t *ki, void *data)
{
	return(-1);		/* Built to run forever */
}

/*
 * Socket NKE protocol handlers
 */
void
skn_init()
{
}

int
skn_output()
{
	return(EOPNOTSUPP);
}

/*
 * Socket NKE user-request functions
 */
int
skn_attach (struct socket *so, int proto, struct proc *p)
{	struct skn_pcb *skp;

	if (so->so_pcb)
		return(EINVAL);

	skp = (struct skn_pcb *)_MALLOC(sizeof (*skp), M_TEMP, M_WAITOK);
	bzero((char *)skp, sizeof (*skp));
	so->so_pcb = (caddr_t)skp;
	return(0);
}

int
skn_detach(struct socket *so)
{	struct skn_pcb *skp;
	struct NFDescriptor *nfp;

	if ((skp = (struct skn_pcb *)so->so_pcb) != NULL)
	{	if ((nfp = skp->sp_nfd) != NULL)
			(*nfp->nf_disconnect)(so, nfp);
		_FREE(skp, sizeof (*skp));
	}
        return(0);
}

int
skn_connect(struct socket *so, struct sockaddr *nam, struct proc *p)
{	struct skn_pcb *skp;
	struct NFDescriptor *nfp;
	int error = 0, s;

	if ((skp = (struct skn_pcb *)so->so_pcb) != NULL)
	{	int cookie;
		struct sockaddr_nke *sa = (struct sockaddr_nke *)nam;

		if (skp->sp_nfd != NULL)
			return(EISCONN);

		nfp = nf_list.tqh_first;
		cookie = sa->sn_handle;
		nfp = find_nke(cookie);
		if (nfp == NULL)
			return(EADDRNOTAVAIL);

		skp->sp_nfd = nfp;
		s = splnet();
		error = (*nfp->nf_connect)(so);
		splx(s);
		if (error == 0)
			soisconnected(so);
	}
	return(error);
}

int
skn_disconnect(struct socket *so)
{	struct skn_pcb *skp;
	struct NFDescriptor *nfp;
	int s;

	if ((skp = (struct skn_pcb *)so->so_pcb) != NULL)
	{	if ((nfp = skp->sp_nfd) == NULL)
	  		return(ENOTCONN);

		s = splnet();
		(*nfp->nf_disconnect)(so);
		skp->sp_nfd = NULL;
		splx(s);
		so->so_state &= ~SS_ISCONNECTED;/* XXX */
	}
	return(0);
}

int
skn_shutdown(struct socket *so)
{
	socantsendmore(so);
	return(0);
}

int
skn_abort(struct socket *so)
{	struct skn_pcb *skp;
	struct NFDescriptor *nfp;
	int s;

	soisdisconnected(so);
	if ((skp = (struct skn_pcb *)so->so_pcb) != NULL)
	{	if ((nfp = skp->sp_nfd) == NULL)
			return(0);
		s = splnet();
		(*nfp->nf_disconnect)(so);
		skp->sp_nfd = NULL;
		splx(s);
	}
	return(0);
}

int
skn_peeraddr(struct socket *so, struct sockaddr **nam)
{	struct skn_pcb *skp;
	struct NFDescriptor *nfp;

	if ((skp = (struct skn_pcb *)so->so_pcb) != NULL)
	{	struct sockaddr_nke *sa;
		if ((nfp = skp->sp_nfd) == NULL)
			return(ENOTCONN);

		sa = _MALLOC(sizeof (*skp), M_SONAME, M_WAITOK);
		bzero((caddr_t)sa, sizeof (*sa));
		sa->sn_family = AF_INET;
		sa->sn_len = sizeof(*sa);
		sa->sn_handle = nfp->nf_handle;
		*nam = (struct sockaddr *)sa;
	}
        return(0);
}

/*
 * DataLink NKE Protocol handlers
 */

void dln_input()
{
}

void
dln_init()
{
}

int dln_output()
{
	return(EOPNOTSUPP);
}

/*
 * DataLink NKE user-request functions
 */
int
dln_attach (struct socket *so, int proto, struct proc *p)
{	struct dln_pcb *dlp;

	if (so->so_pcb)
		return(EINVAL);

	dlp = (struct dln_pcb *)_MALLOC(sizeof (*dlp), M_TEMP, M_WAITOK);
	bzero((char *)dlp, sizeof (*dlp));
	so->so_pcb = (caddr_t)dlp;
	return(0);
}

int
dln_detach(struct socket *so)
{	struct dln_pcb *dlp;
	struct NFDescriptor *nfp;

	if ((dlp = (struct dln_pcb *)so->so_pcb) != NULL)
	{	if ((nfp = dlp->dp_nfd) != NULL)
			(*nfp->nf_disconnect)(so, nfp);
		_FREE(dlp, sizeof (*dlp));
	}
        return(0);
}

int
dln_connect(struct socket *so, struct sockaddr *nam, struct proc *p)
{	struct dln_pcb *dlp;
	struct NFDescriptor *nfp;
	int error = 0, s;
	unsigned int cookie;

	if ((dlp = (struct dln_pcb *)so->so_pcb) != NULL)
	{	struct sockaddr_nke *sa = (struct sockaddr_nke *)nam;

		if (dlp->dp_nfd != NULL)
			return(EISCONN);

		cookie = sa->sn_handle;
		nfp = find_dlnke(cookie);
		if (nfp == NULL)
			return(EADDRNOTAVAIL);

		dlp->dp_sa = *sa;
		s = splnet();
		error = (*nfp->nf_connect)(so);
		splx(s);
		if (error == 0)
			soisconnected(so);
	}
	return(error);
}

int
dln_disconnect(struct socket *so)
{	struct dln_pcb *dlp;
	struct NFDescriptor *nfp;
	int s;

	if ((dlp = (struct dln_pcb *)so->so_pcb) != NULL)
	{	if ((nfp = dlp->dp_nfd) == NULL)
	  		return(ENOTCONN);

		s = splnet();
		(*nfp->nf_disconnect)(so);
		dlp->dp_nfd = NULL;
		splx(s);
		so->so_state &= ~SS_ISCONNECTED;/* XXX */
	}
	return(0);
}

int
dln_shutdown(struct socket *so)
{
	socantsendmore(so);
	return(0);
}

int
dln_abort(struct socket *so)
{	struct dln_pcb *dlp;
	struct NFDescriptor *nfp;
	int s;

	soisdisconnected(so);
	if ((dlp = (struct dln_pcb *)so->so_pcb) != NULL)
	{	if ((nfp = dlp->dp_nfd) == NULL)
			return(0);
		s = splnet();
		(*nfp->nf_disconnect)(so);
		dlp->dp_nfd = NULL;
		splx(s);
	}
	return(0);
}

int
dln_peeraddr(struct socket *so, struct sockaddr **nam)
{	struct dln_pcb *dlp;
	struct NFDescriptor *nfp;

	if ((dlp = (struct dln_pcb *)so->so_pcb) != NULL)
	{	struct sockaddr_nke *sa;

		if ((nfp = dlp->dp_nfd) == 0)
			return(ENOTCONN);
		sa = _MALLOC(sizeof (*dlp), M_SONAME, M_WAITOK);
		bzero((caddr_t)sa, sizeof (*sa));
		sa->sn_family = AF_INET;
		sa->sn_len = sizeof(*sa);
		sa->sn_handle = nfp->nf_handle;
		*nam = (struct sockaddr *)sa;
	}
        return(0);
}

/*
 * Code for kext_net.c:
 * Register/unregister a datalink NKE
 */
int
register_dlinknke(struct NFDescriptor *nfp, struct NFDescriptor *nfp1,
		    struct protosw *pr, int flags)
{	int s;
	static int DLNKE_initted = 0;

	if (nfp == NULL)
		return(EINVAL);

	s = splhigh();
	if (!DLNKE_initted)
	{	DLNKE_initted = 1;
		TAILQ_INIT(&dlnke_list);
	}

	/*
	 * Install the extension:
	 *  put it in the global list of all datalink NKEs
	 */
	TAILQ_INSERT_TAIL(&dlnke_list, nfp, nf_list);
	splx(s);
	return(0);
}

int
unregister_dlinknke(struct NFDescriptor *nfp, struct protosw *pr, int flags)
{	int s;

	s = splhigh();
	TAILQ_REMOVE(&dlnke_list, nfp, nf_list);
	splx(s);
	return(0);
}

/*
 * Locate a DataLink NKE
 */
struct NFDescriptor *
find_dlnke(int handle)
{	struct NFDescriptor *nfp;

	nfp = dlnke_list.tqh_first;
	while (nfp)
	{	if (nfp->nf_handle == handle)
			return(nfp);
		nfp = nfp->nf_next.tqe_next;
	}
	return(NULL);
}
