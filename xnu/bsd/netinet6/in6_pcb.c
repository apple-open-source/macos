/*
 * Copyright (c) 2003-2021 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
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
 *
 */

/*
 * Copyright (c) 1982, 1986, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)in_pcb.c	8.2 (Berkeley) 1/4/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sockio.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/kauth.h>
#include <sys/priv.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/route.h>
#include <net/ntstat.h>
#include <net/restricted_in_port.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/ip6.h>
#include <netinet/ip_var.h>

#include <netinet6/ip6_var.h>
#include <netinet6/nd6.h>
#include <netinet/in_pcb.h>
#include <netinet6/in6_pcb.h>

#include <net/if_types.h>
#include <net/if_var.h>

#include <kern/kern_types.h>
#include <kern/zalloc.h>

#if IPSEC
#include <netinet6/ipsec.h>
#include <netinet6/ipsec6.h>
#include <netinet6/ah.h>
#include <netinet6/ah6.h>
#include <netkey/key.h>
#endif /* IPSEC */

#if NECP
#include <net/necp.h>
#endif /* NECP */

#include <net/sockaddr_utils.h>

/*
 * in6_pcblookup_local_and_cleanup does everything
 * in6_pcblookup_local does but it checks for a socket
 * that's going away. Since we know that the lock is
 * held read+write when this function is called, we
 * can safely dispose of this socket like the slow
 * timer would usually do and return NULL. This is
 * great for bind.
 */
static struct inpcb *
in6_pcblookup_local_and_cleanup(struct inpcbinfo *pcbinfo,
    struct in6_addr *laddr, u_int lport_arg, uint32_t ifscope, int wild_okay)
{
	struct inpcb *__single inp;

	/* Perform normal lookup */
	inp = in6_pcblookup_local(pcbinfo, laddr, lport_arg, ifscope, wild_okay);

	/* Check if we found a match but it's waiting to be disposed */
	if (inp != NULL && inp->inp_wantcnt == WNT_STOPUSING) {
		struct socket *so = inp->inp_socket;

		socket_lock(so, 0);

		if (so->so_usecount == 0) {
			if (inp->inp_state != INPCB_STATE_DEAD) {
				in6_pcbdetach(inp);
			}
			in_pcbdispose(inp);     /* will unlock & destroy */
			inp = NULL;
		} else {
			socket_unlock(so, 0);
		}
	}

	return inp;
}

/*
 * Bind an INPCB to an address and/or port.  This routine should not alter
 * the caller-supplied local address "nam" or remote address "remote".
 */
int
in6_pcbbind(struct inpcb *inp, struct sockaddr *nam, struct sockaddr *remote, struct proc *p)
{
	struct socket *__single so = inp->inp_socket;
	struct inpcbinfo *__single pcbinfo = inp->inp_pcbinfo;
	u_short lport = 0;
	int wild = 0, reuseport = (so->so_options & SO_REUSEPORT);
	ifnet_ref_t outif = NULL;
	struct sockaddr_in6 sin6;
	uint32_t lifscope = IFSCOPE_NONE;
	int error = 0;
#if XNU_TARGET_OS_OSX
	kauth_cred_t __single cred;
#endif /* XNU_TARGET_OS_OSX */

	if (inp->inp_flags2 & INP2_BIND_IN_PROGRESS) {
		return EINVAL;
	}
	inp->inp_flags2 |= INP2_BIND_IN_PROGRESS;

	if (TAILQ_EMPTY(&in6_ifaddrhead)) { /* XXX broken! */
		error = EADDRNOTAVAIL;
		goto done;
	}
	if (!(so->so_options & (SO_REUSEADDR | SO_REUSEPORT))) {
		wild = 1;
	}

	in_pcb_check_management_entitled(inp);
	in_pcb_check_ultra_constrained_entitled(inp);

	socket_unlock(so, 0); /* keep reference */
	lck_rw_lock_exclusive(&pcbinfo->ipi_lock);
	if (inp->inp_lport || !IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_laddr)) {
		/* another thread completed the bind */
		lck_rw_done(&pcbinfo->ipi_lock);
		socket_lock(so, 0);
		error = EINVAL;
		goto done;
	}

	SOCKADDR_ZERO(&sin6, sizeof(sin6));
	if (nam != NULL) {
		if (nam->sa_len != sizeof(struct sockaddr_in6)) {
			lck_rw_done(&pcbinfo->ipi_lock);
			socket_lock(so, 0);
			error = EINVAL;
			goto done;
		}
		/*
		 * family check.
		 */
		if (nam->sa_family != AF_INET6) {
			lck_rw_done(&pcbinfo->ipi_lock);
			socket_lock(so, 0);
			error = EAFNOSUPPORT;
			goto done;
		}
		lport = SIN6(nam)->sin6_port;

		*(&sin6) = *SIN6(nam);

		/* KAME hack: embed scopeid */
		if (in6_embedscope(&sin6.sin6_addr, &sin6, inp, NULL,
		    NULL, &lifscope) != 0) {
			lck_rw_done(&pcbinfo->ipi_lock);
			socket_lock(so, 0);
			error = EINVAL;
			goto done;
		}

		/* Sanitize local copy for address searches */
		sin6.sin6_flowinfo = 0;
		sin6.sin6_port = 0;
		if (in6_embedded_scope) {
			sin6.sin6_scope_id = 0;
		}

		if (IN6_IS_ADDR_MULTICAST(&sin6.sin6_addr)) {
			/*
			 * Treat SO_REUSEADDR as SO_REUSEPORT for multicast;
			 * allow compepte duplication of binding if
			 * SO_REUSEPORT is set, or if SO_REUSEADDR is set
			 * and a multicast address is bound on both
			 * new and duplicated sockets.
			 */
			if (so->so_options & SO_REUSEADDR) {
				reuseport = SO_REUSEADDR | SO_REUSEPORT;
			}
		} else if (!IN6_IS_ADDR_UNSPECIFIED(&sin6.sin6_addr)) {
			struct ifaddr *__single ifa;

			ifa = ifa_ifwithaddr(SA(&sin6));
			if (ifa == NULL) {
				lck_rw_done(&pcbinfo->ipi_lock);
				socket_lock(so, 0);
				error = EADDRNOTAVAIL;
				goto done;
			} else {
				/*
				 * XXX: bind to an anycast address might
				 * accidentally cause sending a packet with
				 * anycast source address.  We should allow
				 * to bind to a deprecated address, since
				 * the application dare to use it.
				 */
				IFA_LOCK_SPIN(ifa);
				if ((ifatoia6(ifa))->ia6_flags &
				    (IN6_IFF_ANYCAST | IN6_IFF_NOTREADY |
				    IN6_IFF_DETACHED | IN6_IFF_CLAT46)) {
					IFA_UNLOCK(ifa);
					ifa_remref(ifa);
					lck_rw_done(&pcbinfo->ipi_lock);
					socket_lock(so, 0);
					error = EADDRNOTAVAIL;
					goto done;
				}
				/*
				 * Opportunistically determine the outbound
				 * interface that may be used; this may not
				 * hold true if we end up using a route
				 * going over a different interface, e.g.
				 * when sending to a local address.  This
				 * will get updated again after sending.
				 */
				outif = ifa->ifa_ifp;
				IFA_UNLOCK(ifa);
				ifa_remref(ifa);
			}
		}

#if SKYWALK
		if (inp->inp_flags2 & INP2_EXTERNAL_PORT) {
			// Extract the external flow info
			struct ns_flow_info nfi = {};
			int netns_error = necp_client_get_netns_flow_info(inp->necp_client_uuid,
			    &nfi);
			if (netns_error != 0) {
				lck_rw_done(&pcbinfo->ipi_lock);
				socket_lock(so, 0);
				error = netns_error;
				goto done;
			}

			// Extract the reserved port
			u_int16_t reserved_lport = 0;
			if (nfi.nfi_laddr.sa.sa_family == AF_INET) {
				reserved_lport = nfi.nfi_laddr.sin.sin_port;
			} else if (nfi.nfi_laddr.sa.sa_family == AF_INET6) {
				reserved_lport = nfi.nfi_laddr.sin6.sin6_port;
			} else {
				lck_rw_done(&pcbinfo->ipi_lock);
				socket_lock(so, 0);
				error = EINVAL;
				goto done;
			}

			// Validate or use the reserved port
			if (lport == 0) {
				lport = reserved_lport;
			} else if (lport != reserved_lport) {
				lck_rw_done(&pcbinfo->ipi_lock);
				socket_lock(so, 0);
				error = EINVAL;
				goto done;
			}
		}

		/* Do not allow reserving a UDP port if remaining UDP port count is below 4096 */
		if (SOCK_PROTO(so) == IPPROTO_UDP && !allow_udp_port_exhaustion) {
			uint32_t current_reservations = 0;
			current_reservations = netns_lookup_reservations_count_in6(inp->in6p_laddr, IPPROTO_UDP);
			if (USHRT_MAX - UDP_RANDOM_PORT_RESERVE < current_reservations) {
				log(LOG_ERR, "UDP port not available, less than 4096 UDP ports left");
				lck_rw_done(&pcbinfo->ipi_lock);
				socket_lock(so, 0);
				error = EADDRNOTAVAIL;
				goto done;
			}
		}

#endif /* SKYWALK */

		if (lport != 0) {
			struct inpcb *__single t;
			uid_t u;

#if XNU_TARGET_OS_OSX
			if (ntohs(lport) < IPV6PORT_RESERVED &&
			    !IN6_IS_ADDR_UNSPECIFIED(&sin6.sin6_addr) &&
			    !(inp->inp_flags2 & INP2_EXTERNAL_PORT)) {
				cred = kauth_cred_proc_ref(p);
				error = priv_check_cred(cred,
				    PRIV_NETINET_RESERVEDPORT, 0);
				kauth_cred_unref(&cred);
				if (error != 0) {
					lck_rw_done(&pcbinfo->ipi_lock);
					socket_lock(so, 0);
					error = EACCES;
					goto done;
				}
			}
#endif /* XNU_TARGET_OS_OSX */
			/*
			 * Check wether the process is allowed to bind to a restricted port
			 */
			if (!current_task_can_use_restricted_in_port(lport,
			    (uint8_t)SOCK_PROTO(so), PORT_FLAGS_BSD)) {
				lck_rw_done(&pcbinfo->ipi_lock);
				socket_lock(so, 0);
				error = EADDRINUSE;
				goto done;
			}

			if (!IN6_IS_ADDR_MULTICAST(&sin6.sin6_addr) &&
			    (u = kauth_cred_getuid(so->so_cred)) != 0) {
				t = in6_pcblookup_local_and_cleanup(pcbinfo,
				    &sin6.sin6_addr, lport, sin6.sin6_scope_id,
				    INPLOOKUP_WILDCARD);
				if (t != NULL &&
				    (!IN6_IS_ADDR_UNSPECIFIED(&sin6.sin6_addr) ||
				    !IN6_IS_ADDR_UNSPECIFIED(&t->in6p_laddr) ||
				    !(t->inp_socket->so_options & SO_REUSEPORT)) &&
				    (u != kauth_cred_getuid(t->inp_socket->so_cred)) &&
				    !(t->inp_socket->so_flags & SOF_REUSESHAREUID) &&
				    (!(t->inp_flags2 & INP2_EXTERNAL_PORT) ||
				    !(inp->inp_flags2 & INP2_EXTERNAL_PORT) ||
				    uuid_compare(t->necp_client_uuid, inp->necp_client_uuid) != 0)) {
					lck_rw_done(&pcbinfo->ipi_lock);
					socket_lock(so, 0);
					error = EADDRINUSE;
					goto done;
				}
				if (!(inp->inp_flags & IN6P_IPV6_V6ONLY) &&
				    IN6_IS_ADDR_UNSPECIFIED(&sin6.sin6_addr)) {
					struct sockaddr_in sin;

					in6_sin6_2_sin(&sin, &sin6);
					t = in_pcblookup_local_and_cleanup(
						pcbinfo, sin.sin_addr, lport,
						INPLOOKUP_WILDCARD);
					if (t != NULL &&
					    !(t->inp_socket->so_options & SO_REUSEPORT) &&
					    (kauth_cred_getuid(so->so_cred) !=
					    kauth_cred_getuid(t->inp_socket->so_cred)) &&
					    (t->inp_laddr.s_addr != INADDR_ANY ||
					    SOCK_DOM(so) == SOCK_DOM(t->inp_socket)) &&
					    (!(t->inp_flags2 & INP2_EXTERNAL_PORT) ||
					    !(inp->inp_flags2 & INP2_EXTERNAL_PORT) ||
					    uuid_compare(t->necp_client_uuid, inp->necp_client_uuid) != 0)) {
						lck_rw_done(&pcbinfo->ipi_lock);
						socket_lock(so, 0);
						error = EADDRINUSE;
						goto done;
					}

#if SKYWALK
					VERIFY(!NETNS_TOKEN_VALID(
						    &inp->inp_wildcard_netns_token));
					if ((SOCK_PROTO(so) == IPPROTO_TCP ||
					    SOCK_PROTO(so) == IPPROTO_UDP) &&
					    !(inp->inp_flags2 & INP2_EXTERNAL_PORT)) {
						if (netns_reserve_in(&inp->
						    inp_wildcard_netns_token,
						    sin.sin_addr,
						    (uint8_t)SOCK_PROTO(so), lport,
						    NETNS_BSD, NULL) != 0) {
							lck_rw_done(&pcbinfo->ipi_lock);
							socket_lock(so, 0);
							error = EADDRINUSE;
							goto done;
						}
					}
#endif /* SKYWALK */
				}
			}
			t = in6_pcblookup_local_and_cleanup(pcbinfo,
			    &sin6.sin6_addr, lport, sin6.sin6_scope_id, wild);
			if (t != NULL &&
			    (reuseport & t->inp_socket->so_options) == 0 &&
			    (!(t->inp_flags2 & INP2_EXTERNAL_PORT) ||
			    !(inp->inp_flags2 & INP2_EXTERNAL_PORT) ||
			    uuid_compare(t->necp_client_uuid, inp->necp_client_uuid) != 0)) {
#if SKYWALK
				netns_release(&inp->inp_wildcard_netns_token);
#endif /* SKYWALK */
				lck_rw_done(&pcbinfo->ipi_lock);
				socket_lock(so, 0);
				error = EADDRINUSE;
				goto done;
			}
			if (!(inp->inp_flags & IN6P_IPV6_V6ONLY) &&
			    IN6_IS_ADDR_UNSPECIFIED(&sin6.sin6_addr)) {
				struct sockaddr_in sin;

				in6_sin6_2_sin(&sin, &sin6);
				t = in_pcblookup_local_and_cleanup(pcbinfo,
				    sin.sin_addr, lport, wild);
				if (t != NULL && (reuseport &
				    t->inp_socket->so_options) == 0 &&
				    (t->inp_laddr.s_addr != INADDR_ANY ||
				    SOCK_DOM(so) == SOCK_DOM(t->inp_socket)) &&
				    (!(t->inp_flags2 & INP2_EXTERNAL_PORT) ||
				    !(inp->inp_flags2 & INP2_EXTERNAL_PORT) ||
				    uuid_compare(t->necp_client_uuid, inp->necp_client_uuid) != 0)) {
#if SKYWALK
					netns_release(&inp->inp_wildcard_netns_token);
#endif /* SKYWALK */
					lck_rw_done(&pcbinfo->ipi_lock);
					socket_lock(so, 0);
					error = EADDRINUSE;
					goto done;
				}
#if SKYWALK
				if ((SOCK_PROTO(so) == IPPROTO_TCP ||
				    SOCK_PROTO(so) == IPPROTO_UDP) &&
				    !(inp->inp_flags2 & INP2_EXTERNAL_PORT) &&
				    (!NETNS_TOKEN_VALID(
					    &inp->inp_wildcard_netns_token))) {
					if (netns_reserve_in(&inp->
					    inp_wildcard_netns_token,
					    sin.sin_addr,
					    (uint8_t)SOCK_PROTO(so), lport,
					    NETNS_BSD, NULL) != 0) {
						lck_rw_done(&pcbinfo->ipi_lock);
						socket_lock(so, 0);
						error = EADDRINUSE;
						goto done;
					}
				}
#endif /* SKYWALK */
			}
#if SKYWALK
			if ((SOCK_PROTO(so) == IPPROTO_TCP ||
			    SOCK_PROTO(so) == IPPROTO_UDP) &&
			    !(inp->inp_flags2 & INP2_EXTERNAL_PORT)) {
				if (netns_reserve_in6(&inp->inp_netns_token,
				    sin6.sin6_addr, (uint8_t)SOCK_PROTO(so), lport,
				    NETNS_BSD, NULL) != 0) {
					netns_release(&inp->inp_wildcard_netns_token);
					lck_rw_done(&pcbinfo->ipi_lock);
					socket_lock(so, 0);
					error = EADDRINUSE;
					goto done;
				}
			}
#endif /* SKYWALK */
		}
	}

	socket_lock(so, 0);
	/*
	 * We unlocked socket's protocol lock for a long time.
	 * The socket might have been dropped/defuncted.
	 * Checking if world has changed since.
	 */
	if (inp->inp_state == INPCB_STATE_DEAD) {
#if SKYWALK
		netns_release(&inp->inp_netns_token);
		netns_release(&inp->inp_wildcard_netns_token);
#endif /* SKYWALK */
		lck_rw_done(&pcbinfo->ipi_lock);
		error = ECONNABORTED;
		goto done;
	}

	/* check if the socket got bound when the lock was released */
	if (inp->inp_lport || !IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_laddr)) {
#if SKYWALK
		netns_release(&inp->inp_netns_token);
		netns_release(&inp->inp_wildcard_netns_token);
#endif /* SKYWALK */
		lck_rw_done(&pcbinfo->ipi_lock);
		error = EINVAL;
		goto done;
	}

	if (!IN6_IS_ADDR_UNSPECIFIED(&sin6.sin6_addr)) {
		inp->in6p_laddr = sin6.sin6_addr;
		inp->in6p_last_outifp = outif;
		inp->inp_lifscope = lifscope;
		in6_verify_ifscope(&inp->in6p_laddr, lifscope);
#if SKYWALK
		if (NETNS_TOKEN_VALID(&inp->inp_netns_token)) {
			netns_set_ifnet(&inp->inp_netns_token,
			    inp->in6p_last_outifp);
		}
#endif /* SKYWALK */
	}

	if (lport == 0) {
		int e;
		if ((e = in6_pcbsetport(&inp->in6p_laddr, remote, inp, p, 1)) != 0) {
			/* Undo any address bind from above. */
#if SKYWALK
			netns_release(&inp->inp_netns_token);
			netns_release(&inp->inp_wildcard_netns_token);
#endif /* SKYWALK */
			inp->in6p_laddr = in6addr_any;
			inp->in6p_last_outifp = NULL;
			inp->inp_lifscope = IFSCOPE_NONE;
			lck_rw_done(&pcbinfo->ipi_lock);
			error = e;
			goto done;
		}
	} else {
		inp->inp_lport = lport;
		if (in_pcbinshash(inp, remote, 1) != 0) {
#if SKYWALK
			netns_release(&inp->inp_netns_token);
			netns_release(&inp->inp_wildcard_netns_token);
#endif /* SKYWALK */
			inp->in6p_laddr = in6addr_any;
			inp->inp_lifscope = IFSCOPE_NONE;
			inp->inp_lport = 0;
			inp->in6p_last_outifp = NULL;
			lck_rw_done(&pcbinfo->ipi_lock);
			error = EAGAIN;
			goto done;
		}
	}
	lck_rw_done(&pcbinfo->ipi_lock);
	sflt_notify(so, sock_evt_bound, NULL);
done:
	inp->inp_flags2 &= ~INP2_BIND_IN_PROGRESS;
	return error;
}

/*
 * Transform old in6_pcbconnect() into an inner subroutine for new
 * in6_pcbconnect(); do some validity-checking on the remote address
 * (in "nam") and then determine local host address (i.e., which
 * interface) to use to access that remote host.
 *
 * This routine may alter the caller-supplied remote address "nam".
 *
 * This routine might return an ifp with a reference held if the caller
 * provides a non-NULL outif, even in the error case.  The caller is
 * responsible for releasing its reference.
 */
int
in6_pcbladdr(struct inpcb *inp, struct sockaddr *nam,
    struct in6_addr *plocal_addr6, struct ifnet **outif)
{
	struct in6_addr *__single addr6 = NULL;
	struct in6_addr src_storage;
	int error = 0;
	unsigned int ifscope;

	if (outif != NULL) {
		*outif = NULL;
	}
	if (nam->sa_len != sizeof(struct sockaddr_in6)) {
		return EINVAL;
	}
	if (SIN6(nam)->sin6_family != AF_INET6) {
		return EAFNOSUPPORT;
	}
	if (SIN6(nam)->sin6_port == 0) {
		return EADDRNOTAVAIL;
	}

	/* KAME hack: embed scopeid */
	if (in6_embedscope(&SIN6(nam)->sin6_addr, SIN6(nam), inp, NULL, NULL, IN6_NULL_IF_EMBEDDED_SCOPE(&SIN6(nam)->sin6_scope_id)) != 0) {
		return EINVAL;
	}

	in_pcb_check_management_entitled(inp);
	in_pcb_check_ultra_constrained_entitled(inp);

	if (!TAILQ_EMPTY(&in6_ifaddrhead)) {
		/*
		 * If the destination address is UNSPECIFIED addr,
		 * use the loopback addr, e.g ::1.
		 */
		if (IN6_IS_ADDR_UNSPECIFIED(&SIN6(nam)->sin6_addr)) {
			SIN6(nam)->sin6_addr = in6addr_loopback;
		}
	}

	ifscope = (inp->inp_flags & INP_BOUND_IF) ?
	    inp->inp_boundifp->if_index : IFSCOPE_NONE;

	/*
	 * XXX: in6_selectsrc might replace the bound local address
	 * with the address specified by setsockopt(IPV6_PKTINFO).
	 * Is it the intended behavior?
	 *
	 * in6_selectsrc() might return outif with its reference held
	 * even in the error case; caller always needs to release it
	 * if non-NULL.
	 */
	addr6 = in6_selectsrc(SIN6(nam), inp->in6p_outputopts, inp,
	    &inp->in6p_route, outif, &src_storage, ifscope, &error);

	if (outif != NULL) {
		rtentry_ref_t rt = inp->in6p_route.ro_rt;
		/*
		 * If in6_selectsrc() returns a route, it should be one
		 * which points to the same ifp as outif.  Just in case
		 * it isn't, use the one from the route for consistency.
		 * Otherwise if there is no route, leave outif alone as
		 * it could still be useful to the caller.
		 */
		if (rt != NULL && rt->rt_ifp != *outif) {
			ifnet_reference(rt->rt_ifp);    /* for caller */
			if (*outif != NULL) {
				ifnet_release(*outif);
			}
			*outif = rt->rt_ifp;
		}
	}

	if (addr6 == NULL) {
		if (outif != NULL && (*outif) != NULL &&
		    inp_restricted_send(inp, *outif)) {
			soevent(inp->inp_socket,
			    (SO_FILT_HINT_LOCKED | SO_FILT_HINT_IFDENIED));
			error = EHOSTUNREACH;
		}
		if (error == 0) {
			error = EADDRNOTAVAIL;
		}
		return error;
	}

	*plocal_addr6 = *addr6;
	/*
	 * Don't do pcblookup call here; return interface in
	 * plocal_addr6 and exit to caller, that will do the lookup.
	 */
	return 0;
}

/*
 * Outer subroutine:
 * Connect from a socket to a specified address.
 * Both address and port must be specified in argument sin.
 * If don't have a local address for this socket yet,
 * then pick one.
 */
int
in6_pcbconnect(struct inpcb *inp, struct sockaddr *nam, struct proc *p)
{
	struct in6_addr addr6;
	struct sockaddr_in6 *__single sin6 = SIN6(nam);
	struct inpcb *__single pcb;
	int error = 0;
	ifnet_ref_t outif = NULL;
	struct socket *so = inp->inp_socket;

#if CONTENT_FILTER
	so->so_state_change_cnt++;
#endif

	if (SOCK_CHECK_PROTO(so, IPPROTO_UDP) &&
	    sin6->sin6_port == htons(53) && !(so->so_flags1 & SOF1_DNS_COUNTED)) {
		so->so_flags1 |= SOF1_DNS_COUNTED;
		INC_ATOMIC_INT64_LIM(net_api_stats.nas_socket_inet_dgram_dns);
	}

	/*
	 * Call inner routine, to assign local interface address.
	 * in6_pcbladdr() may automatically fill in sin6_scope_id.
	 *
	 * in6_pcbladdr() might return an ifp with its reference held
	 * even in the error case, so make sure that it's released
	 * whenever it's non-NULL.
	 */
	if ((error = in6_pcbladdr(inp, nam, &addr6, &outif)) != 0) {
		if (outif != NULL && inp_restricted_send(inp, outif)) {
			soevent(so,
			    (SO_FILT_HINT_LOCKED | SO_FILT_HINT_IFDENIED));
		}
		goto done;
	}
	socket_unlock(so, 0);

	uint32_t lifscope;
	if (!IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_laddr)) {
		lifscope = inp->inp_lifscope;
	} else if (outif != NULL) {
		lifscope = in6_addr2scopeid(outif, &addr6);
	} else {
		lifscope = sin6->sin6_scope_id;
	}

	pcb = in6_pcblookup_hash(inp->inp_pcbinfo, &sin6->sin6_addr,
	    sin6->sin6_port, sin6->sin6_scope_id, IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_laddr) ?
	    &addr6 : &inp->in6p_laddr, inp->inp_lport, lifscope, 0, NULL);
	socket_lock(so, 0);
	if (pcb != NULL) {
		in_pcb_checkstate(pcb, WNT_RELEASE, pcb == inp ? 1 : 0);
		error = EADDRINUSE;
		goto done;
	}
	if (IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_laddr)) {
		if (inp->inp_lport == 0) {
			error = in6_pcbbind(inp, NULL, nam, p);
			if (error) {
				goto done;
			}
		}
		inp->in6p_laddr = addr6;
		inp->in6p_last_outifp = outif;  /* no reference needed */
		if (IN6_IS_SCOPE_EMBED(&inp->in6p_laddr) &&
		    IN6_ARE_ADDR_EQUAL(&inp->in6p_laddr, &sin6->sin6_addr)) {
			inp->inp_lifscope = sin6->sin6_scope_id;
		} else {
			inp->inp_lifscope = lifscope;
		}
		in6_verify_ifscope(&inp->in6p_laddr, inp->inp_lifscope);
#if SKYWALK
		if (NETNS_TOKEN_VALID(&inp->inp_netns_token)) {
			netns_set_ifnet(&inp->inp_netns_token,
			    inp->in6p_last_outifp);
		}
#endif /* SKYWALK */
		inp->in6p_flags |= INP_IN6ADDR_ANY;
	}
	if (!lck_rw_try_lock_exclusive(&inp->inp_pcbinfo->ipi_lock)) {
		/* lock inversion issue, mostly with udp multicast packets */
		socket_unlock(so, 0);
		lck_rw_lock_exclusive(&inp->inp_pcbinfo->ipi_lock);
		socket_lock(so, 0);
	}
	inp->in6p_faddr = sin6->sin6_addr;
	inp->inp_fport = sin6->sin6_port;
	inp->inp_fifscope = sin6->sin6_scope_id;
	in6_verify_ifscope(&inp->in6p_faddr, inp->inp_fifscope);
	if (nstat_collect && SOCK_PROTO(so) == IPPROTO_UDP) {
		nstat_pcb_invalidate_cache(inp);
	}
	in_pcbrehash(inp);
	lck_rw_done(&inp->inp_pcbinfo->ipi_lock);

done:
	if (outif != NULL) {
		ifnet_release(outif);
	}

	return error;
}

void
in6_pcbdisconnect(struct inpcb *inp)
{
	struct socket *__single so = inp->inp_socket;

#if CONTENT_FILTER
	if (so) {
		so->so_state_change_cnt++;
	}
#endif

	if (!lck_rw_try_lock_exclusive(&inp->inp_pcbinfo->ipi_lock)) {
		/* lock inversion issue, mostly with udp multicast packets */
		socket_unlock(so, 0);
		lck_rw_lock_exclusive(&inp->inp_pcbinfo->ipi_lock);
		socket_lock(so, 0);
	}
	if (nstat_collect && SOCK_PROTO(so) == IPPROTO_UDP) {
		nstat_pcb_cache(inp);
	}
	bzero((caddr_t)&inp->in6p_faddr, sizeof(inp->in6p_faddr));
	inp->inp_fport = 0;
	/* clear flowinfo - RFC 6437 */
	inp->inp_flow &= ~IPV6_FLOWLABEL_MASK;
	in_pcbrehash(inp);
	lck_rw_done(&inp->inp_pcbinfo->ipi_lock);
	/*
	 * A multipath subflow socket would have its SS_NOFDREF set by default,
	 * so check for SOF_MP_SUBFLOW socket flag before detaching the PCB;
	 * when the socket is closed for real, SOF_MP_SUBFLOW would be cleared.
	 */
	if (!(so->so_flags & SOF_MP_SUBFLOW) && (so->so_state & SS_NOFDREF)) {
		in6_pcbdetach(inp);
	}
}

void
in6_pcbdetach(struct inpcb *inp)
{
	struct socket *__single so = inp->inp_socket;

	if (so->so_pcb == NULL) {
		/* PCB has been disposed */
		panic("%s: inp=%p so=%p proto=%d so_pcb is null!", __func__,
		    inp, so, SOCK_PROTO(so));
		/* NOTREACHED */
	}

#if IPSEC
	if (inp->in6p_sp != NULL) {
		(void) ipsec6_delete_pcbpolicy(inp);
	}
#endif /* IPSEC */

	if (inp->inp_stat != NULL && SOCK_PROTO(so) == IPPROTO_UDP) {
		if (inp->inp_stat->rxpackets == 0 && inp->inp_stat->txpackets == 0) {
			INC_ATOMIC_INT64_LIM(net_api_stats.nas_socket_inet6_dgram_no_data);
		}
	}

	/*
	 * Let NetworkStatistics know this PCB is going away
	 * before we detach it.
	 */
	if (nstat_collect &&
	    (SOCK_PROTO(so) == IPPROTO_TCP || SOCK_PROTO(so) == IPPROTO_UDP)) {
		nstat_pcb_detach(inp);
	}
	/* mark socket state as dead */
	if (in_pcb_checkstate(inp, WNT_STOPUSING, 1) != WNT_STOPUSING) {
		panic("%s: so=%p proto=%d couldn't set to STOPUSING",
		    __func__, so, SOCK_PROTO(so));
		/* NOTREACHED */
	}

#if SKYWALK
	/* Free up the port in the namespace registrar if not in TIME_WAIT */
	if (!(inp->inp_flags2 & INP2_TIMEWAIT)) {
		netns_release(&inp->inp_netns_token);
		netns_release(&inp->inp_wildcard_netns_token);
	}
#endif /* SKYWALK */

	if (!(so->so_flags & SOF_PCBCLEARING)) {
		struct ip_moptions *__single imo;
		struct ip6_moptions *__single im6o;

		inp->inp_vflag = 0;
		if (inp->in6p_options != NULL) {
			m_freem(inp->in6p_options);
			inp->in6p_options = NULL;
		}
		ip6_freepcbopts(inp->in6p_outputopts);
		inp->in6p_outputopts = NULL;
		ROUTE_RELEASE(&inp->in6p_route);
		/* free IPv4 related resources in case of mapped addr */
		if (inp->inp_options != NULL) {
			(void) m_free(inp->inp_options);
			inp->inp_options = NULL;
		}
		im6o = inp->in6p_moptions;
		inp->in6p_moptions = NULL;
		if (im6o != NULL) {
			IM6O_REMREF(im6o);
		}
		imo = inp->inp_moptions;
		inp->inp_moptions = NULL;
		if (imo != NULL) {
			IMO_REMREF(imo);
		}

		sofreelastref(so, 0);
		inp->inp_state = INPCB_STATE_DEAD;
		/* makes sure we're not called twice from so_close */
		so->so_flags |= SOF_PCBCLEARING;

		inpcb_gc_sched(inp->inp_pcbinfo, INPCB_TIMER_FAST);
	}
}

struct sockaddr *
in6_sockaddr(in_port_t port, struct in6_addr *addr_p, uint32_t ifscope)
{
	struct sockaddr_in6 *__single sin6;

	sin6 = (struct sockaddr_in6 *)(alloc_sockaddr(sizeof(*sin6),
	    Z_WAITOK | Z_NOFAIL));

	sin6->sin6_family = AF_INET6;
	sin6->sin6_port = port;
	sin6->sin6_addr = *addr_p;

	/* would be good to use sa6_recoverscope(), except for locking  */
	if (IN6_IS_SCOPE_EMBED(&sin6->sin6_addr)) {
		sin6->sin6_scope_id = ifscope;
		if (in6_embedded_scope) {
			in6_verify_ifscope(&sin6->sin6_addr, ifscope);
			sin6->sin6_scope_id = ntohs(sin6->sin6_addr.s6_addr16[1]);
		}
	} else {
		sin6->sin6_scope_id = 0;        /* XXX */
	}
	if (in6_embedded_scope && IN6_IS_SCOPE_LINKLOCAL(&sin6->sin6_addr)) {
		sin6->sin6_addr.s6_addr16[1] = 0;
	}

	return SA(sin6);
}

void
in6_sockaddr_s(in_port_t port, struct in6_addr *addr_p,
    struct sockaddr_in6 *sin6, uint32_t ifscope)
{
	SOCKADDR_ZERO(sin6, sizeof(*sin6));
	sin6->sin6_family = AF_INET6;
	sin6->sin6_len = sizeof(*sin6);
	sin6->sin6_port = port;
	sin6->sin6_addr = *addr_p;

	/* would be good to use sa6_recoverscope(), except for locking  */
	if (IN6_IS_SCOPE_LINKLOCAL(&sin6->sin6_addr)) {
		sin6->sin6_scope_id = ifscope;
		if (in6_embedded_scope) {
			in6_verify_ifscope(&sin6->sin6_addr, ifscope);
			sin6->sin6_scope_id = ntohs(sin6->sin6_addr.s6_addr16[1]);
		}
	} else {
		sin6->sin6_scope_id = 0;        /* XXX */
	}
	if (in6_embedded_scope && IN6_IS_SCOPE_LINKLOCAL(&sin6->sin6_addr)) {
		sin6->sin6_addr.s6_addr16[1] = 0;
	}
}

/*
 * The calling convention of in6_getsockaddr() and in6_getpeeraddr() was
 * modified to match the pru_sockaddr() and pru_peeraddr() entry points
 * in struct pr_usrreqs, so that protocols can just reference then directly
 * without the need for a wrapper function.
 */
int
in6_getsockaddr(struct socket *so, struct sockaddr **nam)
{
	struct inpcb *__single inp;
	struct in6_addr addr;
	in_port_t port;

	if ((inp = sotoinpcb(so)) == NULL) {
		return EINVAL;
	}

	port = inp->inp_lport;
	addr = inp->in6p_laddr;

	*nam = in6_sockaddr(port, &addr, inp->inp_lifscope);
	if (*nam == NULL) {
		return ENOBUFS;
	}
	return 0;
}

int
in6_getsockaddr_s(struct socket *so, struct sockaddr_in6 *ss)
{
	struct inpcb *__single inp;
	struct in6_addr addr;
	in_port_t port;

	VERIFY(ss != NULL);
	SOCKADDR_ZERO(ss, sizeof(*ss));

	if ((inp = sotoinpcb(so)) == NULL) {
		return EINVAL;
	}

	port = inp->inp_lport;
	addr = inp->in6p_laddr;

	in6_sockaddr_s(port, &addr, ss, inp->inp_lifscope);
	return 0;
}

int
in6_getpeeraddr(struct socket *so, struct sockaddr **nam)
{
	struct inpcb *__single inp;
	struct in6_addr addr;
	in_port_t port;

	if ((inp = sotoinpcb(so)) == NULL) {
		return EINVAL;
	}

	port = inp->inp_fport;
	addr = inp->in6p_faddr;

	*nam = in6_sockaddr(port, &addr, inp->inp_fifscope);
	if (*nam == NULL) {
		return ENOBUFS;
	}
	return 0;
}

int
in6_mapped_sockaddr(struct socket *so, struct sockaddr **nam)
{
	struct  inpcb *__single inp = sotoinpcb(so);
	int     error;

	if (inp == NULL) {
		return EINVAL;
	}
	if (inp->inp_vflag & INP_IPV4) {
		error = in_getsockaddr(so, nam);
		if (error == 0) {
			error = in6_sin_2_v4mapsin6_in_sock(nam);
		}
	} else {
		/* scope issues will be handled in in6_getsockaddr(). */
		error = in6_getsockaddr(so, nam);
	}
	return error;
}

int
in6_mapped_peeraddr(struct socket *so, struct sockaddr **nam)
{
	struct  inpcb *inp = sotoinpcb(so);
	int     error;

	if (inp == NULL) {
		return EINVAL;
	}
	if (inp->inp_vflag & INP_IPV4) {
		error = in_getpeeraddr(so, nam);
		if (error == 0) {
			error = in6_sin_2_v4mapsin6_in_sock(nam);
		}
	} else {
		/* scope issues will be handled in in6_getpeeraddr(). */
		error = in6_getpeeraddr(so, nam);
	}
	return error;
}

/*
 * Pass some notification to all connections of a protocol
 * associated with address dst.  The local address and/or port numbers
 * may be specified to limit the search.  The "usual action" will be
 * taken, depending on the ctlinput cmd.  The caller must filter any
 * cmds that are uninteresting (e.g., no error in the map).
 * Call the protocol specific routine (if any) to report
 * any errors for each matching socket.
 */
void
in6_pcbnotify(struct inpcbinfo *pcbinfo, struct sockaddr *dst, u_int fport_arg,
    const struct sockaddr *src, u_int lport_arg, int cmd, void *cmdarg,
    void (*notify)(struct inpcb *, int))
{
	struct inpcbhead *__single head = pcbinfo->ipi_listhead;
	struct inpcb *__single inp, *__single ninp;
	struct sockaddr_in6 sa6_src;
	struct sockaddr_in6 *__single sa6_dst;
	uint16_t fport = (uint16_t)fport_arg, lport = (uint16_t)lport_arg;
	u_int32_t flowinfo;
	int errno;

	if ((unsigned)cmd >= PRC_NCMDS || dst->sa_family != AF_INET6) {
		return;
	}

	sa6_dst = SIN6(dst);
	if (IN6_IS_ADDR_UNSPECIFIED(&sa6_dst->sin6_addr)) {
		return;
	}

	/*
	 * note that src can be NULL when we get notify by local fragmentation.
	 */
	sa6_src = (src == NULL) ?
	    sa6_any : *SIN6(src);
	flowinfo = sa6_src.sin6_flowinfo;

	/*
	 * Redirects go to all references to the destination,
	 * and use in6_rtchange to invalidate the route cache.
	 * Dead host indications: also use in6_rtchange to invalidate
	 * the cache, and deliver the error to all the sockets.
	 * Otherwise, if we have knowledge of the local port and address,
	 * deliver only to that socket.
	 */
	if (PRC_IS_REDIRECT(cmd) || cmd == PRC_HOSTDEAD) {
		fport = 0;
		lport = 0;
		bzero((caddr_t)&sa6_src.sin6_addr, sizeof(sa6_src.sin6_addr));

		if (cmd != PRC_HOSTDEAD) {
			notify = in6_rtchange;
		}
	}
	errno = inet6ctlerrmap[cmd];
	lck_rw_lock_shared(&pcbinfo->ipi_lock);
	for (inp = LIST_FIRST(head); inp != NULL; inp = ninp) {
		ninp = LIST_NEXT(inp, inp_list);

		if (!(inp->inp_vflag & INP_IPV6)) {
			continue;
		}

		/*
		 * If the error designates a new path MTU for a destination
		 * and the application (associated with this socket) wanted to
		 * know the value, notify. Note that we notify for all
		 * disconnected sockets if the corresponding application
		 * wanted. This is because some UDP applications keep sending
		 * sockets disconnected.
		 * XXX: should we avoid to notify the value to TCP sockets?
		 */
		if (cmd == PRC_MSGSIZE && cmdarg != NULL) {
			socket_lock(inp->inp_socket, 1);
			ip6_notify_pmtu(inp, SIN6(dst),
			    (u_int32_t *)cmdarg);
			socket_unlock(inp->inp_socket, 1);
		}

		/*
		 * Detect if we should notify the error. If no source and
		 * destination ports are specifed, but non-zero flowinfo and
		 * local address match, notify the error. This is the case
		 * when the error is delivered with an encrypted buffer
		 * by ESP. Otherwise, just compare addresses and ports
		 * as usual.
		 */
		if (lport == 0 && fport == 0 && flowinfo &&
		    inp->inp_socket != NULL &&
		    flowinfo == (inp->inp_flow & IPV6_FLOWLABEL_MASK) &&
		    in6_are_addr_equal_scoped(&inp->in6p_laddr, &sa6_src.sin6_addr, inp->inp_lifscope, sa6_src.sin6_scope_id)) {
			goto do_notify;
		} else if (!in6_are_addr_equal_scoped(&inp->in6p_faddr, &sa6_dst->sin6_addr,
		    inp->inp_fifscope, sa6_dst->sin6_scope_id) || inp->inp_socket == NULL ||
		    (lport && inp->inp_lport != lport) ||
		    (!IN6_IS_ADDR_UNSPECIFIED(&sa6_src.sin6_addr) &&
		    !in6_are_addr_equal_scoped(&inp->in6p_laddr, &sa6_src.sin6_addr, inp->inp_lifscope, sa6_src.sin6_scope_id)) || (fport && inp->inp_fport != fport)) {
			continue;
		}

do_notify:
		if (notify) {
			if (in_pcb_checkstate(inp, WNT_ACQUIRE, 0) ==
			    WNT_STOPUSING) {
				continue;
			}
			socket_lock(inp->inp_socket, 1);
			(*notify)(inp, errno);
			(void) in_pcb_checkstate(inp, WNT_RELEASE, 1);
			socket_unlock(inp->inp_socket, 1);
		}
	}
	lck_rw_done(&pcbinfo->ipi_lock);
}

/*
 * Lookup a PCB based on the local address and port.
 */
struct inpcb *
in6_pcblookup_local(struct inpcbinfo *pcbinfo, struct in6_addr *laddr,
    u_int lport_arg, uint32_t ifscope, int wild_okay)
{
	struct inpcb *__single inp;
	int matchwild = 3, wildcard;
	uint16_t lport = (uint16_t)lport_arg;
	struct inpcbporthead *__single porthash;
	struct inpcb *__single match = NULL;
	struct inpcbport *__single phd;

	if (!wild_okay) {
		struct inpcbhead *__single head;
		/*
		 * Look for an unconnected (wildcard foreign addr) PCB that
		 * matches the local address and port we're looking for.
		 */
		head = &pcbinfo->ipi_hashbase[INP_PCBHASH(INADDR_ANY, lport, 0,
		    pcbinfo->ipi_hashmask)];
		LIST_FOREACH(inp, head, inp_hash) {
			if (!(inp->inp_vflag & INP_IPV6)) {
				continue;
			}
			if (IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_faddr) &&
			    in6_are_addr_equal_scoped(&inp->in6p_laddr, laddr, inp->inp_lifscope, ifscope) &&
			    inp->inp_lport == lport) {
				/*
				 * Found.
				 */
				return inp;
			}
		}
		/*
		 * Not found.
		 */
		return NULL;
	}
	/*
	 * Best fit PCB lookup.
	 *
	 * First see if this local port is in use by looking on the
	 * port hash list.
	 */
	porthash = &pcbinfo->ipi_porthashbase[INP_PCBPORTHASH(lport,
	    pcbinfo->ipi_porthashmask)];
	LIST_FOREACH(phd, porthash, phd_hash) {
		if (phd->phd_port == lport) {
			break;
		}
	}

	if (phd != NULL) {
		/*
		 * Port is in use by one or more PCBs. Look for best
		 * fit.
		 */
		LIST_FOREACH(inp, &phd->phd_pcblist, inp_portlist) {
			wildcard = 0;
			if (!(inp->inp_vflag & INP_IPV6)) {
				continue;
			}
			if (!IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_faddr)) {
				wildcard++;
			}
			if (!IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_laddr)) {
				if (IN6_IS_ADDR_UNSPECIFIED(laddr)) {
					wildcard++;
				} else if (!in6_are_addr_equal_scoped(
					    &inp->in6p_laddr, laddr, inp->inp_lifscope, ifscope)) {
					continue;
				}
			} else {
				if (!IN6_IS_ADDR_UNSPECIFIED(laddr)) {
					wildcard++;
				}
			}
			if (wildcard < matchwild) {
				match = inp;
				matchwild = wildcard;
				if (matchwild == 0) {
					break;
				}
			}
		}
	}
	return match;
}

/*
 * Check for alternatives when higher level complains
 * about service problems.  For now, invalidate cached
 * routing information.  If the route was created dynamically
 * (by a redirect), time to try a default gateway again.
 */
void
in6_losing(struct inpcb *in6p)
{
	rtentry_ref_t rt;

	if ((rt = in6p->in6p_route.ro_rt) != NULL) {
		RT_LOCK(rt);
		if (rt->rt_flags & RTF_DYNAMIC) {
			/*
			 * Prevent another thread from modifying rt_key,
			 * rt_gateway via rt_setgate() after the rt_lock
			 * is dropped by marking the route as defunct.
			 */
			rt->rt_flags |= RTF_CONDEMNED;
			RT_UNLOCK(rt);
			(void) rtrequest(RTM_DELETE, rt_key(rt),
			    rt->rt_gateway, rt_mask(rt), rt->rt_flags, NULL);
		} else {
			RT_UNLOCK(rt);
		}
		/*
		 * A new route can be allocated
		 * the next time output is attempted.
		 */
	}
	ROUTE_RELEASE(&in6p->in6p_route);
}

/*
 * After a routing change, flush old routing
 * and allocate a (hopefully) better one.
 */
void
in6_rtchange(struct inpcb *inp, int errno)
{
#pragma unused(errno)
	/*
	 * A new route can be allocated the next time
	 * output is attempted.
	 */
	ROUTE_RELEASE(&inp->in6p_route);
}

/*
 * Check if PCB exists hash list. Also returns uid and gid of socket
 */
int
in6_pcblookup_hash_exists(struct inpcbinfo *pcbinfo, struct in6_addr *faddr,
    u_int fport_arg, uint32_t fifscope, struct in6_addr *laddr, u_int lport_arg, uint32_t lifscope, int wildcard,
    uid_t *uid, gid_t *gid, struct ifnet *ifp, bool relaxed)
{
	struct inpcbhead *__single head;
	struct inpcb *__single inp;
	uint16_t fport = (uint16_t)fport_arg, lport = (uint16_t)lport_arg;
	int found;

	*uid = UID_MAX;
	*gid = GID_MAX;

	lck_rw_lock_shared(&pcbinfo->ipi_lock);

	/*
	 * First look for an exact match.
	 */
	head = &pcbinfo->ipi_hashbase[INP_PCBHASH(faddr->s6_addr32[3] /* XXX */,
	    lport, fport, pcbinfo->ipi_hashmask)];
	LIST_FOREACH(inp, head, inp_hash) {
		if (!(inp->inp_vflag & INP_IPV6)) {
			continue;
		}

		if (inp_restricted_recv(inp, ifp)) {
			continue;
		}

#if NECP
		if (!necp_socket_is_allowed_to_recv_on_interface(inp, ifp)) {
			continue;
		}
#endif /* NECP */

		if (((in6_are_addr_equal_scoped(&inp->in6p_faddr, faddr, inp->inp_fifscope, fifscope) &&
		    in6_are_addr_equal_scoped(&inp->in6p_laddr, laddr, inp->inp_lifscope, lifscope)) ||
		    (relaxed &&
		    IN6_ARE_ADDR_EQUAL(&inp->in6p_faddr, faddr) &&
		    IN6_ARE_ADDR_EQUAL(&inp->in6p_laddr, laddr))) &&
		    inp->inp_fport == fport &&
		    inp->inp_lport == lport) {
			if ((found = (inp->inp_socket != NULL))) {
				/*
				 * Found. Check if pcb is still valid
				 */
				*uid = kauth_cred_getuid(
					inp->inp_socket->so_cred);
				*gid = kauth_cred_getgid(
					inp->inp_socket->so_cred);
			}
			lck_rw_done(&pcbinfo->ipi_lock);
			return found;
		}
	}
	if (wildcard) {
		struct inpcb *__single local_wild = NULL;

		head = &pcbinfo->ipi_hashbase[INP_PCBHASH(INADDR_ANY, lport, 0,
		    pcbinfo->ipi_hashmask)];
		LIST_FOREACH(inp, head, inp_hash) {
			if (!(inp->inp_vflag & INP_IPV6)) {
				continue;
			}

			if (inp_restricted_recv(inp, ifp)) {
				continue;
			}

#if NECP
			if (!necp_socket_is_allowed_to_recv_on_interface(inp, ifp)) {
				continue;
			}
#endif /* NECP */

			if (IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_faddr) &&
			    inp->inp_lport == lport) {
				if (in6_are_addr_equal_scoped(&inp->in6p_laddr,
				    laddr, inp->inp_lifscope, lifscope) ||
				    (relaxed && IN6_ARE_ADDR_EQUAL(&inp->in6p_laddr, laddr))) {
					found = (inp->inp_socket != NULL);
					if (found) {
						*uid = kauth_cred_getuid(
							inp->inp_socket->so_cred);
						*gid = kauth_cred_getgid(
							inp->inp_socket->so_cred);
					}
					lck_rw_done(&pcbinfo->ipi_lock);
					return found;
				} else if (IN6_IS_ADDR_UNSPECIFIED(
					    &inp->in6p_laddr)) {
					local_wild = inp;
				}
			}
		}
		if (local_wild) {
			if ((found = (local_wild->inp_socket != NULL))) {
				*uid = kauth_cred_getuid(
					local_wild->inp_socket->so_cred);
				*gid = kauth_cred_getgid(
					local_wild->inp_socket->so_cred);
			}
			lck_rw_done(&pcbinfo->ipi_lock);
			return found;
		}
	}

	/*
	 * Not found.
	 */
	lck_rw_done(&pcbinfo->ipi_lock);
	return 0;
}

/*
 * Lookup PCB in hash list.
 */
struct inpcb *
in6_pcblookup_hash(struct inpcbinfo *pcbinfo, struct in6_addr *faddr,
    u_int fport_arg, uint32_t fifscope, struct in6_addr *laddr, u_int lport_arg, uint32_t lifscope, int wildcard,
    struct ifnet *ifp)
{
	struct inpcbhead *__single head;
	struct inpcb *__single inp;
	uint16_t fport = (uint16_t)fport_arg, lport = (uint16_t)lport_arg;

	lck_rw_lock_shared(&pcbinfo->ipi_lock);

	/*
	 * First look for an exact match.
	 */
	head = &pcbinfo->ipi_hashbase[INP_PCBHASH(faddr->s6_addr32[3] /* XXX */,
	    lport, fport, pcbinfo->ipi_hashmask)];
	LIST_FOREACH(inp, head, inp_hash) {
		if (!(inp->inp_vflag & INP_IPV6)) {
			continue;
		}

		if (inp_restricted_recv(inp, ifp)) {
			continue;
		}

#if NECP
		if (!necp_socket_is_allowed_to_recv_on_interface(inp, ifp)) {
			continue;
		}
#endif /* NECP */

		if (in6_are_addr_equal_scoped(&inp->in6p_faddr, faddr, inp->inp_fifscope, fifscope) &&
		    in6_are_addr_equal_scoped(&inp->in6p_laddr, laddr, inp->inp_lifscope, lifscope) &&
		    inp->inp_fport == fport &&
		    inp->inp_lport == lport) {
			/*
			 * Found. Check if pcb is still valid
			 */
			if (in_pcb_checkstate(inp, WNT_ACQUIRE, 0) !=
			    WNT_STOPUSING) {
				lck_rw_done(&pcbinfo->ipi_lock);
				return inp;
			} else {
				/* it's there but dead, say it isn't found */
				lck_rw_done(&pcbinfo->ipi_lock);
				return NULL;
			}
		}
	}
	if (wildcard) {
		struct inpcb *__single local_wild = NULL;

		head = &pcbinfo->ipi_hashbase[INP_PCBHASH(INADDR_ANY, lport, 0,
		    pcbinfo->ipi_hashmask)];
		LIST_FOREACH(inp, head, inp_hash) {
			if (!(inp->inp_vflag & INP_IPV6)) {
				continue;
			}

			if (inp_restricted_recv(inp, ifp)) {
				continue;
			}

#if NECP
			if (!necp_socket_is_allowed_to_recv_on_interface(inp, ifp)) {
				continue;
			}
#endif /* NECP */

			if (IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_faddr) &&
			    inp->inp_lport == lport) {
				if (in6_are_addr_equal_scoped(&inp->in6p_laddr,
				    laddr, inp->inp_lifscope, lifscope)) {
					if (in_pcb_checkstate(inp, WNT_ACQUIRE,
					    0) != WNT_STOPUSING) {
						lck_rw_done(&pcbinfo->ipi_lock);
						return inp;
					} else {
						/* dead; say it isn't found */
						lck_rw_done(&pcbinfo->ipi_lock);
						return NULL;
					}
				} else if (IN6_IS_ADDR_UNSPECIFIED(
					    &inp->in6p_laddr)) {
					local_wild = inp;
				}
			}
		}
		if (local_wild && in_pcb_checkstate(local_wild,
		    WNT_ACQUIRE, 0) != WNT_STOPUSING) {
			lck_rw_done(&pcbinfo->ipi_lock);
			return local_wild;
		} else {
			lck_rw_done(&pcbinfo->ipi_lock);
			return NULL;
		}
	}

	/*
	 * Not found.
	 */
	lck_rw_done(&pcbinfo->ipi_lock);
	return NULL;
}

void
init_sin6(struct sockaddr_in6 *sin6, struct mbuf *m)
{
	struct ip6_hdr *__single ip;

	ip = mtod(m, struct ip6_hdr *);
	SOCKADDR_ZERO(sin6, sizeof(*sin6));
	sin6->sin6_len = sizeof(*sin6);
	sin6->sin6_family = AF_INET6;
	sin6->sin6_addr = ip->ip6_src;
	if (IN6_IS_SCOPE_LINKLOCAL(&sin6->sin6_addr)) {
		if (in6_embedded_scope) {
			sin6->sin6_addr.s6_addr16[1] = 0;
		}
		if ((m->m_pkthdr.pkt_flags & (PKTF_LOOP | PKTF_IFAINFO)) ==
		    (PKTF_LOOP | PKTF_IFAINFO)) {
			sin6->sin6_scope_id = m->m_pkthdr.src_ifindex;
		} else if (m->m_pkthdr.rcvif != NULL) {
			sin6->sin6_scope_id = m->m_pkthdr.rcvif->if_index;
		}
	}
}

/*
 * The following routines implement this scheme:
 *
 * Callers of ip6_output() that intend to cache the route in the inpcb pass
 * a local copy of the struct route to ip6_output().  Using a local copy of
 * the cached route significantly simplifies things as IP no longer has to
 * worry about having exclusive access to the passed in struct route, since
 * it's defined in the caller's stack; in essence, this allows for a lock-
 * less operation when updating the struct route at the IP level and below,
 * whenever necessary. The scheme works as follows:
 *
 * Prior to dropping the socket's lock and calling ip6_output(), the caller
 * copies the struct route from the inpcb into its stack, and adds a reference
 * to the cached route entry, if there was any.  The socket's lock is then
 * dropped and ip6_output() is called with a pointer to the copy of struct
 * route defined on the stack (not to the one in the inpcb.)
 *
 * Upon returning from ip6_output(), the caller then acquires the socket's
 * lock and synchronizes the cache; if there is no route cached in the inpcb,
 * it copies the local copy of struct route (which may or may not contain any
 * route) back into the cache; otherwise, if the inpcb has a route cached in
 * it, the one in the local copy will be freed, if there's any.  Trashing the
 * cached route in the inpcb can be avoided because ip6_output() is single-
 * threaded per-PCB (i.e. multiple transmits on a PCB are always serialized
 * by the socket/transport layer.)
 */
void
in6p_route_copyout(struct inpcb *inp, struct route_in6 *dst)
{
	struct route_in6 *__single src = &inp->in6p_route;

	socket_lock_assert_owned(inp->inp_socket);

	/* Minor sanity check */
	if (src->ro_rt != NULL && rt_key(src->ro_rt)->sa_family != AF_INET6) {
		panic("%s: wrong or corrupted route: %p", __func__, src);
	}

	route_copyout((struct route *)dst, (struct route *)src, sizeof(*dst));
}

void
in6p_route_copyin(struct inpcb *inp, struct route_in6 *src)
{
	struct route_in6 *__single dst = &inp->in6p_route;

	socket_lock_assert_owned(inp->inp_socket);

	/* Minor sanity check */
	if (src->ro_rt != NULL && rt_key(src->ro_rt)->sa_family != AF_INET6) {
		panic("%s: wrong or corrupted route: %p", __func__, src);
	}

	route_copyin((struct route *)src, (struct route *)dst, sizeof(*src));
}
