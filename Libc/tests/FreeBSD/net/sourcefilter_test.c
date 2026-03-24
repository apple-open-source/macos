/*-
 * Copyright (c) 2025 Klara, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <net/if.h>
#include <netinet/in.h>

#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include <atf-c.h>

ATF_TC(getsourcefilter_inet_len);
ATF_TC_HEAD(getsourcefilter_inet_len, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "getsourcefilter() IPv4 address length");
}
ATF_TC_BODY(getsourcefilter_inet_len, tc)
{
	struct sockaddr_in sin = {
		.sin_family = AF_INET,
		.sin_addr = { .s_addr = 0xe00000e0 },
	};
	uint32_t fmode = 0, numsrc = 0;
	unsigned int idx, len;
	int experr, sd;

#ifndef __APPLE__
	if (!feature_present("inet"))
		atf_tc_skip("requires IPv4 support");
#endif /* __APPLE__ */
	ATF_REQUIRE((sd = socket(AF_INET, SOCK_DGRAM, PF_UNSPEC)) >= 0);
	ATF_REQUIRE((idx = if_nametoindex("lo0")) != 0);
	for (len = 0; len <= 255; len++) {
		experr = len == sizeof(sin) ? EADDRNOTAVAIL : EINVAL;
		sin.sin_len = len;
		ATF_CHECK_ERRNO(experr,
		    getsourcefilter(sd, idx, (struct sockaddr *)&sin,
			sizeof(sin), &fmode, &numsrc, NULL) != 0);
	}
	ATF_CHECK_EQ(0, close(sd));
}

ATF_TC(getsourcefilter_inet6_len);
ATF_TC_HEAD(getsourcefilter_inet6_len, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "getsourcefilter(3) IPv6 address length");
}
ATF_TC_BODY(getsourcefilter_inet6_len, tc)
{
	struct sockaddr_in6 sin6 = {
		.sin6_family = AF_INET6,
		.sin6_addr = { .s6_addr = { [0] = 0xff } },
	};
	uint32_t fmode = 0, numsrc = 0;
	unsigned int idx, len;
	int experr, sd;

#ifndef __APPLE__
	if (!feature_present("inet6"))
		atf_tc_skip("requires IPv6 support");
#endif /* __APPLE__ */
	ATF_REQUIRE((sd = socket(AF_INET6, SOCK_DGRAM, PF_UNSPEC)) >= 0);
	ATF_REQUIRE((idx = if_nametoindex("lo0")) != 0);
	for (len = 0; len <= 255; len++) {
		experr = len == sizeof(sin6) ? EADDRNOTAVAIL : EINVAL;
		sin6.sin6_len = len;
		ATF_CHECK_ERRNO(experr,
		    getsourcefilter(sd, idx, (struct sockaddr *)&sin6,
			sizeof(sin6), &fmode, &numsrc, NULL) != 0);
	}
	ATF_CHECK_EQ(0, close(sd));
}

ATF_TC(setsourcefilter_inet_len);
ATF_TC_HEAD(setsourcefilter_inet_len, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "setsourcefilter(3) IPv4 address length");
}
ATF_TC_BODY(setsourcefilter_inet_len, tc)
{
	struct sockaddr_in sin = {
		.sin_family = AF_INET,
		.sin_addr = { .s_addr = 0xe00000e0 },
	};
	unsigned int idx, len;
	int experr, sd;

#ifndef __APPLE__
	if (!feature_present("inet"))
		atf_tc_skip("requires IPv4 support");
#endif /* __APPLE__ */
	ATF_REQUIRE((sd = socket(AF_INET, SOCK_DGRAM, PF_UNSPEC)) >= 0);
	ATF_REQUIRE((idx = if_nametoindex("lo0")) != 0);
	for (len = 0; len <= 255; len++) {
		experr = len == sizeof(sin) ? EADDRNOTAVAIL : EINVAL;
		sin.sin_len = len;
		ATF_CHECK_ERRNO(experr,
		    setsourcefilter(sd, idx, (struct sockaddr *)&sin,
			sizeof(sin), MCAST_INCLUDE, 0, NULL) != 0);
	}
	ATF_CHECK_EQ(0, close(sd));
}

ATF_TC(setsourcefilter_inet6_len);
ATF_TC_HEAD(setsourcefilter_inet6_len, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "setsourcefilter(3) IPv6 address length");
}
ATF_TC_BODY(setsourcefilter_inet6_len, tc)
{
	struct sockaddr_in6 sin6 = {
		.sin6_family = AF_INET6,
		.sin6_addr = { .s6_addr = { [0] = 0xff } },
	};
	unsigned int idx, len;
	int experr, sd;

#ifndef __APPLE__
	if (!feature_present("inet6"))
		atf_tc_skip("requires IPv6 support");
#endif /* __APPLE__ */
	ATF_REQUIRE((sd = socket(AF_INET6, SOCK_DGRAM, PF_UNSPEC)) >= 0);
	ATF_REQUIRE((idx = if_nametoindex("lo0")) != 0);
	for (len = 0; len <= 255; len++) {
		experr = len == sizeof(sin6) ? EADDRNOTAVAIL : EINVAL;
		sin6.sin6_len = len;
		ATF_CHECK_ERRNO(experr,
		    setsourcefilter(sd, idx, (struct sockaddr *)&sin6,
			sizeof(sin6), MCAST_INCLUDE, 0, NULL) != 0);
	}
	ATF_CHECK_EQ(0, close(sd));
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, getsourcefilter_inet_len);
	ATF_TP_ADD_TC(tp, getsourcefilter_inet6_len);
	ATF_TP_ADD_TC(tp, setsourcefilter_inet_len);
	ATF_TP_ADD_TC(tp, setsourcefilter_inet6_len);
	return (atf_no_error());
}
