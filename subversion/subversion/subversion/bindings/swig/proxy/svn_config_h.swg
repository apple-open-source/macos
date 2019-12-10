/* Proxy classes for svn_config.h
 * DO NOT EDIT -- AUTOMATICALLY GENERATED
 * BY build/generator/swig/header_wrappers.py */

/* No default constructors for opaque structs */
#ifdef SWIGPYTHON
%nodefault svn_config_t;
#endif

/* Callback typemaps */
#ifdef SWIGPYTHON
%apply CALLABLE_CALLBACK {
  svn_config_section_enumerator_t,
  svn_config_section_enumerator2_t,
  svn_config_enumerator_t,
  svn_config_enumerator2_t,
  svn_config_auth_walk_func_t
};
%apply CALLABLE_CALLBACK * {
  svn_config_section_enumerator_t *,
  svn_config_section_enumerator2_t *,
  svn_config_enumerator_t *,
  svn_config_enumerator2_t *,
  svn_config_auth_walk_func_t *
};
#endif

/* Baton typemaps */
#ifdef SWIGPYTHON
%apply void *PY_AS_VOID {
  void *baton,
  void *walk_baton
};
#endif

/* Includes */
%{
#include "svn_config.h"
%}
%include svn_config.h

/* Structure definitions */
#ifdef SWIGPYTHON
%opaque_proxy(svn_config_t);
#endif

/* Callbacks */

%inline %{
static svn_boolean_t svn_config_invoke_section_enumerator(
  svn_config_section_enumerator_t _obj, const char *name, void *baton) {
  return _obj(name, baton);
}

static svn_boolean_t svn_config_invoke_section_enumerator2(
  svn_config_section_enumerator2_t _obj, const char *name, void *baton, apr_pool_t *pool) {
  return _obj(name, baton, pool);
}

static svn_boolean_t svn_config_invoke_enumerator(
  svn_config_enumerator_t _obj, const char *name, const char *value, void *baton) {
  return _obj(name, value, baton);
}

static svn_boolean_t svn_config_invoke_enumerator2(
  svn_config_enumerator2_t _obj, const char *name, const char *value, void *baton, apr_pool_t *pool) {
  return _obj(name, value, baton, pool);
}

static svn_error_t * svn_config_invoke_auth_walk_func(
  svn_config_auth_walk_func_t _obj, svn_boolean_t *delete_cred, void *walk_baton, const char *cred_kind, const char *realmstring, apr_hash_t *hash, apr_pool_t *scratch_pool) {
  return _obj(delete_cred, walk_baton, cred_kind, realmstring, hash, scratch_pool);
}

%}

#ifdef SWIGPYTHON
%funcptr_proxy(svn_config_section_enumerator_t, svn_config_invoke_section_enumerator);
%funcptr_proxy(svn_config_section_enumerator2_t, svn_config_invoke_section_enumerator2);
%funcptr_proxy(svn_config_enumerator_t, svn_config_invoke_enumerator);
%funcptr_proxy(svn_config_enumerator2_t, svn_config_invoke_enumerator2);
%funcptr_proxy(svn_config_auth_walk_func_t, svn_config_invoke_auth_walk_func);

#endif
