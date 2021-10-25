/* $Id: getopt.h,v 4.3 2013/10/25 22:08:17 tom Exp $ */

#ifndef OLD_GETOPT_H
#define OLD_GETOPT_H 1

extern char *optarg;
extern int optind, opterr;
extern int getopt (int argc, char **argv, const char *options);

#endif /* OLD_GETOPT_H */
