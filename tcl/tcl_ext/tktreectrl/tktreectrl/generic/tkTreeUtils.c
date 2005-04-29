#include "tkTreeCtrl.h"
#ifdef WIN32
#include "tkWinInt.h"
#endif

/*
 * Forward declarations for procedures defined later in this file:
 */

static int	PadAmountOptionSet _ANSI_ARGS_((ClientData clientData,
			Tcl_Interp *interp, Tk_Window tkwin,
			Tcl_Obj **value, char *recordPtr, int internalOffset,
			char *saveInternalPtr, int flags));
static Tcl_Obj *PadAmountOptionGet _ANSI_ARGS_((ClientData clientData,
			Tk_Window tkwin, char *recordPtr, int internalOffset));
static void	PadAmountOptionRestore _ANSI_ARGS_((ClientData clientData,
			Tk_Window tkwin, char *internalPtr,
			char *saveInternalPtr));
static void	PadAmountOptionFree _ANSI_ARGS_((ClientData clientData,
			Tk_Window tkwin, char *internalPtr));

/*
 * The following Tk_ObjCustomOption structure can be used as clientData entry
 * of a Tk_OptionSpec record with a TK_OPTION_CUSTOM type in the form
 * "(ClientData) &PadAmountOption"; the option will then parse list with
 * one or two screen distances.
 */

Tk_ObjCustomOption PadAmountOption = {
    "pad amount",
    PadAmountOptionSet,
    PadAmountOptionGet,
    PadAmountOptionRestore,
    PadAmountOptionFree
};

void wipefree(char *memPtr, int size)
{
	memset(memPtr, 0xAA, size);
	ckfree(memPtr);
}

void FormatResult(Tcl_Interp *interp, char *fmt, ...)
{
	va_list ap;
	char buf[256];

	va_start(ap, fmt);
	vsprintf(buf, fmt, ap);
	va_end(ap);
	Tcl_SetResult(interp, buf, TCL_VOLATILE);
}

int Ellipsis(Tk_Font tkfont, char *string, int numBytes, int *maxPixels, char *ellipsis)
{
	char staticStr[256], *tmpStr = staticStr;
	int pixels, pixelsTest, bytesThatFit, bytesTest;
	int ellipsisNumBytes = strlen(ellipsis);

	bytesThatFit = Tk_MeasureChars(tkfont, string, numBytes, *maxPixels, 0,
		&pixels);

	/* The whole string fits. No ellipsis needed */
	if (bytesThatFit == numBytes)
	{
		(*maxPixels) = pixels;
		return numBytes;
	}

	if (bytesThatFit <= 1)
	{
		(*maxPixels) = pixels;
		return -bytesThatFit;
	}

	/* Strip off one character at a time, adding ellipsis, until it fits */
	bytesTest = Tcl_UtfPrev(string + bytesThatFit, string) - string;
	if (bytesTest + ellipsisNumBytes > sizeof(staticStr))
		tmpStr = ckalloc(bytesTest + ellipsisNumBytes);
	memcpy(tmpStr, string, bytesTest);
	while (bytesTest > 0)
	{
		memcpy(tmpStr + bytesTest, ellipsis, ellipsisNumBytes);
		numBytes = Tk_MeasureChars(tkfont, tmpStr,
			bytesTest + ellipsisNumBytes,
			*maxPixels, 0, &pixelsTest);
		if (numBytes == bytesTest + ellipsisNumBytes)
		{
			(*maxPixels) = pixelsTest;
			if (tmpStr != staticStr)
				ckfree(tmpStr);
			return bytesTest;
		}
		bytesTest = Tcl_UtfPrev(string + bytesTest, string) - string;
	}

	/* No single char + ellipsis fits. Return number of chars that fit */
	/* Negative tells caller to not add ellipsis */
	(*maxPixels) = pixels;
	if (tmpStr != staticStr)
		ckfree(tmpStr);
	return -bytesThatFit;
}

/* Draws a horizontal 1-pixel tall dotted line */
void HDotLine(TreeCtrl *tree, Drawable drawable, GC gc, int x1, int y1, int x2)
{
#ifdef WIN32
	TkWinDCState state;
	HDC dc;
	HPEN pen, oldPen;
	int nw;
	int wx = x1 + tree->drawableXOrigin;
	int wy = y1 + tree->drawableYOrigin;

	dc = TkWinGetDrawableDC(tree->display, drawable, &state);
	SetROP2(dc, R2_COPYPEN);

	pen = CreatePen(PS_SOLID, 1, gc->foreground);
	oldPen = SelectObject(dc, pen);

	nw = !(wx & 1) == !(wy & 1);
	for (x1 += !nw; x1 < x2; x1 += 2)
	{
		MoveToEx(dc, x1, y1, NULL);
		LineTo(dc, x1 + 1, y1);
	}

	SelectObject(dc, oldPen);
	DeleteObject(pen);

	TkWinReleaseDrawableDC(drawable, dc, &state);
#else
	int nw;
	int wx = x1 + tree->drawableXOrigin;
	int wy = y1 + tree->drawableYOrigin;

	nw = !(wx & 1) == !(wy & 1);
	for (x1 += !nw; x1 < x2; x1 += 2)
	{
		XDrawPoint(tree->display, drawable, gc, x1, y1);
	}
#endif
}

/* Draws a vertical 1-pixel wide dotted line */
void VDotLine(TreeCtrl *tree, Drawable drawable, GC gc, int x1, int y1, int y2)
{
#ifdef WIN32
	TkWinDCState state;
	HDC dc;
	HPEN pen, oldPen;
	int nw;
	int wx = x1 + tree->drawableXOrigin;
	int wy = y1 + tree->drawableYOrigin;

	dc = TkWinGetDrawableDC(tree->display, drawable, &state);
	SetROP2(dc, R2_COPYPEN);

	pen = CreatePen(PS_SOLID, 1, gc->foreground);
	oldPen = SelectObject(dc, pen);

	nw = !(wx & 1) == !(wy & 1);
	for (y1 += !nw; y1 < y2; y1 += 2)
	{
		MoveToEx(dc, x1, y1, NULL);
		LineTo(dc, x1 + 1, y1);
	}

	SelectObject(dc, oldPen);
	DeleteObject(pen);

	TkWinReleaseDrawableDC(drawable, dc, &state);
#else
	int nw;
	int wx = x1 + tree->drawableXOrigin;
	int wy = y1 + tree->drawableYOrigin;

	nw = !(wx & 1) == !(wy & 1);
	for (y1 += !nw; y1 < y2; y1 += 2)
	{
		XDrawPoint(tree->display, drawable, gc, x1, y1);
	}
#endif
}

/* Draws 0 or more sides of a rectangle, dot-on dot-off, XOR style */
void DrawActiveOutline(TreeCtrl *tree, Drawable drawable, int x, int y, int width, int height, int open)
{
#ifdef WIN32
	int wx = x + tree->drawableXOrigin;
	int wy = y + tree->drawableYOrigin;
	int w = !(open & 0x01);
	int n = !(open & 0x02);
	int e = !(open & 0x04);
	int s = !(open & 0x08);
	int nw, ne, sw, se;
	int i;
	TkWinDCState state;
	HDC dc;

	/* Dots on even pixels only */
	nw = !(wx & 1) == !(wy & 1);
	ne = !((wx + width - 1) & 1) == !(wy & 1);
	sw = !(wx & 1) == !((wy + height - 1) & 1);
	se = !((wx + width - 1) & 1) == !((wy + height - 1) & 1);

	dc = TkWinGetDrawableDC(tree->display, drawable, &state);
	SetROP2(dc, R2_NOT);

	if (w) /* left */
	{
		for (i = !nw; i < height; i += 2)
		{
			MoveToEx(dc, x, y + i, NULL);
			LineTo(dc, x + 1, y + i);
		}
	}
	if (n) /* top */
	{
		for (i = nw ? w * 2 : 1; i < width; i += 2)
		{
			MoveToEx(dc, x + i, y, NULL);
			LineTo(dc, x + i + 1, y);
		}
	}
	if (e) /* right */
	{
		for (i = ne ? n * 2 : 1; i < height; i += 2)
		{
			MoveToEx(dc, x + width - 1, y + i, NULL);
			LineTo(dc, x + width, y + i);
		}
	}
	if (s) /* bottom */
	{
		for (i = sw ? w * 2 : 1; i < width - (se && e); i += 2)
		{
			MoveToEx(dc, x + i, y + height - 1, NULL);
			LineTo(dc, x + i + 1, y + height - 1);
		}
	}
	TkWinReleaseDrawableDC(drawable, dc, &state);
#else
	int wx = x + tree->drawableXOrigin;
	int wy = y + tree->drawableYOrigin;
	int w = !(open & 0x01);
	int n = !(open & 0x02);
	int e = !(open & 0x04);
	int s = !(open & 0x08);
	int nw, ne, sw, se;
	int i;
	XGCValues gcValues;
	unsigned long gcMask;
	GC gc;

	/* Dots on even pixels only */
	nw = !(wx & 1) == !(wy & 1);
	ne = !((wx + width - 1) & 1) == !(wy & 1);
	sw = !(wx & 1) == !((wy + height - 1) & 1);
	se = !((wx + width - 1) & 1) == !((wy + height - 1) & 1);

#if defined(MAC_TCL) || defined(MAC_OSX_TK)
	gcValues.function = GXxor;
#else
	gcValues.function = GXinvert;
#endif
	gcMask = GCFunction;
	gc = Tk_GetGC(tree->tkwin, gcMask, &gcValues);

	if (w) /* left */
	{
		for (i = !nw; i < height; i += 2)
		{
			XDrawPoint(tree->display, drawable, gc, x, y + i);
		}
	}
	if (n) /* top */
	{
		for (i = nw ? w * 2 : 1; i < width; i += 2)
		{
			XDrawPoint(tree->display, drawable, gc, x + i, y);
		}
	}
	if (e) /* right */
	{
		for (i = ne ? n * 2 : 1; i < height; i += 2)
		{
			XDrawPoint(tree->display, drawable, gc, x + width - 1, y + i);
		}
	}
	if (s) /* bottom */
	{
		for (i = sw ? w * 2 : 1; i < width - (se && e); i += 2)
		{
			XDrawPoint(tree->display, drawable, gc, x + i, y + height - 1);
		}
	}

	Tk_FreeGC(tree->display, gc);
#endif
}

void DotRect(TreeCtrl *tree, Drawable drawable, int x, int y, int width, int height)
{
	DrawActiveOutline(tree, drawable, x, y, width, height, 0);
}

struct DotStatePriv
{
	TreeCtrl *tree;
	Drawable drawable;
#ifdef WIN32
	HDC dc;
	TkWinDCState dcState;
	HRGN rgn;
#else
	GC gc;
	TkRegion rgn;
#endif
};

void DotRect_Setup(TreeCtrl *tree, Drawable drawable, DotState *p)
{
	struct DotStatePriv *dotState = (struct DotStatePriv *) p;
#ifdef WIN32
#else
	XGCValues gcValues;
	unsigned long mask;
	XRectangle xrect;
#endif

	if (sizeof(*dotState) > sizeof(*p))
		panic("DotRect_Setup: DotState hack is too small");

	dotState->tree = tree;
	dotState->drawable = drawable;
#ifdef WIN32
	dotState->dc = TkWinGetDrawableDC(tree->display, drawable, &dotState->dcState);

	/* XOR drawing */
	SetROP2(dotState->dc, R2_NOT);

	/* Keep drawing inside the contentbox */
	dotState->rgn = CreateRectRgn(
		tree->inset,
		tree->inset + Tree_HeaderHeight(tree),
		Tk_Width(tree->tkwin) - tree->inset,
		Tk_Height(tree->tkwin) - tree->inset);
	SelectClipRgn(dotState->dc, dotState->rgn);
#else
	gcValues.line_style = LineOnOffDash;
	gcValues.line_width = 1;
	gcValues.dash_offset = 0;
	gcValues.dashes = 1;
#if defined(MAC_TCL) || defined(MAC_OSX_TK)
	gcValues.function = GXxor;
#else
	gcValues.function = GXinvert;
#endif
	mask = GCLineWidth | GCLineStyle | GCDashList | GCDashOffset | GCFunction;
	dotState->gc = Tk_GetGC(tree->tkwin, mask, &gcValues);

	/* Keep drawing inside the contentbox */
	dotState->rgn = TkCreateRegion();
	xrect.x = tree->inset;
	xrect.y = tree->inset + Tree_HeaderHeight(tree);
	xrect.width = Tk_Width(tree->tkwin) - tree->inset - xrect.x;
	xrect.height = Tk_Height(tree->tkwin) - tree->inset - xrect.y;
	TkUnionRectWithRegion(&xrect, dotState->rgn, dotState->rgn);
	TkSetRegion(tree->display, dotState->gc, dotState->rgn);
#endif
}

void DotRect_Draw(DotState *p, int x, int y, int width, int height)
{
	struct DotStatePriv *dotState = (struct DotStatePriv *) p;
#ifdef WIN32
#if 1
	RECT rect;

	rect.left = x;
	rect.right = x + width;
	rect.top = y;
	rect.bottom = y + height;
	DrawFocusRect(dotState->dc, &rect);
#else
	HDC dc = dotState->dc; 
	int i;
	int wx = x + dotState->tree->drawableXOrigin;
	int wy = y + dotState->tree->drawableYOrigin;
	int nw, ne, sw, se;

	/* Dots on even pixels only */
	nw = !(wx & 1) == !(wy & 1);
	ne = !((wx + width - 1) & 1) == !(wy & 1);
	sw = !(wx & 1) == !((wy + height - 1) & 1);
	se = !((wx + width - 1) & 1) == !((wy + height - 1) & 1);

	for (i = !nw; i < height; i += 2)
	{
		MoveToEx(dc, x, y + i, NULL);
		LineTo(dc, x + 1, y + i);
	}
	for (i = nw ? 2 : 1; i < width; i += 2)
	{
		MoveToEx(dc, x + i, y, NULL);
		LineTo(dc, x + i + 1, y);
	}
	for (i = ne ? 2 : 1; i < height; i += 2)
	{
		MoveToEx(dc, x + width - 1, y + i, NULL);
		LineTo(dc, x + width, y + i);
	}
	for (i = sw ? 2 : 1; i < width - se; i += 2)
	{
		MoveToEx(dc, x + i, y + height - 1, NULL);
		LineTo(dc, x + i + 1, y + height - 1);
	}
#endif
#else
	XDrawRectangle(dotState->tree->display, dotState->drawable, dotState->gc,
		x, y, width - 1, height - 1);
#endif
}

void DotRect_Restore(DotState *p)
{
	struct DotStatePriv *dotState = (struct DotStatePriv *) p;
#ifdef WIN32
	SelectClipRgn(dotState->dc, NULL);
	DeleteObject(dotState->rgn);
	TkWinReleaseDrawableDC(dotState->drawable, dotState->dc, &dotState->dcState);
#else
	XSetClipMask(dotState->tree->display, dotState->gc, None);
	Tk_FreeGC(dotState->tree->display, dotState->gc);
#endif
}

void Tk_FillRegion(Display *display, Drawable drawable, GC gc, TkRegion rgn)
{
#ifdef WIN32
	HDC dc;
	TkWinDCState dcState;
	HBRUSH brush;

	dc = TkWinGetDrawableDC(display, drawable, &dcState);
	SetROP2(dc, R2_COPYPEN);
	brush = CreateSolidBrush(gc->foreground);
	FillRgn(dc, (HRGN) rgn, brush);
	DeleteObject(brush);
	TkWinReleaseDrawableDC(drawable, dc, &dcState);
#else
	XRectangle box;

	TkClipBox(rgn, &box);
	TkSetRegion(display, gc, rgn);
	XFillRectangle(display, drawable, gc, box.x, box.y, box.width, box.height);
	XSetClipMask(display, gc, None);
#endif
}

/*
 * Replacement for Tk_TextLayout stuff. Allows the caller to break lines
 * on character boundaries (as well as word boundaries). Allows the caller
 * to specify the maximum number of lines to display. Will add ellipsis "..."
 * to the end of text that is too long to fit (when max lines specified).
 */

typedef struct LayoutChunk
{
	CONST char *start;	/* Pointer to simple string to be displayed.
						 * * This is a pointer into the TkTextLayout's
						 * * string. */
	int numBytes;	/* The number of bytes in this chunk. */
	int numChars;	/* The number of characters in this chunk. */
	int numDisplayChars;	/* The number of characters to display when
							 * * this chunk is displayed.  Can be less than
							 * * numChars if extra space characters were
							 * * absorbed by the end of the chunk.  This
							 * * will be < 0 if this is a chunk that is
							 * * holding a tab or newline. */
	int x, y;	/* The origin of the first character in this
				 * * chunk with respect to the upper-left hand
				 * * corner of the TextLayout. */
	int totalWidth;	/* Width in pixels of this chunk.  Used
					 * * when hit testing the invisible spaces at
					 * * the end of a chunk. */
	int displayWidth;	/* Width in pixels of the displayable
						 * * characters in this chunk.  Can be less than
						 * * width if extra space characters were
						 * * absorbed by the end of the chunk. */
	int ellipsis; /* TRUE if adding "..." */
} LayoutChunk;

typedef struct LayoutInfo
{
	Tk_Font tkfont;	/* The font used when laying out the text. */
	CONST char *string;	/* The string that was layed out. */
	int numLines;	/* Number of lines */
	int width;	/* The maximum width of all lines in the
				 * * text layout. */
	int height;
	int numChunks;	/* Number of chunks actually used in
					 * * following array. */
	LayoutChunk chunks[1];	/* Array of chunks.  The actual size will
							 * * be maxChunks.  THIS FIELD MUST BE THE LAST
							 * * IN THE STRUCTURE. */
} LayoutInfo;

static LayoutChunk *NewChunk(LayoutInfo **layoutPtrPtr, int *maxPtr,
	CONST char *start, int numBytes, int curX, int newX, int y)
{
	LayoutInfo *layoutPtr;
	LayoutChunk *chunkPtr;
	int maxChunks, numChars;
	size_t s;

	layoutPtr = *layoutPtrPtr;
	maxChunks = *maxPtr;
	if (layoutPtr->numChunks == maxChunks)
	{
		maxChunks *= 2;
		s = sizeof(LayoutInfo) + ((maxChunks - 1) * sizeof(LayoutChunk));
		layoutPtr = (LayoutInfo *) ckrealloc((char *) layoutPtr, s);

		*layoutPtrPtr = layoutPtr;
		*maxPtr = maxChunks;
	}
	numChars = Tcl_NumUtfChars(start, numBytes);
	chunkPtr = &layoutPtr->chunks[layoutPtr->numChunks];
	chunkPtr->start = start;
	chunkPtr->numBytes = numBytes;
	chunkPtr->numChars = numChars;
	chunkPtr->numDisplayChars = numChars;
	chunkPtr->x = curX;
	chunkPtr->y = y;
	chunkPtr->totalWidth = newX - curX;
	chunkPtr->displayWidth = newX - curX;
	chunkPtr->ellipsis = FALSE;
	layoutPtr->numChunks++;

	return chunkPtr;
}

TextLayout TextLayout_Compute(
	Tk_Font tkfont,	/* Font that will be used to display text. */
	CONST char *string,	/* String whose dimensions are to be
						 ** computed. */
	int numChars,	/* Number of characters to consider from
					 ** string, or < 0 for strlen(). */
	int wrapLength,	/* Longest permissible line length, in
					 ** pixels.  <= 0 means no automatic wrapping:
					 ** just let lines get as long as needed. */
	Tk_Justify justify,	/* How to justify lines. */
	int maxLines,
	int flags	/* Flag bits OR-ed together.
				 ** TK_IGNORE_TABS means that tab characters
				 ** should not be expanded.  TK_IGNORE_NEWLINES
				 ** means that newline characters should not
				 ** cause a line break. */
	)
{
	CONST char *start, *end, *special;
	int n, y, bytesThisChunk, maxChunks;
	int baseline, height, curX, newX, maxWidth;
	LayoutInfo *layoutPtr;
	LayoutChunk *chunkPtr;
	Tk_FontMetrics fm;
	Tcl_DString lineBuffer;
	int *lineLengths;
	int curLine;
	int tabWidth = 20; /* FIXME */

	Tcl_DStringInit(&lineBuffer);

	Tk_GetFontMetrics(tkfont, &fm);
	height = fm.ascent + fm.descent;

	if (numChars < 0)
		numChars = Tcl_NumUtfChars(string, -1);
	if (wrapLength == 0)
		wrapLength = -1;

	maxChunks = 1;

	layoutPtr = (LayoutInfo *) ckalloc(sizeof(LayoutInfo) + (maxChunks -
			1) * sizeof(LayoutChunk));
	layoutPtr->tkfont = tkfont;
	layoutPtr->string = string;
	layoutPtr->numChunks = 0;
	layoutPtr->numLines = 0;

	baseline = fm.ascent;
	maxWidth = 0;

	curX = 0;

	end = Tcl_UtfAtIndex(string, numChars);
	special = string;

	flags &= TK_WHOLE_WORDS | TK_IGNORE_TABS | TK_IGNORE_NEWLINES;
	flags |= TK_AT_LEAST_ONE;
	for (start = string; start < end;)
	{
		if (start >= special)
		{
			for (special = start; special < end; special++)
			{
				if (!(flags & TK_IGNORE_NEWLINES))
				{
					if ((*special == '\n') || (*special == '\r'))
						break;
				}
				if (!(flags & TK_IGNORE_TABS))
				{
					if (*special == '\t')
						break;
				}
			}
		}

		chunkPtr = NULL;
		if (start < special)
		{
			bytesThisChunk = Tk_MeasureChars(tkfont, start, special - start,
				wrapLength - curX, flags, &newX);
			newX += curX;
			flags &= ~TK_AT_LEAST_ONE;
			if (bytesThisChunk > 0)
			{
				chunkPtr = NewChunk(&layoutPtr, &maxChunks, start,
					bytesThisChunk, curX, newX, baseline);
				start += bytesThisChunk;
				curX = newX;
			}
		}

		if ((start == special) && (special < end))
		{
			chunkPtr = NULL;
			if (*special == '\t')
			{
				newX = curX + tabWidth;
				newX -= newX % tabWidth;
				NewChunk(&layoutPtr, &maxChunks, start, 1, curX, newX,
					baseline)->numDisplayChars = -1;
				start++;
				if ((start < end) && ((wrapLength <= 0) ||
					(newX <= wrapLength)))
				{
					curX = newX;
					flags &= ~TK_AT_LEAST_ONE;
					continue;
				}
			}
			else
			{
				NewChunk(&layoutPtr, &maxChunks, start, 1, curX, curX,
					baseline)->numDisplayChars = -1;
				start++;
				goto wrapLine;
			}
		}

		while ((start < end) && isspace(UCHAR(*start)))
		{
			if (!(flags & TK_IGNORE_NEWLINES))
			{
				if ((*start == '\n') || (*start == '\r'))
					break;
			}
			if (!(flags & TK_IGNORE_TABS))
			{
				if (*start == '\t')
					break;
			}
			start++;
		}
		if (chunkPtr != NULL)
		{
			CONST char *end;

			end = chunkPtr->start + chunkPtr->numBytes;
			bytesThisChunk = start - end;
			if (bytesThisChunk > 0)
			{
				bytesThisChunk =
					Tk_MeasureChars(tkfont, end, bytesThisChunk, -1, 0,
					&chunkPtr->totalWidth);
				chunkPtr->numBytes += bytesThisChunk;
				chunkPtr->numChars += Tcl_NumUtfChars(end, bytesThisChunk);
				chunkPtr->totalWidth += curX;
			}
		}

wrapLine:
		flags |= TK_AT_LEAST_ONE;

		if (curX > maxWidth)
			maxWidth = curX;

		Tcl_DStringAppend(&lineBuffer, (char *) &curX, sizeof(curX));

		curX = 0;
		baseline += height;
		layoutPtr->numLines++;

		if ((maxLines > 0) && (layoutPtr->numLines >= maxLines))
			break;
	}

	if (start >= end)
	if ((layoutPtr->numChunks > 0) && !(flags & TK_IGNORE_NEWLINES))
	{
		if (layoutPtr->chunks[layoutPtr->numChunks - 1].start[0] == '\n')
		{
			chunkPtr =
				NewChunk(&layoutPtr, &maxChunks, start, 0, curX, curX,
				baseline);
			chunkPtr->numDisplayChars = -1;
			Tcl_DStringAppend(&lineBuffer, (char *) &curX, sizeof(curX));
			baseline += height;
		}
	}

#if 1
	/* Fiddle with chunks on the last line to add ellipsis if there is some
	 * text remaining */
	if ((start < end) && (layoutPtr->numChunks > 0))
	{
		chunkPtr = &layoutPtr->chunks[layoutPtr->numChunks - 1];
		if (wrapLength > 0)
		{
			y = chunkPtr->y;
			for (n = layoutPtr->numChunks - 1; n >= 0; n--)
			{
				chunkPtr = &layoutPtr->chunks[n];

				/* Only consider the last line */
				if (chunkPtr->y != y)
					break;

				if (chunkPtr->start[0] == '\n')
					continue;

				newX = chunkPtr->totalWidth - 1;
				bytesThisChunk = Ellipsis(tkfont, (char *) chunkPtr->start,
					chunkPtr->numBytes, &newX, "...");
				if (bytesThisChunk > 0)
				{
					chunkPtr->numBytes = bytesThisChunk;
					chunkPtr->numChars = Tcl_NumUtfChars(chunkPtr->start, bytesThisChunk);
					chunkPtr->numDisplayChars = chunkPtr->numChars;
					chunkPtr->ellipsis = TRUE;
					chunkPtr->displayWidth = newX;
					chunkPtr->totalWidth = newX;
					lineLengths = (int *) Tcl_DStringValue(&lineBuffer);
					lineLengths[layoutPtr->numLines - 1] = chunkPtr->x + newX;
					if (chunkPtr->x + newX > maxWidth)
						maxWidth = chunkPtr->x + newX;
					break;
				}
			}
		}
		else
		{
			char staticStr[256], *buf = staticStr;
			char *ellipsis = "...";
			int ellipsisLen = strlen(ellipsis);

			if (chunkPtr->start[0] == '\n')
			{
				if (layoutPtr->numChunks == 1)
					goto finish;
				if (layoutPtr->chunks[layoutPtr->numChunks - 2].y != chunkPtr->y)
					goto finish;
				chunkPtr = &layoutPtr->chunks[layoutPtr->numChunks - 2];
			}

			if (chunkPtr->numBytes + ellipsisLen > sizeof(staticStr))
				buf = ckalloc(chunkPtr->numBytes + ellipsisLen);
			memcpy(buf, chunkPtr->start, chunkPtr->numBytes);
			memcpy(buf + chunkPtr->numBytes, ellipsis, ellipsisLen);
			Tk_MeasureChars(tkfont, buf,
				chunkPtr->numBytes + ellipsisLen, -1, 0,
				&chunkPtr->displayWidth);
			chunkPtr->totalWidth = chunkPtr->displayWidth;
			chunkPtr->ellipsis = TRUE;
			lineLengths = (int *) Tcl_DStringValue(&lineBuffer);
			lineLengths[layoutPtr->numLines - 1] = chunkPtr->x + chunkPtr->displayWidth;
			if (chunkPtr->x + chunkPtr->displayWidth > maxWidth)
				maxWidth = chunkPtr->x + chunkPtr->displayWidth;
			if (buf != staticStr)
				ckfree(buf);
		}
	}
finish:
#endif

	layoutPtr->width = maxWidth;
	layoutPtr->height = baseline - fm.ascent;
	if (layoutPtr->numChunks == 0)
	{
		layoutPtr->height = height;

		layoutPtr->numChunks = 1;
		layoutPtr->chunks[0].start = string;
		layoutPtr->chunks[0].numBytes = 0;
		layoutPtr->chunks[0].numChars = 0;
		layoutPtr->chunks[0].numDisplayChars = -1;
		layoutPtr->chunks[0].x = 0;
		layoutPtr->chunks[0].y = fm.ascent;
		layoutPtr->chunks[0].totalWidth = 0;
		layoutPtr->chunks[0].displayWidth = 0;
	}
	else
	{
		curLine = 0;
		chunkPtr = layoutPtr->chunks;
		y = chunkPtr->y;
		lineLengths = (int *) Tcl_DStringValue(&lineBuffer);
		for (n = 0; n < layoutPtr->numChunks; n++)
		{
			int extra;

			if (chunkPtr->y != y)
			{
				curLine++;
				y = chunkPtr->y;
			}
			extra = maxWidth - lineLengths[curLine];
			if (justify == TK_JUSTIFY_CENTER)
			{
				chunkPtr->x += extra / 2;
			}
			else if (justify == TK_JUSTIFY_RIGHT)
			{
				chunkPtr->x += extra;
			}
			chunkPtr++;
		}
	}

	Tcl_DStringFree(&lineBuffer);

	if (layoutPtr->numLines == 1)
		dbwin("WARNING: single-line TextLayout created\n");

	return (TextLayout) layoutPtr;
}

void TextLayout_Free(TextLayout textLayout)
{
	LayoutInfo *layoutPtr = (LayoutInfo *) textLayout;

	ckfree((char *) layoutPtr);
}

void TextLayout_Size(TextLayout textLayout, int *widthPtr, int *heightPtr)
{
	LayoutInfo *layoutPtr = (LayoutInfo *) textLayout;

	if (widthPtr != NULL)
		(*widthPtr) = layoutPtr->width;
	if (heightPtr != NULL)
		(*heightPtr) = layoutPtr->height;
}

void TextLayout_Draw(
	Display *display,	/* Display on which to draw. */
	Drawable drawable,	/* Window or pixmap in which to draw. */
	GC gc,	/* Graphics context to use for drawing text. */
	TextLayout layout,	/* Layout information, from a previous call
							 * * to Tk_ComputeTextLayout(). */
	int x, int y,	/* Upper-left hand corner of rectangle in
				 * * which to draw (pixels). */
	int firstChar,	/* The index of the first character to draw
					 * * from the given text item.  0 specfies the
					 * * beginning. */
	int lastChar	/* The index just after the last character
					 * * to draw from the given text item.  A number
					 * * < 0 means to draw all characters. */
)
{
	LayoutInfo *layoutPtr = (LayoutInfo *) layout;
	int i, numDisplayChars, drawX;
	CONST char *firstByte;
	CONST char *lastByte;
	LayoutChunk *chunkPtr;

	if (lastChar < 0)
		lastChar = 100000000;
	chunkPtr = layoutPtr->chunks;
	for (i = 0; i < layoutPtr->numChunks; i++)
	{
		numDisplayChars = chunkPtr->numDisplayChars;
		if ((numDisplayChars > 0) && (firstChar < numDisplayChars))
		{
			if (firstChar <= 0)
			{
				drawX = 0;
				firstChar = 0;
				firstByte = chunkPtr->start;
			}
			else
			{
				firstByte = Tcl_UtfAtIndex(chunkPtr->start, firstChar);
				Tk_MeasureChars(layoutPtr->tkfont, chunkPtr->start,
					firstByte - chunkPtr->start, -1, 0, &drawX);
			}
			if (lastChar < numDisplayChars)
				numDisplayChars = lastChar;
			lastByte = Tcl_UtfAtIndex(chunkPtr->start, numDisplayChars);
#if 1
			if (chunkPtr->ellipsis)
			{
				char staticStr[256], *buf = staticStr;
				char *ellipsis = "...";
				int ellipsisLen = strlen(ellipsis);

				if ((lastByte - firstByte) + ellipsisLen > sizeof(staticStr))
					buf = ckalloc((lastByte - firstByte) + ellipsisLen);
				memcpy(buf, firstByte, (lastByte - firstByte));
				memcpy(buf + (lastByte - firstByte), ellipsis, ellipsisLen);
				Tk_DrawChars(display, drawable, gc, layoutPtr->tkfont,
					buf, (lastByte - firstByte) + ellipsisLen,
					x + chunkPtr->x + drawX, y + chunkPtr->y);
				if (buf != staticStr)
					ckfree(buf);
			}
			else
#endif
			Tk_DrawChars(display, drawable, gc, layoutPtr->tkfont,
				firstByte, lastByte - firstByte, x + chunkPtr->x + drawX,
				y + chunkPtr->y);
		}
		firstChar -= chunkPtr->numChars;
		lastChar -= chunkPtr->numChars;
		if (lastChar <= 0)
			break;
		chunkPtr++;
	}
}

/*
 *----------------------------------------------------------------------
 *
 * TreeCtrl_GetPadAmountFromObj --
 *
 *	Parse a pad amount configuration options.
 *	A pad amount (typically the value of an option -XXXpadx or
 *	-XXXpady, where XXX may be a possibly empty string) can
 *	be either a single pixel width, or a list of two pixel widths.
 *	If a single pixel width, the amount specified is used for 
 *	padding on both sides.  If two amounts are specified, then
 *	they specify the left/right or top/bottom padding.
 *
 * Results:
 *	Standard Tcl Result.
 *
 * Side effects:
 *	Sets internal representation of the object. In case of an error
 *	the result of the interpreter is modified.
 *
 *----------------------------------------------------------------------
 */

int
TreeCtrl_GetPadAmountFromObj(interp, tkwin, padObj, topLeftPtr, bottomRightPtr)
    Tcl_Interp *interp;		/* Interpreter for error reporting, or NULL,
				 * if no error message is wanted. */
    Tk_Window tkwin;		/* A window.  Needed by Tk_GetPixels() */
    Tcl_Obj *padObj;		/* Object containing a pad amount. */
    int *topLeftPtr;		/* Pointer to the location, where to store the
				   first component of the padding. */
    int *bottomRightPtr;	/* Pointer to the location, where to store the
				   second component of the padding. */
{
    int padc;			/* Number of element objects in padv. */
    Tcl_Obj **padv;		/* Pointer to the element objects of the
				 * parsed pad amount value. */
	int topLeft, bottomRight;

    if (Tcl_ListObjGetElements(interp, padObj, &padc, &padv) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * The value specifies a non empty string.
     * Check that this string is indeed a valid pad amount.
     */

    if (padc < 1 || padc > 2) {
	if (interp != NULL) {
	error:
	    Tcl_ResetResult(interp);
	    Tcl_AppendResult(interp, "bad pad amount \"",
		Tcl_GetString(padObj), "\": must be a list of ",
		"1 or 2 positive screen distances", (char *) NULL);
	}
	return TCL_ERROR;
    }
    if ((Tk_GetPixelsFromObj(interp, tkwin, padv[0], &topLeft)
	     != TCL_OK) || (topLeft < 0)) {
	goto error;
    }
    if (padc == 2) {
	if ((Tk_GetPixelsFromObj(interp, tkwin, padv[1], &bottomRight)
		!= TCL_OK) || (bottomRight < 0)) {
	    goto error;
	}
    } else {
	bottomRight = topLeft;
    }
    (*topLeftPtr) = topLeft;
    (*bottomRightPtr) = bottomRight;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeCtrl_NewPadAmountObj --
 *
 *	Create a Tcl object with an internal representation, that
 *	corresponds to a pad amount, i.e. an integer Tcl_Obj or a
 *	list Tcl_Obj with 2 elements.
 *
 * Results:
 *	The created object.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
TreeCtrl_NewPadAmountObj(padAmounts)
    int *padAmounts;		/* Internal form of a pad amount. */
{
    Tcl_Obj *newObj;

    /*
     * If both values are the same, create a list with one value,
     * otherwise create a two element list with the top/left value
     * first followed by the bottom/right value.
     */

    if (padAmounts[PAD_TOP_LEFT] == padAmounts[PAD_BOTTOM_RIGHT]) {
	newObj = Tcl_NewIntObj(padAmounts[PAD_TOP_LEFT]);
    } else {
	newObj = Tcl_NewObj();
	Tcl_ListObjAppendElement((Tcl_Interp *) NULL, newObj,
	    Tcl_NewIntObj(padAmounts[PAD_TOP_LEFT]));
	Tcl_ListObjAppendElement((Tcl_Interp *) NULL, newObj,
	    Tcl_NewIntObj(padAmounts[PAD_BOTTOM_RIGHT]));
    }
    return newObj;
}

/*
 *----------------------------------------------------------------------
 *
 * PadAmountOptionSet --
 * PadAmountOptionGet --
 * PadAmountOptionRestore --
 * PadAmountOptionFree --
 *
 *	Handlers for object-based pad amount configuration options.
 *	A pad amount (typically the value of an option -XXXpadx or
 *	-XXXpady, where XXX may be a possibly empty string) can
 *	be either a single pixel width, or a list of two pixel widths.
 *	If a single pixel width, the amount specified is used for 
 *	padding on both sides.  If two amounts are specified, then
 *	they specify the left/right or top/bottom padding.
 *
 * Results:
 *	See user documentation for expected results from these functions.
 *		PadAmountOptionSet	Standard Tcl Result.
 *		PadAmountOptionGet	Tcl_Obj * containing a valid internal
 *					representation of the pad amount.
 *		PadAmountOptionRestore	None.
 *		PadAmountOptionFree	None.
 *
 * Side effects:
 *	Depends on the function.
 *		PadAmountOptionSet	Sets option value to new setting,
 *					allocating a new integer array.
 *		PadAmountOptionGet	Creates a new Tcl_Obj.
 *		PadAmountOptionRestore	Resets option value to original value.
 *		PadAmountOptionFree	Free storage for internal rep.
 *
 *----------------------------------------------------------------------
 */

static int
PadAmountOptionSet(clientData, interp, tkwin, valuePtr, recordPtr,
		   internalOffset, saveInternalPtr, flags)
    ClientData clientData;	/* unused. */
    Tcl_Interp *interp;		/* Interpreter for error reporting, or NULL,
				 * if no error message is wanted. */
    Tk_Window tkwin;		/* A window.  Needed by Tk_GetPixels() */
    Tcl_Obj **valuePtr;		/* The argument to "-padx", "-pady", "-ipadx",
				 * or "-ipady".  The thing to be parsed. */
    char *recordPtr;		/* Pointer to start of widget record. */
    int internalOffset;		/* Offset of internal representation or
				 * -1, if no internal repr is wanted. */
    char *saveInternalPtr;	/* Pointer to the place, where the saved
				 * internal form (of type "int *") resides. */
    int flags;			/* Flags as specified in Tk_OptionSpec. */
{
    int topLeft, bottomRight;	/* The two components of the padding. */
    int *new;			/* Pointer to the allocated array of integers
				 * containing the parsed pad amounts. */
    int **internalPtr;		/* Pointer to the place, where the internal
				 * form (of type "int *") resides. */

    /*
     * Check that the given object indeed specifies a valid pad amount.
     */

    if (TreeCtrl_GetPadAmountFromObj(interp, tkwin, *valuePtr,
	    &topLeft, &bottomRight) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * Store a pointer to an allocated array of the two padding values
     * into the widget record at the specified offset.
     */

    if (internalOffset >= 0) {
	internalPtr = (int **) (recordPtr + internalOffset);
	*(int **) saveInternalPtr = *internalPtr;
	new = (int *) ckalloc(2 * sizeof(int));
	new[PAD_TOP_LEFT]     = topLeft;
	new[PAD_BOTTOM_RIGHT] = bottomRight;
	*internalPtr = new;
    }
    return TCL_OK;
}

static Tcl_Obj *
PadAmountOptionGet(clientData, tkwin, recordPtr, internalOffset)
    ClientData clientData;	/* unused. */
    Tk_Window tkwin;		/* A window; unused. */
    char *recordPtr;		/* Pointer to start of widget record. */
    int internalOffset;		/* Offset of internal representation. */
{
    int *padAmounts = *(int **)(recordPtr + internalOffset);

    return TreeCtrl_NewPadAmountObj(padAmounts);
}

static void
PadAmountOptionRestore(clientData, tkwin, internalPtr, saveInternalPtr)
    ClientData clientData;	/* unused. */
    Tk_Window tkwin;		/* A window; unused. */
    char *internalPtr;		/* Pointer to the place, where the internal
				 * form (of type "int *") resides. */
    char *saveInternalPtr;	/* Pointer to the place, where the saved
				 * internal form (of type "int *") resides. */
{
    *(int **) internalPtr = *(int **) saveInternalPtr;
}

static void
PadAmountOptionFree(clientData, tkwin, internalPtr)
    ClientData clientData;	/* unused. */
    Tk_Window tkwin;		/* A window; unused */
    char *internalPtr;		/* Pointer to the place, where the internal
				 * form (of type "int *") resides. */
{
    if (*(int **)internalPtr != NULL) {
	ckfree((char *) *(int **)internalPtr);
    }
}
