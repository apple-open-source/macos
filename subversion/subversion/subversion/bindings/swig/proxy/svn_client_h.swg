/* Proxy classes for svn_client.h
 * DO NOT EDIT -- AUTOMATICALLY GENERATED */

/* No default constructors for opaque structs */
#ifdef SWIGPYTHON
#endif

/* Includes */
#ifdef SWIGPYTHON
%import proxy.swg
%import apr.swg
%include svn_types_h.swg
%include svn_wc_h.swg
%include svn_string_h.swg
%include svn_error_h.swg
%include svn_opt_h.swg
%include svn_version_h.swg
%include svn_ra_h.swg
%include svn_diff_h.swg
#endif
%{
#include "svn_client.h"
%}
%include svn_client.h

/* Structure definitions */
#ifdef SWIGPYTHON
%proxy(svn_client_proplist_item_t);
%proxy(svn_client_commit_info_t);
%proxy(svn_client_commit_item2_t);
%proxy(svn_client_commit_item_t);
%proxy(svn_client_diff_summarize_t);
%proxy(svn_client_ctx_t);
%proxy(svn_info_t);
#endif

/* Callbacks */

%inline %{
%}
