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
        struct session_network_interface_info* session_table);
int smb2_mc_parse_client_interface_array(
        struct session_network_interface_info* session_table,
        struct smbioc_client_interface* client_info);
int smb2_mc_parse_server_interface_array(
        struct session_network_interface_info* session_table,
        uint8_t* server_info_buffer,
        uint32_t buf_len);


/* Connection trial APIs */

typedef enum _SMB2_MC_CON_TRIAL_STATUS {
    SMB2_MC_TRIAL_UNKNOWN = 0x00,
    SMB2_MC_TRIAL_PASSED  = 0x01,
    SMB2_MC_TRIAL_FAILED  = 0x02,
    SMB2_MC_TRIAL_MAIN    = 0x04,
}_SMB2_MC_CON_TRIAL_STATUS;

/* smb2_mc_ask_for_new_connection_trial
 * According to the table data, need to find the available pairs of NICs,
 * that can be sent to a trial.
 * Output: trial_vector filled with possible connection trials to check, with it's length.
 */
int smb2_mc_ask_for_new_connection_trial(
        struct session_network_interface_info* session_table,
        struct session_con_entry** trial_vector,
        uint8_t max_out_len);

/*
 * smb2_mc_report_connection_trial_results
 * Should be used in order to report connection trail results.
 * In case of a successful trial, a new connection will be created.
 */
int smb2_mc_report_connection_trial_results(
        struct session_network_interface_info* session_table,
        struct sockaddr* client_ip,
        struct sockaddr* server_ip,
        _SMB2_MC_CON_TRIAL_STATUS status);

#endif /* smb2_mc_support_h */
