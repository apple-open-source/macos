/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <net/bpf.h>
#include <string.h>
#include <sys/socket.h>
#include <errno.h>
#include <net/if.h>
#include <netinet/if_ether.h>

#include "bpflib.h"

#define BPF_FORMAT	"/dev/bpf%d"

#ifdef TESTING
#import "util.h"
#endif TESTING

int
bpf_set_timeout(int fd, struct timeval * tv_p)
{
    return (ioctl(fd, BIOCSRTIMEOUT, tv_p));
}

int 
bpf_get_blen(int fd, u_int * blen)
{
    return(ioctl(fd, BIOCGBLEN, blen));
}

int
bpf_dispose(int bpf_fd)
{
    if (bpf_fd >= 0)
	return (close(bpf_fd));
    return (0);
}

int
bpf_new()
{
    char bpfdev[256];
    int i;
    int fd;

    for (i = 0; i < 10; i++) {
	struct stat sb;

	sprintf(bpfdev, BPF_FORMAT, i);
	if (stat(bpfdev, &sb) < 0)
	    return -1;
	fd = open(bpfdev, O_RDWR , 0);
	if (fd < 0) {
	    if (errno != EBUSY)
		return (-1);
	}
	else {
	    return (fd);
	}
    }
    return (-1);
}

int
bpf_setif(int fd, char * en_name)
{
    struct ifreq ifr;

    strcpy(ifr.ifr_name, en_name);
    return (ioctl(fd, BIOCSETIF, &ifr));
}

int
bpf_set_immediate(int fd, u_int value)
{
    return (ioctl(fd, BIOCIMMEDIATE, &value));
}

int
bpf_filter_receive_none(int fd)
{
    struct bpf_insn insns[] = {
	BPF_STMT(BPF_RET+BPF_K, 0),
    };
    struct bpf_program prog;

    prog.bf_len = sizeof(insns) / sizeof(struct bpf_insn);
    prog.bf_insns = insns;
    return ioctl(fd, BIOCSETF, (u_int)&prog);
}

int
bpf_arp_filter(int fd)
{
    struct bpf_insn insns[] = {
	BPF_STMT(BPF_LD+BPF_H+BPF_ABS, 12),
	BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, ETHERTYPE_ARP, 0, 3),
	BPF_STMT(BPF_LD+BPF_H+BPF_ABS, 20),
	BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, ARPOP_REPLY, 0, 1),
	BPF_STMT(BPF_RET+BPF_K, sizeof(struct ether_arp) +
		 sizeof(struct ether_header)),
	BPF_STMT(BPF_RET+BPF_K, 0),
    };
    struct bpf_program prog;

    prog.bf_len = sizeof(insns) / sizeof(struct bpf_insn);
    prog.bf_insns = insns;
    return ioctl(fd, BIOCSETF, (u_int)&prog);
}

int
bpf_write(int fd, void * pkt, int len)
{
    return (write(fd, pkt, len));
}

#ifdef TESTING
void
bpf_read_continuously(int fd, u_int blen)
{
    int n;
    char * rxbuf = malloc(blen);

    printf("rx buf len is %d\n", blen);
    while (1) {
	n = read(fd, rxbuf, blen);
	if (n < 0) {
	    perror("bpf_read_continuously");
	    return;
	}
	if (n == 0)
	    continue;
	printData(rxbuf, n);
    }
}

int
main(int argc, char * argv[])
{
    int fd = bpf_new();
    char * en_name = "en0";
    u_int bpf_blen = 0;

    if (fd < 0) {
	perror("no bpf devices");
	exit(1);
    }
    
    if (argc > 1)
	en_name = argv[1];
    (void)bpf_set_immediate(fd, 1);
    if (bpf_no_packets(fd) < 0) {
	perror("bpf_no_packets");
    }
    if (bpf_attach(fd, en_name) < 0) {
	perror("bpf_attach");
	exit(1);
    }

    if (bpf_get_blen(fd, &bpf_blen) < 0) {
	perror("bpf_get_blen");
	exit(1);
    }
    bpf_read_continuously(fd, bpf_blen);
    exit(0);
    return (0);
}
#endif TESTING
