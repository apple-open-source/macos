//
//  msdos_tests.m
//  msdos_tests
//
//  Created by Kujan Lauz on 01/02/2022.
//

#import "msdos_tests.h"
#import "msdos_tests_utilities.h"

@implementation MsdosUnitTests

-(void)setUp
{
    [super setDelegate:[[MsdosSetupDelegate alloc] initWithSize:[self getVolumeSize]]];
    [super setUp];
}

-(NSString*)getVolumeSize
{
    @throw [NSException exceptionWithName:NSInternalInconsistencyException
                                   reason:[NSString stringWithFormat:@"You must override %s in a subclass", __FUNCTION__]
                                 userInfo:nil];
}

/**
 * NOTE: This test was made to reach a corner-case on the cluster chain cache, in order to increase code-coverage.
 * This test makes sure we handle well the following scenario:
 * A fragmented file, which only its last entry in cluster chain cache is missing on cache, while other entries exists.
 * Most of the test is a preparation for this scenario.
 * To reach the corner-case, we read from the file at an offset which is located in this missing entry.
 * After this read, we write/read to/from the files in the directory, to make sure everything is OK.
 */
-(void)testFragmentedFileLastCacheEntryIsMissing
{
    UVFSFileNode fileNode = NULL;
    UVFSFileNode fileNode1 = NULL;
    UVFSFileNode fileNode2 = NULL;
    const char *fileName1 = "file_1";
    const char *fileName2 = "file_2";
    const char *fileToForceEvictionName = "file_to_force_eviction";
    const char *fileToBeEvictedName = "file_to_be_evicted";
    NSString *fileName;

    FileSystemRecord_s *psFSRecord = GET_FSRECORD(GET_RECORD(self.fs.rootNode));

    size_t clusterSize = getClusterSize(self.factory);

    void* pvReadBuffer = malloc(clusterSize);
    XCTAssertNotEqual(pvReadBuffer, NULL);
    void* pvZeroBuffer = calloc(clusterSize, 1);
    XCTAssertNotEqual(pvZeroBuffer, NULL);
    size_t uActuallyRead, uActuallyWritten;

    USERFS_TEST_LOG("create files under root dir so they will be evicted from cluster chain cache, instead of %s.", fileName1);
    /*
     * On every eviction of the cluster chain cache we evict the LOWER_BOUND_ENTRIES_TO_EVICT oldest entries.
     * So we need exactly LOWER_BOUND_ENTRIES_TO_EVICT - 2 files to have them evicted instead of the first entry of file1.
     */
    int numFilesToBeEvicted = LOWER_BOUND_ENTRIES_TO_EVICT - 2;
    for(int i=0; i<numFilesToBeEvicted; i++) {
        fileName = [NSString stringWithFormat:@"%s_%d", fileToBeEvictedName, i];
        USERFS_TEST_LOG("Create file %@", fileName);
        PRINT_IF_FAILED([self.fs CreateNewFile:self.fs.rootNode newNode:&fileNode name:fileName.UTF8String size:clusterSize]);
    }

    USERFS_TEST_LOG("Create %s", fileName1);
    PRINT_IF_FAILED([self.fs CreateNewFile:self.fs.rootNode newNode:&fileNode1 name:fileName1 size:0]);
    USERFS_TEST_LOG("Create %s", fileName2);
    PRINT_IF_FAILED([self.fs CreateNewFile:self.fs.rootNode newNode:&fileNode2 name:fileName2 size:0]);

    int numOfElementsInCacheEntry = MAX_CHAIN_CACHE_ELEMENTS_PER_ENTRY;
    USERFS_TEST_LOG("Fill the 1st cache entry of %s, by creating %d clusters non-contiguously (that's why we need %s). ", fileName1, numOfElementsInCacheEntry, fileName2);
    for(int i=1; i<=numOfElementsInCacheEntry; i++) {
        PRINT_IF_FAILED([self.fs SetAttrChangeSize:fileNode1 newSize:i * clusterSize]);
        PRINT_IF_FAILED([self.fs SetAttrChangeSize:fileNode2 newSize:i * clusterSize]);
    }

    int numOfClustersFile1 = numOfElementsInCacheEntry + 1;
    USERFS_TEST_LOG("Enlarge %s to size = %d clusters, so it will have a 2nd cache entry in the cluster chain cache", fileName1, numOfClustersFile1);
    PRINT_IF_FAILED([self.fs SetAttrChangeSize:fileNode1 newSize:numOfClustersFile1*clusterSize]);
    
    USERFS_TEST_LOG("Read from %s at offset 0, so its first entry will have a higher LRU value in the cluster chain cache", fileName1);
    PRINT_IF_FAILED([self.fs ReadToBuffer:fileNode1 offset:0 actuallyRead:&uActuallyRead buffer:pvReadBuffer bufferSize:clusterSize]);
    XCTAssertEqual(uActuallyRead, clusterSize);
    XCTAssertEqual(0, memcmp(pvReadBuffer, pvZeroBuffer, clusterSize));

    USERFS_TEST_LOG("Create files under root dir so they will evict the 2nd entry of %s from cache.", fileName1);
    /*
     * Cluster chain cache size is MAX_CHAIN_CACHE_ENTRIES.
     * We need to have MAX_CHAIN_CACHE_ENTRIES + 1 entries to evict the old cache entries.
     */
    int numFilesToForceEviction = MAX_CHAIN_CACHE_ENTRIES - psFSRecord->psClusterChainCache->uAmountOfEntries + 1;
    for(int i=0; i<numFilesToForceEviction; i++) {
        fileName = [NSString stringWithFormat:@"%s_%d", fileToForceEvictionName, i];
        USERFS_TEST_LOG("Create file %@", fileName);
        PRINT_IF_FAILED([self.fs CreateNewFile:self.fs.rootNode newNode:&fileNode name:fileName.UTF8String size:clusterSize]);
    }
    
    // This is the purpose of the test! all previous operation are a preparation for this read!
    USERFS_TEST_LOG("Read from %s at offset = %lu, while the cache entry which contained this offset was evicted.", fileName1, numOfElementsInCacheEntry*clusterSize+1);
    PRINT_IF_FAILED([self.fs ReadToBuffer:fileNode1 offset:numOfElementsInCacheEntry*clusterSize+1 actuallyRead:&uActuallyRead buffer:pvReadBuffer bufferSize:clusterSize-1]);
    XCTAssertEqual(uActuallyRead, clusterSize - 1);
    XCTAssertEqual(0, memcmp(pvReadBuffer, pvZeroBuffer, clusterSize-1));

    pvReadBuffer = realloc(pvReadBuffer, clusterSize * numOfClustersFile1); // enlarge read buffer
    XCTAssertNotEqual(pvReadBuffer, NULL);
    void* pvWriteBuffer = malloc(clusterSize * numOfClustersFile1);
    XCTAssertNotEqual(pvWriteBuffer, NULL);
    NSString *strToWriteToFile = [@"" stringByPaddingToLength:clusterSize withString:@"abcdefg123456789" startingAtIndex:0];

    USERFS_TEST_LOG("Write and read to/from %s to make sure cache handling was OK.", fileName1);
    memcpy(pvWriteBuffer, strToWriteToFile.UTF8String, numOfClustersFile1 * clusterSize);
    PRINT_IF_FAILED([self.fs WriteFromBuffer:fileNode1 offset:0 actuallyWritten:&uActuallyWritten buffer:pvWriteBuffer bufferSize:numOfClustersFile1 * clusterSize]);
    XCTAssertEqual(uActuallyWritten, clusterSize * numOfClustersFile1);
    PRINT_IF_FAILED([self.fs ReadToBuffer:fileNode1 offset:0 actuallyRead:&uActuallyRead buffer:pvReadBuffer bufferSize:numOfClustersFile1 * clusterSize]);
    XCTAssertEqual(uActuallyRead, numOfClustersFile1 * clusterSize);
    XCTAssertEqual(0, memcmp(pvReadBuffer, pvWriteBuffer, numOfClustersFile1 * clusterSize));

    int numOfClustersFile2 = numOfElementsInCacheEntry;
    USERFS_TEST_LOG("Write and read to/from %s to make sure cache handling was OK.", fileName2);
    memcpy(pvWriteBuffer, strToWriteToFile.UTF8String, numOfClustersFile2 * clusterSize);
    PRINT_IF_FAILED([self.fs WriteFromBuffer:fileNode2 offset:0 actuallyWritten:&uActuallyWritten buffer:pvWriteBuffer bufferSize:numOfClustersFile2 * clusterSize]);
    XCTAssertEqual(uActuallyWritten, clusterSize * numOfClustersFile2);
    PRINT_IF_FAILED([self.fs ReadToBuffer:fileNode2 offset:0 actuallyRead:&uActuallyRead buffer:pvReadBuffer bufferSize:numOfClustersFile2 * clusterSize]);
    XCTAssertEqual(uActuallyRead, numOfClustersFile2 * clusterSize);
    XCTAssertEqual(0, memcmp(pvReadBuffer, pvWriteBuffer, numOfClustersFile2 * clusterSize));
    
    USERFS_TEST_LOG("Cleanup");
    
    for(int i=0; i<numFilesToBeEvicted; i++) {
        fileName = [NSString stringWithFormat:@"%s_%d", fileToBeEvictedName, i];
        PRINT_IF_FAILED([self.fs Lookup:self.fs.rootNode newNode:&fileNode name:fileName.UTF8String]);
        PRINT_IF_FAILED([self.fs RemoveFile:self.fs.rootNode name:fileName.UTF8String victim:fileNode]);
        PRINT_IF_FAILED([self.fs CloseFile:fileNode flags:0]);
    }
    
    for(int i=0; i<numFilesToForceEviction; i++) {
        fileName = [NSString stringWithFormat:@"%s_%d", fileToForceEvictionName, i];
        PRINT_IF_FAILED([self.fs Lookup:self.fs.rootNode newNode:&fileNode name:fileName.UTF8String]);
        PRINT_IF_FAILED([self.fs RemoveFile:self.fs.rootNode name:fileName.UTF8String victim:fileNode]);
        PRINT_IF_FAILED([self.fs CloseFile:fileNode flags:0]);
    }
    
    PRINT_IF_FAILED([self.fs RemoveFile:self.fs.rootNode name:fileName1 victim:fileNode1]);
    PRINT_IF_FAILED([self.fs CloseFile:fileNode1 flags:0]);
    PRINT_IF_FAILED([self.fs RemoveFile:self.fs.rootNode name:fileName2 victim:fileNode2]);
    PRINT_IF_FAILED([self.fs CloseFile:fileNode2 flags:0]);
    
    free(pvReadBuffer);
    free(pvZeroBuffer);
    free(pvWriteBuffer);

}

/**
 * NOTE: This test was made to reach a corner-case on the cluster chain cache, in order to increase code-coverage.
 * This test makes sure we handle well the following scenario:
 * A fragmented file, which some of its clusters are in the cluster chain cache, but not its first cluster.
 * Most of the test is a preparation for this scenario.
 * To reach the corner-case, we read from the file at offset 0, while we know that the first cluster is not in cache,
 * and that some other cluster are in the cache.
 * After this read, we write/read to/from the files in the directory, to make sure everything is OK.
 */
-(void)testFragmentedFileStartOfFileIsMissing
{
    UVFSFileNode fileNode = NULL;
    UVFSFileNode fileNode1 = NULL;
    UVFSFileNode fileNode2 = NULL;
    const char *fileName1 = "file_1";
    const char *fileName2 = "file_2";
    const char *fileToForceEvictionName = "file_to_force_eviction";
    NSString *fileName;

    uint32_t seed = arc4random();
    srandom(seed); // getRandNumInRange calls random() which will use the seed we init here.
    USERFS_TEST_LOG("[%s]: seed = %u", __FUNCTION__, seed);

    // we want to have more than 2 cluster, but less than MAX_CHAIN_CACHE_ELEMENTS_PER_ENTRY
    int numOfClustersInFile = getRandNumInRange(3, MAX_CHAIN_CACHE_ELEMENTS_PER_ENTRY);
    USERFS_TEST_LOG("numOfClustersInFile = %d", numOfClustersInFile);

    size_t clusterSize = getClusterSize(self.factory);

    void* pvReadBuffer = malloc(clusterSize * numOfClustersInFile);
    XCTAssertNotEqual(pvReadBuffer, NULL);
    void* pvZeroBuffer = malloc(clusterSize * numOfClustersInFile);
    XCTAssertNotEqual(pvZeroBuffer, NULL);
    memset(pvZeroBuffer, 0, clusterSize * numOfClustersInFile);
    void* pvWriteBuffer = malloc(clusterSize * numOfClustersInFile);
    XCTAssertNotEqual(pvWriteBuffer, NULL);
    
    NSString *strToWriteToFile = [@"" stringByPaddingToLength:clusterSize * numOfClustersInFile withString:@"abcdefg123456789" startingAtIndex:0];
    size_t uActuallyRead, uActuallyWritten;

    USERFS_TEST_LOG("Create %s", fileName1);
    PRINT_IF_FAILED([self.fs CreateNewFile:self.fs.rootNode newNode:&fileNode1 name:fileName1 size:0]);
    USERFS_TEST_LOG("Create %s", fileName2);
    PRINT_IF_FAILED([self.fs CreateNewFile:self.fs.rootNode newNode:&fileNode2 name:fileName2 size:0]);

    USERFS_TEST_LOG("Allocate %d clusters non-contiguously (that's why we need %s). ", numOfClustersInFile, fileName2);
    for(int i=1; i<=numOfClustersInFile; i++) {
        PRINT_IF_FAILED([self.fs SetAttrChangeSize:fileNode1 newSize:i * clusterSize]);
        PRINT_IF_FAILED([self.fs SetAttrChangeSize:fileNode2 newSize:i * clusterSize]);
    }
    
    USERFS_TEST_LOG("Create files under root dir so they will evict the entry of %s from cache.", fileName1);
    int numFilesToForceEviction = MAX_CHAIN_CACHE_ENTRIES;
    for(int i=0; i<numFilesToForceEviction; i++) {
        fileName = [NSString stringWithFormat:@"%s_%d", fileToForceEvictionName, i];
        USERFS_TEST_LOG("Create file %@", fileName);
        PRINT_IF_FAILED([self.fs CreateNewFile:self.fs.rootNode newNode:&fileNode name:fileName.UTF8String size:clusterSize]);
    }
    
    // we want to read from a random cluster, but not from the first one.
    int clusterToRead = getRandNumInRange(2, numOfClustersInFile);
    USERFS_TEST_LOG("clusterToRead = %d", clusterToRead);

    uint64_t offsetToReadFrom = (clusterToRead-1)*clusterSize+1;
    USERFS_TEST_LOG("Read from %s at offset = %llu, while the cache entry which contained this offset was evicted.", fileName1, offsetToReadFrom);
    PRINT_IF_FAILED([self.fs ReadToBuffer:fileNode1 offset:offsetToReadFrom actuallyRead:&uActuallyRead buffer:pvReadBuffer bufferSize:clusterSize-1]);
    XCTAssertEqual(uActuallyRead, clusterSize - 1);
    XCTAssertEqual(0, memcmp(pvReadBuffer, pvZeroBuffer, clusterSize-1));
    
    // This is the purpose of the test! all previous operation are a preparation for this read!
    USERFS_TEST_LOG("Read from %s at offset 0", fileName1);
    PRINT_IF_FAILED([self.fs ReadToBuffer:fileNode1 offset:0 actuallyRead:&uActuallyRead buffer:pvReadBuffer bufferSize:clusterSize * numOfClustersInFile]);
    XCTAssertEqual(uActuallyRead, clusterSize * numOfClustersInFile);
    XCTAssertEqual(0, memcmp(pvReadBuffer, pvZeroBuffer, clusterSize * numOfClustersInFile));

    USERFS_TEST_LOG("Write and read to/from %s to make sure cache handling was OK.", fileName1);
    memcpy(pvWriteBuffer, strToWriteToFile.UTF8String, numOfClustersInFile * clusterSize);
    PRINT_IF_FAILED([self.fs WriteFromBuffer:fileNode1 offset:0 actuallyWritten:&uActuallyWritten buffer:pvWriteBuffer bufferSize:numOfClustersInFile * clusterSize]);
    XCTAssertEqual(uActuallyWritten, clusterSize * numOfClustersInFile);
    PRINT_IF_FAILED([self.fs ReadToBuffer:fileNode1 offset:0 actuallyRead:&uActuallyRead buffer:pvReadBuffer bufferSize:numOfClustersInFile * clusterSize]);
    XCTAssertEqual(uActuallyRead, numOfClustersInFile * clusterSize);
    XCTAssertEqual(0, memcmp(pvReadBuffer, pvWriteBuffer, numOfClustersInFile * clusterSize));

    USERFS_TEST_LOG("Write and read to/from %s to make sure cache handling was OK.", fileName2);
    memcpy(pvWriteBuffer, strToWriteToFile.UTF8String, numOfClustersInFile * clusterSize);
    PRINT_IF_FAILED([self.fs WriteFromBuffer:fileNode2 offset:0 actuallyWritten:&uActuallyWritten buffer:pvWriteBuffer bufferSize:numOfClustersInFile * clusterSize]);
    XCTAssertEqual(uActuallyWritten, clusterSize * numOfClustersInFile);
    PRINT_IF_FAILED([self.fs ReadToBuffer:fileNode2 offset:0 actuallyRead:&uActuallyRead buffer:pvReadBuffer bufferSize:numOfClustersInFile * clusterSize]);
    XCTAssertEqual(uActuallyRead, numOfClustersInFile * clusterSize);
    XCTAssertEqual(0, memcmp(pvReadBuffer, pvWriteBuffer, numOfClustersInFile * clusterSize));

    USERFS_TEST_LOG("Cleanup");
    
    for(int i=0; i<numFilesToForceEviction; i++) {
        fileName = [NSString stringWithFormat:@"%s_%d", fileToForceEvictionName, i];
        PRINT_IF_FAILED([self.fs Lookup:self.fs.rootNode newNode:&fileNode name:fileName.UTF8String]);
        PRINT_IF_FAILED([self.fs RemoveFile:self.fs.rootNode name:fileName.UTF8String victim:fileNode]);
        PRINT_IF_FAILED([self.fs CloseFile:fileNode flags:0]);
    }
    
    PRINT_IF_FAILED([self.fs RemoveFile:self.fs.rootNode name:fileName1 victim:fileNode1]);
    PRINT_IF_FAILED([self.fs CloseFile:fileNode1 flags:0]);
    PRINT_IF_FAILED([self.fs RemoveFile:self.fs.rootNode name:fileName2 victim:fileNode2]);
    PRINT_IF_FAILED([self.fs CloseFile:fileNode2 flags:0]);
    
    free(pvReadBuffer);
    free(pvZeroBuffer);
    free(pvWriteBuffer);

}

/**
 * NOTE: This test was made to reach a corner-case on the cluster chain cache, in order to increase code-coverage.
 * This test makes sure that we handle well a directory with fragmented dir entry clusters.
 * Firstly, it creates enough files to have 3 dir clusters, while the 1st & 2nd clusters are not consecutive.
 * Then, it does some delicate operations to have the 2nd cluster of the dir evicted from the cluster chain cache, while the 1st and 3rd clusters are still in cache.
 * Now, it lookups a file which is in the 2nd cluster of the directory - and expects to find it.
 * At last, it writes and reads from all files in dir, to see that everything is OK.
 */
-(void)testCheckFragmentedDir
{
    UVFSFileNode dirNode = NULL;
    UVFSFileNode fileNode = NULL;
    UVFSFileNode fileNode1 = NULL;
    UVFSFileNode fileNode2 = NULL;
    UVFSFileNode fileNode3 = NULL;
    UVFSFileNode fileUnderRootNode = NULL;
    NSString *fileName;
    const char *dirName = "test_dir";
    const char *fileUnderRootName = "file_under_root";
    const char *fileToForceEvictionName = "file_to_force_eviction";
    const char *fileToBeEvictedName = "file_to_be_evicted";
    // We need long file names to cause the dir entry to be large, so we won't have to create
    // many files to force another cluster creation for the dir.
    int longFileNameLen = NAME_MAX - 10; // need to keep enough space for the "_<fileNum>" postfix
    NSString *longFileName = [@"a" stringByPaddingToLength:longFileNameLen withString:@"a" startingAtIndex:0];

    FileSystemRecord_s *psFSRecord = GET_FSRECORD(GET_RECORD(self.fs.rootNode));
    
    size_t clusterSize = getClusterSize(self.factory);

    PRINT_IF_FAILED([self.fs CreateNewFolder:self.fs.rootNode dirNode:&dirNode name:dirName]);
    
    USERFS_TEST_LOG("Create a file under root dir to make the dir's clusters not contiguous.");
    PRINT_IF_FAILED([self.fs CreateNewFile:self.fs.rootNode newNode:&fileUnderRootNode name:fileUnderRootName size:clusterSize]);

    USERFS_TEST_LOG("Creating file_1, its metadata will be located in the 1st cluster of the dir.");
    PRINT_IF_FAILED([self.fs CreateNewFile:dirNode newNode:&fileNode1 name:"file_1" size:0]);

    // get # of availableClusters
    uint64_t initialAvailableClusters;
    PRINT_IF_FAILED([self.fs GetFSAttr:self.fs.rootNode number:&initialAvailableClusters forAttr:@UVFS_FSATTR_BLOCKSAVAIL]);

    USERFS_TEST_LOG("Create many files in order to force a creation of another cluster for the directory.");
    uint64_t curAvailableClusters = initialAvailableClusters;
    int fileNum = 0;
    while(curAvailableClusters == initialAvailableClusters) { // stop creating files once we notice that a cluster was allocated
        fileName = [NSString stringWithFormat:@"%@_%d", longFileName, fileNum];
        USERFS_TEST_LOG("Create file %@", fileName);
        PRINT_IF_FAILED([self.fs CreateNewFile:dirNode newNode:&fileNode name:fileName.UTF8String size:0]);
        PRINT_IF_FAILED([self.fs GetFSAttr:self.fs.rootNode number:&curAvailableClusters forAttr:@UVFS_FSATTR_BLOCKSAVAIL]);
        fileNum++;
    }
    
    USERFS_TEST_LOG("Creating file_2, its metadata will be located in the 2nd cluster of the dir.");
    PRINT_IF_FAILED([self.fs CreateNewFile:dirNode newNode:&fileNode2 name:"file_2" size:0]);

    // update # of availableClusters
    PRINT_IF_FAILED([self.fs GetFSAttr:self.fs.rootNode number:&initialAvailableClusters forAttr:@UVFS_FSATTR_BLOCKSAVAIL]);
    
    USERFS_TEST_LOG("Create many files in order to force a creation of another cluster for the directory.");
    curAvailableClusters = initialAvailableClusters;
    while(curAvailableClusters == initialAvailableClusters) { // stop creating files once we notice that a cluster was allocated
        fileName = [NSString stringWithFormat:@"%@_%d", longFileName, fileNum];
        USERFS_TEST_LOG("Create file %@", fileName);
        PRINT_IF_FAILED([self.fs CreateNewFile:dirNode newNode:&fileNode name:fileName.UTF8String size:0]);
        PRINT_IF_FAILED([self.fs GetFSAttr:self.fs.rootNode number:&curAvailableClusters forAttr:@UVFS_FSATTR_BLOCKSAVAIL]);
        fileNum++;
    }
    
    USERFS_TEST_LOG("Creating file_3, its metadata will be located in the 3rd cluster of the dir.");
    PRINT_IF_FAILED([self.fs CreateNewFile:dirNode newNode:&fileNode3 name:"file_3" size:0]);

    USERFS_TEST_LOG("create files under root dir so they will be evicted from cluster chain cache, instead of (some of) the dir entry clusters.");
    /*
     * On every eviction of the cluster chain cache we evict the LOWER_BOUND_ENTRIES_TO_EVICT oldest entries.
     * So we need exactly LOWER_BOUND_ENTRIES_TO_EVICT - 2 files to have them evicted instead of two of the dir clusters.
     * (We want only one dir cluster to be evicted from cache).
     */
    int numFilesToBeEvicted = LOWER_BOUND_ENTRIES_TO_EVICT - 2;
    for(int i=0; i<numFilesToBeEvicted; i++) {
        fileName = [NSString stringWithFormat:@"%s_%d", fileToBeEvictedName, i];
        USERFS_TEST_LOG("Create file %@", fileName);
        PRINT_IF_FAILED([self.fs CreateNewFile:self.fs.rootNode newNode:&fileNode name:fileName.UTF8String size:clusterSize]);
    }
    
    USERFS_TEST_LOG("lookup 3rd file - so the 3rd cluster of the dir will be newer in cache than the 2nd cluster.");
    PRINT_IF_FAILED([self.fs Lookup:dirNode newNode:&fileNode3 name:"file_3"]);
    
    USERFS_TEST_LOG("lookup 1st file - so the 1st cluster of the dir will be newer in cache than the 2nd cluster.");
    PRINT_IF_FAILED([self.fs Lookup:dirNode newNode:&fileNode1 name:"file_1"]);

    USERFS_TEST_LOG("create many files under root dir to evict the 2nd cluster of the directory from cache (but not the 1st and 3rd clusters, as they are \"newer\" in cache).");
    /*
     * Cluster chain cache size is MAX_CHAIN_CACHE_ENTRIES.
     * We need to have MAX_CHAIN_CACHE_ENTRIES + 1 entries to evict the old cache entries.
     */
    int numFilesToForceEviction = MAX_CHAIN_CACHE_ENTRIES - psFSRecord->psClusterChainCache->uAmountOfEntries + 1;
    for(int i=0; i<numFilesToForceEviction; i++) {
        fileName = [NSString stringWithFormat:@"%s_%d", fileToForceEvictionName, i];
        USERFS_TEST_LOG("Create file %@", fileName);
        PRINT_IF_FAILED([self.fs CreateNewFile:self.fs.rootNode newNode:&fileNode name:fileName.UTF8String size:clusterSize]);
    }
    
    // This lookup is the purpose of the test! all previous operations are just the preperation of this corner-case...
    USERFS_TEST_LOG("lookup 2nd file after we created a hole in cluster chain cache (1st and 3rd clusters are in the cache but 2nd isn't at this point).");
    PRINT_IF_FAILED([self.fs Lookup:dirNode newNode:&fileNode2 name:"file_2"]);
    
    USERFS_TEST_LOG("lookup, write and read to/from all files in %s, to make sure the handling of the fragmented dir clusters is OK.", dirName);
    void* pvReadBuffer = malloc(clusterSize);
    XCTAssertNotEqual(pvReadBuffer, NULL);
    void* pvWriteBuffer = malloc(clusterSize);
    XCTAssertNotEqual(pvWriteBuffer, NULL);
    memset(pvWriteBuffer, 0, clusterSize);
    size_t uActuallyRead, uActuallyWritten;

     NSString *strToWriteToFile = [@"" stringByPaddingToLength:clusterSize withString:@"abcdefg123456789" startingAtIndex:0];

    for(int i=0; i<fileNum; i++) {
        fileName = [NSString stringWithFormat:@"%@_%d", longFileName, i];
        PRINT_IF_FAILED([self.fs Lookup:dirNode newNode:&fileNode name:fileName.UTF8String]);
        memcpy(pvWriteBuffer, strToWriteToFile.UTF8String, clusterSize);
        PRINT_IF_FAILED([self.fs WriteFromBuffer:fileNode offset:0 actuallyWritten:&uActuallyWritten buffer:pvWriteBuffer bufferSize:clusterSize]);
        XCTAssertEqual(uActuallyWritten, clusterSize);
        PRINT_IF_FAILED([self.fs ReadToBuffer:fileNode offset:0 actuallyRead:&uActuallyRead buffer:pvReadBuffer bufferSize:clusterSize]);
        XCTAssertEqual(uActuallyRead, clusterSize);
        XCTAssertEqual(0, memcmp(pvReadBuffer, pvWriteBuffer, clusterSize));
    }
    
    for(int i=1; i<=3; i++) {
        fileName = [NSString stringWithFormat:@"file_%d", i];
        PRINT_IF_FAILED([self.fs Lookup:dirNode newNode:&fileNode name:fileName.UTF8String]);
        memcpy(pvWriteBuffer, strToWriteToFile.UTF8String, clusterSize);
        PRINT_IF_FAILED([self.fs WriteFromBuffer:fileNode offset:0 actuallyWritten:&uActuallyWritten buffer:pvWriteBuffer bufferSize:clusterSize]);
        XCTAssertEqual(uActuallyWritten, clusterSize);
        PRINT_IF_FAILED([self.fs ReadToBuffer:fileNode offset:0 actuallyRead:&uActuallyRead buffer:pvReadBuffer bufferSize:clusterSize]);
        XCTAssertEqual(uActuallyRead, clusterSize);
        XCTAssertEqual(0, memcmp(pvReadBuffer, pvWriteBuffer, clusterSize));
    }
    
    USERFS_TEST_LOG("Cleanup");
    
    for(int i=fileNum-1; i>=0; i--) {
        fileName = [NSString stringWithFormat:@"%@_%d", longFileName, i];
        PRINT_IF_FAILED([self.fs Lookup:dirNode newNode:&fileNode name:fileName.UTF8String]);
        PRINT_IF_FAILED([self.fs RemoveFile:dirNode name:fileName.UTF8String victim:fileNode]);
        PRINT_IF_FAILED([self.fs CloseFile:fileNode flags:0]);
    }
    
    for(int i=0; i<numFilesToBeEvicted; i++) {
        fileName = [NSString stringWithFormat:@"%s_%d", fileToBeEvictedName, i];
        PRINT_IF_FAILED([self.fs Lookup:self.fs.rootNode newNode:&fileNode name:fileName.UTF8String]);
        PRINT_IF_FAILED([self.fs RemoveFile:self.fs.rootNode name:fileName.UTF8String victim:fileNode]);
        PRINT_IF_FAILED([self.fs CloseFile:fileNode flags:0]);
    }
    
    for(int i=0; i<numFilesToForceEviction; i++) {
        fileName = [NSString stringWithFormat:@"%s_%d", fileToForceEvictionName, i];
        PRINT_IF_FAILED([self.fs Lookup:self.fs.rootNode newNode:&fileNode name:fileName.UTF8String]);
        PRINT_IF_FAILED([self.fs RemoveFile:self.fs.rootNode name:fileName.UTF8String victim:fileNode]);
        PRINT_IF_FAILED([self.fs CloseFile:fileNode flags:0]);
    }
    
    PRINT_IF_FAILED([self.fs RemoveFile:self.fs.rootNode name:fileUnderRootName victim:fileUnderRootNode]);
    PRINT_IF_FAILED([self.fs CloseFile:fileUnderRootNode flags:0]);
    
    PRINT_IF_FAILED([self.fs RemoveFile:dirNode name:"file_1" victim:fileNode1]);
    PRINT_IF_FAILED([self.fs CloseFile:fileNode1 flags:0]);
    PRINT_IF_FAILED([self.fs RemoveFile:dirNode name:"file_2" victim:fileNode2]);
    PRINT_IF_FAILED([self.fs CloseFile:fileNode2 flags:0]);
    PRINT_IF_FAILED([self.fs RemoveFile:dirNode name:"file_3" victim:fileNode3]);
    PRINT_IF_FAILED([self.fs CloseFile:fileNode3 flags:0]);

    PRINT_IF_FAILED([self.fs RemoveFolder:self.fs.rootNode dirName:dirName victim:dirNode]);
    PRINT_IF_FAILED([self.fs CloseFile:dirNode flags:0]);

    free(pvReadBuffer);
    free(pvWriteBuffer);
}

-(void)setEntryAndName:(struct winentry *)winentry
                      :(struct unistr255 *)psName
{
    /*
     * Hardcode winentry and psName to the same values we saw on the damaged
     * media in radar 84966411
     */
    winentry->weCnt = 0x42;
    winentry->weAttributes = 0xf;
    winentry->weReserved1 = 0;
    winentry->weReserved2 = 0;
    winentry->weChksum = 0xd3;
    winentry->wePart1[0] = 0x76;
    winentry->wePart1[2] = 0x69;
    winentry->wePart1[4] = 0x74;
    winentry->wePart1[6] = 0x5f;
    winentry->wePart1[8] = 0x4d;
    winentry->wePart2[0] = 0x65;
    winentry->wePart2[2] = 0x79;
    winentry->wePart2[3] = 0x0;
    winentry->wePart2[4] = 0x74;
    winentry->wePart2[6] = 0x61;
    winentry->wePart2[8] = 0xca;
    winentry->wePart2[9] = 0x8a;
    winentry->wePart2[10] = 0x53;
    winentry->wePart2[11] = 0x53;
    winentry->wePart3[0] = 0xff;
    winentry->wePart3[1] = 0xff;
    winentry->wePart3[2] = 0xff;
    winentry->wePart3[3] = 0xff;

    psName->length = 0x1a;
    psName->chars[13] = 0x76;
    psName->chars[14] = 0x69;
    psName->chars[15] = 0x74;
    psName->chars[16] = 0x5f;
    psName->chars[17] = 0x4d;
    psName->chars[18] = 0x65;
    psName->chars[19] = 0x79;
    psName->chars[20] = 0x74;
    psName->chars[21] = 0x61;
    psName->chars[22] = 0x8aca;
    psName->chars[23] = 0x5353;
}

-(void)testHandlingLongNameEntry
{
    struct winentry winentry = {0};
    struct unistr255 psName = {0};
    int retval = 0;

    [self setEntryAndName:&winentry :&psName];

    /*
     * Test the original damaged entry.
     * uUTF16Counter was 0xb, uUnicodeIndex was 0xd.
     */
    retval = DIROPS_HandleLongNameCharacter(&winentry, 0xb, &psName, 0, 0xd);
    XCTAssertEqual(retval, 2);
    XCTAssertEqual(psName.length, 24); // 0xd + 0xb
    retval = DIROPS_HandleLongNameCharacter(&winentry, 0xb, &psName, 1, 0xd);
    XCTAssertEqual(retval, 1); // psName's length is not updated in this case

    /* Now test a non damaged entry, ending in \0 then FFs */
    winentry.wePart3[0] = 0x0;
    winentry.wePart3[1] = 0x0;
    retval = DIROPS_HandleLongNameCharacter(&winentry, 0xb, &psName, 0, 0xd);
    XCTAssertEqual(retval, 2);
    XCTAssertEqual(psName.length, 24); // 0xd + 0xb
    retval = DIROPS_HandleLongNameCharacter(&winentry, 0xb, &psName, 1, 0xd);
    XCTAssertEqual(retval, 1); // psName's length is not updated in this case

    /* Now test a 26 chars name, no 0 expected, no padding as per spec */
    winentry.wePart3[0] = 0xA;
    winentry.wePart3[1] = 0xB;
    winentry.wePart3[2] = 0xC;
    winentry.wePart3[3] = 0xD;
    retval = DIROPS_HandleLongNameCharacter(&winentry, 0xb, &psName, 0, 0xd);
    XCTAssertEqual(retval, 0);
    /*
     * puCp[0] | (puCp[1] << 8) in DIROPS_HandleLongNameCharacter(),
     * where puCp is winentry.wePart3, so 0xB0A
     */
    XCTAssertEqual(psName.chars[24], 0xB0A);
    XCTAssertEqual(retval, 0);
    /*
     * Increase code coverage - make uUTF16Counter + uUnicodeIndex > 255.
     * We don't expect the psName length to change, verify that.
     */
    uint16_t currNameLen = psName.length;
    retval = DIROPS_HandleLongNameCharacter(&winentry, 0xb, &psName, 0, 0xf5);
    XCTAssertEqual(retval, 1);
    XCTAssertEqual(psName.length, currNameLen);
}

@end


@implementation MsdosUnitTestsFAT12

-(NSString*)getVolumeSize
{
    return @"4M";
}

@end


@implementation MsdosUnitTestsFAT16

-(NSString*)getVolumeSize
{
    return @"500M";
}

@end


@implementation MsdosUnitTestsFAT32

-(NSString*)getVolumeSize
{
    return @"2G";
}

@end
