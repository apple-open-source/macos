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
/*
 * Copyright (c) 1992 NeXT Computer, Inc.
 *
 * UFS byte swapping routines to make a big endian file system useful on a
 * little endian machine.
 *
 * HISTORY
 *
 * 8 Jul 1992 Brian Pinkerton at NeXT
 *      Created.
 */
/*
 *  ufs_byteorder.h - Externs for endian swapping UFS disk structures.
 *
 *  Copyright (c) 1998-2002 Apple Computer, Inc.
 *
 *  DRI: Josh de Cesare
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/disktab.h>
#include <sys/vnode.h>
#include <sys/buf.h>
#include <dev/disk.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ffs/fs.h>

void byte_swap_ints(int *array, int count);
void byte_swap_shorts(short *array, int count);

void byte_swap_disklabel_in(disk_label_t *dl);
void byte_swap_disktab_in(struct disktab *dt);
void byte_swap_partition(struct partition *part);
void byte_swap_inode_in(struct dinode *dc, struct dinode *ic);
void byte_swap_dir_block_in(char *addr, int count);
