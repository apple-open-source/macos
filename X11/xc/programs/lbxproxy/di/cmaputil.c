/* $Xorg: cmaputil.c,v 1.5 2001/02/09 02:05:31 xorgcvs Exp $ */

/*
Copyright 1996, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.
*/

/*
 * Copyright 1994 Network Computing Devices, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name Network Computing Devices, Inc. not be
 * used in advertising or publicity pertaining to distribution of this
 * software without specific, written prior permission.
 *
 * THIS SOFTWARE IS PROVIDED `AS-IS'.  NETWORK COMPUTING DEVICES, INC.,
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING WITHOUT
 * LIMITATION ALL IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE, OR NONINFRINGEMENT.  IN NO EVENT SHALL NETWORK
 * COMPUTING DEVICES, INC., BE LIABLE FOR ANY DAMAGES WHATSOEVER, INCLUDING
 * SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES, INCLUDING LOSS OF USE, DATA,
 * OR PROFITS, EVEN IF ADVISED OF THE POSSIBILITY THEREOF, AND REGARDLESS OF
 * WHETHER IN AN ACTION IN CONTRACT, TORT OR NEGLIGENCE, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */
/* $XFree86: xc/programs/lbxproxy/di/cmaputil.c,v 1.11 2002/05/31 18:46:07 dawes Exp $ */

#include	<stdio.h>
#include	"misc.h"
#include	"lbx.h"
#include	"assert.h"
#include	"colormap.h"
#include	"util.h"
#include	"resource.h"
#include	"wire.h"
#define  XK_LATIN1
#include 	"keysymdef.h"

typedef struct {
    Colormap    mid;
} colorResource;


static int  num_visuals = 0;

LbxVisualPtr *visuals;


int
CreateVisual(depth, vis)
    int         depth;
    xVisualType *vis;
{
    LbxVisualPtr pvis;

    pvis = (LbxVisualPtr) xalloc(sizeof(LbxVisualRec));
    if (!pvis)
	return 0;
    pvis->id = vis->visualID;
    pvis->class = vis->class;
    pvis->depth = depth;
    pvis->bitsPerRGB = vis->bitsPerRGB;
    pvis->colormapEntries = vis->colormapEntries;
    pvis->redMask = vis->redMask;
    pvis->greenMask = vis->greenMask;
    pvis->blueMask = vis->blueMask;
    if ((pvis->class | DynamicClass) == DirectColor) {
	pvis->offsetRed = 0;
	while (pvis->redMask && !((pvis->redMask >> pvis->offsetRed) & 1))
	    pvis->offsetRed++;
	pvis->offsetGreen = 0;
	while (pvis->greenMask && !((pvis->greenMask >> pvis->offsetGreen) & 1))
	    pvis->offsetGreen++;
	pvis->offsetBlue = 0;
	while (pvis->blueMask && !((pvis->blueMask >> pvis->offsetBlue) & 1))
	    pvis->offsetBlue++;
    }
    visuals = (LbxVisualPtr *) xrealloc(visuals,
				   (num_visuals + 1) * sizeof(LbxVisualPtr));
    if (!visuals) {
	xfree(pvis);
	return 0;
    }
    visuals[num_visuals++] = pvis;

    return 1;
}

LbxVisualPtr
GetVisual(vid)
    VisualID    vid;
{
    LbxVisualPtr pvis;
    int         i;

    for (i = 0; i < num_visuals; i++) {
	pvis = visuals[i];
	if (pvis->id == vid)
	    return pvis;
    }
    assert(0);
    return (LbxVisualPtr) 0;
}


/*
 * ISO Latin-1 case conversion routine
 *
 * this routine always null-terminates the result, so
 * beware of too-small buffers
 */

static void
CopyISOLatin1Lowered(dest, source, length)
    register unsigned char *dest, *source;
    int length;
{
    register int i;

    for (i = 0; i < length; i++, source++, dest++)
    {
	if ((*source >= XK_A) && (*source <= XK_Z))
	    *dest = *source + (XK_a - XK_A);
	else if ((*source >= XK_Agrave) && (*source <= XK_Odiaeresis))
	    *dest = *source + (XK_agrave - XK_Agrave);
	else if ((*source >= XK_Ooblique) && (*source <= XK_Thorn))
	    *dest = *source + (XK_oslash - XK_Ooblique);
	else
	    *dest = *source;
    }
    *dest = '\0';
}


/*
 * colormap cache code
 *
 * handles RGB database and AllocColor
 *
 */


/*
 * The dbm routines are a porting hassle. This implementation will do
 * the same thing by reading the rgb.txt file directly, which is much
 * more portable.
 */

#define HASHSIZE 511

typedef struct _dbEntry * dbEntryPtr;
typedef struct _dbEntry {
  dbEntryPtr     link;
  unsigned short red;
  unsigned short green;
  unsigned short blue;
  char           name[1];	/* some compilers complain if [0] */
} dbEntry;


static dbEntryPtr hashTab[HASHSIZE];

static dbEntryPtr
lookup(name, len, create)
     char *name;
     int  len;
     Bool create;
{
  unsigned int h = 0, g;
  dbEntryPtr   entry, *prev = NULL;
  char         *str = name;

  if (!(name = (char*)ALLOCATE_LOCAL(len +1))) return NULL;
  CopyISOLatin1Lowered((unsigned char *)name, (unsigned char *)str, len);
  name[len] = '\0';

  for(str = name; *str; str++) {
    h = (h << 4) + *str;
    if ((g = h) & 0xf0000000) h ^= (g >> 24);
    h &= g;
  }
  h %= HASHSIZE;

  if (( entry = hashTab[h] ))
    {
      for( ; entry; prev = (dbEntryPtr*)entry, entry = entry->link )
	if (! strcmp(name, entry->name) ) break;
    }
  else
    prev = &(hashTab[h]);

  if (!entry && create && (entry = (dbEntryPtr)xalloc(sizeof(dbEntry) +len)))
    {
      *prev = entry;
      entry->link = NULL;
      strcpy( entry->name, name );
    }

  DEALLOCATE_LOCAL(name);

  return entry;
}

extern char *rgbPath;

static Bool have_rgb_db = FALSE;

Bool
InitColors()
{
  FILE       *rgb;
  char       *path;
  char       line[BUFSIZ];
  char       name[BUFSIZ];
  int        red, green, blue, lineno = 0;
  dbEntryPtr entry;

  if (!rgbPath)
      return TRUE;
  if (!have_rgb_db)
    {
#ifdef __UNIXOS2__
      rgbPath = (char*)__XOS2RedirRoot(rgbPath);
#endif
      path = (char*)ALLOCATE_LOCAL(strlen(rgbPath) +5);
      strcpy(path, rgbPath);
      strcat(path, ".txt");

      if (!(rgb = fopen(path, "r")))
        {
	   ErrorF( "Couldn't open RGB_DB '%s'\n", rgbPath );
	   DEALLOCATE_LOCAL(path);
	   return FALSE;
	}

      while(fgets(line, sizeof(line), rgb))
	{
	  lineno++;
	  if (sscanf(line,"%d %d %d %[^\n]\n", &red, &green, &blue, name) == 4)
	    {
	      if (red >= 0   && red <= 0xff &&
		  green >= 0 && green <= 0xff &&
		  blue >= 0  && blue <= 0xff)
		{
		  if ((entry = lookup(name, strlen(name), TRUE)))
		    {
		      entry->red   = (red * 65535)   / 255;
		      entry->green = (green * 65535) / 255;
		      entry->blue  = (blue  * 65535) / 255;
		    }
		}
	      else
		ErrorF("Value for \"%s\" out of range: %s:%d\n",
		       name, path, lineno);
	    }
	  else if (*line && *line != '#' && *line != '!')
	    ErrorF("Syntax Error: %s:%d\n", path, lineno);
	}
      
      fclose(rgb);
      DEALLOCATE_LOCAL(path);

      have_rgb_db = TRUE;
    }

  return TRUE;
}



static Bool
OsLookupColor(name, len, pred, pgreen, pblue)
    char	   *name;
    unsigned	   len;
    unsigned short *pred, *pgreen, *pblue;

{
  dbEntryPtr entry;

  if ((entry = lookup(name, len, FALSE)))
    {
      *pred   = entry->red;
      *pgreen = entry->green;
      *pblue  = entry->blue;
      return TRUE;
    }

  return FALSE;
}

/*
 * colorname cache
 */


static int
Hash(name, len)
    char       *name;
    int         len;
{
    int         hash = 0;

    while (len--)
	hash = (hash << 1) ^ *name++;
    if (hash < 0)
	hash = -hash;
    return hash;
}

RGBEntryPtr
FindColorName(server, name, len, pVisual)
    XServerPtr	 server;
    char        *name;
    int          len;
    LbxVisualPtr pVisual;
{
    RGBCacheEntryPtr ce;
    int hash;
    char cname[BUFSIZ];

    if (have_rgb_db &&
	OsLookupColor(name, len,
		      &server->rgb_buf.xred, 
		      &server->rgb_buf.xgreen, 
		      &server->rgb_buf.xblue)) {

	server->rgb_buf.vred   = server->rgb_buf.xred;
	server->rgb_buf.vgreen = server->rgb_buf.xgreen;
	server->rgb_buf.vblue  = server->rgb_buf.xblue;

	(*LbxResolveColor)(pVisual,
			   &server->rgb_buf.vred, 
			   &server->rgb_buf.vgreen, 
			   &server->rgb_buf.vblue);

	return &server->rgb_buf;
    }

    CopyISOLatin1Lowered((unsigned char *)cname, (unsigned char *)name, len);
    hash = Hash(cname, len) % NBUCKETS;

    ce = server->rgb_cache[hash];

    while (ce) {
	if ((ce->color.visual == pVisual->id) &&
	    (ce->color.namelen == len) &&
	    !strncmp(ce->color.name, cname, len)) {
	    return &ce->color;
	}
	ce = ce->next;
    }
    return (RGBEntryPtr) NULL;
}

Bool
AddColorName(server, name, len, rgbe)
    XServerPtr	server;
    char       *name;
    int         len;
    RGBEntryRec *rgbe;
{
    RGBCacheEntryPtr new;
    int         hash;

    hash = Hash(name, len) % NBUCKETS;

    new = (RGBCacheEntryPtr) xalloc(sizeof(RGBCacheEntryRec));
    if (!new)
	return FALSE;
    new->next = server->rgb_cache[hash];
    server->rgb_cache[hash] = new;
    new->color = *rgbe;
    new->color.name = (char *)xalloc(len + 1);
    CopyISOLatin1Lowered((unsigned char *)new->color.name,
			 (unsigned char *)name, len);
    new->color.namelen = len;
    return TRUE;
}


void
FreeColors()
{
    RGBCacheEntryPtr ce,
                nce;
    int         i, j;

    for (i = 0; i < lbxMaxServers; i++) {
	if (servers[i]) {
	    for (j = 0; j < NBUCKETS; j++) {
		for (ce = servers[i]->rgb_cache[j]; ce; ce = nce) {
		    nce = ce->next;
		    xfree(ce->color.name);
		    xfree(ce);
		}
		servers[i]->rgb_cache[j] = NULL;
	    }
	}
    }
}



/* ARGSUSED */
int
DestroyColormap(client, value, id)
    ClientPtr	client;
    pointer	value;
    XID		id;
{
    ColormapPtr pmap = (ColormapPtr)value;
    xfree(pmap);
    return Success;
}

/* ------------------------------------------------------------------------- */

static int
find_matching_pixel(pent, num, channels, red, green, blue, pe)
    Entry      *pent;
    int         num;
    int		channels;
    CARD32      red,
                green,
                blue;
    Entry     **pe;
{
    int         i;

    /* OK if only server has it allocated, because even if a
     * FreeCell is in transit for it, only we can realloc it */
    for (i = 0; i < num; pent++, i++) {
	if (pent->status == PIXEL_SHARED) {
	    switch (channels) {
	    case DoRed:
		if (pent->red == red)
		{
		    *pe = pent;
		    return 1;
		}
		break;
	    case DoGreen:
		if (pent->green == green)
		{
		    *pe = pent;
		    return 1;
		}
		break;
	    case DoBlue:
		if (pent->blue == blue)
		{
		    *pe = pent;
		    return 1;
		}
		break;
	    default:
		if (pent->red == red &&
		    pent->green == green &&
		    pent->blue == blue)
		{
		    *pe = pent;
		    return 1;
		}
	    }
	}
    }
    *pe = NULL;
    return 0;
}

/* ARGSUSED */
int
FindPixel(client, pmap, red, green, blue, pent)
    ClientPtr   client;
    ColormapPtr pmap;
    CARD32      red,
                green,
                blue;
    Entry     **pent;
{
    Entry *p;

    switch (pmap->pVisual->class) {
    case PseudoColor:
    case GrayScale:
	return find_matching_pixel(pmap->red, pmap->pVisual->colormapEntries,
				   DoRed|DoGreen|DoBlue,
				   red, green, blue, pent);
    case StaticGray:
    case StaticColor:
	if (pmap->grab_status != CMAP_GRABBED)
	    break;
	*pent = (*LbxFindBestPixel)(pmap, red, green, blue,
				    DoRed|DoGreen|DoBlue);
	return 1;
    case TrueColor:
	if (pmap->grab_status != CMAP_GRABBED)
	    break;
	p = (*LbxFindBestPixel)(pmap, red, green, blue, DoRed);
	client->server->rgb_ent.pixel = p->pixel << pmap->pVisual->offsetRed;
	client->server->rgb_ent.red = p->red;
	client->server->rgb_ent.refcnt = p->refcnt;
	p = (*LbxFindBestPixel)(pmap, red, green, blue, DoGreen);
	client->server->rgb_ent.pixel |= p->pixel << pmap->pVisual->offsetGreen;
	client->server->rgb_ent.green = p->green;
	if (!p->refcnt)
	    client->server->rgb_ent.refcnt = 0;
	p = (*LbxFindBestPixel)(pmap, red, green, blue, DoBlue);
	client->server->rgb_ent.pixel |= p->pixel << pmap->pVisual->offsetBlue;
	client->server->rgb_ent.blue = p->blue;
	if (!p->refcnt)
	    client->server->rgb_ent.refcnt = 0;
	*pent = &client->server->rgb_ent;
	return 1;
    case DirectColor:
	if (!find_matching_pixel(pmap->red, NUMRED(pmap->pVisual),
				 DoRed, red, green, blue, &p))
	    break;
	client->server->rgb_ent.pixel = p->pixel << pmap->pVisual->offsetRed;
	client->server->rgb_ent.red = p->red;
	client->server->rgb_ent.refcnt = p->refcnt;
	if (!find_matching_pixel(pmap->green, NUMGREEN(pmap->pVisual),
				 DoGreen, red, green, blue, &p))
	    break;
	client->server->rgb_ent.pixel |= p->pixel << pmap->pVisual->offsetGreen;
	client->server->rgb_ent.green = p->green;
	if (!p->refcnt)
	    client->server->rgb_ent.refcnt = 0;
	if (!find_matching_pixel(pmap->blue, NUMBLUE(pmap->pVisual),
				 DoBlue, red, green, blue, &p))
	    break;
	client->server->rgb_ent.pixel |= p->pixel << pmap->pVisual->offsetBlue;
	client->server->rgb_ent.blue = p->blue;
	if (!p->refcnt)
	    client->server->rgb_ent.refcnt = 0;
	*pent = &client->server->rgb_ent;
	return 1;
    }
    *pent = NULL;
    return 0;
}

static int
AddPixel(pclient, pmap, pixel)
    ClientPtr   pclient;
    ColormapPtr pmap;
    Pixel pixel;
{
    colorResource *pcr;
    int         npix;
    Pixel      *ppix;
    int         client = pclient->index;

    if ((pmap->pVisual->class | DynamicClass) != DirectColor) {
	npix = pmap->numPixelsRed[client];
	ppix = (Pixel *) xrealloc(pmap->clientPixelsRed[client],
				  (npix + 1) * sizeof(Pixel));
	if (!ppix)
	    return 0;
	ppix[npix] = pixel;
	pmap->clientPixelsRed[client] = ppix;
	pmap->numPixelsRed[client]++;
    } else {
	npix = pmap->numPixelsRed[client];
	ppix = (Pixel *) xrealloc(pmap->clientPixelsRed[client],
				  (npix + 1) * sizeof(Pixel));
	if (!ppix)
	    return 0;
	ppix[npix] = REDPART(pmap->pVisual, pixel);
	pmap->clientPixelsRed[client] = ppix;
	pmap->numPixelsRed[client]++;
	npix = pmap->numPixelsGreen[client];
	ppix = (Pixel *) xrealloc(pmap->clientPixelsGreen[client],
				  (npix + 1) * sizeof(Pixel));
	if (!ppix)
	    return 0;
	ppix[npix] = GREENPART(pmap->pVisual, pixel);
	pmap->clientPixelsGreen[client] = ppix;
	pmap->numPixelsGreen[client]++;
	npix = pmap->numPixelsBlue[client];
	ppix = (Pixel *) xrealloc(pmap->clientPixelsBlue[client],
				  (npix + 1) * sizeof(Pixel));
	if (!ppix)
	    return 0;
	ppix[npix] = BLUEPART(pmap->pVisual, pixel);
	pmap->clientPixelsBlue[client] = ppix;
	pmap->numPixelsBlue[client]++;
    }
    if ((pclient->index != client) &&
	(pmap->numPixelsRed[client] == 1)) {
	pcr = (colorResource *) xalloc(sizeof(colorResource));
	if (!pcr)
	    return 0;
	pcr->mid = pmap->id;
	AddResource(pclient, FakeClientID(client), RT_CMAPENTRY, (pointer) pcr);
    }
    return 1;
}

int
IncrementPixel(pclient, pmap, pent, from_server)
    ClientPtr   pclient;
    ColormapPtr pmap;
    Entry      *pent;
    Bool	from_server;
{

    if (!AddPixel(pclient, pmap, pent->pixel))
	return 0;

    /* we have to AllocColor if refcnt is 0, even if server_ref is 1,
     * because the server might have a FreeCell for it in flight */
    if (!from_server) {
	if (!pent->refcnt && (pmap->pVisual->class & DynamicClass))
	    SendAllocColor(pclient, pmap->id, pent->pixel,
			   pent->red, pent->green, pent->blue);
	else
	    SendIncrementPixel(pclient, pmap->id, pent->pixel);
    }
    if ((pmap->pVisual->class | DynamicClass) != DirectColor)
	pent->refcnt++;
    else {
	pmap->red[REDPART(pmap->pVisual, pent->pixel)].refcnt++;
	pmap->green[GREENPART(pmap->pVisual, pent->pixel)].refcnt++;
	pmap->blue[BLUEPART(pmap->pVisual, pent->pixel)].refcnt++;
    }
    return 1;
}

int
AllocCell(client, pmap, pixel)
    ClientPtr   client;
    ColormapPtr pmap;
    Pixel pixel;
{
    Entry *pent;
    Pixel p;

    if (!AddPixel(client, pmap, pixel))
	return 0;
    switch (pmap->pVisual->class) {
    case PseudoColor:
    case GrayScale:
	pent = &pmap->red[pixel];
	pent->pixel = pixel;
	pent->status = PIXEL_PRIVATE;
	pent->server_ref = 0;
	return 1;
    case DirectColor:
	p = REDPART(pmap->pVisual, pixel);
	pent = &pmap->red[p];
	pent->pixel = p;
	pent->status = PIXEL_PRIVATE;
	pent->server_ref = 0;
	p = GREENPART(pmap->pVisual, pixel);
	pent = &pmap->green[p];
	pent->pixel = p;
	pent->status = PIXEL_PRIVATE;
	pent->server_ref = 0;
	p = BLUEPART(pmap->pVisual, pixel);
	pent = &pmap->blue[p];
	pent->pixel = p;
	pent->status = PIXEL_PRIVATE;
	pent->server_ref = 0;
	return 1;
    default:
	fprintf(stderr, "storing pixel in class %d colormap\n",
		pmap->pVisual->class);
	break;
    }
    return 0;
}

int
StorePixel (client, pmap, red, green, blue, pixel, from_server)
    ClientPtr   client;
    ColormapPtr pmap;
    CARD32      red,
                green,
                blue;
    Pixel       pixel;
    Bool	from_server;
{
    Entry      *pent;
    Pixel	p;

    switch (pmap->pVisual->class) {
    case PseudoColor:
    case GrayScale:
	pent = pmap->red;
	pent[pixel].red = red;
	pent[pixel].green = green;
	pent[pixel].blue = blue;
	pent[pixel].pixel = pixel;
	if (pent[pixel].status != PIXEL_SHARED)
	    pent[pixel].refcnt = 0;
	pent[pixel].status = PIXEL_SHARED;
	pent[pixel].server_ref = 1;
	IncrementPixel(client, pmap, &pent[pixel], from_server);
	return 1;
    case DirectColor:
	client->server->rgb_ent.refcnt = 1;
	p = REDPART(pmap->pVisual, pixel);
	pent = pmap->red;
	pent[p].red = red;
	pent[p].pixel = p;
	if (pent[p].status != PIXEL_SHARED)
	    client->server->rgb_ent.refcnt = pent[p].refcnt = 0;
	pent[p].status = PIXEL_SHARED;
	pent[p].server_ref = 1;
	p = GREENPART(pmap->pVisual, pixel);
	pent = pmap->green;
	pent[p].green = green;
	pent[p].pixel = p;
	if (pent[p].status != PIXEL_SHARED)
	    client->server->rgb_ent.refcnt = pent[p].refcnt = 0;
	pent[p].status = PIXEL_SHARED;
	pent[p].server_ref = 1;
	p = BLUEPART(pmap->pVisual, pixel);
	pent = pmap->blue;
	pent[p].blue = blue;
	pent[p].pixel = p;
	if (pent[p].status != PIXEL_SHARED)
	    client->server->rgb_ent.refcnt = pent[p].refcnt = 0;
	pent[p].status = PIXEL_SHARED;
	pent[p].server_ref = 1;
	client->server->rgb_ent.pixel = pixel;
	client->server->rgb_ent.red = red;
	client->server->rgb_ent.green = green;
	client->server->rgb_ent.blue = blue;
	IncrementPixel(client, pmap, &client->server->rgb_ent, from_server);
	return 0;
    default:
	fprintf(stderr, "storing pixel in class %d colormap\n",
		pmap->pVisual->class);
	break;
    }
    return 0;
}

/* ARGSUSED */
static void
FreeCell(pent, pixel)
    Entry      *pent;
    Pixel       pixel;
{
    pent = &pent[pixel];
    if (pent->status == PIXEL_PRIVATE) {
	if (!pent->server_ref)
	    pent->status = PIXEL_FREE;
    } else if (pent->status == PIXEL_SHARED) {
	if (pent->refcnt > 0)
	    pent->refcnt--;
	if (!pent->refcnt && !pent->server_ref)
	    pent->status = PIXEL_FREE;
    }
}

static void
FreeServerCell(pent, pixel)
    Entry      *pent;
    Pixel       pixel;
{
    pent = &pent[pixel];
    if (pent->status == PIXEL_PRIVATE && pent->server_ref)
	pent->status = PIXEL_FREE;
    else if (pent->status == PIXEL_SHARED) {
	if (!pent->refcnt)
	    pent->status = PIXEL_FREE;
	else
	    pent->server_ref = 0;
    }
}

void
GotServerFreeCellsEvent(pmap, pixel_start, pixel_end)
    ColormapPtr pmap;
    Pixel       pixel_start;
    Pixel	pixel_end;
{
    Pixel pixel;
    
    if ((pmap->pVisual->class | DynamicClass) != DirectColor)
	for (pixel = pixel_start; pixel <= pixel_end; pixel++) {
	    FreeServerCell(pmap->red, pixel);
	}
    else
	for (pixel = pixel_start; pixel <= pixel_end; pixel++) {
	    FreeServerCell(pmap->red, REDPART(pmap->pVisual, pixel));
	    FreeServerCell(pmap->green, GREENPART(pmap->pVisual, pixel));
	    FreeServerCell(pmap->blue, BLUEPART(pmap->pVisual, pixel));
	}
}

void
FreeAllClientPixels(pmap, client)
    ColormapPtr pmap;
    int client;
{
    Pixel *ppix, *ppst;
    int n;

    ppst = pmap->clientPixelsRed[client];
    for (ppix = ppst, n = pmap->numPixelsRed[client]; --n >= 0; ppix++)
	FreeCell(pmap->red, *ppix);
    xfree(ppst);
    pmap->clientPixelsRed[client] = (Pixel *) NULL;
    pmap->numPixelsRed[client] = 0;
    if ((pmap->pVisual->class | DynamicClass) == DirectColor) {
	ppst = pmap->clientPixelsGreen[client];
	for (ppix = ppst, n = pmap->numPixelsGreen[client]; --n >= 0; ppix++)
	    FreeCell(pmap->green, *ppix);
	xfree(ppst);
	pmap->clientPixelsGreen[client] = (Pixel *) NULL;
	pmap->numPixelsGreen[client] = 0;
	ppst = pmap->clientPixelsBlue[client];
	for (ppix = ppst, n = pmap->numPixelsBlue[client]; --n >= 0; ppix++)
	    FreeCell(pmap->blue, *ppix);
	xfree(ppst);
	pmap->clientPixelsBlue[client] = (Pixel *) NULL;
	pmap->numPixelsBlue[client] = 0;
    }
}

/* ARGSUSED */
int
FreeClientPixels(client, value, id)
    ClientPtr	client;
    pointer	value;
    XID		id;
{
    colorResource *pcr = (colorResource *)value;
    ColormapPtr pmap;

    pmap = (ColormapPtr) LookupIDByType(client, pcr->mid, RT_COLORMAP);

    if (pmap)
	FreeAllClientPixels(pmap, client->index);
    xfree(pcr);
    return Success;
}

#define GetNextBitsOrBreak(bits, mask, base)	\
	    if((bits) == (mask)) 		\
		break;		 		\
	    (bits) += (base);		 	\
	    while((bits) & ~(mask))		\
		(bits) += ((bits) & ~(mask))

static void
FreeCells(client, pmap, num, pixels, mask, channels)
    ClientPtr   client;
    ColormapPtr pmap;
    int         num;
    Pixel       *pixels;
    Pixel	mask;
    int		channels;
{
    Pixel       pix, base, bits, cmask;
    int         i, zapped, npix, npixnew, offset;
    Pixel      *cptr, *pptr, *ppixClient, **ppixp;
    int	       *npixp;
    Entry      *pent;
    int         idx;

    idx = client->index;
    switch (channels) {
    case DoRed:
	ppixp = &pmap->clientPixelsRed[idx];
	npixp = &pmap->numPixelsRed[idx];
	cmask = pmap->pVisual->redMask;
	offset = pmap->pVisual->offsetRed;
	pent = pmap->red;
	break;
    case DoGreen:
	ppixp = &pmap->clientPixelsGreen[idx];
	npixp = &pmap->numPixelsGreen[idx];
	cmask = pmap->pVisual->greenMask;
	offset = pmap->pVisual->offsetGreen;
	pent = pmap->green;
	break;
    case DoBlue:
	ppixp = &pmap->clientPixelsBlue[idx];
	npixp = &pmap->numPixelsBlue[idx];
	cmask = pmap->pVisual->blueMask;
	offset = pmap->pVisual->offsetBlue;
	pent = pmap->blue;
	break;
    default:
	ppixp = &pmap->clientPixelsRed[idx];
	npixp = &pmap->numPixelsRed[idx];
	cmask = ~((Pixel)0);
	offset = 0;
	pent = pmap->red;
	break;
    }
    mask &= cmask;
    zapped = 0;
    bits = 0;
    base = lowbit(mask);
    while (1)
    {
	for (i = 0; i < num; i++) {
	    pix = (pixels[i] & cmask) >> offset;
	    for (cptr = *ppixp, npix = *npixp;
		 --npix >= 0 && *cptr != pix;
		 cptr++)
		;
	    if (npix >= 0) {
		FreeCell(pent, pix);
		*cptr = ~((Pixel)0);
		zapped++;
	    }
	}
	GetNextBitsOrBreak(bits, mask, base);
    }
    if (zapped) {
	npixnew = *npixp - zapped;
	if (npixnew) {
	    pptr = cptr = *ppixp;
	    for (npix = 0; npix < npixnew; cptr++) {
		if (*cptr != ~((Pixel)0)) {
		    *pptr++ = *cptr;
		    npix++;
		}
	    }
	    ppixClient = *ppixp;
	    pptr = (Pixel *) xrealloc(ppixClient, npixnew * sizeof(Pixel));
	    if (pptr)
		ppixClient = pptr;
	    *npixp = npixnew;
	    *ppixp = ppixClient;
	} else {
	    *npixp = 0;
	    xfree(*ppixp);
	    *ppixp = NULL;
	}
    }
}

int
FreePixels(client, pmap, num, pixels, mask)
    ClientPtr   client;
    ColormapPtr pmap;
    int         num;
    Pixel       *pixels;
    Pixel	mask;
{

    if ((pmap->pVisual->class | DynamicClass) != DirectColor)
	FreeCells(client, pmap, num, pixels, mask, DoRed|DoGreen|DoBlue);
    else {
	FreeCells(client, pmap, num, pixels, mask, DoRed);
	FreeCells(client, pmap, num, pixels, mask, DoGreen);
	FreeCells(client, pmap, num, pixels, mask, DoBlue);
    }
    return 1;
}
