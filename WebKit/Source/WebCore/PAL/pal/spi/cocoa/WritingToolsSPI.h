/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#import <wtf/Compiler.h>
#import <wtf/Platform.h>

DECLARE_SYSTEM_HEADER

#if ENABLE(WRITING_TOOLS)

#if USE(APPLE_INTERNAL_SDK)

#import <WritingTools/WTSession_Private.h>
#import <WritingTools/WritingTools.h>

#else

#import <Foundation/Foundation.h>

extern NSAttributedStringKey const WTWritingToolsPreservedAttributeName;

typedef NS_ENUM(NSInteger, WTRequestedTool) {
    WTRequestedToolIndex = 0,

    WTRequestedToolProofread = 1,
    WTRequestedToolRewrite = 2,
    WTRequestedToolRewriteProofread = 3,

    WTRequestedToolRewriteFriendly = 11,
    WTRequestedToolRewriteProfessional = 12,
    WTRequestedToolRewriteConcise = 13,
    WTRequestedToolRewriteOpenEnded = 19,

    WTRequestedToolTransformSummary = 21,
    WTRequestedToolTransformKeyPoints = 22,
    WTRequestedToolTransformList = 23,
    WTRequestedToolTransformTable = 24,

    WTRequestedToolSmartReply = 101,

    WTRequestedToolCompose = 201,
};

// MARK: WTContext

@protocol WTTextViewDelegate;

@interface WTContext : NSObject

@property (nonatomic, readonly) NSUUID *uuid;

@property (nonatomic, readonly) NSAttributedString *attributedText;

@property (nonatomic) NSRange range;

- (instancetype)initWithAttributedText:(NSAttributedString *)attributedText range:(NSRange)range;

@end

// MARK: WTSession

typedef NS_ENUM(NSInteger, WTCompositionSessionType) {
    WTCompositionSessionTypeNone,
    WTCompositionSessionTypeMagic,
    WTCompositionSessionTypeFriendly,
    WTCompositionSessionTypeProfessional,
    WTCompositionSessionTypeConcise,
    WTCompositionSessionTypeOpenEnded,
    WTCompositionSessionTypeSummary,
    WTCompositionSessionTypeKeyPoints,
    WTCompositionSessionTypeList,
    WTCompositionSessionTypeTable,
    WTCompositionSessionTypeCompose,
    WTCompositionSessionTypeSmartReply,
    WTCompositionSessionTypeProofread,
};

typedef NS_ENUM(NSInteger, WTSessionType) {
    WTSessionTypeProofreading = 1,
    WTSessionTypeComposition,
};

@interface WTSession : NSObject

@property (nonatomic, readonly) NSUUID *uuid;
@property (nonatomic, readonly) WTSessionType type;

@property (nonatomic, weak) id<WTTextViewDelegate> textViewDelegate;

@property (nonatomic) WTCompositionSessionType compositionSessionType;
@property (nonatomic, readonly) WTRequestedTool requestedTool;

- (instancetype)initWithType:(WTSessionType)type textViewDelegate:(id<WTTextViewDelegate>)textViewDelegate;

@end

// MARK: WTTextSuggestion

typedef NS_ENUM(NSInteger, WTTextSuggestionState) {
    WTTextSuggestionStatePending = 0,
    WTTextSuggestionStateReviewing = 1,
    WTTextSuggestionStateRejected = 3,
    WTTextSuggestionStateInvalid = 4,
};

@interface WTTextSuggestion : NSObject

@property (nonatomic, readonly) NSUUID *uuid;

@property (nonatomic, readonly) NSRange originalRange;

@property (nonatomic, readonly) NSString *replacement;

@property (nonatomic) WTTextSuggestionState state;

- (instancetype)initWithOriginalRange:(NSRange)originalRange replacement:(NSString *)replacement;

@end

// MARK: WTTextViewDelegate

#if PLATFORM(IOS_FAMILY)
@class UIView;
#else
@class NSView;
#endif

@protocol WTTextViewDelegate

- (void)proofreadingSessionWithUUID:(NSUUID *)sessionUUID updateState:(WTTextSuggestionState)state forSuggestionWithUUID:(NSUUID *)suggestionUUID;

#if PLATFORM(IOS_FAMILY)
- (void)proofreadingSessionWithUUID:(NSUUID *)sessionUUID showDetailsForSuggestionWithUUID:(NSUUID *)suggestionUUID relativeToRect:(CGRect)rect inView:(UIView *)sourceView;
#else
- (void)proofreadingSessionWithUUID:(NSUUID *)sessionUUID showDetailsForSuggestionWithUUID:(NSUUID *)suggestionUUID relativeToRect:(NSRect)rect inView:(NSView *)sourceView;
#endif

@end

// MARK: WTWritingToolsDelegate

@protocol WTWritingToolsDelegate

- (void)willBeginWritingToolsSession:(WTSession *)session requestContexts:(void (^)(NSArray<WTContext *> *contexts))completion;

- (void)didBeginWritingToolsSession:(WTSession *)session contexts:(NSArray<WTContext *> *)contexts;

typedef NS_ENUM(NSInteger, WTAction) {
    WTActionShowOriginal = 1,
    WTActionShowRewritten,
    WTActionCompositionRestart,
    WTActionCompositionRefine,
};

- (void)writingToolsSession:(WTSession *)session didReceiveAction:(WTAction)action;

- (void)didEndWritingToolsSession:(WTSession *)session accepted:(BOOL)accepted;

- (void)proofreadingSession:(WTSession *)session didReceiveSuggestions:(NSArray<WTTextSuggestion *> *)suggestions processedRange:(NSRange)range inContext:(WTContext *)context finished:(BOOL)finished;

- (void)proofreadingSession:(WTSession *)session didUpdateState:(WTTextSuggestionState)state forSuggestionWithUUID:(NSUUID *)uuid inContext:(WTContext *)context;

- (void)compositionSession:(WTSession *)session didReceiveText:(NSAttributedString *)attributedText replacementRange:(NSRange)range inContext:(WTContext *)context finished:(BOOL)finished;

@end

#endif // USE(APPLE_INTERNAL_SDK)

#endif // ENABLE(WRITING_TOOLS)
