/*
 * Copyright (c) 2017-2018 Apple Inc. All rights reserved.
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


#include <pcap.h>
#include <stdio.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>


#define PAD32(x) (((x) + 3) & ~3)
#define	SWAPLONG(y) \
((((y)&0xff)<<24) | (((y)&0xff00)<<8) | (((y)&0xff0000)>>8) | (((y)>>24)&0xff))
#define	SWAPSHORT(y) \
( (((y)&0xff)<<8) | ((u_short)((y)&0xff00)>>8) )
#define	SWAPLONGLONG(y) \
(SWAPLONG((unsigned long)(y)) << 32 | SWAPLONG((unsigned long)((y) >> 32)))

void hex_and_ascii_print(const char *, const void *, size_t, const char *);

void
read_callback(u_char *user, const struct pcap_pkthdr *hdr, const u_char *bytes)
{
	fprintf(stderr, "pcap_pkthdr ts %ld.%06d caplen %u len %u\n",
			hdr->ts.tv_sec,
			hdr->ts.tv_usec,
			hdr->caplen,
			hdr->len);

	hex_and_ascii_print("", bytes, hdr->caplen, "\n");
}

int
main(int argc, const char * argv[])
{
	int i;
	char errbuf[PCAP_ERRBUF_SIZE];

	for (i = 1; i < argc; i++) {
		pcap_t *pcap;

		if (strcmp(argv[i], "-h") == 0) {
			char *path = strdup((argv[0]));
			printf("# usage: %s  file...\n", getprogname());
			if (path != NULL)
				free(path);
			exit(0);
		}

		printf("#\n# opening %s\n#\n", argv[i]);

		pcap = pcap_open_offline(argv[i], errbuf);
		if (pcap == NULL) {
			warnx("pcap_open_offline(%s) failed: %s\n",
				  argv[i], errbuf);
			continue;
		}
		int result = pcap_loop(pcap, -1, read_callback, (u_char *)pcap);
		if (result < 0) {
			warnx("pcap_dispatch failed: %s\n",
				  pcap_statustostr(result));
		} else {
			printf("# read %d packets\n", result);
		}
		pcap_close(pcap);
	}
	return 0;
}
