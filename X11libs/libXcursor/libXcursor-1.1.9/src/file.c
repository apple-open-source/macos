/*
 * $Id$
 *
 * Copyright © 2002 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "xcursorint.h"
#include <stdlib.h>
#include <string.h>

XcursorImage *
XcursorImageCreate (int width, int height)
{
    XcursorImage    *image;

    image = malloc (sizeof (XcursorImage) +
		    width * height * sizeof (XcursorPixel));
    if (!image)
	return NULL;
    image->version = XCURSOR_IMAGE_VERSION;
    image->pixels = (XcursorPixel *) (image + 1);
    image->size = width > height ? width : height;
    image->width = width;
    image->height = height;
    image->delay = 0;
    return image;
}

void
XcursorImageDestroy (XcursorImage *image)
{
    free (image);
}

XcursorImages *
XcursorImagesCreate (int size)
{
    XcursorImages   *images;

    images = malloc (sizeof (XcursorImages) + 
		     size * sizeof (XcursorImage *));
    if (!images)
	return NULL;
    images->nimage = 0;
    images->images = (XcursorImage **) (images + 1);
    images->name = NULL;
    return images;
}

void
XcursorImagesDestroy (XcursorImages *images)
{
    int	n;

    if (!images)
        return;

    for (n = 0; n < images->nimage; n++)
	XcursorImageDestroy (images->images[n]);
    if (images->name)
	free (images->name);
    free (images);
}

void
XcursorImagesSetName (XcursorImages *images, const char *name)
{
    char    *new;
    
    if (!images || !name)
        return;
    
    new = malloc (strlen (name) + 1);

    if (!new)
	return;

    strcpy (new, name);
    if (images->name)
	free (images->name);
    images->name = new;
}

XcursorComment *
XcursorCommentCreate (XcursorUInt comment_type, int length)
{
    XcursorComment  *comment;

    if (length > XCURSOR_COMMENT_MAX_LEN)
	return NULL;

    comment = malloc (sizeof (XcursorComment) + length + 1);
    if (!comment)
	return NULL;
    comment->version = XCURSOR_COMMENT_VERSION;
    comment->comment_type = comment_type;
    comment->comment = (char *) (comment + 1);
    comment->comment[0] = '\0';
    return comment;
}

void
XcursorCommentDestroy (XcursorComment *comment)
{
    free (comment);
}

XcursorComments *
XcursorCommentsCreate (int size)
{
    XcursorComments *comments;

    comments = malloc (sizeof (XcursorComments) +
		       size * sizeof (XcursorComment *));
    if (!comments)
	return NULL;
    comments->ncomment = 0;
    comments->comments = (XcursorComment **) (comments + 1);
    return comments;
}

void
XcursorCommentsDestroy (XcursorComments *comments)
{
    int	n;

    if (!comments)
        return;

    for (n = 0; n < comments->ncomment; n++)
	XcursorCommentDestroy (comments->comments[n]);
    free (comments);
}

static XcursorBool
_XcursorReadUInt (XcursorFile *file, XcursorUInt *u)
{
    unsigned char   bytes[4];

    if (!file || !u)
        return XcursorFalse;

    if ((*file->read) (file, bytes, 4) != 4)
	return XcursorFalse;
    *u = ((bytes[0] << 0) |
	  (bytes[1] << 8) |
	  (bytes[2] << 16) |
	  (bytes[3] << 24));
    return XcursorTrue;
}

static XcursorBool
_XcursorReadBytes (XcursorFile *file, char *bytes, int length)
{
    if (!file || !bytes || (*file->read) (file, (unsigned char *) bytes, length) != length)
	return XcursorFalse;
    return XcursorTrue;
}

static XcursorBool
_XcursorWriteUInt (XcursorFile *file, XcursorUInt u)
{
    unsigned char   bytes[4];

    if (!file)
        return XcursorFalse;
    
    bytes[0] = u;
    bytes[1] = u >>  8;
    bytes[2] = u >> 16;
    bytes[3] = u >> 24;
    if ((*file->write) (file, bytes, 4) != 4)
	return XcursorFalse;
    return XcursorTrue;
}

static XcursorBool
_XcursorWriteBytes (XcursorFile *file, char *bytes, int length)
{
    if (!file || !bytes || (*file->write) (file, (unsigned char *) bytes, length) != length)
	return XcursorFalse;
    return XcursorTrue;
}

static void
_XcursorFileHeaderDestroy (XcursorFileHeader *fileHeader)
{
    free (fileHeader);
}

static XcursorFileHeader *
_XcursorFileHeaderCreate (int ntoc)
{
    XcursorFileHeader	*fileHeader;

    if (ntoc > 0x10000)
	return NULL;
    fileHeader = malloc (sizeof (XcursorFileHeader) +
			 ntoc * sizeof (XcursorFileToc));
    if (!fileHeader)
	return NULL;
    fileHeader->magic = XCURSOR_MAGIC;
    fileHeader->header = XCURSOR_FILE_HEADER_LEN;
    fileHeader->version = XCURSOR_FILE_VERSION;
    fileHeader->ntoc = ntoc;
    fileHeader->tocs = (XcursorFileToc *) (fileHeader + 1);
    return fileHeader;
}

static XcursorFileHeader *
_XcursorReadFileHeader (XcursorFile *file)
{
    XcursorFileHeader	head, *fileHeader;
    XcursorUInt		skip;
    int			n;

    if (!file)
        return NULL;
    
    if (!_XcursorReadUInt (file, &head.magic))
	return NULL;
    if (head.magic != XCURSOR_MAGIC)
	return NULL;
    if (!_XcursorReadUInt (file, &head.header))
	return NULL;
    if (!_XcursorReadUInt (file, &head.version))
	return NULL;
    if (!_XcursorReadUInt (file, &head.ntoc))
	return NULL;
    skip = head.header - XCURSOR_FILE_HEADER_LEN;
    if (skip)
	if ((*file->seek) (file, skip, SEEK_CUR) == EOF)
	    return NULL;
    fileHeader = _XcursorFileHeaderCreate (head.ntoc);
    if (!fileHeader)
	return NULL;
    fileHeader->magic = head.magic;
    fileHeader->header = head.header;
    fileHeader->version = head.version;
    fileHeader->ntoc = head.ntoc;
    for (n = 0; n < fileHeader->ntoc; n++)
    {
	if (!_XcursorReadUInt (file, &fileHeader->tocs[n].type))
	    break;
	if (!_XcursorReadUInt (file, &fileHeader->tocs[n].subtype))
	    break;
	if (!_XcursorReadUInt (file, &fileHeader->tocs[n].position))
	    break;
    }
    if (n != fileHeader->ntoc)
    {
	_XcursorFileHeaderDestroy (fileHeader);
	return NULL;
    }
    return fileHeader;
}

static XcursorUInt
_XcursorFileHeaderLength (XcursorFileHeader *fileHeader)
{
    return (XCURSOR_FILE_HEADER_LEN + 
	    fileHeader->ntoc * XCURSOR_FILE_TOC_LEN);
}

static XcursorBool
_XcursorWriteFileHeader (XcursorFile *file, XcursorFileHeader *fileHeader)
{
    int	toc;

    if (!file || !fileHeader)
        return XcursorFalse;
    
    if (!_XcursorWriteUInt (file, fileHeader->magic))
	return XcursorFalse;
    if (!_XcursorWriteUInt (file, fileHeader->header))
	return XcursorFalse;
    if (!_XcursorWriteUInt (file, fileHeader->version))
	return XcursorFalse;
    if (!_XcursorWriteUInt (file, fileHeader->ntoc))
	return XcursorFalse;
    for (toc = 0; toc < fileHeader->ntoc; toc++)
    {
	if (!_XcursorWriteUInt (file, fileHeader->tocs[toc].type))
	    return XcursorFalse;
	if (!_XcursorWriteUInt (file, fileHeader->tocs[toc].subtype))
	    return XcursorFalse;
	if (!_XcursorWriteUInt (file, fileHeader->tocs[toc].position))
	    return XcursorFalse;
    }
    return XcursorTrue;
}

static XcursorBool
_XcursorSeekToToc (XcursorFile		*file, 
		   XcursorFileHeader	*fileHeader,
		   int			toc)
{
    if (!file || !fileHeader || \
        (*file->seek) (file, fileHeader->tocs[toc].position, SEEK_SET) == EOF)
	return XcursorFalse;
    return XcursorTrue;
}

static XcursorBool
_XcursorFileReadChunkHeader (XcursorFile	*file,
			     XcursorFileHeader	*fileHeader,
			     int		toc,
			     XcursorChunkHeader	*chunkHeader)
{
    if (!file || !fileHeader || !chunkHeader)
        return XcursorFalse;
    if (!_XcursorSeekToToc (file, fileHeader, toc))
	return XcursorFalse;
    if (!_XcursorReadUInt (file, &chunkHeader->header))
	return XcursorFalse;
    if (!_XcursorReadUInt (file, &chunkHeader->type))
	return XcursorFalse;
    if (!_XcursorReadUInt (file, &chunkHeader->subtype))
	return XcursorFalse;
    if (!_XcursorReadUInt (file, &chunkHeader->version))
	return XcursorFalse;
    /* sanity check */
    if (chunkHeader->type != fileHeader->tocs[toc].type ||
	chunkHeader->subtype != fileHeader->tocs[toc].subtype)
	return XcursorFalse;
    return XcursorTrue;
}

static XcursorBool
_XcursorFileWriteChunkHeader (XcursorFile	    *file,
			      XcursorFileHeader	    *fileHeader,
			      int		    toc,
			      XcursorChunkHeader    *chunkHeader)
{
    if (!file || !fileHeader || !chunkHeader)
        return XcursorFalse;
    if (!_XcursorSeekToToc (file, fileHeader, toc))
	return XcursorFalse;
    if (!_XcursorWriteUInt (file, chunkHeader->header))
	return XcursorFalse;
    if (!_XcursorWriteUInt (file, chunkHeader->type))
	return XcursorFalse;
    if (!_XcursorWriteUInt (file, chunkHeader->subtype))
	return XcursorFalse;
    if (!_XcursorWriteUInt (file, chunkHeader->version))
	return XcursorFalse;
    return XcursorTrue;
}

#define dist(a,b)   ((a) > (b) ? (a) - (b) : (b) - (a))

static XcursorDim
_XcursorFindBestSize (XcursorFileHeader *fileHeader,
		      XcursorDim	size,
		      int		*nsizesp)
{
    int		n;
    int		nsizes = 0;
    XcursorDim	bestSize = 0;
    XcursorDim	thisSize;

    if (!fileHeader || !nsizesp)
        return 0;

    for (n = 0; n < fileHeader->ntoc; n++)
    {
	if (fileHeader->tocs[n].type != XCURSOR_IMAGE_TYPE)
	    continue;
	thisSize = fileHeader->tocs[n].subtype;
	if (!bestSize || dist (thisSize, size) < dist (bestSize, size))
	{
	    bestSize = thisSize;
	    nsizes = 1;
	}
	else if (thisSize == bestSize)
	    nsizes++;
    }
    *nsizesp = nsizes;
    return bestSize;
}

static int
_XcursorFindImageToc (XcursorFileHeader	*fileHeader,
		      XcursorDim	size,
		      int		count)
{
    int			toc;
    XcursorDim		thisSize;

    if (!fileHeader)
        return 0;

    for (toc = 0; toc < fileHeader->ntoc; toc++)
    {
	if (fileHeader->tocs[toc].type != XCURSOR_IMAGE_TYPE)
	    continue;
	thisSize = fileHeader->tocs[toc].subtype;
	if (thisSize != size)
	    continue;
	if (!count)
	    break;
	count--;
    }
    if (toc == fileHeader->ntoc)
	return -1;
    return toc;
}

static XcursorImage *
_XcursorReadImage (XcursorFile		*file, 
		   XcursorFileHeader	*fileHeader,
		   int			toc)
{
    XcursorChunkHeader	chunkHeader;
    XcursorImage	head;
    XcursorImage	*image;
    int			n;
    XcursorPixel	*p;

    if (!file || !fileHeader)
        return NULL;

    if (!_XcursorFileReadChunkHeader (file, fileHeader, toc, &chunkHeader))
	return NULL;
    if (!_XcursorReadUInt (file, &head.width))
	return NULL;
    if (!_XcursorReadUInt (file, &head.height))
	return NULL;
    if (!_XcursorReadUInt (file, &head.xhot))
	return NULL;
    if (!_XcursorReadUInt (file, &head.yhot))
	return NULL;
    if (!_XcursorReadUInt (file, &head.delay))
	return NULL;
    /* sanity check data */
    if (head.width >= 0x10000 || head.height > 0x10000)
	return NULL;
    if (head.width == 0 || head.height == 0)
	return NULL;
    if (head.xhot > head.width || head.yhot > head.height)
	return NULL;
    
    /* Create the image and initialize it */
    image = XcursorImageCreate (head.width, head.height);
    if (chunkHeader.version < image->version)
	image->version = chunkHeader.version;
    image->size = chunkHeader.subtype;
    image->xhot = head.xhot;
    image->yhot = head.yhot;
    image->delay = head.delay;
    n = image->width * image->height;
    p = image->pixels;
    while (n--)
    {
	if (!_XcursorReadUInt (file, p))
	{
	    XcursorImageDestroy (image);
	    return NULL;
	}
	p++;
    }
    return image;
}

static XcursorUInt
_XcursorImageLength (XcursorImage   *image)
{
    if (!image)
        return 0;

    return XCURSOR_IMAGE_HEADER_LEN + (image->width * image->height) * 4;
}

static XcursorBool
_XcursorWriteImage (XcursorFile		*file, 
		    XcursorFileHeader	*fileHeader,
		    int			toc,
		    XcursorImage	*image)
{
    XcursorChunkHeader	chunkHeader;
    int			n;
    XcursorPixel	*p;

    if (!file || !fileHeader || !image)
        return XcursorFalse;

    /* sanity check data */
    if (image->width > XCURSOR_IMAGE_MAX_SIZE  ||
	image->height > XCURSOR_IMAGE_MAX_SIZE)
	return XcursorFalse;
    if (image->width == 0 || image->height == 0)
	return XcursorFalse;
    if (image->xhot > image->width || image->yhot > image->height)
	return XcursorFalse;
    
    /* write chunk header */
    chunkHeader.header = XCURSOR_IMAGE_HEADER_LEN;
    chunkHeader.type = XCURSOR_IMAGE_TYPE;
    chunkHeader.subtype = image->size;
    chunkHeader.version = XCURSOR_IMAGE_VERSION;
    
    if (!_XcursorFileWriteChunkHeader (file, fileHeader, toc, &chunkHeader))
	return XcursorFalse;
    
    /* write extra image header fields */
    if (!_XcursorWriteUInt (file, image->width))
	return XcursorFalse;
    if (!_XcursorWriteUInt (file, image->height))
	return XcursorFalse;
    if (!_XcursorWriteUInt (file, image->xhot))
	return XcursorFalse;
    if (!_XcursorWriteUInt (file, image->yhot))
	return XcursorFalse;
    if (!_XcursorWriteUInt (file, image->delay))
	return XcursorFalse;
    
    /* write the image */
    n = image->width * image->height;
    p = image->pixels;
    while (n--)
    {
	if (!_XcursorWriteUInt (file, *p))
	    return XcursorFalse;
	p++;
    }
    return XcursorTrue;
}

static XcursorComment *
_XcursorReadComment (XcursorFile	    *file, 
		     XcursorFileHeader	    *fileHeader,
		     int		    toc)
{
    XcursorChunkHeader	chunkHeader;
    XcursorUInt		length;
    XcursorComment	*comment;

    if (!file || !fileHeader)
        return NULL;

    /* read chunk header */
    if (!_XcursorFileReadChunkHeader (file, fileHeader, toc, &chunkHeader))
	return NULL;
    /* read extra comment header fields */
    if (!_XcursorReadUInt (file, &length))
	return NULL;
    comment = XcursorCommentCreate (chunkHeader.subtype, length);
    if (!comment)
	return NULL;
    if (!_XcursorReadBytes (file, comment->comment, length))
    {
	XcursorCommentDestroy (comment);
	return NULL;
    }
    comment->comment[length] = '\0';
    return comment;
}

static XcursorUInt
_XcursorCommentLength (XcursorComment	    *comment)
{
    return XCURSOR_COMMENT_HEADER_LEN + strlen (comment->comment);
}

static XcursorBool
_XcursorWriteComment (XcursorFile	    *file, 
		      XcursorFileHeader	    *fileHeader,
		      int		    toc,
		      XcursorComment	    *comment)
{
    XcursorChunkHeader	chunkHeader;
    XcursorUInt		length;

    if (!file || !fileHeader || !comment || !comment->comment)
        return XcursorFalse;

    length = strlen (comment->comment);
    
    /* sanity check data */
    if (length > XCURSOR_COMMENT_MAX_LEN)
	return XcursorFalse;
    
    /* read chunk header */
    chunkHeader.header = XCURSOR_COMMENT_HEADER_LEN;
    chunkHeader.type = XCURSOR_COMMENT_TYPE;
    chunkHeader.subtype = comment->comment_type;
    chunkHeader.version = XCURSOR_COMMENT_VERSION;
    
    if (!_XcursorFileWriteChunkHeader (file, fileHeader, toc, &chunkHeader))
	return XcursorFalse;
    
    /* write extra comment header fields */
    if (!_XcursorWriteUInt (file, length))
	return XcursorFalse;
    
    if (!_XcursorWriteBytes (file, comment->comment, length))
	return XcursorFalse;
    return XcursorTrue;
}

XcursorImage *
XcursorXcFileLoadImage (XcursorFile *file, int size)
{
    XcursorFileHeader	*fileHeader;
    XcursorDim		bestSize;
    int			nsize;
    int			toc;
    XcursorImage	*image;
    
    if (size < 0)
	return NULL;
    fileHeader = _XcursorReadFileHeader (file);
    if (!fileHeader)
	return NULL;
    bestSize = _XcursorFindBestSize (fileHeader, (XcursorDim) size, &nsize);
    if (!bestSize)
	return NULL;
    toc = _XcursorFindImageToc (fileHeader, bestSize, 0);
    if (toc < 0)
	return NULL;
    image = _XcursorReadImage (file, fileHeader, toc);
    _XcursorFileHeaderDestroy (fileHeader);
    return image;
}

XcursorImages *
XcursorXcFileLoadImages (XcursorFile *file, int size)
{
    XcursorFileHeader	*fileHeader;
    XcursorDim		bestSize;
    int			nsize;
    XcursorImages	*images;
    int			n;
    int			toc;
    
    if (!file || size < 0)
	return NULL;
    fileHeader = _XcursorReadFileHeader (file);
    if (!fileHeader)
	return NULL;
    bestSize = _XcursorFindBestSize (fileHeader, (XcursorDim) size, &nsize);
    if (!bestSize)
    {
        _XcursorFileHeaderDestroy (fileHeader);
	return NULL;
    }
    images = XcursorImagesCreate (nsize);
    if (!images)
    {
        _XcursorFileHeaderDestroy (fileHeader);
	return NULL;
    }
    for (n = 0; n < nsize; n++)
    {
	toc = _XcursorFindImageToc (fileHeader, bestSize, n);
	if (toc < 0)
	    break;
	images->images[images->nimage] = _XcursorReadImage (file, fileHeader, 
							    toc);
	if (!images->images[images->nimage])
	    break;
	images->nimage++;
    }
    _XcursorFileHeaderDestroy (fileHeader);
    if (images->nimage != nsize)
    {
	XcursorImagesDestroy (images);
	images = NULL;
    }
    return images;
}

XcursorImages *
XcursorXcFileLoadAllImages (XcursorFile *file)
{
    XcursorFileHeader	*fileHeader;
    XcursorImage	*image;
    XcursorImages	*images;
    int			nimage;
    int			n;
    int			toc;
    
    if (!file)
        return NULL;
    
    fileHeader = _XcursorReadFileHeader (file);
    if (!fileHeader)
	return NULL;
    nimage = 0;
    for (n = 0; n < fileHeader->ntoc; n++)
    {
	switch (fileHeader->tocs[n].type) {
	case XCURSOR_IMAGE_TYPE:
	    nimage++;
	    break;
	}
    }
    images = XcursorImagesCreate (nimage);
    if (!images)
	return NULL;
    for (toc = 0; toc < fileHeader->ntoc; toc++)
    {
	switch (fileHeader->tocs[toc].type) {
	case XCURSOR_IMAGE_TYPE:
	    image = _XcursorReadImage (file, fileHeader, toc);
	    if (image)
	    {
		images->images[images->nimage] = image;
		images->nimage++;
	    }
	    break;
	}
    }
    _XcursorFileHeaderDestroy (fileHeader);
    if (images->nimage != nimage)
    {
	XcursorImagesDestroy (images);
	images = NULL;
    }
    return images;
}

XcursorBool
XcursorXcFileLoad (XcursorFile	    *file,
		   XcursorComments  **commentsp,
		   XcursorImages    **imagesp)
{
    XcursorFileHeader	*fileHeader;
    int			nimage;
    int			ncomment;
    XcursorImages	*images;
    XcursorImage	*image;
    XcursorComment	*comment;
    XcursorComments	*comments;
    int			toc;
    
    if (!file)
        return 0;
    fileHeader = _XcursorReadFileHeader (file);
    if (!fileHeader)
	return 0;
    nimage = 0;
    ncomment = 0;
    for (toc = 0; toc < fileHeader->ntoc; toc++)
    {
	switch (fileHeader->tocs[toc].type) {
	case XCURSOR_COMMENT_TYPE:
	    ncomment++;
	    break;
	case XCURSOR_IMAGE_TYPE:
	    nimage++;
	    break;
	}
    }
    images = XcursorImagesCreate (nimage);
    if (!images)
	return 0;
    comments = XcursorCommentsCreate (ncomment);
    if (!comments)
    {
	XcursorImagesDestroy (images);
	return 0;
    }
    for (toc = 0; toc < fileHeader->ntoc; toc++)
    {
	switch (fileHeader->tocs[toc].type) {
	case XCURSOR_COMMENT_TYPE:
	    comment = _XcursorReadComment (file, fileHeader, toc);
	    if (comment)
	    {
		comments->comments[comments->ncomment] = comment;
		comments->ncomment++;
	    }
	    break;
	case XCURSOR_IMAGE_TYPE:
	    image = _XcursorReadImage (file, fileHeader, toc);
	    if (image)
	    {
		images->images[images->nimage] = image;
		images->nimage++;
	    }
	    break;
	}
    }
    _XcursorFileHeaderDestroy (fileHeader);
    if (images->nimage != nimage || comments->ncomment != ncomment)
    {
	XcursorImagesDestroy (images);
	XcursorCommentsDestroy (comments);
	images = NULL;
	comments = NULL;
	return XcursorFalse;
    }
    *imagesp = images;
    *commentsp = comments;
    return XcursorTrue;
}

XcursorBool
XcursorXcFileSave (XcursorFile		    *file, 
		   const XcursorComments    *comments,
		   const XcursorImages	    *images)
{
    XcursorFileHeader	*fileHeader;
    XcursorUInt		position;
    int			n;
    int			toc;
    
    if (!file || !comments || !images)
        return XcursorFalse;
    
    fileHeader = _XcursorFileHeaderCreate (comments->ncomment + images->nimage);
    if (!fileHeader)
	return XcursorFalse;
    
    position = _XcursorFileHeaderLength (fileHeader);

    /*
     * Compute the toc.  Place the images before the comments
     * as they're more often read
     */
    
    toc = 0;
    for (n = 0; n < images->nimage; n++)
    {
	fileHeader->tocs[toc].type = XCURSOR_IMAGE_TYPE;
	fileHeader->tocs[toc].subtype = images->images[n]->size;
	fileHeader->tocs[toc].position = position;
	position += _XcursorImageLength (images->images[n]);
	toc++;
    }
    
    for (n = 0; n < comments->ncomment; n++)
    {
	fileHeader->tocs[toc].type = XCURSOR_COMMENT_TYPE;
	fileHeader->tocs[toc].subtype = comments->comments[n]->comment_type;
	fileHeader->tocs[toc].position = position;
	position += _XcursorCommentLength (comments->comments[n]);
	toc++;
    }
    
    /*
     * Write the header and the toc
     */
    if (!_XcursorWriteFileHeader (file, fileHeader))
	goto bail;
    
    /*
     * Write the images
     */
    toc = 0;
    for (n = 0; n < images->nimage; n++)
    {
	if (!_XcursorWriteImage (file, fileHeader, toc, images->images[n]))
	    goto bail;
	toc++;
    }
    
    /*
     * Write the comments
     */
    for (n = 0; n < comments->ncomment; n++)
    {
	if (!_XcursorWriteComment (file, fileHeader, toc, comments->comments[n]))
	    goto bail;
	toc++;
    }
    
    _XcursorFileHeaderDestroy (fileHeader);
    return XcursorTrue;
bail:
    _XcursorFileHeaderDestroy (fileHeader);
    return XcursorFalse;
}

static int
_XcursorStdioFileRead (XcursorFile *file, unsigned char *buf, int len)
{
    FILE    *f = file->closure;
    return fread (buf, 1, len, f);
}

static int
_XcursorStdioFileWrite (XcursorFile *file, unsigned char *buf, int len)
{
    FILE    *f = file->closure;
    return fwrite (buf, 1, len, f);
}

static int
_XcursorStdioFileSeek (XcursorFile *file, long offset, int whence)
{
    FILE    *f = file->closure;
    return fseek (f, offset, whence);
}

static void
_XcursorStdioFileInitialize (FILE *stdfile, XcursorFile *file)
{
    file->closure = stdfile;
    file->read = _XcursorStdioFileRead;
    file->write = _XcursorStdioFileWrite;
    file->seek = _XcursorStdioFileSeek;
}

XcursorImage *
XcursorFileLoadImage (FILE *file, int size)
{
    XcursorFile	f;

    if (!file)
        return NULL;

    _XcursorStdioFileInitialize (file, &f);
    return XcursorXcFileLoadImage (&f, size);
}

XcursorImages *
XcursorFileLoadImages (FILE *file, int size)
{
    XcursorFile	f;

    if (!file)
        return NULL;

    _XcursorStdioFileInitialize (file, &f);
    return XcursorXcFileLoadImages (&f, size);
}

XcursorImages *
XcursorFileLoadAllImages (FILE *file)
{
    XcursorFile	f;

    if (!file)
        return NULL;

    _XcursorStdioFileInitialize (file, &f);
    return XcursorXcFileLoadAllImages (&f);
}

XcursorBool
XcursorFileLoad (FILE		    *file, 
		 XcursorComments    **commentsp, 
		 XcursorImages	    **imagesp)
{
    XcursorFile	f;

    if (!file || !commentsp || !imagesp)
        return XcursorFalse;

    _XcursorStdioFileInitialize (file, &f);
    return XcursorXcFileLoad (&f, commentsp, imagesp);
}

XcursorBool
XcursorFileSaveImages (FILE *file, const XcursorImages *images)
{
    XcursorComments *comments = XcursorCommentsCreate (0);
    XcursorFile	    f;
    XcursorBool	    ret;
    if (!comments || !file || !images)
	return 0;
    _XcursorStdioFileInitialize (file, &f);
    ret = XcursorXcFileSave (&f, comments, images) && fflush (file) != EOF;
    XcursorCommentsDestroy (comments);
    return ret;
}

XcursorBool
XcursorFileSave (FILE *			file, 
		 const XcursorComments	*comments,
		 const XcursorImages	*images)
{
    XcursorFile	    f;

    if (!file || !comments || !images)
        return XcursorFalse;
    
    _XcursorStdioFileInitialize (file, &f);
    return XcursorXcFileSave (&f, comments, images) && fflush (file) != EOF;
}

XcursorImage *
XcursorFilenameLoadImage (const char *file, int size)
{
    FILE	    *f;
    XcursorImage    *image;

    if (!file || size < 0)
        return NULL;

    f = fopen (file, "r");
    if (!f)
	return NULL;
    image = XcursorFileLoadImage (f, size);
    fclose (f);
    return image;
}

XcursorImages *
XcursorFilenameLoadImages (const char *file, int size)
{
    FILE	    *f;
    XcursorImages   *images;

    if (!file || size < 0)
        return NULL;
    
    f = fopen (file, "r");
    if (!f)
	return NULL;
    images = XcursorFileLoadImages (f, size);
    fclose (f);
    return images;
}

XcursorImages *
XcursorFilenameLoadAllImages (const char *file)
{
    FILE	    *f;
    XcursorImages   *images;

    if (!file)
        return NULL;

    f = fopen (file, "r");
    if (!f)
	return NULL;
    images = XcursorFileLoadAllImages (f);
    fclose (f);
    return images;
}

XcursorBool
XcursorFilenameLoad (const char		*file,
		     XcursorComments	**commentsp,
		     XcursorImages	**imagesp)
{
    FILE	    *f;
    XcursorBool	    ret;

    if (!file)
        return XcursorFalse;

    f = fopen (file, "r");
    if (!f)
	return 0;
    ret = XcursorFileLoad (f, commentsp, imagesp);
    fclose (f);
    return ret;
}

XcursorBool
XcursorFilenameSaveImages (const char *file, const XcursorImages *images)
{
    FILE	    *f;
    XcursorBool	    ret;

    if (!file || !images)
        return XcursorFalse;

    f = fopen (file, "w");
    if (!f)
	return 0;
    ret = XcursorFileSaveImages (f, images);
    return fclose (f) != EOF && ret;
}

XcursorBool
XcursorFilenameSave (const char		    *file, 
		     const XcursorComments  *comments,
		     const XcursorImages    *images)
{
    FILE	    *f;
    XcursorBool	    ret;

    if (!file || !comments || !images)
        return XcursorFalse;

    f = fopen (file, "w");
    if (!f)
	return 0;
    ret = XcursorFileSave (f, comments, images);
    return fclose (f) != EOF && ret;
}
