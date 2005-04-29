// originally thanks to Llew S. Goodstadt
// from http://www.codeguru.com/gdi/ellipse.shtml

#include "common/Geometry.h"

// Create points to simulate ellipse using beziers
// senses coordinate translation, generates CW (swap top and bottom for CCW)
void ellipse2bezier(Rect &r, Line &out) {
	out.degree = 3;
	out.resize(13,Coord());
    // MAGICAL CONSTANT to map ellipse to beziers
    //  			2/3*(sqrt(2)-1) 
    const double EToBConst = 0.2761423749154,
		translation = (r.b>r.t)?-1.0:1.0;

    Coord offset(r.Width() * EToBConst, r.Height() * EToBConst);

    Coord center(r.Center());

    out[0].x  =                            //------------------------/
    out[1].x  =                            //                        /
    out[11].x =                            //        2___3___4       /
    out[12].x = r.l;	                   //     1             5    /
    out[5].x  =                            //     |             |    /
    out[6].x  =                            //     |             |    /
    out[7].x  = r.r;			   //     0,12          6    /
    out[2].x  =                            //     |             |    /
    out[10].x = center.x - offset.x;	   //     |             |    /
    out[4].x  =                            //    11             7    /
    out[8].x  = center.x + offset.x;	   //       10___9___8       /
    out[3].x  =                            //                        /
    out[9].x  = center.x;                  //------------------------*

    out[2].y  =
    out[3].y  =
    out[4].y  = r.t;
    out[8].y  =
    out[9].y  =
    out[10].y = r.b;
    out[7].y  =
    out[11].y = center.y - translation*offset.y;
    out[1].y  =
    out[5].y  = center.y + translation*offset.y;
    out[0].y  =
    out[12].y =
    out[6].y  = center.y;
}
