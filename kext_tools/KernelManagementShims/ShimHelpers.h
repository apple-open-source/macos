//
//  ShimHelpers.h
//  kext_tools
//
//  Created by jkb on 3/11/20.
//

#ifndef ShimHelpers_h
#define ShimHelpers_h

void initArgShimming();
void addArguments(NSArray<NSString *> *arguments);
void addArgument(NSString *argument);
void runWithShimmedArguments();
NSString *createStringFromShimmedArguments();

#endif /* ShimHelpers_h */
