/* polyinfo.h
 * This is the description of one polyhedron file
 */
/* $XFree86: xc/programs/ico/polyinfo.h,v 1.3 2000/02/17 14:00:32 dawes Exp $ */

#define MAXVERTS 120
	/* great rhombicosidodecahedron has 120 vertices */
#define MAXNV MAXVERTS
#define MAXFACES 30
	/* (hexakis icosahedron has 120 faces) */
#define MAXEDGES 180
	/* great rhombicosidodecahedron has 180 edges */
#define MAXEDGESPERPOLY 20

typedef struct {
	double x, y, z;
} Point3D;

/* structure of the include files which define the polyhedra */
typedef struct {
	const char *longname;	/* long name of object */
	const char *shortname;	/* short name of object */
	const char *dual;	/* long name of dual */
	int numverts;		/* number of vertices */
	int numedges;		/* number of edges */
	int numfaces;		/* number of faces */
	Point3D v[MAXVERTS];	/* the vertices */
	int f[MAXEDGES*2+MAXFACES];	/* the faces */
} Polyinfo;

/* end */
