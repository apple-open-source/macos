/*
 * tiffInit.h --
 */

#include "tifftcl.h"

/*
 * Declarations shared between the .c files of the TIFF format handler.
 */

MODULE_SCOPE int TkimgTIFFInitZip(TIFF *, int);
MODULE_SCOPE int TkimgTIFFInitJpeg(TIFF *, int);
MODULE_SCOPE int TkimgTIFFInitPixar(TIFF *, int);

MODULE_SCOPE void TkimgTIFFfree(tdata_t data);
MODULE_SCOPE tdata_t TkimgTIFFmalloc(tsize_t size);
MODULE_SCOPE tdata_t TkimgTIFFrealloc(tdata_t data, tsize_t size);

#ifndef CONST84
#   define CONST84
#endif
#ifndef CONST86
#   define CONST86
#endif
