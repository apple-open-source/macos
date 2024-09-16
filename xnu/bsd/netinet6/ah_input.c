/*
 * Copyright (c) 2008-2023 Apple Inc. All rights reserved.
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

/*	$FreeBSD: src/sys/netinet6/ah_input.c,v 1.1.2.6 2002/04/28 05:40:26 suz Exp $	*/
/*	$KAME: ah_input.c,v 1.67 2002/01/07 11:39:56 kjc Exp $	*/

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
 */

/*
 * RFC1826/2402 authentication header.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mcache.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_ipsec.h>
#include <net/route.h>
#include <kern/cpu_number.h>
#include <kern/locks.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_ecn.h>
#include <netinet/in_pcb.h>
#include <netinet6/ip6_ecn.h>

#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/in6_pcb.h>
#include <netinet/icmp6.h>
#include <netinet6/ip6protosw.h>

#include <netinet6/ipsec.h>
#include <netinet6/ipsec6.h>
#include <netinet6/ah.h>
#include <netinet6/ah6.h>
#include <netkey/key.h>
#include <netkey/keydb.h>
#if IPSEC_DEBUG
#include <netkey/key_debug.h>
#else
#define KEYDEBUG(lev, arg)
#endif

#include <net/kpi_protocol.h>
#include <netinet/kpi_ipfilter_var.h>
#include <mach/sdt.h>

#include <net/net_osdep.h>

#define IPLEN_FLIPPED

#if INET
void
ah4_input(struct mbuf *m, int off)
{
	union sockaddr_in_4_6 src = {};
	union sockaddr_in_4_6 dst = {};
	struct ip *ip;
	struct ah *ah;
	u_int32_t spi;
	const struct ah_algorithm *algo;
	size_t siz;
	size_t siz1;
	u_char *__bidi_indexable cksum = NULL;
	struct secasvar *sav = NULL;
	u_int16_t nxt;
	u_int8_t hlen;
	size_t stripsiz = 0;
	sa_family_t ifamily;

	if (m->m_len < off + sizeof(struct newah)) {
		m = m_pullup(m, off + sizeof(struct newah));
		if (!m) {
			ipseclog((LOG_DEBUG, "IPv4 AH input: can't pullup;"
			    "dropping the packet for simplicity\n"));
			IPSEC_STAT_INCREMENT(ipsecstat.in_inval);
			goto fail;
		}
	}

	/* Expect 32-bit aligned data pointer on strict-align platforms */
	MBUF_STRICT_DATA_ALIGNMENT_CHECK_32(m);

	ip = mtod(m, struct ip *);
	ah = (struct ah *)(void *)(((caddr_t)ip) + off);
	nxt = ah->ah_nxt;
#ifdef _IP_VHL
	hlen = (u_int8_t)(IP_VHL_HL(ip->ip_vhl) << 2);
#else
	hlen = (u_int8_t)(ip->ip_hl << 2);
#endif

	/* find the sassoc. */
	spi = ah->ah_spi;

	ipsec_fill_ip_sockaddr_4_6(&src, ip->ip_src, 0);
	ipsec_fill_ip_sockaddr_4_6(&dst, ip->ip_dst, 0);

	if ((sav = key_allocsa(&src, &dst, IPPROTO_AH, spi, NULL)) == 0) {
		ipseclog((LOG_WARNING,
		    "IPv4 AH input: no key association found for spi %u\n",
		    (u_int32_t)ntohl(spi)));
		IPSEC_STAT_INCREMENT(ipsecstat.in_nosa);
		goto fail;
	}
	KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
	    printf("DP ah4_input called to allocate SA:0x%llx\n",
	    (uint64_t)VM_KERNEL_ADDRPERM(sav)));
	if (sav->state != SADB_SASTATE_MATURE
	    && sav->state != SADB_SASTATE_DYING) {
		ipseclog((LOG_DEBUG,
		    "IPv4 AH input: non-mature/dying SA found for spi %u\n",
		    (u_int32_t)ntohl(spi)));
		IPSEC_STAT_INCREMENT(ipsecstat.in_badspi);
		goto fail;
	}

	algo = ah_algorithm_lookup(sav->alg_auth);
	if (!algo) {
		ipseclog((LOG_DEBUG, "IPv4 AH input: "
		    "unsupported authentication algorithm for spi %u\n",
		    (u_int32_t)ntohl(spi)));
		IPSEC_STAT_INCREMENT(ipsecstat.in_badspi);
		goto fail;
	}

	siz = (*algo->sumsiz)(sav);
	siz1 = ((siz + 3) & ~(4 - 1));

	/*
	 * sanity checks for header, 1.
	 */
	{
		int sizoff;

		sizoff = (sav->flags & SADB_X_EXT_OLD) ? 0 : 4;

		/*
		 * Here, we do not do "siz1 == siz".  This is because the way
		 * RFC240[34] section 2 is written.  They do not require truncation
		 * to 96 bits.
		 * For example, Microsoft IPsec stack attaches 160 bits of
		 * authentication data for both hmac-md5 and hmac-sha1.  For hmac-sha1,
		 * 32 bits of padding is attached.
		 *
		 * There are two downsides to this specification.
		 * They have no real harm, however, they leave us fuzzy feeling.
		 * - if we attach more than 96 bits of authentication data onto AH,
		 *   we will never notice about possible modification by rogue
		 *   intermediate nodes.
		 *   Since extra bits in AH checksum is never used, this constitutes
		 *   no real issue, however, it is wacky.
		 * - even if the peer attaches big authentication data, we will never
		 *   notice the difference, since longer authentication data will just
		 *   work.
		 *
		 * We may need some clarification in the spec.
		 */
		if (siz1 < siz) {
			ipseclog((LOG_NOTICE, "sum length too short in IPv4 AH input "
			    "(%u, should be at least %u): %s\n",
			    (u_int32_t)siz1, (u_int32_t)siz,
			    ipsec4_logpacketstr(ip, spi)));
			IPSEC_STAT_INCREMENT(ipsecstat.in_inval);
			goto fail;
		}
		if ((ah->ah_len << 2) - sizoff != siz1) {
			ipseclog((LOG_NOTICE, "sum length mismatch in IPv4 AH input "
			    "(%d should be %u): %s\n",
			    (ah->ah_len << 2) - sizoff, (u_int32_t)siz1,
			    ipsec4_logpacketstr(ip, spi)));
			IPSEC_STAT_INCREMENT(ipsecstat.in_inval);
			goto fail;
		}

		if (m->m_len < off + sizeof(struct ah) + sizoff + siz1) {
			VERIFY((off + sizeof(struct ah) + sizoff + siz1) <= INT_MAX);
			m = m_pullup(m, (int)(off + sizeof(struct ah) + sizoff + siz1));
			if (!m) {
				ipseclog((LOG_DEBUG, "IPv4 AH input: can't pullup\n"));
				IPSEC_STAT_INCREMENT(ipsecstat.in_inval);
				goto fail;
			}
			/* Expect 32-bit aligned data ptr on strict-align platforms */
			MBUF_STRICT_DATA_ALIGNMENT_CHECK_32(m);

			ip = mtod(m, struct ip *);
			ah = (struct ah *)(void *)(((caddr_t)ip) + off);
		}
	}

	/*
	 * check for sequence number.
	 */
	if ((sav->flags & SADB_X_EXT_OLD) == 0 && sav->replay[0] != NULL) {
		if (ipsec_chkreplay(ntohl(((struct newah *)ah)->ah_seq), sav, 0)) {
			; /*okey*/
		} else {
			IPSEC_STAT_INCREMENT(ipsecstat.in_ahreplay);
			ipseclog((LOG_WARNING,
			    "replay packet in IPv4 AH input: %s %s\n",
			    ipsec4_logpacketstr(ip, spi), ipsec_logsastr(sav)));
			goto fail;
		}
	}

	/*
	 * alright, it seems sane.  now we are going to check the
	 * cryptographic checksum.
	 */
	cksum = (u_char *)kalloc_data(siz1, Z_NOWAIT);
	if (!cksum) {
		ipseclog((LOG_DEBUG, "IPv4 AH input: "
		    "couldn't alloc temporary region for cksum\n"));
		IPSEC_STAT_INCREMENT(ipsecstat.in_inval);
		goto fail;
	}

	/*
	 * some of IP header fields are flipped to the host endian.
	 * convert them back to network endian.  VERY stupid.
	 */
	if ((ip->ip_len + hlen) > UINT16_MAX) {
		ipseclog((LOG_DEBUG, "IPv4 AH input: "
		    "bad length ip header len %u, total len %u\n",
		    ip->ip_len, hlen));
		IPSEC_STAT_INCREMENT(ipsecstat.in_inval);
		goto fail;
	}

	ip->ip_len = htons((u_int16_t)(ip->ip_len + hlen));
	ip->ip_off = htons(ip->ip_off);
	if (ah4_calccksum(m, (caddr_t)cksum, siz1, algo, sav)) {
		kfree_data(cksum, siz1);
		IPSEC_STAT_INCREMENT(ipsecstat.in_inval);
		goto fail;
	}
	IPSEC_STAT_INCREMENT(ipsecstat.in_ahhist[sav->alg_auth]);
	/*
	 * flip them back.
	 */
	ip->ip_len = ntohs(ip->ip_len) - hlen;
	ip->ip_off = ntohs(ip->ip_off);

	{
		caddr_t sumpos = NULL;

		if (sav->flags & SADB_X_EXT_OLD) {
			/* RFC 1826 */
			sumpos = (caddr_t)(ah + 1);
		} else {
			/* RFC 2402 */
			sumpos = (caddr_t)(((struct newah *)ah) + 1);
		}

		if (bcmp(sumpos, cksum, siz) != 0) {
			ipseclog((LOG_WARNING,
			    "checksum mismatch in IPv4 AH input: %s %s\n",
			    ipsec4_logpacketstr(ip, spi), ipsec_logsastr(sav)));
			kfree_data(cksum, siz1);
			IPSEC_STAT_INCREMENT(ipsecstat.in_ahauthfail);
			goto fail;
		}
	}

	kfree_data(cksum, siz1);

	m->m_flags |= M_AUTHIPHDR;
	m->m_flags |= M_AUTHIPDGM;

	if (m->m_flags & M_AUTHIPHDR && m->m_flags & M_AUTHIPDGM) {
		IPSEC_STAT_INCREMENT(ipsecstat.in_ahauthsucc);
	} else {
		ipseclog((LOG_WARNING,
		    "authentication failed in IPv4 AH input: %s %s\n",
		    ipsec4_logpacketstr(ip, spi), ipsec_logsastr(sav)));
		IPSEC_STAT_INCREMENT(ipsecstat.in_ahauthfail);
		goto fail;
	}

	/*
	 * update sequence number.
	 */
	if ((sav->flags & SADB_X_EXT_OLD) == 0 && sav->replay[0] != NULL) {
		if (ipsec_updatereplay(ntohl(((struct newah *)ah)->ah_seq), sav, 0)) {
			IPSEC_STAT_INCREMENT(ipsecstat.in_ahreplay);
			goto fail;
		}
	}

	/* was it transmitted over the IPsec tunnel SA? */
	if (sav->flags & SADB_X_EXT_OLD) {
		/* RFC 1826 */
		stripsiz = sizeof(struct ah) + siz1;
	} else {
		/* RFC 2402 */
		stripsiz = sizeof(struct newah) + siz1;
	}
	if (ipsec4_tunnel_validate(m, (int)(off + stripsiz), nxt, sav, &ifamily)) {
		ifaddr_t ifa;
		struct sockaddr_storage addr;
		struct sockaddr_in *ipaddr;

		/*
		 * strip off all the headers that precedes AH.
		 *	IP xx AH IP' payload -> IP' payload
		 *
		 * XXX more sanity checks
		 * XXX relationship with gif?
		 */
		u_int8_t tos, otos;
		int sum;

		if (ifamily == AF_INET6) {
			ipseclog((LOG_NOTICE, "ipsec tunnel protocol mismatch "
			    "in IPv4 AH input: %s\n", ipsec_logsastr(sav)));
			goto fail;
		}
		tos = ip->ip_tos;
		m_adj(m, (int)(off + stripsiz));
		if (m->m_len < sizeof(*ip)) {
			m = m_pullup(m, sizeof(*ip));
			if (!m) {
				IPSEC_STAT_INCREMENT(ipsecstat.in_inval);
				goto fail;
			}
		}
		ip = mtod(m, struct ip *);
		otos = ip->ip_tos;
		/* ECN consideration. */
		if (ip_ecn_egress(ip4_ipsec_ecn, &tos, &ip->ip_tos) == 0) {
			IPSEC_STAT_INCREMENT(ipsecstat.in_inval);
			goto fail;
		}

		if (otos != ip->ip_tos) {
			sum = ~ntohs(ip->ip_sum) & 0xffff;
			sum += (~otos & 0xffff) + ip->ip_tos;
			sum = (sum >> 16) + (sum & 0xffff);
			sum += (sum >> 16); /* add carry */
			ip->ip_sum = htons(~sum & 0xffff);
		}

		if (!key_checktunnelsanity(sav, AF_INET,
		    (caddr_t)&ip->ip_src, (caddr_t)&ip->ip_dst)) {
			ipseclog((LOG_NOTICE, "ipsec tunnel address mismatch "
			    "in IPv4 AH input: %s %s\n",
			    ipsec4_logpacketstr(ip, spi), ipsec_logsastr(sav)));
			IPSEC_STAT_INCREMENT(ipsecstat.in_inval);
			goto fail;
		}

#if 1
		/*
		 * Should the inner packet be considered authentic?
		 * My current answer is: NO.
		 *
		 * host1 -- gw1 === gw2 -- host2
		 *	In this case, gw2 can trust the	authenticity of the
		 *	outer packet, but NOT inner.  Packet may be altered
		 *	between host1 and gw1.
		 *
		 * host1 -- gw1 === host2
		 *	This case falls into the same scenario as above.
		 *
		 * host1 === host2
		 *	This case is the only case when we may be able to leave
		 *	M_AUTHIPHDR and M_AUTHIPDGM set.
		 *	However, if host1 is wrongly configured, and allows
		 *	attacker to inject some packet with src=host1 and
		 *	dst=host2, you are in risk.
		 */
		m->m_flags &= ~M_AUTHIPHDR;
		m->m_flags &= ~M_AUTHIPDGM;
#endif

		key_sa_recordxfer(sav, m->m_pkthdr.len);
		if (ipsec_incr_history_count(m, IPPROTO_AH, spi) != 0 ||
		    ipsec_incr_history_count(m, IPPROTO_IPV4, 0) != 0) {
			IPSEC_STAT_INCREMENT(ipsecstat.in_nomem);
			goto fail;
		}

		bzero(&addr, sizeof(addr));
		ipaddr = (__typeof__(ipaddr)) & addr;
		ipaddr->sin_family = AF_INET;
		ipaddr->sin_len = sizeof(*ipaddr);
		ipaddr->sin_addr = ip->ip_dst;

		// update the receiving interface address based on the inner address
		ifa = ifa_ifwithaddr((struct sockaddr *)&addr);
		if (ifa) {
			m->m_pkthdr.rcvif = ifa->ifa_ifp;
			ifa_remref(ifa);
		}

		// Input via IPsec interface
		lck_mtx_lock(sadb_mutex);
		ifnet_t ipsec_if = sav->sah->ipsec_if;
		if (ipsec_if != NULL) {
			// If an interface is found, add a reference count before dropping the lock
			ifnet_reference(ipsec_if);
		}
		lck_mtx_unlock(sadb_mutex);
		if (ipsec_if != NULL) {
			errno_t inject_error = ipsec_inject_inbound_packet(ipsec_if, m);
			ifnet_release(ipsec_if);
			if (inject_error == 0) {
				m = NULL;
				goto done;
			} else {
				goto fail;
			}
		}

		if (proto_input(PF_INET, m) != 0) {
			goto fail;
		}
		nxt = IPPROTO_DONE;
	} else {
		/*
		 * strip off AH.
		 */

		ip = mtod(m, struct ip *);
		/*
		 * We do deep-copy since KAME requires that
		 * the packet is placed in a single external mbuf.
		 */
		ovbcopy((caddr_t)ip, (caddr_t)(((u_char *)ip) + stripsiz), off);
		m->m_data += stripsiz;
		m->m_len -= stripsiz;
		m->m_pkthdr.len -= stripsiz;

		if (m->m_len < sizeof(*ip)) {
			m = m_pullup(m, sizeof(*ip));
			if (m == NULL) {
				IPSEC_STAT_INCREMENT(ipsecstat.in_inval);
				goto fail;
			}
		}
		ip = mtod(m, struct ip *);
#ifdef IPLEN_FLIPPED
		ip->ip_len = (u_short)(ip->ip_len - stripsiz);
#else
		ip->ip_len = htons(ntohs(ip->ip_len) - stripsiz);
#endif
		ip->ip_p = (u_char)nxt;
		/* forget about IP hdr checksum, the check has already been passed */

		key_sa_recordxfer(sav, m->m_pkthdr.len);
		if (ipsec_incr_history_count(m, IPPROTO_AH, spi) != 0) {
			IPSEC_STAT_INCREMENT(ipsecstat.in_nomem);
			goto fail;
		}

		DTRACE_IP6(receive, struct mbuf *, m, struct inpcb *, NULL,
		    struct ip *, ip, struct ifnet *, m->m_pkthdr.rcvif,
		    struct ip *, ip, struct ip6_hdr *, NULL);

		if (nxt != IPPROTO_DONE) {
			// Input via IPsec interface
			lck_mtx_lock(sadb_mutex);
			ifnet_t ipsec_if = sav->sah->ipsec_if;
			if (ipsec_if != NULL) {
				// If an interface is found, add a reference count before dropping the lock
				ifnet_reference(ipsec_if);
			}
			lck_mtx_unlock(sadb_mutex);
			if (ipsec_if != NULL) {
				ip->ip_len = htons(ip->ip_len + hlen);
				ip->ip_off = htons(ip->ip_off);
				ip->ip_sum = 0;
				ip->ip_sum = ip_cksum_hdr_in(m, hlen);
				errno_t inject_error = ipsec_inject_inbound_packet(ipsec_if, m);
				ifnet_release(ipsec_if);
				if (inject_error == 0) {
					m = NULL;
					goto done;
				} else {
					goto fail;
				}
			}

			if ((ip_protox[nxt]->pr_flags & PR_LASTHDR) != 0 &&
			    ipsec4_in_reject(m, NULL)) {
				IPSEC_STAT_INCREMENT(ipsecstat.in_polvio);
				goto fail;
			}
			ip_proto_dispatch_in(m, off, (u_int8_t)nxt, 0);
		} else {
			m_freem(m);
		}
		m = NULL;
	}
done:
	if (sav) {
		KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
		    printf("DP ah4_input call free SA:0x%llx\n",
		    (uint64_t)VM_KERNEL_ADDRPERM(sav)));
		key_freesav(sav, KEY_SADB_UNLOCKED);
	}
	IPSEC_STAT_INCREMENT(ipsecstat.in_success);
	return;

fail:
	if (sav) {
		KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
		    printf("DP ah4_input call free SA:0x%llx\n",
		    (uint64_t)VM_KERNEL_ADDRPERM(sav)));
		key_freesav(sav, KEY_SADB_UNLOCKED);
	}
	if (m) {
		m_freem(m);
	}
	return;
}
#endif /* INET */

int
ah6_input(struct mbuf **mp, int *offp, int proto)
{
#pragma unused(proto)
	union sockaddr_in_4_6 src = {};
	union sockaddr_in_4_6 dst = {};
	struct mbuf *m = *mp;
	int off = *offp;
	struct ip6_hdr *ip6 = NULL;
	struct ah *ah = NULL;
	u_int32_t spi = 0;
	const struct ah_algorithm *algo = NULL;
	size_t siz = 0;
	size_t siz1 = 0;
	u_char *__bidi_indexable cksum = NULL;
	struct secasvar *sav = NULL;
	u_int16_t nxt = IPPROTO_DONE;
	size_t stripsiz = 0;
	sa_family_t ifamily = AF_UNSPEC;

	IP6_EXTHDR_CHECK(m, off, sizeof(struct ah), {return IPPROTO_DONE;});
	ah = (struct ah *)(void *)(mtod(m, caddr_t) + off);
	/* Expect 32-bit aligned data pointer on strict-align platforms */
	MBUF_STRICT_DATA_ALIGNMENT_CHECK_32(m);

	ip6 = mtod(m, struct ip6_hdr *);
	nxt = ah->ah_nxt;

	/* find the sassoc.  */
	spi = ah->ah_spi;

	if (ntohs(ip6->ip6_plen) == 0) {
		ipseclog((LOG_ERR, "IPv6 AH input: "
		    "AH with IPv6 jumbogram is not supported.\n"));
		IPSEC_STAT_INCREMENT(ipsec6stat.in_inval);
		goto fail;
	}

	ipsec_fill_ip6_sockaddr_4_6(&src, &ip6->ip6_src, 0);
	ipsec_fill_ip6_sockaddr_4_6_with_ifscope(&dst, &ip6->ip6_dst, 0,
	    ip6_input_getsrcifscope(m));

	if ((sav = key_allocsa(&src, &dst, IPPROTO_AH, spi, NULL)) == 0) {
		ipseclog((LOG_WARNING,
		    "IPv6 AH input: no key association found for spi %u\n",
		    (u_int32_t)ntohl(spi)));
		IPSEC_STAT_INCREMENT(ipsec6stat.in_nosa);
		goto fail;
	}
	KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
	    printf("DP ah6_input called to allocate SA:0x%llx\n",
	    (uint64_t)VM_KERNEL_ADDRPERM(sav)));
	if (sav->state != SADB_SASTATE_MATURE
	    && sav->state != SADB_SASTATE_DYING) {
		ipseclog((LOG_DEBUG,
		    "IPv6 AH input: non-mature/dying SA found for spi %u; ",
		    (u_int32_t)ntohl(spi)));
		IPSEC_STAT_INCREMENT(ipsec6stat.in_badspi);
		goto fail;
	}

	algo = ah_algorithm_lookup(sav->alg_auth);
	if (!algo) {
		ipseclog((LOG_DEBUG, "IPv6 AH input: "
		    "unsupported authentication algorithm for spi %u\n",
		    (u_int32_t)ntohl(spi)));
		IPSEC_STAT_INCREMENT(ipsec6stat.in_badspi);
		goto fail;
	}

	siz = (*algo->sumsiz)(sav);
	siz1 = ((siz + 3) & ~(4 - 1));

	/*
	 * sanity checks for header, 1.
	 */
	{
		int sizoff;

		sizoff = (sav->flags & SADB_X_EXT_OLD) ? 0 : 4;

		/*
		 * Here, we do not do "siz1 == siz".  See ah4_input() for complete
		 * description.
		 */
		if (siz1 < siz) {
			ipseclog((LOG_NOTICE, "sum length too short in IPv6 AH input "
			    "(%u, should be at least %u): %s\n",
			    (u_int32_t)siz1, (u_int32_t)siz,
			    ipsec6_logpacketstr(ip6, spi)));
			IPSEC_STAT_INCREMENT(ipsec6stat.in_inval);
			goto fail;
		}
		if ((ah->ah_len << 2) - sizoff != siz1) {
			ipseclog((LOG_NOTICE, "sum length mismatch in IPv6 AH input "
			    "(%d should be %u): %s\n",
			    (ah->ah_len << 2) - sizoff, (u_int32_t)siz1,
			    ipsec6_logpacketstr(ip6, spi)));
			IPSEC_STAT_INCREMENT(ipsec6stat.in_inval);
			goto fail;
		}
		VERIFY((sizeof(struct ah) + sizoff + siz1) <= INT_MAX);
		IP6_EXTHDR_CHECK(m, off, (int)(sizeof(struct ah) + sizoff + siz1),
		    {goto fail;});
		ip6 = mtod(m, struct ip6_hdr *);
		ah = (struct ah *)(void *)(mtod(m, caddr_t) + off);
	}

	/*
	 * check for sequence number.
	 */
	if ((sav->flags & SADB_X_EXT_OLD) == 0 && sav->replay[0] != NULL) {
		if (ipsec_chkreplay(ntohl(((struct newah *)ah)->ah_seq), sav, 0)) {
			; /*okey*/
		} else {
			IPSEC_STAT_INCREMENT(ipsec6stat.in_ahreplay);
			ipseclog((LOG_WARNING,
			    "replay packet in IPv6 AH input: %s %s\n",
			    ipsec6_logpacketstr(ip6, spi),
			    ipsec_logsastr(sav)));
			goto fail;
		}
	}

	/*
	 * alright, it seems sane.  now we are going to check the
	 * cryptographic checksum.
	 */
	cksum = (u_char *)kalloc_data(siz1, Z_NOWAIT);
	if (!cksum) {
		ipseclog((LOG_DEBUG, "IPv6 AH input: "
		    "couldn't alloc temporary region for cksum\n"));
		IPSEC_STAT_INCREMENT(ipsec6stat.in_inval);
		goto fail;
	}

	if (ah6_calccksum(m, (caddr_t)cksum, siz1, algo, sav)) {
		kfree_data(cksum, siz1);
		IPSEC_STAT_INCREMENT(ipsec6stat.in_inval);
		goto fail;
	}
	IPSEC_STAT_INCREMENT(ipsec6stat.in_ahhist[sav->alg_auth]);

	{
		caddr_t sumpos = NULL;

		if (sav->flags & SADB_X_EXT_OLD) {
			/* RFC 1826 */
			sumpos = (caddr_t)(ah + 1);
		} else {
			/* RFC 2402 */
			sumpos = (caddr_t)(((struct newah *)ah) + 1);
		}

		if (bcmp(sumpos, cksum, siz) != 0) {
			ipseclog((LOG_WARNING,
			    "checksum mismatch in IPv6 AH input: %s %s\n",
			    ipsec6_logpacketstr(ip6, spi), ipsec_logsastr(sav)));
			kfree_data(cksum, siz1);
			IPSEC_STAT_INCREMENT(ipsec6stat.in_ahauthfail);
			goto fail;
		}
	}

	kfree_data(cksum, siz1);

	m->m_flags |= M_AUTHIPHDR;
	m->m_flags |= M_AUTHIPDGM;

	if (m->m_flags & M_AUTHIPHDR && m->m_flags & M_AUTHIPDGM) {
		IPSEC_STAT_INCREMENT(ipsec6stat.in_ahauthsucc);
	} else {
		ipseclog((LOG_WARNING,
		    "authentication failed in IPv6 AH input: %s %s\n",
		    ipsec6_logpacketstr(ip6, spi), ipsec_logsastr(sav)));
		IPSEC_STAT_INCREMENT(ipsec6stat.in_ahauthfail);
		goto fail;
	}

	/*
	 * update sequence number.
	 */
	if ((sav->flags & SADB_X_EXT_OLD) == 0 && sav->replay[0] != NULL) {
		if (ipsec_updatereplay(ntohl(((struct newah *)ah)->ah_seq), sav, 0)) {
			IPSEC_STAT_INCREMENT(ipsec6stat.in_ahreplay);
			goto fail;
		}
	}

	/* was it transmitted over the IPsec tunnel SA? */
	if (sav->flags & SADB_X_EXT_OLD) {
		/* RFC 1826 */
		stripsiz = sizeof(struct ah) + siz1;
	} else {
		/* RFC 2402 */
		stripsiz = sizeof(struct newah) + siz1;
	}
	if (ipsec6_tunnel_validate(m, (int)(off + stripsiz), nxt, sav, &ifamily)) {
		ifaddr_t ifa;
		struct sockaddr_storage addr;
		struct sockaddr_in6 *ip6addr;
		/*
		 * strip off all the headers that precedes AH.
		 *	IP6 xx AH IP6' payload -> IP6' payload
		 *
		 * XXX more sanity checks
		 * XXX relationship with gif?
		 */
		u_int32_t flowinfo;     /*net endian*/

		if (ifamily == AF_INET) {
			ipseclog((LOG_NOTICE, "ipsec tunnel protocol mismatch "
			    "in IPv6 AH input: %s\n", ipsec_logsastr(sav)));
			goto fail;
		}

		flowinfo = ip6->ip6_flow;
		m_adj(m, (int)(off + stripsiz));
		if (m->m_len < sizeof(*ip6)) {
			/*
			 * m_pullup is prohibited in KAME IPv6 input processing
			 * but there's no other way!
			 */
			m = m_pullup(m, sizeof(*ip6));
			if (!m) {
				IPSEC_STAT_INCREMENT(ipsec6stat.in_inval);
				goto fail;
			}
		}
		ip6 = mtod(m, struct ip6_hdr *);
		/* ECN consideration. */
		if (ip6_ecn_egress(ip6_ipsec_ecn, &flowinfo, &ip6->ip6_flow) == 0) {
			IPSEC_STAT_INCREMENT(ipsec6stat.in_inval);
			goto fail;
		}
		if (!key_checktunnelsanity(sav, AF_INET6,
		    (caddr_t)&ip6->ip6_src, (caddr_t)&ip6->ip6_dst)) {
			ipseclog((LOG_NOTICE, "ipsec tunnel address mismatch "
			    "in IPv6 AH input: %s %s\n",
			    ipsec6_logpacketstr(ip6, spi),
			    ipsec_logsastr(sav)));
			IPSEC_STAT_INCREMENT(ipsec6stat.in_inval);
			goto fail;
		}

		/*
		 * should the inner packet be considered authentic?
		 * see comment in ah4_input().
		 */
		m->m_flags &= ~M_AUTHIPHDR;
		m->m_flags &= ~M_AUTHIPDGM;

		key_sa_recordxfer(sav, m->m_pkthdr.len);
		if (ipsec_incr_history_count(m, IPPROTO_AH, spi) != 0 ||
		    ipsec_incr_history_count(m, IPPROTO_IPV6, 0) != 0) {
			IPSEC_STAT_INCREMENT(ipsec6stat.in_nomem);
			goto fail;
		}

		bzero(&addr, sizeof(addr));
		ip6addr = (__typeof__(ip6addr)) & addr;
		ip6addr->sin6_family = AF_INET6;
		ip6addr->sin6_len = sizeof(*ip6addr);
		ip6addr->sin6_addr = ip6->ip6_dst;

		// update the receiving interface address based on the inner address
		ifa = ifa_ifwithaddr((struct sockaddr *)&addr);
		if (ifa) {
			m->m_pkthdr.rcvif = ifa->ifa_ifp;
			ifa_remref(ifa);
		}

		// Input via IPsec interface
		lck_mtx_lock(sadb_mutex);
		ifnet_t ipsec_if = sav->sah->ipsec_if;
		if (ipsec_if != NULL) {
			// If an interface is found, add a reference count before dropping the lock
			ifnet_reference(ipsec_if);
		}
		lck_mtx_unlock(sadb_mutex);
		if (ipsec_if != NULL) {
			errno_t inject_error = ipsec_inject_inbound_packet(ipsec_if, m);
			ifnet_release(ipsec_if);
			if (inject_error == 0) {
				m = NULL;
				nxt = IPPROTO_DONE;
				goto done;
			} else {
				goto fail;
			}
		}

		if (proto_input(PF_INET6, m) != 0) {
			goto fail;
		}
		nxt = IPPROTO_DONE;
	} else {
		/*
		 * strip off AH.
		 */
		char *prvnxtp;

		/*
		 * Copy the value of the next header field of AH to the
		 * next header field of the previous header.
		 * This is necessary because AH will be stripped off below.
		 */
		prvnxtp = ip6_get_prevhdr(m, off); /* XXX */
		*prvnxtp = (u_int8_t)nxt;

		ip6 = mtod(m, struct ip6_hdr *);
		/*
		 * We do deep-copy since KAME requires that
		 * the packet is placed in a single mbuf.
		 */
		ovbcopy((caddr_t)ip6, ((caddr_t)ip6) + stripsiz, off);
		m->m_data += stripsiz;
		m->m_len -= stripsiz;
		m->m_pkthdr.len -= stripsiz;
		ip6 = mtod(m, struct ip6_hdr *);
		/* XXX jumbogram */
		ip6->ip6_plen = htons((u_int16_t)(ntohs(ip6->ip6_plen) - stripsiz));

		key_sa_recordxfer(sav, m->m_pkthdr.len);
		if (ipsec_incr_history_count(m, IPPROTO_AH, spi) != 0) {
			IPSEC_STAT_INCREMENT(ipsec6stat.in_nomem);
			goto fail;
		}

		// Input via IPsec interface
		lck_mtx_lock(sadb_mutex);
		ifnet_t ipsec_if = sav->sah->ipsec_if;
		if (ipsec_if != NULL) {
			// If an interface is found, add a reference count before dropping the lock
			ifnet_reference(ipsec_if);
		}
		lck_mtx_unlock(sadb_mutex);
		if (ipsec_if != NULL) {
			errno_t inject_error = ipsec_inject_inbound_packet(ipsec_if, m);
			ifnet_release(ipsec_if);
			if (inject_error == 0) {
				m = NULL;
				nxt = IPPROTO_DONE;
				goto done;
			} else {
				goto fail;
			}
		}
	}

done:
	*offp = off;
	*mp = m;
	if (sav) {
		KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
		    printf("DP ah6_input call free SA:0x%llx\n",
		    (uint64_t)VM_KERNEL_ADDRPERM(sav)));
		key_freesav(sav, KEY_SADB_UNLOCKED);
	}
	IPSEC_STAT_INCREMENT(ipsec6stat.in_success);
	return nxt;

fail:
	if (sav) {
		KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
		    printf("DP ah6_input call free SA:0x%llx\n",
		    (uint64_t)VM_KERNEL_ADDRPERM(sav)));
		key_freesav(sav, KEY_SADB_UNLOCKED);
	}
	if (m) {
		m_freem(m);
		*mp = NULL;
	}
	return IPPROTO_DONE;
}

void
ah6_ctlinput(int cmd, struct sockaddr *sa, void *d)
{
	union sockaddr_in_4_6 src = {};
	union sockaddr_in_4_6 dst = {};
	const struct newah *ahp;
	struct newah ah;
	struct secasvar *sav;
	struct ip6_hdr *ip6;
	struct mbuf *m;
	struct ip6ctlparam *ip6cp = NULL;
	struct sockaddr_in6 *sa6_src, *sa6_dst;
	int off = 0;

	if (sa->sa_family != AF_INET6 ||
	    sa->sa_len != sizeof(struct sockaddr_in6)) {
		return;
	}
	if ((unsigned)cmd >= PRC_NCMDS) {
		return;
	}

	/* if the parameter is from icmp6, decode it. */
	if (d != NULL) {
		ip6cp = (struct ip6ctlparam *)d;
		m = ip6cp->ip6c_m;
		ip6 = ip6cp->ip6c_ip6;
		off = ip6cp->ip6c_off;
	} else {
		m = NULL;
		ip6 = NULL;
	}

	if (ip6) {
		/*
		 * XXX: We assume that when ip6 is non NULL,
		 * M and OFF are valid.
		 */

		/* check if we can safely examine src and dst ports */
		if (m->m_pkthdr.len < off + sizeof(ah)) {
			return;
		}

		if (m->m_len < off + sizeof(ah)) {
			/*
			 * this should be rare case,
			 * so we compromise on this copy...
			 */
			m_copydata(m, off, sizeof(ah), (caddr_t)&ah);
			ahp = &ah;
		} else {
			ahp = (struct newah *)(void *)(mtod(m, caddr_t) + off);
		}

		if (cmd == PRC_MSGSIZE) {
			int valid = 0;

			/*
			 * Check to see if we have a valid SA corresponding to
			 * the address in the ICMP message payload.
			 */
			sa6_src = ip6cp->ip6c_src;
			sa6_dst = SIN6(sa);
			ipsec_fill_ip6_sockaddr_4_6(&src, &sa6_src->sin6_addr, 0);
			ipsec_fill_ip6_sockaddr_4_6_with_ifscope(&dst,
			    &sa6_dst->sin6_addr, 0, sa6_dst->sin6_scope_id);

			sav = key_allocsa(&src, &dst, IPPROTO_AH, ahp->ah_spi, NULL);
			if (sav) {
				if (sav->state == SADB_SASTATE_MATURE ||
				    sav->state == SADB_SASTATE_DYING) {
					valid++;
				}
				key_freesav(sav, KEY_SADB_UNLOCKED);
			}

			/* XXX Further validation? */

			/*
			 * Depending on the value of "valid" and routing table
			 * size (mtudisc_{hi,lo}wat), we will:
			 * - recalcurate the new MTU and create the
			 *   corresponding routing entry, or
			 * - ignore the MTU change notification.
			 */
			icmp6_mtudisc_update((struct ip6ctlparam *)d, valid);
		}

		/* we normally notify single pcb here */
	} else {
		/* we normally notify any pcb here */
	}
}
