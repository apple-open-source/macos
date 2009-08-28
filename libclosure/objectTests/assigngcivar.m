
// CONFIG GC
// XXX again, we don't know how to specify GCRR RRGC; maybe we need GC-only

#import <Foundation/Foundation.h>

int GlobalInt = 0;

id objc_assign_global(id val, id *dest) {
    GlobalInt = 1;
    return (id)0;
}

id objc_assign_ivar(id val, id dest, long offset) {
    GlobalInt = 1;
    return (id)0;
}

id objc_assign_strongCast(id val, id *dest) {
    GlobalInt = 1;
    return (id)0;
}

@interface TestObject : NSObject {
@public
    void (^ivarBlock)(void);
    id x;
}
@end

@implementation TestObject
@end


int main(char *argc, char *argv[]) {
   __block int i = 0;
   TestObject *to = [[TestObject alloc] init];
   // assigning a Block into an ivar should elicit a  write-barrier under GC
   to->ivarBlock =  ^ {  ++i; };		// fails to gen write-barrier
   //to->x = to;				// gens write-barrier
   if (GlobalInt == 1) {
        printf("%s: success\n", argv[0]);
        exit(0);
   }
   printf("%s: missing ivar write-barrier for Block\n", argv[0]);
   exit(1);
}
