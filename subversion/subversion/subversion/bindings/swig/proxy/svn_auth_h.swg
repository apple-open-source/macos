/* Proxy classes for svn_auth.h
 * DO NOT EDIT -- AUTOMATICALLY GENERATED */

/* No default constructors for opaque structs */
#ifdef SWIGPYTHON
%nodefault svn_auth_baton_t;
%nodefault svn_auth_iterstate_t;
#endif

/* Includes */
#ifdef SWIGPYTHON
%import proxy.swg
%import apr.swg
%include svn_types_h.swg
#endif
%{
#include "svn_auth.h"
%}
%include svn_auth.h

/* Structure definitions */
#ifdef SWIGPYTHON
%opaque_proxy(svn_auth_baton_t);
%opaque_proxy(svn_auth_iterstate_t);
%proxy(svn_auth_provider_t);
%proxy(svn_auth_provider_object_t);
%proxy(svn_auth_cred_simple_t);
%proxy(svn_auth_cred_username_t);
%proxy(svn_auth_cred_ssl_client_cert_t);
%proxy(svn_auth_cred_ssl_client_cert_pw_t);
%proxy(svn_auth_ssl_server_cert_info_t);
%proxy(svn_auth_cred_ssl_server_trust_t);
#endif

/* Callbacks */

%inline %{
%}
