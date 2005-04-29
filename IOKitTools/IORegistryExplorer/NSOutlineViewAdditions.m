/* NSOutlineViewAdditions.m created by epeyton on Wed 14-Apr-1999 */

#import "NSOutlineViewAdditions.h"

@implementation NSOutlineView (NSOutlineViewAdditions)

- (id)parentItemOfItem:(id)item
{
    id retValue = nil;
    int level = [self levelForItem:item];
    int index = [self rowForItem:item];
    
    if (level == 0 && index <= 0) {
        retValue = nil;
    } else {
        int targetLevel = level - 1;
        int currentIndex;

        for (currentIndex = [self rowForItem:item]; currentIndex >= 0; currentIndex--) {
            if ([self levelForItem:[self itemAtRow:currentIndex]] == targetLevel) {
                return [self itemAtRow:currentIndex];
            }
        }
        
    }
    return retValue;
}

@end
