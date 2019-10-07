
// HIDEvent+HIDEventDesc.m
// HID


#import "HIDEvent+HIDEventFields.h"

#import "HIDEventFieldsPrivate.h"
#import <AssertMacros.h>

NS_ASSUME_NONNULL_BEGIN

#define IS_VALID_EVENT_FIELD(info) (info.field == 0 && info.fieldType == kEventFieldDataType_None && info.name == NULL) ? 0 : 1

@implementation HIDEvent (HIDEventDesc)

-(void) enumerateFieldsWithBlock:(HIDEventFieldInfoBlock) block
{
    HIDEventFieldInfo *info = nil;
    NSInteger index = 0;
    require(block, exit);
    
    info = [self getEventFields];
    require(info, exit);
    
    while(IS_VALID_EVENT_FIELD(info[index])) {
        block(&info[index]);
        index++;
    }
exit:
    return;
}
-(HIDEventFieldInfo* __nullable) getEventFields
{
    HIDEventFieldInfo *info = NULL;
    NSInteger index = 0;
    while (!info && hidEventFieldDescTable[index].type != kIOHIDEventTypeCount) {
        
        if (hidEventFieldDescTable[index].type != self.type) {
            index++;
            continue;
        }
        
        if (!hidEventFieldDescTable[index].selectors) {
            info = hidEventFieldDescTable[index].eventFieldDescTable;
        } else {
            
            NSInteger selectorIndex = 0;
            while (!info && hidEventFieldDescTable[index].selectors[selectorIndex].selectorTables != NULL) {
                
                
                NSInteger selectorValueIndex = 0;
                NSInteger selectorValue =  [self integerValueForField:hidEventFieldDescTable[index].selectors[selectorIndex].value];
                
                while(!info && hidEventFieldDescTable[index].selectors[selectorIndex].selectorTables[selectorValueIndex].eventFieldDescTable
                      != NULL) {
                    
                    if (hidEventFieldDescTable[index].selectors[selectorIndex].selectorTables[selectorValueIndex].value != selectorValue) {
                        selectorValueIndex++;
                        continue;
                    }
                    
                    info = hidEventFieldDescTable[index].selectors[selectorIndex].selectorTables[selectorValueIndex].eventFieldDescTable;
                    selectorValueIndex++;
                }
                
                selectorIndex++;
            }
            
        }
        
        index++;
    }
    
    return info;
}

@end

NS_ASSUME_NONNULL_END


