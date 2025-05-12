/*
 * Copyright (c) 2011 - 2023 Apple Inc. All rights reserved.
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

#ifndef _SMBFS_SMBFS_SUBR_2_H_
#define _SMBFS_SMBFS_SUBR_2_H_

struct compound_pb;

/* SMB Data compression */
int smb_check_user_list(const char* extension, size_t extension_len,
                             char *list[], uint32_t list_cnt);
int smb_compression_allowed(struct mount *mp, vnode_t vp);
bool smb_compression_excluded(const char* extension, size_t extension_len);

/* Directory Enumeration Caching functions */
void smb_dir_cache_add_entry(vnode_t dvp, void *in_cachep,
                             const char *name, size_t name_len,
                             struct smbfattr *fap,
                             uint32_t is_overflow, int is_locked);
void smb_dir_cache_check(vnode_t dvp, void *in_cachep, int is_locked,
                         vfs_context_t context);
int32_t smb_dir_cache_find_entry(vnode_t dvp, void *in_cachep,
                                 char *name, size_t name_len,
                                 struct smbfattr *fap, uint64_t req_attrs,
                                 vfs_context_t context);
int32_t smb_dir_cache_get_attrs(struct smb_share *share, vnode_t dvp,
                                void *in_cachep, int is_locked,
                                vfs_context_t context);
void smb_dir_cache_invalidate(vnode_t vp, uint32_t forceInvalidate);
void smb_dir_cache_remove(vnode_t dvp, void *in_cachep,
						  const char *cache, const char *reason,
						  int is_locked, off_t offset);
void smb_dir_cache_remove_one(vnode_t dvp, void *in_cachep,
                              void *in_one_entryp, int is_locked);


/* Global Directory Enumeration Caching functions */
void smb_global_dir_cache_add_entry(vnode_t dvp, int is_locked);
void smb_global_dir_cache_low_memory(int free_all, void *context_ptr);
void smb_global_dir_cache_prune(void *oldest_ptr, int is_locked,
                                vfs_context_t context);
void smb_global_dir_cache_remove(int is_locked, int remove_all);
void smb_global_dir_cache_remove_one(vnode_t dvp, int is_locked);
int32_t smb_global_dir_cache_update_entry(vnode_t dvp);


/* buf_map_range helper functions */
void smbfs_init_buf_map(void);
void smbfs_teardown_buf_map(void);
uint32_t smbfs_hash_upl_addr(void *addr);
int smbfs_buf_map(struct buf *bp, caddr_t *io_addr, vm_prot_t prot);
int smbfs_buf_unmap(struct buf *bp);


/* Helper functions */
int smb_fphelp(struct smbmount *smp, struct mbchain *mbp, struct smbnode *np,
               int usingUnicode, size_t *lenp);
int smb2fs_fullpath(struct mbchain *mbp, struct smbnode *dnp,
                    const char *namep, size_t name_len,
                    const char *strm_namep, size_t strm_name_len,
                    int name_flags, uint8_t sep_char);
void smb2fs_smb_file_id_check(struct smb_share *share, uint64_t ino,
                              char *network_name, uint32_t network_name_len);
uint64_t smb2fs_smb_file_id_get(struct smbmount *smp, uint64_t ino, const char *name);
vnode_t smbfs_smb_get_parent(struct smbnode *np, uint64_t flags);

int smb2fs_smb_cmpd_check_copyfile(struct smb_share *share,
                                   struct smbnode *create_np,
                                   vfs_context_t context);
int smb2fs_smb_cmpd_create(struct smb_share *share, struct smbnode *np,
                           const char *namep, size_t name_len,
                           const char *strm_namep, size_t strm_name_len,
                           uint32_t desired_access, enum vtype vnode_type,
                           uint32_t share_access, uint32_t disposition,
                           uint64_t create_flags, uint32_t *ntstatus,
                           SMBFID *fidp, struct smbfattr *fap,
                           size_t *acl_cache_len, struct ntsecdesc **acl_cache_data,
                           void *create_contextp, vfs_context_t context);
int smbfs_smb_cmpd_create_read_close(struct smb_share *share, struct smbnode *dnp,
                                   const char *namep, size_t name_len,
                                   const char *snamep, size_t sname_len,
                                   uio_t uio, size_t *sizep,
                                   uint32_t *max_access,
                                   vfs_context_t context);
int smb2fs_smb_cmpd_create_write(struct smb_share *share, struct smbnode *dnp,
                                 const char *namep, size_t name_len,
                                 const char *snamep, size_t sname_len,
                                 uint32_t desired_access, struct smbfattr *fap,
                                 uint64_t create_flags, uio_t uio,
                                 SMBFID *fidp, int ioflag,
                                 vfs_context_t context);
int smb2fs_smb_cmpd_create_write_xattr(struct smb_share *share, struct smbnode *np,
                                       const char *snamep, size_t sname_len,
                                       uint32_t desired_access, uint32_t open_disp,
                                       struct smbfattr *fap, uio_t uio,
                                       enum stream_types stype,
                                       vfs_context_t context);
int smb2fs_smb_cmpd_flush_close(struct smb_share *share,
                                struct smb2_close_rq *in_closep,
                                vfs_context_t context);
int smb2fs_smb_cmpd_query_async(struct smb_share *share, vnode_t dvp,
                                void *in_cachep, int32_t flags,
                                vfs_context_t context);
int smb2fs_smb_cmpd_query_async_fill(struct smb_share *share, struct smbnode *dnp,
                                     struct cached_dir_entry *currp, struct compound_pb *pb,
                                     int32_t flags, vfs_context_t context);
int smb2fs_smb_cmpd_query_async_parse(struct smb_share *share, struct mdchain *mdp, struct compound_pb *pb,
                                      int32_t flags, vfs_context_t context);
int smb2fs_smb_cmpd_query_dir_one(struct smb_share *share, struct smbnode *np,
                                  const char *query_namep, size_t query_name_len,
                                  struct smbfattr *fap, char **namep, size_t *name_lenp, size_t *name_allocsize,
                                  vfs_context_t context);
int smb2fs_smb_cmpd_reparse_point_get(struct smb_share *share, struct smbnode *np,
									  const char *namep, size_t name_len,
									  struct uio *ioctl_uiop, uint64_t *sizep,
									  vfs_context_t context);
int smb2fs_smb_cmpd_resolve_id(struct smb_share *share, struct smbnode *np,
                               uint64_t ino, uint32_t *resolve_errorp, char **pathp, size_t* pathp_allocsize,
                               vfs_context_t context);
int smb2fs_smb_cmpd_set_info(struct smb_share *share, struct smbnode *create_np, enum vtype vnode_type,
                             const char *create_namep, size_t create_name_len,
                             uint32_t create_xattr, uint32_t create_desired_access,
                             uint8_t setinfo_info_type, uint8_t setinfo_file_info_class,
                             uint32_t setinfo_add_info,
                             uint32_t setinfo_input_buffer_len, uint8_t *setinfo_input_buffer,
                             uint32_t *setinfo_ntstatus,
                             vfs_context_t context);

int smb2fs_smb_change_notify(struct smb_share *share, uint32_t output_buffer_len,
                             uint32_t completion_filter, 
                             void *fn_callback, void *fn_callback_args,
                             vfs_context_t context);
int smb2fs_smb_check_dur_handle_v2(struct smb_share *share, struct smbnode *dnp,
								   uint32_t *timeout, vfs_context_t context);
int smb2fs_smb_copyfile(struct smb_share *share, struct smbnode *src_np,
                        struct smbnode *tdnp, const char *tnamep,
                        size_t tname_len, vfs_context_t context);

int smbfs_smb_create_reparse_symlink(struct smb_share *share, struct smbnode *dnp,
                                     const char *namep, size_t name_len,
                                     char *targetp, size_t target_len,
                                     struct smbfattr *fap, vfs_context_t context);
int smbfs_smb_close(struct smb_share *share, SMBFID fid,
                    vfs_context_t context);
int smbfs_smb_close_attr(vnode_t vp, struct smb_share *share,
                         SMBFID fid, struct smb2_close_rq *in_closep,
                         vfs_context_t context);
int smbfs_smb_delete(struct smb_share *share, struct smbnode *np, enum vtype vnode_type,
                     const char *name, size_t nmlen,
                     int xattr, vfs_context_t context);
int smbfs_smb_flush(struct smb_share *share, SMBFID fid, uint32_t full_sync, 
                    vfs_context_t context);
int smb1fs_smb_findclose(struct smbfs_fctx *ctx, vfs_context_t context);
int smbfs_smb_findclose(struct smbfs_fctx *ctx, vfs_context_t context);
int smb1fs_smb_findnext(struct smbfs_fctx *ctx, vfs_context_t context);
int smbfs_smb_findnext(struct smbfs_fctx *ctx, vfs_context_t context);
int smb2fs_smb_lease_upgrade(struct smb_share *share, vnode_t vp,
                             const char *reason, vfs_context_t context);
int smbfs_smb_lock(struct smb_share *share, int op, SMBFID fid, uint32_t pid,
                   off_t start, uint64_t len, uint32_t timo, 
                   vfs_context_t context);
int smbfs_smb_markfordelete(struct smb_share *share, SMBFID fid, 
                            vfs_context_t context);
int smbfs_smb_ntcreatex(struct smb_share *share, struct smbnode *np,
                        uint32_t rights, uint32_t shareMode, enum vtype vt, 
                        SMBFID *fidp, const char *name, size_t in_nmlen, 
                        uint32_t disp, int xattr, struct smbfattr *fap,
                        int do_create, struct smb2_dur_hndl_and_lease *dur_hndl_leasep,
                        size_t *acl_cache_len, struct ntsecdesc **acl_cache_data,
                        vfs_context_t context);
int smbfs_smb_openread(struct smb_share *share, struct smbnode *np,
                       SMBFID *fidp, uint32_t desired_access,
                       uio_t uio, size_t *sizep, const char *stream_namep,
                       struct timespec *mtimep, vfs_context_t context);
void smbfs_smb_qfsattr(struct smbmount *smp, vfs_context_t context);
int smb1fs_smb_qpathinfo(struct smb_share *share, struct smbnode *np,
                         struct smbfattr *fap, short infolevel,
                         const char **namep, size_t *nmlenp, 
                         vfs_context_t context);
int smbfs_smb_qpathinfo(struct smb_share *share, struct smbnode *np, enum vtype vnode_type,
                        struct smbfattr *fap, short infolevel, 
                        const char **namep, size_t *nmlenp,  size_t *name_allocsize,
                        vfs_context_t context);
int smbfs_smb_qstreaminfo(struct smb_share *share, struct smbnode *np, enum vtype vnode_type,
                          const char *namep, size_t name_len,
                          const char *stream_namep,
                          uio_t uio, size_t *sizep,
                          uint64_t *strmsize, uint64_t *strm_alloc_size,
                          uint32_t *stream_flagsp, uint32_t *max_accessp,
                          vfs_context_t context);
int smbfs_smb_query_info(struct smb_share *share, struct smbnode *dnp, enum vtype vnode_type,
                         const char *in_name, size_t len, uint32_t *attr, 
                         vfs_context_t context);
int smbfs_smb_rename(struct smb_share *share, struct smbnode *src, 
                     struct smbnode *tdnp, const char *tname, size_t tnmlen, 
                     vfs_context_t context);
int smbfs_smb_reparse_read_symlink(struct smb_share *share, struct smbnode *np,
                                   struct uio *uiop, vfs_context_t context);
int smbfs_smb_rmdir(struct smb_share *share, struct smbnode *np,
                    vfs_context_t context);
int smb2fs_smb_security_get(struct smb_share *share, struct smbnode *np,
                            uint32_t desired_access, uint32_t security_attrs,
                            struct ntsecdesc **resp, uint32_t *resp_len,
                            vfs_context_t context);
int smb2fs_smb_security_set(struct smb_share *share, struct smbnode *np,
                            uint32_t desired_access, SMBFID fid,
                            uint32_t security_attrs, uint16_t control_flags,
                            struct ntsid *owner, struct ntsid *group,
                            struct ntacl *sacl, struct ntacl *dacl,
                            vfs_context_t context);
int smbfs_smb_set_allocation(struct smb_share *share, SMBFID fid,
                             uint64_t new_size, vfs_context_t context);
int smbfs_smb_seteof(struct smb_share *share, SMBFID fid, uint64_t newsize, 
                     vfs_context_t context);
int smbfs_smb_setfattrNT(struct smb_share *share, uint32_t attr, SMBFID fid, 
                         struct timespec *crtime, struct timespec *mtime, 
                         struct timespec *atime, vfs_context_t context);
int smbfs_smb_setpattr(struct smb_share *share, struct smbnode *np, enum vtype vnode_type,
                       const char *namep, size_t name_len,
                       uint16_t attr, vfs_context_t context);
int smbfs_smb_setpattrNT(struct smb_share *share, struct smbnode *np, enum vtype vnode_type,
                         const char *namep, size_t name_len,
                         uint32_t attr, struct timespec *crtime, 
                         struct timespec *mtime, struct timespec *atime, 
                         vfs_context_t context);
int smb2fs_smb_setpattrNT(struct smb_share *share, struct smbnode *np, enum vtype vnode_type,
                          const char *namep, size_t name_len,
                          uint32_t attr, struct timespec *crtime,
                          struct timespec *mtime, struct timespec *atime,
                          vfs_context_t context);
int smbfs_smb_setsec(struct smb_share *share, struct smbnode *np,
                     uint32_t desired_access, SMBFID fid,
                     uint32_t selector, uint16_t control_flags,
                     struct ntsid *owner, struct ntsid *group,
                     struct ntacl *sacl, struct ntacl *dacl,
                     vfs_context_t context);
int smbfs_smb_statfs(struct smbmount *smp, struct vfsstatfs *sbp,
                     vfs_context_t context);
int smb2fs_smb_validate_neg_info(struct smb_share *share, vfs_context_t context);
int smb2fs_smb_query_network_interface_info(struct smb_share *share, vfs_context_t context);
int smbfs_smb_undollardata(const char *fname, char *name, size_t *nmlen,
                           uint32_t *is_data);
struct smbfs_fctx_query_t* smb2fs_smb_add_fctx_query(struct smbfs_fctx *ctx);
int smb2fs_smb_free_fctx_query_head(struct smbfs_fctx *ctx);
int smb2fs_smb_free_fctx_query_tail(struct smbfs_fctx *ctx);
int smbfs_add_dir_entry(vnode_t dvp, uio_t uio, int flags, const char *name, size_t name_len,
                        struct smbfattr *fap, int is_attrlist);
int smbfs_enum_dir(struct vnode *dvp, uio_t uio, int is_attrlist, void* vnop_argsp);
uint32_t smbfs_fill_direntry(void *de, const char *name, size_t nmlen, uint8_t dtype,
                    uint64_t ino, int flags);
#endif
