/*
    File:        MBCBoardMTLView.mm
    Contains:    Render and manipulate the Metal chess board.
    Copyright:    © 2003-2024 by Apple Inc., all rights reserved.

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

#import "MBCBoardMTLView.h"
#import "MBCBoardCommon.h"
#import "MBCBoardWin.h"
#import "MBCDrawStyle.h"
#import "MBCMetalCamera.h"
#import "MBCMetalMaterials.h"
#import "MBCMetalRenderer.h"

/*!
 @abstract MBCBoardView implementation is distributed across several other files
 */
#import "MBCBoardMTLViewDraw.h"

#import <simd/simd.h>

const float kClearColorRed     = 0.f;
const float kClearColorGreen   = 0.f;
const float kClearColorBlue    = 0.f;

/*!
 @abstract initialize with more than initial piece count since can have more pieces if playing crazy house
 */
const NSUInteger kInitialInstancePoolCapacity = 40;

/*!
 @abstract Following variables are used to initialize the instances for drawing in-hand piece counts
 for crazy house game variant on border.
 */
const int kInHandTotalPieceCount    = 10;
const int kInHandPerSidePieceCount  = 5;
const float kInHandLabelZ           = 6.0f;
const float kInHandLabelInitialZ    = kInHandLabelZ + kBorderLabelSize * 0.5f;
const float kInHandLabelZOffset     = kInHandPieceSize;

/*!
 @abstract Edge notation labels (1 - 8, A - H) align with centers of squares. Square centers are horizontally
 and vertically 10 units apart, where center values for XY are -35, -25, -15, -5, 5, 15, 25, 35.
 */
const float kEdgeNotationLabelStart     = 35.f;
const float kEdgeNotationLabelOffset    = 10.f;
const int kEdgeNotationLabelCount       = 16;
const int kEdgeNotationAlphaStartIndex  = 8;

@implementation MBCBoardMTLView

@synthesize boardReflectivity=_boardReflectivity;

- (instancetype)initWithFrame:(NSRect)rect {
    self = [super initWithFrame:rect];
    if (self) {
        self.clearColor = MTLClearColorMake(kClearColorRed, kClearColorGreen, kClearColorBlue, 1.0);

        _inAnimation = NO;
        _inBoardManipulation = NO;
        _pickedSquare = kInvalidSquare;
        _selectedPiece = EMPTY;
        _selectedDestination = kInvalidSquare;
        _wantMouse = NO;
        
        _keyBuffer = 0;
        
        _pointingHandCursor = [NSCursor pointingHandCursor];
        _grabbingHandCursor = [NSCursor closedHandCursor];
        _arrowCursor = [NSCursor arrowCursor];
        
        NSMutableArray *pool = [[NSMutableArray alloc] initWithCapacity:kInitialInstancePoolCapacity];
        for (int i = 0; i < 32; ++i) {
            MBCPieceInstance *instance = [[MBCPieceInstance alloc] init];
            [pool addObject:instance];
        }
        _piecePool = pool;
        
        _anisotropy = 4.f;
        _boardReflectivity = 0.3f;

        _currentPoolIndex = 0;
        _whitePieceInstances = [[NSMutableArray alloc] initWithCapacity:7];
        _blackPieceInstances = [[NSMutableArray alloc] initWithCapacity:7];
        [self initializePerFramePieceInstanceArray:_whitePieceInstances];
        [self initializePerFramePieceInstanceArray:_blackPieceInstances];
        
        _pieceSelectionInstance = [[MBCBoardDecalInstance alloc] initWithPosition:simd_make_float3(0.f)];
        _pieceSelectionInstance.quadVertexScale = 5.f;
        _pieceSelectionInstance.animateScale = YES;
        
        _performLabelFlip = YES;
        
        // Initialize the edge notation labels (rows 1 - 8, columns A - H)
        NSUInteger capacity = kEdgeNotationLabelCount + kInHandTotalPieceCount;
        _labelInstances = [[NSMutableArray alloc] initWithCapacity:capacity];
        [self initializeEdgeNotationLabels];
        _drawEdgeNotationLabels = YES;
        
        _needsRender = YES;
        
        [self updateTrackingAreas];
    }
    return self;
}

/*!
 @abstract initializePerFramePieceInstanceArray:
 @param instances one of _whitePieceInstances or _blackPieceInstances
 @discussion An instances array tracks all MBCPieceInstance per frame to identify the position and type of each piece to render.
 These are nested arrays, where each entry correlates to indexes from MBCPieceCode enum (EMPTY, KING, QUEEN, BISHOP,
 KNIGHT, ROOK, PAWN). The nested array contains the array of instances of each piece type per frame.
 */
- (void)initializePerFramePieceInstanceArray:(NSMutableArray *)instances {
    // Initialize each array capacity for piece type with the starting piece
    // counts for a new game // [PIECE_TYPE (Initial Count)]
    [instances addObject:[NSNull null]];                         // EMPTY  (0)
    [instances addObject:[NSMutableArray arrayWithCapacity:1]];  // KING   (1)
    [instances addObject:[NSMutableArray arrayWithCapacity:1]];  // QUEEN  (1)
    [instances addObject:[NSMutableArray arrayWithCapacity:2]];  // BISHOP (2)
    [instances addObject:[NSMutableArray arrayWithCapacity:2]];  // KNIGHT (2)
    [instances addObject:[NSMutableArray arrayWithCapacity:2]];  // ROOK   (2)
    [instances addObject:[NSMutableArray arrayWithCapacity:8]];  // PAWN   (8)
}

- (void)updateTrackingAreas {
    [super updateTrackingAreas];
    
    [self removeTrackingArea:_trackingArea];
    _trackingArea = [[NSTrackingArea alloc] initWithRect:[self bounds]
                                                 options: (NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved | NSTrackingActiveInKeyWindow)
                                                   owner:self 
                                                userInfo:nil];
    [self addTrackingArea:_trackingArea];
}

- (void)pickPixelFormat:(BOOL)afterFailure {
    
}

- (void)setStyleForBoard:(NSString *)boardStyle pieces:(NSString *)pieceStyle {
    if ([boardStyle isEqualToString:@"Grass"]) {
        // No longer using Grass board with Metal renderer, will default to Wood.
        boardStyle = @"Wood";
    }
    
    // Only one popup menu is visible for Metal implementation.
    [self.renderer loadMaterialsForNewStyle:boardStyle];

    [self needsUpdate];
}

- (void)awakeFromNib {
    _controller = (MBCBoardWin *)[[self window] windowController];
    _board = [_controller board];
    _interactive = [_controller interactive];
}

- (BOOL)isOpaque {
    return NO;
}

- (BOOL)mouseDownCanMoveWindow {
    return NO;
}

- (BOOL)shouldUpdateAndRenderScene {
    /* 
     Will need to update and render when:
     - Explicitly set _needsRender
     - Have an active MBCAnimation (moving piece or board for example)
     - Need to show animated selection image
     - Showing either the hint or last move arrow
     */
    return _needsRender || _inAnimation || (_pickedSquare != kInvalidSquare) || _hintMove || _lastMove;
}

- (void)drawMetalContent {
    @autoreleasepool {
        if ([self shouldUpdateAndRenderScene]) {
            [self prepareSceneDataForRenderer];
            
            [_renderer drawSceneToView];
        }
    }
    _needsRender = NO;
}

- (void)drawNow {
    _needsRender = YES;
}

- (void)profileDraw {
    dispatch_apply(100, dispatch_get_main_queue(), ^(size_t) {
        [self drawNow];
    });
}

- (void)needsUpdate {
    _needsRender = YES;
}

- (void)reshape {
    [self needsUpdate];
}

- (void)endGame {
    _selectedPiece = EMPTY;
    _pickedSquare = kInvalidSquare;
    _wantMouse = NO;
    [self hideMoves];
    [self needsUpdate];
}

- (void)startGame:(MBCVariant)variant playing:(MBCSide)side {
    _variant = variant;
    _side = side;
    [self endGame];
    if (side != kNeitherSide && [self facing] != kNeitherSide) {
        //
        // We have humans involved, turn board right side up unless
        // it was in a neutral position
        //
        if (side == kBothSides) {
            side = ([_board numMoves] & 1) ? kBlackSide : kWhiteSide;
        }
        if ([self facing] != side) {
            self.azimuth = fmod(self.azimuth + 180.0f, 360.0f);
        }
    }
    
    // Initialize the crazy house count labels if needed
    if (_variant == kVarCrazyhouse) {
        [self initializeCrazyHouseLabels];
    }
}

- (void)initializeEdgeNotationLabels {
    for (int labelIndex = 0; labelIndex < kEdgeNotationLabelCount; ++labelIndex) {
        
        vector_float3 position = simd_make_float3(-kBorderLabelCenterX, MBC_POSITION_Y_LABELS, kBorderLabelCenterZ);
        if (labelIndex < kEdgeNotationAlphaStartIndex) {
            // One of 1 - 8 labels
            position.z = kEdgeNotationLabelStart - (labelIndex * kEdgeNotationLabelOffset);
        } else {
            // One of A - H labels
            position.x = -kEdgeNotationLabelStart + ((labelIndex % 8) * kEdgeNotationLabelOffset);
        }
        
        MBCBoardDecalInstance *label = [[MBCBoardDecalInstance alloc] initWithPosition:position];
        label.uvScale = 0.25;
        label.uvOrigin = simd_make_float2(0.25f, 0.25f);
        label.quadVertexScale = kBorderLabelSize;
        label.visible = YES;
        [_labelInstances addObject:label];
        
        // The texture for digits is a 4x4 grid.  Will need to compute the uv
        // offset in order sample the region of the texture corresponding to value.
        NSInteger row = labelIndex / 4;
        NSInteger column = labelIndex % 4;
        label.uvOrigin = simd_make_float2(0.25f * (float)column, 0.25f * (float)row);
    }
    [self.renderer setLabelInstances:_labelInstances];
}

- (void)initializeCrazyHouseLabels {
    for (int pieceIndex = 0; pieceIndex < kInHandTotalPieceCount; ++pieceIndex) {
        float currentZ;
        if (pieceIndex < kInHandPerSidePieceCount) {
            currentZ = kInHandLabelInitialZ + (pieceIndex * kInHandLabelZOffset);
        } else {
            int blackIndex = pieceIndex % kInHandPerSidePieceCount;
            currentZ = -kInHandLabelInitialZ - (blackIndex * kInHandLabelZOffset);
        }
        vector_float3 position = simd_make_float3(kBorderLabelCenterX, MBC_POSITION_Y_LABELS, currentZ);
        MBCBoardDecalInstance *label = [[MBCBoardDecalInstance alloc] initWithPosition:position];
        label.uvScale = 0.25;
        label.uvOrigin = simd_make_float2(0.25f, 0.25f);
        label.quadVertexScale = kBorderLabelSize;
        [_labelInstances addObject:label];
    }
}

- (void)showMoveAsHint:(MBCMove *)move {
    _hintMove = move;
    
    MBCPiece hintPiece = _hintMove->fPiece;
    if (Piece(hintPiece) == EMPTY) {
        // The fLastPonder in MBCEngine handlePortMessage: is created from an
        // MBCCompactMove, which may not assign the piece in initFromCompactMove.
        _hintMove->fPiece = [_board curContents:_hintMove->fFromSquare];;
    }
    
    [self needsUpdate];
}

- (void)showMoveAsLast:(MBCMove *)move {
    _lastMove = move;
    
    [self needsUpdate];
}

- (void)hideMoves {
    _hintMove = nil;
    _lastMove = nil;
    _lastMoveArrowInstance = nil;
    _hintMoveArrowInstance = nil;
    
    [self.renderer setHintMoveInstance:nil lastMoveInstance:nil];
}

- (void)clickPiece {
    _pickedSquare = _selectedSquare;
    _pieceSelectionInstance.quadVertexScale = kPieceSelectionScales[Piece(_selectedPiece)];

    [self unselectPiece];
}

- (void)selectPiece:(MBCPiece)piece at:(MBCSquare)square {
    _pickedSquare = kInvalidSquare;
    _selectedPiece = piece;
    _selectedSquare = square;

    if (square != kInvalidSquare) {
        _selectedPosition = [self squareToPosition:square];
    }

    [self needsUpdate];
}

- (void)selectPiece:(MBCPiece)piece at:(MBCSquare)square to:(MBCSquare)dest {
    _selectedDestination = dest;

    [self selectPiece:piece at:square];
}

- (void)moveSelectionTo:(MBCPosition *)position {
    _selectedPosition = *position;
    
    [self needsUpdate];
}

- (void)unselectPiece {
    _selectedPiece = EMPTY;
    _selectedSquare = kInvalidSquare;
    _selectedDestination = kInvalidSquare;

    [self needsUpdate];
}

- (MBCSquare)positionToSquare:(const MBCPosition *)position {
    float positionX = (*position)[0];
    float positionZ = (*position)[2];
    float absPositionX = fabs(positionX);
    float absPositionZ = fabs(positionZ);
    
    if (fabs(positionX - kInHandPieceXMTL) < (kInHandPieceSize / 2.0f)) {
        // Position may be one of in hand piece squares that are located to
        // the side of the board in Crazy House game variant.
        absPositionZ -= kInHandPieceZOffset;
        if (absPositionZ > 0.0f && absPositionZ < kInHandPieceSize * 5.0f) {
            MBCPieceCode piece = gMTLInHandOrder[(int)(absPositionZ / kInHandPieceSize)];
            return kInHandSquare + (positionZ < 0 ? Black(piece) : White(piece));
        } else {
            return kInvalidSquare;
        }
    }
    
    if (absPositionX > kBoardRadius || absPositionZ > kBoardRadius) {
        // Clicking outside of the board squares
        return kInvalidSquare;
    }
    
    // Square x or y center position values can be one of
    // -35, -25, -15, -5, 5, 15, 25, 35
    // row and column can be in range [0, 7]
    int row = static_cast<int>((kBoardRadius - positionZ) / 10.0f);
    int column = static_cast<int>((positionX + kBoardRadius) / 10.0f);
    
    return (row << 3) | column;
}

- (MBCSquare)positionToSquareOrRegion:(const MBCPosition *)position {
    float positionX = (*position)[0];
    float positionZ = (*position)[2];
    float absPositionX = fabs(positionX);
    float absPositionZ = fabs(positionZ);
    
    if (_promotionSide == kWhiteSide) {
        // Check if position is the white promotion square.
        if (fabs(positionX + kPromotionPieceX) < 8.0f && fabs(positionZ + kPromotionPieceZ) < 8.0f) {
            return kWhitePromoSquare;
        }
    } else if (_promotionSide == kBlackSide) {
        // Check if position is the black promotion square.
        if (fabs(positionX - kPromotionPieceX) < 8.0f && fabs(positionZ - kPromotionPieceZ) < 8.0f) {
            return kBlackPromoSquare;
        }
    }

    if (fabs(positionX - kInHandPieceXMTL) < (kInHandPieceSize / 2.0f)) {
        // Position may be one of in hand piece squares that are located to
        // the side of the board in Crazy House game variant.
        absPositionZ -= kInHandPieceZOffset;
        if (absPositionZ > 0.0f && absPositionZ < kInHandPieceSize * 5.0f) {
            MBCPieceCode piece = gMTLInHandOrder[(int)(absPositionZ / kInHandPieceSize)];
            return kInHandSquare + (positionZ < 0 ? Black(piece) : White(piece));
        } else {
            return kInvalidSquare;
        }
    }
    
    if (absPositionX > kBoardRadius || absPositionZ > kBoardRadius) {
        if (absPositionX < kBoardRadius + kBorderWidthMTL + 0.1f && absPositionZ < kBoardRadius + kBorderWidthMTL + 0.1f) {
            return kBorderRegion;
        } else {
            return kInvalidSquare;
        }
    }

    // Square x or y center position values can be one of
    // -35, -25, -15, -5, 5, 15, 25, 35
    // row and column can be in range [0, 7]
    int row = static_cast<int>((kBoardRadius - positionZ) / 10.0f);
    int column = static_cast<int>((positionX + kBoardRadius) / 10.0f);

    return (row << 3) | column;
}

- (void)snapToSquare:(MBCPosition *)position {
    (*position)[1] = 0.0f;
}

- (MBCPosition)squareToPosition:(MBCSquare)square {
    MBCPosition pos;

    if (square > kInHandSquare) {
        pos[0] = 44.0f;
        pos[1] = 0.0f;
        pos[2] = Color(square - kInHandSquare) == kBlackPiece ? -20.0f : 20.0f;
    } else {
        // Board squares can have values in range [0, 63]
        // Square x or y center position values can be one of
        // -35, -25, -15, -5, 5, 15, 25, 35

        // Get the column number, which will be 0 to 7.
        pos[0] = (square & 7) * 10.0f - 35.0f;
        
        // On surface of board, Y value is 0.0
        pos[1] = 0.0f;
        
        // Shift right by 3 will divide by 8.
        pos[2] = 35.0f - (square >> 3) * 10.0f;
    }

    return pos;
}

- (BOOL)facingWhite {
    const float azimuth = self.azimuth;
    return azimuth > 90.0f && azimuth <= 270.0f;
}

- (MBCSide)facing {
    const float azimuth = self.azimuth;
    
    if (azimuth > 95.0f && azimuth < 265.0f) {
        return kWhiteSide;
    } else if (azimuth < 85.0f || azimuth > 275.0f) {
        return kBlackSide;
    } else {
        return kNeitherSide;
    }
}

- (void)wantMouse:(BOOL)wantIt {
    _wantMouse = wantIt;
}

- (void)startAnimation {
    _inAnimation = YES;
}

- (void)animationDone {
    _inAnimation = NO;
}

- (IBAction)increaseFSAA:(id)sender {

}

- (IBAction)decreaseFSAA:(id)sender {

}

- (MBCDrawStyle *)boardDrawStyleAtIndex:(NSUInteger)index {
    return nil;
}

- (MBCDrawStyle *)pieceDrawStyleAtIndex:(NSUInteger)index {
    return nil;
}

- (MBCDrawStyle *)borderDrawStyle {
    return nil;
}

- (MBCDrawStyle *)selectedPieceDrawStyle {
    return nil;
}

- (void)setLightPosition:(vector_float3)lightPosition {
    
}

- (vector_float3)lightPosition {
    return {0.f, 0.f, 0.f};
}

- (void)setElevation:(float)elevation {
    _renderer.camera.elevation = elevation;
}

- (float)elevation {
    return _renderer.camera.elevation;
}

- (void)setAzimuth:(float)azimuth {
    if (!_performLabelFlip) {
        // Mouse events may fire faster than render fps, thus do not clear _performLabelFlip if set.
        const bool boardWasFlipped = [self willAzimuthRotateLabels:_renderer.camera.azimuth];
        const bool flipBoard = [self willAzimuthRotateLabels:azimuth];
        _performLabelFlip = boardWasFlipped != flipBoard;
    }
    
    _renderer.camera.azimuth = azimuth;
    [_renderer cameraDidRotateAboutYAxis];
}

- (float)azimuth {
    return _renderer.camera.azimuth;
}

- (BOOL)willAzimuthRotateLabels:(float)azimuth {
    return azimuth < 90.0f || azimuth >= 270.0f;
}

- (void)setDrawEdgeNotationLabels:(BOOL)drawEdgeNotationLabels {
    _drawEdgeNotationLabels = drawEdgeNotationLabels;
    
    for (int labelIndex = 0; labelIndex < kEdgeNotationLabelCount; ++labelIndex) {
        MBCBoardDecalInstance *instance = _labelInstances[labelIndex];
        instance.visible = drawEdgeNotationLabels;
    }
    [self.renderer setLabelInstances:_labelInstances];
}

#ifdef CHESS_TUNER
- (void)savePieceStyles { }
- (void)saveBoardStyles { }
#endif

@end
