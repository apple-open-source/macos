//
//  JABDeleteRosterUserAction.h
//  ChatServer/jabber_autobuddy
//
//  Created by Steve Peralta on 7/18/08.
//  Copyright 2008 Apple. All rights reserved.
//

#import <Foundation/Foundation.h>

#import "JABDatabaseAction.h"

@interface JABDeleteRosterUserAction : JABDatabaseAction {

}

+ (NSArray *) jabGetDeleteRosterUserItems;

- (void) doDBAction;

@end

