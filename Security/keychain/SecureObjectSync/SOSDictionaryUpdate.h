//
//  SOSDictionaryUpdate.h
//

#ifndef SOSDictionaryUpdate_h
#define SOSDictionaryUpdate_h

#include <CoreFoundation/CFBase.h>

@class SOSMessage;

@interface SOSDictionaryUpdate : NSObject
{
    uint8_t *currentHashBuf;
}

-(id) init;
- (bool) hasChanged: (CFDictionaryRef) d;
- (void) reset;

@end

#endif /* SOSDictionaryUpdate_h */
