/*
 * header file for transformed coordinate system.  No rotations
 * supported, as elipses cannot be rotated in X.
 */

typedef struct _transform {
	double	mx, bx;
	double	my, by;
} Transform;

typedef struct _TPoint {
	double	x, y;
} TPoint;

typedef struct _TRectangle {
	double	x, y, width, height;
} TRectangle;

# define Xx(x,y,t)	((int)((t)->mx * (x) + (t)->bx + 0.5))
# define Xy(x,y,t)	((int)((t)->my * (y) + (t)->by + 0.5))
# define Xwidth(w,h,t)	((int)((t)->mx * (w) + 0.5))
# define Xheight(w,h,t)	((int)((t)->my * (h) + 0.5))
# define Tx(x,y,t)	((((double) (x)) - (t)->bx) / (t)->mx)
# define Ty(x,y,t)	((((double) (y)) - (t)->by) / (t)->my)
# define Twidth(w,h,t)	(((double) (w)) / (t)->mx)
# define Theight(w,h,t)	(((double) (h)) / (t)->my)

extern void Trectangle (const Transform *t, const TRectangle *in, TRectangle *out);
extern void SetTransform (Transform *t,
                          int xx1, int xx2, int xy1, int xy2,
                          double tx1, double tx2, double ty1, double ty2);
