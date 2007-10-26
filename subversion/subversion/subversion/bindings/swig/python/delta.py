# This file was created automatically by SWIG.
# Don't modify this file, modify the SWIG interface instead.
# This file is compatible with both classic and new-style classes.

import _delta

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

def svn_swig_py_make_editor(*args):
    """
    svn_swig_py_make_editor(svn_delta_editor_t editor, void edit_baton, PyObject py_editor, 
        apr_pool_t pool)
    """
    return apply(_delta.svn_swig_py_make_editor, args)

def svn_delta_version(*args):
    """svn_delta_version() -> svn_version_t"""
    return apply(_delta.svn_delta_version, args)
svn_txdelta_source = _delta.svn_txdelta_source
svn_txdelta_target = _delta.svn_txdelta_target
svn_txdelta_new = _delta.svn_txdelta_new
class svn_txdelta_op_t:
    """Proxy of C svn_txdelta_op_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, svn_txdelta_op_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, svn_txdelta_op_t, name)
    def __repr__(self):
        return "<%s.%s; proxy of C svn_txdelta_op_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    __swig_setmethods__["action_code"] = _delta.svn_txdelta_op_t_action_code_set
    __swig_getmethods__["action_code"] = _delta.svn_txdelta_op_t_action_code_get
    __swig_setmethods__["offset"] = _delta.svn_txdelta_op_t_offset_set
    __swig_getmethods__["offset"] = _delta.svn_txdelta_op_t_offset_get
    __swig_setmethods__["length"] = _delta.svn_txdelta_op_t_length_set
    __swig_getmethods__["length"] = _delta.svn_txdelta_op_t_length_get
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for svn_txdelta_op_t"""
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
        """__init__(self) -> svn_txdelta_op_t"""
        _swig_setattr(self, svn_txdelta_op_t, 'this', apply(_delta.new_svn_txdelta_op_t, args))
        _swig_setattr(self, svn_txdelta_op_t, 'thisown', 1)
    def __del__(self, destroy=_delta.delete_svn_txdelta_op_t):
        """__del__(self)"""
        try:
            if self.thisown: destroy(self)
        except: pass


class svn_txdelta_op_tPtr(svn_txdelta_op_t):
    def __init__(self, this):
        _swig_setattr(self, svn_txdelta_op_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, svn_txdelta_op_t, 'thisown', 0)
        _swig_setattr(self, svn_txdelta_op_t,self.__class__,svn_txdelta_op_t)
_delta.svn_txdelta_op_t_swigregister(svn_txdelta_op_tPtr)

class svn_txdelta_window_t:
    """Proxy of C svn_txdelta_window_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, svn_txdelta_window_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, svn_txdelta_window_t, name)
    def __repr__(self):
        return "<%s.%s; proxy of C svn_txdelta_window_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    __swig_setmethods__["sview_offset"] = _delta.svn_txdelta_window_t_sview_offset_set
    __swig_getmethods__["sview_offset"] = _delta.svn_txdelta_window_t_sview_offset_get
    __swig_setmethods__["sview_len"] = _delta.svn_txdelta_window_t_sview_len_set
    __swig_getmethods__["sview_len"] = _delta.svn_txdelta_window_t_sview_len_get
    __swig_setmethods__["tview_len"] = _delta.svn_txdelta_window_t_tview_len_set
    __swig_getmethods__["tview_len"] = _delta.svn_txdelta_window_t_tview_len_get
    __swig_setmethods__["num_ops"] = _delta.svn_txdelta_window_t_num_ops_set
    __swig_getmethods__["num_ops"] = _delta.svn_txdelta_window_t_num_ops_get
    __swig_setmethods__["src_ops"] = _delta.svn_txdelta_window_t_src_ops_set
    __swig_getmethods__["src_ops"] = _delta.svn_txdelta_window_t_src_ops_get
    __swig_setmethods__["ops"] = _delta.svn_txdelta_window_t_ops_set
    __swig_getmethods__["ops"] = _delta.svn_txdelta_window_t_ops_get
    __swig_getmethods__["new_data"] = _delta.svn_txdelta_window_t_new_data_get
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for svn_txdelta_window_t"""
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
        """__init__(self) -> svn_txdelta_window_t"""
        _swig_setattr(self, svn_txdelta_window_t, 'this', apply(_delta.new_svn_txdelta_window_t, args))
        _swig_setattr(self, svn_txdelta_window_t, 'thisown', 1)
    def __del__(self, destroy=_delta.delete_svn_txdelta_window_t):
        """__del__(self)"""
        try:
            if self.thisown: destroy(self)
        except: pass


class svn_txdelta_window_tPtr(svn_txdelta_window_t):
    def __init__(self, this):
        _swig_setattr(self, svn_txdelta_window_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, svn_txdelta_window_t, 'thisown', 0)
        _swig_setattr(self, svn_txdelta_window_t,self.__class__,svn_txdelta_window_t)
_delta.svn_txdelta_window_t_swigregister(svn_txdelta_window_tPtr)


def svn_txdelta_window_dup(*args):
    """svn_txdelta_window_dup(svn_txdelta_window_t window, apr_pool_t pool) -> svn_txdelta_window_t"""
    return apply(_delta.svn_txdelta_window_dup, args)

def svn_txdelta_compose_windows(*args):
    """
    svn_txdelta_compose_windows(svn_txdelta_window_t window_A, svn_txdelta_window_t window_B, 
        apr_pool_t pool) -> svn_txdelta_window_t
    """
    return apply(_delta.svn_txdelta_compose_windows, args)

def svn_txdelta_apply_instructions(*args):
    """
    svn_txdelta_apply_instructions(svn_txdelta_window_t window, char sbuf, char tbuf, 
        apr_size_t tlen)
    """
    return apply(_delta.svn_txdelta_apply_instructions, args)

def svn_txdelta_stream_create(*args):
    """
    svn_txdelta_stream_create(void baton, svn_txdelta_next_window_fn_t next_window, 
        svn_txdelta_md5_digest_fn_t md5_digest, apr_pool_t pool) -> svn_txdelta_stream_t
    """
    return apply(_delta.svn_txdelta_stream_create, args)

def svn_txdelta_next_window(*args):
    """
    svn_txdelta_next_window(svn_txdelta_window_t window, svn_txdelta_stream_t stream, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_delta.svn_txdelta_next_window, args)

def svn_txdelta_md5_digest(*args):
    """svn_txdelta_md5_digest(svn_txdelta_stream_t stream) -> unsigned char"""
    return apply(_delta.svn_txdelta_md5_digest, args)

def svn_txdelta(*args):
    """
    svn_txdelta(svn_txdelta_stream_t stream, svn_stream_t source, svn_stream_t target, 
        apr_pool_t pool)
    """
    return apply(_delta.svn_txdelta, args)

def svn_txdelta_target_push(*args):
    """
    svn_txdelta_target_push(svn_txdelta_window_handler_t handler, void handler_baton, 
        svn_stream_t source, apr_pool_t pool) -> svn_stream_t
    """
    return apply(_delta.svn_txdelta_target_push, args)

def svn_txdelta_send_string(*args):
    """
    svn_txdelta_send_string(svn_string_t string, svn_txdelta_window_handler_t handler, 
        void handler_baton, apr_pool_t pool) -> svn_error_t
    """
    return apply(_delta.svn_txdelta_send_string, args)

def svn_txdelta_send_stream(*args):
    """
    svn_txdelta_send_stream(svn_stream_t stream, svn_txdelta_window_handler_t handler, 
        void handler_baton, unsigned char digest, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_delta.svn_txdelta_send_stream, args)

def svn_txdelta_send_txstream(*args):
    """
    svn_txdelta_send_txstream(svn_txdelta_stream_t txstream, svn_txdelta_window_handler_t handler, 
        void handler_baton, apr_pool_t pool) -> svn_error_t
    """
    return apply(_delta.svn_txdelta_send_txstream, args)

def svn_txdelta_apply(*args):
    """
    svn_txdelta_apply(svn_stream_t source, svn_stream_t target, unsigned char result_digest, 
        char error_info, apr_pool_t pool, 
        svn_txdelta_window_handler_t handler, 
        void handler_baton)
    """
    return apply(_delta.svn_txdelta_apply, args)

def svn_txdelta_to_svndiff2(*args):
    """
    svn_txdelta_to_svndiff2(svn_txdelta_window_handler_t handler, void handler_baton, 
        svn_stream_t output, int svndiff_version, 
        apr_pool_t pool)
    """
    return apply(_delta.svn_txdelta_to_svndiff2, args)

def svn_txdelta_to_svndiff(*args):
    """
    svn_txdelta_to_svndiff(svn_stream_t output, apr_pool_t pool, svn_txdelta_window_handler_t handler, 
        void handler_baton)
    """
    return apply(_delta.svn_txdelta_to_svndiff, args)

def svn_txdelta_parse_svndiff(*args):
    """
    svn_txdelta_parse_svndiff(svn_txdelta_window_handler_t handler, void handler_baton, 
        svn_boolean_t error_on_early_close, apr_pool_t pool) -> svn_stream_t
    """
    return apply(_delta.svn_txdelta_parse_svndiff, args)

def svn_txdelta_read_svndiff_window(*args):
    """
    svn_txdelta_read_svndiff_window(svn_txdelta_window_t window, svn_stream_t stream, int svndiff_version, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_delta.svn_txdelta_read_svndiff_window, args)

def svn_txdelta_skip_svndiff_window(*args):
    """svn_txdelta_skip_svndiff_window(apr_file_t file, int svndiff_version, apr_pool_t pool) -> svn_error_t"""
    return apply(_delta.svn_txdelta_skip_svndiff_window, args)
class svn_delta_editor_t:
    """Proxy of C svn_delta_editor_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, svn_delta_editor_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, svn_delta_editor_t, name)
    def __repr__(self):
        return "<%s.%s; proxy of C svn_delta_editor_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    __swig_setmethods__["set_target_revision"] = _delta.svn_delta_editor_t_set_target_revision_set
    __swig_getmethods__["set_target_revision"] = _delta.svn_delta_editor_t_set_target_revision_get
    __swig_setmethods__["open_root"] = _delta.svn_delta_editor_t_open_root_set
    __swig_getmethods__["open_root"] = _delta.svn_delta_editor_t_open_root_get
    __swig_setmethods__["delete_entry"] = _delta.svn_delta_editor_t_delete_entry_set
    __swig_getmethods__["delete_entry"] = _delta.svn_delta_editor_t_delete_entry_get
    __swig_setmethods__["add_directory"] = _delta.svn_delta_editor_t_add_directory_set
    __swig_getmethods__["add_directory"] = _delta.svn_delta_editor_t_add_directory_get
    __swig_setmethods__["open_directory"] = _delta.svn_delta_editor_t_open_directory_set
    __swig_getmethods__["open_directory"] = _delta.svn_delta_editor_t_open_directory_get
    __swig_setmethods__["change_dir_prop"] = _delta.svn_delta_editor_t_change_dir_prop_set
    __swig_getmethods__["change_dir_prop"] = _delta.svn_delta_editor_t_change_dir_prop_get
    __swig_setmethods__["close_directory"] = _delta.svn_delta_editor_t_close_directory_set
    __swig_getmethods__["close_directory"] = _delta.svn_delta_editor_t_close_directory_get
    __swig_setmethods__["absent_directory"] = _delta.svn_delta_editor_t_absent_directory_set
    __swig_getmethods__["absent_directory"] = _delta.svn_delta_editor_t_absent_directory_get
    __swig_setmethods__["add_file"] = _delta.svn_delta_editor_t_add_file_set
    __swig_getmethods__["add_file"] = _delta.svn_delta_editor_t_add_file_get
    __swig_setmethods__["open_file"] = _delta.svn_delta_editor_t_open_file_set
    __swig_getmethods__["open_file"] = _delta.svn_delta_editor_t_open_file_get
    __swig_setmethods__["apply_textdelta"] = _delta.svn_delta_editor_t_apply_textdelta_set
    __swig_getmethods__["apply_textdelta"] = _delta.svn_delta_editor_t_apply_textdelta_get
    __swig_setmethods__["change_file_prop"] = _delta.svn_delta_editor_t_change_file_prop_set
    __swig_getmethods__["change_file_prop"] = _delta.svn_delta_editor_t_change_file_prop_get
    __swig_setmethods__["close_file"] = _delta.svn_delta_editor_t_close_file_set
    __swig_getmethods__["close_file"] = _delta.svn_delta_editor_t_close_file_get
    __swig_setmethods__["absent_file"] = _delta.svn_delta_editor_t_absent_file_set
    __swig_getmethods__["absent_file"] = _delta.svn_delta_editor_t_absent_file_get
    __swig_setmethods__["close_edit"] = _delta.svn_delta_editor_t_close_edit_set
    __swig_getmethods__["close_edit"] = _delta.svn_delta_editor_t_close_edit_get
    __swig_setmethods__["abort_edit"] = _delta.svn_delta_editor_t_abort_edit_set
    __swig_getmethods__["abort_edit"] = _delta.svn_delta_editor_t_abort_edit_get
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for svn_delta_editor_t"""
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
        """__init__(self) -> svn_delta_editor_t"""
        _swig_setattr(self, svn_delta_editor_t, 'this', apply(_delta.new_svn_delta_editor_t, args))
        _swig_setattr(self, svn_delta_editor_t, 'thisown', 1)
    def __del__(self, destroy=_delta.delete_svn_delta_editor_t):
        """__del__(self)"""
        try:
            if self.thisown: destroy(self)
        except: pass


class svn_delta_editor_tPtr(svn_delta_editor_t):
    def __init__(self, this):
        _swig_setattr(self, svn_delta_editor_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, svn_delta_editor_t, 'thisown', 0)
        _swig_setattr(self, svn_delta_editor_t,self.__class__,svn_delta_editor_t)
_delta.svn_delta_editor_t_swigregister(svn_delta_editor_tPtr)


def svn_delta_default_editor(*args):
    """svn_delta_default_editor(apr_pool_t pool) -> svn_delta_editor_t"""
    return apply(_delta.svn_delta_default_editor, args)

def svn_delta_noop_window_handler(*args):
    """svn_delta_noop_window_handler(svn_txdelta_window_t window, void baton) -> svn_error_t"""
    return apply(_delta.svn_delta_noop_window_handler, args)

def svn_delta_get_cancellation_editor(*args):
    """
    svn_delta_get_cancellation_editor(svn_cancel_func_t cancel_func, svn_delta_editor_t wrapped_editor, 
        void wrapped_baton, svn_delta_editor_t editor, 
        void edit_baton, apr_pool_t pool) -> svn_error_t
    """
    return apply(_delta.svn_delta_get_cancellation_editor, args)

def svn_delta_path_driver(*args):
    """
    svn_delta_path_driver(svn_delta_editor_t editor, void edit_baton, svn_revnum_t revision, 
        apr_array_header_t paths, svn_delta_path_driver_cb_func_t callback_func, 
        void callback_baton, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_delta.svn_delta_path_driver, args)
class svn_txdelta_stream_t:
    """Proxy of C svn_txdelta_stream_t struct"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, svn_txdelta_stream_t, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, svn_txdelta_stream_t, name)
    def __init__(self): raise RuntimeError, "No constructor defined"
    def __repr__(self):
        return "<%s.%s; proxy of C svn_txdelta_stream_t instance at %s>" % (self.__class__.__module__, self.__class__.__name__, self.this,)
    def set_parent_pool(self, parent_pool=None):
      """Create a new proxy object for svn_txdelta_stream_t"""
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


class svn_txdelta_stream_tPtr(svn_txdelta_stream_t):
    def __init__(self, this):
        _swig_setattr(self, svn_txdelta_stream_t, 'this', this)
        if not hasattr(self,"thisown"): _swig_setattr(self, svn_txdelta_stream_t, 'thisown', 0)
        _swig_setattr(self, svn_txdelta_stream_t,self.__class__,svn_txdelta_stream_t)
_delta.svn_txdelta_stream_t_swigregister(svn_txdelta_stream_tPtr)


def svn_delta_editor_invoke_set_target_revision(*args):
    """
    svn_delta_editor_invoke_set_target_revision(svn_delta_editor_t _obj, void edit_baton, svn_revnum_t target_revision, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_delta.svn_delta_editor_invoke_set_target_revision, args)

def svn_delta_editor_invoke_open_root(*args):
    """
    svn_delta_editor_invoke_open_root(svn_delta_editor_t _obj, void edit_baton, svn_revnum_t base_revision, 
        apr_pool_t dir_pool, void root_baton) -> svn_error_t
    """
    return apply(_delta.svn_delta_editor_invoke_open_root, args)

def svn_delta_editor_invoke_delete_entry(*args):
    """
    svn_delta_editor_invoke_delete_entry(svn_delta_editor_t _obj, char path, svn_revnum_t revision, 
        void parent_baton, apr_pool_t pool) -> svn_error_t
    """
    return apply(_delta.svn_delta_editor_invoke_delete_entry, args)

def svn_delta_editor_invoke_add_directory(*args):
    """
    svn_delta_editor_invoke_add_directory(svn_delta_editor_t _obj, char path, void parent_baton, 
        char copyfrom_path, svn_revnum_t copyfrom_revision, 
        apr_pool_t dir_pool, void child_baton) -> svn_error_t
    """
    return apply(_delta.svn_delta_editor_invoke_add_directory, args)

def svn_delta_editor_invoke_open_directory(*args):
    """
    svn_delta_editor_invoke_open_directory(svn_delta_editor_t _obj, char path, void parent_baton, 
        svn_revnum_t base_revision, apr_pool_t dir_pool, 
        void child_baton) -> svn_error_t
    """
    return apply(_delta.svn_delta_editor_invoke_open_directory, args)

def svn_delta_editor_invoke_change_dir_prop(*args):
    """
    svn_delta_editor_invoke_change_dir_prop(svn_delta_editor_t _obj, void dir_baton, char name, 
        svn_string_t value, apr_pool_t pool) -> svn_error_t
    """
    return apply(_delta.svn_delta_editor_invoke_change_dir_prop, args)

def svn_delta_editor_invoke_close_directory(*args):
    """svn_delta_editor_invoke_close_directory(svn_delta_editor_t _obj, void dir_baton, apr_pool_t pool) -> svn_error_t"""
    return apply(_delta.svn_delta_editor_invoke_close_directory, args)

def svn_delta_editor_invoke_absent_directory(*args):
    """
    svn_delta_editor_invoke_absent_directory(svn_delta_editor_t _obj, char path, void parent_baton, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_delta.svn_delta_editor_invoke_absent_directory, args)

def svn_delta_editor_invoke_add_file(*args):
    """
    svn_delta_editor_invoke_add_file(svn_delta_editor_t _obj, char path, void parent_baton, 
        char copy_path, svn_revnum_t copy_revision, 
        apr_pool_t file_pool, void file_baton) -> svn_error_t
    """
    return apply(_delta.svn_delta_editor_invoke_add_file, args)

def svn_delta_editor_invoke_open_file(*args):
    """
    svn_delta_editor_invoke_open_file(svn_delta_editor_t _obj, char path, void parent_baton, 
        svn_revnum_t base_revision, apr_pool_t file_pool, 
        void file_baton) -> svn_error_t
    """
    return apply(_delta.svn_delta_editor_invoke_open_file, args)

def svn_delta_editor_invoke_apply_textdelta(*args):
    """
    svn_delta_editor_invoke_apply_textdelta(svn_delta_editor_t _obj, void file_baton, char base_checksum, 
        apr_pool_t pool, svn_txdelta_window_handler_t handler, 
        void handler_baton) -> svn_error_t
    """
    return apply(_delta.svn_delta_editor_invoke_apply_textdelta, args)

def svn_delta_editor_invoke_change_file_prop(*args):
    """
    svn_delta_editor_invoke_change_file_prop(svn_delta_editor_t _obj, void file_baton, char name, 
        svn_string_t value, apr_pool_t pool) -> svn_error_t
    """
    return apply(_delta.svn_delta_editor_invoke_change_file_prop, args)

def svn_delta_editor_invoke_close_file(*args):
    """
    svn_delta_editor_invoke_close_file(svn_delta_editor_t _obj, void file_baton, char text_checksum, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_delta.svn_delta_editor_invoke_close_file, args)

def svn_delta_editor_invoke_absent_file(*args):
    """
    svn_delta_editor_invoke_absent_file(svn_delta_editor_t _obj, char path, void parent_baton, 
        apr_pool_t pool) -> svn_error_t
    """
    return apply(_delta.svn_delta_editor_invoke_absent_file, args)

def svn_delta_editor_invoke_close_edit(*args):
    """svn_delta_editor_invoke_close_edit(svn_delta_editor_t _obj, void edit_baton, apr_pool_t pool) -> svn_error_t"""
    return apply(_delta.svn_delta_editor_invoke_close_edit, args)

def svn_delta_editor_invoke_abort_edit(*args):
    """svn_delta_editor_invoke_abort_edit(svn_delta_editor_t _obj, void edit_baton, apr_pool_t pool) -> svn_error_t"""
    return apply(_delta.svn_delta_editor_invoke_abort_edit, args)

