/*
	File:		MBCBoardViewMouse.mm
	Contains:	Handle mouse coordinate transformations
	Copyright:	© 2002-2003 Apple Computer, Inc. All rights reserved.

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

#import "MBCBoardViewMouse.h"
#import "MBCBoardViewDraw.h" // For drawBoardPlane
#import "MBCInteractivePlayer.h"

#import <OpenGL/glu.h>

#import <algorithm>

using std::min;
using std::max;

//
// We're doing a lot of UnProjects. This class encapsulates them.
//
class MBCUnProjector 
{
public:
	MBCUnProjector(GLdouble winX, GLdouble winY);
	
	MBCPosition UnProject();
	MBCPosition UnProject(GLfloat knownY);
private:
	GLdouble	fWinX;
	GLdouble	fWinY;	
    GLint		fViewport[4];
    GLdouble	fMVMatrix[16];
	GLdouble	fProjMatrix[16];
};

MBCUnProjector::MBCUnProjector(GLdouble winX, GLdouble winY)
	: fWinX(winX), fWinY(winY)
{
    glGetIntegerv(GL_VIEWPORT, fViewport);
    glGetDoublev(GL_MODELVIEW_MATRIX, fMVMatrix);
    glGetDoublev(GL_PROJECTION_MATRIX, fProjMatrix);
}

MBCPosition MBCUnProjector::UnProject()
{
	MBCPosition	pos;
	GLfloat		z;
	GLdouble 	wv[3];

    glReadPixels((GLint)fWinX, (GLint)fWinY, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &z);
    gluUnProject(fWinX, fWinY, z, fMVMatrix, fProjMatrix, fViewport, 
				 wv+0, wv+1, wv+2);

	pos[0] = wv[0];
	pos[1] = wv[1];
	pos[2] = wv[2];

	return pos;
}

MBCPosition MBCUnProjector::UnProject(GLfloat knownY)
{
	MBCPosition	pos;
	GLdouble 	p1[3];
	GLdouble 	p0[3];	

    gluUnProject(fWinX, fWinY, 1.0f, fMVMatrix, fProjMatrix, fViewport, 
				 p1+0, p1+1, p1+2);
    gluUnProject(fWinX, fWinY, 0.0f, fMVMatrix, fProjMatrix, fViewport, 
				 p0+0, p0+1, p0+2);
	GLdouble yint = (knownY-p1[1])/(p0[1]-p1[1]);
	pos[0] = p1[0]+(p0[0]-p1[0])*yint;
	pos[1] = knownY;
	pos[2] = p1[2]+(p0[2]-p1[2])*yint;

	return pos;
}

MBCPosition operator-(const MBCPosition & a, const MBCPosition & b)
{
	MBCPosition	res;
	
	res[0]	= a[0]-b[0];
	res[1]	= a[1]-b[1];
	res[2]	= a[2]-b[2];

	return res;
}

@implementation MBCBoardView ( Mouse )

- (void) mouseMoved:(NSEvent *)event
{
    NSPoint p = [event locationInWindow];
    NSPoint l = [self convertPoint:p fromView:nil];

	MBCUnProjector	unproj(l.x, l.y);
	MBCPosition 	pos 	= unproj.UnProject();
	float 			pxa		= fabs(pos[0]);
	float			pza		= fabs(pos[2]);
	NSCursor *		cursor	= fArrowCursor;

	if (pxa > kBoardRadius || pza > kBoardRadius) 
		if (pxa < kBoardRadius+kBorderWidth+.1f 
		 && pza < kBoardRadius+kBorderWidth+.1f)
			cursor	=	fHandCursor;
	[cursor set];
}

- (void) mouseDown:(NSEvent *)event
{
    NSPoint p 	= [event locationInWindow];
    NSPoint l 	= [self convertPoint:p fromView:nil];

	MBCUnProjector	unproj(l.x, l.y);

	//	
	// On mousedown, we determine the point on the board surface that 
	// corresponds to the mouse location by the frontmost Z value, but
	// then pretend that the click happened at board surface level. Weirdly
	// enough, this seems to give the most natural feeling mouse behavior.
	//
	MBCPosition pos = unproj.UnProject();

	MBCSquare selectedStart = fSelectedDest = 
		[self positionToSquareOrRegion:&pos];
    switch (fSelectedDest) {
	case kInvalidSquare:
		return;
	case kWhitePromoSquare:
	case kBlackPromoSquare:
		return;
	case kBorderRegion:
		fInBoardManipulation= true;
		fOrigMouse 			= l;
		fCurMouse 			= l;
		fRawAzimuth 		= fAzimuth;
		fSelectedPiece		= 0;
		[NSCursor hide];
		[NSEvent startPeriodicEventsAfterDelay:0.1f withPeriod:0.1f];
		break;
	default:
		if (!fWantMouse || fInAnimation || pos[1] < 0.1)
			return;
		//
		// Let interactive player decide whether we hit one of their pieces
		//
		[fInteractive startSelection:fSelectedDest];
		if (!fSelectedPiece) // Apparently not...
			return;
		break;
	}
	pos[1]		    	= 0.0f;
	fSelectedStartPos	= pos;
	gettimeofday(&fLastRedraw, NULL);
	fLastSelectedPos	= pos;
	//
	// For better interactivity, we stop the engine while a drag is in progress
	//
	[[fController engine] interruptEngine];
	[self drawNow];
	
	NSDate * whenever = [NSDate distantFuture];
	for (bool goOn = true; goOn; ) {
		event = 
			[NSApp nextEventMatchingMask: 
					   NSPeriodicMask|NSLeftMouseUpMask|NSLeftMouseDraggedMask
				   untilDate:whenever inMode:NSEventTrackingRunLoopMode 
				   dequeue:YES];
        switch ([event type]) {
		case NSPeriodic:
		case NSLeftMouseDragged:
			[self dragAndRedraw:event forceRedraw:NO];
			break;
		case NSLeftMouseUp: {
			[self dragAndRedraw:event forceRedraw:YES];
			[fInteractive endSelection:fSelectedDest];
			goOn = false;
			if (fInBoardManipulation) {
				fInBoardManipulation = false;
				[NSCursor unhide];	
				[NSEvent stopPeriodicEvents];
			}
			break; }
		default:
			/* Ignore any other kind of event. */
			break;
		}		
	}
	fSelectedDest = kInvalidSquare;
}

- (void) mouseUp:(NSEvent *)event
{
	if (!fWantMouse || fInAnimation)
		return;

	MBCPiece promo;
	if (fSelectedDest == kWhitePromoSquare)
		promo = [fBoard defaultPromotion:YES];
	else if (fSelectedDest == kBlackPromoSquare)
		promo = [fBoard defaultPromotion:NO];
	else
		return;
	
	switch (promo) {
	case QUEEN:
		if (fVariant == kVarSuicide)
			promo = KING;	// King promotion is very popular in suicide
		else
			promo = KNIGHT; // Second most useful
		break;
	case KING: // Suicide only
		promo = KNIGHT;
		break;
	case KNIGHT:
		promo = ROOK;
		break;
	case ROOK:
		promo = BISHOP;
		break;
	case BISHOP:
		promo = QUEEN;
		break;
	}
	[fBoard setDefaultPromotion:promo 
			for:fSelectedDest == kWhitePromoSquare];

	[self setNeedsDisplay:YES];
}

- (void) dragAndRedraw:(NSEvent *)event forceRedraw:(BOOL)force
{
	if ([event type] != NSPeriodic) {
		NSPoint p = [event locationInWindow];
		NSPoint l = [self convertPoint:p fromView:nil];
		fCurMouse = l;
		//
		// On drag, we can use a fairly fast interpolation to determine
		// the 3D coordinate using the y where we touched the piece
		//
		MBCUnProjector	unproj(l.x, l.y);

		fSelectedPos 				= unproj.UnProject(0.0f);
		[self snapToSquare:&fSelectedPos];
	}
	struct timeval	now;
	gettimeofday(&now, NULL);
	MBCPosition		delta		= fSelectedPos-fLastSelectedPos;
	GLfloat			d2      	= delta[0]*delta[0]+delta[2]*delta[2];
	NSTimeInterval	dt			= 
		now.tv_sec - fLastRedraw.tv_sec 
		+ 0.000001 * (now.tv_usec - fLastRedraw.tv_usec);

	const float	kTiltSpeed		=  0.50f;
	const float kSpinSpeed		=  0.50f;
	const float	kThreshold		= 10.0f;
	const float	kAzimuthRound	=  5.0f;

	if (force) {
		[self setNeedsDisplay:YES];	
	} else if (fSelectedDest == kBorderRegion) {
		float dx =  fCurMouse.x-fOrigMouse.x;
		float dy =	fCurMouse.y-fOrigMouse.y;
		if (fabs(dx) > fabs(dy) && fabs(dx) > kThreshold) {
			fRawAzimuth += dx*dt*kSpinSpeed;
			fRawAzimuth = fmod(fRawAzimuth+360.0f, 360.0f);
			float angle	= fmod((fAzimuth = fRawAzimuth), 90.0f);
			if (angle < kAzimuthRound)
				fAzimuth	-= angle;
			else if (angle > 90.0f-kAzimuthRound)
				fAzimuth 	+= 90.0f-angle;
			fNeedPerspective= true;
			fLastRedraw 	= now;
			[self drawNow];
		} else if (fabs(dy) > kThreshold) {
			fElevation -= dy*dt*kTiltSpeed;
			fElevation = 
				max(kMinElevation, min(kMaxElevation, fElevation));
			fNeedPerspective= true;
			fLastRedraw 	= now;
			[self drawNow];
		}
	} else if (d2 > 25.0f || (d2 > 1.0f && dt > 0.02)) {
		fSelectedDest	= [self positionToSquare:&fSelectedPos];
		fLastRedraw 	= now;
		[self drawNow];
	}
}

@end

// Local Variables:
// mode:ObjC
// End:
