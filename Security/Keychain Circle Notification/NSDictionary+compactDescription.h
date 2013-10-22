//
//  NSDictionary+compactDescription.h
//  KeychainMigrator
//
//  Created by J Osborne on 2/19/13.
//
//

#import <Foundation/Foundation.h>

@interface NSDictionary (compactDescription)
-(NSString*)compactDescription;
-(NSString*)compactDescriptionWithoutItemData;
@end
