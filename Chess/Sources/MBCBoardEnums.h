/*
    File:        MBCBoardEnums.h
    Contains:    Define chess board enum types and associated functions.
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

#ifndef MBCBoardEnums_h
#define MBCBoardEnums_h

/*!
 @typedef MBCVariant
 @abstract MBCVariant identifies the game variant used when starting a new game
 @constant kVarNormal Regular game mode
 @constant kVarCrazyhouse Crazy House game mode
 @constant kVarSuicide Suicide game mode
 @constant kVarLosers Losers game mode
*/
enum MBCVariant {
    kVarNormal,
    kVarCrazyhouse,
    kVarSuicide,
    kVarLosers
};

/*!
 @typedef MBCPlayers
 @abstract MBCPlayers Identifies the type of players involved in the current game
 @constant kHumanVsHuman Human player vs human player
 @constant kHumanVsComputer Human player vs computer player
 @constant kComputerVsHuman Computer player vs human player
 @constant kComputerVsComputer Computer player vs computer player
 @constant kHumanVsGameCenter Human player vs another Human player found via Game Center
*/
enum MBCPlayers {
    kHumanVsHuman,
    kHumanVsComputer,
    kComputerVsHuman,
    kComputerVsComputer,
    kHumanVsGameCenter,
};

/*!
 @typedef MBCSideCode
 @abstract MBCSideCode Identifies which side will start a new game
 @constant kPlayWhite White side of board
 @constant kPlayBlack Black side of board
 @constant kPlayEither Either side of the board
*/
enum MBCSideCode {
    kPlayWhite,
    kPlayBlack,
    kPlayEither
};

/*!
 @typedef MBCUniqueCode
 @abstract Used to identify uniqueness of a move
 @constant kMatchingPieceExists Does matching piece exist
 @constant kMatchingPieceOnSameRow Matching piece in same row
 @constant kMatchingPieceOnSameCol Matching piece in same column
 @constant kVarLosers Losers game mode
*/
enum MBCUniqueCode {
    kMatchingPieceExists     = 1,
    kMatchingPieceOnSameRow  = 2,
    kMatchingPieceOnSameCol  = 4,
};

/*!
 @typedef MBCUnique
 @abstract MBCUnique Used to determine uniqueness of a move
*/
typedef int MBCUnique;

/*!
 @typedef MBCPieceCode
 @abstract MBCPieceCode The piece code to identify the characteristics of a piece on a square. These are combined using
 bitwise OR to identify a piece.
 @constant EMPTY not a type of piece
 @constant KING Identifies a king piece
 @constant QUEEN Identifies a queen piece
 @constant BISHOP Identifies a bishop piece
 @constant KNIGHT Identifies a knight piece
 @constant ROOK Identifies a rook piece
 @constant PAWN Identifies a pawn piece
 @constant kWhitePiece Identifies if piece is white
 @constant kBlackPiece Identifies if piece is black
 @constant kPromoted Marks piece as being promoted piece
 @constant kPieceMoved Marks the piece as being moved
*/
typedef enum MBCPieceCode {
    EMPTY = 0,
    KING, QUEEN, BISHOP, KNIGHT, ROOK, PAWN,
    kWhitePiece = 0,
    kBlackPiece = 8,
    kPromoted   = 16,
    kPieceMoved = 32
} MBCPieceCode;

/*!
 @typedef MBCPiece
 @abstract MBCPiece is a combination of MBCPieceCodes that are combined using bitwise OR to
 create a code to easily store information about a chess piece.
*/
typedef unsigned char MBCPiece;

/*!
 @abstract White()
 @param code The type of piece (KING, QUEEN, BISHOP, KNIGHT, ROOK, PAWN)
 @discussion Will return a code that represents a white piece with specified type
*/
inline MBCPiece White(MBCPieceCode code) { return kWhitePiece | code; }

/*!
 @typedef Black()
 @param code The type of piece (KING, QUEEN, BISHOP, KNIGHT, ROOK, PAWN)
 @discussion Will return a code that represents a black piece with specified type
*/
inline MBCPiece Black(MBCPieceCode code) { return kBlackPiece | code; }

/*!
 @typedef Piece()
 @param piece The complete piece code for which to extract the type of piece
 @discussion Will return a code that identifies the type of piece (regardless of color)
*/
inline MBCPieceCode Piece(MBCPiece piece) { return (MBCPieceCode)(piece & 7); }

/*!
 @typedef Color()
 @param piece The complete piece code for which to extract the type of piece
 @discussion Will return a code that identifies the color of the piece (regardless of type)
*/
inline MBCPieceCode Color(MBCPiece piece) { return (MBCPieceCode)(piece & 8); }

/*!
 @typedef What()
 @param piece The complete piece code for which to extract the type and color of the piece.
 @discussion Will extract and return the base piece type and color without any promoted or moved data
*/
inline MBCPieceCode What(MBCPiece piece) { return (MBCPieceCode)(piece & 15); }

/*!
 @typedef Matching()
 @param piece The piece in which to extract the color component for the piece
 @param code The type of piece (KING, QUEEN, etc) to create matchine color of piece param
 @discussion Will create a new piece code for same color as piece param with type specified by code.
*/
inline MBCPiece Matching(MBCPiece piece, MBCPieceCode code) { return (piece & 8) | code; }

/*!
 @typedef Opposite()
 @param piece The complete piece code for which to invert the color
 @discussion Will return a piece of same type that is opposite in color from piece param
*/
inline MBCPiece Opposite(MBCPiece piece) { return piece ^ 8; }

/*!
 @typedef Promoted()
 @param piece Complete piece code to check if it is a promoted piece
 @discussion Will return whether the piece has promoted bit set
*/
inline MBCPieceCode Promoted(MBCPiece piece) { return (MBCPieceCode)(piece & 16); }

/*!
 @typedef PieceMoved()
 @param piece Complete piece code to check if it is moved
 @discussion Will return whether the piece has moved bit set
*/
inline MBCPieceCode PieceMoved(MBCPiece piece) { return (MBCPieceCode)(piece & 32); }

/*!
 @typedef MBCMoveCode
 @abstract MBCMoveCode specifies the type of move that was made
 @constant kCmdNull
 @constant kCmdMove
 @constant kCmdDrop
 @constant kCmdUndo
 @constant kCmdWhiteWins
 @constant kCmdBlackWins
 @constant kCmdDraw
 @constant kCmdPong
 @constant kCmdStartGame
 @constant kCmdPMove
 @constant kCmdPDrop
 @constant kCmdMoveOK
*/
enum MBCMoveCode {
    kCmdNull,
    kCmdMove,        
    kCmdDrop,
    kCmdUndo,
    kCmdWhiteWins,
    kCmdBlackWins,
    kCmdDraw,
    kCmdPong,
    kCmdStartGame,
    kCmdPMove,
    kCmdPDrop,
    kCmdMoveOK
};

/*!
 @typedef MBCSquare
 @abstract MBCSquare represents a character code for one of board sqaures or regions
*/
typedef unsigned char MBCSquare;

/*!
 @abstract These codes specify specific squares or regions of the board in which a piece may occupy
*/
enum {
    kSyntheticSquare    = 0x70,
    kWhitePromoSquare   = 0x71,
    kBlackPromoSquare   = 0x72,
    kBorderRegion       = 0x73,
    kInHandSquare       = 0x80,
    kInvalidSquare      = 0xFF,
    kSquareA8           = 56,
    kBoardSquares       = 64
};

/*!
 @abstract Row()
 @param square Square code for board square
 @discussion Will return which row the square belongs (1 - 8)
*/
inline unsigned Row(MBCSquare square) { return 1 + (square >> 3); }

/*!
 @abstract Col()
 @param square Square code for board square
 @discussion Will return which column the square belongs (a - h)
*/
inline char Col(MBCSquare square) { return 'a'+ (square & 7); }

/*!
 @typedef Square()
 @param col The column for square (a - h)
 @param row The row for square (1 - 8)
 @discussion Returns the char code for the given column and row
 */
inline MBCSquare Square(char col, unsigned row) { return ((row - 1) << 3) | (col - 'a'); }

/*!
 @typedef Square()
 @param colrow pointer to the column and row characters for a square
 @discussion Returns the char code for the given row and column pointed to by the colrow param
*/
inline MBCSquare Square(const char * colrow) { return ((colrow[1] - '1') << 3) | (colrow[0] - 'a'); }

/*!
 @typedef MBCCastling
 @abstract MBCCastling identifies if have active castling
 @constant kUnknownCastle
 @constant kCastleQueenside
 @constant kCastleKingside
 @constant kNoCastle
*/
enum MBCCastling {
    kUnknownCastle, 
    kCastleQueenside,
    kCastleKingside,
    kNoCastle
};

/*!
 @typedef MBCSide
 @abstract MBCSide identifies which sides of board
 @constant kWhiteSide Just the white side
 @constant kBlackSide Just the black side
 @constant kBothSides Both black and white side
 @constant kNeitherSide Neither black nor white side
*/
typedef enum MBCSide {
    kWhiteSide, 
    kBlackSide,
    kBothSides,
    kNeitherSide
} MBCSide;

/*!
 @abstract SideIncludesWhite()
 @param side Type of side to check
 @discussion Returns whether or not side type includes white
*/
inline bool SideIncludesWhite(MBCSide side) { return side == kWhiteSide || side == kBothSides; }

/*!
 @abstract SideIncludesBlack()
 @param side Type of side to check
 @discussion Returns whether or not side type includes black
*/
inline bool SideIncludesBlack(MBCSide side) { return side == kBlackSide || side == kBothSides; }

/*!
 @typedef MBCCompactMove
 @abstract MBCVariant identifies a move that has a very short existence and is only used in places
 where the information absolutely has to be kept to 32 bits.
*/
typedef unsigned MBCCompactMove;

/*!
 @typedef MBCRenderPassType
 @abstract MBCRenderPassType identifies the current render pass type in renderer
 @constant MBCRenderPassTypeShadow Shadow map generation
 @constant MBCRenderPassTypeForward Opaque object forward pass render
 @constant MBCRenderPassTypeForwardAlphaBlend Transparent object forward render
*/
typedef enum MBCRenderPassType {
    MBCRenderPassTypeShadow = 0,
    MBCRenderPassTypeReflection,
    MBCRenderPassTypeForward,
    MBCRenderPassTypeForwardAlphaBlend
} MBCRenderPassType;

#endif /* MBCBoardEnums_h */
