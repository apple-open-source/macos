#include "m68k/xm-m68k.h"

#define HAVE_STRERROR

/* malloc does better with chunks the size of a page.  */ 

#define OBSTACK_CHUNK_SIZE (getpagesize ())
