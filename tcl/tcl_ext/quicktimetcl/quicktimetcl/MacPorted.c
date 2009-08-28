/*
 * MacPorted.c --
 *
 *		Some utility functions just to get some Mac specific 
 *              functionality on Windows for QuickTimeTcl.
 *
 *              IMPORTANT: many functions are stripped down versions just 
 *              to get the basic stuff on Windows, beware!
 */

#include "QuickTimeTclWin.h"
#include "QuickTimeTcl.h"

/*
 *----------------------------------------------------------------------
 *
 * TkSetMacColor --
 *
 *	Populates a Macintosh RGBColor structure from a X style
 *	pixel value.
 *
 * Results:
 *	Returns false if not a real pixel, true otherwise.
 *
 * Side effects:
 *	The variable macColor is updated to the pixels value.
 *
 *----------------------------------------------------------------------
 */

int
TkSetMacColor(
    unsigned long pixel,	/* Pixel value to convert. */
    RGBColor *macColor)		/* Mac color struct to modify. */
{
#ifdef WORDS_BIGENDIAN
    macColor->blue = (unsigned short) ((pixel & 0xFF) << 8);
    macColor->green = (unsigned short) (((pixel >> 8) & 0xFF) << 8);
    macColor->red = (unsigned short) (((pixel >> 16) & 0xFF) << 8);
#else
    macColor->red = (unsigned short) (((pixel >> 24) & 0xFF) << 8);
    macColor->green = (unsigned short) (((pixel >> 16) & 0xFF) << 8);
    macColor->blue = (unsigned short) (((pixel >> 8) & 0xFF) << 8);
#endif
    return true;
}
