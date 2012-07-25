/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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

#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/protosw.h>
#include <kern/locks.h>

#include <net/if_types.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/udp.h>

#include <kern/thread.h>
#include <kern/task.h>
#include <kern/kern_types.h>
#include <kern/sched_prim.h>
#include <sys/sysctl.h>

#include "l2tpk.h"
#include "l2tp_rfc.h"
#include "l2tp_udp.h"
#include "../../../Family/ppp_domain.h"


/* -----------------------------------------------------------------------------
Definitions
----------------------------------------------------------------------------- */

struct l2tp_udp_thread {
	thread_t	thread;
	int			wakeup;
	int			terminate;
  struct pppqueue	outq;
	int			nbclient;
	
	lck_mtx_t       *mtx;
} ; 

#define L2TP_UDP_MAX_THREADS 16
#define L2TP_UDP_DEF_OUTQ_SIZE 1024

void	l2tp_ip_input(mbuf_t , int len);
void l2tp_udp_thread_func(struct l2tp_udp_thread *thread_socket);
kern_return_t thread_terminate(register thread_act_t act);
int l2tp_udp_init_threads(int nb_threads);
void l2tp_udp_dispose_threads();
#if !TARGET_OS_EMBEDDED
static int sysctl_nb_threads SYSCTL_HANDLER_ARGS;
#endif

/* -----------------------------------------------------------------------------
Globals
----------------------------------------------------------------------------- */
extern lck_mtx_t	*ppp_domain_mutex;
static struct l2tp_udp_thread *l2tp_udp_threads = 0;
static int l2tp_udp_thread_outq_size = L2TP_UDP_DEF_OUTQ_SIZE;
static int l2tp_udp_nb_threads = 0;
static int l2tp_udp_inited = 0;

static lck_rw_t			*l2tp_udp_mtx;
static lck_attr_t		*l2tp_udp_mtx_attr;
static lck_grp_t		*l2tp_udp_mtx_grp;
static lck_grp_attr_t	*l2tp_udp_mtx_grp_attr;

#if !TARGET_OS_EMBEDDED
SYSCTL_PROC(_net_ppp_l2tp, OID_AUTO, nb_threads, CTLTYPE_INT|CTLFLAG_RW|CTLFLAG_NOAUTO|CTLFLAG_KERN,
    &l2tp_udp_nb_threads, 0, sysctl_nb_threads, "I", "Number of l2tp output threads 0 - 16");
SYSCTL_INT(_net_ppp_l2tp, OID_AUTO, thread_outq_size, CTLTYPE_INT|CTLFLAG_RW|CTLFLAG_NOAUTO|CTLFLAG_KERN,
    &l2tp_udp_thread_outq_size, 0, "Queue size for each l2tp output thread");
#endif 

/* -----------------------------------------------------------------------------
intialize L2TP/UDP layer
----------------------------------------------------------------------------- */
int l2tp_udp_init()
{
	int err = ENOMEM;
	
	if (l2tp_udp_inited)
		return 0;

	//  allocate lock group attribute and group for udp the list
	l2tp_udp_mtx_grp_attr = lck_grp_attr_alloc_init();
	LOGNULLFAIL(l2tp_udp_mtx_grp_attr, "l2tp_udp_init: can't alloc mutex group attributes\n");

	lck_grp_attr_setstat(l2tp_udp_mtx_grp_attr);
	
	l2tp_udp_mtx_grp = lck_grp_alloc_init("l2tp_udp", l2tp_udp_mtx_grp_attr);
	LOGNULLFAIL(l2tp_udp_mtx_grp, "l2tp_udp_init: can't alloc mutex group\n");

	l2tp_udp_mtx_attr = lck_attr_alloc_init();
	LOGNULLFAIL(l2tp_udp_mtx_attr, "l2tp_udp_init: can't alloc mutex attributes\n");
	
	lck_attr_setdebug(l2tp_udp_mtx_attr);

	l2tp_udp_mtx = lck_rw_alloc_init(l2tp_udp_mtx_grp, l2tp_udp_mtx_attr);
	LOGNULLFAIL(l2tp_udp_mtx, "l2tp_udp_init: can't alloc mutex\n")

	// init threads
	err = l2tp_udp_init_threads(0);
	if (err)
		goto fail;
		
#if !TARGET_OS_EMBEDDED
    sysctl_register_oid(&sysctl__net_ppp_l2tp_nb_threads);
    sysctl_register_oid(&sysctl__net_ppp_l2tp_thread_outq_size);
#endif
	l2tp_udp_inited = 1;

	return 0;

fail:
	if (l2tp_udp_mtx) {
		lck_rw_free(l2tp_udp_mtx, l2tp_udp_mtx_grp);
		l2tp_udp_mtx = 0;
	}
	if (l2tp_udp_mtx_attr) {
		lck_attr_free(l2tp_udp_mtx_attr);
		l2tp_udp_mtx_attr = 0;
	}
	if (l2tp_udp_mtx_grp) {
		lck_grp_free(l2tp_udp_mtx_grp);
		l2tp_udp_mtx_grp = 0;
	}
	if (l2tp_udp_mtx_grp_attr) {
		lck_grp_attr_free(l2tp_udp_mtx_grp_attr);
		l2tp_udp_mtx_grp_attr = 0;
	}	
    return err;
}

/* -----------------------------------------------------------------------------
dispose L2TP/UDP layer
----------------------------------------------------------------------------- */
int l2tp_udp_dispose()
{
	if (!l2tp_udp_inited)
		return 0;
		
#if !TARGET_OS_EMBEDDED
    sysctl_unregister_oid(&sysctl__net_ppp_l2tp_nb_threads);
    sysctl_unregister_oid(&sysctl__net_ppp_l2tp_thread_outq_size);
#endif

	l2tp_udp_dispose_threads();
	
	lck_rw_free(l2tp_udp_mtx, l2tp_udp_mtx_grp);
	l2tp_udp_mtx = 0;
	lck_attr_free(l2tp_udp_mtx_attr);
	l2tp_udp_mtx_attr = 0;
	lck_grp_free(l2tp_udp_mtx_grp);
	l2tp_udp_mtx_grp = 0;
	lck_grp_attr_free(l2tp_udp_mtx_grp_attr);
	l2tp_udp_mtx_grp_attr = 0;

	l2tp_udp_inited = 0;
    return 0;
}

#if !TARGET_OS_EMBEDDED
/* -----------------------------------------------------------------------------
sysctl to change the number of threads
----------------------------------------------------------------------------- */
static int sysctl_nb_threads SYSCTL_HANDLER_ARGS
{
	int error, s;

	s = *(int *)oidp->oid_arg1;

	error = sysctl_handle_int(oidp, &s, 0, req);
	if (error || !req->newptr)
		return error;

	lck_mtx_lock(ppp_domain_mutex);
    error = l2tp_udp_init_threads(s);
	lck_mtx_unlock(ppp_domain_mutex);
	
	return error;
}
#endif

/* -----------------------------------------------------------------------------
initialize the worker threads
----------------------------------------------------------------------------- */
int l2tp_udp_init_threads(int nb_threads)
{
    int				i;
	errno_t			err;

	if (nb_threads < 0) 
		nb_threads = 0;
	else 
		if (nb_threads > L2TP_UDP_MAX_THREADS) 
			nb_threads = L2TP_UDP_MAX_THREADS;

	if (l2tp_udp_nb_threads == nb_threads)
		return 0;
	
	IOLog("l2tp_udp_init_threads: changing # of threads from %d to %d\n", l2tp_udp_nb_threads, nb_threads);

	l2tp_udp_dispose_threads();

	if (nb_threads == 0)
		return 0;

	l2tp_udp_threads = (struct l2tp_udp_thread *)_MALLOC(sizeof(struct l2tp_udp_thread) * nb_threads, M_TEMP, M_WAITOK);
	if (!l2tp_udp_threads) 
		return ENOMEM;
	
	bzero(l2tp_udp_threads, sizeof(struct l2tp_udp_thread) * nb_threads);
		
	for (i = 0; i < nb_threads; i++) {

		err = ENOMEM;
		
		l2tp_udp_threads[i].mtx = lck_mtx_alloc_init(l2tp_udp_mtx_grp, l2tp_udp_mtx_attr);
		LOGNULLFAIL(l2tp_udp_threads[i].mtx, "l2tp_udp_init_threads: can't alloc mutex\n");

		// Start up working thread
		err = kernel_thread_start((thread_continue_t)l2tp_udp_thread_func, &l2tp_udp_threads[i], &l2tp_udp_threads[i].thread);
		LOGGOTOFAIL(err, "l2tp_udp_init_threads: kernel_thread_start failed, error %d\n");
		
		l2tp_udp_nb_threads++;
	}
	
    return 0;
	
fail:
	
	if (l2tp_udp_threads[i].mtx) {
		lck_mtx_free(l2tp_udp_threads[i].mtx, l2tp_udp_mtx_grp);
		l2tp_udp_threads[i].mtx = 0;
	}
	
	l2tp_udp_dispose_threads();
	return err;
}

/* -----------------------------------------------------------------------------
dispose threads
----------------------------------------------------------------------------- */
void l2tp_udp_dispose_threads()
{
	int i;
	
	if (!l2tp_udp_nb_threads)
		return;

	lck_rw_lock_exclusive(l2tp_udp_mtx);

	for (i = 0; i < l2tp_udp_nb_threads; i++) {		

		if (l2tp_udp_threads[i].thread) {
			
			lck_mtx_lock(l2tp_udp_threads[i].mtx);
			l2tp_udp_threads[i].terminate = 1;
			wakeup(&l2tp_udp_threads[i].wakeup);
			msleep(&l2tp_udp_threads[i].terminate, l2tp_udp_threads[i].mtx, PZERO + 1, "l2tp_udp_dispose_threads", 0);
            lck_mtx_unlock(l2tp_udp_threads[i].mtx);
			
			thread_terminate(l2tp_udp_threads[i].thread);
			thread_deallocate(l2tp_udp_threads[i].thread);

			lck_mtx_free(l2tp_udp_threads[i].mtx, l2tp_udp_mtx_grp);
		}
	}
	
	_FREE(l2tp_udp_threads, M_TEMP);
	l2tp_udp_nb_threads = 0;
	
	lck_rw_unlock_exclusive(l2tp_udp_mtx);

}

/* -----------------------------------------------------------------------------
callback from udp
----------------------------------------------------------------------------- */
void l2tp_udp_input(socket_t so, void *arg, int waitflag)
{
    mbuf_t mp = 0;
	size_t recvlen = 1000000000;
    struct sockaddr from;
    struct msghdr msg;
		
    do {
    
		bzero(&from, sizeof(from));
		bzero(&msg, sizeof(msg));
		msg.msg_namelen = sizeof(from);
		msg.msg_name = &from;
	
		if (sock_receivembuf(so, &msg, &mp, MSG_DONTWAIT, &recvlen) != 0)
			break;

        if (mp == 0) 
            break;

		lck_mtx_lock(ppp_domain_mutex);
		l2tp_rfc_lower_input(so, mp, &from);
		lck_mtx_unlock(ppp_domain_mutex);
		
    } while (1);

}

/* -----------------------------------------------------------------------------
called from ppp_proto when data need to be sent
----------------------------------------------------------------------------- */
int l2tp_udp_output(socket_t so, int thread, mbuf_t m, struct sockaddr* to)
{
	int err = 0;
	void	*p;
	
    if (so == 0 || to == 0) {
        mbuf_freem(m);	
        return EINVAL;
    }

	if (thread < 0)
		goto no_thread;

	lck_rw_lock_shared(l2tp_udp_mtx);
	if (!l2tp_udp_nb_threads) {
		lck_rw_unlock_shared(l2tp_udp_mtx);
		goto no_thread;
	}

	if (thread >= l2tp_udp_nb_threads)
		thread %= l2tp_udp_nb_threads;
	
	if (l2tp_udp_threads[thread].outq.len >= l2tp_udp_thread_outq_size) {
		lck_rw_unlock_shared(l2tp_udp_mtx);
		mbuf_free(m);
        return EBUSY;
	}	

	if (err = mbuf_prepend(&m, sizeof(socket_t), M_NOWAIT)) {
		lck_rw_unlock_shared(l2tp_udp_mtx);
        return err;
	}
	p = mbuf_data(m);
	*(socket_t*)p = so;
	sock_retain(so);

	lck_mtx_lock(l2tp_udp_threads[thread].mtx);
	ppp_enqueue(&l2tp_udp_threads[thread].outq, m);
	wakeup(&l2tp_udp_threads[thread].wakeup);
	lck_mtx_unlock(l2tp_udp_threads[thread].mtx);
	
	lck_rw_unlock_shared(l2tp_udp_mtx);

	return 0;
	
no_thread:	
	lck_mtx_unlock(ppp_domain_mutex);
	err = sock_sendmbuf(so, 0, m, MSG_DONTWAIT, 0);
	lck_mtx_lock(ppp_domain_mutex);
	return err;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int l2tp_udp_setpeer(socket_t so, struct sockaddr *addr)
{
	int result;
	
    if (so == 0)
        return EINVAL;
    
	lck_mtx_unlock(ppp_domain_mutex);
    result = sock_connect(so, addr, 0);
	lck_mtx_lock(ppp_domain_mutex);
	
	return result;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void l2tp_udp_thread_func(struct l2tp_udp_thread *thread_socket)
{
	mbuf_t m;
	socket_t so;
	void	*p;
	
	for (;;) {
	
		lck_mtx_lock(thread_socket->mtx);
dequeue:
		m = ppp_dequeue(&thread_socket->outq);
		if (m == NULL) {
			if (thread_socket->terminate) {
				wakeup(&thread_socket->terminate);
				// just sleep again. caller will terminate the thread.
				msleep(&thread_socket->thread, thread_socket->mtx, PZERO + 1, "l2tp_udp_thread_func terminate", 0);
				/* NOT REACHED */
			}
			msleep(&thread_socket->wakeup, thread_socket->mtx, PZERO + 1, "l2tp_udp_thread_func", 0);
			goto dequeue;
		}
		lck_mtx_unlock(thread_socket->mtx);
		
		p = mbuf_data(m);
		so = *(socket_t*)p;
		mbuf_adj(m, sizeof(socket_t));

		// should have a kpi to sendmbuf and release at the same time
		// to avoid too extra lock/unlock
		sock_sendmbuf(so, 0, m, MSG_DONTWAIT, 0);
		sock_release(so);

	}

    /* NOTREACHED */
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int l2tp_udp_attach(socket_t *socket, struct sockaddr *addr, int *thread, int nocksum)
{
    int				val;
	errno_t			err;
    socket_t		so = 0;
	u_int32_t		i, min;
	
	lck_mtx_unlock(ppp_domain_mutex);
    
    /* open a UDP socket for use by the L2TP client */
    if (err = sock_socket(AF_INET, SOCK_DGRAM, 0, l2tp_udp_input, 0, &so)) 
        goto fail;

    /* configure the socket to reuse port */
    val = 1;
    if (err = sock_setsockopt(so, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val)))
        goto fail;

    if (nocksum) {
		val = 1;
		if (err = sock_setsockopt(so, IPPROTO_UDP, UDP_NOCKSUM, &val, sizeof(val)))
			goto fail;
	}
	
    if (err = sock_bind(so, addr))
        goto fail;

    /* fill in the incomplete part of the address assigned by UDP */ 
    if (err = sock_getsockname(so, addr, addr->sa_len))
        goto fail;
    
	lck_mtx_lock(ppp_domain_mutex);
    *socket = so;

	if (l2tp_udp_nb_threads) {		
		min = 0;
		for (i = 1; i < l2tp_udp_nb_threads; i++)
			if (l2tp_udp_threads[i].nbclient < l2tp_udp_threads[min].nbclient) 
				min = i;
		*thread = min;
		l2tp_udp_threads[min].nbclient += 1;
		//IOLog("l2tp_udp_attach: worker thread #%d (total client for thread is now %d)\n", *thread, l2tp_udp_threads[*thread].nbclient);
	}
	else 
	  *thread = -1;

    return 0;
    
fail:
    if (so) 
        sock_close(so);
	lck_mtx_lock(ppp_domain_mutex);
    return err;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int l2tp_udp_detach(socket_t so, int thread)
{

    if (so) {
		if (thread >= 0 && thread < l2tp_udp_nb_threads) {
			if (l2tp_udp_threads[thread].nbclient > 0)
				l2tp_udp_threads[thread].nbclient -= 1;
			//IOLog("l2tp_udp_detach: worker thread #%d (total client for thread is now %d)\n", thread, l2tp_udp_threads[thread].nbclient);
		}
			
		lck_mtx_unlock(ppp_domain_mutex);
        sock_close(so); 		/* close the UDP socket */
		lck_mtx_lock(ppp_domain_mutex);
	}
	
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void l2tp_udp_clear_INP_INADDR_ANY(socket_t so)
{
	struct inpcb *inp;
	
	if (so) {

		lck_mtx_unlock(ppp_domain_mutex);
		inp_clear_INP_INADDR_ANY(so);
		lck_mtx_lock(ppp_domain_mutex);
    }
	
}
