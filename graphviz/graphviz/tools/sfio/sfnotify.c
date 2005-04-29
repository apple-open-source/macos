#include	"sfhdr.h"


/*	Set the function to be called when a stream is opened or closed
**
**	Written by Kiem-Phong Vo.
*/
#if __STD_C
int sfnotify(void (*notify)(Sfio_t*, int, int))
#else
int sfnotify(notify)
void	(*notify)();
#endif
{
	_Sfnotify = notify;
	return 0;
}
