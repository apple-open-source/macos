//
//  AuthorityTableView.h
//  security_systemkeychain
//
//  Created by Love Hörnquist Åstrand on 2012-03-22.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@protocol AuthorityTableDelegate <NSObject>

- (void)deleteAuthority:(NSTableView *)tableView atIndexes:(NSIndexSet *)indexes;

@end

@interface AuthorityTableView : NSTableView

@property (assign) IBOutlet id<AuthorityTableDelegate> authorityDelegate;

@end
