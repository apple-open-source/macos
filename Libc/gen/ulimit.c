#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <ulimit.h>
#include <stdarg.h>
#include <errno.h>

long int ulimit( int cmd, ... )
{
	va_list ap;
	struct rlimit rlim;

	switch (cmd ) {
	case UL_GETFSIZE:
		if( getrlimit( RLIMIT_FSIZE, &rlim ) < 0 )
			return -1;
		return rlim.rlim_cur/512;
	case UL_SETFSIZE:
		va_start(ap, cmd);
		rlim.rlim_cur = 512 * va_arg(ap, long int);
		rlim.rlim_max = rlim.rlim_cur;
		va_end(ap);
		return setrlimit( RLIMIT_FSIZE, &rlim );
	default:
		errno = EINVAL;
		return -1;
	}
	/* NOT REACHED */
	errno = EINVAL;
	return -1;
}
