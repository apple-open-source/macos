/*  block_prop.m
    Created by Chris Parker on 29 Sep 2008
*/

// cmake: gcc -o block_prop block_prop.m -std=gnu99 -framework Foundation -arch x86_64 -fobjc-gc-only

// CONFIG GC rdar://6379842 

#import <Foundation/Foundation.h>
#import <Block.h>

@interface Thing : NSObject {
    void (^someBlock)(void);
}

@property void(^someBlock)(void);

- (void)emit;

@end

@implementation Thing

@synthesize someBlock;

- (void)emit {
    someBlock();
}

- (void)dealloc {
    if (someBlock) Block_release(someBlock);
    [super dealloc];
}

@end

int main (int argc, char const* argv[]) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
    Thing *t = [Thing new];
    
    [t setSomeBlock:^{
        NSLog(@"Things");
    }];
    [t emit];
    
    [t release];
    
    [pool drain];
    
    printf("%s: success\n", argv[0]);
    return 0;
}
