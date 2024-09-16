/*
    File:        MBCMetalMaterials.h
    Contains:    Maintains the current material styles used for Metal rendering.
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

#import <Foundation/Foundation.h>
#import "MBCBoardEnums.h"

@class MBCDrawStyle;
@protocol MTLDevice;
@protocol MTLTexture;

NS_ASSUME_NONNULL_BEGIN

/*!
 @typedef MBCMaterialType
 @abstract MBCMaterialType is used to determine type of asset in which to load material
 @constant MBCMaterialTypeBoard Asset type is the board
 @constant MBCMaterialTypePiece Asset type is a piece
*/
typedef NS_ENUM(NSInteger, MBCMaterialType) {
    MBCMaterialTypeBoard = 0,
    MBCMaterialTypePiece
};

@interface MBCMetalMaterials : NSObject

/*!
 @abstract The cube map texture to use for additional diffuse lighting for scene.
 */
@property (nonatomic, strong, readonly) id<MTLTexture> irradianceMap;

/*!
 @abstract MTLTexture for the base color of the  ground plane
 */
@property (nonatomic, strong, readonly) id<MTLTexture> groundTexture;

/*!
 @abstract MTLTexture for the base color of the  edge notation labels
 */
@property (nonatomic, strong, readonly) id<MTLTexture> edgeNotationTexture;

/*!
 @abstract MTLTexture for the normal map  of the  edge notation labels
 */
@property (nonatomic, strong, readonly) id<MTLTexture> edgeNotationNormalTexture;

/*!
 @abstract MTLTexture for the base color of the  piece selection indicator
 */
@property (nonatomic, strong, readonly) id<MTLTexture> pieceSelectionTexture;

/*!
 @abstract MTLTexture for the base color hint arrow and the last move arrow
 */
@property (nonatomic, strong, readonly) id<MTLTexture> hintArrowTexture;
@property (nonatomic, strong, readonly) id<MTLTexture> lastMoveArrowTexture;

/*!
 @abstract shared
 @discussion Returns the shared instance of the MBCMetalMaterials. Singleton to allow for sharing resources across NSWindows.
 */
+ (instancetype)shared;

- (instancetype)init NS_UNAVAILABLE;

/*!
 @abstract loadMaterialsForRendererID:newStyle:
 @param rendererID The identifier string of renderer requesting material resources
 @param newStyle The name of the style to use for the board and chess pieces
 @discussion Loads the MBCDrawStyle instances that define the materials used to render the board and pieces in the scene.
*/
- (void)loadMaterialsForRendererID:(NSString *)rendererID newStyle:(NSString *)newStyle;

/*!
 @abstract drawStyleForPiece:style:
 @param piece The piece code for which to retrieve style
 @discussion Returns the draw style instance associated with piece parameter
*/
- (MBCDrawStyle *)materialForPiece:(MBCPiece)piece style:(NSString *)style;

/*!
 @abstract boardMaterialWithStyle:
 @discussion Returns the draw style instance associated with the board
*/
- (MBCDrawStyle *)boardMaterialWithStyle:(NSString *)style;

/*!
 @abstract releaseUsageForStyle:rendererID:
 @param style The name of the style being requested (Wood, Marble, Metal)
 @param rendererID The identifier string of renderer requesting material resources
 @discussion Called when a renderer stops using a style. If a style is no longer being used
 by any renderer then that style along with all its textures will be released
 */
- (void)releaseUsageForStyle:(NSString *)style rendererID:(NSString *)rendererID;

@end

NS_ASSUME_NONNULL_END
