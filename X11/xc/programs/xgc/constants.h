/*
** xgc
**
** constants.h
**
** Lots of constants which many files need.
*/

/* Find the max of two numbers */
#ifndef max
#define max(x,y) (((x)>(y))?(x):(y))
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#define MAXCHOICES 16           /* Max # of choices for any option */

#define Black BlackPixel(X.dpy,0) 
#define White WhitePixel(X.dpy,0)

#define CopyArea      0		/* different tests */
#define CopyPlane     1
#define PolyPoint     2
#define PolyLine      3
#define PolySegment   4
#define PolyRectangle 5
#define PolyArc       6
#define FillPolygon   7
#define PolyFillRect  8
#define PolyFillArc   9
#define PutImage     10
#define GetImage     11
#define PolyText8    12
#define ImageText8   13
#define PolyText16   14
#define ImageText16  15

#define CFunction     0		/* different GC things you can choose */
#define CLinestyle    1
#define CCapstyle     2
#define CJoinstyle    3
#define CFillstyle    4
#define CFillrule     5
#define CArcmode      6
#define NUMCHOICES    7
#define CTest         7

#define TLineWidth     0	/* different editable text widgets */
#define TFont          1
#define TForeground    2
#define TBackground    3
#define NUMTEXTWIDGETS 4

#define StartTimer   0		/* flags for timing tests */
#define EndTimer     1
#define start_timer()   timer(StartTimer)
#define end_timer()     timer(EndTimer)

/* the number of toggle widgets in various groups */
#define NUM_TESTS       16
#define NUM_FUNCTIONS   16
#define NUM_LINESTYLES   3
#define NUM_CAPSTYLES    4
#define NUM_JOINSTYLES   3
#define NUM_FILLSTYLES   4
#define NUM_FILLRULES    2
#define NUM_ARCMODES     2

/* The number of bits in the dash description */
#define DASHLENGTH       8
