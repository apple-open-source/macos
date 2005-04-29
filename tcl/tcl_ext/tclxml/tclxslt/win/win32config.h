#ifndef __TCLXSLT_WIN32_CONFIG__
#define __TCLXSLT_WIN32_CONFIG__

/* Microsoft's C runtime names all non-ANSI functions with a leading
   underscore. Since functionality is still the same, they can be used. */
#define snprintf _snprintf
#define vsnprintf _vsnprintf

#endif /* __TCLXSLT_WIN32_CONFIG__ */