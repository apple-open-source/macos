/*
 * dsock.c -- pstat-based HP-UX socket and stream processing functions for lsof
 */


/*
 * Copyright 1999 Purdue Research Foundation, West Lafayette, Indiana
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
"@(#) Copyright 1999 Purdue Research Foundation.\nAll rights reserved.\n";
static char *rcsid = "$Id";
#endif


#include "lsof.h"


/*
 * Local function prototypes
 */

_PROTOTYPE(static void printpsproto,(uint32_t p));


/*
 * printpsproto() -- print PSTAT protocol name
 */

static void
printpsproto(p)
	uint32_t p;			/* protocol number */
{
	int i;
	static int m = -1;
	char *s;

	switch (p) {
	case PS_PROTO_IP:
	    s = "IP";
	    break;
	case PS_PROTO_ICMP:
	    s = "ICMP";
	    break;
	case PS_PROTO_IGMP:
	    s = "IGMP";
	    break;
	case PS_PROTO_GGP:
	    s = "GGP";
	    break;
	case PS_PROTO_IPIP:
	    s = "IPIP";
	    break;
	case PS_PROTO_TCP:
	    s = "TCP";
	    break;
	case PS_PROTO_EGP:
	    s = "EGP";
	    break;
	case PS_PROTO_IGP:
	    s = "IGP";
	    break;
	case PS_PROTO_PUP:
	    s = "PUP";
	    break;
	case PS_PROTO_UDP:
	    s = "UDP";
	    break;
	case PS_PROTO_IDP:
	    s = "IDP";
	    break;
	case PS_PROTO_XTP:
	    s = "XTP";
	    break;
	case PS_PROTO_ESP:
	    s = "ESP";
	    break;
	case PS_PROTO_AH:
	    s = "AH";
	    break;
	case PS_PROTO_OSPF:
	    s = "OSPF";
	    break;
	case PS_PROTO_IPENCAP:
	    s = "IPENCAP";
	    break;
	case PS_PROTO_ENCAP:
	    s = "ENCAP";
	    break;
	case PS_PROTO_PXP:
	    s = "PXP";
	    break;
	case PS_PROTO_RAW:
	    s = "RAW";
	    break;
	default:
	    s = (char *)NULL;
	}
	if (s)
	    (void) snpf(Lf->iproto, sizeof(Lf->iproto), "%.*s", IPROTOL-1, s);
	else {
	    if (m < 0) {
		for (i = 0, m = 1; i < IPROTOL-2; i++)
		    m *= 10;
	    }
	    if (m > p)
		(void) snpf(Lf->iproto, sizeof(Lf->iproto), "%d?", p);
	    else
		(void) snpf(Lf->iproto, sizeof(Lf->iproto), "*%d?",
		    p % (m/10));
	}
}


/*
 * print_tcptpi() -- print TCP/TPI info
 */

void
print_tcptpi(nl)
	int nl;				/* 1 == '\n' required */
{
	char *cp = (char *)NULL;
	char  sbuf[128];
	int i, t;
	int ps = 0;
	unsigned int u;

	if (Ftcptpi & TCPTPI_STATE) {
	    switch ((t = Lf->lts.type)) {
	    case 0:				/* TCP */
		switch ((i = Lf->lts.state.i)) {
		case PS_TCPS_CLOSED:
		    cp = "CLOSED";
		    break;
		case PS_TCPS_IDLE:
		    cp = "IDLE";
		    break;
		case PS_TCPS_BOUND:
		    cp = "BOUND";
		    break;
		case PS_TCPS_LISTEN:
		    cp = "LISTEN";
		    break;
		case PS_TCPS_SYN_SENT:
		    cp = "SYN_SENT";
		    break;
		case PS_TCPS_SYN_RCVD:
		    cp = "SYN_RCVD";
		    break;
		case PS_TCPS_ESTABLISHED:
		    cp = "ESTABLISHED";
		    break;
		case PS_TCPS_CLOSE_WAIT:
		    cp = "CLOSE_WAIT";
		    break;
		case PS_TCPS_FIN_WAIT_1:
		    cp = "FIN_WAIT_1";
		    break;
		case PS_TCPS_CLOSING:
		    cp = "CLOSING";
		    break;
		case PS_TCPS_LAST_ACK:
		    cp = "LAST_ACK";
		    break;
		case PS_TCPS_FIN_WAIT_2:
		    cp = "FIN_WAIT_2";
		    break;
		case PS_TCPS_TIME_WAIT:
		    cp = "TIME_WAIT";
		    break;
		default:
		    (void) snpf(sbuf, sizeof(sbuf), "UknownState_%d", i);
		    cp = sbuf;
		}
		break;
	    case 1:				/* TPI */
		switch ((u = Lf->lts.state.ui)) {
		case PS_TS_UNINIT:
		    cp = "Uninitialized";
		    break;
		case PS_TS_UNBND:
		    cp = "Unbound";
		    break;
		case PS_TS_WACK_BREQ:
		    cp = "Wait_BIND_REQ_Ack";
		    break;
		case PS_TS_WACK_UREQ:
		    cp = "Wait_UNBIND_REQ_Ack";
		    break;
		case PS_TS_IDLE:
		    cp = "Idle";
		    break;
		case PS_TS_WACK_OPTREQ:
		    cp = "Wait_OPT_REQ_Ack";
		    break;
		case PS_TS_WACK_CREQ:
		    cp = "Wait_CONN_REQ_Ack";
		    break;
		case PS_TS_WCON_CREQ:
		    cp = "Wait_CONN_REQ_Confirm";
		    break;
		case PS_TS_WRES_CIND:
		    cp = "Wait_CONN_IND_Response";
		    break;
		case PS_TS_WACK_CRES:
		    cp = "Wait_CONN_RES_Ack";
		    break;
		case PS_TS_DATA_XFER:
		    cp = "Wait_Data_Xfr";
		    break;
		case PS_TS_WIND_ORDREL:
		    cp = "Wait_Read_Release";
		    break;
		case PS_TS_WREQ_ORDREL:
		    cp = "Wait_Write_Release";
		    break;
		case PS_TS_WACK_DREQ6:
		case PS_TS_WACK_DREQ7:
		case PS_TS_WACK_DREQ9:
		case PS_TS_WACK_DREQ10:
		case PS_TS_WACK_DREQ11:
		    cp = "Wait_DISCON_REQ_Ack";
		    break;
		case PS_TS_WACK_ORDREL:
		    cp = "Internal";
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
 * process_socket() -- process socket
 */

void
process_socket(f, s)
	struct pst_fileinfo2 *f;		/* file information */
	struct pst_socket *s;			/* socket information */
{
	int af, err, fp, lp;
	char buf[1024], tbuf[32];
	unsigned char *fa = (unsigned char *)NULL;
	unsigned char *la = (unsigned char *)NULL;
	size_t len;
	KA_T na, nau;
	char *nma = (char *)NULL;
	struct pst_filedetails pd;
	struct sockaddr_in *sa;

#if	defined(HASIPv6)
	int oaf;
	struct sockaddr_in6 *sa6;
#endif	/* defined(HASIPv6) */

	struct sockaddr_un *ua;

	(void) snpf(Lf->type, sizeof(Lf->type), "sock");
	Lf->inp_ty = 2;
/*
 * Generate and save node ID.
 */
	na = (KA_T)(((KA_T)(f->psf_hi_nodeid & 0xffffffff) << 32)
	   |        (KA_T)(f->psf_lo_nodeid & 0xffffffff));

#if	defined(HASFSTRUCT)
	if (na && (Fsv & FSV_NI)) {
	    if (na) {
		Lf->fna = na;
		Lf->fsv |= FSV_NI;
	    }
	}
#endif	/* defined(HASFSTRUCT) */

/*
 * Read socket info, as required.
 */
	if (!s) {
	    if (!(s = read_sock(f))) {
		(void) snpf(Namech, Namechl,
		    "can't read pst_socket%s%s", errno ? ": " : "",
		    errno ? strerror(errno) : "");
		(void) enter_nm(Namech);
		return;
	    }
	}
/*
 * Save size information, as requested.
 */
	if (Fsize) {
	    if (Lf->access == 'r')
		Lf->sz = (SZOFFTYPE)s->pst_idata;
	    else if (Lf->access == 'w')
		Lf->sz = (SZOFFTYPE)s->pst_odata;
	    else
		Lf->sz = (SZOFFTYPE)(s->pst_idata + s->pst_odata);
	    Lf->sz_def = 1;
	} else
	    Lf->off_def = 1;
	
#if     defined(HASTCPTPIQ)
/*
 * Enter queue sizes.
 */
	switch (s->pst_family) {
	case PS_AF_INET:
	case PS_AF_INET6:
	    Lf->lts.rq = (unsigned long)s->pst_idata;
	    Lf->lts.sq = (unsigned long)s->pst_odata;
	    Lf->lts.rqs = Lf->lts.sqs = 1;
	}
#endif  /* defined(HASTCPTPIQ) */

#if	defined(HASTCPTPIW)
/*
 * Enter window sizes.
 */
	switch (s->pst_family) {
	case PS_AF_INET:
	case PS_AF_INET6:
	    Lf->lts.rw = (unsigned long)s->pst_rwnd;
	    Lf->lts.ww = (unsigned long)s->pst_swnd;
	    Lf->lts.rws = Lf->lts.wws = 1;
	}
#endif	/* defined(HASTCPTPIW) */

/*
 * Process socket by the associated domain family.
 */
	switch (s->pst_family) {
	case PS_AF_INET:
	    if (Fnet && (FnetTy != 6))
		Lf->sf |= SELNET;
	    (void) snpf(Lf->type, sizeof(Lf->type),
	    
#if	defined(HASIPv6)
		"IPv4"
#else	/* !defined(HASIPv6) */
		"inet"
#endif	/* defined(HASIPv6) */

	    );
	    printpsproto(s->pst_protocol);
	    enter_dev_ch(print_kptr(na, (char *)NULL, 0));
	    switch (s->pst_protocol) {
	    case PS_PROTO_TCP:
		Lf->lts.type = 0;
		Lf->lts.state.i = (int)s->pst_pstate;
		break;
	    case PS_PROTO_UDP:
		Lf->lts.type = 1;
		Lf->lts.state.i = (unsigned int)s->pst_pstate;
	    }
	/*
	 * Enter local and remote addresses, being careful to generate
	 * proper IPv4 address alignment by copying, since IPv4 addresses
	 * may not be properly aligned in pst_boundaddr[] and pst_remaddr[].
	 */
	    if ((size_t)s->pst_boundaddr_len == sizeof(struct sockaddr_in)) {
		sa = (struct sockaddr_in *)s->pst_boundaddr;
		la = (unsigned char *)&sa->sin_addr;
		lp = (int)htons(sa->sin_port);
	    }
	    if ((size_t)s->pst_remaddr_len == sizeof(struct sockaddr_in)) {
		sa = (struct sockaddr_in *)s->pst_remaddr;
		fp = (int)htons(sa->sin_port);
		if ((sa->sin_addr.s_addr != INADDR_ANY) || fp)
		    fa = (unsigned char *)&sa->sin_addr;
	    }
	    if (fa || la)
		(void) ent_inaddr(la, lp, fa, fp, AF_INET, -1);
	    break;

#if	defined(HASIPv6)
	case PS_AF_INET6:
	    af = AF_INET6;
	    if (Fnet && (FnetTy != 4))
		Lf->sf |= SELNET;
	    (void) snpf(Lf->type, sizeof(Lf->type), "IPv6");
	    printpsproto(s->pst_protocol);
	    enter_dev_ch(print_kptr(na, (char *)NULL, 0));
	    switch (s->pst_protocol) {
	    case PS_PROTO_TCP:
		Lf->lts.type = 0;
		Lf->lts.state.i = (int)s->pst_pstate;
		break;
	    case PS_PROTO_UDP:
		Lf->lts.type = 1;
		Lf->lts.state.ui = (unsigned int)s->pst_pstate;
	    }
	/*
	 * Enter local and remote addresses, being careful to generate
	 * proper IPv6 address alignment by copying, since IPv6 addresses
	 * may not be properly aligned in pst_boundaddr[] and pst_remaddr[].
	 */
	    if ((size_t)s->pst_boundaddr_len == sizeof(struct sockaddr_in6)) {
		sa6 = (struct sockaddr_in6 *)s->pst_boundaddr;
		la = (unsigned char *)&sa6->sin6_addr;
		lp = (int)htons(sa6->sin6_port);
	    }
	    if ((size_t)s->pst_remaddr_len == sizeof(struct sockaddr_in6)) {
		sa6 = (struct sockaddr_in6 *)s->pst_remaddr;
		if ((fp = (int)htons(sa6->sin6_port))
		||  !IN6_IS_ADDR_UNSPECIFIED(sa6->sin6_addr))
		    fa = (unsigned char *)&sa6->sin6_addr;
	    }
	    if (la || fa) {
		if ((la && IN6_IS_ADDR_V4MAPPED(*((struct in6_addr *)la)))
		||  (fa && IN6_IS_ADDR_V4MAPPED(*((struct in6_addr *)fa))))
		{
		    if (la)
			la = (unsigned char *)IPv6_2_IPv4(la);
		    if (fa)
			fa = (unsigned char *)IPv6_2_IPv4(fa);
		    oaf = af;
		    af = AF_INET;
		} else
		    oaf = -1;
	    }
	    if (fa || la)
		(void) ent_inaddr(la, lp, fa, fp, af, oaf);
	    break;
#endif	/* defined(HASIPv6) */

	case PS_AF_UNIX:
	    if (Funix)
		Lf->sf |= SELUNX;
	    (void) snpf(Lf->type, sizeof(Lf->type), "unix");
	    if (((len = (size_t)s->pst_boundaddr_len) > 0)
	    &&  (len <= sizeof(struct sockaddr_un)))
	    {
		ua = (struct sockaddr_un *)s->pst_boundaddr;
		if (ua->sun_path[0]) {

		/*
		 * The AF_UNIX socket has a bound address (file path).
		 *
		 * Save it.  If there is a low nodeid, put that in
		 * parentheses after the name.  If there is a low peer
		 * nodeid, put that in the parentheses, too.
		 */
		    s->pst_boundaddr[PS_ADDR_SZ - 1] = '\0';
		    if (s->pst_lo_nodeid) {
			(void) snpf(buf, sizeof(buf), "(%s%s%s)",
			    print_kptr((KA_T)s->pst_lo_nodeid,
				       tbuf, sizeof(tbuf)),
			    s->pst_peer_lo_nodeid ? "->" : "",
			    s->pst_peer_lo_nodeid ?
			    		print_kptr((KA_T)s->pst_peer_lo_nodeid,
						   (char *)NULL, 0)
					: ""
			);
			len = strlen(buf) + 1;
			if (!(nma = (char *)malloc((MALLOC_S)len))) {
			    (void) fprintf(stderr,
				"%s: no unix nma space(1): PID %ld, FD %s",
				Pn, (long)Lp->pid, Lf->fd);
			}
			(void) snpf(nma, len, "%s", buf);
			Lf->nma = nma;
		    }
		/*
		 * Read the pst_filedetails for the bound address and process
		 * them as for a regular file.  The already-entered file type,
		 * file name, size or offset, and name appendix will be
		 * preserved.
		 */
		    if ((nau = read_det(&f->psf_fid, f->psf_hi_fileid,
					f->psf_lo_fileid, f->psf_hi_nodeid,
					f->psf_lo_nodeid, &pd)))
		    {
			enter_nm(ua->sun_path);
			(void) process_finfo(&pd, &f->psf_fid, &f->psf_id, nau);
			return;
		    } else {

		    /*
		     * Couldn't read file details.  Erase any name appendix.
		     * Put the socket nodeid in the DEVICE column, put the
		     * bound address (path) in the NAME column, and build
		     * a new name appendix with the peer address.  Add an
		     * error message if pstat_getfiledetails() set errno to
		     * something other than ENOENT.
		     */
			if ((err = errno) == ENOENT)
			    err = 0;
			if (nma) {
			    (void) free((MALLOC_P *)nma);
			    Lf->nma = (char *)NULL;
			}
			if (s->pst_lo_nodeid) {
	    		    enter_dev_ch(print_kptr((KA_T)s->pst_lo_nodeid,
					 (char *)NULL, 0));
			}
			(void) snpf(Namech, Namechl, "%s", ua->sun_path);
			if (err || s->pst_peer_lo_nodeid) {
			    (void) snpf(buf, sizeof(buf),
				"%s%s%s%s%s%s%s",
				err ? "(Error: " : "",
				err ? strerror(err) : "",
				err ? ")" : "",
				(err && s->pst_peer_lo_nodeid) ? " " : "",
				s->pst_peer_lo_nodeid ? "(->" : "",
				s->pst_peer_lo_nodeid ?
					print_kptr((KA_T)s->pst_peer_lo_nodeid,
						   (char *)NULL, 0)
				:	"",
				s->pst_peer_lo_nodeid ? ")" : ""
			    );
			    len = strlen(buf) + 1;
			    if (!(nma = (char *)malloc((MALLOC_S)len))) {
				(void) fprintf(stderr,
				    "%s: no unix nma space(2): PID %ld, FD %s",
				    Pn, (long)Lp->pid, Lf->fd);
			    }
			    (void) snpf(nma, len, "%s", buf);
			    Lf->nma = nma;
			}
			if (Sfile && is_file_named(ua->sun_path, 0))
			    Lf->sf |= SELNM;
			break;
		    }
		}
	    }
	/*
	 * If the UNIX socket has no bound address (file path), display the
	 * low nodeid in the DEVICE column and the peer's low nodeid in the
	 * NAME column.
	 */
	    if (s->pst_peer_lo_nodeid) {
		(void) snpf(Namech, Namechl, "->%s",
		    print_kptr((KA_T)s->pst_peer_lo_nodeid, (char *)NULL, 0));
	    }
	    if (s->pst_lo_nodeid)
		enter_dev_ch(print_kptr((KA_T)s->pst_lo_nodeid,(char *)NULL,0));
	    break;
	default:
	    (void) snpf(Namech, Namechl, "unsupported family: AF_%d",
		s->pst_family);
	}
	if (Namech[0])
	    enter_nm(Namech);
}


/*
 * process_stream() -- process stream
 */

void
process_stream(f)
	struct pst_fileinfo2 *f;		/* pst_fileinfo2 */
{
	struct clone *cl;
	char *cp;
	struct l_dev *dp = (struct l_dev *)NULL;
	int i, ncx, nsn, nsr;
	mode_t m;
	size_t nb, nl;
	KA_T na;
	static int nsa = 0;
	SZOFFTYPE rb, wb;
	dev_t rdev;
	static struct pst_stream *s = (struct pst_stream *)NULL;
	static size_t sz = sizeof(struct pst_stream);
/*
 * Generate and save node ID.
 */
	na = (KA_T)(((KA_T)(f->psf_hi_nodeid & 0xffffffff) << 32)
	   |		(KA_T)(f->psf_lo_nodeid & 0xffffffff));

#if	defined(HASFSTRUCT)
	if (na && (Fsv & FSV_NI)) {
	    Lf->fna = na;
	    Lf->fsv |= FSV_NI;
	}
#endif	/* defined(HASFSTRUCT) */

/*
 * Enter type.
 */
	switch (f->psf_ftype) {
	case PS_TYPE_STREAMS:
	    cp = "STR";
	    break;
	case PS_TYPE_SOCKET:
	    if (f->psf_subtype == PS_SUBTYPE_SOCKSTR) {
		cp = "STSO";
		break;
	    }
	    /* fall through */
	default:
	    cp = "unkn";
	}
	(void) snpf(Lf->type, sizeof(Lf->type), "%s", cp);
/*
 * Allocate sufficient space for stream structures, then read them.
 */
	if ((nsn = f->psf_nstrentt) && (nsn >= nsa)) {
	    nb = (size_t)(nsn * sizeof(struct pst_stream));
	    if (s)
		s = (struct pst_stream *)realloc((MALLOC_P *)s, nb);
	    else
		s = (struct pst_stream *)malloc(nb);
	    if (!s) {
		(void) fprintf(stderr,
		    "%s: no space for %ld pst_stream bytes\n", Pn, (long)nb);
		Exit(1);
	    }
	    nsa = nsn;
	}
	errno = 0;
	if ((nsr = pstat_getstream(s, sz, (size_t)nsn, 0, &f->psf_fid)) < 1) {
	    if (nsn) {
		(void) snpf(Namech, Namechl,
		    "can't read %d stream structures%s%s", nsn,
		    errno ? ": " : "", errno ? strerror(errno) : "");
		enter_nm(Namech);
	    } else
		enter_nm("no stream structures present");
	    return;
	}
/*
 * Make sure the stream head's fileid and nodeid match the ones in the
 * pst_fileino2 structure.  Enter size from stream head's structure,
 * if requested.
 */
	if (f->psf_hi_fileid != s[0].val.head.pst_hi_fileid
	 |  f->psf_lo_fileid != s[0].val.head.pst_lo_fileid
	 |  f->psf_hi_nodeid != s[0].val.head.pst_hi_nodeid
	 |  f->psf_lo_nodeid != s[0].val.head.pst_lo_nodeid) {
	    enter_nm("no matching stream data available");
	    return;
	}
	if (Fsize) {
	    if (Lf->access = 'r') {
		Lf->sz = (SZOFFTYPE)s[0].val.head.pst_rbytes;
		Lf->sz_def = 1;
	    } else if (Lf->access = 'w') {
		Lf->sz = (SZOFFTYPE)s[0].val.head.pst_wbytes;
		Lf->sz_def = 1;
	    } else if (Lf->access = 'u') {
		Lf->sz = (SZOFFTYPE)s[0].val.head.pst_rbytes
		       + (SZOFFTYPE)s[0].val.head.pst_wbytes;
		Lf->sz_def = 1;
	    }
	}
/*
 * Get the the device number from the stream head.
 *
 * If the stream is a clone:
 *
 *	if there's a clone list, search it for the device, based on the stream
 *	    head's minor device number only;
 *	if there's no clone list, search Devtp[], using a device number made
 *	    from the stream head's major and minor device numbers;
 *	set the printable clone device number to one whose major device number
 *	    is the stream head's minor device number, and whose minor device
 *	    number is the stream head's device sequence number.
 *
 * If the stream isn't a clone, make the device number from the stream head's
 * major and minor numbers, and look up the non-clone device number in Devtp[].
 */
	if (!Sdev)
	    readdev(0);
	if (s[0].val.head.pst_flag & PS_STR_ISACLONE) {
	    if (HaveCloneMaj && (CloneMaj == s[0].val.head.pst_dev_major)) {
		for (cl = Clone; cl; cl = cl->next) {
		    if (GET_MIN_DEV(Devtp[cl->dx].rdev)
		    ==  s[0].val.head.pst_dev_minor)
		    {
			dp = &Devtp[cl->dx];
			break;
		    }
		}
	    } else {
		rdev = makedev(s[0].val.head.pst_dev_major,
			       s[0].val.head.pst_dev_minor);
		dp = lkupdev(&DevDev, &rdev, 0, 1);
	    }
	    rdev = makedev(s[0].val.head.pst_dev_minor,
			   s[0].val.head.pst_dev_seq);
	} else {
	    rdev = makedev(s[0].val.head.pst_dev_major,
			   s[0].val.head.pst_dev_minor);
	    dp = lkupdev(&DevDev, &rdev, 0, 1);
	}
	Lf->dev = DevDev;
	Lf->rdev = rdev;
	Lf->dev_def = Lf->rdev_def = 1;
/*
 * If the device was located, enter the device name and save the node number.
 *
 * If the device wasn't located, save a positive file ID number from the
 * pst_fileinfo as a node number.
 */
	if (dp) {
	    (void) snpf(Namech, Namechl, "%s", dp->name);
	    ncx = strlen(Namech);
	    Lf->inode = (unsigned long)dp->inode;
	    Lf->inp_ty = 1;
	} else {
	    ncx = (size_t)0;
	    if (f->psf_id.psf_fileid > 0) {
		Lf->inode = (unsigned long)f->psf_id.psf_fileid;
		Lf->inp_ty = 1;
	    }
	}
/*
 * Enter stream module names.
 */
	for (i = 1; i < nsr; i++) {
	    if (!(nl = strlen(s[i].val.module.pst_name)))
		continue;
	    if (ncx) {
		if ((ncx + 2) > (Namechl - 1))
		    break;
		(void) snpf(&Namech[ncx], Namechl - ncx, "->");
		ncx += 2;
	    }
	    if ((ncx + nl) > (Namechl - 1))
		break;
	    (void) snpf(Namech+ncx,Namechl-ncx,"%s",s[i].val.module.pst_name);
	    ncx += nl;
	}
/*
 * Set node type.
 *
 * Set offset defined if file size not requested or if no size was
 * obtained from the stream head.
 */
	Lf->ntype = N_STREAM;
	Lf->is_stream = 1;
	if (!Fsize || (Fsize && !Lf->sz_def))
	    Lf->off_def = 1;
/*
 * Test for specified file.
 */
	if ((f->psf_subtype == PS_SUBTYPE_CHARDEV)
	||  (f->psf_subtype == PS_SUBTYPE_BLKDEV))
	    i = 1;
	else
	    i = 0;
	if (Sfile && is_file_named((char *)NULL, i))
	    Lf->sf |= SELNM;
/*
 * Enter any name characters.
 */
	if (Namech[0])
	    enter_nm(Namech);
}


/*
 * read_sock() -- read pst_socket info for file
 */

struct pst_socket *
read_sock(f)
	struct pst_fileinfo2 *f;		/* file information */
{
	static struct pst_socket s;

	errno = 0;
	if (f) {
	    if (pstat_getsocket(&s, sizeof(s), &f->psf_fid) > 0
	    &&  f->psf_hi_fileid == s.pst_hi_fileid
	    &&  f->psf_lo_fileid == s.pst_lo_fileid
	    &&  f->psf_hi_nodeid == s.pst_hi_nodeid
	    &&  f->psf_lo_nodeid == s.pst_lo_nodeid)
		return(&s);
	}
	return((struct pst_socket *)NULL);
}
