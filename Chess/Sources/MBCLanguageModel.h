/*
	File:		MBCLanguageModel.h
	Contains:	Build and interpret speech recognition language model
	Version:	1.0
	Copyright:	© 2003 by Apple Computer, Inc., all rights reserved.
*/

#import "MBCMoveGenerator.h"

#import <Carbon/Carbon.h>

/*
 * An MBCLanguageModel builds a speech recognition language model from a 
 * collection of legal moves, and derives the move from a recognition
 * result.
 */
@interface MBCLanguageModel : NSObject {
	SRRecognitionSystem	fSystem;
	SRLanguageObject 	fToModel;
	SRLanguageObject 	fPromotionModel;
	MBCMoveCollection *	fMoves;
	BOOL				fDumpModels;
}

- (id) initWithRecognitionSystem:(SRRecognitionSystem)system;
- (void) buildLanguageModel:(SRLanguageModel)model 
				  fromMoves:(MBCMoveCollection *)moves
				   takeback:(BOOL)takeback;
- (MBCMove *) recognizedMove:(SRRecognitionResult)result;

@end

// Local Variables:
// mode:ObjC
// End:
