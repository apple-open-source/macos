/* This file is an Xserver-specific version */

/* $XFree86: xc/extras/FreeType/lib/arch/unix/freetype.c,v 1.3 2000/02/13 05:03:56 dawes Exp $ */

/* Single object library component for Unix */

#include "ttapi.c"
#include "ttcache.c"
#include "ttcalc.c"
#include "ttcmap.c"
#include "ttdebug.c"
#include "ttgload.c"
#include "ttinterp.c"
#include "ttload.c"
#include "ttobjs.c"
#include "ttraster.c"

#ifdef HAVE_MMAP
#include "ttmmap.c"
#else
#include "ttfile.c"
#endif
#include "ttmemory.c"
#include "ttmutex.c"

#ifdef TT_CONFIG_OPTION_EXTEND_ENGINE
#include "ttextend.c"
#endif


/* END */
