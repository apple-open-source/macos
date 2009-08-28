/*
 * Portions Copyright (c) 1999-2003 Apple Computer, Inc. All Rights
 *  Reserved.
 *
 *  This file contains Original Code and/or Modifications of Original Code
 *  as defined in and that are subject to the Apple Public Source License
 *  Version 2.0 (the 'License'). You may not use this file except in
 *  compliance with the License. Please obtain a copy of the License at
 *  http://www.opensource.apple.com/apsl/ and read it before using this
 *  file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
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
	int32_t	rec_type;
	int32_t	rec_version;    
	int32_t	rec_count;
	int32_t	rec_size;
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
	int32_t	drivepath_id;		/* compressed table id */
	int32_t	state;
	char	BSDName[MAXDRIVENAME + 1];
	io_string_t	ioreg_path;	/* unique id, hardware path */
};
    

struct drivestats
{
	io_registry_entry_t    	driver;

	int32_t			drivepath_id;
	uint64_t		blocksize;

	uint64_t		Reads;
	uint64_t		BytesRead;

	uint64_t		Writes;
	uint64_t		BytesWritten;

	uint64_t		LatentReadTime;
	uint64_t		LatentWriteTime;

	uint64_t		ReadErrors;
	uint64_t		WriteErrors;

	uint64_t		ReadRetries;
	uint64_t		WriteRetries;

	uint64_t		TotalReadTime;
	uint64_t		TotalWriteTime;
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
	char		tname_unit[MAX_TNAME_UNIT_SIZE + 1];
	uint32_t	gen_counter;        /* unit generation counter */
    
	uint64_t	net_ipackets;
	uint64_t	net_ierrors;        
	uint64_t	net_opackets;
	uint64_t	net_oerrors;
	uint64_t	net_collisions;    
	uint64_t	net_ibytes;
	uint64_t	net_obytes;
	uint64_t	net_imcasts;
	uint64_t	net_omcasts;
	uint64_t	net_drops;
};
