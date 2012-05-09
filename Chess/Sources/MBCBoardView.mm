/*
	File:		MBCBoardView.mm
	Contains:	General view handling infrastructure
	Version:	1.0
	Copyright:	© 2002-2011 by Apple Computer, Inc., all rights reserved.
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

	fHandCursor			= [[NSCursor pointingHandCursor] retain];
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
