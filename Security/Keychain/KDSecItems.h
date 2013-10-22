//
//  KDSecItems.h
//  Security
//
//  Created by J Osborne on 2/14/13.
//
//

#import <Foundation/Foundation.h>

extern NSString *kKDSecItemsUpdated;

@interface KDSecItems : NSObject<NSTableViewDataSource>
-(NSInteger)numberOfRowsInTableView:(NSTableView*)t;
- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(NSInteger)rowIndex;

-(void)loadItems;
@end
