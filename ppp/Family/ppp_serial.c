/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
*     ibytes = nb of correct PPP bytes received (does not include escapes...)
*     obytes = nb of correct PPP bytes sent (does not include escapes...)
*     ipackets = nb of PPP packet received
*     opackets = nb of PPP packet sent
*     ierrors = nb on input packets in error
*     oerrors = nb on ouptut packets in error
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
#include <sys/malloc.h>
#include <sys/syslog.h>
#include <sys/sockio.h>
#include <sys/kauth.h>

#include <kern/thread.h>
#include <kern/task.h>
#include <kern/locks.h>

#include <sys/vnode.h>
#include <net/if_types.h>
#include <net/if.h>

#include "if_ppplink.h"
#include "ppp_defs.h"
#include "if_ppp.h"

#include "ppp_domain.h"
#include "ppp_serial.h"


/* -----------------------------------------------------------------------------
Definitions
----------------------------------------------------------------------------- */

kern_return_t thread_terminate(register thread_act_t act);

/* Macro facilities */

#define LKIFNET(lk)		(((struct ppp_link *)lk)->lk_ifnet)
#define LKNAME(lk) 		(((struct ppp_link*)lk)->lk_name)
#define LKUNIT(lk) 		(((struct ppp_link*)lk)->lk_unit)

#define LKIFFDEBUG(lk) 		(LKIFNET(lk) ? ifnet_flags(LKIFNET(lk)) & IFF_DEBUG : 0 )
#define LKIFNAME(lk) 		(LKIFNET(lk) ? ifnet_name(LKIFNET(lk)) : "???")
#define LKIFUNIT(lk) 		(LKIFNET(lk) ? ifnet_unit(LKIFNET(lk)) : 0)

#define LOGLKDBG(lk, text) \
    if (LKIFNET(lk) && (ifnet_flags(LKIFNET(lk)) & IFF_DEBUG)) {	\
        IOLog text; 		\
    }


#define PPPSERIAL_MRU	2048

/* Does c need to be escaped? */
#define ESCAPE_P(c)	(ld->asyncmap[(c) >> 5] & (1 << ((c) & 0x1F)))


#define CCOUNT(q)	((q)->c_cc)


/*
 * State bits in flags.
 */
#define STATE_TBUSY	0x10000000	/* xmitter doesn't need a packet yet */
#define STATE_PKTLOST	0x20000000	/* have lost or dropped a packet */
#define	STATE_FLUSH	0x40000000	/* flush input until next PPP_FLAG */
#define	STATE_ESCAPED	0x80000000	/* saw a PPP_ESCAPE */
#define STATE_RBUSY	0x01000000	/* reception in progress */
#define STATE_CLOSING	0x02000000	/* closing the line discipline */
#define STATE_LKBUSY	0x04000000	/* activity in the link in progress */

/*  We steal two bits in the mbuf m_flags, to mark high-priority packets
for output, and received packets following lost/corrupted packets. */
//#define M_HIGHPRI	0x2000	/* output packet for sc_fastq */
#define M_ERRMARK	MBUF_BCAST	/* steal a bit in mbuf m_flags */

struct pppserial {
    /* first, the ifnet structure... */
    struct ppp_link link;			/* link interface */

    /* administrative info */
    TAILQ_ENTRY(pppserial) next;
    void			*devp;			/* pointer to device-dep structure */
    u_int16_t		lref;			/* our line number, as given by mux */
    u_int32_t		flags;			/* control/status bits */
    u_int32_t		state;			/* control/status bits */
    u_int32_t		mru;			/* mru for the line  */

    /* settings */
    ext_accm 		asyncmap;		/* async control character map */
    u_int32_t		rasyncmap;		/* receive async control char map */

    /* output data */
    struct pppqueue outq;			/* out queue */
    struct pppqueue	oobq;			/* out-of-band out queue */
    u_int16_t		outfcs;			/* FCS so far for output packet */
    mbuf_t			outm;			/* mbuf chain currently being output */

    /* input data */
    struct pppqueue	inq;			/* received packets */
    mbuf_t			inm;			/* pointer to input mbuf chain */
    char			*inmp;			/* ptr to next char in input mbuf */
    mbuf_t			inmc;			/* pointer to current input mbuf */
    int16_t			inlen;			/* length of input packet so far */
    u_int16_t		infcs;			/* FCS so far (input) */

    /* log purpose */
    u_char			rawin[16];		/* chars as received */
    int				rawinlen;		/* # in rawin */
};

/* -----------------------------------------------------------------------------
Forward declarations
----------------------------------------------------------------------------- */

static int	pppserial_open(dev_t dev, struct tty *tp);
static int	pppserial_close(struct tty *tp, int flag);
static int	pppserial_read(struct tty *tp,  uio_t uio, int flag);
static int	pppserial_write(struct tty *tp,  uio_t uio, int flag);
static int	pppserial_ioctl(struct tty *tp, u_long cmd, caddr_t data, int flag, struct proc *);
static int	pppserial_input(int c, struct tty *tp);
static void	pppserial_start(struct tty *tp);


static u_int16_t	pppserial_fcs(u_int16_t fcs, u_char *cp, int len);
static void	pppserial_getm(struct pppserial *ld);
static void	pppserial_logchar(struct pppserial *, int);
static int	pppserial_lk_output(struct ppp_link *link, mbuf_t m);
static int 	pppserial_lk_ioctl(struct ppp_link *link, u_long cmd, void *data);
static int 	pppserial_attach(struct tty *ttyp, struct ppp_link **link);
static int 	pppserial_detach(struct ppp_link *link);
static int 	pppserial_findfreeunit(u_int16_t *freeunit);

static void 	pppisr_thread(void);
static void 	pppserial_intr();

/* -----------------------------------------------------------------------------
Globals
----------------------------------------------------------------------------- */

/*
 * The locking strategy relies on the global ppp domain mutex 
 * The mutex is taken when entering a PPP line discpline  function
 * The mutex is released when calling into the underlying driver, e.g. when calling putc()
 * The mutex is assumed to be already taken when entering a PPP link function
 * The mutex protect access to the globals and to the pppserial structure
 */
extern lck_mtx_t	*ppp_domain_mutex;

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
    pppserial_ioctl, pppserial_input, pppserial_start, ttymodem,
};


int	pppsoft_net_wakeup;
int	pppsoft_net_terminate;
int	pppnetisr;


#define	setpppsoftnet()	(wakeup((caddr_t)&pppsoft_net_wakeup))
#define	schedpppnetisr()	{ pppnetisr = 1; setpppsoftnet(); }


static TAILQ_HEAD(, pppserial) 	pppserial_head;
static thread_t pppserial_thread;


/* -----------------------------------------------------------------------------
register line discipline
----------------------------------------------------------------------------- */
int pppserial_init()
{
	kern_return_t ret;

    /* No need to lock the mutex here as the structures are not known yet */

    pppserial_disc = linesw[PPPDISC];
    linesw[PPPDISC] = pppdisc;

    TAILQ_INIT(&pppserial_head);
    
    // Start up netisr thread
    pppsoft_net_terminate = 0;
    pppsoft_net_wakeup = 0;
    pppnetisr = 0;
    pppserial_thread = 0;
	ret = kernel_thread_start((thread_continue_t)pppisr_thread, NULL, &pppserial_thread);
	if (ret != KERN_SUCCESS)
		return ret;
		
    return KERN_SUCCESS;
}

/* -----------------------------------------------------------------------------
deregister line discipline
----------------------------------------------------------------------------- */
int pppserial_dispose()
{
    // can't dispose if serial lines are in use
    if (TAILQ_FIRST(&pppserial_head))
        return EBUSY;
    
    // be sure are not already terminated
	lck_mtx_lock(ppp_domain_mutex);

    if (!pppsoft_net_terminate) {
        pppsoft_net_terminate = 1;
        wakeup(&pppsoft_net_wakeup);
        msleep(&pppsoft_net_terminate, ppp_domain_mutex, PZERO+1, 0, 0);
        linesw[PPPDISC] = pppserial_disc;
    }

	lck_mtx_unlock(ppp_domain_mutex);
	
	if (pppserial_thread) {
		thread_deallocate(pppserial_thread);
		pppserial_thread = 0;
	}
	
    return KERN_SUCCESS;
}

/* -----------------------------------------------------------------------------
find a free unit in the interface list
----------------------------------------------------------------------------- */
int pppserial_findfreeunit(u_short *freeunit)
{
    struct pppserial  	*ld = TAILQ_FIRST(&pppserial_head);
    u_short 		unit = 0;
	
    while (ld) {
    	if (ld->link.lk_unit == unit) {
            unit++;
            ld = TAILQ_FIRST(&pppserial_head); // restart
        }
        else 
            ld = TAILQ_NEXT(ld, next); // continue
    }
    *freeunit = unit;
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int pppserial_attach(struct tty *ttyp, struct ppp_link **link)
{
    int 		ret;
    u_short 		unit;
    struct ppp_link  	*lk;
    struct pppserial  	*ld;

    //IOLog("pppserial_attach\n");

    // Note : we allocate/find number/insert in queue in that specific order
    // because of funnels and race condition issues

    MALLOC(ld, struct pppserial *, sizeof(struct pppserial), M_TEMP, M_WAITOK);
    if (!ld)
        return ENOMEM;
		
	lck_mtx_lock(ppp_domain_mutex);
    
    if (pppserial_findfreeunit(&unit)) {
        FREE(ld, M_TEMP);
        lck_mtx_unlock(ppp_domain_mutex);
        return ENOMEM;
    }
        
    bzero(ld, sizeof(struct pppserial));
    
    TAILQ_INSERT_TAIL(&pppserial_head, ld, next);
    lk = (struct ppp_link *) ld;

    // it's time now to register our brand new link
    lk->lk_name 	= (u_char*)APPLE_PPP_NAME_SERIAL;
    lk->lk_mtu 		= PPP_MTU;
    lk->lk_mru 		= PPP_MTU; //PPP_MAXMRU;
    lk->lk_type 	= PPP_TYPE_SERIAL;
    lk->lk_hdrlen 	= PPP_HDRLEN;
    lk->lk_baudrate     = ttyp->t_ospeed;
	lk->lk_support = PPP_LINK_ASYNC + PPP_LINK_OOB_QUEUE + PPP_LINK_ERRORDETECT;
    lk->lk_ioctl 	= pppserial_lk_ioctl;
    lk->lk_output 	= pppserial_lk_output;
    lk->lk_unit 	= unit;

    ld->devp 		= ttyp;
    ld->asyncmap[0] 	= 0xffffffff;
    ld->asyncmap[3] 	= 0x60000000;
    ld->inq.maxlen 	= IFQ_MAXLEN;
    ld->outq.maxlen = IFQ_MAXLEN;
    ld->oobq.maxlen = 10;
    pppserial_getm(ld);
    
    ret = ppp_link_attach(&ld->link);
    if (ret) {
        TAILQ_REMOVE(&pppserial_head, ld, next);

		lck_mtx_unlock(ppp_domain_mutex);

		IOLog("pppserial_attach, error = %d, (ld = 0x%x)\n", ret, &ld->link);
        FREE(ld, M_TEMP);
        return ret;
    }
	lck_mtx_unlock(ppp_domain_mutex);

    //IOLog("pppserial_attach, link index = %d, (ld = 0x%x)\n", lk->lk_index, lk);

    *link = lk;

    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int pppserial_detach(struct ppp_link *link)
{
    struct pppserial  	*ld = (struct pppserial *)link;
    mbuf_t			m;

	lck_mtx_lock(ppp_domain_mutex);

    ld->state |= STATE_CLOSING; 
    
    while (ld->state & STATE_LKBUSY) {
        msleep(&ld->state, ppp_domain_mutex, PZERO+1, 0, 0);
    }

    for (;;) {
        m = ppp_dequeue(&ld->inq);
        if (m == NULL)
            break;
        mbuf_freem(m);
    }

    for (;;) {
        m = ppp_dequeue(&ld->outq);
        if (m == NULL)
            break;
        mbuf_freem(m);
    }

    for (;;) {
        m = ppp_dequeue(&ld->oobq);
        if (m == NULL)
            break;
        mbuf_freem(m);
    }
    
    if (ld->outm) {
        mbuf_freem(ld->outm);
        ld->outm = 0;
    }
    
    TAILQ_REMOVE(&pppserial_head, ld, next);

    ppp_link_detach(link);

	lck_mtx_unlock(ppp_domain_mutex);

    FREE(ld, M_TEMP);

    return 0;
}

/* -----------------------------------------------------------------------------
All routines this thread calls expect to be called at splnet
all line discipline share the same thread
----------------------------------------------------------------------------- */
void pppisr_thread(void)
{
	lck_mtx_lock(ppp_domain_mutex);

    while (!pppsoft_net_terminate) {
        if (pppnetisr) {

            pppnetisr = 0;
            pppserial_intr();
        }

        msleep(&pppsoft_net_wakeup, ppp_domain_mutex, PZERO+1, 0, 0);
    }

	lck_mtx_unlock(ppp_domain_mutex);

    wakeup(&pppsoft_net_terminate);

    thread_terminate(current_thread());
    /* NOTREACHED */
}

/* -----------------------------------------------------------------------------
Line specific open routine for async tty devices.
Attach the given tty to the first available ppp unit.
Called from device open routine or ttioctl() at >= splsofttty()
----------------------------------------------------------------------------- */
int pppserial_open(dev_t dev, struct tty *tp)
{
    struct pppserial 	*ld;
    int 		error;

    if (kauth_cred_issuser(kauth_cred_get()) == 0) {
        printf("pppserial_open EPERM\n");
        return EPERM;
    }
    if (tp->t_line == PPPDISC) {
        ld = (struct pppserial *) tp->t_sc;
        if (ld && ld->devp == (void *)tp) {
            return 0;		// already opened
        }
    }

    ttyflush(tp, FREAD | FWRITE);

    error = pppserial_attach(tp, (struct ppp_link **)&tp->t_sc);

    return error;
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

    ttyflush(tp, FREAD | FWRITE);
    tp->t_line = 0;

    if (ld) {
        if (tp == (struct tty *) ld->devp)
            pppserial_detach((struct ppp_link *)ld);
            
        tp->t_sc = NULL;
    }
    return 0;
}

/* -----------------------------------------------------------------------------
Line specific (tty) read routine.
there shouldn't be any write to the file descriptor from user level
----------------------------------------------------------------------------- */
int pppserial_read(struct tty *tp, uio_t uio, int flag)
{
    return 0;
}

/* -----------------------------------------------------------------------------
Line specific (tty) write routine.
there shouldn't be any write to the file descriptor from user level
----------------------------------------------------------------------------- */
int pppserial_write(struct tty *tp,  uio_t uio, int flag)
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
    struct pppserial 	*ld;
    int error = ENOTTY;

    /*
        return -1 is the ioctl is ignored by the line discipline, 
           ttioctl will then be called for further processing
        return 0 is the ioctl is successfully processed
        return any positive error if ioctl is processed with an error
    */
    
    lck_mtx_lock(ppp_domain_mutex);
	
    ld = (struct pppserial *) tp->t_sc;
    
    switch (cmd) {
        case TIOCGETD:
            LOGLKDBG(ld, ("pppserial_ioctl: (ifnet = %s%d) (link = %s%d) TIOCGETD\n", 
                LKIFNAME(ld), LKIFUNIT(ld), LKNAME(ld), LKUNIT(ld)));
            break;
        case TIOCSETD:
            LOGLKDBG(ld, ("pppserial_ioctl: (ifnet = %s%d) (link = %s%d) TIOCSETD\n", 
                LKIFNAME(ld), LKIFUNIT(ld), LKNAME(ld), LKUNIT(ld)));
            break;
        case TIOCFLUSH:
            LOGLKDBG(ld, ("pppserial_ioctl: (ifnet = %s%d) (link = %s%d) TIOCFLUSH\n", 
                LKIFNAME(ld), LKIFUNIT(ld), LKNAME(ld), LKUNIT(ld)));
            break;
        case TCSAFLUSH:
            LOGLKDBG(ld, ("pppserial_ioctl: (ifnet = %s%d) (link = %s%d) TCSAFLUSH\n", 
                LKIFNAME(ld), LKIFUNIT(ld), LKNAME(ld), LKUNIT(ld)));
            break;
        case TIOCMGET:
            LOGLKDBG(ld, ("pppserial_ioctl: (ifnet = %s%d) (link = %s%d) TIOCMGET\n", 
                LKIFNAME(ld), LKIFUNIT(ld), LKNAME(ld), LKUNIT(ld)));
            break;

        case PPPIOCGCHAN:
            LOGLKDBG(ld, ("pppserial_ioctl: (ifnet = %s%d) (link = %s%d) PPPIOCGCHAN = %d\n", 
                LKIFNAME(ld), LKIFUNIT(ld), LKNAME(ld), LKUNIT(ld), ld ? ld->link.lk_index : 0));
            if (!ld) {
                error = ENXIO;	// can it happen ?
            } else {
                *(u_int32_t *)data = ld->link.lk_index;
                error = 0;
            }
            break;
    }
	lck_mtx_unlock(ppp_domain_mutex);

    return error;
}

/* -----------------------------------------------------------------------------
Called when character is available from device driver.
Only guaranteed to be at splsofttty() or spltty()
This is safe to be called while the upper half's netisr is preempted
----------------------------------------------------------------------------- */
int pppserial_input(int c, struct tty *tp)
{
    struct pppserial 	*ld = (struct pppserial *) tp->t_sc;
    mbuf_t		m;
    int 		ilen, err;

    //IOLog("pppserial_input, %s c = 0x%x '%c'\n", c == 0x7e ? "----------------" : "", c, ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))? c : '.');

    if (ld == NULL || tp != (struct tty *) ld->devp) {
        return 0;
    }

    ++tk_nin;

    //IOLog("input c = 0x%x\n", c);

	lck_mtx_lock(ppp_domain_mutex);

    if ((tp->t_state & TS_CONNECTED) == 0) {
        LOGLKDBG(ld, ("pppserial_input: (ifnet = %s%d) (link = %s%d) no carrier\n", 
            LKIFNAME(ld), LKIFUNIT(ld), LKNAME(ld), LKUNIT(ld)));
        goto flush;
    }

    if (c & TTY_ERRORMASK) {
        /* framing error or overrun on this char - abort packet */
        LOGLKDBG(ld, ("pppserial_input: (ifnet = %s%d) (link = %s%d) line error %x\n", 
            LKIFNAME(ld), LKIFUNIT(ld), LKNAME(ld), LKUNIT(ld), c & TTY_ERRORMASK));
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
                lck_mtx_unlock(ppp_domain_mutex);
                (*cdevsw[major(tp->t_dev)].d_stop)(tp, 0);
            } else {
                lck_mtx_unlock(ppp_domain_mutex);
            }
            return 0;
        }
        if (c == tp->t_cc[VSTART] && tp->t_cc[VSTART] != _POSIX_VDISABLE) {
            tp->t_state &= ~TS_TTSTOP;
            if (tp->t_oproc != NULL) {
                lck_mtx_unlock(ppp_domain_mutex);
                (*tp->t_oproc)(tp);
            } else {
                lck_mtx_unlock(ppp_domain_mutex);
            }
            return 0;
        }
    }

    //s = spltty();
    if (c & 0x80)
        ld->flags |= SC_RCV_B7_1;
    else
        ld->flags |= SC_RCV_B7_0;
    if (paritytab[c >> 5] & (1 << (c & 0x1F)))
        ld->flags |= SC_RCV_ODDP;
    else
        ld->flags |= SC_RCV_EVNP;
    //splx(s);

    if (ld->flags & SC_LOG_RAWIN)
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
        if (ld->state & (STATE_FLUSH | STATE_ESCAPED)
            || (ilen > 0 && ld->infcs != PPP_GOODFCS)) {
            //s = spltty();
            ld->state |= STATE_PKTLOST;	/* note the dropped packet */
            if ((ld->state & (STATE_FLUSH | STATE_ESCAPED)) == 0){
                LOGLKDBG(ld, ("pppserial_input: (ifnet = %s%d) (link = %s%d) bad fcs %x, pkt len %d\n", 
                    LKIFNAME(ld), LKIFUNIT(ld), LKNAME(ld), LKUNIT(ld), ld->infcs, ilen));
                ld->link.lk_ierrors++;                
           } else
                ld->state &= ~(STATE_FLUSH | STATE_ESCAPED);
            //splx(s);
            lck_mtx_unlock(ppp_domain_mutex);

            return 0;
        }

        if (ilen < PPP_HDRLEN + PPP_FCSLEN) {
            if (ilen) {
                LOGLKDBG(ld, ("pppserial_input: (ifnet = %s%d) (link = %s%d) too short (%d)\n", 
                    LKIFNAME(ld), LKIFUNIT(ld), LKNAME(ld), LKUNIT(ld), ilen));
                //s = spltty();
                ld->link.lk_ierrors++;
                ld->state |= STATE_PKTLOST;
                //splx(s);
            }
            lck_mtx_unlock(ppp_domain_mutex);

            return 0;
        }

        /* Remove FCS trailer.  Somewhat painful...
            remove 2 bytes from the last mbuf of the packet */
        ilen -= 2;
        mbuf_setlen(ld->inmc, mbuf_len(ld->inmc) - 1);
        if (mbuf_len(ld->inmc) == 0) {
            for (m = ld->inm; mbuf_next(m) != ld->inmc; m = mbuf_next(m))	 // find last header
                ;
            ld->inmc = m;
        }
        mbuf_setlen(ld->inmc, mbuf_len(ld->inmc) - 1);

        /* excise this mbuf chain */
        m = ld->inm;
        ld->inm = mbuf_next(ld->inmc);
        mbuf_setnext(ld->inmc, NULL);

        if (ld->state & STATE_PKTLOST) {
            //s = spltty();
            ld->state &= ~STATE_PKTLOST;
            mbuf_setflags(m, mbuf_flags(m) | M_ERRMARK);
            //splx(s);
        }
        mbuf_pkthdr_setlen(m, ilen);
        ld->link.lk_ipackets++;
        ppp_enqueue(&ld->inq, m);

        schedpppnetisr();

        pppserial_getm(ld);

        lck_mtx_unlock(ppp_domain_mutex);

        return 0;
    }

    if (ld->state & STATE_FLUSH) {
        if (ld->flags & SC_LOG_FLUSH)
            pppserial_logchar(ld, c);

        lck_mtx_unlock(ppp_domain_mutex);

        return 0;
    }

    if (c < 0x20 && (ld->rasyncmap & (1 << c))) {
        lck_mtx_unlock(ppp_domain_mutex);

        return 0;
    }

    //s = spltty();
    if (ld->state & STATE_ESCAPED) {
        ld->state &= ~STATE_ESCAPED;
        c ^= PPP_TRANS;
    } else if (c == PPP_ESCAPE) {
        ld->state |= STATE_ESCAPED;
        //splx(s);

        lck_mtx_unlock(ppp_domain_mutex);

        return 0;
    }
    //splx(s);

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
    /* packet beyond configured mru? */
        /* reset the first input mbuf */
        if (ld->inm == NULL) {
            pppserial_getm(ld);
            if (ld->inm == NULL) {
                LOGLKDBG(ld, ("pppserial_input: (ifnet = %s%d) (link = %s%d) no input mbufs!\n", 
                    LKIFNAME(ld), LKIFUNIT(ld), LKNAME(ld), LKUNIT(ld)));
                goto flush;
            }
        }
        m = ld->inm;
        if ((err = mbuf_setdata(m, mbuf_datastart(ld->inm), 0))) {
            LOGLKDBG(ld, ("pppserial_input: (ifnet = %s%d) (link = %s%d) mbuf_setdata failed!\n", 
                          LKIFNAME(ld), LKIFUNIT(ld), LKNAME(ld), LKUNIT(ld)));
            goto flush;
        }
        mbuf_setflags(m, mbuf_flags(m) & ~M_ERRMARK);
        ld->inmc = m;
        ld->inmp = mbuf_data(m);
        ld->infcs = PPP_INITFCS;
        if (c != PPP_ALLSTATIONS) {
            if (ld->flags & SC_REJ_COMP_AC) {
                LOGLKDBG(ld, ("pppserial_input: (ifnet = %s%d) (link = %s%d) garbage received: 0x%x (need 0xFF)\n", 
                    LKIFNAME(ld), LKIFUNIT(ld), LKNAME(ld), LKUNIT(ld), c));
                goto flush;
            }
           *ld->inmp++ = PPP_ALLSTATIONS;
            *ld->inmp++ = PPP_UI;
            ld->inlen += 2;
			mbuf_setlen(m, mbuf_len(m) + 2);
        }
    }
    if (ld->inlen == 1 && c != PPP_UI) {
        LOGLKDBG(ld, ("pppserial_input: (ifnet = %s%d) (link = %s%d) missing UI (0x3), got 0x%x\n", 
            LKIFNAME(ld), LKIFUNIT(ld), LKNAME(ld), LKUNIT(ld), c));
        goto flush;
    }
    if (ld->inlen == 2 && (c & 1) == 1) {
       /* a compressed protocol */
        *ld->inmp++ = 0;
        ld->inlen++;
		mbuf_setlen(ld->inmc, mbuf_len(ld->inmc) + 1);
    }
    if (ld->inlen == 3 && (c & 1) == 0) {
        LOGLKDBG(ld, ("pppserial_input: (ifnet = %s%d) (link = %s%d) bad protocol %x\n", 
            LKIFNAME(ld), LKIFUNIT(ld), LKNAME(ld), LKUNIT(ld), (ld->inmp[-1] << 8) + c));
        goto flush;
    }

    /* packet beyond configured mru? */
    if (++ld->inlen > ld->mru + PPP_HDRLEN + PPP_FCSLEN) {
            LOGLKDBG(ld, 
                ("pppserial_input: (ifnet = %s%d) (link = %s%d) packet too big, mru = %d, inlen = %d\n", 
                LKIFNAME(ld), LKIFUNIT(ld), LKNAME(ld), LKUNIT(ld), ld->mru, ld->inlen));
        goto flush;
    }

    /* is this mbuf full? */
    m = ld->inmc;
    if (mbuf_trailingspace(m) <= 0) {
        if (mbuf_next(m) == NULL) {
            pppserial_getm(ld);
            if (mbuf_next(m) == NULL) {
                LOGLKDBG(ld, ("pppserial_input: (ifnet = %s%d) (link = %s%d) too few input mbufs!\n", 
                    LKIFNAME(ld), LKIFUNIT(ld), LKNAME(ld), LKUNIT(ld)));
                goto flush;
            }
        }
        ld->inmc = m = mbuf_next(m);
        if ((err = mbuf_setdata(m, mbuf_datastart(m), 0))) {
            LOGLKDBG(ld, ("pppserial_input: (ifnet = %s%d) (link = %s%d) mbuf_setdata!\n", 
                     LKIFNAME(ld), LKIFUNIT(ld), LKNAME(ld), LKUNIT(ld)));
            goto flush;
        }
        ld->inmp = mbuf_data(m);
    }

	mbuf_setlen(m, mbuf_len(m) + 1);
    *ld->inmp++ = c;
    ld->link.lk_ibytes++;	/* the if_bytes reflects the nb of actual PPP bytes received on this link */
    ld->infcs = PPP_FCS(ld->infcs, c);

    lck_mtx_unlock(ppp_domain_mutex);

    return 0;

flush:
    if (!(ld->state & STATE_FLUSH)) {
        //s = spltty();
        ld->link.lk_ierrors++;
        ld->state |= STATE_FLUSH;
        //splx(s);
        if (ld->flags & SC_LOG_FLUSH)
            pppserial_logchar(ld, c);
    }

    lck_mtx_unlock(ppp_domain_mutex);

    return 0;
}

/* -----------------------------------------------------------------------------
Called at netisr when we need to send data to tty
----------------------------------------------------------------------------- */
int pppserial_ouput(struct pppserial *ld)
{
    struct tty 		*tp = (struct tty *) ld->devp;
    mbuf_t		m,m2;
    u_char 		*start, *stop, *cp;
    int 		len, n, ndone, done, idle;

	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);

	/* Enforce lock ordering without ref counting: open race window */
	lck_mtx_unlock(ppp_domain_mutex);
	tty_lock(tp);
	lck_mtx_lock(ppp_domain_mutex);

    idle = 0;

    while (CCOUNT(&tp->t_outq) < tp->t_hiwat) {
        /*
         * See if we have an existing packet partly sent.
         * If not, get a new packet and start sending it.
         */
        m = ld->outm;
        if (m == NULL) {
            /*
             * Get another packet to be sent.
             */
            m = ppp_dequeue(&ld->oobq);
            if (m == NULL) {
				m = ppp_dequeue(&ld->outq);
				if (m == NULL) {
					idle = 1;
					break;
				}
            }


#if 0
                // the link driver need to add ppp framing header (FF03)
                // does it only if address compresison has not been negociated if the link need it.
do that if not LCP
           if (!(ld->flags & SC_COMP_AC)) {
                M_PREPEND(m, 2, MBUF_DONTWAIT);
                if (m == 0) {
                    idle = 1;
                    break;
                }
                p = mtod(m, u_char *);
                *p++ = PPP_ALLSTATIONS;
                *p++ = PPP_UI;
            }
#endif
            ld->link.lk_opackets++;
            ld->link.lk_obytes += mbuf_pkthdr_len(m);

            /*
             * The extra PPP_FLAG will start up a new packet, and thus
             * will flush any accumulated garbage.  We do this whenever
             * the line may have been idle for some time.
             */
            /* XXX as above. */
            if (CCOUNT(&tp->t_outq) == 0) {
                lck_mtx_unlock(ppp_domain_mutex);
                
                (void) putc(PPP_FLAG, &tp->t_outq);
                
                lck_mtx_lock(ppp_domain_mutex);
            }

            /* Calculate the FCS for the first mbuf's worth. */
            ld->outfcs = pppserial_fcs(PPP_INITFCS, mbuf_data(m), mbuf_len(m));
        }

        for (;;) {
            start = mbuf_data(m);
            len = mbuf_len(m);
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
                    lck_mtx_unlock(ppp_domain_mutex);
                    if (putc(PPP_ESCAPE, &tp->t_outq)) {
                        lck_mtx_lock(ppp_domain_mutex);
                        break;
                    }
                    if (putc(*start ^ PPP_TRANS, &tp->t_outq)) {
                        (void) unputc(&tp->t_outq);
                        lck_mtx_lock(ppp_domain_mutex);
                        break;
                    }
                    lck_mtx_lock(ppp_domain_mutex);
                    //splx(s);
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
            if (done && mbuf_next(m) == NULL) {
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
                //s = spltty();
                lck_mtx_unlock(ppp_domain_mutex);
                for (q = endseq; q < p; ++q) {
                    if (putc(*q, &tp->t_outq)) {
                        done = 0;
                        for (; q > endseq; --q)
                            unputc(&tp->t_outq);
                        break;
                    }
                }
                lck_mtx_lock(ppp_domain_mutex);
                        //splx(s);
            }

            if (!done) {
                /* remember where we got to */
                mbuf_setdata(m, start, len);
                break;
            }

            /* Finished with this mbuf; free it and move on. */
			m2 = mbuf_free(m);
            m = m2;
            if (m == NULL) {
                /* Finished a packet */
                break;
            }
            ld->outfcs = pppserial_fcs(ld->outfcs, mbuf_data(m), mbuf_len(m));
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
    
    /* We release ppp_domain_mutex before calling t_oproc to avoid deadlock */
	lck_mtx_unlock(ppp_domain_mutex);

    /* Call oproc output funtion. */
    if (tp->t_oproc != NULL)
        (*tp->t_oproc)(tp);

	tty_unlock(tp);
	lck_mtx_lock(ppp_domain_mutex);
	
    return 0;
}

/* -----------------------------------------------------------------------------
Start output on async tty interface.  If the transmit queue
has drained sufficiently, arrange for pppserial_start to be
called later at splnet.
Called at spltty or higher
----------------------------------------------------------------------------- */
void pppserial_start(struct tty *tp)
{
    struct pppserial *ld = (struct pppserial *) tp->t_sc;

    lck_mtx_lock(ppp_domain_mutex);

    // clear output flag
    ld->state &= ~STATE_TBUSY;

    /* If the transmit queue has drained and the tty has not hung up
        or been disconnected from the ppp unit, then try to send more data */
    if (CCOUNT(&tp->t_outq) < tp->t_lowat
        && !((tp->t_state & TS_CONNECTED) == 0)
        && ld && tp == (struct tty *) ld->devp) {

        schedpppnetisr();
    }

    lck_mtx_unlock(ppp_domain_mutex);
	
	ttwwakeup(tp);

    return;
}


/* -----------------------------------------------------------------------------
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
Allocate enough mbuf to handle current MRU
humm ? not sure that works
----------------------------------------------------------------------------- */
void pppserial_getm(struct pppserial *ld)
{
    mbuf_t		m, m1 = NULL;
    int 		len;
	
	/* get the first cluster */
	if (ld->inm == 0) {
		if (mbuf_getpacket(MBUF_DONTWAIT, &ld->inm) != 0)
			return;
	}

	/* get chain max len */
	for (len = 0, m = ld->inm; m != 0; m = mbuf_next(m)) {
        len += mbuf_maxlen(m);
		m1 = m;
	}
	
    while (len < ld->mru + PPP_HDRLEN + PPP_FCSLEN) {
		
		m = 0;
		if (mbuf_getpacket(MBUF_DONTWAIT, &m) != 0)
			return;
		
		mbuf_setnext(m1, m);
        len += mbuf_maxlen(m);
		m1 = m;
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
        IOLog("ppp line %d input: %*D\n", ld->lref, ld->rawinlen, ld->rawin, " ");
        ld->rawinlen = 0;
    }
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
Process an ioctl request to the ppp link interface
----------------------------------------------------------------------------- */
int pppserial_lk_ioctl(struct ppp_link *link, u_long cmd, void *data)
{
    struct pppserial 	*ld = (struct pppserial *)link;
    int 		error = 0;
    u_short		mru;

	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);

    switch (cmd) {

        case PPPIOCSMRU:
            LOGLKDBG(ld, ("pppserial_lk_ioctl: (ifnet = %s%d) (link = %s%d) ld = 0x%x, PPPIOCSMRU = 0x%x\n", 
                    LKIFNAME(ld), LKIFUNIT(ld), LKNAME(ld), LKUNIT(ld), ld, *(u_int32_t *)data));
            if (kauth_cred_issuser(kauth_cred_get()) == 0) {
                error = EPERM;
                break;
            }
            mru = *(u_int32_t *)data;
            ld->mru = mru;
            pppserial_getm(ld);
            break;
            
        case PPPIOCSASYNCMAP:
            LOGLKDBG(ld, ("pppserial_lk_ioctl: (ifnet = %s%d) (link = %s%d) ld = 0x%x, PPPIOCSASYNCMAP = 0x%x\n", 
                    LKIFNAME(ld), LKIFUNIT(ld), LKNAME(ld), LKUNIT(ld), ld, *(u_int32_t *)data));
            if (kauth_cred_issuser(kauth_cred_get()) == 0) {
                error = EPERM;
                break;
            }
            ld->asyncmap[0] = *(u_int32_t *)data;
            break;

        case PPPIOCSRASYNCMAP:
            LOGLKDBG(ld, ("pppserial_lk_ioctl: (ifnet = %s%d) (link = %s%d) ld = 0x%x, PPPIOCSRASYNCMAP = 0x%x\n", 
                    LKIFNAME(ld), LKIFUNIT(ld), LKNAME(ld), LKUNIT(ld), ld, *(u_int32_t *)data));
            if (kauth_cred_issuser(kauth_cred_get()) == 0) {
                error = EPERM;
                break;
            }
            ld->rasyncmap = *(u_int32_t *)data;
            break;
            
        case PPPIOCSXASYNCMAP:
            LOGLKDBG(ld, ("pppserial_lk_ioctl: (ifnet = %s%d) (link = %s%d) ld = 0x%x, PPPIOCSXASYNCMAP\n", 
                    LKIFNAME(ld), LKIFUNIT(ld), LKNAME(ld), LKUNIT(ld), ld));
            if (kauth_cred_issuser(kauth_cred_get()) == 0) {
                error = EPERM;
                break;
            }
            bcopy(data, ld->asyncmap, sizeof(ld->asyncmap));
            ld->asyncmap[1] = 0;		/* mustn't escape 0x20 - 0x3f */
            ld->asyncmap[2] &= ~0x40000000;  	/* mustn't escape 0x5e */
            ld->asyncmap[3] |= 0x60000000;   	/* must escape 0x7d, 0x7e */
            break;

         case PPPIOCGASYNCMAP:
            LOGLKDBG(ld, ("pppserial_lk_ioctl: (ifnet = %s%d) (link = %s%d) ld = 0x%x, PPPIOCGASYNCMAP = 0x%x\n", 
                    LKIFNAME(ld), LKIFUNIT(ld), LKNAME(ld), LKUNIT(ld), ld, ld->asyncmap[0]));
            *(u_int32_t *)data = ld->asyncmap[0];
            break;
                
        case PPPIOCGRASYNCMAP:
            LOGLKDBG(ld, ("pppserial_lk_ioctl: (ifnet = %s%d) (link = %s%d) ld = 0x%x, PPPIOCGRASYNCMAP = 0x%x\n", 
                    LKIFNAME(ld), LKIFUNIT(ld), LKNAME(ld), LKUNIT(ld), ld, ld->rasyncmap));
            *(u_int32_t *)data = ld->rasyncmap;
            break;
            
         case PPPIOCGXASYNCMAP:
            LOGLKDBG(ld, ("pppserial_lk_ioctl: (ifnet = %s%d) (link = %s%d) ld = 0x%x, PPPIOCGXASYNCMAP\n", 
                    LKIFNAME(ld), LKIFUNIT(ld), LKNAME(ld), LKUNIT(ld), ld));
            bcopy(ld->asyncmap, data, sizeof(ld->asyncmap));
            break;

        default:
            error = ENOTSUP;
    }

    return error;
}

/* -----------------------------------------------------------------------------
This gets called at splnet from if_ppp.c at various times
when there is data ready to be sent
----------------------------------------------------------------------------- */
int pppserial_lk_output(struct ppp_link *link, mbuf_t m)
{
    struct pppserial 	*ld = (struct pppserial *)link;
    int 		ret = ENOBUFS;

	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);

#if 0
    int i;
    u_char *p = mbuf_data(m);
    IOLog("pppserial_lk_output, 0x ");
    for (i = 0; i < mbuf_len(m); i++)
        IOLog("%x ", p[i]);
    IOLog("\n");
#endif
    
	/* check for oob packet first */
	if (mbuf_type(m) == MBUF_TYPE_OOBDATA) {
		if (ppp_qfull(&ld->oobq)) {
			mbuf_settype(m, MBUF_TYPE_DATA);
			ppp_drop(&ld->oobq);
			goto dropit;
		}
	}
	else {
		/* queue is already full, caller should have checked flag */
		if (link->lk_flags & SC_XMIT_FULL) {
			ppp_drop(&ld->outq);
			goto dropit;
		}
	}
    
	if (mbuf_type(m) == MBUF_TYPE_OOBDATA) {
		mbuf_settype(m, MBUF_TYPE_DATA);
		ppp_enqueue(&ld->oobq, m);
	}
	else {
		ppp_enqueue(&ld->outq, m);
		if (ppp_qfull(&ld->outq)) {
			/* queue is now full, flag it for caller */
			link->lk_flags |= SC_XMIT_FULL;
		}
	}
	
    schedpppnetisr();

    return 0;
	
dropit:
	link->lk_oerrors++;
	mbuf_freem(m);
	return ret;
}

/* -----------------------------------------------------------------------------
Software interrupt routine, called at spl[soft]net, from thread.
all line discipline share the same interrupt, so loop to process each one.
----------------------------------------------------------------------------- */
void pppserial_intr()
{
    struct pppserial 	*ld;
    struct ppp_link 	*link;
    mbuf_t				m;

	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);

    TAILQ_FOREACH(ld, &pppserial_head, next) {

        // try to output data
        if (!(ld->state & STATE_TBUSY)
            && (ld->outq.head || ld->oobq.head || ld->outm)) {
            
            ld->state |= STATE_TBUSY;
            pppserial_ouput(ld);
            
            link = (struct ppp_link *)ld;
            if (link->lk_flags & SC_XMIT_FULL
                && !ppp_qfull(&ld->outq)) {
                
                ld->state |= STATE_LKBUSY;

                link->lk_flags &= ~SC_XMIT_FULL;
                ppp_link_event(link, PPP_LINK_EVT_XMIT_OK, 0);

                ld->state &= ~STATE_LKBUSY;
                
                if (ld->state & STATE_CLOSING) {
                    wakeup(&ld->state);
                    goto nextlink;
                }
            }
        }

        // try to input data
        for (;;) {
        
            m = ppp_dequeue(&ld->inq);
            if (m == NULL)
                break;

            ld->state |= STATE_LKBUSY;

            if (mbuf_flags(m) & M_ERRMARK) {
                ppp_link_event((struct ppp_link *)ld, PPP_LINK_EVT_INPUTERROR, 0);
				mbuf_setflags(m, mbuf_flags(m) & ~M_ERRMARK);
            }
            ppp_link_input(&ld->link, m);

            ld->state &= ~STATE_LKBUSY;
            
            if (ld->state & STATE_CLOSING) {
                wakeup(&ld->state);
                goto nextlink;
            }
        }
        
nextlink:
    ;
    }
}  
