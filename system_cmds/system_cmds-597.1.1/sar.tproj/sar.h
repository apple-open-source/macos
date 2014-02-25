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

#define MAXDRIVENAME 31  /* largest drive name we allow */


struct drivestats_report
{
	char		*next;
	int32_t		present;
	int32_t		avg_count;
	int32_t		drivepath_id;
	char		name[MAXDRIVENAME+1];
	uint64_t	blocksize;
    
	uint64_t	cur_Reads;
	uint64_t	prev_Reads;
	uint64_t	avg_Reads;
    
	uint64_t	cur_BytesRead;
	uint64_t	prev_BytesRead;
	uint64_t	avg_BytesRead;

	uint64_t	cur_Writes;
	uint64_t	prev_Writes;
	uint64_t	avg_Writes;
    
	uint64_t	cur_BytesWritten;
	uint64_t	prev_BytesWritten;
	uint64_t	avg_BytesWritten;
    
	uint64_t	cur_LatentReadTime;
	uint64_t	prev_LatentReadTime;
	uint64_t	avg_LatentReadTime;
    
	uint64_t	cur_LatentWriteTime;
	uint64_t	prev_LatentWriteTime;
	uint64_t	avg_LatentWriteTime;
    
	uint64_t	cur_ReadErrors;
	uint64_t	prev_ReadErrors;
	uint64_t	avg_ReadErrors;
    
	uint64_t	cur_WriteErrors;
	uint64_t	prev_WriteErrors;
	uint64_t	avg_WriteErrors;
    
	uint64_t	cur_ReadRetries;
	uint64_t	prev_ReadRetries;
	uint64_t	avg_ReadRetries;
    
	uint64_t	cur_WriteRetries;
	uint64_t	prev_WriteRetries;
	uint64_t	avg_WriteRetries;    

	uint64_t	cur_TotalReadTime;
	uint64_t	prev_TotalReadTime;
	uint64_t	avg_TotalReadTime;
    
	uint64_t	cur_TotalWriteTime;
	uint64_t	prev_TotalWriteTime;
	uint64_t	avg_TotalWriteTime;    
};

struct netstats_report
{
	int32_t		valid;
	int32_t		present;
	int32_t		avg_count;
	uint32_t	gen_counter;
	char		tname_unit[MAX_TNAME_UNIT_SIZE +1 ];

	uint64_t	cur_ipackets;
	uint64_t	prev_ipackets;
	uint64_t	avg_ipackets;
    
	uint64_t	cur_ierrors;
	uint64_t	prev_ierrors;
	uint64_t	avg_ierrors;    
    
	uint64_t	cur_opackets;
	uint64_t	prev_opackets;
	uint64_t	avg_opackets;

	uint64_t	cur_oerrors;
	uint64_t	prev_oerrors;
	uint64_t	avg_oerrors;    
    
	uint64_t	cur_collisions;
	uint64_t	prev_collisions;
	uint64_t	avg_collisions;

	uint64_t	cur_ibytes;
	uint64_t	prev_ibytes;
	uint64_t	avg_ibytes;
    
	uint64_t	cur_obytes;
	uint64_t	prev_obytes;
	uint64_t	avg_obytes;
    
	uint64_t	cur_imcasts;
	uint64_t	prev_imcasts;
	uint64_t	avg_imcasts;

	uint64_t	cur_omcasts;
	uint64_t	prev_omcasts;
	uint64_t	avg_omcasts;

	uint64_t	cur_drops;
	uint64_t	prev_drops;
	uint64_t	avg_drops;
};
