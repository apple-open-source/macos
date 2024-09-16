/*
    File:        MBCBoardMTLView.h
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

#import "MBCBoardViewInterface.h"
#import "MBCBoardCommon.h"

#import <Cocoa/Cocoa.h>
#import <MetalKit/MetalKit.h>

@class MBCBoard;
@class MBCBoardWin;
@class MBCDrawStyle;
@class MBCInteractivePlayer;
@class MBCMetalRenderer;
@class MBCMove;

/*!
 @abstract Used for pieces that are "in hand" on side of board during crazy house game.
*/
extern MBCPieceCode gMTLInHandOrder[];

/*!
 @abstract Constant values used to initialize the edge notation labels for row and  column (1 - 8, A - H)
 */
const float kBorderLabelSize = 2.0f;
const float kBorderLabelX = 44.25f;
const float kBorderLabelCenterX = kBorderLabelX + kBorderLabelSize * 0.5f;
const float kBorderLabelCenterZ = kBorderLabelCenterX;

@interface MBCBoardMTLView : MTKView <MBCBoardViewInterface> {
    /*!
     @abstract Reference to the MBCBoard instance for game
     */
    MBCBoard *_board;
    
    /*!
     @abstract Reference to the MBCBoardWin window controller for game
     */
    MBCBoardWin *_controller;
    
    /*!
     @abstract The human player for the game
     */
    MBCInteractivePlayer *_interactive;

    /*!
     @abstract The dynamic azimuth value that changes during mouse drag computations
     */
    float _rawAzimuth;
    
    /*!
     @abstract Currently selected XYZ position in world space
     */
    MBCPosition _selectedPosition;
    
    /*!
     @abstract The last selected XYZ position
     */
    MBCPosition _lastSelectedPosition;
    
    /*!
     @abstract Will contain the square of a piece if user clicks occupying piece without dragging piece.
     */
    MBCSquare _pickedSquare;
    
    /*!
     @abstract Will contain the square of piece when user starts mouse down on piece
     */
    MBCSquare _selectedSquare;
    
    /*!
     @abstract The currently selected piece, set when user performs mouse down on piece
     */
    MBCPiece _selectedPiece;
    
    /*!
     @abstract Selected destination square for a moving piece
     */
    MBCSquare _selectedDestination;
    
    /*!
     @abstract Reference to instance of hand shaped cursor with pointing finger
     */
    NSCursor *_pointingHandCursor;
    
    /*!
     @abstract Reference to instance of hand shaped cursor with fingers closed
     */
    NSCursor *_grabbingHandCursor;
    
    /*!
     @abstract Reference to instance of arrow shaped cursor
     */
    NSCursor *_arrowCursor;
    
    /*!
     @abstract Stores the previous mouse move event's mouse position
     */
    NSPoint _previousMousePosition;
    
    /*!
     @abstract Current mouse position tracked during mouse moves
     */
    NSPoint _currentMousePosition;
    
    /*!
     @abstract Whether or not currently rotating the board (camera)
     */
    BOOL _inBoardManipulation;
    
    /*!
     @abstract Set if want to pass mouse clicks to interactive player
     */
    BOOL _wantMouse;
    
    /*!
     @abstract Whether or not in an active piece animation
     */
    BOOL _inAnimation;
    
    /*!
     @abstract Last time that mouse drag event was processed
     */
    struct timeval _lastMouseDragUpdate;
    
    /*!
     @abstract Indicates if update and render is needed on upcoming frame.
     */
    BOOL _needsRender;
    
    /*!
     @abstract MBCMove instance for hint move when activated to draw hint move arrow on board
     */
    MBCMove *_hintMove;
    
    /*!
     @abstract MBCMove instance for the last move taken when activated to draw last move arrow on board
     */
    MBCMove *_lastMove;
    
    /*!
     @abstract A pool of instance assets that are used each frame for updating dynamic piece data for rendering.
     Using this to avoid allocating same number of items that are needed each frame.
    */
    NSMutableArray<MBCPieceInstance *> *_piecePool;
    
    /*!
     @abstract Index to determine which instance to grab next from the pool
    */
    NSInteger _currentPoolIndex;
    
    /*!
     @abstract Tracks the white and black MBCPieceInstance objects per frame.  These are nested arrays, where each entry
     correlates to indexes from MBCPieceCode enum (EMPTY, KING, QUEEN, BISHOP, KNIGHT, ROOK, PAWN). The nested array
     contains the array of instances of each piece type per frame. They are cleared each frame as the count of each piece type may
     change from frame to frame. This data is used by MBCMetalRenderer to place instances of each piece.
    */
    NSMutableArray<NSMutableArray<MBCPieceInstance *> *> *_whitePieceInstances;
    NSMutableArray<NSMutableArray<MBCPieceInstance *> *> *_blackPieceInstances;
    
    /*!
     @abstract When dragging a chess piece, an overlapped piece will be drawn transparent. Tracking separately
     because transparent piece must be drawn in a render pass with alpha blending enabled. At most only one transparent
     piece exists at a time in current implementatoion
    */
    MBCPieceInstance *_transparentInstance;
    
    /*!
     @abstract Instance data for the hint and move arrows when needed to be displayed.
     */
    MBCArrowInstance *_hintMoveArrowInstance;
    MBCArrowInstance *_lastMoveArrowInstance;
    
    /*!
     @abstract Instance data for the piece selection graphic to render under selected piece.
     */
    MBCBoardDecalInstance *_pieceSelectionInstance;
    
    /*!
     @abstract Instance data for rendering in-hand piece count labels on side of board for the Crazy House game variant.
     */
    NSMutableArray<MBCBoardDecalInstance *> *_labelInstances;
    
    /*!
     @abstract The variant for the current game associated with the view
    */
    MBCVariant _variant;
    
    /*!
     @abstract Starting side for the game
    */
    MBCSide _side;
    
    /*!
     @abstract Side for an active piece promotion
    */
    MBCSide _promotionSide;
    
    /*!
     @abstract Anisotropy value read from material defaults
    */
    float _anisotropy;
    
    /*!
     @abstract Amount of reflection for board when enabled
    */
    float _boardReflectivity;
    
    /*!
     @abstract Tracks the last pressed key on keyboard
     */
    char _keyBuffer;
    
    /*!
     @abstract The tracking area for mouse events
     */
    NSTrackingArea *_trackingArea;
}

/*!
 @abstract Match the aspect ratio for the main window for resizing.
 This will maintain the aspect of the view when the game log is revealed.
 */
@property (nonatomic, strong) IBOutlet NSLayoutConstraint *aspectConstraint;

/*!
 @abstract The horizontal angle of the camera about the vertical (Y) axis of the board. Updated as drag the board to change viewing angle.
*/
@property (nonatomic, assign) float azimuth;

/*!
 @abstract The vertical angle of the camera relative to the horizontal plane of the board.
*/
@property (nonatomic, assign) float elevation;

/*!
 @abstract Will control the amount of reflection to use for reflecting pieces on the board.
*/
@property (nonatomic, assign) float boardReflectivity;

/*!
 @abstract The intensity that is used to draw the labels on board when labels drawn independent from board texture.
*/
@property (nonatomic, assign) float labelIntensity;

/*!
 @abstract The float value to be used for all ambient components for lighting.
*/
@property (nonatomic, assign) float ambient;

/*!
 @abstract The renderer that encapsulates all Metal rendering.
*/
@property (nonatomic, weak) MBCMetalRenderer *renderer;

/*!
 @abstract Indicates if camera position relative to board changed in a way where labels need to be rotated
 so that they are not rendered upside down.
 */
@property (nonatomic) BOOL performLabelFlip;

/*!
 @abstract Determines whether or not the rank (rows) and file (columns)  labels [1 - 8, A - H] are shown or hidden.
 */
@property (nonatomic) BOOL drawEdgeNotationLabels;

/*!
 @abstract drawMetalContent
 @discussion Called from MBCBoardWin (MTKViewDelegate) to render Metal content to view
 */
- (void)drawMetalContent;

/*!
 @abstract boardDrawStyleAtIndex:
 @param index The board draw style for white (0) and black (1) board squares
 @discussion Returns the MBCDrawStyle instance that represents the material attributes for corresponding index.
*/
- (MBCDrawStyle *)boardDrawStyleAtIndex:(NSUInteger)index;

/*!
 @abstract pieceDrawStyleAtIndex:
 @param index The piece draw style for white (0) and black (1) pieces
 @discussion Returns the MBCDrawStyle instance that represents the material attributes for corresponding index.
*/
- (MBCDrawStyle *)pieceDrawStyleAtIndex:(NSUInteger)index;

/*!
 @abstract borderDrawStyle:
 @discussion Returns the MBCDrawStyle instance that represents the material attributes for drawing the border geometry.
*/
- (MBCDrawStyle *)borderDrawStyle;

/*!
 @abstract selectedPieceDrawStyle
 @discussion Returns the MBCDrawStyle instance that represents the material attributes for selected promotion piece.
*/
- (MBCDrawStyle *)selectedPieceDrawStyle;

/*!
 @abstract setLightPosition:
 @param lightPosition is theXYZ position for the main directional light in the scene.
 @discussion Updates the position for the main directional light, which is only changed from the Chess Tuner.
*/
- (void)setLightPosition:(vector_float3)lightPosition;

/*!
 @abstract lightPosition
 @return The XYZ position for the main directional light in the scene.
 @discussion Called to get the current position for the main directional light.
*/
- (vector_float3)lightPosition;

#ifdef CHESS_TUNER
- (void)savePieceStyles;
- (void)saveBoardStyles;
#endif

/*!
 @abstract startGame:playing:
 @param variant The variant of the chess game to play
 @param side The side that is active
 @discussion This will start a new game with the specified variant, starting with side.
*/
- (void)startGame:(MBCVariant)variant playing:(MBCSide)side;

/*!
 @abstract drawNow
 @discussion Will trigger immediate redrawing for the view.
*/
- (void)drawNow;

/*!
 @abstract profileDraw
 @discussion Redraw content in a tight loop.
*/
- (void)profileDraw;

/*!
 @abstract needsUpdate
 @discussion OpenGL uses this to know when need perspective matrix update
*/
- (void)needsUpdate;

/*!
 @abstract endGame
 @discussion Cleans up the previous game
*/
- (void)endGame;

/*!
 @abstract startAnimation
 @discussion Start an animation
*/
- (void)startAnimation;

/*!
 @abstract animationDone
 @discussion An animation is finished
*/
- (void)animationDone;

/*!
 @abstract pickPixelFormat:
 @param afterFailure Whether or not method is called from a failure condition
 @discussion Fall back to less memory hungry graphics format (OpenGL)
*/
- (void)pickPixelFormat:(BOOL)afterFailure;

/*!
 @abstract setStyleForBoard:pieces:
 @param boardStyle The style to use for the board
 @param pieceStyle The style to use for the pieces
 @discussion Will update the textures used for the board and pieces
*/
- (void)setStyleForBoard:(NSString *)boardStyle pieces:(NSString *)pieceStyle;

/*!
 @abstract selectPiece:at:
 @param piece The type of piece
 @param square The code of the square location on board
 @discussion Called to select a piece at the given square
*/
- (void)selectPiece:(MBCPiece)piece at:(MBCSquare)square;

/*!
 @abstract selectPiece:at:
 @param piece The type of piece
 @param square The code of the square location on board
 @param to The destination square for the selected piece
 @discussion Called to select a piece at the given square and move to destination square
*/
- (void)selectPiece:(MBCPiece)piece at:(MBCSquare)square to:(MBCSquare)dest;

/*!
 @abstract moveSelectionTo:
 @param position The position in which to move the current selection
 @discussion Called to move a piece to given position if there is a currently selected piece.
*/
- (void)moveSelectionTo:(MBCPosition *)position;

/*!
 @abstract unselectPiece
 @discussion Will unselect piece if currently selected
*/
- (void)unselectPiece;

/*!
 @abstract clickPiece
 @discussion Sets the picked square to selected square.
*/
- (void)clickPiece;

/*!
 @abstract showMoveAsHint:
 @param move A move that will be drawn with hint (arrow from source to destination)
 @discussion Will draw an arrow to illustrate the given move.
*/
- (void)showMoveAsHint:(MBCMove *)move;

/*!
 @abstract showMoveAsLast:
 @param move A move that will be drawn with as last move (arrow from source to destination)
 @discussion Will draw an arrow to illustrate the given move (different color than showMoveAsHint:).
*/
- (void)showMoveAsLast:(MBCMove *)move;

/*!
 @abstract hideMoves
 @discussion Hide both the Hint and Last move indicators.
*/
- (void)hideMoves;

/*!
 @abstract positionToSquare:
 @param position The XYZ position to convert to square code
 @return The square code for the passed in position
 @discussion Determines a board square code based on given XYZ position
*/
- (MBCSquare)positionToSquare:(const MBCPosition *)position;

/*!
 @abstract positionToSquareOrRegion:
 @param position The XYZ position to convert to region code
 @return The square or region code for the passed in position (see MBCSquare for options)
 @discussion Determines a board square code or region code based on given XYZ position
*/
- (MBCSquare)positionToSquareOrRegion:(const MBCPosition *)position;

/*!
 @abstract squareToPosition:
 @param square Square code to convert into a XYZ position
 @return The XYZ position for the passed in square code
 @discussion Determines a board position based on given square code
*/
- (MBCPosition)squareToPosition:(MBCSquare)square;

/*!
 @abstract snapToSquare:
 @param position Position that will be snapped to square position.
 @discussion This function will simply set the position's Y coordinate to 0.
*/
- (void)snapToSquare:(MBCPosition *)position;

/*!
 @abstract facing
 @return Which player that is currently be faced.
 @discussion Tells caller which player is currently being faced.
*/
- (MBCSide)facing;

/*!
 @abstract facingWhite
 @return Whether or not currently facing white side of board.
 @discussion Tells caller whether or not currently facing white side.
*/
- (BOOL)facingWhite;

/*!
 @abstract wantMouse:
 @param wantIt Set if want to pass mouse clicks to interactive player
 @discussion Pass YES if want to pass mouse clicks on to interactive player.
*/
- (void)wantMouse:(BOOL)wantIt;

/*!
 @abstract willAzimuthRotateLabels:
 @param azimuth Camera azimuth value to test
 @discussion Will check if the value in azimuth param would make board labels render upside down
 due to the position of the camera for this azimuth value.
 */
- (BOOL)willAzimuthRotateLabels:(float)azimuth;

@end
