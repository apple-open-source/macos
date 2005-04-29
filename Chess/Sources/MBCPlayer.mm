/*
	File:		MBCPlayer.mm
	Contains:	Infrastructure for agents participating in game
	Version:	1.0
	Copyright:	© 2002 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				Matthias Neeracher    x43683

	Writers:

		(MN)	Matthias Neeracher

	Change History (most recent first):

		$Log: MBCPlayer.mm,v $
		Revision 1.4  2003/05/24 20:25:45  neerache
		Fix flaw in declaration
		
		Revision 1.3  2003/04/05 05:45:08  neeri
		Add PGN export
		
		Revision 1.2  2003/04/02 18:21:09  neeri
		Support saving games
		
		Revision 1.1  2002/08/22 23:47:06  neeri
		Initial Checkin
		
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
