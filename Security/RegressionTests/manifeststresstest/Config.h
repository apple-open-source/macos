//
//  Config.h
//  Security
//
//  Created by Ben Williamson on 6/2/17.
//
//

#import <Foundation/Foundation.h>

@interface Config : NSObject

// Number of distinct item names to chose from.
@property (nonatomic, assign) unsigned distinctNames;

// Number of distinct data values to chose from.
@property (nonatomic, assign) unsigned distinctValues;

// Max number of items we are allowed to create.
@property (nonatomic, assign) unsigned maxItems;

// Probability weighting for adding an item.
@property (nonatomic, assign) unsigned addItemWeight;

// Probability weighting for updating an item's name.
@property (nonatomic, assign) unsigned updateNameWeight;

// Probability weighting for updating an item's data.
@property (nonatomic, assign) unsigned updateDataWeight;

// Probability weighting for updating an item's name and data.
@property (nonatomic, assign) unsigned updateNameAndDataWeight;

// Probability weighting for deleting an item.
@property (nonatomic, assign) unsigned deleteItemWeight;

// Additional item name configuration, for isolating changes
@property (nonatomic) NSString *name;

// Additional view name
@property (nonatomic) NSString *view;

@end
