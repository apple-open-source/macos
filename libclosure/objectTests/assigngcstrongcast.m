#import <Foundation/Foundation.h>
// CONFIG GC
// XXX again, we don't know how to specify GCRR RRGC; maybe we need GC-only

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

typedef struct {
    void (^ivarBlock)(void);
} StructWithBlock_t;


int main(char *argc, char *argv[]) {
   StructWithBlock_t *swbp = (StructWithBlock_t *)malloc(sizeof(StructWithBlock_t*));
   __block int i = 10;
   // assigning a Block into an struct slot should elicit a write-barrier under GC
   swbp->ivarBlock = ^ { ++i; };
   if (GlobalInt == 1) {
        printf("%s: success\n", argv[0]);
        exit(0);
   }
   printf("%s: missing strong cast write-barrier for Block\n", argv[0]);
   exit(1);
}
