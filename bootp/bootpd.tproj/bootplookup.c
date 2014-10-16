/* * Copyright (c) 2006 Apple Inc. All rights reserved.
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

#include <stdio.h>
#include <servers/bootstrap.h>
#include <DirectoryService/DirServicesConst.h>
#include <opendirectory/DSlibinfoMIG_types.h>
#include <syslog.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <mach/mach.h>
#include <net/ethernet.h>
#include <kvbuf.h>
#include "bootplookup.h"

extern kern_return_t 
libinfoDSmig_GetProcedureNumber(mach_port_t server,
                                proc_name_t name,
                                int32_t *procno,
                                security_token_t *usertoken);

extern kern_return_t
libinfoDSmig_Query(mach_port_t server,
                   int32_t proc,
                   inline_data_t request,
                   mach_msg_type_number_t requestCnt,
                   inline_data_t reply,
                   mach_msg_type_number_t *replyCnt,
                   vm_offset_t *ooreply,
                   mach_msg_type_number_t *ooreplyCnt,
                   security_token_t *usertoken);

typedef struct bootpent
{
    struct bootpent *bp_next;
    char            *bp_name;       // will only be populated at head of list
    char            *bp_bootfile;   // will only be populated at head of list
    char            *bp_hw;
    char            *bp_addr;
    void            *reserved[4];
} bootpent;


enum {
    kDSLUgetbootpbyhw = 0,
    kDSLUgetbootpbyaddr,
    kDSLUlastentry
};

static char *gProcNames[] =
{
    "getbootpbyhw",
    "getbootpbyaddr",
    NULL
};

static bootpent *
dolookup(int32_t inProc, kvbuf_t *inRequest)
{
    static int32_t	    procs[kDSLUlastentry]   = { 0 };
    static mach_port_t	    serverPort		    = MACH_PORT_NULL;
    security_token_t	    userToken;
    kern_return_t	    kr			    = KERN_SUCCESS;
    int32_t		    retryCount		    = 0;
    char		    reply[16384]	    = { 0, };
    mach_msg_type_number_t  replyCnt		    = 0;
    vm_offset_t		    ooreply		    = 0;
    mach_msg_type_number_t  ooreplyCnt		    = 0;
    bootpent		    *returnValue	    = NULL;

    if (inProc > kDSLUlastentry) {
	return NULL;
    }

    do {
	// see if we have a port
	if (serverPort == MACH_PORT_NULL) {
	    kr = bootstrap_look_up(bootstrap_port, kDSStdMachDSLookupPortName, &serverPort);
	    if (kr != KERN_SUCCESS) {
		syslog(LOG_CRIT, "Cannot find bootstrap port for DirectoryService\n");
		return NULL;
	    }
	}

	if (serverPort != MACH_PORT_NULL && procs[inProc] == 0) {
	    kr = libinfoDSmig_GetProcedureNumber(serverPort, gProcNames[inProc], &procs[inProc], &userToken);
	    if (kr != KERN_SUCCESS) {
		syslog(LOG_CRIT, "Cannot find procedure number for lookup %s\n", gProcNames[inProc]);
		return NULL;
	    }
	}

	if (procs[inProc] != 0) {
	    kr = libinfoDSmig_Query(serverPort, procs[inProc], inRequest->databuf, inRequest->datalen, reply, &replyCnt, &ooreply, &ooreplyCnt, &userToken);
	    if (KERN_SUCCESS == kr) {
		uint32_t    length = (replyCnt ? replyCnt : ooreplyCnt);
		char	    *buffer = (replyCnt ? reply : (char *)ooreply);
		kvbuf_t *   kv;

		kv = kvbuf_init(buffer, length);
		if (ooreplyCnt != 0) {
		    vm_deallocate(mach_task_self(), ooreply, ooreplyCnt);
		}

		uint32_t    dictCount	= kvbuf_reset(kv);

		// time to parse this into a list of bootpent...
		if (dictCount > 0) {
		    uint32_t	count	    = 0;
		    char	*key	    = NULL;
		    char	*keys[]	    =	{
						    "bp_name",
						    "bp_bootfile",
						    "bp_hw",
						    "bp_addr"
						};
		    uint32_t	addrCnt	    = 0;
		    uint32_t	hwCnt	    = 0;

		    returnValue = (bootpent *) calloc(1, sizeof(bootpent));
		    returnValue->reserved[0] = kv;

		    kvbuf_next_dict(kv);

		    // expand the results to a list
		    while ((key = kvbuf_next_key(kv, &count)) != NULL) {
			// bp_name
			if (keys[0] != NULL && strcmp(key, keys[0]) == 0) {
			    returnValue->bp_name = kvbuf_next_val(kv);
			    keys[0] = NULL;
			}
			// bp_bootfile
			else if (keys[1] != NULL && strcmp(key, keys[1]) == 0) {
			    returnValue->bp_bootfile = kvbuf_next_val(kv);
			    keys[1] = NULL;
			}
			// bp_hw
			else if (keys[2] != NULL && strcmp(key, keys[2]) == 0) {
			    uint32_t	ii	= 0;
			    bootpent	*entry	= returnValue;

			    do {
				entry->bp_hw = kvbuf_next_val(kv);
				if ((++ii) < count) {
				    if (entry->bp_next == NULL) {
					entry->bp_next = (bootpent *) calloc(1, sizeof(bootpent));
				    }
				    entry = entry->bp_next;
				    continue;
				}
				break;
			    } while (1);

			    keys[2] = NULL;
			    hwCnt = count;
			}
			// bp_addr
			else if (keys[3] != NULL && strcmp(key, keys[3]) == 0) {
			    uint32_t	ii	= 0;
			    bootpent	*entry	= returnValue;

			    do {
				entry->bp_addr = kvbuf_next_val(kv);
				if ((++ii) < count) {
				    if (entry->bp_next == NULL) {
					entry->bp_next = (bootpent *) calloc(1, sizeof(bootpent));
				    }
				    entry = entry->bp_next;
				    continue;
				}
				break;
			    } while (1);

			    keys[3] = NULL;
			    addrCnt = count;
			}
		    }

		    // if our counts aren't the same we need to expand them out
		    if (addrCnt != hwCnt && addrCnt > 0 && hwCnt > 0) {
			// we also trim the list if there are more addr than hw
			bootpent    *currEntry	= returnValue;
			bootpent    *copyEntry	= returnValue;
			int	    ii		= 0;
			int	    expAddr	= (addrCnt < hwCnt);
			int	    expCnt	= (expAddr ? addrCnt : hwCnt);

			while (currEntry != NULL) {
			    if (expAddr) {
				currEntry->bp_addr = copyEntry->bp_addr;
			    }
			    else {
				currEntry->bp_hw = copyEntry->bp_hw;
			    }

			    currEntry = currEntry->bp_next;

			    if ((++ii) < expCnt) {
				// move to the next one
				copyEntry = copyEntry->bp_next;
			    }
			    else {
				// reset to top of list to start copy again
				copyEntry = returnValue;
				ii = 0;
			    }
			}
		    }
		}
		else {
		    kvbuf_free(kv);
		}
	    }
	    else if (MACH_SEND_INVALID_DEST == kr) {
		mach_port_mod_refs(mach_task_self(),
				   serverPort, MACH_PORT_RIGHT_SEND, -1);
		bzero(procs, sizeof(procs));
		serverPort = MACH_PORT_NULL;
		retryCount++;
	    }
	    else {
		return (NULL);
	    }
	}

    } while (KERN_SUCCESS != kr && retryCount < 10);

    return returnValue;
}

static bootpent *
getbootpbyhw(const char *hw)
{
    kvbuf_t *request = kvbuf_new();

    kvbuf_add_dict(request);

    kvbuf_add_key(request, "hw");
    kvbuf_add_val(request, hw);

    bootpent *result = dolookup(kDSLUgetbootpbyhw, request);

    kvbuf_free(request);

    return result;
}

static bootpent *
getbootpbyaddr(const char *addr)
{
    kvbuf_t *request = kvbuf_new();

    kvbuf_add_dict(request);

    kvbuf_add_key(request, "addr");
    kvbuf_add_val(request, addr);

    bootpent *result = dolookup(kDSLUgetbootpbyaddr, request);

    kvbuf_free(request);

    return result;
}

void
freebootpent(bootpent *listhead)
{
    if (listhead == NULL) {
	return;
    }

    if (listhead->reserved[0] == NULL) {
	syslog(LOG_CRIT, "freebootpent called without the head of the entry list");
	abort();
    }

    kvbuf_free(listhead->reserved[0]);

    // free the chain first
    bootpent *nextEntry = listhead->bp_next;
    while (nextEntry != NULL) {
	bootpent *temp = nextEntry->bp_next;
	free(nextEntry);
	nextEntry = temp;
    }

    free(listhead);
}

#define ETHER_ADDR_BUFLEN	sizeof("xx:xx:xx:xx:xx:xx")
#define EI(i)   (u_char)(e->ether_addr_octet[(i)])

static char *
my_ether_ntoa(const struct ether_addr *e,
	      char *buf, int bufLen)
{
    if (bufLen < ETHER_ADDR_BUFLEN) {
	return NULL;
    }

    buf[0] = 0;
    snprintf(buf, bufLen, "%02x:%02x:%02x:%02x:%02x:%02x",
            EI(0), EI(1), EI(2), EI(3), EI(4), EI(5));
    return (buf);
}

#ifdef	NOT_NEEDED
static struct ether_addr *
my_ether_aton(const char *a, struct ether_addr *ea, int eaLen)
{
    register int i;

    if (eaLen < ETHER_ADDR_LEN) {
	return NULL;
    }

    i = sscanf(a, " %x:%x:%x:%x:%x:%x",
	       &ea[0], &ea[1], &ea[2], &ea[3], &ea[4], &ea[5]);
    if (i != 6) {
	return NULL;
    }

    return ea;
}
#endif	// 0

boolean_t
bootp_getbyhw_ds(uint8_t hwtype, void * hwaddr, int hwlen, 
		 subnet_match_func_t * func, void * arg,
		 struct in_addr * iaddr_p, 
		 char * * hostname_p, char * * bootfile_p)
{
    bootpent		*be;
    bootpent		*bp;
    char		buf[ETHER_ADDR_BUFLEN];
    struct in_addr	ia;

    (void) my_ether_ntoa(hwaddr, buf, sizeof(buf));
    be = getbootpbyhw(buf);
    if (be == NULL)
	return FALSE;

    for (bp = be; bp != NULL; bp = bp->bp_next) {
	if ((bp->bp_hw != NULL) && (strcmp(buf, bp->bp_hw) == 0)) {
	    if ((bp->bp_addr == NULL) || (inet_aton(bp->bp_addr, &ia) == 0)
		|| ia.s_addr == 0) {
		/* don't return 0.0.0.0 */
		continue;
	    }
	    
	    if ((func == NULL) || (*func)(arg, ia)) {
		goto done;
	    }
	}
    }

    freebootpent(be);
    return FALSE;

 done :
 
    if (hostname_p != NULL)
	*hostname_p = (be->bp_name != NULL) ? strdup(be->bp_name) : NULL;
    if (bootfile_p != NULL)
	*bootfile_p = (be->bp_bootfile != NULL) ? strdup(be->bp_bootfile) : NULL;
    *iaddr_p = ia;

    freebootpent(be);
    return TRUE;
}

#define INET_ADDR_BUFLEN	sizeof("255.255.255.255")

boolean_t
bootp_getbyip_ds(struct in_addr ciaddr, char * * hostname_p, 
		 char * * bootfile_p)
{
    bootpent	*be;
    bootpent	*bp;
    char	buf[INET_ADDR_BUFLEN];

    (void)inet_ntop(AF_INET, &ciaddr, buf, sizeof(buf));
    be = getbootpbyaddr(buf);
    if (be == NULL)
	return FALSE;

    for (bp = be; bp != NULL; bp = bp->bp_next) {
	if ((bp->bp_addr != NULL) && (strcmp(buf, bp->bp_addr) == 0)) {
	    goto done;
	}
    }

    freebootpent(be);
    return FALSE;

 done :
 
    if (hostname_p != NULL)
	*hostname_p = (be->bp_name != NULL) ? strdup(be->bp_name) : NULL;
    if (bootfile_p != NULL)
	*bootfile_p = (be->bp_bootfile != NULL) ? strdup(be->bp_bootfile) : NULL;

    freebootpent(be);
    return TRUE;
}

#ifdef	TEST_BOOTPLOOKUP

#include <sys/time.h>
#include "util.h"

void
dumpEntry(bootpent *entry)
{
    bootpent *entryTemp;

    printf("  bp_name     = %s\n", entry->bp_name);
    printf("  bp_bootfile = %s\n", entry->bp_bootfile);

    for(entryTemp = entry; entryTemp != NULL; entryTemp = entryTemp->bp_next) {
	printf("\n");
	printf("  bp_hw   = %s\n", entryTemp->bp_hw);
	printf("  bp_addr = %s\n", entryTemp->bp_addr);
    }

    printf("\n");
}

#define HTYPE_ETHER	1

typedef enum {
    isIP,
    isMAC,
    isUnknown
} queryType_t;

int
main(int argc, const char * argv[])
{
    char		buf[256];
    FILE		*f;
    char		*line;
    int			n		= 0;
    int			n_stalls	= 0;
    boolean_t		showRaw		= FALSE;
    struct timeval	tv_elapsed	= { 0, 0 };
    struct timeval	tv_max		= { 0, 0 };
    struct timeval	tv_min		= { 99999, 0 };
	
    if ((argc > 1) && (strcmp(argv[1], "RAW") == 0)) {
	showRaw = TRUE;
	argv++;
	argc--;
    }

    if (argc < 2) {
	f = stdin;
    }
    else {
	f = fopen(argv[1], "r");
	if (f == NULL) {
	    fprintf(stderr, "%s: could not open %s\n", argv[0], argv[1]);
	    exit(1);
	}
    }
    if (isatty(0)) {
	fprintf(stdout, "Enter a MAC address or IP Address"
		" (e.g. 00:0d:93:9d:e7:d7 or 17.203.16.241)\n");
    }
    while ((line = fgets(buf, sizeof(buf), f)) != NULL) {
	bootpent		*entry;
	boolean_t		found	= FALSE;
	struct ether_addr	*hw_address;
	char			*hn	= NULL;
	struct in_addr		ip_address;
	size_t			len;
	queryType_t		qt;
	char			*query;
	struct timeval		tv_end;
	struct timeval		tv_query;
	struct timeval		tv_start;

	len = strlen(line) - 1;
	while ((len >= 0) && (line[len] == '\n')) {
		line[len--] = 0;
	}

	if (inet_aton(line, &ip_address) == 1) {
	    // process IP address
	    qt = isIP;
	    query = inet_ntoa(ip_address);
	    if (showRaw) {
		gettimeofday(&tv_start, 0);
		entry = getbootpbyaddr(query);
	    }
	    else {
		gettimeofday(&tv_start, 0);
		found = bootp_getbyip_ds(ip_address,
					 &hn,
					 NULL);
	    }
	}
	else if ((hw_address = ether_aton(line)) != NULL) {
	    char	buf[ETHER_ADDR_BUFLEN];

	    // process MAC address
	    qt = isMAC;
	    query = my_ether_ntoa(hw_address, buf, sizeof(buf));
	    if (showRaw) {
		gettimeofday(&tv_start, 0);
		entry = getbootpbyhw(query);
	    }
	    else {
		gettimeofday(&tv_start, 0);
		found = bootp_getbyhw_ds(HTYPE_ETHER, hw_address, sizeof(*hw_address),
					 NULL, NULL,
					 &ip_address,
					 &hn,
					 NULL);
	    }
	}
	else {
	    // 'tis neither an IP or MAC address
	    fprintf(stdout, "Invalid entry\n");
	    continue;
	}

	gettimeofday(&tv_end, 0);
	timeval_subtract(tv_end, tv_start, &tv_query);
	if (tv_query.tv_sec == 0) {
	    timeval_add(tv_elapsed, tv_query, &tv_elapsed);
	    n++;
	    
	    if (timeval_compare(tv_query, tv_min) < 0) {
		tv_min = tv_query;
	    }

	    if (timeval_compare(tv_query, tv_max) > 0) {
		tv_max = tv_query;
	    }
	}
	else {
	    n_stalls++;
	}

	if (qt == isIP) {
	    if (showRaw && (entry != NULL)) {
		printf("IP: %-*s                        host: %-20s",
		       (int)INET_ADDR_BUFLEN - 1, query,
		       entry->bp_name);
		dumpEntry(entry);
		freebootpent(entry);
	    }
	    else if (found) {
		printf("IP: %-*s                        host: %-20s",
		       (int)INET_ADDR_BUFLEN - 1, query,
		       hn);
	    }
	    else {
		printf("IP: %-*s                        host: %-20s",
		       (int)INET_ADDR_BUFLEN - 1, query,
		       "NOT FOUND");
	    }
	}
	else if (qt == isMAC) {
	    if (showRaw && (entry != NULL)) {
		printf("IP: %-*s MAC: %s host: %-20s",
		       (int)INET_ADDR_BUFLEN - 1, "",
		       query,
		       entry->bp_name);
		dumpEntry(entry);
		freebootpent(entry);
	    }
	    else if (found) {
		printf("IP: %-*s MAC: %s host: %-20s",
		       (int)INET_ADDR_BUFLEN - 1, inet_ntoa(ip_address),
		       query,
		       hn);
	    }
	    else {
		printf("IP: %-*s MAC: %s host: %-20s",
		       (int)INET_ADDR_BUFLEN - 1, "",
		       query,
		       "NOT FOUND");
	    }
	}

	printf(" (%d.%06d%s)\n",
	       (int)tv_query.tv_sec,
	       (int)tv_query.tv_usec,
	       (tv_query.tv_sec > 0) ? ", stalled?" : "");

	if (hn != NULL) free(hn);
    }

    if (n > 0) {
	time_t	t;

	t = tv_elapsed.tv_sec * USECS_PER_SEC + tv_elapsed.tv_usec;
	t = t / n;

	printf("processed %d queries (min %d.%06d, avg %d.%06d, max %d.%06d)\n",
		       n,
		       (int)tv_min.tv_sec,
		       (int)tv_min.tv_usec,
		       (int)(t / USECS_PER_SEC),
		       (int)(t % USECS_PER_SEC),
		       (int)tv_max.tv_sec,
		       (int)tv_max.tv_usec);
    }

    if (n_stalls > 0) {
	printf("%4d %s took longer than 1 second (and %s\n",
		   n_stalls,
		   (n_stalls == 1) ? "query" : "queries",
		   (n_stalls == 1) ? "was" : "were");
	printf("     not included in the per-query statistics)\n");
    }

    return 0;
}

#endif	// TEST_BOOTPLOOKUP
