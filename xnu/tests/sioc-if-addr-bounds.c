/*
 * Copyright (c) 2020-2022 Apple Inc. All rights reserved.
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

/* -*- compile-command: "xcrun --sdk iphoneos.internal make sioc-if-addr-bounds" -*- */

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#include <net/if.h>
#include <net/if_dl.h>

#include <netinet/in.h>
#include <netinet/in_var.h>

#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>

#include <darwintest.h>
#include <darwintest_utils.h>

#include <uuid/uuid.h>

#include <ifaddrs.h>

#include <arpa/inet.h>

#include "ioc_str.h"

T_GLOBAL_META(T_META_NAMESPACE("xnu.net"));

#ifndef STRINGIFY
#define __STR(x)        #x              /* just a helper macro */
#define STRINGIFY(x)    __STR(x)
#endif /* STRINGIFY */

#define IF_NAME       "bridge"

/* On some platforms with DEBUG kernel, we need to wait a while */
#define SIFCREATE_RETRY 600

#define PATTERN_SIZE 8

static int
ifnet_destroy(int s, const char * ifname, bool fail_on_error)
{
	int             err;
	struct ifreq    ifr;

	bzero(&ifr, sizeof(ifr));
	strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	err = ioctl(s, SIOCIFDESTROY, &ifr);
	if (fail_on_error) {
		T_QUIET;
		T_ASSERT_POSIX_SUCCESS(err, "SIOCSIFDESTROY %s", ifr.ifr_name);
	}
	if (err < 0) {
		T_LOG("SIOCSIFDESTROY %s", ifr.ifr_name);
	}
	return err;
}

static int
ifnet_set_flags(int s, const char * ifname,
    uint16_t flags_set, uint16_t flags_clear)
{
	uint16_t        flags_after;
	uint16_t        flags_before;
	struct ifreq    ifr;
	int             ret;

	bzero(&ifr, sizeof(ifr));
	strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	ret = ioctl(s, SIOCGIFFLAGS, (caddr_t)&ifr);
	if (ret != 0) {
		T_LOG("SIOCGIFFLAGS %s", ifr.ifr_name);
		return ret;
	}
	flags_before = (uint16_t)ifr.ifr_flags;
	ifr.ifr_flags |= flags_set;
	ifr.ifr_flags &= ~(flags_clear);
	flags_after = (uint16_t)ifr.ifr_flags;
	if (flags_before == flags_after) {
		/* nothing to do */
		ret = 0;
	} else {
		/* issue the ioctl */
		T_QUIET;
		T_ASSERT_POSIX_SUCCESS(ioctl(s, SIOCSIFFLAGS, &ifr),
		    "SIOCSIFFLAGS %s 0x%x",
		    ifr.ifr_name, (uint16_t)ifr.ifr_flags);
	}
	return ret;
}

static int
ifnet_create(int s, char * ifname, size_t ifname_size)
{
	int error = 0;
	struct ifreq ifr;

	bzero(&ifr, sizeof(ifr));
	strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));

	for (int i = 0; i < SIFCREATE_RETRY; i++) {
		if (ioctl(s, SIOCIFCREATE, &ifr) < 0) {
			error = errno;
			T_LOG("SIOCSIFCREATE %s: %s", ifname,
			    strerror(error));
			if (error == EBUSY) {
				/* interface is tearing down, try again */
				usleep(10000);
			} else if (error == EEXIST) {
				/* interface exists, try destroying it */
				(void)ifnet_destroy(s, ifname, false);
			} else {
				/* unexpected failure */
				break;
			}
		} else {
			error = 0;
			break;
		}
	}
	if (error == 0) {
		/* Copy back the interface name with unit number */
		strlcpy(ifname, ifr.ifr_name, ifname_size);
		error = ifnet_set_flags(s, ifname, IFF_UP, 0);
	}
	return error;
}

#define MAXBUF 32

static void
HexDump(void *data, size_t len)
{
	size_t i, j, k;
	unsigned char *ptr = (unsigned char *)data;
	unsigned char buf[3 * MAXBUF + 1];

	for (i = 0; i < len; i += MAXBUF) {
		for (j = i, k = 0; j < i + MAXBUF && j < len; j++) {
			unsigned char msnbl = ptr[j] >> 4;
			unsigned char lsnbl = ptr[j] & 0x0f;

			buf[k++] = msnbl < 10 ? msnbl + '0' : msnbl + 'a' - 10;
			buf[k++] = lsnbl < 10 ? lsnbl + '0' : lsnbl + 'a' - 10;
			if ((j % 2) == 1) {
				buf[k++] = ' ';
			}
			if ((j % MAXBUF) == MAXBUF - 1) {
				buf[k++] = ' ';
			}
		}
		buf[k] = 0;
		T_LOG("%5zd: %s\n", i, buf);
	}
}

static size_t
snprint_dottedhex(char *str, size_t strsize, const void *data, const size_t datasize)
{
	size_t is = 0, ip = 0;
	const unsigned char *ptr = (const unsigned char *)data;

	for (is = 0, ip = 0; is + 3 < strsize - 1 && ip < datasize; ip++) {
		unsigned char msnbl = ptr[ip] >> 4;
		unsigned char lsnbl = ptr[ip] & 0x0f;

		if (ip > 0) {
			str[is++] = '.';
		}
		str[is++] = (char)(msnbl + (msnbl < 10 ? '0' : 'a' - 10));
		str[is++] = (char)(lsnbl + (lsnbl < 10 ? '0' : 'a' - 10));
	}
	str[is] = 0;
	return is;
}

static void
print_sockaddr_dl(const char *pre, const struct sockaddr *sa, const char *post)
{
	char nbuffer[256];
	char abuffer[256];
	char sbuffer[256];
	struct sockaddr_dl sdl = { 0 };

	if (sa == NULL) {
		return;
	}
	memcpy(&sdl, sa, MIN(sizeof(sdl), sa->sa_len));
	strlcpy(nbuffer, sdl.sdl_data, sdl.sdl_nlen);
	snprint_dottedhex(abuffer, sizeof(abuffer), sdl.sdl_data + sdl.sdl_nlen, sdl.sdl_alen);
	snprint_dottedhex(sbuffer, sizeof(sbuffer), sdl.sdl_data + sdl.sdl_nlen + sdl.sdl_alen, sdl.sdl_slen);

	T_LOG("%ssdl_len %u sdl_family %u sdl_index %u sdl_type %u sdl_nlen %u (%s) sdl_alen %u (%s) sdl_slen %u (%s)%s",
	    pre != NULL ? pre : "",
	    sdl.sdl_len, sdl.sdl_family, sdl.sdl_index, sdl.sdl_type,
	    sdl.sdl_nlen, nbuffer, sdl.sdl_alen, abuffer, sdl.sdl_slen, sbuffer,
	    post != NULL ? post : "");
}

static void
print_sockaddr_in(const char *pre, const struct sockaddr *sa, const char *post)
{
	char abuffer[256];
	char zbuffer[256];
	struct sockaddr_in sin = { 0 };

	if (sa == NULL) {
		return;
	}

	memcpy(&sin, sa, MIN(sizeof(sin), sa->sa_len));
	inet_ntop(AF_INET, &sin.sin_addr, abuffer, sizeof(abuffer));
	snprint_dottedhex(zbuffer, sizeof(zbuffer), sin.sin_zero, sizeof(sin.sin_zero));

	T_LOG("%ssin_len %u sin_family %u sin_port %u sin_addr %s sin_zero %s%s",
	    pre != NULL ? pre : "",
	    sin.sin_len, sin.sin_family, htons(sin.sin_port), abuffer, zbuffer,
	    post != NULL ? post : "");
}

static void
print_sockaddr_in6(const char *pre, const struct sockaddr *sa, const char *post)
{
	char abuffer[256];
	struct sockaddr_in6 sin6 = { 0 };

	if (sa == NULL) {
		return;
	}

	memcpy(&sin6, sa, MIN(sizeof(sin6), sa->sa_len));
	inet_ntop(AF_INET6, &sin6.sin6_addr, abuffer, sizeof(abuffer));

	T_LOG("%ssin6_len %u sin6_family %u sin6_port %u sin6_flowinfo %u sin6_addr %s sin6_scope_id %u%s",
	    pre != NULL ? pre : "",
	    sin6.sin6_len, sin6.sin6_family, htons(sin6.sin6_port), sin6.sin6_flowinfo, abuffer, sin6.sin6_scope_id,
	    post != NULL ? post : "");
}

static void
print_sockaddr(const char *pre, const struct sockaddr *sa, const char *post)
{
	char buffer[256];

	if (sa == NULL) {
		return;
	}

	snprint_dottedhex(buffer, sizeof(buffer), sa->sa_data, sa->sa_len - 2);

	T_LOG("%ssa_len %u sa_family %u sa_data %s%s",
	    pre != NULL ? pre : "",
	    sa->sa_len, sa->sa_family, buffer,
	    post != NULL ? post : "");
}


#define ROUNDUP(a, size) (((a) & ((size) - 1)) ? (1 + ((a)|(size - 1))) : (a))

#define NEXT_SA(p) (struct sockaddr *) \
    ((caddr_t)p + (p->sa_len ? ROUNDUP(p->sa_len, sizeof(u_int32_t)) : \
     sizeof(u_long)))

static size_t
get_rti_info(int addrs, struct sockaddr *sa, struct sockaddr **rti_info)
{
	int i;
	size_t len = 0;

	for (i = 0; i < RTAX_MAX; i++) {
		if (addrs & (1 << i)) {
			rti_info[i] = sa;
			if (sa->sa_len < sizeof(struct sockaddr)) {
				len += sizeof(struct sockaddr);
			} else {
				len += sa->sa_len;
			}
			sa = NEXT_SA(sa);
		} else {
			rti_info[i] = NULL;
		}
	}
	return len;
}

static void
print_address(const char *pre, const struct sockaddr *sa, const char *post, u_char asFamily)
{
	if (sa == NULL) {
		T_LOG("%s(NULL)%s",
		    pre != NULL ? pre : "",
		    post != NULL ? post : "");
		return;
	}
	if (sa->sa_len == 0) {
		T_LOG("%ssa_len 0%s",
		    pre != NULL ? pre : "",
		    post != NULL ? post : "");
		return;
	}
	if (sa->sa_len == 1) {
		T_LOG("%ssa_len 1%s",
		    pre != NULL ? pre : "",
		    post != NULL ? post : "");
		return;
	}

	// If not forced
	if (asFamily == AF_UNSPEC) {
		asFamily = sa->sa_family;
	}
	switch (asFamily) {
	case AF_INET: {
		print_sockaddr_in(pre, sa, post);
		break;
	}
	case AF_INET6: {
		print_sockaddr_in6(pre, sa, post);
		break;
	}
	case AF_LINK: {
		print_sockaddr_dl(pre, sa, post);
		break;
	}
	default:
		print_sockaddr(pre, sa, post);
		break;
	}
}

static void
print_rti_info(struct sockaddr *rti_info[])
{
	struct sockaddr *sa;
	u_char asFamily = 0;

	if ((sa = rti_info[RTAX_IFA])) {
		asFamily = sa->sa_family;
		print_address(" RTAX_IFA         ", sa, "\n", 0);
	}
	if ((sa = rti_info[RTAX_DST])) {
		asFamily = sa->sa_family;
		print_address(" RTAX_DST         ", sa, "\n", 0);
	}
	if ((sa = rti_info[RTAX_BRD])) {
		print_address(" RTAX_BRD         ", sa, "\n", asFamily);
	}

	if ((sa = rti_info[RTAX_NETMASK])) {
		print_address(" RTAX_NETMASK     ", sa, "\n", asFamily);
	}

	if ((sa = rti_info[RTAX_GATEWAY])) {
		print_address(" RTAX_GATEWAY     ", sa, "\n", 0);
	}

	if ((sa = rti_info[RTAX_GENMASK])) {
		print_address(" RTAX_GENMASK     ", sa, "\n", asFamily);
	}

	if ((sa = rti_info[RTAX_AUTHOR])) {
		print_address(" RTAX_AUTHOR      ", sa, "\n", asFamily);
	}

	if ((sa = rti_info[RTAX_IFP])) {
		print_address(" RTAX_IFP         ", sa, "\n", 0);
	}
}

static void
print_rt_iflist2(const char *label)
{
	size_t len;
	int mib[6] = { CTL_NET, PF_ROUTE, 0, 0, NET_RT_IFLIST2, 0 };
	unsigned char *buf = NULL;
	unsigned char *lim, *next;
	struct if_msghdr *ifmsg;

	T_LOG("interface address list for %s", label);

	T_QUIET; T_EXPECT_POSIX_SUCCESS(sysctl(mib, 6, NULL, &len, NULL, 0), "sysctl NET_RT_IFLIST2");

	T_QUIET; T_ASSERT_NOTNULL(buf = calloc(1, len), "rt_if_list_buf calloc(1, %zd)", len);

	T_QUIET; T_EXPECT_POSIX_SUCCESS(sysctl(mib, 6, buf, &len, NULL, 0), "sysctl NET_RT_IFLIST2");

	lim = buf + len;
	for (next = buf; next < lim; next += ifmsg->ifm_msglen) {
		ifmsg = (struct if_msghdr *)(void *)next;
		char ifname[IF_NAMESIZE + 1];

		if (ifmsg->ifm_type == RTM_IFINFO2) {
			struct if_msghdr2 *ifm = (struct if_msghdr2 *)ifmsg;
			struct sockaddr *sa = (struct sockaddr *)(ifm + 1);

			(void)if_indextoname(ifm->ifm_index, ifname);
			T_LOG("interface: %s", ifname);
			print_address(" PRIMARY          ", sa, "", 0);
		} else if (ifmsg->ifm_type == RTM_NEWADDR) {
			struct sockaddr *rti_info[RTAX_MAX];
			struct ifa_msghdr *ifam = (struct ifa_msghdr *)ifmsg;

			(void) get_rti_info(ifam->ifam_addrs, (struct sockaddr *)(ifam + 1), rti_info);

			print_rti_info(rti_info);
		}
	}
	free(buf);
}

static int
check_rt_if_list_for_pattern(const char *label, unsigned char pattern, size_t pattern_size)
{
	size_t i;
	size_t len;
	int mib[6] = { CTL_NET, PF_ROUTE, 0, 0, NET_RT_IFLIST, 0 };
	unsigned char *rt_if_list_buf = NULL;
	int count = 0;
	unsigned char *pattern_buf = NULL;

	T_QUIET; T_ASSERT_NOTNULL(pattern_buf = calloc(1, pattern_size), "pattern_buf calloc(1, %zd)", pattern_size);
	memset(pattern_buf, pattern, pattern_size);

	T_QUIET; T_EXPECT_POSIX_SUCCESS(sysctl(mib, 6, NULL, &len, NULL, 0), "sysctl NET_RT_IFLIST");

	T_QUIET; T_ASSERT_NOTNULL(rt_if_list_buf = calloc(1, len), "rt_if_list_buf calloc(1, %zd)", len);

	T_QUIET; T_EXPECT_POSIX_SUCCESS(sysctl(mib, 6, rt_if_list_buf, &len, NULL, 0), "sysctl NET_RT_IFLIST");

	if (label != NULL) {
		T_LOG("%s sysctl NET_RT_IFLIST buffer length %zd\n", label, len);
	}

	count = 0;
	for (i = 0; i < len - pattern_size; i++) {
		if (memcmp(rt_if_list_buf + i, pattern_buf, pattern_size) == 0) {
			count++;
			i += pattern_size - 1;
		}
	}

	if (label != NULL) {
		if (label != NULL && count > 0) {
			T_LOG("%s found pattern at %zd count %d times\n", label, i, count);
			HexDump(rt_if_list_buf, len);
		}
	}

	free(rt_if_list_buf);
	free(pattern_buf);

	return count;
}

static bool
find_unused_pattern_in_rt_if_list(unsigned char *pattern, size_t pattern_size)
{
	bool found_pattern = false;
	unsigned char i;
	unsigned char *pattern_buf = NULL;

	T_QUIET; T_ASSERT_NOTNULL(pattern_buf = calloc(1, pattern_size), "pattern_buf calloc(1, %zd)", pattern_size);

	/* Try 10 times to find an unused pattern */
	for (i = 1; i < 255; i++) {
		if (check_rt_if_list_for_pattern(NULL, i, pattern_size) == 0) {
			found_pattern = true;
			*pattern = i;
			memset(pattern_buf, i, pattern_size);
			T_LOG("PATTERN: ");
			HexDump(pattern_buf, pattern_size);
			break;
		}
	}
	free(pattern_buf);

	return found_pattern;
}

static const char *
ioc_str(unsigned long cmd)
{
	switch (cmd) {
#define X(a) case a: return #a;
		SIOC_LIST
#undef X
	default:
		break;
	}
	return "";
}

struct ioc_ifreq {
	unsigned long ioc_cmd;
	uint8_t salen;
	uint8_t safamily;
	const char *sastr;
	int error; // 0 means no error, -1, end of list, otherwise expected errno
};

static struct ioc_ifreq ioc_list[] = {
	{ SIOCSIFADDR, sizeof(struct sockaddr_in), AF_INET6, "10.2.3.1", EINVAL },
	{ SIOCDIFADDR, sizeof(struct sockaddr_in), AF_INET6, "10.2.3.1", EADDRNOTAVAIL },
	{ SIOCDIFADDR, sizeof(struct sockaddr_in6), AF_INET, "10.2.3.1", EADDRNOTAVAIL },
	{ SIOCDIFADDR, sizeof(struct sockaddr_in), AF_INET, "10.2.3.1", EADDRNOTAVAIL },

	{ SIOCSIFADDR, 0xf0, AF_INET6, "10.2.3.1", EINVAL },
	{ SIOCDIFADDR, 0xf0, AF_INET6, "10.2.3.1", EADDRNOTAVAIL },
	{ SIOCDIFADDR, 0xf0, AF_INET, "10.2.3.1", EADDRNOTAVAIL },
	{ SIOCDIFADDR, 0xf0, AF_INET, "10.2.3.1", EADDRNOTAVAIL },

	{ SIOCSIFADDR, 0, AF_INET6, "10.2.3.1", EINVAL },
	{ SIOCDIFADDR, 0, AF_INET6, "10.2.3.1", EADDRNOTAVAIL },
	{ SIOCDIFADDR, 0, AF_INET, "10.2.3.1", EADDRNOTAVAIL },
	{ SIOCDIFADDR, 0, AF_INET, "10.2.3.1", EADDRNOTAVAIL },

	{ SIOCSIFADDR, sizeof(struct sockaddr_in6), AF_INET, "10.2.3.2", 0 },
	{ SIOCDIFADDR, sizeof(struct sockaddr_in), AF_INET6, "10.2.3.2", 0 },

	{ SIOCSIFADDR, sizeof(struct sockaddr_in), AF_INET, "10.2.3.3", 0 },
	{ SIOCDIFADDR, sizeof(struct sockaddr_in6), AF_INET, "10.2.3.3", 0 },

	{ SIOCSIFADDR, sizeof(struct sockaddr_in6), AF_INET6, "10.2.3.4", EINVAL },
	{ SIOCDIFADDR, sizeof(struct sockaddr_in6), AF_INET, "10.2.3.4", EADDRNOTAVAIL },

	{ SIOCSIFADDR, sizeof(struct sockaddr_in), AF_INET, "10.2.3.5", 0 },
	{ SIOCDIFADDR, sizeof(struct sockaddr_in), AF_INET, "0.0.0.0", EADDRNOTAVAIL },
	{ SIOCDIFADDR, sizeof(struct sockaddr_in), AF_INET, "10.2.3.5", 0 },

	{ SIOCSIFADDR, sizeof(struct sockaddr_in), AF_INET, "10.2.3.6", 0 },

	{ SIOCSIFNETMASK, sizeof(struct sockaddr_in), 0, "ff.00.00.00", 0 },
	{ SIOCSIFNETMASK, sizeof(struct sockaddr_in), AF_INET, "ff.00.00.00", 0 },
	{ SIOCSIFNETMASK, sizeof(struct sockaddr_in), AF_INET6, "ff.f.00.00", 0 },

	{ SIOCSIFNETMASK, sizeof(struct sockaddr_in6), 0, "ff.ff.00.00", 0 },
	{ SIOCSIFNETMASK, sizeof(struct sockaddr_in6), AF_INET, "ff.ff.00.00", 0 },
	{ SIOCSIFNETMASK, sizeof(struct sockaddr_in6), AF_INET6, "ff.ff.f0.00", 0 },

	{ SIOCSIFNETMASK, 0, 0, "ff.ff.00.00", 0 },
	{ SIOCSIFNETMASK, 0, AF_INET, "ff.ff.00.00", 0 },
	{ SIOCSIFNETMASK, 0, AF_INET6, "ff.ff.f0.00", 0 },

	{ SIOCSIFNETMASK, 0xf0, 0, "ff.ff.00.00", 0 },
	{ SIOCSIFNETMASK, 0xf0, AF_INET, "ff.ff.00.00", 0 },
	{ SIOCSIFNETMASK, 0xf0, AF_INET6, "ff.ff.f0.00", 0 },

	{ SIOCSIFBRDADDR, sizeof(struct sockaddr_in), 0, "10.255.255.255", 0 },
	{ SIOCSIFBRDADDR, sizeof(struct sockaddr_in), AF_INET, "10.255.255.255", 0 },
	{ SIOCSIFBRDADDR, sizeof(struct sockaddr_in), AF_INET6, "10.255.255.255", 0 },
	{ SIOCSIFBRDADDR, sizeof(struct sockaddr_in6), AF_INET, "10.255.255.255", 0 },

	{ SIOCSIFBRDADDR, 0xf0, 0, "10.255.255.255", 0 },
	{ SIOCSIFBRDADDR, 0xf0, AF_INET, "10.255.255.255", 0 },
	{ SIOCSIFBRDADDR, 0xf0, AF_INET6, "10.255.255.255", 0 },
	{ SIOCSIFBRDADDR, 0xf0, AF_INET, "10.255.255.255", 0 },

	{ SIOCSIFBRDADDR, 0, 0, "10.255.255.255", 0 },
	{ SIOCSIFBRDADDR, 0, AF_INET, "10.255.255.255", 0 },
	{ SIOCSIFBRDADDR, 0, AF_INET6, "10.255.255.255", 0 },
	{ SIOCSIFBRDADDR, 0, AF_INET, "10.255.255.255", 0 },

	{ 0, 0, 0, "", -1 },
};

static void
test_sioc_ifr_bounds(struct ioc_ifreq *ioc_ifreq, int s, const char *ifname)
{
	struct ifreq ifr = { 0 };
	unsigned char pattern;
	struct sockaddr_in *sin;

	T_LOG("");
	T_LOG("TEST CASE: %s ioctl(%s, sa_len %u, sa_family %u, %s) -> %d", __func__,
	    ioc_str(ioc_ifreq->ioc_cmd), ioc_ifreq->salen, ioc_ifreq->safamily, ioc_ifreq->sastr, ioc_ifreq->error);


	if (find_unused_pattern_in_rt_if_list(&pattern, PATTERN_SIZE) == false) {
		T_SKIP("Could not find unused pattern");
	}

	strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	memset(&ifr.ifr_addr.sa_data, pattern, sizeof(ifr.ifr_dstaddr.sa_data));

	sin = (struct sockaddr_in *)(void *)&ifr.ifr_addr;
	sin->sin_len = ioc_ifreq->salen;
	sin->sin_family = ioc_ifreq->safamily;
	sin->sin_addr.s_addr = inet_addr(ioc_ifreq->sastr);

	int retval;
	if (ioc_ifreq->error == 0) {
		T_EXPECT_POSIX_SUCCESS(retval = ioctl(s, ioc_ifreq->ioc_cmd, &ifr),
		    "%s, %s: retval %d", ioc_str(ioc_ifreq->ioc_cmd), ioc_ifreq->sastr, retval);
	} else {
		T_EXPECT_POSIX_FAILURE(retval = ioctl(s, ioc_ifreq->ioc_cmd, &ifr), ioc_ifreq->error,
		    "%s, %s: retval %d errno %s", ioc_str(ioc_ifreq->ioc_cmd), ioc_ifreq->sastr, retval, strerror(errno));
	}

	T_EXPECT_EQ(check_rt_if_list_for_pattern("test_sioc_ifr_bounds", pattern, PATTERN_SIZE), 0, "pattern should not be found");


	fflush(stdout);
	fflush(stderr);
}

T_DECL(sioc_ifr_bounds, "test bound checks on struct ifreq addresses passed to interface ioctls",
    T_META_ASROOT(true), T_META_TAG_VM_PREFERRED)
{
	int s = -1;
	char ifname[IFNAMSIZ];

	T_LOG("%s", __func__);

	T_QUIET; T_EXPECT_POSIX_SUCCESS(s = socket(AF_INET, SOCK_DGRAM, 0), "socket");

	strlcpy(ifname, IF_NAME, sizeof(ifname));

	int error = 0;
	if ((error = ifnet_create(s, ifname, sizeof(ifname))) != 0) {
		if (error == EINVAL) {
			T_SKIP("The system does not support the %s cloning interface", IF_NAME);
		}
		T_SKIP("This test failed creating a %s cloning interface", IF_NAME);
	}
	T_LOG("created clone interface '%s'", ifname);

	struct ioc_ifreq *ioc_ifreq;
	for (ioc_ifreq = ioc_list; ioc_ifreq->error != -1; ioc_ifreq++) {
		test_sioc_ifr_bounds(ioc_ifreq, s, ifname);
	}
	print_rt_iflist2(__func__);
	(void)ifnet_destroy(s, ifname, true);

	close(s);
}

struct ioc_ifra {
	const char *description;

	uint8_t addr_len;
	uint8_t addr_fam;
	const char *addr_str;

	uint8_t broad_len;
	uint8_t broad_fam;
	const char *broad_str;

	uint8_t mask_len;
	uint8_t mask_fam;
	const char *mask_str;

	int error; // 0 means no error, -1, end of list, otherwise expected errno
};

static struct ioc_ifra ioc_ifra_list[] = {
	{
		.description = "fully formed",
		.addr_len = sizeof(struct sockaddr_in), .addr_fam = AF_INET, .addr_str = "10.1.1.1",
		.broad_len = sizeof(struct sockaddr_in), .broad_fam = AF_INET, .broad_str = "1.1.1.255",
		.mask_len = sizeof(struct sockaddr_in), .mask_fam = AF_INET, .mask_str = "255.255.0.0",
		.error = 0
	},
	{
		.description = "addr_len 0",
		.addr_len = 0, .addr_fam = AF_INET, .addr_str = "10.2.2.0",
		.broad_len = sizeof(struct sockaddr_in), .broad_fam = AF_INET, .broad_str = "10.2.2.255",
		.mask_len = sizeof(struct sockaddr_in), .mask_fam = AF_INET, .mask_str = "255.0.0.0",
		.error = 0
	},
	{
		.description = "addr_len 1",
		.addr_len = 1, .addr_fam = AF_INET, .addr_str = "10.2.2.1",
		.broad_len = sizeof(struct sockaddr_in), .broad_fam = AF_INET, .broad_str = "10.2.2.255",
		.mask_len = sizeof(struct sockaddr_in), .mask_fam = AF_INET, .mask_str = "255.0.0.0",
		.error = 0
	},
	{
		.description = "addr_len 250",
		.addr_len = 250, .addr_fam = AF_INET, .addr_str = "10.2.2.250",
		.broad_len = sizeof(struct sockaddr_in), .broad_fam = AF_INET, .broad_str = "10.2.2.255",
		.mask_len = sizeof(struct sockaddr_in), .mask_fam = AF_INET, .mask_str = "255.0.0.0",
		.error = 0
	},
	{
		.description = "addr_family AF_INET6",
		.addr_len = sizeof(struct sockaddr_in), .addr_fam = AF_INET6, .addr_str = "10.3.3.3",
		.broad_len = sizeof(struct sockaddr_in), .broad_fam = AF_INET, .broad_str = "10.3.255.255",
		.mask_len = sizeof(struct sockaddr_in), .mask_fam = AF_INET, .mask_str = "255.255.255.0",
		.error = EINVAL
	},
	{
		.description = "broadcast_len 0xf0",
		.addr_len = sizeof(struct sockaddr_in), .addr_fam = AF_INET, .addr_str = "10.4.4.4",
		.broad_len = 0xf0, .broad_fam = AF_INET, .broad_str = "10.4.4.255",
		.mask_len = sizeof(struct sockaddr_in), .mask_fam = AF_INET, .mask_str = "255.255.255.0",
		.error = 0
	},
	{
		.description = "broadcast_family AF_INET6",
		.addr_len = sizeof(struct sockaddr_in), .addr_fam = AF_INET, .addr_str = "10.5.5.5",
		.broad_len = sizeof(struct sockaddr_in), .broad_fam = AF_INET6, .broad_str = "10.5.5.255",
		.mask_len = sizeof(struct sockaddr_in), .mask_fam = AF_INET, .mask_str = "255.255.0.0",
		.error = 0
	},
	{
		.description = "mask_len 0xf0",
		.addr_len = sizeof(struct sockaddr_in), .addr_fam = AF_INET, .addr_str = "10.6.6.6",
		.broad_len = sizeof(struct sockaddr_in), .broad_fam = AF_INET, .broad_str = "1.6.6.255",
		.mask_len = 0xf0, .mask_fam = AF_INET, .mask_str = "255.255.0.0",
		.error = 0
	},
	{
		.description = "mask_family AF_INET6",
		.addr_len = sizeof(struct sockaddr_in), .addr_fam = AF_INET, .addr_str = "10.7.7.7",
		.broad_len = sizeof(struct sockaddr_in), .broad_fam = AF_INET, .broad_str = "10.7.7.255",
		.mask_len = sizeof(struct sockaddr_in), .mask_fam = AF_INET6, .mask_str = "255.255.0.0",
		.error = 0
	},
	{
		.description = "ifra address only",
		.addr_len = sizeof(struct sockaddr_in), .addr_fam = AF_INET, .addr_str = "10.8.8.8",
		.broad_len = 0, .broad_str = NULL,
		.mask_len = 0, .mask_str = NULL,
		.error = 0
	},
	{
		.description = "ifra mask len 1",
		.addr_len = sizeof(struct sockaddr_in), .addr_fam = AF_INET, .addr_str = "10.9.9.1",
		.broad_len = 0, .broad_str = NULL,
		.mask_len = 1, .mask_fam = AF_INET, .mask_str = "255.255.255.0",
		.error = 0
	},
	{
		.description = "ifra mask len 3",
		.addr_len = sizeof(struct sockaddr_in), .addr_fam = AF_INET, .addr_str = "10.9.9.3",
		.broad_len = 0, .broad_str = NULL,
		.mask_len = 1, .mask_fam = AF_INET, .mask_str = "255.255.255.0",
		.error = 0
	},
	{
		.description = "ifra mask len 5",
		.addr_len = sizeof(struct sockaddr_in), .addr_fam = AF_INET, .addr_str = "10.9.9.5",
		.broad_len = 0, .broad_str = NULL,
		.mask_len = 1, .mask_fam = AF_INET, .mask_str = "255.255.255.0",
		.error = 0
	},
	{
		.description = "ifra mask len 7",
		.addr_len = sizeof(struct sockaddr_in), .addr_fam = AF_INET, .addr_str = "10.9.9.7",
		.broad_len = 0, .broad_str = NULL,
		.mask_len = 1, .mask_fam = AF_INET, .mask_str = "255.255.255.0",
		.error = 0
	},
	{
		.description = "ifra mask len 9",
		.addr_len = sizeof(struct sockaddr_in), .addr_fam = AF_INET, .addr_str = "10.9.9.9",
		.broad_len = 0, .broad_str = NULL,
		.mask_len = 1, .mask_fam = AF_INET, .mask_str = "255.255.255.0",
		.error = 0
	},
	{
		.description = "ifra mask len 11",
		.addr_len = sizeof(struct sockaddr_in), .addr_fam = AF_INET, .addr_str = "10.9.9.11",
		.broad_len = 0, .broad_str = NULL,
		.mask_len = 1, .mask_fam = AF_INET, .mask_str = "255.255.255.0",
		.error = 0
	},
	{
		.description = "ifra mask len 13",
		.addr_len = sizeof(struct sockaddr_in), .addr_fam = AF_INET, .addr_str = "10.9.9.13",
		.broad_len = 0, .broad_str = NULL,
		.mask_len = 1, .mask_fam = AF_INET, .mask_str = "255.255.255.0",
		.error = 0
	},
	{
		.description = NULL,
		.error = -1
	}
};

T_DECL(sioc_ifra_addr_bounds, "test bound checks on socket address passed to interface ioctls",
    T_META_ASROOT(true), T_META_TAG_VM_PREFERRED)
{
	int s = -1;

	T_QUIET; T_EXPECT_POSIX_SUCCESS(s = socket(AF_INET, SOCK_DGRAM, 0), "socket");

	char ifname[IFNAMSIZ];
	strlcpy(ifname, IF_NAME, sizeof(ifname));
	int error = 0;
	if ((error = ifnet_create(s, ifname, sizeof(ifname))) != 0) {
		if (error == EINVAL) {
			T_SKIP("The system does not support the %s cloning interface", IF_NAME);
		}
		T_SKIP("This test failed creating a %s cloning interface", IF_NAME);
	}
	T_LOG("created clone interface '%s'", ifname);

	struct ioc_ifra *ioc_ifra;

	for (ioc_ifra = ioc_ifra_list; ioc_ifra->error != -1; ioc_ifra++) {
		struct in_aliasreq ifra = { 0 };
		unsigned char pattern;
		int retval;

		T_LOG("");
		T_LOG("TEST CASE: %s, ioctl(SIOCAIFADDR, %s)", ioc_ifra->description, ioc_ifra->addr_str != NULL ? ioc_ifra->addr_str : "");

		if (find_unused_pattern_in_rt_if_list(&pattern, PATTERN_SIZE) == false) {
			T_SKIP("Could not find unused pattern in rt_if_list");
			return;
		}

		memset(&ifra, pattern, sizeof(ifra));

		strlcpy(ifra.ifra_name, ifname, sizeof(ifra.ifra_name));

		ifra.ifra_addr.sin_len = ioc_ifra->addr_len;
		ifra.ifra_addr.sin_family = ioc_ifra->addr_fam;
		if (ioc_ifra->addr_str != NULL) {
			ifra.ifra_addr.sin_addr.s_addr = inet_addr(ioc_ifra->addr_str);
		}

		ifra.ifra_broadaddr.sin_len = ioc_ifra->broad_len;
		ifra.ifra_broadaddr.sin_family = ioc_ifra->broad_fam;
		if (ioc_ifra->broad_str != NULL) {
			ifra.ifra_broadaddr.sin_addr.s_addr = inet_addr(ioc_ifra->broad_str);
		}

		ifra.ifra_mask.sin_len = ioc_ifra->mask_len;
		ifra.ifra_mask.sin_family = ioc_ifra->mask_fam;
		if (ioc_ifra->mask_str != NULL) {
			ifra.ifra_mask.sin_addr.s_addr = inet_addr(ioc_ifra->mask_str);
		}

		if (ioc_ifra->error == 0) {
			T_EXPECT_POSIX_SUCCESS(retval = ioctl(s, SIOCAIFADDR, &ifra), "SIOCAIFADDR retval %d", retval);
		} else {
			T_EXPECT_POSIX_FAILURE(retval = ioctl(s, SIOCAIFADDR, &ifra), EINVAL, "SIOCAIFADDR retval %d, %s", retval, strerror(errno));
		}

		T_EXPECT_EQ(check_rt_if_list_for_pattern("after ioctl SIOCAIFADDR", pattern, PATTERN_SIZE), 0, "pattern should not be found");
	}

	print_rt_iflist2(__func__);

	(void)ifnet_destroy(s, ifname, true);

	close(s);
}

T_DECL(sioc_ifr_dstaddr_leak, "test bound checks on socket address passed to interface ioctls",
    T_META_ASROOT(true), T_META_TAG_VM_PREFERRED)
{
	int s = -1;
	struct ifreq ifr = { 0 };
	unsigned char pattern;
	struct ifaddrs *ifap = NULL, *ifa;
	bool found_gif0 = false;

	T_QUIET; T_EXPECT_POSIX_SUCCESS(getifaddrs(&ifap), "getifaddrs");
	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (strcmp(ifa->ifa_name, "gif0") == 0) {
			found_gif0 = true;
			break;
		}
	}
	freeifaddrs(ifap);
	ifap = NULL;
	if (found_gif0 == false) {
		T_SKIP("gif0 does not exists");
	}

	if (find_unused_pattern_in_rt_if_list(&pattern, PATTERN_SIZE) == false) {
		T_SKIP("Could not find unused pattern");
		return;
	}

	T_QUIET; T_EXPECT_POSIX_SUCCESS(s = socket(AF_INET, SOCK_DGRAM, 0), "socket");

	strlcpy(ifr.ifr_name, "gif0", sizeof(ifr.ifr_name));
	ifr.ifr_dstaddr.sa_family = AF_INET6;
	ifr.ifr_dstaddr.sa_len = 0xf0;
	memset(&ifr.ifr_dstaddr.sa_data, pattern, PATTERN_SIZE);

	T_EXPECT_POSIX_SUCCESS(ioctl(s, SIOCSIFDSTADDR, &ifr), "ioctl(SIOCSIFDSTADDR)");

	print_rt_iflist2(__func__);

	close(s);

	T_EXPECT_EQ(check_rt_if_list_for_pattern("AFTER", pattern, PATTERN_SIZE), 0, "pattern should not be found");
}

static struct ioc_ifreq ioc_list_config[] = {
	{ SIOCSIFADDR, sizeof(struct sockaddr_in), AF_INET, "0.0.0.0", EINVAL },
	{ SIOCSIFADDR, sizeof(struct sockaddr_in), AF_INET, "255.255.255.255", EINVAL },
	{ SIOCSIFADDR, sizeof(struct sockaddr_in), AF_INET, "238.207.49.91", EINVAL },

	{ SIOCAIFADDR_IN6, sizeof(struct sockaddr_in6), AF_INET6, "::", EINVAL },
	{ SIOCAIFADDR_IN6, sizeof(struct sockaddr_in6), AF_INET6, "ff33:40:fd25:549c:675b::1", EINVAL },
	{ SIOCAIFADDR_IN6, sizeof(struct sockaddr_in6), AF_INET6, "::ffff:0a0a:0a0a", EINVAL },
	{ SIOCAIFADDR_IN6, sizeof(struct sockaddr_in6), AF_INET6, "::0a0a:0a0a", EINVAL },

	{ 0, 0, 0, "", -1 },
};

static void
test_sioc_ifr_addr_config_v4(struct ioc_ifreq *ioc_ifreq, int s, const char *ifname)
{
	struct ifreq ifr = { 0 };
	struct sockaddr_in *sin;

	T_LOG("");
	T_LOG("TEST CASE: %s ioctl(%s, sa_len %u, sa_family %u, %s) -> %d", __func__,
	    ioc_str(ioc_ifreq->ioc_cmd), ioc_ifreq->salen, ioc_ifreq->safamily, ioc_ifreq->sastr, ioc_ifreq->error);


	strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));

	sin = (struct sockaddr_in *)(void *)&ifr.ifr_addr;
	sin->sin_len = ioc_ifreq->salen;
	sin->sin_family = ioc_ifreq->safamily;
	sin->sin_addr.s_addr = inet_addr(ioc_ifreq->sastr);

	int retval;
	if (ioc_ifreq->error == 0) {
		T_EXPECT_POSIX_SUCCESS(retval = ioctl(s, ioc_ifreq->ioc_cmd, &ifr),
		    "%s, %s: retval %d", ioc_str(ioc_ifreq->ioc_cmd), ioc_ifreq->sastr, retval);
	} else {
		T_EXPECT_POSIX_FAILURE(retval = ioctl(s, ioc_ifreq->ioc_cmd, &ifr), ioc_ifreq->error,
		    "%s, %s: retval %d errno %s", ioc_str(ioc_ifreq->ioc_cmd), ioc_ifreq->sastr, retval, strerror(errno));
	}

	fflush(stdout);
	fflush(stderr);
}

static void
test_sioc_ifr_addr_config_v6(struct ioc_ifreq *ioc_ifreq, int s, const char *ifname)
{
	struct in6_aliasreq     ifra = {{ 0 }, { 0 }, { 0 }, { 0 }, 0, { 0, 0, ND6_INFINITE_LIFETIME, ND6_INFINITE_LIFETIME }};
	struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)(void *)&ifra.ifra_addr;
	struct sockaddr_in6 *mask6 = (struct sockaddr_in6 *)(void *)&ifra.ifra_prefixmask;
	T_LOG("");
	T_LOG("TEST CASE: %s ioctl(%s, sa_len %u, sa_family %u, %s) -> %d", __func__,
	    ioc_str(ioc_ifreq->ioc_cmd), ioc_ifreq->salen, ioc_ifreq->safamily, ioc_ifreq->sastr, ioc_ifreq->error);

	strlcpy(ifra.ifra_name, ifname, sizeof(ifra.ifra_name));

	sin6->sin6_len = ioc_ifreq->salen;
	sin6->sin6_family = ioc_ifreq->safamily;
	T_ASSERT_EQ(inet_pton(AF_INET6, ioc_ifreq->sastr, &sin6->sin6_addr), 1, NULL);
	sin6->sin6_scope_id = if_nametoindex(ifname);

	mask6->sin6_len = ioc_ifreq->salen;
	mask6->sin6_family = ioc_ifreq->safamily;
	T_ASSERT_EQ(inet_pton(AF_INET6, "ffff:ffff:ffff:ffff::", &mask6->sin6_addr), 1, NULL);

	ifra.ifra_lifetime.ia6t_vltime = ND6_INFINITE_LIFETIME;
	ifra.ifra_lifetime.ia6t_pltime = ND6_INFINITE_LIFETIME;

	int retval;
	if (ioc_ifreq->error == 0) {
		T_EXPECT_POSIX_SUCCESS(retval = ioctl(s, ioc_ifreq->ioc_cmd, &ifra),
		    "%s, %s: retval %d", ioc_str(ioc_ifreq->ioc_cmd), ioc_ifreq->sastr, retval);
	} else {
		T_EXPECT_POSIX_FAILURE(retval = ioctl(s, ioc_ifreq->ioc_cmd, &ifra), ioc_ifreq->error,
		    "%s, %s: retval %d errno %s", ioc_str(ioc_ifreq->ioc_cmd), ioc_ifreq->sastr, retval, strerror(errno));
	}

	fflush(stdout);
	fflush(stderr);
}


T_DECL(sioc_ifr_addr_config, "test failure cases for interface address configuration ioctls",
    T_META_ASROOT(true), T_META_TAG_VM_PREFERRED)
{
	int s = -1;
	int s6 = -1;
	char ifname[IFNAMSIZ];

	T_LOG("%s", __func__);

	T_QUIET; T_EXPECT_POSIX_SUCCESS(s = socket(AF_INET, SOCK_DGRAM, 0), "socket");
	T_QUIET; T_EXPECT_POSIX_SUCCESS(s6 = socket(AF_INET6, SOCK_DGRAM, 0), "socket");

	strlcpy(ifname, IF_NAME, sizeof(ifname));

	int error = 0;
	if ((error = ifnet_create(s, ifname, sizeof(ifname))) != 0) {
		if (error == EINVAL) {
			T_SKIP("The system does not support the %s cloning interface", IF_NAME);
		}
		T_SKIP("This test failed creating a %s cloning interface", IF_NAME);
	}
	T_LOG("created clone interface '%s'", ifname);

	struct ioc_ifreq *ioc_ifreq;
	for (ioc_ifreq = ioc_list_config; ioc_ifreq->error != -1; ioc_ifreq++) {
		if (ioc_ifreq->safamily == AF_INET) {
			test_sioc_ifr_addr_config_v4(ioc_ifreq, s, ifname);
		} else if (ioc_ifreq->safamily == AF_INET6) {
			test_sioc_ifr_addr_config_v6(ioc_ifreq, s6, ifname);
		}
	}
	print_rt_iflist2(__func__);
	(void)ifnet_destroy(s, ifname, true);

	close(s);
	close(s6);
}

/**
** Test SIOCPROTOATTACH, SIOCPROTOATTACH_IN6
**/
static int S_socket = -1;

static int
get_inet_dgram_socket(void)
{
	if (S_socket >= 0) {
		return S_socket;
	}
	S_socket = socket(AF_INET, SOCK_DGRAM, 0);
	T_QUIET;
	T_ASSERT_POSIX_SUCCESS(S_socket, "socket(AF_INET, SOCK_DGRAM, 0)");
	return S_socket;
}

static void
close_inet_dgram_socket(void)
{
	if (S_socket >= 0) {
		close(S_socket);
		S_socket = -1;
	}
}

static int S_socket_6 = -1;

static int
get_inet6_dgram_socket(void)
{
	if (S_socket_6 >= 0) {
		return S_socket_6;
	}
	S_socket_6 = socket(AF_INET6, SOCK_DGRAM, 0);
	T_QUIET;
	T_ASSERT_POSIX_SUCCESS(S_socket_6, "socket(AF_INET6, SOCK_DGRAM, 0)");
	return S_socket_6;
}

static void
close_inet6_dgram_socket(void)
{
	if (S_socket_6 >= 0) {
		close(S_socket_6);
		S_socket_6 = -1;
	}
}

static char ifname[IFNAMSIZ];

static void
test_proto_attach_cleanup(void)
{
	if (ifname[0] != '\0') {
		int     s;

		s = get_inet_dgram_socket();
		(void)ifnet_destroy(s, ifname, false);
		T_LOG("Destroyed '%s'", ifname);
		ifname[0] = '\0';
	}
}

static void
test_proto_attach_atend(void)
{
	test_proto_attach_cleanup();
	close_inet_dgram_socket();
	close_inet6_dgram_socket();
}

static void
test_proto_attach(void)
{
	int             error;
	struct ifreq    ifr;
	int             s;
	int             s6;

	T_ATEND(test_proto_attach_atend);
	s = get_inet_dgram_socket();
	strlcpy(ifname, "feth", sizeof(ifname));
	error = ifnet_create(s, ifname, sizeof(ifname));
	if (error != 0) {
		if (error == EINVAL) {
			T_SKIP("%s cloning interface not supported", ifname);
		}
		T_SKIP("%s cloning interface failed", ifname);
	}
	T_LOG("Created '%s'", ifname);

	/* first proto attach should succeed */
	bzero(&ifr, sizeof(ifr));
	strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	error = ioctl(s, SIOCPROTOATTACH, &ifr);
	T_ASSERT_POSIX_SUCCESS(error, "SIOCPROTOATTACH %s succeeded",
	    ifr.ifr_name);

	/* second proto attach should fail with EEXIST */
	error = ioctl(s, SIOCPROTOATTACH, &ifr);
	T_ASSERT_POSIX_FAILURE(error, EEXIST,
	    "SIOCPROTOATTACH %s failed as expected",
	    ifr.ifr_name);

	/* first proto attach should succeed */
	s6 = get_inet6_dgram_socket();
	bzero(&ifr, sizeof(ifr));
	strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	error = ioctl(s6, SIOCPROTOATTACH_IN6, &ifr);
	T_ASSERT_POSIX_SUCCESS(error,
	    "SIOCPROTOATTACH_IN6 %s succeeded",
	    ifr.ifr_name);

	/* second proto attach should fail with EEXIST */
	error = ioctl(s6, SIOCPROTOATTACH_IN6, &ifr);
	T_ASSERT_POSIX_FAILURE(error, EEXIST,
	    "SIOCPROTOATTACH_IN6 %s failed as expected",
	    ifr.ifr_name);
	test_proto_attach_cleanup();
}

T_DECL(sioc_proto_attach, "test protocol attachment",
    T_META_ASROOT(true), T_META_TAG_VM_PREFERRED)
{
	test_proto_attach();
}

/*
 * The following is only meant to be run from the command line
 * when one adds a new socket interface ioctl
 */
T_DECL(sioc_print_all, "print all socket interface ioctls", T_META_ENABLED(false), T_META_TAG_VM_PREFERRED)
{
#define X(a) T_LOG("%-32s 0x%08lx GROUP %3lu BASECMD %3ld LEN %4lu\n", \
    #a, a, (a & 0x00000ff00) >> 8, a & 0x000000ff, IOCPARM_LEN(a));
	SIOC_LIST
#undef X
}
