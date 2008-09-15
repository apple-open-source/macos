/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 *
 * Changes Copyright (C) 2001 Martin Pool <mbp@samba.org>
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * "#ifdef FAITH" part is local hack for supporting IPv4-v6 translator.
 *
 * Issues to be discussed:
 * - Thread safe-ness must be checked.
 * - Return values.  There are nonstandard return values defined and used
 *   in the source code.  This is because RFC2133 is silent about which error
 *   code must be returned for which situation.
 * - PF_UNSPEC case would be handled in getipnodebyname() with the AI_ALL flag.
 */

#include <rsync.h>

#if defined(__KAME__) && defined(INET6)
# define FAITH
#endif

#define SUCCESS 0
#define ANY 0
#define YES 1
#define NO  0

#ifdef FAITH
static int translate = NO;
static struct in6_addr faith_prefix = IN6ADDR_ANY_INIT;
#endif /* FAITH */

/* Amdahl's UTS 2.1.2 defines NO_ADDRESS instead of NO_DATA. */

#ifndef NO_DATA
#ifdef NO_ADDRESS
#define NO_DATA NO_ADDRESS
#endif
#endif /* ndef NO_DATA */

static const char in_addrany[] = { 0, 0, 0, 0 };
static const char in_loopback[] = { 127, 0, 0, 1 }; 
#ifdef INET6
static const char in6_addrany[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};
static const char in6_loopback[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1
};
#endif

struct sockinet {
	u_char	si_len;
	u_char	si_family;
	u_short	si_port;
};

static struct afd {
	int a_af;
	int a_addrlen;
	int a_socklen;
	int a_off;
	const char *a_addrany;
	const char *a_loopback;	
} afdl [] = {
#ifdef INET6
#define N_INET6 0
	{PF_INET6, sizeof(struct in6_addr),
	 sizeof(struct sockaddr_in6),
	 offsetof(struct sockaddr_in6, sin6_addr),
	 in6_addrany, in6_loopback},
#define N_INET  1
#else
#define N_INET  0
#endif
	{PF_INET, sizeof(struct in_addr),
	 sizeof(struct sockaddr_in),
	 offsetof(struct sockaddr_in, sin_addr),
	 in_addrany, in_loopback},
	{0, 0, 0, 0, NULL, NULL},
};

#ifdef INET6
#define PTON_MAX	16
#else
#define PTON_MAX	4
#endif


static int get_name (const char *, struct afd *,
		     struct addrinfo **, char *, struct addrinfo *,
		     int);
static int get_addr (const char *, int, struct addrinfo **,
			struct addrinfo *, int);
static int str_isnumber (const char *);
	
static char *ai_errlist[] = {
	"success.",
	"address family for hostname not supported.",	/* EAI_ADDRFAMILY */
	"temporary failure in name resolution.",	/* EAI_AGAIN      */
	"invalid value for ai_flags.",		       	/* EAI_BADFLAGS   */
	"non-recoverable failure in name resolution.", 	/* EAI_FAIL       */
	"ai_family not supported.",			/* EAI_FAMILY     */
	"memory allocation failure.", 			/* EAI_MEMORY     */
	"no address associated with hostname.", 	/* EAI_NODATA     */
	"hostname nor servname provided, or not known.",/* EAI_NONAME     */
	"servname not supported for ai_socktype.",	/* EAI_SERVICE    */
	"ai_socktype not supported.", 			/* EAI_SOCKTYPE   */
	"system error returned in errno.", 		/* EAI_SYSTEM     */
	"invalid value for hints.",			/* EAI_BADHINTS	  */
	"resolved protocol is unknown.",		/* EAI_PROTOCOL   */
	"unknown error.", 				/* EAI_MAX        */
};

#define GET_CANONNAME(ai, str) \
if (pai->ai_flags & AI_CANONNAME) {\
	int name_size = strlen(str) + 1;\
	if (((ai)->ai_canonname = (char *)malloc(name_size)) != NULL) {\
		memcpy((ai)->ai_canonname, (str), name_size);\
	} else {\
		error = EAI_MEMORY;\
		goto free;\
	}\
}


static int get_ai(struct addrinfo ** to_ai,
		   struct addrinfo const * pai,
		   struct afd *afd,
		   const char *addr,
		   short port)
{
	char *p;
	if ((*to_ai = (struct addrinfo *)malloc(sizeof(struct addrinfo) +
					      ((afd)->a_socklen)))
	    == NULL) 
		return 0;
	memcpy(*to_ai, pai, sizeof(struct addrinfo));
	(*to_ai)->ai_addr = (struct sockaddr *)((*to_ai) + 1);
	memset((*to_ai)->ai_addr, 0, (afd)->a_socklen);
	(*to_ai)->ai_addrlen = (afd)->a_socklen;
#ifdef HAVE_SOCKADDR_LEN
	(*to_ai)->ai_addr->sa_len = (afd)->a_socklen;
#endif
	(*to_ai)->ai_addr->sa_family = (*to_ai)->ai_family = (afd)->a_af;
	((struct sockinet *)(*to_ai)->ai_addr)->si_port = port;
	p = (char *)((*to_ai)->ai_addr);
	memcpy(p + (afd)->a_off, (addr), (afd)->a_addrlen);
	return 1;
}

#define ERR(err) do { error = (err); if (1) goto bad; } while (0)

char *
gai_strerror(ecode)
	int ecode;
{
	if (ecode < 0 || ecode > EAI_MAX)
		ecode = EAI_MAX;
	return ai_errlist[ecode];
}

void
freeaddrinfo(ai)
	struct addrinfo *ai;
{
	struct addrinfo *next;

	do {
		next = ai->ai_next;
		if (ai->ai_canonname)
			free(ai->ai_canonname);
		/* no need to free(ai->ai_addr) */
		free(ai);
	} while ((ai = next) != NULL);
}

static int
str_isnumber(p)
	const char *p;
{
	char *q = (char *)p;
	while (*q) {
		if (! isdigit(*q))
			return NO;
		q++;
	}
	return YES;
}

int
getaddrinfo(hostname, servname, hints, res)
	const char *hostname, *servname;
	const struct addrinfo *hints;
	struct addrinfo **res;
{
	struct addrinfo sentinel;
	struct addrinfo *top = NULL;
	struct addrinfo *cur;
	int i, error = 0;
	char pton[PTON_MAX];
	struct addrinfo ai;
	struct addrinfo *pai;
	u_short port;

#ifdef FAITH
	static int firsttime = 1;

	if (firsttime) {
		/* translator hack */
		{
			char *q = getenv("GAI");
			if (q && inet_pton(AF_INET6, q, &faith_prefix) == 1)
				translate = YES;
		}
		firsttime = 0;
	}
#endif

	/* initialize file static vars */
	sentinel.ai_next = NULL;
	cur = &sentinel;
	pai = &ai;
	pai->ai_flags = 0;
	pai->ai_family = PF_UNSPEC;
	pai->ai_socktype = ANY;
	pai->ai_protocol = ANY;
	pai->ai_addrlen = 0;
	pai->ai_canonname = NULL;
	pai->ai_addr = NULL;
	pai->ai_next = NULL;
	port = ANY;
	
	if (hostname == NULL && servname == NULL)
		return EAI_NONAME;
	if (hints) {
		/* error check for hints */
		if (hints->ai_addrlen || hints->ai_canonname ||
		    hints->ai_addr || hints->ai_next)
			ERR(EAI_BADHINTS); /* xxx */
		if (hints->ai_flags & ~AI_MASK)
			ERR(EAI_BADFLAGS);
		switch (hints->ai_family) {
		case PF_UNSPEC:
		case PF_INET:
#ifdef INET6
		case PF_INET6:
#endif
			break;
		default:
			ERR(EAI_FAMILY);
		}
		memcpy(pai, hints, sizeof(*pai));
		switch (pai->ai_socktype) {
		case ANY:
			switch (pai->ai_protocol) {
			case ANY:
				break;
			case IPPROTO_UDP:
				pai->ai_socktype = SOCK_DGRAM;
				break;
			case IPPROTO_TCP:
				pai->ai_socktype = SOCK_STREAM;
				break;
			default:
				pai->ai_socktype = SOCK_RAW;
				break;
			}
			break;
		case SOCK_RAW:
			break;
		case SOCK_DGRAM:
			if (pai->ai_protocol != IPPROTO_UDP &&
			    pai->ai_protocol != ANY)
				ERR(EAI_BADHINTS);	/*xxx*/
			pai->ai_protocol = IPPROTO_UDP;
			break;
		case SOCK_STREAM:
			if (pai->ai_protocol != IPPROTO_TCP &&
			    pai->ai_protocol != ANY)
				ERR(EAI_BADHINTS);	/*xxx*/
			pai->ai_protocol = IPPROTO_TCP;
			break;
		default:
			ERR(EAI_SOCKTYPE);
			break;
		}
	}

	/*
	 * service port
	 */
	if (servname) {
		if (str_isnumber(servname)) {
			if (pai->ai_socktype == ANY) {
				/* caller accept *ANY* socktype */
				pai->ai_socktype = SOCK_DGRAM;
				pai->ai_protocol = IPPROTO_UDP;
			}
			port = htons(atoi(servname));
		} else {
			struct servent *sp;
			char *proto;

			proto = NULL;
			switch (pai->ai_socktype) {
			case ANY:
				proto = NULL;
				break;
			case SOCK_DGRAM:
				proto = "udp";
				break;
			case SOCK_STREAM:
				proto = "tcp";
				break;
			default:
				fprintf(stderr, "panic!\n");
				break;
			}
			if ((sp = getservbyname(servname, proto)) == NULL)
				ERR(EAI_SERVICE);
			port = sp->s_port;
			if (pai->ai_socktype == ANY) {
				if (strcmp(sp->s_proto, "udp") == 0) {
					pai->ai_socktype = SOCK_DGRAM;
					pai->ai_protocol = IPPROTO_UDP;
				} else if (strcmp(sp->s_proto, "tcp") == 0) {
					pai->ai_socktype = SOCK_STREAM;
					pai->ai_protocol = IPPROTO_TCP;
				} else
					ERR(EAI_PROTOCOL);	/*xxx*/
			}
		}
	}
	
	/*
	 * hostname == NULL.
	 * passive socket -> anyaddr (0.0.0.0 or ::)
	 * non-passive socket -> localhost (127.0.0.1 or ::1)
	 */
	if (hostname == NULL) {
		struct afd *afd;

		for (afd = &afdl[0]; afd->a_af; afd++) {
			if (!(pai->ai_family == PF_UNSPEC
			   || pai->ai_family == afd->a_af)) {
				continue;
			}

			if (pai->ai_flags & AI_PASSIVE) {
				if (!get_ai(&cur->ai_next, pai, afd, afd->a_addrany, port))
					goto free;
				/* xxx meaningless?
				 * GET_CANONNAME(cur->ai_next, "anyaddr");
				 */
			} else {
				if (!get_ai(&cur->ai_next, pai, afd, afd->a_loopback,
					port))
					goto free;
				/* xxx meaningless?
				 * GET_CANONNAME(cur->ai_next, "localhost");
				 */
			}
			cur = cur->ai_next;
		}
		top = sentinel.ai_next;
		if (top)
			goto good;
		else
			ERR(EAI_FAMILY);
	}
	
	/* hostname as numeric name */
	for (i = 0; afdl[i].a_af; i++) {
		if (inet_pton(afdl[i].a_af, hostname, pton)) {
			u_long v4a;

			switch (afdl[i].a_af) {
			case AF_INET:
				v4a = ((struct in_addr *)pton)->s_addr;
				if (IN_MULTICAST(v4a) || IN_EXPERIMENTAL(v4a))
					pai->ai_flags &= ~AI_CANONNAME;
				v4a >>= IN_CLASSA_NSHIFT;
				if (v4a == 0 || v4a == IN_LOOPBACKNET)
					pai->ai_flags &= ~AI_CANONNAME;
				break;
#ifdef INET6
			case AF_INET6:
			{
				u_char pfx;
				pfx = ((struct in6_addr *)pton)->s6_addr[0];
				if (pfx == 0 || pfx == 0xfe || pfx == 0xff)
					pai->ai_flags &= ~AI_CANONNAME;
				break;
			}
#endif
			}
			
			if (pai->ai_family == afdl[i].a_af ||
			    pai->ai_family == PF_UNSPEC) {
				if (! (pai->ai_flags & AI_CANONNAME)) {
					if (get_ai(&top, pai, &afdl[i], pton, port))
						goto good;
					else
						goto free;
				}
				/*
				 * if AI_CANONNAME and if reverse lookup
				 * fail, return ai anyway to pacify
				 * calling application.
				 *
				 * XXX getaddrinfo() is a name->address
				 * translation function, and it looks strange
				 * that we do addr->name translation here.
				 */
				get_name(pton, &afdl[i], &top, pton, pai, port);
				goto good;
			} else 
				ERR(EAI_FAMILY);	/*xxx*/
		}
	}

	if (pai->ai_flags & AI_NUMERICHOST)
		ERR(EAI_NONAME);

	/* hostname as alphabetical name */
	error = get_addr(hostname, pai->ai_family, &top, pai, port);
	if (error == 0) {
		if (top) {
 good:
			*res = top;
			return SUCCESS;
		} else
			error = EAI_FAIL;
	}
 free:
	if (top)
		freeaddrinfo(top);
 bad:
	*res = NULL;
	return error;
}

static int
get_name(addr, afd, res, numaddr, pai, port0)
	const char *addr;
	struct afd *afd;
	struct addrinfo **res;
	char *numaddr;
	struct addrinfo *pai;
	int port0;
{
	u_short port = port0 & 0xffff;
	struct hostent *hp;
	struct addrinfo *cur;
	int error = 0;
	
#ifdef INET6
	{
		int h_error;
		hp = getipnodebyaddr(addr, afd->a_addrlen, afd->a_af, &h_error);
	}
#else
	hp = gethostbyaddr(addr, afd->a_addrlen, AF_INET);
#endif
	if (hp && hp->h_name && hp->h_name[0] && hp->h_addr_list[0]) {
		if (!get_ai(&cur, pai, afd, hp->h_addr_list[0], port))
			goto free;
		GET_CANONNAME(cur, hp->h_name);
	} else {
		if (!get_ai(&cur, pai, afd, numaddr, port))
			goto free;
	}
	
#ifdef INET6
	if (hp)
		freehostent(hp);
#endif
	*res = cur;
	return SUCCESS;
 free:
	if (cur)
		freeaddrinfo(cur);
#ifdef INET6
	if (hp)
		freehostent(hp);
#endif
 /* bad: */
	*res = NULL;
	return error;
}

static int
get_addr(hostname, af, res, pai, port0)
	const char *hostname;
	int af;
	struct addrinfo **res;
	struct addrinfo *pai;
	int port0;
{
	u_short port = port0 & 0xffff;
	struct addrinfo sentinel;
	struct hostent *hp;
	struct addrinfo *top, *cur;
	struct afd *afd;
	int i, error = 0, h_error;
	char *ap;
#ifndef INET6
	extern int h_errno;
#endif

	top = NULL;
	sentinel.ai_next = NULL;
	cur = &sentinel;
#ifdef INET6
	if (af == AF_UNSPEC) {
		hp = getipnodebyname(hostname, AF_INET6,
				AI_ADDRCONFIG|AI_ALL|AI_V4MAPPED, &h_error);
	} else
		hp = getipnodebyname(hostname, af, AI_ADDRCONFIG, &h_error);
#else
	hp = gethostbyname(hostname);
	h_error = h_errno;
#endif
	if (hp == NULL) {
		switch (h_error) {
		case HOST_NOT_FOUND:
		case NO_DATA:
			error = EAI_NODATA;
			break;
		case TRY_AGAIN:
			error = EAI_AGAIN;
			break;
		case NO_RECOVERY:
		default:
			error = EAI_FAIL;
			break;
		}
		goto bad;
	}

	if ((hp->h_name == NULL) || (hp->h_name[0] == 0) ||
	    (hp->h_addr_list[0] == NULL))
		ERR(EAI_FAIL);
	
	for (i = 0; (ap = hp->h_addr_list[i]) != NULL; i++) {
		switch (af) {
#ifdef INET6
		case AF_INET6:
			afd = &afdl[N_INET6];
			break;
#endif
#ifndef INET6
		default:	/* AF_UNSPEC */
#endif
		case AF_INET:
			afd = &afdl[N_INET];
			break;
#ifdef INET6
		default:	/* AF_UNSPEC */
			if (IN6_IS_ADDR_V4MAPPED((struct in6_addr *)ap)) {
				ap += sizeof(struct in6_addr) -
					sizeof(struct in_addr);
				afd = &afdl[N_INET];
			} else
				afd = &afdl[N_INET6];
			break;
#endif
		}
#ifdef FAITH
		if (translate && afd->a_af == AF_INET) {
			struct in6_addr *in6;

			if (!get_ai(&cur->ai_next, pai, &afdl[N_INET6], ap, port))
				goto free;
			in6 = &((struct sockaddr_in6 *)cur->ai_next->ai_addr)->sin6_addr;
			memcpy(&in6->s6_addr32[0], &faith_prefix,
			    sizeof(struct in6_addr) - sizeof(struct in_addr));
			memcpy(&in6->s6_addr32[3], ap, sizeof(struct in_addr));
		} else
#endif /* FAITH */
		if (!get_ai(&cur->ai_next, pai, afd, ap, port))
			goto free;
		if (cur == &sentinel) {
			top = cur->ai_next;
			GET_CANONNAME(top, hp->h_name);
		}
		cur = cur->ai_next;
	}
#ifdef INET6
	freehostent(hp);
#endif
	*res = top;
	return SUCCESS;
 free:
	if (top)
		freeaddrinfo(top);
#ifdef INET6
	if (hp)
		freehostent(hp);
#endif
 bad:
	*res = NULL;
	return error;
}
