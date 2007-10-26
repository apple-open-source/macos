/* Proxy classes for svn_io.h
 * DO NOT EDIT -- AUTOMATICALLY GENERATED */

/* No default constructors for opaque structs */
#ifdef SWIGPYTHON
%nodefault svn_stream_t;
#endif

/* Includes */
#ifdef SWIGPYTHON
%import proxy.swg
%import apr.swg
%include svn_types_h.swg
%include svn_error_h.swg
%include svn_string_h.swg
#endif
%{
#include "svn_io.h"
%}
%include svn_io.h

/* Structure definitions */
#ifdef SWIGPYTHON
%proxy(svn_io_dirent_t);
%opaque_proxy(svn_stream_t);
#endif

/* Callbacks */

%inline %{
%}
