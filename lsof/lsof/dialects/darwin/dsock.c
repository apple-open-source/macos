/*
 * dsock.c - Darwin socket processing functions for lsof
 */

/*
 * Special Darwin socket info: Justin Walker, 000927
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
static char *rcsid = "$Id: dsock.c,v 1.3 2001/11/01 20:28:14 abe Exp $";
#endif


#include "lsof.h"

/*
 * process_socket() - process socket
 */

void
process_socket(sa)
	KA_T sa;			/* socket address in kernel */
{
	struct domain d;
	unsigned char *fa = (unsigned char *)NULL;
	int fam;
	int fp, lp;
	struct inpcb inp;
	unsigned char *la = (unsigned char *)NULL;
	struct mbuf mb;
	struct protosw p;
	struct socket s;
	struct tcpcb t;
	struct unpcb uc, unp;
	struct sockaddr_un *ua = NULL;
	struct sockaddr_un un;

#if	defined(AF_SYSTEM)
	struct kern_event_pcb kev_cb;
#endif	/* defined(AF_SYSTEM) */

#if	defined(AF_NDRV)
	char buf[IFNAMSIZ];
	struct ndrv_cb ndrv_cb;
	struct ifnet ifnet;
#endif	/* defined(AF_NDRV) */

	(void) snpf(Lf->type, sizeof(Lf->type), "sock");
	Lf->inp_ty = 2;
/*
 * Read the socket, protocol, and domain structures.
 */
	if (!sa) {
	    enter_nm("no socket address");
	    return;
	}
	if (kread(sa, (char *) &s, sizeof(s))) {
	    (void) snpf(Namech, Namechl, "can't read socket struct from %s",
		print_kptr(sa, (char *)NULL, 0));
	    enter_nm(Namech);
	    return;
	}
	if (!s.so_type) {
	    enter_nm("no socket type");
	    return;
	}
	if (!s.so_proto
	||  kread((KA_T)s.so_proto, (char *)&p, sizeof(p))) {
	    (void) snpf(Namech, Namechl, "can't read protocol switch from %s",
		print_kptr((KA_T)s.so_proto, (char *)NULL, 0));
	    enter_nm(Namech);
	    return;
	}
	if (!p.pr_domain
	||  kread((KA_T)p.pr_domain, (char *)&d, sizeof(d))) {
	    (void) snpf(Namech, Namechl, "can't read domain struct from %s",
		print_kptr((KA_T)p.pr_domain, (char *)NULL, 0));
	    enter_nm(Namech);
	    return;
	}
/*
 * Save size information.
 */
	if (Fsize) {
	    if (Lf->access == 'r')
		Lf->sz = (SZOFFTYPE)s.so_rcv.sb_cc;
	    else if (Lf->access == 'w')
		Lf->sz = (SZOFFTYPE)s.so_snd.sb_cc;
	    else
		Lf->sz = (SZOFFTYPE)(s.so_rcv.sb_cc + s.so_snd.sb_cc);
	    Lf->sz_def = 1;
	} else
	    Lf->off_def = 1;

#if	defined(HASTCPTPIQ)
	Lf->lts.rq = s.so_rcv.sb_cc;
	Lf->lts.sq = s.so_snd.sb_cc;
	Lf->lts.rqs = Lf->lts.sqs = 1;
#endif	/* defined(HASTCPTPIQ) */

/*
 * Process socket by the associated domain family.
 */
	switch ((fam = d.dom_family)) {
/*
 * Process an Internet domain socket.
 */
	case AF_INET:
	    if (Fnet)
		Lf->sf |= SELNET;
	    printiproto(p.pr_protocol);
	    (void) snpf(Lf->type, sizeof(Lf->type), "inet");
	/*
	 * Read protocol control block.
	 */
	    if (!s.so_pcb
	    ||  kread((KA_T)s.so_pcb, (char *) &inp, sizeof(inp))) {
		if (!s.so_pcb) {
		    (void) snpf(Namech, Namechl, "no PCB%s%s",
			(s.so_state & SS_CANTSENDMORE) ? ", CANTSENDMORE"
						       : "",
			(s.so_state & SS_CANTRCVMORE) ? ", CANTRCVMORE"
						      : "");
		} else {
		    (void) snpf(Namech, Namechl, "can't read inpcb at %s",
			print_kptr((KA_T)s.so_pcb, (char *)NULL, 0));
		}
		enter_nm(Namech);
		return;
	    }
	/*
	 * Print Internet socket information.
	 */
	    enter_dev_ch(print_kptr((KA_T)(inp.inp_ppcb ? inp.inp_ppcb
							: s.so_pcb),
				    (char *)NULL, 0));
	    la = (unsigned char *)&inp.inp_laddr;
	    lp = (int)ntohs(inp.inp_lport);
	    if (inp.inp_faddr.s_addr != INADDR_ANY || inp.inp_fport) {
		fa = (unsigned char *)&inp.inp_faddr;
		fp = (int)ntohs(inp.inp_fport);
	    }
	    if (fa || la)
		(void) ent_inaddr(la, lp, fa, fp, fam, -1);
	    if (p.pr_protocol == IPPROTO_TCP && inp.inp_ppcb
	    &&  !kread((KA_T)inp.inp_ppcb, (char *)&t, sizeof(t))) {
		Lf->lts.type = 0;
		Lf->lts.state.i = (int)t.t_state;
	    }
	    break;

#if	defined(AF_NDRV)
/*
 * Process an NDRV domain socket.
 */
	case AF_NDRV:
	{
	    (void) snpf(Lf->type, sizeof(Lf->type), "ndrv");
	/*
	 * Read protocol control block.
	 */
	    if (!s.so_pcb
	    ||  kread((KA_T) s.so_pcb, (char *) &ndrv_cb, sizeof(ndrv_cb))) {
		(void) snpf(Namech, Namechl, "can't read ndrv_cb at %s",
		    print_kptr((KA_T)s.so_pcb, (char *)NULL, 0));
		enter_nm(Namech);
		return;
	    }
	/*
	 * Print NDRV socket information.
	 */
	    enter_dev_ch(print_kptr((KA_T)(s.so_pcb), (char *)NULL, 0));
	/*
	 * Print device name, if bound
	 */
	    if (!ndrv_cb.nd_if ||
		kread((KA_T) ndrv_cb.nd_if, (char *) &ifnet, sizeof(ifnet))){
		(void) snpf(Namech, Namechl, "can't read ifnet at %s",
			    print_kptr((KA_T)ndrv_cb.nd_if, (char *)NULL, 0));
		enter_nm(Namech);
		return;
	    }
	    if (!ifnet.if_name ||
		kread((KA_T) ifnet.if_name, buf, sizeof(buf))){
		(void) snpf(Namech, Namechl, "can't read ifnet.if_name at %s",
			    print_kptr((KA_T)ifnet.if_name, (char *)NULL, 0));
		enter_nm(Namech);
		return;
	    }
	    (void) snpf(Namech, Namechl, "-> %s%d", buf, ifnet.if_unit,
			(char *)NULL, 0);
	}
	break;
#endif	/* defined(AF_NDRV) */

#if	defined(AF_SYSTEM)
/*
 * Process a SYSTEM domain socket
 */
	case AF_SYSTEM:
	{
	    (void) snpf(Lf->type, sizeof(Lf->type), "systm");
	/*
	 * Read protocol control block.
	 */
	    if (!s.so_pcb
	    ||  kread((KA_T) s.so_pcb, (char *) &kev_cb, sizeof(kev_cb))) {
		(void) snpf(Namech, Namechl, "can't read kev_cb at %s",
		    print_kptr((KA_T)s.so_pcb, (char *)NULL, 0));
		enter_nm(Namech);
		return;
	    }
	/*
	 * Print SYSTEM socket information.
	 */
	    enter_dev_ch(print_kptr((KA_T)(s.so_pcb), (char *)NULL, 0));
	/*
	 * Determine SYSTEM protocol...
	 */
	    if ( kread((KA_T) s.so_pcb, (char *) &kev_cb, sizeof(kev_cb))) {
		(void) snpf(Namech, Namechl, "can't read kev_cb at %s",
		    print_kptr((KA_T)s.so_pcb, (char *)NULL, 0));
		enter_nm(Namech);
		return;
	    }
	    (void) snpf(Namech, Namechl, "[%x:%x:%x]",
			kev_cb.vendor_code_filter,
			kev_cb.class_filter, kev_cb.subclass_filter,
			(char *)NULL, 0);
	}
	break;
#endif	/* defined(AF_SYSTEM) */

/*
 * Process a ROUTE domain socket.
 */
	case AF_ROUTE:
	    (void) snpf(Lf->type, sizeof(Lf->type), "rte");
	    if (s.so_pcb)
		enter_dev_ch(print_kptr((KA_T)(s.so_pcb), (char *)NULL, 0));
	    else
		(void) snpf(Namech, Namechl, "no protocol control block");
	    if (!Fsize)
		Lf->off_def = 1;
	    break;
/*
 * Process a Unix domain socket.
 */
	case AF_UNIX:
	    if (Funix)
		Lf->sf |= SELUNX;
	    (void) snpf(Lf->type, sizeof(Lf->type), "unix");
	/*
	 * Read Unix protocol control block and the Unix address structure.
	 */

	    enter_dev_ch(print_kptr(sa, (char *)NULL, 0));
	    if (kread((KA_T) s.so_pcb, (char *) &unp, sizeof(unp))) {
		(void) snpf(Namech, Namechl, "can't read unpcb at %s",
		    print_kptr((KA_T)s.so_pcb, (char *)NULL, 0));
		break;
	    }
	    if ((struct socket *)sa != unp.unp_socket) {
		(void) snpf(Namech, Namechl, "unp_socket (%s) mismatch",
		    print_kptr((KA_T)unp.unp_socket, (char *)NULL, 0));
		break;
	    }
	    if (unp.unp_addr) {
		if (kread((KA_T) unp.unp_addr, (char *) &mb, sizeof(mb))) {
		    (void) snpf(Namech, Namechl, "can't read unp_addr at %s",
			print_kptr((KA_T)unp.unp_addr, (char *)NULL, 0));
		    break;
		}
		if (mb.m_hdr.mh_len == sizeof(struct sockaddr_un))
		    ua = (struct sockaddr_un *) ((char *) &mb
		       + (mb.m_hdr.mh_data - (caddr_t) unp.unp_addr));
	    }
	    if (!ua) {
		ua = &un;
		(void) bzero((char *)ua, sizeof(un));
		ua->sun_family = AF_UNSPEC;
	    }
	/*
	 * Print information on Unix socket that has no address bound
	 * to it, although it may be connected to another Unix domain
	 * socket as a pipe.
	 */
	    if (ua->sun_family != AF_UNIX) {
		if (ua->sun_family == AF_UNSPEC) {
		    if (unp.unp_conn) {
			if (kread((KA_T)unp.unp_conn, (char *)&uc, sizeof(uc)))
			    (void) snpf(Namech, Namechl,
				"can't read unp_conn at %s",
				print_kptr((KA_T)unp.unp_conn,(char *)NULL,0));
			else
			    (void) snpf(Namech, Namechl, "->%s",
				print_kptr((KA_T)uc.unp_socket,(char *)NULL,0));
		    } else
			(void) snpf(Namech, Namechl, "->(none)");
		} else
		    (void) snpf(Namech, Namechl, "unknown sun_family (%d)",
			ua->sun_family);
		break;
	    }
	    if (ua->sun_path[0]) {
		if (mb.m_len >= sizeof(struct sockaddr_un))
		    mb.m_len = sizeof(struct sockaddr_un) - 1;
		*((char *)ua + mb.m_len) = '\0';
		if (Sfile && is_file_named(ua->sun_path, 0))
		    Lf->sf |= SELNM;
		if (!Namech[0])
		    (void) snpf(Namech, Namechl, "%s", ua->sun_path);
	    } else
		(void) snpf(Namech, Namechl, "no address");
	    break;
	default:
	    printunkaf(fam, 1);
	}
	if (Namech[0])
	    enter_nm(Namech);
}
