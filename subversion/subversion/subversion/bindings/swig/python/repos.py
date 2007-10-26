# This file was created automatically by SWIG.
# Don't modify this file, modify the SWIG interface instead.
# This file is compatible with both classic and new-style classes.

import _repos

def _swig_setattr_nondynamic(self,class_type,name,value,static=1):
    if (name == "this"):
        if isinstance(value, class_type):
            self.__dict__[name] = value.this
            if hasattr(value,"thisown"): self.__dict__["thisown"] = value.thisown
            del value.thisown
            return
    method = class_type.__swig_setmethods__.get(name,None)
    if method: return method(self,value)
    if (not static) or hasattr(self,name) or (name == "thisown"):
        self.__dict__[name] = value
    else:
        raise AttributeError("You cannot add attributes to %s" % self)

def _swig_setattr(self,class_type,name,value):
    return _swig_setattr_nondynamic(self,class_type,name,value,0)

def _swig_getattr(self,class_type,name):
    method = class_type.__swig_getmethods__.get(name,None)
    if method: return method(self)
    raise AttributeError,name

import core
import delta
import fs

def svn_repos_version(*args):
    """svn_repos_version() -> svn_version_t"""
    return apply(_repos.svn_repos_version, args)
svn_authz_none = _repos.svn_authz_none
svn_authz_read = _repos.svn_authz_read
svn_authz_write = _repos.svn_authz_write
svn_authz_recursive = _repos.svn_authz_recursive

def svn_repos_find_root_path(*args):
    """svn_repos_find_root_path(char path, apr_pool_t pool) -> char"""
    return apply(_repos.svn_repos_find_root_path, args)

def svn_repos_open(*args):
    """svn_repos_open(svn_repos_t repos_p, char path, apr_pool_t pool) -> svn_error_t"""
    return apply(_repos.svn_repos_open, args)

def svn_repos_create(*args):
    """
    svn_repos_create(svn_repos_t repos_p, char path, char unused_1, char unused_2, 
        apr_hash_t config, apr_hash_t fs_config, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_create, args)

def svn_repos_delete(*args):
    """svn_repos_delete(char path, apr_pool_t pool) -> svn_error_t"""
    return apply(_repos.svn_repos_delete, args)

def svn_repos_fs(*args):
    """svn_repos_fs(svn_repos_t repos) -> svn_fs_t"""
    return apply(_repos.svn_repos_fs, args)

def svn_repos_hotcopy(*args):
    """
    svn_repos_hotcopy(char src_path, char dst_path, svn_boolean_t clean_logs, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_hotcopy, args)

def svn_repos_recover(*args):
    """svn_repos_recover(char path, apr_pool_t pool) -> svn_error_t"""
    return apply(_repos.svn_repos_recover, args)

def svn_repos_recover2(*args):
    """
    svn_repos_recover2(char path, svn_boolean_t nonblocking, svn_error_t start_callback, 
        void start_callback_baton, apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_recover2, args)

def svn_repos_db_logfiles(*args):
    """
    svn_repos_db_logfiles(apr_array_header_t logfiles, char path, svn_boolean_t only_unused, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_db_logfiles, args)

def svn_repos_path(*args):
    """svn_repos_path(svn_repos_t repos, apr_pool_t pool) -> char"""
    return apply(_repos.svn_repos_path, args)

def svn_repos_db_env(*args):
    """svn_repos_db_env(svn_repos_t repos, apr_pool_t pool) -> char"""
    return apply(_repos.svn_repos_db_env, args)

def svn_repos_conf_dir(*args):
    """svn_repos_conf_dir(svn_repos_t repos, apr_pool_t pool) -> char"""
    return apply(_repos.svn_repos_conf_dir, args)

def svn_repos_svnserve_conf(*args):
    """svn_repos_svnserve_conf(svn_repos_t repos, apr_pool_t pool) -> char"""
    return apply(_repos.svn_repos_svnserve_conf, args)

def svn_repos_lock_dir(*args):
    """svn_repos_lock_dir(svn_repos_t repos, apr_pool_t pool) -> char"""
    return apply(_repos.svn_repos_lock_dir, args)

def svn_repos_db_lockfile(*args):
    """svn_repos_db_lockfile(svn_repos_t repos, apr_pool_t pool) -> char"""
    return apply(_repos.svn_repos_db_lockfile, args)

def svn_repos_db_logs_lockfile(*args):
    """svn_repos_db_logs_lockfile(svn_repos_t repos, apr_pool_t pool) -> char"""
    return apply(_repos.svn_repos_db_logs_lockfile, args)

def svn_repos_hook_dir(*args):
    """svn_repos_hook_dir(svn_repos_t repos, apr_pool_t pool) -> char"""
    return apply(_repos.svn_repos_hook_dir, args)

def svn_repos_start_commit_hook(*args):
    """svn_repos_start_commit_hook(svn_repos_t repos, apr_pool_t pool) -> char"""
    return apply(_repos.svn_repos_start_commit_hook, args)

def svn_repos_pre_commit_hook(*args):
    """svn_repos_pre_commit_hook(svn_repos_t repos, apr_pool_t pool) -> char"""
    return apply(_repos.svn_repos_pre_commit_hook, args)

def svn_repos_post_commit_hook(*args):
    """svn_repos_post_commit_hook(svn_repos_t repos, apr_pool_t pool) -> char"""
    return apply(_repos.svn_repos_post_commit_hook, args)

def svn_repos_pre_revprop_change_hook(*args):
    """svn_repos_pre_revprop_change_hook(svn_repos_t repos, apr_pool_t pool) -> char"""
    return apply(_repos.svn_repos_pre_revprop_change_hook, args)

def svn_repos_post_revprop_change_hook(*args):
    """svn_repos_post_revprop_change_hook(svn_repos_t repos, apr_pool_t pool) -> char"""
    return apply(_repos.svn_repos_post_revprop_change_hook, args)

def svn_repos_pre_lock_hook(*args):
    """svn_repos_pre_lock_hook(svn_repos_t repos, apr_pool_t pool) -> char"""
    return apply(_repos.svn_repos_pre_lock_hook, args)

def svn_repos_post_lock_hook(*args):
    """svn_repos_post_lock_hook(svn_repos_t repos, apr_pool_t pool) -> char"""
    return apply(_repos.svn_repos_post_lock_hook, args)

def svn_repos_pre_unlock_hook(*args):
    """svn_repos_pre_unlock_hook(svn_repos_t repos, apr_pool_t pool) -> char"""
    return apply(_repos.svn_repos_pre_unlock_hook, args)

def svn_repos_post_unlock_hook(*args):
    """svn_repos_post_unlock_hook(svn_repos_t repos, apr_pool_t pool) -> char"""
    return apply(_repos.svn_repos_post_unlock_hook, args)

def svn_repos_begin_report(*args):
    """
    svn_repos_begin_report(void report_baton, svn_revnum_t revnum, char username, 
        svn_repos_t repos, char fs_base, char target, 
        char tgt_path, svn_boolean_t text_deltas, 
        svn_boolean_t recurse, svn_boolean_t ignore_ancestry, 
        svn_delta_editor_t editor, void edit_baton, 
        svn_repos_authz_func_t authz_read_func, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_begin_report, args)

def svn_repos_set_path2(*args):
    """
    svn_repos_set_path2(void report_baton, char path, svn_revnum_t revision, 
        svn_boolean_t start_empty, char lock_token, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_set_path2, args)

def svn_repos_set_path(*args):
    """
    svn_repos_set_path(void report_baton, char path, svn_revnum_t revision, 
        svn_boolean_t start_empty, apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_set_path, args)

def svn_repos_link_path2(*args):
    """
    svn_repos_link_path2(void report_baton, char path, char link_path, svn_revnum_t revision, 
        svn_boolean_t start_empty, 
        char lock_token, apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_link_path2, args)

def svn_repos_link_path(*args):
    """
    svn_repos_link_path(void report_baton, char path, char link_path, svn_revnum_t revision, 
        svn_boolean_t start_empty, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_link_path, args)

def svn_repos_delete_path(*args):
    """svn_repos_delete_path(void report_baton, char path, apr_pool_t pool) -> svn_error_t"""
    return apply(_repos.svn_repos_delete_path, args)

def svn_repos_finish_report(*args):
    """svn_repos_finish_report(void report_baton, apr_pool_t pool) -> svn_error_t"""
    return apply(_repos.svn_repos_finish_report, args)

def svn_repos_abort_report(*args):
    """svn_repos_abort_report(void report_baton, apr_pool_t pool) -> svn_error_t"""
    return apply(_repos.svn_repos_abort_report, args)

def svn_repos_dir_delta(*args):
    """
    svn_repos_dir_delta(svn_fs_root_t src_root, char src_parent_dir, char src_entry, 
        svn_fs_root_t tgt_root, char tgt_path, 
        svn_delta_editor_t editor, void edit_baton, 
        svn_repos_authz_func_t authz_read_func, svn_boolean_t text_deltas, 
        svn_boolean_t recurse, 
        svn_boolean_t entry_props, svn_boolean_t ignore_ancestry, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_dir_delta, args)

def svn_repos_replay2(*args):
    """
    svn_repos_replay2(svn_fs_root_t root, char base_dir, svn_revnum_t low_water_mark, 
        svn_boolean_t send_deltas, svn_delta_editor_t editor, 
        void edit_baton, svn_repos_authz_func_t authz_read_func, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_replay2, args)

def svn_repos_replay(*args):
    """
    svn_repos_replay(svn_fs_root_t root, svn_delta_editor_t editor, void edit_baton, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_replay, args)

def svn_repos_get_commit_editor4(*args):
    """
    svn_repos_get_commit_editor4(svn_delta_editor_t editor, void edit_baton, svn_repos_t repos, 
        svn_fs_txn_t txn, char repos_url, 
        char base_path, char user, char log_msg, svn_commit_callback2_t callback, 
        void callback_baton, 
        svn_repos_authz_callback_t authz_callback, 
        void authz_baton, apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_get_commit_editor4, args)

def svn_repos_get_commit_editor3(*args):
    """
    svn_repos_get_commit_editor3(svn_delta_editor_t editor, void edit_baton, svn_repos_t repos, 
        svn_fs_txn_t txn, char repos_url, 
        char base_path, char user, char log_msg, svn_commit_callback_t callback, 
        void callback_baton, 
        svn_repos_authz_callback_t authz_callback, 
        void authz_baton, apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_get_commit_editor3, args)

def svn_repos_get_commit_editor2(*args):
    """
    svn_repos_get_commit_editor2(svn_delta_editor_t editor, void edit_baton, svn_repos_t repos, 
        svn_fs_txn_t txn, char repos_url, 
        char base_path, char user, char log_msg, svn_commit_callback_t callback, 
        void callback_baton, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_get_commit_editor2, args)

def svn_repos_get_commit_editor(*args):
    """
    svn_repos_get_commit_editor(svn_delta_editor_t editor, void edit_baton, svn_repos_t repos, 
        char repos_url, char base_path, char user, 
        char log_msg, svn_commit_callback_t callback, 
        void callback_baton, apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_get_commit_editor, args)

def svn_repos_dated_revision(*args):
    """
    svn_repos_dated_revision(svn_revnum_t revision, svn_repos_t repos, apr_time_t tm, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_dated_revision, args)

def svn_repos_get_committed_info(*args):
    """
    svn_repos_get_committed_info(svn_revnum_t committed_rev, char committed_date, char last_author, 
        svn_fs_root_t root, char path, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_get_committed_info, args)

def svn_repos_stat(*args):
    """
    svn_repos_stat(svn_dirent_t dirent, svn_fs_root_t root, char path, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_stat, args)

def svn_repos_history2(*args):
    """
    svn_repos_history2(svn_fs_t fs, char path, svn_repos_history_func_t history_func, 
        svn_repos_authz_func_t authz_read_func, 
        svn_revnum_t start, svn_revnum_t end, 
        svn_boolean_t cross_copies, apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_history2, args)

def svn_repos_history(*args):
    """
    svn_repos_history(svn_fs_t fs, char path, svn_repos_history_func_t history_func, 
        svn_revnum_t start, svn_revnum_t end, 
        svn_boolean_t cross_copies, apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_history, args)

def svn_repos_trace_node_locations(*args):
    """
    svn_repos_trace_node_locations(svn_fs_t fs, apr_hash_t locations, char fs_path, svn_revnum_t peg_revision, 
        apr_array_header_t location_revisions, 
        svn_repos_authz_func_t authz_read_func, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_trace_node_locations, args)

def svn_repos_get_logs3(*args):
    """
    svn_repos_get_logs3(svn_repos_t repos, apr_array_header_t paths, svn_revnum_t start, 
        svn_revnum_t end, int limit, svn_boolean_t discover_changed_paths, 
        svn_boolean_t strict_node_history, 
        svn_repos_authz_func_t authz_read_func, 
        svn_log_message_receiver_t receiver, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_get_logs3, args)

def svn_repos_get_logs2(*args):
    """
    svn_repos_get_logs2(svn_repos_t repos, apr_array_header_t paths, svn_revnum_t start, 
        svn_revnum_t end, svn_boolean_t discover_changed_paths, 
        svn_boolean_t strict_node_history, 
        svn_repos_authz_func_t authz_read_func, 
        svn_log_message_receiver_t receiver, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_get_logs2, args)

def svn_repos_get_logs(*args):
    """
    svn_repos_get_logs(svn_repos_t repos, apr_array_header_t paths, svn_revnum_t start, 
        svn_revnum_t end, svn_boolean_t discover_changed_paths, 
        svn_boolean_t strict_node_history, 
        svn_log_message_receiver_t receiver, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_get_logs, args)

def svn_repos_get_file_revs(*args):
    """
    svn_repos_get_file_revs(svn_repos_t repos, char path, svn_revnum_t start, svn_revnum_t end, 
        svn_repos_authz_func_t authz_read_func, 
        svn_repos_file_rev_handler_t handler, 
        void handler_baton, apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_get_file_revs, args)

def svn_repos_fs_commit_txn(*args):
    """
    svn_repos_fs_commit_txn(char conflict_p, svn_repos_t repos, svn_revnum_t new_rev, 
        svn_fs_txn_t txn, apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_fs_commit_txn, args)

def svn_repos_fs_begin_txn_for_commit(*args):
    """
    svn_repos_fs_begin_txn_for_commit(svn_fs_txn_t txn_p, svn_repos_t repos, svn_revnum_t rev, 
        char author, char log_msg, apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_fs_begin_txn_for_commit, args)

def svn_repos_fs_begin_txn_for_update(*args):
    """
    svn_repos_fs_begin_txn_for_update(svn_fs_txn_t txn_p, svn_repos_t repos, svn_revnum_t rev, 
        char author, apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_fs_begin_txn_for_update, args)

def svn_repos_fs_lock(*args):
    """
    svn_repos_fs_lock(svn_lock_t lock, svn_repos_t repos, char path, char token, 
        char comment, svn_boolean_t is_dav_comment, 
        apr_time_t expiration_date, svn_revnum_t current_rev, 
        svn_boolean_t steal_lock, apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_fs_lock, args)

def svn_repos_fs_unlock(*args):
    """
    svn_repos_fs_unlock(svn_repos_t repos, char path, char token, svn_boolean_t break_lock, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_fs_unlock, args)

def svn_repos_fs_get_locks(*args):
    """
    svn_repos_fs_get_locks(apr_hash_t locks, svn_repos_t repos, char path, svn_repos_authz_func_t authz_read_func, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_fs_get_locks, args)

def svn_repos_fs_change_rev_prop2(*args):
    """
    svn_repos_fs_change_rev_prop2(svn_repos_t repos, svn_revnum_t rev, char author, char name, 
        svn_string_t new_value, svn_repos_authz_func_t authz_read_func, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_fs_change_rev_prop2, args)

def svn_repos_fs_change_rev_prop(*args):
    """
    svn_repos_fs_change_rev_prop(svn_repos_t repos, svn_revnum_t rev, char author, char name, 
        svn_string_t new_value, apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_fs_change_rev_prop, args)

def svn_repos_fs_revision_prop(*args):
    """
    svn_repos_fs_revision_prop(svn_string_t value_p, svn_repos_t repos, svn_revnum_t rev, 
        char propname, svn_repos_authz_func_t authz_read_func, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_fs_revision_prop, args)

def svn_repos_fs_revision_proplist(*args):
    """
    svn_repos_fs_revision_proplist(apr_hash_t table_p, svn_repos_t repos, svn_revnum_t rev, 
        svn_repos_authz_func_t authz_read_func, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_fs_revision_proplist, args)

def svn_repos_fs_change_node_prop(*args):
    """
    svn_repos_fs_change_node_prop(svn_fs_root_t root, char path, char name, svn_string_t value, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_fs_change_node_prop, args)

def svn_repos_fs_change_txn_prop(*args):
    """svn_repos_fs_change_txn_prop(svn_fs_txn_t txn, char name, svn_string_t value, apr_pool_t pool) -> svn_error_t"""
    return apply(_repos.svn_repos_fs_change_txn_prop, args)
class svn_repos_node_t:
    """Proxy of C svn_repos_node_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, svn_repos_node_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, svn_repos_node_t, name)
    def __init__(self): raise RuntimeError, "No constructor defined"
    def __repr__(self):
        return "<%s.%s; proxy of C svn_repos_node_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    __swig_setmethods__["kind"] = _repos.svn_repos_node_t_kind_set
    __swig_getmethods__["kind"] = _repos.svn_repos_node_t_kind_get
    __swig_setmethods__["action"] = _repos.svn_repos_node_t_action_set
    __swig_getmethods__["action"] = _repos.svn_repos_node_t_action_get
    __swig_setmethods__["text_mod"] = _repos.svn_repos_node_t_text_mod_set
    __swig_getmethods__["text_mod"] = _repos.svn_repos_node_t_text_mod_get
    __swig_setmethods__["prop_mod"] = _repos.svn_repos_node_t_prop_mod_set
    __swig_getmethods__["prop_mod"] = _repos.svn_repos_node_t_prop_mod_get
    __swig_setmethods__["name"] = _repos.svn_repos_node_t_name_set
    __swig_getmethods__["name"] = _repos.svn_repos_node_t_name_get
    __swig_setmethods__["copyfrom_rev"] = _repos.svn_repos_node_t_copyfrom_rev_set
    __swig_getmethods__["copyfrom_rev"] = _repos.svn_repos_node_t_copyfrom_rev_get
    __swig_setmethods__["copyfrom_path"] = _repos.svn_repos_node_t_copyfrom_path_set
    __swig_getmethods__["copyfrom_path"] = _repos.svn_repos_node_t_copyfrom_path_get
    __swig_setmethods__["sibling"] = _repos.svn_repos_node_t_sibling_set
    __swig_getmethods__["sibling"] = _repos.svn_repos_node_t_sibling_get
    __swig_setmethods__["child"] = _repos.svn_repos_node_t_child_set
    __swig_getmethods__["child"] = _repos.svn_repos_node_t_child_get
    __swig_setmethods__["parent"] = _repos.svn_repos_node_t_parent_set
    __swig_getmethods__["parent"] = _repos.svn_repos_node_t_parent_get
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for svn_repos_node_t"""
      import libsvn.core, weakref
      self.__dict__["_parent_pool"] = \
        parent_pool or libsvn.core.application_pool;
      if self.__dict__["_parent_pool"]:
        self.__dict__["_is_valid"] = weakref.ref(
          self.__dict__["_parent_pool"]._is_valid)

    def assert_valid(self):
      """Assert that this object is using valid pool memory"""
      if "_is_valid" in self.__dict__:
        assert self.__dict__["_is_valid"](), "Variable has already been deleted"

    def __getattr__(self, name):
      """Get an attribute from this object"""
      self.assert_valid()
      value = _swig_getattr(self, self.__class__, name)
      try:
        old_dict = self.__dict__["_member_dicts"][name]
        value.__dict__["_parent_pool"] = old_dict.get("_parent_pool")
        value.__dict__["_member_dicts"] = old_dict.get("_member_dicts")
        value.__dict__["_is_valid"] = old_dict.get("_is_valid")
        value.assert_valid()
      except KeyError:
        pass
      return value

    def __setattr__(self, name, value):
      """Set an attribute on this object"""
      self.assert_valid()
      try:
        self.__dict__.setdefault("_member_dicts",{})[name] = value.__dict__
      except AttributeError:
        pass
      return _swig_setattr(self, self.__class__, name, value)


class svn_repos_node_tPtr(svn_repos_node_t):
    def __init__(self, this):
        _swig_setattr(self, svn_repos_node_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, svn_repos_node_t, 'thisown', 0)
        _swig_setattr(self, svn_repos_node_t,self.__class__,svn_repos_node_t)
_repos.svn_repos_node_t_swigregister(svn_repos_node_tPtr)


def svn_repos_node_editor(*args):
    """
    svn_repos_node_editor(svn_delta_editor_t editor, void edit_baton, svn_repos_t repos, 
        svn_fs_root_t base_root, svn_fs_root_t root, 
        apr_pool_t node_pool, apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_node_editor, args)

def svn_repos_node_from_baton(*args):
    """svn_repos_node_from_baton(void edit_baton) -> svn_repos_node_t"""
    return apply(_repos.svn_repos_node_from_baton, args)
SVN_REPOS_DUMPFILE_MAGIC_HEADER = _repos.SVN_REPOS_DUMPFILE_MAGIC_HEADER
SVN_REPOS_DUMPFILE_FORMAT_VERSION = _repos.SVN_REPOS_DUMPFILE_FORMAT_VERSION
SVN_REPOS_DUMPFILE_UUID = _repos.SVN_REPOS_DUMPFILE_UUID
SVN_REPOS_DUMPFILE_CONTENT_LENGTH = _repos.SVN_REPOS_DUMPFILE_CONTENT_LENGTH
SVN_REPOS_DUMPFILE_REVISION_NUMBER = _repos.SVN_REPOS_DUMPFILE_REVISION_NUMBER
SVN_REPOS_DUMPFILE_NODE_PATH = _repos.SVN_REPOS_DUMPFILE_NODE_PATH
SVN_REPOS_DUMPFILE_NODE_KIND = _repos.SVN_REPOS_DUMPFILE_NODE_KIND
SVN_REPOS_DUMPFILE_NODE_ACTION = _repos.SVN_REPOS_DUMPFILE_NODE_ACTION
SVN_REPOS_DUMPFILE_NODE_COPYFROM_PATH = _repos.SVN_REPOS_DUMPFILE_NODE_COPYFROM_PATH
SVN_REPOS_DUMPFILE_NODE_COPYFROM_REV = _repos.SVN_REPOS_DUMPFILE_NODE_COPYFROM_REV
SVN_REPOS_DUMPFILE_TEXT_COPY_SOURCE_CHECKSUM = _repos.SVN_REPOS_DUMPFILE_TEXT_COPY_SOURCE_CHECKSUM
SVN_REPOS_DUMPFILE_TEXT_CONTENT_CHECKSUM = _repos.SVN_REPOS_DUMPFILE_TEXT_CONTENT_CHECKSUM
SVN_REPOS_DUMPFILE_PROP_CONTENT_LENGTH = _repos.SVN_REPOS_DUMPFILE_PROP_CONTENT_LENGTH
SVN_REPOS_DUMPFILE_TEXT_CONTENT_LENGTH = _repos.SVN_REPOS_DUMPFILE_TEXT_CONTENT_LENGTH
SVN_REPOS_DUMPFILE_PROP_DELTA = _repos.SVN_REPOS_DUMPFILE_PROP_DELTA
SVN_REPOS_DUMPFILE_TEXT_DELTA = _repos.SVN_REPOS_DUMPFILE_TEXT_DELTA
svn_node_action_change = _repos.svn_node_action_change
svn_node_action_add = _repos.svn_node_action_add
svn_node_action_delete = _repos.svn_node_action_delete
svn_node_action_replace = _repos.svn_node_action_replace
svn_repos_load_uuid_default = _repos.svn_repos_load_uuid_default
svn_repos_load_uuid_ignore = _repos.svn_repos_load_uuid_ignore
svn_repos_load_uuid_force = _repos.svn_repos_load_uuid_force

def svn_repos_dump_fs2(*args):
    """
    svn_repos_dump_fs2(svn_repos_t repos, svn_stream_t dumpstream, svn_stream_t feedback_stream, 
        svn_revnum_t start_rev, 
        svn_revnum_t end_rev, svn_boolean_t incremental, 
        svn_boolean_t use_deltas, svn_cancel_func_t cancel_func, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_dump_fs2, args)

def svn_repos_dump_fs(*args):
    """
    svn_repos_dump_fs(svn_repos_t repos, svn_stream_t dumpstream, svn_stream_t feedback_stream, 
        svn_revnum_t start_rev, 
        svn_revnum_t end_rev, svn_boolean_t incremental, 
        svn_cancel_func_t cancel_func, apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_dump_fs, args)

def svn_repos_load_fs2(*args):
    """
    svn_repos_load_fs2(svn_repos_t repos, svn_stream_t dumpstream, svn_stream_t feedback_stream, 
        enum svn_repos_load_uuid uuid_action, 
        char parent_dir, svn_boolean_t use_pre_commit_hook, 
        svn_boolean_t use_post_commit_hook, 
        svn_cancel_func_t cancel_func, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_load_fs2, args)

def svn_repos_load_fs(*args):
    """
    svn_repos_load_fs(svn_repos_t repos, svn_stream_t dumpstream, svn_stream_t feedback_stream, 
        enum svn_repos_load_uuid uuid_action, 
        char parent_dir, svn_cancel_func_t cancel_func, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_load_fs, args)
class svn_repos_parse_fns2_t:
    """Proxy of C svn_repos_parse_fns2_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, svn_repos_parse_fns2_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, svn_repos_parse_fns2_t, name)
    def __init__(self): raise RuntimeError, "No constructor defined"
    def __repr__(self):
        return "<%s.%s; proxy of C svn_repos_parse_fns2_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    __swig_setmethods__["new_revision_record"] = _repos.svn_repos_parse_fns2_t_new_revision_record_set
    __swig_getmethods__["new_revision_record"] = _repos.svn_repos_parse_fns2_t_new_revision_record_get
    __swig_setmethods__["uuid_record"] = _repos.svn_repos_parse_fns2_t_uuid_record_set
    __swig_getmethods__["uuid_record"] = _repos.svn_repos_parse_fns2_t_uuid_record_get
    __swig_setmethods__["new_node_record"] = _repos.svn_repos_parse_fns2_t_new_node_record_set
    __swig_getmethods__["new_node_record"] = _repos.svn_repos_parse_fns2_t_new_node_record_get
    __swig_setmethods__["set_revision_property"] = _repos.svn_repos_parse_fns2_t_set_revision_property_set
    __swig_getmethods__["set_revision_property"] = _repos.svn_repos_parse_fns2_t_set_revision_property_get
    __swig_setmethods__["set_node_property"] = _repos.svn_repos_parse_fns2_t_set_node_property_set
    __swig_getmethods__["set_node_property"] = _repos.svn_repos_parse_fns2_t_set_node_property_get
    __swig_setmethods__["delete_node_property"] = _repos.svn_repos_parse_fns2_t_delete_node_property_set
    __swig_getmethods__["delete_node_property"] = _repos.svn_repos_parse_fns2_t_delete_node_property_get
    __swig_setmethods__["remove_node_props"] = _repos.svn_repos_parse_fns2_t_remove_node_props_set
    __swig_getmethods__["remove_node_props"] = _repos.svn_repos_parse_fns2_t_remove_node_props_get
    __swig_setmethods__["set_fulltext"] = _repos.svn_repos_parse_fns2_t_set_fulltext_set
    __swig_getmethods__["set_fulltext"] = _repos.svn_repos_parse_fns2_t_set_fulltext_get
    __swig_setmethods__["apply_textdelta"] = _repos.svn_repos_parse_fns2_t_apply_textdelta_set
    __swig_getmethods__["apply_textdelta"] = _repos.svn_repos_parse_fns2_t_apply_textdelta_get
    __swig_setmethods__["close_node"] = _repos.svn_repos_parse_fns2_t_close_node_set
    __swig_getmethods__["close_node"] = _repos.svn_repos_parse_fns2_t_close_node_get
    __swig_setmethods__["close_revision"] = _repos.svn_repos_parse_fns2_t_close_revision_set
    __swig_getmethods__["close_revision"] = _repos.svn_repos_parse_fns2_t_close_revision_get
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for svn_repos_parse_fns2_t"""
      import libsvn.core, weakref
      self.__dict__["_parent_pool"] = \
        parent_pool or libsvn.core.application_pool;
      if self.__dict__["_parent_pool"]:
        self.__dict__["_is_valid"] = weakref.ref(
          self.__dict__["_parent_pool"]._is_valid)

    def assert_valid(self):
      """Assert that this object is using valid pool memory"""
      if "_is_valid" in self.__dict__:
        assert self.__dict__["_is_valid"](), "Variable has already been deleted"

    def __getattr__(self, name):
      """Get an attribute from this object"""
      self.assert_valid()
      value = _swig_getattr(self, self.__class__, name)
      try:
        old_dict = self.__dict__["_member_dicts"][name]
        value.__dict__["_parent_pool"] = old_dict.get("_parent_pool")
        value.__dict__["_member_dicts"] = old_dict.get("_member_dicts")
        value.__dict__["_is_valid"] = old_dict.get("_is_valid")
        value.assert_valid()
      except KeyError:
        pass
      return value

    def __setattr__(self, name, value):
      """Set an attribute on this object"""
      self.assert_valid()
      try:
        self.__dict__.setdefault("_member_dicts",{})[name] = value.__dict__
      except AttributeError:
        pass
      return _swig_setattr(self, self.__class__, name, value)


class svn_repos_parse_fns2_tPtr(svn_repos_parse_fns2_t):
    def __init__(self, this):
        _swig_setattr(self, svn_repos_parse_fns2_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, svn_repos_parse_fns2_t, 'thisown', 0)
        _swig_setattr(self, svn_repos_parse_fns2_t,self.__class__,svn_repos_parse_fns2_t)
_repos.svn_repos_parse_fns2_t_swigregister(svn_repos_parse_fns2_tPtr)


def svn_repos_parse_dumpstream2(*args):
    """
    svn_repos_parse_dumpstream2(svn_stream_t stream, svn_repos_parse_fns2_t parse_fns, 
        void parse_baton, svn_cancel_func_t cancel_func, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_parse_dumpstream2, args)

def svn_repos_get_fs_build_parser2(*args):
    """
    svn_repos_get_fs_build_parser2(svn_repos_parse_fns2_t parser, void parse_baton, svn_repos_t repos, 
        svn_boolean_t use_history, enum svn_repos_load_uuid uuid_action, 
        svn_stream_t outstream, 
        char parent_dir, apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_get_fs_build_parser2, args)
class svn_repos_parser_fns_t:
    """Proxy of C svn_repos_parser_fns_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, svn_repos_parser_fns_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, svn_repos_parser_fns_t, name)
    def __init__(self): raise RuntimeError, "No constructor defined"
    def __repr__(self):
        return "<%s.%s; proxy of C svn_repos_parser_fns_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    __swig_setmethods__["new_revision_record"] = _repos.svn_repos_parser_fns_t_new_revision_record_set
    __swig_getmethods__["new_revision_record"] = _repos.svn_repos_parser_fns_t_new_revision_record_get
    __swig_setmethods__["uuid_record"] = _repos.svn_repos_parser_fns_t_uuid_record_set
    __swig_getmethods__["uuid_record"] = _repos.svn_repos_parser_fns_t_uuid_record_get
    __swig_setmethods__["new_node_record"] = _repos.svn_repos_parser_fns_t_new_node_record_set
    __swig_getmethods__["new_node_record"] = _repos.svn_repos_parser_fns_t_new_node_record_get
    __swig_setmethods__["set_revision_property"] = _repos.svn_repos_parser_fns_t_set_revision_property_set
    __swig_getmethods__["set_revision_property"] = _repos.svn_repos_parser_fns_t_set_revision_property_get
    __swig_setmethods__["set_node_property"] = _repos.svn_repos_parser_fns_t_set_node_property_set
    __swig_getmethods__["set_node_property"] = _repos.svn_repos_parser_fns_t_set_node_property_get
    __swig_setmethods__["remove_node_props"] = _repos.svn_repos_parser_fns_t_remove_node_props_set
    __swig_getmethods__["remove_node_props"] = _repos.svn_repos_parser_fns_t_remove_node_props_get
    __swig_setmethods__["set_fulltext"] = _repos.svn_repos_parser_fns_t_set_fulltext_set
    __swig_getmethods__["set_fulltext"] = _repos.svn_repos_parser_fns_t_set_fulltext_get
    __swig_setmethods__["close_node"] = _repos.svn_repos_parser_fns_t_close_node_set
    __swig_getmethods__["close_node"] = _repos.svn_repos_parser_fns_t_close_node_get
    __swig_setmethods__["close_revision"] = _repos.svn_repos_parser_fns_t_close_revision_set
    __swig_getmethods__["close_revision"] = _repos.svn_repos_parser_fns_t_close_revision_get
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for svn_repos_parse_fns_t"""
      import libsvn.core, weakref
      self.__dict__["_parent_pool"] = \
        parent_pool or libsvn.core.application_pool;
      if self.__dict__["_parent_pool"]:
        self.__dict__["_is_valid"] = weakref.ref(
          self.__dict__["_parent_pool"]._is_valid)

    def assert_valid(self):
      """Assert that this object is using valid pool memory"""
      if "_is_valid" in self.__dict__:
        assert self.__dict__["_is_valid"](), "Variable has already been deleted"

    def __getattr__(self, name):
      """Get an attribute from this object"""
      self.assert_valid()
      value = _swig_getattr(self, self.__class__, name)
      try:
        old_dict = self.__dict__["_member_dicts"][name]
        value.__dict__["_parent_pool"] = old_dict.get("_parent_pool")
        value.__dict__["_member_dicts"] = old_dict.get("_member_dicts")
        value.__dict__["_is_valid"] = old_dict.get("_is_valid")
        value.assert_valid()
      except KeyError:
        pass
      return value

    def __setattr__(self, name, value):
      """Set an attribute on this object"""
      self.assert_valid()
      try:
        self.__dict__.setdefault("_member_dicts",{})[name] = value.__dict__
      except AttributeError:
        pass
      return _swig_setattr(self, self.__class__, name, value)


class svn_repos_parser_fns_tPtr(svn_repos_parser_fns_t):
    def __init__(self, this):
        _swig_setattr(self, svn_repos_parser_fns_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, svn_repos_parser_fns_t, 'thisown', 0)
        _swig_setattr(self, svn_repos_parser_fns_t,self.__class__,svn_repos_parser_fns_t)
_repos.svn_repos_parser_fns_t_swigregister(svn_repos_parser_fns_tPtr)


def svn_repos_parse_dumpstream(*args):
    """
    svn_repos_parse_dumpstream(svn_stream_t stream,  parse_fns, void parse_baton, 
        svn_cancel_func_t cancel_func, apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_parse_dumpstream, args)

def svn_repos_get_fs_build_parser(*args):
    """
    svn_repos_get_fs_build_parser( parser, void parse_baton, svn_repos_t repos, svn_boolean_t use_history, 
        enum svn_repos_load_uuid uuid_action, 
        svn_stream_t outstream, char parent_dir, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_get_fs_build_parser, args)

def svn_repos_authz_read(*args):
    """
    svn_repos_authz_read(svn_authz_t authz_p, char file, svn_boolean_t must_exist, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_authz_read, args)

def svn_repos_authz_check_access(*args):
    """
    svn_repos_authz_check_access(svn_authz_t authz, char repos_name, char path, char user, 
        svn_repos_authz_access_t required_access, 
        svn_boolean_t access_granted, apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_authz_check_access, args)
class svn_repos_t:
    """Proxy of C svn_repos_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, svn_repos_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, svn_repos_t, name)
    def __init__(self): raise RuntimeError, "No constructor defined"
    def __repr__(self):
        return "<%s.%s; proxy of C svn_repos_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for svn_repos_t"""
      import libsvn.core, weakref
      self.__dict__["_parent_pool"] = \
        parent_pool or libsvn.core.application_pool;
      if self.__dict__["_parent_pool"]:
        self.__dict__["_is_valid"] = weakref.ref(
          self.__dict__["_parent_pool"]._is_valid)

    def assert_valid(self):
      """Assert that this object is using valid pool memory"""
      if "_is_valid" in self.__dict__:
        assert self.__dict__["_is_valid"](), "Variable has already been deleted"

    def __getattr__(self, name):
      """Get an attribute from this object"""
      self.assert_valid()
      value = _swig_getattr(self, self.__class__, name)
      try:
        old_dict = self.__dict__["_member_dicts"][name]
        value.__dict__["_parent_pool"] = old_dict.get("_parent_pool")
        value.__dict__["_member_dicts"] = old_dict.get("_member_dicts")
        value.__dict__["_is_valid"] = old_dict.get("_is_valid")
        value.assert_valid()
      except KeyError:
        pass
      return value

    def __setattr__(self, name, value):
      """Set an attribute on this object"""
      self.assert_valid()
      try:
        self.__dict__.setdefault("_member_dicts",{})[name] = value.__dict__
      except AttributeError:
        pass
      return _swig_setattr(self, self.__class__, name, value)


class svn_repos_tPtr(svn_repos_t):
    def __init__(self, this):
        _swig_setattr(self, svn_repos_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, svn_repos_t, 'thisown', 0)
        _swig_setattr(self, svn_repos_t,self.__class__,svn_repos_t)
_repos.svn_repos_t_swigregister(svn_repos_tPtr)

class svn_authz_t:
    """Proxy of C svn_authz_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, svn_authz_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, svn_authz_t, name)
    def __init__(self): raise RuntimeError, "No constructor defined"
    def __repr__(self):
        return "<%s.%s; proxy of C svn_authz_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for svn_authz_t"""
      import libsvn.core, weakref
      self.__dict__["_parent_pool"] = \
        parent_pool or libsvn.core.application_pool;
      if self.__dict__["_parent_pool"]:
        self.__dict__["_is_valid"] = weakref.ref(
          self.__dict__["_parent_pool"]._is_valid)

    def assert_valid(self):
      """Assert that this object is using valid pool memory"""
      if "_is_valid" in self.__dict__:
        assert self.__dict__["_is_valid"](), "Variable has already been deleted"

    def __getattr__(self, name):
      """Get an attribute from this object"""
      self.assert_valid()
      value = _swig_getattr(self, self.__class__, name)
      try:
        old_dict = self.__dict__["_member_dicts"][name]
        value.__dict__["_parent_pool"] = old_dict.get("_parent_pool")
        value.__dict__["_member_dicts"] = old_dict.get("_member_dicts")
        value.__dict__["_is_valid"] = old_dict.get("_is_valid")
        value.assert_valid()
      except KeyError:
        pass
      return value

    def __setattr__(self, name, value):
      """Set an attribute on this object"""
      self.assert_valid()
      try:
        self.__dict__.setdefault("_member_dicts",{})[name] = value.__dict__
      except AttributeError:
        pass
      return _swig_setattr(self, self.__class__, name, value)


class svn_authz_tPtr(svn_authz_t):
    def __init__(self, this):
        _swig_setattr(self, svn_authz_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, svn_authz_t, 'thisown', 0)
        _swig_setattr(self, svn_authz_t,self.__class__,svn_authz_t)
_repos.svn_authz_t_swigregister(svn_authz_tPtr)


def svn_repos_parse_fns2_invoke_new_revision_record(*args):
    """
    svn_repos_parse_fns2_invoke_new_revision_record(svn_repos_parse_fns2_t _obj, void revision_baton, apr_hash_t headers, 
        void parse_baton, apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_parse_fns2_invoke_new_revision_record, args)

def svn_repos_parse_fns2_invoke_uuid_record(*args):
    """
    svn_repos_parse_fns2_invoke_uuid_record(svn_repos_parse_fns2_t _obj, char uuid, void parse_baton, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_parse_fns2_invoke_uuid_record, args)

def svn_repos_parse_fns2_invoke_new_node_record(*args):
    """
    svn_repos_parse_fns2_invoke_new_node_record(svn_repos_parse_fns2_t _obj, void node_baton, apr_hash_t headers, 
        void revision_baton, apr_pool_t pool) -> svn_error_t
    """
    return apply(_repos.svn_repos_parse_fns2_invoke_new_node_record, args)

def svn_repos_parse_fns2_invoke_set_revision_property(*args):
    """
    svn_repos_parse_fns2_invoke_set_revision_property(svn_repos_parse_fns2_t _obj, void revision_baton, char name, 
        svn_string_t value) -> svn_error_t
    """
    return apply(_repos.svn_repos_parse_fns2_invoke_set_revision_property, args)

def svn_repos_parse_fns2_invoke_set_node_property(*args):
    """
    svn_repos_parse_fns2_invoke_set_node_property(svn_repos_parse_fns2_t _obj, void node_baton, char name, 
        svn_string_t value) -> svn_error_t
    """
    return apply(_repos.svn_repos_parse_fns2_invoke_set_node_property, args)

def svn_repos_parse_fns2_invoke_delete_node_property(*args):
    """svn_repos_parse_fns2_invoke_delete_node_property(svn_repos_parse_fns2_t _obj, void node_baton, char name) -> svn_error_t"""
    return apply(_repos.svn_repos_parse_fns2_invoke_delete_node_property, args)

def svn_repos_parse_fns2_invoke_remove_node_props(*args):
    """svn_repos_parse_fns2_invoke_remove_node_props(svn_repos_parse_fns2_t _obj, void node_baton) -> svn_error_t"""
    return apply(_repos.svn_repos_parse_fns2_invoke_remove_node_props, args)

def svn_repos_parse_fns2_invoke_set_fulltext(*args):
    """svn_repos_parse_fns2_invoke_set_fulltext(svn_repos_parse_fns2_t _obj, svn_stream_t stream, void node_baton) -> svn_error_t"""
    return apply(_repos.svn_repos_parse_fns2_invoke_set_fulltext, args)

def svn_repos_parse_fns2_invoke_apply_textdelta(*args):
    """
    svn_repos_parse_fns2_invoke_apply_textdelta(svn_repos_parse_fns2_t _obj, svn_txdelta_window_handler_t handler, 
        void handler_baton, void node_baton) -> svn_error_t
    """
    return apply(_repos.svn_repos_parse_fns2_invoke_apply_textdelta, args)

def svn_repos_parse_fns2_invoke_close_node(*args):
    """svn_repos_parse_fns2_invoke_close_node(svn_repos_parse_fns2_t _obj, void node_baton) -> svn_error_t"""
    return apply(_repos.svn_repos_parse_fns2_invoke_close_node, args)

def svn_repos_parse_fns2_invoke_close_revision(*args):
    """svn_repos_parse_fns2_invoke_close_revision(svn_repos_parse_fns2_t _obj, void revision_baton) -> svn_error_t"""
    return apply(_repos.svn_repos_parse_fns2_invoke_close_revision, args)

