/* Misc routines used elsewhere in driver */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/rendition/vmisc.c,v 1.4 2000/03/31 20:13:27 dawes Exp $ */

#include "rendition.h"
#include "vtypes.h"
#include "vos.h"
#include "vmisc.h"

#undef DEBUG

/* block copy from and to the card */
void
verite_bustomem_cpy(vu8 *dst, vu8 *src, vu32 num)
{
    int i;

#ifdef DEBUG
    ErrorF ("Rendition: DEBUG verite_bustomem_cpy called\n");
#endif  
    for (i=0; i<num; i++)
        dst[i] = verite_read_memory8(src, i);
}

void
verite_memtobus_cpy(vu8 *dst, vu8 *src, vu32 num)
{
    int i;

#ifdef DEBUG
    ErrorF ("Rendition: DEBUG verite_memtobus_cpy called\n");
#endif  

    for (i=0; i<num; i++)
        verite_write_memory8(dst, i, src[i]);
}
