//
//  AuthorityTableView.m
//  security_systemkeychain
//
//  Created by Love Hörnquist Åstrand on 2012-03-22.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#import "AuthorityTableView.h"

@implementation AuthorityTableView

- (void)keyDown:(NSEvent *)event
{
    NSString *string = [event charactersIgnoringModifiers];
    if ([string length] == 0) {
	[super keyDown:event];
	return;
    }

    unichar key = [string characterAtIndex:0];
    if (key == NSDeleteCharacter) {
	[self.authorityDelegate deleteAuthority:self atIndexes:[self selectedRowIndexes]];
	return;
    }
    [super keyDown:event];
}

@end
