/* STARTHEADER
 *
 * File :       sgi.c
 *
 * Author :     Paul Obermeier (paul@poSoft.de)
 *
 * Date :       Wed Nov 22 21:45:17 CET 2000
 *
 * Copyright :  (C) 2000-2002 Paul Obermeier
 *
 * Description :
 *
 * A photo image handler for Silicon Graphics' native file format.
 *
 * The following image types are supported:
 *
 * 24-bit pixels: True-color (RGB, each channel 8 bit).
 * 32-bit pixels: True-color with alpha channel (RGBA, each channel 8 bit).
 * 48-bit pixels: True-color (RGB, each channel 16 bit).
 * 64-bit pixels: True-color with alpha channel (RGBA, each channel 16 bit).
 *
 * List of currently supported features:
 *
 * Type   |     Read      |     Write     |
 *        | -file | -data | -file | -data |
 * ----------------------------------------
 * 24-bit | Yes   | Yes*  | Yes   | Yes*  |
 * 32-bit | Yes   | Yes*  | Yes   | Yes*  |
 * 48-bit | Yes   | Yes*  | No    | No    |
 * 64-bit | Yes   | Yes*  | No    | No    |
 *
 * *: Implemented by reading/writing from/to a temporary file. This will
 *    be slow for larger images.
 *
 * All images types may be either uncompressed or run-length encoded.
 *
 *
 * The following format options are available:
 *
 * Read  SGI image: "sgi -matte <bool> -verbose <bool>"
 * Write SGI image: "sgi -matte <bool> -verbose <bool> -compression <type>"
 *
 * -matte <bool>:       If set to false, a matte (alpha) channel is ignored
 *                      during reading or writing. Default is true.
 * -verbose <bool>:     If set to true, additional information about the file
 *                      format is printed to stdout. Default is false.
 * -compression <type>: Set the compression mode to either "none" or "rle".
 *                      Default is "rle".
 *
 * Notes:
 *
 * - Parts of this code are taken from Paul Haeberli's original
 *   image library code, written in 1984.
 *
 * - Due to the heavy use of file seeks in Haeberli's code and the behaviour
 *   of Tcl_Seek on Windows when writing to files (sounds like smashing your
 *   HD), there is some workaround to use fseek and fwrite instead.
 *   See "#define TCLSEEK_WORKAROUND".
 *
 * ENDHEADER
 *
 * $Id: sgi.c 175 2008-12-01 15:41:34Z nijtmans $
 *
 */

/* #define DEBUG_LOCAL */

/*
 * Generic initialization code, parameterized via CPACKAGE and PACKAGE.
 */

#include "init.c"


/* OPA TODO: Change from ANSI-C arguments to _ANSI_ARGS_ macro. */

#if defined (WIN32)
    #define TCLSEEK_WORKAROUND
#endif

#if defined (TCLSEEK_WORKAROUND)
    static int ioMode = 0; /* Needed for Windows patch */

    static int MyWrite (Tcl_Channel chan, char *buf, int size)
    {
	if (1 == fwrite(buf, size, 1, (FILE *)chan)) {
	    return size;
	} else {
	    return -1;
	}
    }

    static int MyClose (Tcl_Interp *interp, Tcl_Channel chan)
    {
	if (0 == fclose((FILE *)chan)) {
	    return TCL_OK;
	} else {
	    return TCL_ERROR;
	}
    }

    static int MySeek (Tcl_Channel chan, int offset, int seekMode)
    {
	if (ioMode == 0) { /* Read mode */
	    return Tcl_Seek (chan, offset, seekMode);
	} else {
	    return fseek((FILE *)chan, offset, seekMode);
	}
    }

    #define MYCHANNEL Tcl_Channel
    #undef Tcl_Seek
    #define Tcl_Seek  MySeek
    #undef Tcl_Write
    #define Tcl_Write MyWrite
    #define MYCLOSE   MyClose
#else
    #define MYCHANNEL Tcl_Channel
    #define MYCLOSE   Tcl_Close
#endif

/* Some defines and typedefs for compatibility reasons. */
#define TRUE  1
#define FALSE 0
typedef unsigned char Boln;	/* Boolean value: TRUE or FALSE */
typedef unsigned char UByte;	/* Unsigned  8 bit integer */
typedef char  Byte;		/* Signed    8 bit integer */
typedef short Short;		/* Signed   16 bit integer */
typedef unsigned short UShort;	/* Unsigned 16 bit integer */
typedef int Int;		/* Signed   32 bit integer */
typedef unsigned int UInt;	/* Unsigned 32 bit integer */


/* Start of original code from SGI image library, slightly modified. */

#define IMAGIC        0x01DA

/* colormap of images */
#define CM_NORMAL               0       /* file contains rows of values which
                                         * are either RGB values (zsize == 3)
                                         * or greyramp values (zsize == 1) */
#define CM_DITHERED             1
#define CM_SCREEN               2       /* file contains data which is a screen
                                         * image; getrow returns buffer which
                                         * can be displayed directly with
                                         * writepixels */
#define CM_COLORMAP             3       /* a colormap file */

#define TYPEMASK                0xff00
#define BPPMASK                 0x00ff
#define ITYPE_UNCOMPRESSED      0x0000
#define ITYPE_RLE               0x0100
#define ISRLE(type)             (((type) & 0xff00) == ITYPE_RLE)
#define ISUNCOMPRESSED(type)    (((type) & 0xff00) == ITYPE_UNCOMPRESSED)
#define BPP(type)               ((type) & BPPMASK)
#define RLE(bpp)                (ITYPE_RLE | (bpp))
#define UNCOMPRESSED(bpp)       (ITYPE_UNCOMPRESSED | (bpp))
#define IBUFSIZE(pixels)        ((pixels+(pixels>>6))<<2)
#define RLE_NOP                 0x00

#define ierror(p)               (((p)->flags&_IOERR)!=0)
#define ifileno(p)              ((p)->file)
#define getpix(p)               (--(p)->cnt>=0 ? *(p)->ptr++ : ifilbuf(p))
#define putpix(p,x)             (--(p)->cnt>=0 \
                                    ? ((int)(*(p)->ptr++=(unsigned)(x))) \
                                    : iflsbuf(p,(unsigned)(x)))

/* The number of bytes of the IMAGE struct, which must be written to disk.
 * All other information is needed only internally. It is filled with zeros
 * on disk. */
#define RELEVANT_HEADER_BYTES 108

typedef struct {
    UShort    imagic;         /* stuff saved on disk . . */
    UShort    type;
    UShort    dim;
    UShort    xsize;
    UShort    ysize;
    UShort    zsize;
    UInt      min;
    UInt      max;
    UInt      wastebytes;
    char      name[80];
    UInt      colormap;

    MYCHANNEL file;	/* Stuff not stored in the file. */
    UShort    flags;
    Short     dorev;
    Short     x;
    Short     y;
    Short     z;
    Short     cnt;
    UShort    *ptr;
    UShort    *base;
    UShort    *tmpbuf;
    UInt      offset;
    UInt      rleend;         /* for rle images */
    UInt      *rowstart;      /* for rle images */
    Int       *rowsize;       /* for rle images */
    char      dummy[512-146]; /* Fill bytes, so that this structure is greater
                                 than 512 bytes */
} IMAGE;

#if !defined (_IOWRT)
    #define _IOWRT  1
#endif
#if !defined (_IOREAD)
    #define _IOREAD 2
#endif
#if !defined (_IORW)
    #define _IORW   4
#endif
#if !defined (_IOERR)
    #define _IOERR  8
#endif
#if !defined (_IOEOF)
    #define _IOEOF 16
#endif

static int img_badrow(IMAGE *image, unsigned int y, unsigned int z);
static int img_write(IMAGE *image, char *buffer,int count);
static int img_writeheader(IMAGE *image);
static int iflush(IMAGE *image);
static unsigned short *ibufalloc(IMAGE *image);
static unsigned int img_optseek(IMAGE *image, unsigned int offset);
static int imgopen(int, MYCHANNEL, IMAGE *, const char *,unsigned int, unsigned int,
		unsigned int, unsigned int, unsigned int);
static int getrow(IMAGE *image, unsigned short *buffer,
		unsigned int y, unsigned int z);
static int putrow(IMAGE *image, unsigned short *buffer,
		unsigned int y, unsigned int z);

/*	error handler for the image library.  If the iseterror() routine
	has been called, sprintf's the args into a string and calls the
	error function.  Otherwise calls fprintf with the args and then
	exit.  This allows 'old' programs to assume that no errors
	ever need be worried about, while programs that know how and
	want to can handle the errors themselves.  Olson, 11/88
*/
static void i_errhdlr(fmt, a1, a2, a3, a4)  /* most args currently used is 2 */
char *fmt;
{
    /* fprintf(stderr, fmt); */
    return;
}

/*
 *	isetname and isetcolormap -
 *
 *				Paul Haeberli - 1984
 *
 */

static void isetname(IMAGE *image, const char *name)
{
    strncpy(image->name,name,80);
}

/* This function is commented out because it is not used anywhere
static void isetcolormap(IMAGE *image, int colormap)
{
    image->colormap = colormap;
}
*/

static void cvtshorts( buffer, n)
register unsigned short buffer[];
register int n;
{
    register short i;
    register int nshorts = n>>1;
    register unsigned short swrd;

    for(i=0; i<nshorts; i++) {
	swrd = *buffer;
	*buffer++ = (swrd>>8) | (swrd<<8);
    }
}

static void cvtlongs( buffer, n)
register int buffer[];
register int n;
{
    register short i;
    register int nlongs = n>>2;
    register int lwrd;
    Byte *bytePtr;

    bytePtr = (Byte *) buffer;
    for(i=0; i<nlongs; i++) {
	lwrd = buffer[i];
	*bytePtr = (Byte) (lwrd >> 24); bytePtr++;
	*bytePtr = (Byte) (lwrd >> 16); bytePtr++;
	*bytePtr = (Byte) (lwrd >> 8);  bytePtr++;
	*bytePtr = (Byte) (lwrd);       bytePtr++;
    }
}

static void cvtimage( buffer )
int buffer[];
{
    cvtshorts((unsigned short *)buffer,12);
    cvtlongs(buffer+3,12);
}

/*
 *	iopen -
 *
 *				Paul Haeberli - 1984
 *
 */

static unsigned short *ibufalloc(IMAGE *image)
{
    return (unsigned short *)malloc(IBUFSIZE(image->xsize));
}

static int imgOpenRead (MYCHANNEL file, IMAGE *image, const char *mode)
{
    #if defined (TCLSEEK_WORKAROUND)
	ioMode = 0;
    #endif
    return imgopen (0, file, image, mode, 0, 0, 0, 0, 0);
}

static int imgOpenWrite (MYCHANNEL file, IMAGE *image, const char *mode,
                     unsigned int type, unsigned int dim,
                     unsigned int xsize, unsigned int ysize, unsigned int zsize)
{
    #if defined (TCLSEEK_WORKAROUND)
	ioMode = 1;
    #endif
    return imgopen (0, file, image, mode, type, dim, xsize, ysize, zsize);
}

static int imgopen(int f, MYCHANNEL file, IMAGE *image, const char *mode,
		unsigned int type, unsigned int dim,
		unsigned int xsize, unsigned int ysize, unsigned int zsize)
{
    register int rw;
    int tablesize;
    register int i, max;

    rw = mode[1] == '+';
    if(rw) {
	i_errhdlr("iopen: read/write mode not supported\n");
	return 0;
    }
    if (*mode=='w') {
	image->type = type;
	image->xsize = xsize;
	image->ysize = 1;
	image->zsize = 1;
	if (dim>1)
	    image->ysize = ysize;
	if (dim>2)
	    image->zsize = zsize;
	if(image->zsize == 1) {
	    image->dim = 2;
	    if(image->ysize == 1)
		image->dim = 1;
	} else {
	    image->dim = 3;
	}
	image->min = 10000000;
	image->max = 0;
	isetname(image,"no name");
	image->wastebytes = 0;
	if (512 != Tcl_Write (file, (char *)image, 512)) {
	    i_errhdlr("iopen: error on write of image header\n");
	    return 0;
	}
    } else {
	if (512 != Tcl_Read (file, (char *)image, 512)) {
	    i_errhdlr("iopen: error on read of image header\n");
	    return 0;
	}
	if( ((image->imagic>>8) | ((image->imagic&0xff)<<8)) == IMAGIC ) {
	    image->dorev = 1;
	    cvtimage((int *)image);
	} else
	    image->dorev = 0;
	if (image->imagic != IMAGIC) {
	    i_errhdlr("iopen: bad magic in image file %x\n",image->imagic);
	    return 0;
	}
    }
    if (rw)
	image->flags = _IORW;
    else if (*mode != 'r')
	image->flags = _IOWRT;
    else
	image->flags = _IOREAD;
    if(ISRLE(image->type)) {
	tablesize = image->ysize*image->zsize*sizeof(int);
	image->rowstart = (unsigned int *)malloc(tablesize);
	image->rowsize = (int *)malloc(tablesize);
	if( image->rowstart == 0 || image->rowsize == 0 ) {
	    i_errhdlr("iopen: error on table alloc\n");
	    return 0;
	}
	image->rleend = 512L+2*tablesize;
	if (*mode=='w') {
	    max = image->ysize*image->zsize;
	    for(i=0; i<max; i++) {
		image->rowstart[i] = 0;
		image->rowsize[i] = -1;
	    }
	} else {
	    tablesize = image->ysize*image->zsize*sizeof(int);
	    Tcl_Seek (file, 512L, 0);
	    if (tablesize != Tcl_Read (file, (char *)image->rowstart, tablesize)) {
		i_errhdlr("iopen: error on read of rowstart\n");
		return 0;
	    }
	    if(image->dorev)
		cvtlongs(image->rowstart,tablesize);
	    if (Tcl_Read (file, (char *)image->rowsize, tablesize) != tablesize) {
		i_errhdlr("iopen: error on read of rowsize\n");
		return 0;
	    }
	    if(image->dorev)
		cvtlongs(image->rowsize,tablesize);
	}
    }
    image->cnt = 0;
    image->ptr = 0;
    image->base = 0;
    if( (image->tmpbuf = ibufalloc(image)) == 0 ) {
	i_errhdlr("iopen: error on tmpbuf alloc %d\n",image->xsize);
	return 0;
    }
    image->x = image->y = image->z = 0;
    image->file = file;
    image->offset = 512L;			/* set up for img_optseek */
    Tcl_Seek (image->file, 512L, 0);
    return 1;
}


/*
 *	iclose and iflush -
 *
 *				Paul Haeberli - 1984
 *
 */

static int iclose(IMAGE *image)
{
    int tablesize;

    iflush(image);
    img_optseek(image, 0);
    if (image->flags&_IOWRT) {
	if(image->dorev)
	    cvtimage((int *)image);
	if ( !img_writeheader(image)) {
	    i_errhdlr("iclose: error on write of image header\n");
	    return EOF;
	}
	if(image->dorev)
	    cvtimage((int *)image);
	if(ISRLE(image->type)) {
	    img_optseek(image, 512L);
	    tablesize = image->ysize*image->zsize*sizeof(int);
	    if(image->dorev)
		cvtlongs(image->rowstart,tablesize);
	    if (img_write(image,(char *)(image->rowstart),tablesize) != tablesize) {
		i_errhdlr("iclose: error on write of rowstart\n");
		return EOF;
	    }
	    if(image->dorev)
		cvtlongs(image->rowsize,tablesize);
	    if (img_write(image,(char *)(image->rowsize),tablesize) != tablesize) {
		i_errhdlr("iclose: error on write of rowsize\n");
		return EOF;
	    }
	}
    }
    if(image->base) {
	free(image->base);
	image->base = 0;
    }
    if(image->tmpbuf) {
	free(image->tmpbuf);
	image->tmpbuf = 0;
    }
    if(ISRLE(image->type)) {
	free(image->rowstart);
	image->rowstart = 0;
	free(image->rowsize);
	image->rowsize = 0;
    }
    return 0;
}

static int iflush(IMAGE *image)
{
    unsigned short *base;

    if ( (image->flags&_IOWRT)
     && (base=image->base)!=NULL && (image->ptr-base)>0) {
	    if (putrow(image, base, image->y,image->z)!=image->xsize) {
		    image->flags |= _IOERR;
		    return(EOF);
	    }
    }
    return(0);
}

/*
 *	ifilbuf -
 *
 *				Paul Haeberli - 1984
 *
 */

/* This function is commented out because it is not used anywhere
static int ifilbuf(IMAGE *image)
{
    if ((image->flags&_IOREAD) == 0)
	return(EOF);
    if (image->base==NULL) {
	if ((image->base = ibufalloc(image)) == NULL) {
	    i_errhdlr("can't alloc image buffer\n");
	    return EOF;
	}
    }
    image->cnt = getrow(image,image->base,image->y,image->z);
    image->ptr = image->base;
    if (--image->cnt < 0) {
	if (image->cnt == -1) {
	    image->flags |= _IOEOF;
	    if (image->flags & _IORW)
		image->flags &= ~_IOREAD;
	} else
	    image->flags |= _IOERR;
	image->cnt = 0;
	return -1;
    }
    if(++image->y >= image->ysize) {
	image->y = 0;
	if(++image->z >= image->zsize) {
	    image->z = image->zsize-1;
	    image->flags |= _IOEOF;
	    return -1;
	}
    }
    return *image->ptr++ & 0xffff;
}
*/

/*
 *	iflsbuf -
 *
 *				Paul Haeberli - 1984
 *
 */

/* This function is commented out because it is not used anywhere
static unsigned int iflsbuf(IMAGE *image, unsigned int c)
{
    register unsigned short *base;
    register int n, rn;

    if ((image->flags&_IOWRT)==0)
	return(EOF);
    if ((base=image->base)==NULL) {
	if ((image->base=base=ibufalloc(image)) == NULL) {
	    i_errhdlr("flsbuf: error on buf alloc\n");
	    return EOF;
	}
	rn = n = 0;
    } else if ((rn = n = image->ptr - base) > 0)  {
	n = putrow(image,base,image->y,image->z);
	if(++image->y >= image->ysize) {
	    image->y = 0;
	    if(++image->z >= image->zsize) {
		image->z = image->zsize-1;
		image->flags |= _IOEOF;
		return -1;
	    }
	}
    }
    image->cnt = image->xsize-1;
    *base++ = c;
    image->ptr = base;
    if (rn != n) {
	image->flags |= _IOERR;
	return(EOF);
    }
    return(c);
}
*/


/*
 *	img_seek, img_write, img_read, img_optseek -
 *
 *				Paul Haeberli - 1984
 *
 */

static unsigned int img_seek(IMAGE *image, unsigned int y, unsigned int z)
{
    if(img_badrow(image,y,z)) {
	i_errhdlr("img_seek: row number out of range\n");
	return EOF;
    }
    image->x = 0;
    image->y = y;
    image->z = z;
    if(ISUNCOMPRESSED(image->type)) {
	switch(image->dim) {
	    case 1:
		return img_optseek(image, 512L);
	    case 2:
		return img_optseek(image,512L+(y*image->xsize)*BPP(image->type));
	    case 3:
		return img_optseek(image,
		    512L+(y*image->xsize+z*image->xsize*image->ysize)*
							BPP(image->type));
	    default:
		i_errhdlr("img_seek: weird dim\n");
		break;
	}
    } else if(ISRLE(image->type)) {
	switch(image->dim) {
	    case 1:
		return img_optseek(image, image->rowstart[0]);
	    case 2:
		return img_optseek(image, image->rowstart[y]);
	    case 3:
		return img_optseek(image, image->rowstart[y+z*image->ysize]);
	    default:
		i_errhdlr("img_seek: weird dim\n");
		break;
	}
    } else
	i_errhdlr("img_seek: weird image type\n");
    return((unsigned int)-1);
}

static int img_badrow(IMAGE *image, unsigned int y, unsigned int z)
{
    if(y>=image->ysize || z>=image->zsize)
	return 1;
    else
        return 0;
}

static int img_write(IMAGE *image, char *buffer,int count)
{
    int retval;

    retval = Tcl_Write (image->file, buffer, count);
    if(retval == count)
	image->offset += count;
    else
	image->offset = -1;
    return retval;
}

static int img_writeheader(IMAGE *image)
{
    int retval;

    retval = Tcl_Write (image->file, (char *)image, RELEVANT_HEADER_BYTES);
    if(retval == RELEVANT_HEADER_BYTES)
	image->offset += sizeof (IMAGE);
    else
	image->offset = -1;
    return retval;
}

static int img_read(IMAGE *image, char *buffer, int count)
{
    int retval;

    retval = Tcl_Read (image->file, buffer, count);
    if (retval == count)
	image->offset += count;
    else
	image->offset = -1;
    return retval;
}

static unsigned int img_optseek(IMAGE *image, unsigned int offset)
{
    if(image->offset != offset) {
       image->offset = offset;
       return ((unsigned int) Tcl_Seek (image->file,offset,0));
   }
   return offset;
}

/*
 *	getpix and putpix -
 *
 *				Paul Haeberli - 1984
 *
 */

#undef getpix
#undef putpix

/* These functions are commented out because they are not used anywhere
static int getpix(IMAGE *image)
{
    if(--(image)->cnt>=0)
    	return (int)(*(image)->ptr++);
    else
	return ifilbuf(image);
}

static unsigned int putpix(IMAGE *image, unsigned int pix)
{
    if(--(image)->cnt>=0)
        return (unsigned int)(*(image)->ptr++ = pix);
    else
	return iflsbuf(image,pix);
}
*/

/*
 *	img_getrowsize, img_setrowsize, img_rle_compact, img_rle_expand -
 *
 *				Paul Haeberli - 1984
 *
 */

static int img_getrowsize(IMAGE *image)
{
    switch(image->dim) {
	case 1:
	    return image->rowsize[0];
	case 2:
	    return image->rowsize[image->y];
	case 3:
	    return image->rowsize[image->y+image->z*image->ysize];
    }
    return -1;
}

static void img_setrowsize(IMAGE *image, int cnt, int y, int z)
{
    int *sizeptr;

    if(img_badrow(image,y,z))
	return;
    switch(image->dim) {
	case 1:
	    sizeptr = &image->rowsize[0];
	    image->rowstart[0] = image->rleend;
	    break;
	case 2:
	    sizeptr = &image->rowsize[y];
	    image->rowstart[y] = image->rleend;
	    break;
	case 3:
	    sizeptr = &image->rowsize[y+z*image->ysize];
	    image->rowstart[y+z*image->ysize] = image->rleend;
	    break;
        default:
	    i_errhdlr ("img_setrowsize: bad dim: %d\n", image->dim);
	    return;
    }
    if(*sizeptr != -1)
	image->wastebytes += *sizeptr;
    *sizeptr = cnt;
    image->rleend += cnt;
}

#define docompact 							\
	while(iptr<ibufend) {						\
	    sptr = iptr;						\
	    iptr += 2;							\
	    while((iptr<ibufend)&&((iptr[-2]!=iptr[-1])||(iptr[-1]!=iptr[0])))\
		iptr++;							\
	    iptr -= 2;							\
	    count = iptr-sptr;						\
	    while(count) {						\
		todo = count>126 ? 126:count; 				\
		count -= todo;						\
		*optr++ = 0x80|todo;					\
		while(todo--)						\
		    *optr++ = *sptr++;					\
	    }								\
	    sptr = iptr;						\
	    cc = *iptr++;						\
	    while( (iptr<ibufend) && (*iptr == cc) )			\
		iptr++;							\
	    count = iptr-sptr;						\
	    while(count) {						\
		todo = count>126 ? 126:count; 				\
		count -= todo;						\
		*optr++ = todo;						\
		*optr++ = cc;						\
	    }								\
	}								\
	*optr++ = 0;

static int img_rle_compact(unsigned short *expbuf, int ibpp,
			unsigned short *rlebuf, int obpp, int cnt)
{
    if(ibpp == 1 && obpp == 1) {
	register unsigned char *iptr = (unsigned char *)expbuf;
	register unsigned char *ibufend = iptr+cnt;
	register unsigned char *sptr;
	register unsigned char *optr = (unsigned char *)rlebuf;
	register short todo, cc;
	register int count;

	docompact;
	return optr - (unsigned char *)rlebuf;
    } else if(ibpp == 1 && obpp == 2) {
	register unsigned char *iptr = (unsigned char *)expbuf;
	register unsigned char *ibufend = iptr+cnt;
	register unsigned char *sptr;
	register unsigned short *optr = rlebuf;
	register short todo, cc;
	register int count;

	docompact;
	return optr - rlebuf;
    } else if(ibpp == 2 && obpp == 1) {
	register unsigned short *iptr = expbuf;
	register unsigned short *ibufend = iptr+cnt;
	register unsigned short *sptr;
	register unsigned char *optr = (unsigned char *)rlebuf;
	register short todo, cc;
	register int count;

	docompact;
	return optr - (unsigned char *)rlebuf;
    } else if(ibpp == 2 && obpp == 2) {
	register unsigned short *iptr = expbuf;
	register unsigned short *ibufend = iptr+cnt;
	register unsigned short *sptr;
	register unsigned short *optr = rlebuf;
	register short todo, cc;
	register int count;

	docompact;
	return optr - rlebuf;
    } else  {
	i_errhdlr("rle_compact: bad bpp: %d %d\n",ibpp,obpp);
	return 0;
    }
}

#define doexpand				\
	while(1) {				\
	    pixel = *iptr++;			\
	    if ( !(count = (pixel & 0x7f)) )	\
		return;				\
	    if(pixel & 0x80) {			\
	       while(count--)			\
		    *optr++ = *iptr++;		\
	    } else {				\
	       pixel = *iptr++;			\
	       while(count--)			\
		    *optr++ = pixel;		\
	    }					\
	}

static void img_rle_expand(unsigned short *rlebuf, int ibpp,
			unsigned short *expbuf, int obpp)
{
    if(ibpp == 1 && obpp == 1) {
	register unsigned char *iptr = (unsigned char *)rlebuf;
	register unsigned char *optr = (unsigned char *)expbuf;
	register unsigned short pixel,count;

	doexpand;
    } else if(ibpp == 1 && obpp == 2) {
	register unsigned char *iptr = (unsigned char *)rlebuf;
	register unsigned short *optr = expbuf;
	register unsigned short pixel,count;

	doexpand;
    } else if(ibpp == 2 && obpp == 1) {
	register unsigned short *iptr = rlebuf;
	register unsigned char  *optr = (unsigned char *)expbuf;
	register unsigned short pixel,count;

	doexpand;
    } else if(ibpp == 2 && obpp == 2) {
	register unsigned short *iptr = rlebuf;
	register unsigned short *optr = expbuf;
	register unsigned short pixel,count;

	doexpand;
    } else
	i_errhdlr("rle_expand: bad bpp: %d %d\n",ibpp,obpp);
}

/*
 *	putrow, getrow -
 *
 *				Paul Haeberli - 1984
 *
 */

static int putrow(IMAGE *image, unsigned short *buffer,
		unsigned int y, unsigned int z)
{
    register unsigned short 	*sptr;
    register unsigned char      *cptr;
    register unsigned int x;
    register unsigned int min, max;
    register int cnt;

    if( !(image->flags & (_IORW|_IOWRT)) )
	return -1;
    if(image->dim<3)
	z = 0;
    if(image->dim<2)
	y = 0;
    if(ISUNCOMPRESSED(image->type)) {
	switch(BPP(image->type)) {
	    case 1:
		min = image->min;
		max = image->max;
		cptr = (unsigned char *)image->tmpbuf;
		sptr = buffer;
		for(x=image->xsize; x--;) {
		    *cptr = *sptr++;
		    if (*cptr > max) max = *cptr;
		    if (*cptr < min) min = *cptr;
		    cptr++;
		}
		image->min = min;
		image->max = max;
		img_seek(image,y,z);
		cnt = image->xsize;
		if (img_write(image,(char *)(image->tmpbuf),cnt) != cnt)
		    return -1;
		else
		    return cnt;
		/* NOTREACHED */

	    case 2:
		min = image->min;
		max = image->max;
		sptr = buffer;
		for(x=image->xsize; x--;) {
		    if (*sptr > max) max = *sptr;
		    if (*sptr < min) min = *sptr;
		    sptr++;
		}
		image->min = min;
		image->max = max;
		img_seek(image,y,z);
		cnt = image->xsize<<1;
		if(image->dorev)
		    cvtshorts(buffer,cnt);
		if (img_write(image,(char *)(buffer),cnt) != cnt) {
		    if(image->dorev)
			cvtshorts(buffer,cnt);
		    return -1;
		} else {
		    if(image->dorev)
			cvtshorts(buffer,cnt);
		    return image->xsize;
		}
		/* NOTREACHED */

	    default:
		i_errhdlr("putrow: weird bpp\n");
	}
    } else if(ISRLE(image->type)) {
	switch(BPP(image->type)) {
	    case 1:
		min = image->min;
		max = image->max;
		sptr = buffer;
		for(x=image->xsize; x--;) {
		    if (*sptr > max) max = *sptr;
		    if (*sptr < min) min = *sptr;
		    sptr++;
		}
		image->min = min;
		image->max = max;
		cnt = img_rle_compact(buffer,2,image->tmpbuf,1,image->xsize);
		img_setrowsize(image,cnt,y,z);
		img_seek(image,y,z);
		if (img_write(image,(char *)(image->tmpbuf),cnt) != cnt)
		    return -1;
		else
		    return image->xsize;
		/* NOTREACHED */

	    case 2:
		min = image->min;
		max = image->max;
		sptr = buffer;
		for(x=image->xsize; x--;) {
		    if (*sptr > max) max = *sptr;
		    if (*sptr < min) min = *sptr;
		    sptr++;
		}
		image->min = min;
		image->max = max;
		cnt = img_rle_compact(buffer,2,image->tmpbuf,2,image->xsize);
		cnt <<= 1;
		img_setrowsize(image,cnt,y,z);
		img_seek(image,y,z);
		if(image->dorev)
		    cvtshorts(image->tmpbuf,cnt);
		if (img_write(image,(char *)(image->tmpbuf),cnt) != cnt) {
		    if(image->dorev)
			cvtshorts(image->tmpbuf,cnt);
		    return -1;
		} else {
		    if(image->dorev)
			cvtshorts(image->tmpbuf,cnt);
		    return image->xsize;
		}
		/* NOTREACHED */

	    default:
		i_errhdlr("putrow: weird bpp\n");
	}
    } else
	i_errhdlr("putrow: weird image type\n");
    return(-1);
}

static int getrow(IMAGE *image, unsigned short *buffer,
		unsigned int y, unsigned int z)
{
    register short i;
    register unsigned char *cptr;
    register unsigned short *sptr;
    register short cnt;

    if( !(image->flags & (_IORW|_IOREAD)) )
	return -1;
    if(image->dim<3)
	z = 0;
    if(image->dim<2)
	y = 0;
    img_seek(image, y, z);
    if(ISUNCOMPRESSED(image->type)) {
	switch(BPP(image->type)) {
	    case 1:
		if (img_read(image,(char *)image->tmpbuf,image->xsize)
							    != image->xsize)
		    return -1;
		else {
		    cptr = (unsigned char *)image->tmpbuf;
		    sptr = buffer;
		    for(i=image->xsize; i--;)
			*sptr++ = *cptr++;
		}
		return image->xsize;
		/* NOTREACHED */

	    case 2:
		cnt = image->xsize<<1;
		if (img_read(image,(char *)(buffer),cnt) != cnt)
		    return -1;
		else {
		    if(image->dorev)
			cvtshorts(buffer,cnt);
		    return image->xsize;
		}
		/* NOTREACHED */

	    default:
		i_errhdlr("getrow: weird bpp\n");
		break;
	}
    } else if(ISRLE(image->type)) {
	switch(BPP(image->type)) {
	    case 1:
		if( (cnt = img_getrowsize(image)) == -1 )
		    return -1;
		if( img_read(image,(char *)(image->tmpbuf),cnt) != cnt )
		    return -1;
		else {
		    img_rle_expand(image->tmpbuf,1,buffer,2);
		    return image->xsize;
		}
		/* NOTREACHED */

	    case 2:
		if( (cnt = img_getrowsize(image)) == -1 )
		    return -1;
		if( cnt != img_read(image,(char *)(image->tmpbuf),cnt) )
		    return -1;
		else {
		    if(image->dorev)
			cvtshorts(image->tmpbuf,cnt);
		    img_rle_expand(image->tmpbuf,2,buffer,2);
		    return image->xsize;
		}
		/* NOTREACHED */

	    default:
		i_errhdlr("getrow: weird bpp\n");
		break;
	}
    } else
	i_errhdlr("getrow: weird image type\n");
    return -1;
}

/* End of original SGI image code */

/* Structure to hold information about the SGI file being processed. */
typedef struct {
    IMAGE th;
    UByte *red,		/* Pointers to step through scanlines */
	  *green,
	  *blue,
	  *matte;
    UByte *redScan,	/* Buffer for one scanline: Red   channel */
	  *greenScan,	/* Buffer for one scanline: Green channel */
	  *blueScan,	/* Buffer for one scanline: Blue  channel */
	  *matteScan;	/* Buffer for one scanline: Matte channel */
    UByte *scanline;
    unsigned short *pixbuf;
} SGIFILE;

/* This function determines at runtime, whether we have to switch bytes.
   The SGI image format expects data to be in big-endian format. */

static int isIntel (void)
{
    char order[] = { 1, 2, 3, 4};
    unsigned int val = (unsigned int)*((short *)order);
    /* On Intel (little-endian) systems this value is equal to 513.
       On big-endian systems this value equals 258. */
    return (val == 513);
}

static void sgiClose (SGIFILE *tf)
{
    if (tf->redScan)   ckfree ((char *)tf->redScan);
    if (tf->greenScan) ckfree ((char *)tf->greenScan);
    if (tf->blueScan)  ckfree ((char *)tf->blueScan);
    if (tf->matteScan) ckfree ((char *)tf->matteScan);
    if (tf->pixbuf)    ckfree ((char *)tf->pixbuf);
    return;
}

#define OUT Tcl_WriteChars (outChan, str, -1)
static void printImgInfo (IMAGE *th, const char *filename, const char *msg)
{
    Tcl_Channel outChan;
    char str[256];

    outChan = Tcl_GetStdChannel (TCL_STDOUT);
    if (!outChan) {
        return;
    }
    sprintf(str, "%s %s\n", msg, filename);                                      OUT;
    sprintf(str, "\tSize in pixel      : %d x %d\n", th->xsize, th->ysize);      OUT;
    sprintf(str, "\tNo. of channels    : %d\n", (th->zsize));                    OUT;
    sprintf(str, "\tBytes per pixel    : %d\n", BPP(th->type));                  OUT;
    sprintf(str, "\tCompression        : %s\n", ISRLE(th->type)? "RLE": "None"); OUT;
    Tcl_Flush(outChan);
}
#undef OUT

static Boln readHeader (tkimg_MFile *handle, IMAGE *th)
{
    if (512 != tkimg_Read(handle, (char *)th, 512)) {
	return FALSE;
    }
										    if( ((th->imagic>>8) | ((th->imagic&0xff)<<8)) == IMAGIC ) {
	th->dorev = 1;
	cvtimage((int *)th);
    } else {
	th->dorev = 0;
    }
    if (th->imagic != IMAGIC) {
	return FALSE;
    }

    return TRUE;
}

static Boln writeHeader(tkimg_MFile *handle, IMAGE *th, UInt type, UInt dim,
                         UInt xsize, UInt ysize, UInt zsize)
{
    if (!imgOpenWrite ((MYCHANNEL)handle->data, th, "w",
                       type, dim, xsize, ysize, zsize))
	return FALSE;
    return TRUE;
}

static Boln readChannel (SGIFILE *tf, UByte *dest, Int sgichn, Int nchan,
			 Int y, Int n)
{
    unsigned short *src = tf->pixbuf;
    unsigned short *stop = src + n;

    if (-1 == getrow (&tf->th, tf->pixbuf, y, sgichn))
	return FALSE;

    dest += sgichn;
    switch (BPP(tf->th.type)) {
	case 1:	{	/* 8-bit pixel values */
	    while (src < stop) {
		*dest = *(src++);
		dest += nchan;
	    }
	    break;
	}
	case 2: {	/* 16-bit values will be linearly mapped to 8-bit. */
	    while (src < stop) {
		*dest = *(src++) >> 8;
		dest += nchan;
	    }
	    break;
	}
    }
    return TRUE;
}

static Boln sgiReadScan (Tcl_Interp *interp, tkimg_MFile *handle,
                         SGIFILE *tf, Int y)
{
    Int nchan;

    nchan = tf->th.zsize;

    /* Read 1. channel: This is either the red or brightness channel. */
    if (!readChannel (tf, tf->scanline, 0, nchan, y, tf->th.xsize)) {
        return FALSE;
    }

    if ( nchan >= 3) {
	/* This is either a RGB or RGBA image. Read green and blue channels. */
        if (!readChannel (tf, tf->scanline, 1, nchan, y, tf->th.xsize) ||
	    !readChannel (tf, tf->scanline, 2, nchan, y, tf->th.xsize))
	    return FALSE;
    }
    if (nchan > 3 || nchan == 2) {
        /* If nchan is 2, we have a brightness-alpha image, if nchan is 4, we
           have RGBA. */
        if (!readChannel (tf, tf->scanline, nchan == 2? 1: 3, nchan,
                          y, tf->th.xsize))
            return FALSE;
    }
    return TRUE;
}

static Boln writeChannel (SGIFILE *tf, UByte *src, Int sgichn, Int y, Int n)
{
    unsigned short *dest = tf->pixbuf;
    UByte *stop = src + n;

    while (src < stop)
	*(dest++) = *(src++);

    if (-1 == putrow (&tf->th, tf->pixbuf, y, sgichn))
	return FALSE;

    return TRUE;
}

static Boln sgiWriteScan(Tcl_Interp *interp, tkimg_MFile *handle,
                          SGIFILE *tf, Int y)
{
    if (!writeChannel (tf, tf->redScan,   0, y, tf->th.xsize) ||
        !writeChannel (tf, tf->greenScan, 1, y, tf->th.xsize) ||
        !writeChannel (tf, tf->blueScan,  2, y, tf->th.xsize))
        return FALSE;

    if (tf->th.zsize > 3)
        if (!writeChannel (tf, tf->matteScan, 3, y, tf->th.xsize))
            return FALSE;
    return TRUE;
}

/*
 * Here is the start of the standard functions needed for every image format.
 */

/*
 * Prototypes for local procedures defined in this file:
 */

static int   ParseFormatOpts _ANSI_ARGS_((Tcl_Interp *interp, Tcl_Obj *format,
                 int *comp, int *verb, int *matte));
static int   CommonMatch _ANSI_ARGS_((tkimg_MFile *handle, int *widthPtr,
	         int *heightPtr, IMAGE *sgiHeaderPtr));
static int   CommonRead _ANSI_ARGS_((Tcl_Interp *interp, tkimg_MFile *handle,
                 const char *filename, Tcl_Obj *format,
	         Tk_PhotoHandle imageHandle, int destX, int destY,
                 int width, int height, int srcX, int srcY));
static int   CommonWrite _ANSI_ARGS_((Tcl_Interp *interp,
                 const char *filename, Tcl_Obj *format,
	         tkimg_MFile *handle, Tk_PhotoImageBlock *blockPtr));

static int ParseFormatOpts (interp, format, comp, verb, matte)
    Tcl_Interp *interp;
    Tcl_Obj *format;
    int *comp;
    int *verb;
    int *matte;
{
    static const char *const sgiOptions[] = {"-compression", "-verbose", "-matte"};
    int objc, length, c, i, index;
    Tcl_Obj **objv;
    CONST84 char *compression, *verbose, *transp;

    *comp  = 1;
    *verb  = 0;
    *matte = 1;

    if (tkimg_ListObjGetElements(interp, format, &objc, &objv) != TCL_OK)
	return TCL_ERROR;
    if (objc) {
	compression = "rle";
	verbose     = "0";
	transp      = "1";
	for (i=1; i<objc; i++) {
	    if (Tcl_GetIndexFromObj(interp, objv[i], (CONST84 char *CONST86 *)sgiOptions,
		    "format option", 0, &index) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (++i >= objc) {
		Tcl_AppendResult(interp, "No value for option \"",
			Tcl_GetStringFromObj (objv[--i], (int *) NULL),
			"\"", (char *) NULL);
		return TCL_ERROR;
	    }
	    switch(index) {
		case 0:
		    compression = Tcl_GetStringFromObj(objv[i], (int *) NULL);
		    break;
		case 1:
		    verbose = Tcl_GetStringFromObj(objv[i], (int *) NULL);
		    break;
		case 2:
		    transp = Tcl_GetStringFromObj(objv[i], (int *) NULL);
		    break;
	    }
	}

	c = compression[0]; length = strlen (compression);
	if ((c == 'n') && (!strncmp (compression, "none", length))) {
	    *comp = ITYPE_UNCOMPRESSED;
	} else if ((c == 'r') && (!strncmp (compression, "rle",length))) {
	    *comp = ITYPE_RLE;
	} else {
	    Tcl_AppendResult(interp, "invalid compression mode \"",
		    compression, "\": should be rle or none", (char *) NULL);
	    return TCL_ERROR;
	}

	c = verbose[0]; length = strlen (verbose);
	if (!strncmp (verbose, "1", length) || \
	    !strncmp (verbose, "true", length) || \
	    !strncmp (verbose, "on", length)) {
	    *verb = 1;
	} else if (!strncmp (verbose, "0", length) || \
	    !strncmp (verbose, "false", length) || \
	    !strncmp (verbose, "off", length)) {
	    *verb = 0;
	} else {
	    Tcl_AppendResult(interp, "invalid verbose mode \"", verbose,
                              "\": should be 1 or 0, on or off, true or false",
			      (char *) NULL);
	    return TCL_ERROR;
	}

	c = transp[0]; length = strlen (transp);
	if (!strncmp (transp, "1", length) || \
	    !strncmp (transp, "true", length) || \
	    !strncmp (transp, "on", length)) {
	    *matte = 1;
	} else if (!strncmp (transp, "0", length) || \
	    !strncmp (transp, "false", length) || \
	    !strncmp (transp, "off", length)) {
	    *matte = 0;
	} else {
	    Tcl_AppendResult(interp, "invalid alpha (matte) mode \"", verbose,
                              "\": should be 1 or 0, on or off, true or false",
			      (char *) NULL);
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}

static int ChnMatch(interp, chan, filename, format, widthPtr, heightPtr)
    Tcl_Interp *interp;
    Tcl_Channel chan;
    const char *filename;
    Tcl_Obj *format;
    int *widthPtr, *heightPtr;
{
    tkimg_MFile handle;

    tkimg_FixChanMatchProc (&interp, &chan, &filename, &format,
			 &widthPtr, &heightPtr);

    handle.data = (char *) chan;
    handle.state = IMG_CHAN;

    return CommonMatch(&handle, widthPtr, heightPtr, NULL);
}

static int ObjMatch(interp, data, format, widthPtr, heightPtr)
    Tcl_Interp *interp;
    Tcl_Obj *data;
    Tcl_Obj *format;
    int *widthPtr, *heightPtr;
{
    tkimg_MFile handle;

    tkimg_FixObjMatchProc (&interp, &data, &format, &widthPtr, &heightPtr);

    if (!tkimg_ReadInit(data, '\001', &handle)) {
        return 0;
    }
    return CommonMatch(&handle, widthPtr, heightPtr, NULL);
}

static int CommonMatch(handle, widthPtr, heightPtr, sgiHeaderPtr)
    tkimg_MFile *handle;
    int *widthPtr;
    int *heightPtr;
    IMAGE *sgiHeaderPtr;
{
    IMAGE th;

    if (!sgiHeaderPtr) {
	if (!readHeader (handle, &th))
	    return 0;
    } else {
	if (!imgOpenRead ((MYCHANNEL)handle->data, &th, "r"))
	    return 0;
    }

    if (th.xsize <= 0 || th.ysize <= 0)
	return 0;

    *widthPtr  = th.xsize;
    *heightPtr = th.ysize;
    if (sgiHeaderPtr)
	*sgiHeaderPtr = th;
    return 1;
}

static int ChnRead(interp, chan, filename, format, imageHandle,
	               destX, destY, width, height, srcX, srcY)
    Tcl_Interp *interp;
    Tcl_Channel chan;
    const char *filename;
    Tcl_Obj *format;
    Tk_PhotoHandle imageHandle;
    int destX, destY;
    int width, height;
    int srcX, srcY;
{
    tkimg_MFile handle;

    handle.data = (char *) chan;
    handle.state = IMG_CHAN;

    return CommonRead (interp, &handle, filename, format,
                       imageHandle, destX, destY,
		       width, height, srcX, srcY);
}

#define BUFLEN 2048

static int ObjRead (interp, data, format, imageHandle,
	            destX, destY, width, height, srcX, srcY)
    Tcl_Interp *interp;
    Tcl_Obj *data;
    Tcl_Obj *format;
    Tk_PhotoHandle imageHandle;
    int destX, destY;
    int width, height;
    int srcX, srcY;
{
    tkimg_MFile handle;
    char tempFileName[256];
    char buffer[BUFLEN];
    MYCHANNEL outchan;
    Tcl_Channel inchan;
    int count, retVal;

    tkimg_ReadInit (data, '\001', &handle);

    tmpnam(tempFileName);
#if defined (TCLSEEK_WORKAROUND)
    outchan = (Tcl_Channel)fopen (tempFileName, "wb");
#else
    outchan = tkimg_OpenFileChannel (interp, tempFileName, 0644);
#endif
    if (!outchan) {
	return TCL_ERROR;
    }

    count = tkimg_Read(&handle, buffer, BUFLEN);
    while (count == BUFLEN) {
	Tcl_Write (outchan, buffer, count);
	count = tkimg_Read(&handle, buffer, BUFLEN);
    }
    if (count>0) {
	Tcl_Write (outchan, buffer, count);
    }
    if (MYCLOSE (interp, outchan) == TCL_ERROR) {
	return TCL_ERROR;
    }

    inchan = tkimg_OpenFileChannel (interp, tempFileName, 0);
    if (!inchan) {
	return TCL_ERROR;
    }

    handle.data = (char *) inchan;
    handle.state = IMG_CHAN;

    retVal = CommonRead (interp, &handle, tempFileName, format, imageHandle,
                            destX, destY, width, height, srcX, srcY);
    if (Tcl_Close (interp, inchan) == TCL_ERROR) {
	return TCL_ERROR;
    }
    remove (tempFileName);
    return retVal;
}

typedef struct myblock {
    Tk_PhotoImageBlock ck;
    int dummy; /* extra space for offset[3], in case it is not
		  included already in Tk_PhotoImageBlock */
} myblock;

#define block bl.ck

static int CommonRead (interp, handle, filename, format, imageHandle,
                       destX, destY, width, height, srcX, srcY)
    Tcl_Interp *interp;         /* Interpreter to use for reporting errors. */
    tkimg_MFile *handle;        /* The image file, open for reading. */
    const char *filename;       /* The name of the image file. */
    Tcl_Obj *format;            /* User-specified format object, or NULL. */
    Tk_PhotoHandle imageHandle; /* The photo image to write into. */
    int destX, destY;           /* Coordinates of top-left pixel in
                                 * photo image to be written to. */
    int width, height;          /* Dimensions of block of photo image to
                                 * be written to. */
    int srcX, srcY;             /* Coordinates of top-left pixel to be used
                                 * in image being read. */
{
    myblock bl;
    Int y, nchan;
    int fileWidth, fileHeight;
    int stopY, outY, outWidth, outHeight;
    SGIFILE tf;
    int compr, verbose, matte;

    memset (&tf, 0, sizeof (SGIFILE));
    if (ParseFormatOpts(interp, format, &compr, &verbose, &matte) != TCL_OK) {
        return TCL_ERROR;
    }

    CommonMatch(handle, &fileWidth, &fileHeight, &tf.th);
    if (verbose)
	printImgInfo (&tf.th, filename, "Reading image:");

    if ((srcX + width) > fileWidth) {
	outWidth = fileWidth - srcX;
    } else {
	outWidth = width;
    }
    if ((srcY + height) > fileHeight) {
	outHeight = fileHeight - srcY;
    } else {
	outHeight = height;
    }
    if ((outWidth <= 0) || (outHeight <= 0)
	|| (srcX >= fileWidth) || (srcY >= fileHeight)) {
	return TCL_OK;
    }

    Tk_PhotoExpand(imageHandle, destX + outWidth, destY + outHeight);

    nchan = tf.th.zsize;

    tf.pixbuf   = (UShort *) ckalloc (fileWidth * nchan * sizeof (UShort));
    tf.scanline = (UByte  *) ckalloc (fileWidth * nchan);

    block.pixelSize = nchan;
    block.pitch = fileWidth * nchan;
    block.width = outWidth;
    block.height = 1;
    switch (nchan) {
	case 1: /* Brightness */
	case 2: /* Brightness + Matte */
	    block.offset[0] = 0;
	    block.offset[1] = 0;
	    block.offset[2] = 0;
	    block.offset[3] = matte? 1: 0;
	    break;
	case 3: /* RGB */
	case 4: /* RGB + Matte */
	    block.offset[0] = 0;
	    block.offset[1] = 1;
	    block.offset[2] = 2;
	    block.offset[3] = matte? 3: 0;
	    break;
	default:
	    printf("Invalid number of channels: %d\n", (int) nchan);
	    return TCL_ERROR;
	    break;
    }
    block.pixelPtr = tf.scanline + srcX * nchan;

    stopY = srcY + outHeight;
    outY = destY;

    for (y=0; y<stopY; y++) {
	sgiReadScan (interp, handle, &tf, fileHeight-1-y);
	if (y >= srcY) {
	    tkimg_PhotoPutBlock(interp, imageHandle, &block, destX, outY, outWidth, 1, matte? TK_PHOTO_COMPOSITE_OVERLAY: TK_PHOTO_COMPOSITE_SET);
	    outY++;
	}
    }
    sgiClose (&tf);
    return TCL_OK ;
}

static int ChnWrite (interp, filename, format, blockPtr)
    Tcl_Interp *interp;
    CONST84 char *filename;
    Tcl_Obj *format;
    Tk_PhotoImageBlock *blockPtr;
{
    MYCHANNEL chan;
    tkimg_MFile handle;
    int result;

#if defined (TCLSEEK_WORKAROUND)
    chan = (Tcl_Channel)fopen(filename, "wb");
#else
    chan = tkimg_OpenFileChannel (interp, filename, 0644);
#endif
    if (!chan) {
	return TCL_ERROR;
    }

    handle.data = (char *) chan;
    handle.state = IMG_CHAN;

    result = CommonWrite (interp, filename, format, &handle, blockPtr);
    if (MYCLOSE(interp, chan) == TCL_ERROR) {
	return TCL_ERROR;
    }
    return result;
}

static int StringWrite (interp, dataPtr, format, blockPtr)
    Tcl_Interp *interp;
    Tcl_DString *dataPtr;
    Tcl_Obj *format;
    Tk_PhotoImageBlock *blockPtr;
{
    tkimg_MFile handle;
    int result;
    Tcl_DString data;
    Tcl_Channel inchan;
    MYCHANNEL outchan;
    char tempFileName[256];
    char buffer[BUFLEN];
    int count;

    tkimg_FixStringWriteProc (&data, &interp, &dataPtr, &format, &blockPtr);

    tmpnam(tempFileName);
#if defined (TCLSEEK_WORKAROUND)
    outchan = (Tcl_Channel)fopen(tempFileName, "wb");
#else
    outchan = tkimg_OpenFileChannel (interp, tempFileName, 0644);
#endif
    if (!outchan) {
	return TCL_ERROR;
    }

    handle.data = (char *) outchan;
    handle.state = IMG_CHAN;

    result = CommonWrite (interp, tempFileName, format, &handle, blockPtr);
    if (MYCLOSE (interp, outchan) == TCL_ERROR) {
	return TCL_ERROR;
    }

    tkimg_WriteInit(dataPtr, &handle);

    inchan = tkimg_OpenFileChannel (interp, tempFileName, 0);
    if (!inchan) {
	return TCL_ERROR;
    }

    count = Tcl_Read (inchan, buffer, BUFLEN);
    while (count == BUFLEN) {
	tkimg_Write(&handle, buffer, count);
	count = Tcl_Read (inchan, buffer, BUFLEN);
    }
    if (count>0) {
	tkimg_Write(&handle, buffer, count);
    }
    if (Tcl_Close (interp, inchan) == TCL_ERROR) {
	return TCL_ERROR;
    }
    remove (tempFileName);
    tkimg_Putc (IMG_DONE, &handle);

    if ((result == TCL_OK) && (dataPtr == &data)) {
	Tcl_DStringResult (interp, dataPtr);
    }
    return result;
}

static int CommonWrite (interp, filename, format, handle, blockPtr)
    Tcl_Interp *interp;
    const char *filename;
    Tcl_Obj *format;
    tkimg_MFile *handle;
    Tk_PhotoImageBlock *blockPtr;
{
    Int     x, y, bpp, nchan;
    Int     redOffset, greenOffset, blueOffset, alphaOffset;
    UByte   *pixelPtr, *rowPixPtr;
    SGIFILE tf;
    int compr, verbose, matte; /* Format options */

    memset (&tf, 0, sizeof (SGIFILE));
    if (ParseFormatOpts(interp, format, &compr, &verbose, &matte) != TCL_OK) {
        return TCL_ERROR;
    }

    bpp = 1;

    redOffset   = 0;
    greenOffset = blockPtr->offset[1] - blockPtr->offset[0];
    blueOffset  = blockPtr->offset[2] - blockPtr->offset[0];
    alphaOffset = blockPtr->offset[0];

    if (alphaOffset < blockPtr->offset[2]) {
	alphaOffset = blockPtr->offset[2];
    }
    if (++alphaOffset < blockPtr->pixelSize) {
	alphaOffset -= blockPtr->offset[0];
    } else {
	alphaOffset = 0;
    }
    nchan = ((matte && alphaOffset)? 4: 3);

    tf.redScan   = (UByte *)  ckalloc (blockPtr->width);
    tf.greenScan = (UByte *)  ckalloc (blockPtr->width);
    tf.blueScan  = (UByte *)  ckalloc (blockPtr->width);
    tf.matteScan = (UByte *)  ckalloc (blockPtr->width);
    tf.pixbuf    = (UShort *) ckalloc (blockPtr->width * sizeof (UShort));
    tf.th.imagic = IMAGIC;
    tf.th.dorev  = isIntel();

    if (!writeHeader(handle, &tf.th,
                      compr? RLE(bpp): UNCOMPRESSED(bpp),
                      nchan, blockPtr->width, blockPtr->height, nchan)) {
	return TCL_ERROR;
    }

    rowPixPtr = blockPtr->pixelPtr + blockPtr->offset[0];
    for (y = blockPtr->height -1; y >=0; y--) {
	tf.red = tf.redScan;
	tf.green = tf.greenScan;
	tf.blue = tf.blueScan;
	tf.matte = tf.matteScan;
	pixelPtr = rowPixPtr;
	for (x = 0; x < blockPtr->width; x++) {
	    *(tf.red++)   = pixelPtr[redOffset];
	    *(tf.green++) = pixelPtr[greenOffset];
	    *(tf.blue++)  = pixelPtr[blueOffset];
	    if (nchan == 4) {
                /* Have a matte channel and write it. */
		*(tf.matte++) = pixelPtr[alphaOffset];
	    }
	    pixelPtr += blockPtr->pixelSize;
	}
	if (!sgiWriteScan(interp, handle, &tf, y)) {
	    sgiClose (&tf);
	    return TCL_ERROR;
	}
	rowPixPtr += blockPtr->pitch;
    }
    if (verbose)
        printImgInfo (&tf.th, filename, "Saving image:");

    iclose (&tf.th);
    sgiClose (&tf);
    return TCL_OK;
}
