/* LGets.h */

#ifdef HAVE_LIBREADLINE
char *ReadlineGets(char *, char *, size_t);
#endif	/* HAVE_LIBREADLINE */

#ifdef HAVE_LIBGETLINE
char *GetlineGets(char *, char *, size_t);
#endif	/* HAVE_LIBGETLINE */

char *LineModeGets(char *, char *, size_t);
char *StdioGets(char *, char *, size_t);
