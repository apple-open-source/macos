/*
 * tiffInit.h --
 */

#include <tifftcl.h>

/*
 * Declarations shared between the .c files of the TIFF format handler.
 */

extern int     TkimgTIFFInitZip   _ANSI_ARGS_((TIFF *, int));
extern int     TkimgTIFFInitJpeg  _ANSI_ARGS_((TIFF *, int));
extern int     TkimgTIFFInitPixar _ANSI_ARGS_((TIFF *, int));

extern void    TkimgTIFFfree    _ANSI_ARGS_((tdata_t data));
extern tdata_t TkimgTIFFmalloc  _ANSI_ARGS_((tsize_t size));
extern tdata_t TkimgTIFFrealloc _ANSI_ARGS_((tdata_t data, tsize_t size));

#ifndef CONST84
#define CONST84
#endif
