/*
	File:		MBCBoardViewDraw.h
	Contains:	Drawing the OpenGL chess board view
	Version:	1.0
	Copyright:	© 2002 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				Matthias Neeracher    x43683

	Writers:

		(MN)	Matthias Neeracher

	Change History (most recent first):

		$Log: MBCBoardViewDraw.h,v $
		Revision 1.6  2004/07/10 04:53:29  neerache
		Tweak visuals
		
		Revision 1.5  2003/06/02 04:21:40  neerache
		Start implementing drawing styles for board elements
		
		Revision 1.4  2003/05/02 01:16:33  neerache
		Simplify drawing methods
		
		Revision 1.3  2003/04/28 22:13:25  neerache
		Eliminate drawBoardPlane
		
		Revision 1.2  2002/12/04 02:30:50  neeri
		Experiment (unsuccessfully so far) with ways to speed up piece movement
		
		Revision 1.1  2002/08/22 23:47:06  neeri
		Initial Checkin
		
*/

#import "MBCBoardView.h"

@interface MBCDrawStyle : NSObject
{
@public
	float   fDiffuse;	// Diffuse color component
	float	fSpecular;	// Specular color component
	float	fShininess;	// Shininess of element
	float	fAlpha;		// Opacity of element
@private
	GLuint	fTexture;
}

- (id) init;
- (id) initWithTexture:(GLuint)tex;
- (void) unloadTexture;
- (void) startStyle:(float)alpha;

@end

@interface MBCBoardView ( Draw )

- (void) drawPosition;				// Draw the whole board and pieces

@end

// Local Variables:
// mode:ObjC
// End:
