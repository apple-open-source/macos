/*
	File:		MBCEngine.h
	Contains:	An agent represening the chess playing engine
	Copyright:	© 2002-2003 Apple Computer, Inc. All rights reserved.

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
	of Apple Computer, Inc. may be used to endorse or promote products
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

#import "MBCPlayer.h"

//
// MBCEngine is an instance of MBCPlayer, but it also serves other
// purposes like move generation and checking.
//
@interface MBCEngine : MBCPlayer
{
	NSTask *				fEngineTask;	// The chess engine
	NSFileHandle * 			fToEngine;		// Writing to the engine
	NSFileHandle * 			fFromEngine;	// Reading from the engine
	NSPipe *				fToEnginePipe;
	NSPipe *				fFromEnginePipe;
    NSRunLoop *				fMainRunLoop;	
	NSPort *				fEngineMoves;	// Moves parsed from engine
	NSPortMessage *			fMove;			// 	... the move
	MBCMove *	 			fLastMove;		// Last move played by player
	MBCMove *				fLastPonder;	// Last move pondered by engine
	MBCMove *				fLastEngineMove;// Last move played by engine
	MBCSide					fLastSide;		// Side of player
	bool					fThinking;		// Engine currently thinking
	bool					fWaitForStart;	// Wait for StartGame command
	bool					fSetPosition;	// Position set up already
	bool					fTakeback;		// Pending takeback
	bool					fEngineEnabled;	// Engine moves enabled?
	bool					fNeedsGo;		// Engine needs explicit start
	MBCSide					fSide;			// What side(s) engine is playing
	int						fSearchTime;	// Thinking time per move
}

- (id) init;
- (void) shutdown;
- (void) startGame:(MBCVariant)variant playing:(MBCSide)sideToPlay;
- (void) setSearchTime:(int)time;
- (MBCMove *) lastPonder;
- (MBCMove *) lastEngineMove;
- (void) setGame:(MBCVariant)variant fen:(NSString *)fen holding:(NSString *)holding moves:(NSString *)moves;
- (void) takeback;

//
// Hooked up internally
//
- (void) opponentMoved:(NSNotification *)notification;

//
// Used internally
//
- (void) runEngine:(id)sender;
- (void) enableEngineMoves:(BOOL)enable;
- (void) handlePortMessage:(NSPortMessage *)message;
- (void) writeToEngine:(NSString *)string;
- (void) interruptEngine;
- (void) flipSide;
- (NSString *) notificationForSide;
- (NSString *) squareToCoord:(MBCSquare)square;

@end

// Local Variables:
// mode:ObjC
// End:
