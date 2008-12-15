/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 1998-2003 Apple Computer, Inc.  All Rights Reserved.
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */


#import <Cocoa/Cocoa.h>
#import <Security/Authorization.h>
#import <Security/AuthorizationTags.h>
#import <sys/wait.h>
#import "USBLogger.h"

#define LOGGER_REFRESH_INTERVAL 0.1

@interface LoggerEntry : NSObject
{
    NSString *  _text;
    int         _level;
}

+ (void)initialize;
+ (void)replenishFreshEntries;
+ (LoggerEntry *)cachedFreshEntry;
- init;
- initWithText:(NSString *)text level:(int)level;
- (void)setText:(NSString *)text level:(int)level;
- (NSString *)text;
- (int)level;

@end

@interface USBLoggerController : NSObject <USBLoggerListener>
{
    IBOutlet id DumpCheckBox;
    IBOutlet id FilterTextField;
    IBOutlet id FilterProgressIndicator;
    IBOutlet id LoggerOutputTV;
    IBOutlet id LoggingLevelPopUp;
    IBOutlet id StartStopButton;
    
    NSMutableArray *    _outputLines;
    NSMutableString *   _outputBuffer;
    NSLock *            _bufferLock;
    NSLock *            _outputLock;
    NSString *          _currentFilterString;
    FILE *              _dumpingFile;
    
    USBLogger *         _logger;
    NSTimer *           _refreshTimer;
    
    BOOL                _klogKextisPresent;
    BOOL                _klogKextIsCorrectRevision;
}

- (void)setupRecentSearchesMenu;
- (IBAction)ChangeLoggingLevel:(id)sender;
- (IBAction)ClearOutput:(id)sender;
- (IBAction)MarkOutput:(id)sender;
- (IBAction)SaveOutput:(id)sender;
- (IBAction)Start:(id)sender;
- (IBAction)Stop:(id)sender;
- (IBAction)ToggleDumping:(id)sender;
- (IBAction)FilterOutput:(id)sender;

- (BOOL)isKlogKextPresent;
- (BOOL)isKlogCorrectRevision;
- (BOOL)installKLogKext;
- (BOOL)removeAndinstallKLogKext;

- (NSArray *)logEntries;
- (NSArray *)displayedLogLines;
- (void)scrollToVisibleLine:(NSString *)line;

- (void)handlePendingOutput:(NSTimer *)timer;
- (void)appendOutput:(NSString *)aString atLevel:(NSNumber *)level;
- (void)appendLoggerEntry:(LoggerEntry *)entry;
- (void)usbLoggerTextAvailable:(NSString *)text forLevel:(int)level;

@end

