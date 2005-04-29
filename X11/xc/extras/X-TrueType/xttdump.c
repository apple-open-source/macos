/* ===EmacsMode: -*- Mode: C; tab-width:4; c-basic-offset: 4; -*- === */
/* ===FileName: ===
   Copyright (c) 1997 Jyunji Takagi, All rights reserved.
   Copyright (c) 1998 Go Watanabe, All rights reserved.
   Copyright (c) 1998 X-TrueType Server Project, All rights reserved.
  
===Notice
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
   SUCH DAMAGE.

   Major Release ID: X-TrueType Server Version 1.3 [Aoi MATSUBARA Release 3]

Notice===
*/
/* $XFree86: xc/extras/X-TrueType/xttdump.c,v 1.4 2003/10/22 16:25:23 tsi Exp $ */

#include "xttversion.h"

#if 0
static char const * const releaseID =
    _XTT_RELEASE_NAME;
#endif

#ifdef DUMP

#ifndef FONTMODULE
#include <X11/Xos.h>
#endif
#include "xttcommon.h"
#include "fntfilst.h"

void
DumpFontPathElement(FontPathElementPtr ptr)
{
  fprintf(stderr, "FontPathElement 0x%x\n", ptr);
  if ((ptr == NULL) || ((unsigned) ptr >= 0xf0000000)) {
    return;
  }
  /*
  fprintf(stderr, "  name_length %d\n", ptr->name_length);
  fprintf(stderr, "  name \"%s\"\n", ptr->name);
  fprintf(stderr, "  type %d\n", ptr->type);
  fprintf(stderr, "  refcount %d\n", ptr->refcount);
  fprintf(stderr, "  private 0x%x\n", ptr->private);
  fprintf(stderr, "\n");
  */
}

void
DumpxCharInfo(xCharInfo *ptr)
{
  fprintf(stderr, "FontInfo 0x%x\n", ptr);
  if (ptr == NULL) {
    return;
  }
  fprintf(stderr, "  leftSideBearing %d\n", ptr->leftSideBearing);
  fprintf(stderr, "  rightSideBearing %d\n", ptr->rightSideBearing);
  fprintf(stderr, "  characterWidth %d\n", ptr->characterWidth);
  fprintf(stderr, "  ascent %d\n", ptr->ascent);
  fprintf(stderr, "  descent %d\n", ptr->descent);
  fprintf(stderr, "  attributes 0x%x\n", ptr->attributes);
  fprintf(stderr, "\n");
}

void
DumpFontInfo(FontInfoPtr ptr)
{
  fprintf(stderr, "FontInfo 0x%x\n", ptr);
  if (ptr == NULL) {
    return;
  }
  fprintf(stderr, "  firstCol 0x%x\n", ptr->firstCol);
  fprintf(stderr, "  lastCol 0x%x\n", ptr->lastCol);
  fprintf(stderr, "  firstRow 0x%x\n", ptr->firstRow);
  fprintf(stderr, "  lastRow 0x%x\n", ptr->lastRow);
  fprintf(stderr, "  defaultCh 0x%x\n", ptr->defaultCh);
  fprintf(stderr, "  noOverlap %d\n", ptr->noOverlap);
  fprintf(stderr, "  terminalFont %d\n", ptr->terminalFont);
  fprintf(stderr, "  constantMetrics %d\n", ptr->constantMetrics);
  fprintf(stderr, "  constantWidth %d\n", ptr->constantWidth);
  fprintf(stderr, "  inkInside %d\n", ptr->inkInside);
  fprintf(stderr, "  inkMetrics %d\n", ptr->inkMetrics);
  fprintf(stderr, "  allExist %d\n", ptr->allExist);
  fprintf(stderr, "  drawDirection %d\n", ptr->drawDirection);
  fprintf(stderr, "  cachable %d\n", ptr->cachable);
  fprintf(stderr, "  anamorphic %d\n", ptr->anamorphic);
  fprintf(stderr, "  maxOverlap %d\n", ptr->maxOverlap);
  fprintf(stderr, "  pad 0x%x\n", ptr->pad);
  fprintf(stderr, "  maxbounds ->\n"); DumpxCharInfo(&ptr->maxbounds);
  fprintf(stderr, "  minbounds ->\n"); DumpxCharInfo(&ptr->minbounds);
  fprintf(stderr, "  ink_maxbounds ->\n"); DumpxCharInfo(&ptr->ink_maxbounds);
  fprintf(stderr, "  ink_minbounds ->\n"); DumpxCharInfo(&ptr->ink_minbounds);
  fprintf(stderr, "  fontAscent %d\n", ptr->fontAscent);
  fprintf(stderr, "  fontDescent %d\n", ptr->fontDescent);
  fprintf(stderr, "  nprops %d\n", ptr->nprops);
  fprintf(stderr, "  props -> 0x%x\n", ptr->props);
  fprintf(stderr, "  isStringProp \"%s\"\n", ptr->isStringProp);
  fprintf(stderr, "\n");
}

void
DumpFont(FontPtr ptr)
{
  fprintf(stderr, "Font 0x%x\n", ptr);
  if (ptr == NULL) {
    return;
  }
  fprintf(stderr, "  refcnt %d\n", ptr->refcnt);
  fprintf(stderr, "  info ->\n"); DumpFontInfo(&ptr->info);
  fprintf(stderr, "  bit 0x%x\n", ptr->bit);
  fprintf(stderr, "  byte 0x%x\n", ptr->byte);
  fprintf(stderr, "  glyph 0x%x\n", ptr->glyph);
  fprintf(stderr, "  scan 0x%x\n", ptr->scan);
  fprintf(stderr, "  format %x\n", ptr->format);
  fprintf(stderr, "  get_glyphs 0x%x\n", ptr->get_glyphs);
  fprintf(stderr, "  get_metrics 0x%x\n", ptr->get_metrics);
  fprintf(stderr, "  unload_font 0x%x\n", ptr->unload_font);
  fprintf(stderr, "  unload_glyphs 0x%x\n", ptr->unload_glyphs);
  fprintf(stderr, "  fpe ->\n"); DumpFontPathElement(ptr->fpe);
  fprintf(stderr, "  svrPrivate 0x%x\n", ptr->svrPrivate);
  fprintf(stderr, "  fontPrivate 0x%x\n", ptr->fontPrivate);
  fprintf(stderr, "  fpePrivate 0x%x\n", ptr->fpePrivate);
  fprintf(stderr, "  maxPrivate %d\n", ptr->maxPrivate);
  fprintf(stderr, "  devPrivates 0x%x\n", ptr->devPrivates);
  fprintf(stderr, "\n");
}

void
DumpFontName(FontNamePtr ptr)
{
  fprintf(stderr, "FontName 0x%x\n", ptr);
  if (ptr == NULL) {
    return;
  }
  fprintf(stderr, "  name \"%s\"\n", ptr->name);
  fprintf(stderr, "  length %d\n", ptr->length);
  fprintf(stderr, "  ndashes %d\n", ptr->ndashes);
  fprintf(stderr, "\n");
}

void
DumpFontEntry(FontEntryPtr ptr)
{
  fprintf(stderr, "FontEntry 0x%x\n", ptr);
  if (ptr == NULL) {
    return;
  }
  fprintf(stderr, "  name ->\n"); DumpFontName(&ptr->name);
  fprintf(stderr, "  type %d\n", ptr->type);
  switch (ptr->type) {
  case FONT_ENTRY_SCALABLE:
    fprintf(stderr, "  u.scalable.renderer -> 0x%x\n", ptr->u.scalable.renderer);
    fprintf(stderr, "  u.scalable.fileName \"%s\"\n", ptr->u.scalable.fileName);
    fprintf(stderr, "  u.scalable.extra -> 0x%x\n", ptr->u.scalable.extra);
    break;
  case FONT_ENTRY_SCALE_ALIAS:
    break;
  case FONT_ENTRY_BITMAP:
    fprintf(stderr, "  u.bitmap.renderer -> 0x%x\n", ptr->u.bitmap.renderer);
    fprintf(stderr, "  u.bitmap.fileName \"%s\"\n", ptr->u.bitmap.fileName);
    fprintf(stderr, "  u.bitmap.pFont -> 0x%x\n", ptr->u.bitmap.pFont);
    break;
  case FONT_ENTRY_ALIAS:
    fprintf(stderr, "  u.alias.resolved \"%s\"\n", ptr->u.alias.resolved);
    break;
  case FONT_ENTRY_BC:
    fprintf(stderr, "  u.bc.vals ???\n");
    fprintf(stderr, "  u.bc.entry ->\n"); DumpFontEntry(ptr->u.bc.entry);
    break;
  }
  fprintf(stderr, "\n");
}

void
DumpfsRange(fsRange *ptr)
{
  fprintf(stderr, "fsRange 0x%x\n", ptr);
  if (ptr == NULL) {
    return;
  }
  fprintf(stderr, "  min_char_high %d\n", ptr->min_char_high);
  fprintf(stderr, "  min_char_low %d\n", ptr->min_char_low);
  fprintf(stderr, "  max_char_high %d\n", ptr->max_char_high);
  fprintf(stderr, "  max_char_low %d\n", ptr->max_char_low);
  fprintf(stderr, "\n");
}

void
DumpFontScalable(FontScalablePtr ptr)
{
  int i;
  fprintf(stderr, "FontScalable 0x%x\n", ptr);
  if (ptr == NULL) {
    return;
  }
  fprintf(stderr, "  values_supplied 0x%x\n", ptr->values_supplied);
  fprintf(stderr, "  pixel_matrix[] = {%g, %g, %g, %g}\n",
          ptr->pixel_matrix[0], ptr->pixel_matrix[1],
          ptr->pixel_matrix[2], ptr->pixel_matrix[3]);
  fprintf(stderr, "  point_matrix[] = {%g, %g, %g, %g}\n",
          ptr->point_matrix[0], ptr->point_matrix[1],
          ptr->point_matrix[2], ptr->point_matrix[3]);
  fprintf(stderr, "  pixel %d\n", ptr->pixel);
  fprintf(stderr, "  point %d\n", ptr->point);
  fprintf(stderr, "  x %d\n", ptr->x);
  fprintf(stderr, "  y %d\n", ptr->y);
  fprintf(stderr, "  width %d\n", ptr->width);
  fprintf(stderr, "  xlfdName \"%s\"\n", ptr->xlfdName);
  fprintf(stderr, "  nranges %d\n", ptr->nranges);
  for (i = 0; i < ptr->nranges; i++) {
    DumpfsRange(&ptr->ranges[i]);
  }
  fprintf(stderr, "\n");
}

#endif /* DUMP */


/* end of file */
