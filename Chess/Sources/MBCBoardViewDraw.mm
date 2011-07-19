/*
	File:		MBCBoardViewDraw.mm
	Contains:	Draw chess board
	Version:	1.0
	Copyright:	© 2002-2011 by Apple Computer, Inc., all rights reserved.
	
	Derived from glChess, Copyright © 2002 Robert Ancell and Michael Duelli
	Permission granted to Apple to relicense under the following terms:

	File Ownership:

		DRI:				Matthias Neeracher    x43683

	Writers:

		(MN)	Matthias Neeracher

	Change History (most recent first):

		$Log: MBCBoardViewDraw.mm,v $
		Revision 1.44  2011/03/14 21:10:27  neerache
		<rdar://problem/9129384> HiDPI adoption for Chess.app
		
		Revision 1.43  2010/10/08 00:15:36  neerache
		Tweak window background
		
		Revision 1.42  2008/10/24 01:17:14  neerache
		<rdar://problem/5973744> Chess Needs To Move from SGI Format Images to PNG or JPEG
		
		Revision 1.41  2007/01/16 21:23:47  neerache
		Adapt to changed HiDPI code for NSOpenGLView
		
		Revision 1.40  2006/10/09 21:05:01  neerache
		Correct HiDPI problems <rdar://problem/4412218>
		
		Revision 1.39  2006/07/27 22:00:02  neerache
		Work around OpenGL driver issue for WWDC <rdar://problem/4655889>
		
		Revision 1.38  2006/03/28 02:53:00  neerache
		Fix incorrect type
		
		Revision 1.37  2004/12/20 09:39:29  neerache
		Implement self test (RADAR 3590419 / Feature 8905)
		
		Revision 1.36  2004/08/16 07:50:23  neerache
		Hilight picked piece
		
		Revision 1.35  2004/07/10 04:53:29  neerache
		Tweak visuals
		
		Revision 1.34  2003/11/06 23:30:51  neerache
		Adjust wording as suggested by Joyce Chow
		
		Revision 1.33  2003/10/29 22:39:31  neerache
		Add tools & clean up copyright references for release
		
		Revision 1.32  2003/08/06 00:13:43  neerache
		Respect label intensities again
		
		Revision 1.31  2003/08/01 23:53:19  neerache
		Get rid of erroneous use of GL_SRC_COLOR (RADAR 3343477)
		
		Revision 1.30  2003/07/14 23:21:49  neerache
		Move promotion defaults into MBCBoard
		
		Revision 1.29  2003/07/07 23:50:42  neerache
		Work around a graphics bug
		
		Revision 1.28  2003/07/07 09:16:42  neerache
		Textured windows are too slow for low end machines, disable
		
		Revision 1.27  2003/07/07 08:47:53  neerache
		Switch to textured main window
		
		Revision 1.26  2003/06/18 21:55:17  neerache
		More (mostly unsuccessful) tweaking of floating windows
		
		Revision 1.25  2003/06/16 02:18:03  neerache
		Implement floating board
		
		Revision 1.24  2003/06/15 21:13:09  neerache
		Adjust lights, fix animation, work on other drawing issues
		
		Revision 1.23  2003/06/05 08:31:26  neerache
		Added Tuner
		
		Revision 1.22  2003/06/05 00:14:37  neerache
		Reduce excessive threshold
		
		Revision 1.21  2003/06/04 23:14:05  neerache
		Neater manipulation widget; remove obsolete graphics options
		
		Revision 1.20  2003/06/04 09:25:47  neerache
		New and improved board manipulation metaphor
		
		Revision 1.19  2003/06/02 05:44:48  neerache
		Implement direct board manipulation
		
		Revision 1.18  2003/06/02 04:21:40  neerache
		Start implementing drawing styles for board elements
		
		Revision 1.17  2003/05/27 07:25:48  neerache
		Apply scaling and translation in proper order
		
		Revision 1.16  2003/05/24 20:31:23  neerache
		Lots of graphics improvements, espcially with specular light
		
		Revision 1.15  2003/05/23 03:22:16  neerache
		Add FPS computation
		
		Revision 1.14  2003/05/05 23:50:40  neerache
		Tweak appearance, add border, add animations
		
		Revision 1.13  2003/05/02 01:16:55  neerache
		Antialias squares, experiment with translucent board
		
		Revision 1.12  2003/04/28 22:19:29  neerache
		Eliminate drawBoardPlane; simplify background; move labels closer to board; vary square textures; properly rotate selected knights; reorder elements to be drawn
		
		Revision 1.11  2003/04/24 23:20:35  neeri
		Support pawn promotions
		
		Revision 1.10  2003/04/10 23:03:16  neeri
		Load positions
		
		Revision 1.9  2003/04/02 18:44:01  neeri
		Tweak perspective
		
		Revision 1.8  2003/03/28 01:31:07  neeri
		Support hints, last move
		
		Revision 1.7  2002/12/04 02:30:50  neeri
		Experiment (unsuccessfully so far) with ways to speed up piece movement
		
		Revision 1.6  2002/10/15 22:49:39  neeri
		Add support for texture styles
		
		Revision 1.5  2002/10/08 22:59:11  neeri
		Refine drawing, support flipped board
		
		Revision 1.4  2002/09/13 23:57:05  neeri
		Support for Crazyhouse display and mouse
		
		Revision 1.3  2002/09/12 17:46:46  neeri
		Introduce dual board representation, in-hand pieces
		
		Revision 1.2  2002/08/26 23:14:08  neeri
		Switched to Azimuth/Elevation model; fixed lighting issue manifesting with white knights
		
		Revision 1.1  2002/08/22 23:47:06  neeri
		Initial Checkin
		
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
	fDiffuse	= 1.0f;
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
		{fDiffuse, fDiffuse, fDiffuse, fAlpha*alpha};
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
	NSRect bounds = [self bounds];
	fIsFloating			= ![[self window] styleMask];
	if (!fIsFloating) {
		//
		// Regular window, draw background (grey in window, black in full screen)
		//
		const float		kAspect					= bounds.size.width / bounds.size.height;
		const float		kScreenAspect			= 4.0 / 3.0;
		const float		kDefaultAspect			= 740.0 / 680.0;
		const float		kWindowBrightness		= 0.6f;
		const float		kFullScreenBrightness	= 0.0f;
		float			kBrightness				= std::max(kWindowBrightness +
			((kAspect-kDefaultAspect) / (kScreenAspect-kDefaultAspect))
		  * (kFullScreenBrightness-kWindowBrightness), kFullScreenBrightness);

		glClearColor(kBrightness, kBrightness, kBrightness, 1.0);
	} else {
		//
		// Floating window, transparent background
		//
		GLint opaque = NO;
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

	const float kDistance 		= 300.0f;
	const float kBoardSize 		= fVariant==kVarCrazyhouse ? 55.0f : 50.0f;
	const float kDeg2Rad  		= M_PI / 180.0f;
	const float kRad2Deg		= 180.0f / M_PI;
	const float kAngleOfView	= 2.0f * atan2(kBoardSize, kDistance) * kRad2Deg;

	glViewport(0, 0, (GLsizei)(bounds.size.width), (GLsizei)(bounds.size.height));

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

    gluPerspective(kAngleOfView, bounds.size.width / bounds.size.height, 
				   10.0, 1000.0); 

	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glScalef(1.0f, -1.0f, 0.0f);
	
	glMatrixMode(GL_MODELVIEW);

	float	cameraY = kDistance * sin(fElevation * kDeg2Rad);
	float   cameraXZ= kDistance * cos(fElevation * kDeg2Rad);
	float	cameraX = cameraXZ  * sin(fAzimuth * kDeg2Rad);
	float	cameraZ = cameraXZ  *-cos(fAzimuth * kDeg2Rad);

	gluLookAt(cameraX, cameraY, cameraZ,
			  0.0, 0.0, 0.0, 0.0, 1.0, 0.0);

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
	const float kNHOff	= 1.00f;
	const float kNVOff	= 3.25f;
	const float kLHOff	= 3.25f;
	const float kLVOff	= 1.00f;

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
	const float kDiffuse = 0.75;
	GLfloat l_diffuse[4] = { kDiffuse, kDiffuse, kDiffuse, 1.0 };
	GLfloat l_ambient[4] = { fAmbient, fAmbient, fAmbient, 0.0 };

	glEnable(GL_LIGHT0);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, l_diffuse);
	glLightfv(GL_LIGHT0, GL_AMBIENT, l_ambient);
	glLightfv(GL_LIGHT0, GL_SPECULAR, l_diffuse);
	glLightfv(GL_LIGHT0, GL_POSITION, fLightPos);

	if (fPickedSquare != kInvalidSquare) {
		GLfloat spot_color[4]		= { 1.0, 1.0, 1.0, 1.0 };
		GLfloat spot_pos[4] 		= { 0.0, 100.0, 0.0, 1.0};
		GLfloat spot_direction[3]   = { 0.0, -1.0, 0.0 };

		MBCPosition pickedPos = [self squareToPosition:fPickedSquare];

		spot_pos[0]	= pickedPos[0];
		spot_pos[2] = pickedPos[2];

		glEnable(GL_LIGHT1);
		glLightfv(GL_LIGHT1, GL_DIFFUSE, spot_color);
		glLightfv(GL_LIGHT1, GL_SPECULAR, spot_color);
		glLightfv(GL_LIGHT1, GL_POSITION, spot_pos);
		glLightfv(GL_LIGHT1, GL_SPOT_DIRECTION, spot_direction);
		glLighti(GL_LIGHT1, GL_SPOT_EXPONENT, 100);
		glLighti(GL_LIGHT1, GL_SPOT_CUTOFF, 5);
		glLightf(GL_LIGHT1, GL_CONSTANT_ATTENUATION, 0.0f);
		glLightf(GL_LIGHT1, GL_QUADRATIC_ATTENUATION, 0.0001f);
	} else 
		glDisable(GL_LIGHT1);
}

MBCPieceCode gInHandOrder[] = {PAWN, BISHOP, KNIGHT, ROOK, QUEEN};

- (void) drawPiecesInHand
{
	const float kLabelX 	=  42.0f;
	const float kLabelSize	=  4.0f;
	const float	kLabelLeft	=  kLabelX;
	const float kLabelRight	=  kLabelX+kLabelSize;
	const float kSpacing	=  kInHandPieceSize;
	const float kLabelZ		=  4.0f;
	const float kPieceX		=  kInHandPieceX;
	const float kPieceZ		=  kInHandPieceZOffset+kInHandPieceSize/2.0f;
	const float kScale		=  0.95f;
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
	const float	kUserSpaceScale	= [[self superview] convertSizeToBacking:NSMakeSize(1, 1)].width;
	const float	kScale			= kUserSpaceScale;
	const float	kCircleSize		= 10.0f*kScale;
	const float	kArrowClearance	= 15.0f*kScale;
	const float	kArrowLength	= 30.0f*kScale;
	const float kArrowWidth		= 10.0f*kScale;
	const float	kThreshold		= 10.0f*kScale;
	const float kWellSize		= 55.0f*kScale;
	const float kWellRound		= 20.0f*kScale;

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
	gluDisk(q, 0.0, kCircleSize, 40, 1);
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

	//
	// Work around OpenGL driver issue
	//
#ifdef __LP64__
	glFlush();
#endif

	[[self openGLContext] flushBuffer];
}

@end

// Local Variables:
// mode:ObjC
// End:
