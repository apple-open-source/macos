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

#include <sys/smb_apple.h>
#include <netbios.h>
#include <netsmb/smb_subr.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb2_mc_support.h>

#define SMB2_QUERY_NETWORK_RESPONSE_IPV4_FAMILY     0x0002
#define SMB2_QUERY_NETWORK_RESPONSE_IPV6_FAMILY     0x0017

#define SMB2_MC_IPV4_LEN (0x10)
#define SMB2_MC_IPV6_LEN (0x1C)

#define SMB2_IF_CAP_RSS_CAPABLE  (0x01)
#define SMB2_IF_CAP_RDMA_CAPABLE (0x02)

static int	smb2_mc_add_new_interface_info_to_list(
            struct interface_info_list* list,
            uint32_t* list_counter,
            struct network_nic_info* new_info,
            uint32_t rss_val,
            bool in_black_list);
static void smb2_mc_reset_con_status(struct session_network_interface_info *session_table,
            struct complete_nic_info_entry *nic,
            bool is_client);
static int  smb2_mc_update_con_list(
            struct session_network_interface_info *session_table);
static struct complete_nic_info_entry* smb2_mc_get_nic_by_index(
		    struct interface_info_list* list,
		    uint64_t index);
static struct complete_nic_info_entry* smb2_mc_get_nic_by_ip(
            struct interface_info_list* list,
            struct sockaddr* ip);
static void smb2_mc_insert_new_nic_by_speed(
            struct interface_info_list* list,
            uint32_t* list_counter,
            struct complete_nic_info_entry* nic_info,
            struct complete_nic_info_entry** prev_nic_info);
static uint8_t  smb2_mc_protocol_family_to_inet(uint16_t family);
static void smb2_mc_release_connection_list(
            struct session_network_interface_info* session_table);
static void smb2_mc_release_interface(
            struct interface_info_list* list,
            struct complete_nic_info_entry* interface,
            uint32_t *p_nic_count);
static void smb2_mc_release_interface_list(
            struct interface_info_list* list);
static bool smb2_mc_sockaddr_cmp(struct sockaddr *x, struct sockaddr *y);
static bool smb2_mc_safe_buffer_get_and_advance_16(
            uint8_t** buff,
            uint8_t* buff_end,
            uint16_t* res);
static bool smb2_mc_safe_buffer_get_and_advance_32(
            uint8_t** buff,
            uint8_t* buff_end,
            uint32_t* res);
static bool smb2_mc_safe_buffer_get_and_advance_64(
            uint8_t** buff,
            uint8_t* buff_end,
            uint64_t* res);
static bool smb2_mc_safe_buffer_get_and_advance_in6_addr(
            uint8_t** buff,
            uint8_t* buff_end,
            in6_addr_t* addr);
static int  smb2_mc_update_info_with_ip(
            struct complete_nic_info_entry* nic_info,
            struct sockaddr* addr,
            bool *changed);
static int  smb2_mc_update_new_connection(
            struct session_network_interface_info* session_table,
            uint64_t client_idx,
            uint64_t server_idx,
			struct session_con_entry **con_entry_p,
            struct smbiod* iod);

struct conn_states_results {
    uint32_t active_cnt;          /* the number of active channels */
    uint32_t inactive_cnt;        /* the number of inactive channels */
    uint32_t wired_inactive_cnt;  /* number of wired inactive channels */
    uint64_t max_active_speed;    /* the max speed of active connections */
    uint64_t max_inactive_speed;  /* the max speed of inactive connection */
    uint64_t max_potential_speed; /* the max speed of a potential connection */
    uint64_t max_potential_wired_speed; /* the max speed of a potential wired connection */
    struct session_con_entry *potential_con_with_max_speed; /* pointer to the fastest potential connection */
    struct session_con_entry *potential_wired_con_with_max_speed; /* pointer to the fastest potential wired connection */
    struct session_con_entry *fastest_inactive; /* pointer to the fastest inactive connection */
};

/*
 * Count the number of active and inactive session connections.
 * The interface_table_lck is expected to be held before entering this function
 */
static int
smb2_mc_count_connection_stats(
    struct session_network_interface_info* session_table,
    struct conn_states_results *results  )
{
    int error = 0;
    
    uint32_t local_active_cnt = 0;
    uint32_t local_inactive_cnt = 0;
    uint64_t local_active_speed = 0;
    uint64_t local_inactive_speed = 0;
    uint64_t local_potential_speed = 0;
    uint64_t local_potential_wired_speed = 0;
    uint32_t local_wired_inactive_cnt = 0;
    struct session_con_entry *local_potential_con_with_max_speed = NULL;
    struct session_con_entry *local_potential_wired_con_with_max_speed = NULL;
    struct session_con_entry *local_fastest_inactive = NULL;
    
    if (session_table == NULL)
    {
        SMBERROR("NULL pointer");
        error = EINVAL;
        goto exit;
    }

    struct session_con_entry *con;
    TAILQ_FOREACH(con, &session_table->session_con_list, next) {

        if (con->state == SMB2_MC_STATE_CONNECTED) {
            
            if (con->active_state == SMB2_MC_FUNC_ACTIVE) {
                local_active_cnt++;
                if (con->con_speed > local_active_speed)
                    local_active_speed = con->con_speed;
            } else if (con->active_state == SMB2_MC_FUNC_INACTIVE) {
                    local_inactive_cnt++;
                if (con->con_speed > local_inactive_speed) {
                    local_inactive_speed   = con->con_speed;
                    local_fastest_inactive = con;
                }
                if (con->con_client_nic->nic_type == IFM_ETHER)
                {
                    local_wired_inactive_cnt++;
                }
            }
            
        } else if ((con->state == SMB2_MC_STATE_POTENTIAL) &&
                   (con->con_client_nic->nic_state == SMB2_MC_STATE_IDLE) &&
                   (con->con_server_nic->nic_state == SMB2_MC_STATE_IDLE)) {
            con->con_speed = MIN(con->con_client_nic->nic_link_speed, con->con_server_nic->nic_link_speed);
            if (con->con_speed > local_potential_speed) {
                local_potential_speed = con->con_speed;
                local_potential_con_with_max_speed = con;
            }
            /*
             * <72204412> if should prefer wired channels, find the best wired
             * potential in case it is not the global potential.
             */
            if (session_table->prefer_wired &&
                (con->con_client_nic->nic_type == IFM_ETHER) &&
                (con->con_speed > local_potential_wired_speed)) {
                local_potential_wired_speed = con->con_speed;
                local_potential_wired_con_with_max_speed = con;
            }
        }
    }

    results->active_cnt          = local_active_cnt;
    results->inactive_cnt        = local_inactive_cnt;
    results->max_active_speed    = local_active_speed;
    results->max_inactive_speed  = local_inactive_speed;
    results->fastest_inactive    = local_fastest_inactive;
    results->max_potential_speed = local_potential_speed;
    results->potential_con_with_max_speed = local_potential_con_with_max_speed;
    results->max_potential_wired_speed = local_potential_wired_speed;
    results->potential_wired_con_with_max_speed = local_potential_wired_con_with_max_speed;
    results->wired_inactive_cnt = local_wired_inactive_cnt;

exit:
    return error;
}

int
smb2_mc_return_excess_connections(
    struct session_network_interface_info *session_table,
    struct session_con_entry** removal_vector,
    int32_t max_out_len)
{
    struct session_con_entry* con_entry;
    struct conn_states_results con_stats;
    int ret_val = 0;
    lck_mtx_lock(&session_table->interface_table_lck);

    // Sort through connections and set active/inactive/redundant accordingly.
    uint64_t max_connected_speed = 0;
    uint64_t max_connected_wired_speed = 0;
    struct session_con_entry *con;
    TAILQ_FOREACH(con, &session_table->successful_con_list, success_next) {
        // <72204412> if set to prefer wired, find the max wired speed
        if (session_table->prefer_wired &&
            (con->con_client_nic->nic_type == IFM_ETHER) &&
            (con->con_speed > max_connected_wired_speed)) {
            max_connected_wired_speed = con->con_speed;
        }
        // Find max global speed
        if (con->con_speed > max_connected_speed) {
            max_connected_speed = con->con_speed;
        }
    }

    /*
     * <72204412> in case max_connected_wired_speed is not 0, we have a wired connection
     * and session was set to prefer wired so we should use the wired max speed
     * as the global max to make sure any wireless connection will be marked inactive.
     */
    if (max_connected_wired_speed)
    {
        max_connected_speed = max_connected_wired_speed;
    }

    uint64_t max_inactive_speed = 0;
    uint64_t max_inactive_wired_speed = 0;
    uint32_t num_of_inactives = 0;
    struct session_con_entry *fastest_inactive = NULL;
    struct session_con_entry *fastest_wired_inactive = NULL;
    TAILQ_FOREACH(con, &session_table->successful_con_list, success_next) {
        /*
         * 2) mark all connections at max_speed as active, and inactive otherwise.
         * notify iod if needed. <72204412> if max_connected_wired_speed, wireless
         * connection should be inactive even if its speed is equal to the max.
         */
        if ((con->con_speed == max_connected_speed) &&
            ((max_connected_wired_speed == 0) ||
             (con->con_client_nic->nic_type == IFM_ETHER))) {
            if (con->active_state != SMB2_MC_FUNC_ACTIVE) {
                con->active_state = SMB2_MC_FUNC_ACTIVE;
                smb_iod_active(con->iod);
            }
        } else {
            if (con->active_state != SMB2_MC_FUNC_INACTIVE) {
                con->active_state = SMB2_MC_FUNC_INACTIVE;
                smb_iod_inactive(con->iod);
            }
        }
        /*
         * 3) Find the highest inactive connection. <72204412> if set to prefer
         * wired, also find the highest inactive wired connection
         */
        if (con->active_state == SMB2_MC_FUNC_INACTIVE) {
            num_of_inactives++;
            if (con->con_speed > max_inactive_speed) {
                max_inactive_speed = con->con_speed;
                fastest_inactive   = con;
            }
            
            if (session_table->prefer_wired &&
                (con->con_client_nic->nic_type == IFM_ETHER) &&
                (con->con_speed > max_inactive_wired_speed)) {
                max_inactive_wired_speed = con->con_speed;
                fastest_wired_inactive = con;
            }
        }
    }

    // 4) Set all inactive other than the fastest for removal
    if (num_of_inactives > 1) {
        /*
         * <72204412> in case fastest_wired_inactive is not NULL, session was
         * set to preffer wired and have a wired inactive connection so use it.
         */
        if (fastest_wired_inactive)
        {
            fastest_inactive = fastest_wired_inactive;
        }

        TAILQ_FOREACH(con, &session_table->successful_con_list, success_next) {
            if ((con != fastest_inactive) &&
                (con->active_state == SMB2_MC_FUNC_INACTIVE)) {
                con->active_state = SMB2_MC_FUNC_INACTIVE_REDUNDANT;
                num_of_inactives--;
                if (num_of_inactives == 1)
                    break;
            }
        }
    }
    
    /*
     * Remove all redundant inactive connections and also remove active connections,
     * in case the total number of active and inactive is over the limit, while assuming
     * the number of inactive connections is the required minimum of such channels
     */
    int error = smb2_mc_count_connection_stats(session_table, &con_stats);
    /* In case of error, return the number of connections to be removed as 0 */
    if (error != 0) {
        SMBERROR("failed to count connection states");
        goto exit;
    }

    uint32_t   active_cnt = con_stats.active_cnt;
    uint32_t inactive_cnt = con_stats.inactive_cnt;
    bool remove_active_conn = ((inactive_cnt + active_cnt) > session_table->max_channels);

    TAILQ_FOREACH(con_entry,
                  &session_table->successful_con_list,
                  success_next) {
        /* Skip connection waiting to be removed */
        if (con_entry->state == SMB2_MC_STATE_IN_REMOVAL) continue;
        bool remove_conn = false;
        
        if (remove_active_conn &&
            (con_entry->active_state == SMB2_MC_FUNC_ACTIVE)) {
            remove_conn = true;
            active_cnt--;
            remove_active_conn = ((inactive_cnt + active_cnt) > session_table->max_channels);
        } else if (con_entry->active_state == SMB2_MC_FUNC_INACTIVE_REDUNDANT){
            remove_conn = true;
        }
        
        if (!remove_conn) continue;
        
        // Ask iod to terminate and remove con_entry from successful_con_list
        con_entry->state = SMB2_MC_STATE_IN_REMOVAL;
        *removal_vector = con_entry;
        removal_vector++;
        ret_val++;
        max_out_len--;

        /* Check we are not overflowing the array */
        if (!max_out_len) goto exit;
    }
    
    /* If we got here we must enforce the limit (can be removed in the future) */
    if ((inactive_cnt + active_cnt) > session_table->max_channels)
    {
        SMBERROR("failed to remove enough channels to enforce the limit");
    }
    
exit:
    lck_mtx_unlock(&session_table->interface_table_lck);
    return ret_val;
}

int
smb2_mc_ask_for_new_connection_trial(
    struct session_network_interface_info* session_table,
    struct session_con_entry** trial_vector,
    uint32_t max_out_len)
{
    int ret_val = 0;
    bool potential_inactive_added = false;

    lck_mtx_lock(&session_table->interface_table_lck);

    if (session_table->pause_trials)
        goto exit;

    /* Limit the number of on_trial to max_channels */
    if (session_table->active_on_trial_connections > session_table->max_channels)
        goto exit;

    while(max_out_len) {
        struct conn_states_results con_stats;
        struct session_con_entry *con_to_add = NULL;
        struct session_con_entry* potential_con = NULL;
        uint64_t potential_speed = 0;
        int dont_test_for_active = 0;

        int error = smb2_mc_count_connection_stats(session_table, &con_stats);

        /* In case of error, return the number of connection trials as 0 */
        if (error != 0) {
            SMBERROR("failed to count connection states");
            goto exit;
        }

        // if we lost all active channels, convert an inactive to active
        if ((con_stats.active_cnt == 0) && (con_stats.inactive_cnt)) {
            if (con_stats.fastest_inactive && con_stats.fastest_inactive->iod) {
                con_stats.fastest_inactive->active_state = SMB2_MC_FUNC_ACTIVE;
                smb_iod_active(con_stats.fastest_inactive->iod);
                continue;
            }
        }

        /* Return if reached the max number of connections */
        if ((con_stats.inactive_cnt + con_stats.active_cnt) >= session_table->max_channels) {
            if ((con_stats.max_active_speed >= con_stats.max_potential_speed) &&
                (con_stats.max_active_speed >  con_stats.max_inactive_speed)  ) {
                goto exit;
            }
        }
        
        /*
         * <72204412> in case session was set to prefer wired -- always prefer
         * wired potential connection. if there is no wired potential, meaning
         * there is only wireless potential, it should be used only if there
         * is no inactive channel but anyway it should not be a main candidate.
         * o.w just use the highest potential connection.
         */
        if (session_table->prefer_wired)
        {
            if (con_stats.potential_wired_con_with_max_speed) {
                potential_con = con_stats.potential_wired_con_with_max_speed;
                potential_speed = con_stats.max_potential_wired_speed;
            } else if (con_stats.inactive_cnt == 0) {
                potential_con = con_stats.potential_con_with_max_speed;
                potential_speed = con_stats.max_potential_speed;
                dont_test_for_active = 1;
            }
        } else {
            potential_con = con_stats.potential_con_with_max_speed;
            potential_speed = con_stats.max_potential_speed;
        }

        /* Have we found a connection worth adding? */
        if (potential_con) {
            // if max_potential_speed is faster than our active speed, try to connect to it
            if ((potential_speed >= con_stats.max_active_speed) &&
                (dont_test_for_active == 0)) {
                con_to_add = potential_con;
                con_to_add->active_state = SMB2_MC_FUNC_ACTIVE;
                
            /*
             * try to add an inactive connection if either:
             * 1) we have a potential connection that's faster than our current inactive, or,
             * 2) we have no inactive connection at all (and we haven't recently added one), or,
             * 3) <72204412> we have no wired inactive connection and we should prefer wired
             * (and we haven't recently added one).
             */
            } else if ((potential_speed > con_stats.max_inactive_speed) ||
                       ((potential_inactive_added == false) &&
                        ((con_stats.inactive_cnt == 0) ||
                        ((session_table->prefer_wired) && (con_stats.wired_inactive_cnt == 0)))
                        )) {
                con_to_add = potential_con;
                con_to_add->active_state = SMB2_MC_FUNC_INACTIVE;
                potential_inactive_added = true;
            }
        }
        
        if (con_to_add) {
    
            /* We found a potential connection */
            con_to_add->state = SMB2_MC_STATE_IN_TRIAL;
            con_to_add->con_client_nic->nic_state = SMB2_MC_STATE_ON_TRIAL;
            con_to_add->con_server_nic->nic_state = SMB2_MC_STATE_ON_TRIAL;
            con_to_add->con_speed = MIN(con_to_add->con_client_nic->nic_link_speed, con_to_add->con_server_nic->nic_link_speed);
            *trial_vector = con_to_add;
            trial_vector++;
            ret_val++;
            // active_on_trial_connections counts the number of interface-pairs we're
            // going to attempt to connect to, concurently.
            session_table->active_on_trial_connections++;
            max_out_len--;
            
        } else {
            break;
        }
    }

exit:
    lck_mtx_unlock(&session_table->interface_table_lck);
    return ret_val;
}

/*
 * Parse the client's network_interface_instance_info array and create a
 * complete_interface_info
 */
int
smb2_mc_parse_client_interface_array(
    struct session_network_interface_info* session_table,
    struct smbioc_client_interface* client_info)
{
	struct network_nic_info *client_info_array = NULL;
	int error = 0;
    bool in_blacklist = false;

	uint32_t array_size = client_info->total_buffer_size;
    
    lck_mtx_lock(&session_table->interface_table_lck);

    SMB_MALLOC(client_info_array,
		   struct network_nic_info *, array_size,
		   M_SMBTEMP, M_WAITOK | M_ZERO);
	if (!client_info_array) {
		SMBERROR("failed to allocate struct client_info_array!");
		error = ENOMEM;
		goto exit;
	}

	error = copyin((user_addr_t) (void*) client_info->ioc_info_array, (caddr_t) (void*) client_info_array, array_size);
	if (error) {
		SMBERROR("Couldn't copyin the client interface table arguments!");
		goto exit;
	}

	struct network_nic_info * client_info_entry = (struct network_nic_info *) client_info_array;
	for (uint32_t counter = 0; counter < client_info->interface_instance_count; counter++) {
        in_blacklist = false;

        for (uint32_t i = 0; i < session_table->client_if_blacklist_len; i++)
        {
            if (session_table->client_if_blacklist[i] == client_info_entry->nic_index)
            {
                in_blacklist = true;
                break;
            }
        }

        error = smb2_mc_add_new_interface_info_to_list(&session_table->client_nic_info_list,
                                                       &session_table->client_nic_count,
                                                       client_info_entry, 0, in_blacklist);
        if (error) {
            SMBERROR("Adding new interface info ended with error %d!", error);
            smb2_mc_release_interface_list(&session_table->client_nic_info_list);
            break;
        }

		client_info_entry = (struct network_nic_info *) ((uint8_t*) client_info_entry + client_info_entry->next_offset);
	}

exit:
	if (client_info_array != NULL) {
		SMB_FREE(client_info_array, M_SMBTEMP);
	}

    lck_mtx_unlock(&session_table->interface_table_lck);
    
    return error;
}

/*
 * smb2_mc_query_info_response_event
 * Parse the server's network_interface_instance_info array and create a
 * complete_interface_info
 */
int
smb2_mc_query_info_response_event(
    struct session_network_interface_info *session_table,
    uint8_t *server_info_buffer,
    uint32_t buf_len)
{
	int error = 0;
    uint32_t rss_nic_index;

	if (server_info_buffer == NULL)
		return EINVAL;

    struct session_network_interface_info updated_interface_table;
    smb2_mc_init(&updated_interface_table, 0, 0, NULL, 0, 0);

    uint32_t next_offset = 0;
	uint8_t* buf_end = server_info_buffer + buf_len;

	struct network_nic_info* new_info = NULL;
	SMB_MALLOC(new_info, struct network_nic_info *, sizeof(struct network_nic_info), M_NSMBDEV, M_WAITOK | M_ZERO);
	if (new_info == NULL) {
		error = ENOMEM;
		goto done;
	}

	do {
        // Parse the query_interface_respose message, extract the server NICs info
		server_info_buffer += next_offset;
		uint8_t* interface_data = server_info_buffer;

		/* Extract the data from the buffer */
		if (!smb2_mc_safe_buffer_get_and_advance_32(&interface_data, buf_end, &next_offset)) { goto bad_offset; }

		if (!smb2_mc_safe_buffer_get_and_advance_32(&interface_data, buf_end, &new_info->nic_index)) { goto bad_offset; }

		if (!smb2_mc_safe_buffer_get_and_advance_32(&interface_data, buf_end, &new_info->nic_caps)) { goto bad_offset; }

		uint32_t reserved;
		if (!smb2_mc_safe_buffer_get_and_advance_32(&interface_data, buf_end, &reserved)) { goto bad_offset; }

		if (!smb2_mc_safe_buffer_get_and_advance_64(&interface_data, buf_end, &new_info->nic_link_speed)) { goto bad_offset; }

		uint16_t family;
		if (!smb2_mc_safe_buffer_get_and_advance_16(&interface_data, buf_end, &family)) { goto bad_offset; }
		new_info->addr.sa_family = smb2_mc_protocol_family_to_inet(family);

		if (family == SMB2_QUERY_NETWORK_RESPONSE_IPV4_FAMILY) {
			new_info->addr.sa_len = SMB2_MC_IPV4_LEN;

			if (!smb2_mc_safe_buffer_get_and_advance_16(&interface_data, buf_end, &new_info->addr_4.sin_port)) { goto bad_offset; }
			if (!smb2_mc_safe_buffer_get_and_advance_32(&interface_data, buf_end, &new_info->addr_4.sin_addr.s_addr)) { goto bad_offset; }

		} else if (family == SMB2_QUERY_NETWORK_RESPONSE_IPV6_FAMILY) {
			new_info->addr.sa_len = SMB2_MC_IPV6_LEN;
			if (!smb2_mc_safe_buffer_get_and_advance_16(&interface_data, buf_end, &new_info->addr_16.sin6_port)) { goto bad_offset; }

			uint32_t flow_info;
			if (!smb2_mc_safe_buffer_get_and_advance_32(&interface_data, buf_end, &flow_info)) { goto bad_offset; }

			if (!smb2_mc_safe_buffer_get_and_advance_in6_addr(&interface_data, buf_end, &new_info->addr_16.sin6_addr)) { goto bad_offset; }

			uint32_t scope_id;
			if (!smb2_mc_safe_buffer_get_and_advance_32(&interface_data, buf_end, &scope_id)) { goto bad_offset; }
		} else {
			/* We don't support other kinds */
			continue;
		}

		/* Use the data to create new interface info */
		error = smb2_mc_add_new_interface_info_to_list(&updated_interface_table.server_nic_info_list,
                                                       &updated_interface_table.server_nic_count,
                                                       new_info, 0, false);
		if (error)
			break;

        if (new_info->nic_caps & SMB2_IF_CAP_RSS_CAPABLE) {
            for (rss_nic_index = 1; rss_nic_index < session_table->max_rss_channels; rss_nic_index++)
            {
                /* SNIA-Feb-2020:
                 * Samba & Azure use RSS NICs and can safely connect multiple
                 * connections to each RSS-NIC
                 */
                struct network_nic_info* rss_nic = NULL;
                SMB_MALLOC(rss_nic, struct network_nic_info *,
                           sizeof(struct network_nic_info),
                           M_NSMBDEV, M_WAITOK | M_ZERO);

                if (rss_nic == NULL) {
                    error = ENOMEM;
                    goto done;
                }

                memcpy(rss_nic, new_info, sizeof(struct network_nic_info));
                error = smb2_mc_add_new_interface_info_to_list(&updated_interface_table.server_nic_info_list,
                                                               &updated_interface_table.server_nic_count,
                                                               rss_nic, rss_nic_index, false);
                if (error)
                    break;
            }
        }
    } while (next_offset);


    error = smb2_mc_update_nic_list_from_notifier(session_table, &updated_interface_table, false);
    if (error) {
        printf("smb2_mc_update_client_interface_array returned %d.\n", error);
        goto done;
    }

	goto done;

bad_offset:
	error = EINVAL;
done:

    smb2_mc_destroy(&updated_interface_table);

    return error;
}

/*
* smb2_mc_remove_nic_if_unused
*  Check if the provided NIC is unsued (ie in IDLE state and all of
*  the connections it points to are in FAILED_TO_CONNECT), and if so,
* remove it.
*/
static int
smb2_mc_remove_nic_if_unused(
        struct session_network_interface_info *session_table,
        struct complete_nic_info_entry        *nic,
        bool is_client)
{
    
    struct session_con_entry *con;
    uint32_t *nic_cnt_p;
    struct interface_info_list *nic_list_p;

    if (is_client) {
        nic_cnt_p          = (&session_table->client_nic_count);
        nic_list_p         = (&session_table->client_nic_info_list);

    } else {
        nic_cnt_p          = (&session_table->server_nic_count);
        nic_list_p         = (&session_table->server_nic_info_list);

    }
    
    bool can_remove = true;
    uint32_t num_of_cons = 0;
    TAILQ_FOREACH(con, &session_table->session_con_list, next) {
        
        if ((( is_client) && (con->con_client_nic == nic)) ||
            ((!is_client) && (con->con_server_nic == nic))) {
            
            num_of_cons++;

            // Keep NICs that are black-listed (so we'll remember not to use them),
            //   or NICs that are in use (!=FAILED and != NO_POT)
            if ( (nic->nic_flags & SMB2_MC_NIC_IN_BLACKLIST) ||
                ((con->state != SMB2_MC_STATE_FAILED_TO_CONNECT) && (con->state != SMB2_MC_STATE_NO_POTENTIAL)) ) {
                can_remove = false;
                break;
            }
        }
    }
    // If there are connections, and all of them are invalid (failed or no-pot)
    // then the nic is useless. remove it to free some space. 
    if (num_of_cons && can_remove) {
        #ifdef SMB_DEBUG
            char str[128];
            smb2_sockaddr_to_str(nic->addr_list.tqh_first->addr, str, sizeof(str));
            SMBDEBUG("Removing if %llu (RSS_%llu) addr %s.\n",
                     nic->nic_index  & SMB2_IF_INDEX_MASK,
                     nic->nic_index >> SMB2_IF_RSS_INDEX_SHIFT,
                     str);
        #endif
        // Remove all associated connections
        struct session_con_entry *con_t;
        TAILQ_FOREACH_SAFE(con, &session_table->session_con_list, next, con_t) {
            
            if ((( is_client) && (con->con_client_nic == nic)) ||
                ((!is_client) && (con->con_server_nic == nic))) {

                /* Remove from client's possible_connections list */
                struct complete_nic_info_entry *client_nic = con->con_client_nic;
                if (client_nic) {
                    TAILQ_REMOVE(&client_nic->possible_connections, con, client_next);
                }
                
                /* remove it from the main list */
                TAILQ_REMOVE(&session_table->session_con_list, con, next);
            
                SMB_FREE(con, M_NSMBDEV);
            }
        }
        // Release the nic
        smb2_mc_release_interface(nic_list_p, nic, nic_cnt_p);
    }
    return 0;
}

static bool
smb2_mc_update_ip_list(
        struct complete_nic_info_entry *existing_nic,
        struct complete_nic_info_entry *new_nic)
{
    
    struct sock_addr_entry *existing_addr, *new_addr, *existing_addr_t;
    bool ret_val = false;

    // 1) Remove IP addresses that are not in new
    TAILQ_FOREACH_SAFE(existing_addr, &existing_nic->addr_list, next, existing_addr_t) {

        bool found = false;
        TAILQ_FOREACH(new_addr, &new_nic->addr_list, next) {

            if (smb2_mc_sockaddr_cmp(new_addr->addr, existing_addr->addr)) {
                found = true;
            }
        }
        if (!found) {
            TAILQ_REMOVE(&existing_nic->addr_list, existing_addr, next);
            ret_val = true;
        }
    }
    
    // Add new addresses
    TAILQ_FOREACH(new_addr, &new_nic->addr_list, next) {
        smb2_mc_update_info_with_ip(existing_nic, new_addr->addr, &ret_val);
    }
    
    return ret_val;
}


/*
 * smb2_mc_update_nic_list_from_notifier
 * Handle notifier updates on the client interfaces:
 * 1) If a NIC does not already exist, add it.
 * 2) If a NIC exists: update its parameters (speed, capablibities).
 * 3) If we have a NIC in the session table but it isn't on the notifier list,
 *    (and it is in IDLE state and all of its connections are FAILED_TO_CONNECT),
 *    then we remove it from the session_table.
 */
int
smb2_mc_update_nic_list_from_notifier(
    struct session_network_interface_info *session_table,
    struct session_network_interface_info *updated_session_table,
    bool is_client)
{
    int error = 0;
    struct complete_nic_info_entry *updated_nic;
    struct complete_nic_info_entry *nic, *nic_t, *updated_nic_t;
    uint32_t *nic_cnt_p, *updated_nic_cnt_p;
    struct interface_info_list *nic_list_p;
    struct interface_info_list *updated_nic_list_p;
    bool exists = false;

    lck_mtx_lock(&session_table->interface_table_lck);
    
    if (is_client) {
        nic_cnt_p          = (&session_table->client_nic_count);
        nic_list_p         = (&session_table->client_nic_info_list);
        updated_nic_list_p = (&updated_session_table->client_nic_info_list);
        updated_nic_cnt_p  = (&updated_session_table->client_nic_count);

    } else {
        nic_cnt_p          = (&session_table->server_nic_count);
        nic_list_p         = (&session_table->server_nic_info_list);
        updated_nic_list_p = (&updated_session_table->server_nic_info_list);
        updated_nic_cnt_p  = (&updated_session_table->server_nic_count);

    }
        
    // Add or update existing nics
    TAILQ_FOREACH_SAFE(updated_nic, updated_nic_list_p, next, updated_nic_t) {

        exists = false;
        TAILQ_FOREACH_SAFE(nic, nic_list_p, next, nic_t) {
            
            if (nic->nic_index == updated_nic->nic_index) {
                
                bool ip_addr_list_change = smb2_mc_update_ip_list(nic, updated_nic);
                bool nic_params_change   = (nic->nic_link_speed != updated_nic->nic_link_speed) ||
                                           (nic->nic_type       != updated_nic->nic_type);
                
                if (ip_addr_list_change || nic_params_change) {

                    char spd[20], upd_spd[20];
                    smb2_spd_to_txt(nic->nic_link_speed, spd, sizeof(spd));
                    smb2_spd_to_txt(updated_nic->nic_link_speed, upd_spd, sizeof(upd_spd));
                    SMBDEBUG("%s nic idx %llu (RSS_%llu) changed: speed %s %s type 0x%x 0x%x.\n",
                             is_client?"client":"server",
                             nic->nic_index  & SMB2_IF_INDEX_MASK,
                             nic->nic_index >> SMB2_IF_RSS_INDEX_SHIFT,
                             spd,           upd_spd,
                             nic->nic_type, updated_nic->nic_type);
                    
                    nic->nic_link_speed = updated_nic->nic_link_speed;
                    nic->nic_type       = updated_nic->nic_type;
                    
                    smb2_mc_reset_con_status(session_table, nic, is_client);
                    
                } else if (is_client) {
                    // there was no change in parameters, but we received a notification
                    // from the notifier so allow searching of failed connections again
                    smb2_mc_reset_con_status(session_table, nic, is_client);
                }
                exists = true;
                break;
            }
        }
        
        // If a NIC does not already exist, add it.
        if (!exists) {
            TAILQ_REMOVE(updated_nic_list_p, updated_nic, next);
            (*updated_nic_cnt_p)--;
            
            char spd[20];
            smb2_spd_to_txt(updated_nic->nic_link_speed, spd, sizeof(spd));
            SMBDEBUG("Adding %s nic idx %llu (RSS_%llu) spd %s.\n",
                     is_client?"client":"server",
                     updated_nic->nic_index  & SMB2_IF_INDEX_MASK,
                     updated_nic->nic_index >> SMB2_IF_RSS_INDEX_SHIFT,
                     spd);
            
            smb2_mc_insert_new_nic_by_speed(nic_list_p,
                                            nic_cnt_p,
                                            updated_nic, NULL);

            error = smb2_mc_update_con_list(session_table);
            
        } else {
            /* Remove updated_nic from the update table and free */
            smb2_mc_release_interface(updated_nic_list_p,
                                      updated_nic,
                                      updated_nic_cnt_p);
        }

    }

    // Remove excess NICs
    // (ie NICs that are in session_table but no in the update_table)
    TAILQ_FOREACH_SAFE(nic, nic_list_p, next, nic_t) {

        exists = false;
        TAILQ_FOREACH_SAFE(updated_nic,
                           updated_nic_list_p, next, updated_nic_t) {

            if (nic->nic_index == updated_nic->nic_index) {
                exists = true;
                break;
            }
        }
        
        if (!exists) {
            // If we have a NIC in the session table but it isn't on the notifier list,
            //   (and it is in IDLE state and all of its connections are FAILED_TO_CONNECT),
            //   then we remove it from the session_table.
            smb2_mc_remove_nic_if_unused(session_table, nic, is_client);
        }

    }
    
    lck_mtx_unlock(&session_table->interface_table_lck);
    return error;
}

void
smb2_mc_destroy(
    struct session_network_interface_info* session_table)
{
    lck_mtx_lock(&session_table->interface_table_lck);

    smb2_mc_release_connection_list(session_table);
    smb2_mc_release_interface_list(&session_table->server_nic_info_list);
    smb2_mc_release_interface_list(&session_table->client_nic_info_list);

    if (session_table->client_if_blacklist != NULL)
    {
        SMB_FREE(session_table->client_if_blacklist, M_NSMBDEV);
    }

    lck_mtx_unlock(&session_table->interface_table_lck);

    lck_mtx_destroy(&session_table->interface_table_lck, session_st_lck_group);
}

void
smb2_mc_init(
    struct session_network_interface_info* session_table,
    int32_t max_channels,
    int32_t max_rss_channels,
    uint32_t* client_if_blacklist,
    uint32_t client_if_blacklist_len,
    int32_t prefer_wired)
{
    size_t array_size;

    SMBDEBUG("init mc\n");
    session_table->client_nic_count = 0;
    session_table->server_nic_count = 0;
    session_table->max_channels = max_channels ? max_channels : 9;
    session_table->max_rss_channels = max_rss_channels ? max_rss_channels : 4;
    session_table->prefer_wired = prefer_wired;
    session_table->pause_trials = 0;

    if (client_if_blacklist_len && client_if_blacklist) {
        array_size = sizeof(uint32_t) * client_if_blacklist_len;
        SMB_MALLOC(session_table->client_if_blacklist, uint32_t*,
                   array_size, M_NSMBDEV, M_WAITOK | M_ZERO);

        if (session_table->client_if_blacklist == NULL) {
            SMBERROR("failed to allocate session_table->client_if_blacklist (size %u)!", client_if_blacklist_len);
            session_table->client_if_blacklist_len = 0;
        } else {
            memcpy(session_table->client_if_blacklist,
                   client_if_blacklist, array_size);
            session_table->client_if_blacklist_len = client_if_blacklist_len;
        }
    } else {
        session_table->client_if_blacklist = NULL;
        session_table->client_if_blacklist_len = 0;
    }

    TAILQ_INIT(&session_table->server_nic_info_list);
    TAILQ_INIT(&session_table->client_nic_info_list);
    TAILQ_INIT(&session_table->session_con_list);
    TAILQ_INIT(&session_table->successful_con_list);

    lck_mtx_init(&session_table->interface_table_lck, session_st_lck_group, session_st_lck_attr);
}

/*
 * smb2_mc_inform_connection_disconnected
 * Inform mc_support that the channel has been disconnected.
 * Set the connection back to potential - and allow the mc_support
 * to try this connection again.
 */
int
smb2_mc_inform_connection_disconnected(
     struct session_network_interface_info *session_table,
     struct session_con_entry *con_entry)
{

    int error = 0;

    if ((!session_table) || (!con_entry)) {
        return EINVAL;
    }

    lck_mtx_lock(&session_table->interface_table_lck);

    if ((con_entry->state == SMB2_MC_STATE_CONNECTED) || (con_entry->state == SMB2_MC_STATE_IN_REMOVAL)) {
        TAILQ_REMOVE(&session_table->successful_con_list, con_entry, success_next);
        con_entry->state = SMB2_MC_STATE_POTENTIAL;
        
    } else if ((con_entry->state != SMB2_MC_STATE_FAILED_TO_CONNECT) &&
               (con_entry->state != SMB2_MC_STATE_NO_POTENTIAL)) {
        // Do not completely give up on this connection (ie do not mark as failed to connect).
        // This gives the system another go on that connection
        con_entry->state = SMB2_MC_STATE_POTENTIAL;
    }
    con_entry->con_client_nic->nic_state = SMB2_MC_STATE_IDLE;
    con_entry->con_server_nic->nic_state = SMB2_MC_STATE_IDLE;
    con_entry->iod = NULL;

    error = smb2_mc_update_con_list(session_table);

    lck_mtx_unlock(&session_table->interface_table_lck);
    return error;
}

int
smb2_mc_inform_connection_active_state_change(
     struct session_network_interface_info *session_table,
     struct session_con_entry *con_entry,
     bool active)
{
    int error = 0;
    
    SMB_LOG_MC("inform active state %u.\n", (unsigned)active);

    if ((!session_table) || (!con_entry)) {
        return EINVAL;
    }

    lck_mtx_lock(&session_table->interface_table_lck);
    if (active) {
        con_entry->active_state = SMB2_MC_FUNC_ACTIVE;
    } else {
        con_entry->active_state = SMB2_MC_FUNC_INACTIVE;
    }

    lck_mtx_unlock(&session_table->interface_table_lck);
    return error;
}


int
smb2_mc_update_main_channel(
    struct session_network_interface_info* session_table,
    struct sockaddr* client_ip,
    struct sockaddr* server_ip,
    struct session_con_entry **con_entry_p,
    struct smbiod* iod)
{
    int error = 0;

    if (client_ip == NULL || server_ip == NULL) {
        SMBERROR("got bad ip");
        return EINVAL;
    }

    lck_mtx_lock(&session_table->interface_table_lck);

	struct complete_nic_info_entry* client_nic = smb2_mc_get_nic_by_ip(&session_table->client_nic_info_list, client_ip);
	struct complete_nic_info_entry* server_nic = smb2_mc_get_nic_by_ip(&session_table->server_nic_info_list, server_ip);

    if (client_nic == NULL || server_nic == NULL) {

        char c_str[128];
        smb2_sockaddr_to_str(client_ip, c_str, sizeof(c_str));
        char s_str[128];
        smb2_sockaddr_to_str(server_ip, s_str, sizeof(s_str));
        SMBERROR("could not find one of the nics! c=%s, s=%s.\n", c_str, s_str);
        error = EINVAL;
        goto exit;
    }

    SMBDEBUG("Update main connection trial id %d [c- %llu, s- %llu (RSS %llu)] passed, trials %u.\n",
             iod->iod_id,
             client_nic->nic_index,
             server_nic->nic_index  & SMB2_IF_INDEX_MASK,
             server_nic->nic_index >> SMB2_IF_RSS_INDEX_SHIFT,
             session_table->active_on_trial_connections);


    error = smb2_mc_update_new_connection(session_table, client_nic->nic_index, server_nic->nic_index, con_entry_p, iod);

exit:
    lck_mtx_unlock(&session_table->interface_table_lck);
	return error;
}

int
smb2_mc_report_connection_trial_results(
    struct session_network_interface_info* session_table,
    _SMB2_MC_CON_TRIAL_STATUS status,
    struct session_con_entry *con_entry_p)
{
    struct complete_nic_info_entry* client_nic;
    struct complete_nic_info_entry* server_nic;

    if (con_entry_p == NULL) {
        SMBERROR("got bad con_entry_p");
        return EINVAL;
    }

    lck_mtx_lock(&session_table->interface_table_lck);

    int error = 0;

    session_table->active_on_trial_connections--;

    client_nic = con_entry_p->con_client_nic;
    server_nic = con_entry_p->con_server_nic;

    SMB_LOG_MC("trial id %d [c- %llu, s- %llu (RSS %llu)], status %d (%s), trials %u.\n",
             (con_entry_p->iod) ? (con_entry_p->iod->iod_id) : -1,
             client_nic->nic_index,
             con_entry_p->con_server_nic->nic_index &  SMB2_IF_INDEX_MASK,
             con_entry_p->con_server_nic->nic_index >> SMB2_IF_RSS_INDEX_SHIFT,
             status,(status&SMB2_MC_TRIAL_PASSED)?"passed":(status&SMB2_MC_TRIAL_FAILED)?"failed":"??",
             session_table->active_on_trial_connections);

    if (status & SMB2_MC_TRIAL_PASSED) {
        error = smb2_mc_update_new_connection(session_table,
                                              client_nic->nic_index,
                                              server_nic->nic_index,
                                              NULL, NULL);
    } else {
        // Trial failed - ie we were not able to connect those two NICs
        client_nic->nic_state = SMB2_MC_STATE_IDLE;
        server_nic->nic_state = SMB2_MC_STATE_IDLE;
        con_entry_p->iod = NULL;

        // Clean up session table
        struct session_con_entry *con = NULL;
        uint64_t server_nic_idx = server_nic->nic_index;
        uint64_t client_nic_idx = client_nic->nic_index;
        
        TAILQ_FOREACH(con, &session_table->session_con_list, next) {
            if (con->con_client_nic->nic_index == client_nic_idx) {
                // there is no need trying to connect the client NIC to all of the RSS permutations.
                if ((con->con_server_nic->nic_index & SMB2_IF_INDEX_MASK) ==
                    (server_nic_idx & SMB2_IF_INDEX_MASK)) {
                        // If one RSS failed, we can fail all the rest.
                        con->state = SMB2_MC_STATE_FAILED_TO_CONNECT;

                // Enable other connections for these nics
                } else if (con->state == SMB2_MC_STATE_SURPLUS) {
                    con->state = SMB2_MC_STATE_POTENTIAL;
                }

            } else if (con->con_server_nic->nic_index == server_nic_idx) {
                // Enable other connections for these nics
                if (con->state == SMB2_MC_STATE_SURPLUS) {
                    con->state = SMB2_MC_STATE_POTENTIAL;
                }
            }
        }
    }
    if (!error) {
        error = smb2_mc_update_con_list(session_table);
    }
    lck_mtx_unlock(&session_table->interface_table_lck);
    return error;
}

static int
smb2_mc_add_new_interface_info_to_list(
    struct interface_info_list* list,
    uint32_t* list_counter,
    struct network_nic_info* new_info,
    uint32_t rss_val,
    bool in_black_list)
{
    int error = 0;
    uint64_t nic_index = ((uint64_t)rss_val << SMB2_IF_RSS_INDEX_SHIFT) | new_info->nic_index;
    struct complete_nic_info_entry* nic_info = smb2_mc_get_nic_by_index(list, nic_index);
    bool new_nic = false;
    if (nic_info == NULL) { // Need to create a new nic element in the list
        new_nic = true;
        SMB_MALLOC(nic_info, struct complete_nic_info_entry*, sizeof(struct complete_nic_info_entry), M_NSMBDEV, M_WAITOK | M_ZERO);
        if (nic_info == NULL) {
            SMBERROR("failed to allocate struct complete_interface_entry!");
            return ENOMEM;
        }

        memset(nic_info, 0, sizeof(struct complete_nic_info_entry));
        TAILQ_INIT(&nic_info->addr_list);
        TAILQ_INIT(&nic_info->possible_connections);
    }

    /* Update with the most recent data */
    nic_info->nic_caps = new_info->nic_caps;
    nic_info->nic_index = nic_index;
    nic_info->nic_link_speed = new_info->nic_link_speed;
    nic_info->nic_type = new_info->nic_type;

    if (in_black_list) {
        nic_info->nic_flags |= SMB2_MC_NIC_IN_BLACKLIST;
    }

    error = smb2_mc_update_info_with_ip(nic_info, &new_info->addr, NULL);
    if (error) {
        SMBERROR("failed to smb2_mc_update_info_with_ip!");
        smb2_mc_release_interface(NULL, nic_info, NULL);
        nic_info = NULL;
    }
    
    if (!error && new_nic) {
        smb2_mc_insert_new_nic_by_speed(list, list_counter, nic_info, NULL);
    }

    return error;
}

static struct session_con_entry *find_con_by_nics(
                            struct session_network_interface_info *session_table,
                            struct complete_nic_info_entry *client_nic,
                            struct complete_nic_info_entry *server_nic)
{
    
    struct session_con_entry *con;
    TAILQ_FOREACH(con, &session_table->session_con_list, next) {
        if ((con->con_client_nic == client_nic) &&
            (con->con_server_nic == server_nic)) {
            return con;
        }
    }
    return NULL;
}

/*
 * smb2_mc_reset_con_status
 * Touchup Connections of NICs that were updated by notifier.
 */
static void
smb2_mc_reset_con_status(struct session_network_interface_info *session_table,
                         struct complete_nic_info_entry *nic,
                         bool is_client)
{

    struct session_con_entry *con;
    TAILQ_FOREACH(con, &session_table->session_con_list, next) {
        if ((( is_client) && (con->con_client_nic == nic)) ||
            ((!is_client) && (con->con_server_nic == nic))) {
            
            if ( (nic->nic_flags & SMB2_MC_NIC_IN_BLACKLIST) == 0) {
                if ((con->state == SMB2_MC_STATE_FAILED_TO_CONNECT) ||
                    (con->state == SMB2_MC_STATE_NO_POTENTIAL) ) {
                    con->state = SMB2_MC_STATE_POTENTIAL;
                } else {
                    con->con_speed = MIN(con->con_client_nic->nic_link_speed, con->con_server_nic->nic_link_speed);
                }
            }
        }
    }
}

int smb2_mc_pause_trials(struct session_network_interface_info* session_table)
{
    lck_mtx_lock(&session_table->interface_table_lck);
    session_table->pause_trials += 1;
    lck_mtx_unlock(&session_table->interface_table_lck);
    return 0;
}

int smb2_mc_abort_trials(struct session_network_interface_info* session_table)
{
    struct session_con_entry *con;

    lck_mtx_lock(&session_table->interface_table_lck);

    if (session_table->pause_trials) {
        TAILQ_FOREACH(con, &session_table->session_con_list, next) {
            if ((con->state == SMB2_MC_STATE_IN_TRIAL) && (con->iod)) {
                /*
                 * <71930272> set SMBIOD_ABORT_CONNECT to reduce the time
                 * it will take for the trial to end.
                 */
                lck_mtx_lock(&con->iod->iod_flagslock);
                con->iod->iod_flags |= SMBIOD_ABORT_CONNECT;
                lck_mtx_unlock(&con->iod->iod_flagslock);
            }
        }
    }

    lck_mtx_unlock(&session_table->interface_table_lck);
    return 0;
}

int smb2_mc_resume_trials(struct session_network_interface_info* session_table)
{
    lck_mtx_lock(&session_table->interface_table_lck);
    if (session_table->pause_trials) {
        session_table->pause_trials -= 1;
    }
    lck_mtx_unlock(&session_table->interface_table_lck);
    
    return 0;
}

int smb2_mc_check_for_active_trials(struct session_network_interface_info* session_table)
{
    struct session_con_entry *con;
    int trials_cnt = 0;
    lck_mtx_lock(&session_table->interface_table_lck);
    TAILQ_FOREACH(con, &session_table->session_con_list, next) {
        if (con->state == SMB2_MC_STATE_IN_TRIAL) {
            SMB_LOG_MC("id %d in trial c- %llu, s-%llu\n",
                       (con->iod) ? con->iod->iod_id : -1,
                       con->con_client_nic->nic_index,
                       con->con_server_nic->nic_index);
            trials_cnt++;
            /*
             * single ongoing trial is enough but for debug we might want
             * to log all onging trials.
             */
#ifndef SMB_DEBUG
            break;
#endif
        }
    }
    lck_mtx_unlock(&session_table->interface_table_lck);
    return trials_cnt;
}

static int
smb2_mc_update_con_list(struct session_network_interface_info *session_table)
{
    struct complete_nic_info_entry *server_nic, *client_nic;
    struct session_con_entry *con;
    
    TAILQ_FOREACH(client_nic,
                 &session_table->client_nic_info_list, next) {

        TAILQ_FOREACH(server_nic,
                     &session_table->server_nic_info_list, next) {

            
            con = find_con_by_nics(session_table, client_nic, server_nic);
            if (con == NULL) {
                /* in con does not exist, create one */
                SMB_MALLOC(con, struct session_con_entry*, sizeof(struct session_con_entry), M_NSMBDEV, M_WAITOK | M_ZERO);
                if (con == NULL) {
                    SMBERROR("failed to allocate struct session_con_entry!");
                    return ENOMEM;
                }

                con->con_client_nic = client_nic;
                con->con_server_nic = server_nic;

                // If there is no common ip-version or if black-listed,
                // mark as no_potential
                if (((client_nic->nic_ip_types & server_nic->nic_ip_types) == 0) ||
                    (client_nic->nic_flags & SMB2_MC_NIC_IN_BLACKLIST))
                    con->state = SMB2_MC_STATE_NO_POTENTIAL;
                else if (server_nic->nic_state == SMB2_MC_STATE_USED)
                    con->state = SMB2_MC_STATE_SURPLUS;
                else
                    con->state = SMB2_MC_STATE_POTENTIAL;

                TAILQ_INSERT_TAIL(&session_table->session_con_list, con, next);
                TAILQ_INSERT_TAIL(&client_nic->possible_connections, con, client_next);
            
            } else { // connection exists
                if ((client_nic->nic_state == SMB2_MC_STATE_IDLE) &&
                    (server_nic->nic_state == SMB2_MC_STATE_IDLE) &&
                    (con->state == SMB2_MC_STATE_SURPLUS)) {
                    con->state = SMB2_MC_STATE_POTENTIAL;
                }
            }
        }
    }

    return 0;
}

static struct complete_nic_info_entry*
smb2_mc_get_nic_by_index(struct interface_info_list* list, uint64_t index)
{
	struct complete_nic_info_entry* info = NULL;

	TAILQ_FOREACH(info, list, next) {
		if (info->nic_index == index) {
			break;
		}
	}

	return info;
}

static struct complete_nic_info_entry*
smb2_mc_get_nic_by_ip(struct interface_info_list* list, struct sockaddr* ip)
{
	struct complete_nic_info_entry* info = NULL;

	TAILQ_FOREACH(info, list, next) {
        /* Check if ip belong to nic only if the nic RSS index is 0 */
		if (!(info->nic_index & SMB2_IF_RSS_INDEX_MASK) &&
            smb2_mc_does_ip_belong_to_interface(info, ip)) {
			break;
		}
	}

	return info;
}

static void
smb2_mc_insert_new_nic_by_speed(
    struct interface_info_list* list,
    uint32_t* list_counter,
    struct complete_nic_info_entry* nic_info,
    struct complete_nic_info_entry** prev_nic_info)
{
    if (TAILQ_EMPTY(list)) {
        TAILQ_INSERT_HEAD(list, nic_info, next);
        goto end;
    }

    bool inserted = false;
    struct complete_nic_info_entry* info;
    TAILQ_FOREACH(info, list, next) {
        if (info->nic_link_speed <= nic_info->nic_link_speed) {
            TAILQ_INSERT_BEFORE(info, nic_info,  next);
            inserted = true;
            break;
        }
        
        if (prev_nic_info) *prev_nic_info = info;
    }

    if (!inserted)
        TAILQ_INSERT_TAIL(list, nic_info, next);

end:
    if (list_counter)
        (*list_counter)++;
}

struct sock_addr_entry*
smb2_mc_does_ip_belong_to_interface(
    struct complete_nic_info_entry* info,
    struct sockaddr* ip)
{
	struct sock_addr_entry* addr;

	TAILQ_FOREACH(addr, &info->addr_list, next) {
		if (smb2_mc_sockaddr_cmp(addr->addr, ip)) {
            return addr;
			break;
		}
	}

	return NULL;
}

static uint8_t
smb2_mc_protocol_family_to_inet(uint16_t family)
{
	if (family == SMB2_QUERY_NETWORK_RESPONSE_IPV4_FAMILY) return AF_INET;
	if (family == SMB2_QUERY_NETWORK_RESPONSE_IPV6_FAMILY) return AF_INET6;

	return 0;
}

/*
 * Release connection list.
 * should be called only from ms_destroy
 * (ie a total tear down of the session_table).
 */
static void
smb2_mc_release_connection_list(struct session_network_interface_info* session_table)
{
    struct session_con_entry* con;
    if (TAILQ_EMPTY(&session_table->session_con_list)) return;

    while ((con = TAILQ_FIRST(&session_table->session_con_list)) != NULL) {
        /* remove it from the main list */
        TAILQ_REMOVE(&session_table->session_con_list, con, next);
        SMB_FREE(con, M_NSMBDEV);
    }
}

static void
smb2_mc_release_interface(
    struct interface_info_list* list,
    struct complete_nic_info_entry* interface,
    uint32_t *p_nic_count)
{
    struct sock_addr_entry* addr;
    while ((addr = TAILQ_FIRST(&interface->addr_list)) != NULL) {
        TAILQ_REMOVE(&interface->addr_list, addr, next);
        SMB_FREE(addr->addr, M_NSMBDEV);
        SMB_FREE(addr, M_NSMBDEV);
    }

    if (list != NULL)
        TAILQ_REMOVE(list, interface, next);
    
    if (p_nic_count)
        (*p_nic_count)--;
    
    SMB_LOG_MC("freeing nic idx %llu(RSS_%llu).\n",  
             interface->nic_index  & SMB2_IF_INDEX_MASK,
             interface->nic_index >> SMB2_IF_RSS_INDEX_SHIFT);
    
    SMB_FREE(interface, M_NSMBDEV);
}

static void
smb2_mc_release_interface_list(struct interface_info_list* list)
{
    struct complete_nic_info_entry* info;

    if (TAILQ_EMPTY(list))
        return;

    while ((info = TAILQ_FIRST(list)) != NULL) {
        smb2_mc_release_interface(list, info, NULL);
    }
}

/*
 * smb2_mc_reset_nic_list
 * This function is called after a successful reconnect to clear out any
 * current failed connections. After a successful reconnect, we assume the
 * server NICs have not changed, but the client NICs may be different.
 * This also fixes the problem where the alt channels are trying to establish
 * connections but reconnect has not finished yet and the alt channels get
 * marked as failed before reconnect succeeds.
 *
 * For all NICs, if they are currently set to failed or no potential,
 * reset them back to potential to be checked again.
 */
int
smb2_mc_reset_nic_list(struct session_network_interface_info *session_table)
{
    int error = 0;
    struct complete_nic_info_entry *nic, *nic_t;

    lck_mtx_lock(&session_table->interface_table_lck);
    
    TAILQ_FOREACH_SAFE(nic, &session_table->client_nic_info_list, next, nic_t) {
        /*
         * smb2_mc_reset_con_status handles() handles black listed NICs
         * and will change any failed/no potential connections to potential
         */
        smb2_mc_reset_con_status(session_table, nic, true);
    }

    lck_mtx_unlock(&session_table->interface_table_lck);
    return error;
}

static bool
smb2_mc_sockaddr_cmp(struct sockaddr *x, struct sockaddr *y)
{
    void *x_addr = NULL, *y_addr = NULL;
    uint32_t x_len = 0, y_len = 0;

    if (!smb2_extract_ip_and_len(x, &x_addr, &x_len)) {
        SMBERROR("Unsupported sockaddr x");
        return false;
    }

    if (!smb2_extract_ip_and_len(y, &y_addr, &y_len)) {
        SMBERROR("Unsupported sockaddr y");
        return false;
    }

    if (x_len != y_len) {
        return false;
    }

    if (memcmp(x_addr, y_addr, x_len)) {
        return false;
    } else {
        return true;
    }
}

static bool
    smb2_mc_safe_buffer_get_and_advance_16(
    uint8_t** buff,
    uint8_t* buff_end,
    uint16_t* res)
{
	if ((uint32_t)(buff_end - *buff) < sizeof(uint16_t))
		return false;

	*res = *((uint16_t*) *buff);
	*buff= *buff+ sizeof(uint16_t);
	return true;
}

static bool
smb2_mc_safe_buffer_get_and_advance_32(
    uint8_t** buff,
    uint8_t* buff_end,
    uint32_t* res)
{
	if ((uint32_t)(buff_end - *buff) < sizeof(uint32_t))
		return false;

	*res = *((uint32_t*) *buff);
	*buff= *buff+sizeof(uint32_t);
	return true;
}

static bool
smb2_mc_safe_buffer_get_and_advance_64(
    uint8_t** buff,
    uint8_t* buff_end,
    uint64_t* res)
{
	if ((uint32_t)(buff_end - *buff) < sizeof(uint64_t))
		return false;

	*res = *((uint64_t*) *buff);
	*buff= *buff+sizeof(uint64_t);
	return true;
}

static bool
smb2_mc_safe_buffer_get_and_advance_in6_addr(
    uint8_t** buff,
    uint8_t* buff_end,
    in6_addr_t* addr)
{
	if ((uint32_t)(buff_end - *buff) < sizeof(in6_addr_t))
		return false;

	memcpy((void*)addr, *buff, sizeof(in6_addr_t));
	*buff= *buff+sizeof(in6_addr_t);
	return true;
}

static int
smb2_mc_update_info_with_ip(
    struct complete_nic_info_entry* nic_info,
    struct sockaddr *addr,
    bool *changed)
{
    /* No need to add new existing IP */
    if (smb2_mc_does_ip_belong_to_interface(nic_info, addr))
        return 0;

    /* Make sure the params are the same */
    struct sock_addr_entry* new_addr;
    SMB_MALLOC(new_addr, struct sock_addr_entry*, sizeof(struct sock_addr_entry), M_NSMBDEV, M_WAITOK | M_ZERO);
    if (new_addr == NULL) {
        SMBERROR("failed to allocate struct sock_addr_entry!");
        return ENOMEM;
    }

    SMB_MALLOC(new_addr->addr, struct sockaddr *, addr->sa_len, M_NSMBDEV, M_WAITOK | M_ZERO);
    if (new_addr->addr == NULL) {
        SMB_FREE(new_addr, M_NSMBDEV);
        SMBERROR("failed to allocate struct sockaddr!");
        return ENOMEM;
    }
    memcpy((void*) new_addr->addr, (void*) addr, addr->sa_len);

    if (addr->sa_family == AF_INET) {
        nic_info->nic_ip_types |= SMB2_MC_IPV4;
    } else {
        nic_info->nic_ip_types |= SMB2_MC_IPV6;
    }

    TAILQ_INSERT_HEAD(&nic_info->addr_list, new_addr, next);

#ifdef SMB_DEBUG
    char str[128];
    smb2_sockaddr_to_str(new_addr->addr, str, sizeof(str));
    char spd[20];
    smb2_spd_to_txt(nic_info->nic_link_speed, spd, sizeof(spd));
    SMBDEBUG("Inserting if %3llu (RSS_%llu) addr %s (spd: %s).\n",
             nic_info->nic_index  & SMB2_IF_INDEX_MASK,
             nic_info->nic_index >> SMB2_IF_RSS_INDEX_SHIFT,
             str, spd);
#endif
    if (changed) (*changed) = true;
    return 0;
}

/*
 * smb2_mc_update_new_connection
 * This function is being used to create new connection entry.
 * It gets 2 nic index (client & server) and create a connection according
 * to the NIC's info.
 * This function should define the functionality of the new connection
 * according to the speed of the new connection.
 * Since the new connection speed might effect other connection, we need to
 * return an update.
 * The update will include any changes that accured due to the new connection.
 */
static int   
smb2_mc_update_new_connection(
    struct session_network_interface_info* session_table,
    uint64_t client_idx,
    uint64_t server_idx,
    struct session_con_entry **con_entry_p,
    struct smbiod* iod)
{
    struct complete_nic_info_entry* client_nic = smb2_mc_get_nic_by_index(&session_table->client_nic_info_list, client_idx);
    struct complete_nic_info_entry* server_nic = smb2_mc_get_nic_by_index(&session_table->server_nic_info_list, server_idx);
    if (client_nic == NULL || server_nic == NULL) {
		SMBERROR("couldn't find one of the nics [c- %llu, s-%llu (RSS %llu)]!\n",
                 client_idx,
                 server_idx  & SMB2_IF_INDEX_MASK,
                 server_idx >> SMB2_IF_RSS_INDEX_SHIFT);
		return EINVAL;
	}

    /* Set proper state for all client possible connections */
    struct session_con_entry* con_entry;
    TAILQ_FOREACH(con_entry, &client_nic->possible_connections, client_next) {
        if (con_entry->con_server_nic->nic_index == server_idx) {
            if (iod) con_entry->iod = iod;
            con_entry->state = SMB2_MC_STATE_CONNECTED;
            con_entry->con_client_nic->nic_state = SMB2_MC_STATE_USED;
            con_entry->con_server_nic->nic_state = SMB2_MC_STATE_USED;
            con_entry->con_speed = MIN(con_entry->con_client_nic->nic_link_speed, con_entry->con_server_nic->nic_link_speed);
            TAILQ_INSERT_TAIL(&session_table->successful_con_list, con_entry, success_next);
            if (con_entry_p) *con_entry_p = con_entry;
            
        } else if ( con_entry->state == SMB2_MC_STATE_POTENTIAL) {
            con_entry->state = SMB2_MC_STATE_SURPLUS;
        }
    }

    /* Set proper state for all server possible connections */
    TAILQ_FOREACH(con_entry, &session_table->session_con_list, next) {
        if ((con_entry->con_server_nic->nic_index == server_idx) &&
            (con_entry->state == SMB2_MC_STATE_POTENTIAL)) {
            con_entry->state = SMB2_MC_STATE_SURPLUS;
        }
    }

    return 0;
}

int smb2_mc_notifier_event(struct session_network_interface_info *session_table,
                           struct smbioc_client_interface        *client_info) {
    int error = 0;
    struct session_network_interface_info updated_interface_table;

    smb2_mc_init(&updated_interface_table, 0, 0, NULL, 0, 0);
    smb2_mc_parse_client_interface_array(&updated_interface_table, client_info);
    error = smb2_mc_update_nic_list_from_notifier(session_table, &updated_interface_table, true);
    smb2_mc_destroy(&updated_interface_table);
    if (error) {
        SMBDEBUG("smb2_mc_update_client_interface_array returned %d.\n", error);
        goto exit;
    }
    
exit:
    return error;
}



#ifdef SMB_DEBUG
static const char *smb2_mc_state_to_str(enum _SMB2_MC_CON_STATE state) {
    switch(state) {
        case SMB2_MC_STATE_POTENTIAL:
            return("Potential");
        case SMB2_MC_STATE_NO_POTENTIAL:
            return("No Potential");
        case SMB2_MC_STATE_IN_TRIAL:
            return("On Trial");
        case SMB2_MC_STATE_FAILED_TO_CONNECT:
            return("Failed");
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

static const char *smb2_mc_active_state_to_str(enum _SMB2_MC_CON_STATE state,
                                enum _SMB2_MC_CON_ACTIVE_STATE active_state) {
    
    if (state == SMB2_MC_STATE_CONNECTED) {
        switch(active_state) {
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


void smb2_mc_print_all_connections(struct session_network_interface_info *session_table) {
    struct session_con_entry *con;
    struct complete_nic_info_entry *client_nic;
    struct complete_nic_info_entry *server_nic;
    char spd[30];
    TAILQ_FOREACH(con, &session_table->session_con_list, next) {
        client_nic = con->con_client_nic;
        server_nic = con->con_server_nic;
        smb2_spd_to_txt(con->con_speed, spd, sizeof(spd));
        SMBDEBUG("Connection [c-%3llu(%u) s-%3llu(RSS_%llu)(%u)] Speed %14s, state %-12s %-8s, id %d %s\n",
                 client_nic->nic_index,
                 client_nic->nic_state,
                 server_nic->nic_index &  SMB2_IF_INDEX_MASK,
                 server_nic->nic_index >> SMB2_IF_RSS_INDEX_SHIFT,
                 server_nic->nic_state,
                 spd,
                 smb2_mc_state_to_str(con->state),
                 smb2_mc_active_state_to_str(con->state, con->active_state),
                 (con->iod) ? (con->iod->iod_id) : -1,
                 (con->iod) ? ((con->iod->iod_flags & SMBIOD_ALTERNATE_CHANNEL) ? "ALT" : "MAIN") : "");
    }

}
#endif
