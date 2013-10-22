/*
 * Copyright (c) 2011 - 2012 Apple Inc. All rights reserved.
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

/*
  * Fun with FIDs
 * (1) SMB2 uses two uint64_t of fid_persistent and fid_volatile
 * (2) SMB1 uses a single uint16_t for the FID
 * (3) User space functions use a single uint64_t for the FID
 *
 * Solution: All functions will pass around a single uint64_t FID.
 * (1) SMB2 will map the uint64_t FID to the two uint64_t SMB2 FID.  This will
 *     done at the very last layer when the SMB2 packets are being built and
 *     in the code that parses out the reply packet.
 * (2) SMB1 will just saves its uint16_t FID inside the uint64_t and will just
 *     assign it or read it to/from a uint16_t temp value.
 * (3) User space functions will remain unchanged and continue to use a 
 *     uint64_t regardless of whether they are using SMB1 or SMB2. This will
 *     also make reconnect "invisible" to the user level since a reconnect will
 *     just update the mapping table.
 *
 */

//#include <sys/msfscc.h>
#include <sys/smb_apple.h>
#include <sys/param.h>
#include <sys/kauth.h>

#include <netsmb/smb.h>
#include <netsmb/smb_2.h>
#include <netsmb/smb_packets_2.h>
#include <netsmb/smb_rq.h>
#include <netsmb/smb_rq_2.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_conn_2.h>
#include <netsmb/smb_tran.h>
#include <netsmb/smb_gss.h>
#include <netsmb/smb_fid.h>
#include <smbfs/smbfs_subr.h>
#include <smbfs/smbfs_subr_2.h>
#include <smbfs/smbfs_node.h>
#include <netsmb/smb_converter.h>

static void smb_fid_insert_new_node(struct smb_share *share, SMB_FID_NODE *node);
static uint32_t smb_fid_one_at_a_time(uint8_t *key, uint32_t len);

/* smb_fid_count_all() is used for Debugging */
uint64_t
smb_fid_count_all(struct smb_share *share)
{
    FID_HASH_TABLE_SLOT *slotPtr;
    SMB_FID_NODE *node, *temp_node;
    uint32_t table_index;
    uint64_t count = 0;
    
    if (share == NULL) {
        SMBERROR("share is null\n");
        return (0);
    }
    
    smb_fid_table_lock(share);
    
    for (table_index = 0; table_index < SMB_FID_TABLE_SIZE; table_index += 1) {
        slotPtr = &share->ss_fid_table[table_index];
        if (slotPtr == NULL) {
            continue;
        }
        
        LIST_FOREACH_SAFE(node, &slotPtr->fid_list, link, temp_node) {
            count++;
        }
    }
    
    smb_fid_table_unlock(share);
    
    return count;
}

void
smb_fid_delete_all(struct smb_share *share)
{
    FID_HASH_TABLE_SLOT *slotPtr;
    SMB_FID_NODE *node, *temp_node;
    uint32_t table_index;

    if (share == NULL) {
        SMBERROR("share is null\n");
        return;
    }
    
    smb_fid_table_lock(share);
    
    for (table_index = 0; table_index < SMB_FID_TABLE_SIZE; table_index += 1) {
        slotPtr = &share->ss_fid_table[table_index];
        if (slotPtr == NULL) {
            continue;
        }
        
        LIST_FOREACH_SAFE(node, &slotPtr->fid_list, link, temp_node) {
            LIST_REMOVE(node, link);
            SMB_FREE(node, M_TEMP);
        }
    }
    
    smb_fid_table_unlock(share);
}

int 
smb_fid_get_kernel_fid(struct smb_share *share, SMBFID fid, int remove_fid,
                       SMB2FID *smb2_fid)
{
    FID_HASH_TABLE_SLOT *slotPtr;
    SMB_FID_NODE *node;
    uint32_t table_index, iter;
    uint32_t found_it = 0;
    int error = EINVAL;
    
    /* cant put it into smp because that is NULL for DCERPC calls to srvsvc */
    
    if (share == NULL) {
        SMBERROR("share is null\n");
        return EINVAL;
    };    
    
    /* handle compound requests */
    if (fid == 0xffffffffffffffff) {
        smb2_fid->fid_persistent = 0xffffffffffffffff;
        smb2_fid->fid_volatile = 0xffffffffffffffff;
        return (0);
    }
    
    smb_fid_table_lock(share);
    
    /* calculate the slot */
    table_index = fid & SMB_FID_TABLE_MASK;
    if (table_index >= SMB_FID_TABLE_SIZE) {
        SMBERROR("Bad table_index: %u for fid %llx\n", table_index, fid);
        goto exit;
    };
    
    slotPtr = &share->ss_fid_table[table_index];
    if (slotPtr == NULL) {
        SMBERROR("slotPtr is null for table_index %u, fid %llx\n", 
                 table_index, fid);
        goto exit;
    };
    
    iter = 0;
    LIST_FOREACH(node, &slotPtr->fid_list, link) {
        if (node->fid == fid) {
            *smb2_fid = node->smb2_fid;
            found_it = 1;
            
            if (remove_fid == 1) {
                /*SMBERROR("remove smb2 fid %llx %llx -> fid %llx\n",
                         node->smb2_fid.fid_persistent,
                         node->smb2_fid.fid_volatile,
                         fid);*/
                LIST_REMOVE(node, link);
				SMB_FREE(node, M_TEMP);
            }
            break;
        }
        iter++;
    }
    
    if (iter >= share->ss_fid_max_iter) {
        share->ss_fid_max_iter = iter;
    }

    if (found_it == 1) {
        /*SMBERROR("fid %llx -> smb2 fid %llx %llx\n",
                 fid,
                 smb2_fid->fid_persistent,
                 smb2_fid->fid_volatile);*/
        error = 0;
    }
    else {
        SMBERROR("No smb2 fid found for fid %llx\n", fid);
    }
exit:    
    smb_fid_table_unlock(share);
    return (error);
}

int 
smb_fid_get_user_fid(struct smb_share *share, SMB2FID smb2_fid, SMBFID *ret_fid)
{
    uint64_t fid, val1, val2;
    SMB_FID_NODE *node;
    int error = EINVAL;
    
    if (share == NULL) {
        SMBERROR("share is null\n");
        return EINVAL;
    };    
    
    smb_fid_table_lock(share);
    
    val1 = smb_fid_one_at_a_time((uint8_t *)&smb2_fid.fid_persistent,
                                 sizeof(smb2_fid.fid_persistent));
    val2 = smb_fid_one_at_a_time((uint8_t *)&smb2_fid.fid_volatile,
                                 sizeof(smb2_fid.fid_volatile));
    
    fid = (val1 << 32) | val2;
    
    SMB_MALLOC(node, SMB_FID_NODE *, sizeof(SMB_FID_NODE), M_TEMP, M_WAITOK);
    if (node != NULL) {
        node->fid = fid;
        node->smb2_fid = smb2_fid;
        
        // insert our new node into the hash table
        smb_fid_insert_new_node(share, node);
        /*SMBERROR("insert smb2 fid %llx %llx -> fid %llx\n",
                 smb2_fid.fid_persistent,
                 smb2_fid.fid_volatile,
                 fid);*/
        *ret_fid = fid;
        error = 0;
    }
    else {
        SMBERROR("malloc failed\n");
        error = ENOMEM;
    }
    
    smb_fid_table_unlock(share);
    return (error);
}

static void
smb_fid_insert_new_node(struct smb_share *share, SMB_FID_NODE *node)
{
    FID_HASH_TABLE_SLOT *slotPtr;
    uint32_t table_index;
    
    // calculate the slot
    table_index = node->fid & SMB_FID_TABLE_MASK;
    DBG_ASSERT(table_index < SMB_FID_TABLE_SIZE);
    
    slotPtr = &share->ss_fid_table[table_index];
    
    if (LIST_EMPTY(&slotPtr->fid_list)) {
    } 
    else {
        share->ss_fid_collisions++;
    }
    LIST_INSERT_HEAD(&slotPtr->fid_list, node, link);
    share->ss_fid_inserted++;
}

/* A quick little hash function
 * Written by Bob Jenkins, and put in the public domain
 * See http://burtleburtle.net/bob/hash/doobs.html
 */
static uint32_t 
smb_fid_one_at_a_time(uint8_t *key, uint32_t len)
{
    uint32_t   hash, i;
    
    for (hash = 0, i = 0; i < len; ++i)
    {
        hash += key[i];
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return (hash);
}


