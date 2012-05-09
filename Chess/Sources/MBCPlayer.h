/*
	File:		MBCPlayer.h
	Contains:	Infrastructure for agents participating in game
	Version:	1.0
	Copyright:	© 2002 by Apple Computer, Inc., all rights reserved.
*/

#import <Cocoa/Cocoa.h>

#import "MBCBoard.h"

//
// Moves are sent to all interested parties. Trusted clients,
// e.g., chess engines, broadcast MBC*MoveNotification. Untrusted
// clients, e.g. human players, broadcast MBCUnchecked*MoveNotification,
// which is checked by the engine and either turned into a
// MBC*MoveNotification or a MBCIllegalMoveNotification. The legal move
// notification is executed by the board view (possibly using animation)
// and turned into a MBCEndMoveNotification. When the user takes back a move,
// MBCTakebackNotification is broadcast.
//
extern NSString * const MBCWhiteMoveNotification;
extern NSString * const MBCBlackMoveNotification;
extern NSString * const MBCUncheckedWhiteMoveNotification;
extern NSString * const MBCUncheckedBlackMoveNotification;
extern NSString * const MBCIllegalMoveNotification;
extern NSString * const MBCEndMoveNotification;
extern NSString * const MBCTakebackNotification;
extern NSString * const MBCGameEndNotification;

extern NSString * const kMBCHumanPlayer;
extern NSString * const kMBCEnginePlayer;

//
// MBCPlayer is an abstract superclass for all possible agents
// that can play a side (a human, a chess engine, a network connection)
//
@interface MBCPlayer : NSObject
{
}

//
// Start a game playing the black side, the white side, both sides,
// or neither side (in observation mode).
// 
- (void) startGame:(MBCVariant)variant playing:(MBCSide)sideToPlay;

@end

// Local Variables:
// mode:ObjC
// End:
