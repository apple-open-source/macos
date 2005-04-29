#ifndef GETOPT_H
#define GETOPT_H

/* On most systems, these are defined in unistd.h or stdlib.h,
 * but some systems have no external definitions (UTS, SunOS 4.1),
 * so we provide a declaration if needed.
 */

extern int  getopt(int, char *const *, const char *);
extern char *optarg;
extern int  opterr, optind, optopt;

#endif
