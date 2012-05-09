/*
	File:		MBCPlayer.mm
	Contains:	Infrastructure for agents participating in game
	Version:	1.0
	Copyright:	© 2002 by Apple Computer, Inc., all rights reserved.
*/

/*
 */

#import "MBCPlayer.h"

NSString * const MBCWhiteMoveNotification			= @"MBCWhMove";
NSString * const MBCBlackMoveNotification			= @"MBCBlMove";
NSString * const MBCUncheckedWhiteMoveNotification  = @"MBCUWMove";
NSString * const MBCUncheckedBlackMoveNotification	= @"MBCUBMove";
NSString * const MBCIllegalMoveNotification			= @"MBCIlMove";
NSString * const MBCEndMoveNotification				= @"MBCEnMove";
NSString * const MBCTakebackNotification			= @"MBCTakeback";
NSString * const MBCGameEndNotification				= @"MBCGameEnd";

NSString * const kMBCHumanPlayer					= @"human";
NSString * const kMBCEnginePlayer					= @"program";

@implementation MBCPlayer 

- (void) startGame:(MBCVariant)variant playing:(MBCSide)sideToPlay
{
}

@end

// Local Variables:
// mode:ObjC
// End:
