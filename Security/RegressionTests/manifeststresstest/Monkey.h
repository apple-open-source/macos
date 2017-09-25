//
//  Monkey.h
//  Security
//
//  Created by Ben Williamson on 6/1/17.
//
//

#import <Foundation/Foundation.h>

@class Config;
@class Keychain;

// A monkey has an array of items that it has created.
// It can randomly choose to add an item, delete an item, or update the data in an item.
//
// All items exist within the access group "manifeststresstest"
// which is set to have the appropriate view hint so that it syncs via CKKS.
//
// Items are generic password items, having a service, an account and data.
// The service, account and data values are chosen from sets of a limited size, to encourage
// the possibility of collisions.


// Adds and deletes generic password items

@interface Monkey : NSObject

@property (nonatomic, strong) Keychain *keychain; // if nil, this is a dry run
@property (nonatomic, assign) unsigned step;

// Incremented when we try to add an item and it already exists.
@property (nonatomic, assign) unsigned addDuplicateCounter;

// Incremented when we try to update or delete an item and it does not exist.
@property (nonatomic, assign) unsigned notFoundCounter;

// Peak number of items we have created so far.
@property (nonatomic, assign) unsigned peakItems;

// Current number of items written
@property (nonatomic, readonly) unsigned itemCount;

@property (nonatomic, readonly) Config *config;

- (instancetype)initWithConfig:(Config *)config;

- (void)advanceOneStep;

- (void)cleanup;

@end
