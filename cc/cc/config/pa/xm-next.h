#include "pa/xm-pa.h"

#define HAVE_STRERROR

/* malloc does better with chunks the size of a page.  */ 

#define OBSTACK_CHUNK_SIZE (getpagesize ())
