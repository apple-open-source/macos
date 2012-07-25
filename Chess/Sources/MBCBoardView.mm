/*
	File:		MBCBoardView.mm
	Contains:	General view handling infrastructure
	Copyright:	© 2002-2012 by Apple Inc., all rights reserved.

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
	of Apple Inc. may be used to endorse or promote products
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
#import "MBCBoardView.h"
#import "MBCBoardAnimation.h"
#import "MBCMoveAnimation.h"

//
// Our implementation is distributed across several other files
//
#import "MBCBoardViewDraw.h"
#import "MBCBoardViewModels.h"
#import "MBCBoardViewTextures.h"
#import "MBCBoardViewMouse.h"

#import "MBCAnimation.h"
#import "MBCController.h"
#import "MBCPlayer.h"
#import "MBCBoardWin.h"
#import "MBCUserDefaults.h"

#include <algorithm>

NSColor * MBCColor::GetColor() const
{
	return [NSColor 
			   colorWithCalibratedRed:color[0] green:color[1] blue:color[2] 
			   alpha:color[3]];
}

void MBCColor::SetColor(NSColor * newColor)
{
	color[0] = [newColor redComponent];
	color[1] = [newColor greenComponent];
	color[2] = [newColor blueComponent];
}

@implementation MBCBoardView

- (NSOpenGLPixelFormat *)pixelFormatWithFSAA:(int)fsaaSamples
{
    NSOpenGLPixelFormatAttribute fsaa_attr[] = 
    {
		NSOpenGLPFAAllowOfflineRenderers,
        NSOpenGLPFADoubleBuffer,
		NSOpenGLPFAWindow,
		NSOpenGLPFAAccelerated,
		NSOpenGLPFANoRecovery,
		NSOpenGLPFAColorSize, 		(NSOpenGLPixelFormatAttribute)24,
		NSOpenGLPFAAlphaSize, 		(NSOpenGLPixelFormatAttribute)8,
   		NSOpenGLPFADepthSize, 		(NSOpenGLPixelFormatAttribute)16,
        NSOpenGLPFAStencilSize, 	(NSOpenGLPixelFormatAttribute)1,
		NSOpenGLPFAMultisample,		
		NSOpenGLPFASampleBuffers, 	(NSOpenGLPixelFormatAttribute)1, 
		NSOpenGLPFASamples, 		(NSOpenGLPixelFormatAttribute)fsaaSamples,
        (NSOpenGLPixelFormatAttribute)0 
	};
    NSOpenGLPixelFormatAttribute jaggy_attr[] = 
    {
		NSOpenGLPFAAllowOfflineRenderers,
		NSOpenGLPFADoubleBuffer,
		NSOpenGLPFAWindow,
		NSOpenGLPFAColorSize, 		(NSOpenGLPixelFormatAttribute)24,
		NSOpenGLPFAAlphaSize, 		(NSOpenGLPixelFormatAttribute)8,
   		NSOpenGLPFADepthSize, 		(NSOpenGLPixelFormatAttribute)16,
        NSOpenGLPFAStencilSize, 	(NSOpenGLPixelFormatAttribute)1,
        (NSOpenGLPixelFormatAttribute)0 
	};
	NSOpenGLPixelFormat *	pixelFormat = 
		[[NSOpenGLPixelFormat alloc] initWithAttributes:(fsaaSamples > 0) ? fsaa_attr : jaggy_attr];

	return [pixelFormat autorelease];
}

static int MaxAntiAliasing()
{
    //
    //  Analyze VRAM configuration and limit FSAA for low memory configurations.
    //
	static int sMax = -1;

	if (sMax < 0) {
		GLint min_vram = 0;
		CGLRendererInfoObj rend;
		GLint n_rend = 0;
		CGLQueryRendererInfo(0xffffff, &rend, &n_rend);
		for (GLint i=0; i<n_rend; ++i) {
			GLint cur_vram = 0;
			CGLDescribeRenderer(rend, i, kCGLRPVideoMemoryMegabytes, &cur_vram);
            if (!min_vram)
                min_vram = cur_vram;
            else if (cur_vram)
                min_vram = std::min(min_vram, cur_vram);
		}
		sMax = (min_vram > 256 ? 8 : 4);
	}

	return sMax;
}

- (id) initWithFrame:(NSRect)rect
{
	float	light_ambient		= 0.125f;
	GLfloat light_pos[4] 		= { -60.0, 200.0, 0.0, 0.0};
	
	//
	// We first try to enable Full Scene Anti Aliasing if our graphics
	// hardware lets use get away with it.
	//
	NSOpenGLPixelFormat * pixelFormat = nil;
	for (fMaxFSAA = MaxAntiAliasing()+2; !pixelFormat; )
		pixelFormat = [self pixelFormatWithFSAA:(fMaxFSAA -= 2)];
	fCurFSAA = fMaxFSAA;
    [super initWithFrame:rect pixelFormat:pixelFormat];
    [self setWantsBestResolutionOpenGLSurface:YES];
    [[self openGLContext] makeCurrentContext];
	glEnable(GL_MULTISAMPLE);
	
	//
	// Determine some of our graphics limit
	//
	const char * const kGlExt = (const char *)glGetString(GL_EXTENSIONS);
	if (kGlExt && strstr(kGlExt, "GL_EXT_texture_filter_anisotropic")) {
		glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &fAnisotropy);
		fAnisotropy = std::min(fAnisotropy, 4.0f);
	} else
		fAnisotropy	= 0.0f;
	
	
	fBoardReflectivity	= 0.3f;
	fBoardDrawStyle[0]	= [[MBCDrawStyle alloc] init];
	fBoardDrawStyle[1]	= [[MBCDrawStyle alloc] init];
	fPieceDrawStyle[0]	= [[MBCDrawStyle alloc] init];
	fPieceDrawStyle[1]	= [[MBCDrawStyle alloc] init];
	fBorderDrawStyle	= [[MBCDrawStyle alloc] init];
	fSelectedPieceDrawStyle	= [[MBCDrawStyle alloc] init];
	fBoardAttr 			= nil;
	fPieceAttr			= nil;
	fBoardStyle			= nil;
	fPieceStyle			= nil;
	fNeedStaticModels	= true;

	fElevation			=  60.0f;
	fAzimuth			= 180.0f;
	fInAnimation		= false;
	fInBoardManipulation= false;
	fPickedSquare		= kInvalidSquare;
	fSelectedPiece		= EMPTY;
	fSelectedDest		= kInvalidSquare;
	fWantMouse			= false;
	fNeedPerspective	= true;
	fAmbient			= light_ambient;
	fIsPickingFormat	= false;
	fLastFSAASize		= 2000000000;
	memcpy(fLightPos, light_pos, sizeof(fLightPos));
	fKeyBuffer			= 0;

	fHandCursor			= [[NSCursor pointingHandCursor] retain];
	fArrowCursor		= [[NSCursor arrowCursor] retain];
    [self updateTrackingAreas];

    return self;
}

- (void)updateTrackingAreas 
{
    [self removeTrackingArea:fTrackingArea];
    [fTrackingArea release];
    fTrackingArea = [[NSTrackingArea alloc] initWithRect:[self bounds]
                                                 options: (NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved | NSTrackingActiveInKeyWindow)
                                                   owner:self userInfo:nil];
    [self addTrackingArea:fTrackingArea];
}

- (void) pickPixelFormat:(BOOL)afterFailure
{
	if (fIsPickingFormat)
		return; // Avoid recursive picking
	//
	// If we didn't fail with the current format, try whether we can become more aggressive again
	//
	NSRect 	bounds = [self bounds];
	int 	curSize= bounds.size.width*bounds.size.height;
	if (afterFailure) {
		fLastFSAASize 		= curSize;
	} else {
		if (fCurFSAA == fMaxFSAA || curSize >= fLastFSAASize)
			return; // Won't get any better
		fCurFSAA 		= fMaxFSAA+2;
		fLastFSAASize	= 2000000000;
	}
	fIsPickingFormat = true;
	do {
		fCurFSAA -= 2;
		[self clearGLContext];
		[self setPixelFormat:[self pixelFormatWithFSAA:fCurFSAA]];
		[[self openGLContext] setView:self];
	} while (fCurFSAA && [[self openGLContext] view] != self);
	[[self openGLContext] makeCurrentContext];
	if (fCurFSAA)
		glEnable(GL_MULTISAMPLE);
	fIsPickingFormat 	= false;
	NSLog(@"Size is now %.0fx%.0f FSAA = %d [Max %d]\n", bounds.size.width, bounds.size.height, fCurFSAA, fMaxFSAA);
}

- (void) setStyleForBoard:(NSString *)boardStyle pieces:(NSString *)pieceStyle
{
	[[self openGLContext] makeCurrentContext];
	[fBoardStyle release];
	fBoardStyle = 
		[[@"Styles" stringByAppendingPathComponent:boardStyle] retain];
	[fPieceStyle release];
	fPieceStyle = 
		[[@"Styles" stringByAppendingPathComponent:pieceStyle] retain];
	[self loadStyles];
    [self setNeedsDisplay: YES];
}

- (void)awakeFromNib
{
    fController     = [[self window] windowController];
	fBoard          = [fController board];
	fInteractive    = [fController interactive];
}

- (BOOL) isOpaque
{
	return NO;
}

- (BOOL) mouseDownCanMoveWindow
{
	return NO;
}

- (void) drawRect:(NSRect)rect
{
	[self drawPosition];
}

- (void) reshape
{
	[self pickPixelFormat:NO];
	[self needsUpdate];
}

- (void) drawNow
{
	[self lockFocus];
	[self drawPosition];
	[self unlockFocus];
    [self setNeedsDisplay:NO];
}

- (void) profileDraw
{
	[self lockFocus];
	for (int i=0; i<100; ++i) {
		[self drawPosition];
	}
	[self unlockFocus];
}

- (void) needsUpdate
{
    fNeedPerspective	= true;

    [self setNeedsDisplay: YES];
}

- (void) endGame;
{
	[MBCAnimation cancelCurrentAnimation];
	fSelectedPiece		= EMPTY;
	fPickedSquare		= kInvalidSquare;
	fWantMouse			= false;
	[self hideMoves];
	[self needsUpdate];
}

- (void) startGame:(MBCVariant)variant playing:(MBCSide)side
{
	fVariant 		= variant;
	fSide	 		= side;
	[self endGame];
	if (side != kNeitherSide && [self facing] != kNeitherSide) {
		//
		// We have humans involved, turn board right side up unless
		// it was in a neutral position
		//
		if (side == kBothSides)
			side = ([fBoard numMoves]&1) ? kBlackSide : kWhiteSide;
		if ([self facing] != side)
			fAzimuth	= fmod(fAzimuth+180.0f, 360.0f);
	}
}

- (void) showMoveAsHint:(MBCMove *)move
{
	[move retain];
	[fHintMove autorelease];
	fHintMove	= move;
	[self setNeedsDisplay:YES];
}

- (void) showMoveAsLast:(MBCMove *)move
{
	[move retain];
	[fLastMove autorelease];
	fLastMove	= move;
	[self setNeedsDisplay:YES];
}

- (void) hideMoves
{
	[fHintMove release];
	[fLastMove release];
	fHintMove	= nil;
	fLastMove	= nil;
}

- (void) clickPiece
{
	fPickedSquare = fSelectedSquare;

    [self unselectPiece];
}

- (void) selectPiece:(MBCPiece)piece at:(MBCSquare)square
{
	fPickedSquare	=   kInvalidSquare;
	fSelectedPiece	=	piece;
	fSelectedSquare	=   square;

	if (square != kInvalidSquare) 
		fSelectedPos	= [self squareToPosition:square];

	[self setNeedsDisplay:YES];
}

- (void) selectPiece:(MBCPiece)piece at:(MBCSquare)square to:(MBCSquare)dest
{
	fSelectedDest	= dest;

	[self selectPiece:piece at:square];
}

- (void) moveSelectionTo:(MBCPosition *)position
{
	fSelectedPos	= *position;
	
	[self setNeedsDisplay:YES];
}

- (void) unselectPiece
{
	fSelectedPiece	= EMPTY;
	fSelectedSquare = kInvalidSquare;
	fSelectedDest 	= kInvalidSquare;

	[self setNeedsDisplay:YES];
}

- (MBCSquare) 	positionToSquare:(const MBCPosition *)position
{
	GLfloat	px	= (*position)[0];
	GLfloat pz	= (*position)[2];
	GLfloat pxa = fabs(px);
	GLfloat pza = fabs(pz);
	
	if (fabs(px - kInHandPieceX) < (kInHandPieceSize/2.0f)) {
		pza -= kInHandPieceZOffset;
		if (pza > 0.0f && pza < kInHandPieceSize*5.0f) {
			MBCPieceCode piece = gInHandOrder[(int)(pza / kInHandPieceSize)];
			return kInHandSquare+(pz < 0 ? Black(piece) : White(piece));
		} else	
			return kInvalidSquare;
	}
	if (pxa > kBoardRadius || pza > kBoardRadius) 
		return kInvalidSquare;

	int row	= static_cast<int>((kBoardRadius-pz)/10.0f);
	int col = static_cast<int>((px+kBoardRadius)/10.0f);

	return (row<<3)|col;
}

- (MBCSquare) positionToSquareOrRegion:(const MBCPosition *)position
{
	GLfloat	px	= (*position)[0];
	GLfloat pz	= (*position)[2];
	GLfloat pxa = fabs(px);
	GLfloat pza = fabs(pz);
	
	if (fPromotionSide == kWhiteSide) {
		if (fabs(px + kPromotionPieceX) < 8.0f
		 && fabs(pz + kPromotionPieceZ) < 8.0f
		)
			return kWhitePromoSquare;
	} else if (fPromotionSide == kBlackSide) {
		if (fabs(px - kPromotionPieceX) < 8.0f
		 && fabs(pz - kPromotionPieceZ) < 8.0f
		)
			return kBlackPromoSquare;
	}

	if (fabs(px - kInHandPieceX) < (kInHandPieceSize/2.0f)) {
		pza -= kInHandPieceZOffset;
		if (pza > 0.0f && pza < kInHandPieceSize*5.0f) {
			MBCPieceCode piece = gInHandOrder[(int)(pza / kInHandPieceSize)];
			return kInHandSquare+(pz < 0 ? Black(piece) : White(piece));
		} else	
			return kInvalidSquare;
	}
	if (pxa > kBoardRadius || pza > kBoardRadius) 
		if (pxa < kBoardRadius+kBorderWidth+.1f 
		 && pza < kBoardRadius+kBorderWidth+.1f)
			return kBorderRegion;
		else
			return kInvalidSquare;

	int row	= static_cast<int>((kBoardRadius-pz)/10.0f);
	int col = static_cast<int>((px+kBoardRadius)/10.0f);

	return (row<<3)|col;
}

- (void) snapToSquare:(MBCPosition *)position
{
#if 0
	GLfloat	&	px	= (*position)[0];
	GLfloat	&	py	= (*position)[1];
	GLfloat & 	pz	= (*position)[2];
	GLfloat 	pxa = fabs(px);
	GLfloat 	pza = fabs(pz);
	
	py 		= 0.0f;
	if (pxa < kBoardRadius && pza < kBoardRadius) {
		const float kRes	= 10.0f;
		const float kRes2	= kRes / 2.0f;
		//
		// Within board, snap to square
		//
		px = copysignf(floorf(pxa/kRes)*kRes+kRes2, px);
		pz = copysignf(floorf(pza/kRes)*kRes+kRes2, pz);
	}
#else
	(*position)[1] = 0.0f;
#endif
}

- (MBCPosition)	squareToPosition:(MBCSquare)square
{
	MBCPosition	pos;

	if (square > kInHandSquare) {
		pos[0] = 44.0f;
		pos[1] = 0.0f;
		pos[2] = Color(square-kInHandSquare) == kBlackPiece ? -20.0f : 20.0f;
	} else {
		pos[0] = (square&7)*10.0f-35.0f;
		pos[1] = 0.0f;
		pos[2] = 35.0f - (square>>3)*10.0f;
	}

	return pos;
}

- (BOOL) facingWhite
{
	return fAzimuth > 90.0f && fAzimuth <= 270.0f;
}

- (MBCSide) facing
{
	if (fAzimuth > 95.0f && fAzimuth < 265.0f)
		return kWhiteSide;
	else if (fAzimuth < 85.0f || fAzimuth > 275.0f)
		return kBlackSide;
	else
		return kNeitherSide;
}

- (void) wantMouse:(BOOL)wantIt
{
	fWantMouse	= wantIt;
}

- (void) startAnimation
{
	fInAnimation	= true;
}

- (void) animationDone
{
	fInAnimation	= false;
}

@end

// Local Variables:
// mode:ObjC
// End:
