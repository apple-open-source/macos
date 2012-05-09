/*
  File:		MBCBoardViewAccessibility.mm
  Contains:	Accessibility navigation for chess board
  Version:	1.0
  Copyright:	© 2004-2008 by Apple Computer, Inc., all rights reserved.
  File Ownership:
*/

#import "MBCBoardViewAccessibility.h"
#import "MBCBoardViewMouse.h"
#import "MBCInteractivePlayer.h"

@implementation MBCBoardAccessibilityProxy

+ (id) proxyWithView:(MBCBoardView *)view square:(MBCSquare)square
{
	return [[[MBCBoardAccessibilityProxy alloc] 
				initWithView:view square:square]
			   autorelease];
}

- (id) initWithView:(MBCBoardView *)view square:(MBCSquare)square
{
	fView	= view;
	fSquare = square;

	return self;
}

- (BOOL) isEqual:(MBCBoardAccessibilityProxy *)other
{
	return [other isKindOfClass:[MBCBoardAccessibilityProxy class]]
		&& fSquare == other->fSquare;
}

- (NSUInteger)hash {
    // Equal objects must hash the same.
    return [fView hash] + fSquare;
}

- (NSString *) description
{
	return [NSString stringWithFormat:@"Square %c%u", 
					 Col(fSquare), Row(fSquare)];
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
	return [NSArray arrayWithObject:NSAccessibilityPressAction];
}

- (NSString *)accessibilityActionDescription:(NSString *)action
{
	if ([action isEqual:NSAccessibilityPressAction])
		return NSLocalizedString(@"select_square", "select");
	else
		return NSAccessibilityActionDescription(action);
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
	NSRect r = [fView approximateBoundsOfSquare:fSquare];

	r.origin = [[fView window] convertBaseToScreen:
								   [fView convertPoint:r.origin toView:nil]];

	return r;
}

- (id)accessibilityAttributeValue:(NSString *)attribute 
{
	if ([attribute isEqual:NSAccessibilityParentAttribute])
		return fView;
	else if ([attribute isEqual:NSAccessibilityChildrenAttribute])
		return [NSArray array];
	else if ([attribute isEqual:NSAccessibilityWindowAttribute])
		return [fView window];
	else if ([attribute isEqual:NSAccessibilityRoleAttribute])
		return NSAccessibilityButtonRole;
	else if ([attribute isEqual:NSAccessibilityRoleDescriptionAttribute])
		return NSAccessibilityRoleDescription(NSAccessibilityButtonRole, nil);
	else if ([attribute isEqual:NSAccessibilityPositionAttribute])
		return [NSValue valueWithPoint:
							[self accessibilityFocusRingBounds].origin];
	else if ([attribute isEqual:NSAccessibilitySizeAttribute])
		return [NSValue valueWithSize:
							[self accessibilityFocusRingBounds].size];
	else if ([attribute isEqual:NSAccessibilityTitleAttribute])
		return [fView describeSquare:fSquare];
	else if ([attribute isEqual:NSAccessibilityDescriptionAttribute])
		return [fView describeSquare:fSquare];
	else if ([attribute isEqual:NSAccessibilityValueAttribute])
		return nil;
	else if ([attribute isEqual:NSAccessibilityDescriptionAttribute])
		return nil;
	else if ([attribute isEqual:NSAccessibilityFocusedAttribute])
		return [NSNumber numberWithBool:
							 [[NSApp accessibilityFocusedUIElement] 
								 isEqual:self]];
	else if ([attribute isEqual:NSAccessibilityEnabledAttribute])
		return [NSNumber numberWithBool:YES];
	else if ([attribute isEqual:NSAccessibilityTopLevelUIElementAttribute])
		return [fView window];
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

- (void)accessibilityPerformAction:(NSString *)action
{
	if ([action isEqual:NSAccessibilityPressAction]) {
		[fView selectSquare:fSquare];
	}
}

@end

@implementation MBCBoardView ( Accessibility )

- (NSString *)accessibilityRoleAttribute 
{
    return NSAccessibilityGroupRole;
}

- (NSArray *)accessibilityChildrenAttribute 
{
	NSMutableArray * kids = [[[NSMutableArray alloc] init] autorelease];
	for (MBCSquare square = 0; square<64; ++square)
		[kids addObject: [MBCBoardAccessibilityProxy proxyWithView:self 
													 square:square]];
	return kids;
}

- (BOOL)accessibilityIsIgnored
{
	return NO;
}

- (id)accessibilityHitTest:(NSPoint)point
{
    NSPoint local 		= 
		[self convertPoint:[[self window] 
							   convertScreenToBase:point] fromView:nil];
	MBCPosition	pos		= [self mouseToPosition:local];
	MBCSquare 	where 	= [self positionToSquare:&pos];

	id hit;
	if (where == kInvalidSquare) 
		hit = self;
	else 
		hit = [MBCBoardAccessibilityProxy proxyWithView:self square:where];

	return hit;
}

#if 0
- (id)accessibilityAttributeValue:(NSString *)attribute 
{
	id v = [super accessibilityAttributeValue:attribute];
	NSLog(@"Value %@ = %@\n", attribute, v);

	return v;
}

- (BOOL)accessibilityIsAttributeSettable:(NSString *)attribute 
{
	BOOL s = [super accessibilityIsAttributeSettable:attribute];
	NSLog(@"IsSettable %@ = %d\n", attribute, s);

	return s;
}

- (void)accessibilitySetValue:(id)value forAttribute:(NSString *)attribute 
{
	NSLog(@"Set %@ = %@\n", attribute, value);
	[super accessibilitySetValue:value forAttribute:attribute];
}
#endif

static NSString * sPieceID[] = {
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

- (NSString *) describeSquare:(MBCSquare)square
{
	MBCPiece p = What([fBoard curContents:square]);

	if (p)
		return [NSString stringWithFormat:@"%@, %c%u", 
						 NSLocalizedString(sPieceID[p], sPieceName[p]),
						 Col(square), Row(square)];
	else
		return [NSString stringWithFormat:@"%c%u",
						 Col(square), Row(square)];
}

- (void) selectSquare:(MBCSquare)square
{
	if (fPickedSquare != kInvalidSquare) {
		[fInteractive startSelection:fPickedSquare];
		[fInteractive endSelection:square animate:YES];
	} else {
		[fInteractive startSelection:square];
		[self clickPiece];
	}
}

@end

// Local Variables:
// mode:ObjC
// End:
