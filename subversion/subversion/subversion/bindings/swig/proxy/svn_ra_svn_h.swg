/* Proxy classes for svn_ra_svn.h
 * DO NOT EDIT -- AUTOMATICALLY GENERATED */

/* No default constructors for opaque structs */
#ifdef SWIGPYTHON
%nodefault svn_ra_svn_conn_st;
#endif

/* Includes */
#ifdef SWIGPYTHON
%import proxy.swg
%import apr.swg
%include svn_config_h.swg
%include svn_delta_h.swg
#endif
%{
#include "svn_ra_svn.h"
%}
%include svn_ra_svn.h

/* Structure definitions */
#ifdef SWIGPYTHON
%opaque_proxy(svn_ra_svn_conn_st);
%proxy(svn_ra_svn_cmd_entry_t);
%proxy(svn_ra_svn_item_t);
#endif

/* Callbacks */

%inline %{
%}
