# This file was created automatically by SWIG.
# Don't modify this file, modify the SWIG interface instead.
# This file is compatible with both classic and new-style classes.

import _wc

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
import ra

def svn_wc_version(*args):
    """svn_wc_version() -> svn_version_t"""
    return apply(_wc.svn_wc_version, args)
SVN_WC_TRANSLATE_FROM_NF = _wc.SVN_WC_TRANSLATE_FROM_NF
SVN_WC_TRANSLATE_TO_NF = _wc.SVN_WC_TRANSLATE_TO_NF
SVN_WC_TRANSLATE_FORCE_EOL_REPAIR = _wc.SVN_WC_TRANSLATE_FORCE_EOL_REPAIR
SVN_WC_TRANSLATE_NO_OUTPUT_CLEANUP = _wc.SVN_WC_TRANSLATE_NO_OUTPUT_CLEANUP
SVN_WC_TRANSLATE_FORCE_COPY = _wc.SVN_WC_TRANSLATE_FORCE_COPY
SVN_WC_TRANSLATE_USE_GLOBAL_TMP = _wc.SVN_WC_TRANSLATE_USE_GLOBAL_TMP

def svn_wc_adm_open3(*args):
    """
    svn_wc_adm_open3(svn_wc_adm_access_t adm_access, svn_wc_adm_access_t associated, 
        char path, svn_boolean_t write_lock, 
        int depth, svn_cancel_func_t cancel_func, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_adm_open3, args)

def svn_wc_adm_open2(*args):
    """
    svn_wc_adm_open2(svn_wc_adm_access_t adm_access, svn_wc_adm_access_t associated, 
        char path, svn_boolean_t write_lock, 
        int depth, apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_adm_open2, args)

def svn_wc_adm_open(*args):
    """
    svn_wc_adm_open(svn_wc_adm_access_t adm_access, svn_wc_adm_access_t associated, 
        char path, svn_boolean_t write_lock, 
        svn_boolean_t tree_lock, apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_adm_open, args)

def svn_wc_adm_probe_open3(*args):
    """
    svn_wc_adm_probe_open3(svn_wc_adm_access_t adm_access, svn_wc_adm_access_t associated, 
        char path, svn_boolean_t write_lock, 
        int depth, svn_cancel_func_t cancel_func, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_adm_probe_open3, args)

def svn_wc_adm_probe_open2(*args):
    """
    svn_wc_adm_probe_open2(svn_wc_adm_access_t adm_access, svn_wc_adm_access_t associated, 
        char path, svn_boolean_t write_lock, 
        int depth, apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_adm_probe_open2, args)

def svn_wc_adm_probe_open(*args):
    """
    svn_wc_adm_probe_open(svn_wc_adm_access_t adm_access, svn_wc_adm_access_t associated, 
        char path, svn_boolean_t write_lock, 
        svn_boolean_t tree_lock, apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_adm_probe_open, args)

def svn_wc_adm_open_anchor(*args):
    """
    svn_wc_adm_open_anchor(svn_wc_adm_access_t anchor_access, svn_wc_adm_access_t target_access, 
        char target, char path, svn_boolean_t write_lock, 
        int depth, svn_cancel_func_t cancel_func, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_adm_open_anchor, args)

def svn_wc_adm_retrieve(*args):
    """
    svn_wc_adm_retrieve(svn_wc_adm_access_t adm_access, svn_wc_adm_access_t associated, 
        char path, apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_adm_retrieve, args)

def svn_wc_adm_probe_retrieve(*args):
    """
    svn_wc_adm_probe_retrieve(svn_wc_adm_access_t adm_access, svn_wc_adm_access_t associated, 
        char path, apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_adm_probe_retrieve, args)

def svn_wc_adm_probe_try3(*args):
    """
    svn_wc_adm_probe_try3(svn_wc_adm_access_t adm_access, svn_wc_adm_access_t associated, 
        char path, svn_boolean_t write_lock, 
        int depth, svn_cancel_func_t cancel_func, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_adm_probe_try3, args)

def svn_wc_adm_probe_try2(*args):
    """
    svn_wc_adm_probe_try2(svn_wc_adm_access_t adm_access, svn_wc_adm_access_t associated, 
        char path, svn_boolean_t write_lock, 
        int depth, apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_adm_probe_try2, args)

def svn_wc_adm_probe_try(*args):
    """
    svn_wc_adm_probe_try(svn_wc_adm_access_t adm_access, svn_wc_adm_access_t associated, 
        char path, svn_boolean_t write_lock, 
        svn_boolean_t tree_lock, apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_adm_probe_try, args)

def svn_wc_adm_close(*args):
    """svn_wc_adm_close(svn_wc_adm_access_t adm_access) -> svn_error_t"""
    return apply(_wc.svn_wc_adm_close, args)

def svn_wc_adm_access_path(*args):
    """svn_wc_adm_access_path(svn_wc_adm_access_t adm_access) -> char"""
    return apply(_wc.svn_wc_adm_access_path, args)

def svn_wc_adm_access_pool(*args):
    """svn_wc_adm_access_pool(svn_wc_adm_access_t adm_access) -> apr_pool_t"""
    return apply(_wc.svn_wc_adm_access_pool, args)

def svn_wc_adm_locked(*args):
    """svn_wc_adm_locked(svn_wc_adm_access_t adm_access) -> svn_boolean_t"""
    return apply(_wc.svn_wc_adm_locked, args)

def svn_wc_locked(*args):
    """svn_wc_locked(svn_boolean_t locked, char path, apr_pool_t pool) -> svn_error_t"""
    return apply(_wc.svn_wc_locked, args)

def svn_wc_is_adm_dir(*args):
    """svn_wc_is_adm_dir(char name, apr_pool_t pool) -> svn_boolean_t"""
    return apply(_wc.svn_wc_is_adm_dir, args)

def svn_wc_get_adm_dir(*args):
    """svn_wc_get_adm_dir(apr_pool_t pool) -> char"""
    return apply(_wc.svn_wc_get_adm_dir, args)

def svn_wc_set_adm_dir(*args):
    """svn_wc_set_adm_dir(char name, apr_pool_t pool) -> svn_error_t"""
    return apply(_wc.svn_wc_set_adm_dir, args)

def svn_wc_init_traversal_info(*args):
    """svn_wc_init_traversal_info(apr_pool_t pool) -> svn_wc_traversal_info_t"""
    return apply(_wc.svn_wc_init_traversal_info, args)

def svn_wc_edited_externals(*args):
    """
    svn_wc_edited_externals(apr_hash_t externals_old, apr_hash_t externals_new, 
        svn_wc_traversal_info_t traversal_info)
    """
    return apply(_wc.svn_wc_edited_externals, args)
class svn_wc_external_item_t:
    """Proxy of C svn_wc_external_item_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, svn_wc_external_item_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, svn_wc_external_item_t, name)
    def __repr__(self):
        return "<%s.%s; proxy of C svn_wc_external_item_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    __swig_setmethods__["target_dir"] = _wc.svn_wc_external_item_t_target_dir_set
    __swig_getmethods__["target_dir"] = _wc.svn_wc_external_item_t_target_dir_get
    __swig_setmethods__["url"] = _wc.svn_wc_external_item_t_url_set
    __swig_getmethods__["url"] = _wc.svn_wc_external_item_t_url_get
    __swig_setmethods__["revision"] = _wc.svn_wc_external_item_t_revision_set
    __swig_getmethods__["revision"] = _wc.svn_wc_external_item_t_revision_get
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for svn_wc_external_item_t"""
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

    def __init__(self, *args):
        """__init__(self) -> svn_wc_external_item_t"""
        _swig_setattr(self, svn_wc_external_item_t, 'this', apply(_wc.new_svn_wc_external_item_t, args))
        _swig_setattr(self, svn_wc_external_item_t, 'thisown', 1)
    def __del__(self, destroy=_wc.delete_svn_wc_external_item_t):
        """__del__(self)"""
        try:
            if self.thisown: destroy(self)
        except: pass


class svn_wc_external_item_tPtr(svn_wc_external_item_t):
    def __init__(self, this):
        _swig_setattr(self, svn_wc_external_item_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, svn_wc_external_item_t, 'thisown', 0)
        _swig_setattr(self, svn_wc_external_item_t,self.__class__,svn_wc_external_item_t)
_wc.svn_wc_external_item_t_swigregister(svn_wc_external_item_tPtr)


def svn_wc_external_item_dup(*args):
    """svn_wc_external_item_dup(svn_wc_external_item_t item, apr_pool_t pool) -> svn_wc_external_item_t"""
    return apply(_wc.svn_wc_external_item_dup, args)

def svn_wc_parse_externals_description2(*args):
    """
    svn_wc_parse_externals_description2(apr_array_header_t externals_p, char parent_directory, 
        char desc, apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_parse_externals_description2, args)

def svn_wc_parse_externals_description(*args):
    """
    svn_wc_parse_externals_description(apr_hash_t externals_p, char parent_directory, char desc, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_parse_externals_description, args)
svn_wc_notify_add = _wc.svn_wc_notify_add
svn_wc_notify_copy = _wc.svn_wc_notify_copy
svn_wc_notify_delete = _wc.svn_wc_notify_delete
svn_wc_notify_restore = _wc.svn_wc_notify_restore
svn_wc_notify_revert = _wc.svn_wc_notify_revert
svn_wc_notify_failed_revert = _wc.svn_wc_notify_failed_revert
svn_wc_notify_resolved = _wc.svn_wc_notify_resolved
svn_wc_notify_skip = _wc.svn_wc_notify_skip
svn_wc_notify_update_delete = _wc.svn_wc_notify_update_delete
svn_wc_notify_update_add = _wc.svn_wc_notify_update_add
svn_wc_notify_update_update = _wc.svn_wc_notify_update_update
svn_wc_notify_update_completed = _wc.svn_wc_notify_update_completed
svn_wc_notify_update_external = _wc.svn_wc_notify_update_external
svn_wc_notify_status_completed = _wc.svn_wc_notify_status_completed
svn_wc_notify_status_external = _wc.svn_wc_notify_status_external
svn_wc_notify_commit_modified = _wc.svn_wc_notify_commit_modified
svn_wc_notify_commit_added = _wc.svn_wc_notify_commit_added
svn_wc_notify_commit_deleted = _wc.svn_wc_notify_commit_deleted
svn_wc_notify_commit_replaced = _wc.svn_wc_notify_commit_replaced
svn_wc_notify_commit_postfix_txdelta = _wc.svn_wc_notify_commit_postfix_txdelta
svn_wc_notify_blame_revision = _wc.svn_wc_notify_blame_revision
svn_wc_notify_locked = _wc.svn_wc_notify_locked
svn_wc_notify_unlocked = _wc.svn_wc_notify_unlocked
svn_wc_notify_failed_lock = _wc.svn_wc_notify_failed_lock
svn_wc_notify_failed_unlock = _wc.svn_wc_notify_failed_unlock
svn_wc_notify_state_inapplicable = _wc.svn_wc_notify_state_inapplicable
svn_wc_notify_state_unknown = _wc.svn_wc_notify_state_unknown
svn_wc_notify_state_unchanged = _wc.svn_wc_notify_state_unchanged
svn_wc_notify_state_missing = _wc.svn_wc_notify_state_missing
svn_wc_notify_state_obstructed = _wc.svn_wc_notify_state_obstructed
svn_wc_notify_state_changed = _wc.svn_wc_notify_state_changed
svn_wc_notify_state_merged = _wc.svn_wc_notify_state_merged
svn_wc_notify_state_conflicted = _wc.svn_wc_notify_state_conflicted
svn_wc_notify_lock_state_inapplicable = _wc.svn_wc_notify_lock_state_inapplicable
svn_wc_notify_lock_state_unknown = _wc.svn_wc_notify_lock_state_unknown
svn_wc_notify_lock_state_unchanged = _wc.svn_wc_notify_lock_state_unchanged
svn_wc_notify_lock_state_locked = _wc.svn_wc_notify_lock_state_locked
svn_wc_notify_lock_state_unlocked = _wc.svn_wc_notify_lock_state_unlocked
class svn_wc_notify_t:
    """Proxy of C svn_wc_notify_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, svn_wc_notify_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, svn_wc_notify_t, name)
    def __repr__(self):
        return "<%s.%s; proxy of C svn_wc_notify_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    __swig_setmethods__["path"] = _wc.svn_wc_notify_t_path_set
    __swig_getmethods__["path"] = _wc.svn_wc_notify_t_path_get
    __swig_setmethods__["action"] = _wc.svn_wc_notify_t_action_set
    __swig_getmethods__["action"] = _wc.svn_wc_notify_t_action_get
    __swig_setmethods__["kind"] = _wc.svn_wc_notify_t_kind_set
    __swig_getmethods__["kind"] = _wc.svn_wc_notify_t_kind_get
    __swig_setmethods__["mime_type"] = _wc.svn_wc_notify_t_mime_type_set
    __swig_getmethods__["mime_type"] = _wc.svn_wc_notify_t_mime_type_get
    __swig_setmethods__["lock"] = _wc.svn_wc_notify_t_lock_set
    __swig_getmethods__["lock"] = _wc.svn_wc_notify_t_lock_get
    __swig_setmethods__["err"] = _wc.svn_wc_notify_t_err_set
    __swig_getmethods__["err"] = _wc.svn_wc_notify_t_err_get
    __swig_setmethods__["content_state"] = _wc.svn_wc_notify_t_content_state_set
    __swig_getmethods__["content_state"] = _wc.svn_wc_notify_t_content_state_get
    __swig_setmethods__["prop_state"] = _wc.svn_wc_notify_t_prop_state_set
    __swig_getmethods__["prop_state"] = _wc.svn_wc_notify_t_prop_state_get
    __swig_setmethods__["lock_state"] = _wc.svn_wc_notify_t_lock_state_set
    __swig_getmethods__["lock_state"] = _wc.svn_wc_notify_t_lock_state_get
    __swig_setmethods__["revision"] = _wc.svn_wc_notify_t_revision_set
    __swig_getmethods__["revision"] = _wc.svn_wc_notify_t_revision_get
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for svn_wc_notify_t"""
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

    def __init__(self, *args):
        """__init__(self) -> svn_wc_notify_t"""
        _swig_setattr(self, svn_wc_notify_t, 'this', apply(_wc.new_svn_wc_notify_t, args))
        _swig_setattr(self, svn_wc_notify_t, 'thisown', 1)
    def __del__(self, destroy=_wc.delete_svn_wc_notify_t):
        """__del__(self)"""
        try:
            if self.thisown: destroy(self)
        except: pass


class svn_wc_notify_tPtr(svn_wc_notify_t):
    def __init__(self, this):
        _swig_setattr(self, svn_wc_notify_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, svn_wc_notify_t, 'thisown', 0)
        _swig_setattr(self, svn_wc_notify_t,self.__class__,svn_wc_notify_t)
_wc.svn_wc_notify_t_swigregister(svn_wc_notify_tPtr)


def svn_wc_create_notify(*args):
    """svn_wc_create_notify(char path, svn_wc_notify_action_t action, apr_pool_t pool) -> svn_wc_notify_t"""
    return apply(_wc.svn_wc_create_notify, args)

def svn_wc_dup_notify(*args):
    """svn_wc_dup_notify(svn_wc_notify_t notify, apr_pool_t pool) -> svn_wc_notify_t"""
    return apply(_wc.svn_wc_dup_notify, args)
class svn_wc_diff_callbacks2_t:
    """Proxy of C svn_wc_diff_callbacks2_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, svn_wc_diff_callbacks2_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, svn_wc_diff_callbacks2_t, name)
    def __repr__(self):
        return "<%s.%s; proxy of C svn_wc_diff_callbacks2_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    __swig_setmethods__["file_changed"] = _wc.svn_wc_diff_callbacks2_t_file_changed_set
    __swig_getmethods__["file_changed"] = _wc.svn_wc_diff_callbacks2_t_file_changed_get
    __swig_setmethods__["file_added"] = _wc.svn_wc_diff_callbacks2_t_file_added_set
    __swig_getmethods__["file_added"] = _wc.svn_wc_diff_callbacks2_t_file_added_get
    __swig_setmethods__["file_deleted"] = _wc.svn_wc_diff_callbacks2_t_file_deleted_set
    __swig_getmethods__["file_deleted"] = _wc.svn_wc_diff_callbacks2_t_file_deleted_get
    __swig_setmethods__["dir_added"] = _wc.svn_wc_diff_callbacks2_t_dir_added_set
    __swig_getmethods__["dir_added"] = _wc.svn_wc_diff_callbacks2_t_dir_added_get
    __swig_setmethods__["dir_deleted"] = _wc.svn_wc_diff_callbacks2_t_dir_deleted_set
    __swig_getmethods__["dir_deleted"] = _wc.svn_wc_diff_callbacks2_t_dir_deleted_get
    __swig_setmethods__["dir_props_changed"] = _wc.svn_wc_diff_callbacks2_t_dir_props_changed_set
    __swig_getmethods__["dir_props_changed"] = _wc.svn_wc_diff_callbacks2_t_dir_props_changed_get
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for svn_wc_diff_callbacks2_t"""
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

    def __init__(self, *args):
        """__init__(self) -> svn_wc_diff_callbacks2_t"""
        _swig_setattr(self, svn_wc_diff_callbacks2_t, 'this', apply(_wc.new_svn_wc_diff_callbacks2_t, args))
        _swig_setattr(self, svn_wc_diff_callbacks2_t, 'thisown', 1)
    def __del__(self, destroy=_wc.delete_svn_wc_diff_callbacks2_t):
        """__del__(self)"""
        try:
            if self.thisown: destroy(self)
        except: pass


class svn_wc_diff_callbacks2_tPtr(svn_wc_diff_callbacks2_t):
    def __init__(self, this):
        _swig_setattr(self, svn_wc_diff_callbacks2_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, svn_wc_diff_callbacks2_t, 'thisown', 0)
        _swig_setattr(self, svn_wc_diff_callbacks2_t,self.__class__,svn_wc_diff_callbacks2_t)
_wc.svn_wc_diff_callbacks2_t_swigregister(svn_wc_diff_callbacks2_tPtr)

class svn_wc_diff_callbacks_t:
    """Proxy of C svn_wc_diff_callbacks_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, svn_wc_diff_callbacks_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, svn_wc_diff_callbacks_t, name)
    def __repr__(self):
        return "<%s.%s; proxy of C svn_wc_diff_callbacks_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    __swig_setmethods__["file_changed"] = _wc.svn_wc_diff_callbacks_t_file_changed_set
    __swig_getmethods__["file_changed"] = _wc.svn_wc_diff_callbacks_t_file_changed_get
    __swig_setmethods__["file_added"] = _wc.svn_wc_diff_callbacks_t_file_added_set
    __swig_getmethods__["file_added"] = _wc.svn_wc_diff_callbacks_t_file_added_get
    __swig_setmethods__["file_deleted"] = _wc.svn_wc_diff_callbacks_t_file_deleted_set
    __swig_getmethods__["file_deleted"] = _wc.svn_wc_diff_callbacks_t_file_deleted_get
    __swig_setmethods__["dir_added"] = _wc.svn_wc_diff_callbacks_t_dir_added_set
    __swig_getmethods__["dir_added"] = _wc.svn_wc_diff_callbacks_t_dir_added_get
    __swig_setmethods__["dir_deleted"] = _wc.svn_wc_diff_callbacks_t_dir_deleted_set
    __swig_getmethods__["dir_deleted"] = _wc.svn_wc_diff_callbacks_t_dir_deleted_get
    __swig_setmethods__["props_changed"] = _wc.svn_wc_diff_callbacks_t_props_changed_set
    __swig_getmethods__["props_changed"] = _wc.svn_wc_diff_callbacks_t_props_changed_get
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for svn_wc_diff_callbacks_t"""
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

    def __init__(self, *args):
        """__init__(self) -> svn_wc_diff_callbacks_t"""
        _swig_setattr(self, svn_wc_diff_callbacks_t, 'this', apply(_wc.new_svn_wc_diff_callbacks_t, args))
        _swig_setattr(self, svn_wc_diff_callbacks_t, 'thisown', 1)
    def __del__(self, destroy=_wc.delete_svn_wc_diff_callbacks_t):
        """__del__(self)"""
        try:
            if self.thisown: destroy(self)
        except: pass


class svn_wc_diff_callbacks_tPtr(svn_wc_diff_callbacks_t):
    def __init__(self, this):
        _swig_setattr(self, svn_wc_diff_callbacks_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, svn_wc_diff_callbacks_t, 'thisown', 0)
        _swig_setattr(self, svn_wc_diff_callbacks_t,self.__class__,svn_wc_diff_callbacks_t)
_wc.svn_wc_diff_callbacks_t_swigregister(svn_wc_diff_callbacks_tPtr)


def svn_wc_check_wc(*args):
    """svn_wc_check_wc(char path, int wc_format, apr_pool_t pool) -> svn_error_t"""
    return apply(_wc.svn_wc_check_wc, args)

def svn_wc_has_binary_prop(*args):
    """
    svn_wc_has_binary_prop(svn_boolean_t has_binary_prop, char path, svn_wc_adm_access_t adm_access, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_has_binary_prop, args)

def svn_wc_text_modified_p(*args):
    """
    svn_wc_text_modified_p(svn_boolean_t modified_p, char filename, svn_boolean_t force_comparison, 
        svn_wc_adm_access_t adm_access, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_text_modified_p, args)

def svn_wc_props_modified_p(*args):
    """
    svn_wc_props_modified_p(svn_boolean_t modified_p, char path, svn_wc_adm_access_t adm_access, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_props_modified_p, args)
SVN_WC_ADM_DIR_NAME = _wc.SVN_WC_ADM_DIR_NAME
svn_wc_schedule_normal = _wc.svn_wc_schedule_normal
svn_wc_schedule_add = _wc.svn_wc_schedule_add
svn_wc_schedule_delete = _wc.svn_wc_schedule_delete
svn_wc_schedule_replace = _wc.svn_wc_schedule_replace
class svn_wc_entry_t:
    """Proxy of C svn_wc_entry_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, svn_wc_entry_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, svn_wc_entry_t, name)
    def __repr__(self):
        return "<%s.%s; proxy of C svn_wc_entry_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    __swig_setmethods__["name"] = _wc.svn_wc_entry_t_name_set
    __swig_getmethods__["name"] = _wc.svn_wc_entry_t_name_get
    __swig_setmethods__["revision"] = _wc.svn_wc_entry_t_revision_set
    __swig_getmethods__["revision"] = _wc.svn_wc_entry_t_revision_get
    __swig_setmethods__["url"] = _wc.svn_wc_entry_t_url_set
    __swig_getmethods__["url"] = _wc.svn_wc_entry_t_url_get
    __swig_setmethods__["repos"] = _wc.svn_wc_entry_t_repos_set
    __swig_getmethods__["repos"] = _wc.svn_wc_entry_t_repos_get
    __swig_setmethods__["uuid"] = _wc.svn_wc_entry_t_uuid_set
    __swig_getmethods__["uuid"] = _wc.svn_wc_entry_t_uuid_get
    __swig_setmethods__["kind"] = _wc.svn_wc_entry_t_kind_set
    __swig_getmethods__["kind"] = _wc.svn_wc_entry_t_kind_get
    __swig_setmethods__["schedule"] = _wc.svn_wc_entry_t_schedule_set
    __swig_getmethods__["schedule"] = _wc.svn_wc_entry_t_schedule_get
    __swig_setmethods__["copied"] = _wc.svn_wc_entry_t_copied_set
    __swig_getmethods__["copied"] = _wc.svn_wc_entry_t_copied_get
    __swig_setmethods__["deleted"] = _wc.svn_wc_entry_t_deleted_set
    __swig_getmethods__["deleted"] = _wc.svn_wc_entry_t_deleted_get
    __swig_setmethods__["absent"] = _wc.svn_wc_entry_t_absent_set
    __swig_getmethods__["absent"] = _wc.svn_wc_entry_t_absent_get
    __swig_setmethods__["incomplete"] = _wc.svn_wc_entry_t_incomplete_set
    __swig_getmethods__["incomplete"] = _wc.svn_wc_entry_t_incomplete_get
    __swig_setmethods__["copyfrom_url"] = _wc.svn_wc_entry_t_copyfrom_url_set
    __swig_getmethods__["copyfrom_url"] = _wc.svn_wc_entry_t_copyfrom_url_get
    __swig_setmethods__["copyfrom_rev"] = _wc.svn_wc_entry_t_copyfrom_rev_set
    __swig_getmethods__["copyfrom_rev"] = _wc.svn_wc_entry_t_copyfrom_rev_get
    __swig_setmethods__["conflict_old"] = _wc.svn_wc_entry_t_conflict_old_set
    __swig_getmethods__["conflict_old"] = _wc.svn_wc_entry_t_conflict_old_get
    __swig_setmethods__["conflict_new"] = _wc.svn_wc_entry_t_conflict_new_set
    __swig_getmethods__["conflict_new"] = _wc.svn_wc_entry_t_conflict_new_get
    __swig_setmethods__["conflict_wrk"] = _wc.svn_wc_entry_t_conflict_wrk_set
    __swig_getmethods__["conflict_wrk"] = _wc.svn_wc_entry_t_conflict_wrk_get
    __swig_setmethods__["prejfile"] = _wc.svn_wc_entry_t_prejfile_set
    __swig_getmethods__["prejfile"] = _wc.svn_wc_entry_t_prejfile_get
    __swig_setmethods__["text_time"] = _wc.svn_wc_entry_t_text_time_set
    __swig_getmethods__["text_time"] = _wc.svn_wc_entry_t_text_time_get
    __swig_setmethods__["prop_time"] = _wc.svn_wc_entry_t_prop_time_set
    __swig_getmethods__["prop_time"] = _wc.svn_wc_entry_t_prop_time_get
    __swig_setmethods__["checksum"] = _wc.svn_wc_entry_t_checksum_set
    __swig_getmethods__["checksum"] = _wc.svn_wc_entry_t_checksum_get
    __swig_setmethods__["cmt_rev"] = _wc.svn_wc_entry_t_cmt_rev_set
    __swig_getmethods__["cmt_rev"] = _wc.svn_wc_entry_t_cmt_rev_get
    __swig_setmethods__["cmt_date"] = _wc.svn_wc_entry_t_cmt_date_set
    __swig_getmethods__["cmt_date"] = _wc.svn_wc_entry_t_cmt_date_get
    __swig_setmethods__["cmt_author"] = _wc.svn_wc_entry_t_cmt_author_set
    __swig_getmethods__["cmt_author"] = _wc.svn_wc_entry_t_cmt_author_get
    __swig_setmethods__["lock_token"] = _wc.svn_wc_entry_t_lock_token_set
    __swig_getmethods__["lock_token"] = _wc.svn_wc_entry_t_lock_token_get
    __swig_setmethods__["lock_owner"] = _wc.svn_wc_entry_t_lock_owner_set
    __swig_getmethods__["lock_owner"] = _wc.svn_wc_entry_t_lock_owner_get
    __swig_setmethods__["lock_comment"] = _wc.svn_wc_entry_t_lock_comment_set
    __swig_getmethods__["lock_comment"] = _wc.svn_wc_entry_t_lock_comment_get
    __swig_setmethods__["lock_creation_date"] = _wc.svn_wc_entry_t_lock_creation_date_set
    __swig_getmethods__["lock_creation_date"] = _wc.svn_wc_entry_t_lock_creation_date_get
    __swig_setmethods__["has_props"] = _wc.svn_wc_entry_t_has_props_set
    __swig_getmethods__["has_props"] = _wc.svn_wc_entry_t_has_props_get
    __swig_setmethods__["has_prop_mods"] = _wc.svn_wc_entry_t_has_prop_mods_set
    __swig_getmethods__["has_prop_mods"] = _wc.svn_wc_entry_t_has_prop_mods_get
    __swig_setmethods__["cachable_props"] = _wc.svn_wc_entry_t_cachable_props_set
    __swig_getmethods__["cachable_props"] = _wc.svn_wc_entry_t_cachable_props_get
    __swig_setmethods__["present_props"] = _wc.svn_wc_entry_t_present_props_set
    __swig_getmethods__["present_props"] = _wc.svn_wc_entry_t_present_props_get
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for svn_wc_entry_t"""
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

    def __init__(self, *args):
        """__init__(self) -> svn_wc_entry_t"""
        _swig_setattr(self, svn_wc_entry_t, 'this', apply(_wc.new_svn_wc_entry_t, args))
        _swig_setattr(self, svn_wc_entry_t, 'thisown', 1)
    def __del__(self, destroy=_wc.delete_svn_wc_entry_t):
        """__del__(self)"""
        try:
            if self.thisown: destroy(self)
        except: pass


class svn_wc_entry_tPtr(svn_wc_entry_t):
    def __init__(self, this):
        _swig_setattr(self, svn_wc_entry_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, svn_wc_entry_t, 'thisown', 0)
        _swig_setattr(self, svn_wc_entry_t,self.__class__,svn_wc_entry_t)
_wc.svn_wc_entry_t_swigregister(svn_wc_entry_tPtr)

SVN_WC_ENTRY_THIS_DIR = _wc.SVN_WC_ENTRY_THIS_DIR

def svn_wc_entry(*args):
    """
    svn_wc_entry(svn_wc_entry_t entry, char path, svn_wc_adm_access_t adm_access, 
        svn_boolean_t show_hidden, apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_entry, args)

def svn_wc_entries_read(*args):
    """
    svn_wc_entries_read(apr_hash_t entries, svn_wc_adm_access_t adm_access, 
        svn_boolean_t show_hidden, apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_entries_read, args)

def svn_wc_entry_dup(*args):
    """svn_wc_entry_dup(svn_wc_entry_t entry, apr_pool_t pool) -> svn_wc_entry_t"""
    return apply(_wc.svn_wc_entry_dup, args)

def svn_wc_conflicted_p(*args):
    """
    svn_wc_conflicted_p(svn_boolean_t text_conflicted_p, svn_boolean_t prop_conflicted_p, 
        char dir_path, svn_wc_entry_t entry, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_conflicted_p, args)

def svn_wc_get_ancestry(*args):
    """
    svn_wc_get_ancestry(char url, svn_revnum_t rev, char path, svn_wc_adm_access_t adm_access, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_get_ancestry, args)
class svn_wc_entry_callbacks_t:
    """Proxy of C svn_wc_entry_callbacks_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, svn_wc_entry_callbacks_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, svn_wc_entry_callbacks_t, name)
    def __repr__(self):
        return "<%s.%s; proxy of C svn_wc_entry_callbacks_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    __swig_setmethods__["found_entry"] = _wc.svn_wc_entry_callbacks_t_found_entry_set
    __swig_getmethods__["found_entry"] = _wc.svn_wc_entry_callbacks_t_found_entry_get
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for svn_wc_entry_callbacks_t"""
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

    def __init__(self, *args):
        """__init__(self) -> svn_wc_entry_callbacks_t"""
        _swig_setattr(self, svn_wc_entry_callbacks_t, 'this', apply(_wc.new_svn_wc_entry_callbacks_t, args))
        _swig_setattr(self, svn_wc_entry_callbacks_t, 'thisown', 1)
    def __del__(self, destroy=_wc.delete_svn_wc_entry_callbacks_t):
        """__del__(self)"""
        try:
            if self.thisown: destroy(self)
        except: pass


class svn_wc_entry_callbacks_tPtr(svn_wc_entry_callbacks_t):
    def __init__(self, this):
        _swig_setattr(self, svn_wc_entry_callbacks_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, svn_wc_entry_callbacks_t, 'thisown', 0)
        _swig_setattr(self, svn_wc_entry_callbacks_t,self.__class__,svn_wc_entry_callbacks_t)
_wc.svn_wc_entry_callbacks_t_swigregister(svn_wc_entry_callbacks_tPtr)


def svn_wc_walk_entries2(*args):
    """
    svn_wc_walk_entries2(char path, svn_wc_adm_access_t adm_access, svn_wc_entry_callbacks_t walk_callbacks, 
        void walk_baton, 
        svn_boolean_t show_hidden, svn_cancel_func_t cancel_func, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_walk_entries2, args)

def svn_wc_walk_entries(*args):
    """
    svn_wc_walk_entries(char path, svn_wc_adm_access_t adm_access, svn_wc_entry_callbacks_t walk_callbacks, 
        void walk_baton, 
        svn_boolean_t show_hidden, apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_walk_entries, args)

def svn_wc_mark_missing_deleted(*args):
    """svn_wc_mark_missing_deleted(char path, svn_wc_adm_access_t parent, apr_pool_t pool) -> svn_error_t"""
    return apply(_wc.svn_wc_mark_missing_deleted, args)

def svn_wc_ensure_adm2(*args):
    """
    svn_wc_ensure_adm2(char path, char uuid, char url, char repos, svn_revnum_t revision, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_ensure_adm2, args)

def svn_wc_ensure_adm(*args):
    """
    svn_wc_ensure_adm(char path, char uuid, char url, svn_revnum_t revision, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_ensure_adm, args)

def svn_wc_maybe_set_repos_root(*args):
    """
    svn_wc_maybe_set_repos_root(svn_wc_adm_access_t adm_access, char path, char repos, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_maybe_set_repos_root, args)
svn_wc_status_none = _wc.svn_wc_status_none
svn_wc_status_unversioned = _wc.svn_wc_status_unversioned
svn_wc_status_normal = _wc.svn_wc_status_normal
svn_wc_status_added = _wc.svn_wc_status_added
svn_wc_status_missing = _wc.svn_wc_status_missing
svn_wc_status_deleted = _wc.svn_wc_status_deleted
svn_wc_status_replaced = _wc.svn_wc_status_replaced
svn_wc_status_modified = _wc.svn_wc_status_modified
svn_wc_status_merged = _wc.svn_wc_status_merged
svn_wc_status_conflicted = _wc.svn_wc_status_conflicted
svn_wc_status_ignored = _wc.svn_wc_status_ignored
svn_wc_status_obstructed = _wc.svn_wc_status_obstructed
svn_wc_status_external = _wc.svn_wc_status_external
svn_wc_status_incomplete = _wc.svn_wc_status_incomplete
class svn_wc_status2_t:
    """Proxy of C svn_wc_status2_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, svn_wc_status2_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, svn_wc_status2_t, name)
    def __repr__(self):
        return "<%s.%s; proxy of C svn_wc_status2_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    __swig_setmethods__["entry"] = _wc.svn_wc_status2_t_entry_set
    __swig_getmethods__["entry"] = _wc.svn_wc_status2_t_entry_get
    __swig_setmethods__["text_status"] = _wc.svn_wc_status2_t_text_status_set
    __swig_getmethods__["text_status"] = _wc.svn_wc_status2_t_text_status_get
    __swig_setmethods__["prop_status"] = _wc.svn_wc_status2_t_prop_status_set
    __swig_getmethods__["prop_status"] = _wc.svn_wc_status2_t_prop_status_get
    __swig_setmethods__["locked"] = _wc.svn_wc_status2_t_locked_set
    __swig_getmethods__["locked"] = _wc.svn_wc_status2_t_locked_get
    __swig_setmethods__["copied"] = _wc.svn_wc_status2_t_copied_set
    __swig_getmethods__["copied"] = _wc.svn_wc_status2_t_copied_get
    __swig_setmethods__["switched"] = _wc.svn_wc_status2_t_switched_set
    __swig_getmethods__["switched"] = _wc.svn_wc_status2_t_switched_get
    __swig_setmethods__["repos_text_status"] = _wc.svn_wc_status2_t_repos_text_status_set
    __swig_getmethods__["repos_text_status"] = _wc.svn_wc_status2_t_repos_text_status_get
    __swig_setmethods__["repos_prop_status"] = _wc.svn_wc_status2_t_repos_prop_status_set
    __swig_getmethods__["repos_prop_status"] = _wc.svn_wc_status2_t_repos_prop_status_get
    __swig_setmethods__["repos_lock"] = _wc.svn_wc_status2_t_repos_lock_set
    __swig_getmethods__["repos_lock"] = _wc.svn_wc_status2_t_repos_lock_get
    __swig_setmethods__["url"] = _wc.svn_wc_status2_t_url_set
    __swig_getmethods__["url"] = _wc.svn_wc_status2_t_url_get
    __swig_setmethods__["ood_last_cmt_rev"] = _wc.svn_wc_status2_t_ood_last_cmt_rev_set
    __swig_getmethods__["ood_last_cmt_rev"] = _wc.svn_wc_status2_t_ood_last_cmt_rev_get
    __swig_setmethods__["ood_last_cmt_date"] = _wc.svn_wc_status2_t_ood_last_cmt_date_set
    __swig_getmethods__["ood_last_cmt_date"] = _wc.svn_wc_status2_t_ood_last_cmt_date_get
    __swig_setmethods__["ood_kind"] = _wc.svn_wc_status2_t_ood_kind_set
    __swig_getmethods__["ood_kind"] = _wc.svn_wc_status2_t_ood_kind_get
    __swig_setmethods__["ood_last_cmt_author"] = _wc.svn_wc_status2_t_ood_last_cmt_author_set
    __swig_getmethods__["ood_last_cmt_author"] = _wc.svn_wc_status2_t_ood_last_cmt_author_get
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for svn_wc_status2_t"""
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

    def __init__(self, *args):
        """__init__(self) -> svn_wc_status2_t"""
        _swig_setattr(self, svn_wc_status2_t, 'this', apply(_wc.new_svn_wc_status2_t, args))
        _swig_setattr(self, svn_wc_status2_t, 'thisown', 1)
    def __del__(self, destroy=_wc.delete_svn_wc_status2_t):
        """__del__(self)"""
        try:
            if self.thisown: destroy(self)
        except: pass


class svn_wc_status2_tPtr(svn_wc_status2_t):
    def __init__(self, this):
        _swig_setattr(self, svn_wc_status2_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, svn_wc_status2_t, 'thisown', 0)
        _swig_setattr(self, svn_wc_status2_t,self.__class__,svn_wc_status2_t)
_wc.svn_wc_status2_t_swigregister(svn_wc_status2_tPtr)

class svn_wc_status_t:
    """Proxy of C svn_wc_status_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, svn_wc_status_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, svn_wc_status_t, name)
    def __repr__(self):
        return "<%s.%s; proxy of C svn_wc_status_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    __swig_setmethods__["entry"] = _wc.svn_wc_status_t_entry_set
    __swig_getmethods__["entry"] = _wc.svn_wc_status_t_entry_get
    __swig_setmethods__["text_status"] = _wc.svn_wc_status_t_text_status_set
    __swig_getmethods__["text_status"] = _wc.svn_wc_status_t_text_status_get
    __swig_setmethods__["prop_status"] = _wc.svn_wc_status_t_prop_status_set
    __swig_getmethods__["prop_status"] = _wc.svn_wc_status_t_prop_status_get
    __swig_setmethods__["locked"] = _wc.svn_wc_status_t_locked_set
    __swig_getmethods__["locked"] = _wc.svn_wc_status_t_locked_get
    __swig_setmethods__["copied"] = _wc.svn_wc_status_t_copied_set
    __swig_getmethods__["copied"] = _wc.svn_wc_status_t_copied_get
    __swig_setmethods__["switched"] = _wc.svn_wc_status_t_switched_set
    __swig_getmethods__["switched"] = _wc.svn_wc_status_t_switched_get
    __swig_setmethods__["repos_text_status"] = _wc.svn_wc_status_t_repos_text_status_set
    __swig_getmethods__["repos_text_status"] = _wc.svn_wc_status_t_repos_text_status_get
    __swig_setmethods__["repos_prop_status"] = _wc.svn_wc_status_t_repos_prop_status_set
    __swig_getmethods__["repos_prop_status"] = _wc.svn_wc_status_t_repos_prop_status_get
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for svn_wc_status_t"""
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

    def __init__(self, *args):
        """__init__(self) -> svn_wc_status_t"""
        _swig_setattr(self, svn_wc_status_t, 'this', apply(_wc.new_svn_wc_status_t, args))
        _swig_setattr(self, svn_wc_status_t, 'thisown', 1)
    def __del__(self, destroy=_wc.delete_svn_wc_status_t):
        """__del__(self)"""
        try:
            if self.thisown: destroy(self)
        except: pass


class svn_wc_status_tPtr(svn_wc_status_t):
    def __init__(self, this):
        _swig_setattr(self, svn_wc_status_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, svn_wc_status_t, 'thisown', 0)
        _swig_setattr(self, svn_wc_status_t,self.__class__,svn_wc_status_t)
_wc.svn_wc_status_t_swigregister(svn_wc_status_tPtr)


def svn_wc_dup_status2(*args):
    """svn_wc_dup_status2(svn_wc_status2_t orig_stat, apr_pool_t pool) -> svn_wc_status2_t"""
    return apply(_wc.svn_wc_dup_status2, args)

def svn_wc_dup_status(*args):
    """svn_wc_dup_status(svn_wc_status_t orig_stat, apr_pool_t pool) -> svn_wc_status_t"""
    return apply(_wc.svn_wc_dup_status, args)

def svn_wc_status2(*args):
    """
    svn_wc_status2(svn_wc_status2_t status, char path, svn_wc_adm_access_t adm_access, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_status2, args)

def svn_wc_status(*args):
    """
    svn_wc_status(svn_wc_status_t status, char path, svn_wc_adm_access_t adm_access, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_status, args)

def svn_wc_get_status_editor2(*args):
    """
    svn_wc_get_status_editor2(svn_delta_editor_t editor, void edit_baton, void set_locks_baton, 
        svn_revnum_t edit_revision, svn_wc_adm_access_t anchor, 
        char target, apr_hash_t config, 
        svn_boolean_t recurse, svn_boolean_t get_all, 
        svn_boolean_t no_ignore, svn_wc_status_func2_t status_func, 
        void status_baton, 
        svn_cancel_func_t cancel_func, svn_wc_traversal_info_t traversal_info, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_get_status_editor2, args)

def svn_wc_get_status_editor(*args):
    """
    svn_wc_get_status_editor(svn_delta_editor_t editor, void edit_baton, svn_revnum_t edit_revision, 
        svn_wc_adm_access_t anchor, 
        char target, apr_hash_t config, svn_boolean_t recurse, 
        svn_boolean_t get_all, svn_boolean_t no_ignore, 
        svn_wc_status_func_t status_func, 
        svn_cancel_func_t cancel_func, svn_wc_traversal_info_t traversal_info, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_get_status_editor, args)

def svn_wc_status_set_repos_locks(*args):
    """
    svn_wc_status_set_repos_locks(void set_locks_baton, apr_hash_t locks, char repos_root, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_status_set_repos_locks, args)

def svn_wc_copy2(*args):
    """
    svn_wc_copy2(char src, svn_wc_adm_access_t dst_parent, char dst_basename, 
        svn_cancel_func_t cancel_func, svn_wc_notify_func2_t notify_func, 
        void notify_baton, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_copy2, args)

def svn_wc_copy(*args):
    """
    svn_wc_copy(char src, svn_wc_adm_access_t dst_parent, char dst_basename, 
        svn_cancel_func_t cancel_func, svn_wc_notify_func_t notify_func, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_copy, args)

def svn_wc_delete2(*args):
    """
    svn_wc_delete2(char path, svn_wc_adm_access_t adm_access, svn_cancel_func_t cancel_func, 
        svn_wc_notify_func2_t notify_func, 
        void notify_baton, apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_delete2, args)

def svn_wc_delete(*args):
    """
    svn_wc_delete(char path, svn_wc_adm_access_t adm_access, svn_cancel_func_t cancel_func, 
        svn_wc_notify_func_t notify_func, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_delete, args)

def svn_wc_add2(*args):
    """
    svn_wc_add2(char path, svn_wc_adm_access_t parent_access, char copyfrom_url, 
        svn_revnum_t copyfrom_rev, svn_cancel_func_t cancel_func, 
        svn_wc_notify_func2_t notify_func, 
        void notify_baton, apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_add2, args)

def svn_wc_add(*args):
    """
    svn_wc_add(char path, svn_wc_adm_access_t parent_access, char copyfrom_url, 
        svn_revnum_t copyfrom_rev, svn_cancel_func_t cancel_func, 
        svn_wc_notify_func_t notify_func, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_add, args)

def svn_wc_add_repos_file2(*args):
    """
    svn_wc_add_repos_file2(char dst_path, svn_wc_adm_access_t adm_access, char new_text_base_path, 
        char new_text_path, apr_hash_t new_base_props, 
        apr_hash_t new_props, 
        char copyfrom_url, svn_revnum_t copyfrom_rev, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_add_repos_file2, args)

def svn_wc_add_repos_file(*args):
    """
    svn_wc_add_repos_file(char dst_path, svn_wc_adm_access_t adm_access, char new_text_path, 
        apr_hash_t new_props, char copyfrom_url, 
        svn_revnum_t copyfrom_rev, apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_add_repos_file, args)

def svn_wc_remove_from_revision_control(*args):
    """
    svn_wc_remove_from_revision_control(svn_wc_adm_access_t adm_access, char name, svn_boolean_t destroy_wf, 
        svn_boolean_t instant_error, 
        svn_cancel_func_t cancel_func, apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_remove_from_revision_control, args)

def svn_wc_resolved_conflict2(*args):
    """
    svn_wc_resolved_conflict2(char path, svn_wc_adm_access_t adm_access, svn_boolean_t resolve_text, 
        svn_boolean_t resolve_props, 
        svn_boolean_t recurse, svn_wc_notify_func2_t notify_func, 
        void notify_baton, svn_cancel_func_t cancel_func, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_resolved_conflict2, args)

def svn_wc_resolved_conflict(*args):
    """
    svn_wc_resolved_conflict(char path, svn_wc_adm_access_t adm_access, svn_boolean_t resolve_text, 
        svn_boolean_t resolve_props, 
        svn_boolean_t recurse, svn_wc_notify_func_t notify_func, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_resolved_conflict, args)

def svn_wc_process_committed3(*args):
    """
    svn_wc_process_committed3(char path, svn_wc_adm_access_t adm_access, svn_boolean_t recurse, 
        svn_revnum_t new_revnum, char rev_date, 
        char rev_author, apr_array_header_t wcprop_changes, 
        svn_boolean_t remove_lock, unsigned char digest, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_process_committed3, args)

def svn_wc_process_committed2(*args):
    """
    svn_wc_process_committed2(char path, svn_wc_adm_access_t adm_access, svn_boolean_t recurse, 
        svn_revnum_t new_revnum, char rev_date, 
        char rev_author, apr_array_header_t wcprop_changes, 
        svn_boolean_t remove_lock, apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_process_committed2, args)

def svn_wc_process_committed(*args):
    """
    svn_wc_process_committed(char path, svn_wc_adm_access_t adm_access, svn_boolean_t recurse, 
        svn_revnum_t new_revnum, char rev_date, 
        char rev_author, apr_array_header_t wcprop_changes, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_process_committed, args)

def svn_wc_crawl_revisions2(*args):
    """
    svn_wc_crawl_revisions2(char path, svn_wc_adm_access_t adm_access, svn_ra_reporter2_t reporter, 
        void report_baton, svn_boolean_t restore_files, 
        svn_boolean_t recurse, 
        svn_boolean_t use_commit_times, svn_wc_notify_func2_t notify_func, 
        void notify_baton, svn_wc_traversal_info_t traversal_info, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_crawl_revisions2, args)

def svn_wc_crawl_revisions(*args):
    """
    svn_wc_crawl_revisions(char path, svn_wc_adm_access_t adm_access, svn_ra_reporter_t reporter, 
        void report_baton, svn_boolean_t restore_files, 
        svn_boolean_t recurse, 
        svn_boolean_t use_commit_times, svn_wc_notify_func_t notify_func, 
        svn_wc_traversal_info_t traversal_info, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_crawl_revisions, args)

def svn_wc_is_wc_root(*args):
    """
    svn_wc_is_wc_root(svn_boolean_t wc_root, char path, svn_wc_adm_access_t adm_access, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_is_wc_root, args)

def svn_wc_get_actual_target(*args):
    """svn_wc_get_actual_target(char path, char anchor, char target, apr_pool_t pool) -> svn_error_t"""
    return apply(_wc.svn_wc_get_actual_target, args)

def svn_wc_get_update_editor2(*args):
    """
    svn_wc_get_update_editor2(svn_revnum_t target_revision, svn_wc_adm_access_t anchor, 
        char target, svn_boolean_t use_commit_times, 
        svn_boolean_t recurse, svn_wc_notify_func2_t notify_func, 
        void notify_baton, svn_cancel_func_t cancel_func, 
        char diff3_cmd, svn_delta_editor_t editor, 
        void edit_baton, svn_wc_traversal_info_t ti, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_get_update_editor2, args)

def svn_wc_get_update_editor(*args):
    """
    svn_wc_get_update_editor(svn_revnum_t target_revision, svn_wc_adm_access_t anchor, 
        char target, svn_boolean_t use_commit_times, 
        svn_boolean_t recurse, svn_wc_notify_func_t notify_func, 
        svn_cancel_func_t cancel_func, 
        char diff3_cmd, svn_delta_editor_t editor, 
        void edit_baton, svn_wc_traversal_info_t ti, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_get_update_editor, args)

def svn_wc_get_switch_editor2(*args):
    """
    svn_wc_get_switch_editor2(svn_revnum_t target_revision, svn_wc_adm_access_t anchor, 
        char target, char switch_url, svn_boolean_t use_commit_times, 
        svn_boolean_t recurse, 
        svn_wc_notify_func2_t notify_func, void notify_baton, 
        svn_cancel_func_t cancel_func, char diff3_cmd, 
        svn_delta_editor_t editor, void edit_baton, 
        svn_wc_traversal_info_t ti, apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_get_switch_editor2, args)

def svn_wc_get_switch_editor(*args):
    """
    svn_wc_get_switch_editor(svn_revnum_t target_revision, svn_wc_adm_access_t anchor, 
        char target, char switch_url, svn_boolean_t use_commit_times, 
        svn_boolean_t recurse, 
        svn_wc_notify_func_t notify_func, svn_cancel_func_t cancel_func, 
        char diff3_cmd, svn_delta_editor_t editor, 
        void edit_baton, svn_wc_traversal_info_t ti, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_get_switch_editor, args)

def svn_wc_prop_list(*args):
    """
    svn_wc_prop_list(apr_hash_t props, char path, svn_wc_adm_access_t adm_access, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_prop_list, args)

def svn_wc_prop_get(*args):
    """
    svn_wc_prop_get(svn_string_t value, char name, char path, svn_wc_adm_access_t adm_access, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_prop_get, args)

def svn_wc_prop_set2(*args):
    """
    svn_wc_prop_set2(char name, svn_string_t value, char path, svn_wc_adm_access_t adm_access, 
        svn_boolean_t skip_checks, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_prop_set2, args)

def svn_wc_prop_set(*args):
    """
    svn_wc_prop_set(char name, svn_string_t value, char path, svn_wc_adm_access_t adm_access, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_prop_set, args)

def svn_wc_is_normal_prop(*args):
    """svn_wc_is_normal_prop(char name) -> svn_boolean_t"""
    return apply(_wc.svn_wc_is_normal_prop, args)

def svn_wc_is_wc_prop(*args):
    """svn_wc_is_wc_prop(char name) -> svn_boolean_t"""
    return apply(_wc.svn_wc_is_wc_prop, args)

def svn_wc_is_entry_prop(*args):
    """svn_wc_is_entry_prop(char name) -> svn_boolean_t"""
    return apply(_wc.svn_wc_is_entry_prop, args)

def svn_wc_get_diff_editor3(*args):
    """
    svn_wc_get_diff_editor3(svn_wc_adm_access_t anchor, char target, svn_wc_diff_callbacks2_t callbacks, 
        void callback_baton, 
        svn_boolean_t recurse, svn_boolean_t ignore_ancestry, 
        svn_boolean_t use_text_base, svn_boolean_t reverse_order, 
        svn_cancel_func_t cancel_func, 
        svn_delta_editor_t editor, void edit_baton, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_get_diff_editor3, args)

def svn_wc_get_diff_editor2(*args):
    """
    svn_wc_get_diff_editor2(svn_wc_adm_access_t anchor, char target, svn_wc_diff_callbacks_t callbacks, 
        void callback_baton, 
        svn_boolean_t recurse, svn_boolean_t ignore_ancestry, 
        svn_boolean_t use_text_base, svn_boolean_t reverse_order, 
        svn_cancel_func_t cancel_func, 
        svn_delta_editor_t editor, void edit_baton, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_get_diff_editor2, args)

def svn_wc_get_diff_editor(*args):
    """
    svn_wc_get_diff_editor(svn_wc_adm_access_t anchor, char target, svn_wc_diff_callbacks_t callbacks, 
        void callback_baton, 
        svn_boolean_t recurse, svn_boolean_t use_text_base, 
        svn_boolean_t reverse_order, svn_cancel_func_t cancel_func, 
        svn_delta_editor_t editor, 
        void edit_baton, apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_get_diff_editor, args)

def svn_wc_diff3(*args):
    """
    svn_wc_diff3(svn_wc_adm_access_t anchor, char target, svn_wc_diff_callbacks2_t callbacks, 
        void callback_baton, 
        svn_boolean_t recurse, svn_boolean_t ignore_ancestry, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_diff3, args)

def svn_wc_diff2(*args):
    """
    svn_wc_diff2(svn_wc_adm_access_t anchor, char target, svn_wc_diff_callbacks_t callbacks, 
        void callback_baton, 
        svn_boolean_t recurse, svn_boolean_t ignore_ancestry, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_diff2, args)

def svn_wc_diff(*args):
    """
    svn_wc_diff(svn_wc_adm_access_t anchor, char target, svn_wc_diff_callbacks_t callbacks, 
        void callback_baton, 
        svn_boolean_t recurse, apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_diff, args)

def svn_wc_get_prop_diffs(*args):
    """
    svn_wc_get_prop_diffs(apr_array_header_t propchanges, apr_hash_t original_props, 
        char path, svn_wc_adm_access_t adm_access, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_get_prop_diffs, args)
svn_wc_merge_unchanged = _wc.svn_wc_merge_unchanged
svn_wc_merge_merged = _wc.svn_wc_merge_merged
svn_wc_merge_conflict = _wc.svn_wc_merge_conflict
svn_wc_merge_no_merge = _wc.svn_wc_merge_no_merge

def svn_wc_merge2(*args):
    """
    svn_wc_merge2(enum svn_wc_merge_outcome_t merge_outcome, char left, 
        char right, char merge_target, svn_wc_adm_access_t adm_access, 
        char left_label, char right_label, 
        char target_label, svn_boolean_t dry_run, 
        char diff3_cmd, apr_array_header_t merge_options, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_merge2, args)

def svn_wc_merge(*args):
    """
    svn_wc_merge(char left, char right, char merge_target, svn_wc_adm_access_t adm_access, 
        char left_label, char right_label, 
        char target_label, svn_boolean_t dry_run, 
        enum svn_wc_merge_outcome_t merge_outcome, 
        char diff3_cmd, apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_merge, args)

def svn_wc_merge_props(*args):
    """
    svn_wc_merge_props(svn_wc_notify_state_t state, char path, svn_wc_adm_access_t adm_access, 
        apr_hash_t baseprops, apr_array_header_t propchanges, 
        svn_boolean_t base_merge, 
        svn_boolean_t dry_run, apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_merge_props, args)

def svn_wc_merge_prop_diffs(*args):
    """
    svn_wc_merge_prop_diffs(svn_wc_notify_state_t state, char path, svn_wc_adm_access_t adm_access, 
        apr_array_header_t propchanges, 
        svn_boolean_t base_merge, svn_boolean_t dry_run, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_merge_prop_diffs, args)

def svn_wc_get_pristine_copy_path(*args):
    """svn_wc_get_pristine_copy_path(char path, char pristine_path, apr_pool_t pool) -> svn_error_t"""
    return apply(_wc.svn_wc_get_pristine_copy_path, args)

def svn_wc_cleanup2(*args):
    """
    svn_wc_cleanup2(char path, char diff3_cmd, svn_cancel_func_t cancel_func, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_cleanup2, args)

def svn_wc_cleanup(*args):
    """
    svn_wc_cleanup(char path, svn_wc_adm_access_t optional_adm_access, 
        char diff3_cmd, svn_cancel_func_t cancel_func, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_cleanup, args)

def svn_wc_relocate2(*args):
    """
    svn_wc_relocate2(char path, svn_wc_adm_access_t adm_access, char from, 
        char to, svn_boolean_t recurse, svn_wc_relocation_validator2_t validator, 
        void validator_baton, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_relocate2, args)

def svn_wc_relocate(*args):
    """
    svn_wc_relocate(char path, svn_wc_adm_access_t adm_access, char from, 
        char to, svn_boolean_t recurse, svn_wc_relocation_validator_t validator, 
        void validator_baton, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_relocate, args)

def svn_wc_revert2(*args):
    """
    svn_wc_revert2(char path, svn_wc_adm_access_t parent_access, svn_boolean_t recursive, 
        svn_boolean_t use_commit_times, 
        svn_cancel_func_t cancel_func, svn_wc_notify_func2_t notify_func, 
        void notify_baton, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_revert2, args)

def svn_wc_revert(*args):
    """
    svn_wc_revert(char path, svn_wc_adm_access_t parent_access, svn_boolean_t recursive, 
        svn_boolean_t use_commit_times, 
        svn_cancel_func_t cancel_func, svn_wc_notify_func_t notify_func, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_revert, args)

def svn_wc_create_tmp_file2(*args):
    """
    svn_wc_create_tmp_file2(apr_file_t fp, char new_name, char path, svn_io_file_del_t delete_when, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_create_tmp_file2, args)

def svn_wc_create_tmp_file(*args):
    """
    svn_wc_create_tmp_file(apr_file_t fp, char path, svn_boolean_t delete_on_close, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_create_tmp_file, args)

def svn_wc_translated_file2(*args):
    """
    svn_wc_translated_file2(char xlated_path, char src, char versioned_file, svn_wc_adm_access_t adm_access, 
        apr_uint32_t flags, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_translated_file2, args)

def svn_wc_translated_file(*args):
    """
    svn_wc_translated_file(char xlated_p, char vfile, svn_wc_adm_access_t adm_access, 
        svn_boolean_t force_repair, apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_translated_file, args)

def svn_wc_transmit_text_deltas2(*args):
    """
    svn_wc_transmit_text_deltas2(char tempfile, unsigned char digest, char path, svn_wc_adm_access_t adm_access, 
        svn_boolean_t fulltext, 
        svn_delta_editor_t editor, void file_baton, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_transmit_text_deltas2, args)

def svn_wc_transmit_text_deltas(*args):
    """
    svn_wc_transmit_text_deltas(char path, svn_wc_adm_access_t adm_access, svn_boolean_t fulltext, 
        svn_delta_editor_t editor, void file_baton, 
        char tempfile, apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_transmit_text_deltas, args)

def svn_wc_transmit_prop_deltas(*args):
    """
    svn_wc_transmit_prop_deltas(char path, svn_wc_adm_access_t adm_access, svn_wc_entry_t entry, 
        svn_delta_editor_t editor, void baton, 
        char tempfile, apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_transmit_prop_deltas, args)

def svn_wc_get_default_ignores(*args):
    """svn_wc_get_default_ignores(apr_array_header_t patterns, apr_hash_t config, apr_pool_t pool) -> svn_error_t"""
    return apply(_wc.svn_wc_get_default_ignores, args)

def svn_wc_get_ignores(*args):
    """
    svn_wc_get_ignores(apr_array_header_t patterns, apr_hash_t config, svn_wc_adm_access_t adm_access, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_get_ignores, args)

def svn_wc_add_lock(*args):
    """
    svn_wc_add_lock(char path, svn_lock_t lock, svn_wc_adm_access_t adm_access, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_add_lock, args)

def svn_wc_remove_lock(*args):
    """svn_wc_remove_lock(char path, svn_wc_adm_access_t adm_access, apr_pool_t pool) -> svn_error_t"""
    return apply(_wc.svn_wc_remove_lock, args)
class svn_wc_revision_status_t:
    """Proxy of C svn_wc_revision_status_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, svn_wc_revision_status_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, svn_wc_revision_status_t, name)
    def __repr__(self):
        return "<%s.%s; proxy of C svn_wc_revision_status_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    __swig_setmethods__["min_rev"] = _wc.svn_wc_revision_status_t_min_rev_set
    __swig_getmethods__["min_rev"] = _wc.svn_wc_revision_status_t_min_rev_get
    __swig_setmethods__["max_rev"] = _wc.svn_wc_revision_status_t_max_rev_set
    __swig_getmethods__["max_rev"] = _wc.svn_wc_revision_status_t_max_rev_get
    __swig_setmethods__["switched"] = _wc.svn_wc_revision_status_t_switched_set
    __swig_getmethods__["switched"] = _wc.svn_wc_revision_status_t_switched_get
    __swig_setmethods__["modified"] = _wc.svn_wc_revision_status_t_modified_set
    __swig_getmethods__["modified"] = _wc.svn_wc_revision_status_t_modified_get
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for svn_wc_revision_status_t"""
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

    def __init__(self, *args):
        """__init__(self) -> svn_wc_revision_status_t"""
        _swig_setattr(self, svn_wc_revision_status_t, 'this', apply(_wc.new_svn_wc_revision_status_t, args))
        _swig_setattr(self, svn_wc_revision_status_t, 'thisown', 1)
    def __del__(self, destroy=_wc.delete_svn_wc_revision_status_t):
        """__del__(self)"""
        try:
            if self.thisown: destroy(self)
        except: pass


class svn_wc_revision_status_tPtr(svn_wc_revision_status_t):
    def __init__(self, this):
        _swig_setattr(self, svn_wc_revision_status_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, svn_wc_revision_status_t, 'thisown', 0)
        _swig_setattr(self, svn_wc_revision_status_t,self.__class__,svn_wc_revision_status_t)
_wc.svn_wc_revision_status_t_swigregister(svn_wc_revision_status_tPtr)


def svn_wc_revision_status(*args):
    """
    svn_wc_revision_status(svn_wc_revision_status_t result_p, char wc_path, char trail_url, 
        svn_boolean_t committed, svn_cancel_func_t cancel_func, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_revision_status, args)
class svn_wc_adm_access_t:
    """Proxy of C svn_wc_adm_access_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, svn_wc_adm_access_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, svn_wc_adm_access_t, name)
    def __init__(self): raise RuntimeError, "No constructor defined"
    def __repr__(self):
        return "<%s.%s; proxy of C svn_wc_adm_access_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for svn_wc_adm_access_t"""
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


class svn_wc_adm_access_tPtr(svn_wc_adm_access_t):
    def __init__(self, this):
        _swig_setattr(self, svn_wc_adm_access_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, svn_wc_adm_access_t, 'thisown', 0)
        _swig_setattr(self, svn_wc_adm_access_t,self.__class__,svn_wc_adm_access_t)
_wc.svn_wc_adm_access_t_swigregister(svn_wc_adm_access_tPtr)

class svn_wc_traversal_info_t:
    """Proxy of C svn_wc_traversal_info_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, svn_wc_traversal_info_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, svn_wc_traversal_info_t, name)
    def __init__(self): raise RuntimeError, "No constructor defined"
    def __repr__(self):
        return "<%s.%s; proxy of C svn_wc_traversal_info_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for svn_wc_traversal_info_t"""
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


class svn_wc_traversal_info_tPtr(svn_wc_traversal_info_t):
    def __init__(self, this):
        _swig_setattr(self, svn_wc_traversal_info_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, svn_wc_traversal_info_t, 'thisown', 0)
        _swig_setattr(self, svn_wc_traversal_info_t,self.__class__,svn_wc_traversal_info_t)
_wc.svn_wc_traversal_info_t_swigregister(svn_wc_traversal_info_tPtr)


def svn_wc_diff_callbacks2_invoke_file_changed(*args):
    """
    svn_wc_diff_callbacks2_invoke_file_changed(svn_wc_diff_callbacks2_t _obj, svn_wc_adm_access_t adm_access, 
        svn_wc_notify_state_t contentstate, 
        svn_wc_notify_state_t propstate, char path, 
        char tmpfile1, char tmpfile2, svn_revnum_t rev1, 
        svn_revnum_t rev2, char mimetype1, char mimetype2, 
        apr_array_header_t propchanges, apr_hash_t originalprops, 
        void diff_baton) -> svn_error_t
    """
    return apply(_wc.svn_wc_diff_callbacks2_invoke_file_changed, args)

def svn_wc_diff_callbacks2_invoke_file_added(*args):
    """
    svn_wc_diff_callbacks2_invoke_file_added(svn_wc_diff_callbacks2_t _obj, svn_wc_adm_access_t adm_access, 
        svn_wc_notify_state_t contentstate, 
        svn_wc_notify_state_t propstate, char path, 
        char tmpfile1, char tmpfile2, svn_revnum_t rev1, 
        svn_revnum_t rev2, char mimetype1, char mimetype2, 
        apr_array_header_t propchanges, apr_hash_t originalprops, 
        void diff_baton) -> svn_error_t
    """
    return apply(_wc.svn_wc_diff_callbacks2_invoke_file_added, args)

def svn_wc_diff_callbacks2_invoke_file_deleted(*args):
    """
    svn_wc_diff_callbacks2_invoke_file_deleted(svn_wc_diff_callbacks2_t _obj, svn_wc_adm_access_t adm_access, 
        svn_wc_notify_state_t state, char path, 
        char tmpfile1, char tmpfile2, char mimetype1, 
        char mimetype2, apr_hash_t originalprops, 
        void diff_baton) -> svn_error_t
    """
    return apply(_wc.svn_wc_diff_callbacks2_invoke_file_deleted, args)

def svn_wc_diff_callbacks2_invoke_dir_added(*args):
    """
    svn_wc_diff_callbacks2_invoke_dir_added(svn_wc_diff_callbacks2_t _obj, svn_wc_adm_access_t adm_access, 
        svn_wc_notify_state_t state, char path, 
        svn_revnum_t rev, void diff_baton) -> svn_error_t
    """
    return apply(_wc.svn_wc_diff_callbacks2_invoke_dir_added, args)

def svn_wc_diff_callbacks2_invoke_dir_deleted(*args):
    """
    svn_wc_diff_callbacks2_invoke_dir_deleted(svn_wc_diff_callbacks2_t _obj, svn_wc_adm_access_t adm_access, 
        svn_wc_notify_state_t state, char path, 
        void diff_baton) -> svn_error_t
    """
    return apply(_wc.svn_wc_diff_callbacks2_invoke_dir_deleted, args)

def svn_wc_diff_callbacks2_invoke_dir_props_changed(*args):
    """
    svn_wc_diff_callbacks2_invoke_dir_props_changed(svn_wc_diff_callbacks2_t _obj, svn_wc_adm_access_t adm_access, 
        svn_wc_notify_state_t state, char path, 
        apr_array_header_t propchanges, apr_hash_t original_props, 
        void diff_baton) -> svn_error_t
    """
    return apply(_wc.svn_wc_diff_callbacks2_invoke_dir_props_changed, args)

def svn_wc_diff_callbacks_invoke_file_changed(*args):
    """
    svn_wc_diff_callbacks_invoke_file_changed(svn_wc_diff_callbacks_t _obj, svn_wc_adm_access_t adm_access, 
        svn_wc_notify_state_t state, char path, 
        char tmpfile1, char tmpfile2, svn_revnum_t rev1, 
        svn_revnum_t rev2, char mimetype1, 
        char mimetype2, void diff_baton) -> svn_error_t
    """
    return apply(_wc.svn_wc_diff_callbacks_invoke_file_changed, args)

def svn_wc_diff_callbacks_invoke_file_added(*args):
    """
    svn_wc_diff_callbacks_invoke_file_added(svn_wc_diff_callbacks_t _obj, svn_wc_adm_access_t adm_access, 
        svn_wc_notify_state_t state, char path, 
        char tmpfile1, char tmpfile2, svn_revnum_t rev1, 
        svn_revnum_t rev2, char mimetype1, 
        char mimetype2, void diff_baton) -> svn_error_t
    """
    return apply(_wc.svn_wc_diff_callbacks_invoke_file_added, args)

def svn_wc_diff_callbacks_invoke_file_deleted(*args):
    """
    svn_wc_diff_callbacks_invoke_file_deleted(svn_wc_diff_callbacks_t _obj, svn_wc_adm_access_t adm_access, 
        svn_wc_notify_state_t state, char path, 
        char tmpfile1, char tmpfile2, char mimetype1, 
        char mimetype2, void diff_baton) -> svn_error_t
    """
    return apply(_wc.svn_wc_diff_callbacks_invoke_file_deleted, args)

def svn_wc_diff_callbacks_invoke_dir_added(*args):
    """
    svn_wc_diff_callbacks_invoke_dir_added(svn_wc_diff_callbacks_t _obj, svn_wc_adm_access_t adm_access, 
        svn_wc_notify_state_t state, char path, 
        svn_revnum_t rev, void diff_baton) -> svn_error_t
    """
    return apply(_wc.svn_wc_diff_callbacks_invoke_dir_added, args)

def svn_wc_diff_callbacks_invoke_dir_deleted(*args):
    """
    svn_wc_diff_callbacks_invoke_dir_deleted(svn_wc_diff_callbacks_t _obj, svn_wc_adm_access_t adm_access, 
        svn_wc_notify_state_t state, char path, 
        void diff_baton) -> svn_error_t
    """
    return apply(_wc.svn_wc_diff_callbacks_invoke_dir_deleted, args)

def svn_wc_diff_callbacks_invoke_props_changed(*args):
    """
    svn_wc_diff_callbacks_invoke_props_changed(svn_wc_diff_callbacks_t _obj, svn_wc_adm_access_t adm_access, 
        svn_wc_notify_state_t state, char path, 
        apr_array_header_t propchanges, apr_hash_t original_props, 
        void diff_baton) -> svn_error_t
    """
    return apply(_wc.svn_wc_diff_callbacks_invoke_props_changed, args)

def svn_wc_entry_callbacks_invoke_found_entry(*args):
    """
    svn_wc_entry_callbacks_invoke_found_entry(svn_wc_entry_callbacks_t _obj, char path, svn_wc_entry_t entry, 
        void walk_baton, apr_pool_t pool) -> svn_error_t
    """
    return apply(_wc.svn_wc_entry_callbacks_invoke_found_entry, args)

def svn_wc_swig_init_asp_dot_net_hack(*args):
    """svn_wc_swig_init_asp_dot_net_hack(apr_pool_t pool) -> svn_error_t"""
    return apply(_wc.svn_wc_swig_init_asp_dot_net_hack, args)
svn_wc_swig_init_asp_dot_net_hack() 

