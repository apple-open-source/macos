/*
 * Copyright (c) 2000-2022 Apple Inc. All rights reserved.
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
 * Copyright (c) 1990, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from the Stanford/CMU enet packet filter,
 * (net/enet.c) distributed as part of 4.3BSD, and code contributed
 * to Berkeley by Steven McCanne and Van Jacobson both of Lawrence
 * Berkeley Laboratory.
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
 *      @(#)bpfdesc.h	8.1 (Berkeley) 6/10/93
 *
 * $FreeBSD: src/sys/net/bpfdesc.h,v 1.14.2.1 2000/03/19 05:55:36 rwatson Exp $
 */

#ifndef _NET_BPFDESC_H_
#define _NET_BPFDESC_H_
#include <sys/appleapiopts.h>
#ifdef KERNEL_PRIVATE
/*
 * The items in this header file should be wrapped in #ifdef KERNEL.
 */

#include <sys/proc.h>
#include <sys/select.h>
#include <kern/thread_call.h>
#include <net/if.h>
#include <uuid/uuid.h>

#if __has_ptrcheck
#define BPF_BIDI_INDEXABLE __bidi_indexable
#else
#define BPF_BIDI_INDEXABLE
#endif

/*
 * Descriptor associated with each open bpf file.
 */
struct bpf_d {
	struct bpf_d    *bd_next;       /* Linked list of descriptors */
	/*
	 * Buffer slots: two mbuf clusters buffer the incoming packets.
	 *   The model has three slots.  Sbuf is always occupied.
	 *   sbuf (store) - Receive interrupt puts packets here.
	 *   hbuf (hold) - When sbuf is full, put cluster here and
	 *                 wakeup read (replace sbuf with fbuf).
	 *   fbuf (free) - When read is done, put cluster here.
	 * On receiving, if sbuf is full and fbuf is 0, packet is dropped.
	 *
	 * Ideally, these buffers would all be marked with
	 * __sized_by(bd_bufsize), but due to how they are rotated, it's
	 * not possible to maintain that relationship.
	 */
	caddr_t BPF_BIDI_INDEXABLE bd_sbuf; /* store slot */
	caddr_t BPF_BIDI_INDEXABLE bd_hbuf; /* hold slot */
	caddr_t BPF_BIDI_INDEXABLE bd_fbuf; /* free slot */
	uint32_t        bd_slen;        /* current length of store buffer */
	uint32_t        bd_hlen;        /* current length of hold buffer */
	uint32_t        bd_scnt;        /* number of packets in store buffer */
	uint32_t        bd_hcnt;        /* number of packets in hold buffer */

	uint32_t        bd_bufsize;     /* absolute length of buffers */
	bool            bd_hbuf_read;   /* reading from hbuf */
	bool            bd_hbuf_write;  /* writing on device */
	bool            bd_headdrop;    /* Keep newer packets */

	uint32_t        bd_write_size_max;      /* max length of packet when writing */

	uint32_t        bd_rtout;       /* Read timeout in 'ticks' */
	struct bpf_if   *bd_bif;        /* interface descriptor */
	struct bpf_insn *__counted_by(bd_filter_len) bd_filter; /* filter code */
	uint32_t        bd_filter_len;  /* filter code length  */
	uint64_t        bd_rcount;      /* number of packets received */
	uint64_t        bd_dcount;      /* number of received packets dropped */
	uint64_t        bd_fcount;      /* number of received packets which matched filter */

	uint64_t        bd_wcount;      /* number of packets written */
	uint64_t        bd_wdcount;     /* number of packets dropped during a write */

	uint8_t         bd_promisc;     /* true if listening promiscuously */
	uint8_t         bd_state;       /* idle, waiting, or timed out */
	uint8_t         bd_immediate;   /* true to return on packet arrival */
	uint32_t        bd_dev_minor;   /* for logging */
	int             bd_async;       /* non-zero if packet reception should generate signal */
	int             bd_sig;         /* signal to send upon packet reception */
	pid_t           bd_sigio;

	struct selinfo  bd_sel;         /* bsd select info */

	int             bd_hdrcmplt;    /* false to fill in src lladdr automatically */
	u_int           bd_direction;   /* direction of packets to see */
	int             bd_oflags;      /* device open flags */
	thread_call_t   bd_thread_call; /* for BPF timeouts with select */
	int             bd_traffic_class; /* traffic service class */
	int             bd_flags;       /* flags */
	int             bd_tstamp;      /* select time stamping function */

	int             bd_refcnt;
#define BPF_REF_HIST    4               /* how many callers to keep around */
	void            *bd_ref_lr[BPF_REF_HIST];
	void            *bd_unref_lr[BPF_REF_HIST];
	int             bd_next_ref_lr;
	int             bd_next_unref_lr;

	struct proc     *bd_opened_by;
	uuid_t          bd_uuid;
	pid_t           bd_pid;

	uint8_t         bd_prev_slen;
	caddr_t BPF_BIDI_INDEXABLE bd_prev_sbuf;
	caddr_t BPF_BIDI_INDEXABLE bd_prev_fbuf;

	struct bpf_comp_stats bd_bcs;
};

/* Values for bd_state */
#define BPF_IDLE                0    /* no select in progress or kqueue pending */
#define BPF_WAITING             1    /* waiting for read timeout in select/kqueue */
#define BPF_TIMED_OUT   2        /* read timeout has expired in select/kqueue */
#define BPF_DRAINING    3        /* waiting for timeout routine to finish during close */

/* Test whether a BPF is ready for read(). */
#define bpf_ready(bd)   ((bd)->bd_hlen != 0 ||          \
	                 (((bd)->bd_immediate || (bd)->bd_state == BPF_TIMED_OUT) && \
	                  (bd)->bd_slen != 0))

/* Values for bd_flags */
#define BPF_EXTENDED_HDR        0x0001  /* process req. the extended header */
#define BPF_WANT_PKTAP          0x0002  /* knows how to handle DLT_PKTAP */
#define BPF_FINALIZE_PKTAP      0x0004  /* finalize pktap header on read */
#define BPF_KNOTE               0x0008  /* kernel note attached */
#define BPF_DETACHING           0x0010  /* bpf_d is being detached */
#define BPF_DETACHED            0x0020  /* bpf_d is detached */
#define BPF_CLOSING             0x0040  /* bpf_d is being closed */
#define BPF_TRUNCATE            0x0080  /* truncate the packet payload */
#define BPF_PKTHDRV2            0x0100  /* pktap header version 2 */
#define BPF_COMP_REQ            0x0200  /* compression requested */
#define BPF_COMP_ENABLED        0x0400  /* compression enabled */
#define BPF_BATCH_WRITE         0x0800  /* batch write enabled */
#define BPF_DIVERT_IN           0x1000  /* divert input */

/*
 * Descriptor associated with each attached hardware interface.
 */
struct bpf_if {
	struct bpf_if *bif_next;        /* list of all interfaces */
	struct bpf_d *bif_dlist;        /* descriptor list */
	uint32_t bif_dlt;                  /* link layer type */
	uint32_t bif_hdrlen;               /* length of header (with padding) */
	uint32_t bif_exthdrlen;            /* length of ext header */
	uint32_t bif_comphdrlen;        /* length of compressed header */
	struct ifnet *bif_ifp;          /* corresponding interface */
	bpf_send_func   bif_send;
	bpf_tap_func    bif_tap;
};

#endif /* KERNEL_PRIVATE */

#endif
