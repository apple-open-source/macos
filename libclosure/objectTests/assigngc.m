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


//void (^GlobalVoidVoid)(void);


int main(char *argc, char *argv[]) {
   // an object should not be retained within a stack Block
   __block int i = 0;
   void (^blockA)(void) = ^ {  ++i; };
   if (GlobalInt == 0) {
        printf("%s: success\n", argv[0]);
        exit(0);
   }
   printf("%s: write-barrier assignment of stack block\n", argv[0]);
   exit(1);
}
