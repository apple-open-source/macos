/*
    File:		MBCUserDefaults.h
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

extern NSString * const kMBCBoardStyle;
extern NSString * const kMBCListenForMoves;
extern NSString * const kMBCPieceStyle;
extern NSString * const kMBCNewGamePlayers;
extern NSString * const kMBCNewGameVariant;
extern NSString * const kMBCNewGameSides;
extern NSString * const kMBCSearchTime;
extern NSString * const kMBCMinSearchTime;
extern NSString * const kMBCSpeakMoves;
extern NSString * const kMBCSpeakHumanMoves;
extern NSString * const kMBCDefaultVoice;
extern NSString * const kMBCAlternateVoice;
extern NSString * const kMBCGameCity;
extern NSString * const kMBCGameCountry;
extern NSString * const kMBCGameEvent;
extern NSString * const kMBCHumanName;
extern NSString * const kMBCHumanName2;
extern NSString * const kMBCBattleScars;
extern NSString * const kMBCBoardAngle;
extern NSString * const kMBCBoardSpin;
extern NSString * const kMBCCastleSides;
extern NSString * const kMBCGCVictories;
extern NSString * const kMBCShowGameLog;
extern NSString * const kMBCShowEdgeNotation;

extern NSString * const kMBCShareplayEnabledFF;
extern NSString * const kMBCUseMetalRendererFF;

@interface MBCUserDefaults : NSObject

/*!
 @abstract isSharePlayEnabled
 @return BOOL value indicating whether or not SharePlay is enabled.
 @discussion This function reads the BOOL value for key "SharePlayEnabled" from the Defaults.plist in the bundle.
*/
+ (BOOL)isSharePlayEnabled;

/*!
 @abstract isMetalRenderingEnabled
 @return BOOL value indicating whether or not to render with Metal (YES), or OpenGL (NO)
 @discussion This function returns whether or not to use Metal rendering. Default NO.
*/
+ (BOOL)isMetalRenderingEnabled;

/*!
 @abstract usingScreenCaptureKit
 @return BOOL value indicating whether or not enabled SCK screen recording
 @discussion This function returns whether or not to use SCK recording. Default NO.
*/
+ (BOOL)usingScreenCaptureKit;

@end
