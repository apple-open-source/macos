//
//  BatteryHealthUnitTests.m
//  BatteryHealthUnitTests
//
//  Created by Faramola Isiaka on 11/18/20.
//

#import <XCTest/XCTest.h>
#import "BatteryTimeRemaining.h"
#define CELSIUS_TO_KELVIN 273.15
#define KELVIN_TO_DECIKELVIN 10

@interface BatteryHealthUnitTests : XCTestCase

@end

@implementation BatteryHealthUnitTests

extern void calculateNominalCapacity(struct nominalCapacityParams *params);
extern int __rawToNominal(int val, int base);
uint64_t currentTime = 0;
long currentRow = 1;

typedef struct
{
    // Inputs
    uint64_t timeStamp;
    struct nominalCapacityParams inputParams;
    
    // Gold Outputs
    struct capacitySample goldSampleOutputs[vactModesCount];
} BHUITestVector;

- (void)setUp {
    // Put setup code here. This method is called before the invocation of each test method in the class.
    self.continueAfterFailure = true;
    currentRow = 1;
}

- (void)tearDown {
    // Put teardown code here. This method is called after the invocation of each test method in the class.
}

/**
 * @brief sets the current time read by powerd's BHUI algorithm
 * @param newTime the new desired time
 */
- (void)setCurrentTime:(uint64_t) newTime
{
    currentTime = newTime;
}

/**
 * @brief generates a key to index map from the csvArray and stores the mapping in headerToColumn
 * @param csvArray array containing all of the time series test vectors
 * @param headerToColumn pointer to the dictionary that will contain the header to column mapping
 * @return function returns true on success and false on failure.
 */
-(BOOL) generateKeyToIdxMap:(NSMutableArray *) csvArray headerToColumn:(NSMutableDictionary *) headerToColumn
{
    NSString *keysString = csvArray[0];
    NSArray *header = [keysString componentsSeparatedByString:@","];
    for (NSString *key in header) {
        NSUInteger index = [header indexOfObject:key];
        NSString *formatedKey = [key stringByTrimmingCharactersInSet: [NSCharacterSet whitespaceAndNewlineCharacterSet]];
        [headerToColumn setObject:[NSNumber numberWithUnsignedLongLong:index] forKey:formatedKey];
        
    }
    return (true);
}

/**
 * @brief extracts test vectors from a CSV file
 * @param path array containing all of the time series test vectors
 * @param headerToColumn pointer to the dictionary that will contain the header to column mapping
 * @return an NSMutableArray containing all test vectors from the CSV
 */
- (NSMutableArray *) testVectorLoad:(NSString *) path headerToColumn:(NSMutableDictionary *)  headerToColumn
{
    NSError *error = nil;
    NSString *sourceFileString = [NSString stringWithContentsOfFile:path encoding:NSASCIIStringEncoding  error:&error];
    if (error) {
        NSLog(@"Error %@", error);
    }
    NSMutableArray *csvArray = [[NSMutableArray alloc] init];
    csvArray = [[sourceFileString componentsSeparatedByString:@"\n"] mutableCopy];

    [self generateKeyToIdxMap:csvArray headerToColumn:headerToColumn];

    return(csvArray);
}

/**
 * @brief finds the assigned index of a given key from the generateKeyToIdxMap function
 * @param dict the dicitionary as an NSMutableDictionary
 * @param key the key to find index of as an NSString
 * @return the assigned index of the given key from the generateKeyToIdxMap function
 */
-(int) keyToindex:(NSMutableDictionary *) dict key:(NSString *) key
{
    int idx = [(NSNumber *)dict[key] intValue];
    return idx;
}

/**
 * @brief load one test vector entry
 * @param csvArray array containing all of the time series test vectors
 * @param headerToColumn pointer to the dictunary that will contain the header to column mapping
 * @param tv vector output information
 * @return function returns true on success and false on failure.
 */
-(BOOL) Load:(NSMutableArray *) csvArray headerToColumn:(NSMutableDictionary *) headerToColumn tv:(BHUITestVector *) tv
{
    long totalRows = [csvArray count];

    if (currentRow >= totalRows) {
        return (false);
    }

    NSString *curRowString = [csvArray objectAtIndex:currentRow++];
    if (curRowString == NULL) {
        NSLog(@"Failed to get the row");
        return false;
    }
    
    NSArray *row = [curRowString componentsSeparatedByString:@","];
    if (row == NULL || ([row count] < 12)) {
        NSLog(@"Failed to parse the row %@", curRowString);
        return false;
    }

    tv->timeStamp = [row[[self keyToindex:headerToColumn key:@"timestamp"]] longLongValue];
    tv->inputParams.current = [row[[self keyToindex:headerToColumn key:@"InstantAmperage"]] intValue];
    tv->inputParams.temperature = (int)(([row[[self keyToindex:headerToColumn key:@"Temperature"]] floatValue] + CELSIUS_TO_KELVIN) * KELVIN_TO_DECIKELVIN);
    tv->inputParams.designCapacity = [row[[self keyToindex:headerToColumn key:@"DesignCapacity"]] intValue];
    tv->inputParams.cycleCount = [row[[self keyToindex:headerToColumn key:@"CycleCount"]] intValue];
    tv->inputParams.fcc = [row[[self keyToindex:headerToColumn key:@"AppleRawMaxCapacity"]] intValue];
    for(int vactMode=0; vactMode < vactModesCount; vactMode++)
    {
        // only need to load the FCC for inputs
        tv->inputParams.sample[vactMode].fcc = tv->inputParams.fcc;
        
        tv->goldSampleOutputs[vactMode].fcc = tv->inputParams.fcc;
        if(vactMode == vactModeDisabled)
        {
            tv->goldSampleOutputs[vactMode].ncc = [row[[self keyToindex:headerToColumn key:@"DisabledNCC"]] intValue];
        }
        else
        {
            tv->goldSampleOutputs[vactMode].ncc = [row[[self keyToindex:headerToColumn key:@"EnabledNCC"]] intValue];
        }
    }
    return (true);
}

/**
 * @brief utility function that runs all unit tests
 * @param inputFileName string specifying the name of the input file containing the test vectors
 * @param outputFileName string specifying what to name the output file
 * @param pathToOutputFile string specifying where to write the output file
 * @param initialfccDaySampleCount the initial value to set initialfccDaySampleCount to
 * @param initialfccAvgHistoryCount the initial value to set initialfccAvgHistoryCount to
 * @param bypassAssertions boolean that bypasses XCTAsserts
 */
- (void)runBatteryHealthUnitTest:(const char *) inputFileName outputFileName:(const char *) outputFileName pathToOutputFile:(const char *) pathToOutputFile initialfccDaySampleCount:(unsigned int) initialfccDaySampleCount initialfccAvgHistoryCount:(unsigned int) initialfccAvgHistoryCount bypassAssertions:(bool) bypassAssertions
{
    BHUITestVector tv;
    bool r;
    FILE *pOutFile;
    
    NSBundle *bundle = [NSBundle bundleForClass:[self class]];
    NSString *testVectorFileStr = [bundle pathForResource:@(inputFileName) ofType:@"csv" inDirectory:@""];
    
    NSMutableDictionary *headerToColumn = [[NSMutableDictionary alloc] init];
    NSMutableArray * testVector = [self testVectorLoad:testVectorFileStr headerToColumn:headerToColumn];
    
    if (!pathToOutputFile) {
        pathToOutputFile = "~";
    }
    if (!outputFileName) {
        outputFileName = "OutputFile";
    }
    
    NSString* outputFilePath = [NSString stringWithFormat:@"%s/%s.csv", pathToOutputFile, outputFileName];
    pOutFile = fopen([[outputFilePath stringByExpandingTildeInPath] UTF8String],"w");
    
    // add the output file header
    NSString* outputFileHeader = @"Timestamp,InstantAmperage,Temperature,DesignCapacity,AppleRawMaxCapacity,CycleCount,fccDaySampleCount,fccAvgHistoryCount,DisabledModeFCC,DisabledModeFCCDaySampleAvg,DisabledModeNCC,DisabledModeMonoNCC,EnabledModeFCC,EnabledModeFCCDaySampleAvg,EnabledModeNCC,EnabledModeMonoNCCP";
    outputFileHeader = [outputFileHeader stringByAppendingString:@"\n"];
    fputs([outputFileHeader UTF8String], pOutFile);
    
    tv.inputParams.fccDaySampleCount = initialfccDaySampleCount;
    tv.inputParams.fccAvgHistoryCount = initialfccAvgHistoryCount;
    //test vectors here
    while ((r = [self Load:testVector headerToColumn:headerToColumn tv:&tv]) == true) {
        calculateNominalCapacity(&tv.inputParams);
        
        NSString* outputTestVector = @"";
        outputTestVector = [outputTestVector stringByAppendingString:[NSString stringWithFormat:@"%llu,%i,%i,%i,%i,%i,%i,%i", tv.timeStamp, tv.inputParams.current, tv.inputParams.temperature, tv.inputParams.designCapacity, tv.inputParams.fcc,  tv.inputParams.cycleCount, tv.inputParams.fccDaySampleCount, tv.inputParams.fccAvgHistoryCount]];
        [self setCurrentTime:tv.timeStamp];
        for(int vactMode=0; vactMode < vactModesCount; vactMode++) {
            tv.inputParams.sample[vactMode].nccpMonotonic = __rawToNominal(tv.inputParams.sample[vactMode].ncc, tv.inputParams.designCapacity);
            
            if(!bypassAssertions) {
                XCTAssertEqual(tv.inputParams.sample[vactMode].ncc, tv.goldSampleOutputs[vactMode].ncc, "Row:%ld VactMode:%d computedNCC:%d goldNCC:%d", currentRow - 1, vactMode, tv.inputParams.sample[vactMode].ncc, tv.goldSampleOutputs[vactMode].ncc);
            }
            
            outputTestVector = [outputTestVector stringByAppendingString:[NSString stringWithFormat:@",%i,%i,%i,%i", tv.inputParams.sample[vactMode].fcc, tv.inputParams.sample[vactMode].fccDaySampleAvg, tv.inputParams.sample[vactMode].ncc, tv.inputParams.sample[vactMode].nccpMonotonic]];
        }
        outputTestVector = [outputTestVector stringByAppendingString:@"\n"];
        fputs([outputTestVector UTF8String], pOutFile);
    }
    fclose(pOutFile);
}

- (void)testPowerlogAnalysisExample {
    // This is an example of how to set up a non-standard subtest which we use for analysis/experimentation
    // of various iterations of the BHUI algorithms
    [self runBatteryHealthUnitTest:"applesiliconvalues" outputFileName:"ExampleOutput" pathToOutputFile:NULL initialfccDaySampleCount:0 initialfccAvgHistoryCount:0 bypassAssertions:true];
}

- (void)testSeedingAboveCycleCountThreshold {
    [self runBatteryHealthUnitTest:"SeedingAboveCycleCountThreshold" outputFileName:NULL pathToOutputFile:NULL initialfccDaySampleCount:0 initialfccAvgHistoryCount:0 bypassAssertions:false];
}

- (void)testSeedingBelowCycleCountThreshold {
    [self runBatteryHealthUnitTest:"SeedingBelowCycleCountThreshold" outputFileName:NULL pathToOutputFile:NULL initialfccDaySampleCount:0 initialfccAvgHistoryCount:0 bypassAssertions:false];
}

@end
