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

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOBlockStorageDriver.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/IOBSD.h>

/* record types in sadc raw data output */

#define   SAR_NOTSET        0
#define   SAR_RESTART       1
#define   SAR_TIMESTAMP     2
#define   SAR_NETSTATS      3
#define   SAR_DRIVEPATH     4
#define   SAR_DRIVESTATS    5
#define   SAR_VMSTAT        6
#define   SAR_CPU           7

struct record_hdr
{
    int    rec_type;
    int   rec_version;    
    int   rec_count;
    long  rec_size;
};

#define rec_data rec_size
#define rec_timestamp rec_size

#define MAXDRIVENAME    31      /* largest drive name we allow */

#define DPSTATE_UNINITIALIZED 0
#define DPSTATE_NEW           1
#define DPSTATE_CHANGED       2
#define DPSTATE_ACTIVE        3

struct drivepath
{
    int	        drivepath_id;            /* compressed table id */
    int         state;
    char        BSDName[MAXDRIVENAME + 1];
    io_string_t ioreg_path;              /* unique id, hardware path */
};
    

struct drivestats
{
    io_registry_entry_t    	driver;
    int				drivepath_id;
    u_int64_t			blocksize;
    
    u_int64_t			Reads;    
    u_int64_t			BytesRead;
    
    u_int64_t			Writes;    
    u_int64_t			BytesWritten;
    
    u_int64_t			LatentReadTime;
    u_int64_t			LatentWriteTime;
    
    u_int64_t			ReadErrors;
    u_int64_t			WriteErrors;
    
    u_int64_t			ReadRetries;
    u_int64_t			WriteRetries;    

    u_int64_t			TotalReadTime;
    u_int64_t			TotalWriteTime;
};


/*
 * netstat mode drives the
 * collection of ppp interface data
 */

#define NET_DEV_MODE  0x1            /* Turn on network interface counters */
#define NET_EDEV_MODE 0x2            /* Turn on network interface error counters */
#define NET_PPP_MODE  0x4            /* Include ppp interface counters - further 
                                    * modifies NET_DEV_MODE and NET_EDEV_MODE */

#define MAX_TNAME_SIZE 15
#define MAX_TNAME_UNIT_SIZE 23

struct netstats
{
    char		    tname_unit[MAX_TNAME_UNIT_SIZE + 1];
    unsigned long           gen_counter;        /* unit generation counter */
    
    unsigned long long      net_ipackets;
    unsigned long long      net_ierrors;        
    unsigned long long      net_opackets;
    unsigned long long      net_oerrors;
    unsigned long long      net_collisions;    
    unsigned long long      net_ibytes;
    unsigned long long      net_obytes;
    unsigned long long      net_imcasts;
    unsigned long long      net_omcasts;
    unsigned long long	    net_drops;
};
