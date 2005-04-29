/*
 * Copyright (c) 1983, 1989, 1993
 *    The Regents of the University of California.  All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 */

/*
 * Copyright (c) 1996,1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */




#ifndef _WIN32_RESOLV_H
#define _WIN32_RESOLV_H

#include <sys/types.h>
#include <sys/param.h>



/**
 * Query domain
 *
 * @param name Name of service to query for
 * @param domain Name of domain service is in
 * @param class Class of query
 * @param type Type of record to retrieve
 * @param dstBuffer Space to put received packet
 * @param dstLength Size of received packet buffer
 * @return Size of packet received, or < 0 on failure
 */
int res_querydomain(const char *name,
                    const char *domain,
                    int class, int type,
                    u_char *dstBuffer,
                    int dstLength);




// possible DNS response codes
#define RCODE_OK 0
#undef NOERROR
#define NOERROR 0
#define RCODE_FORMAT 1
#define RCODE_SERVERFAILURE 2
#define RCODE_NAMEERROR 3
#define RCODE_NOTIMPL 4
#define RCODE_REFUSED 5



// ------------------------------------------------------
// Everything below this line is taken from bind v8.2.3
// See copyright notices above


/** 
 * DNS Packet Header format
 */
typedef struct {
        unsigned        id :16;         /* query identification number */
#if BYTE_ORDER == BIG_ENDIAN
                        /* fields in third byte */
        unsigned        qr: 1;          /* response flag */
        unsigned        opcode: 4;      /* purpose of message */
        unsigned        aa: 1;          /* authoritive answer */
        unsigned        tc: 1;          /* truncated message */
        unsigned        rd: 1;          /* recursion desired */
                        /* fields in fourth byte */
        unsigned        ra: 1;          /* recursion available */
        unsigned        unused :1;      /* unused bits (MBZ as of 4.9.3a3) */
        unsigned        ad: 1;          /* authentic data from named */
        unsigned        cd: 1;          /* checking disabled by resolver */
        unsigned        rcode :4;       /* response code */
#endif
#if BYTE_ORDER == LITTLE_ENDIAN || BYTE_ORDER == PDP_ENDIAN
                        /* fields in third byte */
        unsigned        rd :1;          /* recursion desired */
        unsigned        tc :1;          /* truncated message */
        unsigned        aa :1;          /* authoritive answer */
        unsigned        opcode :4;      /* purpose of message */
        unsigned        qr :1;          /* response flag */
                        /* fields in fourth byte */
        unsigned        rcode :4;       /* response code */
        unsigned        cd: 1;          /* checking disabled by resolver */
        unsigned        ad: 1;          /* authentic data from named */
        unsigned        unused :1;      /* unused bits (MBZ as of 4.9.3a3) */
        unsigned        ra :1;          /* recursion available */
#endif
                        /* remaining bytes */
        unsigned        qdcount :16;    /* number of question entries */
        unsigned        ancount :16;    /* number of answer entries */
        unsigned        nscount :16;    /* number of authority entries */
        unsigned        arcount :16;    /* number of resource entries */
} HEADER;


/*
 * Expand compressed domain name 'comp_dn' to full domain name.
 * 'msg' is a pointer to the begining of the message,
 * 'eomorig' points to the first location after the message,
 * 'exp_dn' is a pointer to a buffer of size 'length' for the result.
 * Return size of compressed name or -1 if there was an error.
 */
int dn_expand(const u_char *msg, const u_char *eom, const u_char *src,
              char *dst, int dstsiz);


// Flag bits indicating name compression
#define NS_CMPRSFLGS    0xc0
#define NS_MAXDNAME     1025    /* maximum domain name */
#define NS_MAXCDNAME    255     /* maximum compressed domain name */
#define NS_PACKETSZ     512     /* maximum packet size */


// ------------------------------------------------------
// Other fields

// Query classes
#define C_IN 1
#define C_CS 2
#define C_CH 3
#define C_HS 4

// Query types
#define T_A 1
#define T_NS 2
#define T_MD 3
#define T_MF 4
#define T_CNAME 5
#define T_SOA 6
#define T_MB 7
#define T_MG 8
#define T_MR 9
#define T_NULL 10
#define T_WKS 11
#define T_PTR 12
#define T_HINFO 13
#define T_MINFO 14
#define T_MX 15
#define T_TXT 16
#define T_RP 17
#define T_AFSDB 18
#define T_X25 19
#define T_ISDN 20
#define T_RT 21
#define T_NSAP 22
#define T_NSAP_PTR 23
#define T_SIG 24
#define T_KEY 25
#define T_PX 26
#define T_GPOS 27
#define T_AAAA 28
#define T_LOC 29
#define T_NXT 30
#define T_EID 31
#define T_NIMLOC 32
#define T_SRV 33
#define T_ATMA 34
#define T_NAPTR 35
#define T_KX 36
#define T_CERT 37
#define T_A6 38
#define T_DNAME 39
#define T_SINK 40
#define T_OPT 41
#define T_TSIG 250
#define T_IXFR 251
#define T_AXFR 252
#define T_MAILB 253
#define T_MAILA 254
#define T_ALL 255



#endif
