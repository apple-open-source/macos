//
//  msdos_tests.h
//  msdos_tests
//
//  Created by Tomer Afek on 27/06/2022.
//

#ifndef msdos_tests_h
#define msdos_tests_h

#import <XCTest/XCTest.h>
#import <UVFSPluginTesting/UVFSPluginTests.h>
#import <UVFSPluginTesting/UVFSPluginTestingUtilities.h>
#import <UVFSPluginTesting/UVFSPluginInterfacesFactory.h>
#include "FileOPS_Handler.h"
#include "DirOPS_Handler.h"
#include "FSOPS_Handler.h"
#include "FileRecord_M.h"
#include "Conv.h"


@interface MsdosUnitTests : UVFSPluginUnitTests

-(NSString*)getVolumeSize;
-(void)testFragmentedFileLastCacheEntryIsMissing;
-(void)testFragmentedFileStartOfFileIsMissing;
-(void)testCheckFragmentedDir;
-(void)testHandlingLongNameEntry;

@end


@interface MsdosUnitTestsFAT12 : MsdosUnitTests
@end


@interface MsdosUnitTestsFAT16 : MsdosUnitTests
@end


@interface MsdosUnitTestsFAT32 : MsdosUnitTests
@end


@interface MsdosPerformanceTests : UVFSPluginPerformanceTests

-(NSString*)getVolumeSize;

@end


@interface MsdosPerformanceTestsFAT12 : MsdosPerformanceTests
@end


@interface MsdosPerformanceTestsFAT16 : MsdosPerformanceTests
@end


@interface MsdosPerformanceTestsFAT32 : MsdosPerformanceTests
@end

#endif /* msdos_tests_h */
