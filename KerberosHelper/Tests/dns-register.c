/*
 * Copyright (c) 2008 Apple Inc. All rights reserved.
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


#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <dns_sd.h>
#include <err.h>
#include <unistd.h>


void
TXTRegisterCallback(DNSServiceRef sdRef __attribute__((unused)),
		    DNSRecordRef RecordRef __attribute__((unused)), 
		    DNSServiceFlags flags __attribute__((unused)), 
		    DNSServiceErrorType errorCode __attribute__((unused)),
		    void *context __attribute__((unused)))
{
}


int
main(int argc __attribute__((unused)), char **argv __attribute__((unused)))
{
    DNSServiceErrorType error;
    DNSServiceRef dnsRef;
    int i, fd;

    if (argc < 2)
	errx(1, "argc < 2");

    for (i = 1; i < argc; i++) {
	DNSRecordRef recordRef;
	char *hostname = argv[i];
	char *recordName;
	char *realm;
	size_t len;
	const char *prefix = "LKDC:SHA1.fake";

	asprintf(&recordName, "_kerberos.%s.", hostname);
	if (recordName == NULL)
	    errx(1, "malloc");
	
	len = strlen(prefix) + strlen(hostname);
	asprintf(&realm, "%c%s%s", len, prefix, hostname);
	if (realm == NULL)
	    errx(1, "malloc");
	
	error = DNSServiceCreateConnection(&dnsRef);
	if (error)
	    errx(1, "DNSServiceCreateConnection");
	
	error =  DNSServiceRegisterRecord(dnsRef, 
					  &recordRef,
					  kDNSServiceFlagsShared | kDNSServiceFlagsAllowRemoteQuery,
					  0,
					  recordName,
					  kDNSServiceType_TXT,
					  kDNSServiceClass_IN,
					  len+1,
					  realm,
					  300,
					  TXTRegisterCallback,
					  NULL);
	if (error)
	    errx(1, "DNSServiceRegisterRecord: %d", error);
    }

    fd = DNSServiceRefSockFD(dnsRef);

    while (1) {
	int ret;
	fd_set rfd;

	FD_ZERO(&rfd);
	FD_SET(fd, &rfd);

	ret = select(fd + 1, &rfd, NULL, NULL, NULL);
	if (ret == 0)
	    errx(1, "timeout ?");
	else if (ret < 0)
	    err(1, "select");
	
	if (FD_ISSET(fd, &rfd))
	    DNSServiceProcessResult(dnsRef);
    }
}
