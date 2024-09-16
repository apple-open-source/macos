/*
  File:        MBCBoardMTLViewAccessibility.mm
  Contains:    Accessibility navigation for chess board
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

#import "MBCBoardMTLViewAccessibility.h"
#import "MBCBoard.h"
#import "MBCBoardMTLViewMouse.h"
#import "MBCInteractivePlayer.h"

@implementation MBCBoardMTLAccessibilityProxy

- (id)initWithView:(MBCBoardMTLView *)view square:(MBCSquare)square {
    self = [super init];
    if (self) {
        _view = view;
        _square = square;
    }
    return self;
}

+ (id)proxyWithView:(MBCBoardMTLView *)view square:(MBCSquare)square {
    return [[MBCBoardMTLAccessibilityProxy alloc] initWithView:view square:square];
}

- (BOOL)isEqual:(MBCBoardMTLAccessibilityProxy *)other {
    return [other isKindOfClass:[MBCBoardMTLAccessibilityProxy class]]
        && (_square == other->_square);
}

- (NSUInteger)hash {
    // Equal objects must hash the same.
    return [_view hash] + _square;
}

- (NSString *)description {
    return [NSString stringWithFormat:@"Square %c%u", Col(_square), Row(_square)];
}

- (NSArray *)accessibilityAttributeNames {
    static NSArray *sAttributeNames;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        sAttributeNames = @[
                           NSAccessibilityRoleAttribute,
                           NSAccessibilityRoleDescriptionAttribute,
                           NSAccessibilityParentAttribute,
                           NSAccessibilityWindowAttribute,
                           NSAccessibilityPositionAttribute,
                           NSAccessibilitySizeAttribute,
                           NSAccessibilityTitleAttribute,
                           NSAccessibilityDescriptionAttribute,
                           NSAccessibilityFocusedAttribute,
                           NSAccessibilityEnabledAttribute,
                           NSAccessibilityTopLevelUIElementAttribute,
                           ];
    });
    return sAttributeNames;
}

- (NSArray *)accessibilityActionNames {
    return @[NSAccessibilityPressAction];
}

- (NSString *)accessibilityActionDescription:(NSString *)action {
    if ([action isEqual:NSAccessibilityPressAction]) {
        return NSLocalizedString(@"select_square", "select");
    } else {
        return NSAccessibilityActionDescription(action);
    }
}

- (id)accessibilityFocusedUIElement {
    return self;
}

- (BOOL)accessibilityIsIgnored {
    return NO;
}

- (NSRect)accessibilityFocusRingBounds {
    NSRect rect = [_view approximateBoundsOfSquare:_square];

    rect.origin = [[_view window] convertPointToScreen:[_view convertPoint:rect.origin toView:nil]];

    return rect;
}

- (id)accessibilityAttributeValue:(NSString *)attribute {
    if ([attribute isEqual:NSAccessibilityParentAttribute]) {
        return _view;
    } else if ([attribute isEqual:NSAccessibilityChildrenAttribute]) {
        return [NSArray array];
    } else if ([attribute isEqual:NSAccessibilityWindowAttribute]) {
        return [_view window];
    } else if ([attribute isEqual:NSAccessibilityRoleAttribute]) {
        return NSAccessibilityButtonRole;
    } else if ([attribute isEqual:NSAccessibilityRoleDescriptionAttribute]) {
        return NSAccessibilityRoleDescription(NSAccessibilityButtonRole, nil);
    } else if ([attribute isEqual:NSAccessibilityPositionAttribute]) {
        return [NSValue valueWithPoint:[self accessibilityFocusRingBounds].origin];
    } else if ([attribute isEqual:NSAccessibilitySizeAttribute]) {
        return [NSValue valueWithSize:[self accessibilityFocusRingBounds].size];
    } else if ([attribute isEqual:NSAccessibilityTitleAttribute]) {
        return [_view describeSquare:_square];
    } else if ([attribute isEqual:NSAccessibilityDescriptionAttribute]) {
        return [_view describeSquare:_square];
    } else if ([attribute isEqual:NSAccessibilityValueAttribute]) {
        return nil;
    } else if ([attribute isEqual:NSAccessibilityDescriptionAttribute]) {
        return nil;
    } else if ([attribute isEqual:NSAccessibilityFocusedAttribute]) {
        return [NSNumber numberWithBool:[[NSApp accessibilityFocusedUIElement] isEqual:self]];
    } else if ([attribute isEqual:NSAccessibilityEnabledAttribute]) {
        return [NSNumber numberWithBool:YES];
    } else if ([attribute isEqual:NSAccessibilityTopLevelUIElementAttribute]) {
        return [_view window];
    }

    return nil;
}

- (BOOL)accessibilityIsAttributeSettable:(NSString *)attribute {
    return [attribute isEqual:NSAccessibilityFocusedAttribute];
}

- (void)accessibilitySetValue:(id)value forAttribute:(NSString *)attribute {
    
}

- (void)accessibilityPerformAction:(NSString *)action {
    if ([action isEqual:NSAccessibilityPressAction]) {
        [_view selectSquare:_square];
    }
}

@end

@implementation MBCBoardMTLView (Accessibility)

- (NSString *)accessibilityRoleAttribute {
    return NSAccessibilityGroupRole;
}

- (NSArray *)accessibilityChildrenAttribute {
    NSMutableArray * children = [[NSMutableArray alloc] init];
    for (MBCSquare square = 0; square < 64; ++square) {
        [children addObject:[MBCBoardMTLAccessibilityProxy proxyWithView:self square:square]];
    }
    return children;
}

- (BOOL)accessibilityIsIgnored {
    return NO;
}

- (id)accessibilityHitTest:(NSPoint)point {
    NSPoint local = [self convertPoint:[[self window] convertPointFromScreen:point] fromView:nil];
    MBCPosition position = [self mouseToPosition:local];
    MBCSquare square = [self positionToSquare:&position];

    id hit = nil;
    if (square == kInvalidSquare) {
        hit = self;
    } else {
        hit = [MBCBoardMTLAccessibilityProxy proxyWithView:self square:square];
    }

    return hit;
}

static NSString *sPieceID[] = {
    @"",
    @"white_king",
    @"white_queen",
    @"white_bishop",
    @"white_knight",
    @"white_rook",
    @"white_pawn",
    @"",
    @"",
    @"black_king",
    @"black_queen",
    @"black_bishop",
    @"black_knight",
    @"black_rook",
    @"black_pawn"
};

static NSString * sPieceName[] = {
    @"",
    @"white king",
    @"white queen",
    @"white bishop",
    @"white knight",
    @"white rook",
    @"white pawn",
    @"",
    @"",
    @"black king",
    @"black queen",
    @"black bishop",
    @"black knight",
    @"black rook",
    @"black pawn"
};

- (NSString *)describeSquare:(MBCSquare)square {
    MBCPiece piece = What([_board curContents:square]);

    if (piece) {
        return [NSString localizedStringWithFormat:@"%@, %c%u",
                NSLocalizedString(sPieceID[piece], sPieceName[p]),
                Col(square), Row(square)];
    } else {
        return [NSString localizedStringWithFormat:@"%c%u",
                Col(square), Row(square)];
    }
}

- (void)selectSquare:(MBCSquare)square {
    if (_pickedSquare != kInvalidSquare) {
        [_interactive startSelection:_pickedSquare];
        [_interactive endSelection:square animate:YES];
    } else {
        [_interactive startSelection:square];
        [self clickPiece];
    }
}

@end
