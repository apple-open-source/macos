/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 1993 The Australian National University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the Australian National University.  The name of the University
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Copyright (c) 1991 Gregory M. Christy.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by Gregory M. Christy.  The name of the author may not be used to
 * endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/* -----------------------------------------------------------------------------
*
*  History :
*
*  Jun 2000 - 	Christophe Allie - created.
*		Reuse large portions of the original ppp driver
*
*  Theory of operation :
*
*  this file implements the serial driver for the ppp family,
*   	and the asynchronous line discipline for tty devices
*
*  it's the responsability of the driver to update the statistics
*  whenever that makes sense
*     ifnet.if_lastchange = a packet is present, a packet starts to be sent
*     ifnet.if_ibytes = nb of correct PPP bytes received (does not include escapes...)
*     ifnet.if_obytes = nb of correct PPP bytes sent (does not include escapes...)
*     ifnet.if_ipackets = nb of PPP packet received
*     ifnet.if_opackets = nb of PPP packet sent
*     ifnet.if_ierrors = nb on input packets in error
*     ifnet.if_oerrors = nb on ouptut packets in error
*
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
Includes
----------------------------------------------------------------------------- */

#include <sys/ttycom.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/mbuf.h>
#include <sys/dkstat.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <sys/tty.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/syslog.h>
#include <sys/sockio.h>
#include <machine/spl.h>

#include <kern/thread.h>
#include <kern/task.h>

#include <sys/vnode.h>
#include <net/if_types.h>
#include <net/dlil.h>
#include <net/if.h>

#include "../../../Family/PPP.kmodproj/ppp.h"

#include "pppserial.h"


/* -----------------------------------------------------------------------------
Definitions
----------------------------------------------------------------------------- */


#define LOGVAL 	LOG_INFO
#define LOG(text) 	log(LOGVAL, text)
#define LOGDBG(ifp, text) \
    if (ifp->if_flags & IFF_DEBUG) {	\
        log text; 		\
    }

#define IFFDEBUG(ifp) (((struct ifnet*)ifp)->if_flags & IFF_DEBUG)
#define IFNAME(ifp) (((struct ifnet*)ifp)->if_unit)
#define IFUNIT(ifp) (((struct ifnet*)ifp)->if_unit)

/* Some useful mbuf macros not in mbuf.h. */
#define M_IS_CLUSTER(m)	((m)->m_flags & M_EXT)

#define M_DATASTART(m)	\
        (M_IS_CLUSTER(m) ? (m)->m_ext.ext_buf : \
            (m)->m_flags & M_PKTHDR ? (m)->m_pktdat : (m)->m_dat)

#define M_DATASIZE(m)	\
        (M_IS_CLUSTER(m) ? (m)->m_ext.ext_size : \
            (m)->m_flags & M_PKTHDR ? MHLEN: MLEN)

#define MAX_PPPSERIAL 	8
#define PPPSERIAL_MRU	2048

/* Does c need to be escaped? */
#define ESCAPE_P(c)	(ld->asyncmap[(c) >> 5] & (1 << ((c) & 0x1F)))


#define CCOUNT(q)	((q)->c_cc)
#define PPP_LOWAT	100	/* Process more output when < LOWAT on queue */
#define	PPP_HIWAT	400	/* Don't start a new packet if HIWAT on que */


/*
 * State bits in sc_flags, not changeable by user.
 */
#define LK_TIMEOUT	0x00000400	/* timeout is currently pending */
#define LK_TBUSY	0x10000000	/* xmitter doesn't need a packet yet */
#define LK_PKTLOST	0x20000000	/* have lost or dropped a packet */
#define	LK_FLUSH	0x40000000	/* flush input until next PPP_FLAG */
#define	LK_ESCAPED	0x80000000	/* saw a PPP_ESCAPE */


struct pppserial {
    /* first, the ifnet structure... */
    struct ifnet 	lk_if;			/* network-visible interface */

    /* administrative info */
    void		*devp;			/* pointer to device-dep structure */
    u_short 		lref;			/* our line number, as given by mux */
    u_long		flags;			/* control/status bits */
    pid_t		pid;			/* pid of the process that control the line */
    u_long		mru;			/* mru for the line  */

    /* settings */
    ext_accm 		asyncmap;		/* async control character map */
    u_long		rasyncmap;		/* receive async control char map */

    /* output data */
    u_short		outfcs;			/* FCS so far for output packet */
    struct mbuf 	*outm;			/* mbuf chain currently being output */
    struct ifqueue 	outfastq;		/* a line may provide a queue for urgent data */

    /* input data */
    struct ifqueue 	inrawq;			/* received packets */
    struct mbuf 	*inm;			/* pointer to input mbuf chain */
    char		*inmp;			/* ptr to next char in input mbuf */
    struct mbuf 	*inmc;			/* pointer to current input mbuf */
    short		inlen;			/* length of input packet so far */
    u_short		infcs;			/* FCS so far (input) */

    /* log purpose */
    u_char		rawin[16];		/* chars as received */
    int			rawinlen;		/* # in rawin */
};

/* -----------------------------------------------------------------------------
Forward declarations
----------------------------------------------------------------------------- */

static int	pppserial_open(dev_t dev, struct tty *tp);
static int	pppserial_close(struct tty *tp, int flag);
static int	pppserial_read(struct tty *tp, struct uio *uio, int flag);
static int	pppserial_write(struct tty *tp, struct uio *uio, int flag);
static int	pppserial_ioctl(struct tty *tp, u_long cmd, caddr_t data, int flag, struct proc *);
static int	pppserial_input(int c, struct tty *tp);
static int	pppserial_start(struct tty *tp);


static u_short	pppserial_fcs(u_short fcs, u_char *cp, int len);
static void	pppserial_relinq(struct pppserial *ld);
static void	pppserial_timeout(void *arg);
static void	pppserial_getm(struct pppserial *ld);
static void	pppserial_logchar(struct pppserial *, int);
static struct mbuf *pppserial_dequeue(struct pppserial *ld);
static int 	pppserial_enqueue(struct pppserial *ld, struct mbuf *m);
static int	pppserial_if_output(struct ifnet *ifp, struct mbuf *m);
static int	pppserial_if_free(struct ifnet *ifp);
static int 	pppserial_if_ioctl(struct ifnet *ifp, u_long cmd, void *data);
static int 	pppserial_sendevent(struct ifnet *ifp, u_long code);
static int 	pppserial_attach(u_short unit);
static int 	pppserial_detach(u_short unit);

static void 	pppisr_thread(void);
static void 	pppserial_intr();

/* -----------------------------------------------------------------------------
Globals
----------------------------------------------------------------------------- */

static struct linesw pppserial_disc;

/* tty interface receiver interrupt. */
static unsigned paritytab[8] = {
    0x96696996, 0x69969669, 0x69969669, 0x96696996,
    0x69969669, 0x96696996, 0x96696996, 0x69969669
};

/* FCS lookup table as calculated by genfcstab */
static u_short fcstab[256] = {
    0x0000,	0x1189,	0x2312,	0x329b,	0x4624,	0x57ad,	0x6536,	0x74bf,
    0x8c48,	0x9dc1,	0xaf5a,	0xbed3,	0xca6c,	0xdbe5,	0xe97e,	0xf8f7,
    0x1081,	0x0108,	0x3393,	0x221a,	0x56a5,	0x472c,	0x75b7,	0x643e,
    0x9cc9,	0x8d40,	0xbfdb,	0xae52,	0xdaed,	0xcb64,	0xf9ff,	0xe876,
    0x2102,	0x308b,	0x0210,	0x1399,	0x6726,	0x76af,	0x4434,	0x55bd,
    0xad4a,	0xbcc3,	0x8e58,	0x9fd1,	0xeb6e,	0xfae7,	0xc87c,	0xd9f5,
    0x3183,	0x200a,	0x1291,	0x0318,	0x77a7,	0x662e,	0x54b5,	0x453c,
    0xbdcb,	0xac42,	0x9ed9,	0x8f50,	0xfbef,	0xea66,	0xd8fd,	0xc974,
    0x4204,	0x538d,	0x6116,	0x709f,	0x0420,	0x15a9,	0x2732,	0x36bb,
    0xce4c,	0xdfc5,	0xed5e,	0xfcd7,	0x8868,	0x99e1,	0xab7a,	0xbaf3,
    0x5285,	0x430c,	0x7197,	0x601e,	0x14a1,	0x0528,	0x37b3,	0x263a,
    0xdecd,	0xcf44,	0xfddf,	0xec56,	0x98e9,	0x8960,	0xbbfb,	0xaa72,
    0x6306,	0x728f,	0x4014,	0x519d,	0x2522,	0x34ab,	0x0630,	0x17b9,
    0xef4e,	0xfec7,	0xcc5c,	0xddd5,	0xa96a,	0xb8e3,	0x8a78,	0x9bf1,
    0x7387,	0x620e,	0x5095,	0x411c,	0x35a3,	0x242a,	0x16b1,	0x0738,
    0xffcf,	0xee46,	0xdcdd,	0xcd54,	0xb9eb,	0xa862,	0x9af9,	0x8b70,
    0x8408,	0x9581,	0xa71a,	0xb693,	0xc22c,	0xd3a5,	0xe13e,	0xf0b7,
    0x0840,	0x19c9,	0x2b52,	0x3adb,	0x4e64,	0x5fed,	0x6d76,	0x7cff,
    0x9489,	0x8500,	0xb79b,	0xa612,	0xd2ad,	0xc324,	0xf1bf,	0xe036,
    0x18c1,	0x0948,	0x3bd3,	0x2a5a,	0x5ee5,	0x4f6c,	0x7df7,	0x6c7e,
    0xa50a,	0xb483,	0x8618,	0x9791,	0xe32e,	0xf2a7,	0xc03c,	0xd1b5,
    0x2942,	0x38cb,	0x0a50,	0x1bd9,	0x6f66,	0x7eef,	0x4c74,	0x5dfd,
    0xb58b,	0xa402,	0x9699,	0x8710,	0xf3af,	0xe226,	0xd0bd,	0xc134,
    0x39c3,	0x284a,	0x1ad1,	0x0b58,	0x7fe7,	0x6e6e,	0x5cf5,	0x4d7c,
    0xc60c,	0xd785,	0xe51e,	0xf497,	0x8028,	0x91a1,	0xa33a,	0xb2b3,
    0x4a44,	0x5bcd,	0x6956,	0x78df,	0x0c60,	0x1de9,	0x2f72,	0x3efb,
    0xd68d,	0xc704,	0xf59f,	0xe416,	0x90a9,	0x8120,	0xb3bb,	0xa232,
    0x5ac5,	0x4b4c,	0x79d7,	0x685e,	0x1ce1,	0x0d68,	0x3ff3,	0x2e7a,
    0xe70e,	0xf687,	0xc41c,	0xd595,	0xa12a,	0xb0a3,	0x8238,	0x93b1,
    0x6b46,	0x7acf,	0x4854,	0x59dd,	0x2d62,	0x3ceb,	0x0e70,	0x1ff9,
    0xf78f,	0xe606,	0xd49d,	0xc514,	0xb1ab,	0xa022,	0x92b9,	0x8330,
    0x7bc7,	0x6a4e,	0x58d5,	0x495c,	0x3de3,	0x2c6a,	0x1ef1,	0x0f78
};

/* Define the PPP line discipline. */
static struct linesw pppdisc = {
    pppserial_open,	pppserial_close,	pppserial_read,	pppserial_write,
    pppserial_ioctl,	pppserial_input,	pppserial_start,	ttymodem,
};


int	pppsoft_net_wakeup;
int	pppsoft_net_terminate;
int	pppnetisr;


#define	setpppsoftnet()	(wakeup((caddr_t)&pppsoft_net_wakeup))
#define	schedpppnetisr()	{ pppnetisr = 1; setpppsoftnet(); }


static struct pppserial	*pppserial[MAX_PPPSERIAL];
static int pppserial_inited = 0;
static int pppserial_firstinuse = 0;


/* -----------------------------------------------------------------------------
 NKE entry point, start routine
----------------------------------------------------------------------------- */
int pppserial_module_start(struct kmod_info *ki, void *data)
{
    extern int pppserial_init();
    boolean_t 	funnel_state;
    int		ret;
    
    funnel_state = thread_funnel_set(network_flock, TRUE);
    ret = pppserial_init();
    thread_funnel_set(network_flock, funnel_state);

    return ret;
}

/* -----------------------------------------------------------------------------
  NKE entry point, stop routine
----------------------------------------------------------------------------- */
int pppserial_module_stop(struct kmod_info *ki, void *data)
{
    extern int pppserial_dispose();
    boolean_t 	funnel_state;
    int		ret;

    funnel_state = thread_funnel_set(network_flock, TRUE);
    ret = pppserial_dispose();
    thread_funnel_set(network_flock, funnel_state);

    return ret;
}

/* -----------------------------------------------------------------------------
register line discipline
----------------------------------------------------------------------------- */
int pppserial_init()
{
    u_short 		i;

//    log(LOGVAL, "pppserial_init\n");

    if (pppserial_inited)
        return KERN_SUCCESS;

    pppserial_disc = linesw[PPPDISC];
    linesw[PPPDISC]= pppdisc;

    for (i = 0; i < MAX_PPPSERIAL; i++) 
        pppserial[i] = 0;
    
    for (i = 0; i < 1 ; i++) {
    
        pppserial_attach(i);
    }

    // Start up netisr thread
    pppsoft_net_terminate = 0;
    pppsoft_net_wakeup = 0;
    pppnetisr = 0;
    kernel_thread(kernel_task, pppisr_thread);

    /* NKE is ready ! */
    pppserial_inited = 1;
    return KERN_SUCCESS;
}

/* -----------------------------------------------------------------------------
deregister line discipline
----------------------------------------------------------------------------- */
int pppserial_dispose()
{
    u_short	i;

//    log(LOGVAL, "pppserial_dispose\n");

    if (!pppserial_inited)
        return(KERN_SUCCESS);

    pppsoft_net_terminate = 1;
    wakeup(&pppsoft_net_wakeup);
    sleep(&pppsoft_net_terminate, PZERO+1);

    for (i = 0; i < MAX_PPPSERIAL; i++) {
        if (pppserial[i]) {
            pppserial_detach(i);
        }
    }

    linesw[PPPDISC] = pppserial_disc;
    return KERN_SUCCESS;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int pppserial_attach(u_short unit)
{
    int 		ret;
    struct pppserial 	*ld;

    // then, alloc the structure...
    MALLOC(pppserial[unit], struct pppserial *, sizeof(struct pppserial), M_DEVBUF, M_NOWAIT);
    if (!pppserial[unit])
        return ENOMEM;

    ld = pppserial[unit];
    bzero(ld, sizeof(struct pppserial));

    // it's time now to register our brand new channel
    ld->lk_if.if_name 	= APPLE_PPP_NAME_SERIAL;
    ld->lk_if.if_family 	= (((u_long) APPLE_IF_FAM_PPP_SERIAL) << 16) + APPLE_IF_FAM_PPP;
    ld->lk_if.if_mtu 	= PPP_MTU;
    ld->lk_if.if_flags 	= IFF_POINTOPOINT | IFF_MULTICAST;
    ld->lk_if.if_type 	= IFT_PPP;
    ld->lk_if.if_physical 	= PPP_PHYS_SERIAL;
    ld->lk_if.if_hdrlen 	= PPP_HDRLEN;
    ld->lk_if.if_ioctl 	= pppserial_if_ioctl;
    ld->lk_if.if_output 	= pppserial_if_output;
    ld->lk_if.if_free 	= pppserial_if_free;
    ld->lk_if.if_snd.ifq_maxlen = IFQ_MAXLEN;
    getmicrotime(&ld->lk_if.if_lastchange);
    //ld->lk_if.if_baudrate 	= tp->t_ospeed;

    ld->lk_if.if_unit = unit; 
    ret = dlil_if_attach(&ld->lk_if);
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int pppserial_detach(u_short unit)
{
    int 		ret;

    ret = dlil_if_detach(&pppserial[unit]->lk_if);
    switch (ret) {
        case 0:
            break;
        case DLIL_WAIT_FOR_FREE:
            sleep(&pppsoft_net_terminate, PZERO+1);
            break;
        default:
            return KERN_FAILURE;
    }

    FREE(pppserial[unit], M_DEVBUF);
    pppserial[unit] = 0;
    return 0;
}

/* -----------------------------------------------------------------------------
All routines this thread calls expect to be called at splnet
all line discipline share the same thread
----------------------------------------------------------------------------- */
void pppisr_thread_continue(void)
{
    spl_t 	s;
    boolean_t 	funnel_state;

    funnel_state = thread_funnel_set(network_flock, TRUE);
    s = splnet();

    while (!pppsoft_net_terminate) {
        if (pppnetisr) {

            pppnetisr = 0;
            pppserial_intr();
        }

        /*        assert_wait(&pppsoft_net_wakeup, FALSE);
#if 0
        thread_block(pppisr_thread_continue);
#else
        thread_block(0);
#endif*/
        sleep(&pppsoft_net_wakeup, PZERO+1);
    }
    splx(s);

    wakeup(&pppsoft_net_terminate);

    thread_funnel_set(network_flock, funnel_state);

    //thread_terminate(current_act());
    thread_terminate_self();
    /* NOTREACHED */
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void pppisr_thread(void)
{
    register thread_t self = current_thread();
    extern void stack_privilege(thread_t thread);
#if NCPUS > 1
    extern void thread_bind(thread_t, processor_t);
#endif
    /* Make sure that this thread always has a kernel stack,
        and bind it to the master cpu. */
    stack_privilege(self);
#if NCPUS > 1
    thread_bind(self, master_processor);
#endif
#if 0
    thread_block(pppisr_thread_continue);
    /* NOTREACHED */
#else
    pppisr_thread_continue();
#endif
}

/* -----------------------------------------------------------------------------
Line specific open routine for async tty devices.
Attach the given tty to the first available ppp unit.
Called from device open routine or ttioctl() at >= splsofttty()
----------------------------------------------------------------------------- */
int pppserial_open(dev_t dev, struct tty *tp)
{
    struct proc 	*p = current_proc();		/* XXX */
    struct pppserial 	*ld;
    int 		error, s, slot;

    if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
        return error;
    
    s = spltty();

    if (tp->t_line == PPPDISC) {
        ld = (struct pppserial *) tp->t_sc;
        if (ld && ld->devp == (void *)tp) {
            splx(s);
            return 0;
        }
    }

    for (slot = 0; slot < MAX_PPPSERIAL; slot++) {
        // pick up the first interface which is not up
        // fix it, use others flags to determine availability
        //if (pppserial[slot] && !(pppserial[slot]->lk_if.if_flags & IFF_UP))
        if (pppserial[slot] && !(pppserial[slot]->lk_if.if_flags & IFF_PPP_CONNECTED))
            break;
    }

    if (slot == MAX_PPPSERIAL) {
        splx(s);
        return ENXIO; // ENOMEM ?
    }

    ld = pppserial[slot];
    tp->t_sc = (caddr_t) ld;
    
    ttyflush(tp, FREAD | FWRITE);

    ld->devp = (void *) tp;
    ld->asyncmap[0] = 0xffffffff;
    ld->asyncmap[3] = 0x60000000;
    //    sc->sc_line.sc_relinq = pppserial_relinq;
    ld->pid 			= p->p_pid;
    ld->outfastq.ifq_maxlen 	= IFQ_MAXLEN;
    ld->inrawq.ifq_maxlen 	= IFQ_MAXLEN;

    pppserial_getm(ld);
    
    thread_funnel_switch(KERNEL_FUNNEL, NETWORK_FUNNEL);
    //ld->lk_if.if_flags 		|= IFF_RUNNING | IFF_UP | IFF_DEBUG;
    ld->lk_if.if_flags 		|= IFF_PPP_CONNECTED | IFF_DEBUG;
    getmicrotime(&ld->lk_if.if_lastchange);
    ld->lk_if.if_baudrate 	= tp->t_ospeed;

    // le relinq sert au dial on demand, car dans ce cas on recupere la meme interface et on la reinitialise
    //   if (sc->sc_relinq)
    //       (*sc->sc_relinq)(sc);	/* get previous owner to relinquish the unit */

    pppserial_sendevent(&ld->lk_if, KEV_PPP_CONNECTED);
    thread_funnel_switch(NETWORK_FUNNEL, KERNEL_FUNNEL);

    splx(s);
    return 0;
}

/* -----------------------------------------------------------------------------
* Line specific close routine, called from device close routine
* and from ttioctl at >= splsofttty().
* Detach the tty from the ppp unit.
* Mimics part of ttyclose().
----------------------------------------------------------------------------- */
int pppserial_close(struct tty *tp, int flag)
{
    struct pppserial 	*ld = (struct pppserial *) tp->t_sc;
    int 		s;
    struct mbuf		*m;

    log(LOG_INFO, "pppserial_close\n");
    s = spltty();
    ttyflush(tp, FREAD | FWRITE);
    tp->t_line = 0;

    if (ld) {
        tp->t_sc = NULL;
        if (tp == (struct tty *) ld->devp) {

            for (;;) {
                IF_DEQUEUE(&ld->inrawq, m);
                if (m == NULL)
                    break;
                m_freem(m);
            }

            for (;;) {
                IF_DEQUEUE(&ld->outfastq, m);
                if (m == NULL)
                    break;
                m_freem(m);
            }

            pppserial_relinq(ld);

            log(LOG_INFO, "pppserial_close will send event\n");
            thread_funnel_switch(KERNEL_FUNNEL, NETWORK_FUNNEL);
            pppserial_sendevent(&ld->lk_if, KEV_PPP_DISCONNECTED);

            ld->lk_if.if_ipackets = 0;
            ld->lk_if.if_opackets = 0;
            ld->lk_if.if_ibytes = 0;
            ld->lk_if.if_obytes = 0;
            ld->lk_if.if_ierrors = 0;
            ld->lk_if.if_oerrors = 0;
            ld->lk_if.if_flags 	&= ~IFF_PPP_CONNECTED;
            //ld->lk_if.if_flags 	&= ~(IFF_RUNNING | IFF_UP);
            thread_funnel_switch(NETWORK_FUNNEL, KERNEL_FUNNEL);
        }
    }
    splx(s);
    return 0;
}

/* -----------------------------------------------------------------------------
Line specific (tty) read routine.
there shouldn't be any write to the file descriptor from user level
----------------------------------------------------------------------------- */
int pppserial_read(struct tty *tp, struct uio *uio, int flag)
{
    return 0;
}

/* -----------------------------------------------------------------------------
Line specific (tty) write routine.
there shouldn't be any write to the file descriptor from user level
----------------------------------------------------------------------------- */
int pppserial_write(struct tty *tp, struct uio *uio, int flag)
{
    return 0;
}

/* -----------------------------------------------------------------------------
Line specific (tty) ioctl routine.
in the case of line discipline, pppd connects the driver separatly,
and doesn't specify a unit a priori
----------------------------------------------------------------------------- */
int pppserial_ioctl(struct tty *tp, u_long cmd, caddr_t data, int flag, struct proc *p)
{
    struct pppserial 	*ld = (struct pppserial *) tp->t_sc;
    u_long		flags;
    int 		error, s;

    switch (cmd) {
        case TIOCGETD:
        case TIOCSETD:
            return -1;
            
        case PPPIOCGUNIT:
            *(int *)data = ld->lk_if.if_unit;
            break;

        case PPPIOCGFLAGS:
            *(u_int *)data = ld->flags;
            break;

        case PPPIOCSFLAGS:
            if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
                return error;
            flags = *(u_long *)data & LK_MASK;
            s = splnet();
            splimp();
            ld->flags = (ld->flags & ~LK_MASK) | flags;
            splx(s);
            break;
    }
    return 0;
}

/* -----------------------------------------------------------------------------
Called when character is available from device driver.
Only guaranteed to be at splsofttty() or spltty()
This is safe to be called while the upper half's netisr is preempted
----------------------------------------------------------------------------- */
int pppserial_input(int c, struct tty *tp)
{
    struct pppserial 	*ld = (struct pppserial *) tp->t_sc;
    struct mbuf 	*m;
    int 		ilen, s;

    //log(LOGVAL, "pppserial_input, %s c = 0x%x \n", c == 0x7e ? "----------------" : "", c);

    if (ld == NULL || tp != (struct tty *) ld->devp) {
        return 0;
    }

    ++tk_nin;

    //log(LOG_INFO, "input c = 0x%x\n", c);

    if ((tp->t_state & TS_CONNECTED) == 0) {
        LOGDBG((&ld->lk_if), (LOGVAL, "ppplink %s%d: no carrier\n", IFNAME(ld), IFUNIT(ld)));
        goto flush;
    }

    if (c & TTY_ERRORMASK) {
        /* framing error or overrun on this char - abort packet */
        LOGDBG((&ld->lk_if), (LOGVAL, "ppp line %d: line error %x\n", ld->lref, c & TTY_ERRORMASK));
        goto flush;
    }

    c &= TTY_CHARMASK;

    /*
     * Handle software flow control of output.
     */
    if (tp->t_iflag & IXON) {
        if (c == tp->t_cc[VSTOP] && tp->t_cc[VSTOP] != _POSIX_VDISABLE) {
            if ((tp->t_state & TS_TTSTOP) == 0) {
                tp->t_state |= TS_TTSTOP;
                (*cdevsw[major(tp->t_dev)].d_stop)(tp, 0);
            }
            return 0;
        }
        if (c == tp->t_cc[VSTART] && tp->t_cc[VSTART] != _POSIX_VDISABLE) {
            tp->t_state &= ~TS_TTSTOP;
            if (tp->t_oproc != NULL)
                (*tp->t_oproc)(tp);
            return 0;
        }
    }

    s = spltty();
    if (c & 0x80)
        ld->flags |= LK_RCV_B7_1;
    else
        ld->flags |= LK_RCV_B7_0;
    if (paritytab[c >> 5] & (1 << (c & 0x1F)))
        ld->flags |= LK_RCV_ODDP;
    else
        ld->flags |= LK_RCV_EVNP;
    splx(s);

    if (ld->flags & LK_LOG_RAWIN)
        pppserial_logchar(ld, c);

    if (c == PPP_FLAG) {
        ilen = ld->inlen;
        ld->inlen = 0;

        if (ld->rawinlen > 0)
            pppserial_logchar(ld, -1);

        /*
         * If LK_ESCAPED is set, then we've seen the packet
         * abort sequence "}~".
         */
        if (ld->flags & (LK_FLUSH | LK_ESCAPED)
            || (ilen > 0 && ld->infcs != PPP_GOODFCS)) {
            s = spltty();
            ld->flags |= LK_PKTLOST;	/* note the dropped packet */
            if ((ld->flags & (LK_FLUSH | LK_ESCAPED)) == 0){
                LOGDBG((&ld->lk_if), (LOGVAL, "ppp line %d: bad fcs %x, pkt len %d\n", ld->lref, ld->infcs, ilen));
                // may be should we use a mutex before updating stats
                ld->lk_if.if_ierrors++;
           } else
                ld->flags &= ~(LK_FLUSH | LK_ESCAPED);
            splx(s);
            return 0;
        }

        if (ilen < PPP_HDRLEN + PPP_FCSLEN) {
            if (ilen) {
                LOGDBG((&ld->lk_if), (LOGVAL, "ppp line %d: too short (%d)\n", ld->lref, ilen));
                s = spltty();
                // may be should we use a mutex before updating stats
                ld->lk_if.if_ierrors++;
                ld->flags |= LK_PKTLOST;
                splx(s);
            }
            return 0;
        }

        /* Remove FCS trailer.  Somewhat painful...
            remove 2 bytes from the last mbuf of the packet */
        ilen -= 2;
        if (--ld->inmc->m_len == 0) {
            for (m = ld->inm; m->m_next != ld->inmc; m = m->m_next)	 // find last header
                ;
            ld->inmc = m;
        }
        ld->inmc->m_len--;

        /* excise this mbuf chain */
        m = ld->inm;
        ld->inm = ld->inmc->m_next;
        ld->inmc->m_next = NULL;

        if (ld->flags & LK_PKTLOST) {
            s = spltty();
            ld->flags &= ~LK_PKTLOST;
            m->m_flags |= M_ERRMARK;
            splx(s);
        }

        m->m_pkthdr.len = ilen;
        IF_ENQUEUE(&ld->inrawq, m);

        schedpppnetisr();

        pppserial_getm(ld);
        return 0;
    }

    if (ld->flags & LK_FLUSH) {
        if (ld->flags & LK_LOG_FLUSH)
            pppserial_logchar(ld, c);
        return 0;
    }

    if (c < 0x20 && (ld->rasyncmap & (1 << c))) {
        return 0;
    }

    s = spltty();
    if (ld->flags & LK_ESCAPED) {
        ld->flags &= ~LK_ESCAPED;
        c ^= PPP_TRANS;
    } else if (c == PPP_ESCAPE) {
        ld->flags |= LK_ESCAPED;
        splx(s);
        return 0;
    }
    splx(s);

    /*
     * Initialize buffer on first octet received.
     * First octet could be address or protocol (when compressing
                                                 * address/control).
     * Second octet is control.
     * Third octet is first or second (when compressing protocol)
     * octet of protocol.
     * Fourth octet is second octet of protocol.
     */
    if (ld->inlen == 0) {
        /* reset the first input mbuf */
        if (ld->inm == NULL) {
            pppserial_getm(ld);
            if (ld->inm == NULL) {
                LOGDBG((&ld->lk_if), (LOGVAL, "ppp line %d: no input mbufs!\n", ld->lref));
                goto flush;
            }
        }
        m = ld->inm;
        m->m_len = 0;
        m->m_data = M_DATASTART(ld->inm);
        m->m_flags &= ~M_ERRMARK;
        ld->inmc = m;
        ld->inmp = mtod(m, char *);
        ld->infcs = PPP_INITFCS;
        if (c != PPP_ALLSTATIONS) {
        // should we switch funnel to test if_eflags ??? 
            if (!(ld->lk_if.if_eflags & IFF_PPP_ACPT_ACCOMP)) {
                LOGDBG((&ld->lk_if), (LOGVAL, "ppp line %d: garbage received: 0x%x (need 0xFF)\n", ld->lref, c));
                goto flush;
            }
           *ld->inmp++ = PPP_ALLSTATIONS;
            *ld->inmp++ = PPP_UI;
            ld->inlen += 2;
            m->m_len += 2;
        }
    }
    if (ld->inlen == 1 && c != PPP_UI) {
        LOGDBG((&ld->lk_if), (LOGVAL, "ppp line %d: missing UI (0x3), got 0x%x\n", ld->lref, c));
        goto flush;
    }
    if (ld->inlen == 2 && (c & 1) == 1) {
       /* a compressed protocol */
        *ld->inmp++ = 0;
        ld->inlen++;
        ld->inmc->m_len++;
    }
    if (ld->inlen == 3 && (c & 1) == 0) {
        LOGDBG((&ld->lk_if), (LOGVAL, "ppp line %d: bad protocol %x\n", ld->lref, (ld->inmp[-1] << 8) + c));
        goto flush;
    }

    /* packet beyond configured mru? */
    if (++ld->inlen > ld->mru + PPP_HDRLEN + PPP_FCSLEN) {
        log(LOG_INFO, "mru = %d, inlen = %d\n", ld->mru, ld->inlen);
        LOGDBG((&ld->lk_if), (LOGVAL, "ppp line %d: packet too big\n", ld->lref));
        goto flush;
    }

    /* is this mbuf full? */
    m = ld->inmc;
    if (M_TRAILINGSPACE(m) <= 0) {
        if (m->m_next == NULL) {
            pppserial_getm(ld);
            if (m->m_next == NULL) {
                LOGDBG((&ld->lk_if), (LOGVAL, "ppp line %d: too few input mbufs!\n", ld->lref));
                goto flush;
            }
        }
        ld->inmc = m = m->m_next;
        m->m_len = 0;
        m->m_data = M_DATASTART(m);
        ld->inmp = mtod(m, char *);
    }

    ++m->m_len;
    *ld->inmp++ = c;
    //ld->lk_if.if_ibytes++;	/* the if_bytes reflects the nb of actual PPP bytes received on this link */
    ld->infcs = PPP_FCS(ld->infcs, c);
    return 0;

flush:
        if (!(ld->flags & LK_FLUSH)) {
            s = spltty();
            // should we protect this one ?
            ld->lk_if.if_ierrors++;
            ld->flags |= LK_FLUSH;
            splx(s);
            if (ld->flags & LK_LOG_FLUSH)
                pppserial_logchar(ld, c);
        }
    return 0;
}

/* -----------------------------------------------------------------------------
Called at netisr when we need to send data to tty
----------------------------------------------------------------------------- */
int pppserial_ouput(struct pppserial *ld)
{
    struct tty 		*tp = (struct tty *) ld->devp;
    struct mbuf 	*m, *m2;
    u_char 		*start, *stop, *cp;
    int 		len, s, n, ndone, done, idle;

    idle = 0;
    /* XXX assumes atomic access to *tp although we're not at spltty(). */
    while (CCOUNT(&tp->t_outq) < PPP_HIWAT) {
        /*
         * See if we have an existing packet partly sent.
         * If not, get a new packet and start sending it.
         */
        m = ld->outm;
        if (m == NULL) {
            /*
             * Get another packet to be sent.
             */
            m = pppserial_dequeue(ld);
            if (m == NULL) {
                idle = 1;
                break;
            }
#if 0
            ld->lk_if.if_opackets++;
            ld->lk_if.if_obytes += m->m_pkthdr.len;
            getmicrotime(&ld->lk_if.if_lastchange);
#endif
            /*
             * The extra PPP_FLAG will start up a new packet, and thus
             * will flush any accumulated garbage.  We do this whenever
             * the line may have been idle for some time.
             */
            /* XXX as above. */
            if (CCOUNT(&tp->t_outq) == 0) {
                (void) putc(PPP_FLAG, &tp->t_outq);
            }

            /* Calculate the FCS for the first mbuf's worth. */
            ld->outfcs = pppserial_fcs(PPP_INITFCS, mtod(m, u_char *), m->m_len);
        }

        for (;;) {
            start = mtod(m, u_char *);
            len = m->m_len;
            stop = start + len;
            while (len > 0) {
                /*
                 * Find out how many bytes in the string we can
                 * handle without doing something special.
                 */
                for (cp = start; cp < stop; cp++)
                    if (ESCAPE_P(*cp))
                        break;
                n = cp - start;
                if (n) {
                    /* NetBSD (0.9 or later), 4.3-Reno or similar. */
                    ndone = n - b_to_q(start, n, &tp->t_outq);
                    len -= ndone;
                    start += ndone;

                    if (ndone < n)
                        break;	/* packet doesn't fit */
                }
                /*
                 * If there are characters left in the mbuf,
                 * the first one must be special.
                 * Put it out in a different form.
                 */
                if (len) {
                    s = spltty();
                    if (putc(PPP_ESCAPE, &tp->t_outq)) {
                        splx(s);
                        break;
                    }
                    if (putc(*start ^ PPP_TRANS, &tp->t_outq)) {
                        (void) unputc(&tp->t_outq);
                        splx(s);
                        break;
                    }
                    splx(s);
                    start++;
                    len--;
                }
            }

            /*
             * If we didn't empty this mbuf, remember where we're up to.
             * If we emptied the last mbuf, try to add the FCS and closing
             * flag, and if we can't, leave sc_outm pointing to m, but with
             * m->m_len == 0, to remind us to output the FCS and flag later.
             */
            done = len == 0;
            if (done && m->m_next == NULL) {
                u_char *p, *q;
                int c;
                u_char endseq[8];

                /*
                 * We may have to escape the bytes in the FCS.
                 */
                p = endseq;
                c = ~ld->outfcs & 0xFF;
                if (ESCAPE_P(c)) {
                    *p++ = PPP_ESCAPE;
                    *p++ = c ^ PPP_TRANS;
                } else
                    *p++ = c;
                c = (~ld->outfcs >> 8) & 0xFF;
                if (ESCAPE_P(c)) {
                    *p++ = PPP_ESCAPE;
                    *p++ = c ^ PPP_TRANS;
                } else
                    *p++ = c;
                *p++ = PPP_FLAG;

                /*
                 * Try to output the FCS and flag.  If the bytes
                 * don't all fit, back out.
                 */
                s = spltty();
                for (q = endseq; q < p; ++q)
                    if (putc(*q, &tp->t_outq)) {
                        done = 0;
                        for (; q > endseq; --q)
                            unputc(&tp->t_outq);
                        break;
                    }
                        splx(s);
            }

            if (!done) {
                /* remember where we got to */
                m->m_data = start;
                m->m_len = len;
                break;
            }

            /* Finished with this mbuf; free it and move on. */
            MFREE(m, m2);
            m = m2;
            if (m == NULL) {
                /* Finished a packet */
                break;
            }
            ld->outfcs = pppserial_fcs(ld->outfcs, mtod(m, u_char *), m->m_len);
        }

        /*
         * If m == NULL, we have finished a packet.
         * If m != NULL, we've either done as much work this time
         * as we need to, or else we've filled up the output queue.
         */
        ld->outm = m;
        if (m)
            break;
    }

    /* Call pppstart to start output again if necessary. */
    s = spltty();
    pppserial_start(tp);

    /*
     * This timeout is needed for operation on a pseudo-tty,
     * because the pty code doesn't call pppstart after it has
     * drained the t_outq.
     */
    if (!idle && (ld->flags & LK_TIMEOUT) == 0) {
        timeout(pppserial_timeout, (void *) ld, 1);
        ld->flags |= LK_TIMEOUT;
    }

    splx(s);
    return 0;
}

/* -----------------------------------------------------------------------------
Start output on async tty interface.  If the transmit queue
has drained sufficiently, arrange for pppserial_start to be
called later at splnet.
Called at spltty or higher
----------------------------------------------------------------------------- */
int pppserial_start(struct tty *tp)
{
    struct pppserial *ld = (struct pppserial *) tp->t_sc;

    // clear output flag
    ld->flags &= ~LK_TBUSY;

    /* Call output process whether or not there is any output.
        We are being called in lieu of ttstart and must do what it would. */
    if (tp->t_oproc != NULL)
        (*tp->t_oproc)(tp);

    /* If the transmit queue has drained and the tty has not hung up
        or been disconnected from the ppp unit, then try to send more data */
    if (CCOUNT(&tp->t_outq) < PPP_LOWAT
        && !((tp->t_state & TS_CONNECTED) == 0)
        && ld && tp == (struct tty *) ld->devp) {

        schedpppnetisr();
    }

    return 0;
}


/* -----------------------------------------------------------------------------
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
Timeout routine - try to start some more output.
----------------------------------------------------------------------------- */
void pppserial_timeout(void *arg)
{
    struct pppserial 	*ld = (struct pppserial *) arg;
    struct tty 		*tp = (struct tty *) ld->devp;
    int 		s;
    boolean_t 		funnel_state;

    funnel_state = thread_funnel_set(kernel_flock, TRUE);
    s = spltty();
    ld->flags &= ~LK_TIMEOUT;
    pppserial_start(tp);
    splx(s);
    (void) thread_funnel_set(kernel_flock, funnel_state);

   
}

/* -----------------------------------------------------------------------------
Allocate enough mbuf to handle current MRU
humm ? not sure that works
----------------------------------------------------------------------------- */
void pppserial_getm(struct pppserial *ld)
{
    struct mbuf 	*m, **mp;
    int 		len;

    mp = &ld->inm;
    for (len = ld->mru + PPP_HDRLEN + PPP_FCSLEN; len > 0; ){
        if ((m = *mp) == NULL) {
            MGETHDR(m, M_DONTWAIT, MT_DATA);
            if (m == NULL)
                break;
            *mp = m;
            MCLGET(m, M_DONTWAIT);
        }
        len -= M_DATASIZE(m);
        mp = &m->m_next;
    }
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void pppserial_logchar(struct pppserial *ld, int c)
{
    if (c >= 0)
        ld->rawin[ld->rawinlen++] = c;
    if (ld->rawinlen >= sizeof(ld->rawin)
        || (c < 0 && ld->rawinlen > 0)) {
        log(LOGVAL, "ppp line %d input: %*D", ld->lref, ld->rawinlen, ld->rawin, " ");
        ld->rawinlen = 0;
    }
}

/* -----------------------------------------------------------------------------
Relinquish the interface unit to another device
----------------------------------------------------------------------------- */
void pppserial_relinq(struct pppserial *ld)
{
    int s;

    s = spltty();
    if (ld->outm) {
        m_freem(ld->outm);
        ld->outm = NULL;
    }
    if (ld->inm) {
        m_freem(ld->inm);
        ld->inm = NULL;
    }
    if (ld->flags & LK_TIMEOUT) {
        untimeout(pppserial_timeout, (void *) ld);
        ld->flags &= ~LK_TIMEOUT;
    }
    splx(s);
}

/* -----------------------------------------------------------------------------
Calculate a new FCS given the current FCS and the new data
----------------------------------------------------------------------------- */
u_short pppserial_fcs(u_short fcs, u_char *cp, int len)
{
    while (len--)
        fcs = PPP_FCS(fcs, *cp++);
    return (fcs);
}

/* -----------------------------------------------------------------------------
This gets called when the interface is freed
(if dlil_if_detach has returned DLIL_WAIT_FOR_FREE)
----------------------------------------------------------------------------- */
int pppserial_if_free(struct ifnet *ifp)
{
    wakeup(&pppsoft_net_terminate);
    return 0;
}

/* -----------------------------------------------------------------------------
Process an ioctl request to the ppp link interface
----------------------------------------------------------------------------- */
int pppserial_if_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
    struct pppserial 	*ld = (struct pppserial *)ifp;
    struct ifreq 	*ifr = (struct ifreq *)data;
    struct ifpppreq 	*ifpppr = (struct ifpppreq *)data;
    int 		s = splimp(), error = 0, i;
    u_short		mru;
    struct proc		*p = current_proc();	/* XXX */

    //LOGDBG(ifp, (LOGVAL, "pppserial_if_ioctl, cmd = 0x%x\n", cmd));

    switch (cmd) {

        case SIOCSIFPPP:
            if ((error = suser(p->p_ucred, &p->p_acflag)) != 0) {
                splx(s);
                return error;
            }
            switch (ifpppr->ifr_code) {
                case IFPPP_MRU:
                    mru = ifpppr->ifr_mru;
                    if (mru >= PPP_MRU && mru <= PPP_MAXMRU) {
                        thread_funnel_switch(NETWORK_FUNNEL, KERNEL_FUNNEL);
                        ld->mru = mru;
                        pppserial_getm(ld);
                        thread_funnel_switch(KERNEL_FUNNEL, NETWORK_FUNNEL);
                    }
                   break;
                case IFPPP_ASYNCMAP:
                    thread_funnel_switch(NETWORK_FUNNEL, KERNEL_FUNNEL);
                    ld->asyncmap[0] = ifpppr->ifr_map;
                    thread_funnel_switch(KERNEL_FUNNEL, NETWORK_FUNNEL);
                   break;
                case IFPPP_NEWIF:
                    if (pppserial_firstinuse) {
                        error = ENODEV;
                        for (i = 0; i < MAX_PPPSERIAL; i++) {
                            if (!pppserial[i]) {
                                error = pppserial_attach(i);
                                if (!error) 
                                    ifpppr->ifr_newif = 0x10000 + i; // really new
                                break;
                            }
                        }
                    }
                    else {
                        ifpppr->ifr_newif = 0; 
                        pppserial_firstinuse = 1;
                    }
 
                   break;
                case IFPPP_DISPOSEIF:
                    if (ifpppr->ifr_disposeif) 
                        error = pppserial_detach(ifpppr->ifr_disposeif);
                    else 
                        pppserial_firstinuse = 0;
                   break;
                case IFPPP_RASYNCMAP:
                    thread_funnel_switch(NETWORK_FUNNEL, KERNEL_FUNNEL);
                    ld->rasyncmap = ifpppr->ifr_map;
                    thread_funnel_switch(KERNEL_FUNNEL, NETWORK_FUNNEL);
                   break;
                case IFPPP_XASYNCMAP:
                    thread_funnel_switch(NETWORK_FUNNEL, KERNEL_FUNNEL);
                    s = spltty();
                    bcopy(&ifpppr->ifr_xmap[0], ld->asyncmap, sizeof(ld->asyncmap));
                    ld->asyncmap[1] = 0;		/* mustn't escape 0x20 - 0x3f */
                    ld->asyncmap[2] &= ~0x40000000;  	/* mustn't escape 0x5e */
                    ld->asyncmap[3] |= 0x60000000;   	/* must escape 0x7d, 0x7e */
                    splx(s);
                    thread_funnel_switch(KERNEL_FUNNEL, NETWORK_FUNNEL);
                    break;
            }
            break;

        case SIOCGIFPPP:
            switch (ifpppr->ifr_code) {
                case IFPPP_MRU:
                    ifpppr->ifr_mru = ld->mru;
                   break;
                case IFPPP_ASYNCMAP:
                    ifpppr->ifr_map = ld->asyncmap[0];
                   break;
                case IFPPP_RASYNCMAP:
                    ifpppr->ifr_map = ld->rasyncmap;
                   break;
                case IFPPP_XASYNCMAP:
                    bcopy(ld->asyncmap, &ifpppr->ifr_xmap[0], sizeof(ld->asyncmap));
                   break;
                case IFPPP_CAPS:
                    bzero(&ifpppr->ifr_caps, sizeof(ifpppr->ifr_caps));
                    strcpy(ifpppr->ifr_caps.link_name, "PPP Asynchronous Line Discipline");
                    ifpppr->ifr_caps.physical = PPP_PHYS_SERIAL;
                    ifpppr->ifr_caps.flags = PPP_CAPS_ERRORDETECT + PPP_CAPS_ASYNCFRAME
                        + PPP_CAPS_PCOMP + PPP_CAPS_ACCOMP;
                    ifpppr->ifr_caps.max_mtu = PPPSERIAL_MRU;
                    ifpppr->ifr_caps.max_mru = PPPSERIAL_MRU;
                    //ifpppr->ifr_caps.cur_links = MAX_PPPSERIAL;
                    //ifpppr->ifr_caps.max_links = MAX_PPPSERIAL;
                    break;
           }
            break;

            // mtu and mru
            // driver wanted more restricted MTU must check the ioctl here
        case SIOCSIFMTU:
            if (ifr->ifr_mtu > PPP_MAXMTU)
                error = EINVAL;
            else
                ifp->if_mtu = ifr->ifr_mtu;
            break;

 //      case SIOCGIFPPPFLAGS:
            //ifr->ifr_data = sc->sc_flags;
 //           break;

        default:
            error = EOPNOTSUPP;
    }
    splx(s);
    return (error);
}

/* -----------------------------------------------------------------------------
This gets called at splnet from if_ppp.c at various times
when there is data ready to be sent
----------------------------------------------------------------------------- */
int pppserial_if_output(struct ifnet *ifp, struct mbuf *m)
{
    struct pppserial 	*ld = (struct pppserial *)ifp;
    int 		ret, len;

   //if ((ifp->if_flags & IFF_RUNNING) == 0
    if ((ifp->if_flags & IFF_PPP_CONNECTED) == 0) {
//        || (ifp->if_flags & IFF_UP) == 0) {

        return ENETDOWN;
    }

#if 0
    p = mtod(m, u_char *);
    log(LOGVAL, "pppserial_if_output, 0x ");
    for (i = 0; i < m->m_len; i++)
        log(LOGVAL, "%x ", p[i]);
    log(LOGVAL, "\n");
#endif
    
    len = m->m_pkthdr.len;
    
    thread_funnel_switch(NETWORK_FUNNEL, KERNEL_FUNNEL);
    ret = pppserial_enqueue(ld, m);
    thread_funnel_switch(KERNEL_FUNNEL, NETWORK_FUNNEL);
    if (ret)
        return ret;

    ld->lk_if.if_opackets++;
    ld->lk_if.if_obytes += len;
    getmicrotime(&ld->lk_if.if_lastchange);
    
    schedpppnetisr();

    return 0;
}

/* -----------------------------------------------------------------------------
Grab a packet to send: first try the fast queue, then the normal queue
----------------------------------------------------------------------------- */
struct mbuf *pppserial_dequeue(struct pppserial *ld)
{
    struct mbuf *m;

    IF_DEQUEUE(&ld->outfastq, m);
    if (m == NULL)
        IF_DEQUEUE(&ld->lk_if.if_snd, m);

    return m;
}

/* -----------------------------------------------------------------------------
try to enqueue the packet to the appropriate que, according to urgent flag
if fastq is full, try eith regular queue
----------------------------------------------------------------------------- */
int pppserial_enqueue(struct pppserial *ld, struct mbuf *m)
{
    int 	s = spltty();

    if ((m->m_flags & M_HIGHPRI) && !IF_QFULL(&ld->outfastq)) {
        IF_ENQUEUE(&ld->outfastq, m);
        splx(s);
        return 0;
    }

    if (!IF_QFULL(&ld->lk_if.if_snd)) {
        IF_ENQUEUE(&ld->lk_if.if_snd, m);
        splx(s);
        return 0;
    }

    IF_DROP(&ld->lk_if.if_snd);
    ld->lk_if.if_oerrors++;

    splx(s);
    return ENOBUFS;
}

/* -----------------------------------------------------------------------------
Software interrupt routine, called at spl[soft]net, from thread.
all line discipline share the same interrupt, so loop to process each one.
----------------------------------------------------------------------------- */
void pppserial_intr()
{
    int 		i, s;
    struct pppserial 	*ld;
    struct mbuf	 	*m;
    u_char 		*p;

    for (i = 0; i < MAX_PPPSERIAL; i++) {

        if (!(ld = pppserial[i]))
            continue;

        // try to output data
        thread_funnel_switch(NETWORK_FUNNEL, KERNEL_FUNNEL);
        s = splimp();
        if (!(ld->flags & LK_TBUSY)
            && (ld->lk_if.if_snd.ifq_head || ld->outfastq.ifq_head || ld->outm)) {
            ld->flags |= LK_TBUSY;
            splx(s);
            pppserial_ouput(ld);
        } else
            splx(s);
        thread_funnel_switch(KERNEL_FUNNEL, NETWORK_FUNNEL);


        // try to input data
        for (;;) {
            thread_funnel_switch(NETWORK_FUNNEL, KERNEL_FUNNEL);
            s = splimp();
            IF_DEQUEUE(&ld->inrawq, m);
            splx(s);
            thread_funnel_switch(KERNEL_FUNNEL, NETWORK_FUNNEL);

           if (m == NULL)
                break;

            ld->lk_if.if_ipackets++;
            ld->lk_if.if_ibytes += m->m_pkthdr.len;
            getmicrotime(&ld->lk_if.if_lastchange);

            if (m->m_flags & M_ERRMARK) {

                pppserial_sendevent(&ld->lk_if, KEV_PPP_PACKET_LOST);
                m->m_flags &= ~M_ERRMARK;
            }

            /* detach the ppp header from the buffer */
            p = mtod(m, u_char *);
            //log(LOGVAL, "pppserial_intr, input packet = 0x %x %x %x %x %x %x %x %x\n", p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
            m->m_pkthdr.rcvif = &ld->lk_if;
            // ppp packets are passed raw, header will be reconstructed in the packet
            m->m_pkthdr.header = 0;
            dlil_input(&ld->lk_if, m, m);	
        }
    }
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int pppserial_sendevent(struct ifnet *ifp, u_long code)
{
    struct kev_ppp_msg		evt;

    bzero(&evt, sizeof(struct kev_ppp_msg));
    evt.total_size 	= KEV_MSG_HEADER_SIZE + sizeof(struct kev_ppp_data);
    evt.vendor_code 	= KEV_VENDOR_APPLE;
    evt.kev_class 	= KEV_NETWORK_CLASS;
    evt.kev_subclass 	= KEV_PPP_SUBCLASS;
    evt.id 		= 0;
    evt.event_code 	= code;
    evt.event_data.link_data.if_family = ifp->if_family;
    evt.event_data.link_data.if_unit = ifp->if_unit;
    bcopy(ifp->if_name, &evt.event_data.link_data.if_name[0], IFNAMSIZ);

    dlil_event(ifp, (struct kern_event_msg *)&evt);
    return 0;
}
