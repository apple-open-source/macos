/* Strn.h */

#ifndef _Strn_h_
#define _Strn_h_ 1

char *Strncat(char *, char *, size_t);
char *Strncpy(char *, char *, size_t);

#define STRNCPY(d,s) Strncpy((char *) (d), (char *) (s), (size_t) sizeof(d))
#define STRNCAT(d,s) Strncat((char *) (d), (char *) (s), (size_t) sizeof(d))

#endif	/* _Strn_h_ */

/* eof Strn.h */
