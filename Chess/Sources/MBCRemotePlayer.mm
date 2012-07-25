/*
    File:		MBCRemotePlayer.mm
    Contains:	Proxy for GameKit remote player participating in game
    Copyright:	© 2011 by Apple Inc., all rights reserved.

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

/*
 */

#import "MBCRemotePlayer.h"
#import "MBCDocument.h"

@implementation MBCRemotePlayer 

- (void) removeChessObservers
{
    if (!fHasObservers)
        return;
    
    NSNotificationCenter * notificationCenter = [NSNotificationCenter defaultCenter];
    [notificationCenter removeObserver:self name:MBCWhiteMoveNotification object:nil];
    [notificationCenter removeObserver:self name:MBCBlackMoveNotification object:nil];
    [notificationCenter removeObserver:self name:MBCGameEndNotification object:nil];
    [notificationCenter removeObserver:self name:MBCTakebackNotification object:nil];
    
    fHasObservers = NO;
}

- (void)dealloc
{
    [self removeChessObservers];
    [super dealloc];
}

- (void) startGame:(MBCVariant)variant playing:(MBCSide)sideToPlay
{
    [self removeChessObservers];
    NSNotificationCenter * notificationCenter = [NSNotificationCenter defaultCenter];
	switch (sideToPlay) {
    default:
    case kWhiteSide:
        [notificationCenter
         addObserver:self
            selector:@selector(opponentMoved:)
                name:MBCBlackMoveNotification
              object:fDocument];
        break;
    case kBlackSide:
        [notificationCenter 
         addObserver:self
            selector:@selector(opponentMoved:)
                name:MBCWhiteMoveNotification
              object:fDocument];
        break;
	}
	[notificationCenter 
     addObserver:self
     selector:@selector(takeback:)
     name:MBCTakebackNotification
     object:fDocument];
	[notificationCenter 
     addObserver:self
     selector:@selector(endOfGame:)
     name:MBCGameEndNotification
     object:fDocument];
    fHasObservers = YES;
}

- (void)opponentMoved:(NSNotification *)n
{
    dispatch_async(dispatch_get_main_queue(), ^{
        [fDocument updateMatchForLocalMove];
    });
}

- (void)takeback:(NSNotification *)n
{
}

- (void)endOfGame:(NSNotification *)n
{
    dispatch_async(dispatch_get_main_queue(), ^{
        [fDocument updateMatchForEndOfGame:((MBCMove *)[n userInfo])->fCommand];
    });
}

@end

// Local Variables:
// mode:ObjC
// End:
