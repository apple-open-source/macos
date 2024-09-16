/*
    File:		MBCUserDefaults.mm
    Contains:	User defaults keys
    Copyright:	© 2003-2024 by Apple Inc., all rights reserved.

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

#import <Foundation/Foundation.h>

#import "MBCUserDefaults.h"


NSString * const kMBCBoardStyle         = @"MBCBoardStyle";
NSString * const kMBCListenForMoves     = @"MBCListenForMoves";
NSString * const kMBCPieceStyle         = @"MBCPieceStyle";
NSString * const kMBCNewGamePlayers     = @"MBCNewGamePlayers";
NSString * const kMBCNewGameVariant     = @"MBCNewGameVariant";
NSString * const kMBCNewGameSides       = @"MBCNewGameSides";
NSString * const kMBCSearchTime         = @"MBCSearchTime";
NSString * const kMBCMinSearchTime      = @"MBCMinSearchTime";
NSString * const kMBCSpeakMoves         = @"MBCSpeakMoves";
NSString * const kMBCSpeakHumanMoves    = @"MBCSpeakHumanMoves";
NSString * const kMBCDefaultVoice       = @"MBCDefaultVoice";
NSString * const kMBCAlternateVoice     = @"MBCAlternateVoice";
NSString * const kMBCGameCity           = @"MBCGameCity";
NSString * const kMBCGameCountry        = @"MBCGameCountry";
NSString * const kMBCGameEvent          = @"MBCGameEvent";
NSString * const kMBCHumanName          = @"MBCHumanName";
NSString * const kMBCHumanName2         = @"MBCHumanName2";
NSString * const kMBCBattleScars        = @"MBCBattleScars";
NSString * const kMBCBoardAngle         = @"MBCBoardAngle";
NSString * const kMBCBoardSpin          = @"MBCBoardSpin";
NSString * const kMBCCastleSides        = @"MBCCastleSides";
NSString * const kMBCGCVictories        = @"MBCGCVictories";
NSString * const kMBCShowGameLog        = @"MBCShowGameLog";
NSString * const kMBCShowEdgeNotation   = @"MBCShowEdgeNotation";
NSString * const kMBCSharePlayEnabledFF = @"SharePlayEnabled";
NSString * const kMBCUseMetalRendererFF = @"UseMetalRenderer";

@implementation MBCUserDefaults

+ (BOOL)isSharePlayEnabled {
    static BOOL sIsEnabled;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        NSString *path = [[NSBundle mainBundle] pathForResource:@"Defaults" ofType:@"plist"];
        NSDictionary *dict = [[NSDictionary alloc] initWithContentsOfFile:path];
        sIsEnabled = [[dict objectForKey:kMBCSharePlayEnabledFF] boolValue];
    });
    return sIsEnabled;
}

+ (BOOL)isMetalRenderingEnabled {
    static BOOL sUsingMetal;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        sUsingMetal = NO;
    });
    return sUsingMetal;
}

+ (BOOL)usingScreenCaptureKit {
    static BOOL sUsingSCK;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        sUsingSCK = NO;
    });
    return sUsingSCK;
}

@end
