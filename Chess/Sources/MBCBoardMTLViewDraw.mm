/*
    File:        MBCBoardMTLViewDraw.mm
    Contains:    Manages the draw related data updates for Metal rendering.
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

#import "MBCBoardMTLViewDraw.h"
#import "MBCMathUtilities.h"
#import "MBCBoard.h"
#import "MBCDrawStyle.h"
#import "MBCMetalCamera.h"
#import "MBCMetalRenderer.h"

MBCPieceCode gMTLInHandOrder[] = {PAWN, BISHOP, KNIGHT, ROOK, QUEEN};

/*!
 @abstract Spacing between centers of consecutive in-hand pieces in crazy house game
 */
const float kInHandZOffset = kInHandPieceSize;

/*!
 @abstract Starting z position for first  in-hand piece (PAWN) in crazy house game
 */
const float kInHandPieceZ = kInHandPieceZOffset + kInHandPieceSize * 0.5f;

/*!
 @abstract Scale factor for in-hand piece drawn on side of board
 */
const float kInHandPieceScale = 0.95f;

/*!
 @abstract The digit texture is a 4x4 grid, thus UVs will be modified from [0.f, 1.f] range to a subregion that is 1/4 of this
 in order to sample the correct digit value within the grid.
 */
const float kDigitGridUVOffset = 0.25f;
const float kDigitGridUVScale = 0.25f;

/*!
 @abstract Colors used to multiply selection ring texture by when selected pieces is on either white or black square.
 This allows for the ring to have more contrast when on a white square.
 */
const vector_float3 kSelectionColorWhiteSquare = simd_make_float3(0.86f, 0.86f, 0.86f);
const vector_float3 kSelectionColorBlackSquare = simd_make_float3(0.9f, 0.9f, 0.9f);

/*!
 @abstract IsPieceRenderable
 @param piece The complete piece code to examine
 @discussion Will return true if the piece has valid value and is not EMPTY
 */
inline bool IsPieceRenderable(MBCPiece piece) {
    return piece && Piece(piece) != EMPTY;
}

@implementation MBCBoardMTLView (Draw)

- (void)resetPerFrameData {
    // Instance data objects from pool reused each frame.
    _currentPoolIndex = 0;
    
    // Clear last frame piece instance data, start at 1 because 0 is EMPTY.
    for (int i = 1; i < _whitePieceInstances.count; ++i) {
        [_whitePieceInstances[i] removeAllObjects];
    }
    for (int i = 1; i < _blackPieceInstances.count; ++i) {
        [_blackPieceInstances[i] removeAllObjects];
    }
    _transparentInstance = nil;
}

- (void)prepareSceneDataForRenderer {
    [self resetPerFrameData];
    
    if (self.performLabelFlip) {
        [self updateEdgeNotationLabels];
    }
    if (_variant == kVarCrazyhouse) {
        [self drawPiecesInHand];
    }
    
    [self drawPieces];
    
    if (IsPieceRenderable(_selectedPiece)) {
        [self drawSelectedPiece];
    }
    
    // If have a transparent instance, append to the back of the array for piece type. At most
    // one transparent piece per frame when moving a piece over another piece.
    if (_transparentInstance) {
        if (_transparentInstance.color == kWhitePiece) {
            [_whitePieceInstances[_transparentInstance.type] addObject:_transparentInstance];
        } else {
            [_blackPieceInstances[_transparentInstance.type] addObject:_transparentInstance];
        }
    }
    
    [self.renderer setWhitePieceInstances:_whitePieceInstances
                      blackPieceInstances:_blackPieceInstances
                      transparentInstance:_transparentInstance];

    [self drawPromotionPiece];
    
    [self drawMove:_hintMove asHint:YES];
    [self drawMove:_lastMove asHint:NO];
    
    [self placePieceSelection];
}

#pragma mark - Piece Drawing

/* Creates an instance object used to pass render data to renderer */
- (void )simplyDrawPiece:(MBCPiece)piece at:(MBCPosition)pos scale:(float)scale alpha:(float)alpha {
    
    if (_currentPoolIndex >= _piecePool.count) {
        [_piecePool addObject:[[MBCPieceInstance alloc] init]];
    }
    MBCPieceInstance *instance = _piecePool[_currentPoolIndex];
    
    instance.type = Piece(piece);
    instance.color = Color(piece);
    instance.position = simd_make_float4(pos[0], pos[1], pos[2], 1.f);
    instance.scale = scale;
    instance.alpha = alpha;
    
    // Move to next item in piece pool.
    _currentPoolIndex += 1;
    
    if (alpha < 1.f) {
        _transparentInstance = instance;
    } else {
        if (instance.color == kWhitePiece) {
            [_whitePieceInstances[instance.type] addObject:instance];
        } else {
            [_blackPieceInstances[instance.type] addObject:instance];
        }
    }
}

- (void)drawPiece:(MBCPiece)piece at:(MBCPosition)pos alpha:(float)alpha {
    [self simplyDrawPiece:piece at:pos scale:1.0f alpha:alpha];
}

- (void)drawPiece:(MBCPiece)piece at:(MBCPosition)pos {
    [self simplyDrawPiece:piece at:pos scale:1.0f alpha:1.f];
}

- (void)drawPiece:(MBCPiece)piece at:(MBCPosition)pos scale:(float)scale {
    [self simplyDrawPiece:piece at:pos scale:scale alpha:1.f];
}

/* 
 Pass the frame drawing data for pieces to the renderer.  Data includes
 transform data and color data for each piece to render.
 */
- (void)drawPieces {
    for (MBCSquare square = 0; square<64; ++square) {
        MBCPiece piece = _inAnimation ? [_board oldContents:square] : [_board curContents:square];
        
        if (_selectedPiece && square == _selectedSquare) {
            // Skip original position of selected piece
            continue;
        }
        
        if (IsPieceRenderable(piece)) {
            const MBCPosition pos = [self squareToPosition:square];
            const float distance =
                _selectedPiece && (!_inAnimation || square == _selectedDestination)
                ? _selectedPosition.FlatDistance(pos)
                : 100.0f;
            
            const float kProximity = 5.0f;
            if (distance < kProximity) {
                // If another piece is within proximity of moving piece then fade non-moving piece
                const float alpha = pow(distance / kProximity, 4.f);
                [self drawPiece:piece at:pos alpha:alpha];
            }
            else {
                [self drawPiece:piece at:pos];
            }
        }
    }
}

/*!
 Draw the selected piece (may be off grid)
 */
- (void)drawSelectedPiece {
    [self drawPiece:_selectedPiece at:_selectedPosition];
}

/*! 
 Draw the promotion piece, which appears when pawn is in position to enter square at other end of board.
 The promotion piece appears so user may decide with type of piece to use for replacing pawn.
 */
- (void)drawPromotionPiece {
    MBCPiece piece = EMPTY;
    MBCPosition position;

    _promotionSide = kNeitherSide;

    if (_side == kWhiteSide || _side == kBothSides) {
        if ([_board canPromote:kWhiteSide]) {
            // Stored as one of KING, QUEEN, BISHOP, KNIGHT, ROOK, PAWN without color.
            piece = White((MBCPieceCode)[_board defaultPromotion:YES]);
            _promotionSide = kWhiteSide;
            position[0] = -kPromotionPieceX;
            position[1] = 0.0f;
            position[2] = -kPromotionPieceZ;
        }
    }
    if (_side == kBlackSide || _side == kBothSides) {
        if ([_board canPromote:kBlackSide]) {
            // Stored as one of KING, QUEEN, BISHOP, KNIGHT, ROOK, PAWN without color.
            piece = Black((MBCPieceCode)[_board defaultPromotion:NO]);
            _promotionSide = kBlackSide;
            position[0] = kPromotionPieceX;
            position[1] = 0.0f;
            position[2] = kPromotionPieceZ;
        }
    }
    
    if (_promotionSide == kNeitherSide) {
        return;
    }
    
    [self drawPiece:piece at:position];
}

- (void)placePieceSelection {
    BOOL updateRenderer = NO;
    if (_pickedSquare != kInvalidSquare) {
        if (!_pieceSelectionInstance.visible) {
            // Place and turn on piece selection
            MBCPosition pickedPos = [self squareToPosition:_pickedSquare];
            vector_float3 position = { pickedPos[0], MBC_POSITION_Y_PIECE_SELECTION, pickedPos[2] };
            
            const unsigned zeroIndexRow = Row(_pickedSquare) - 1;
            const unsigned column = _pickedSquare & 7;
            BOOL firstSquareInRowIsBlack = (zeroIndexRow & 0x01) == 0;
            BOOL isBlackSquare = firstSquareInRowIsBlack ? (column & 0x01) != 1 : (column & 0x01) == 1;
            
            _pieceSelectionInstance.visible = YES;
            _pieceSelectionInstance.position = position;
            _pieceSelectionInstance.color = isBlackSquare ? kSelectionColorBlackSquare : kSelectionColorWhiteSquare;
            updateRenderer = YES;
        }
    } else {
        // Turn off piece selection
        _pieceSelectionInstance.visible = NO;
        updateRenderer = YES;
    }
    if (updateRenderer) {
        [self.renderer setPieceSelectionInstance:_pieceSelectionInstance];
    }
}

- (void)updateEdgeNotationLabels {
    // If board is rotated leaving digit upside down, will need to rotate the labels
    const bool rotateLabel = [self willAzimuthRotateLabels:self.azimuth];
    
    for (int labelIndex = 0; labelIndex < 16; ++labelIndex) {
        MBCBoardDecalInstance *label = _labelInstances[labelIndex];
        label.rotate = rotateLabel;
        
        if (labelIndex >= 8) {
            vector_float3 position = label.position;
            position.z = rotateLabel ? -kBorderLabelCenterZ : kBorderLabelCenterZ;
            label.position = position;
        }
    }
    [self.renderer setLabelInstances:_labelInstances];
    self.performLabelFlip = NO;
}

/*!
 Draw the pieces on side of board if playing Crazy House game variant.
 */
- (void)drawPiecesInHand {
    // If board is rotated leaving number upside down, will need to rotate the labels
    const bool rotateLabel = [self willAzimuthRotateLabels:self.azimuth];

    for (int pieceIndex = 0; pieceIndex < 10; ++pieceIndex) {
        // White side in-hand pieces are first 5, followed by the black side in-hand pieces.
        BOOL isWhitePiece = pieceIndex < 5;
        int currentColorIndex = isWhitePiece ? pieceIndex : pieceIndex % 5;
        MBCPiece piece = isWhitePiece ? White(gMTLInHandOrder[currentColorIndex]) : Black(gMTLInHandOrder[currentColorIndex]);
        
        int inHandCount = _inAnimation ? [_board oldInHand:piece] : [_board curInHand:piece];
        inHandCount -= (piece + kInHandSquare == _selectedSquare);
        inHandCount = MIN(inHandCount, 8);
        
        // Update the count label values
        int labelIndex = pieceIndex + 16;
        MBCBoardDecalInstance *label = _labelInstances[labelIndex];
        label.visible = inHandCount > 1;
        if (label.isVisible) {
            label.uvScale = kDigitGridUVScale;
            label.rotate = rotateLabel;
            
            // The texture for digits is a 4x4 grid.  Will need to compute the uv
            // offset in order sample the region of the texture corresponding to value.
            NSInteger row = (inHandCount - 1) / 4;
            NSInteger column = (inHandCount - 1) % 4;
            label.uvOrigin = simd_make_float2(kDigitGridUVOffset * (float)column, kDigitGridUVOffset * (float)row);
        }
        
        // Draw the piece if have in-hand
        if (inHandCount > 0) {
            float sign = (isWhitePiece) ? 1.f : -1.f;
            float zPosition = sign * (kInHandPieceZ + currentColorIndex * kInHandZOffset);
            MBCPosition pos = { {kInHandPieceXMTL, 0.f, zPosition} };
            [self drawPiece:piece at:pos scale:kInHandPieceScale];
        }
    }
    [self.renderer setLabelInstances:_labelInstances];
}

- (void)drawMove:(MBCMove *)move asHint:(BOOL)hint {
    if (!move) {
        return;
    }
    
    if ((hint && _hintMoveArrowInstance) ||
        (!hint && _lastMoveArrowInstance)) {
        // Already have instance for this type, they will be cleared when a new move is made.
        return;
    }

    MBCPosition fromPosition = [self squareToPosition:move->fFromSquare];
    MBCPosition toPosition = [self squareToPosition:move->fToSquare];

    MBCArrowInstance *arrow = [[MBCArrowInstance alloc] initWithFromPosition:fromPosition
                                                                  toPosition:toPosition
                                                                       piece:move->fPiece
                                                                      isHint:hint];
    
    if (hint) {
        _hintMoveArrowInstance = arrow;
    } else {
        _lastMoveArrowInstance = arrow;
    }
    
    [self.renderer setHintMoveInstance:_hintMoveArrowInstance
                      lastMoveInstance:_lastMoveArrowInstance];
}

@end
