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

#ifndef smb2_mc_support_h
#define smb2_mc_support_h

#include <netsmb/smb_dev_2.h>
#include <netsmb/smb2_mc.h>
/*
 * Interface table API's
 */
void smb2_mc_destroy(
        struct session_network_interface_info* session_table);
void smb2_mc_init(
        struct session_network_interface_info* session_table,
        int32_t max_channels,
        int32_t max_rss_channels,
        uint32_t *ignored_client_nic,
        uint32_t ignored_client_nic_len,
        int32_t prefer_wired);
int smb2_mc_parse_client_interface_array(
        struct session_network_interface_info* session_table,
        struct smbioc_client_interface* client_info);
int smb2_mc_query_info_response_event(
        struct session_network_interface_info* session_table,
        uint8_t* server_info_buffer,
        uint32_t buf_len);
int smb2_mc_inform_connection_active_state_change(
     struct session_network_interface_info *session_table,
     struct session_con_entry *con_entry,
     bool active);
int smb2_mc_update_nic_list_from_notifier(
        struct session_network_interface_info* session_table,
        struct session_network_interface_info* updated_session_table,
        bool is_client);
int smb2_mc_notifier_event(
        struct session_network_interface_info *psSessionTable,
        struct smbioc_client_interface        *psNotifierClientInfo);
int smb2_mc_reset_nic_list(struct session_network_interface_info *session_table);

#define SMB2_IF_RSS_INDEX_MASK   (0xF00000000)
#define SMB2_IF_RSS_INDEX_SHIFT  (32)
#define SMB2_IF_INDEX_MASK       (0xFFFFFFFF)

/* Connection trial APIs */

typedef enum _SMB2_MC_CON_TRIAL_STATUS {
    SMB2_MC_TRIAL_UNKNOWN = 0x00,
    SMB2_MC_TRIAL_PASSED  = 0x01,
    SMB2_MC_TRIAL_FAILED  = 0x02,
    SMB2_MC_DISCONNECTED  = 0x04,
}_SMB2_MC_CON_TRIAL_STATUS;

/* smb2_mc_ask_for_new_connection_trial
 * According to the table data, need to find the available pairs of NICs,
 * that can be sent to a trial.
 * Output: trial_vector filled with possible connection trials to check, with it's length.
 */
int smb2_mc_ask_for_new_connection_trial(
        struct session_network_interface_info* session_table,
        struct session_con_entry** trial_vector,
        uint32_t max_out_len);

/*
 * smb2_mc_report_connection_trial_results
 * Should be used in order to report connection entry trial results.
 */
int smb2_mc_report_connection_trial_results(
        struct session_network_interface_info* session_table,
        _SMB2_MC_CON_TRIAL_STATUS status,
        struct session_con_entry *con_entry_p);

/*
 * smb2_mc_update_main_channel
 * Should be used in order to report on main connection success.
 * A new connection will be created.
 */
int smb2_mc_update_main_channel(
        struct session_network_interface_info* session_table,
        struct sockaddr* client_ip,
        struct sockaddr* server_ip,
        struct session_con_entry **con_entry_p,
        struct smbiod* iod);

/*
 * smb2_mc_does_ip_belong_to_interface
 * Find an ip address in a list of ip addresses associated to a nic.
 */
struct sock_addr_entry *smb2_mc_does_ip_belong_to_interface(
                                    struct complete_nic_info_entry* info,
                                    struct sockaddr* ip);

/*
 * smb2_mc_inform_connection_disconnected
 * iod reports a channel has been disconnected.
 */
int
smb2_mc_inform_connection_disconnected(
     struct session_network_interface_info *session_table,
     struct session_con_entry *con_entry);

/*
 * smb2_mc_return_excess_connections
 * According to the table data, need to find the extra connections
 * that can be removed.
 * Output: removal_vector filled with connection to remove, with it's length.
 */
int
smb2_mc_return_excess_connections(
    struct session_network_interface_info *session_table,
    struct session_con_entry** removal_vector,
    int32_t max_out_len);

/*
 * smb2_mc_check_for_active_trials
 * Return 0 if there is no ongoing connection trial
 */
int
smb2_mc_check_for_active_trials(
    struct session_network_interface_info* session_table);

/*
 * smb2_mc_pause_trials
 * Use this function to stop new connection trial from being returned by
 * smb2_mc_ask_for_new_connection_trial. smb2_mc_resume_trials should be used
 * to exit the pause mode.
 */
int
smb2_mc_pause_trials(
    struct session_network_interface_info* session_table);

/*
 * smb2_mc_abort_trials
 * set SMBIOD_ABORT_CONNECT flag to any iod currently trying to establish new
 * connection.
 */
int
smb2_mc_abort_trials(
    struct session_network_interface_info* session_table);

/*
 * smb2_mc_resume_trials
 * Use this function to resume new connection trials after paused by
 * smb2_mc_pause_trials
 */
int
smb2_mc_resume_trials(
    struct session_network_interface_info* session_table);

#ifdef SMB_DEBUG
void smb2_mc_print_all_connections(struct session_network_interface_info *session_table);
#endif

#endif /* smb2_mc_support_h */
