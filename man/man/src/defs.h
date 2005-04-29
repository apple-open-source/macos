/* defs.h */
#undef MAXPATHLEN		/* make sure struct dirs has a
				   well-defined size (thanks to
				   Pierre.Humblet@eurecom.fr) */
#include <sys/param.h>

#define MAN 0
#define CAT 1
#define SCAT 2

#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#endif

struct dir {
    struct dir *nxt;
    char *dir;
};

struct dirs {
    struct dirs *nxt;
    char mandir[MAXPATHLEN];
    char catdir[MAXPATHLEN];
    char bindir[MAXPATHLEN];
    int mandatory;
};
