#ifdef __GNUC__
/** Use gcc attribute to check printf fns.  a1 is the 1-based index of
 * the parameter containing the format, and a2 the index of the first
 * argument.  **/
#define PRINTF_ATTRIBUTE(a1, a2) __attribute__ ((format (__printf__, a1, a2)))
#else
#define PRINTF_ATTRIBUTE(a1, a2)
#endif


#ifndef HAVE_DECL_VASPRINTF
int vasprintf(char **ptr, const char *format, va_list ap);
#endif
#ifndef HAVE_DECL_SNPRINTF
int snprintf(char *,size_t ,const char *, ...) PRINTF_ATTRIBUTE(3,4);
#endif
#ifndef HAVE_DECL_ASPRINTF
int asprintf(char **,const char *, ...) PRINTF_ATTRIBUTE(2,3);
#endif

#ifndef HAVE_DECL_VSNPRINTF
int vsnprintf(char *, size_t, const char *, va_list);
#endif
