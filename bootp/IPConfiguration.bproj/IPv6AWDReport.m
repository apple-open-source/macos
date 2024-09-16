/*
 * Copyright (c) 2017-2024 Apple Inc. All rights reserved.
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
 * IPv6AWDReport.m
 * - C shim layer to interact with AWD to generate and submit a metric
 */


#import <Foundation/Foundation.h>

#include "IPv6AWDReport.h"
#include "symbol_scope.h"
#include "mylog.h"


PRIVATE_EXTERN IPv6AWDReportRef
IPv6AWDReportCreate(InterfaceType type)
{
    return (NULL);
}

PRIVATE_EXTERN void
IPv6AWDReportSubmit(IPv6AWDReportRef report)
{
    return;
}

void
IPv6AWDReportSetAPNName(IPv6AWDReportRef report, CFStringRef apn_name)
{
    return;
}

#define IPV6_REPORT_SET_PROP(report, name, value)

void
IPv6AWDReportSetLinkLocalAddressDuplicated(IPv6AWDReportRef report)
{
    IPV6_REPORT_SET_PROP(report, linklocalAddressDuplicated, YES);
}

void
IPv6AWDReportSetAutoconfAddressDuplicated(IPv6AWDReportRef report)
{
    IPV6_REPORT_SET_PROP(report, autoconfAddressDuplicated, YES);
}

void
IPv6AWDReportSetAutoconfAddressDeprecated(IPv6AWDReportRef report)
{
    IPV6_REPORT_SET_PROP(report, autoconfAddressDeprecated, YES);
}

void
IPv6AWDReportSetAutoconfAddressDetached(IPv6AWDReportRef report)
{
    IPV6_REPORT_SET_PROP(report, autoconfAddressDetached, YES);
}

void
IPv6AWDReportSetAutoconfAddressAcquired(IPv6AWDReportRef report)
{
    IPV6_REPORT_SET_PROP(report, autoconfAddressAcquired, YES);
}

void
IPv6AWDReportSetAutoconfRestarted(IPv6AWDReportRef report)
{
    IPV6_REPORT_SET_PROP(report, autoconfRestarted, YES);
}

void
IPv6AWDReportSetAutoconfRDNSS(IPv6AWDReportRef report)
{
    IPV6_REPORT_SET_PROP(report, autoconfRdnss, YES);
}

void
IPv6AWDReportSetAutoconfDNSSL(IPv6AWDReportRef report)
{
    IPV6_REPORT_SET_PROP(report, autoconfDnssl, YES);
}

void
IPv6AWDReportSetDHCPv6AddressAcquired(IPv6AWDReportRef report)
{
    IPV6_REPORT_SET_PROP(report, dhcpv6AddressAcquired, YES);
}

void
IPv6AWDReportSetDHCPv6DNSServers(IPv6AWDReportRef report)
{
    IPV6_REPORT_SET_PROP(report, dhcpv6DnsServers, YES);
}

void
IPv6AWDReportSetDHCPv6DNSDomainList(IPv6AWDReportRef report)
{
    IPV6_REPORT_SET_PROP(report, dhcpv6DnsDomainList, YES);
}

void
IPv6AWDReportSetManualAddressConfigured(IPv6AWDReportRef report)
{
    IPV6_REPORT_SET_PROP(report, manualAddressConfigured, YES);
}

void
IPv6AWDReportSetPrefixPreferredLifetime(IPv6AWDReportRef report, uint32_t val)
{
    IPV6_REPORT_SET_PROP(report, prefixPreferredLifetimeSeconds, val);
}

void
IPv6AWDReportSetPrefixValidLifetime(IPv6AWDReportRef report, uint32_t val)
{
    IPV6_REPORT_SET_PROP(report, prefixValidLifetimeSeconds, val);
}

void
IPv6AWDReportSetPrefixLifetimeNotInfinite(IPv6AWDReportRef report)
{
    IPV6_REPORT_SET_PROP(report, prefixLifetimeNotInfinite, YES);
}

void
IPv6AWDReportSetRouterLifetime(IPv6AWDReportRef report, uint16_t val)
{
    IPV6_REPORT_SET_PROP(report, routerLifetimeSeconds, val);
}

void
IPv6AWDReportSetRouterLifetimeNotMaximum(IPv6AWDReportRef report)
{
    IPV6_REPORT_SET_PROP(report, routerLifetimeNotMaximum, YES);
}

void
IPv6AWDReportSetRouterSourceAddressCollision(IPv6AWDReportRef report)
{
    IPV6_REPORT_SET_PROP(report, routerSourceAddressCollision, YES);
}

void
IPv6AWDReportSetRouterLifetimeZero(IPv6AWDReportRef report)
{
    IPV6_REPORT_SET_PROP(report, routerLifetimeZero, YES);
}

void
IPv6AWDReportSetXLAT464Enabled(IPv6AWDReportRef report)
{
    IPV6_REPORT_SET_PROP(report, xlat464Enabled, YES);
}

void
IPv6AWDReportSetXLAT464PLATDiscoveryFailed(IPv6AWDReportRef report)
{
    IPV6_REPORT_SET_PROP(report, xlat464PlatDiscoveryFailed, YES);
}

void
IPv6AWDReportSetDefaultRouterCount(IPv6AWDReportRef report, UInt32 count)
{
    IPV6_REPORT_SET_PROP(report, defaultRouterCount, count);
}

void
IPv6AWDReportSetExpiredDefaultRouterCount(IPv6AWDReportRef report, UInt32 count)
{
    IPV6_REPORT_SET_PROP(report, expiredDefaultRouterCount, count);
}

void
IPv6AWDReportSetPrefixCount(IPv6AWDReportRef report, UInt32 count)
{
    IPV6_REPORT_SET_PROP(report, prefixCount, count);
}

void
IPv6AWDReportSetExpiredPrefixCount(IPv6AWDReportRef report, UInt32 count)
{
    IPV6_REPORT_SET_PROP(report, expiredPrefixCount, count);
}

void
IPv6AWDReportSetRouterSolicitationCount(IPv6AWDReportRef report, UInt32 count)
{
    IPV6_REPORT_SET_PROP(report, routerSolicitationCount, count);
}

void
IPv6AWDReportSetControlQueueUnsentCount(IPv6AWDReportRef report, UInt32 count)
{
    IPV6_REPORT_SET_PROP(report, controlQueueUnsentCount, count);
}

void
IPv6AWDReportSetAutoconfAddressAcquisitionSeconds(IPv6AWDReportRef report,
						  UInt32 seconds)
{
    IPV6_REPORT_SET_PROP(report, autoconfAddressAcquisitionSeconds, seconds);
}

void
IPv6AWDReportSetDHCPv6AddressAcquisitionSeconds(IPv6AWDReportRef report,
						UInt32 seconds)
{
    IPV6_REPORT_SET_PROP(report, dhcpv6AddressAcquisitionSeconds, seconds);
}

void
IPv6AWDReportSetDNSConfigurationAcquisitionSeconds(IPv6AWDReportRef report,
						   UInt32 seconds)
{
    IPV6_REPORT_SET_PROP(report, dnsConfigurationAcquisitionSeconds, seconds);
}

#ifdef TEST_IPV6_AWD_REPORT

int
main(int argc, char * argv[])
{
    IPv6AWDReportRef	report;

    report = IPv6AWDReportCreate(kInterfaceTypeCellular);
    if (report == NULL) {
	fprintf(stderr, "WirelessDiagnostics framework not available\n");
	exit(1);
    }
    printf("Before setting values:\n");
    CFShow(report);
    fflush(stdout);

    IPv6AWDReportSetAPNName(report, CFSTR("MyAPNName"));
    IPv6AWDReportSetLinkLocalAddressDuplicated(report);
    IPv6AWDReportSetAutoconfAddressDuplicated(report);
    IPv6AWDReportSetAutoconfAddressDeprecated(report);
    IPv6AWDReportSetAutoconfAddressDetached(report);
    IPv6AWDReportSetAutoconfAddressAcquired(report);
    IPv6AWDReportSetAutoconfRestarted(report);
    IPv6AWDReportSetAutoconfRDNSS(report);
    IPv6AWDReportSetAutoconfDNSSL(report);
    IPv6AWDReportSetDHCPv6AddressAcquired(report);
    IPv6AWDReportSetDHCPv6DNSServers(report);
    IPv6AWDReportSetDHCPv6DNSDomainList(report);
    IPv6AWDReportSetManualAddressConfigured(report);
    IPv6AWDReportSetPrefixPreferredLifetime(report, 1800);
    IPv6AWDReportSetPrefixValidLifetime(report, 3600);
    IPv6AWDReportSetPrefixLifetimeNotInfinite(report);
    IPv6AWDReportSetRouterLifetime(report, 360);
    IPv6AWDReportSetRouterLifetimeNotMaximum(report);
    IPv6AWDReportSetRouterSourceAddressCollision(report);
    IPv6AWDReportSetRouterLifetimeZero(report);
    IPv6AWDReportSetXLAT464Enabled(report);

    IPv6AWDReportSetDefaultRouterCount(report, 1);
    IPv6AWDReportSetExpiredDefaultRouterCount(report, 2);
    IPv6AWDReportSetPrefixCount(report, 3);
    IPv6AWDReportSetExpiredPrefixCount(report, 10);
    IPv6AWDReportSetRouterSolicitationCount(report, 11);
    IPv6AWDReportSetControlQueueUnsentCount(report, 12);
    IPv6AWDReportSetAutoconfAddressAcquisitionSeconds(report, 120);
    IPv6AWDReportSetDHCPv6AddressAcquisitionSeconds(report, 250);
    IPv6AWDReportSetDNSConfigurationAcquisitionSeconds(report, 1001);

    printf("After setting values:\n");
    CFShow(report);
    fflush(stdout);

    IPv6AWDReportSubmit(report);
    CFRelease(report);
    if (argc > 1) {
	fprintf(stderr, "pid is %d\n", getpid());
	sleep(120);
    }
}

#endif /* TEST_IPV6_AWD_REPORT */
