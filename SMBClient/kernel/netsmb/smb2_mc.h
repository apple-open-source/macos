/*
 * Copyright (c) 2019 Apple Inc. All rights reserved.
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

#ifndef smb2_mc_h
#define smb2_mc_h

#include <sys/types.h>
#include <sys/queue.h>
#include <stdint.h>
#include <net/if_media.h>
#include <net/if.h>
#include <netsmb/smb_dev.h>
#include <netsmb/smb_2.h>
#include <sys/lock.h>

/*
 * The raw NIC's info coming from the client and the server
 * Contains only one IP address
 * will be used to construct the complete_nic_info_entry
 */
struct network_nic_info {
    uint32_t next_offset;
    uint32_t nic_index;
    uint32_t nic_caps;
    uint64_t nic_link_speed;
    uint32_t nic_type;
    in_port_t port;
    union { // alloc the largest sock addr possible
        struct sockaddr         addr;
        struct sockaddr_in      addr_4;
        struct sockaddr_in6     addr_16;
        struct sockaddr_storage addr_strg; // A place holder to make sure enough memory is allocated
                                           // for the largest possible sockaddr (ie NetBIOS's sockaddr_nb)
    };
};

/*
 * List of NIC's IPs
 */
struct sock_addr_entry {
    struct sockaddr* addr;
    TAILQ_ENTRY(sock_addr_entry) next;
};
TAILQ_HEAD(sock_addr_list, sock_addr_entry);

/*
 * NIC's IP types
 */
enum {
    SMB2_MC_IPV4   = 0x01,
    SMB2_MC_IPV6   = 0x02,
};

typedef enum _SMB2_MC_CON_STATE {
    SMB2_MC_STATE_POTENTIAL          = 0x00,
    SMB2_MC_STATE_NO_POTENTIAL       = 0x01, /* NICs doesn't have potential for connection (mainly ip mismatch). */
    SMB2_MC_STATE_IN_TRIAL           = 0x02, /* this connection sent to connect flow */
    SMB2_MC_STATE_FAILED_TO_CONNECT  = 0x03, /* this connection failed in connect flow */
    SMB2_MC_STATE_CONNECTED          = 0x04, /* this connection is being used */
    SMB2_MC_STATE_SURPLUS            = 0x05, /* this connection can't be used since one of it's NICs
                                              * is being used in another connection. */
    SMB2_MC_STATE_IN_REMOVAL         = 0x06, /* this connection sent to removal flow because it was no longer
                                              * needed (redundant, too slow, etc). It will switch to potential
                                              * once the iod is destroyed */
}_SMB2_MC_CON_STATE;

typedef enum _SMB2_MC_CON_ACTIVE_STATE {
    SMB2_MC_FUNC_ACTIVE              = 0x00, /* the connection is active */
    SMB2_MC_FUNC_INACTIVE            = 0x01, /* the connection is inactive */
    SMB2_MC_FUNC_INACTIVE_REDUNDANT  = 0x02, /* the connection is inactive and redundant */

}_SMB2_MC_CON_ACTIVE_STATE;

/*
 * The complete connection info.
 */
struct session_con_entry {
    _SMB2_MC_CON_STATE        state;        /* the state of the connection couple */
    _SMB2_MC_CON_ACTIVE_STATE active_state; /* the active state of a connected connection couple */
    uint64_t                  con_speed;    /* the min between the 2 nics */

    struct smbiod * iod;      /* pointer to the iod that is responsible
                                 for this connection*/
    
    struct complete_nic_info_entry* con_client_nic;
    struct complete_nic_info_entry* con_server_nic;
    
    TAILQ_ENTRY(session_con_entry) next;         /* link list of all connection entries */
    TAILQ_ENTRY(session_con_entry) client_next;  /* for quick access from client nic */
    TAILQ_ENTRY(session_con_entry) success_next; /* for quick access when
                                                    looking for successful connections */
};
TAILQ_HEAD(connection_info_list, session_con_entry);

typedef enum _SMB2_MC_NIC_STATE {
    SMB2_MC_STATE_IDLE          = 0x00, /* This NIC is ready to be used */
    SMB2_MC_STATE_ON_TRIAL      = 0x01, /* This NIC was sent to connection trial */
    SMB2_MC_STATE_USED          = 0x02, /* This NIC is being used */
    SMB2_MC_STATE_DISCONNECTED  = 0x03, /* An update found this NIC to be disconnected */

}_SMB2_MC_NIC_STATE;

#define SMB2_MC_NIC_IN_BLACKLIST 0x0001

/*
 * The complete nic's info. Contains all available IP addresses.
 * Will be saved in the session, in order to create alternative channels
 */
struct complete_nic_info_entry {
    uint64_t nic_index;       /* the nic's index */
    uint64_t nic_link_speed;  /* the link speed of the nic */
    uint32_t nic_caps;        /* the nic's capabilities */
    uint32_t nic_type;        /* indicates the type of the nic */
    uint8_t  nic_ip_types;    /* indicates the types of IPs this nic has */
    uint32_t nic_flags;       /* Black-listed, etc */
    _SMB2_MC_NIC_STATE nic_state;

    struct sock_addr_list addr_list;
    TAILQ_ENTRY(complete_nic_info_entry) next;
    
    struct connection_info_list possible_connections; /* possible connections list for this client
                                                         mainly for quick access - use client_next */
};
TAILQ_HEAD(interface_info_list, complete_nic_info_entry);

#endif
