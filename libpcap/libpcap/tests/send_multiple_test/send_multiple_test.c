//
//  send_multiple_test.c
//
//   Copyright (c) 2023 Apple Inc. All rights reserved.
//

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>

#include <pcap/pcap.h>

#include "pcap-util.h"

static char ebuf[PCAP_ERRBUF_SIZE];

/*
 * Send UDP packets to loopback port 12345 with strings "hello", "hello7", "hello88", "hello999"
 */
static unsigned char p1[64] = { 0x02, 0x00, 0x00, 0x00, 0x45,0x00,0x00,0x22,0x9a,0xbf,0x00,0x00,0x40,0x11,0xe2,0x09,0x7f,0x00,0x00,0x01,0x7f,0x00,0x00,0x01,0xfa,0x17,0x30,0x39,0x00,0x0e,0x93,0xa2,0x68,0x65,0x6c,0x6c,0x6f,0x0a };

static unsigned char p2[64] = { 0x02, 0x00, 0x00, 0x00, 0x45,0x00,0x00,0x23,0x74,0x80,0x00,0x00,0x40,0x11,0x08,0x48,0x7f,0x00,0x00,0x01,0x7f,0x00,0x00,0x01,0xfd,0x4b,0x30,0x39,0x00,0x0f,0x86,0x3f,0x68,0x65,0x6c,0x6c,0x6f,0x37,0x0a };

static unsigned char p3[64] = { 0x02, 0x00, 0x00, 0x00, 0x45,0x00,0x00,0x24,0x14,0xe0,0x00,0x00,0x40,0x11,0x67,0xe7,0x7f,0x00,0x00,0x01,0x7f,0x00,0x00,0x01,0xfb,0xee,0x30,0x39,0x00,0x10,0x59,0x8f,0x68,0x65,0x6c,0x6c,0x6f,0x38,0x38,0x0a };

static unsigned char p4[64] = { 0x02, 0x00, 0x00, 0x00, 0x45,0x00,0x00,0x25,0x07,0x32,0x00,0x00,0x40,0x11,0x75,0x94,0x7f,0x00,0x00,0x01,0x7f,0x00,0x00,0x01,0xcc,0x65,0x30,0x39,0x00,0x11,0x7d,0xe6,0x68,0x65,0x6c,0x6c,0x6f,0x39,0x39,0x39,0x0a };

static struct iovec iov_array[4] = {
	{ .iov_base = p1, .iov_len = 38 },
	{ .iov_base = p2, .iov_len = 39 },
	{ .iov_base = p3, .iov_len = 40 },
	{ .iov_base = p4, .iov_len = 41 },
};

static void
send_with_one_iovec(pcap_t *pcap)
{
	struct pcap_pkt_hdr_priv pcap_pkt_array[4] = {};

	for (int i = 0; i < 4; i++) {
		pcap_pkt_array[i].pcap_priv_hdr_len = sizeof(struct pcap_pkt_hdr_priv);
		pcap_pkt_array[i].pcap_priv_len = (bpf_u_int32) iov_array[i].iov_len;
		pcap_pkt_array[i].pcap_priv_iov_count = 1;
		pcap_pkt_array[i].pcap_priv_iov_array = &iov_array[i];
	}

	/*
	 * Send packets in increasing number
	 */
	for (u_int i = 1; i <= 4; i++) {
		int retval = pcap_sendpacket_multiple(pcap, i, pcap_pkt_array);
		if (retval < 0) {
			errx(EX_SOFTWARE, "pcap_sendpacket_multiple(%u) failed: %s", i, pcap_geterr(pcap));
		}
		fprintf(stderr, "pcap_sendpacket_multiple(%u) send %d packets\n", i, retval);
		sleep(1);
	}

	sleep(1);
}

static void
send_with_many_iovecv(pcap_t *pcap)
{
	struct pcap_pkt_hdr_priv pcap_pkt_array[4] = {};

	for (int i = 0; i < 4; i++) {
		pcap_pkt_array[i].pcap_priv_hdr_len = sizeof(struct pcap_pkt_hdr_priv);
		pcap_pkt_array[i].pcap_priv_len = (bpf_u_int32) iov_array[i].iov_len;
		pcap_pkt_array[i].pcap_priv_iov_count = pcap_pkt_array[i].pcap_priv_len;
		pcap_pkt_array[i].pcap_priv_iov_array = calloc(pcap_pkt_array[i].pcap_priv_iov_count, sizeof(struct iovec));

		for (int j = 0; j < pcap_pkt_array[i].pcap_priv_iov_count; j++) {
			pcap_pkt_array[i].pcap_priv_iov_array[j].iov_base = (u_char *)iov_array[i].iov_base + j;
			pcap_pkt_array[i].pcap_priv_iov_array[j].iov_len = 1;
		}
	}

	/*
	 * Send packets in increasing number
	 */
	for (u_int i = 1; i <= 4; i++) {
		int retval = pcap_sendpacket_multiple(pcap, i, pcap_pkt_array);
		if (retval < 0) {
			errx(EX_SOFTWARE, "pcap_sendpacket_multiple(%u) failed: %s", i, pcap_geterr(pcap));
		}
		fprintf(stderr, "pcap_sendpacket_multiple(%u) send %d packets\n", i, retval);
		sleep(1);
	}
}



int
main(int argc, const char * argv[])
{
	pcap_t *pcap = pcap_open_live("lo0", 65535, 0, 1000, ebuf);
	if (pcap == NULL) {
		errx(EX_SOFTWARE, "pcap_open_live failed: %s", ebuf);
	}

	fprintf(stderr, "# sending with one iovec before setting send multiple\n");
	send_with_one_iovec(pcap);

	sleep(1);

	fprintf(stderr, "# sending with multiple iovecs before setting send multiple\n");
	send_with_many_iovecv(pcap);

	sleep(1);

	if (pcap_set_send_multiple(pcap, 1) == PCAP_ERROR) {
		errx(EX_SOFTWARE, "pcap_set_send_multiple failed: %s", pcap_geterr(pcap));
	}

	fprintf(stderr, "# sending with one iovec after setting send multiple\n");
	send_with_one_iovec(pcap);

	sleep(1);

	fprintf(stderr, "# sending with multiple iovecs after setting send multiple\n");
	send_with_many_iovecv(pcap);

	sleep(1);

	return 0;
}
