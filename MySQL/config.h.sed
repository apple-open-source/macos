s/^.*WORDS_BIGENDIAN.*$/\#if __BIG_ENDIAN__\
\#define WORDS_BIGENDIAN __BIG_ENDIAN__\
\#else\
\#undef WORDS_BIGENDIAN\
\#endif/1
s/\#.*SIZEOF_CHARP[[:space:]].*$/\#if defined (__ppc64__) || defined (__x86_64__)\
\#warning defining SIZEOF_CHARP = 8 \
\#define SIZEOF_CHARP 8\
\#elif defined (__ppc__) || defined(__i386__)\
\#warning defining SIZEOF_CHARP = 4 \
\#define SIZEOF_CHARP 4\
\#else\
\#error architecture not supported\
\#endif/1
s/\#.*SIZEOF_LONG[[:space:]].*$/\#if defined (__ppc64__) || defined (__x86_64__)\
\#warning defining SIZEOF_LONG = 8 \
\#define SIZEOF_LONG 8\
\#elif defined (__ppc__) || defined(__i386__)\
\#warning defining SIZEOF_LONG = 4 \
\#define SIZEOF_LONG 4\
\#else\
\#error architecture not supported\
\#endif/1
s/\#.*SIZEOF_VOIDP[[:space:]].*$/\#if defined (__ppc64__) || defined (__x86_64__)\
\#warning defining SIZEOF_VOIDP = 8 \
\#define SIZEOF_VOIDP 8\
\#elif defined (__ppc__) || defined(__i386__)\
\#warning defining SIZEOF_VOIDP = 4 \
\#define SIZEOF_VOIDP 4\
\#else\
\#error architecture not supported\
\#endif/1
