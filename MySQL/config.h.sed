s/\#.*WORDS_BIGENDIAN.*$/\#if __BIG_ENDIAN__\
\#define WORDS_BIGENDIAN __BIG_ENDIAN__\
\#else\
\#undef WORDS_BIGENDIAN\
\#endif/1
