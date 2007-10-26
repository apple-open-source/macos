#if !defined(NV_INF) && defined(USE_LONG_DOUBLE) && defined(LDBL_INFINITY)
#  define NV_INF LDBL_INFINITY
#endif
#if !defined(NV_INF) && defined(DBL_INFINITY)
#  define NV_INF (NV)DBL_INFINITY
#endif
#if !defined(NV_INF) && defined(INFINITY)
#  define NV_INF (NV)INFINITY
#endif
#if !defined(NV_INF) && defined(INF)
#  define NV_INF (NV)INF
#endif
#if !defined(NV_INF) && defined(USE_LONG_DOUBLE) && defined(HUGE_VALL)
#  define NV_INF (NV)HUGE_VALL
#endif
#if !defined(NV_INF) && defined(HUGE_VAL)
#  define NV_INF (NV)HUGE_VAL
#endif

#if !defined(NV_NAN) && defined(USE_LONG_DOUBLE)
#   if !defined(NV_NAN) && defined(LDBL_NAN)
#       define NV_NAN LDBL_NAN
#   endif
#   if !defined(NV_NAN) && defined(LDBL_QNAN)
#       define NV_NAN LDBL_QNAN
#   endif
#   if !defined(NV_NAN) && defined(LDBL_SNAN)
#       define NV_NAN LDBL_SNAN
#   endif
#endif
#if !defined(NV_NAN) && defined(DBL_NAN)
#  define NV_NAN (NV)DBL_NAN
#endif
#if !defined(NV_NAN) && defined(DBL_QNAN)
#  define NV_NAN (NV)DBL_QNAN
#endif
#if !defined(NV_NAN) && defined(DBL_SNAN)
#  define NV_NAN (NV)DBL_SNAN
#endif
#if !defined(NV_NAN) && defined(QNAN)
#  define NV_NAN (NV)QNAN
#endif
#if !defined(NV_NAN) && defined(SNAN)
#  define NV_NAN (NV)SNAN
#endif
#if !defined(NV_NAN) && defined(NAN)
#  define NV_NAN (NV)NAN
#endif
