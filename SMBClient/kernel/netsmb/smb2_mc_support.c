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
#include <netsmb/smb_subr.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb2_mc_support.h>

#define SMB2_QUERY_NETWORK_RESPONSE_IPV4_FAMILY     0x0002
#define SMB2_QUERY_NETWORK_RESPONSE_IPV6_FAMILY     0x0017

#define SMB2_MC_IPV4_LEN (0x10)
#define SMB2_MC_IPV6_LEN (0x1C)

static int  smb2_mc_add_new_interface_info_to_list(
            struct interface_info_list* list,
            uint32_t* list_counter,
            struct network_nic_info* new_info);
static int  smb2_mc_create_client_new_con_list(
            struct session_network_interface_info* info,
            struct session_con_entry* insert_after_entry,
            struct complete_nic_info_entry* client_nic);
static int  smb2_mc_create_session_con_list(
            struct session_network_interface_info* info);
static void smb2_mc_define_connection_functionality(
            struct session_network_interface_info* session_table,
            struct session_con_entry* new_con_entry);
static struct complete_nic_info_entry* smb2_mc_get_nic_by_index(
            struct interface_info_list* list,
            uint32_t index);
static struct complete_nic_info_entry* smb2_mc_get_nic_by_ip(
            struct interface_info_list* list,
            struct sockaddr* ip);
static void smb2_mc_insert_new_nic_by_speed(
            struct interface_info_list* list,
            uint32_t* list_counter,
            struct complete_nic_info_entry* nic_info);
static bool smb2_mc_is_ip_belongs_to_interface(
            struct complete_nic_info_entry* info,
            struct sockaddr* ip);
static uint8_t  smb2_mc_protocol_family_to_inet(uint16_t family);
static void smb2_mc_release_connection_entry(
            struct session_network_interface_info* info,
            struct session_con_entry* entry);
static void smb2_mc_release_connection_list(
            struct session_network_interface_info* session_table);
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
            struct network_nic_info *new_info);
static int  smb2_mc_update_new_connection(
            struct session_network_interface_info* session_table,
            uint32_t client_idx,
            uint32_t server_idx);

int
smb2_mc_ask_for_new_connection_trial(
    struct session_network_interface_info* session_table,
    struct session_con_entry** trial_vector,
    uint8_t max_out_len)
{
    struct complete_nic_info_entry* client_nic_to_trail;
    struct session_con_entry* con_entry;
    int ret_val = 0;

    lck_mtx_lock(&session_table->interface_table_lck);
    
    /* While we have open on trial connections,
       we don't want to make any decision */
    if (session_table->active_on_trial_connections != 0) goto exit;
    
    /* Find the first client NIC, that isn't being used */
    TAILQ_FOREACH(client_nic_to_trail,
                  &session_table->client_nic_info_list,
                  next) {
        if (client_nic_to_trail->nic_state == SMB2_MC_STATE_IDLE) {
            /*
             * Find the possible connection
             * for this unused client NIC
             */
            TAILQ_FOREACH(con_entry,
                          &client_nic_to_trail->possible_connections,
                          client_next) {
                
                if ((con_entry->state == SMB2_MC_STATE_POTENTIAL) &&
                    (con_entry->con_server_nic->nic_state == SMB2_MC_STATE_IDLE)) {

                    /* We found a pair */
                    con_entry->state = SMB2_MC_STATE_IN_TRIAL;
                    client_nic_to_trail->nic_state = SMB2_MC_STATE_ON_TRIAL;
                    con_entry->con_server_nic->nic_state = SMB2_MC_STATE_ON_TRIAL;

                    *trial_vector = con_entry;
                    trial_vector++;
                    ret_val++;
                    session_table->active_on_trial_connections++;
                    max_out_len--;
                    break;
                }
            }
        }
        /* Check we are not overflowing the array */
        if (!max_out_len) break;
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
    struct network_nic_info * client_info_array = NULL;
    int error = 0;

    uint32_t array_size = sizeof(struct network_nic_info) * client_info->total_buffer_size;
    
    lck_mtx_lock(&session_table->interface_table_lck);
    
    SMB_MALLOC(client_info_array,
           struct network_nic_info *, array_size,
           M_SMBTEMP, M_WAITOK | M_ZERO);
    if (!client_info_array) {
        SMBERROR("falied to allocate struct client_info_array!");
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
        error = smb2_mc_add_new_interface_info_to_list(&session_table->client_nic_info_list, &session_table->client_nic_count, client_info_entry);
        if (error) {
            SMBERROR("Adding new interface info ended with error %d!",
                 error);
            break;
        }

        client_info_entry = (struct network_nic_info *) ((uint8_t*) client_info_entry + client_info_entry->next_offset);
    }
    
exit:
    if (error) {
        smb2_mc_release_interface_list(&session_table->client_nic_info_list);
    }

    if (client_info_array != NULL) {
        SMB_FREE(client_info_array, M_SMBTEMP);
    }
    
    lck_mtx_unlock(&session_table->interface_table_lck);

    return error;
}

/*
 * Parse the server's network_interface_instance_info array and create a
 * complete_interface_info
 */
int
smb2_mc_parse_server_interface_array(
    struct session_network_interface_info* session_table,
    uint8_t* server_info_buffer,
    uint32_t buf_len)
{
    int error = 0;

    if (server_info_buffer == NULL)
        return EINVAL;

    lck_mtx_lock(&session_table->interface_table_lck);
    
    uint32_t next_offset = 0;
    uint8_t* buf_end = server_info_buffer + buf_len;

    struct network_nic_info* new_info = NULL;
    SMB_MALLOC(new_info, struct network_nic_info *, sizeof(struct network_nic_info) + 6, M_NSMBDEV, M_WAITOK | M_ZERO);
    if (new_info == NULL) {
        error = ENOMEM;
        goto done;
    }

    do {
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

            if (!smb2_mc_safe_buffer_get_and_advance_16(&interface_data, buf_end, &new_info->port)) { goto bad_offset; }
            uint32_t ipv4_addr;
            if (!smb2_mc_safe_buffer_get_and_advance_32(&interface_data, buf_end, &ipv4_addr)) { goto bad_offset; }
            ((struct sockaddr_in *)(void*)&new_info->addr)->sin_addr.s_addr = ipv4_addr;

        } else if (family == SMB2_QUERY_NETWORK_RESPONSE_IPV6_FAMILY) {
            new_info->addr.sa_len = SMB2_MC_IPV6_LEN;
            if (!smb2_mc_safe_buffer_get_and_advance_16(&interface_data, buf_end, &new_info->port)) { goto bad_offset; }

            uint32_t flow_info;
            if (!smb2_mc_safe_buffer_get_and_advance_32(&interface_data, buf_end, &flow_info)) { goto bad_offset; }

            in6_addr_t addr6;
            if (!smb2_mc_safe_buffer_get_and_advance_in6_addr(&interface_data, buf_end, &addr6)) { goto bad_offset; }
            memcpy( &((struct sockaddr_in6 *)(void*) &new_info->addr)->sin6_addr, (void*)&addr6, sizeof(in6_addr_t));

            uint32_t scope_id;
            if (!smb2_mc_safe_buffer_get_and_advance_32(&interface_data, buf_end, &scope_id)) { goto bad_offset; }
        } else {
            /* We don't support other kinds */
            continue;
        }

        /* Use the data to create new interface info */
        error = smb2_mc_add_new_interface_info_to_list(&session_table->server_nic_info_list, &session_table->server_nic_count, new_info);
        if (error)
            break;
    } while (next_offset);

    smb2_mc_create_session_con_list(session_table);

    goto done;

bad_offset:
    error = EINVAL;
done:
    if (error) smb2_mc_release_interface_list(&session_table->server_nic_info_list);
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
    
    lck_mtx_unlock(&session_table->interface_table_lck);
    
    lck_mtx_destroy(&session_table->interface_table_lck, session_st_lck_group);
}

void
smb2_mc_init(
    struct session_network_interface_info* session_table)
{
    
    session_table->client_nic_count = 0;
    session_table->server_nic_count = 0;
    TAILQ_INIT(& session_table->server_nic_info_list);
    TAILQ_INIT(& session_table->client_nic_info_list);
    TAILQ_INIT(& session_table->session_con_list);
    
    lck_mtx_init(&session_table->interface_table_lck, session_st_lck_group, session_st_lck_attr);
}

int
smb2_mc_report_connection_trial_results(
    struct session_network_interface_info* session_table,
    struct sockaddr* client_ip,
    struct sockaddr* server_ip,
    _SMB2_MC_CON_TRIAL_STATUS status)
{

    if (client_ip == NULL || server_ip == NULL) {
        SMBERROR("got bad ip");
        return EINVAL;
    }
    
    lck_mtx_lock(&session_table->interface_table_lck);
    
    int error = 0;
    struct complete_nic_info_entry* client_nic = smb2_mc_get_nic_by_ip(&session_table->client_nic_info_list, client_ip);
    struct complete_nic_info_entry* server_nic= smb2_mc_get_nic_by_ip(&session_table->server_nic_info_list, server_ip);
    if (client_nic == NULL || server_nic == NULL) {
        SMBERROR("couldn't find one of the nics!");
        error = EINVAL;
        goto exit;
    }
    
    if (!(status & SMB2_MC_TRIAL_MAIN))
        session_table->active_on_trial_connections--;

    SMBDEBUG("Update connection trial [c- %d, s- %d], status %d\n", client_nic->nic_index, server_nic->nic_index, status);

    if (status & SMB2_MC_TRIAL_PASSED) {
        error = smb2_mc_update_new_connection(session_table, client_nic->nic_index, server_nic->nic_index);
    } else {
        client_nic->nic_state = SMB2_MC_STATE_IDLE;
        server_nic->nic_state = SMB2_MC_STATE_IDLE;
        struct session_con_entry* con_entry;
        TAILQ_FOREACH(con_entry, &client_nic->possible_connections, client_next) {
            if (con_entry->con_server_nic->nic_index == server_nic->nic_index) {
                con_entry->state = SMB2_MC_STATE_FAILED_TO_CONNECT;
                break;
            }
        }
    }
    
exit:
    lck_mtx_unlock(&session_table->interface_table_lck);
    return error;
}

static int
smb2_mc_add_new_interface_info_to_list(
    struct interface_info_list* list,
    uint32_t* list_counter,
    struct network_nic_info* new_info)
{
    int error = 0;
    struct complete_nic_info_entry* nic_info = smb2_mc_get_nic_by_index(list, new_info->nic_index);
    bool new_nic = false;
    if (nic_info == NULL) { //Need to create a new nic element in the list
        new_nic = true;
        SMB_MALLOC(nic_info, struct complete_nic_info_entry*, sizeof(struct complete_nic_info_entry), M_NSMBDEV, M_WAITOK | M_ZERO);
        if (nic_info == NULL) {
            SMBERROR("falied to allocate struct complete_interface_entry!");
            return ENOMEM;
        }

        memset(nic_info, 0, sizeof(struct complete_nic_info_entry));
        TAILQ_INIT(&nic_info->addr_list);
    }

    /* Update with the most recent data */
    nic_info->nic_caps = new_info->nic_caps;
    nic_info->nic_index = new_info->nic_index;
    nic_info->nic_link_speed = new_info->nic_link_speed;
    nic_info->nic_type = new_info->nic_type;

    error = smb2_mc_update_info_with_ip(nic_info, new_info);

    if (!error && new_nic) {
        smb2_mc_insert_new_nic_by_speed(list, list_counter, nic_info);
    }

    return error;
}

static int
smb2_mc_create_client_new_con_list(struct session_network_interface_info* info,
                               struct session_con_entry* insert_after_entry,
                               struct complete_nic_info_entry* client_nic)
{
    TAILQ_INIT(&client_nic->possible_connections);
    struct complete_nic_info_entry* server_nic;
    struct session_con_entry* con_entry;
    
    /* Create new con entry for each server */
    TAILQ_FOREACH(server_nic,
                 &info->server_nic_info_list, next) {
        
        SMB_MALLOC(con_entry, struct session_con_entry*, sizeof(struct session_con_entry), M_NSMBDEV, M_WAITOK | M_ZERO);
        if (con_entry == NULL) {
            SMBERROR("falied to allocate struct session_con_entry!");
            return ENOMEM;
        }
        
        con_entry->con_client_nic = client_nic;
        con_entry->con_server_nic = server_nic;
        
        if (!(client_nic->nic_ip_types & server_nic->nic_ip_types))
            con_entry->state = SMB2_MC_STATE_NO_POTENTIAL;
        else
            con_entry->state = SMB2_MC_STATE_POTENTIAL;
        
        if (insert_after_entry == NULL)
            TAILQ_INSERT_TAIL(&info->session_con_list, con_entry, next);
        else
            TAILQ_INSERT_AFTER(&info->session_con_list, insert_after_entry, con_entry, next);
        TAILQ_INSERT_TAIL(&client_nic->possible_connections, con_entry, client_next);
    }
    
    return 0;
}

static int
smb2_mc_create_session_con_list(struct session_network_interface_info* info)
{
    int error = 0;
    TAILQ_INIT(&info->session_con_list);
    TAILQ_INIT(&info->successful_con_list);
    
    struct complete_nic_info_entry* client_nic;
    struct session_con_entry* insert_after_entry = NULL;

    /* For each client we have */
    TAILQ_FOREACH(client_nic,
                 &info->client_nic_info_list, next) {
        error = smb2_mc_create_client_new_con_list(info, insert_after_entry, client_nic);
        if (error) {
            smb2_mc_release_connection_list(info);
            break;
        }
        insert_after_entry = *info->session_con_list.tqh_last;
    }
    
    return error;
}

/*
 * smb2_mc_define_connection_functionality
 * Defines the new connection functionality according to it's speed
 * comparing to the existing connections speed.
 * In case the max speed changes, need to update all active connections
 */
static void
smb2_mc_define_connection_functionality(
    struct session_network_interface_info* session_table,
    struct session_con_entry* new_con_entry)
{

    TAILQ_INSERT_TAIL(&session_table->successful_con_list, new_con_entry, success_next);
    /*
     * If the new connection has the same speed as the max speed
     * this is an active connection
     * If the new connection has lower speed then the max speed
     * this is an inactive connection
     */
    if (new_con_entry->con_speed == session_table->current_max_con_speed) {
        new_con_entry->state |= SMB2_MC_FUNC_ACTIVE;
        return;
    } else if (new_con_entry->con_speed < session_table->current_max_con_speed) {
        new_con_entry->state |= SMB2_MC_FUNC_INACTIVE;
        return;
    } else {
        new_con_entry->state |= SMB2_MC_FUNC_ACTIVE;
    }

    /*
     * If the new connection has higher speed then the max speed
     * this is an active connection and we need to change all other
     * existing active connection to be inactive
     */
    session_table->current_max_con_speed = new_con_entry->con_speed;
    struct session_con_entry* exist_con;
    TAILQ_FOREACH(exist_con, &session_table->successful_con_list, success_next) {
        if ((exist_con->state & SMB2_MC_FUNC_ACTIVE) &&
            (exist_con->con_speed < session_table->current_max_con_speed)) {
            
            exist_con->state &= ~SMB2_MC_FUNC_ACTIVE;
            exist_con->state |= SMB2_MC_FUNC_INACTIVE;
            
            SMBERROR("connection functionality was changed to inactive: [c- %d, s- %d]",
                 exist_con->con_client_nic->nic_index,
                 exist_con->con_server_nic->nic_index);
            
            /* TODO: - this update should go out to the given iod */
        }
    }
}

static struct complete_nic_info_entry*
smb2_mc_get_nic_by_index(struct interface_info_list* list, uint32_t index)
{
    struct complete_nic_info_entry* info;

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
    struct complete_nic_info_entry* info;

    TAILQ_FOREACH(info, list, next) {
        if (smb2_mc_is_ip_belongs_to_interface(info, ip)) {
            break;
        }
    }

    return info;
}

static void
smb2_mc_insert_new_nic_by_speed(
    struct interface_info_list* list,
    uint32_t* list_counter,
    struct complete_nic_info_entry* nic_info)
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
    }

    if (!inserted)
        TAILQ_INSERT_TAIL(list, nic_info, next);

end:
    (*list_counter)++;
}

static bool
smb2_mc_is_ip_belongs_to_interface(
    struct complete_nic_info_entry* info,
    struct sockaddr* ip)
{
    bool found = false;
    struct sock_addr_entry* addr;

    TAILQ_FOREACH(addr, &info->addr_list, next) {
        if (smb2_mc_sockaddr_cmp(addr->addr, ip)) {
            found = true;
            break;
        }
    }

    return found;
}

static uint8_t
smb2_mc_protocol_family_to_inet(uint16_t family)
{
    if (family == SMB2_QUERY_NETWORK_RESPONSE_IPV4_FAMILY) return AF_INET;
    if (family == SMB2_QUERY_NETWORK_RESPONSE_IPV6_FAMILY) return AF_INET6;

    return 0;
}

static void
smb2_mc_release_connection_entry(struct session_network_interface_info* info, struct session_con_entry* entry)
{
    /* if the connection is successful connection
       need to remove it from the success list */
    if (entry->state == SMB2_MC_STATE_CONNECTED)
        TAILQ_REMOVE(&info->successful_con_list, entry, success_next);
    
    /* remove it from the client list */
    TAILQ_REMOVE(&entry->con_client_nic->possible_connections, entry, client_next);
    
    /* remove it from the main list */
    TAILQ_REMOVE(&info->session_con_list, entry, next);
    
    SMB_FREE(entry, M_NSMBDEV);
}

/*
 * Release connection list
 */
static void
smb2_mc_release_connection_list(struct session_network_interface_info* session_table)
{
    struct session_con_entry* con;
    if (TAILQ_EMPTY(&session_table->session_con_list)) return;

    while ((con = TAILQ_FIRST(&session_table->session_con_list)) != NULL) {
        smb2_mc_release_connection_entry(session_table, con);
    }
}

static void
smb2_mc_release_interface_list(struct interface_info_list* list)
{
    struct complete_nic_info_entry* info;
 
    if (TAILQ_EMPTY(list))
        return;

    while ((info = TAILQ_FIRST(list)) != NULL) {
        struct sock_addr_entry* addr;
        while ((addr = TAILQ_FIRST(&info->addr_list)) != NULL) {
            TAILQ_REMOVE(&info->addr_list, addr, next);
            SMB_FREE(addr->addr, M_NSMBDEV);
            SMB_FREE(addr, M_NSMBDEV);
        }

        TAILQ_REMOVE(list, info, next);
        SMB_FREE(info, M_NSMBDEV);
    }
}

static bool
smb2_mc_sockaddr_cmp(struct sockaddr *x, struct sockaddr *y)
{
    if (x->sa_family != y->sa_family)
        return false;

    if (x->sa_family == AF_INET) {
        struct sockaddr_in *xin = (void*)x, *yin = (void*)y;
        if (memcmp(&xin->sin_addr.s_addr, &yin->sin_addr.s_addr, sizeof(in_addr_t)))
            return false;
    } else if (x->sa_family == AF_INET6) {
        struct sockaddr_in6 *xin6 = (void*)x, *yin6 = (void*)y;
        if (memcmp(&xin6->sin6_addr, &yin6->sin6_addr, sizeof(struct in6_addr)))
            return false;
    }

    return true;
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
    struct network_nic_info *new_info)
{
    /* No need to add new existing IP */
    if (smb2_mc_is_ip_belongs_to_interface(nic_info, &new_info->addr))
        return 0;

    /* Make sure the params are the same */
    struct sock_addr_entry* new_addr;
    SMB_MALLOC(new_addr, struct sock_addr_entry*, sizeof(struct sock_addr_entry), M_NSMBDEV, M_WAITOK | M_ZERO);
    if (new_addr == NULL) {
        SMBERROR("falied to allocate struct sock_addr_entry!");
        return ENOMEM;
    }

    SMB_MALLOC(new_addr->addr, struct sockaddr *, new_info->addr.sa_len, M_NSMBDEV, M_WAITOK | M_ZERO);
    if (new_addr->addr == NULL) {
        SMB_FREE(new_addr, M_NSMBDEV);
        SMBERROR("falied to allocate struct sockaddr!");
        return ENOMEM;
    }
    memcpy((void*) new_addr->addr, (void*) &new_info->addr, new_info->addr.sa_len);

    if (new_info->addr.sa_family == AF_INET) {
        nic_info->nic_ip_types |= SMB2_MC_IPV4;
    } else {
        nic_info->nic_ip_types |= SMB2_MC_IPV6;
    }

    TAILQ_INSERT_HEAD(&nic_info->addr_list, new_addr, next);

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
    uint32_t client_idx,
    uint32_t server_idx)
{
    struct complete_nic_info_entry* client_nic = smb2_mc_get_nic_by_index(&session_table->client_nic_info_list, client_idx);
    struct complete_nic_info_entry* server_nic = smb2_mc_get_nic_by_index(&session_table->server_nic_info_list, server_idx);
    
    if (client_nic == NULL || server_nic == NULL) {
        SMBERROR("couldn't find one of the nics [c- %d, s-%d]!", client_idx, server_idx);
        return EINVAL;
    }

    //Set proper state for all client possible connections
    struct session_con_entry* con_entry;
    TAILQ_FOREACH(con_entry, &client_nic->possible_connections, client_next) {
        if (con_entry->con_server_nic->nic_index == server_idx) {
            con_entry->state = SMB2_MC_STATE_CONNECTED;
            con_entry->con_client_nic->nic_state = SMB2_MC_STATE_USED;
            con_entry->con_server_nic->nic_state = SMB2_MC_STATE_USED;
            con_entry->con_speed = MIN(con_entry->con_client_nic->nic_link_speed, con_entry->con_server_nic->nic_link_speed);
            smb2_mc_define_connection_functionality(session_table, con_entry);
        } else if ( con_entry->state == SMB2_MC_STATE_POTENTIAL) {
            con_entry->state = SMB2_MC_STATE_SURPLUS;
        }
    }

    //Set proper state for all server possible connections
    TAILQ_FOREACH(con_entry, &session_table->session_con_list, next) {
        if ((con_entry->con_server_nic->nic_index == server_idx) &&
            (con_entry->state == SMB2_MC_STATE_POTENTIAL)) {
            con_entry->state = SMB2_MC_STATE_SURPLUS;
        }
    }
    
    return 0;
}
