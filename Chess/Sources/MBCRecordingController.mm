/*
    File:        MBCRecordingController.mm
    Contains:    Controls ScreenCaptureKit recording for all windows.
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

#import "MBCRecordingController.h"
#import "MBCBoardWin.h"
#import "MBCDocument.h"

#import <ScreenCaptureKit/ScreenCaptureKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

NSString *const MBCRecordingErrorDomain = @"com.apple.Chess.MBCRecordingErrorDomain";

#define MP4_FILE_EXTENSION @"mp4"

#define GAME_MENU_ITEM_INDEX 1

@interface MBCRecordingHighlightWindow : NSWindow
@end

@implementation MBCRecordingHighlightWindow

- (BOOL)canBecomeKeyWindow {
    return NO;
}

@end

/*!
 @abstract MBCRecordingSession
 @discussion Data object used to store items used for recording a window.
 */
@interface MBCRecordingSession : NSObject

@property (nonatomic, strong) SCStream *stream;
@property (nonatomic, strong) NSURL *outputURL;
@property (nonatomic, strong) SCRecordingOutput *recordingOutput;

@end

@implementation MBCRecordingSession

@end

@interface MBCRecordingController() <SCStreamDelegate, SCRecordingOutputDelegate, NSMenuDelegate>

@end

@implementation MBCRecordingController {
    /*!
     @abstract Stores an instance of MBCRecordingSession for each active stream.
     The objects are used to store data per recorded window. The keys are
     the windowID of NSWindow as an NSNumber.
     */
    NSMutableDictionary<NSNumber *, MBCRecordingSession *> *_activeStreams;
    
    /*!
     @abstract Separator that will be inserted into Game menu if either start
     or stop recording items are present.
     */
    NSMenuItem *_separatorMenuItem;
    
    /*!
     @abstract The main menu item added to Game menu to start a recording a game.
     */
    NSMenuItem *_startRecordingMenuItem;
    
    /*!
     @abstract Will be added as a submenu for `_startRecordingMenuItem` if more than
     one window is eligible for recording.
     */
    NSMenu *_startRecordingSubmenu;
    
    /*!
     @abstract The main menu item added to Game menu to stop a recording.
     */
    NSMenuItem *_stopRecordingMenuItem;
    
    /*!
     @abstract Will be added as a submenu for `_stopRecordingMenuItem` if more than
     one recording is active (and thus can be stopped).
     */
    NSMenu *_stopRecordingSubmenu;
    
    /*!
     @abstract Transparent window placed above window to illustrate how a menu item
     relates to game window for starting or stopping recording.
     */
    NSWindowController *_highlightWindowController;
}

- (instancetype)init {
    self = [super init];
    if (self) {
        _activeStreams = [[NSMutableDictionary alloc] initWithCapacity:2];
        
        [self initializeGameMenuForRecording];
    }
    return self;
}

- (void)initializeGameMenuForRecording {
    NSMenu *gameMenu = [[[NSApp mainMenu] itemAtIndex:GAME_MENU_ITEM_INDEX] submenu];
    gameMenu.delegate = self;
    
    _separatorMenuItem = [NSMenuItem separatorItem];
    
    NSString *itemTitle = NSLocalizedString(@"menu_item_record_game", @"Title for Record Game menu item");
    _startRecordingMenuItem = [[NSMenuItem alloc] initWithTitle:itemTitle action:nil keyEquivalent:@""];

    itemTitle = NSLocalizedString(@"menu_item_stop_recording", @"Title for Stop Recording menu item");
    _stopRecordingMenuItem = [[NSMenuItem alloc] initWithTitle:itemTitle
                                                        action:nil
                                                 keyEquivalent:@""];
}

- (BOOL)isRecording {
    return _activeStreams.count > 0;
}

- (NSArray<NSNumber *> *)recordedWindowIDs {
    return [_activeStreams allKeys];
}

- (SCStreamConfiguration *)streamConfigurationForWindow:(SCWindow *)window displayName:(NSString *)displayName {
    SCStreamConfiguration *streamConfiguration = [[SCStreamConfiguration alloc] init];
    streamConfiguration.capturesAudio = NO;
    
    streamConfiguration.width = (size_t)CGRectGetWidth(window.frame) * 2;
    streamConfiguration.height = (size_t)CGRectGetHeight(window.frame) * 2;
    
    streamConfiguration.minimumFrameInterval = CMTimeMake(1, 60);
    
    streamConfiguration.streamName = displayName;
    
    return streamConfiguration;
}

- (BOOL)isRecordingWindow:(NSWindow *)window {
    for (NSNumber *number in [_activeStreams allKeys]) {
        if ([number integerValue] == window.windowNumber) {
            return YES;
        }
    }
    return NO;
}

- (void)startRecordingWindow:(NSWindow *)window completionHandler:(void(^)(NSError * _Nullable error))completionHandler {
    
    if ([self isRecordingWindow:window]) {
        NSError *alreadyRecordingError = [NSError errorWithDomain:MBCRecordingErrorDomain
                                                             code:MBCRecordingErrorCodeAlreadyRecording
                                                         userInfo:nil];
        completionHandler(alreadyRecordingError);
    }

    MBCDocument *gameDocument = (MBCDocument *)[window windowController].document;
    NSString *displayName = gameDocument.displayName;
    
    [SCShareableContent getCurrentProcessShareableContentWithCompletionHandler:^(SCShareableContent *shareableContent, NSError *error) {
        dispatch_async(dispatch_get_main_queue(), ^{
            NSError *localError = error;
            if (!localError) {
                SCWindow *foundWindow = nil;
                for (SCWindow *scWindow in shareableContent.windows) {
                    if (scWindow.windowID == (CGWindowID)window.windowNumber) {
                        foundWindow = scWindow;
                        break;
                    }
                }
                if (foundWindow) {
                    [self startSCStreamForWindow:foundWindow displayName:displayName completionHandler:completionHandler];
                } else {
                    localError = [NSError errorWithDomain:MBCRecordingErrorDomain
                                                     code:MBCRecordingErrorCodeInvalidWindow
                                                 userInfo:nil];
                }
            }
            
            if (localError) {
                // If foundWindow == YES, completion handler is passed
                // to startSCStreamForWindow:completionHandler: above
                completionHandler(localError);
            }
        });
    }];
}

- (NSURL *)tempFilePathForStreamOutput:(SCStream *)stream {
    NSURL *tmpURL = [NSURL fileURLWithPath:NSTemporaryDirectory()];
    tmpURL = [tmpURL URLByAppendingPathComponent:[NSString stringWithFormat:@"ChessRecording-%p", stream]];
    return [tmpURL URLByAppendingPathExtension:MP4_FILE_EXTENSION];
}

- (void)startSCStreamForWindow:(SCWindow *)window displayName:(NSString *)displayName completionHandler:(MBCRecordingBlock)completionHandler {
    // Create filter to only record the game window
    SCContentFilter *contentFilter = [[SCContentFilter alloc] initWithDesktopIndependentWindow:window];
    
    // Create the stream configuration for the window
    SCStreamConfiguration *streamConfiguration = [self streamConfigurationForWindow:window displayName:displayName];
    
    // Create the stream for recording from filter and configuration
    SCStream *stream = [[SCStream alloc] initWithFilter:contentFilter
                                          configuration:streamConfiguration
                                               delegate:self];
    
    MBCRecordingSession *session = [[MBCRecordingSession alloc] init];
    session.stream = stream;
    NSNumber *windowID = @(window.windowID);
    [_activeStreams setObject:session forKey:windowID];
    
    NSError *addOutputError;
    if (@available(macOS 15, *)) {
        // Set up the recording output for the stream
        SCRecordingOutputConfiguration *outputConfig = [[SCRecordingOutputConfiguration alloc] init];
        outputConfig.outputURL = [self tempFilePathForStreamOutput:stream];
        
        SCRecordingOutput *recordingOutput = [[SCRecordingOutput alloc] initWithConfiguration:outputConfig delegate:self];
        session.outputURL = outputConfig.outputURL;
        session.recordingOutput = recordingOutput;
        
        [stream addRecordingOutput:recordingOutput error:&addOutputError];
    }
    
    if (addOutputError) {
        // Failure adding stream output, do not proceed with stream.
        [_activeStreams removeObjectForKey:windowID];
        completionHandler(addOutputError);
    } else {
        [stream startCaptureWithCompletionHandler:^(NSError * _Nullable error) {
            dispatch_async(dispatch_get_main_queue(), ^{
                if (error) {
                    [_activeStreams removeObjectForKey:windowID];
                }
                completionHandler(error);
            });
        }];
    }
}

- (void)stopRecordingWindow:(NSWindow *)window 
          completionHandler:(MBCRecordingBlock)completionHandler {
    NSNumber *windowNumber = @(window.windowNumber);
    SCStream *stream = [_activeStreams objectForKey:windowNumber].stream;
    if (stream) {
        [stream stopCaptureWithCompletionHandler:^(NSError * _Nullable error) {
            dispatch_async(dispatch_get_main_queue(), ^{
                if (!error) {
                    [self promptUserToSaveRecording:window completionHandler:completionHandler];
                } else {
                    completionHandler(error);
                }
            });
        }];
    } else {
        completionHandler([NSError errorWithDomain:MBCRecordingErrorDomain
                                              code:MBCRecordingErrorCodeInvalidStream
                                          userInfo:nil]);
    }
}

- (void)cleanupRecordingSessionForWindow:(NSWindow *)window {
    NSNumber *windowNumber = @(window.windowNumber);
    [_activeStreams removeObjectForKey:windowNumber];
}

- (NSString *)defaultFileNameForSavePanel {
    NSDateFormatter *formatter = [[NSDateFormatter alloc] init];
    [formatter setDateFormat:@"yyyy-MM-dd hh.mm.ss a"];

    NSDate *currentDate = [NSDate date];
    NSString *dateString = [formatter stringFromDate:currentDate];
    NSString *filePrefix = NSLocalizedString(@"recorded_game_file_name", @"Chess Recording");
    
    return [NSString localizedStringWithFormat:NSLocalizedString(@"recorded_game_file_name_fmt", @"recorded game file name format %@ %@.%@"), filePrefix, dateString, MP4_FILE_EXTENSION];
}

- (void)promptUserToSaveRecording:(NSWindow *)window
                completionHandler:(MBCRecordingBlock)completionHandler {
    NSSavePanel *panel = [NSSavePanel savePanel];
    panel.allowedContentTypes = @[UTTypeMPEG4Movie, UTTypeQuickTimeMovie];
    panel.allowsOtherFileTypes = NO;
    panel.canCreateDirectories = YES;
    panel.extensionHidden = NO;
    panel.directoryURL = [NSURL URLWithString:NSHomeDirectory()];
    panel.nameFieldStringValue = [self defaultFileNameForSavePanel];
    
    [panel beginSheetModalForWindow:window 
                  completionHandler:^(NSModalResponse result) {
        NSError *error;
        NSNumber *windowNumber = @(window.windowNumber);
        NSURL *tmpURL = [_activeStreams objectForKey:windowNumber].outputURL;
        
        if (result == NSModalResponseOK) {
            NSURL *destinationURL = panel.URL;
            if (destinationURL) {
                [[NSFileManager defaultManager] moveItemAtURL:tmpURL
                                                        toURL:destinationURL
                                                        error:&error];
            }
        }
        
        completionHandler(error);
    }];
}

#pragma mark - SCStreamDelegate

- (void)stream:(SCStream *)stream didStopWithError:(NSError *)error {
    // Normal user stopping is handled below in recordingOutputDidFinishRecording
    if (error.code != SCStreamErrorUserStopped) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self didStopStream:stream];
        });
    }
}

- (void)didStopStream:(SCStream *)stream {
    // Locate the saved session that has this same SCStream in order to find recorded window
    __block NSWindow *window = nil;
    [_activeStreams enumerateKeysAndObjectsUsingBlock:^(NSNumber *windowID, MBCRecordingSession *session, BOOL *stop) {
        if ([session.stream isEqual:stream]) {
            window = [NSApp windowWithWindowNumber:[windowID integerValue]];
            *stop = YES;
        }
    }];
    
    [self promptUserToSaveRecording:window completionHandler:^(NSError * _Nullable error) {
        [self.delegate recordingController:self didStopRecordingWindow:window error:error];
        [self cleanupRecordingSessionForWindow:window];
    }];
}

#pragma mark - SCRecordingOutputDelegate

- (NSWindow *)windowForRecordingOutput:(SCRecordingOutput *)recordingOutput {
    // Locate the saved session that has this same SCRecordingOutput in order to find recorded window.
    __block NSWindow *window = nil;
    [_activeStreams enumerateKeysAndObjectsUsingBlock:^(NSNumber *windowID, MBCRecordingSession *session, BOOL *stop) {
        if ([session.recordingOutput isEqual:recordingOutput]) {
            window = [NSApp windowWithWindowNumber:[windowID integerValue]];
            *stop = YES;
        }
    }];
    return window;
}

- (void)recordingOutputDidStartRecording:(SCRecordingOutput *)recordingOutput {
    dispatch_async(dispatch_get_main_queue(), ^{
        // Locate the saved session that has this same SCRecordingOutput in order to find recorded window.
        NSWindow *window = [self windowForRecordingOutput:recordingOutput];
        [self.delegate recordingController:self didStartRecordingWindow:window];
    });
}

- (void)recordingOutputDidFinishRecording:(SCRecordingOutput *)recordingOutput {
    dispatch_async(dispatch_get_main_queue(), ^{
        // Locate the saved session that has this same SCRecordingOutput in order to find recorded window.
        NSWindow *window = [self windowForRecordingOutput:recordingOutput];
        if (window) {
            [self promptUserToSaveRecording:window completionHandler:^(NSError * _Nullable error) {
                [self.delegate recordingController:self didStopRecordingWindow:window error:error];
                [self cleanupRecordingSessionForWindow:window];
            }];
        }
    });
}

- (void)recordingOutput:(SCRecordingOutput *)recordingOutput didFailWithError:(NSError *)error {
    dispatch_async(dispatch_get_main_queue(), ^{
        // Locate the saved session that has this same SCRecordingOutput in order to find recorded window.
        NSWindow *window = [self windowForRecordingOutput:recordingOutput];
        [self.delegate recordingController:self didStopRecordingWindow:window error:error];
        [self cleanupRecordingSessionForWindow:window];
    });
}

#pragma mark - Game Menu Items

/*!
 @abstract menuWillOpen:
 @discussion When the Game menu opens, will determine what items need to be added for
 starting or stopping a recording. This all depends on the number of open games and
 whether or not there are active recordings.
 */
- (void)menuWillOpen:(NSMenu *)menu {
    NSMenu *gameMenu = [[[NSApp mainMenu] itemAtIndex:GAME_MENU_ITEM_INDEX] submenu];
    if ([menu isEqual:gameMenu]) {
        // Start recording menu item(s)
        [self prepareStartRecordingItemsForGameMenu:gameMenu];
        
        // Stop recording menu item(s)
        if (self.isRecording) {
            [self prepareStopRecordingItemsForGameMenu:gameMenu];
        }
    }
}

- (void)prepareStartRecordingItemsForGameMenu:(NSMenu *)gameMenu {
    _startRecordingMenuItem.tag = NSNotFound;
    _startRecordingMenuItem.action = nil;
    _startRecordingMenuItem.target = nil;
    
    NSMenu *recordableWindowMenu = [self prepareSubmenuForStartRecordingMenuItem];
    
    // Start Recording not added if no windows available to record.
    if (recordableWindowMenu.numberOfItems > 0) {
        // Separator is removed on close, start recording will always show
        // before stop recording so add the separator now
        [gameMenu addItem:_separatorMenuItem];
        
        // Add start recording item to menu, will be removed when Game menu closes
        [gameMenu addItem:_startRecordingMenuItem];
    }
    
    // If have exactly one window that can be recorded, no submenu needed.
    // More than one window that can be recorded, then a submenu of these window
    // titles to record is added.
    if (recordableWindowMenu.numberOfItems == 1) {
        // Have a single window eligible to record
        // The window number associated with window is stored in NSMenuItem's tag
        NSMenuItem *menuItem = [recordableWindowMenu itemAtIndex:0];
        _startRecordingMenuItem.tag = menuItem.tag;
        _startRecordingMenuItem.action = @selector(startRecordingWindowForSelectedMenuItem:);
        _startRecordingMenuItem.target = self;
        _startRecordingMenuItem.title = [NSString localizedStringWithFormat:NSLocalizedString(@"menu_item_record_fmt", "Record <Window Title>"), menuItem.title];
    } else if (recordableWindowMenu.numberOfItems > 1) {
        // More than one item to record, will show a submenu of items
        _startRecordingSubmenu = recordableWindowMenu;
        _startRecordingSubmenu.delegate = self;
        
        _startRecordingMenuItem.submenu = _startRecordingSubmenu;
        _startRecordingMenuItem.title = NSLocalizedString(@"menu_item_record_game", "Record Game");
    }
}

- (NSMenu *)prepareSubmenuForStartRecordingMenuItem {
    NSMenu *submenu = [[NSMenu alloc] init];
    NSArray<NSNumber *> *windowNumbers = [NSWindow windowNumbersWithOptions:0];
    
    // Find game windows that are not currently being recorded and add them to menu.
    for (NSNumber *numberObj in windowNumbers) {
        NSInteger windowNumber = [numberObj integerValue];
        NSWindow *currentWindow = [NSApp windowWithWindowNumber:windowNumber];
        if (currentWindow && [[currentWindow windowController] isKindOfClass:[MBCBoardWin class]]) {
            if (![self isRecordingWindow:currentWindow]) {
                MBCDocument *gameDocument = (MBCDocument *)[currentWindow windowController].document;
                NSMenuItem *newItem = [[NSMenuItem alloc] initWithTitle:gameDocument.displayName
                                                                 action:@selector(startRecordingWindowForSelectedMenuItem:)
                                                          keyEquivalent:@""];
                newItem.target = self;
                newItem.tag = windowNumber;
                [submenu addItem:newItem];
            }
        }
    }
    
    return submenu;
}

- (void)prepareStopRecordingItemsForGameMenu:(NSMenu *)gameMenu {
    // If actively recording, then show the stop recording menu item

    _stopRecordingMenuItem.tag = NSNotFound;
    _stopRecordingMenuItem.action = nil;
    _stopRecordingMenuItem.target = nil;
    
    NSMenu *recordedWindowMenu = [self prepareSubmenuForStopRecordingMenuItem];
    
    if (recordedWindowMenu.numberOfItems > 0) {
        if (!_separatorMenuItem.menu) {
            // May not have Start Recording item and thus separator not yet present
            [gameMenu addItem:_separatorMenuItem];
        }
        
        // Add stop recording item to menu, will be removed when Game menu closes
        [gameMenu addItem:_stopRecordingMenuItem];
    }
    
    // If have exactly one window that can be stopped, no submenu needed.
    // More than one window that can be stopped, then a submenu of these window
    // titles to stop recording is added.
    if (recordedWindowMenu.numberOfItems == 1) {
        NSMenuItem *menuItem = [recordedWindowMenu itemAtIndex:0];
        // The window number associated with recorded window is stored in NSMenuItem's tag
        _stopRecordingMenuItem.tag = menuItem.tag;
        _stopRecordingMenuItem.action = @selector(stopRecordingWindowForSelectedMenuItem:);
        _stopRecordingMenuItem.target = self;
        _stopRecordingMenuItem.title = [NSString localizedStringWithFormat:NSLocalizedString(@"menu_item_stop_recording_fmt", "Stop Recording <Window Title>"), menuItem.title];
    } else if (recordedWindowMenu.numberOfItems > 1) {
        // More than one window being recorded, will show a submenu of window titles
        _stopRecordingSubmenu = recordedWindowMenu;
        _stopRecordingSubmenu.delegate = self;
        
        _stopRecordingMenuItem.submenu = _stopRecordingSubmenu;
        _stopRecordingMenuItem.title = NSLocalizedString(@"menu_item_stop_recording", "Stop Recording");
    }
}

- (NSMenu *)prepareSubmenuForStopRecordingMenuItem {
    NSMenu *submenu = [[NSMenu alloc] init];
    
    // Get the recorded windows to populate the menu items for stop recording submenu.
    NSArray *recordedWindowIDs = [self recordedWindowIDs];
    for (NSNumber *windowID in recordedWindowIDs) {
        NSWindow *recordedWindow = [NSApp windowWithWindowNumber:[windowID integerValue]];
        MBCDocument *gameDocument = (MBCDocument *)[recordedWindow windowController].document;
        NSMenuItem *newItem = [[NSMenuItem alloc] initWithTitle:gameDocument.displayName
                                                         action:@selector(stopRecordingWindowForSelectedMenuItem:)
                                                  keyEquivalent:@""];
        newItem.target = self;
        newItem.tag = recordedWindow.windowNumber;
        [submenu addItem:newItem];
    }
    
    return submenu;
}

- (void)initializeHighlightingWindowController {
    NSWindow *window = [[MBCRecordingHighlightWindow alloc] init];
    window.styleMask = NSWindowStyleMaskBorderless;
    window.backingType = NSBackingStoreBuffered;
    window.alphaValue = 0.1;
    window.ignoresMouseEvents = YES;
    window.level = NSNormalWindowLevel;
    _highlightWindowController = [[NSWindowController alloc] initWithWindow:window];
}

- (void)highlightWindow:(NSWindow *)window isStop:(BOOL)isStop {
    if (!_highlightWindowController) {
        // Create the window used to draw the highlight over window associated with menu item
        [self initializeHighlightingWindowController];
    }
    
    _highlightWindowController.window.backgroundColor = NSColor.blueColor;
    
    // Position the highlight window over the window associated with menu item
    [_highlightWindowController.window setFrame:window.frame display:YES];
    [_highlightWindowController showWindow:nil];
    [_highlightWindowController.window orderWindow:NSWindowAbove relativeTo:window.windowNumber];
}

- (void)menu:(NSMenu *)menu willHighlightItem:(NSMenuItem *)item {
    // Will only highlight if have more than one menu item to choose from.
    BOOL shouldHighlight = ([_stopRecordingSubmenu.itemArray containsObject:item] ||
                            [_startRecordingSubmenu.itemArray containsObject:item] ||
                            (_startRecordingMenuItem.menu && _stopRecordingMenuItem.menu));
    if (!shouldHighlight) {
        return;
    }
    
    NSWindow *windowToHighlight = nil;
    BOOL isStopRecordingItem = NO;
    
    // Determine if item is one of the start / stop recording menu items.
    // If so, get the windowNumber from the item's tag to look up NSWindow.
    if ([menu isEqualTo:_stopRecordingSubmenu] ||
        ([item isEqualTo:_stopRecordingMenuItem] && !_stopRecordingSubmenu)) {
        windowToHighlight = [NSApp windowWithWindowNumber:item.tag];
        isStopRecordingItem = YES;
    } else if ([menu isEqualTo:_startRecordingSubmenu] ||
               ([item isEqualTo:_startRecordingMenuItem] && !_startRecordingSubmenu)) {
        windowToHighlight = [NSApp windowWithWindowNumber:item.tag];
    }
    
    // If have a window, then add the highlight over that window to show which
    // window is highlighted in either the start or stop recording menu item(s)
    if (windowToHighlight) {
        [self highlightWindow:windowToHighlight isStop:isStopRecordingItem];
    } else {
        [_highlightWindowController close];
    }
}

/*!
 @abstract startRecordingWindowForSelectedMenuItem:
 @param item The menu item that was selected
 @discussion This is the action set on menu items to start recording.
 The item's tag contains the windowNumber of the window to record.
 */
- (void)startRecordingWindowForSelectedMenuItem:(NSMenuItem *)item {
    if (NSWindow *window = [NSApp windowWithWindowNumber:item.tag]) {
        [self startRecordingWindow:window completionHandler:^(NSError * _Nullable error) {
            NSLog(@"Started recording for window");
        }];
    } else {
        NSLog(@"No NSWindow found with windowNumber %@", @(item.tag));
    }
}

/*!
 @abstract stopRecordingWindowForSelectedMenuItem:
 @param item The menu item that was selected
 @discussion This is the action set on menu items to stop recording.
 The item's tag contains the windowNumber of the window to stop recording.
 */
- (void)stopRecordingWindowForSelectedMenuItem:(NSMenuItem *)item {
    if (NSWindow *window = [NSApp windowWithWindowNumber:item.tag]) {
        [self stopRecordingWindow:window completionHandler:^(NSError * _Nullable error) {
            // Save panel has been presented, clean up recording session.
            [self cleanupRecordingSessionForWindow:window];
        }];
    } else {
        NSLog(@"No NSWindow found with windowNumber %@", @(item.tag));
    }
}

- (void)menuDidClose:(NSMenu *)menu {
    NSMenu *gameMenu = [[[NSApp mainMenu] itemAtIndex:GAME_MENU_ITEM_INDEX] submenu];
    if ([menu isEqualTo:gameMenu]) {
        // Clean up menu for next presentation, where not all the
        // items may be needed when Game menu is presented again.
        
        // Clean up the start recording submenu
        _startRecordingMenuItem.submenu = nil;
        _startRecordingSubmenu.delegate = nil;
        _startRecordingSubmenu = nil;
        
        // Clean up the stop recording submenu
        _stopRecordingMenuItem.submenu = nil;
        _stopRecordingSubmenu.delegate = nil;
        _stopRecordingSubmenu = nil;
        
        // Remove the menu items if they are present in Game menu
        if (_separatorMenuItem.menu) {
            [gameMenu removeItem:_separatorMenuItem];
        }
        if (_startRecordingMenuItem.menu) {
            [gameMenu removeItem:_startRecordingMenuItem];
        }
        if (_stopRecordingMenuItem.menu) {
            [gameMenu removeItem:_stopRecordingMenuItem];
        }
        
        if ([_highlightWindowController.window isVisible]) {
            [_highlightWindowController close];
        }
    }
}

@end
