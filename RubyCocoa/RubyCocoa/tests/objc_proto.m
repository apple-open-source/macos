// $Id: $
// test for r2219 reload_protocols()

#import <Foundation/Foundation.h>

// additional protocol. these methods are added into objc-runtime from
@protocol TCAdditonalProtocol
+ (int)tc_clmWithArg1:(id)arg1 arg2:(BOOL)arg2 arg3:(char *)arg3;
- (id)tc_instmWithArg1:(int)arg1 arg2:(double)arg2 arg3:(id)arg3;
@end

@interface DummyProtoImpl <TCAdditonalProtocol>
@end

@implementation DummyProtoImpl
+ (int)tc_clmWithArg1:(id)arg1 arg2:(BOOL)arg2 arg3:(char *)arg3
{
    return 0;
}
- (id)tc_instmWithArg1:(int)arg1 arg2:(double)arg2 arg3:(id)arg3
{
    return nil;
}
@end

void Init_objc_proto(){
  // dummy initializer for ruby's `require'
}

