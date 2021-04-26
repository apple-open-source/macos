/*
 * Copyright (c) 2020 Apple Inc. All rights reserved
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Apple Inc.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <stdio.h>

#include <smbclient/smbclient.h>
#include <smbclient/smbclient_internal.h>
#include <smbclient/smbclient_private.h>
#include <smbclient/smbclient_netfs.h>
#include <smbclient/ntstatus.h>

#include <netsmb/smb_dev.h>
#include <netsmb/smb_2.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb2_mc.h>

#include <net/if_media.h>

#include "common.h"

#include <json_support.h>


CFMutableDictionaryRef smbMCShares = NULL;


const char *iod_state_to_text[] = {
    [SMBIOD_ST_NOTCONN]        = "not connected",
    [SMBIOD_ST_CONNECT]        = "attempting to connect",
    [SMBIOD_ST_TRANACTIVE]     = "transport on",
    [SMBIOD_ST_NEGOACTIVE]     = "negotiated",
    [SMBIOD_ST_SSNSETUP]       = "in session setup",
    [SMBIOD_ST_SESSION_ACTIVE] = "session active",
    [SMBIOD_ST_DEAD]           = "session is dead",
    [SMBIOD_ST_RECONNECT]      = "reconnecting",
};

#define RSS_MASK  (0xF00000000)
#define RSS_SHIFT (32)

#define SMB2_IF_CAP_RSS_CAPABLE  (0x01)
#define SMB2_IF_CAP_RDMA_CAPABLE (0x02)


#define SMB2_MC_STATE_IDLE          0x00 /* This NIC is ready to be used */
#define SMB2_MC_STATE_ON_TRIAL      0x01 /* This NIC was sent to connection trial */
#define SMB2_MC_STATE_USED          0x02 /* This NIC is being used */
#define SMB2_MC_STATE_DISCONNECTED  0x03 /* An update found this NIC to be disconnected */

const char *state_to_str(uint32_t state);
void print_nic_info(int server_nic, struct nic_properties *prop);
void print_delimiter(FILE *fp, enum OutputFormat output_format);


static const char *
print_iod_state(enum smbiod_state state, uint32_t flags) {
    if (state == SMBIOD_ST_SESSION_ACTIVE) {
        return (flags & SMBIOD_INACTIVE_CHANNEL)? "session inactive" : iod_state_to_text[state];
    } else if (state < sizeof(iod_state_to_text)/sizeof(iod_state_to_text[0])) {
        return iod_state_to_text[state];
    } else {
        return "unknown";
    }
}

static void
speed_to_str(uint64_t speed, char * buf)
{
    int whole, dec;
    if (speed == 0) {
        sprintf(buf, "N/A");
        return;
    }
    if (speed >= 1000000000) {
        whole = (int)(speed / 1000000000);
        dec = (speed / 100000000) % 10;
        sprintf(buf, "%d.%d Gb" , whole, dec);
    } else if (speed >= 1000000) {
        whole = (int)(speed / 1000000);
        dec = (speed / 100000) % 10;
        sprintf(buf, "%d.%d Mb" , whole, dec);
    } else if (speed >= 1000) {
        whole = (int)(speed / 1000);
        dec = (speed / 100) % 10;
        sprintf(buf, "%d.%d Kb" , whole, dec);
    } else {
        sprintf(buf, "%llu b" , speed);
    }
}


static void
address_to_str(struct adress_propreties addr, char * buf)
{
    switch(addr.addr_family) {
        case AF_INET:
        {
            uint8_t *ipv4 = (void*)addr.addr_ipv4;
            sprintf(buf, "%u.%u.%u.%u", ipv4[0], ipv4[1], ipv4[2], ipv4[3]);
        }
        break;
        case AF_INET6:
        {
            uint8_t *ipv6 = (void*)addr.addr_ipv6;
            sprintf(buf, "%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x",
                     ipv6[0], ipv6[1], ipv6[2], ipv6[3], ipv6[4], ipv6[5], ipv6[6], ipv6[7],
                     ipv6[8], ipv6[9], ipv6[10], ipv6[11], ipv6[12], ipv6[13], ipv6[14], ipv6[15]);
        }
        break;
        default:
            sprintf(buf, "[Unknown ip type %u ]", addr.addr_family);
    }
}


static CFMutableDictionaryRef
format_nic_info(struct nic_properties *props, bool is_server)
{
    char buf[40];
    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (dict == NULL) {
        fprintf(stderr, "CFDictionaryCreateMutable failed\n");
        return NULL;
    }

    CFMutableArrayRef ip_addrs = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFMutableArrayRef ip_types = CFArrayCreateMutable(NULL, 2, &kCFTypeArrayCallBacks);
    CFMutableArrayRef caps = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    if (ip_addrs == NULL || ip_types == NULL || caps == NULL) {
        fprintf(stderr, "CFArrayCreateMutable failed\n");
        return NULL;
    }

    sprintf(buf, "%llu", props->if_index);
    json_add_str(dict, "ifindex", buf);

    if (!is_server) {
        if_indextoname((uint32_t)props->if_index, buf);
        json_add_str(dict, "ifname", buf);
        json_add_str(dict, "media_type", IFM_TYPE(props->nic_type) == IFM_ETHER ? "wired" : "wireless");
    } else {
        /*
         * <73460888> For the server we do not know the nic type as it is
         * not reported when we query the server network interfaces so we
         * print NA. we also print NA for server nic names.
         */
        json_add_str(dict, "ifname", "NA");
        json_add_str(dict, "media_type", "NA");
    }

    if (props->ip_types & SMB2_MC_IPV4) {
        CFArrayAppendValue(ip_types, CFSTR("ipv4"));
    }
    if (props->ip_types & SMB2_MC_IPV6) {
        CFArrayAppendValue(ip_types, CFSTR("ipv6"));
    }
    CFDictionaryAddValue(dict, CFSTR("ip_types"), ip_types);

    if (props->capabilities & SMB2_IF_CAP_RSS_CAPABLE) {
        CFArrayAppendValue(caps, CFSTR("RSS_CAPABLE"));
    }
    if (props->capabilities & SMB2_IF_CAP_RDMA_CAPABLE) {
        CFArrayAppendValue(caps, CFSTR("RDMA_CAPABLE"));
    }
    CFDictionaryAddValue(dict, CFSTR("capabilities"), caps);

    if (props->speed > 0 ) {
        sprintf(buf, "%llu", props->speed);
        json_add_str(dict, "link_speed_bps", buf);
    }

    const char *state_str = state_to_str(props->state);
    json_add_str(dict, "state", state_str);

    for (uint32_t i=0; i<props->num_of_addrs; i++) {
        address_to_str(props->addr_list[i], buf);
        CFStringRef cf_addr = CFStringCreateWithCString(kCFAllocatorDefault, buf, kCFStringEncodingUTF8);
        CFArrayAppendValue(ip_addrs, cf_addr);
    }
    CFDictionaryAddValue(dict, CFSTR("addr_list"), ip_addrs);

    return dict;
}

static CFMutableDictionaryRef
format_mc_status(struct smbioc_iod_prop * props)
{

    char buf[40];
    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (dict == NULL) {
        fprintf(stderr, "CFDictionaryCreateMutable failed\n");
        return NULL;
    }

    CFMutableArrayRef flags = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    if (flags == NULL) {
        fprintf(stderr, "CFArrayCreateMutable failed\n");
        return NULL;
    }

    sprintf(buf,"%u", props->iod_prop_id);
    json_add_str(dict, "iod_id", buf);

    if (!(props->iod_flags & SMBIOD_ALTERNATE_CHANNEL)) {
        CFArrayAppendValue(flags, CFSTR("main"));
    }
    if (props->iod_prop_s_if_caps & SMB2_IF_CAP_RSS_CAPABLE) {
        sprintf(buf,"rss_%llu", (props->iod_prop_s_if & (RSS_MASK)) >> RSS_SHIFT);
        CFStringRef cf_rss = CFStringCreateWithCString(kCFAllocatorDefault,
                                                       buf,
                                                       kCFStringEncodingUTF8);
        CFArrayAppendValue(flags, cf_rss);
    }

    CFDictionarySetValue(dict, CFSTR("flags"), flags);

    json_add_str(dict, "state", print_iod_state(props->iod_prop_state, props->iod_flags));

    if (props->iod_prop_c_if != (uint64_t)(-1)) {
        sprintf(buf, "%llu", props->iod_prop_c_if);
        json_add_str(dict, "client_ifIndex", buf);

        if_indextoname((uint32_t)props->iod_prop_c_if, buf);
        json_add_str(dict, "client_interface", buf);
    }
    
    if (props->iod_prop_con_speed > 0 ) {
        sprintf(buf, "%llu", props->iod_prop_con_speed);
        json_add_str(dict, "link_speed", buf);
    }

    address_to_str(props->iod_prop_s_addr, buf);
    json_add_str(dict, "server_inet", buf);

    if (props->iod_prop_s_if != (uint64_t)(-1)) {
        sprintf(buf, "%llu", props->iod_prop_s_if & (~RSS_MASK));
        json_add_str(dict, "server_ifIndex", buf);
    }

    if (props->iod_prop_con_port > 0) {
        sprintf(buf, "%u", props->iod_prop_con_port);
        json_add_str(dict, "port", buf);
    }

    if (props->iod_prop_rx > 0) {
        sprintf(buf, "%llu", props->iod_prop_rx);
        json_add_str(dict, "total_rx_bytes", buf);
    }

    if (props->iod_prop_tx > 0) {
        sprintf(buf, "%llu", props->iod_prop_tx);
        json_add_str(dict, "total_tx_bytes", buf);
    }

    if (props->iod_prop_setup_time.tv_sec > 0) {
        strftime(buf, sizeof buf, "%F %T", localtime(&props->iod_prop_setup_time.tv_sec));
        json_add_str(dict, "session_setup_time", buf);
    }

    return dict;
}

static void
print_header(FILE *fp, enum OutputFormat output_format)
{
    if (output_format == None) {
        fprintf(fp, "       id         client IF             server IF   state                     server ip                 port   speed");
        fprintf(fp, "\n========================================================================================================================\n");
    }
}

void
print_delimiter(FILE *fp, enum OutputFormat output_format)
{
    if (output_format == None) {
        fprintf(fp, "\n-----------------------------------------------------------------------------------------------------------------------\n\n");
    }
}

const char *state_to_str(uint32_t state) {
    switch (state) {
        case SMB2_MC_STATE_IDLE:
            return("idle");
        case SMB2_MC_STATE_ON_TRIAL:
            return("connecting");
        case SMB2_MC_STATE_USED:
            return("connected");
        case SMB2_MC_STATE_DISCONNECTED:
            return("disconnected");
        default:
            fprintf(stderr, "unknown nic state: %u", state);
            return("unknown");
    }
}

void print_nic_info(int server_nic, struct nic_properties *props) {
    char *ifname, *ifindex, *media_type, *state, *buf;
    CFDictionaryRef dict = format_nic_info(props, server_nic);
    CFArrayRef caps = CFDictionaryGetValue(dict, CFSTR("capabilities"));
    CFIndex caps_cnt = CFArrayGetCount(caps);
    char speed_buf[20] = "NA";
    /*
     * nic speed is not taken from dictionary "link_speed_bps" key value
     * as we want it more readable in this format and more QA suitable in JSON
     * format.
     */
    speed_to_str(props->speed, speed_buf);

    fprintf(stdout, "%s NIC:\n\tname: %s, idx: %s, type: %5s, speed %s, state %s\n",
            server_nic ? "Server" : "Client",
            ifname = CStringCreateWithCFString(CFDictionaryGetValue(dict, CFSTR("ifname"))),
            ifindex = CStringCreateWithCFString(CFDictionaryGetValue(dict, CFSTR("ifindex"))),
            media_type = CStringCreateWithCFString(CFDictionaryGetValue(dict, CFSTR("media_type"))),
            speed_buf,
            state = CStringCreateWithCFString(CFDictionaryGetValue(dict, CFSTR("state"))));

    free(ifname);
    free(ifindex);
    free(media_type);
    free(state);

    CFArrayRef ip_addrs = CFDictionaryGetValue(dict, CFSTR("addr_list"));

    for (uint32_t i = 0; i < CFArrayGetCount(ip_addrs); i++) {
        buf = CStringCreateWithCFString(CFArrayGetValueAtIndex(ip_addrs, i));
        fprintf(stdout, "\t\tip_addr: %s\n", buf);
        free(buf);
    }

    if (caps_cnt) {
        fprintf(stdout, "\t\tcapabilities:");
        for (uint32_t i = 0; i < (caps_cnt - 1); i++) {
            buf = CStringCreateWithCFString(CFArrayGetValueAtIndex(caps, i));
            fprintf(stdout, " %s,", buf);
            free(buf);
        }
        buf = CStringCreateWithCFString(CFArrayGetValueAtIndex(caps, (caps_cnt - 1)));
        fprintf(stdout, " %s\n", buf);
        free(buf);
    }

    if (dict) {
        CFRelease(dict);
    }
}



#define MC_STATUS    0x1
#define S_NIC_INFO   0x2
#define C_NIC_INFO   0x4
#define SESSION_INFO 0x8

static NTSTATUS
stat_multichannel(char *share_mp, enum OutputFormat output_format, uint8_t flags)
{
    SMBHANDLE inConnection = NULL;
    NTSTATUS status = STATUS_SUCCESS;
    struct statfs statbuf;
    char tmp_name[MNAMELEN];
    char *share_name = NULL, *end = NULL;
    char *unescaped_share_name = NULL;

    CFMutableDictionaryRef status_dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (output_format == Json && status_dict == NULL) {
        fprintf(stderr, "CFDictionaryCreateMutable failed\n");
        return EINVAL;
    }

    if ((statfs((const char*)share_mp, &statbuf) == -1) || (strncmp(statbuf.f_fstypename, "smbfs", 5) != 0)) {
        status = STATUS_INVALID_PARAMETER;
        errno = EINVAL;
        return  status;
    }

    /* If root user, change to the owner who mounted the share */
    if (getuid() == 0) {
        setuid(statbuf.f_owner);
    }

    /*
     * Need to specify a share name, else you get IPC$
     * Use the f_mntfromname so we dont have to worry about -1, -2 on the
     * mountpath and skip the initial "//"
     */
    strlcpy(tmp_name, &statbuf.f_mntfromname[2], sizeof(tmp_name));
    share_name = strchr(tmp_name, '/');
    if (share_name != NULL) {
        /* skip over the / to point at share name */
        share_name += 1;

        /* Check for submount and if found, strip it off */
        end = strchr(share_name, '/');
        if (end != NULL) {
            /* Found submount, just null it out as we only want sharepoint */
            *end = 0x00;
        }
    }
    else {
        fprintf(stderr, "%s : Failed to find share name in %s\n",
                __FUNCTION__, statbuf.f_mntfromname);
        status = STATUS_INVALID_PARAMETER;
        errno = EINVAL;
        return  status;
    }

    unescaped_share_name = get_share_name(share_name);

    if (unescaped_share_name == NULL) {
        fprintf(stderr, "%s : Failed to unescape share name <%s>\n",
                __FUNCTION__, share_name);
        status = STATUS_INVALID_PARAMETER;
        errno = EINVAL;
        return  status;
    }
    status = SMBOpenServerWithMountPoint(share_mp,
                                         unescaped_share_name,
                                         &inConnection,
                                         0);
    if (!NT_SUCCESS(status)) {
        fprintf(stderr, "%s : SMBOpenServerWithMountPoint() failed for %s <%s>\n",
                __FUNCTION__, share_mp, unescaped_share_name);
        return status;
    }

    if (output_format == None) {
      fprintf(stdout, "Session: %s\n", share_mp);
    }

    if (flags & SESSION_INFO) {

        struct smbioc_session_properties session_info;
        CFMutableDictionaryRef dict;
        char buf[40];

        dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                         &kCFTypeDictionaryKeyCallBacks,
                                         &kCFTypeDictionaryValueCallBacks);

        if (dict == NULL) {
            fprintf(stderr, "CFArrayCreateMutable failed\n");
            status = EINVAL;
            goto exit;
        }

        status = SMBGetMultichannelSessionInfoProperties(inConnection,
                                                         &session_info);

        if (!NT_SUCCESS(status)) {
            fprintf(stderr,
                    "%s : SMBGetMultichannelSessionInfoProperties() failed (client nics)\n",
                    __FUNCTION__);
            return status;
        }

        if (output_format == Json) {
            if (session_info.ioc_session_setup_time.tv_sec > 0) {
                strftime(buf, sizeof buf, "%F %T",
                         localtime(&session_info.ioc_session_setup_time.tv_sec));
                json_add_str(dict, "session_setup_time", buf);
            }

            sprintf(buf, "%llu", session_info.ioc_total_rx_bytes);
            json_add_str(dict, "total_rx_bytes", buf);

            sprintf(buf, "%llu", session_info.ioc_total_tx_bytes);
            json_add_str(dict, "total_tx_bytes", buf);

            sprintf(buf, "%u", session_info.ioc_session_reconnect_count);
            json_add_str(dict, "session_reconnect_count", buf);

            if (session_info.ioc_session_reconnect_count > 0 &&
                session_info.ioc_session_reconnect_time.tv_sec > 0) {
                strftime(buf, sizeof buf, "%F %T",
                         localtime(&session_info.ioc_session_reconnect_time.tv_sec));
                json_add_str(dict, "session_reconnect_time", buf);
            }
            
            sprintf(buf, "%s",
                 (session_info.flags & SMBV_MULTICHANNEL_ON) ? "yes" : "no");
            json_add_str(dict, "mc_on", buf);
            

            json_add_dict(status_dict, "session_info", dict);
            json_add_dict(smbMCShares, share_mp, status_dict);

        } else if (output_format == None) {

            fprintf(stdout, "Info: ");
            if (session_info.ioc_session_setup_time.tv_sec > 0) {
                strftime(buf, sizeof buf, "%F %T",
                         localtime(&session_info.ioc_session_setup_time.tv_sec));
                fprintf(stdout, "Setup Time: %s, ", buf);
            }

            fprintf(stdout, "Multichannel ON: %s, ", (session_info.flags & SMBV_MULTICHANNEL_ON) ? "yes" : "no");
            fprintf(stdout, "Reconnect Count: %u", session_info.ioc_session_reconnect_count);
            if (session_info.ioc_session_reconnect_count > 0 &&
                session_info.ioc_session_reconnect_time.tv_sec > 0) {
                strftime(buf, sizeof buf, "%F %T",
                         localtime(&session_info.ioc_session_reconnect_time.tv_sec));
                fprintf(stdout, "Reconnect Time %s", buf);
            }
            fprintf(stdout, "\n\tTotal RX Bytes: %llu, ", session_info.ioc_total_rx_bytes);
            fprintf(stdout, "Total TX Bytes: %llu", session_info.ioc_total_tx_bytes);
            fprintf(stdout, "\n");
        }
    }
    
    if (flags & MC_STATUS) {
        
        struct smbioc_multichannel_properties mc_props;
        CFMutableDictionaryRef channel_dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        if (output_format == Json && channel_dict == NULL) {
            fprintf(stderr, "CFDictionaryCreateMutable failed\n");
            status = EINVAL;
            goto exit;
        }

        status = SMBGetMultichannelProperties(inConnection, &mc_props);
        if (!NT_SUCCESS(status)) {
            fprintf(stderr, "%s : SMBGetMultichannelProperties() failed\n",
                    __FUNCTION__);
            return status;
        }

        if (mc_props.num_of_iod_properties > MAX_NUM_OF_IODS_IN_QUERY) {
            fprintf(stderr, "%s : incorrect num_of_iod_properties (%u))\n",
                    __FUNCTION__, mc_props.num_of_iod_properties);
            status = EINVAL;
            goto exit;
        }
        
        if (output_format == None) {
            print_header(stdout,output_format);
        }

        for(uint32_t u=0; u<mc_props.num_of_iod_properties; u++) {

            struct smbioc_iod_prop *p = &mc_props.iod_properties[u];

            // Find client interface name
            char c_if[IF_NAMESIZE] = "";
            if_indextoname((uint32_t)p->iod_prop_c_if, c_if);

            // print server ip address
            char s_ip[128];
            address_to_str(p->iod_prop_s_addr, s_ip);

            if (output_format == Json) {
                char buf[20];
                sprintf(buf,"%u", p->iod_prop_id);
                json_add_dict(channel_dict, buf, format_mc_status(p));
            } else {
                char speed_buf[20];
                 speed_to_str(p->iod_prop_con_speed, speed_buf);

                char rss_buf[20] = "     N/A";
                if (p->iod_prop_s_if != (uint64_t)(-1)) {
                    if (p->iod_prop_s_if_caps & SMB2_IF_CAP_RSS_CAPABLE) {
                        sprintf(rss_buf,"%4llu (RSS_%llu)",
                                p->iod_prop_s_if & (~RSS_MASK),
                                (p->iod_prop_s_if & (RSS_MASK)) >> RSS_SHIFT);
                    } else {
                        sprintf(rss_buf,"%4llu        ",
                                p->iod_prop_s_if & (~RSS_MASK));
                    }
                }
                
                char c_if_type[40]="    N/A";
                if (p->iod_prop_c_if != (uint64_t)(-1)) {
                    sprintf(c_if_type, " %-6s (%-8s) %3llu ",
                            c_if,
                            (p->iod_prop_c_if_type == IFM_ETHER) ? "Ethernet" : "wifi",
                            p->iod_prop_c_if);
                }

                // Print as follows:
                //    M       0      en11   (Ethernet)   18 (RSS)  [session active       ]   192.168.0.100             445    1.0 Gb
                fprintf(stdout, "%-3s%6u %-27s %-12s  [%-21s]   %-24s  %3u    %s\n",
                        (p->iod_flags & SMBIOD_ALTERNATE_CHANNEL)?"ALT":"M",
                        p->iod_prop_id,
                        c_if_type,
                        rss_buf,
                        print_iod_state(p->iod_prop_state, p->iod_flags),
                        s_ip, p->iod_prop_con_port, speed_buf);
                }
        }
        if (output_format == Json) {
            json_add_dict(status_dict, "multi_channel_status", channel_dict);
            json_add_dict(smbMCShares, share_mp, status_dict);
        }

    }

    if (flags & S_NIC_INFO) {

        struct smbioc_nic_info server_nics;
        CFMutableDictionaryRef s_nics = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        if (s_nics == NULL) {
            fprintf(stderr, "CFArrayCreateMutable failed\n");
            status = EINVAL;
            goto exit;
        }

        status = SMBGetNicInfoProperties(inConnection, &server_nics, SERVER_NICS);
        if (!NT_SUCCESS(status)) {
            fprintf(stderr, "%s : SMBGetNicInfoProperties() failed (server nics)\n",
                    __FUNCTION__);
            return status;
        }

        if (server_nics.num_of_nics > MAX_NUM_OF_NICS) {
            fprintf(stderr, "%s : incorrect number of server interfaces (%u))\n",
                    __FUNCTION__, server_nics.num_of_nics);
            status = EINVAL;
            goto exit;
        }

        for(uint32_t u=0; u<server_nics.num_of_nics; u++) {
            struct nic_properties *nic_info = &server_nics.nic_props[u];
            if (nic_info->if_index & RSS_MASK) {
                continue;
            }
            
            if (output_format == None) {
                print_nic_info(1, nic_info);

            } else if (output_format == Json) {

                char buf[20];
                sprintf(buf, "%llu", nic_info->if_index);
                json_add_dict(s_nics, buf, format_nic_info(nic_info, true));
            }

        }

        if ((server_nics.num_of_nics) && (output_format == Json)) {
            json_add_dict(status_dict, "server_interfaces", s_nics);
            json_add_dict(smbMCShares, share_mp, status_dict);
        }
    }

    if (flags & C_NIC_INFO) {

        struct smbioc_nic_info client_nics;
        CFMutableDictionaryRef c_nics = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        if (c_nics == NULL) {
            fprintf(stderr, "CFArrayCreateMutable failed\n");
            status = EINVAL;
            goto exit;
        }

        status = SMBGetNicInfoProperties(inConnection, &client_nics, CLIENT_NICS);
        if (!NT_SUCCESS(status)) {
            fprintf(stderr, "%s : SMBGetNicInfoProperties() failed (client nics)\n",
                    __FUNCTION__);
            return status;
        }

        if (client_nics.num_of_nics > MAX_NUM_OF_NICS) {
            fprintf(stderr, "%s : incorrect number of client interfaces (%u))\n",
                    __FUNCTION__, client_nics.num_of_nics);
            status = EINVAL;
            goto exit;
        }

        for(uint32_t u=0; u<client_nics.num_of_nics; u++) {

            struct nic_properties *nic_info = &client_nics.nic_props[u];

            if (output_format == None) {
                print_nic_info(0, nic_info);

            } else if (output_format == Json) {
                char buf[20];
                sprintf(buf,"%llu", nic_info->if_index);
                json_add_dict(c_nics, buf, format_nic_info(nic_info, false));
            }
        }

        if ((client_nics.num_of_nics) && (output_format == Json)) {
            json_add_dict(status_dict, "client_interfaces", c_nics);
            json_add_dict(smbMCShares, share_mp, status_dict);
        }
    }

    if (output_format == None) {
        printf("\n");
    }

exit:
    SMBReleaseServer(inConnection);

    return status;
}

static NTSTATUS
stat_all_multichannel(enum OutputFormat output_format, uint8_t flags)
{
    NTSTATUS error = STATUS_SUCCESS;
    struct statfs *fs = NULL;
    int fs_cnt = 0;
    int i = 0;

    fs = smb_getfsstat(&fs_cnt);
    if (!fs || fs_cnt <= 0)
        return ENOENT;

    for (i = 0; i < fs_cnt; i++, fs++) {
        NTSTATUS status;

        if (strncmp(fs->f_fstypename, "smbfs", 5) != 0)
            continue;
        if (fs->f_flags & MNT_AUTOMOUNTED)
            continue;

        status = stat_multichannel(fs->f_mntonname, output_format, flags) ;
        if (!NT_SUCCESS(status)) {
            fprintf(stderr, "%s : stat_multichannel() failed for %s\n",
                    __FUNCTION__, fs->f_mntonname);
            error = status;
        }
    }

    return error;
}

int
cmd_multichannel(int argc, char *argv[])
{
    NTSTATUS status = STATUS_SUCCESS;
    int opt;
    enum OutputFormat output_format = None;
    bool printShare = false;
    bool printAll = false;
    char *mountPath = NULL;
    uint8_t flags = 0;

    while ((opt = getopt(argc, argv, "aicsxm:f:")) != EOF) {
        switch(opt) {
            case 'f':
                if (strcasecmp(optarg, "json") == 0) {
                    output_format = Json;

                    /* Init smbMCShares dictionary */
                    smbMCShares = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                            &kCFTypeDictionaryKeyCallBacks,
                                                            &kCFTypeDictionaryValueCallBacks);
                    if (smbMCShares == NULL) {
                        fprintf(stderr, "CFDictionaryCreateMutable failed\n");
                        return EINVAL;
                    }
                }
                else {
                    multichannel_usage();
                }
                break;
            case 'a':
                printAll = true;
                if (printShare)
                    multichannel_usage();
                break;
            case 'm':
                printShare = true;
                if (printAll)
                    multichannel_usage();
                mountPath = optarg;
                break;
            case 'c':
                flags |= C_NIC_INFO;
                break;
            case 's':
                flags |= S_NIC_INFO;
                break;
            case 'x':
                flags |= MC_STATUS;
                break;
            case 'i':
                flags |= SESSION_INFO;
                break;
            default:
                break;
        }
    }

    if (!flags) {
        // default is to print all
        flags = C_NIC_INFO | S_NIC_INFO | MC_STATUS | SESSION_INFO;
    }

    if (!printShare && !printAll) {
        multichannel_usage();
    }

    if (printAll) {
        status = stat_all_multichannel(output_format, flags);
    }

    if (printShare) {
        status = stat_multichannel(mountPath, output_format, flags);
    }

    if (output_format == Json) {
        json_print_cf_object(smbMCShares, NULL);
        printf("\n");
    }

    if (!NT_SUCCESS(status))
        ntstatus_to_err(status);

    return 0;
}

void
multichannel_usage(void)
{
    fprintf(stderr, "usage : smbutil multichannel [-m <mount_path> | -a] [-f <format>]\n");
    fprintf(stderr, "\
          [\n \
             description :\n \
             -a : attributes of all mounted shares\n \
             -m <mount_path> : attributes of share mounted at mount_path\n \
             -i : display session information\n \
             -c : show information about client interfaces\n \
             -s : show information about server interfaces\n \
             -x : show information about the established connections\n \
             -f <format> : print info in the provided format. Supported formats: JSON\n \
          ]\n");
    exit(1);
}
