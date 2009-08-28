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


void (^GlobalVoidVoid)(void);


int main(char *argc, char *argv[]) {
   __block int i = 0;
   // assigning a Block into a global should elicit a global write-barrier under GC
   GlobalVoidVoid = ^ {  ++i; };
   if (GlobalInt == 1) {
        printf("%s: success\n", argv[0]);
        exit(0);
   }
   printf("%s: missing global write-barrier for Block\n", argv[0]);
   exit(1);
}
