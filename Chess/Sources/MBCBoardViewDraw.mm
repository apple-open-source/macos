/*
	File:		MBCBoardViewDraw.mm
	Contains:	Draw chess board
	Copyright:	© 2002-2003 Apple Computer, Inc. All rights reserved.
	
	Derived from glChess, Copyright © 2002 Robert Ancell and Michael Duelli
	Permission granted to Apple to relicense under the following terms:

	IMPORTANT: This Apple software is supplied to you by Apple Computer,
	Inc.  ("Apple") in consideration of your agreement to the following
	terms, and your use, installation, modification or redistribution of
	this Apple software constitutes acceptance of these terms.  If you do
	not agree with these terms, please do not use, install, modify or
	redistribute this Apple software.
	
	In consideration of your agreement to abide by the following terms,
	and subject to these terms, Apple grants you a personal, non-exclusive
	license, under Apple's copyrights in this original Apple software (the
	"Apple Software"), to use, reproduce, modify and redistribute the
	Apple Software, with or without modifications, in source and/or binary
	forms; provided that if you redistribute the Apple Software in its
	entirety and without modifications, you must retain this notice and
	the following text and disclaimers in all such redistributions of the
	Apple Software.  Neither the name, trademarks, service marks or logos
	of Apple Computer, Inc. may be used to endorse or promote products
	derived from the Apple Software without specific prior written
	permission from Apple.  Except as expressly stated in this notice, no
	other rights or licenses, express or implied, are granted by Apple
	herein, including but not limited to any patent rights that may be
	infringed by your derivative works or by other works in which the
	Apple Software may be incorporated.
	
	The Apple Software is provided by Apple on an "AS IS" basis.  APPLE
	MAKES NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION
	THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND
	FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS
	USE AND OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS.
	
	IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT,
	INCIDENTAL OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
	PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
	PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE,
	REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE,
	HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING
	NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN
	ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#import "MBCBoardViewDraw.h"

#import <math.h>
#import <OpenGL/glu.h>
#import <algorithm>
#import <sys/time.h>

using std::min;

@implementation MBCDrawStyle

- (id) init
{
	fTexture	= 0;

	return self;
}

- (id) initWithTexture:(GLuint)tex
{
	fTexture	= tex;
	fSpecular	= 0.2f;
	fShininess	= 5.0f;
	fAlpha		= 1.0f;

	return self;
}

- (void) unloadTexture
{
	if (fTexture)
		glDeleteTextures(1, &fTexture);
}

- (void) startStyle:(float)alpha
{
	GLfloat white_texture_color[4] 	= 
		{1.0f, 1.0f, 1.0f, fAlpha*alpha};
	GLfloat emission_color[4] 		= 
		{0.0f, 0.0f, 0.0f, fAlpha*alpha};
	GLfloat specular_color[4] 		= 
		{fSpecular, fSpecular, fSpecular, fAlpha*alpha};

	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, white_texture_color);
	glMaterialfv(GL_FRONT, GL_EMISSION, emission_color);
	glMaterialfv(GL_FRONT, GL_SPECULAR, specular_color);
	glMaterialf(GL_FRONT, GL_SHININESS, fShininess);
	glBindTexture(GL_TEXTURE_2D, fTexture);
}

@end

@implementation MBCBoardView ( Draw )

- (void) setupPerspective
{
	fIsFloating			= ![[self window] styleMask];
	if (!fIsFloating) {
		//
		// Regular window, draw background
		//
		const float kBrightness = 0.7f;
		glClearColor(kBrightness, kBrightness, kBrightness, 1.0);
	} else {
		//
		// Floating window, transparent background
		//
		long opaque = NO;
		[[self openGLContext] setValues:&opaque 
							  forParameter:NSOpenGLCPSurfaceOpacity];
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	}

	/* Stuff you can't do without */
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glEnable(GL_NORMALIZE);

	/* Textures */
	glEnable(GL_TEXTURE_2D);

	/* The lighting */
	glEnable(GL_LIGHTING);
		
	glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, 1);
	glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL, GL_SEPARATE_SPECULAR_COLOR);
	glShadeModel(GL_SMOOTH);

	glDisable(GL_FOG);

	const float kDistance 		= 200.0f;
	const float kBoardSize 		= fVariant==kVarCrazyhouse ? 60.0f : 55.0f;
	const float kDeg2Rad  		= M_PI / 180.0f;
	const float kRad2Deg		= 180.0f / M_PI;
	const float kAngleOfView	= 2.0f * atan2(kBoardSize, kDistance) * kRad2Deg;

	NSRect bounds = [self bounds];
	glViewport(0, 0, (long)bounds.size.width, (long)bounds.size.height);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

    gluPerspective(kAngleOfView, bounds.size.width / bounds.size.height, 
				   100.0, 250.0); 

	glMatrixMode(GL_MODELVIEW);

	float	cameraY = kDistance * sin(fElevation * kDeg2Rad);
	float   cameraXZ= kDistance * cos(fElevation * kDeg2Rad);
	float	cameraX = cameraXZ  * sin(fAzimuth * kDeg2Rad);
	float	cameraZ = cameraXZ  *-cos(fAzimuth * kDeg2Rad);

	gluLookAt(cameraX, cameraY, cameraZ,
			  0.0, 0.0, 0.0, 0.0, 1.0, 0.0);

#if 0
	//
	// Work around an apparent bug in some graphics driver versions
	// where the stencil buffer matters although the stencil test
	// is disabled.
	//
	if (!fBoardReflectivity)
		glClear(GL_STENCIL_BUFFER_BIT);
#endif
	
	fNeedPerspective	= false;
}

- (void) drawBoard:(BOOL)overReflection
{
	int x, y, color;

	//
	// We want variation in the squares, not psychedelic effects
	//
	srandom(1);

	glPushAttrib(GL_ENABLE_BIT | GL_TEXTURE_BIT | GL_COLOR_BUFFER_BIT);

	//
	// Blend edges of squares
	//
	if (overReflection)
		glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_LINE_SMOOTH);
	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
	// glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, !overReflection);

	glNormal3f(0.0f, 1.0f, 0.0f);

	for (x = 0; x < 8; x++) {
		for (y = 0; y < 8; y++) {
			/* Get board piece color */
			color = (x % 2 == y % 2);
			
			[fBoardDrawStyle[color] 
							startStyle:overReflection 	
							? 1.0f-fBoardReflectivity 
							: 1.0f];

			float r = random()/8589934588.0f; /* 4*(2**31-1) */

			/* draw one square */
			glBegin(GL_TRIANGLE_STRIP);
			glTexCoord2f(0.0f+r, 0.0f+r);
			glVertex3f(x * 10.0f - 40.0f, 0.0f, 40.0f - y * 10.0f);
			glTexCoord2f(0.5f+r, 0.0f+r);
			glVertex3f(x * 10.0f - 30.0f, 0.0f, 40.0f - y * 10.0f);
			glTexCoord2f(0.0f+r, 0.5f+r);
			glVertex3f(x * 10.0f - 40.0f, 0.0f, 30.0f - y * 10.0f);
			glTexCoord2f(0.5f+r, 0.5f+r);
			glVertex3f(x * 10.0f - 30.0f, 0.0f, 30.0f - y * 10.0f);
			glEnd();
			
#if 0
			if (!overReflection) {
				glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
				glBegin(GL_QUADS);
				glTexCoord2f(0.0f+r, 0.0f+r);
				glVertex3f(x * 10.0f - 40.0f, 0.0f, 40.0f - y * 10.0f);
				glTexCoord2f(0.5f+r, 0.0f+r);
				glVertex3f(x * 10.0f - 30.0f, 0.0f, 40.0f - y * 10.0f);
				glTexCoord2f(0.5f+r, 0.5f+r);
				glVertex3f(x * 10.0f - 30.0f, 0.0f, 30.0f - y * 10.0f);
				glTexCoord2f(0.0f+r, 0.5f+r);
				glVertex3f(x * 10.0f - 40.0f, 0.0f, 30.0f - y * 10.0f);
				glEnd();
				glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
			}
#endif
		}
	}

	if (overReflection) {
		glPopAttrib();
		
		return;
	}


	//
	// Draw border
	//
	const float IB = kBoardRadius; 		// Inside border
	const float OB = IB+kBorderWidth; 	// Outside border
	const float DP =  5.0f;				// Depth
	const float TO = 0.5f*(1.0f - IB/OB); 	// Texture offset

	[fBorderDrawStyle startStyle:1.0f];

	//
	// Front
	//
	glBegin(GL_TRIANGLE_STRIP);
	glNormal3f(0.0f, 1.0f, 0.0f);
	glTexCoord2f(0.0f, 0.0f);
	glVertex3f(-OB, 0.0f, +OB);
	glTexCoord2f(TO, 0.0f);
	glVertex3f(-IB, 0.0f, +OB);
	glTexCoord2f(TO, 1.0f);
	glVertex3f(-IB, 0.0f, +IB);
	glTexCoord2f(1.0f-TO, 0.0f);
	glVertex3f(+IB, 0.0f, +OB);
	glTexCoord2f(1.0f-TO, 1.0f);
	glVertex3f(+IB, 0.0f, +IB);
	glTexCoord2f(1.0f, 0.0f);
	glVertex3f(+OB, 0.0f, +OB);
	glEnd();
	glBegin(GL_TRIANGLE_STRIP);
	glNormal3f(0.0f, 0.0f, 1.0f);
	glTexCoord2f(0.0f, 1.0f);
	glVertex3f(-OB, 0.0f, +OB);
	glTexCoord2f(0.0f, 0.0f);
	glVertex3f(-OB,  -DP, +OB);
	glTexCoord2f(1.0f, 1.0f);
	glVertex3f(+OB, 0.0f, +OB);
	glTexCoord2f(1.0f, 0.0f);
	glVertex3f(+OB,  -DP, +OB);
	glEnd();
	//
	// Back
	//
	glBegin(GL_TRIANGLE_STRIP);
	glNormal3f(0.0f, 1.0f, 0.0f);
	glTexCoord2f(0.0f, 0.0f);
	glVertex3f(-OB, 0.0f, -OB);
	glTexCoord2f(TO, 1.0f);
	glVertex3f(-IB, 0.0f, -IB);
	glTexCoord2f(TO, 0.0f);
	glVertex3f(-IB, 0.0f, -OB);
	glTexCoord2f(1.0f-TO, 1.0f);
	glVertex3f(+IB, 0.0f, -IB);
	glTexCoord2f(1.0f-TO, 0.0f);
	glVertex3f(+IB, 0.0f, -OB);
	glTexCoord2f(1.0f, 0.0f);
	glVertex3f(+OB, 0.0f, -OB);
	glEnd();
	glBegin(GL_TRIANGLE_STRIP);
	glNormal3f(0.0f, 0.0f, -1.0f);
	glTexCoord2f(0.0f, 1.0f);
	glVertex3f(-OB, 0.0f, -OB);
	glTexCoord2f(1.0f, 1.0f);
	glVertex3f(+OB, 0.0f, -OB);
	glTexCoord2f(0.0f, 0.0f);
	glVertex3f(-OB,  -DP, -OB);
	glTexCoord2f(1.0f, 0.0f);
	glVertex3f(+OB,  -DP, -OB);
	glEnd();
	//
	// Left
	//
	glBegin(GL_TRIANGLE_STRIP);
	glNormal3f(0.0f, 1.0f, 0.0f);
	glTexCoord2f(1.0f, 0.0f);
	glVertex3f(-OB, 0.0f, +OB);
	glTexCoord2f(1.0f-TO, 1.0f);
	glVertex3f(-IB, 0.0f, +IB);
	glTexCoord2f(1.0f-TO, 0.0f);
	glVertex3f(-OB, 0.0f, +IB);
	glTexCoord2f(TO, 1.0f);
	glVertex3f(-IB, 0.0f, -IB);
	glTexCoord2f(TO, 0.0f);
	glVertex3f(-OB, 0.0f, -IB);
	glTexCoord2f(0.0f, 0.0f);
	glVertex3f(-OB, 0.0f, -OB);
	glEnd();
	glBegin(GL_TRIANGLE_STRIP);
	glNormal3f(-1.0f, 0.0f, 0.0f);
	glTexCoord2f(1.0f, 1.0f);
	glVertex3f(-OB, 0.0f, +OB);
	glTexCoord2f(0.0f, 1.0f);
	glVertex3f(-OB, 0.0f, -OB);
	glTexCoord2f(1.0f, 0.0f);
	glVertex3d(-OB,  -DP, +OB);
	glTexCoord2f(0.0f, 0.0f);
	glVertex3d(-OB,  -DP, -OB);
	glEnd();
	//
	// Right
	//
	glBegin(GL_TRIANGLE_STRIP);
	glNormal3f(0.0f, 1.0f, 0.0f);
	glTexCoord2f(0.0f, 0.0f);
	glVertex3f(+OB, 0.0f, +OB);
	glTexCoord2f(TO, 0.0f);
	glVertex3f(+OB, 0.0f, +IB);
	glTexCoord2f(TO, 1.0f);
	glVertex3f(+IB, 0.0f, +IB);
	glTexCoord2f(1.0f-TO, 0.0f);
	glVertex3f(+OB, 0.0f, -IB);
	glTexCoord2f(1.0f-TO, 1.0f);
	glVertex3f(+IB, 0.0f, -IB);
	glTexCoord2f(1.0f, 0.0f);
	glVertex3f(+OB, 0.0f, -OB);
	glEnd();
	glBegin(GL_TRIANGLE_STRIP);
	glNormal3f(1.0f, 0.0f, 0.0f);
	glTexCoord2f(0.0f, 1.0f);
	glVertex3f(+OB, 0.0f, +OB);
	glTexCoord2f(0.0f, 0.0f);
	glVertex3d(+OB,  -DP, +OB);
	glTexCoord2f(1.0f, 1.0f);
	glVertex3f(+OB, 0.0f, -OB);
	glTexCoord2f(1.0f, 0.0f);
	glVertex3d(+OB,  -DP, -OB);
	glEnd();

#if 0
	//
	// Draw outline of border so boundaries are antialiased
	//
	glBegin(GL_LINE_STRIP);
	glVertex3f(-OB,  -DP, +OB);
	glVertex3f(+OB,  -DP, +OB);
	glVertex3f(+OB,  -DP, -OB);
	glVertex3f(-OB,  -DP, -OB);
	glVertex3f(-OB,  -DP, +OB);
	glEnd();
	glBegin(GL_LINE_STRIP);
	glVertex3f(-OB, 0.0f, +OB);
	glVertex3f(+OB, 0.0f, +OB);
	glVertex3f(+OB, 0.0f, -OB);
	glVertex3f(-OB, 0.0f, -OB);
	glVertex3f(-OB, 0.0f, +OB);
	glEnd();
	glBegin(GL_LINE_STRIP);
	glVertex3f(-IB, 0.0f, +IB);
	glVertex3f(+IB, 0.0f, +IB);
	glVertex3f(+IB, 0.0f, -IB);
	glVertex3f(-IB, 0.0f, -IB);
	glVertex3f(-IB, 0.0f, +IB);
	glEnd();
#endif

	glPopAttrib();
}

/* Draws the co-ordinates around the edge of the board */
- (void) drawCoords
{
	glPushAttrib(GL_LIGHTING | GL_TEXTURE_BIT | GL_ENABLE_BIT);

	glEnable(GL_TEXTURE_2D);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glEnable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_LIGHTING);

	const float kSize	= 3.5f;
	const float kNHOff	= 0.75f;
	const float kNVOff	= 3.25f;
	const float kLHOff	= 3.25f;
	const float kLVOff	= 0.75f;

	/* Draw the numbers, always on the left and upright, no matter
	   which color we're playing
	*/
	glColor4f(1.0f, 1.0f, 1.0f, fLabelIntensity);
	for (int rows = 0; rows < 8; rows++)  {
 		glBindTexture(GL_TEXTURE_2D, fNumberTextures[rows]);

		float	t,l,b,r;

		if ([self facingWhite]) {
			l = -(40.0f + kNHOff + kSize);
			r = -(40.0f + kNHOff);
			t = 40.0f - kNVOff - rows*10.0f - kSize;
			b = 40.0f - kNVOff - rows*10.0f;
		} else {
			r = -(40.0f + kNHOff + kSize);
			l = -(40.0f + kNHOff);
			b = 40.0f - kNVOff - rows*10.0f - kSize;
			t = 40.0f - kNVOff - rows*10.0f;
		}
		glBegin(GL_QUADS);
		glTexCoord2f(0.0f, 0.0f);
		glVertex3f(l, 0.0f, b);
		glTexCoord2f(1.0f, 0.0f);
		glVertex3f(r, 0.0f, b);
		glTexCoord2f(1.0f, 1.0f);
		glVertex3f(r, 0.0f, t);
		glTexCoord2f(0.0f, 1.0f);
		glVertex3f(l, 0.0f, t);
		glEnd();
	}

	/* Draw the letters */
	for (int cols = 0; cols < 8; cols++) {
		glBindTexture(GL_TEXTURE_2D, fLetterTextures[cols]);

		float	t,l,b,r;

		if ([self facingWhite]) {
			t = 40.0f + kLVOff;
			b = 40.0f + kLVOff + kSize;
			l = cols*10.f + kLHOff - 40.0f;
			r = cols*10.f + kLHOff - 40.0f + kSize;
		} else {
			t = -(40.0f + kLVOff);
			b = -(40.0f + kLVOff + kSize);
			r = cols*10.f + kLHOff - 40.0f;
			l = cols*10.f + kLHOff - 40.0f + kSize;
		}
		glBegin(GL_QUADS);
		glTexCoord2f(0.0f, 0.0f);
		glVertex3f(l, 0.0f, b);
		glTexCoord2f(1.0f, 0.0f);
		glVertex3f(r, 0.0f, b);
		glTexCoord2f(1.0f, 1.0f);
		glVertex3f(r, 0.0f, t);
		glTexCoord2f(0.0f, 1.0f);
		glVertex3f(l, 0.0f, t);
		glEnd();
	}

	glPopAttrib();
}

- (void) setupPieceDrawing:(BOOL)white reflect:(BOOL)reflection alpha:(float)alpha
{
	glPushAttrib(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	if (reflection)
		alpha *= fBoardReflectivity;
	[fPieceDrawStyle[!white] startStyle:alpha];
	glDepthMask(alpha > 0.5f);
}

- (void) endPieceDrawing
{
	glPopAttrib();
}

/* Draws a single piece */
- (void) simplyDrawPiece:(MBCPiece)piece at:(MBCPosition)pos scale:(float)scale
{
	bool		wkr 	= false; /* white knight rotate flag */
	int 		color	= Color(piece) != 0;
	piece				= Piece(piece);

	if (!color) 			/* white */
		if (piece == KNIGHT)	/* white knight */
			wkr = true;		/* white knight */

	glPushMatrix();
	glTranslatef(pos[0], pos[1], pos[2]);
	if (wkr)		/* is white knight */
		glRotatef(180.0f, 0.0f, 1.0f, 0.0f);
	glScalef(scale, scale, scale);
	glCallList(piece);
	glPopMatrix();

	fLastPieceDrawn	= piece;
}

- (void) drawPiece:(MBCPiece)piece at:(MBCPosition)pos scale:(float)scale reflect:(BOOL)reflection alpha:(float)alpha
{
	[self setupPieceDrawing:!Color(piece) reflect:reflection alpha:alpha];
	[self simplyDrawPiece:piece at:pos scale:scale];
	[self endPieceDrawing];
}

- (void) drawPiece:(MBCPiece)piece at:(MBCPosition)pos reflect:(BOOL)reflection alpha:(float)alpha
{
	[self setupPieceDrawing:!Color(piece) reflect:reflection alpha:alpha];
	[self simplyDrawPiece:piece at:pos scale:1.0f];
	[self endPieceDrawing];
}

- (void) drawPiece:(MBCPiece)piece at:(MBCPosition)pos reflect:(BOOL)reflection
{
	[self setupPieceDrawing:!Color(piece) reflect:reflection alpha:1.0f];
	[self simplyDrawPiece:piece at:pos scale:1.0f];
	[self endPieceDrawing];
} 

- (void) drawPiece:(MBCPiece)piece at:(MBCPosition)pos scale:(float)scale
{
	[self setupPieceDrawing:!Color(piece) reflect:NO alpha:1.0f];
	[self simplyDrawPiece:piece at:pos scale:scale];
	[self endPieceDrawing];
}

/* Draws the pieces */
- (void) drawPieces:(BOOL)reflection
{
	for (MBCSquare square = 0; square<64; ++square) {
		MBCPiece  piece = fInAnimation 
			? [fBoard oldContents:square]
			: [fBoard curContents:square];
		if (fSelectedPiece && square == fSelectedSquare)
				continue;	// Skip original position of selected piece
		if (piece) {
			const MBCPosition pos = [self squareToPosition:square];
			float dist			  = 
				fSelectedPiece && (!fInAnimation || square == fSelectedDest)
				? fSelectedPos.FlatDistance(pos) 
				: 100.0f;
			const float	kProximity = 5.0f;
			if (dist < kProximity)
				[self drawPiece:piece at:pos reflect:reflection
					  alpha:pow(dist/kProximity, 4.0)];
			else	
				[self drawPiece:piece at:pos reflect:reflection];
		}
	}
}


/* Draw the selected piece (may be off grid) */
- (void) drawSelectedPiece:(BOOL)reflection
{
	[self drawPiece:fSelectedPiece at:fSelectedPos reflect:reflection];
}

/* Draw the promotion piece (transparent) */
- (void) drawPromotionPiece
{
	MBCPiece 	piece = EMPTY;
	MBCPosition pos;

	fPromotionSide = kNeitherSide;

	if (fSide == kWhiteSide || fSide == kBothSides)
		if ([fBoard canPromote:kWhiteSide]) {
			piece			=	[fBoard defaultPromotion:YES];
			fPromotionSide	=	kWhiteSide;
			pos[0]			= 	-kPromotionPieceX;
			pos[1]			= 	  0.0f;
			pos[2]			=   -kPromotionPieceZ;
		} 
	if (fSide == kBlackSide || fSide == kBothSides)
		if ([fBoard canPromote:kBlackSide]) {
			piece			=	[fBoard defaultPromotion:NO];
			fPromotionSide	= 	kBlackSide;
			pos[0]			= 	kPromotionPieceX;
			pos[1]			= 	  0.0f;
			pos[2]			=   kPromotionPieceZ;
		}
	if (fPromotionSide == kNeitherSide)
		return;
	
	bool		wkr		= (fPromotionSide == kWhiteSide && piece == KNIGHT);

	glPushAttrib(GL_ENABLE_BIT);
	
	[fSelectedPieceDrawStyle startStyle:1.0f];

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glPushMatrix();
	glTranslatef(pos[0], pos[1], pos[2]);
	if (wkr)			/* is white knight */
		glRotatef(180.0f, 0.0f, 1.0f, 0.0f);
	glCallList(Piece(piece));
	glPopMatrix();

	glPopAttrib();
}

- (void) placeLights
{
	GLfloat l_diffuse[4] = { 1.0, 1.0, 1.0, 1.0 };
	GLfloat l_ambient[4] = { fAmbient, fAmbient, fAmbient, 0.0 };

	glEnable(GL_LIGHT0);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, l_diffuse);
	glLightfv(GL_LIGHT0, GL_AMBIENT, l_ambient);
	glLightfv(GL_LIGHT0, GL_SPECULAR, l_diffuse);
	glLightfv(GL_LIGHT0, GL_POSITION, fLightPos);
}

MBCPieceCode gInHandOrder[] = {PAWN, BISHOP, KNIGHT, ROOK, QUEEN};

- (void) drawPiecesInHand
{
	const float kLabelX 	=  41.0f;
	const float kLabelSize	=  4.0f;
	const float	kLabelLeft	=  kLabelX;
	const float kLabelRight	=  kLabelX+kLabelSize;
	const float kSpacing	=  kInHandPieceSize;
	const float kLabelZ		=  4.0f;
	const float kPieceX		=  kInHandPieceX;
	const float kPieceZ		=  kInHandPieceZOffset+kInHandPieceSize/2.0f;
	const float kScale		=  0.70f;
	const bool  kFlip		=  fAzimuth < 90.0f || fAzimuth >= 270.0f;
	const float	kTexLeft	=  kFlip ? 1.0f : 0.0f;
	const float kTexRight	=  1.0f-kTexLeft;
	const float kTexBottom	=  kFlip ? 1.0f : 0.0f;
	const float kTexTop		=  1.0f-kTexBottom;

	glPushAttrib(GL_LIGHTING | GL_ENABLE_BIT | GL_TEXTURE_BIT);

	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_LIGHTING);
	glDisable(GL_DEPTH_TEST);

	/* Draw the numbers */
	glColor4f(1.0f, 1.0f, 1.0f, fLabelIntensity);

	for (int p = 0; p<5; ++p) {
		MBCPiece piece = White(gInHandOrder[p]);
		int numInHand = fInAnimation 
			? [fBoard oldInHand:piece] 
			: [fBoard curInHand:piece];
		numInHand -= (piece+kInHandSquare == fSelectedSquare);
		if (numInHand > 1) {
			glBindTexture(GL_TEXTURE_2D, fNumberTextures[min(numInHand,8)-1]);

			float z	= p * kSpacing;

			glBegin(GL_QUADS);
			glTexCoord2f(kTexLeft, kTexBottom);
			glVertex3f(kLabelLeft, 0.0f,  kLabelZ+z+kLabelSize);
			glTexCoord2f(kTexRight, kTexBottom);
			glVertex3f(kLabelRight, 0.0f, kLabelZ+z+kLabelSize);
			glTexCoord2f(kTexRight, kTexTop);
			glVertex3f(kLabelRight, 0.0f, kLabelZ+z);
			glTexCoord2f(kTexLeft, kTexTop);
			glVertex3f(kLabelLeft, 0.0f,  kLabelZ+z);
			glEnd();			
		}
	}

	for (int p = 0; p<5; ++p) {
		MBCPiece piece = Black(gInHandOrder[p]);
		int numInHand = fInAnimation 
			? [fBoard oldInHand:piece] 
			: [fBoard curInHand:piece];
		numInHand -= (piece+kInHandSquare == fSelectedSquare);
		if (numInHand > 1) {
			glBindTexture(GL_TEXTURE_2D, fNumberTextures[min(numInHand,8)-1]);

			float z	= p * kSpacing;

			glBegin(GL_QUADS);
			glTexCoord2f(kTexLeft, kTexBottom);
			glVertex3f(kLabelLeft, 0.0f,  -kLabelZ-z);
			glTexCoord2f(kTexRight, kTexBottom);
			glVertex3f(kLabelRight, 0.0f, -kLabelZ-z);
			glTexCoord2f(kTexRight, kTexTop);
			glVertex3f(kLabelRight, 0.0f, -kLabelZ-z-kLabelSize);
			glTexCoord2f(kTexLeft, kTexTop);
			glVertex3f(kLabelLeft, 0.0f,  -kLabelZ-z-kLabelSize);
			glEnd();			
		}
	}

	glDisable(GL_BLEND);

	glPopAttrib();

	[self placeLights];

	for (int p = 0; p<5; ++p) {
		MBCPiece piece = White(gInHandOrder[p]);
		int numInHand = fInAnimation 
			? [fBoard oldInHand:piece] 
			: [fBoard curInHand:piece];
		numInHand -= (piece+kInHandSquare == fSelectedSquare);
		if (numInHand) {
			MBCPosition pos = {{kPieceX, 0.0f,  kPieceZ}};
			pos[2]		   += p*kSpacing;

			[self drawPiece:piece at:pos scale:kScale];			
		}
	}

	for (int p = 0; p<5; ++p) {
		MBCPiece piece = Black(gInHandOrder[p]);
		int numInHand = fInAnimation 
			? [fBoard oldInHand:piece] 
			: [fBoard curInHand:piece];
		numInHand -= (piece+kInHandSquare == fSelectedSquare);
		if (numInHand) {
			MBCPosition pos = {{kPieceX, 0.0f,  -kPieceZ}};
			pos[2]		   -= p*kSpacing;

			[self drawPiece:piece at:pos scale:kScale];			
		}
	}
	glPopMatrix();
}

- (void) drawArrowFrom:(MBCPosition)fromPos to:(MBCPosition)toPos width:(float)w
{
	glPushAttrib(GL_ENABLE_BIT);	/* Save states */
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_LIGHTING);
	glEnable(GL_BLEND);
   	glDisable(GL_CULL_FACE);	    /* Too lazy to figure out winding */
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	const float	kStemW  = w;
	const float	kPointW	= 2.00f*w;
	const float	kPointH	= 1.50f*w;
	const float	kHeight	= 0.01f;

	const float len				= 
		hypot(toPos[0]-fromPos[0], toPos[2]-fromPos[2]);
	const float	alpha			= 
		atan2(toPos[2]-fromPos[2], toPos[0]-fromPos[0]);
	const float sinAlpha		= sin(alpha);
	const float cosAlpha		= cos(alpha);

	MBCPosition p1	=	fromPos;
	p1.pos[0]	   -= 	kStemW*sinAlpha;
	p1.pos[1]		= 	kHeight;
	p1.pos[2]	   += 	kStemW*cosAlpha;
	MBCPosition p2	=	fromPos;
	p2.pos[0]	   += 	kStemW*sinAlpha;
	p2.pos[1]		= 	kHeight;
	p2.pos[2]	   -= 	kStemW*cosAlpha;
	MBCPosition p3	=	p1;
	p3.pos[0]	   += 	(len-kPointH)*cosAlpha;
	p3.pos[2]	   += 	(len-kPointH)*sinAlpha;
	MBCPosition p4	=	p2;
	p4.pos[0]	   += 	(len-kPointH)*cosAlpha;
	p4.pos[2]	   += 	(len-kPointH)*sinAlpha;
	MBCPosition p5	=	p3;
	p5.pos[0]	   -= 	(kPointW-kStemW)*sinAlpha;
	p5.pos[2]	   += 	(kPointW-kStemW)*cosAlpha;
	MBCPosition p6	=	p4;
	p6.pos[0]	   += 	(kPointW-kStemW)*sinAlpha;
	p6.pos[2]	   -= 	(kPointW-kStemW)*cosAlpha;
	MBCPosition p7	=	toPos;
	p7.pos[1]		= 	kHeight;

	glBegin(GL_TRIANGLES);
	glVertex3fv(p1);
	glVertex3fv(p2);
	glVertex3fv(p4);
	glVertex3fv(p4);
	glVertex3fv(p3);
	glVertex3fv(p1);
	glVertex3fv(p5);
	glVertex3fv(p6);
	glVertex3fv(p7);
	glEnd();

	glPopAttrib();
}

- (void) drawMove:(MBCMove *)move asHint:(BOOL)hint
{
	if (!move)
		return;

	MBCPosition	fromPos	= [self squareToPosition: move->fFromSquare];
	MBCPosition	toPos	= [self squareToPosition: move->fToSquare];

	if (hint) 
		glColor4f(1.0f, 0.0f, 0.0f, 0.5f);
	else
		glColor4f(0.0f, 0.0f, 1.0f, 0.5f);
	
	[self drawArrowFrom:fromPos to:toPos width:2.0f];
}

- (void) drawManipulator
{
	//
	// Save normal projection and superimpose an Ortho projection
	//
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	NSRect b = [self bounds];
	gluOrtho2D(NSMinX(b), NSMaxX(b), NSMinY(b), NSMaxY(b));
	glMatrixMode(GL_MODELVIEW);
    glRotatef(-90.0, 1.0, 0.0, 0.0);
	//
	// Draw navigation aid
	//
	bool	    horizontal		= 
		fabs(fCurMouse.x-fOrigMouse.x) > fabs(fCurMouse.y-fOrigMouse.y);
	const float	kCircleSize		= 10.0f;
	const float	kArrowClearance	= 15.0f;
	const float	kArrowLength	= 30.0f;
	const float kArrowWidth		= 10.0f;
	const float	kThreshold		= 10.0f;
	const float kWellSize		= 55.0f;
	const float kWellRound		= 20.0f;

	GLfloat on_color[4] 		= {1.0f, 1.0f, 1.0f, 1.0f};
	GLfloat off_color[4] 		= {1.0f, 1.0f, 1.0f, 0.4f};
	GLfloat	well_color[4]		= {0.5f, 0.5f, 0.5f, 0.6f};
	//
	// Well & Circle
	//
	glPushAttrib(GL_ENABLE_BIT);	/* Save states */
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_LIGHTING);
	glEnable(GL_BLEND);
   	glDisable(GL_CULL_FACE);	    /* Too lazy to figure out winding */
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	GLUquadricObj * q 		= gluNewQuadric();

	glPushMatrix();
    glRotatef(90.0, 1.0, 0.0, 0.0);
	glTranslatef(fOrigMouse.x, fOrigMouse.y, 0.01f);
	
	glColor4fv(well_color);
	glBegin(GL_QUADS);
	glVertex3f(-kWellSize+kWellRound, -kWellSize, 0.0f);
	glVertex3f( kWellSize-kWellRound, -kWellSize, 0.0f);
	glVertex3f( kWellSize-kWellRound, -kWellSize+kWellRound, 0.0f);
	glVertex3f(-kWellSize+kWellRound, -kWellSize+kWellRound, 0.0f);
	glVertex3f(-kWellSize, -kWellSize+kWellRound, 0.0f);
	glVertex3f( kWellSize, -kWellSize+kWellRound, 0.0f);
	glVertex3f( kWellSize,  kWellSize-kWellRound, 0.0f);
	glVertex3f(-kWellSize,  kWellSize-kWellRound, 0.0f);
	glVertex3f(-kWellSize+kWellRound, kWellSize-kWellRound, 0.0f);
	glVertex3f( kWellSize-kWellRound, kWellSize-kWellRound, 0.0f);
	glVertex3f( kWellSize-kWellRound, kWellSize, 0.0f);
	glVertex3f(-kWellSize+kWellRound, kWellSize, 0.0f);
	glEnd();
	glTranslatef(-kWellSize+kWellRound, -kWellSize+kWellRound, 0.0f);
	gluPartialDisk(q, 0.0, kWellRound, 10, 1, 180.0, 90.0);
	glTranslatef(2.0*(kWellSize-kWellRound), 0.0f, 0.0f);
	gluPartialDisk(q, 0.0, kWellRound, 10, 1,  90.0, 90.0);
	glTranslatef(0.0, 2.0*(kWellSize-kWellRound), 0.0f);
	gluPartialDisk(q, 0.0, kWellRound, 10, 1,   0.0, 90.0);
	glTranslatef(-2.0*(kWellSize-kWellRound), 0.0f, 0.0f);
	gluPartialDisk(q, 0.0, kWellRound, 10, 1, 270.0, 90.0);
	glTranslatef( kWellSize-kWellRound, -kWellSize+kWellRound, 0.0f);

	glColor4fv(fabs(fCurMouse.x-fOrigMouse.x)<kThreshold 
			&& fabs(fCurMouse.y-fOrigMouse.y)<kThreshold 
			   ? on_color : off_color);
	gluDisk(q, 0.0, kCircleSize, 10, 1);
	glPopMatrix();
	gluDeleteQuadric(q);

	MBCPosition	fromPos, toPos;

	//
	// Up
	//
	fromPos[0] = fOrigMouse.x;
	fromPos[1] = 0;
	fromPos[2] = fOrigMouse.y+kArrowClearance;
	toPos	   = fromPos;
	toPos[2]  += kArrowLength;
	glColor4fv((!horizontal && (fCurMouse.y > fOrigMouse.y+kThreshold))
			   ? on_color : off_color);
	[self drawArrowFrom:fromPos to:toPos width:kArrowWidth];

	//
	// Down
	//
	fromPos[0] = fOrigMouse.x;
	fromPos[1] = 0;
	fromPos[2] = fOrigMouse.y-kArrowClearance;
	toPos	   = fromPos;
	toPos[2]  -= kArrowLength;
	glColor4fv((!horizontal && (fCurMouse.y < fOrigMouse.y-kThreshold))
			   ? on_color : off_color);
	[self drawArrowFrom:fromPos to:toPos width:kArrowWidth];

	//
	// Right
	//
	fromPos[0] = fOrigMouse.x+kArrowClearance;
	fromPos[1] = 0;
	fromPos[2] = fOrigMouse.y;
	toPos	   = fromPos;
	toPos[0]  += kArrowLength;
	glColor4fv((horizontal && (fCurMouse.x > fOrigMouse.x+kThreshold))
			   ? on_color : off_color);
	[self drawArrowFrom:fromPos to:toPos width:kArrowWidth];

	//
	// Left
	//
	fromPos[0] = fOrigMouse.x-kArrowClearance;
	fromPos[1] = 0;
	fromPos[2] = fOrigMouse.y;
	toPos	   = fromPos;
	toPos[0]  -= kArrowLength;
	glColor4fv((horizontal && (fCurMouse.x < fOrigMouse.x-kThreshold))
			   ? on_color : off_color);
	[self drawArrowFrom:fromPos to:toPos width:kArrowWidth];

	glPopAttrib();
	//
	// Restore projection
	//
	glPopMatrix();	
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
}

- (void) makeBoardSolid
{
	//
	// If we're in a transparent window, we have to make sure that the
	// board itself always remains opaque, no matter what blending we've
	// done with it
	//
	const float IB = kBoardRadius; 		// Inside border
	const float OB = IB+kBorderWidth; 	// Outside border
	const float DP =  5.0f;				// Depth

	glPushAttrib(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_ENABLE_BIT);
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
	glDisable(GL_BLEND);
	glDisable(GL_LIGHTING);
   	glDisable(GL_CULL_FACE);	    /* Too lazy to figure out winding */
	glDisable(GL_DEPTH_TEST);
	glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
	glBegin(GL_QUADS);
	glVertex3f(-OB, 0.0f,  OB);
	glVertex3f( OB, 0.0f,  OB);
	glVertex3f( OB, 0.0f, -OB);
	glVertex3f(-OB, 0.0f, -OB);
	glEnd();	
	glBegin(GL_QUAD_STRIP);
	glVertex3f(-OB,  -DP,  OB);
	glVertex3f(-OB, 0.0f,  OB);
	glVertex3f( OB,  -DP,  OB);
	glVertex3f( OB, 0.0f,  OB);
	glVertex3f( OB,  -DP, -OB);
	glVertex3f( OB, 0.0f, -OB);
	glVertex3f(-OB,  -DP, -OB);
	glVertex3f(-OB, 0.0f, -OB);
	glEnd();
	glPopAttrib();
}

/* Draw the scene for a game */
- (void) drawPosition
{
	if (fIsFloating) {
		[[NSColor clearColor] set];
		NSRectFill([self bounds]);
	}

	if (fNeedPerspective)
		[self setupPerspective];

	/* Clear the buffers */
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	if (fBoardReflectivity)
		glClear(GL_STENCIL_BUFFER_BIT);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	/* Place lights */
	[self placeLights];

	/* Draw the board */
   	[self drawBoard:NO];

	/* Make a stencil of the floor if reflections are done */
	if (fBoardReflectivity) {
		/* Save the old scene attributes */
		glPushAttrib(GL_COLOR_BUFFER_BIT | GL_ENABLE_BIT | GL_LIGHTING_BIT);

		/* Disable stuff */
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_LIGHTING);
		glDisable(GL_BLEND);

		/* Don't draw to the screen or the depth buffer at this moment */
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		glDepthMask(GL_FALSE);

		/* Write to the stencil buffer */
		glEnable(GL_STENCIL_TEST);
		glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
		glStencilFunc(GL_ALWAYS, 1, 0xffffffff);

		glBegin(GL_QUADS);
		glVertex3f(-40.0f, 0.0f,  40.0f);
		glVertex3f( 40.0f, 0.0f,  40.0f);
		glVertex3f( 40.0f, 0.0f, -40.0f);
		glVertex3f(-40.0f, 0.0f, -40.0f);
		glEnd();

		// 
		// Re-enable writing to the depth buffer and to the color channels
		// but NOT to the alpha channel in case we have a translucent window
		//
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
		glDepthMask(GL_TRUE);

		/* Draw only if stencil is set to 1 */
		glStencilFunc(GL_EQUAL, 1, 0xffffffff);
		glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

		/* draw the reflected pieces */

		/* Reflect in floor */
		glPushMatrix(); {
			glScalef(1.0f, -1.0f, 1.0f);

			glEnable(GL_LIGHTING);
			[self placeLights];

			glCullFace(GL_FRONT); {
			    [self drawPieces:YES];

			    if (fSelectedPiece)
					[self drawSelectedPiece:YES];
			} glCullFace(GL_BACK);
		} glPopMatrix();

		/* Restore the scene attributes */
		glPopAttrib();

		/* Now blend board back into the reflections */
		[self drawBoard:YES];
	}

	/* Draw the co-ordinates [1-8] [a-h] */
	[self drawCoords];

	/* Draw hint and last move */
	[self drawMove:fHintMove asHint:YES];
	[self drawMove:fLastMove asHint:NO];

	glDisable(GL_BLEND);

	/* Draw the pieces */
	[self drawPieces:NO];

	if (fSelectedPiece) 
		[self drawSelectedPiece:NO];

	if (fVariant == kVarCrazyhouse)
		[self drawPiecesInHand];

	[self drawPromotionPiece];

#if 0
	//
	// Some graphics cards seem to mess up the lighting when the knight
	// or king model were the last ones drawn
	//
	if (fLastPieceDrawn == KING || fLastPieceDrawn == KNIGHT) {
		MBCPosition pos = {{0.0f, 0.0f, 0.0f}};
		[self drawPiece:PAWN at:pos reflect:NO alpha:0.0f];
	}
#endif

	if (fInBoardManipulation)
		[self drawManipulator];

	// Update the GL context
	if (fIsFloating) 
		[self makeBoardSolid];

	[[self openGLContext] flushBuffer];
}

@end

// Local Variables:
// mode:ObjC
// End:
