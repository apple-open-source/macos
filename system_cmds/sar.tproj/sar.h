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

#define MAXDRIVENAME 31  /* largest drive name we allow */


struct drivestats_report
{
    char                        *next;
    int				present;
    int				avg_count;
    int                         drivepath_id;
    char			name[MAXDRIVENAME+1];
    u_int64_t			blocksize;
    
    u_int64_t			cur_Reads;
    u_int64_t			prev_Reads;
    u_int64_t			avg_Reads;
    
    u_int64_t			cur_BytesRead;
    u_int64_t			prev_BytesRead;
    u_int64_t			avg_BytesRead;

    u_int64_t			cur_Writes;
    u_int64_t			prev_Writes;
    u_int64_t			avg_Writes;
    
    u_int64_t			cur_BytesWritten;
    u_int64_t			prev_BytesWritten;
    u_int64_t			avg_BytesWritten;
    
    u_int64_t			cur_LatentReadTime;
    u_int64_t			prev_LatentReadTime;
    u_int64_t			avg_LatentReadTime;
    
    u_int64_t			cur_LatentWriteTime;
    u_int64_t			prev_LatentWriteTime;
    u_int64_t			avg_LatentWriteTime;
    
    u_int64_t			cur_ReadErrors;
    u_int64_t			prev_ReadErrors;
    u_int64_t			avg_ReadErrors;
    
    u_int64_t			cur_WriteErrors;
    u_int64_t			prev_WriteErrors;
    u_int64_t			avg_WriteErrors;
    
    u_int64_t			cur_ReadRetries;
    u_int64_t			prev_ReadRetries;
    u_int64_t			avg_ReadRetries;
    
    u_int64_t			cur_WriteRetries;
    u_int64_t			prev_WriteRetries;
    u_int64_t			avg_WriteRetries;    

    u_int64_t			cur_TotalReadTime;
    u_int64_t			prev_TotalReadTime;
    u_int64_t			avg_TotalReadTime;
    
    u_int64_t			cur_TotalWriteTime;
    u_int64_t			prev_TotalWriteTime;
    u_int64_t			avg_TotalWriteTime;    
};

struct netstats_report
{
    int                     valid;
    int		            present;
    int                     avg_count;
    unsigned long           gen_counter;
    char                    tname_unit[MAX_TNAME_UNIT_SIZE +1 ];

    unsigned long long      cur_ipackets;
    unsigned long long      prev_ipackets;
    unsigned long long      avg_ipackets;
    
    unsigned long long      cur_ierrors;
    unsigned long long      prev_ierrors;
    unsigned long long      avg_ierrors;    
    
    unsigned long long      cur_opackets;
    unsigned long long      prev_opackets;
    unsigned long long      avg_opackets;

    unsigned long long      cur_oerrors;
    unsigned long long      prev_oerrors;
    unsigned long long      avg_oerrors;    
    
    unsigned long long      cur_collisions;
    unsigned long long      prev_collisions;
    unsigned long long      avg_collisions;

    unsigned long long      cur_ibytes;
    unsigned long long      prev_ibytes;
    unsigned long long      avg_ibytes;
    
    unsigned long long      cur_obytes;
    unsigned long long      prev_obytes;
    unsigned long long      avg_obytes;
    
    unsigned long long      cur_imcasts;
    unsigned long long      prev_imcasts;
    unsigned long long      avg_imcasts;

    unsigned long long      cur_omcasts;
    unsigned long long      prev_omcasts;
    unsigned long long      avg_omcasts;

    unsigned long long      cur_drops;
    unsigned long long      prev_drops;
    unsigned long long      avg_drops;

    
};
