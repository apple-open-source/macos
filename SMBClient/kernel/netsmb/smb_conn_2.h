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

#ifndef _NETSMB_CONN_2_H_
#define _NETSMB_CONN_2_H_

#ifdef _KERNEL

int smb2_smb_change_notify(struct smb_share *share, struct smb2_change_notify_rq *changep,
                           struct smb_rq **in_rqp, vfs_context_t context);
int smb2_smb_close(struct smb_share *share, struct smb2_close_rq *closep, 
                   struct smb_rq **compound_rqp, struct smbiod *iod, vfs_context_t context);
int smb2_smb_close_fid(struct smb_share *share, SMBFID fid,
                       struct smb_rq **compound_rqp,
                       struct smb2_close_rq **in_closep,
                       struct smbiod *iod,
                       vfs_context_t context);
int smb2_smb_create(struct smb_share *share, struct smb2_create_rq *createp, 
                    struct smb_rq **compound_rqp, vfs_context_t context);
int smb2_smb_dur_handle_init(struct smb_share *share, struct smbnode *np,
							 uint64_t flags, struct smb2_durable_handle *dur_handlep);
void smb2_smb_dur_handle_free(struct smb2_durable_handle *dur_handlep);
int smb_smb_echo(struct smbiod *iod, int timeout, uint32_t EchoCount,
                 vfs_context_t context);
int smb2_smb_flush(struct smb_share *share, SMBFID fid, uint32_t full_sync,
				   vfs_context_t context);
uint32_t smb2_smb_get_client_capabilities(struct smb_session *sessionp);
uint32_t smb2_smb_get_client_dialects(struct smb_session *sessionp, int inReconnect,
                                      uint16_t *dialect_cnt, uint16_t dialects[],
                                      size_t max_dialects_size);
uint16_t smb2_smb_get_client_security_mode(struct smb_session *sessionp);
int smb2_smb_gss_session_setup(struct smbiod *iod, uint16_t *sess_flags,
                               vfs_context_t context);
int smb2_smb_ioctl(struct smb_share *share, struct smbiod *iod, struct smb2_ioctl_rq *ioctlp, 
                   struct smb_rq **compound_rqp, vfs_context_t context);
int smb2_smb_lease_break_ack(struct smb_share *share, struct smbiod *iod, uint64_t lease_key_hi, uint64_t lease_key_low,
                             uint32_t lease_state, uint32_t *ret_lease_state, vfs_context_t context);
int smb2_smb_lock(struct smb_share *share, int op, SMBFID fid,
                  off_t offset, uint64_t length, vfs_context_t context);
int smb2_smb_negotiate(struct smbiod *iod, struct smb_rq *rqp,
                       int inReconnect, vfs_context_t user_context,
                       vfs_context_t context);
int smb_smb_negotiate(struct smbiod *iod, vfs_context_t user_context, 
                      int inReconnect, vfs_context_t context);
int smb_smb_nomux(struct smb_session *sessionp, const char *name, vfs_context_t context);
int smb2_smb_parse_change_notify(struct smb_rq *rqp, uint32_t *events);
int smb2_smb_parse_create(struct smb_share *share, struct mdchain *mdp,
                          struct smb2_create_rq *createp);
int smb2_smb_parse_close(struct mdchain *mdp, struct smb2_close_rq *closep);
int smb2_smb_parse_ioctl(struct mdchain *mdp, struct smb2_ioctl_rq *ioctlp);
int smb2_smb_parse_lease_break(struct smbiod *iod, mbuf_t m);
int smb2_smb_parse_read_one(struct mdchain *mdp, user_ssize_t *rresid,
                            struct smb2_rw_rq *rwp);
int smb2_smb_parse_svrmsg_notify(struct smb_rq *rqp,
                                 uint32_t *svr_action,
                                 uint32_t *delay);
int smb2_smb_parse_query_dir(struct mdchain *mdp,
                             struct smb2_query_dir_rq *queryp);
int smb2_smb_parse_query_dir_both_dir_info(struct smb_share *share, struct mdchain *mdp,
                                           uint16_t info_level,
                                           void *ctxp, struct smbfattr *fap,
                                           char *network_name, uint32_t *network_name_len,
                                           size_t max_network_name_buffer_size);
int smb2_smb_parse_query_info(struct mdchain *mdp,
                              struct smb2_query_info_rq *queryp);
int smb2_smb_parse_set_info(struct mdchain *mdp,
                            struct smb2_set_info_rq *infop);
int smb2_smb_parse_security(struct mdchain *mdp,
                            struct smb2_query_info_rq *queryp);
int smb2_smb_parse_write_one(struct mdchain *mdp,
                             user_ssize_t *rresid,
                             struct smb2_rw_rq *writep);
int smb2_smb_query_dir(struct smb_share *share, struct smb2_query_dir_rq *queryp,
                       struct smb_rq **compound_rqp, struct smbiod *iod, vfs_context_t context);
int smb2_smb_query_info(struct smb_share *share, struct smb2_query_info_rq *queryp, 
                        struct smb_rq **compound_rqp, struct smbiod *iod, vfs_context_t context);
int smb2_smb_read_one(struct smb_share *share,
                      struct smb2_rw_rq *readp,
                      user_ssize_t *len,
                      user_ssize_t *rresid,
                      struct smb_rq **compound_rqp,
                      struct smbiod *iod,
                      vfs_context_t context);
int smb2_smb_read(struct smb_share *share, struct smb2_rw_rq *readp,
                  vfs_context_t context);
int smb_smb_read(struct smb_share *share, SMBFID fid, uio_t uio, 
                 vfs_context_t context);
int smb2_smb_set_info(struct smb_share *share, struct smb2_set_info_rq *infop,
                      struct smb_rq **compound_rqp, struct smbiod *iod, vfs_context_t context);
int smb1_smb_ssnclose(struct smb_session *sessionp, vfs_context_t context);
int smb_smb_ssnclose(struct smb_session *sessionp, vfs_context_t context);
int smb2_smb_tree_connect(struct smb_session *sessionp, struct smb_share *share,
                          const char *serverName, size_t serverNameLen, 
                          vfs_context_t context);
int smb1_smb_treedisconnect(struct smb_share *share, vfs_context_t context);
int smb_smb_treedisconnect(struct smb_share *share, vfs_context_t context);
int smb2_smb_write(struct smb_share *share, struct smb2_rw_rq *writep,
                   vfs_context_t context);
int smb2_smb_write_one(struct smb_share *share,
                       struct smb2_rw_rq *writep,
                       user_ssize_t *len,
                       user_ssize_t *rresid,
                       struct smb_rq **compound_rqp,
                       struct smbiod *iod,
                       vfs_context_t context);
int smb_smb_write(struct smb_share *share, SMBFID fid, uio_t uio, int ioflag,
                  vfs_context_t context);

uint32_t smb2_session_maxread(struct smb_session *sessionp, uint32_t max_read);
uint32_t smb2_session_maxwrite(struct smb_session *sessionp, uint32_t max_write);
int smb_get_share(struct smb_session *sessionp, struct smb_share **sharepp);


#endif /* _KERNEL */
#endif /* _NETSMB_CONN_2_H_ */
