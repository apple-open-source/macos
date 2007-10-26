/* Proxy classes for svn_repos.h
 * DO NOT EDIT -- AUTOMATICALLY GENERATED */

/* No default constructors for opaque structs */
#ifdef SWIGPYTHON
%nodefault svn_repos_t;
%nodefault svn_authz_t;
#endif

/* Includes */
#ifdef SWIGPYTHON
%import proxy.swg
%import apr.swg
%include svn_fs_h.swg
%include svn_delta_h.swg
%include svn_types_h.swg
%include svn_error_h.swg
%include svn_version_h.swg
#endif
%{
#include "svn_repos.h"
%}
%include svn_repos.h

/* Structure definitions */
#ifdef SWIGPYTHON
%opaque_proxy(svn_repos_t);
%proxy(svn_repos_node_t);
%proxy(svn_repos_parse_fns2_t);
%proxy(svn_repos_parse_fns_t);
%opaque_proxy(svn_authz_t);
#endif

/* Callbacks */

%inline %{
static svn_error_t *svn_repos_parse_fns2_invoke_new_revision_record(
  svn_repos_parse_fns2_t *_obj, void **revision_baton, apr_hash_t *headers, void *parse_baton, apr_pool_t *pool) {
  return _obj->new_revision_record(revision_baton, headers, parse_baton, pool);
}

static svn_error_t *svn_repos_parse_fns2_invoke_uuid_record(
  svn_repos_parse_fns2_t *_obj, const char *uuid, void *parse_baton, apr_pool_t *pool) {
  return _obj->uuid_record(uuid, parse_baton, pool);
}

static svn_error_t *svn_repos_parse_fns2_invoke_new_node_record(
  svn_repos_parse_fns2_t *_obj, void **node_baton, apr_hash_t *headers, void *revision_baton, apr_pool_t *pool) {
  return _obj->new_node_record(node_baton, headers, revision_baton, pool);
}

static svn_error_t *svn_repos_parse_fns2_invoke_set_revision_property(
  svn_repos_parse_fns2_t *_obj, void *revision_baton, const char *name, const svn_string_t *value) {
  return _obj->set_revision_property(revision_baton, name, value);
}

static svn_error_t *svn_repos_parse_fns2_invoke_set_node_property(
  svn_repos_parse_fns2_t *_obj, void *node_baton, const char *name, const svn_string_t *value) {
  return _obj->set_node_property(node_baton, name, value);
}

static svn_error_t *svn_repos_parse_fns2_invoke_delete_node_property(
  svn_repos_parse_fns2_t *_obj, void *node_baton, const char *name) {
  return _obj->delete_node_property(node_baton, name);
}

static svn_error_t *svn_repos_parse_fns2_invoke_remove_node_props(
  svn_repos_parse_fns2_t *_obj, void *node_baton) {
  return _obj->remove_node_props(node_baton);
}

static svn_error_t *svn_repos_parse_fns2_invoke_set_fulltext(
  svn_repos_parse_fns2_t *_obj, svn_stream_t **stream, void *node_baton) {
  return _obj->set_fulltext(stream, node_baton);
}

static svn_error_t *svn_repos_parse_fns2_invoke_apply_textdelta(
  svn_repos_parse_fns2_t *_obj, svn_txdelta_window_handler_t *handler, void **handler_baton, void *node_baton) {
  return _obj->apply_textdelta(handler, handler_baton, node_baton);
}

static svn_error_t *svn_repos_parse_fns2_invoke_close_node(
  svn_repos_parse_fns2_t *_obj, void *node_baton) {
  return _obj->close_node(node_baton);
}

static svn_error_t *svn_repos_parse_fns2_invoke_close_revision(
  svn_repos_parse_fns2_t *_obj, void *revision_baton) {
  return _obj->close_revision(revision_baton);
}

%}
