//
//  mark.h
//  Security
//
//  Created by Ben Williamson on 6/2/17.
//
//

#import <Foundation/Foundation.h>

@class Keychain;

NSDictionary<NSString *, NSString *> *markForIdent(NSString *ident);
NSDictionary<NSString *, NSString *> *updateForIdent(NSString *ident);

void writeMark(NSString *ident, NSString *view);

void deleteMark(NSString *ident);

void updateMark(NSString *ident);

// Returns YES if the access group contains exactly the expected items
// of the given idents (and no other items) or NO otherwise.
BOOL verifyMarks(NSArray<NSString *> *idents);
BOOL verifyUpdateMarks(NSArray<NSString *> *idents);
