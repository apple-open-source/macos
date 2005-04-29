#include	"sfhdr.h"

#undef sfputm

#if __STD_C
int sfputm(reg Sfio_t* f, Sfulong_t u, Sfulong_t m)
#else
int sfputm(f,u,m)
reg Sfio_t*	f;
reg Sfulong_t	u;
reg Sfulong_t	m;
#endif
{
	return __sf_putm(f,u,m);
}
