/*
	File:		MBCBoardViewDraw.h
	Contains:	Drawing the OpenGL chess board view
	Version:	1.0
	Copyright:	© 2002 by Apple Computer, Inc., all rights reserved.
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
