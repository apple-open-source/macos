#include "sessreg.h"

#ifdef UTMPX_FILE
# define UTF UTMPX_FILE
#else
# define UTF UTMP_FILE
#endif

#ifdef WTMPX_FILE
# define WTF WTMPX_FILE
#else
# define WTF WTMP_FILE
#endif

#ifndef TTYS_FILE
# define TTYS_FILE "/etc/ttys"
#endif

s|__utmp_file__|UTF|g
s|__wtmp_file__|WTF|g
s|__ttys_file__|TTYS_FILE|g
