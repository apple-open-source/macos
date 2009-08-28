// ..\clang -rewrite-objc -fms-extensions simpleblock.c

// #include <iostream>
// using namespace std;

// #include "Block.h"

int main(int argc, char **argv) {
    void(^aBlock)(int x);
    void(^bBlock)(int x);

    aBlock = ^(int x) {
	// cout << "Hello, " << x << endl;
    };

    aBlock(42);

    bBlock = (void *)Block_copy(aBlock);

    bBlock(46);

    Block_release(bBlock);

    return 0;
}
