/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/rendition/vmisc.h,v 1.3 2000/03/31 20:13:27 dawes Exp $ */

#ifndef __VMISC_H__
#define __VMISC_H__

#include "rendition.h"
#include "vtypes.h"
#include "vos.h"

void verite_bustomem_cpy (vu8 *, vu8 *, vu32);
void verite_memtobus_cpy (vu8 *, vu8 *, vu32);

#endif /* __VMISC_H__ */
