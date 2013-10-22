//
//  NSArray+mapWithBlock.h
//  Security
//
//  Created by J Osborne on 2/26/13.
//
//

#import <Foundation/Foundation.h>

typedef id (^mapBlock)(id obj);

@interface NSArray (mapWithBlock)
-(NSArray*)mapWithBlock:(mapBlock)block;
@end
