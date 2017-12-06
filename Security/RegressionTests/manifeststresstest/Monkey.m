//
//  Monkey.m
//  Security
//
//  Created by Ben Williamson on 6/1/17.
//
//

#import "Monkey.h"
#import "Config.h"
#import "Keychain.h"

#import <stdio.h>
#import <stdlib.h>


@interface Monkey ()

@property (nonatomic, strong) Config *config;

// Names that we could choose from when adding a new item.
// When we add an item its name is moved to usedNames.
@property (nonatomic, strong) NSMutableArray<NSString *> *freeNames;

// Names and persistent references of the items we have written, which we can update or delete.
// When we delete an item its name is moved to freeNames.
@property (nonatomic, strong) NSMutableArray<NSMutableArray *> *usedNames;

@end


@implementation Monkey

- (instancetype)initWithConfig:(Config *)config
{
    if (self = [super init]) {
        _config = config;
        _freeNames = [NSMutableArray arrayWithCapacity:config.distinctNames];
        _usedNames = [NSMutableArray arrayWithCapacity:config.distinctNames];
        for (unsigned i = 0; i < config.distinctNames; i++) {
            if (config.name) {
                [_freeNames addObject:[NSString stringWithFormat:@"item-%@-%u", config.name, i]];
            } else {
                [_freeNames addObject:[NSString stringWithFormat:@"item-%u", i]];
            }
        }
    }
    return self;
}

- (void)advanceOneStep
{
    static const int tryLimit = 1000;
    for (int tries = 0; tries < tryLimit; tries++) {
        if ([self tryAdvanceOneStep]) {
            self.step++;
            return;
        }
    }
    printf("Chose %d random actions but none of them made sense, giving up!\n", tryLimit);
    printf("This is an internal failure of the manifeststresstest tool, not the system under test.\n");
    NSLog(@"Chose %d random actions but none of them made sense, giving up!", tryLimit);
    exit(1);
}

// return YES if it actually took a step, NO if the randomly chosen action was invalid
- (BOOL)tryAdvanceOneStep
{
    unsigned totalWeight =
        self.config.addItemWeight +
        self.config.updateNameWeight +
        self.config.updateDataWeight +
        self.config.updateNameAndDataWeight +
        self.config.deleteItemWeight;

    if (totalWeight == 0) {
        printf("Invalid manifeststresstest configuration:\n"
               "At least one weight must be nonzero, or else we cannot choose any action.\n");
        exit(1);
    }
    if (totalWeight > RAND_MAX) {
        printf("Invalid manifeststresstest configuration:\n"
               "The total of the weights must not exceed RAND_MAX == %d.\n", RAND_MAX);
        exit(1);
    }
    unsigned r = random() % totalWeight;
    
    if (r < self.config.addItemWeight) {
        return [self addItem];
    }
    r -= self.config.addItemWeight;
    
    if (r < self.config.updateNameWeight) {
        return [self updateNameAndValue:NO];
    }
    r -= self.config.updateNameWeight;
    
    if (r < self.config.updateDataWeight) {
        return [self updateValue];
    }
    r -= self.config.updateDataWeight;
    
    if (r < self.config.updateNameAndDataWeight) {
        return [self updateNameAndValue:YES];
    }
    r -= self.config.updateNameAndDataWeight;
    
    if (r < self.config.deleteItemWeight) {
        return [self deleteItem];
    }
    NSAssert(false, @"Chosen random number was beyond totalWeight?!?");
    return NO;
}

- (NSString *)randomValue
{
    if (0 == self.config.distinctValues) {
        printf("Invalid manifeststresstest configuration:\n"
               "Must allow a nonzero number of distinct values.\n");
        exit(1);
    }
    unsigned n = random() % self.config.distinctValues;
    return [NSString stringWithFormat:@"data-%u", n];
}

- (NSString *)prefix
{
    return [NSString stringWithFormat:@"[step:%d items:%d dup:%d gone:%d]",
            self.step,
            self.itemCount,
            self.addDuplicateCounter,
            self.notFoundCounter];
}

- (void)unexpectedError:(OSStatus)status
{
    NSLog(@"Unexpected error %d at step %d", (int)status, self.step);
    exit(1);
}

- (BOOL)addItem
{
    if ([self.usedNames count] >= self.config.maxItems) {
        return NO;
    }
    NSUInteger freeCount = [self.freeNames count];
    if (freeCount == 0) {
        return NO;
    }
    NSUInteger index = random() % freeCount;
    NSString *name = self.freeNames[index];
    NSString *value = [self randomValue];

    NSLog(@"%@ addItem in %@ %@ = %@", [self prefix], self.config.view, name, value);
    NSArray *itemRef = nil;
    OSStatus status = [self.keychain addItem:name value:value view:self.config.view pRef:&itemRef];
    switch (status) {
        case errSecSuccess:
            [self.freeNames removeObjectAtIndex:index];
            [self.usedNames addObject:[NSMutableArray arrayWithArray:@[name, itemRef]]];

            unsigned usedCount = (unsigned)[self.usedNames count];
            if (self.peakItems < usedCount) {
                self.peakItems = usedCount;
            }
            break;
        case errSecDuplicateItem:
            self.addDuplicateCounter++;
            break;
        default:
            [self unexpectedError:status];
    }
    return YES;
}

- (BOOL)updateNameAndValue:(BOOL)updateValue
{
    NSUInteger freeCount = [self.freeNames count];
    NSUInteger usedCount = [self.usedNames count];
    if (usedCount == 0 || freeCount == 0) {
        return NO;
    }
    NSUInteger usedIndex = random() % usedCount;
    NSUInteger freeIndex = random() % freeCount;
    NSMutableArray *oldItem = self.usedNames[usedIndex];
    NSArray *oldRef = oldItem[1];
    NSString *oldName = oldItem[0];
    NSString *newName = self.freeNames[freeIndex];
    
    OSStatus status;
    if (updateValue) {
        NSString *value = [self randomValue];
        NSLog(@"%@ updateNameAndValue %@(%@) -> %@ = %@\n", [self prefix], oldName, oldRef, newName, value);
        status = [self.keychain updateItem:oldRef newName:newName newValue:value];
    } else {
        NSLog(@"%@ updateName %@(%@) -> %@\n", [self prefix], oldName, oldRef, newName);
        status = [self.keychain updateItem:oldRef newName:newName];
    }
    switch (status) {
        case errSecSuccess:
            /* Update tracking arrays */
            [self.freeNames removeObject:newName];
            [self.freeNames addObject:oldName];
            self.usedNames[usedIndex][0] = newName;
            break;
        case errSecItemNotFound:
            /* Update tracking arrays (someone else deleted this item) */
            [self.usedNames removeObject:oldItem];
            [self.freeNames addObject:oldName];
            self.notFoundCounter++;
            break;
        case errSecDuplicateItem:
            self.addDuplicateCounter++;
            // newName already exists, which means our attempted updateItem operation
            // did not rename oldName to newName, so the item still has oldName.
            // No action required.
            break;
        default:
            [self unexpectedError:status];
    }
    return YES;
}

- (BOOL)updateValue
{
    NSUInteger usedCount = [self.usedNames count];
    if (usedCount == 0) {
        return NO;
    }
    NSUInteger usedIndex = random() % usedCount;
    NSArray *item = self.usedNames[usedIndex];

    NSString *value = [self randomValue];
    
    NSLog(@"%@ updateValue %@(%@) = %@", [self prefix], item[0], item[1], value);
    OSStatus status = [self.keychain updateItem:item[1] newValue:value];
    switch (status) {
        case errSecSuccess:
            break;
        case errSecItemNotFound:
            self.notFoundCounter++;
            break;
        default:
            [self unexpectedError:status];
    }
    return YES;
}

- (BOOL)deleteItem
{
    NSUInteger usedCount = [self.usedNames count];
    if (usedCount == 0) {
        return NO;
    }
    NSUInteger usedIndex = random() % usedCount;
    NSArray *item = self.usedNames[usedIndex];
    
    NSLog(@"%@ deleteItem %@(%@)", [self prefix], item[0], item[1]);
    OSStatus status = [self.keychain deleteItem:item[1]];
    switch (status) {
        case errSecItemNotFound:
            /* someone else deleted this item */
            self.notFoundCounter++;
            /* fall through */
        case errSecSuccess:
            [self.usedNames removeObjectAtIndex:usedIndex];
            [self.freeNames addObject:item[0]];
            break;
        default:
            [self unexpectedError:status];
    }
    return YES;
}

- (unsigned)itemCount
{
    return (unsigned)[self.usedNames count];
}

- (void)cleanup
{
    for (NSArray *item in self.usedNames) {
        NSLog(@"cleanup deleting %@(%@)", item[0], item[1]);
        [self.keychain deleteItem:item[1]];
    }
}

@end
