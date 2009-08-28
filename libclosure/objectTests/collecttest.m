#import <Foundation/Foundation.h>
#import <Block.h>


// CONFIG GC

int countem(NSHashTable *table) {
    int result = 0;
    for (id elem in table)
        ++result;
    return result;
}

int main(char *argc, char *argv[]) {
    NSHashTable *weakSet = [NSHashTable hashTableWithWeakObjects];
    void (^local)(void) = ^{ [weakSet count]; };
    extern id _Block_copy_collectable(void *);
    //[weakSet addObject:_Block_copy_collectable(local)];
    [weakSet addObject:Block_copy(local)];
    [weakSet addObject:Block_copy(local)];
    [weakSet addObject:Block_copy(local)];
    [weakSet addObject:Block_copy(local)];
    [weakSet addObject:Block_copy(local)];
    [weakSet addObject:Block_copy(local)];
    //printf("gc block... we hope\n%s\n", _Block_dump(Block_copy(local)));
    [[NSGarbageCollector defaultCollector] collectExhaustively];
    int count = countem(weakSet);
    if (count == 6) {
        printf("%s: success\n", argv[0]);
        exit(0);
    }
    else {
        printf("%s: didn't recovered %d of %d items\n", argv[0], count, 6);
    }
    exit(1);
}
