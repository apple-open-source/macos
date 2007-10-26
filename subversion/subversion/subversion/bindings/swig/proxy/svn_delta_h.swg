/* Proxy classes for svn_delta.h
 * DO NOT EDIT -- AUTOMATICALLY GENERATED */

/* No default constructors for opaque structs */
#ifdef SWIGPYTHON
%nodefault svn_txdelta_stream_t;
#endif

/* Includes */
#ifdef SWIGPYTHON
%import proxy.swg
%import apr.swg
%include svn_types_h.swg
%include svn_string_h.swg
%include svn_error_h.swg
%include svn_io_h.swg
%include svn_version_h.swg
#endif
%{
#include "svn_delta.h"
%}
%include svn_delta.h

/* Structure definitions */
#ifdef SWIGPYTHON
%proxy(svn_txdelta_op_t);
%proxy(svn_txdelta_window_t);
%opaque_proxy(svn_txdelta_stream_t);
%proxy(svn_delta_editor_t);
#endif

/* Callbacks */

%inline %{
static svn_error_t *svn_delta_editor_invoke_set_target_revision(
  svn_delta_editor_t *_obj, void *edit_baton, svn_revnum_t target_revision, apr_pool_t *pool) {
  return _obj->set_target_revision(edit_baton, target_revision, pool);
}

static svn_error_t *svn_delta_editor_invoke_open_root(
  svn_delta_editor_t *_obj, void *edit_baton, svn_revnum_t base_revision, apr_pool_t *dir_pool, void **root_baton) {
  return _obj->open_root(edit_baton, base_revision, dir_pool, root_baton);
}

static svn_error_t *svn_delta_editor_invoke_delete_entry(
  svn_delta_editor_t *_obj, const char *path, svn_revnum_t revision, void *parent_baton, apr_pool_t *pool) {
  return _obj->delete_entry(path, revision, parent_baton, pool);
}

static svn_error_t *svn_delta_editor_invoke_add_directory(
  svn_delta_editor_t *_obj, const char *path, void *parent_baton, const char *copyfrom_path, svn_revnum_t copyfrom_revision, apr_pool_t *dir_pool, void **child_baton) {
  return _obj->add_directory(path, parent_baton, copyfrom_path, copyfrom_revision, dir_pool, child_baton);
}

static svn_error_t *svn_delta_editor_invoke_open_directory(
  svn_delta_editor_t *_obj, const char *path, void *parent_baton, svn_revnum_t base_revision, apr_pool_t *dir_pool, void **child_baton) {
  return _obj->open_directory(path, parent_baton, base_revision, dir_pool, child_baton);
}

static svn_error_t *svn_delta_editor_invoke_change_dir_prop(
  svn_delta_editor_t *_obj, void *dir_baton, const char *name, const svn_string_t *value, apr_pool_t *pool) {
  return _obj->change_dir_prop(dir_baton, name, value, pool);
}

static svn_error_t *svn_delta_editor_invoke_close_directory(
  svn_delta_editor_t *_obj, void *dir_baton, apr_pool_t *pool) {
  return _obj->close_directory(dir_baton, pool);
}

static svn_error_t *svn_delta_editor_invoke_absent_directory(
  svn_delta_editor_t *_obj, const char *path, void *parent_baton, apr_pool_t *pool) {
  return _obj->absent_directory(path, parent_baton, pool);
}

static svn_error_t *svn_delta_editor_invoke_add_file(
  svn_delta_editor_t *_obj, const char *path, void *parent_baton, const char *copy_path, svn_revnum_t copy_revision, apr_pool_t *file_pool, void **file_baton) {
  return _obj->add_file(path, parent_baton, copy_path, copy_revision, file_pool, file_baton);
}

static svn_error_t *svn_delta_editor_invoke_open_file(
  svn_delta_editor_t *_obj, const char *path, void *parent_baton, svn_revnum_t base_revision, apr_pool_t *file_pool, void **file_baton) {
  return _obj->open_file(path, parent_baton, base_revision, file_pool, file_baton);
}

static svn_error_t *svn_delta_editor_invoke_apply_textdelta(
  svn_delta_editor_t *_obj, void *file_baton, const char *base_checksum, apr_pool_t *pool, svn_txdelta_window_handler_t *handler, void **handler_baton) {
  return _obj->apply_textdelta(file_baton, base_checksum, pool, handler, handler_baton);
}

static svn_error_t *svn_delta_editor_invoke_change_file_prop(
  svn_delta_editor_t *_obj, void *file_baton, const char *name, const svn_string_t *value, apr_pool_t *pool) {
  return _obj->change_file_prop(file_baton, name, value, pool);
}

static svn_error_t *svn_delta_editor_invoke_close_file(
  svn_delta_editor_t *_obj, void *file_baton, const char *text_checksum, apr_pool_t *pool) {
  return _obj->close_file(file_baton, text_checksum, pool);
}

static svn_error_t *svn_delta_editor_invoke_absent_file(
  svn_delta_editor_t *_obj, const char *path, void *parent_baton, apr_pool_t *pool) {
  return _obj->absent_file(path, parent_baton, pool);
}

static svn_error_t *svn_delta_editor_invoke_close_edit(
  svn_delta_editor_t *_obj, void *edit_baton, apr_pool_t *pool) {
  return _obj->close_edit(edit_baton, pool);
}

static svn_error_t *svn_delta_editor_invoke_abort_edit(
  svn_delta_editor_t *_obj, void *edit_baton, apr_pool_t *pool) {
  return _obj->abort_edit(edit_baton, pool);
}

%}
