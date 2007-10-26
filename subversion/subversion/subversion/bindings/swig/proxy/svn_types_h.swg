/* Proxy classes for svn_types.h
 * DO NOT EDIT -- AUTOMATICALLY GENERATED */

/* No default constructors for opaque structs */
#ifdef SWIGPYTHON
#endif

/* Includes */
#ifdef SWIGPYTHON
%import proxy.swg
%import apr.swg
#endif
%{
#include "svn_types.h"
%}
%include svn_types.h

/* Structure definitions */
#ifdef SWIGPYTHON
%proxy(svn_error_t);
%proxy(svn_dirent_t);
%proxy(svn_commit_info_t);
%proxy(svn_log_changed_path_t);
%proxy(svn_lock_t);
#endif

/* Callbacks */

%inline %{
%}
