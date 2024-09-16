/*
 * Copyright (c) 2020 Apple Inc. All rights reserved.
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

#include <uuid/uuid.h>

#include <PrivateHeaders/net/if_var.h>
#include <PrivateHeaders/net/if.h>
#include <PrivateHeaders/sys/sockio.h>
#include <net/if_types.h>
#include <skywalk/os_skywalk_private.h>
#include <skywalk/os_sysctls_private.h>

#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <NetFS/NetFS.h>
#include <NetFS/NetFSPrivate.h>

#include <netsmb/smb_conn.h>
#include <smbclient/smbclient.h>
#include <smbclient/smbclient_internal.h>

#include <ifaddrs.h>
#include <spawn.h>
#include <netsmb/smb_2.h>
#include <netsmb/smb_dev_2.h>


struct smb_ifmedia_desc {
    int     ifmt_word;
    uint64_t speed;
};

/*
 * table for converting IFM_SUBTYPE -> speed[b], based on
 * IFM_SUBTYPE_ETHERNET_DESCRIPTIONS.
 */
static struct smb_ifmedia_desc ifm_subtype_ethernet_descriptions[] = {  \
    { IFM_10_T,                 10000000    },                          \
    { IFM_10_2,                 10000000    },                          \
    { IFM_10_5,                 10000000    },                          \
    { IFM_100_TX,               100000000   },                          \
    { IFM_100_FX,               100000000   },                          \
    { IFM_100_T4,               100000000   },                          \
    { IFM_100_VG,               100000000   },                          \
    { IFM_100_T2,               100000000   },                          \
    { IFM_10_STP,               10000000    },                          \
    { IFM_10_FL,                10000000    },                          \
    { IFM_1000_SX,              1000000000  },                          \
    { IFM_1000_LX,              1000000000  },                          \
    { IFM_1000_CX,              1000000000  },                          \
    { IFM_1000_T,               1000000000  },                          \
    { IFM_HPNA_1,               0           },                          \
    { IFM_10G_LR,               10000000000 },                          \
    { IFM_10G_SR,               10000000000 },                          \
    { IFM_10G_CX4,              10000000000 },                          \
    { IFM_2500_SX,              2500000000  },                          \
    { IFM_10G_LRM,              10000000000 },                          \
    { IFM_10G_TWINAX,           10000000000 },                          \
    { IFM_10G_TWINAX_LONG,      10000000000 },                          \
    { IFM_10G_T,                10000000000 },                          \
    { IFM_40G_CR4,              40000000000 },                          \
    { IFM_40G_SR4,              40000000000 },                          \
    { IFM_40G_LR4,              40000000000 },                          \
    { IFM_1000_KX,              1000000000  },                          \
    { IFM_OTHER,                0           },                          \
    { IFM_10G_KX4,              10000000000 },                          \
    { IFM_10G_KR,               10000000000 },                          \
    { IFM_10G_CR1,              10000000000 },                          \
    { IFM_20G_KR2,              20000000000 },                          \
    { IFM_2500_KX,              2500000000  },                          \
    { IFM_2500_T,               2500000000  },                          \
    { IFM_5000_T,               5000000000  },                          \
    { IFM_50G_PCIE,             50000000000 },                          \
    { IFM_25G_PCIE,             25000000000 },                          \
    { IFM_1000_SGMII,           1000000000  },                          \
    { IFM_10G_SFI,              10000000000 },                          \
    { IFM_40G_XLPPI,            40000000000 },                          \
    { IFM_1000_CX_SGMII,        1000000000  },                          \
    { IFM_40G_KR4,              40000000000 },                          \
    { IFM_10G_ER,               10000000000 },                          \
    { IFM_100G_CR4,             100000000000 },                         \
    { IFM_100G_SR4,             100000000000 },                         \
    { IFM_100G_KR4,             100000000000 },                         \
    { IFM_100G_LR4,             100000000000 },                         \
    { IFM_56G_R4,               56000000000 },                          \
    { IFM_100_T,                100000000   },                          \
    { IFM_25G_CR,               25000000000 },                          \
    { IFM_25G_KR,               25000000000 },                          \
    { IFM_25G_SR,               25000000000 },                          \
    { IFM_50G_CR2,              50000000000 },                          \
    { IFM_50G_KR2,              50000000000 },                          \
    { IFM_25G_LR,               25000000000 },                          \
    { IFM_10G_AOC,              10000000000 },                          \
    { IFM_25G_ACC,              25000000000 },                          \
    { IFM_25G_AOC,              25000000000 },                          \
    { IFM_100_SGMII,            100000000   },                          \
    { IFM_2500_X,               2500000000  },                          \
    { IFM_5000_KR,              5000000000  },                          \
    { IFM_25G_T,                25000000000 },                          \
    { IFM_25G_CR_S,             25000000000 },                          \
    { IFM_25G_CR1,              25000000000 },                          \
    { IFM_25G_KR_S,             25000000000 },                          \
    { IFM_5000_KR_S,            5000000000  },                          \
    { IFM_5000_KR1,             5000000000  },                          \
    { IFM_25G_AUI,              25000000000 },                          \
    { IFM_40G_XLAUI,            40000000000 },                          \
    { IFM_40G_XLAUI_AC,         40000000000 },                          \
    { IFM_40G_ER4,              40000000000 },                          \
    { IFM_50G_SR2,              50000000000 },                          \
    { IFM_50G_LR2,              50000000000 },                          \
    { IFM_50G_LAUI2_AC,         50000000000 },                          \
    { IFM_50G_LAUI2,            50000000000 },                          \
    { IFM_50G_AUI2_AC,          50000000000 },                          \
    { IFM_50G_AUI2,             50000000000 },                          \
    { IFM_50G_CP,               50000000000 },                          \
    { IFM_50G_SR,               50000000000 },                          \
    { IFM_50G_LR,               50000000000 },                          \
    { IFM_50G_FR,               50000000000 },                          \
    { IFM_50G_KR_PAM4,          50000000000 },                          \
    { IFM_25G_KR1,              25000000000 },                          \
    { IFM_50G_AUI1_AC,          50000000000 },                          \
    { IFM_50G_AUI1,             50000000000 },                          \
    { IFM_100G_CAUI4_AC,        100000000000 },                         \
    { IFM_100G_CAUI4,           100000000000 },                         \
    { IFM_100G_AUI4_AC,         100000000000 },                         \
    { IFM_100G_AUI4,            100000000000 },                         \
    { IFM_100G_CR_PAM4,         100000000000 },                         \
    { IFM_100G_KR_PAM4,         100000000000 },                         \
    { IFM_100G_CP2,             100000000000 },                         \
    { IFM_100G_SR2,             100000000000 },                         \
    { IFM_100G_DR,              100000000000 },                         \
    { IFM_100G_KR2_PAM4,        100000000000 },                         \
    { IFM_100G_CAUI2_AC,        100000000000 },                         \
    { IFM_100G_CAUI2,           100000000000 },                         \
    { IFM_100G_AUI2_AC,         100000000000 },                         \
    { IFM_100G_AUI2,            100000000000 },                         \
    { IFM_200G_CR4_PAM4,        200000000000 },                         \
    { IFM_200G_SR4,             200000000000 },                         \
    { IFM_200G_FR4,             200000000000 },                         \
    { IFM_200G_LR4,             200000000000 },                         \
    { IFM_200G_DR4,             200000000000 },                         \
    { IFM_200G_KR4_PAM4,        200000000000 },                         \
    { IFM_200G_AUI4_AC,         200000000000 },                         \
    { IFM_200G_AUI4,            200000000000 },                         \
    { IFM_200G_AUI8_AC,         200000000000 },                         \
    { IFM_200G_AUI8,            200000000000 },                         \
    { IFM_400G_FR8,             400000000000 },                         \
    { IFM_400G_LR8,             400000000000 },                         \
    { IFM_400G_DR4,             400000000000 },                         \
    { IFM_400G_AUI8_AC,         400000000000 },                         \
    { IFM_400G_AUI8,            400000000000 },                         \
    { 0, -1 },                                                        \
};

/*
 * <72436918> Get wireless link speed using SIOCGIFLINKPARAMS
 * Implementation based on ifconfig calculation of wireless link rates
 */
static uint64_t
getWifiLinkSpeed(int fd, char *name)
{
    uint64_t res = 0;
    struct if_linkparamsreq iflpr;

    bzero(&iflpr, sizeof (iflpr));
    strlcpy(iflpr.iflpr_name, name, sizeof (iflpr.iflpr_name));
    if (ioctl(fd, SIOCGIFLINKPARAMS, &iflpr) != -1) {
        u_int64_t ibw_eff = iflpr.iflpr_input_bw.eff_bw;
        u_int64_t obw_eff = iflpr.iflpr_output_bw.eff_bw;
        u_int64_t obw_tbr = iflpr.iflpr_output_tbr_rate;

        if (obw_tbr != 0 && obw_eff > obw_tbr)
            obw_eff = obw_tbr;

        res = MIN(obw_eff, ibw_eff);
    }
    return res;
}

/* 
 * Code for finding number of RSS queues copied from skywalkctl.c
 */
static int
sysctl_get_buf(const char *oid_name, void **buffer, size_t *len)
{
#define SYSCTL_RETRY_MAX 10
    int ret = 0;

    *buffer = NULL;
    for (int try = 0; try < SYSCTL_RETRY_MAX; try++) {
        ret = sysctlbyname(oid_name, NULL, len, NULL, 0);
        if (ret != 0) {
            os_log_error(OS_LOG_DEFAULT, "%s: sysctlbyname for buffer length failed %d retrying",
                         __FUNCTION__, ret);
            continue;
        }

        if (*len == 0) {
            os_log_error(OS_LOG_DEFAULT, "%s: buf len is 0???", __FUNCTION__);
            *buffer = NULL;
            return (0);
        }
        
        *buffer = malloc(*len);
        if (*buffer == NULL) {
            os_log_error(OS_LOG_DEFAULT, "%s: malloc failed", __FUNCTION__);
            return(ENOBUFS);
        }
        
        ret = sysctlbyname(oid_name, *buffer, len, NULL, 0);
        if (ret != 0) {
            if (errno == ENOMEM) {
                os_log_error(OS_LOG_DEFAULT, "%s: sysctlbyname failed with ENOMEM, retrying",
                             __FUNCTION__);
                free(*buffer);
                *buffer = NULL;
                *len = 0;
                continue;
            }
            os_log_error(OS_LOG_DEFAULT, "%s: sysctlbyname failed %d",
                         __FUNCTION__, errno);
            return(errno);
        }
        break;
    }

    return (ret);
}

static uint8_t
getNumberRSSQueues(char *if_name)
{
    int error = 0;
    size_t sns_list_length = 0;
    void *sns_list_buffer = NULL;
    size_t nli_list_length = 0;
    void *nli_list_buffer = NULL;
    struct sk_stats_net_if *sns = NULL;
    void *sns_list_end = NULL;
    struct nx_llink_info *nli = NULL;
    void *nli_list_end = NULL;
    int j = 0;
    struct nx_qset_info *nqi = NULL;
    size_t obj_width = 0;
    
    if (if_name == NULL) {
        os_log_error(OS_LOG_DEFAULT, "%s: if_name is NULL? ", __FUNCTION__);
        /* Assume just one receive queue */
        goto exit;
    }

    sns_list_length = 0;
    sns_list_buffer = NULL;
    obj_width = sizeof(struct sk_stats_net_if);
    error = sysctl_get_buf(SK_STATS_NET_IF, &sns_list_buffer, &sns_list_length);
    if ((error != 0) || (sns_list_buffer == NULL) || (sns_list_length == 0)) {
        os_log_error(OS_LOG_DEFAULT, "%s: SK_STATS_NET_IF failed error [%d] sns_list_buffer %s sns_list_length %zu",
                     __FUNCTION__, error, sns_list_buffer == NULL ? "Null" : "notNull", sns_list_length);
        /* Assume just one receive queue */
        goto exit;
    }
    
    if ((sns_list_length % obj_width) != 0) {
        os_log_error(OS_LOG_DEFAULT, "%s: sns_list_length incorrect ", __FUNCTION__);
        /* Assume just one receive queue */
        goto exit;
    }

    nli_list_length = 0;
    nli_list_buffer = NULL;
    obj_width = sizeof(struct nx_llink_info);
    error = sysctl_get_buf((SK_LLINK_LIST_SYSCTL), &nli_list_buffer, &nli_list_length);
    if ((error != 0) || (nli_list_buffer == NULL) || (nli_list_length == 0)) {
        os_log_error(OS_LOG_DEFAULT, "%s: SK_LLINK_LIST_SYSCTL failed error [%d] nli_list_buffer %s nli_list_length %zu",
                     __FUNCTION__, error, nli_list_buffer == NULL ? "Null" : "notNull", nli_list_length);
        /* Assume just one receive queue */
        goto exit;
    }
    if ((nli_list_length % obj_width) != 0) {
        os_log_error(OS_LOG_DEFAULT, "%s: nli_list_length incorrect ", __FUNCTION__);
        /* Assume just one receive queue */
        goto exit;
    }
    
    sns_list_end = (char*) sns_list_buffer + sns_list_length;

    for (sns = sns_list_buffer; (void *)sns < sns_list_end; sns++) {
        /* Search sns list for matching if_name string */
        if ((strnlen(sns->sns_if_name, IFNAMSIZ) != strnlen(if_name, IFNAMSIZ)) ||
            (strcasestr(sns->sns_if_name, if_name) == NULL)) {
            /* Strings do not match, so skip this one */
            continue;
        }
                
        /* Using the sns uuid, search nli list for a matching uuid */
        nli_list_end = (char *) nli_list_buffer + nli_list_length;
        for (nli = nli_list_buffer; (void *)nli < nli_list_end; nli++) {
            /* Does uuid match? */
            if (uuid_compare(nli->nli_netif_uuid, sns->sns_nx_uuid) == 0) {
                /* 
                 * UUIDs match. Search and see if any of the sets have multiple
                 * rx queues which indicates RSS support
                 */
                for (j = 0; j < nli->nli_qset_cnt; j++) {
                    /*
                     * Note that we can also check nqi_num_tx_queues here
                     * someday if we need it
                     */
                    nqi = &nli->nli_qset[j];

                    //os_log_error(OS_LOG_DEFAULT, "%s: rx_queues %d for <%s> ", __FUNCTION__, nqi->nqi_num_rx_queues, if_name);
                    if (nqi->nqi_num_rx_queues > 1) {
                        return(nqi->nqi_num_rx_queues);
                    }
                }
                
                /* Assume there is only one matching uuid in list */
                /* Assume just one receive queue */
                goto exit;
            }
        }

        break;
    }

exit:
    if (sns_list_buffer != NULL) {
        free(sns_list_buffer);
    }
    
    if (nli_list_buffer != NULL) {
        free(nli_list_buffer);
    }

    /* Assume just one receive queue */
    return(1);
}
            


struct network_interface_info_vector {
    struct network_interface_info_vector* next;
    struct network_nic_info info;
};


static void
releaseNetworkInterfaceVector(struct network_interface_info_vector* vector)
{
    struct network_interface_info_vector* nextItem;
    while (vector != NULL) {
        nextItem = vector->next;
        free(vector);
        vector = nextItem;
    }
}

/*
 * This routine query the Client's interface and build an Interface List
 * so later on we could use it for Multi Channel
 */
static int
createNetworkInterfaceVector(struct network_interface_info_vector** responseVector,
                             uint32_t *vector_length, uint32_t *extra_data_size)
{
    int sockfd = 0;
    int err = 0;
    struct ifaddrs *ifaddr = NULL, *ifa = NULL;
    struct ifmediareq ifmr = {0};
    struct smb_ifmedia_desc *desc = NULL;
    struct network_interface_info_vector* interfaceInfoItem = NULL;
    *vector_length = 0;
    *extra_data_size = 0;
    uint64_t link_speed = 0;
    struct ifreq ifr = {0};

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        os_log_error(OS_LOG_DEFAULT,
                     "%s: failed to open socket, with error [%d]",
                     __FUNCTION__, errno);
        return errno;
    }

    if (getifaddrs(&ifaddr) < 0) {
        os_log_error(OS_LOG_DEFAULT,
                     "%s: failed to get getifaddrs info, with error [%d]",
                     __FUNCTION__, errno);
        close(sockfd);
        return errno;
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        memset(&ifmr, 0, sizeof(struct ifmediareq));
        link_speed = 0;

        /* Skip invalid address */
        if (ifa->ifa_addr == NULL) {
            continue;
        }

        /* We currently support only IPV4 / IPV6 families */
        if (ifa->ifa_addr->sa_family != AF_INET &&
            ifa->ifa_addr->sa_family != AF_INET6) {
            continue;
        }

        /* Query interface media */
        strlcpy(ifmr.ifm_name, ifa->ifa_name, sizeof(ifmr.ifm_name));
        if (ioctl(sockfd, SIOCGIFXMEDIA, &ifmr)) {
            continue;
        }

        /* Skip inactive interfaces */
        if ((ifmr.ifm_status & IFM_ACTIVE) == 0) {
            continue;
        }

        /*
         * <58407760> SIOCGIFXMEDIA is the way to find the link speed of
         * Ethernet ports but does not apply to Wi-Fi.
         */
        if (IFM_TYPE(ifmr.ifm_active) == IFM_ETHER) {
            /* Does the interface support the subtype? */
            if (IFM_SUBTYPE(ifmr.ifm_active) != 0) {
                /* convert IFM_SUBTYPE to speed[b] */
                uint64_t invalid_speed = -1;
                for (desc = &ifm_subtype_ethernet_descriptions[0]; desc->speed != invalid_speed; desc++) {
                    if (IFM_SUBTYPE(ifmr.ifm_active) == desc->ifmt_word) {
                        link_speed = desc->speed;
                    }
                }
            }
            else {
                /*
                 * <104620653> No IFM_SUBTYPE found.
                 * Is it Ethernet over Thunderbolt or is it Ethernet over
                 * Thunderbolt bridge ("bridge#")?
                 *
                 * Note: We can possibly remove this check once <103029620> gets
                 * resolved and we get back an actual Thunderbolt speed to use
                 * from IFM_SUBTYPE
                 */
                strlcpy(ifr.ifr_name, ifa->ifa_name, sizeof(ifr.ifr_name));

                if (ioctl(sockfd, SIOCGIFTYPE, &ifr) != -1) {
                    if ((ifr.ifr_type.ift_type == IFT_ETHER) &&
                        (ifr.ifr_type.ift_subfamily == IFRTYPE_SUBFAMILY_THUNDERBOLT)) {
                        /*
                         * Its Ethernet over Thunderbolt, so hardcode
                         * to 10 gbps
                         */
                        link_speed = 10000000000;
                    }
                    else {
                        if ((ifr.ifr_type.ift_type == IFT_BRIDGE) &&
                            (ifr.ifr_type.ift_family == IFRTYPE_FAMILY_ETHERNET)) {
                            /*
                             * Assume its Ethernet over Thunderbolt bridge, so
                             * hardcode to 10 gbps
                             */
                            link_speed = 10000000000;
                        }
                        else {
                            os_log_error(OS_LOG_DEFAULT, "%s: IFM_SUBTYPE is 0 for interface <%s>?",
                                         __FUNCTION__, ifa->ifa_name);
                        }
                    }
                }
                else {
                    os_log_error(OS_LOG_DEFAULT, "%s: SIOCGIFTYPE failed %d for interface <%s>",
                                 __FUNCTION__, errno, ifr.ifr_name);
                }
            }
        } else if ((IFM_TYPE(ifmr.ifm_active) == IFM_IEEE80211)) {
            link_speed = getWifiLinkSpeed(sockfd, ifa->ifa_name);
        } else {
            // We currently support only Ethernet and WiFi
            continue;
        }

        if (link_speed == 0) {
            os_log_error(OS_LOG_DEFAULT,
                         "%s: could not get %s speed -- set it to 990Mb",
                         __FUNCTION__, ifa->ifa_name);
            link_speed = 990000000;
        }

        if ((*responseVector) == NULL) {
            (*responseVector) = (struct network_interface_info_vector*) malloc(sizeof(struct network_interface_info_vector));
            interfaceInfoItem = *responseVector;
        } else {
            (*responseVector)->next = (struct network_interface_info_vector*) malloc(sizeof(struct network_interface_info_vector));
            (*responseVector) = (*responseVector)->next;
        }

        if ((*responseVector) == NULL) {
            err = ENOMEM;
            goto fail;
        }

        memset((*responseVector), 0, sizeof(struct network_nic_info));

        (*responseVector)->next = NULL;
        (*responseVector)->info.nic_index = if_nametoindex(ifa->ifa_name);

        if (getNumberRSSQueues(ifa->ifa_name) > 1) {
            /* This interface supports client side RSS */
            (*responseVector)->info.nic_caps = SMB2_IF_CAP_RSS_CAPABLE;
        }
        
        (*responseVector)->info.nic_link_speed = link_speed;
        (*responseVector)->info.nic_type = IFM_TYPE(ifmr.ifm_active);
        (*responseVector)->info.port = 0;

        memset((void*) &((*responseVector)->info.addr), 0, ifa->ifa_addr->sa_len);
        memcpy((void*) &((*responseVector)->info.addr), ifa->ifa_addr, ifa->ifa_addr->sa_len);

        *extra_data_size+= ifa->ifa_addr->sa_len;
        (*vector_length)++;
    }

    /* Free memory */
    close(sockfd);
    freeifaddrs(ifaddr);
    *responseVector = interfaceInfoItem;
    return 0;
fail:
    /* Release all allocated interface */
    releaseNetworkInterfaceVector(interfaceInfoItem);
    return err;
}


int
get_client_interfaces( struct smbioc_client_interface *client_interface_update)
{

    int error = 0;
    struct network_interface_info_vector* info_vector = NULL;
    uint32_t extra_sockaddr_size;
    
    bzero(client_interface_update, sizeof(struct smbioc_client_interface));
    error = createNetworkInterfaceVector(&info_vector, &client_interface_update->interface_instance_count, &extra_sockaddr_size);

    if (!error) {
        client_interface_update->total_buffer_size = client_interface_update->interface_instance_count * sizeof(struct network_nic_info) + extra_sockaddr_size;
        client_interface_update->ioc_info_array = (struct network_nic_info*) malloc(client_interface_update->total_buffer_size);
        if (client_interface_update->ioc_info_array != NULL) {
            memset ((void*) client_interface_update->ioc_info_array, 0, client_interface_update->total_buffer_size);
            struct network_interface_info_vector* tmp = info_vector;
            struct network_nic_info* interface_entry = client_interface_update->ioc_info_array;

            while (tmp) {
                memcpy((void*) interface_entry, (void*) &tmp->info, sizeof(struct network_nic_info));
                memcpy((void*) &interface_entry->addr, (void*)&tmp->info.addr, tmp->info.addr.sa_len);
                interface_entry->next_offset = sizeof(struct network_nic_info) + tmp->info.addr.sa_len;

                interface_entry = (struct network_nic_info*)((void*)((uint8_t *)interface_entry + interface_entry->next_offset));
                tmp = tmp->next;
            }
        }
        else
        {
            error = ENOMEM;
        }
        releaseNetworkInterfaceVector(info_vector);
    }

    return error;
}
