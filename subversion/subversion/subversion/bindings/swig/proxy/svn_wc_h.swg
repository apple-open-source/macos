/* Proxy classes for svn_wc.h
 * DO NOT EDIT -- AUTOMATICALLY GENERATED */

/* No default constructors for opaque structs */
#ifdef SWIGPYTHON
%nodefault svn_wc_adm_access_t;
%nodefault svn_wc_traversal_info_t;
#endif

/* Includes */
#ifdef SWIGPYTHON
%import proxy.swg
%import apr.swg
%include svn_types_h.swg
%include svn_string_h.swg
%include svn_delta_h.swg
%include svn_error_h.swg
%include svn_opt_h.swg
%include svn_ra_h.swg
#endif
%{
#include "svn_wc.h"
%}
%include svn_wc.h

/* Structure definitions */
#ifdef SWIGPYTHON
%opaque_proxy(svn_wc_adm_access_t);
%opaque_proxy(svn_wc_traversal_info_t);
%proxy(svn_wc_external_item_t);
%proxy(svn_wc_notify_t);
%proxy(svn_wc_diff_callbacks2_t);
%proxy(svn_wc_diff_callbacks_t);
%proxy(svn_wc_entry_t);
%proxy(svn_wc_entry_callbacks_t);
%proxy(svn_wc_status2_t);
%proxy(svn_wc_status_t);
%proxy(svn_wc_revision_status_t);
#endif

/* Callbacks */

%inline %{
static svn_error_t *svn_wc_diff_callbacks2_invoke_file_changed(
  svn_wc_diff_callbacks2_t *_obj, svn_wc_adm_access_t *adm_access, svn_wc_notify_state_t *contentstate, svn_wc_notify_state_t *propstate, const char *path, const char *tmpfile1, const char *tmpfile2, svn_revnum_t rev1, svn_revnum_t rev2, const char *mimetype1, const char *mimetype2, const apr_array_header_t *propchanges, apr_hash_t *originalprops, void *diff_baton) {
  return _obj->file_changed(adm_access, contentstate, propstate, path, tmpfile1, tmpfile2, rev1, rev2, mimetype1, mimetype2, propchanges, originalprops, diff_baton);
}

static svn_error_t *svn_wc_diff_callbacks2_invoke_file_added(
  svn_wc_diff_callbacks2_t *_obj, svn_wc_adm_access_t *adm_access, svn_wc_notify_state_t *contentstate, svn_wc_notify_state_t *propstate, const char *path, const char *tmpfile1, const char *tmpfile2, svn_revnum_t rev1, svn_revnum_t rev2, const char *mimetype1, const char *mimetype2, const apr_array_header_t *propchanges, apr_hash_t *originalprops, void *diff_baton) {
  return _obj->file_added(adm_access, contentstate, propstate, path, tmpfile1, tmpfile2, rev1, rev2, mimetype1, mimetype2, propchanges, originalprops, diff_baton);
}

static svn_error_t *svn_wc_diff_callbacks2_invoke_file_deleted(
  svn_wc_diff_callbacks2_t *_obj, svn_wc_adm_access_t *adm_access, svn_wc_notify_state_t *state, const char *path, const char *tmpfile1, const char *tmpfile2, const char *mimetype1, const char *mimetype2, apr_hash_t *originalprops, void *diff_baton) {
  return _obj->file_deleted(adm_access, state, path, tmpfile1, tmpfile2, mimetype1, mimetype2, originalprops, diff_baton);
}

static svn_error_t *svn_wc_diff_callbacks2_invoke_dir_added(
  svn_wc_diff_callbacks2_t *_obj, svn_wc_adm_access_t *adm_access, svn_wc_notify_state_t *state, const char *path, svn_revnum_t rev, void *diff_baton) {
  return _obj->dir_added(adm_access, state, path, rev, diff_baton);
}

static svn_error_t *svn_wc_diff_callbacks2_invoke_dir_deleted(
  svn_wc_diff_callbacks2_t *_obj, svn_wc_adm_access_t *adm_access, svn_wc_notify_state_t *state, const char *path, void *diff_baton) {
  return _obj->dir_deleted(adm_access, state, path, diff_baton);
}

static svn_error_t *svn_wc_diff_callbacks2_invoke_dir_props_changed(
  svn_wc_diff_callbacks2_t *_obj, svn_wc_adm_access_t *adm_access, svn_wc_notify_state_t *state, const char *path, const apr_array_header_t *propchanges, apr_hash_t *original_props, void *diff_baton) {
  return _obj->dir_props_changed(adm_access, state, path, propchanges, original_props, diff_baton);
}

static svn_error_t *svn_wc_diff_callbacks_invoke_file_changed(
  svn_wc_diff_callbacks_t *_obj, svn_wc_adm_access_t *adm_access, svn_wc_notify_state_t *state, const char *path, const char *tmpfile1, const char *tmpfile2, svn_revnum_t rev1, svn_revnum_t rev2, const char *mimetype1, const char *mimetype2, void *diff_baton) {
  return _obj->file_changed(adm_access, state, path, tmpfile1, tmpfile2, rev1, rev2, mimetype1, mimetype2, diff_baton);
}

static svn_error_t *svn_wc_diff_callbacks_invoke_file_added(
  svn_wc_diff_callbacks_t *_obj, svn_wc_adm_access_t *adm_access, svn_wc_notify_state_t *state, const char *path, const char *tmpfile1, const char *tmpfile2, svn_revnum_t rev1, svn_revnum_t rev2, const char *mimetype1, const char *mimetype2, void *diff_baton) {
  return _obj->file_added(adm_access, state, path, tmpfile1, tmpfile2, rev1, rev2, mimetype1, mimetype2, diff_baton);
}

static svn_error_t *svn_wc_diff_callbacks_invoke_file_deleted(
  svn_wc_diff_callbacks_t *_obj, svn_wc_adm_access_t *adm_access, svn_wc_notify_state_t *state, const char *path, const char *tmpfile1, const char *tmpfile2, const char *mimetype1, const char *mimetype2, void *diff_baton) {
  return _obj->file_deleted(adm_access, state, path, tmpfile1, tmpfile2, mimetype1, mimetype2, diff_baton);
}

static svn_error_t *svn_wc_diff_callbacks_invoke_dir_added(
  svn_wc_diff_callbacks_t *_obj, svn_wc_adm_access_t *adm_access, svn_wc_notify_state_t *state, const char *path, svn_revnum_t rev, void *diff_baton) {
  return _obj->dir_added(adm_access, state, path, rev, diff_baton);
}

static svn_error_t *svn_wc_diff_callbacks_invoke_dir_deleted(
  svn_wc_diff_callbacks_t *_obj, svn_wc_adm_access_t *adm_access, svn_wc_notify_state_t *state, const char *path, void *diff_baton) {
  return _obj->dir_deleted(adm_access, state, path, diff_baton);
}

static svn_error_t *svn_wc_diff_callbacks_invoke_props_changed(
  svn_wc_diff_callbacks_t *_obj, svn_wc_adm_access_t *adm_access, svn_wc_notify_state_t *state, const char *path, const apr_array_header_t *propchanges, apr_hash_t *original_props, void *diff_baton) {
  return _obj->props_changed(adm_access, state, path, propchanges, original_props, diff_baton);
}

static svn_error_t *svn_wc_entry_callbacks_invoke_found_entry(
  svn_wc_entry_callbacks_t *_obj, const char *path, const svn_wc_entry_t *entry, void *walk_baton, apr_pool_t *pool) {
  return _obj->found_entry(path, entry, walk_baton, pool);
}

%}
