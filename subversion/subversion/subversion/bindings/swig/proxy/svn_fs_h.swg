/* Proxy classes for svn_fs.h
 * DO NOT EDIT -- AUTOMATICALLY GENERATED */

/* No default constructors for opaque structs */
#ifdef SWIGPYTHON
%nodefault svn_fs_t;
%nodefault svn_fs_access_t;
%nodefault svn_fs_id_t;
%nodefault svn_fs_txn_t;
%nodefault svn_fs_root_t;
%nodefault svn_fs_history_t;
#endif

/* Includes */
#ifdef SWIGPYTHON
%import proxy.swg
%import apr.swg
%include svn_types_h.swg
%include svn_error_h.swg
%include svn_delta_h.swg
%include svn_io_h.swg
#endif
%{
#include "svn_fs.h"
%}
%include svn_fs.h

/* Structure definitions */
#ifdef SWIGPYTHON
%opaque_proxy(svn_fs_t);
%opaque_proxy(svn_fs_access_t);
%opaque_proxy(svn_fs_id_t);
%opaque_proxy(svn_fs_txn_t);
%opaque_proxy(svn_fs_root_t);
%proxy(svn_fs_path_change_t);
%opaque_proxy(svn_fs_history_t);
%proxy(svn_fs_dirent_t);
#endif

/* Callbacks */

%inline %{
%}
