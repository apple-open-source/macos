#include <dotneato.h>

char *Info[] = {
    "libdotneato",      /* Program */
    VERSION,            /* Version */
    BUILDDATE           /* Build Date */
};

GVC_t *gvContext()
{
	return gvNEWcontext(Info, "");
}
