# This file was created automatically by SWIG.
# Don't modify this file, modify the SWIG interface instead.
# This file is compatible with both classic and new-style classes.

import _core

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

class svn_error_t:
    """Proxy of C svn_error_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, svn_error_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, svn_error_t, name)
    def __repr__(self):
        return "<%s.%s; proxy of C svn_error_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    __swig_setmethods__["apr_err"] = _core.svn_error_t_apr_err_set
    __swig_getmethods__["apr_err"] = _core.svn_error_t_apr_err_get
    __swig_getmethods__["message"] = _core.svn_error_t_message_get
    __swig_setmethods__["child"] = _core.svn_error_t_child_set
    __swig_getmethods__["child"] = _core.svn_error_t_child_get
    __swig_setmethods__["pool"] = _core.svn_error_t_pool_set
    __swig_getmethods__["pool"] = _core.svn_error_t_pool_get
    __swig_getmethods__["file"] = _core.svn_error_t_file_get
    __swig_setmethods__["line"] = _core.svn_error_t_line_set
    __swig_getmethods__["line"] = _core.svn_error_t_line_get
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for svn_error_t"""
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
        """__init__(self) -> svn_error_t"""
        _swig_setattr(self, svn_error_t, 'this', apply(_core.new_svn_error_t, args))
        _swig_setattr(self, svn_error_t, 'thisown', 1)
    def __del__(self, destroy=_core.delete_svn_error_t):
        """__del__(self)"""
        try:
            if self.thisown: destroy(self)
        except: pass


class svn_error_tPtr(svn_error_t):
    def __init__(self, this):
        _swig_setattr(self, svn_error_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, svn_error_t, 'thisown', 0)
        _swig_setattr(self, svn_error_t,self.__class__,svn_error_t)
_core.svn_error_t_swigregister(svn_error_tPtr)

svn_node_none = _core.svn_node_none
svn_node_file = _core.svn_node_file
svn_node_dir = _core.svn_node_dir
svn_node_unknown = _core.svn_node_unknown
SVN_REVNUM_T_FMT = _core.SVN_REVNUM_T_FMT
TRUE = _core.TRUE
FALSE = _core.FALSE
svn_nonrecursive = _core.svn_nonrecursive
svn_recursive = _core.svn_recursive
SVN_DIRENT_KIND = _core.SVN_DIRENT_KIND
SVN_DIRENT_SIZE = _core.SVN_DIRENT_SIZE
SVN_DIRENT_HAS_PROPS = _core.SVN_DIRENT_HAS_PROPS
SVN_DIRENT_CREATED_REV = _core.SVN_DIRENT_CREATED_REV
SVN_DIRENT_TIME = _core.SVN_DIRENT_TIME
SVN_DIRENT_LAST_AUTHOR = _core.SVN_DIRENT_LAST_AUTHOR
class svn_dirent_t:
    """Proxy of C svn_dirent_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, svn_dirent_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, svn_dirent_t, name)
    def __repr__(self):
        return "<%s.%s; proxy of C svn_dirent_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    __swig_setmethods__["kind"] = _core.svn_dirent_t_kind_set
    __swig_getmethods__["kind"] = _core.svn_dirent_t_kind_get
    __swig_setmethods__["size"] = _core.svn_dirent_t_size_set
    __swig_getmethods__["size"] = _core.svn_dirent_t_size_get
    __swig_setmethods__["has_props"] = _core.svn_dirent_t_has_props_set
    __swig_getmethods__["has_props"] = _core.svn_dirent_t_has_props_get
    __swig_setmethods__["created_rev"] = _core.svn_dirent_t_created_rev_set
    __swig_getmethods__["created_rev"] = _core.svn_dirent_t_created_rev_get
    __swig_setmethods__["time"] = _core.svn_dirent_t_time_set
    __swig_getmethods__["time"] = _core.svn_dirent_t_time_get
    __swig_getmethods__["last_author"] = _core.svn_dirent_t_last_author_get
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for svn_dirent_t"""
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
        """__init__(self) -> svn_dirent_t"""
        _swig_setattr(self, svn_dirent_t, 'this', apply(_core.new_svn_dirent_t, args))
        _swig_setattr(self, svn_dirent_t, 'thisown', 1)
    def __del__(self, destroy=_core.delete_svn_dirent_t):
        """__del__(self)"""
        try:
            if self.thisown: destroy(self)
        except: pass


class svn_dirent_tPtr(svn_dirent_t):
    def __init__(self, this):
        _swig_setattr(self, svn_dirent_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, svn_dirent_t, 'thisown', 0)
        _swig_setattr(self, svn_dirent_t,self.__class__,svn_dirent_t)
_core.svn_dirent_t_swigregister(svn_dirent_tPtr)


def svn_dirent_dup(*args):
    """svn_dirent_dup(svn_dirent_t dirent, apr_pool_t pool) -> svn_dirent_t"""
    return apply(_core.svn_dirent_dup, args)
SVN_KEYWORD_MAX_LEN = _core.SVN_KEYWORD_MAX_LEN
SVN_KEYWORD_REVISION_LONG = _core.SVN_KEYWORD_REVISION_LONG
SVN_KEYWORD_REVISION_SHORT = _core.SVN_KEYWORD_REVISION_SHORT
SVN_KEYWORD_REVISION_MEDIUM = _core.SVN_KEYWORD_REVISION_MEDIUM
SVN_KEYWORD_DATE_LONG = _core.SVN_KEYWORD_DATE_LONG
SVN_KEYWORD_DATE_SHORT = _core.SVN_KEYWORD_DATE_SHORT
SVN_KEYWORD_AUTHOR_LONG = _core.SVN_KEYWORD_AUTHOR_LONG
SVN_KEYWORD_AUTHOR_SHORT = _core.SVN_KEYWORD_AUTHOR_SHORT
SVN_KEYWORD_URL_LONG = _core.SVN_KEYWORD_URL_LONG
SVN_KEYWORD_URL_SHORT = _core.SVN_KEYWORD_URL_SHORT
SVN_KEYWORD_ID = _core.SVN_KEYWORD_ID
class svn_commit_info_t:
    """Proxy of C svn_commit_info_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, svn_commit_info_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, svn_commit_info_t, name)
    def __repr__(self):
        return "<%s.%s; proxy of C svn_commit_info_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    __swig_setmethods__["revision"] = _core.svn_commit_info_t_revision_set
    __swig_getmethods__["revision"] = _core.svn_commit_info_t_revision_get
    __swig_setmethods__["date"] = _core.svn_commit_info_t_date_set
    __swig_getmethods__["date"] = _core.svn_commit_info_t_date_get
    __swig_setmethods__["author"] = _core.svn_commit_info_t_author_set
    __swig_getmethods__["author"] = _core.svn_commit_info_t_author_get
    __swig_setmethods__["post_commit_err"] = _core.svn_commit_info_t_post_commit_err_set
    __swig_getmethods__["post_commit_err"] = _core.svn_commit_info_t_post_commit_err_get
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for svn_commit_info_t"""
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
        """__init__(self) -> svn_commit_info_t"""
        _swig_setattr(self, svn_commit_info_t, 'this', apply(_core.new_svn_commit_info_t, args))
        _swig_setattr(self, svn_commit_info_t, 'thisown', 1)
    def __del__(self, destroy=_core.delete_svn_commit_info_t):
        """__del__(self)"""
        try:
            if self.thisown: destroy(self)
        except: pass


class svn_commit_info_tPtr(svn_commit_info_t):
    def __init__(self, this):
        _swig_setattr(self, svn_commit_info_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, svn_commit_info_t, 'thisown', 0)
        _swig_setattr(self, svn_commit_info_t,self.__class__,svn_commit_info_t)
_core.svn_commit_info_t_swigregister(svn_commit_info_tPtr)


def svn_create_commit_info(*args):
    """svn_create_commit_info(apr_pool_t pool) -> svn_commit_info_t"""
    return apply(_core.svn_create_commit_info, args)

def svn_commit_info_dup(*args):
    """svn_commit_info_dup(svn_commit_info_t src_commit_info, apr_pool_t pool) -> svn_commit_info_t"""
    return apply(_core.svn_commit_info_dup, args)
class svn_log_changed_path_t:
    """Proxy of C svn_log_changed_path_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, svn_log_changed_path_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, svn_log_changed_path_t, name)
    def __repr__(self):
        return "<%s.%s; proxy of C svn_log_changed_path_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    __swig_setmethods__["action"] = _core.svn_log_changed_path_t_action_set
    __swig_getmethods__["action"] = _core.svn_log_changed_path_t_action_get
    __swig_getmethods__["copyfrom_path"] = _core.svn_log_changed_path_t_copyfrom_path_get
    __swig_setmethods__["copyfrom_rev"] = _core.svn_log_changed_path_t_copyfrom_rev_set
    __swig_getmethods__["copyfrom_rev"] = _core.svn_log_changed_path_t_copyfrom_rev_get
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for svn_log_changed_path_t"""
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
        """__init__(self) -> svn_log_changed_path_t"""
        _swig_setattr(self, svn_log_changed_path_t, 'this', apply(_core.new_svn_log_changed_path_t, args))
        _swig_setattr(self, svn_log_changed_path_t, 'thisown', 1)
    def __del__(self, destroy=_core.delete_svn_log_changed_path_t):
        """__del__(self)"""
        try:
            if self.thisown: destroy(self)
        except: pass


class svn_log_changed_path_tPtr(svn_log_changed_path_t):
    def __init__(self, this):
        _swig_setattr(self, svn_log_changed_path_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, svn_log_changed_path_t, 'thisown', 0)
        _swig_setattr(self, svn_log_changed_path_t,self.__class__,svn_log_changed_path_t)
_core.svn_log_changed_path_t_swigregister(svn_log_changed_path_tPtr)


def svn_log_changed_path_dup(*args):
    """svn_log_changed_path_dup(svn_log_changed_path_t changed_path, apr_pool_t pool) -> svn_log_changed_path_t"""
    return apply(_core.svn_log_changed_path_dup, args)

def svn_compat_wrap_commit_callback(*args):
    """
    svn_compat_wrap_commit_callback(svn_commit_callback2_t callback2, void callback2_baton, 
        svn_commit_callback_t callback, void callback_baton, 
        apr_pool_t pool)
    """
    return apply(_core.svn_compat_wrap_commit_callback, args)
SVN_STREAM_CHUNK_SIZE = _core.SVN_STREAM_CHUNK_SIZE
SVN__STREAM_CHUNK_SIZE = _core.SVN__STREAM_CHUNK_SIZE

def svn_mime_type_validate(*args):
    """svn_mime_type_validate(char mime_type, apr_pool_t pool) -> svn_error_t"""
    return apply(_core.svn_mime_type_validate, args)

def svn_mime_type_is_binary(*args):
    """svn_mime_type_is_binary(char mime_type) -> svn_boolean_t"""
    return apply(_core.svn_mime_type_is_binary, args)
class svn_lock_t:
    """Proxy of C svn_lock_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, svn_lock_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, svn_lock_t, name)
    def __repr__(self):
        return "<%s.%s; proxy of C svn_lock_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    __swig_setmethods__["path"] = _core.svn_lock_t_path_set
    __swig_getmethods__["path"] = _core.svn_lock_t_path_get
    __swig_setmethods__["token"] = _core.svn_lock_t_token_set
    __swig_getmethods__["token"] = _core.svn_lock_t_token_get
    __swig_setmethods__["owner"] = _core.svn_lock_t_owner_set
    __swig_getmethods__["owner"] = _core.svn_lock_t_owner_get
    __swig_setmethods__["comment"] = _core.svn_lock_t_comment_set
    __swig_getmethods__["comment"] = _core.svn_lock_t_comment_get
    __swig_setmethods__["is_dav_comment"] = _core.svn_lock_t_is_dav_comment_set
    __swig_getmethods__["is_dav_comment"] = _core.svn_lock_t_is_dav_comment_get
    __swig_setmethods__["creation_date"] = _core.svn_lock_t_creation_date_set
    __swig_getmethods__["creation_date"] = _core.svn_lock_t_creation_date_get
    __swig_setmethods__["expiration_date"] = _core.svn_lock_t_expiration_date_set
    __swig_getmethods__["expiration_date"] = _core.svn_lock_t_expiration_date_get
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for svn_lock_t"""
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
        """__init__(self) -> svn_lock_t"""
        _swig_setattr(self, svn_lock_t, 'this', apply(_core.new_svn_lock_t, args))
        _swig_setattr(self, svn_lock_t, 'thisown', 1)
    def __del__(self, destroy=_core.delete_svn_lock_t):
        """__del__(self)"""
        try:
            if self.thisown: destroy(self)
        except: pass


class svn_lock_tPtr(svn_lock_t):
    def __init__(self, this):
        _swig_setattr(self, svn_lock_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, svn_lock_t, 'thisown', 0)
        _swig_setattr(self, svn_lock_t,self.__class__,svn_lock_t)
_core.svn_lock_t_swigregister(svn_lock_tPtr)


def svn_lock_create(*args):
    """svn_lock_create(apr_pool_t pool) -> svn_lock_t"""
    return apply(_core.svn_lock_create, args)

def svn_lock_dup(*args):
    """svn_lock_dup(svn_lock_t lock, apr_pool_t pool) -> svn_lock_t"""
    return apply(_core.svn_lock_dup, args)

def svn_uuid_generate(*args):
    """svn_uuid_generate(apr_pool_t pool) -> char"""
    return apply(_core.svn_uuid_generate, args)
class svn_string_t:
    """Proxy of C svn_string_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, svn_string_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, svn_string_t, name)
    def __repr__(self):
        return "<%s.%s; proxy of C svn_string_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    __swig_setmethods__["data"] = _core.svn_string_t_data_set
    __swig_getmethods__["data"] = _core.svn_string_t_data_get
    __swig_setmethods__["len"] = _core.svn_string_t_len_set
    __swig_getmethods__["len"] = _core.svn_string_t_len_get
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for svn_string_t"""
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
        """__init__(self) -> svn_string_t"""
        _swig_setattr(self, svn_string_t, 'this', apply(_core.new_svn_string_t, args))
        _swig_setattr(self, svn_string_t, 'thisown', 1)
    def __del__(self, destroy=_core.delete_svn_string_t):
        """__del__(self)"""
        try:
            if self.thisown: destroy(self)
        except: pass


class svn_string_tPtr(svn_string_t):
    def __init__(self, this):
        _swig_setattr(self, svn_string_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, svn_string_t, 'thisown', 0)
        _swig_setattr(self, svn_string_t,self.__class__,svn_string_t)
_core.svn_string_t_swigregister(svn_string_tPtr)

class svn_stringbuf_t:
    """Proxy of C svn_stringbuf_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, svn_stringbuf_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, svn_stringbuf_t, name)
    def __repr__(self):
        return "<%s.%s; proxy of C svn_stringbuf_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    __swig_setmethods__["pool"] = _core.svn_stringbuf_t_pool_set
    __swig_getmethods__["pool"] = _core.svn_stringbuf_t_pool_get
    __swig_setmethods__["data"] = _core.svn_stringbuf_t_data_set
    __swig_getmethods__["data"] = _core.svn_stringbuf_t_data_get
    __swig_setmethods__["len"] = _core.svn_stringbuf_t_len_set
    __swig_getmethods__["len"] = _core.svn_stringbuf_t_len_get
    __swig_setmethods__["blocksize"] = _core.svn_stringbuf_t_blocksize_set
    __swig_getmethods__["blocksize"] = _core.svn_stringbuf_t_blocksize_get
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for svn_stringbuf_t"""
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
        """__init__(self) -> svn_stringbuf_t"""
        _swig_setattr(self, svn_stringbuf_t, 'this', apply(_core.new_svn_stringbuf_t, args))
        _swig_setattr(self, svn_stringbuf_t, 'thisown', 1)
    def __del__(self, destroy=_core.delete_svn_stringbuf_t):
        """__del__(self)"""
        try:
            if self.thisown: destroy(self)
        except: pass


class svn_stringbuf_tPtr(svn_stringbuf_t):
    def __init__(self, this):
        _swig_setattr(self, svn_stringbuf_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, svn_stringbuf_t, 'thisown', 0)
        _swig_setattr(self, svn_stringbuf_t,self.__class__,svn_stringbuf_t)
_core.svn_stringbuf_t_swigregister(svn_stringbuf_tPtr)


def svn_string_create(*args):
    """svn_string_create(char cstring, apr_pool_t pool) -> svn_string_t"""
    return apply(_core.svn_string_create, args)

def svn_string_ncreate(*args):
    """svn_string_ncreate(char bytes, apr_size_t size, apr_pool_t pool) -> svn_string_t"""
    return apply(_core.svn_string_ncreate, args)

def svn_string_create_from_buf(*args):
    """svn_string_create_from_buf(svn_stringbuf_t strbuf, apr_pool_t pool) -> svn_string_t"""
    return apply(_core.svn_string_create_from_buf, args)

def svn_string_createf(*args):
    """svn_string_createf(apr_pool_t pool, char fmt, v(...) ??) -> svn_string_t"""
    return apply(_core.svn_string_createf, args)

def svn_string_isempty(*args):
    """svn_string_isempty(svn_string_t str) -> svn_boolean_t"""
    return apply(_core.svn_string_isempty, args)

def svn_string_dup(*args):
    """svn_string_dup(svn_string_t original_string, apr_pool_t pool) -> svn_string_t"""
    return apply(_core.svn_string_dup, args)

def svn_string_compare(*args):
    """svn_string_compare(svn_string_t str1, svn_string_t str2) -> svn_boolean_t"""
    return apply(_core.svn_string_compare, args)

def svn_string_first_non_whitespace(*args):
    """svn_string_first_non_whitespace(svn_string_t str) -> apr_size_t"""
    return apply(_core.svn_string_first_non_whitespace, args)

def svn_string_find_char_backward(*args):
    """svn_string_find_char_backward(svn_string_t str, char ch) -> apr_size_t"""
    return apply(_core.svn_string_find_char_backward, args)

def svn_stringbuf_create(*args):
    """svn_stringbuf_create(char cstring, apr_pool_t pool) -> svn_stringbuf_t"""
    return apply(_core.svn_stringbuf_create, args)

def svn_stringbuf_ncreate(*args):
    """svn_stringbuf_ncreate(char bytes, apr_size_t size, apr_pool_t pool) -> svn_stringbuf_t"""
    return apply(_core.svn_stringbuf_ncreate, args)

def svn_stringbuf_create_from_string(*args):
    """svn_stringbuf_create_from_string(svn_string_t str, apr_pool_t pool) -> svn_stringbuf_t"""
    return apply(_core.svn_stringbuf_create_from_string, args)

def svn_stringbuf_createf(*args):
    """svn_stringbuf_createf(apr_pool_t pool, char fmt, v(...) ??) -> svn_stringbuf_t"""
    return apply(_core.svn_stringbuf_createf, args)

def svn_stringbuf_ensure(*args):
    """svn_stringbuf_ensure(svn_stringbuf_t str, apr_size_t minimum_size)"""
    return apply(_core.svn_stringbuf_ensure, args)

def svn_stringbuf_set(*args):
    """svn_stringbuf_set(svn_stringbuf_t str, char value)"""
    return apply(_core.svn_stringbuf_set, args)

def svn_stringbuf_setempty(*args):
    """svn_stringbuf_setempty(svn_stringbuf_t str)"""
    return apply(_core.svn_stringbuf_setempty, args)

def svn_stringbuf_isempty(*args):
    """svn_stringbuf_isempty(svn_stringbuf_t str) -> svn_boolean_t"""
    return apply(_core.svn_stringbuf_isempty, args)

def svn_stringbuf_chop(*args):
    """svn_stringbuf_chop(svn_stringbuf_t str, apr_size_t bytes)"""
    return apply(_core.svn_stringbuf_chop, args)

def svn_stringbuf_fillchar(*args):
    """svn_stringbuf_fillchar(svn_stringbuf_t str, unsigned char c)"""
    return apply(_core.svn_stringbuf_fillchar, args)

def svn_stringbuf_appendbytes(*args):
    """svn_stringbuf_appendbytes(svn_stringbuf_t targetstr, char bytes, apr_size_t count)"""
    return apply(_core.svn_stringbuf_appendbytes, args)

def svn_stringbuf_appendstr(*args):
    """svn_stringbuf_appendstr(svn_stringbuf_t targetstr, svn_stringbuf_t appendstr)"""
    return apply(_core.svn_stringbuf_appendstr, args)

def svn_stringbuf_appendcstr(*args):
    """svn_stringbuf_appendcstr(svn_stringbuf_t targetstr, char cstr)"""
    return apply(_core.svn_stringbuf_appendcstr, args)

def svn_stringbuf_dup(*args):
    """svn_stringbuf_dup(svn_stringbuf_t original_string, apr_pool_t pool) -> svn_stringbuf_t"""
    return apply(_core.svn_stringbuf_dup, args)

def svn_stringbuf_compare(*args):
    """svn_stringbuf_compare(svn_stringbuf_t str1, svn_stringbuf_t str2) -> svn_boolean_t"""
    return apply(_core.svn_stringbuf_compare, args)

def svn_stringbuf_first_non_whitespace(*args):
    """svn_stringbuf_first_non_whitespace(svn_stringbuf_t str) -> apr_size_t"""
    return apply(_core.svn_stringbuf_first_non_whitespace, args)

def svn_stringbuf_strip_whitespace(*args):
    """svn_stringbuf_strip_whitespace(svn_stringbuf_t str)"""
    return apply(_core.svn_stringbuf_strip_whitespace, args)

def svn_stringbuf_find_char_backward(*args):
    """svn_stringbuf_find_char_backward(svn_stringbuf_t str, char ch) -> apr_size_t"""
    return apply(_core.svn_stringbuf_find_char_backward, args)

def svn_string_compare_stringbuf(*args):
    """svn_string_compare_stringbuf(svn_string_t str1, svn_stringbuf_t str2) -> svn_boolean_t"""
    return apply(_core.svn_string_compare_stringbuf, args)

def svn_cstring_split(*args):
    """
    svn_cstring_split(char input, char sep_chars, svn_boolean_t chop_whitespace, 
        apr_pool_t pool) -> apr_array_header_t
    """
    return apply(_core.svn_cstring_split, args)

def svn_cstring_split_append(*args):
    """
    svn_cstring_split_append(apr_array_header_t array, char input, char sep_chars, 
        svn_boolean_t chop_whitespace, apr_pool_t pool)
    """
    return apply(_core.svn_cstring_split_append, args)

def svn_cstring_match_glob_list(*args):
    """svn_cstring_match_glob_list(char str, apr_array_header_t list) -> svn_boolean_t"""
    return apply(_core.svn_cstring_match_glob_list, args)

def svn_cstring_count_newlines(*args):
    """svn_cstring_count_newlines(char msg) -> int"""
    return apply(_core.svn_cstring_count_newlines, args)

def svn_cstring_join(*args):
    """svn_cstring_join(apr_array_header_t strings, char separator, apr_pool_t pool) -> char"""
    return apply(_core.svn_cstring_join, args)

def svn_prop_dup(*args):
    """svn_prop_dup( prop, apr_pool_t pool)"""
    return apply(_core.svn_prop_dup, args)

def svn_prop_array_dup(*args):
    """svn_prop_array_dup(apr_array_header_t array, apr_pool_t pool) -> apr_array_header_t"""
    return apply(_core.svn_prop_array_dup, args)
svn_prop_entry_kind = _core.svn_prop_entry_kind
svn_prop_wc_kind = _core.svn_prop_wc_kind
svn_prop_regular_kind = _core.svn_prop_regular_kind

def svn_property_kind(*args):
    """svn_property_kind(int prefix_len, char prop_name) -> int"""
    return apply(_core.svn_property_kind, args)

def svn_prop_is_svn_prop(*args):
    """svn_prop_is_svn_prop(char prop_name) -> svn_boolean_t"""
    return apply(_core.svn_prop_is_svn_prop, args)

def svn_prop_needs_translation(*args):
    """svn_prop_needs_translation(char propname) -> svn_boolean_t"""
    return apply(_core.svn_prop_needs_translation, args)

def svn_categorize_props(*args):
    """
    svn_categorize_props(apr_array_header_t proplist, apr_array_header_t entry_props, 
        apr_array_header_t wc_props, apr_array_header_t regular_props, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_core.svn_categorize_props, args)

def svn_prop_diffs(*args):
    """
    svn_prop_diffs(apr_array_header_t propdiffs, apr_hash_t target_props, 
        apr_hash_t source_props, apr_pool_t pool) -> svn_error_t
    """
    return apply(_core.svn_prop_diffs, args)
SVN_PROP_PREFIX = _core.SVN_PROP_PREFIX
SVN_PROP_MIME_TYPE = _core.SVN_PROP_MIME_TYPE
SVN_PROP_IGNORE = _core.SVN_PROP_IGNORE
SVN_PROP_EOL_STYLE = _core.SVN_PROP_EOL_STYLE
SVN_PROP_KEYWORDS = _core.SVN_PROP_KEYWORDS
SVN_PROP_EXECUTABLE = _core.SVN_PROP_EXECUTABLE
SVN_PROP_EXECUTABLE_VALUE = _core.SVN_PROP_EXECUTABLE_VALUE
SVN_PROP_NEEDS_LOCK = _core.SVN_PROP_NEEDS_LOCK
SVN_PROP_NEEDS_LOCK_VALUE = _core.SVN_PROP_NEEDS_LOCK_VALUE
SVN_PROP_SPECIAL = _core.SVN_PROP_SPECIAL
SVN_PROP_SPECIAL_VALUE = _core.SVN_PROP_SPECIAL_VALUE
SVN_PROP_EXTERNALS = _core.SVN_PROP_EXTERNALS
SVN_PROP_WC_PREFIX = _core.SVN_PROP_WC_PREFIX
SVN_PROP_ENTRY_PREFIX = _core.SVN_PROP_ENTRY_PREFIX
SVN_PROP_ENTRY_COMMITTED_REV = _core.SVN_PROP_ENTRY_COMMITTED_REV
SVN_PROP_ENTRY_COMMITTED_DATE = _core.SVN_PROP_ENTRY_COMMITTED_DATE
SVN_PROP_ENTRY_LAST_AUTHOR = _core.SVN_PROP_ENTRY_LAST_AUTHOR
SVN_PROP_ENTRY_UUID = _core.SVN_PROP_ENTRY_UUID
SVN_PROP_ENTRY_LOCK_TOKEN = _core.SVN_PROP_ENTRY_LOCK_TOKEN
SVN_PROP_CUSTOM_PREFIX = _core.SVN_PROP_CUSTOM_PREFIX
SVN_PROP_REVISION_AUTHOR = _core.SVN_PROP_REVISION_AUTHOR
SVN_PROP_REVISION_LOG = _core.SVN_PROP_REVISION_LOG
SVN_PROP_REVISION_DATE = _core.SVN_PROP_REVISION_DATE
SVN_PROP_REVISION_ORIG_DATE = _core.SVN_PROP_REVISION_ORIG_DATE
SVN_PROP_REVISION_AUTOVERSIONED = _core.SVN_PROP_REVISION_AUTOVERSIONED
SVNSYNC_PROP_PREFIX = _core.SVNSYNC_PROP_PREFIX
SVNSYNC_PROP_LOCK = _core.SVNSYNC_PROP_LOCK
SVNSYNC_PROP_FROM_URL = _core.SVNSYNC_PROP_FROM_URL
SVNSYNC_PROP_FROM_UUID = _core.SVNSYNC_PROP_FROM_UUID
SVNSYNC_PROP_LAST_MERGED_REV = _core.SVNSYNC_PROP_LAST_MERGED_REV
SVNSYNC_PROP_CURRENTLY_COPYING = _core.SVNSYNC_PROP_CURRENTLY_COPYING
SVN_ERR_CATEGORY_SIZE = _core.SVN_ERR_CATEGORY_SIZE
SVN_WARNING = _core.SVN_WARNING
SVN_ERR_BAD_CONTAINING_POOL = _core.SVN_ERR_BAD_CONTAINING_POOL
SVN_ERR_BAD_FILENAME = _core.SVN_ERR_BAD_FILENAME
SVN_ERR_BAD_URL = _core.SVN_ERR_BAD_URL
SVN_ERR_BAD_DATE = _core.SVN_ERR_BAD_DATE
SVN_ERR_BAD_MIME_TYPE = _core.SVN_ERR_BAD_MIME_TYPE
SVN_ERR_BAD_VERSION_FILE_FORMAT = _core.SVN_ERR_BAD_VERSION_FILE_FORMAT
SVN_ERR_XML_ATTRIB_NOT_FOUND = _core.SVN_ERR_XML_ATTRIB_NOT_FOUND
SVN_ERR_XML_MISSING_ANCESTRY = _core.SVN_ERR_XML_MISSING_ANCESTRY
SVN_ERR_XML_UNKNOWN_ENCODING = _core.SVN_ERR_XML_UNKNOWN_ENCODING
SVN_ERR_XML_MALFORMED = _core.SVN_ERR_XML_MALFORMED
SVN_ERR_XML_UNESCAPABLE_DATA = _core.SVN_ERR_XML_UNESCAPABLE_DATA
SVN_ERR_IO_INCONSISTENT_EOL = _core.SVN_ERR_IO_INCONSISTENT_EOL
SVN_ERR_IO_UNKNOWN_EOL = _core.SVN_ERR_IO_UNKNOWN_EOL
SVN_ERR_IO_CORRUPT_EOL = _core.SVN_ERR_IO_CORRUPT_EOL
SVN_ERR_IO_UNIQUE_NAMES_EXHAUSTED = _core.SVN_ERR_IO_UNIQUE_NAMES_EXHAUSTED
SVN_ERR_IO_PIPE_FRAME_ERROR = _core.SVN_ERR_IO_PIPE_FRAME_ERROR
SVN_ERR_IO_PIPE_READ_ERROR = _core.SVN_ERR_IO_PIPE_READ_ERROR
SVN_ERR_IO_WRITE_ERROR = _core.SVN_ERR_IO_WRITE_ERROR
SVN_ERR_STREAM_UNEXPECTED_EOF = _core.SVN_ERR_STREAM_UNEXPECTED_EOF
SVN_ERR_STREAM_MALFORMED_DATA = _core.SVN_ERR_STREAM_MALFORMED_DATA
SVN_ERR_STREAM_UNRECOGNIZED_DATA = _core.SVN_ERR_STREAM_UNRECOGNIZED_DATA
SVN_ERR_NODE_UNKNOWN_KIND = _core.SVN_ERR_NODE_UNKNOWN_KIND
SVN_ERR_NODE_UNEXPECTED_KIND = _core.SVN_ERR_NODE_UNEXPECTED_KIND
SVN_ERR_ENTRY_NOT_FOUND = _core.SVN_ERR_ENTRY_NOT_FOUND
SVN_ERR_ENTRY_EXISTS = _core.SVN_ERR_ENTRY_EXISTS
SVN_ERR_ENTRY_MISSING_REVISION = _core.SVN_ERR_ENTRY_MISSING_REVISION
SVN_ERR_ENTRY_MISSING_URL = _core.SVN_ERR_ENTRY_MISSING_URL
SVN_ERR_ENTRY_ATTRIBUTE_INVALID = _core.SVN_ERR_ENTRY_ATTRIBUTE_INVALID
SVN_ERR_WC_OBSTRUCTED_UPDATE = _core.SVN_ERR_WC_OBSTRUCTED_UPDATE
SVN_ERR_WC_UNWIND_MISMATCH = _core.SVN_ERR_WC_UNWIND_MISMATCH
SVN_ERR_WC_UNWIND_EMPTY = _core.SVN_ERR_WC_UNWIND_EMPTY
SVN_ERR_WC_UNWIND_NOT_EMPTY = _core.SVN_ERR_WC_UNWIND_NOT_EMPTY
SVN_ERR_WC_LOCKED = _core.SVN_ERR_WC_LOCKED
SVN_ERR_WC_NOT_LOCKED = _core.SVN_ERR_WC_NOT_LOCKED
SVN_ERR_WC_INVALID_LOCK = _core.SVN_ERR_WC_INVALID_LOCK
SVN_ERR_WC_NOT_DIRECTORY = _core.SVN_ERR_WC_NOT_DIRECTORY
SVN_ERR_WC_NOT_FILE = _core.SVN_ERR_WC_NOT_FILE
SVN_ERR_WC_BAD_ADM_LOG = _core.SVN_ERR_WC_BAD_ADM_LOG
SVN_ERR_WC_PATH_NOT_FOUND = _core.SVN_ERR_WC_PATH_NOT_FOUND
SVN_ERR_WC_NOT_UP_TO_DATE = _core.SVN_ERR_WC_NOT_UP_TO_DATE
SVN_ERR_WC_LEFT_LOCAL_MOD = _core.SVN_ERR_WC_LEFT_LOCAL_MOD
SVN_ERR_WC_SCHEDULE_CONFLICT = _core.SVN_ERR_WC_SCHEDULE_CONFLICT
SVN_ERR_WC_PATH_FOUND = _core.SVN_ERR_WC_PATH_FOUND
SVN_ERR_WC_FOUND_CONFLICT = _core.SVN_ERR_WC_FOUND_CONFLICT
SVN_ERR_WC_CORRUPT = _core.SVN_ERR_WC_CORRUPT
SVN_ERR_WC_CORRUPT_TEXT_BASE = _core.SVN_ERR_WC_CORRUPT_TEXT_BASE
SVN_ERR_WC_NODE_KIND_CHANGE = _core.SVN_ERR_WC_NODE_KIND_CHANGE
SVN_ERR_WC_INVALID_OP_ON_CWD = _core.SVN_ERR_WC_INVALID_OP_ON_CWD
SVN_ERR_WC_BAD_ADM_LOG_START = _core.SVN_ERR_WC_BAD_ADM_LOG_START
SVN_ERR_WC_UNSUPPORTED_FORMAT = _core.SVN_ERR_WC_UNSUPPORTED_FORMAT
SVN_ERR_WC_BAD_PATH = _core.SVN_ERR_WC_BAD_PATH
SVN_ERR_WC_INVALID_SCHEDULE = _core.SVN_ERR_WC_INVALID_SCHEDULE
SVN_ERR_WC_INVALID_RELOCATION = _core.SVN_ERR_WC_INVALID_RELOCATION
SVN_ERR_WC_INVALID_SWITCH = _core.SVN_ERR_WC_INVALID_SWITCH
SVN_ERR_FS_GENERAL = _core.SVN_ERR_FS_GENERAL
SVN_ERR_FS_CLEANUP = _core.SVN_ERR_FS_CLEANUP
SVN_ERR_FS_ALREADY_OPEN = _core.SVN_ERR_FS_ALREADY_OPEN
SVN_ERR_FS_NOT_OPEN = _core.SVN_ERR_FS_NOT_OPEN
SVN_ERR_FS_CORRUPT = _core.SVN_ERR_FS_CORRUPT
SVN_ERR_FS_PATH_SYNTAX = _core.SVN_ERR_FS_PATH_SYNTAX
SVN_ERR_FS_NO_SUCH_REVISION = _core.SVN_ERR_FS_NO_SUCH_REVISION
SVN_ERR_FS_NO_SUCH_TRANSACTION = _core.SVN_ERR_FS_NO_SUCH_TRANSACTION
SVN_ERR_FS_NO_SUCH_ENTRY = _core.SVN_ERR_FS_NO_SUCH_ENTRY
SVN_ERR_FS_NO_SUCH_REPRESENTATION = _core.SVN_ERR_FS_NO_SUCH_REPRESENTATION
SVN_ERR_FS_NO_SUCH_STRING = _core.SVN_ERR_FS_NO_SUCH_STRING
SVN_ERR_FS_NO_SUCH_COPY = _core.SVN_ERR_FS_NO_SUCH_COPY
SVN_ERR_FS_TRANSACTION_NOT_MUTABLE = _core.SVN_ERR_FS_TRANSACTION_NOT_MUTABLE
SVN_ERR_FS_NOT_FOUND = _core.SVN_ERR_FS_NOT_FOUND
SVN_ERR_FS_ID_NOT_FOUND = _core.SVN_ERR_FS_ID_NOT_FOUND
SVN_ERR_FS_NOT_ID = _core.SVN_ERR_FS_NOT_ID
SVN_ERR_FS_NOT_DIRECTORY = _core.SVN_ERR_FS_NOT_DIRECTORY
SVN_ERR_FS_NOT_FILE = _core.SVN_ERR_FS_NOT_FILE
SVN_ERR_FS_NOT_SINGLE_PATH_COMPONENT = _core.SVN_ERR_FS_NOT_SINGLE_PATH_COMPONENT
SVN_ERR_FS_NOT_MUTABLE = _core.SVN_ERR_FS_NOT_MUTABLE
SVN_ERR_FS_ALREADY_EXISTS = _core.SVN_ERR_FS_ALREADY_EXISTS
SVN_ERR_FS_ROOT_DIR = _core.SVN_ERR_FS_ROOT_DIR
SVN_ERR_FS_NOT_TXN_ROOT = _core.SVN_ERR_FS_NOT_TXN_ROOT
SVN_ERR_FS_NOT_REVISION_ROOT = _core.SVN_ERR_FS_NOT_REVISION_ROOT
SVN_ERR_FS_CONFLICT = _core.SVN_ERR_FS_CONFLICT
SVN_ERR_FS_REP_CHANGED = _core.SVN_ERR_FS_REP_CHANGED
SVN_ERR_FS_REP_NOT_MUTABLE = _core.SVN_ERR_FS_REP_NOT_MUTABLE
SVN_ERR_FS_MALFORMED_SKEL = _core.SVN_ERR_FS_MALFORMED_SKEL
SVN_ERR_FS_TXN_OUT_OF_DATE = _core.SVN_ERR_FS_TXN_OUT_OF_DATE
SVN_ERR_FS_BERKELEY_DB = _core.SVN_ERR_FS_BERKELEY_DB
SVN_ERR_FS_BERKELEY_DB_DEADLOCK = _core.SVN_ERR_FS_BERKELEY_DB_DEADLOCK
SVN_ERR_FS_TRANSACTION_DEAD = _core.SVN_ERR_FS_TRANSACTION_DEAD
SVN_ERR_FS_TRANSACTION_NOT_DEAD = _core.SVN_ERR_FS_TRANSACTION_NOT_DEAD
SVN_ERR_FS_UNKNOWN_FS_TYPE = _core.SVN_ERR_FS_UNKNOWN_FS_TYPE
SVN_ERR_FS_NO_USER = _core.SVN_ERR_FS_NO_USER
SVN_ERR_FS_PATH_ALREADY_LOCKED = _core.SVN_ERR_FS_PATH_ALREADY_LOCKED
SVN_ERR_FS_PATH_NOT_LOCKED = _core.SVN_ERR_FS_PATH_NOT_LOCKED
SVN_ERR_FS_BAD_LOCK_TOKEN = _core.SVN_ERR_FS_BAD_LOCK_TOKEN
SVN_ERR_FS_NO_LOCK_TOKEN = _core.SVN_ERR_FS_NO_LOCK_TOKEN
SVN_ERR_FS_LOCK_OWNER_MISMATCH = _core.SVN_ERR_FS_LOCK_OWNER_MISMATCH
SVN_ERR_FS_NO_SUCH_LOCK = _core.SVN_ERR_FS_NO_SUCH_LOCK
SVN_ERR_FS_LOCK_EXPIRED = _core.SVN_ERR_FS_LOCK_EXPIRED
SVN_ERR_FS_OUT_OF_DATE = _core.SVN_ERR_FS_OUT_OF_DATE
SVN_ERR_FS_UNSUPPORTED_FORMAT = _core.SVN_ERR_FS_UNSUPPORTED_FORMAT
SVN_ERR_REPOS_LOCKED = _core.SVN_ERR_REPOS_LOCKED
SVN_ERR_REPOS_HOOK_FAILURE = _core.SVN_ERR_REPOS_HOOK_FAILURE
SVN_ERR_REPOS_BAD_ARGS = _core.SVN_ERR_REPOS_BAD_ARGS
SVN_ERR_REPOS_NO_DATA_FOR_REPORT = _core.SVN_ERR_REPOS_NO_DATA_FOR_REPORT
SVN_ERR_REPOS_BAD_REVISION_REPORT = _core.SVN_ERR_REPOS_BAD_REVISION_REPORT
SVN_ERR_REPOS_UNSUPPORTED_VERSION = _core.SVN_ERR_REPOS_UNSUPPORTED_VERSION
SVN_ERR_REPOS_DISABLED_FEATURE = _core.SVN_ERR_REPOS_DISABLED_FEATURE
SVN_ERR_REPOS_POST_COMMIT_HOOK_FAILED = _core.SVN_ERR_REPOS_POST_COMMIT_HOOK_FAILED
SVN_ERR_REPOS_POST_LOCK_HOOK_FAILED = _core.SVN_ERR_REPOS_POST_LOCK_HOOK_FAILED
SVN_ERR_REPOS_POST_UNLOCK_HOOK_FAILED = _core.SVN_ERR_REPOS_POST_UNLOCK_HOOK_FAILED
SVN_ERR_RA_ILLEGAL_URL = _core.SVN_ERR_RA_ILLEGAL_URL
SVN_ERR_RA_NOT_AUTHORIZED = _core.SVN_ERR_RA_NOT_AUTHORIZED
SVN_ERR_RA_UNKNOWN_AUTH = _core.SVN_ERR_RA_UNKNOWN_AUTH
SVN_ERR_RA_NOT_IMPLEMENTED = _core.SVN_ERR_RA_NOT_IMPLEMENTED
SVN_ERR_RA_OUT_OF_DATE = _core.SVN_ERR_RA_OUT_OF_DATE
SVN_ERR_RA_NO_REPOS_UUID = _core.SVN_ERR_RA_NO_REPOS_UUID
SVN_ERR_RA_UNSUPPORTED_ABI_VERSION = _core.SVN_ERR_RA_UNSUPPORTED_ABI_VERSION
SVN_ERR_RA_NOT_LOCKED = _core.SVN_ERR_RA_NOT_LOCKED
SVN_ERR_RA_DAV_SOCK_INIT = _core.SVN_ERR_RA_DAV_SOCK_INIT
SVN_ERR_RA_DAV_CREATING_REQUEST = _core.SVN_ERR_RA_DAV_CREATING_REQUEST
SVN_ERR_RA_DAV_REQUEST_FAILED = _core.SVN_ERR_RA_DAV_REQUEST_FAILED
SVN_ERR_RA_DAV_OPTIONS_REQ_FAILED = _core.SVN_ERR_RA_DAV_OPTIONS_REQ_FAILED
SVN_ERR_RA_DAV_PROPS_NOT_FOUND = _core.SVN_ERR_RA_DAV_PROPS_NOT_FOUND
SVN_ERR_RA_DAV_ALREADY_EXISTS = _core.SVN_ERR_RA_DAV_ALREADY_EXISTS
SVN_ERR_RA_DAV_INVALID_CONFIG_VALUE = _core.SVN_ERR_RA_DAV_INVALID_CONFIG_VALUE
SVN_ERR_RA_DAV_PATH_NOT_FOUND = _core.SVN_ERR_RA_DAV_PATH_NOT_FOUND
SVN_ERR_RA_DAV_PROPPATCH_FAILED = _core.SVN_ERR_RA_DAV_PROPPATCH_FAILED
SVN_ERR_RA_DAV_MALFORMED_DATA = _core.SVN_ERR_RA_DAV_MALFORMED_DATA
SVN_ERR_RA_DAV_RESPONSE_HEADER_BADNESS = _core.SVN_ERR_RA_DAV_RESPONSE_HEADER_BADNESS
SVN_ERR_RA_LOCAL_REPOS_NOT_FOUND = _core.SVN_ERR_RA_LOCAL_REPOS_NOT_FOUND
SVN_ERR_RA_LOCAL_REPOS_OPEN_FAILED = _core.SVN_ERR_RA_LOCAL_REPOS_OPEN_FAILED
SVN_ERR_RA_SVN_CMD_ERR = _core.SVN_ERR_RA_SVN_CMD_ERR
SVN_ERR_RA_SVN_UNKNOWN_CMD = _core.SVN_ERR_RA_SVN_UNKNOWN_CMD
SVN_ERR_RA_SVN_CONNECTION_CLOSED = _core.SVN_ERR_RA_SVN_CONNECTION_CLOSED
SVN_ERR_RA_SVN_IO_ERROR = _core.SVN_ERR_RA_SVN_IO_ERROR
SVN_ERR_RA_SVN_MALFORMED_DATA = _core.SVN_ERR_RA_SVN_MALFORMED_DATA
SVN_ERR_RA_SVN_REPOS_NOT_FOUND = _core.SVN_ERR_RA_SVN_REPOS_NOT_FOUND
SVN_ERR_RA_SVN_BAD_VERSION = _core.SVN_ERR_RA_SVN_BAD_VERSION
SVN_ERR_AUTHN_CREDS_UNAVAILABLE = _core.SVN_ERR_AUTHN_CREDS_UNAVAILABLE
SVN_ERR_AUTHN_NO_PROVIDER = _core.SVN_ERR_AUTHN_NO_PROVIDER
SVN_ERR_AUTHN_PROVIDERS_EXHAUSTED = _core.SVN_ERR_AUTHN_PROVIDERS_EXHAUSTED
SVN_ERR_AUTHN_CREDS_NOT_SAVED = _core.SVN_ERR_AUTHN_CREDS_NOT_SAVED
SVN_ERR_AUTHZ_ROOT_UNREADABLE = _core.SVN_ERR_AUTHZ_ROOT_UNREADABLE
SVN_ERR_AUTHZ_UNREADABLE = _core.SVN_ERR_AUTHZ_UNREADABLE
SVN_ERR_AUTHZ_PARTIALLY_READABLE = _core.SVN_ERR_AUTHZ_PARTIALLY_READABLE
SVN_ERR_AUTHZ_INVALID_CONFIG = _core.SVN_ERR_AUTHZ_INVALID_CONFIG
SVN_ERR_AUTHZ_UNWRITABLE = _core.SVN_ERR_AUTHZ_UNWRITABLE
SVN_ERR_SVNDIFF_INVALID_HEADER = _core.SVN_ERR_SVNDIFF_INVALID_HEADER
SVN_ERR_SVNDIFF_CORRUPT_WINDOW = _core.SVN_ERR_SVNDIFF_CORRUPT_WINDOW
SVN_ERR_SVNDIFF_BACKWARD_VIEW = _core.SVN_ERR_SVNDIFF_BACKWARD_VIEW
SVN_ERR_SVNDIFF_INVALID_OPS = _core.SVN_ERR_SVNDIFF_INVALID_OPS
SVN_ERR_SVNDIFF_UNEXPECTED_END = _core.SVN_ERR_SVNDIFF_UNEXPECTED_END
SVN_ERR_SVNDIFF_INVALID_COMPRESSED_DATA = _core.SVN_ERR_SVNDIFF_INVALID_COMPRESSED_DATA
SVN_ERR_DIFF_DATASOURCE_MODIFIED = _core.SVN_ERR_DIFF_DATASOURCE_MODIFIED
SVN_ERR_APMOD_MISSING_PATH_TO_FS = _core.SVN_ERR_APMOD_MISSING_PATH_TO_FS
SVN_ERR_APMOD_MALFORMED_URI = _core.SVN_ERR_APMOD_MALFORMED_URI
SVN_ERR_APMOD_ACTIVITY_NOT_FOUND = _core.SVN_ERR_APMOD_ACTIVITY_NOT_FOUND
SVN_ERR_APMOD_BAD_BASELINE = _core.SVN_ERR_APMOD_BAD_BASELINE
SVN_ERR_APMOD_CONNECTION_ABORTED = _core.SVN_ERR_APMOD_CONNECTION_ABORTED
SVN_ERR_CLIENT_VERSIONED_PATH_REQUIRED = _core.SVN_ERR_CLIENT_VERSIONED_PATH_REQUIRED
SVN_ERR_CLIENT_RA_ACCESS_REQUIRED = _core.SVN_ERR_CLIENT_RA_ACCESS_REQUIRED
SVN_ERR_CLIENT_BAD_REVISION = _core.SVN_ERR_CLIENT_BAD_REVISION
SVN_ERR_CLIENT_DUPLICATE_COMMIT_URL = _core.SVN_ERR_CLIENT_DUPLICATE_COMMIT_URL
SVN_ERR_CLIENT_IS_BINARY_FILE = _core.SVN_ERR_CLIENT_IS_BINARY_FILE
SVN_ERR_CLIENT_INVALID_EXTERNALS_DESCRIPTION = _core.SVN_ERR_CLIENT_INVALID_EXTERNALS_DESCRIPTION
SVN_ERR_CLIENT_MODIFIED = _core.SVN_ERR_CLIENT_MODIFIED
SVN_ERR_CLIENT_IS_DIRECTORY = _core.SVN_ERR_CLIENT_IS_DIRECTORY
SVN_ERR_CLIENT_REVISION_RANGE = _core.SVN_ERR_CLIENT_REVISION_RANGE
SVN_ERR_CLIENT_INVALID_RELOCATION = _core.SVN_ERR_CLIENT_INVALID_RELOCATION
SVN_ERR_CLIENT_REVISION_AUTHOR_CONTAINS_NEWLINE = _core.SVN_ERR_CLIENT_REVISION_AUTHOR_CONTAINS_NEWLINE
SVN_ERR_CLIENT_PROPERTY_NAME = _core.SVN_ERR_CLIENT_PROPERTY_NAME
SVN_ERR_CLIENT_UNRELATED_RESOURCES = _core.SVN_ERR_CLIENT_UNRELATED_RESOURCES
SVN_ERR_CLIENT_MISSING_LOCK_TOKEN = _core.SVN_ERR_CLIENT_MISSING_LOCK_TOKEN
SVN_ERR_BASE = _core.SVN_ERR_BASE
SVN_ERR_PLUGIN_LOAD_FAILURE = _core.SVN_ERR_PLUGIN_LOAD_FAILURE
SVN_ERR_MALFORMED_FILE = _core.SVN_ERR_MALFORMED_FILE
SVN_ERR_INCOMPLETE_DATA = _core.SVN_ERR_INCOMPLETE_DATA
SVN_ERR_INCORRECT_PARAMS = _core.SVN_ERR_INCORRECT_PARAMS
SVN_ERR_UNVERSIONED_RESOURCE = _core.SVN_ERR_UNVERSIONED_RESOURCE
SVN_ERR_TEST_FAILED = _core.SVN_ERR_TEST_FAILED
SVN_ERR_UNSUPPORTED_FEATURE = _core.SVN_ERR_UNSUPPORTED_FEATURE
SVN_ERR_BAD_PROP_KIND = _core.SVN_ERR_BAD_PROP_KIND
SVN_ERR_ILLEGAL_TARGET = _core.SVN_ERR_ILLEGAL_TARGET
SVN_ERR_DELTA_MD5_CHECKSUM_ABSENT = _core.SVN_ERR_DELTA_MD5_CHECKSUM_ABSENT
SVN_ERR_DIR_NOT_EMPTY = _core.SVN_ERR_DIR_NOT_EMPTY
SVN_ERR_EXTERNAL_PROGRAM = _core.SVN_ERR_EXTERNAL_PROGRAM
SVN_ERR_SWIG_PY_EXCEPTION_SET = _core.SVN_ERR_SWIG_PY_EXCEPTION_SET
SVN_ERR_CHECKSUM_MISMATCH = _core.SVN_ERR_CHECKSUM_MISMATCH
SVN_ERR_CANCELLED = _core.SVN_ERR_CANCELLED
SVN_ERR_INVALID_DIFF_OPTION = _core.SVN_ERR_INVALID_DIFF_OPTION
SVN_ERR_PROPERTY_NOT_FOUND = _core.SVN_ERR_PROPERTY_NOT_FOUND
SVN_ERR_NO_AUTH_FILE_PATH = _core.SVN_ERR_NO_AUTH_FILE_PATH
SVN_ERR_VERSION_MISMATCH = _core.SVN_ERR_VERSION_MISMATCH
SVN_ERR_CL_ARG_PARSING_ERROR = _core.SVN_ERR_CL_ARG_PARSING_ERROR
SVN_ERR_CL_INSUFFICIENT_ARGS = _core.SVN_ERR_CL_INSUFFICIENT_ARGS
SVN_ERR_CL_MUTUALLY_EXCLUSIVE_ARGS = _core.SVN_ERR_CL_MUTUALLY_EXCLUSIVE_ARGS
SVN_ERR_CL_ADM_DIR_RESERVED = _core.SVN_ERR_CL_ADM_DIR_RESERVED
SVN_ERR_CL_LOG_MESSAGE_IS_VERSIONED_FILE = _core.SVN_ERR_CL_LOG_MESSAGE_IS_VERSIONED_FILE
SVN_ERR_CL_LOG_MESSAGE_IS_PATHNAME = _core.SVN_ERR_CL_LOG_MESSAGE_IS_PATHNAME
SVN_ERR_CL_COMMIT_IN_ADDED_DIR = _core.SVN_ERR_CL_COMMIT_IN_ADDED_DIR
SVN_ERR_CL_NO_EXTERNAL_EDITOR = _core.SVN_ERR_CL_NO_EXTERNAL_EDITOR
SVN_ERR_CL_BAD_LOG_MESSAGE = _core.SVN_ERR_CL_BAD_LOG_MESSAGE
SVN_ERR_CL_UNNECESSARY_LOG_MESSAGE = _core.SVN_ERR_CL_UNNECESSARY_LOG_MESSAGE
SVN_ERR_LAST = _core.SVN_ERR_LAST
SVN_NO_ERROR = _core.SVN_NO_ERROR

def svn_error__locate(*args):
    """svn_error__locate(char file, long line)"""
    return apply(_core.svn_error__locate, args)

def svn_strerror(*args):
    """svn_strerror(apr_status_t statcode, char buf, apr_size_t bufsize) -> char"""
    return apply(_core.svn_strerror, args)

def svn_err_best_message(*args):
    """svn_err_best_message(svn_error_t err, char buf, apr_size_t bufsize) -> char"""
    return apply(_core.svn_err_best_message, args)

def svn_error_create(*args):
    """svn_error_create(apr_status_t apr_err, svn_error_t child, char message) -> svn_error_t"""
    return apply(_core.svn_error_create, args)

def svn_error_createf(*args):
    """
    svn_error_createf(apr_status_t apr_err, svn_error_t child, char fmt, 
        v(...) ??) -> svn_error_t
    """
    return apply(_core.svn_error_createf, args)

def svn_error_wrap_apr(*args):
    """svn_error_wrap_apr(apr_status_t status, char fmt, v(...) ??) -> svn_error_t"""
    return apply(_core.svn_error_wrap_apr, args)

def svn_error_quick_wrap(*args):
    """svn_error_quick_wrap(svn_error_t child, char new_msg) -> svn_error_t"""
    return apply(_core.svn_error_quick_wrap, args)

def svn_error_compose(*args):
    """svn_error_compose(svn_error_t chain, svn_error_t new_err)"""
    return apply(_core.svn_error_compose, args)

def svn_error_dup(*args):
    """svn_error_dup(svn_error_t err) -> svn_error_t"""
    return apply(_core.svn_error_dup, args)

def svn_error_clear(*args):
    """svn_error_clear(svn_error_t error)"""
    return apply(_core.svn_error_clear, args)

def svn_handle_error2(*args):
    """
    svn_handle_error2(svn_error_t error, FILE stream, svn_boolean_t fatal, 
        char prefix)
    """
    return apply(_core.svn_handle_error2, args)

def svn_handle_error(*args):
    """svn_handle_error(svn_error_t error, FILE stream, svn_boolean_t fatal)"""
    return apply(_core.svn_handle_error, args)

def svn_handle_warning2(*args):
    """svn_handle_warning2(FILE stream, svn_error_t error, char prefix)"""
    return apply(_core.svn_handle_warning2, args)

def svn_handle_warning(*args):
    """svn_handle_warning(FILE stream, svn_error_t error)"""
    return apply(_core.svn_handle_warning, args)

def svn_time_to_cstring(*args):
    """svn_time_to_cstring(apr_time_t when, apr_pool_t pool) -> char"""
    return apply(_core.svn_time_to_cstring, args)

def svn_time_from_cstring(*args):
    """svn_time_from_cstring(apr_time_t when, char data, apr_pool_t pool) -> svn_error_t"""
    return apply(_core.svn_time_from_cstring, args)

def svn_time_to_human_cstring(*args):
    """svn_time_to_human_cstring(apr_time_t when, apr_pool_t pool) -> char"""
    return apply(_core.svn_time_to_human_cstring, args)

def svn_parse_date(*args):
    """
    svn_parse_date(svn_boolean_t matched, apr_time_t result, char text, 
        apr_time_t now, apr_pool_t pool) -> svn_error_t
    """
    return apply(_core.svn_parse_date, args)

def svn_sleep_for_timestamps(*args):
    """svn_sleep_for_timestamps()"""
    return apply(_core.svn_sleep_for_timestamps, args)
SWIG_SVN_INVALID_REVNUM = _core.SWIG_SVN_INVALID_REVNUM
SWIG_SVN_IGNORED_REVNUM = _core.SWIG_SVN_IGNORED_REVNUM

def apr_initialize(*args):
    """apr_initialize() -> apr_status_t"""
    return apply(_core.apr_initialize, args)

def apr_terminate(*args):
    """apr_terminate()"""
    return apply(_core.apr_terminate, args)

def apr_time_ansi_put(*args):
    """apr_time_ansi_put(apr_time_t result, time_t input) -> apr_status_t"""
    return apply(_core.apr_time_ansi_put, args)

def apr_pool_destroy(*args):
    """apr_pool_destroy(apr_pool_t p)"""
    return apply(_core.apr_pool_destroy, args)

def apr_pool_clear(*args):
    """apr_pool_clear(apr_pool_t p)"""
    return apply(_core.apr_pool_clear, args)

def apr_file_open_stdout(*args):
    """apr_file_open_stdout(apr_file_t out, apr_pool_t pool) -> apr_status_t"""
    return apply(_core.apr_file_open_stdout, args)

def apr_file_open_stderr(*args):
    """apr_file_open_stderr(apr_file_t out, apr_pool_t pool) -> apr_status_t"""
    return apply(_core.apr_file_open_stderr, args)

def svn_swig_py_exception_type(*args):
    """svn_swig_py_exception_type() -> PyObject"""
    return apply(_core.svn_swig_py_exception_type, args)
SVN_ALLOCATOR_RECOMMENDED_MAX_FREE = _core.SVN_ALLOCATOR_RECOMMENDED_MAX_FREE

def svn_pool_create(*args):
    """svn_pool_create(apr_pool_t parent_pool, apr_allocator_t allocator) -> apr_pool_t"""
    return apply(_core.svn_pool_create, args)
SVN_VER_MAJOR = _core.SVN_VER_MAJOR
SVN_VER_MINOR = _core.SVN_VER_MINOR
SVN_VER_PATCH = _core.SVN_VER_PATCH
SVN_VER_MICRO = _core.SVN_VER_MICRO
SVN_VER_LIBRARY = _core.SVN_VER_LIBRARY
SVN_VER_TAG = _core.SVN_VER_TAG
SVN_VER_NUMTAG = _core.SVN_VER_NUMTAG
SVN_VER_REVISION = _core.SVN_VER_REVISION
class svn_version_t:
    """Proxy of C svn_version_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, svn_version_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, svn_version_t, name)
    def __repr__(self):
        return "<%s.%s; proxy of C svn_version_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    __swig_setmethods__["major"] = _core.svn_version_t_major_set
    __swig_getmethods__["major"] = _core.svn_version_t_major_get
    __swig_setmethods__["minor"] = _core.svn_version_t_minor_set
    __swig_getmethods__["minor"] = _core.svn_version_t_minor_get
    __swig_setmethods__["patch"] = _core.svn_version_t_patch_set
    __swig_getmethods__["patch"] = _core.svn_version_t_patch_get
    __swig_setmethods__["tag"] = _core.svn_version_t_tag_set
    __swig_getmethods__["tag"] = _core.svn_version_t_tag_get
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for svn_version_t"""
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
        """__init__(self) -> svn_version_t"""
        _swig_setattr(self, svn_version_t, 'this', apply(_core.new_svn_version_t, args))
        _swig_setattr(self, svn_version_t, 'thisown', 1)
    def __del__(self, destroy=_core.delete_svn_version_t):
        """__del__(self)"""
        try:
            if self.thisown: destroy(self)
        except: pass


class svn_version_tPtr(svn_version_t):
    def __init__(self, this):
        _swig_setattr(self, svn_version_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, svn_version_t, 'thisown', 0)
        _swig_setattr(self, svn_version_t,self.__class__,svn_version_t)
_core.svn_version_t_swigregister(svn_version_tPtr)


def svn_ver_compatible(*args):
    """svn_ver_compatible(svn_version_t my_version, svn_version_t lib_version) -> svn_boolean_t"""
    return apply(_core.svn_ver_compatible, args)

def svn_ver_equal(*args):
    """svn_ver_equal(svn_version_t my_version, svn_version_t lib_version) -> svn_boolean_t"""
    return apply(_core.svn_ver_equal, args)
class svn_version_checklist_t:
    """Proxy of C svn_version_checklist_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, svn_version_checklist_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, svn_version_checklist_t, name)
    def __repr__(self):
        return "<%s.%s; proxy of C svn_version_checklist_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    __swig_setmethods__["label"] = _core.svn_version_checklist_t_label_set
    __swig_getmethods__["label"] = _core.svn_version_checklist_t_label_get
    __swig_setmethods__["version_query"] = _core.svn_version_checklist_t_version_query_set
    __swig_getmethods__["version_query"] = _core.svn_version_checklist_t_version_query_get
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for svn_version_checklist_t"""
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
        """__init__(self) -> svn_version_checklist_t"""
        _swig_setattr(self, svn_version_checklist_t, 'this', apply(_core.new_svn_version_checklist_t, args))
        _swig_setattr(self, svn_version_checklist_t, 'thisown', 1)
    def __del__(self, destroy=_core.delete_svn_version_checklist_t):
        """__del__(self)"""
        try:
            if self.thisown: destroy(self)
        except: pass


class svn_version_checklist_tPtr(svn_version_checklist_t):
    def __init__(self, this):
        _swig_setattr(self, svn_version_checklist_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, svn_version_checklist_t, 'thisown', 0)
        _swig_setattr(self, svn_version_checklist_t,self.__class__,svn_version_checklist_t)
_core.svn_version_checklist_t_swigregister(svn_version_checklist_tPtr)


def svn_ver_check_list(*args):
    """svn_ver_check_list(svn_version_t my_version, svn_version_checklist_t checklist) -> svn_error_t"""
    return apply(_core.svn_ver_check_list, args)

def svn_subr_version(*args):
    """svn_subr_version() -> svn_version_t"""
    return apply(_core.svn_subr_version, args)
SVN_OPT_MAX_ALIASES = _core.SVN_OPT_MAX_ALIASES
SVN_OPT_MAX_OPTIONS = _core.SVN_OPT_MAX_OPTIONS
SVN_OPT_FIRST_LONGOPT_ID = _core.SVN_OPT_FIRST_LONGOPT_ID
class svn_opt_subcommand_desc2_t:
    """Proxy of C svn_opt_subcommand_desc2_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, svn_opt_subcommand_desc2_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, svn_opt_subcommand_desc2_t, name)
    def __repr__(self):
        return "<%s.%s; proxy of C svn_opt_subcommand_desc2_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    __swig_setmethods__["name"] = _core.svn_opt_subcommand_desc2_t_name_set
    __swig_getmethods__["name"] = _core.svn_opt_subcommand_desc2_t_name_get
    __swig_setmethods__["cmd_func"] = _core.svn_opt_subcommand_desc2_t_cmd_func_set
    __swig_getmethods__["cmd_func"] = _core.svn_opt_subcommand_desc2_t_cmd_func_get
    __swig_setmethods__["aliases"] = _core.svn_opt_subcommand_desc2_t_aliases_set
    __swig_getmethods__["aliases"] = _core.svn_opt_subcommand_desc2_t_aliases_get
    __swig_setmethods__["help"] = _core.svn_opt_subcommand_desc2_t_help_set
    __swig_getmethods__["help"] = _core.svn_opt_subcommand_desc2_t_help_get
    __swig_setmethods__["valid_options"] = _core.svn_opt_subcommand_desc2_t_valid_options_set
    __swig_getmethods__["valid_options"] = _core.svn_opt_subcommand_desc2_t_valid_options_get
    __swig_getmethods__["desc_overrides"] = _core.svn_opt_subcommand_desc2_t_desc_overrides_get
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for svn_opt_subcommand_desc2_t"""
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
        """__init__(self) -> svn_opt_subcommand_desc2_t"""
        _swig_setattr(self, svn_opt_subcommand_desc2_t, 'this', apply(_core.new_svn_opt_subcommand_desc2_t, args))
        _swig_setattr(self, svn_opt_subcommand_desc2_t, 'thisown', 1)
    def __del__(self, destroy=_core.delete_svn_opt_subcommand_desc2_t):
        """__del__(self)"""
        try:
            if self.thisown: destroy(self)
        except: pass


class svn_opt_subcommand_desc2_tPtr(svn_opt_subcommand_desc2_t):
    def __init__(self, this):
        _swig_setattr(self, svn_opt_subcommand_desc2_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, svn_opt_subcommand_desc2_t, 'thisown', 0)
        _swig_setattr(self, svn_opt_subcommand_desc2_t,self.__class__,svn_opt_subcommand_desc2_t)
_core.svn_opt_subcommand_desc2_t_swigregister(svn_opt_subcommand_desc2_tPtr)

class svn_opt_subcommand_desc2_t_desc_overrides:
    """Proxy of C svn_opt_subcommand_desc2_t_desc_overrides struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, svn_opt_subcommand_desc2_t_desc_overrides, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, svn_opt_subcommand_desc2_t_desc_overrides, name)
    def __repr__(self):
        return "<%s.%s; proxy of C svn_opt_subcommand_desc2_t_desc_overrides instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    __swig_setmethods__["optch"] = _core.svn_opt_subcommand_desc2_t_desc_overrides_optch_set
    __swig_getmethods__["optch"] = _core.svn_opt_subcommand_desc2_t_desc_overrides_optch_get
    __swig_setmethods__["desc"] = _core.svn_opt_subcommand_desc2_t_desc_overrides_desc_set
    __swig_getmethods__["desc"] = _core.svn_opt_subcommand_desc2_t_desc_overrides_desc_get
    def __init__(self, *args):
        """__init__(self) -> svn_opt_subcommand_desc2_t_desc_overrides"""
        _swig_setattr(self, svn_opt_subcommand_desc2_t_desc_overrides, 'this', apply(_core.new_svn_opt_subcommand_desc2_t_desc_overrides, args))
        _swig_setattr(self, svn_opt_subcommand_desc2_t_desc_overrides, 'thisown', 1)
    def __del__(self, destroy=_core.delete_svn_opt_subcommand_desc2_t_desc_overrides):
        """__del__(self)"""
        try:
            if self.thisown: destroy(self)
        except: pass


class svn_opt_subcommand_desc2_t_desc_overridesPtr(svn_opt_subcommand_desc2_t_desc_overrides):
    def __init__(self, this):
        _swig_setattr(self, svn_opt_subcommand_desc2_t_desc_overrides, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, svn_opt_subcommand_desc2_t_desc_overrides, 'thisown', 0)
        _swig_setattr(self, svn_opt_subcommand_desc2_t_desc_overrides,self.__class__,svn_opt_subcommand_desc2_t_desc_overrides)
_core.svn_opt_subcommand_desc2_t_desc_overrides_swigregister(svn_opt_subcommand_desc2_t_desc_overridesPtr)

class svn_opt_subcommand_desc_t:
    """Proxy of C svn_opt_subcommand_desc_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, svn_opt_subcommand_desc_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, svn_opt_subcommand_desc_t, name)
    def __repr__(self):
        return "<%s.%s; proxy of C svn_opt_subcommand_desc_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    __swig_setmethods__["name"] = _core.svn_opt_subcommand_desc_t_name_set
    __swig_getmethods__["name"] = _core.svn_opt_subcommand_desc_t_name_get
    __swig_setmethods__["cmd_func"] = _core.svn_opt_subcommand_desc_t_cmd_func_set
    __swig_getmethods__["cmd_func"] = _core.svn_opt_subcommand_desc_t_cmd_func_get
    __swig_setmethods__["aliases"] = _core.svn_opt_subcommand_desc_t_aliases_set
    __swig_getmethods__["aliases"] = _core.svn_opt_subcommand_desc_t_aliases_get
    __swig_setmethods__["help"] = _core.svn_opt_subcommand_desc_t_help_set
    __swig_getmethods__["help"] = _core.svn_opt_subcommand_desc_t_help_get
    __swig_setmethods__["valid_options"] = _core.svn_opt_subcommand_desc_t_valid_options_set
    __swig_getmethods__["valid_options"] = _core.svn_opt_subcommand_desc_t_valid_options_get
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for svn_opt_subcommand_desc_t"""
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
        """__init__(self) -> svn_opt_subcommand_desc_t"""
        _swig_setattr(self, svn_opt_subcommand_desc_t, 'this', apply(_core.new_svn_opt_subcommand_desc_t, args))
        _swig_setattr(self, svn_opt_subcommand_desc_t, 'thisown', 1)
    def __del__(self, destroy=_core.delete_svn_opt_subcommand_desc_t):
        """__del__(self)"""
        try:
            if self.thisown: destroy(self)
        except: pass


class svn_opt_subcommand_desc_tPtr(svn_opt_subcommand_desc_t):
    def __init__(self, this):
        _swig_setattr(self, svn_opt_subcommand_desc_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, svn_opt_subcommand_desc_t, 'thisown', 0)
        _swig_setattr(self, svn_opt_subcommand_desc_t,self.__class__,svn_opt_subcommand_desc_t)
_core.svn_opt_subcommand_desc_t_swigregister(svn_opt_subcommand_desc_tPtr)


def svn_opt_get_canonical_subcommand2(*args):
    """svn_opt_get_canonical_subcommand2(svn_opt_subcommand_desc2_t table, char cmd_name) -> svn_opt_subcommand_desc2_t"""
    return apply(_core.svn_opt_get_canonical_subcommand2, args)

def svn_opt_get_canonical_subcommand(*args):
    """svn_opt_get_canonical_subcommand(svn_opt_subcommand_desc_t table, char cmd_name) -> svn_opt_subcommand_desc_t"""
    return apply(_core.svn_opt_get_canonical_subcommand, args)

def svn_opt_get_option_from_code2(*args):
    """
    svn_opt_get_option_from_code2(int code, apr_getopt_option_t option_table, svn_opt_subcommand_desc2_t command, 
        apr_pool_t pool) -> apr_getopt_option_t
    """
    return apply(_core.svn_opt_get_option_from_code2, args)

def svn_opt_get_option_from_code(*args):
    """svn_opt_get_option_from_code(int code, apr_getopt_option_t option_table) -> apr_getopt_option_t"""
    return apply(_core.svn_opt_get_option_from_code, args)

def svn_opt_subcommand_takes_option2(*args):
    """svn_opt_subcommand_takes_option2(svn_opt_subcommand_desc2_t command, int option_code) -> svn_boolean_t"""
    return apply(_core.svn_opt_subcommand_takes_option2, args)

def svn_opt_subcommand_takes_option(*args):
    """svn_opt_subcommand_takes_option(svn_opt_subcommand_desc_t command, int option_code) -> svn_boolean_t"""
    return apply(_core.svn_opt_subcommand_takes_option, args)

def svn_opt_print_generic_help2(*args):
    """
    svn_opt_print_generic_help2(char header, svn_opt_subcommand_desc2_t cmd_table, 
        apr_getopt_option_t opt_table, char footer, apr_pool_t pool, 
        FILE stream)
    """
    return apply(_core.svn_opt_print_generic_help2, args)

def svn_opt_format_option(*args):
    """
    svn_opt_format_option(char string, apr_getopt_option_t opt, svn_boolean_t doc, 
        apr_pool_t pool)
    """
    return apply(_core.svn_opt_format_option, args)

def svn_opt_subcommand_help2(*args):
    """
    svn_opt_subcommand_help2(char subcommand, svn_opt_subcommand_desc2_t table, 
        apr_getopt_option_t options_table, apr_pool_t pool)
    """
    return apply(_core.svn_opt_subcommand_help2, args)

def svn_opt_subcommand_help(*args):
    """
    svn_opt_subcommand_help(char subcommand, svn_opt_subcommand_desc_t table, apr_getopt_option_t options_table, 
        apr_pool_t pool)
    """
    return apply(_core.svn_opt_subcommand_help, args)
svn_opt_revision_unspecified = _core.svn_opt_revision_unspecified
svn_opt_revision_number = _core.svn_opt_revision_number
svn_opt_revision_date = _core.svn_opt_revision_date
svn_opt_revision_committed = _core.svn_opt_revision_committed
svn_opt_revision_previous = _core.svn_opt_revision_previous
svn_opt_revision_base = _core.svn_opt_revision_base
svn_opt_revision_working = _core.svn_opt_revision_working
svn_opt_revision_head = _core.svn_opt_revision_head
class svn_opt_revision_value_t:
    """Proxy of C svn_opt_revision_value_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, svn_opt_revision_value_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, svn_opt_revision_value_t, name)
    def __repr__(self):
        return "<%s.%s; proxy of C svn_opt_revision_value_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    __swig_setmethods__["number"] = _core.svn_opt_revision_value_t_number_set
    __swig_getmethods__["number"] = _core.svn_opt_revision_value_t_number_get
    __swig_setmethods__["date"] = _core.svn_opt_revision_value_t_date_set
    __swig_getmethods__["date"] = _core.svn_opt_revision_value_t_date_get
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for svn_opt_revision_value_t"""
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
        """__init__(self) -> svn_opt_revision_value_t"""
        _swig_setattr(self, svn_opt_revision_value_t, 'this', apply(_core.new_svn_opt_revision_value_t, args))
        _swig_setattr(self, svn_opt_revision_value_t, 'thisown', 1)
    def __del__(self, destroy=_core.delete_svn_opt_revision_value_t):
        """__del__(self)"""
        try:
            if self.thisown: destroy(self)
        except: pass


class svn_opt_revision_value_tPtr(svn_opt_revision_value_t):
    def __init__(self, this):
        _swig_setattr(self, svn_opt_revision_value_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, svn_opt_revision_value_t, 'thisown', 0)
        _swig_setattr(self, svn_opt_revision_value_t,self.__class__,svn_opt_revision_value_t)
_core.svn_opt_revision_value_t_swigregister(svn_opt_revision_value_tPtr)

class svn_opt_revision_t:
    """Proxy of C svn_opt_revision_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, svn_opt_revision_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, svn_opt_revision_t, name)
    def __repr__(self):
        return "<%s.%s; proxy of C svn_opt_revision_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    __swig_setmethods__["kind"] = _core.svn_opt_revision_t_kind_set
    __swig_getmethods__["kind"] = _core.svn_opt_revision_t_kind_get
    __swig_setmethods__["value"] = _core.svn_opt_revision_t_value_set
    __swig_getmethods__["value"] = _core.svn_opt_revision_t_value_get
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for svn_opt_revision_t"""
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
        """__init__(self) -> svn_opt_revision_t"""
        _swig_setattr(self, svn_opt_revision_t, 'this', apply(_core.new_svn_opt_revision_t, args))
        _swig_setattr(self, svn_opt_revision_t, 'thisown', 1)
    def __del__(self, destroy=_core.delete_svn_opt_revision_t):
        """__del__(self)"""
        try:
            if self.thisown: destroy(self)
        except: pass


class svn_opt_revision_tPtr(svn_opt_revision_t):
    def __init__(self, this):
        _swig_setattr(self, svn_opt_revision_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, svn_opt_revision_t, 'thisown', 0)
        _swig_setattr(self, svn_opt_revision_t,self.__class__,svn_opt_revision_t)
_core.svn_opt_revision_t_swigregister(svn_opt_revision_tPtr)


def svn_opt_parse_revision(*args):
    """
    svn_opt_parse_revision(svn_opt_revision_t start_revision, svn_opt_revision_t end_revision, 
        char arg, apr_pool_t pool) -> int
    """
    return apply(_core.svn_opt_parse_revision, args)

def svn_opt_args_to_target_array2(*args):
    """
    svn_opt_args_to_target_array2(apr_array_header_t targets_p, apr_getopt_t os, apr_array_header_t known_targets, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_core.svn_opt_args_to_target_array2, args)

def svn_opt_args_to_target_array(*args):
    """
    svn_opt_args_to_target_array(apr_array_header_t targets_p, apr_getopt_t os, apr_array_header_t known_targets, 
        svn_opt_revision_t start_revision, 
        svn_opt_revision_t end_revision, 
        svn_boolean_t extract_revisions, apr_pool_t pool) -> svn_error_t
    """
    return apply(_core.svn_opt_args_to_target_array, args)

def svn_opt_push_implicit_dot_target(*args):
    """svn_opt_push_implicit_dot_target(apr_array_header_t targets, apr_pool_t pool)"""
    return apply(_core.svn_opt_push_implicit_dot_target, args)

def svn_opt_parse_num_args(*args):
    """
    svn_opt_parse_num_args(apr_array_header_t args_p, apr_getopt_t os, int num_args, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_core.svn_opt_parse_num_args, args)

def svn_opt_parse_all_args(*args):
    """svn_opt_parse_all_args(apr_array_header_t args_p, apr_getopt_t os, apr_pool_t pool) -> svn_error_t"""
    return apply(_core.svn_opt_parse_all_args, args)

def svn_opt_parse_path(*args):
    """svn_opt_parse_path(svn_opt_revision_t rev, char truepath, char path, apr_pool_t pool) -> svn_error_t"""
    return apply(_core.svn_opt_parse_path, args)

def svn_opt_print_help2(*args):
    """
    svn_opt_print_help2(apr_getopt_t os, char pgm_name, svn_boolean_t print_version, 
        svn_boolean_t quiet, char version_footer, 
        char header, svn_opt_subcommand_desc2_t cmd_table, 
        apr_getopt_option_t option_table, 
        char footer, apr_pool_t pool) -> svn_error_t
    """
    return apply(_core.svn_opt_print_help2, args)

def svn_opt_print_help(*args):
    """
    svn_opt_print_help(apr_getopt_t os, char pgm_name, svn_boolean_t print_version, 
        svn_boolean_t quiet, char version_footer, 
        char header, svn_opt_subcommand_desc_t cmd_table, 
        apr_getopt_option_t option_table, 
        char footer, apr_pool_t pool) -> svn_error_t
    """
    return apply(_core.svn_opt_print_help, args)
class svn_auth_provider_t:
    """Proxy of C svn_auth_provider_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, svn_auth_provider_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, svn_auth_provider_t, name)
    def __repr__(self):
        return "<%s.%s; proxy of C svn_auth_provider_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    __swig_setmethods__["cred_kind"] = _core.svn_auth_provider_t_cred_kind_set
    __swig_getmethods__["cred_kind"] = _core.svn_auth_provider_t_cred_kind_get
    __swig_setmethods__["first_credentials"] = _core.svn_auth_provider_t_first_credentials_set
    __swig_getmethods__["first_credentials"] = _core.svn_auth_provider_t_first_credentials_get
    __swig_setmethods__["next_credentials"] = _core.svn_auth_provider_t_next_credentials_set
    __swig_getmethods__["next_credentials"] = _core.svn_auth_provider_t_next_credentials_get
    __swig_setmethods__["save_credentials"] = _core.svn_auth_provider_t_save_credentials_set
    __swig_getmethods__["save_credentials"] = _core.svn_auth_provider_t_save_credentials_get
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for svn_auth_provider_t"""
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
        """__init__(self) -> svn_auth_provider_t"""
        _swig_setattr(self, svn_auth_provider_t, 'this', apply(_core.new_svn_auth_provider_t, args))
        _swig_setattr(self, svn_auth_provider_t, 'thisown', 1)
    def __del__(self, destroy=_core.delete_svn_auth_provider_t):
        """__del__(self)"""
        try:
            if self.thisown: destroy(self)
        except: pass


class svn_auth_provider_tPtr(svn_auth_provider_t):
    def __init__(self, this):
        _swig_setattr(self, svn_auth_provider_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, svn_auth_provider_t, 'thisown', 0)
        _swig_setattr(self, svn_auth_provider_t,self.__class__,svn_auth_provider_t)
_core.svn_auth_provider_t_swigregister(svn_auth_provider_tPtr)

class svn_auth_provider_object_t:
    """Proxy of C svn_auth_provider_object_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, svn_auth_provider_object_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, svn_auth_provider_object_t, name)
    def __repr__(self):
        return "<%s.%s; proxy of C svn_auth_provider_object_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    __swig_setmethods__["vtable"] = _core.svn_auth_provider_object_t_vtable_set
    __swig_getmethods__["vtable"] = _core.svn_auth_provider_object_t_vtable_get
    __swig_setmethods__["provider_baton"] = _core.svn_auth_provider_object_t_provider_baton_set
    __swig_getmethods__["provider_baton"] = _core.svn_auth_provider_object_t_provider_baton_get
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for svn_auth_provider_object_t"""
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
        """__init__(self) -> svn_auth_provider_object_t"""
        _swig_setattr(self, svn_auth_provider_object_t, 'this', apply(_core.new_svn_auth_provider_object_t, args))
        _swig_setattr(self, svn_auth_provider_object_t, 'thisown', 1)
    def __del__(self, destroy=_core.delete_svn_auth_provider_object_t):
        """__del__(self)"""
        try:
            if self.thisown: destroy(self)
        except: pass


class svn_auth_provider_object_tPtr(svn_auth_provider_object_t):
    def __init__(self, this):
        _swig_setattr(self, svn_auth_provider_object_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, svn_auth_provider_object_t, 'thisown', 0)
        _swig_setattr(self, svn_auth_provider_object_t,self.__class__,svn_auth_provider_object_t)
_core.svn_auth_provider_object_t_swigregister(svn_auth_provider_object_tPtr)

SVN_AUTH_CRED_SIMPLE = _core.SVN_AUTH_CRED_SIMPLE
class svn_auth_cred_simple_t:
    """Proxy of C svn_auth_cred_simple_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, svn_auth_cred_simple_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, svn_auth_cred_simple_t, name)
    def __repr__(self):
        return "<%s.%s; proxy of C svn_auth_cred_simple_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    __swig_setmethods__["username"] = _core.svn_auth_cred_simple_t_username_set
    __swig_getmethods__["username"] = _core.svn_auth_cred_simple_t_username_get
    __swig_setmethods__["password"] = _core.svn_auth_cred_simple_t_password_set
    __swig_getmethods__["password"] = _core.svn_auth_cred_simple_t_password_get
    __swig_setmethods__["may_save"] = _core.svn_auth_cred_simple_t_may_save_set
    __swig_getmethods__["may_save"] = _core.svn_auth_cred_simple_t_may_save_get
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for svn_auth_cred_simple_t"""
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
        """__init__(self) -> svn_auth_cred_simple_t"""
        _swig_setattr(self, svn_auth_cred_simple_t, 'this', apply(_core.new_svn_auth_cred_simple_t, args))
        _swig_setattr(self, svn_auth_cred_simple_t, 'thisown', 1)
    def __del__(self, destroy=_core.delete_svn_auth_cred_simple_t):
        """__del__(self)"""
        try:
            if self.thisown: destroy(self)
        except: pass


class svn_auth_cred_simple_tPtr(svn_auth_cred_simple_t):
    def __init__(self, this):
        _swig_setattr(self, svn_auth_cred_simple_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, svn_auth_cred_simple_t, 'thisown', 0)
        _swig_setattr(self, svn_auth_cred_simple_t,self.__class__,svn_auth_cred_simple_t)
_core.svn_auth_cred_simple_t_swigregister(svn_auth_cred_simple_tPtr)

SVN_AUTH_CRED_USERNAME = _core.SVN_AUTH_CRED_USERNAME
class svn_auth_cred_username_t:
    """Proxy of C svn_auth_cred_username_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, svn_auth_cred_username_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, svn_auth_cred_username_t, name)
    def __repr__(self):
        return "<%s.%s; proxy of C svn_auth_cred_username_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    __swig_setmethods__["username"] = _core.svn_auth_cred_username_t_username_set
    __swig_getmethods__["username"] = _core.svn_auth_cred_username_t_username_get
    __swig_setmethods__["may_save"] = _core.svn_auth_cred_username_t_may_save_set
    __swig_getmethods__["may_save"] = _core.svn_auth_cred_username_t_may_save_get
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for svn_auth_cred_username_t"""
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
        """__init__(self) -> svn_auth_cred_username_t"""
        _swig_setattr(self, svn_auth_cred_username_t, 'this', apply(_core.new_svn_auth_cred_username_t, args))
        _swig_setattr(self, svn_auth_cred_username_t, 'thisown', 1)
    def __del__(self, destroy=_core.delete_svn_auth_cred_username_t):
        """__del__(self)"""
        try:
            if self.thisown: destroy(self)
        except: pass


class svn_auth_cred_username_tPtr(svn_auth_cred_username_t):
    def __init__(self, this):
        _swig_setattr(self, svn_auth_cred_username_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, svn_auth_cred_username_t, 'thisown', 0)
        _swig_setattr(self, svn_auth_cred_username_t,self.__class__,svn_auth_cred_username_t)
_core.svn_auth_cred_username_t_swigregister(svn_auth_cred_username_tPtr)

SVN_AUTH_CRED_SSL_CLIENT_CERT = _core.SVN_AUTH_CRED_SSL_CLIENT_CERT
class svn_auth_cred_ssl_client_cert_t:
    """Proxy of C svn_auth_cred_ssl_client_cert_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, svn_auth_cred_ssl_client_cert_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, svn_auth_cred_ssl_client_cert_t, name)
    def __repr__(self):
        return "<%s.%s; proxy of C svn_auth_cred_ssl_client_cert_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    __swig_setmethods__["cert_file"] = _core.svn_auth_cred_ssl_client_cert_t_cert_file_set
    __swig_getmethods__["cert_file"] = _core.svn_auth_cred_ssl_client_cert_t_cert_file_get
    __swig_setmethods__["may_save"] = _core.svn_auth_cred_ssl_client_cert_t_may_save_set
    __swig_getmethods__["may_save"] = _core.svn_auth_cred_ssl_client_cert_t_may_save_get
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for svn_auth_cred_ssl_client_cert_t"""
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
        """__init__(self) -> svn_auth_cred_ssl_client_cert_t"""
        _swig_setattr(self, svn_auth_cred_ssl_client_cert_t, 'this', apply(_core.new_svn_auth_cred_ssl_client_cert_t, args))
        _swig_setattr(self, svn_auth_cred_ssl_client_cert_t, 'thisown', 1)
    def __del__(self, destroy=_core.delete_svn_auth_cred_ssl_client_cert_t):
        """__del__(self)"""
        try:
            if self.thisown: destroy(self)
        except: pass


class svn_auth_cred_ssl_client_cert_tPtr(svn_auth_cred_ssl_client_cert_t):
    def __init__(self, this):
        _swig_setattr(self, svn_auth_cred_ssl_client_cert_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, svn_auth_cred_ssl_client_cert_t, 'thisown', 0)
        _swig_setattr(self, svn_auth_cred_ssl_client_cert_t,self.__class__,svn_auth_cred_ssl_client_cert_t)
_core.svn_auth_cred_ssl_client_cert_t_swigregister(svn_auth_cred_ssl_client_cert_tPtr)

SVN_AUTH_CRED_SSL_CLIENT_CERT_PW = _core.SVN_AUTH_CRED_SSL_CLIENT_CERT_PW
class svn_auth_cred_ssl_client_cert_pw_t:
    """Proxy of C svn_auth_cred_ssl_client_cert_pw_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, svn_auth_cred_ssl_client_cert_pw_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, svn_auth_cred_ssl_client_cert_pw_t, name)
    def __repr__(self):
        return "<%s.%s; proxy of C svn_auth_cred_ssl_client_cert_pw_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    __swig_setmethods__["password"] = _core.svn_auth_cred_ssl_client_cert_pw_t_password_set
    __swig_getmethods__["password"] = _core.svn_auth_cred_ssl_client_cert_pw_t_password_get
    __swig_setmethods__["may_save"] = _core.svn_auth_cred_ssl_client_cert_pw_t_may_save_set
    __swig_getmethods__["may_save"] = _core.svn_auth_cred_ssl_client_cert_pw_t_may_save_get
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for svn_auth_cred_ssl_client_cert_pw_t"""
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
        """__init__(self) -> svn_auth_cred_ssl_client_cert_pw_t"""
        _swig_setattr(self, svn_auth_cred_ssl_client_cert_pw_t, 'this', apply(_core.new_svn_auth_cred_ssl_client_cert_pw_t, args))
        _swig_setattr(self, svn_auth_cred_ssl_client_cert_pw_t, 'thisown', 1)
    def __del__(self, destroy=_core.delete_svn_auth_cred_ssl_client_cert_pw_t):
        """__del__(self)"""
        try:
            if self.thisown: destroy(self)
        except: pass


class svn_auth_cred_ssl_client_cert_pw_tPtr(svn_auth_cred_ssl_client_cert_pw_t):
    def __init__(self, this):
        _swig_setattr(self, svn_auth_cred_ssl_client_cert_pw_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, svn_auth_cred_ssl_client_cert_pw_t, 'thisown', 0)
        _swig_setattr(self, svn_auth_cred_ssl_client_cert_pw_t,self.__class__,svn_auth_cred_ssl_client_cert_pw_t)
_core.svn_auth_cred_ssl_client_cert_pw_t_swigregister(svn_auth_cred_ssl_client_cert_pw_tPtr)

SVN_AUTH_CRED_SSL_SERVER_TRUST = _core.SVN_AUTH_CRED_SSL_SERVER_TRUST
class svn_auth_ssl_server_cert_info_t:
    """Proxy of C svn_auth_ssl_server_cert_info_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, svn_auth_ssl_server_cert_info_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, svn_auth_ssl_server_cert_info_t, name)
    def __repr__(self):
        return "<%s.%s; proxy of C svn_auth_ssl_server_cert_info_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    __swig_setmethods__["hostname"] = _core.svn_auth_ssl_server_cert_info_t_hostname_set
    __swig_getmethods__["hostname"] = _core.svn_auth_ssl_server_cert_info_t_hostname_get
    __swig_setmethods__["fingerprint"] = _core.svn_auth_ssl_server_cert_info_t_fingerprint_set
    __swig_getmethods__["fingerprint"] = _core.svn_auth_ssl_server_cert_info_t_fingerprint_get
    __swig_setmethods__["valid_from"] = _core.svn_auth_ssl_server_cert_info_t_valid_from_set
    __swig_getmethods__["valid_from"] = _core.svn_auth_ssl_server_cert_info_t_valid_from_get
    __swig_setmethods__["valid_until"] = _core.svn_auth_ssl_server_cert_info_t_valid_until_set
    __swig_getmethods__["valid_until"] = _core.svn_auth_ssl_server_cert_info_t_valid_until_get
    __swig_setmethods__["issuer_dname"] = _core.svn_auth_ssl_server_cert_info_t_issuer_dname_set
    __swig_getmethods__["issuer_dname"] = _core.svn_auth_ssl_server_cert_info_t_issuer_dname_get
    __swig_setmethods__["ascii_cert"] = _core.svn_auth_ssl_server_cert_info_t_ascii_cert_set
    __swig_getmethods__["ascii_cert"] = _core.svn_auth_ssl_server_cert_info_t_ascii_cert_get
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for svn_auth_ssl_server_cert_info_t"""
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
        """__init__(self) -> svn_auth_ssl_server_cert_info_t"""
        _swig_setattr(self, svn_auth_ssl_server_cert_info_t, 'this', apply(_core.new_svn_auth_ssl_server_cert_info_t, args))
        _swig_setattr(self, svn_auth_ssl_server_cert_info_t, 'thisown', 1)
    def __del__(self, destroy=_core.delete_svn_auth_ssl_server_cert_info_t):
        """__del__(self)"""
        try:
            if self.thisown: destroy(self)
        except: pass


class svn_auth_ssl_server_cert_info_tPtr(svn_auth_ssl_server_cert_info_t):
    def __init__(self, this):
        _swig_setattr(self, svn_auth_ssl_server_cert_info_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, svn_auth_ssl_server_cert_info_t, 'thisown', 0)
        _swig_setattr(self, svn_auth_ssl_server_cert_info_t,self.__class__,svn_auth_ssl_server_cert_info_t)
_core.svn_auth_ssl_server_cert_info_t_swigregister(svn_auth_ssl_server_cert_info_tPtr)


def svn_auth_ssl_server_cert_info_dup(*args):
    """svn_auth_ssl_server_cert_info_dup(svn_auth_ssl_server_cert_info_t info, apr_pool_t pool) -> svn_auth_ssl_server_cert_info_t"""
    return apply(_core.svn_auth_ssl_server_cert_info_dup, args)
class svn_auth_cred_ssl_server_trust_t:
    """Proxy of C svn_auth_cred_ssl_server_trust_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, svn_auth_cred_ssl_server_trust_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, svn_auth_cred_ssl_server_trust_t, name)
    def __repr__(self):
        return "<%s.%s; proxy of C svn_auth_cred_ssl_server_trust_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    __swig_setmethods__["may_save"] = _core.svn_auth_cred_ssl_server_trust_t_may_save_set
    __swig_getmethods__["may_save"] = _core.svn_auth_cred_ssl_server_trust_t_may_save_get
    __swig_setmethods__["accepted_failures"] = _core.svn_auth_cred_ssl_server_trust_t_accepted_failures_set
    __swig_getmethods__["accepted_failures"] = _core.svn_auth_cred_ssl_server_trust_t_accepted_failures_get
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for svn_auth_cred_ssl_server_trust_t"""
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
        """__init__(self) -> svn_auth_cred_ssl_server_trust_t"""
        _swig_setattr(self, svn_auth_cred_ssl_server_trust_t, 'this', apply(_core.new_svn_auth_cred_ssl_server_trust_t, args))
        _swig_setattr(self, svn_auth_cred_ssl_server_trust_t, 'thisown', 1)
    def __del__(self, destroy=_core.delete_svn_auth_cred_ssl_server_trust_t):
        """__del__(self)"""
        try:
            if self.thisown: destroy(self)
        except: pass


class svn_auth_cred_ssl_server_trust_tPtr(svn_auth_cred_ssl_server_trust_t):
    def __init__(self, this):
        _swig_setattr(self, svn_auth_cred_ssl_server_trust_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, svn_auth_cred_ssl_server_trust_t, 'thisown', 0)
        _swig_setattr(self, svn_auth_cred_ssl_server_trust_t,self.__class__,svn_auth_cred_ssl_server_trust_t)
_core.svn_auth_cred_ssl_server_trust_t_swigregister(svn_auth_cred_ssl_server_trust_tPtr)

SVN_AUTH_SSL_NOTYETVALID = _core.SVN_AUTH_SSL_NOTYETVALID
SVN_AUTH_SSL_EXPIRED = _core.SVN_AUTH_SSL_EXPIRED
SVN_AUTH_SSL_CNMISMATCH = _core.SVN_AUTH_SSL_CNMISMATCH
SVN_AUTH_SSL_UNKNOWNCA = _core.SVN_AUTH_SSL_UNKNOWNCA
SVN_AUTH_SSL_OTHER = _core.SVN_AUTH_SSL_OTHER

def svn_auth_open(*args):
    """
    svn_auth_open(svn_auth_baton_t auth_baton, apr_array_header_t providers, 
        apr_pool_t pool)
    """
    return apply(_core.svn_auth_open, args)

def svn_auth_set_parameter(*args):
    """svn_auth_set_parameter(svn_auth_baton_t auth_baton, char name, void value)"""
    return apply(_core.svn_auth_set_parameter, args)
SVN_AUTH_PARAM_PREFIX = _core.SVN_AUTH_PARAM_PREFIX
SVN_AUTH_PARAM_DEFAULT_USERNAME = _core.SVN_AUTH_PARAM_DEFAULT_USERNAME
SVN_AUTH_PARAM_DEFAULT_PASSWORD = _core.SVN_AUTH_PARAM_DEFAULT_PASSWORD
SVN_AUTH_PARAM_NON_INTERACTIVE = _core.SVN_AUTH_PARAM_NON_INTERACTIVE
SVN_AUTH_PARAM_DONT_STORE_PASSWORDS = _core.SVN_AUTH_PARAM_DONT_STORE_PASSWORDS
SVN_AUTH_PARAM_NO_AUTH_CACHE = _core.SVN_AUTH_PARAM_NO_AUTH_CACHE
SVN_AUTH_PARAM_SSL_SERVER_FAILURES = _core.SVN_AUTH_PARAM_SSL_SERVER_FAILURES
SVN_AUTH_PARAM_SSL_SERVER_CERT_INFO = _core.SVN_AUTH_PARAM_SSL_SERVER_CERT_INFO
SVN_AUTH_PARAM_CONFIG = _core.SVN_AUTH_PARAM_CONFIG
SVN_AUTH_PARAM_SERVER_GROUP = _core.SVN_AUTH_PARAM_SERVER_GROUP
SVN_AUTH_PARAM_CONFIG_DIR = _core.SVN_AUTH_PARAM_CONFIG_DIR

def svn_auth_first_credentials(*args):
    """
    svn_auth_first_credentials(void credentials, svn_auth_iterstate_t state, char cred_kind, 
        char realmstring, svn_auth_baton_t auth_baton, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_core.svn_auth_first_credentials, args)

def svn_auth_next_credentials(*args):
    """svn_auth_next_credentials(void credentials, svn_auth_iterstate_t state, apr_pool_t pool) -> svn_error_t"""
    return apply(_core.svn_auth_next_credentials, args)

def svn_auth_save_credentials(*args):
    """svn_auth_save_credentials(svn_auth_iterstate_t state, apr_pool_t pool) -> svn_error_t"""
    return apply(_core.svn_auth_save_credentials, args)

def svn_auth_get_simple_prompt_provider(*args):
    """
    svn_auth_get_simple_prompt_provider(svn_auth_provider_object_t provider, svn_auth_simple_prompt_func_t prompt_func, 
        void prompt_baton, 
        int retry_limit, apr_pool_t pool)
    """
    return apply(_core.svn_auth_get_simple_prompt_provider, args)

def svn_auth_get_username_prompt_provider(*args):
    """
    svn_auth_get_username_prompt_provider(svn_auth_provider_object_t provider, svn_auth_username_prompt_func_t prompt_func, 
        void prompt_baton, 
        int retry_limit, apr_pool_t pool)
    """
    return apply(_core.svn_auth_get_username_prompt_provider, args)

def svn_auth_get_simple_provider(*args):
    """svn_auth_get_simple_provider(svn_auth_provider_object_t provider, apr_pool_t pool)"""
    return apply(_core.svn_auth_get_simple_provider, args)

def svn_auth_get_username_provider(*args):
    """svn_auth_get_username_provider(svn_auth_provider_object_t provider, apr_pool_t pool)"""
    return apply(_core.svn_auth_get_username_provider, args)

def svn_auth_get_ssl_server_trust_file_provider(*args):
    """svn_auth_get_ssl_server_trust_file_provider(svn_auth_provider_object_t provider, apr_pool_t pool)"""
    return apply(_core.svn_auth_get_ssl_server_trust_file_provider, args)

def svn_auth_get_ssl_client_cert_file_provider(*args):
    """svn_auth_get_ssl_client_cert_file_provider(svn_auth_provider_object_t provider, apr_pool_t pool)"""
    return apply(_core.svn_auth_get_ssl_client_cert_file_provider, args)

def svn_auth_get_ssl_client_cert_pw_file_provider(*args):
    """svn_auth_get_ssl_client_cert_pw_file_provider(svn_auth_provider_object_t provider, apr_pool_t pool)"""
    return apply(_core.svn_auth_get_ssl_client_cert_pw_file_provider, args)

def svn_auth_get_ssl_server_trust_prompt_provider(*args):
    """
    svn_auth_get_ssl_server_trust_prompt_provider(svn_auth_provider_object_t provider, svn_auth_ssl_server_trust_prompt_func_t prompt_func, 
        void prompt_baton, 
        apr_pool_t pool)
    """
    return apply(_core.svn_auth_get_ssl_server_trust_prompt_provider, args)

def svn_auth_get_ssl_client_cert_prompt_provider(*args):
    """
    svn_auth_get_ssl_client_cert_prompt_provider(svn_auth_provider_object_t provider, svn_auth_ssl_client_cert_prompt_func_t prompt_func, 
        void prompt_baton, 
        int retry_limit, apr_pool_t pool)
    """
    return apply(_core.svn_auth_get_ssl_client_cert_prompt_provider, args)

def svn_auth_get_ssl_client_cert_pw_prompt_provider(*args):
    """
    svn_auth_get_ssl_client_cert_pw_prompt_provider(svn_auth_provider_object_t provider, svn_auth_ssl_client_cert_pw_prompt_func_t prompt_func, 
        void prompt_baton, 
        int retry_limit, apr_pool_t pool)
    """
    return apply(_core.svn_auth_get_ssl_client_cert_pw_prompt_provider, args)
class svn_auth_baton_t:
    """Proxy of C svn_auth_baton_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, svn_auth_baton_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, svn_auth_baton_t, name)
    def __init__(self): raise RuntimeError, "No constructor defined"
    def __repr__(self):
        return "<%s.%s; proxy of C svn_auth_baton_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for svn_auth_baton_t"""
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


class svn_auth_baton_tPtr(svn_auth_baton_t):
    def __init__(self, this):
        _swig_setattr(self, svn_auth_baton_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, svn_auth_baton_t, 'thisown', 0)
        _swig_setattr(self, svn_auth_baton_t,self.__class__,svn_auth_baton_t)
_core.svn_auth_baton_t_swigregister(svn_auth_baton_tPtr)

class svn_auth_iterstate_t:
    """Proxy of C svn_auth_iterstate_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, svn_auth_iterstate_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, svn_auth_iterstate_t, name)
    def __init__(self): raise RuntimeError, "No constructor defined"
    def __repr__(self):
        return "<%s.%s; proxy of C svn_auth_iterstate_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for svn_auth_iterstate_t"""
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


class svn_auth_iterstate_tPtr(svn_auth_iterstate_t):
    def __init__(self, this):
        _swig_setattr(self, svn_auth_iterstate_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, svn_auth_iterstate_t, 'thisown', 0)
        _swig_setattr(self, svn_auth_iterstate_t,self.__class__,svn_auth_iterstate_t)
_core.svn_auth_iterstate_t_swigregister(svn_auth_iterstate_tPtr)

SVN_CONFIG_CATEGORY_SERVERS = _core.SVN_CONFIG_CATEGORY_SERVERS
SVN_CONFIG_SECTION_GROUPS = _core.SVN_CONFIG_SECTION_GROUPS
SVN_CONFIG_SECTION_GLOBAL = _core.SVN_CONFIG_SECTION_GLOBAL
SVN_CONFIG_OPTION_HTTP_PROXY_HOST = _core.SVN_CONFIG_OPTION_HTTP_PROXY_HOST
SVN_CONFIG_OPTION_HTTP_PROXY_PORT = _core.SVN_CONFIG_OPTION_HTTP_PROXY_PORT
SVN_CONFIG_OPTION_HTTP_PROXY_USERNAME = _core.SVN_CONFIG_OPTION_HTTP_PROXY_USERNAME
SVN_CONFIG_OPTION_HTTP_PROXY_PASSWORD = _core.SVN_CONFIG_OPTION_HTTP_PROXY_PASSWORD
SVN_CONFIG_OPTION_HTTP_PROXY_EXCEPTIONS = _core.SVN_CONFIG_OPTION_HTTP_PROXY_EXCEPTIONS
SVN_CONFIG_OPTION_HTTP_TIMEOUT = _core.SVN_CONFIG_OPTION_HTTP_TIMEOUT
SVN_CONFIG_OPTION_HTTP_COMPRESSION = _core.SVN_CONFIG_OPTION_HTTP_COMPRESSION
SVN_CONFIG_OPTION_NEON_DEBUG_MASK = _core.SVN_CONFIG_OPTION_NEON_DEBUG_MASK
SVN_CONFIG_OPTION_SSL_AUTHORITY_FILES = _core.SVN_CONFIG_OPTION_SSL_AUTHORITY_FILES
SVN_CONFIG_OPTION_SSL_TRUST_DEFAULT_CA = _core.SVN_CONFIG_OPTION_SSL_TRUST_DEFAULT_CA
SVN_CONFIG_OPTION_SSL_CLIENT_CERT_FILE = _core.SVN_CONFIG_OPTION_SSL_CLIENT_CERT_FILE
SVN_CONFIG_OPTION_SSL_CLIENT_CERT_PASSWORD = _core.SVN_CONFIG_OPTION_SSL_CLIENT_CERT_PASSWORD
SVN_CONFIG_CATEGORY_CONFIG = _core.SVN_CONFIG_CATEGORY_CONFIG
SVN_CONFIG_SECTION_AUTH = _core.SVN_CONFIG_SECTION_AUTH
SVN_CONFIG_OPTION_STORE_PASSWORDS = _core.SVN_CONFIG_OPTION_STORE_PASSWORDS
SVN_CONFIG_OPTION_STORE_AUTH_CREDS = _core.SVN_CONFIG_OPTION_STORE_AUTH_CREDS
SVN_CONFIG_SECTION_HELPERS = _core.SVN_CONFIG_SECTION_HELPERS
SVN_CONFIG_OPTION_EDITOR_CMD = _core.SVN_CONFIG_OPTION_EDITOR_CMD
SVN_CONFIG_OPTION_DIFF_CMD = _core.SVN_CONFIG_OPTION_DIFF_CMD
SVN_CONFIG_OPTION_DIFF3_CMD = _core.SVN_CONFIG_OPTION_DIFF3_CMD
SVN_CONFIG_OPTION_DIFF3_HAS_PROGRAM_ARG = _core.SVN_CONFIG_OPTION_DIFF3_HAS_PROGRAM_ARG
SVN_CONFIG_SECTION_MISCELLANY = _core.SVN_CONFIG_SECTION_MISCELLANY
SVN_CONFIG_OPTION_GLOBAL_IGNORES = _core.SVN_CONFIG_OPTION_GLOBAL_IGNORES
SVN_CONFIG_OPTION_LOG_ENCODING = _core.SVN_CONFIG_OPTION_LOG_ENCODING
SVN_CONFIG_OPTION_USE_COMMIT_TIMES = _core.SVN_CONFIG_OPTION_USE_COMMIT_TIMES
SVN_CONFIG_OPTION_TEMPLATE_ROOT = _core.SVN_CONFIG_OPTION_TEMPLATE_ROOT
SVN_CONFIG_OPTION_ENABLE_AUTO_PROPS = _core.SVN_CONFIG_OPTION_ENABLE_AUTO_PROPS
SVN_CONFIG_OPTION_NO_UNLOCK = _core.SVN_CONFIG_OPTION_NO_UNLOCK
SVN_CONFIG_SECTION_TUNNELS = _core.SVN_CONFIG_SECTION_TUNNELS
SVN_CONFIG_SECTION_AUTO_PROPS = _core.SVN_CONFIG_SECTION_AUTO_PROPS
SVN_CONFIG_SECTION_GENERAL = _core.SVN_CONFIG_SECTION_GENERAL
SVN_CONFIG_OPTION_ANON_ACCESS = _core.SVN_CONFIG_OPTION_ANON_ACCESS
SVN_CONFIG_OPTION_AUTH_ACCESS = _core.SVN_CONFIG_OPTION_AUTH_ACCESS
SVN_CONFIG_OPTION_PASSWORD_DB = _core.SVN_CONFIG_OPTION_PASSWORD_DB
SVN_CONFIG_OPTION_REALM = _core.SVN_CONFIG_OPTION_REALM
SVN_CONFIG_OPTION_AUTHZ_DB = _core.SVN_CONFIG_OPTION_AUTHZ_DB
SVN_CONFIG_SECTION_USERS = _core.SVN_CONFIG_SECTION_USERS
SVN_CONFIG_DEFAULT_GLOBAL_IGNORES = _core.SVN_CONFIG_DEFAULT_GLOBAL_IGNORES
SVN_CONFIG_TRUE = _core.SVN_CONFIG_TRUE
SVN_CONFIG_FALSE = _core.SVN_CONFIG_FALSE

def svn_config_get_config(*args):
    """svn_config_get_config(apr_hash_t cfg_hash, char config_dir, apr_pool_t pool) -> svn_error_t"""
    return apply(_core.svn_config_get_config, args)

def svn_config_read(*args):
    """
    svn_config_read(svn_config_t cfgp, char file, svn_boolean_t must_exist, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_core.svn_config_read, args)

def svn_config_merge(*args):
    """svn_config_merge(svn_config_t cfg, char file, svn_boolean_t must_exist) -> svn_error_t"""
    return apply(_core.svn_config_merge, args)

def svn_config_get(*args):
    """
    svn_config_get(svn_config_t cfg, char valuep, char section, char option, 
        char default_value)
    """
    return apply(_core.svn_config_get, args)

def svn_config_set(*args):
    """svn_config_set(svn_config_t cfg, char section, char option, char value)"""
    return apply(_core.svn_config_set, args)

def svn_config_get_bool(*args):
    """
    svn_config_get_bool(svn_config_t cfg, svn_boolean_t valuep, char section, 
        char option, svn_boolean_t default_value) -> svn_error_t
    """
    return apply(_core.svn_config_get_bool, args)

def svn_config_set_bool(*args):
    """svn_config_set_bool(svn_config_t cfg, char section, char option, svn_boolean_t value)"""
    return apply(_core.svn_config_set_bool, args)

def svn_config_enumerate_sections(*args):
    """
    svn_config_enumerate_sections(svn_config_t cfg, svn_config_section_enumerator_t callback, 
        void baton) -> int
    """
    return apply(_core.svn_config_enumerate_sections, args)

def svn_config_enumerate_sections2(*args):
    """
    svn_config_enumerate_sections2(svn_config_t cfg, svn_config_section_enumerator2_t callback, 
        void baton, apr_pool_t pool) -> int
    """
    return apply(_core.svn_config_enumerate_sections2, args)

def svn_config_enumerate(*args):
    """
    svn_config_enumerate(svn_config_t cfg, char section, svn_config_enumerator_t callback, 
        void baton) -> int
    """
    return apply(_core.svn_config_enumerate, args)

def svn_config_enumerate2(*args):
    """
    svn_config_enumerate2(svn_config_t cfg, char section, svn_config_enumerator2_t callback, 
        void baton, apr_pool_t pool) -> int
    """
    return apply(_core.svn_config_enumerate2, args)

def svn_config_has_section(*args):
    """svn_config_has_section(svn_config_t cfg, char section) -> svn_boolean_t"""
    return apply(_core.svn_config_has_section, args)

def svn_config_find_group(*args):
    """svn_config_find_group(svn_config_t cfg, char key, char master_section, apr_pool_t pool) -> char"""
    return apply(_core.svn_config_find_group, args)

def svn_config_get_server_setting(*args):
    """
    svn_config_get_server_setting(svn_config_t cfg, char server_group, char option_name, 
        char default_value) -> char
    """
    return apply(_core.svn_config_get_server_setting, args)

def svn_config_get_server_setting_int(*args):
    """
    svn_config_get_server_setting_int(svn_config_t cfg, char server_group, char option_name, 
        apr_int64_t default_value, apr_int64_t result_value, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_core.svn_config_get_server_setting_int, args)

def svn_config_ensure(*args):
    """svn_config_ensure(char config_dir, apr_pool_t pool) -> svn_error_t"""
    return apply(_core.svn_config_ensure, args)
SVN_CONFIG_REALMSTRING_KEY = _core.SVN_CONFIG_REALMSTRING_KEY

def svn_config_read_auth_data(*args):
    """
    svn_config_read_auth_data(apr_hash_t hash, char cred_kind, char realmstring, 
        char config_dir, apr_pool_t pool) -> svn_error_t
    """
    return apply(_core.svn_config_read_auth_data, args)

def svn_config_write_auth_data(*args):
    """
    svn_config_write_auth_data(apr_hash_t hash, char cred_kind, char realmstring, 
        char config_dir, apr_pool_t pool) -> svn_error_t
    """
    return apply(_core.svn_config_write_auth_data, args)
class svn_config_t:
    """Proxy of C svn_config_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, svn_config_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, svn_config_t, name)
    def __init__(self): raise RuntimeError, "No constructor defined"
    def __repr__(self):
        return "<%s.%s; proxy of C svn_config_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for svn_config_t"""
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


class svn_config_tPtr(svn_config_t):
    def __init__(self, this):
        _swig_setattr(self, svn_config_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, svn_config_t, 'thisown', 0)
        _swig_setattr(self, svn_config_t,self.__class__,svn_config_t)
_core.svn_config_t_swigregister(svn_config_tPtr)


def svn_utf_initialize(*args):
    """svn_utf_initialize(apr_pool_t pool)"""
    return apply(_core.svn_utf_initialize, args)

def svn_utf_stringbuf_to_utf8(*args):
    """svn_utf_stringbuf_to_utf8(svn_stringbuf_t dest, svn_stringbuf_t src, apr_pool_t pool) -> svn_error_t"""
    return apply(_core.svn_utf_stringbuf_to_utf8, args)

def svn_utf_string_to_utf8(*args):
    """svn_utf_string_to_utf8(svn_string_t dest, svn_string_t src, apr_pool_t pool) -> svn_error_t"""
    return apply(_core.svn_utf_string_to_utf8, args)

def svn_utf_cstring_to_utf8(*args):
    """svn_utf_cstring_to_utf8(char dest, char src, apr_pool_t pool) -> svn_error_t"""
    return apply(_core.svn_utf_cstring_to_utf8, args)

def svn_utf_cstring_to_utf8_ex2(*args):
    """svn_utf_cstring_to_utf8_ex2(char dest, char src, char frompage, apr_pool_t pool) -> svn_error_t"""
    return apply(_core.svn_utf_cstring_to_utf8_ex2, args)

def svn_utf_cstring_to_utf8_ex(*args):
    """
    svn_utf_cstring_to_utf8_ex(char dest, char src, char frompage, char convset_key, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_core.svn_utf_cstring_to_utf8_ex, args)

def svn_utf_stringbuf_from_utf8(*args):
    """svn_utf_stringbuf_from_utf8(svn_stringbuf_t dest, svn_stringbuf_t src, apr_pool_t pool) -> svn_error_t"""
    return apply(_core.svn_utf_stringbuf_from_utf8, args)

def svn_utf_string_from_utf8(*args):
    """svn_utf_string_from_utf8(svn_string_t dest, svn_string_t src, apr_pool_t pool) -> svn_error_t"""
    return apply(_core.svn_utf_string_from_utf8, args)

def svn_utf_cstring_from_utf8(*args):
    """svn_utf_cstring_from_utf8(char dest, char src, apr_pool_t pool) -> svn_error_t"""
    return apply(_core.svn_utf_cstring_from_utf8, args)

def svn_utf_cstring_from_utf8_ex2(*args):
    """svn_utf_cstring_from_utf8_ex2(char dest, char src, char topage, apr_pool_t pool) -> svn_error_t"""
    return apply(_core.svn_utf_cstring_from_utf8_ex2, args)

def svn_utf_cstring_from_utf8_ex(*args):
    """
    svn_utf_cstring_from_utf8_ex(char dest, char src, char topage, char convset_key, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_core.svn_utf_cstring_from_utf8_ex, args)

def svn_utf_cstring_from_utf8_fuzzy(*args):
    """svn_utf_cstring_from_utf8_fuzzy(char src, apr_pool_t pool) -> char"""
    return apply(_core.svn_utf_cstring_from_utf8_fuzzy, args)

def svn_utf_cstring_from_utf8_stringbuf(*args):
    """svn_utf_cstring_from_utf8_stringbuf(char dest, svn_stringbuf_t src, apr_pool_t pool) -> svn_error_t"""
    return apply(_core.svn_utf_cstring_from_utf8_stringbuf, args)

def svn_utf_cstring_from_utf8_string(*args):
    """svn_utf_cstring_from_utf8_string(char dest, svn_string_t src, apr_pool_t pool) -> svn_error_t"""
    return apply(_core.svn_utf_cstring_from_utf8_string, args)

def svn_nls_init(*args):
    """svn_nls_init() -> svn_error_t"""
    return apply(_core.svn_nls_init, args)

def svn_path_is_empty(*args):
    """svn_path_is_empty(char path) -> int"""
    return apply(_core.svn_path_is_empty, args)

def svn_path_canonicalize(*args):
    """svn_path_canonicalize(char path, apr_pool_t pool) -> char"""
    return apply(_core.svn_path_canonicalize, args)

def svn_path_compare_paths(*args):
    """svn_path_compare_paths(char path1, char path2) -> int"""
    return apply(_core.svn_path_compare_paths, args)

def svn_path_get_longest_ancestor(*args):
    """svn_path_get_longest_ancestor(char path1, char path2, apr_pool_t pool) -> char"""
    return apply(_core.svn_path_get_longest_ancestor, args)

def svn_path_is_uri_safe(*args):
    """svn_path_is_uri_safe(char path) -> svn_boolean_t"""
    return apply(_core.svn_path_is_uri_safe, args)
svn_io_file_del_none = _core.svn_io_file_del_none
svn_io_file_del_on_close = _core.svn_io_file_del_on_close
svn_io_file_del_on_pool_cleanup = _core.svn_io_file_del_on_pool_cleanup
class svn_io_dirent_t:
    """Proxy of C svn_io_dirent_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, svn_io_dirent_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, svn_io_dirent_t, name)
    def __repr__(self):
        return "<%s.%s; proxy of C svn_io_dirent_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    __swig_setmethods__["kind"] = _core.svn_io_dirent_t_kind_set
    __swig_getmethods__["kind"] = _core.svn_io_dirent_t_kind_get
    __swig_setmethods__["special"] = _core.svn_io_dirent_t_special_set
    __swig_getmethods__["special"] = _core.svn_io_dirent_t_special_get
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for svn_io_dirent_t"""
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
        """__init__(self) -> svn_io_dirent_t"""
        _swig_setattr(self, svn_io_dirent_t, 'this', apply(_core.new_svn_io_dirent_t, args))
        _swig_setattr(self, svn_io_dirent_t, 'thisown', 1)
    def __del__(self, destroy=_core.delete_svn_io_dirent_t):
        """__del__(self)"""
        try:
            if self.thisown: destroy(self)
        except: pass


class svn_io_dirent_tPtr(svn_io_dirent_t):
    def __init__(self, this):
        _swig_setattr(self, svn_io_dirent_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, svn_io_dirent_t, 'thisown', 0)
        _swig_setattr(self, svn_io_dirent_t,self.__class__,svn_io_dirent_t)
_core.svn_io_dirent_t_swigregister(svn_io_dirent_tPtr)


def svn_io_open_unique_file2(*args):
    """
    svn_io_open_unique_file2(apr_file_t f, char unique_name_p, char path, char suffix, 
        svn_io_file_del_t delete_when, apr_pool_t pool) -> svn_error_t
    """
    return apply(_core.svn_io_open_unique_file2, args)

def svn_io_open_unique_file(*args):
    """
    svn_io_open_unique_file(apr_file_t f, char unique_name_p, char path, char suffix, 
        svn_boolean_t delete_on_close, apr_pool_t pool) -> svn_error_t
    """
    return apply(_core.svn_io_open_unique_file, args)

def svn_io_file_checksum(*args):
    """svn_io_file_checksum(unsigned char digest, char file, apr_pool_t pool) -> svn_error_t"""
    return apply(_core.svn_io_file_checksum, args)

def svn_io_files_contents_same_p(*args):
    """svn_io_files_contents_same_p(svn_boolean_t same, char file1, char file2, apr_pool_t pool) -> svn_error_t"""
    return apply(_core.svn_io_files_contents_same_p, args)

def svn_stream_empty(*args):
    """svn_stream_empty(apr_pool_t pool) -> svn_stream_t"""
    return apply(_core.svn_stream_empty, args)

def svn_stream_disown(*args):
    """svn_stream_disown(svn_stream_t stream, apr_pool_t pool) -> svn_stream_t"""
    return apply(_core.svn_stream_disown, args)

def svn_stream_from_aprfile2(*args):
    """svn_stream_from_aprfile2(apr_file_t file, svn_boolean_t disown, apr_pool_t pool) -> svn_stream_t"""
    return apply(_core.svn_stream_from_aprfile2, args)

def svn_stream_from_aprfile(*args):
    """svn_stream_from_aprfile(apr_file_t file, apr_pool_t pool) -> svn_stream_t"""
    return apply(_core.svn_stream_from_aprfile, args)

def svn_stream_for_stdout(*args):
    """svn_stream_for_stdout(svn_stream_t out, apr_pool_t pool) -> svn_error_t"""
    return apply(_core.svn_stream_for_stdout, args)

def svn_stream_from_stringbuf(*args):
    """svn_stream_from_stringbuf(svn_stringbuf_t str, apr_pool_t pool) -> svn_stream_t"""
    return apply(_core.svn_stream_from_stringbuf, args)

def svn_stream_compressed(*args):
    """svn_stream_compressed(svn_stream_t stream, apr_pool_t pool) -> svn_stream_t"""
    return apply(_core.svn_stream_compressed, args)

def svn_stream_read(*args):
    """svn_stream_read(svn_stream_t stream, char buffer) -> svn_error_t"""
    return apply(_core.svn_stream_read, args)

def svn_stream_write(*args):
    """svn_stream_write(svn_stream_t stream, char data) -> svn_error_t"""
    return apply(_core.svn_stream_write, args)

def svn_stream_close(*args):
    """svn_stream_close(svn_stream_t stream) -> svn_error_t"""
    return apply(_core.svn_stream_close, args)

def svn_stream_readline(*args):
    """
    svn_stream_readline(svn_stream_t stream, svn_stringbuf_t stringbuf, char eol, 
        svn_boolean_t eof, apr_pool_t pool) -> svn_error_t
    """
    return apply(_core.svn_stream_readline, args)

def svn_stream_copy(*args):
    """svn_stream_copy(svn_stream_t from, svn_stream_t to, apr_pool_t pool) -> svn_error_t"""
    return apply(_core.svn_stream_copy, args)

def svn_stream_contents_same(*args):
    """
    svn_stream_contents_same(svn_boolean_t same, svn_stream_t stream1, svn_stream_t stream2, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_core.svn_stream_contents_same, args)

def svn_stringbuf_from_file(*args):
    """svn_stringbuf_from_file(svn_stringbuf_t result, char filename, apr_pool_t pool) -> svn_error_t"""
    return apply(_core.svn_stringbuf_from_file, args)

def svn_stringbuf_from_aprfile(*args):
    """svn_stringbuf_from_aprfile(svn_stringbuf_t result, apr_file_t file, apr_pool_t pool) -> svn_error_t"""
    return apply(_core.svn_stringbuf_from_aprfile, args)

def svn_io_detect_mimetype(*args):
    """svn_io_detect_mimetype(char mimetype, char file, apr_pool_t pool) -> svn_error_t"""
    return apply(_core.svn_io_detect_mimetype, args)
class svn_stream_t:
    """Proxy of C svn_stream_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, svn_stream_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, svn_stream_t, name)
    def __init__(self): raise RuntimeError, "No constructor defined"
    def __repr__(self):
        return "<%s.%s; proxy of C svn_stream_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for svn_stream_t"""
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


class svn_stream_tPtr(svn_stream_t):
    def __init__(self, this):
        _swig_setattr(self, svn_stream_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, svn_stream_t, 'thisown', 0)
        _swig_setattr(self, svn_stream_t,self.__class__,svn_stream_t)
_core.svn_stream_t_swigregister(svn_stream_tPtr)


def svn_swig_py_set_application_pool(*args):
    """svn_swig_py_set_application_pool(PyObject py_pool, apr_pool_t pool)"""
    return apply(_core.svn_swig_py_set_application_pool, args)

def svn_swig_py_clear_application_pool(*args):
    """svn_swig_py_clear_application_pool()"""
    return apply(_core.svn_swig_py_clear_application_pool, args)
SubversionException = _core.SubversionException

class apr_array_header_t:
    """Proxy of C apr_array_header_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, apr_array_header_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, apr_array_header_t, name)
    def __init__(self): raise RuntimeError, "No constructor defined"
    def __repr__(self):
        return "<%s.%s; proxy of C apr_array_header_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for apr_array_header_t"""
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


class apr_array_header_tPtr(apr_array_header_t):
    def __init__(self, this):
        _swig_setattr(self, apr_array_header_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, apr_array_header_t, 'thisown', 0)
        _swig_setattr(self, apr_array_header_t,self.__class__,apr_array_header_t)
_core.apr_array_header_t_swigregister(apr_array_header_tPtr)

class apr_file_t:
    """Proxy of C apr_file_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, apr_file_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, apr_file_t, name)
    def __init__(self): raise RuntimeError, "No constructor defined"
    def __repr__(self):
        return "<%s.%s; proxy of C apr_file_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for apr_file_t"""
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


class apr_file_tPtr(apr_file_t):
    def __init__(self, this):
        _swig_setattr(self, apr_file_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, apr_file_t, 'thisown', 0)
        _swig_setattr(self, apr_file_t,self.__class__,apr_file_t)
_core.apr_file_t_swigregister(apr_file_tPtr)

class apr_hash_t:
    """Proxy of C apr_hash_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, apr_hash_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, apr_hash_t, name)
    def __init__(self): raise RuntimeError, "No constructor defined"
    def __repr__(self):
        return "<%s.%s; proxy of C apr_hash_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for apr_hash_t"""
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


class apr_hash_tPtr(apr_hash_t):
    def __init__(self, this):
        _swig_setattr(self, apr_hash_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, apr_hash_t, 'thisown', 0)
        _swig_setattr(self, apr_hash_t,self.__class__,apr_hash_t)
_core.apr_hash_t_swigregister(apr_hash_tPtr)

import threading

application_pool = None
application_pool_lock = threading.Lock()
class GenericSWIGWrapper:
  def __init__(self, this, pool):
    """Create new Generic SWIG wrapper object"""
    import weakref
    self.this = this
    self._parent_pool = pool
    self._is_valid = weakref.ref(pool._is_valid)

  def set_parent_pool(self, pool):
    """Set the parent pool of this object"""
    self._parent_pool = pool

  def valid(self):
    """Is this object valid?"""
    return self._is_valid()

  def assert_valid(self):
    """Assert that this object is still valid"""
    assert self.valid(), "This object has already been destroyed"

  def _unwrap(self):
    """Return underlying SWIG object"""
    self.assert_valid()
    return self.this
 
def _mark_weakpool_invalid(weakpool):
  if weakpool and weakpool() and hasattr(weakpool(), "_is_valid"):
    del weakpool()._is_valid


class apr_pool_t:
    """Proxy of C apr_pool_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, apr_pool_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, apr_pool_t, name)
    def __init__(self): raise RuntimeError, "No constructor defined"
    def __repr__(self):
        return "<%s.%s; proxy of C apr_pool_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    def set_parent_pool(self, parent_pool=None):
      """Create a new memory pool"""
      global application_pool

      try:
        application_pool_lock.acquire()

        self._parent_pool = parent_pool or application_pool
        self._mark_valid()

        # Protect important functions from GC
        self._apr_pool_destroy = _core.apr_pool_destroy
        self._svn_swig_py_clear_application_pool = \
          _core.svn_swig_py_clear_application_pool

        # If we are an application-level pool,
        # then set this pool to be the application-level pool
        if not self._parent_pool:
          svn_swig_py_set_application_pool(self, self)
          application_pool = self
      finally:
        application_pool_lock.release()

    def valid(self):
      """Check whether this memory pool and its parents
      are still valid"""
      return hasattr(self,"_is_valid")

    def assert_valid(self):
      """Assert that this memory_pool is still valid."""
      assert self.valid(), "This pool has already been destroyed"

    def clear(self):
      """Clear embedded memory pool. Invalidate all subpools."""
      pool = self._parent_pool
      apr_pool_clear(self)
      self.set_parent_pool(pool)

    def destroy(self):
      """Destroy embedded memory pool. If you do not destroy
      the memory pool manually, Python will destroy it
      automatically."""
      global application_pool

      self.assert_valid()

      is_application_pool = not self._parent_pool

      # Destroy pool
      self._apr_pool_destroy(self)

      # Clear application pool if necessary
      if is_application_pool:
        application_pool = None
        self._svn_swig_py_clear_application_pool()

      # Mark self as invalid
      if hasattr(self, "_parent_pool"):
        del self._parent_pool
      if hasattr(self, "_is_valid"):
        del self._is_valid

    def __del__(self):
      """Automatically destroy memory pools, if necessary"""
      if self.valid():
        self.destroy()

    def _mark_valid(self):
      """Mark pool as valid"""

      self._weakparent = None

      if self._parent_pool:
        import weakref

        # Make sure that the parent object is valid
        self._parent_pool.assert_valid()

        # Refer to self using a weakrefrence so that we don't
        # create a reference cycle
        weakself = weakref.ref(self)

        # Set up callbacks to mark pool as invalid when parents
        # are destroyed
        self._weakparent = weakref.ref(self._parent_pool._is_valid,
          lambda x: _mark_weakpool_invalid(weakself))

      # Mark pool as valid
      self._is_valid = lambda: 1

    def _wrap(self, obj):
      """Mark a SWIG object as owned by this pool"""
      self.assert_valid()
      if hasattr(obj, "set_parent_pool"):
        obj.set_parent_pool(self)
        return obj 
      elif obj is None:
        return None
      else:
        return GenericSWIGWrapper(obj, self)



class apr_pool_tPtr(apr_pool_t):
    def __init__(self, this):
        _swig_setattr(self, apr_pool_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, apr_pool_t, 'thisown', 0)
        _swig_setattr(self, apr_pool_t,self.__class__,apr_pool_t)
_core.apr_pool_t_swigregister(apr_pool_tPtr)

# Initialize a global pool
svn_pool_create()


