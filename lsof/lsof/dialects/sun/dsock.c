/*
 * dsock.c - Solaris socket processing functions for lsof
 */


/*
 * Copyright 1994 Purdue Research Foundation, West Lafayette, Indiana
 * 47907.  All rights reserved.
 *
 * Written by Victor A. Abell
 *
 * This software is not subject to any license of the American Telephone
 * and Telegraph Company or the Regents of the University of California.
 *
 * Permission is granted to anyone to use this software for any purpose on
 * any computer system, and to alter it and redistribute it freely, subject
 * to the following restrictions:
 *
 * 1. Neither the authors nor Purdue University are responsible for any
 *    consequences of the use of this software.
 *
 * 2. The origin of this software must not be misrepresented, either by
 *    explicit claim or by omission.  Credit to the authors and Purdue
 *    University must appear in documentation and sources.
 *
 * 3. Altered versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 *
 * 4. This notice may not be removed or altered.
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright 1994 Purdue Research Foundation.\nAll rights reserved.\n";
static char *rcsid = "$Id: dsock.c,v 1.21 2001/11/01 20:28:49 abe Exp $";
#endif


#include "lsof.h"

#if	defined(HASIPv6)

/*
 * IPv6_2_IPv4()  -- macro to define the address of an IPv4 address contained
 *		     in an IPv6 address
 */

#define IPv6_2_IPv4(v6)	(((uint8_t *)((struct in6_addr *)v6)->s6_addr)+12)

/*
 * IPv_ADDR_UNSPEC() -- macro to test an IP[46] address for an unspecified
 *			address value
 */

#define IPv_ADDR_UNSPEC(af, p) \
    (((af) == AF_INET6) ? (IN6_IS_ADDR_UNSPECIFIED((struct in6_addr *)p)) \
			: (((struct in_addr *)(p))->s_addr == INADDR_ANY))
#else	/* !defined(HASIPv6) */

/*
 * IPv_ADDR_UNSPEC() -- IPv4-only form of macro to test for an unspecified
 *			address value
 */

#define	IPv_ADDR_UNSPEC(af, p) (((struct in_addr *)(p))->s_addr == INADDR_ANY)

#endif	/* !defined(HASIPv6) */


/*
 * print_tcptpi() - print TCP/TPI info
 */

void
print_tcptpi(nl)
	int nl;				/* 1 == '\n' required */
{
	char *cp = (char *)NULL;
	char  sbuf[128];
	int i;
	int ps = 0;
	unsigned int u;

	if (Ftcptpi & TCPTPI_STATE) {
	    switch (Lf->lts.type) {
	    case 0:				/* TCP */
		switch ((i = Lf->lts.state.i)) {
		case TCPS_CLOSED:
		    cp = "CLOSED";
		    break;
		case TCPS_IDLE:
		    cp = "IDLE";
		    break;
		case TCPS_BOUND:
		    cp = "BOUND";
		    break;
		case TCPS_LISTEN:
		    cp = "LISTEN";
		    break;
		case TCPS_SYN_SENT:
		    cp = "SYN_SENT";
		    break;
		case TCPS_SYN_RCVD:
		    cp = "SYN_RCVD";
		    break;
		case TCPS_ESTABLISHED:
		    cp = "ESTABLISHED";
		    break;
		case TCPS_CLOSE_WAIT:
		    cp = "CLOSE_WAIT";
		    break;
		case TCPS_FIN_WAIT_1:
		    cp = "FIN_WAIT_1";
		    break;
		case TCPS_CLOSING:
		    cp = "CLOSING";
		    break;
		case TCPS_LAST_ACK:
		    cp = "LAST_ACK";
		    break;
		case TCPS_FIN_WAIT_2:
		    cp = "FIN_WAIT_2";
		    break;
		case TCPS_TIME_WAIT:
		    cp = "TIME_WAIT";
		    break;
		default:
		    (void) snpf(sbuf, sizeof(sbuf), "UknownState_%d", i);
		    cp = sbuf;
		}
		break;
	    case 1:				/* TPI */
		switch ((u = Lf->lts.state.ui)) {
		case TS_UNBND:
		    cp = "Unbound";
		    break;
		case TS_WACK_BREQ:
		    cp = "Wait_BIND_REQ_Ack";
		    break;
		case TS_WACK_UREQ:
		    cp = "Wait_UNBIND_REQ_Ack";
		    break;
		case TS_IDLE:
		    cp = "Idle";
		    break;
		case TS_WACK_OPTREQ:
		    cp = "Wait_OPT_REQ_Ack";
		    break;
		case TS_WACK_CREQ:
		    cp = "Wait_CONN_REQ_Ack";
		    break;
		case TS_WCON_CREQ:
		    cp = "Wait_CONN_REQ_Confirm";
		    break;
		case TS_WRES_CIND:
		    cp = "Wait_CONN_IND_Response";
		    break;
		case TS_WACK_CRES:
		    cp = "Wait_CONN_RES_Ack";
		    break;
		case TS_DATA_XFER:
		    cp = "Wait_Data_Xfr";
		    break;
		case TS_WIND_ORDREL:
		    cp = "Wait_Read_Release";
		    break;
		case TS_WREQ_ORDREL:
		    cp = "Wait_Write_Release";
		    break;
		case TS_WACK_DREQ6:
		case TS_WACK_DREQ7:
		case TS_WACK_DREQ9:
		case TS_WACK_DREQ10:
		case TS_WACK_DREQ11:
		    cp = "Wait_DISCON_REQ_Ack";
		    break;
		default:
		    (void) snpf(sbuf, sizeof(sbuf), "UNKNOWN_TPI_STATE_%u", u);
		    cp = sbuf;
		}
	    }
	    if (Ffield)
		(void) printf("%cST=%s%c", LSOF_FID_TCPTPI, cp, Terminator);
	    else {
		putchar('(');
		(void) fputs(cp, stdout);
	    }
	    ps++;
	}

# if	defined(HASTCPTPIQ)
	if (Ftcptpi & TCPTPI_QUEUES) {
	    if (Lf->lts.rqs) {
		if (Ffield)
			putchar(LSOF_FID_TCPTPI);
		else {
		    if (ps)
			putchar(' ');
		    else
			putchar('(');
		}
		(void) printf("QR=%lu", Lf->lts.rq);
		if (Ffield)
		    putchar(Terminator);
		ps++;
	    }
	    if (Lf->lts.sqs) {
		if (Ffield)
			putchar(LSOF_FID_TCPTPI);
		else {
		    if (ps)
			putchar(' ');
		    else
			putchar('(');
		}
		(void) printf("QS=%lu", Lf->lts.sq);
		if (Ffield)
		    putchar(Terminator);
		ps++;
	    }
	}
# endif	/* defined(HASTCPTPIQ) */

# if	defined(HASTCPTPIW)
	if (Ftcptpi & TCPTPI_WINDOWS) {
	    if (Lf->lts.rws) {
		if (Ffield)
			putchar(LSOF_FID_TCPTPI);
		else {
		    if (ps)
			putchar(' ');
		    else
			putchar('(');
		}
		(void) printf("WR=%lu", Lf->lts.rw);
		if (Ffield)
		    putchar(Terminator);
		ps++;
	    }
	    if (Lf->lts.wws) {
		if (Ffield)
			putchar(LSOF_FID_TCPTPI);
		else {
		    if (ps)
			putchar(' ');
		    else
			putchar('(');
		}
		(void) printf("WW=%lu", Lf->lts.ww);
		if (Ffield)
		    putchar(Terminator);
		ps++;
	    }
	}
# endif	/* defined(HASTCPTPIW) */

	if (Ftcptpi && !Ffield && ps)
	    putchar(')');
	if (nl)
	    putchar('\n');
}


/*
 * process_socket() - process Solaris socket
 */

void
process_socket(sa, ty)
	KA_T sa;			/* stream's data address in kernel */
	char *ty;			/* socket type name */
{
	int af;
	char *cp;
	unsigned char *fa = (unsigned char *)NULL;
	int fp = 0;
	int i, lp;
	struct ipc_s ic;
	int ics = 0;
	unsigned char *la = (unsigned char *)NULL;
	struct module_info mi;
	KA_T ka;
	int oaf = -1;
	u_short p;
	KA_T pcb = (KA_T)NULL;
	struct queue q;
	struct qinit qi;
	KA_T qp;
	u_short *s;
	struct stdata sd;
	unsigned char *ta;
	char tbuf[32];

#if	solaris<20600
	struct tcp_s {			/* should come from kernel source
					 * file ../uts/common/inet/tcp.c */

# if	solaris>=20400
	    struct tcp_s *d1[8];
# endif	/* solaris>=20400 */

# if	defined(P101318) && P101318>=32
	    struct tcp_s *d1[6];
# endif	/* defined(P101318) && P101318>=32 */

	    int tcp_state;
	    queue_t *d3[2];
	    mblk_t *d4[2];
	    u_long d5;
	    mblk_t *d6;
	    u_long d7;
	    u_long tcp_snxt;	/* Senders next seq num */
	    u_long tcp_suna;	/* Sender unacknowledged */
	    u_long tcp_swnd;	/* Senders window (relative to suna) */
	    u_long d8[5];
	    int tcp_hdr_len;	/* combined TCP/IP header length */
	    tcph_t *tcp_tcph;	/* pointer to combined header */
	    int d9;
	    unsigned int d10;
	    int d11;
	    mblk_t *d12;
	    long d13;
	    mblk_t *d14;
	    u_long d15;

# if	solaris<20400 && (!defined(P101318) || P101318<32)
	    mblk_t *d16;
# endif	/* solaris<20400 && (!defined(P101318) || P101318<32) */

	    unsigned int d17;
	    u_long tcp_rnxt;	/* Seq we expect to recv next */
	    u_long tcp_rwnd;	/* Current receive window */
	    u_long d18;
	    long d19[2];
	    mblk_t *d20[4];
	    u_long d21[5];
	    long d22[3];

# if	solaris<20500
	    u_long d23[2];
	    u_long tcp_rack;	/* Seq # we have acked */
# else	/* solaris>=20500 */
	    u_long d23[3];
# endif	/* solaris<20500 */

# if	solaris<20400
	    u_long d24[28];
# else	/* solaris>=20400 */
#  if	solaris<20500
	    u_long d24[67];
#  else	/* solaris>=20500 */
#   if	solaris<20501
	    u_long d25[6];
#   else	/* solaris>=20501 */
	    u_long d25[8];
#   endif	/* solaris<20501 */
	    u_long tcp_rack;	/* Seq # we have acked */
#   if	solaris<20501
	    u_long d26[29];
#   else	/* solaris>=20501 */
	    u_long d26[33];
#   endif	/* solaris>=20501 */
#  endif	/* solaris<20500 */
# endif	/* solaris<20400 */

	    iph_t tcp_iph;
	} tc;
#else	/* solaris>=20600 */
	struct tcp_s tc;
#endif	/* solaris<20600 */

#if	solaris>=80000
	tcpb_t	tcb;
#endif	/* solaris>=80000 */

	int tcs = 0;
	tcph_t th;
	struct ud_s {			/* should come from kernel source
					 * file ../uts/common/inet/udp.c */
	    uint udp_state;		/* TPI state */
	    unsigned char d1[2];
	    unsigned char udp_port[2];	/* port bound to this stream */
	    unsigned char udp_src[4];	/* source address of this stream */
	} uc;
	int ucs = 0;

# if	defined(HASIPv6)
	if (strrchr(ty, '6')) {
	    (void) snpf(Lf->type, sizeof(Lf->type), "IPv6");
	    af = AF_INET6;
	} else {
	    (void) snpf(Lf->type, sizeof(Lf->type), "IPv4");
	    af = AF_INET;
	}
# else	/* !defined(HASIPv6) */
	(void) snpf(Lf->type, sizeof(Lf->type), "inet");
	af = AF_INET;
# endif	/* defined(HASIPv6) */
/*
 * Set network file selection status.
 */
	if (Fnet) {
	    if (!FnetTy
	    ||  ((FnetTy == 4) && (af == AF_INET))

# if	defined(HASIPv6)
	    ||  ((FnetTy == 6) && (af == AF_INET6))
# endif	/* defined(HASIPv6) */

	    )
		Lf->sf |= SELNET;
	}
	Lf->inp_ty = 2;
/*
 * Convert type to upper case protocol name.
 */
	if (ty) {
	    for (i = 0; (ty[i] != '\0') && (i < IPROTOL) && (i < 3); i++) {
		if (islower((unsigned char)ty[i]))
		    Lf->iproto[i] = toupper((unsigned char)ty[i]);
		else
		    Lf->iproto[i] = ty[i];
	    }
	} else
	    i = 0;
	Lf->iproto[i] = '\0';
/*
 * Read stream queue entries to obtain private IP, TCP, and UDP structures.
 */
	if (!sa || readstdata(sa, &sd))
	    qp = (KA_T)NULL;
	else
	    qp = (KA_T)sd.sd_wrq;
	for (i = 0; qp && i < 20; i++, qp = (KA_T)q.q_next) {
	    if (kread(qp, (char *)&q, sizeof(q)))
		break;
	    if ((ka = (KA_T)q.q_qinfo) == (KA_T)NULL
	    ||  kread(ka, (char *)&qi, sizeof(qi)))
		continue;
	    if ((ka = (KA_T)qi.qi_minfo) == (KA_T)NULL
	    ||  kread(ka, (char *)&mi, sizeof(mi))
	    ||  (ka = (KA_T)mi.mi_idname) == (KA_T)NULL)
		continue;
	    if (kread(ka, (char *)&tbuf, sizeof(tbuf) - 1))
		continue;
	    if ((pcb = (KA_T)q.q_ptr) == (KA_T)NULL)
		continue;
	    if (strncasecmp(tbuf, "IP",  2) == 0) {
		if (kread(pcb, (char *)&ic, sizeof(ic)) == 0)
		    ics = 1;
		continue;
	    }
	    if (strncasecmp(tbuf, "TCP", 3) == 0) {
		if (kread((KA_T)q.q_ptr, (char *)&tc, sizeof(tc)) == 0)

#if	solaris>=80000
		{
		    if (tc.tcp_base
		    &&  !kread((KA_T)tc.tcp_base, (char *)&tcb, sizeof(tcb)))
			tcs = 1;
		    tc.tcp_base = &tcb;		/* support for macroes */
		    tcb.tcpb_tcp = &tc;		/* support for macroes */
		}
#else	/* solaris<80000 */
		    tcs = 1;
#endif	/* solaris>=80000 */

		continue;
	    }
	    if (strncasecmp(tbuf, "UDP", 3) == 0) {
		if (kread((KA_T)q.q_ptr, (char *)&uc, sizeof(uc)) == 0)
		    ucs = 1;
		continue;
	    }
	}
	if (ics) {

	/*
	 * Print stream head's q_ptr address as protocol control block address.
	 */
	    if (pcb)
		enter_dev_ch(print_kptr(pcb, (char *)NULL, 0));
	    if (strncmp(Lf->iproto, "UDP", 3) == 0) {

	/*
	 * Save UDP address and TPI state.
	 */

#if	solaris<20600
		la = (unsigned char *)&ic.ipc_udp_addr;
		p = (u_short)ic.ipc_udp_port;
#else	/* solaris>=20600 */
# if	defined(HASIPv6)
		la = (af == AF_INET6) ? (unsigned char *)&ic.ipc_v6laddr
		   :  (unsigned char *)IPv6_2_IPv4(&ic.ipc_v6laddr);
# else	/* !defined(HASIPv6 */
		la = (unsigned char *)&ic.ipc_laddr;
# endif	/* defined(HASIPv6) */

		p = (u_short)ic.ipc_lport;
#endif	/* solaris<20600 */

		if (IPv_ADDR_UNSPEC(af, la) && !p && ucs) {

		/*
		 * If the ipc_s structure has no local address, use
		 * the port in the ud_s structure.
		 */
		    s = (u_short *)&uc.udp_port[0];
		    p = *s;
		}

# if	defined(HASIPv6)
		if ((af == AF_INET6) && la
		&&  IN6_IS_ADDR_V4MAPPED((struct in6_addr *)la)) {

		/*
		 * Convert a local IPv4 address in an IPv6 structure to an IPv4
		 * address in an IPv4 structure.  Change the address family to
		 * AF_INET.
		 */
		    la = (unsigned char *)IPv6_2_IPv4(la);
		    oaf = af;
		    af = AF_INET;
		}
# endif	/* defined(HASIPv6) */

		(void) ent_inaddr(la, (int)ntohs(p), (unsigned char *)NULL,
				  -1, af, oaf);
		if (!Fsize)
		    Lf->off_def = 1;
		if (ucs) {
		    Lf->lts.type = 1;
		    Lf->lts.state.ui = (unsigned int)uc.udp_state;
		}
	    } else if (strncmp(Lf->iproto, "TCP", 3) == 0) {

	    /*
	     * Save TCP address.
	     */

#if	solaris<20400
		la = (unsigned char *)&ic.ipc_tcp_addr[0];
		p = (u_short)ic.ipc_tcp_addr[5];
#else	/* solaris>=20400 */
# if	solaris<20600
		la = (unsigned char *)&ic.ipc_tcp_laddr;
		p = (u_short)((short *)&ic.ipc_tcp_ports)[1];
# else	/* solaris>=20600 */
#  if	defined(HASIPv6)
		la = (af == AF_INET6) ? (unsigned char *)&ic.ipc_v6laddr
		   :  (unsigned char *)IPv6_2_IPv4(&ic.ipc_v6laddr);
#  else		/* !defined(HASIPv6 */
		la = (unsigned char *)&ic.ipc_laddr;
#  endif	/* defined(HASIPv6) */

		p = (u_short)ic.ipc_lport;
# endif	/* solaris<20600 */
#endif	/* solaris<20400 */

		if (IPv_ADDR_UNSPEC(af, la) && !p && tcs) {

		/*
		 * If the ipc_s structure has no local address, use the local
		 * address in the stream's tcp_iph structure (except for
		 * Solaris 2.4), and the port number in the stream's tcph
		 * structure.
		 */

#if	solaris!=20400 && solaris<80000
		    la = (unsigned char *)&tc.tcp_iph.iph_src[0];
#else	/* solaris==20400 || solaris<80000 */
# if	solaris>=80000
#  if	defined(HASIPv6)
		    la = (af == AF_INET6) ? (unsigned char *)&tcb.tcpb_ip_src_v6
		       :  (unsigned char *)IPv6_2_IPv4(&tcb.tcpb_ip_src_v6);
#  else	/* !defined(HASIPv6) */
		    la = (unsigned char *)&tcb.tcpb_ip_src;
#  endif	/* defined(HASIPv6) */
# endif	/* solaris>=80000 */
#endif	/* solaris!=20400 && !defined(HASIPv6) */

		    if (tc.tcp_hdr_len && tc.tcp_tcph
		    &&  !kread((KA_T)tc.tcp_tcph, (char *)&th, sizeof(th))) {
			s = (u_short *)&th.th_lport[0];
			p = *s;
		    }
		}
		lp = (int)ntohs(p);

#if	solaris<20400
		if ((int)ic.ipc_tcp_addr[2] != INADDR_ANY
		||  ic.ipc_tcp_addr[4] != 0)
		{
		    fa = (unsigned char *)&ic.ipc_tcp_addr[2];
		    fp = (int)ntohs(ic.ipc_tcp_addr[4]);
		}
#else	/* solaris>=20400 */
# if	solaris<20600
		if ((int)ic.ipc_tcp_faddr != INADDR_ANY
		||  ((u_short *) &ic.ipc_tcp_ports)[0] != 0)
		{
		    fa = (unsigned char *)&ic.ipc_tcp_faddr;
		    fp = (int)ntohs(((u_short *)&ic.ipc_tcp_ports)[0]);
		}
# else	/* solaris>=20600 */

#  if	defined(HASIPv6)
		ta = (af == AF_INET6) ? (unsigned char *)&ic.ipc_v6faddr
		   :  (unsigned char *)IPv6_2_IPv4(&ic.ipc_v6faddr);
#  else	/* !defined(HASIPv6) */
		ta = (unsigned char *)&ic.ipc_faddr;
#  endif	/* defined(HASIPv6) */

		if (!IPv_ADDR_UNSPEC(af, ta) || ((u_short)ic.ipc_fport)) {
		    fa = ta;
		    fp = (int)ntohs(((u_short)ic.ipc_fport));
		}
# endif	/* solaris<20600 */
#endif	/* solaris <20400 */

#if	defined(HASIPv6)
		if ((af == AF_INET6)
		&&  ((la && IN6_IS_ADDR_V4MAPPED((struct in6_addr *)la))
		||  ((fa && IN6_IS_ADDR_V4MAPPED((struct in6_addr *)fa))))) {

		/*
		 * Convert IPv4 addresses in IPv6 structures to IPv4 addresses
		 * in IPv4 structures.  Change the address family to AF_INET.
		 */
		    if (la)
			la = (unsigned char *)IPv6_2_IPv4(la);
		    if (fa)
			fa = (unsigned char *)IPv6_2_IPv4(fa);
		    oaf = af;
		    af = AF_INET;
		}
#endif	/* defined(HASIPv6) */

		if (fa || la)
		    (void) ent_inaddr(la, lp, fa, fp, af, oaf);
	    /*
	     * Save TCP state information.
	     */
		if (tcs) {
		    Lf->lts.type = 0;
		    Lf->lts.state.i = (int)tc.tcp_state;
		}
	    /*
	     * Save TCP size information.
	     */

#if	defined(HASTCPTPIQ) || defined(HASTCPTPIW)
		if (tcs) {

		    int rq, sq;

# if	defined(HASTCPTPIW)
		    Lf->lts.rw = (int)tc.tcp_rwnd;
		    Lf->lts.ww = (int)tc.tcp_swnd;
		    Lf->lts.rws = Lf->lts.wws = 1;
# endif	/* defined(HASTCPTPIW) */

		    if ((rq = (int)tc.tcp_rnxt - (int)tc.tcp_rack) < 0)
			rq = 0;
		    if ((sq = (int)tc.tcp_snxt - (int)tc.tcp_suna - 1) < 0)
			sq  = 0;

# if	defined(HASTCPTPIQ)
		    Lf->lts.rq = (unsigned long)rq;
		    Lf->lts.sq = (unsigned long)sq;
		    Lf->lts.rqs = Lf->lts.sqs = 1;
# endif	/* defined(HASTCPTPIQ) */

		    if (Fsize) {
			if (Lf->access == 'r')
			    Lf->sz = (SZOFFTYPE)rq;
			else if (Lf->access == 'w')
			    Lf->sz = (SZOFFTYPE)sq;
			else
			    Lf->sz = (SZOFFTYPE)(rq + sq);
			Lf->sz_def = 1;
		    } else
			Lf->off_def = 1;
		}
#else	/* !defined(HASTCPTPIQ) && !defined(HASTCPTPIW) */
		Lf->off_def = 1;
#endif	/* defined(HASTCPTPIQ) || defined(HASTCPTPIW) */

	    } else {
		if (!Fsize)
		    Lf->off_def = 1;
	    }
	} else
	    (void) strcat(Namech, "no TCP/UDP/IP information available");
/*
 * Enter name characters if there are some.
 */
	if (Namech[0])
	    enter_nm(Namech);
}
