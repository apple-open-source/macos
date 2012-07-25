/*
  File:		MBCBoardViewAccessibility.mm
  Contains:	Accessibility navigation for chess board
  Copyright:	© 2004-2008 by Apple Inc., all rights reserved.
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

#import "MBCMoveTableViewAccessibility.h"
#import "MBCMoveTableView.h"

@implementation MBCMoveAccessibilityProxy

+ (id) proxyWithInfo:(MBCGameInfo *)info move:(int)move
{
	return [[[MBCMoveAccessibilityProxy alloc] 
				initWithInfo:info move:move]
			   autorelease];
}

- (id) initWithInfo:(MBCGameInfo *)info move:(int)move
{
	fInfo	= info;
	fMove   = move;

	return self;
}

- (BOOL) isEqual:(MBCMoveAccessibilityProxy *)other
{
	return [other isKindOfClass:[MBCMoveAccessibilityProxy class]]
		&& fInfo == other->fInfo && fMove == other->fMove;
}

- (NSUInteger)hash {
    // Equal objects must hash the same.
    return [fInfo hash] + fMove;
}

- (NSString *) description
{
	return [NSString stringWithFormat:@"Move %d", fMove];
}

- (NSArray *)accessibilityAttributeNames 
{
	return [NSArray arrayWithObjects:
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
					nil];
}

- (NSArray *)accessibilityActionNames
{
    return [NSArray array];
}

- (id)accessibilityFocusedUIElement
{
	return self;
}

- (BOOL)accessibilityIsIgnored
{
	return NO;
}

- (NSRect)accessibilityFocusRingBounds
{
    NSRect  rect = [[fInfo moveList] rectOfRow:fMove-1];
    rect         = [[fInfo moveList] convertRect:rect toView:nil];
    
    return [[[fInfo moveList] window] convertRectToScreen:rect];
}

- (id)accessibilityAttributeValue:(NSString *)attribute 
{
 	if ([attribute isEqual:NSAccessibilityParentAttribute])
		return [fInfo moveList];
	else if ([attribute isEqual:NSAccessibilityChildrenAttribute])
		return [NSArray array];
	else if ([attribute isEqual:NSAccessibilityWindowAttribute])
		return [[fInfo moveList] window];
	else if ([attribute isEqual:NSAccessibilityRoleAttribute])
		return NSAccessibilityStaticTextRole;
	else if ([attribute isEqual:NSAccessibilityRoleDescriptionAttribute])
		return NSAccessibilityRoleDescription(NSAccessibilityStaticTextRole, nil);
	else if ([attribute isEqual:NSAccessibilityPositionAttribute])
		return [NSValue valueWithPoint:
							[self accessibilityFocusRingBounds].origin];
	else if ([attribute isEqual:NSAccessibilitySizeAttribute])
		return [NSValue valueWithSize:
							[self accessibilityFocusRingBounds].size];
	else if ([attribute isEqual:NSAccessibilityTitleAttribute])
		return [fInfo describeMove:fMove];
	else if ([attribute isEqual:NSAccessibilityValueAttribute])
		return nil;
	else if ([attribute isEqual:NSAccessibilityDescriptionAttribute])
		return @"";
	else if ([attribute isEqual:NSAccessibilityFocusedAttribute])
		return [NSNumber numberWithBool:
							 [[NSApp accessibilityFocusedUIElement] 
								 isEqual:self]];
	else if ([attribute isEqual:NSAccessibilityEnabledAttribute])
		return [NSNumber numberWithBool:YES];
	else if ([attribute isEqual:NSAccessibilityTopLevelUIElementAttribute])
		return [[fInfo moveList] window];
#if 0
	else
		NSLog(@"unknown attr: %@\n", attribute);
#endif

	return nil;
}

- (BOOL)accessibilityIsAttributeSettable:(NSString *)attribute 
{
	if ([attribute isEqual:NSAccessibilityFocusedAttribute])
		return YES;

	return NO;
}

- (void)accessibilitySetValue:(id)value forAttribute:(NSString *)attribute 
{
}

@end

@implementation MBCMoveTableView ( Accessibility )


- (NSArray *)accessibilityAttributeNames 
{
	return [NSArray arrayWithObjects:
            NSAccessibilityRoleAttribute,
            NSAccessibilityRoleDescriptionAttribute,
            NSAccessibilityParentAttribute,
            NSAccessibilityChildrenAttribute,
            NSAccessibilityContentsAttribute,
            NSAccessibilityWindowAttribute,
            NSAccessibilityPositionAttribute,
            NSAccessibilitySizeAttribute,
            NSAccessibilityTopLevelUIElementAttribute,
            NSAccessibilitySelectedChildrenAttribute,
            NSAccessibilityDescriptionAttribute,
            nil];
}

- (NSArray *)accessibilityActionNames
{
    return [NSArray array];
}

- (NSString *)accessibilityRoleAttribute 
{
    return NSAccessibilityGroupRole;
}

- (NSArray *)accessibilityChildrenAttribute 
{
	NSInteger           numMoves    = [self numberOfRows];
    NSMutableArray *    kids        = [NSMutableArray arrayWithCapacity:numMoves];
	for (NSInteger move = 0; move++ < numMoves; )
		[kids addObject: [MBCMoveAccessibilityProxy proxyWithInfo:[self dataSource]
													 move:move]];
    
	return kids;
}

- (NSArray *)accessibilitySelectedChildrenAttribute
{
    return [NSArray arrayWithObject:
            [MBCMoveAccessibilityProxy
             proxyWithInfo:[self dataSource] move:[self selectedRow]+1]];
}

- (id)accessibilityAttributeValue:(NSString *)attribute 
{
	if ([attribute isEqual:NSAccessibilityChildrenAttribute] || [attribute isEqual:NSAccessibilityContentsAttribute]) {
		return [self accessibilityChildrenAttribute];
    } else if ([attribute isEqual:NSAccessibilitySelectedChildrenAttribute]) {
        return [self accessibilitySelectedChildrenAttribute];
    } else if ([attribute isEqual:NSAccessibilityDescriptionAttribute]) {
        return NSLocalizedStringFromTable(@"move_table_desc", @"Spoken", @"Moves");
    } else {
        return [super accessibilityAttributeValue:attribute];
    }
}

- (NSUInteger)accessibilityIndexOfChild:(id)child
{
    if ([child isKindOfClass:[MBCMoveAccessibilityProxy class]]) {
        MBCMoveAccessibilityProxy * moveProxy = (MBCMoveAccessibilityProxy *)child;
        
        if (moveProxy->fInfo == [self dataSource])
            return moveProxy->fMove-1;
        else 
            return NSNotFound;
    }
    return [super accessibilityIndexOfChild:child];
}

- (NSUInteger)accessibilityArrayAttributeCount:(NSString *)attribute
{
    if ([attribute isEqual:NSAccessibilityChildrenAttribute])
        return [self numberOfRows];
    else 
        return [super accessibilityArrayAttributeCount:attribute];
}

- (NSArray *)accessibilityArrayAttributeValues:(NSString *)attribute index:(NSUInteger)index maxCount:(NSUInteger)maxCount
{
    if ([attribute isEqual:NSAccessibilityChildrenAttribute]) {
        NSUInteger numKids = [self numberOfRows];
        NSMutableArray *    kids        = [NSMutableArray arrayWithCapacity:numKids];
        while (index++ < numKids && maxCount--)
            [kids addObject: [MBCMoveAccessibilityProxy proxyWithInfo:[self dataSource]
                                                                 move:index]];
        return kids;
    } else {
        return [super accessibilityArrayAttributeValues:attribute index:index maxCount:maxCount];
    }
}
            
- (BOOL)accessibilityIsIgnored
{
	return NO;
}

- (id)accessibilityHitTest:(NSPoint)point
{
    NSInteger   move = [self rowAtPoint:point];
    id          hit;
	if (move < 0) 
		hit = self;
	else 
		hit = [MBCMoveAccessibilityProxy proxyWithInfo:[self dataSource] move:move+1];

	return hit;
}

- (void)accessibilityPostNotification:(NSString *)notification 
{
    //
    // We are a group, groups have children, not rows
    //
    if ([notification isEqual:NSAccessibilitySelectedRowsChangedNotification])
        notification = NSAccessibilitySelectedChildrenChangedNotification;
    [super accessibilityPostNotification:notification];
}

@end

// Local Variables:
// mode:ObjC
// End:
