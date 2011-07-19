#include "sessreg.h"

#ifdef UTMPX_FILE
# define UTF UTMPX_FILE
# define UTM utmpx
#else
# define UTF UTMP_FILE
# define UTM utmp
#endif

#ifdef WTMPX_FILE
# define WTF WTMPX_FILE
#else
# define WTF WTMP_FILE
#endif

#ifndef TTYS_FILE
# define TTYS_FILE "/etc/ttys"
#endif

#ifndef LLOG_FILE
# define LLOG_FILE "/var/log/lastlog"
#endif

s|__utmp_manpage__|UTM|g
s|__utmp_file__|UTF|g
s|__wtmp_file__|WTF|g
s|__ttys_file__|TTYS_FILE|g
s|__lastlog_file__|LLOG_FILE|g
