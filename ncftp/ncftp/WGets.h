/* WGets.h */

#ifndef _wgets_h_
#define _wgets_h_ 1

#ifdef USE_CURSES

#define wg_TraceFileName "trace"

#define wg_EOF (-1)
#define wg_BadParamBlock (-2)
#define wg_DstSizeTooSmall (-3)
#define wg_WindowTooSmall (-4)
#define wg_BadCursesWINDOW (-5)
#define wg_BadCoordinates (-6)
#define wg_BadBufferPointer (-7)

#define wg_RegularEcho 0
#define wg_BulletEcho 1
#define wg_NoEcho 2

#define wg_Bullet '.'

#define wg_NoHistory	((LineListPtr) 0)

typedef struct WGetsParams {
	WINDOW *w;				/* in */
	int sy, sx;				/* in */
	char *dst;				/* in, out */
	int fieldLen;			/* in */
	size_t dstSize;			/* in */
	int useCurrentContents;	/* in */
	int echoMode;			/* in */
	int changed;			/* out */
	int dstLen;				/* out */
	LineListPtr history;	/* in, out */
} WGetsParams, *WGetsParamPtr;

int wg_Gets(WGetsParamPtr wgpp);

#endif

#endif	/* _wgets_h_ */
