/* Proxy classes for svn_fs.h
 * DO NOT EDIT -- AUTOMATICALLY GENERATED
 * BY build/generator/swig/header_wrappers.py */

/* No default constructors for opaque structs */
#ifdef SWIGPYTHON
%nodefault svn_fs_t;
%nodefault svn_fs_access_t;
%nodefault svn_fs_id_t;
%nodefault svn_fs_txn_t;
%nodefault svn_fs_root_t;
%nodefault svn_fs_path_change_iterator_t;
%nodefault svn_fs_history_t;
%nodefault svn_fs_lock_target_t;
#endif

/* Callback typemaps */
#ifdef SWIGPYTHON
%apply CALLABLE_CALLBACK {
  svn_fs_warning_callback_t,
  svn_fs_upgrade_notify_t,
  svn_fs_progress_notify_func_t,
  svn_fs_hotcopy_notify_t,
  svn_fs_freeze_func_t,
  svn_fs_mergeinfo_receiver_t,
  svn_fs_process_contents_func_t,
  svn_fs_lock_callback_t,
  svn_fs_get_locks_callback_t,
  svn_fs_pack_notify_t
};
%apply CALLABLE_CALLBACK * {
  svn_fs_warning_callback_t *,
  svn_fs_upgrade_notify_t *,
  svn_fs_progress_notify_func_t *,
  svn_fs_hotcopy_notify_t *,
  svn_fs_freeze_func_t *,
  svn_fs_mergeinfo_receiver_t *,
  svn_fs_process_contents_func_t *,
  svn_fs_lock_callback_t *,
  svn_fs_get_locks_callback_t *,
  svn_fs_pack_notify_t *
};
#endif

/* Baton typemaps */
#ifdef SWIGPYTHON
%apply void *PY_AS_VOID {
  void *baton,
  void *warning_baton,
  void *notify_baton,
  void *cancel_baton,
  void *freeze_baton,
  void *lock_baton,
  void *get_locks_baton
};
#endif

/* Includes */
%{
#include "svn_fs.h"
%}
%include svn_fs.h

/* Structure definitions */
#ifdef SWIGPYTHON
%opaque_proxy(svn_fs_t);
%opaque_proxy(svn_fs_access_t);
%opaque_proxy(svn_fs_id_t);
%opaque_proxy(svn_fs_txn_t);
%opaque_proxy(svn_fs_root_t);
%proxy(svn_fs_path_change3_t);
%proxy(svn_fs_path_change2_t);
%proxy(svn_fs_path_change_t);
%opaque_proxy(svn_fs_path_change_iterator_t);
%opaque_proxy(svn_fs_history_t);
%proxy(svn_fs_dirent_t);
%opaque_proxy(svn_fs_lock_target_t);
%proxy(svn_fs_fsfs_info_t);
%proxy(svn_fs_fsx_info_t);
%proxy(svn_fs_info_placeholder_t);
#endif

/* Callbacks */

%inline %{
static void svn_fs_invoke_warning_callback(
  svn_fs_warning_callback_t _obj, void *baton, svn_error_t *err) {
  _obj(baton, err);
}

static svn_error_t * svn_fs_invoke_upgrade_notify(
  svn_fs_upgrade_notify_t _obj, void *baton, apr_uint64_t number, svn_fs_upgrade_notify_action_t action, apr_pool_t *scratch_pool) {
  return _obj(baton, number, action, scratch_pool);
}

static void svn_fs_invoke_progress_notify_func(
  svn_fs_progress_notify_func_t _obj, svn_revnum_t revision, void *baton, apr_pool_t *pool) {
  _obj(revision, baton, pool);
}

static void svn_fs_invoke_hotcopy_notify(
  svn_fs_hotcopy_notify_t _obj, void *baton, svn_revnum_t start_revision, svn_revnum_t end_revision, apr_pool_t *scratch_pool) {
  _obj(baton, start_revision, end_revision, scratch_pool);
}

static svn_error_t * svn_fs_invoke_freeze_func(
  svn_fs_freeze_func_t _obj, void *baton, apr_pool_t *pool) {
  return _obj(baton, pool);
}

static svn_error_t * svn_fs_invoke_mergeinfo_receiver(
  svn_fs_mergeinfo_receiver_t _obj, const char *path, svn_mergeinfo_t mergeinfo, void *baton, apr_pool_t *scratch_pool) {
  return _obj(path, mergeinfo, baton, scratch_pool);
}

static svn_error_t * svn_fs_invoke_process_contents_func(
  svn_fs_process_contents_func_t _obj, const unsigned char *contents, apr_size_t len, void *baton, apr_pool_t *scratch_pool) {
  return _obj(contents, len, baton, scratch_pool);
}

static svn_error_t * svn_fs_invoke_lock_callback(
  svn_fs_lock_callback_t _obj, void *baton, const char *path, const svn_lock_t *lock, svn_error_t *fs_err, apr_pool_t *scratch_pool) {
  return _obj(baton, path, lock, fs_err, scratch_pool);
}

static svn_error_t * svn_fs_invoke_get_locks_callback(
  svn_fs_get_locks_callback_t _obj, void *baton, svn_lock_t *lock, apr_pool_t *pool) {
  return _obj(baton, lock, pool);
}

static svn_error_t * svn_fs_invoke_pack_notify(
  svn_fs_pack_notify_t _obj, void *baton, apr_int64_t shard, svn_fs_pack_notify_action_t action, apr_pool_t *pool) {
  return _obj(baton, shard, action, pool);
}

%}

#ifdef SWIGPYTHON
%funcptr_proxy(svn_fs_warning_callback_t, svn_fs_invoke_warning_callback);
%funcptr_proxy(svn_fs_upgrade_notify_t, svn_fs_invoke_upgrade_notify);
%funcptr_proxy(svn_fs_progress_notify_func_t, svn_fs_invoke_progress_notify_func);
%funcptr_proxy(svn_fs_hotcopy_notify_t, svn_fs_invoke_hotcopy_notify);
%funcptr_proxy(svn_fs_freeze_func_t, svn_fs_invoke_freeze_func);
%funcptr_proxy(svn_fs_mergeinfo_receiver_t, svn_fs_invoke_mergeinfo_receiver);
%funcptr_proxy(svn_fs_process_contents_func_t, svn_fs_invoke_process_contents_func);
%funcptr_proxy(svn_fs_lock_callback_t, svn_fs_invoke_lock_callback);
%funcptr_proxy(svn_fs_get_locks_callback_t, svn_fs_invoke_get_locks_callback);
%funcptr_proxy(svn_fs_pack_notify_t, svn_fs_invoke_pack_notify);

#endif
