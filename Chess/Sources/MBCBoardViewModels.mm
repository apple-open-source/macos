/*
	File:		MBCBoardViewModels.mm
	Contains:	Define OpenGL models for chess pieces
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

#import "MBCBoardViewModels.h"

#import <math.h>
#import <OpenGL/gl.h>
#import <OpenGL/glu.h>

#undef POLY_STATISTICS

#ifdef POLY_STATISTICS
static int sPolyCount;
#define POLY_STAT_BEGIN()	sPolyCount = 0
#define POLY_STAT_END(id)	\
            fprintf(stderr, "Poly count for %s: %d\n", id, sPolyCount)
#define POLY_STAT(count)    sPolyCount += count
#else
#define POLY_STAT_BEGIN()	
#define POLY_STAT_END(id)	
#define POLY_STAT(count)    
#endif
#define POLY_STAT_QUAD()		POLY_STAT(2)
#define POLY_STAT_TRIANGLE()	POLY_STAT(1)

const float kPieceSize = 1.0f;

/* Revolutions start in the positive z-axis (towards camera) and go
 * anti-clockwise */

/* Make a revolved piece */
int revolve_line(float *trace_r, float *trace_h, float max_iheight)
{
	const int	nsteps = 16;
	const float kTexScale	= 10.0f;
	GLUquadricObj * q 		= gluNewQuadric();
	gluQuadricNormals(q, GLU_SMOOTH);
	gluQuadricTexture(q, true);
	glPushMatrix();
	glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
	while (trace_r[1] != 0.0f || trace_h[1] != 0.0f) {
		float	dh = trace_h[1]-trace_h[0];
		if (dh == 0.0f) {
			if (trace_r[1] > trace_r[0]) {
				gluQuadricOrientation(q, GLU_INSIDE);
				gluDisk(q, trace_r[0], trace_r[1], nsteps, 1);
				gluQuadricOrientation(q, GLU_OUTSIDE);
			} else {
				gluDisk(q, trace_r[1], trace_r[0], nsteps, 1);
			}
			POLY_STAT(nsteps);
		} else {
			glMatrixMode(GL_TEXTURE);
			glPushMatrix();
			glScalef(1.0f, fabs(dh) / kTexScale, 1.0f);
			glMatrixMode(GL_MODELVIEW);
			if (dh < 0.0f) {
				gluQuadricOrientation(q, GLU_INSIDE);
				glTranslatef(0.0f, 0.0f, dh);
				gluCylinder(q, trace_r[1], trace_r[0], -dh, nsteps, 1);
				gluQuadricOrientation(q, GLU_OUTSIDE);
			} else {
				gluCylinder(q, trace_r[0], trace_r[1], dh, nsteps, 1);
				glTranslatef(0.0f, 0.0f, dh);
			}
			POLY_STAT(2*nsteps);
			glMatrixMode(GL_TEXTURE);
			glPopMatrix();
			glMatrixMode(GL_MODELVIEW);
		}
		++trace_h;
		++trace_r;
	}
	glPopMatrix();
	gluDeleteQuadric(q);

	return 1;
}

void draw_pawn(void)
{
	float trace_r[] =
		{ 3.5f, 3.5f, 2.5f, 2.5f, 1.5f, 1.0f, 1.8f, 1.0f, 2.0f, 1.0f, 0.0f,
		  0.0f
		};
	float trace_h[] =
		{ 0.0f, 2.0f, 3.0f, 4.0f, 6.0f, 8.8f, 8.8f, 9.2f, 11.6f, 13.4f,
		  13.4f, 0.0f
		};

	POLY_STAT_BEGIN();
	revolve_line(trace_r, trace_h, 0.0f);
	POLY_STAT_END("pawn");
}

void draw_rook(void)
{
	float trace_r[] =
		{ 3.8f, 3.8f, 2.6f, 2.0f, 2.8f, 2.8f, 2.2f, 2.2f, 0.0f, 0.0f };
	float trace_h[] =
		{ 0.0f, 2.0f, 5.0f, 10.2f, 10.2f, 13.6f, 13.6f, 13.0f, 13.0f,
		  0.0f
		};

	POLY_STAT_BEGIN();
	revolve_line(trace_r, trace_h, 0.0f);
	POLY_STAT_END("rook");
}

void draw_knight(void)
{
	float trace_r[] = { 4.1f, 4.1f, 2.0f, 2.0f, 2.6f, 0.0f };
	float trace_h[] = { 0.0f, 2.0f, 3.6f, 4.8f, 5.8f, 0.0f };

	POLY_STAT_BEGIN();
	/* Revolved base */
	revolve_line(trace_r, trace_h, 17.8f);

	/* Non revolved pieces */
	/* Quads */
	glBegin(GL_QUADS);

	/* Square base */
	glNormal3f(0.0, -1.0, 0.0);
	glTexCoord2f(0.0f * kPieceSize, 5.8f / 17.8f * kPieceSize);
	glVertex3f(2.6 * kPieceSize, 5.8 * kPieceSize, 2.6 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 5.8f / 17.8f * kPieceSize);
	glVertex3f(-2.6 * kPieceSize, 5.8 * kPieceSize, 2.6 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 5.8f / 17.8f * kPieceSize);
	glVertex3f(-2.6 * kPieceSize, 5.8 * kPieceSize, -0.8 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 5.8f / 17.8f * kPieceSize);
	glVertex3f(2.6 * kPieceSize, 5.8 * kPieceSize, -0.8 * kPieceSize);
	POLY_STAT_QUAD();

	/* Upper edge of nose */
	glNormal3f(0.0, 0.707107, 0.707107);
	glTexCoord2f(0.0f * kPieceSize, 16.2f / 17.8f * kPieceSize);
	glVertex3f(0.8 * kPieceSize, 16.2 * kPieceSize, 4.0 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(1.0 * kPieceSize, 16.8 * kPieceSize, 3.4 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(-1.0 * kPieceSize, 16.8 * kPieceSize, 3.4 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 16.2f / 17.8f * kPieceSize);
	glVertex3f(-0.8 * kPieceSize, 16.2 * kPieceSize, 4.0 * kPieceSize);
	POLY_STAT_QUAD();

	/* Above head */
	glNormal3f(0.0, 1.0, 0.0);
	glTexCoord2f(0.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(1.0 * kPieceSize, 16.8 * kPieceSize, 3.4 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(1.0 * kPieceSize, 16.8 * kPieceSize, 3.0 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(-1.0 * kPieceSize, 16.8 * kPieceSize, 3.0 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(-1.0 * kPieceSize, 16.8 * kPieceSize, 3.4 * kPieceSize);
	POLY_STAT_QUAD();

	glTexCoord2f(0.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(1.0 * kPieceSize, 16.8 * kPieceSize, 3.0 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(0.5 * kPieceSize, 16.8 * kPieceSize, 1.6 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(-0.5 * kPieceSize, 16.8 * kPieceSize, 1.6 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(-1.0 * kPieceSize, 16.8 * kPieceSize, 3.0 * kPieceSize);
	POLY_STAT_QUAD();

	glTexCoord2f(0.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(0.5 * kPieceSize, 16.8 * kPieceSize, 1.6 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(1.0 * kPieceSize, 16.8 * kPieceSize, 0.2 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(-1.0 * kPieceSize, 16.8 * kPieceSize, 0.2 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(-0.5 * kPieceSize, 16.8 * kPieceSize, 1.6 * kPieceSize);
	POLY_STAT_QUAD();

	glTexCoord2f(0.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(1.0 * kPieceSize, 16.8 * kPieceSize, 0.2 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(1.0 * kPieceSize, 16.8 * kPieceSize, -0.2 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(-1.0 * kPieceSize, 16.8 * kPieceSize, -0.2 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(-1.0 * kPieceSize, 16.8 * kPieceSize, 0.2 * kPieceSize);
	POLY_STAT_QUAD();

	glTexCoord2f(0.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(1.0 * kPieceSize, 16.8 * kPieceSize, -0.2 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(0.4 * kPieceSize, 16.8 * kPieceSize, -1.1 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(-0.4 * kPieceSize, 16.8 * kPieceSize, -1.1 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(-1.0 * kPieceSize, 16.8 * kPieceSize, -0.2 * kPieceSize);
	POLY_STAT_QUAD();

	glTexCoord2f(0.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(0.4 * kPieceSize, 16.8 * kPieceSize, -1.1 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(1.0 * kPieceSize, 16.8 * kPieceSize, -2.0 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(-1.0 * kPieceSize, 16.8 * kPieceSize, -2.0 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(-0.4 * kPieceSize, 16.8 * kPieceSize, -1.1 * kPieceSize);
	POLY_STAT_QUAD();

	glTexCoord2f(0.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(1.0 * kPieceSize, 16.8 * kPieceSize, -2.0 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(1.0 * kPieceSize, 16.8 * kPieceSize, -4.4 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(-1.0 * kPieceSize, 16.8 * kPieceSize, -4.4 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(-1.0 * kPieceSize, 16.8 * kPieceSize, -2.0 * kPieceSize);
	POLY_STAT_QUAD();

	/* Back of head */
	glNormal3f(0.0, 0.0, -1.0);
	glTexCoord2f(0.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(-1.0 * kPieceSize, 16.8 * kPieceSize, -4.4 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(1.0 * kPieceSize, 16.8 * kPieceSize, -4.4 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 15.0f / 17.8f * kPieceSize);
	glVertex3f(1.0 * kPieceSize, 15.0 * kPieceSize, -4.4 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 15.0f / 17.8f * kPieceSize);
	glVertex3f(-1.0 * kPieceSize, 15.0 * kPieceSize, -4.4 * kPieceSize);
	POLY_STAT_QUAD();

	/* Under back */
	glNormal3f(0.0, 0.0, -1.0);
	glTexCoord2f(0.0f * kPieceSize, 15.0f / 17.8f * kPieceSize);
	glVertex3f(-1.0 * kPieceSize, 15.0 * kPieceSize, -4.4 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 15.0f / 17.8f * kPieceSize);
	glVertex3f(1.0 * kPieceSize, 15.0 * kPieceSize, -4.4 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 14.8f / 17.8f * kPieceSize);
	glVertex3f(0.55 * kPieceSize, 14.8 * kPieceSize, -2.8 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 14.8f / 17.8f * kPieceSize);
	glVertex3f(-0.55 * kPieceSize, 14.8 * kPieceSize, -2.8 * kPieceSize);
	POLY_STAT_QUAD();

	/* Right side of face */
	glNormal3f(-0.933878, 0.128964, -0.333528);
	glTexCoord2f(0.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(-1.0 * kPieceSize, 16.8 * kPieceSize, 3.0 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(-0.5 * kPieceSize, 16.8 * kPieceSize, 1.6 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 14.0f / 17.8f * kPieceSize);
	glVertex3f(-1.0 * kPieceSize, 14.0 * kPieceSize, 1.3 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 13.8f / 17.8f * kPieceSize);
	glVertex3f(-1.2 * kPieceSize, 13.8 * kPieceSize, 2.4 * kPieceSize);
	POLY_STAT_QUAD();

	glNormal3f(-0.966676, 0.150427, 0.207145);
	glTexCoord2f(0.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(-0.5 * kPieceSize, 16.8 * kPieceSize, 1.6 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(-0.8 * kPieceSize, 16.8 * kPieceSize, 0.2 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 13.8f / 17.8f * kPieceSize);
	glVertex3f(-1.2 * kPieceSize, 13.8 * kPieceSize, 0.2 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 14.0f / 17.8f * kPieceSize);
	glVertex3f(-1.0 * kPieceSize, 14.0 * kPieceSize, 1.3 * kPieceSize);
	POLY_STAT_QUAD();

	/* (above and below eye) */
	glNormal3f(-0.934057, 0.124541, -0.334704);
	glTexCoord2f(0.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(-0.82666667 * kPieceSize, 16.6 * kPieceSize,
			   0.2 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(-0.8 * kPieceSize, 16.8 * kPieceSize, 0.2 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(-1.0 * kPieceSize, 16.8 * kPieceSize, -0.2 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 16.6f / 17.8f * kPieceSize);
	glVertex3f(-1.0 * kPieceSize, 16.6 * kPieceSize, -0.38 * kPieceSize);
	POLY_STAT_QUAD();

	glTexCoord2f(0.0f * kPieceSize, 13.8f / 17.8f * kPieceSize);
	glVertex3f(-1.2 * kPieceSize, 13.8 * kPieceSize, 0.2 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 16.2f / 17.8f * kPieceSize);
	glVertex3f(-0.88 * kPieceSize, 16.2 * kPieceSize, 0.2 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 16.2f / 17.8f * kPieceSize);
	glVertex3f(-1.0 * kPieceSize, 16.2 * kPieceSize, -0.74 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 13.6f / 17.8f * kPieceSize);
	glVertex3f(-1.2 * kPieceSize, 13.6 * kPieceSize, -0.2 * kPieceSize);
	POLY_STAT_QUAD();

	glTexCoord2f(0.0f * kPieceSize, 13.6f / 17.8f * kPieceSize);
	glVertex3f(-1.2 * kPieceSize, 13.6 * kPieceSize, -0.2 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 16.2f / 17.8f * kPieceSize);
	glVertex3f(-1.0 * kPieceSize, 16.2 * kPieceSize, -0.74 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 15.8f / 17.8f * kPieceSize);
	glVertex3f(-1.0 * kPieceSize, 15.8 * kPieceSize, -1.1 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 14.0f / 17.8f * kPieceSize);
	glVertex3f(-0.6 * kPieceSize, 14.0 * kPieceSize, -1.4 * kPieceSize);
	POLY_STAT_QUAD();

	glNormal3f(-0.970801, -0.191698, -0.144213);
	glTexCoord2f(0.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(-1.0 * kPieceSize, 16.8 * kPieceSize, -2.0 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 14.8f / 17.8f * kPieceSize);
	glVertex3f(-0.55 * kPieceSize, 14.8 * kPieceSize, -2.8 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 14.0f / 17.8f * kPieceSize);
	glVertex3f(-0.6 * kPieceSize, 14.0 * kPieceSize, -1.4 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 15.8f / 17.8f * kPieceSize);
	glVertex3f(-1.0 * kPieceSize, 15.8 * kPieceSize, -1.1 * kPieceSize);
	POLY_STAT_QUAD();

	glNormal3f(-0.975610, 0.219512, 0.0);
	glTexCoord2f(0.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(-1.0 * kPieceSize, 16.8 * kPieceSize, -2.0 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(-1.0 * kPieceSize, 16.8 * kPieceSize, -4.4 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 15.0f / 17.8f * kPieceSize);
	glVertex3f(-1.0 * kPieceSize, 15.0 * kPieceSize, -4.4 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 14.8f / 17.8f * kPieceSize);
	glVertex3f(-0.55 * kPieceSize, 14.8 * kPieceSize, -2.8 * kPieceSize);
	POLY_STAT_QUAD();

	/* Left side of face */
	glNormal3f(0.933878, 0.128964, -0.333528);
	glTexCoord2f(0.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(1.0 * kPieceSize, 16.8 * kPieceSize, 3.0 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 13.8f / 17.8f * kPieceSize);
	glVertex3f(1.2 * kPieceSize, 13.8 * kPieceSize, 2.4 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 14.0f / 17.8f * kPieceSize);
	glVertex3f(1.0 * kPieceSize, 14.0 * kPieceSize, 1.3 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(0.5 * kPieceSize, 16.8 * kPieceSize, 1.6 * kPieceSize);
	POLY_STAT_QUAD();

	glNormal3f(0.966676, 0.150427, 0.207145);
	glTexCoord2f(0.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(0.5 * kPieceSize, 16.8 * kPieceSize, 1.6 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 14.0f / 17.8f * kPieceSize);
	glVertex3f(1.0 * kPieceSize, 14.0 * kPieceSize, 1.3 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 13.8f / 17.8f * kPieceSize);
	glVertex3f(1.2 * kPieceSize, 13.8 * kPieceSize, 0.2 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(0.8 * kPieceSize, 16.8 * kPieceSize, 0.2 * kPieceSize);
	POLY_STAT_QUAD();

	/* (above and below eye) */
	glNormal3f(0.934057, 0.124541, -0.334704);
	glTexCoord2f(0.0f * kPieceSize, 16.6f / 17.8f * kPieceSize);
	glVertex3f(0.82666667 * kPieceSize, 16.6 * kPieceSize, 0.2 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 16.6f / 17.8f * kPieceSize);
	glVertex3f(1.0 * kPieceSize, 16.6 * kPieceSize, -0.38 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(1.0 * kPieceSize, 16.8 * kPieceSize, -0.2 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(0.8 * kPieceSize, 16.8 * kPieceSize, 0.2 * kPieceSize);
	POLY_STAT_QUAD();

	glTexCoord2f(0.0f * kPieceSize, 13.8f / 17.8f * kPieceSize);
	glVertex3f(1.2 * kPieceSize, 13.8 * kPieceSize, 0.2 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 13.6f / 17.8f * kPieceSize);
	glVertex3f(1.2 * kPieceSize, 13.6 * kPieceSize, -0.2 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 16.2f / 17.8f * kPieceSize);
	glVertex3f(1.0 * kPieceSize, 16.2 * kPieceSize, -0.74 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 16.2f / 17.8f * kPieceSize);
	glVertex3f(0.88 * kPieceSize, 16.2 * kPieceSize, 0.2 * kPieceSize);
	POLY_STAT_QUAD();

	glTexCoord2f(0.0f * kPieceSize, 13.6f / 17.8f * kPieceSize);
	glVertex3f(1.2 * kPieceSize, 13.6 * kPieceSize, -0.2 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 14.0f / 17.8f * kPieceSize);
	glVertex3f(0.6 * kPieceSize, 14.0 * kPieceSize, -1.4 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 15.8f / 17.8f * kPieceSize);
	glVertex3f(1.0 * kPieceSize, 15.8 * kPieceSize, -1.1 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 16.2f / 17.8f * kPieceSize);
	glVertex3f(1.0 * kPieceSize, 16.2 * kPieceSize, -0.74 * kPieceSize);
	POLY_STAT_QUAD();

	glNormal3f(0.970801, -0.191698, -0.144213);
	glTexCoord2f(0.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(1.0 * kPieceSize, 16.8 * kPieceSize, -2.0 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 15.8f / 17.8f * kPieceSize);
	glVertex3f(1.0 * kPieceSize, 15.8 * kPieceSize, -1.1 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 14.0f / 17.8f * kPieceSize);
	glVertex3f(0.6 * kPieceSize, 14.0 * kPieceSize, -1.4 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 14.8f / 17.8f * kPieceSize);
	glVertex3f(0.55 * kPieceSize, 14.8 * kPieceSize, -2.8 * kPieceSize);
	POLY_STAT_QUAD();

	glNormal3f(0.975610, -0.219512, 0.0);
	glTexCoord2f(0.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(1.0 * kPieceSize, 16.8 * kPieceSize, -2.0 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 14.8f / 17.8f * kPieceSize);
	glVertex3f(0.55 * kPieceSize, 14.8 * kPieceSize, -2.8 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 15.0f / 17.8f * kPieceSize);
	glVertex3f(1.0 * kPieceSize, 15.0 * kPieceSize, -4.4 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(1.0 * kPieceSize, 16.8 * kPieceSize, -4.4 * kPieceSize);
	POLY_STAT_QUAD();

	/* Eyes */
	glNormal3f(0.598246, 0.797665, 0.076372);
	glTexCoord2f(0.0f * kPieceSize, 16.2f / 17.8f * kPieceSize);
	glVertex3f(0.88 * kPieceSize, 16.2 * kPieceSize, 0.2 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 16.2f / 17.8f * kPieceSize);
	glVertex3f(1.0 * kPieceSize, 16.2 * kPieceSize, -0.74 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 16.4f / 17.8f * kPieceSize);
	glVertex3f(0.8 * kPieceSize, 16.4 * kPieceSize, -0.56 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 16.4f / 17.8f * kPieceSize);
	glVertex3f(0.61333334 * kPieceSize, 16.4 * kPieceSize, 0.2 * kPieceSize);
	POLY_STAT_QUAD();

	glNormal3f(0.670088, -0.714758, 0.200256);
	glTexCoord2f(0.0f * kPieceSize, 16.4f / 17.8f * kPieceSize);
	glVertex3f(0.61333334 * kPieceSize, 16.4 * kPieceSize, 0.2 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 16.4f / 17.8f * kPieceSize);
	glVertex3f(0.8 * kPieceSize, 16.4 * kPieceSize, -0.56 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 16.6f / 17.8f * kPieceSize);
	glVertex3f(1.0 * kPieceSize, 16.6 * kPieceSize, -0.38 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 16.6f / 17.8f * kPieceSize);
	glVertex3f(0.82666667 * kPieceSize, 16.6 * kPieceSize, 0.2 * kPieceSize);
	POLY_STAT_QUAD();

	glNormal3f(-0.598246, 0.797665, 0.076372);
	glTexCoord2f(0.0f * kPieceSize, 16.2f / 17.8f * kPieceSize);
	glVertex3f(-0.88 * kPieceSize, 16.2 * kPieceSize, 0.2 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 16.4f / 17.8f * kPieceSize);
	glVertex3f(-0.61333334 * kPieceSize, 16.4 * kPieceSize,
			   0.2 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 16.4f / 17.8f * kPieceSize);
	glVertex3f(-0.8 * kPieceSize, 16.4 * kPieceSize, -0.56 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 16.2f / 17.8f * kPieceSize);
	glVertex3f(-1.0 * kPieceSize, 16.2 * kPieceSize, -0.74 * kPieceSize);
	POLY_STAT_QUAD();

	glNormal3f(-0.670088, -0.714758, 0.200256);
	glTexCoord2f(0.0f * kPieceSize, 16.4f / 17.8f * kPieceSize);
	glVertex3f(-0.61333334 * kPieceSize, 16.4 * kPieceSize,
			   0.2 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 16.6f / 17.8f * kPieceSize);
	glVertex3f(-0.82666667 * kPieceSize, 16.6 * kPieceSize,
			   0.2 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 16.6f / 17.8f * kPieceSize);
	glVertex3f(-1.0 * kPieceSize, 16.6 * kPieceSize, -0.38 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 16.4f / 17.8f * kPieceSize);
	glVertex3f(-0.8 * kPieceSize, 16.4 * kPieceSize, -0.56 * kPieceSize);
	POLY_STAT_QUAD();

	/* Hair */
	glNormal3f(0.0, 1.0, 0.0);
	glTexCoord2f(0.0f * kPieceSize, 17.8f / 17.8f * kPieceSize);
	glVertex3f(0.35 * kPieceSize, 17.8 * kPieceSize, -0.8 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 17.8f / 17.8f * kPieceSize);
	glVertex3f(0.35 * kPieceSize, 17.8 * kPieceSize, -4.4 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 17.8f / 17.8f * kPieceSize);
	glVertex3f(-0.35 * kPieceSize, 17.8 * kPieceSize, -4.4 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 17.8f / 17.8f * kPieceSize);
	glVertex3f(-0.35 * kPieceSize, 17.8 * kPieceSize, -0.8 * kPieceSize);
	POLY_STAT_QUAD();

	glNormal3f(1.0, 0.0, 0.0);
	glTexCoord2f(0.0f * kPieceSize, 17.8f / 17.8f * kPieceSize);
	glVertex3f(0.35 * kPieceSize, 17.8 * kPieceSize, -0.8 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(0.35 * kPieceSize, 16.8 * kPieceSize, -0.8 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(0.35 * kPieceSize, 16.8 * kPieceSize, -4.4 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 17.8f / 17.8f * kPieceSize);
	glVertex3f(0.35 * kPieceSize, 17.8 * kPieceSize, -4.4 * kPieceSize);
	POLY_STAT_QUAD();

	glNormal3f(-1.0, 0.0, 0.0);
	glTexCoord2f(0.0f * kPieceSize, 17.8f / 17.8f * kPieceSize);
	glVertex3f(-0.35 * kPieceSize, 17.8 * kPieceSize, -0.8 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 17.8f / 17.8f * kPieceSize);
	glVertex3f(-0.35 * kPieceSize, 17.8 * kPieceSize, -4.4 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(-0.35 * kPieceSize, 16.8 * kPieceSize, -4.4 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(-0.35 * kPieceSize, 16.8 * kPieceSize, -0.8 * kPieceSize);
	POLY_STAT_QUAD();

	glNormal3f(0.0, 0.0, 1.0);
	glTexCoord2f(0.0f * kPieceSize, 17.8f / 17.8f * kPieceSize);
	glVertex3f(0.35 * kPieceSize, 17.8 * kPieceSize, -0.8 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 17.8f / 17.8f * kPieceSize);
	glVertex3f(-0.35 * kPieceSize, 17.8 * kPieceSize, -0.8 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(-0.35 * kPieceSize, 16.8 * kPieceSize, -0.8 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(0.35 * kPieceSize, 16.8 * kPieceSize, -0.8 * kPieceSize);
	POLY_STAT_QUAD();

	glNormal3f(0.0, 0.0, -1.0);
	glTexCoord2f(0.0f * kPieceSize, 17.8f / 17.8f * kPieceSize);
	glVertex3f(0.35 * kPieceSize, 17.8 * kPieceSize, -4.4 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(0.35 * kPieceSize, 16.8 * kPieceSize, -4.4 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(-0.35 * kPieceSize, 16.8 * kPieceSize, -4.4 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 17.8f / 17.8f * kPieceSize);
	glVertex3f(-0.35 * kPieceSize, 17.8 * kPieceSize, -4.4 * kPieceSize);
	POLY_STAT_QUAD();

	/* Under chin */
	glNormal3f(0.0, -0.853282, 0.521450);
	glTexCoord2f(0.0f * kPieceSize, 14.0f / 17.8f * kPieceSize);
	glVertex3f(-1.0 * kPieceSize, 14.0 * kPieceSize, 1.3 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 13.8f / 17.8f * kPieceSize);
	glVertex3f(-1.2 * kPieceSize, 13.8 * kPieceSize, 0.2 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 13.8f / 17.8f * kPieceSize);
	glVertex3f(1.2 * kPieceSize, 13.8 * kPieceSize, 0.2 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 14.0f / 17.8f * kPieceSize);
	glVertex3f(1.0 * kPieceSize, 14.0 * kPieceSize, 1.3 * kPieceSize);
	POLY_STAT_QUAD();

	glNormal3f(0.0, -0.983870, -0.178885);
	glTexCoord2f(0.0f * kPieceSize, 14.0f / 17.8f * kPieceSize);
	glVertex3f(-1.0 * kPieceSize, 14.0 * kPieceSize, 1.3 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 14.0f / 17.8f * kPieceSize);
	glVertex3f(1.0 * kPieceSize, 14.0 * kPieceSize, 1.3 * kPieceSize);
	glTexCoord2f(1.0f * kPieceSize, 13.8f / 17.8f * kPieceSize);
	glVertex3f(1.2 * kPieceSize, 13.8 * kPieceSize, 2.4 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 13.8f / 17.8f * kPieceSize);
	glVertex3f(-1.2 * kPieceSize, 13.8 * kPieceSize, 2.4 * kPieceSize);
	POLY_STAT_QUAD();

	/* Mane */

	const float kVTex = 2.0f;

	/* Right */
	glNormal3f(-0.788443, 0.043237, -0.613587);
	glTexCoord2f(0.0f, kVTex * 5.8f / 17.8f);
	glVertex3f(-2.6 * kPieceSize, 5.8 * kPieceSize, -0.8 * kPieceSize);
	glTexCoord2f(0.0f, kVTex * 14.8f / 17.8f);
	glVertex3f(-0.55 * kPieceSize, 14.8 * kPieceSize, -2.8 * kPieceSize);
	glTexCoord2f(1.0f, kVTex * 15.0f / 17.8f);
	glVertex3f(0.0 * kPieceSize, 15.0 * kPieceSize, -3.6 * kPieceSize);
	glTexCoord2f(1.0f, kVTex * 7.8f / 17.8f);
	glVertex3f(0.0 * kPieceSize, 7.8 * kPieceSize, -4.0 * kPieceSize);
	POLY_STAT_QUAD();

	/* Left */
	glNormal3f(0.788443, 0.043237, -0.613587);
	glTexCoord2f(0.0f, kVTex * 5.8f / 17.8f);
	glVertex3f(2.6 * kPieceSize, 5.8 * kPieceSize, -0.8 * kPieceSize);
	glTexCoord2f(1.0f, kVTex * 7.8f / 17.8f);
	glVertex3f(0.0 * kPieceSize, 7.8 * kPieceSize, -4.0 * kPieceSize);
	glTexCoord2f(1.0f, kVTex * 15.0f / 17.8f);
	glVertex3f(0.0 * kPieceSize, 15.0 * kPieceSize, -3.6 * kPieceSize);
	glTexCoord2f(0.0f, kVTex * 14.8f / 17.8f);
	glVertex3f(0.55 * kPieceSize, 14.8 * kPieceSize, -2.8 * kPieceSize);
	POLY_STAT_QUAD();

	/* Chest */
	/* Front */
	glNormal3f(0.0, 0.584305, 0.811534);
	glTexCoord2f(0.0f, kVTex * 13.8f / 17.8f);
	glVertex3f(-0.5 * kPieceSize, 13.8 * kPieceSize, 0.4 * kPieceSize);
	glTexCoord2f(0.0f, kVTex * 8.8f / 17.8f);
	glVertex3f(-2.0 * kPieceSize, 8.8 * kPieceSize, 4.0 * kPieceSize);
	glTexCoord2f(1.0f, kVTex * 8.8f / 17.8f);
	glVertex3f(2.0 * kPieceSize, 8.8 * kPieceSize, 4.0 * kPieceSize);
	glTexCoord2f(1.0f, kVTex * 13.8f / 17.8f);
	glVertex3f(0.5 * kPieceSize, 13.8 * kPieceSize, 0.4 * kPieceSize);
	POLY_STAT_QUAD();

	/* Bottom */
	glNormal3f(0.0, -0.422886, 0.906183);
	glTexCoord2f(0.0f, kVTex * 8.8f / 17.8f);
	glVertex3f(-2.0 * kPieceSize, 8.8 * kPieceSize, 4.0 * kPieceSize);
	glTexCoord2f(0.0f, kVTex * 5.8f / 17.8f);
	glVertex3f(-2.6 * kPieceSize, 5.8 * kPieceSize, 2.6 * kPieceSize);
	glTexCoord2f(1.0f, kVTex * 5.8f / 17.8f);
	glVertex3f(2.6 * kPieceSize, 5.8 * kPieceSize, 2.6 * kPieceSize);
	glTexCoord2f(1.0f, kVTex * 8.8f / 17.8f);
	glVertex3f(2.0 * kPieceSize, 8.8 * kPieceSize, 4.0 * kPieceSize);
	POLY_STAT_QUAD();

	/* Right */
	glNormal3f(-0.969286, 0.231975, -0.081681);
	glTexCoord2f(0.0f, kVTex * 13.8f / 17.8f);
	glVertex3f(-0.5 * kPieceSize, 13.8 * kPieceSize, 0.4 * kPieceSize);
	glTexCoord2f(1.0f, kVTex * 12.2f / 17.8f);
	glVertex3f(-1.4 * kPieceSize, 12.2 * kPieceSize, -0.4 * kPieceSize);
	glTexCoord2f(1.0f, kVTex * 5.8f / 17.8f);
	glVertex3f(-2.6 * kPieceSize, 5.8 * kPieceSize, 2.6 * kPieceSize);
	glTexCoord2f(0.0f, kVTex * 8.8f / 17.8f);
	glVertex3f(-2.0 * kPieceSize, 8.8 * kPieceSize, 4.0 * kPieceSize);
	POLY_STAT_QUAD();

	glNormal3f(-0.982872, 0.184289, 0.0);
	glTexCoord2f(0.0f, kVTex * 12.2f / 17.8f);
	glVertex3f(-1.4 * kPieceSize, 12.2 * kPieceSize, -0.4 * kPieceSize);
	glTexCoord2f(1.0f, kVTex * 12.2f / 17.8f);
	glVertex3f(-1.1422222222 * kPieceSize, 12.2 * kPieceSize,
			   -2.2222222222 * kPieceSize);
	glTexCoord2f(1.0f, kVTex * 5.8f / 17.8f);
	glVertex3f(-2.6 * kPieceSize, 5.8 * kPieceSize, -0.8 * kPieceSize);
	glTexCoord2f(0.0f, kVTex * 5.8f / 17.8f);
	glVertex3f(-2.6 * kPieceSize, 5.8 * kPieceSize, 2.6 * kPieceSize);
	POLY_STAT_QUAD();

	glTexCoord2f(0.0f, kVTex * 14.8f / 17.8f);
	glVertex3f(-0.55 * kPieceSize, 14.8 * kPieceSize, -2.8 * kPieceSize);
	glTexCoord2f(0.0f, kVTex * 12.2f / 17.8f);
	glVertex3f(-1.1422222222 * kPieceSize, 12.2 * kPieceSize,
			   -2.2222222222 * kPieceSize);
	glTexCoord2f(1.0f, kVTex * 12.2f / 17.8f);
	glVertex3f(-1.4 * kPieceSize, 12.2 * kPieceSize, -0.4 * kPieceSize);
	glTexCoord2f(1.0f, kVTex * 14.0f / 17.8f);
	glVertex3f(-0.6 * kPieceSize, 14.0 * kPieceSize, -1.4 * kPieceSize);
	POLY_STAT_QUAD();

	/* Left */
	glNormal3f(0.969286, 0.231975, -0.081681);
	glTexCoord2f(0.0f, kVTex * 13.8f / 17.8f);
	glVertex3f(0.5 * kPieceSize, 13.8 * kPieceSize, 0.4 * kPieceSize);
	glTexCoord2f(0.0f, kVTex * 8.8f / 17.8f);
	glVertex3f(2.0 * kPieceSize, 8.8 * kPieceSize, 4.0 * kPieceSize);
	glTexCoord2f(1.0f, kVTex * 5.8f / 17.8f);
	glVertex3f(2.6 * kPieceSize, 5.8 * kPieceSize, 2.6 * kPieceSize);
	glTexCoord2f(1.0f, kVTex * 12.2f / 17.8f);
	glVertex3f(1.4 * kPieceSize, 12.2 * kPieceSize, -0.4 * kPieceSize);
	POLY_STAT_QUAD();

	glNormal3f(0.982872, 0.184289, 0.0);
	glTexCoord2f(0.0f, kVTex * 12.2f / 17.8f);
	glVertex3f(1.4 * kPieceSize, 12.2 * kPieceSize, -0.4 * kPieceSize);
	glTexCoord2f(0.0f, kVTex * 5.8f / 17.8f);
	glVertex3f(2.6 * kPieceSize, 5.8 * kPieceSize, 2.6 * kPieceSize);
	glTexCoord2f(1.0f, kVTex * 5.8f / 17.8f);
	glVertex3f(2.6 * kPieceSize, 5.8 * kPieceSize, -0.8 * kPieceSize);
	glTexCoord2f(1.0f, kVTex * 12.2f / 17.8f);
	glVertex3f(1.1422222222 * kPieceSize, 12.2 * kPieceSize,
			   -2.2222222222 * kPieceSize);
	POLY_STAT_QUAD();

	glTexCoord2f(0.0f, kVTex * 14.8f / 17.8f);
	glVertex3f(0.55 * kPieceSize, 14.8 * kPieceSize, -2.8 * kPieceSize);
	glTexCoord2f(1.0f, kVTex * 14.0f / 17.8f);
	glVertex3f(0.6 * kPieceSize, 14.0 * kPieceSize, -1.4 * kPieceSize);
	glTexCoord2f(1.0f, kVTex * 12.2f / 17.8f);
	glVertex3f(1.4 * kPieceSize, 12.2 * kPieceSize, -0.4 * kPieceSize);
	glTexCoord2f(0.0f, kVTex * 12.2f / 17.8f);
	glVertex3f(1.1422222222 * kPieceSize, 12.2 * kPieceSize,
			   -2.2222222222 * kPieceSize);
	POLY_STAT_QUAD();
	glEnd();

	/* Triangles */
	glBegin(GL_TRIANGLES);

	/* Under mane */
	glNormal3f(0.819890, -0.220459, -0.528373);
	glTexCoord2f(0.0f * kPieceSize, 5.8f / 17.8f * kPieceSize);
	glVertex3f(2.6 * kPieceSize, 5.8 * kPieceSize, -0.8 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 5.8f / 17.8f * kPieceSize);
	glVertex3f(1.44 * kPieceSize, 5.8 * kPieceSize, -2.6 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 7.8f / 17.8f * kPieceSize);
	glVertex3f(0.0 * kPieceSize, 7.8 * kPieceSize, -4.0 * kPieceSize);
	POLY_STAT_TRIANGLE();

	glNormal3f(0.0, -0.573462, -0.819232);
	glTexCoord2f(0.0f * kPieceSize, 5.8f / 17.8f * kPieceSize);
	glVertex3f(1.44 * kPieceSize, 5.8 * kPieceSize, -2.6 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 5.8f / 17.8f * kPieceSize);
	glVertex3f(-1.44 * kPieceSize, 5.8 * kPieceSize, -2.6 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 7.8f / 17.8f * kPieceSize);
	glVertex3f(0.0 * kPieceSize, 7.8 * kPieceSize, -4.0 * kPieceSize);
	POLY_STAT_TRIANGLE();

	glNormal3f(-0.819890, -0.220459, -0.528373);
	glTexCoord2f(0.0f * kPieceSize, 5.8f / 17.8f * kPieceSize);
	glVertex3f(-2.6 * kPieceSize, 5.8 * kPieceSize, -0.8 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 7.8f / 17.8f * kPieceSize);
	glVertex3f(0.0 * kPieceSize, 7.8 * kPieceSize, -4.0 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 5.8f / 17.8f * kPieceSize);
	glVertex3f(-1.44 * kPieceSize, 5.8 * kPieceSize, -2.6 * kPieceSize);
	POLY_STAT_TRIANGLE();

	/* Nose tip */
	glNormal3f(0.0, 0.0, 1.0);
	glTexCoord2f(0.0f * kPieceSize, 14.0f / 17.8f * kPieceSize);
	glVertex3f(0.0 * kPieceSize, 14.0 * kPieceSize, 4.0 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 16.2f / 17.8f * kPieceSize);
	glVertex3f(0.8 * kPieceSize, 16.2 * kPieceSize, 4.0 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 16.2f / 17.8f * kPieceSize);
	glVertex3f(-0.8 * kPieceSize, 16.2 * kPieceSize, 4.0 * kPieceSize);
	POLY_STAT_TRIANGLE();

	/* Mouth left */
	glNormal3f(-0.752714, -0.273714, 0.598750);
	glTexCoord2f(0.0f * kPieceSize, 14.0f / 17.8f * kPieceSize);
	glVertex3f(0.0 * kPieceSize, 14.0 * kPieceSize, 4.0 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 16.2f / 17.8f * kPieceSize);
	glVertex3f(-0.8 * kPieceSize, 16.2 * kPieceSize, 4.0 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 13.8f / 17.8f * kPieceSize);
	glVertex3f(-1.2 * kPieceSize, 13.8 * kPieceSize, 2.4 * kPieceSize);
	POLY_STAT_TRIANGLE();

	glNormal3f(-0.957338, 0.031911, 0.287202);
	glTexCoord2f(0.0f * kPieceSize, 13.8f / 17.8f * kPieceSize);
	glVertex3f(-1.2 * kPieceSize, 13.8 * kPieceSize, 2.4 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 16.2f / 17.8f * kPieceSize);
	glVertex3f(-0.8 * kPieceSize, 16.2 * kPieceSize, 4.0 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(-1.0 * kPieceSize, 16.8 * kPieceSize, 3.4 * kPieceSize);
	POLY_STAT_TRIANGLE();

	glNormal3f(-0.997785, 0.066519, 0.0);
	glTexCoord2f(0.0f * kPieceSize, 13.8f / 17.8f * kPieceSize);
	glVertex3f(-1.2 * kPieceSize, 13.8 * kPieceSize, 2.4 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(-1.0 * kPieceSize, 16.8 * kPieceSize, 3.4 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(-1.0 * kPieceSize, 16.8 * kPieceSize, 3.0 * kPieceSize);
	POLY_STAT_TRIANGLE();

	/* Mouth right */
	glNormal3f(0.752714, -0.273714, 0.598750);
	glTexCoord2f(0.0f * kPieceSize, 14.0f / 17.8f * kPieceSize);
	glVertex3f(0.0 * kPieceSize, 14.0 * kPieceSize, 4.0 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 13.8f / 17.8f * kPieceSize);
	glVertex3f(1.2 * kPieceSize, 13.8 * kPieceSize, 2.4 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 16.2f / 17.8f * kPieceSize);
	glVertex3f(0.8 * kPieceSize, 16.2 * kPieceSize, 4.0 * kPieceSize);
	POLY_STAT_TRIANGLE();

	glNormal3f(0.957338, 0.031911, 0.287202);
	glTexCoord2f(0.0f * kPieceSize, 13.8f / 17.8f * kPieceSize);
	glVertex3f(1.2 * kPieceSize, 13.8 * kPieceSize, 2.4 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(1.0 * kPieceSize, 16.8 * kPieceSize, 3.4 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 16.2f / 17.8f * kPieceSize);
	glVertex3f(0.8 * kPieceSize, 16.2 * kPieceSize, 4.0 * kPieceSize);
	POLY_STAT_TRIANGLE();

	glNormal3f(0.997785, 0.066519, 0.0);
	glTexCoord2f(0.0f * kPieceSize, 13.8f / 17.8f * kPieceSize);
	glVertex3f(1.2 * kPieceSize, 13.8 * kPieceSize, 2.4 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(1.0 * kPieceSize, 16.8 * kPieceSize, 3.0 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(1.0 * kPieceSize, 16.8 * kPieceSize, 3.4 * kPieceSize);
	POLY_STAT_TRIANGLE();

	/* Under nose */
	glNormal3f(0.0, -0.992278, 0.124035);
	glTexCoord2f(0.0f * kPieceSize, 14.0f / 17.8f * kPieceSize);
	glVertex3f(0.0 * kPieceSize, 14.0 * kPieceSize, 4.0 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 13.8f / 17.8f * kPieceSize);
	glVertex3f(-1.2 * kPieceSize, 13.8 * kPieceSize, 2.4 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 13.8f / 17.8f * kPieceSize);
	glVertex3f(1.2 * kPieceSize, 13.8 * kPieceSize, 2.4 * kPieceSize);
	POLY_STAT_TRIANGLE();

	/* Neck indents */
	glNormal3f(-0.854714, 0.484047, 0.187514);
	glTexCoord2f(0.0f * kPieceSize, 14.0f / 17.8f * kPieceSize);
	glVertex3f(-0.6 * kPieceSize, 14.0 * kPieceSize, -1.4 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 12.2f / 17.8f * kPieceSize);
	glVertex3f(-1.4 * kPieceSize, 12.2 * kPieceSize, -0.4 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 13.8f / 17.8f * kPieceSize);
	glVertex3f(-0.45 * kPieceSize, 13.8 * kPieceSize, -0.2 * kPieceSize);
	POLY_STAT_TRIANGLE();

	glNormal3f(-0.853747, 0.515805, -0.071146);
	glTexCoord2f(0.0f * kPieceSize, 13.8f / 17.8f * kPieceSize);
	glVertex3f(-0.45 * kPieceSize, 13.8 * kPieceSize, -0.2 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 12.2f / 17.8f * kPieceSize);
	glVertex3f(-1.4 * kPieceSize, 12.2 * kPieceSize, -0.4 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 13.8f / 17.8f * kPieceSize);
	glVertex3f(-0.5 * kPieceSize, 13.8 * kPieceSize, 0.4 * kPieceSize);
	POLY_STAT_TRIANGLE();

	glNormal3f(0.854714, 0.484047, 0.187514);
	glTexCoord2f(0.0f * kPieceSize, 14.0f / 17.8f * kPieceSize);
	glVertex3f(0.6 * kPieceSize, 14.0 * kPieceSize, -1.4 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 13.8f / 17.8f * kPieceSize);
	glVertex3f(0.45 * kPieceSize, 13.8 * kPieceSize, -0.2 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 12.2f / 17.8f * kPieceSize);
	glVertex3f(1.4 * kPieceSize, 12.2 * kPieceSize, -0.4 * kPieceSize);
	POLY_STAT_TRIANGLE();

	glNormal3f(0.853747, 0.515805, -0.071146);
	glTexCoord2f(0.0f * kPieceSize, 13.8f / 17.8f * kPieceSize);
	glVertex3f(0.45 * kPieceSize, 13.8 * kPieceSize, -0.2 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 13.8f / 17.8f * kPieceSize);
	glVertex3f(0.5 * kPieceSize, 13.8 * kPieceSize, 0.4 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 12.2f / 17.8f * kPieceSize);
	glVertex3f(1.4 * kPieceSize, 12.2 * kPieceSize, -0.4 * kPieceSize);
	POLY_STAT_TRIANGLE();

	/* Under chin */
	glNormal3f(0.252982, -0.948683, -0.189737);
	glTexCoord2f(0.0f * kPieceSize, 14.0f / 17.8f * kPieceSize);
	glVertex3f(0.6 * kPieceSize, 14.0 * kPieceSize, -1.4 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 13.6f / 17.8f * kPieceSize);
	glVertex3f(1.2 * kPieceSize, 13.6 * kPieceSize, -0.2 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 13.8f / 17.8f * kPieceSize);
	glVertex3f(0.45 * kPieceSize, 13.8 * kPieceSize, -0.2 * kPieceSize);
	POLY_STAT_TRIANGLE();

	glNormal3f(0.257603, -0.966012, 0.021467);
	glTexCoord2f(0.0f * kPieceSize, 13.8f / 17.8f * kPieceSize);
	glVertex3f(0.5 * kPieceSize, 13.8 * kPieceSize, 0.4 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 13.8f / 17.8f * kPieceSize);
	glVertex3f(0.45 * kPieceSize, 13.8 * kPieceSize, -0.2 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 13.6f / 17.8f * kPieceSize);
	glVertex3f(1.2 * kPieceSize, 13.6 * kPieceSize, -0.2 * kPieceSize);
	POLY_STAT_TRIANGLE();

	glNormal3f(0.126745, -0.887214, 0.443607);
	glTexCoord2f(0.0f * kPieceSize, 13.8f / 17.8f * kPieceSize);
	glVertex3f(0.5 * kPieceSize, 13.8 * kPieceSize, 0.4 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 13.6f / 17.8f * kPieceSize);
	glVertex3f(1.2 * kPieceSize, 13.6 * kPieceSize, -0.2 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 13.8f / 17.8f * kPieceSize);
	glVertex3f(1.2 * kPieceSize, 13.8 * kPieceSize, 0.2 * kPieceSize);
	POLY_STAT_TRIANGLE();

	glNormal3f(-0.252982, -0.948683, -0.189737);
	glTexCoord2f(0.0f * kPieceSize, 14.0f / 17.8f * kPieceSize);
	glVertex3f(-0.6 * kPieceSize, 14.0 * kPieceSize, -1.4 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 13.8f / 17.8f * kPieceSize);
	glVertex3f(-0.45 * kPieceSize, 13.8 * kPieceSize, -0.2 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 13.6f / 17.8f * kPieceSize);
	glVertex3f(-1.2 * kPieceSize, 13.6 * kPieceSize, -0.2 * kPieceSize);
	POLY_STAT_TRIANGLE();

	glNormal3f(-0.257603, -0.966012, 0.021467);
	glTexCoord2f(0.0f * kPieceSize, 13.8f / 17.8f * kPieceSize);
	glVertex3f(-0.5 * kPieceSize, 13.8 * kPieceSize, 0.4 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 13.6f / 17.8f * kPieceSize);
	glVertex3f(-1.2 * kPieceSize, 13.6 * kPieceSize, -0.2 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 13.8f / 17.8f * kPieceSize);
	glVertex3f(-0.45 * kPieceSize, 13.8 * kPieceSize, -0.2 * kPieceSize);
	POLY_STAT_TRIANGLE();

	glNormal3f(-0.126745, -0.887214, 0.443607);
	glTexCoord2f(0.0f * kPieceSize, 13.8f / 17.8f * kPieceSize);
	glVertex3f(-0.5 * kPieceSize, 13.8 * kPieceSize, 0.4 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 13.8f / 17.8f * kPieceSize);
	glVertex3f(-1.2 * kPieceSize, 13.8 * kPieceSize, 0.2 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 13.6f / 17.8f * kPieceSize);
	glVertex3f(-1.2 * kPieceSize, 13.6 * kPieceSize, -0.2 * kPieceSize);
	POLY_STAT_TRIANGLE();

	/* Eyes */
	glNormal3f(0.0, 0.0, -1.0);
	glTexCoord2f(0.0f * kPieceSize, 16.2f / 17.8f * kPieceSize);
	glVertex3f(0.88 * kPieceSize, 16.2 * kPieceSize, 0.2 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 16.4f / 17.8f * kPieceSize);
	glVertex3f(0.61333334 * kPieceSize, 16.4 * kPieceSize, 0.2 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 16.6f / 17.8f * kPieceSize);
	glVertex3f(0.82666667 * kPieceSize, 16.6 * kPieceSize, 0.2 * kPieceSize);
	POLY_STAT_TRIANGLE();

	glNormal3f(0.000003, -0.668965, 0.743294);
	glTexCoord2f(0.0f * kPieceSize, 16.2f / 17.8f * kPieceSize);
	glVertex3f(1.0 * kPieceSize, 16.2 * kPieceSize, -0.74 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 16.6f / 17.8f * kPieceSize);
	glVertex3f(1.0 * kPieceSize, 16.6 * kPieceSize, -0.38 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 16.4f / 17.8f * kPieceSize);
	glVertex3f(0.8 * kPieceSize, 16.4 * kPieceSize, -0.56 * kPieceSize);
	POLY_STAT_TRIANGLE();

	glNormal3f(0.0, 0.0, -1.0);
	glTexCoord2f(0.0f * kPieceSize, 16.2f / 17.8f * kPieceSize);
	glVertex3f(-0.88 * kPieceSize, 16.2 * kPieceSize, 0.2 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 16.6f / 17.8f * kPieceSize);
	glVertex3f(-0.82666667 * kPieceSize, 16.6 * kPieceSize,
			   0.2 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 16.4f / 17.8f * kPieceSize);
	glVertex3f(-0.61333334 * kPieceSize, 16.4 * kPieceSize,
			   0.2 * kPieceSize);
	POLY_STAT_TRIANGLE();

	glNormal3f(-0.000003, -0.668965, 0.743294);
	glTexCoord2f(0.0f * kPieceSize, 16.2f / 17.8f * kPieceSize);
	glVertex3f(-1.0 * kPieceSize, 16.2 * kPieceSize, -0.74 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 16.4f / 17.8f * kPieceSize);
	glVertex3f(-0.8 * kPieceSize, 16.4 * kPieceSize, -0.56 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 16.6f / 17.8f * kPieceSize);
	glVertex3f(-1.0 * kPieceSize, 16.6 * kPieceSize, -0.38 * kPieceSize);
	POLY_STAT_TRIANGLE();

	/* Behind eyes */
	/* Right */
	glNormal3f(-0.997484, 0.070735, 0.004796);
	glTexCoord2f(0.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(-0.4 * kPieceSize, 16.8 * kPieceSize, -1.1 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(-1.0 * kPieceSize, 16.8 * kPieceSize, -2.0 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 15.8f / 17.8f * kPieceSize);
	glVertex3f(-1.0 * kPieceSize, 15.8 * kPieceSize, -1.1 * kPieceSize);
	POLY_STAT_TRIANGLE();

	glNormal3f(-0.744437, 0.446663, -0.496292);
	glTexCoord2f(0.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(-1.0 * kPieceSize, 16.8 * kPieceSize, -0.2 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(-0.4 * kPieceSize, 16.8 * kPieceSize, -1.1 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 15.8f / 17.8f * kPieceSize);
	glVertex3f(-1.0 * kPieceSize, 15.8 * kPieceSize, -1.1 * kPieceSize);
	POLY_STAT_TRIANGLE();

	/* Left */
	glNormal3f(0.997484, 0.070735, 0.004796);
	glTexCoord2f(0.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(0.4 * kPieceSize, 16.8 * kPieceSize, -1.1 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 15.8f / 17.8f * kPieceSize);
	glVertex3f(1.0 * kPieceSize, 15.8 * kPieceSize, -1.1 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(1.0 * kPieceSize, 16.8 * kPieceSize, -2.0 * kPieceSize);
	POLY_STAT_TRIANGLE();

	glNormal3f(0.744437, 0.446663, -0.496292);
	glTexCoord2f(0.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(1.0 * kPieceSize, 16.8 * kPieceSize, -0.2 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 15.8f / 17.8f * kPieceSize);
	glVertex3f(1.0 * kPieceSize, 15.8 * kPieceSize, -1.1 * kPieceSize);
	glTexCoord2f(0.0f * kPieceSize, 16.8f / 17.8f * kPieceSize);
	glVertex3f(0.4 * kPieceSize, 16.8 * kPieceSize, -1.1 * kPieceSize);
	POLY_STAT_TRIANGLE();

	glEnd();
	POLY_STAT_END("knight");
}

void draw_bishop(void)
{
	float trace_r[] =
		{ 4.0f, 4.0f, 2.5f, 2.5f, 1.5f, 1.2f, 2.5f, 1.7f, 1.7f, 2.2f, 2.2f,
		  1.0f, 0.8f, 1.2f, 0.8f, 0.0f, 0.0f
		};
	float trace_h[] =
		{ 0.0f, 2.0f, 3.0f, 4.0f, 7.0f, 9.4f, 9.4f, 11.0f, 12.2f, 13.2f,
		  14.8f, 16.0f, 17.0f, 17.7f, 18.4f, 18.4f, 0.0f
		};

	POLY_STAT_BEGIN();
	revolve_line(trace_r, trace_h, 0.0f);
	POLY_STAT_END("bishop");
}

void draw_queen(void)
{
	float trace_r[] =
		{ 4.8f, 4.8f, 3.4f, 3.4f, 1.8f, 1.4f, 2.9f, 1.8f, 1.8f, 2.0f, 2.7f,
		  2.4f, 1.7f, 0.95f, 0.7f, 0.9f, 0.7f, 0.0f, 0.0f
		};
	float trace_h[] =
		{ 0.0f, 2.2f, 4.0f, 5.0f, 8.0f, 11.8f, 11.8f, 13.6f, 15.2f, 17.8f,
		  19.2f, 20.0f, 20.0f, 20.8f, 20.8f, 21.4f, 22.0f, 22.0f, 0.0f
		};

	POLY_STAT_BEGIN();
	revolve_line(trace_r, trace_h, 0.0f);
	POLY_STAT_END("queen");
}

void draw_king(void)
{
	float trace_r[] =
		{ 5.0f, 5.0f, 3.5f, 3.5f, 2.0f, 1.4f, 3.0f, 2.0f, 2.0f, 2.8f, 1.6f,
		  1.6f, 0.0f, 0.0f
		};
	float trace_h[] =
		{ 0.0f, 2.0f, 3.0f, 4.6f, 7.6f, 12.6f, 12.6f, 14.6f, 15.6f, 19.1f,
		  19.7f, 20.1f, 20.1f, 0.0f
		};

	POLY_STAT_BEGIN();
	revolve_line(trace_r, trace_h, 0.0f);

	glBegin(GL_QUADS);

	/* Cross front */
	glNormal3f(0.0, 0.0, 1.0);

	glVertex3f(-0.3 * kPieceSize, 20.1 * kPieceSize, 0.351 * kPieceSize);
	glVertex3f(0.3 * kPieceSize, 20.1 * kPieceSize, 0.35 * kPieceSize);
	glVertex3f(0.3 * kPieceSize, 23.1 * kPieceSize, 0.35 * kPieceSize);
	glVertex3f(-0.3 * kPieceSize, 23.1 * kPieceSize, 0.35 * kPieceSize);
	POLY_STAT_QUAD();

	glVertex3f(-0.9 * kPieceSize, 21.1 * kPieceSize, 0.35 * kPieceSize);
	glVertex3f(-0.3 * kPieceSize, 21.1 * kPieceSize, 0.35 * kPieceSize);
	glVertex3f(-0.3 * kPieceSize, 22.1 * kPieceSize, 0.35 * kPieceSize);
	glVertex3f(-0.9 * kPieceSize, 22.1 * kPieceSize, 0.35 * kPieceSize);
	POLY_STAT_QUAD();

	glVertex3f(0.9 * kPieceSize, 21.1 * kPieceSize, 0.35 * kPieceSize);
	glVertex3f(0.9 * kPieceSize, 22.1 * kPieceSize, 0.35 * kPieceSize);
	glVertex3f(0.3 * kPieceSize, 22.1 * kPieceSize, 0.35 * kPieceSize);
	glVertex3f(0.3 * kPieceSize, 21.1 * kPieceSize, 0.35 * kPieceSize);
	POLY_STAT_QUAD();

	/* Cross back */
	glNormal3f(0.0, 0.0, -1.0);

	glVertex3f(0.3 * kPieceSize, 20.1 * kPieceSize, -0.35 * kPieceSize);
	glVertex3f(-0.3 * kPieceSize, 20.1 * kPieceSize, -0.35 * kPieceSize);
	glVertex3f(-0.3 * kPieceSize, 23.1 * kPieceSize, -0.35 * kPieceSize);
	glVertex3f(0.3 * kPieceSize, 23.1 * kPieceSize, -0.35 * kPieceSize);
	POLY_STAT_QUAD();

	glVertex3f(-0.3 * kPieceSize, 21.1 * kPieceSize, -0.35 * kPieceSize);
	glVertex3f(-0.9 * kPieceSize, 21.1 * kPieceSize, -0.35 * kPieceSize);
	glVertex3f(-0.9 * kPieceSize, 22.1 * kPieceSize, -0.35 * kPieceSize);
	glVertex3f(-0.3 * kPieceSize, 22.1 * kPieceSize, -0.35 * kPieceSize);
	POLY_STAT_QUAD();

	glVertex3f(0.3 * kPieceSize, 21.1 * kPieceSize, -0.35 * kPieceSize);
	glVertex3f(0.3 * kPieceSize, 22.1 * kPieceSize, -0.35 * kPieceSize);
	glVertex3f(0.9 * kPieceSize, 22.1 * kPieceSize, -0.35 * kPieceSize);
	glVertex3f(0.9 * kPieceSize, 21.1 * kPieceSize, -0.35 * kPieceSize);
	POLY_STAT_QUAD();

	/* Cross left */
	glNormal3f(-1.0, 0.0, 0.0);

	glVertex3f(-0.9 * kPieceSize, 21.1 * kPieceSize, 0.35 * kPieceSize);
	glVertex3f(-0.9 * kPieceSize, 22.1 * kPieceSize, 0.35 * kPieceSize);
	glVertex3f(-0.9 * kPieceSize, 22.1 * kPieceSize, -0.35 * kPieceSize);
	glVertex3f(-0.9 * kPieceSize, 21.1 * kPieceSize, -0.35 * kPieceSize);
	POLY_STAT_QUAD();

	glVertex3f(-0.3 * kPieceSize, 20.1 * kPieceSize, 0.35 * kPieceSize);
	glVertex3f(-0.3 * kPieceSize, 21.1 * kPieceSize, 0.35 * kPieceSize);
	glVertex3f(-0.3 * kPieceSize, 21.1 * kPieceSize, -0.35 * kPieceSize);
	glVertex3f(-0.3 * kPieceSize, 20.1 * kPieceSize, -0.35 * kPieceSize);
	POLY_STAT_QUAD();

	glVertex3f(-0.3 * kPieceSize, 22.1 * kPieceSize, 0.3 * kPieceSize);
	glVertex3f(-0.3 * kPieceSize, 23.1 * kPieceSize, 0.3 * kPieceSize);
	glVertex3f(-0.3 * kPieceSize, 23.1 * kPieceSize, -0.3 * kPieceSize);
	glVertex3f(-0.3 * kPieceSize, 22.1 * kPieceSize, -0.3 * kPieceSize);
	POLY_STAT_QUAD();

	/* Cross right */
	glNormal3f(1.0, 0.0, 0.0);

	glVertex3f(0.9 * kPieceSize, 21.1 * kPieceSize, -0.35 * kPieceSize);
	glVertex3f(0.9 * kPieceSize, 22.1 * kPieceSize, -0.35 * kPieceSize);
	glVertex3f(0.9 * kPieceSize, 22.1 * kPieceSize, 0.35 * kPieceSize);
	glVertex3f(0.9 * kPieceSize, 21.1 * kPieceSize, 0.35 * kPieceSize);
	POLY_STAT_QUAD();

	glVertex3f(0.3 * kPieceSize, 20.1 * kPieceSize, -0.35 * kPieceSize);
	glVertex3f(0.3 * kPieceSize, 21.1 * kPieceSize, -0.35 * kPieceSize);
	glVertex3f(0.3 * kPieceSize, 21.1 * kPieceSize, 0.35 * kPieceSize);
	glVertex3f(0.3 * kPieceSize, 20.1 * kPieceSize, 0.35 * kPieceSize);
	POLY_STAT_QUAD();

	glVertex3f(0.3 * kPieceSize, 22.1 * kPieceSize, -0.35 * kPieceSize);
	glVertex3f(0.3 * kPieceSize, 23.1 * kPieceSize, -0.35 * kPieceSize);
	glVertex3f(0.3 * kPieceSize, 23.1 * kPieceSize, 0.35 * kPieceSize);
	glVertex3f(0.3 * kPieceSize, 22.1 * kPieceSize, 0.35 * kPieceSize);
	POLY_STAT_QUAD();

	/* Cross top */
	glNormal3f(0.0, 1.0, 0.0);

	glVertex3f(-0.9 * kPieceSize, 22.1 * kPieceSize, -0.35 * kPieceSize);
	glVertex3f(-0.9 * kPieceSize, 22.1 * kPieceSize, 0.35 * kPieceSize);
	glVertex3f(-0.3 * kPieceSize, 22.1 * kPieceSize, 0.35 * kPieceSize);
	glVertex3f(-0.3 * kPieceSize, 22.1 * kPieceSize, -0.35 * kPieceSize);
	POLY_STAT_QUAD();

	glVertex3f(0.3 * kPieceSize, 22.1 * kPieceSize, -0.35 * kPieceSize);
	glVertex3f(0.3 * kPieceSize, 22.1 * kPieceSize, 0.35 * kPieceSize);
	glVertex3f(0.9 * kPieceSize, 22.1 * kPieceSize, 0.35 * kPieceSize);
	glVertex3f(0.9 * kPieceSize, 22.1 * kPieceSize, -0.35 * kPieceSize);
	POLY_STAT_QUAD();

	glVertex3f(-0.3 * kPieceSize, 23.1 * kPieceSize, -0.35 * kPieceSize);
	glVertex3f(-0.3 * kPieceSize, 23.1 * kPieceSize, 0.35 * kPieceSize);
	glVertex3f(0.3 * kPieceSize, 23.1 * kPieceSize, 0.35 * kPieceSize);
	glVertex3f(0.3 * kPieceSize, 23.1 * kPieceSize, -0.35 * kPieceSize);
	POLY_STAT_QUAD();

	glEnd();
	POLY_STAT_END("king");
}

@implementation MBCBoardView ( Models )

void DrawWithScaledTextures(GLuint list, void (*draw)())
{
	float texScale	= 2.0f;

	glNewList(list, GL_COMPILE);
	glMatrixMode(GL_TEXTURE);
	glPushMatrix();
	glScalef(texScale, texScale, 1.0f);
	glMatrixMode(GL_MODELVIEW);
	(*draw)();
	glMatrixMode(GL_TEXTURE);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glEndList();
}

- (void) generateModelLists
{
	DrawWithScaledTextures(1, draw_king);
	DrawWithScaledTextures(2, draw_queen);
	DrawWithScaledTextures(3, draw_bishop);
	DrawWithScaledTextures(4, draw_knight);
	DrawWithScaledTextures(5, draw_rook);
	DrawWithScaledTextures(6, draw_pawn);
}

@end

// Local Variables:
// mode:ObjC
// End:
