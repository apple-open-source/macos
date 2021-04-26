//
//  main.c
//  mc_support_tester
//
//  Created by Oded Shoshani on 08/09/2020.
//

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#define MC_TESTER   // fake mc code to think we're running at the kernel space

// fake environment
#define lck_mtx_t pthread_mutex_t
#define lck_grp_attr_t uint32_t
#define lck_grp_t      uint32_t
#define lck_attr_t     uint32_t
#define mbuf_t         char
#define vfs_context_t  uint32_t

lck_grp_attr_t *session_st_grp_attr;
lck_grp_t *session_st_lck_group;
lck_attr_t *session_st_lck_attr;

typedef struct uio {} uio_t;
typedef struct au_asid {} au_asid_t;

#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))

#define SMB_MALLOC(addr, cast, size, type, flags) do { (addr) = (cast)calloc((size), (type)); } while(0)
#define SMB_FREE(addr, type) do { free(addr); } while(0)

#define M_WAITOK  0x0000
#define M_NOWAIT  0x0001
#define M_ZERO    0x0004          /* bzero the allocation */
#define M_NULL    0x0008          /* return NULL if space is unavailable*/
#define M_TEMP        80          /* misc temporary data buffers */
#define M_SMBTEMP M_TEMP
#define M_NSMBDEV M_TEMP

#include "smb_conn.h"
#include "smb2_mc_support.h"
#include "netbios.h"

void lck_mtx_init(lck_mtx_t *lck, __unused lck_grp_t *grp, __unused lck_attr_t *attr) {
    pthread_mutex_init(lck, NULL);
}
void lck_mtx_lock(lck_mtx_t *lck) {
    pthread_mutex_lock(lck);
}
void lck_mtx_unlock(lck_mtx_t *lck) {
    pthread_mutex_unlock(lck);
}
void lck_mtx_destroy(lck_mtx_t *lck, __unused lck_grp_t *grp) {
    pthread_mutex_destroy(lck);
}


int copyin(const user_addr_t uaddr, void *kaddr, size_t len) {
    memcpy((void*)kaddr, (void*)uaddr, len);
    return 0;
}
bool smb2_extract_ip_and_len(struct sockaddr *addr, void **addrp, uint32_t *addr_lenp) {

    if (!addr) {
        /* This should never happen */
        SMBERROR("NULL addr.\n");
        return 0;
    }

    if ((!addrp) || (!addr_lenp)) {
        SMBERROR("invalid addrp or addr_lenp.\n");
        return false;
    }

    switch (addr->sa_family) {
        case AF_INET:
        {
            struct sockaddr_in *xin = (void*)addr;
            *addrp     = &xin->sin_addr.s_addr;
            *addr_lenp = sizeof(xin->sin_addr.s_addr);
        }
        return true;

        case AF_NETBIOS:
        {
            struct sockaddr_nb *xnb = (void*)addr;
            *addrp     = &xnb->snb_addrin.sin_addr.s_addr;
            *addr_lenp = sizeof(xnb->snb_addrin.sin_addr.s_addr);
        }
        return true;

        case AF_INET6:
        {
            struct sockaddr_in6 *xin6 = (void*)addr;
            *addrp     = &xin6->sin6_addr;
            *addr_lenp = sizeof(xin6->sin6_addr);
        }
        return true;

        default:
            SMBERROR("invalid sa_family %u.\n", addr->sa_family);
            return false;
    }
}

int smb2_sockaddr_to_str(struct sockaddr *addr, char *str, uint32_t max_str_len) {
    uint32_t str_len = 0;

    if (!addr) {
        /* This should never happen */
        SMBERROR("NULL addr.\n");
        return 0;
    }

    if (!str) {
        /* This should never happen */
        SMBERROR("NULL str.\n");
        return 0;
    }

    uint32_t port = smb2_get_port_from_sockaddr(addr);

    switch (addr->sa_family) {
        case AF_INET:
            str_len = snprintf(str, max_str_len, "IPv4[%u.%u.%u.%u]:%u",
                                 (uint8_t)addr->sa_data[2],
                                 (uint8_t)addr->sa_data[3],
                                 (uint8_t)addr->sa_data[4],
                                 (uint8_t)addr->sa_data[5],
                                 port);
            break;

        case AF_NETBIOS:
            str_len = snprintf(str, max_str_len, "NetBIOS[%u.%u.%u.%u]:%u",
                                 (uint8_t)addr->sa_data[6],
                                 (uint8_t)addr->sa_data[7],
                                 (uint8_t)addr->sa_data[8],
                                 (uint8_t)addr->sa_data[9],
                                 port);
            break;

        case AF_INET6:
            if (addr->sa_len < 22) {
                str_len = snprintf(str, max_str_len, "IPv6[Invalid sa_len]:%u", port);

            } else {
                str_len = snprintf(str, max_str_len, "IPv6[%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x]:%u",
                         (uint8_t)addr->sa_data[6],
                         (uint8_t)addr->sa_data[7],
                         (uint8_t)addr->sa_data[8],
                         (uint8_t)addr->sa_data[9],
                         (uint8_t)addr->sa_data[10],
                         (uint8_t)addr->sa_data[11],
                         (uint8_t)addr->sa_data[12],
                         (uint8_t)addr->sa_data[13],
                         (uint8_t)addr->sa_data[14],
                         (uint8_t)addr->sa_data[15],
                         (uint8_t)addr->sa_data[16],
                         (uint8_t)addr->sa_data[17],
                         (uint8_t)addr->sa_data[18],
                         (uint8_t)addr->sa_data[19],
                         (uint8_t)addr->sa_data[20],
                         (uint8_t)addr->sa_data[21],
                         port);
            }
            break;

        default:
            str_len = snprintf(str, max_str_len, "Unsupported sa_family %u", addr->sa_family);
            break;
    }
    return str_len;
}

uint32_t smb2_get_port_from_sockaddr(struct sockaddr *addr)
{
    uint16_t port = 0;

    if (!addr) {
        /* This should never happen */
        SMBERROR("NULL addr.\n");
        return 0;
    }

    switch(addr->sa_family) {
        case AF_INET:
        {
            struct sockaddr_in *xin = (void*)addr;
            port = ntohs(xin->sin_port);
        }
        break;

        case AF_NETBIOS:
        {
            struct sockaddr_nb *xnb = (void*)addr;
            port = ntohs(xnb->snb_addrin.sin_port);
        }
        break;

        case AF_INET6:
        {
            struct sockaddr_in6 *xin6 = (void*)addr;
            port = ntohs(xin6->sin6_port);
        }
        break;

        default:
            SMBERROR("Unsupported sa_family %u.\n", addr->sa_family);
            break;
    }

    return port;
}


int smb_iod_inactive(struct smbiod *iod) {
    printf("id %d: switch to inactive.\n", iod->iod_id);
    iod->iod_flags |= SMBIOD_INACTIVE_CHANNEL;
    return 0;
}
int smb_iod_active(struct smbiod *iod) {
    printf("id %d: switch to active.\n", iod->iod_id);
    iod->iod_flags &= ~SMBIOD_INACTIVE_CHANNEL;
    return 0;
}

// include code under test 
#include "../kernel/netsmb/smb2_mc_support.c"

enum PrintFlags {
    PRINT_CLIENT      = 0x01,
    PRINT_SERVER      = 0x02,
    PRINT_CONNECTIONS = 0x04,
    PRINT_ALL         = 0x0F
};

char *ConvertNicStateToStr(enum _SMB2_MC_NIC_STATE eNicState) {
    switch(eNicState) {
        case SMB2_MC_STATE_IDLE:
            return("Idle");
        case SMB2_MC_STATE_ON_TRIAL:
            return("On Trial");
        case SMB2_MC_STATE_USED:
            return("In Use");
        case SMB2_MC_STATE_DISCONNECTED:
            return("Disconnected");
        default:
            return("Unknown");
    }
}

char *ConvertConStateToStr(enum _SMB2_MC_CON_STATE eState) {
    switch(eState) {
        case SMB2_MC_STATE_POTENTIAL:
            return("Potential");
        case SMB2_MC_STATE_NO_POTENTIAL:
            return("No Potential");
        case SMB2_MC_STATE_IN_TRIAL:
            return("On Trial");
        case SMB2_MC_STATE_FAILED_TO_CONNECT:
            return("Failed to connect");
        case SMB2_MC_STATE_CONNECTED:
            return("Connected");
        case SMB2_MC_STATE_SURPLUS:
            return("Surplus");
        case SMB2_MC_STATE_IN_REMOVAL:
            return("In Removal");
        default:
            return("Unknown");
    }
}

char * ConvertConActiveStateToStr(enum _SMB2_MC_CON_STATE eState,
                                enum _SMB2_MC_CON_ACTIVE_STATE eActiveState) {
    
    if (eState == SMB2_MC_STATE_CONNECTED) {
        switch(eActiveState) {
            case SMB2_MC_FUNC_ACTIVE:
                return("Active");
            case SMB2_MC_FUNC_INACTIVE:
                return("InActive");
            case SMB2_MC_FUNC_INACTIVE_REDUNDANT:
                return("InActive Redundant");
            default:
                return("Unknown");
        }
    }
    return("");
}

void PrintClient(struct session_network_interface_info *psSessionTable) {
    struct complete_nic_info_entry *psClientNIC;
    TAILQ_FOREACH(psClientNIC, &psSessionTable->client_nic_info_list, next) {
        printf("Client NIC index %3u,         speed %5llu, state %s\n",
               psClientNIC->nic_index, psClientNIC->nic_link_speed, ConvertNicStateToStr(psClientNIC->nic_state));
    }
}
void PrintServer(struct session_network_interface_info *psSessionTable) {
    struct complete_nic_info_entry *psServerNIC;
    TAILQ_FOREACH(psServerNIC, &psSessionTable->server_nic_info_list, next) {
        printf("Server NIC index %3u (RSS_%u), speed %5llu, state %s\n",
               psServerNIC->nic_index &  SMB2_IF_INDEX_MASK,
               psServerNIC->nic_index >> SMB2_IF_RSS_INDEX_SHIFT,
               psServerNIC->nic_link_speed, ConvertNicStateToStr(psServerNIC->nic_state));
    }
}
void PrintConnections(struct session_network_interface_info *psSessionTable) {
    struct session_con_entry *psConEntry;
    struct complete_nic_info_entry *psClientNIC;
    struct complete_nic_info_entry *psServerNIC;
    TAILQ_FOREACH(psConEntry, &psSessionTable->session_con_list, next) {
        psClientNIC = psConEntry->con_client_nic;
        psServerNIC = psConEntry->con_server_nic;
        printf("Connection %p [c- %3u s- %3u (RSS_%u)] Speed %5llu, state %s %s",
               psConEntry,
               psClientNIC->nic_index,
               psServerNIC->nic_index &  SMB2_IF_INDEX_MASK,
               psServerNIC->nic_index >> SMB2_IF_RSS_INDEX_SHIFT,
               psConEntry->con_speed,
               ConvertConStateToStr(psConEntry->state),
               ConvertConActiveStateToStr(psConEntry->state, psConEntry->active_state));
        if (psConEntry->iod)
            printf(" id %u\n", psConEntry->iod->iod_id);
        else
            printf("\n");
    }
}

void PrintSessionTable(struct session_network_interface_info *psSessionTable,
                       char *psHeaderStr,
                       enum PrintFlags ePrintFlags) {
    printf("%s\n", psHeaderStr);
    if (ePrintFlags & PRINT_CLIENT) {
        PrintClient(psSessionTable);
        ePrintFlags &= ~PRINT_CLIENT;
    }
    if (ePrintFlags & PRINT_SERVER) {
        PrintServer(psSessionTable);
        ePrintFlags &= ~PRINT_SERVER;
    }
    if (ePrintFlags & PRINT_CONNECTIONS) {
        PrintConnections(psSessionTable);
        ePrintFlags &= ~PRINT_CONNECTIONS;
    }
}

uint32_t gpClientNicDownArray[32];
uint32_t guClientNicDownEnable = 0;

void AddToClientNICDownList(uint32_t uClinetNicIndex) {
    for(unsigned u=0; u<32; u++) {
        if ((1<<u) & guClientNicDownEnable)
            continue;
        gpClientNicDownArray[u] = uClinetNicIndex;
        guClientNicDownEnable  |= (1<<u);
        break;
    }
}

void RemoveFromClientNICDownList(uint32_t uClinetNicIndex) {
    for(unsigned u=0; u<32; u++) {
        if ((1<<u) & guClientNicDownEnable) {
            if (gpClientNicDownArray[u] == uClinetNicIndex) {
                gpClientNicDownArray[u] = 0;
                guClientNicDownEnable  &= ~(1<<u);
            }
        }
    }
}

bool IsClientNICInDownList(uint32_t uClinetNicIndex) {
    for(unsigned u=0; u<32; u++) {
        if ((1<<u) & guClientNicDownEnable) {
            if (gpClientNicDownArray[u] == uClinetNicIndex) {
                return true;
            }
        }
    }
    return false;
}

void ClearNICDownList(void) {
    guClientNicDownEnable = 0x0;
}

int SubnetMatchOnConnection(struct session_network_interface_info *psSessionTable,
                            struct session_con_entry *psConEntry,
                            uint8_t pSubnet[4]) {
    int iMatch = 1;
    
    struct complete_nic_info_entry *psClientNIC    = psConEntry->con_client_nic;
    struct sockaddr                *psClientIPaddr = TAILQ_FIRST(&psClientNIC->addr_list)->addr;
    
    struct complete_nic_info_entry *psServerNIC    = psConEntry->con_server_nic;
    struct sockaddr                *psServerIPaddr = TAILQ_FIRST(&psServerNIC->addr_list)->addr;

    for(int i=0; i<4; i++) {
        if ( (psClientIPaddr->sa_data[i] & pSubnet[i]) != (psServerIPaddr->sa_data[i] & pSubnet[i])) {
            iMatch = 0;
            break;
        }
    }
    return iMatch;
}

struct iod_list {
    struct smbiod *psIod;
    TAILQ_ENTRY(iod_list) next;
};
TAILQ_HEAD(iod_list_head, iod_list);
struct iod_list_head gsIodList;

struct smbiod *mc_tester_iod_create(struct session_con_entry *con_entry) {
    static int giID = 0;
    
    struct smbiod *psIod = calloc(sizeof(struct smbiod), 1);
    psIod->iod_id = giID++;
    
    struct iod_list *psIodListElm = calloc(sizeof(struct iod_list), 1);
    if (con_entry) {
        psIod->iod_conn_entry.con_entry = con_entry;
        if (con_entry->active_state == SMB2_MC_FUNC_INACTIVE) {
            psIod->iod_flags |= SMBIOD_INACTIVE_CHANNEL;
        }
    }
    
    psIodListElm->psIod = psIod;
    TAILQ_INSERT_HEAD(&gsIodList, psIodListElm, next);
    
    return psIod;
}

struct smbiod *FindIodBySpeed(uint64_t uSpeed) {
    struct iod_list *psIodList;
    TAILQ_FOREACH(psIodList, &gsIodList, next) {
        if (psIodList->psIod->iod_conn_entry.con_entry->state == SMB2_MC_STATE_CONNECTED) {
            if (psIodList->psIod->iod_conn_entry.con_entry->con_speed == uSpeed) {
                return psIodList->psIod;
            }
        }
    }
    return NULL;
}

struct smbiod *FindIodByClientNicSpeed(uint64_t uSpeed) {
    struct iod_list *psIodList;
    TAILQ_FOREACH(psIodList, &gsIodList, next) {
        if (psIodList->psIod->iod_conn_entry.con_entry->state == SMB2_MC_STATE_CONNECTED) {
            if (psIodList->psIod->iod_conn_entry.con_entry->con_client_nic->nic_link_speed == uSpeed) {
                return psIodList->psIod;
            }
        }
    }
    return NULL;
}

int CountIodsBySpeedAndFunc(uint64_t uSpeed, bool bActive) {
    unsigned uCnt = 0;
    
    struct iod_list *psIodList;
    TAILQ_FOREACH(psIodList, &gsIodList, next) {
        if ((uSpeed==0) ||
            (psIodList->psIod->iod_conn_entry.con_entry->con_speed == uSpeed)) {
            if (psIodList->psIod->iod_conn_entry.con_entry->state == SMB2_MC_STATE_CONNECTED) {
                if (( bActive && (!(psIodList->psIod->iod_flags & SMBIOD_INACTIVE_CHANNEL))) ||
                    (!bActive &&   (psIodList->psIod->iod_flags & SMBIOD_INACTIVE_CHANNEL))) {
                    uCnt++;
                }
            }
        }
    }
    return uCnt;
}

void mc_tester_iod_detach_con_entry(struct session_network_interface_info *psSessionTable, struct smbiod *iod)
{
    if (iod->iod_conn_entry.con_entry)
    {
        lck_mtx_lock(&psSessionTable->interface_table_lck);
        iod->iod_conn_entry.con_entry->iod = NULL;
        lck_mtx_unlock(&psSessionTable->interface_table_lck);
    }
}


void mc_tester_iod_destroy(struct smbiod *psIod) {
    struct iod_list *psIodList, *psIodListVar;
    TAILQ_FOREACH_SAFE(psIodList, &gsIodList, next, psIodListVar) {
        if (psIod == psIodList->psIod) {
            TAILQ_REMOVE(&gsIodList, psIodList, next);
            free(psIod);
            return;
        }
    }
    printf("iod not found!");
    assert(0);
}
    
int mc_tester_iod_establish_alt_ch(struct session_network_interface_info *psSessionTable) {
    #define MAX_CHANNEL_LIST (12)

    int iError = 0;
    struct session_con_entry *con_entry[MAX_CHANNEL_LIST] = {NULL};
    uint8_t con_entry_len = MAX_CHANNEL_LIST;
    uint32_t num_of_cons_to_try = 0;

    // Soft and remove excessive connections
    do {
        num_of_cons_to_try = smb2_mc_return_excess_connections(psSessionTable, con_entry, con_entry_len);

        for(uint32_t u=0; u<num_of_cons_to_try; u++) {

            SMBDEBUG("Going to remove connection %u from %u [c-%02d s-%02d (RSS %d)].\n",
                     u, num_of_cons_to_try,
                     con_entry[u]->con_client_nic->nic_index,
                     con_entry[u]->con_server_nic->nic_index &  SMB2_IF_INDEX_MASK,
                     con_entry[u]->con_server_nic->nic_index >> SMB2_IF_RSS_INDEX_SHIFT);
            
            struct smbiod *psIod = con_entry[u]->iod;
            iError = smb2_mc_inform_connection_disconnected(psSessionTable,
                                                            con_entry[u]);
            if (iError) {
                printf("smb2_mc_inform_connection_disconnected returned %d.\n", iError);
                goto exit;
            }

            mc_tester_iod_destroy(psIod);
        }
        
    } while (num_of_cons_to_try != 0);

    num_of_cons_to_try = smb2_mc_ask_for_new_connection_trial(psSessionTable, con_entry, con_entry_len);

    if (!num_of_cons_to_try) {
        SMBDEBUG("no more connnections to try.\n");
    }

    for(uint32_t u=0; u<num_of_cons_to_try; u++) {
    
        SMBDEBUG("Trying to establish connection %u from %u [c-%02d s-%02d (RSS %d)].\n",
                 u, num_of_cons_to_try,
                 con_entry[u]->con_client_nic->nic_index,
                 con_entry[u]->con_server_nic->nic_index &  SMB2_IF_INDEX_MASK,
                 con_entry[u]->con_server_nic->nic_index >> SMB2_IF_RSS_INDEX_SHIFT);
        
        uint32_t uClientNicIndex = con_entry[u]->con_client_nic->nic_index;
        uint8_t pSubNet[4] = {0, 255, 255, 255};
        enum _SMB2_MC_CON_TRIAL_STATUS eTrialStatus;

        if ((!IsClientNICInDownList(uClientNicIndex)) &&
            SubnetMatchOnConnection(psSessionTable, con_entry[u], pSubNet)) {
            struct smbiod *psIod = mc_tester_iod_create(con_entry[u]);
            con_entry[u]->iod = psIod;
            eTrialStatus = SMB2_MC_TRIAL_PASSED;
        } else {
            eTrialStatus = SMB2_MC_TRIAL_FAILED;
        }
            
        iError = smb2_mc_report_connection_trial_results(psSessionTable,
                                                         eTrialStatus,
                                                         con_entry[u]);
        if (iError) {
            SMBERROR("smb2_mc_report_alt_ch_status(fail) returned %d.\n", iError);
            return iError;
        }
    }
exit:
    return(iError);
}

enum mc_support_tests {
    MCS_TEST_BASIC,
    MCS_TEST_LAST
};

int BasicTest(enum mc_support_tests eTestNum) {
    int iError = 0;
    
    // init
    struct session_network_interface_info sSessionTable = {0};
    smb2_mc_init(&sSessionTable,
            32,    //  max_channels,
            2,     //  max_rss_channels,
            NULL,  // *ignored_client_nic,
            0,     //  ignored_client_nic_len
            0)     //  prefer_wired
    // load Client NICs (4x100M (3 valid, 1 invalid), 2x10M)
    #define BASIC_TEST_NUM_OF_CLIENT_NICS 6
    struct network_nic_info psClientNICs[10] =
    {
        [0] = {
            .next_offset = sizeof(struct network_nic_info),
            .nic_index   = 50,
            .nic_caps    = SMB2_IF_CAP_RSS_CAPABLE,
            .nic_link_speed = 100,       // 100M
            .nic_type    = IFM_ETHER,    // wired
            .port        = 445,
            .addr = {
                .sa_len = SMB2_MC_IPV4_LEN,
                .sa_family = AF_INET,
                .sa_data = {0, 0, 192, 168, 10, 0 }
            }
        },
        [1] = {
            .next_offset = sizeof(struct network_nic_info),
            .nic_index   = 51,
            .nic_caps    = SMB2_IF_CAP_RSS_CAPABLE,
            .nic_link_speed = 100,       // 100M
            .nic_type    = IFM_ETHER,    // wired
            .port        = 445,
            .addr = {
                .sa_len = SMB2_MC_IPV4_LEN,
                .sa_family = AF_INET,
                .sa_data = {0, 0, 192, 168, 10, 1 }
            }
        },
        [2] = {
            .next_offset = sizeof(struct network_nic_info),
            .nic_index   = 52,
            .nic_caps    = SMB2_IF_CAP_RSS_CAPABLE,
            .nic_link_speed = 100,      // 100M
            .nic_type    = IFM_ETHER,    // wired
            .port        = 445,
            .addr = {
                .sa_len = SMB2_MC_IPV4_LEN,
                .sa_family = AF_INET,
                .sa_data = {0, 0, 192, 168, 10, 2 }
            },
        },
        [3] = {   // subnet mismatch
            .next_offset = sizeof(struct network_nic_info),
            .nic_index   = 53,
            .nic_caps    = SMB2_IF_CAP_RSS_CAPABLE,
            .nic_link_speed = 100,       // 100M
            .nic_type    = IFM_ETHER,    // wired
            .port        = 445,
            .addr = {
                .sa_len = SMB2_MC_IPV4_LEN,
                .sa_family = AF_INET,
                .sa_data = {0, 0, 10, 0, 0, 3 }
            }
        },
        [4] = {   // Lower speed #1
            .next_offset = sizeof(struct network_nic_info),
            .nic_index   = 54,
            .nic_caps    = SMB2_IF_CAP_RSS_CAPABLE,
            .nic_link_speed = 10,        // 10M
            .nic_type    = IFM_ETHER,    // wired
            .port        = 445,
            .addr = {
                .sa_len = SMB2_MC_IPV4_LEN,
                .sa_family = AF_INET,
                .sa_data = {0, 0, 192, 168, 10, 4 }
            }
        },
        [5] = {   // Lower speed #2
            .next_offset = 0,
            .nic_index   = 55,
            .nic_caps    = SMB2_IF_CAP_RSS_CAPABLE,
            .nic_link_speed = 10,        // 10M
            .nic_type    = IFM_ETHER,    // wired
            .port        = 445,
            .addr = {
                .sa_len = SMB2_MC_IPV4_LEN,
                .sa_family = AF_INET,
                .sa_data = {0, 0, 192, 168, 10, 4 }
            }
        },
    };
    struct smbioc_client_interface sClientNICs = {
        .ioc_info_array           = psClientNICs,
        .interface_instance_count = BASIC_TEST_NUM_OF_CLIENT_NICS,
        .total_buffer_size        = BASIC_TEST_NUM_OF_CLIENT_NICS * sizeof(struct network_nic_info),
        .ioc_errno                = 0
    };
    iError = smb2_mc_parse_client_interface_array(&sSessionTable, &sClientNICs);
    if (iError) {
        printf("smb2_mc_parse_client_interface_array returned %d.\n", iError);
        goto exit;
    }
    
    // Load Server NICs (3x100M, RSS capable).
    #define BASIC_TEST_NUM_OF_SERVER_NICS 3
    struct QueryIfInfo {
        uint32_t next_offset;
        uint32_t nic_index;
        uint32_t nic_caps;
        uint32_t reserved;
        uint64_t nic_speed;
        uint16_t ip_family;
        uint16_t ip_port;
        uint8_t  ip_addr[4];
        
    } psQueryIfInfo[BASIC_TEST_NUM_OF_SERVER_NICS] = {
        [0] = {
            .next_offset = sizeof(struct QueryIfInfo),
            .nic_index   = 100,
            .nic_caps    = SMB2_IF_CAP_RSS_CAPABLE,
            .nic_speed   = 100, // 100M
            .ip_family   = SMB2_QUERY_NETWORK_RESPONSE_IPV4_FAMILY,
            .ip_port     = 0,
            .ip_addr     = { 192, 168, 10, 10 },
        },
        [1] = {
            .next_offset = sizeof(struct QueryIfInfo),
            .nic_index   = 101,
            .nic_caps    = SMB2_IF_CAP_RSS_CAPABLE,
            .nic_speed   = 1000, // 1000M
            .ip_family   = SMB2_QUERY_NETWORK_RESPONSE_IPV4_FAMILY,
            .ip_port     = 0,
            .ip_addr     = { 192, 168, 10, 11 },
        },
        [2] = {
            .next_offset = 0,
            .nic_index   = 102,
            .nic_caps    = SMB2_IF_CAP_RSS_CAPABLE,
            .nic_speed   = 100, // 100M
            .ip_family   = SMB2_QUERY_NETWORK_RESPONSE_IPV4_FAMILY,
            .ip_port     = 0,
            .ip_addr     = { 192, 168, 10, 12 },
        },
    };
    iError = smb2_mc_query_info_response_event(&sSessionTable, (void*)psQueryIfInfo, sizeof(psQueryIfInfo));
    if (iError) {
        printf("smb2_mc_query_info_response_event returned %d.\n", iError);
        goto exit;
    }

    // Main report success
    #define MAIN_CLIENT_NIC 0
    #define MAIN_SERVER_NIC 0
    struct sockaddr sClientIp = psClientNICs[MAIN_CLIENT_NIC].addr;
    struct sockaddr sServerIp = {
        .sa_len    = SMB2_MC_IPV4_LEN,
        .sa_family = AF_INET,
        .sa_data   = {
            0,0, // port num
            psQueryIfInfo[MAIN_SERVER_NIC].ip_addr[0],
            psQueryIfInfo[MAIN_SERVER_NIC].ip_addr[1],
            psQueryIfInfo[MAIN_SERVER_NIC].ip_addr[2],
            psQueryIfInfo[MAIN_SERVER_NIC].ip_addr[3],
        }
    };
    
    struct smbiod *psMainIod = mc_tester_iod_create(NULL);
    struct session_con_entry *psMainCon = NULL;
    
    iError = smb2_mc_update_main_channel(
            &sSessionTable,
            &sClientIp,
            &sServerIp,
            &psMainCon,
            psMainIod);
    if (iError) {
        printf("smb2_mc_update_main_channel returned %d.\n", iError);
        goto exit;
    }
    psMainIod->iod_conn_entry.con_entry = psMainCon;
    
    PrintSessionTable(&sSessionTable, "After main connects:", PRINT_ALL);
    
    // Alternate channels connect
    iError = mc_tester_iod_establish_alt_ch(&sSessionTable);
    if (iError) {
        printf("mc_tester_iod_establish_alt_ch returned %d.\n", iError);
        goto exit;
    }
    iError = mc_tester_iod_establish_alt_ch(&sSessionTable);
    if (iError) {
        printf("mc_tester_iod_establish_alt_ch returned %d.\n", iError);
        goto exit;
    }

    PrintSessionTable(&sSessionTable, "After alternate channels connect (expecting Active 3x100M, InActive 1x10M):", PRINT_ALL);

    // Validate: 3x100M Active Connections, 1x10M InActive Connection
    uint32_t num_of_ch = 0;
    if ((num_of_ch = CountIodsBySpeedAndFunc(100, true)) != 3) { //100M
        iError = EINVAL;
        printf("Expecting 3 active channels (found %u).\n", num_of_ch);
        goto exit;
    }
    if ((num_of_ch = CountIodsBySpeedAndFunc(10, false)) != 1) { //10M
        iError = EINVAL;
        printf("Expecting one inactive channel (found %u).\n", num_of_ch);
        goto exit;
    }

    
    // *** 3x100M Connection Report Failure. Disconnect and switch over to InActive channels
    unsigned u=3;
    while(u) {
        struct smbiod *psIod = FindIodBySpeed(100); // 100M
        if (psIod) {
            uint32_t uClinetNicIndex = psIod->iod_conn_entry.con_entry->con_client_nic->nic_index;
            iError = smb2_mc_inform_connection_disconnected(&sSessionTable,
                                                             psIod->iod_conn_entry.con_entry);
            if (iError) {
                printf("smb2_mc_inform_connection_disconnected returned %d.\n", iError);
                goto exit;
            }
            mc_tester_iod_destroy(psIod);
            u--;
            AddToClientNICDownList(uClinetNicIndex);

            iError = mc_tester_iod_establish_alt_ch(&sSessionTable);
            if (iError) {
                printf("mc_tester_iod_establish_alt_ch returned %d.\n", iError);
                goto exit;
            }
            
        } else {
            printf("FindIodBySpeed returned NULL.\n");
            iError = EINVAL;
            goto exit;
        }
    }

    PrintSessionTable(&sSessionTable, "After fast channels have been removed (expecting Active 2x10M, 0 InActive):", PRINT_ALL);

    // Validate: 2x10M Active Connections, 0 InActive Connection
    if ((num_of_ch = CountIodsBySpeedAndFunc(10, true)) != 2) { //10M
        iError = EINVAL;
        printf("Expecting 2 active channels (found %u).\n", num_of_ch);
        goto exit;
    }
    if ((num_of_ch = CountIodsBySpeedAndFunc(0, false)) != 0) { // any speed
        iError = EINVAL;
        printf("Expecting 0 inactive channel (found %u).\n", num_of_ch);
        goto exit;
    }
    
    // Add a Client NIC (1x1000M)
    struct network_nic_info sNewClientNIC =
    {
        .next_offset = 0,
        .nic_index   = 60,
        .nic_caps    = SMB2_IF_CAP_RSS_CAPABLE,
        .nic_link_speed = 1000,      // 1000M
        .nic_type    = IFM_ETHER,    // wired
        .port        = 445,
        .addr = {
            .sa_len = SMB2_MC_IPV4_LEN,
            .sa_family = AF_INET,
            .sa_data = {0, 0, 192, 168, 0, 60 }
        }
    };
    memcpy(&psClientNICs[6], &sNewClientNIC, sizeof(sNewClientNIC));
    psClientNICs[5].next_offset = sizeof(sNewClientNIC);
    struct smbioc_client_interface sNotifierClientInfo_1 = {
        .ioc_info_array           = psClientNICs,
        .interface_instance_count = 7,
        .total_buffer_size        = 7 * sizeof(struct network_nic_info),
        .ioc_errno                = 0
    };

    iError = smb2_mc_notifier_event(&sSessionTable, &sNotifierClientInfo_1);
    if (iError) {
        printf("smb2_mc_notifier_event returned %d.\n", iError);
        goto exit;
    }
    iError = mc_tester_iod_establish_alt_ch(&sSessionTable);
    if (iError) {
        printf("mc_tester_iod_establish_alt_ch returned %d.\n", iError);
        goto exit;
    }

    iError = mc_tester_iod_establish_alt_ch(&sSessionTable);
    if (iError) {
        printf("mc_tester_iod_establish_alt_ch returned %d.\n", iError);
        goto exit;
    }

    PrintSessionTable(&sSessionTable, "After adding a 1000M client interface:", PRINT_ALL);
    
    // Validate we have one active 100M and one inactive 10M
    if ((num_of_ch = CountIodsBySpeedAndFunc(10, false)) != 1) { //10M
        iError = EINVAL;
        printf("Expecting 1 inactive channels (found %u).\n", num_of_ch);
        goto exit;
    }
    if ((num_of_ch = CountIodsBySpeedAndFunc(1000, true)) != 1) {
        iError = EINVAL;
        printf("Expecting 1 active channel (found %u).\n", num_of_ch);
        goto exit;
    }
    
    // Open blocked paths and run notifier again
    ClearNICDownList();
    iError = smb2_mc_notifier_event(&sSessionTable, &sNotifierClientInfo_1);
    if (iError) {
        printf("smb2_mc_notifier_event returned %d.\n", iError);
        goto exit;
    }
    iError = mc_tester_iod_establish_alt_ch(&sSessionTable);
    if (iError) {
        printf("mc_tester_iod_establish_alt_ch returned %d.\n", iError);
        goto exit;
    }

    iError = mc_tester_iod_establish_alt_ch(&sSessionTable);
    if (iError) {
        printf("mc_tester_iod_establish_alt_ch returned %d.\n", iError);
        goto exit;
    }

    PrintSessionTable(&sSessionTable, "After enabling back all client NICs and notifier:", PRINT_ALL);

    // Validate: 1x100M InActive Connection, and 1X1000M Active Connection
    if ((num_of_ch = CountIodsBySpeedAndFunc(1000, true)) != 1) { //1000M
        iError = EINVAL;
        printf("Expecting 1 active channels (found %u).\n", num_of_ch);
        goto exit;
    }
    if ((num_of_ch = CountIodsBySpeedAndFunc(100, false)) != 1) {
        iError = EINVAL;
        printf("Expecting 1 inactive channel (found %u).\n", num_of_ch);
        goto exit;
    }
    


    // Remove all NIC Clients but the 1000M one
    #define BASIC_TEST_NEW_CLIENT_NICS 1
    struct network_nic_info psNewClientNICs[BASIC_TEST_NEW_CLIENT_NICS] =
    {
        [0] = {
            .next_offset = 0,
            .nic_index   = 60,
            .nic_caps    = SMB2_IF_CAP_RSS_CAPABLE,
            .nic_link_speed = 1000,      // 1000M
            .nic_type    = IFM_ETHER,    // wired
            .port        = 445,
            .addr = {
                .sa_len = SMB2_MC_IPV4_LEN,
                .sa_family = AF_INET,
                .sa_data = {0, 0, 192, 168, 0, 60 }
            }
        },
    };
    struct smbioc_client_interface sNotifierClientInfo = {
        .ioc_info_array           = psNewClientNICs,
        .interface_instance_count = BASIC_TEST_NEW_CLIENT_NICS,
        .total_buffer_size        = BASIC_TEST_NEW_CLIENT_NICS * sizeof(struct network_nic_info),
        .ioc_errno                = 0
    };
    
    iError = smb2_mc_notifier_event(&sSessionTable, &sNotifierClientInfo);
    if (iError) {
        printf("smb2_mc_notifier_event returned %d.\n", iError);
        goto exit;
    }
    iError = mc_tester_iod_establish_alt_ch(&sSessionTable);
    if (iError) {
        printf("mc_tester_iod_establish_alt_ch returned %d.\n", iError);
        goto exit;
    }

    PrintSessionTable(&sSessionTable, "After removal of all client interfaces but 1000M:", PRINT_ALL);
    
    // Validate 1x1000M Active + 100M InActive remain connected (notify can not cause a disconnect)
    if ((num_of_ch = CountIodsBySpeedAndFunc(1000, true)) != 1) { //1000M
        iError = EINVAL;
        printf("Expecting 1 active channels (found %u).\n", num_of_ch);
        goto exit;
    }
    if ((num_of_ch = CountIodsBySpeedAndFunc(100, false)) != 1) {
        iError = EINVAL;
        printf("Expecting 1 inactive channel (found %u).\n", num_of_ch);
        goto exit;
    }

    // Disconnect the Client NIC 1000M + Notify with all NICs
    struct smbiod *psIod = FindIodBySpeed(1000); // 1000M
    if (psIod) {
        uint32_t uClinetNicIndex = psIod->iod_conn_entry.con_entry->con_client_nic->nic_index;
        iError = smb2_mc_inform_connection_disconnected(&sSessionTable,
                                                         psIod->iod_conn_entry.con_entry);
        if (iError) {
            printf("smb2_mc_inform_connection_disconnected returned %d.\n", iError);
            goto exit;
        }
        mc_tester_iod_destroy(psIod);
        u--;
        AddToClientNICDownList(uClinetNicIndex);

        iError = mc_tester_iod_establish_alt_ch(&sSessionTable);
        if (iError) {
            printf("mc_tester_iod_establish_alt_ch returned %d.\n", iError);
            goto exit;
        }
        
    } else {
        printf("FindIodBySpeed returned NULL.\n");
        iError = EINVAL;
        goto exit;
    }
    
    iError = smb2_mc_notifier_event(&sSessionTable, &sNotifierClientInfo_1);
    if (iError) {
        printf("smb2_mc_notifier_event returned %d.\n", iError);
        goto exit;
    }
    iError = mc_tester_iod_establish_alt_ch(&sSessionTable);
    if (iError) {
        printf("mc_tester_iod_establish_alt_ch returned %d.\n", iError);
        goto exit;
    }

    iError = mc_tester_iod_establish_alt_ch(&sSessionTable);
    if (iError) {
        printf("mc_tester_iod_establish_alt_ch returned %d.\n", iError);
        goto exit;
    }

    PrintSessionTable(&sSessionTable, "After reinsertion of client interfaces and 1000M I/F blocked:", PRINT_ALL);
    
    // Validate we switch back to the 3x100M + 1x10M
    if ((num_of_ch = CountIodsBySpeedAndFunc(100, true)) != 3) { //100M
        iError = EINVAL;
        printf("Expecting 3 active channels (found %u).\n", num_of_ch);
        goto exit;
    }
    if ((num_of_ch = CountIodsBySpeedAndFunc(10, false)) != 1) {
        iError = EINVAL;
        printf("Expecting 1 inactive channel (found %u).\n", num_of_ch);
        goto exit;
    }

    // Update/add a Client NIC IP address & disconnect.
    // Validate the new ip address is being used.

    // Update/change Client NIC IP address & disconnect.
    // Validate the new ip address is being used.

    // Unblock 1000M + Change Server's 1000M to 100M
    ClearNICDownList();
    iError = smb2_mc_notifier_event(&sSessionTable, &sNotifierClientInfo_1);
    if (iError) {
        printf("smb2_mc_notifier_event returned %d.\n", iError);
        goto exit;
    }
    iError = mc_tester_iod_establish_alt_ch(&sSessionTable);
    if (iError) {
        printf("mc_tester_iod_establish_alt_ch returned %d.\n", iError);
        goto exit;
    }
    psQueryIfInfo[1].nic_speed = 10;
    iError = smb2_mc_query_info_response_event(&sSessionTable, (void*)psQueryIfInfo, sizeof(psQueryIfInfo));
    if (iError) {
        printf("smb2_mc_query_info_response_event returned %d.\n", iError);
        goto exit;
    }
    iError = mc_tester_iod_establish_alt_ch(&sSessionTable);
    if (iError) {
        printf("mc_tester_iod_establish_alt_ch returned %d.\n", iError);
        goto exit;
    }
    iError = mc_tester_iod_establish_alt_ch(&sSessionTable);
    if (iError) {
        printf("mc_tester_iod_establish_alt_ch returned %d.\n", iError);
        goto exit;
    }

    PrintSessionTable(&sSessionTable, "After change server 1000M nic to 10M:", PRINT_ALL);
    // Validate Active 3x100M + InActive 1x10M
    if ((num_of_ch = CountIodsBySpeedAndFunc(100, true)) != 3) { //100M
        iError = EINVAL;
        printf("Expecting 3 active channels (found %u).\n", num_of_ch);
        goto exit;
    }
    if ((num_of_ch = CountIodsBySpeedAndFunc(10, false)) != 1) {
        iError = EINVAL;
        printf("Expecting 1 inactive channel (found %u).\n", num_of_ch);
        goto exit;
    }

    // Change Server's 100M to 1000M
    psQueryIfInfo[1].nic_speed = 1000;
    iError = smb2_mc_query_info_response_event(&sSessionTable, (void*)psQueryIfInfo, sizeof(psQueryIfInfo));
    if (iError) {
        printf("smb2_mc_query_info_response_event returned %d.\n", iError);
        goto exit;
    }
    
    // Kill the 1000M client NIC to force reconnect
    psIod = FindIodByClientNicSpeed(1000); // 1000M
    if (!psIod) {
        printf("FindIodByClientNicSpeed returned NULL.\n");
        iError = EINVAL;
        goto exit;
    }

    iError = smb2_mc_inform_connection_disconnected(&sSessionTable,
                                                     psIod->iod_conn_entry.con_entry);
    if (iError) {
        printf("smb2_mc_inform_connection_disconnected returned %d.\n", iError);
        goto exit;
    }
    mc_tester_iod_destroy(psIod);

    iError = smb2_mc_notifier_event(&sSessionTable, &sNotifierClientInfo_1);
    if (iError) {
        printf("smb2_mc_notifier_event returned %d.\n", iError);
        goto exit;
    }

    iError = mc_tester_iod_establish_alt_ch(&sSessionTable);
    if (iError) {
        printf("mc_tester_iod_establish_alt_ch returned %d.\n", iError);
        goto exit;
    }
    iError = mc_tester_iod_establish_alt_ch(&sSessionTable);
    if (iError) {
        printf("mc_tester_iod_establish_alt_ch returned %d.\n", iError);
        goto exit;
    }
    PrintSessionTable(&sSessionTable, "After change server's 10M nic to 1000M:", PRINT_ALL);
    // Validate Active 1x1000M + InActive 1x100M
    if ((num_of_ch = CountIodsBySpeedAndFunc(1000, true)) != 1) { //1000M
        iError = EINVAL;
        printf("Expecting 1 active channels (found %u).\n", num_of_ch);
        goto exit;
    }
    if ((num_of_ch = CountIodsBySpeedAndFunc(100, false)) != 1) {
        iError = EINVAL;
        printf("Expecting 1 inactive channel (found %u).\n", num_of_ch);
        goto exit;
    }

exit:
    return iError;
}


int main(int argc, const char * argv[]) {
    
    printf("mc_support_tester begin:\n");
    
    enum mc_support_tests eTestNum = 0;
    int iError = 0;
    
    for(eTestNum=0; eTestNum<MCS_TEST_LAST; eTestNum++) {
        printf("Running test %u:\n", eTestNum);
        switch(eTestNum) {
            case MCS_TEST_BASIC:
                iError = BasicTest(eTestNum);
                break;
            default:
                printf("unknown test %u.\n", eTestNum);
                iError = EINVAL;
                break;
        }
        
        if (iError) {
            printf("iError is %d. stop!\n", iError);
            break;
        }
    }
    
    printf("mc_support_tester end.\n");
    return 0;
}

