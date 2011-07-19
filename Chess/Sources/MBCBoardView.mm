/*
	File:		MBCBoardView.mm
	Contains:	General view handling infrastructure
	Version:	1.0
	Copyright:	© 2002-2008 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				Matthias Neeracher    x43683

	Writers:

		(MN)	Matthias Neeracher

	Change History (most recent first):

		$Log: MBCBoardView.mm,v $
		Revision 1.37  2011/03/14 21:10:27  neerache
		<rdar://problem/9129384> HiDPI adoption for Chess.app
		
		Revision 1.36  2010/08/10 20:28:10  neerache
		<rdar://problem/7335693> CrashTracer: 54 crashes in Chess at com.apple. -[MBCBoardView initWithFrame:]
		
		Revision 1.35  2008/10/24 21:39:06  neerache
		Insist on acceleration for fancy graphics
		
		Revision 1.34  2008/10/24 20:06:17  neerache
		<rdar://problem/3710028> ER: Chessboard anti-aliasing
		
		Revision 1.33  2008/06/03 17:29:49  neerache
		<rdar://problem/5793568> - Chess needs to add NSOpenGLPFAAllowOfflineDisplays pixel format attribute to OpenGL contexts
		
		Revision 1.32  2007/03/02 23:06:00  neerache
		<rdar://problem/4038207> Allow the user to type in a move in Chess
		
		Revision 1.31  2006/10/09 21:05:01  neerache
		Correct HiDPI problems <rdar://problem/4412218>
		
		Revision 1.30  2006/07/14 04:11:23  neerache
		Reset move arrows <rdar://problem/4376681>
		
		Revision 1.29  2005/06/17 22:26:31  neerache
		gcc4 enforces language rules more strictly
		
		Revision 1.28  2004/08/16 07:50:55  neerache
		Support accessibility
		
		Revision 1.27  2003/07/14 23:21:49  neerache
		Move promotion defaults into MBCBoard
		
		Revision 1.26  2003/07/07 09:16:42  neerache
		Textured windows are too slow for low end machines, disable
		
		Revision 1.25  2003/07/07 08:47:53  neerache
		Switch to textured main window
		
		Revision 1.24  2003/06/30 05:16:30  neerache
		Transfer move execution to Controller
		
		Revision 1.23  2003/06/16 02:18:03  neerache
		Implement floating board
		
		Revision 1.22  2003/06/15 21:13:09  neerache
		Adjust lights, fix animation, work on other drawing issues
		
		Revision 1.21  2003/06/05 08:31:26  neerache
		Added Tuner
		
		Revision 1.20  2003/06/04 23:14:05  neerache
		Neater manipulation widget; remove obsolete graphics options
		
		Revision 1.19  2003/06/04 09:25:47  neerache
		New and improved board manipulation metaphor
		
		Revision 1.18  2003/06/02 05:44:48  neerache
		Implement direct board manipulation
		
		Revision 1.17  2003/06/02 04:21:40  neerache
		Start implementing drawing styles for board elements
		
		Revision 1.16  2003/05/24 20:28:27  neerache
		Address race conditions between ploayer and engine
		
		Revision 1.15  2003/05/23 03:22:16  neerache
		Add FPS computation
		
		Revision 1.14  2003/05/05 23:50:40  neerache
		Tweak appearance, add border, add animations
		
		Revision 1.13  2003/05/02 01:16:33  neerache
		Simplify drawing methods
		
		Revision 1.12  2003/04/28 22:13:25  neerache
		Eliminate drawBoardPlane
		
		Revision 1.11  2003/04/25 22:26:23  neerache
		Simplify mouse model, fix startup bug
		
		Revision 1.10  2003/04/24 23:20:35  neeri
		Support pawn promotions
		
		Revision 1.9  2003/04/02 18:45:57  neeri
		Experiment with different snap techniques
		
		Revision 1.8  2003/03/28 01:31:07  neeri
		Support hints, last move
		
		Revision 1.7  2002/12/04 02:30:50  neeri
		Experiment (unsuccessfully so far) with ways to speed up piece movement
		
		Revision 1.6  2002/10/15 22:49:39  neeri
		Add support for texture styles
		
		Revision 1.5  2002/10/08 23:02:54  neeri
		Rotated board, changeable colors
		
		Revision 1.4  2002/09/13 23:57:05  neeri
		Support for Crazyhouse display and mouse
		
		Revision 1.3  2002/09/12 17:46:46  neeri
		Introduce dual board representation, in-hand pieces
		
		Revision 1.2  2002/08/26 23:11:17  neeri
		Switched to Azimuth/Elevation based Camera positioning model
		
		Revision 1.1  2002/08/22 23:47:06  neeri
		Initial Checkin
		
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

- (id) initWithFrame:(NSRect)rect
{
	float	light_ambient		= 0.125f;
	GLfloat light_pos[4] 		= { -60.0, 200.0, 0.0, 0.0};
	
	//
	// We first try to enable Full Scene Anti Aliasing if our graphics
	// hardware lets use get away with it.
	//
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
		NSOpenGLPFASamples, 		(NSOpenGLPixelFormatAttribute)4,
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
		[[NSOpenGLPixelFormat alloc] initWithAttributes:fsaa_attr];
	if (!(fHasFSAA = pixelFormat != 0))
		pixelFormat = 
			[[NSOpenGLPixelFormat alloc] initWithAttributes:jaggy_attr];
    [super initWithFrame:rect pixelFormat:pixelFormat];
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
	[self loadColors];
	[self generateModelLists];

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
	memcpy(fLightPos, light_pos, sizeof(fLightPos));
	fKeyBuffer			= 0;

	fHandCursor			= [[NSCursor alloc]
							  initWithImage:[NSImage imageNamed:@"handCursor"]
							  hotSpot:NSMakePoint(0.5f, 0.75f)];
	fArrowCursor		= [[NSCursor arrowCursor] retain];

    [self setWantsBestResolutionOpenGLSurface:YES];

    return self;
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
	fBoard			= [fController board];
	fInteractive	= [fController interactive];
	
	const float	kUserSpaceScale = 1.0f / [self convertSizeToBacking:NSMakeSize(1, 1)].width;
		
	[self scaleUnitSquareToSize:NSMakeSize(kUserSpaceScale, kUserSpaceScale)];
}

- (BOOL) isOpaque
{
	return YES;
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

- (void) showHint:(MBCMove *)move
{
	[move retain];
	[fHintMove release];
	fHintMove	= move;
	[self setNeedsDisplay:YES];
}

- (void) showLastMove:(MBCMove *)move
{
	[move retain];
	[fLastMove release];
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
