//
//  sort.m
//  testObjects
//
//  Created by Blaine Garst on 1/9/09.
//  Copyright 2009 Apple. All rights reserved.
//


// CONFIG GC RR

#import <Foundation/Foundation.h>

int main(int argc, char *argv[]) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSArray *array = [[NSArray array] sortedArrayUsingComparator:^(id one, id two) { if ([one self]) return (NSComparisonResult)NSOrderedSame; return (NSComparisonResult)NSOrderedAscending; }];
    printf("%s: Success\n", argv[0]);
    return 0;
}
