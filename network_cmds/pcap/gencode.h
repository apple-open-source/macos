/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*	$OpenBSD: gencode.h,v 1.4 1996/07/12 13:19:08 mickey Exp $	*/

/*
 * Copyright (c) 1990, 1991, 1992, 1993, 1994, 1995, 1996
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * @(#) Header: gencode.h,v 1.33 96/06/23 02:21:09 leres Exp (LBL)
 */

/* Address qualifiers. */

#define Q_HOST		1
#define Q_NET		2
#define Q_PORT		3
#define Q_GATEWAY	4
#define Q_PROTO		5

/* Protocol qualifiers. */

#define Q_LINK		1
#define Q_IP		2
#define Q_ARP		3
#define Q_RARP		4
#define Q_TCP		5
#define Q_UDP		6
#define Q_ICMP		7
#define Q_IGMP		8

#define	Q_DECNET	9
#define	Q_LAT		10
#define	Q_MOPRC		11
#define	Q_MOPDL		12

/* Directional qualifiers. */

#define Q_SRC		1
#define Q_DST		2
#define Q_OR		3
#define Q_AND		4

#define Q_DEFAULT	0
#define Q_UNDEF		255

struct stmt {
	int code;
	bpf_int32 k;
};

struct slist {
	struct stmt s;
	struct slist *next;
};

/* 
 * A bit vector to represent definition sets.  We assume TOT_REGISTERS
 * is smaller than 8*sizeof(atomset).
 */
typedef bpf_u_int32 atomset;
#define ATOMMASK(n) (1 << (n))
#define ATOMELEM(d, n) (d & ATOMMASK(n))

/*
 * An unbounded set.
 */
typedef bpf_u_int32 *uset;

/*
 * Total number of atomic entities, including accumulator (A) and index (X).
 * We treat all these guys similarly during flow analysis.
 */
#define N_ATOMS (BPF_MEMWORDS+2)

struct edge {
	int id;
	int code;
	uset edom;
	struct block *succ;
	struct block *pred;
	struct edge *next;	/* link list of incoming edges for a node */
};

struct block {
	int id;
	struct slist *stmts;	/* side effect stmts */
	struct stmt s;		/* branch stmt */
	int mark;
	int longjt;		/* jt branch requires long jump */
	int longjf;		/* jf branch requires long jump */
	int level;
	int offset;
	int sense;
	struct edge et;
	struct edge ef;
	struct block *head;
	struct block *link;	/* link field used by optimizer */
	uset dom;
	uset closure;
	struct edge *in_edges;
	atomset def, kill;
	atomset in_use;
	atomset out_use;
	int oval;
	int val[N_ATOMS];
};

struct arth {
	struct block *b;	/* protocol checks */
	struct slist *s;	/* stmt list */
	int regno;		/* virtual register number of result */
};

struct qual {
	unsigned char addr;
	unsigned char proto;
	unsigned char dir;
	unsigned char pad;
};

struct arth *gen_loadi(int);
struct arth *gen_load(int, struct arth *, int);
struct arth *gen_loadlen(void);
struct arth *gen_neg(struct arth *);
struct arth *gen_arth(int, struct arth *, struct arth *);

void gen_and(struct block *, struct block *);
void gen_or(struct block *, struct block *);
void gen_not(struct block *);

struct block *gen_scode(char *, struct qual);
struct block *gen_ecode(u_char *, struct qual);
struct block *gen_ncode(bpf_u_int32, struct qual);
struct block *gen_proto_abbrev(int);
struct block *gen_relation(int, struct arth *, struct arth *, int);
struct block *gen_less(int);
struct block *gen_greater(int);
struct block *gen_byteop(int, int, int);
struct block *gen_broadcast(int);
struct block *gen_multicast(int);
struct block *gen_inbound(int);

void bpf_optimize(struct block **);
#if __STDC__
__dead void bpf_error(const char *, ...)
    __attribute__((volatile, format (printf, 1, 2)));
#endif

void finish_parse(struct block *);
char *sdup(char *);

struct bpf_insn *icode_to_fcode(struct block *, int *);
int pcap_parse(void);
void lex_init(char *);
void sappend(struct slist *, struct slist *);

/* XXX */
#define JT(b)  ((b)->et.succ)
#define JF(b)  ((b)->ef.succ)
