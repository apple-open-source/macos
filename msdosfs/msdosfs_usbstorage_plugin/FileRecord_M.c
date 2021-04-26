/* Copyright © 2017 Apple Inc. All rights reserved.
 *
 *  FileRecord_M.c
 *  msdosfs.kext
 *
 *  Created by Susanne Romano on 04/10/2017.
 */

#include "FileRecord_M.h"
#include "FAT_Access_M.h"
#include "DirOPS_Handler.h"
#include "Conv.h"
#include "FileOPS_Handler.h"
#include "diagnostic.h"
#include "Logger.h"
#include "Naming_Hash.h"

#define GET_ENTRY_END(entry, fsRecord) (entry->uFileOffset + (uint64_t) ((uint64_t)entry->uAmountOfClusters * CLUSTER_SIZE(fsRecord)))

/* ------------------------------------------------------------------------------------------
 *                                  File Record operations
 * ------------------------------------------------------------------------------------------ */
int
FILERECORD_AllocateRecord(NodeRecord_s** ppvNode, FileSystemRecord_s *psFSRecord, uint32_t uFirstCluster,
                          RecordIdentifier_e eRecordID, NodeDirEntriesData_s* psNodeDirEntriesData, const char *pcUTF8Name,
                          uint32_t uParentFirstCluster, bool bIsParentRoot )
{
    int iError      = 0;
    void* pvBuffer  = NULL;
    size_t uRecordSize = sizeof(NodeRecord_s);

    if (uFirstCluster != 0 && !CLUSTER_IS_VALID(uFirstCluster, psFSRecord)) {
        MSDOS_LOG(LEVEL_ERROR, "FILERECORD_AllocateRecord: got bad start cluster for: %s, %u", pcUTF8Name, uFirstCluster);
        return EINVAL;
    }

    *ppvNode = malloc(uRecordSize);

    if (*ppvNode!= NULL)
    {
        memset( *ppvNode, 0, uRecordSize );
        SET_NODE_AS_VALID((*ppvNode));

        (*ppvNode)->sRecordData.psFSRecord      = psFSRecord;
        (*ppvNode)->sRecordData.eRecordID       = eRecordID;
        (*ppvNode)->sRecordData.uFirstCluster   = uFirstCluster;
        (*ppvNode)->sRecordData.uParentisRoot   = bIsParentRoot;
        TAILQ_INIT(&(*ppvNode)->sRecordData.psClusterChainList);
        
        if ( eRecordID == RECORD_IDENTIFIER_DIR || eRecordID == RECORD_IDENTIFIER_ROOT )
        {
            (*ppvNode)->sExtraData.sDirData.uDirVersion     = 1;
            (*ppvNode)->sExtraData.sDirData.sHT = NULL;

#ifdef MSDOS_NLINK_IS_CHILD_COUNT
            (*ppvNode)->sExtraData.sDirData.uChildCount     = 0;
#endif
        }
        else if ( eRecordID == RECORD_IDENTIFIER_LINK )
        {
            uint32_t uBytesPerSector    = SECTOR_SIZE(psFSRecord);
            pvBuffer                    = malloc(uBytesPerSector);
            if ( NULL == pvBuffer )
            {
                iError = ENOMEM;
                goto exit_with_error;
            }

            uint32_t uStartCluster  = DIROPS_GetStartCluster(psFSRecord, &psNodeDirEntriesData->sDosDirEntry);
            uint64_t uReadSize      = pread(psFSRecord->iFD, pvBuffer, uBytesPerSector, DIROPS_VolumeOffsetForCluster(psFSRecord,uStartCluster));
            if ( uBytesPerSector != uReadSize )
            {
                iError = errno;
                MSDOS_LOG(LEVEL_ERROR, "FILERECORD_AllocateRecord failed to read errno = %d\n", iError);
                goto exit_with_error;
            }

            struct symlink* psLink = (struct symlink*)pvBuffer;
            for (uint8_t uLengthCounter = 0; uLengthCounter < SYMLINK_LENGTH_LENGTH ; ++uLengthCounter)
            {
                char cLengthChar = psLink->length[uLengthCounter];
                (*ppvNode)->sExtraData.sSymLinkData.uSymLinkLength = 10 * ((*ppvNode)->sExtraData.sSymLinkData.uSymLinkLength) + cLengthChar - '0';
            }

            free(pvBuffer);
        }
        else
        {
            (*ppvNode)->sExtraData.sFileData.bIsPreAllocated = false;
        }
        
        if ( psNodeDirEntriesData )
        {
            (*ppvNode)->sRecordData.bIsNDEValid = true;
            (*ppvNode)->sRecordData.sNDE = *psNodeDirEntriesData;
        }
        
        if ( uFirstCluster == 0 )
        {
            (*ppvNode)->sRecordData.uLastAllocatedCluster   = 0;
            //WA for root in case of fat12/16. cluster chain length must be "1".
            if ( IS_FAT_12_16_ROOT_DIR((*ppvNode)) )
            {
                (*ppvNode)->sRecordData.uClusterChainLength = 1;
            }
            else
            {
                (*ppvNode)->sRecordData.uClusterChainLength = 0;
            }
        }
        else
        {
            iError = FAT_Access_M_ChainLength( psFSRecord, uFirstCluster, &(*ppvNode)->sRecordData.uClusterChainLength, NULL, &(*ppvNode)->sRecordData.uLastAllocatedCluster );
            if ( iError != 0 )
            {
                goto exit_with_error;
            }
        }

        //Save the name for quick FSAtter
        CONV_DuplicateName(&(*ppvNode)->sRecordData.pcUtf8Name, pcUTF8Name);

        //Initialize locks
        MultiReadSingleWrite_Init(&(*ppvNode)->sRecordData.sRecordLck);
        pthread_mutexattr_t sAttr;
        pthread_mutexattr_init(&sAttr);
        pthread_mutexattr_settype(&sAttr, PTHREAD_MUTEX_ERRORCHECK);
        
        if (eRecordID == RECORD_IDENTIFIER_FILE) {
            pthread_mutex_init(&(*ppvNode)->sExtraData.sFileData.sUnAlignedWriteLck, &sAttr);
            for (int iCond = 0; iCond < NUM_OF_COND; iCond++) {
                pthread_cond_init( &(*ppvNode)->sExtraData.sFileData.sCondTable[iCond].sCond, NULL );
                (*ppvNode)->sExtraData.sFileData.sCondTable[iCond].uSectorNum = 0;
            }
        }
        
        if (eRecordID != RECORD_IDENTIFIER_ROOT) {
            //Set the parent dir cluster lock
            (*ppvNode)->sRecordData.uParentFirstCluster = uParentFirstCluster;
            iError = DIROPS_SetParentDirClusterCacheLock((*ppvNode));
            if ( iError != 0 )
            {
                MSDOS_LOG(LEVEL_ERROR, "DIROPS_SetParentDirClusterCacheLock failed with (%d).\n", iError);
                goto exit_with_error;
            }   
        }
        
        //If its a dir calculate hash table
        if ( eRecordID == RECORD_IDENTIFIER_DIR || eRecordID == RECORD_IDENTIFIER_ROOT)
        {
            iError = DIROPS_InitDirEntryLockListEntry((*ppvNode));
            if ( iError != 0 )
            {
                MSDOS_LOG(LEVEL_ERROR, "DIROPS_InitDirClusterCacheEntry failed with (%d).\n", iError);
                goto exit_with_error;
            }
            
            if (eRecordID == RECORD_IDENTIFIER_ROOT) {
                (*ppvNode)->sRecordData.sParentDirClusterCacheLck = (*ppvNode)->sExtraData.sDirData.sSelfDirClusterCacheLck;
            }
        }

    }
    else
    {
        MSDOS_LOG(LEVEL_ERROR, "FILERECORD_AllocateRecord: fail to allocate record.\n");
        iError = ENOMEM;
        goto exit_with_error;
    }

#ifdef MSDOS_NLINK_IS_CHILD_COUNT
    //If its a dir calculate num of children
    if ( eRecordID == RECORD_IDENTIFIER_DIR )
    {
        iError = DIROPS_CountChildrenInADirectory(*ppvNode);
        if ( iError != 0 )
        {
            goto exit_with_error;
        }
    }
#endif

    return iError;

exit_with_error:
    if ( NULL != *ppvNode )
    {
        free(*ppvNode);
    }
    *ppvNode = NULL;

    if ( NULL != pvBuffer )
    {
        free(pvBuffer);
    }

    return iError;
}

void FILERECORD_FreeRecord(NodeRecord_s* psNode)
{
wait_for_write_conter:
    while (psNode->sExtraData.sFileData.uWriteCounter != 0) {
        usleep(100);
    }

    MultiReadSingleWrite_LockWrite( &psNode->sRecordData.sRecordLck );
    if (psNode->sExtraData.sFileData.uWriteCounter != 0) {
        MultiReadSingleWrite_FreeWrite( &psNode->sRecordData.sRecordLck );
        goto wait_for_write_conter;
    }
    FILERECORD_EvictAllFileChainCacheEntriesFromGivenOffset(psNode, 0, true);
    MultiReadSingleWrite_FreeWrite( &psNode->sRecordData.sRecordLck );

    if (IS_DIR(psNode))
    {
        // Free any cached directory data (cached cluster, if any, and
        // hash table).
        DIROPS_DestroyHTForDirectory(psNode);

        DIROPS_DereferenceDirEntrlyLockListEntry(psNode, true);
    }

    if (psNode->sRecordData.pcUtf8Name != NULL)
        free(psNode->sRecordData.pcUtf8Name);
    
    //Deinit locks
    DIROPS_DereferenceDirEntrlyLockListEntry(psNode, false);
    MultiReadSingleWrite_DeInit(&psNode->sRecordData.sRecordLck);
    
    if (psNode->sRecordData.eRecordID == RECORD_IDENTIFIER_FILE) {
        pthread_mutex_destroy(&psNode->sExtraData.sFileData.sUnAlignedWriteLck);
        for (int iCond = 0; iCond < NUM_OF_COND; iCond++) {
            pthread_cond_destroy( &psNode->sExtraData.sFileData.sCondTable[iCond].sCond);
        }
    }
    
    // Invalidate magic
    INVALIDATE_NODE(psNode);

    //free File Record
    free(psNode);

    psNode = NULL;

}

/* ------------------------------------------------------------------------------------------
 *                              Cluster chain cache SPI
 * ------------------------------------------------------------------------------------------ */

#define LOWER_BOUND_ENTRIES_TO_EVICT (10)

static uint32_t FILERECORD_GetLastKnownClusterOfChaceEntry(ClusterChainCacheEntry_s* psEntry);
static void     FILERECORD_EvictLRUChainCacheEntry(FileSystemRecord_s* psFSRecord);
static void     FILERECORD_RemoveChainCacheEntry(ClusterChainCacheEntry_s* psEntryToRemove);
static uint8_t  FILERECORD_GetLastElementNumInCacheEntry(ClusterChainCacheEntry_s* psEntry);
static int      FILERECORD_UpdateLastElementNumInCacheEntry(ClusterChainCacheEntry_s* psEntry);

static int      FILERECORD_CreateChainCacheEntry(NodeRecord_s* psNodeRecord,uint32_t uFirstCluster,uint64_t uOffsetInFile);
static void     FILERECORD_AddChainCacheEntryToMainList(FileSystemRecord_s* psFSRecord,ClusterChainCacheEntry_s* psNewClusterChainEntry);
static void     FILERECORD_SetChainCacheEntryInFileLocations(ClusterChainCacheEntry_s* psNewClusterChainEntry,NodeRecord_s* psNodeRecord);
static int      FILERECORD_FillChainCacheEntryWithData(NodeRecord_s* psNodeRecord, uint32_t uFirstCluster,uint64_t uOffsetInFile,
                                                       ClusterChainCacheEntry_s* psClusterChainEntry);
static void     FILERECORD_GetClusterFromChainArray(NodeRecord_s* psNodeRecord,ClusterChainCacheEntry_s* psLookupEntry, uint32_t* puWantedCluster,
                                                    uint32_t* puContiguousClusterLength, uint64_t uWantedOffsetInFile, bool* pbFoundLocation);
static void     FILERECORD_LookForOffsetInFileChainCache(NodeRecord_s* psNodeRecord,ClusterChainCacheEntry_s** psLookupEntry,uint64_t uWantedOffsetInFile,
                                                         uint32_t* puWantedCluster, uint32_t* puContiguousClusterLength, bool* bFoundLocation);
static int      FILERECORD_FindClusterToCreateChainCacheEntry(bool* pbFoundLocation, NodeRecord_s* psNodeRecord, uint32_t uOffsetLocationInClusterChain,
                                                              uint64_t uWantedOffsetInFile, uint32_t* puWantedCluster, uint32_t* puContiguousClusterLength);
/* ------------------------------------------------------------------------------------------ */

static uint32_t FILERECORD_GetLastKnownClusterOfChaceEntry(ClusterChainCacheEntry_s* psEntry) {
    uint8_t uArrayCounter = FILERECORD_GetLastElementNumInCacheEntry(psEntry);
    return psEntry->psConsecutiveCluster[uArrayCounter].uActualStartCluster +
           psEntry->psConsecutiveCluster[uArrayCounter].uAmountOfConsecutiveClusters - 1;
}

/* Assumption: Under write lock */
static void FILERECORD_FillConsecutiveClusterData(FileSystemRecord_s* psFSRecord, ClusterChainCacheEntry_s* psClusterChainEntry, uint32_t uCluster, uint8_t uConClusterCounter, uint32_t uAmountOfConsecutiveCluster, uint64_t* puOffsetInFile)
{
    psClusterChainEntry->psConsecutiveCluster[uConClusterCounter].uActualStartCluster = uCluster;
    psClusterChainEntry->psConsecutiveCluster[uConClusterCounter].uAmountOfConsecutiveClusters = uAmountOfConsecutiveCluster;
    psClusterChainEntry->uAmountOfClusters += uAmountOfConsecutiveCluster;
    *puOffsetInFile += ((uint64_t)uAmountOfConsecutiveCluster) * CLUSTER_SIZE(psFSRecord);
}

static int
FILERECORD_FillChainCacheEntryWithData(NodeRecord_s* psNodeRecord, uint32_t uFirstCluster, uint64_t uOffsetInFile, ClusterChainCacheEntry_s* psClusterChainEntry)
{
    errno_t Error = 0;
    uint32_t uCluster = uFirstCluster;
    FileSystemRecord_s* psFSRecord = GET_FSRECORD(psNodeRecord);
    psClusterChainEntry->uFileOffset = uOffsetInFile;
    psClusterChainEntry->uAmountOfClusters = 0;
    psClusterChainEntry->pvFileOwner = psNodeRecord;

    // look for consecutive clusters from the given first cluster
    for (uint8_t uConClusterCounter = 0; uConClusterCounter < MAX_CHAIN_CACHE_ELEMENTS_PER_ENTRY; uConClusterCounter++) {
        uint32_t uNextCluster;
        uint32_t uAmountOfConsecutiveCluster = FAT_Access_M_ContiguousClustersInChain(psFSRecord, uCluster, &uNextCluster, &Error);
        if (Error != 0)
            return Error;

        FILERECORD_FillConsecutiveClusterData(psFSRecord, psClusterChainEntry, uCluster, uConClusterCounter,
                                              uAmountOfConsecutiveCluster, &uOffsetInFile);
        
        uCluster = uNextCluster;
        // Check if reached to the end of the file
        if (!CLUSTER_IS_VALID(uCluster, psFSRecord)) {
            break;
        }
    }

    return Error;
}

/* Assumption: Under write lock */
static void
FILERECORD_SetChainCacheEntryInFileLocations(ClusterChainCacheEntry_s* psNewClusterChainEntry, NodeRecord_s* psNodeRecord)
{
    // If this is the first cluster chain entry for this file
    if (TAILQ_EMPTY(&psNodeRecord->sRecordData.psClusterChainList)) {
        TAILQ_INSERT_HEAD(&psNodeRecord->sRecordData.psClusterChainList, psNewClusterChainEntry, psClusterChainCacheListEntry);
        return;
    }

    FileSystemRecord_s* psFSRecord = GET_FSRECORD(psNodeRecord);
    uint64_t uNewStart = psNewClusterChainEntry->uFileOffset; /* |-----| */
    uint64_t uNewEnd = GET_ENTRY_END(psNewClusterChainEntry,psFSRecord);

    // Need to look for the correct location to insert the record
    // Need to check if there is an overlap between them
    bool bFoundLocation = false;
    struct sConsecutiveCluster* psLookupEntry;
    struct sConsecutiveCluster* psPrevLookupEntry = NULL;

    TAILQ_FOREACH(psLookupEntry,
                 &psNodeRecord->sRecordData.psClusterChainList, psClusterChainCacheListEntry) {
        // If current entry offset is larger then the new entry offset - we found our spot
        if (psLookupEntry->uFileOffset > uNewStart) {
            uint64_t uEntryEnd = GET_ENTRY_END(psLookupEntry, psFSRecord); /* \---\ */
            uint64_t uPrevEntryEnd = (psPrevLookupEntry!= NULL) ? GET_ENTRY_END(psPrevLookupEntry, psFSRecord) : 0; /* /---/ */

            TAILQ_INSERT_BEFORE(psLookupEntry, psNewClusterChainEntry, psClusterChainCacheListEntry);
            bFoundLocation = true;

            // Check for overlap
            if (uEntryEnd <= uNewEnd) {
                // New entry overlaps psLookupEntry: |--\===\--| or  |---\===\|
                FILERECORD_RemoveChainCacheEntry(psLookupEntry);
            } else if ((psPrevLookupEntry!= NULL) && (uNewStart <= uPrevEntryEnd)) {
                // Making sure it's not the first one since it has no perv entry
                // New Entry overlaps prevEntry : /--|==/--| or |/===/---|
                FILERECORD_RemoveChainCacheEntry(psPrevLookupEntry);
            }
            break;
        }
        psPrevLookupEntry = psLookupEntry;
    }

    // If we reached to the end of the entries list
    if (!bFoundLocation) {
        TAILQ_INSERT_TAIL(&psNodeRecord->sRecordData.psClusterChainList, psNewClusterChainEntry, psClusterChainCacheListEntry);
    }

    DIAGNOSTIC_CHECK_CHAIN_CACHE_ENTRY(psNodeRecord);
}

/* Assumption: Under write lock */
static void
FILERECORD_RemoveChainCacheEntry(ClusterChainCacheEntry_s* psEntryToRemove)
{
    NodeRecord_s* psFileOwner = GET_RECORD(psEntryToRemove->pvFileOwner);
    FileSystemRecord_s* psFSRecord = GET_FSRECORD(psFileOwner);
    
    TAILQ_REMOVE(&psFSRecord->psClusterChainCache->psChainCacheList, psEntryToRemove, psCacheListEntry);
    TAILQ_REMOVE(&psFileOwner->sRecordData.psClusterChainList, psEntryToRemove, psClusterChainCacheListEntry);
    
    psFSRecord->psClusterChainCache->uAmountOfEntries--;
    free(psEntryToRemove);
    psEntryToRemove = NULL;
}

/* Assumption: Under write lock */
static void
FILERECORD_EvictLRUChainCacheEntry(FileSystemRecord_s* psFSRecord)
{
    uint64_t uLargestLRUCounterInArray = 0;
    uint8_t uLargestElementLocation = 0;
    uint8_t uCounter = 0;
    ClusterChainCacheEntry_s* psLRUEntryToEvict[LOWER_BOUND_ENTRIES_TO_EVICT] = {NULL};
    ClusterChainCacheEntry_s* psLookupEntry = NULL;

    // Store the first LOWER_BOUND_ENTRIES_TO_EVICT elements in a temp array temp[0..k-1].
    // Find the largest element in psLRUEntryToEvict[].
    TAILQ_FOREACH(psLookupEntry,
                 &psFSRecord->psClusterChainCache->psChainCacheList, psCacheListEntry) {
        if (uCounter < LOWER_BOUND_ENTRIES_TO_EVICT) {
            psLRUEntryToEvict[uCounter] = psLookupEntry;
            if (uLargestLRUCounterInArray < psLookupEntry->uLRUCounter) {
                uLargestLRUCounterInArray = psLookupEntry->uLRUCounter;
                uLargestElementLocation = uCounter;
            }
            uCounter++;
        } else {
            // If element uLRUCounter is smaller than uLargestLRUCounterInArray
            // then replace and find new uLargestLRUCounterInArray.
            if (psLookupEntry->uLRUCounter < uLargestLRUCounterInArray) {
                psLRUEntryToEvict[uLargestElementLocation] = psLookupEntry;
                uLargestLRUCounterInArray = psLookupEntry->uLRUCounter;

                // find the largest element in the array
                for (int i = 0; i < LOWER_BOUND_ENTRIES_TO_EVICT; i++) {
                    if (uLargestLRUCounterInArray < psLookupEntry->uLRUCounter) {
                        uLargestLRUCounterInArray = psLookupEntry->uLRUCounter;
                        uLargestElementLocation = i;
                    }
                }
            }
        }
    }

    // remove the smallest LOWER_BOUND_ENTRIES_TO_EVICT elements
    for (int i = 0; i < LOWER_BOUND_ENTRIES_TO_EVICT; i++) {
        FILERECORD_RemoveChainCacheEntry(psLRUEntryToEvict[i]);
    }
}

/* Assumption: Under write lock */
static void
FILERECORD_AddChainCacheEntryToMainList(FileSystemRecord_s* psFSRecord, ClusterChainCacheEntry_s* psNewClusterChainEntry)
{
    psNewClusterChainEntry->uLRUCounter = atomic_fetch_add(&psFSRecord->psClusterChainCache->uMaxLRUCounter, 1);

    psFSRecord->psClusterChainCache->uAmountOfEntries++;
    if (psFSRecord->psClusterChainCache->uAmountOfEntries > MAX_CHAIN_CACHE_ENTRIES)
    {// Need to evict the LRU item before adding a new one
        FILERECORD_EvictLRUChainCacheEntry(psFSRecord);
    }

    TAILQ_INSERT_TAIL(&psFSRecord->psClusterChainCache->psChainCacheList, psNewClusterChainEntry, psCacheListEntry);
}

int FILERECORD_UpdateNewAllocatedClustersInChain(NodeRecord_s* psNodeRecord, uint32_t uFirstCluster, uint32_t uChainLength, uint64_t uOffsetInFile)
{
    FileSystemRecord_s* psFSRecord = GET_FSRECORD(psNodeRecord);
    bool bForceInsert = false;
new_one:
    //If file node doesn't have cache
    if (TAILQ_EMPTY(&psNodeRecord->sRecordData.psClusterChainList) || bForceInsert) {
        ClusterChainCacheEntry_s* psNewClusterChainEntry = malloc(sizeof(ClusterChainCacheEntry_s));
        if (psNewClusterChainEntry == NULL) {
            MSDOS_LOG(LEVEL_ERROR, "FILERECORD_UpdateNewAllocatedClustersInChain failed to allocate memory\n");
            return ENOMEM;
        }

        memset(psNewClusterChainEntry, 0, sizeof(ClusterChainCacheEntry_s));
        psNewClusterChainEntry->uFileOffset = uOffsetInFile;
        psNewClusterChainEntry->uAmountOfClusters = 0;
        psNewClusterChainEntry->pvFileOwner = psNodeRecord;

        FILERECORD_FillConsecutiveClusterData(psFSRecord, psNewClusterChainEntry, uFirstCluster, 0, uChainLength, &uOffsetInFile);

        // Set in main cache list
        FILERECORD_AddChainCacheEntryToMainList(psFSRecord, psNewClusterChainEntry);

        // Set in file list
        FILERECORD_SetChainCacheEntryInFileLocations(psNewClusterChainEntry, psNodeRecord);

        return 0;
    }

    ClusterChainCacheEntry_s* psLastEntry = TAILQ_LAST(&psNodeRecord->sRecordData.psClusterChainList, sConsecutiveClusterList);
    //update the last entry, only if asked offset is the same as the entry end offset
    // otherwise create a new one
    if (GET_ENTRY_END(psLastEntry,psFSRecord) == uOffsetInFile) {
        uint8_t uArrayCounter = FILERECORD_GetLastElementNumInCacheEntry(psLastEntry);
        //This Entry is Full, need new one
        if (uArrayCounter + 1 == MAX_CHAIN_CACHE_ELEMENTS_PER_ENTRY) {
            bForceInsert = true;
            goto new_one;
        } else {
            // if we can extend the last entry, o.w fill a new one
            if (FILERECORD_GetLastKnownClusterOfChaceEntry(psLastEntry) ==  uFirstCluster - 1) {
                psLastEntry->psConsecutiveCluster[uArrayCounter].uAmountOfConsecutiveClusters += uChainLength ;
                psLastEntry->uAmountOfClusters += uChainLength;
                DIAGNOSTIC_CHECK_CHAIN_CACHE_ENTRY(psNodeRecord);
            } else {
                FILERECORD_FillConsecutiveClusterData(psFSRecord, psLastEntry, uFirstCluster, uArrayCounter + 1, uChainLength, &uOffsetInFile);
            }
            psLastEntry->uLRUCounter = atomic_fetch_add(&psFSRecord->psClusterChainCache->uMaxLRUCounter, 1);
        }
    } else {
        bForceInsert = true;
        goto new_one;
    }

    return 0;
}

/* Assumption: Under write lock */
static int
FILERECORD_UpdateLastElementNumInCacheEntry(ClusterChainCacheEntry_s* psEntry )
{
    int iError = 0;
    NodeRecord_s* psNodeRecord = GET_RECORD(psEntry->pvFileOwner);
    FileSystemRecord_s *psFSRecord = GET_FSRECORD(psNodeRecord);

    uint32_t uLastKnownCluster = FILERECORD_GetLastKnownClusterOfChaceEntry(psEntry);
    if (!CLUSTER_IS_VALID(uLastKnownCluster, psFSRecord)) {
        iError = EINVAL;
        MSDOS_LOG(LEVEL_ERROR, "FILERECORD_UpdateLastElementNumInCacheEntry: Last known cluster is invalid");
        goto exit;
    }

    //Check if last known element can be extended
    uint32_t NextCluster;
    uint32_t uNewChainLength = 0;
    iError = FAT_Access_M_GetClustersFatEntryContent(psFSRecord, uLastKnownCluster, &NextCluster);
    if (iError) {
        goto exit;
    }

    uint8_t uArrayCounter = FILERECORD_GetLastElementNumInCacheEntry(psEntry);
    if (NextCluster == uLastKnownCluster + 1) {
        uLastKnownCluster = NextCluster;
        uNewChainLength = FAT_Access_M_ContiguousClustersInChain(psFSRecord, uLastKnownCluster, &NextCluster, &iError);
        if (iError) {
            goto exit;
        }

        psEntry->psConsecutiveCluster[uArrayCounter].uAmountOfConsecutiveClusters += uNewChainLength;
        psEntry->uAmountOfClusters += uNewChainLength;
    }

    //While we still can populate -> still not the end of the file
    uint8_t uNextArrayCounter = uArrayCounter + 1;
    while (uNextArrayCounter < MAX_CHAIN_CACHE_ELEMENTS_PER_ENTRY && CLUSTER_IS_VALID(NextCluster, psFSRecord)) {
        uLastKnownCluster = NextCluster;
        uNewChainLength = FAT_Access_M_ContiguousClustersInChain(psFSRecord, uLastKnownCluster, &NextCluster, &iError);
        if (iError) {
            break;
        }

        uint64_t uOffsetInFile = GET_ENTRY_END(psEntry, psFSRecord);
        FILERECORD_FillConsecutiveClusterData(psFSRecord, psEntry, uLastKnownCluster, uNextArrayCounter, uNewChainLength, &uOffsetInFile);

        uNextArrayCounter++;
    }

exit:
    return iError;
}

/*
 * Since the array containing the contiguous clusters, dosn't have to be always fully occupied,
 * in order to get the last known cluster we need to look for an element with cluster == 0
 * (since cluster 0 can't be a valid cluster).
 *
 * Assumption: Under write lock
 */
static uint8_t
FILERECORD_GetLastElementNumInCacheEntry(ClusterChainCacheEntry_s* psEntry)
{
    uint8_t uArrayCounter;
    for ( uArrayCounter = 0; uArrayCounter < MAX_CHAIN_CACHE_ELEMENTS_PER_ENTRY; uArrayCounter++)
    {
        if ( psEntry->psConsecutiveCluster[uArrayCounter].uActualStartCluster == 0)
        {
            break;
        }
    }
    
    if (uArrayCounter == 0) {
        MSDOS_LOG(LEVEL_ERROR, "FILERECORD_GetLastElementNumInCacheEntry: got psEntry with no valid elements");
        assert(0);
    }
    return --uArrayCounter;
}

/* Assumption: Under write lock */
static int
FILERECORD_FindClusterToCreateChainCacheEntry(bool* pbFoundLocation, NodeRecord_s* psNodeRecord, uint32_t uClusterOffsetFromWantedCluster,
                                              uint64_t uWantedOffsetInFile, uint32_t* puWantedCluster, uint32_t* puContiguousClusterLength)
{
    int iError = 0;
    FileSystemRecord_s* psFSRecord = GET_FSRECORD(psNodeRecord);
    while (!(*pbFoundLocation))
    {
        uint32_t NextCluster = 0;
        uint32_t uNewChainLength = FAT_Access_M_ContiguousClustersInChain(psFSRecord, *puWantedCluster, &NextCluster, &iError);
        if (iError || uNewChainLength == 0)
        {
            MSDOS_LOG(LEVEL_ERROR, "FILERECORD_FindClusterToCreateChainCacheEntry failed allocateerror [%d].\n", iError);
            *puContiguousClusterLength = 0;
            break;
        }

        if (uClusterOffsetFromWantedCluster < uNewChainLength)
        {
            *puWantedCluster += uClusterOffsetFromWantedCluster;
            *puContiguousClusterLength = uNewChainLength - uClusterOffsetFromWantedCluster;

            // If we reached the end of the chain
            // Create new cache entry after the last existing one
            iError = FILERECORD_CreateChainCacheEntry(psNodeRecord, *puWantedCluster, uWantedOffsetInFile);
            if (iError)
            {
                *puContiguousClusterLength = 0;
                MSDOS_LOG(LEVEL_ERROR, "FILERECORD_FindClusterToCreateChainCacheEntry fail to create cache entry [%d].\n", iError);
                break;
            }

            *pbFoundLocation = true;
        }
        else if ((uClusterOffsetFromWantedCluster == uNewChainLength) && !CLUSTER_IS_VALID(NextCluster, psFSRecord))
        {
            //If we got to the end of the cluster chain
            *puContiguousClusterLength = 0;
            break;
        }
        else
        {
            *puWantedCluster = NextCluster;
            uClusterOffsetFromWantedCluster -= uNewChainLength;
        }
    }

    return iError;
}

/* Assumption: Under write lock */
static int
FILERECORD_CreateChainCacheEntry(NodeRecord_s* psNodeRecord, uint32_t uFirstCluster, uint64_t uOffsetInFile)
{
    int iError = 0;
    FileSystemRecord_s* psFSRecord = GET_FSRECORD(psNodeRecord);
    ClusterChainCacheEntry_s* psNewClusterChainEntry = malloc(sizeof(ClusterChainCacheEntry_s));
    if (psNewClusterChainEntry == NULL)
    {
        MSDOS_LOG(LEVEL_ERROR, "FILERECORD_CreateChainCacheEntry failed to allocate memory\n");
        return ENOMEM;
    }

    memset(psNewClusterChainEntry,0,sizeof(ClusterChainCacheEntry_s));
    //make sure to round the offset down
    uOffsetInFile = ROUND_DOWN(uOffsetInFile, CLUSTER_SIZE(psFSRecord));
    iError = FILERECORD_FillChainCacheEntryWithData(psNodeRecord, uFirstCluster, uOffsetInFile, psNewClusterChainEntry);
    if (iError)
    {
        free(psNewClusterChainEntry);
        return iError;
    }

    // Set in main cache list
    FILERECORD_AddChainCacheEntryToMainList(psFSRecord, psNewClusterChainEntry);

    // Set in file list
    FILERECORD_SetChainCacheEntryInFileLocations(psNewClusterChainEntry, psNodeRecord);
    
    return 0;
}

/* Assumption: Under read lock */
static void
FILERECORD_GetClusterFromChainArray(NodeRecord_s* psNodeRecord, ClusterChainCacheEntry_s* psLookupEntry, uint32_t* puWantedCluster,
                                    uint32_t* puContiguousClusterLength, uint64_t uWantedOffsetInFile, bool* pbFoundLocation)
{
    *puContiguousClusterLength = 0;
    if (psLookupEntry->uFileOffset > uWantedOffsetInFile) {
        MSDOS_LOG(LEVEL_ERROR, "FILERECORD_GetClusterFromChainArray: psLookupEntry->uFileOffset [%llu] > uWantedOffsetInFile [%llu].\n",
                  psLookupEntry->uFileOffset, uWantedOffsetInFile);
        assert(0);
        return;
    }

    uint32_t uClusterDistanceFromEntryStart = (uint32_t) ((uWantedOffsetInFile - psLookupEntry->uFileOffset)/CLUSTER_SIZE(GET_FSRECORD(psNodeRecord)));

    //Go over the elements and look for the wanted cluster
    for (uint8_t uArrayCounter = 0; uArrayCounter < MAX_CHAIN_CACHE_ELEMENTS_PER_ENTRY; uArrayCounter++)
    {
        // If we reached to an empty element we are done
        if (psLookupEntry->psConsecutiveCluster[uArrayCounter].uAmountOfConsecutiveClusters == 0) {
            MSDOS_LOG(LEVEL_ERROR, "FILERECORD_GetClusterFromChainArray: failed to find offset [%llu, %u, %llu, %u, %d].\n",
                      psLookupEntry->uFileOffset, psLookupEntry->uAmountOfClusters, uWantedOffsetInFile,
                      uClusterDistanceFromEntryStart, uArrayCounter);
            break;
        }

        if (uClusterDistanceFromEntryStart < psLookupEntry->psConsecutiveCluster[uArrayCounter].uAmountOfConsecutiveClusters) {
            *puWantedCluster = psLookupEntry->psConsecutiveCluster[uArrayCounter].uActualStartCluster + uClusterDistanceFromEntryStart;
            *puContiguousClusterLength = psLookupEntry->psConsecutiveCluster[uArrayCounter].uAmountOfConsecutiveClusters - uClusterDistanceFromEntryStart;
            *pbFoundLocation = true;
            break;
        }
        uClusterDistanceFromEntryStart -= psLookupEntry->psConsecutiveCluster[uArrayCounter].uAmountOfConsecutiveClusters;
    }
}

/* Assumption: Under read lock
 * Returns the last known psLookupEntry in case uWantedOffsetInFile is not in the cache*/
static void
FILERECORD_LookForOffsetInFileChainCache(NodeRecord_s* psNodeRecord, ClusterChainCacheEntry_s** ppsLookupEntry, uint64_t uWantedOffsetInFile,
                                         uint32_t* puWantedCluster, uint32_t* puContiguousClusterLength, bool* bFoundLocation)
{
    *ppsLookupEntry = NULL;
    *bFoundLocation = false;
    FileSystemRecord_s* psFSRecord = GET_FSRECORD(psNodeRecord);

    // If the cache is empty
    // or if the first entry is already bigger then wanted offset
    ClusterChainCacheEntry_s* psFirstEntry = TAILQ_FIRST(&psNodeRecord->sRecordData.psClusterChainList);
    if (TAILQ_EMPTY(&psNodeRecord->sRecordData.psClusterChainList) ||
        psFirstEntry->uFileOffset > uWantedOffsetInFile) {
        return;
    }

    // if the last entry end offset is smaller then wanted offset - just return the last entry
    ClusterChainCacheEntry_s* psLastEntry = TAILQ_LAST(&psNodeRecord->sRecordData.psClusterChainList, sConsecutiveClusterList);
    uint64_t uLastEntryEnd = GET_ENTRY_END(psLastEntry, psFSRecord);
    if (uLastEntryEnd < uWantedOffsetInFile) {
        *ppsLookupEntry = psLastEntry;
        return;
    }

    ClusterChainCacheEntry_s* psPrevLookupEntry = NULL;
    TAILQ_FOREACH(*ppsLookupEntry, &psNodeRecord->sRecordData.psClusterChainList, psClusterChainCacheListEntry) {
        // If LookupEntryOffset is larger then the wanted offset
        // we know that there is no cache for the wanted offset
        if ((*ppsLookupEntry)->uFileOffset > uWantedOffsetInFile)
        {
            *ppsLookupEntry = psPrevLookupEntry;
            break;
        }

        uint64_t uEndOfEntryOffset = GET_ENTRY_END((*ppsLookupEntry), psFSRecord);
        //If the wanted offset is within the current entry range
        if (uWantedOffsetInFile < uEndOfEntryOffset) {
            // Need to find the wanted cluster according to the distance between
            // the wanted offset to the current entry offset
            FILERECORD_GetClusterFromChainArray(psNodeRecord, *ppsLookupEntry, puWantedCluster, puContiguousClusterLength,
                                                uWantedOffsetInFile, bFoundLocation);
            if (*bFoundLocation ) {
                break;
            } else {
                *ppsLookupEntry = NULL;
                break;
            }
        }
        psPrevLookupEntry = *ppsLookupEntry;
    }
}


/* ------------------------------------------------------------------------------------------
 *                              Cluster chain cache API
 * ------------------------------------------------------------------------------------------ */

int
FILERECORD_InitChainCache(FileSystemRecord_s *psFSRecord)
{
    psFSRecord->psClusterChainCache = malloc(sizeof(ClusterChainCache_s));
    if (psFSRecord->psClusterChainCache != NULL)
    {
        TAILQ_INIT(&psFSRecord->psClusterChainCache->psChainCacheList);
        psFSRecord->psClusterChainCache->uAmountOfEntries = 0;
        psFSRecord->psClusterChainCache->uMaxLRUCounter = 0;

        //Init Locks
        pthread_mutexattr_t sAttr;
        pthread_mutexattr_init(&sAttr);
        pthread_mutexattr_settype(&sAttr, PTHREAD_MUTEX_ERRORCHECK );
        pthread_mutex_init(&psFSRecord->psClusterChainCache->sClusterChainMutex, &sAttr);
    }
    else
    {
        MSDOS_LOG(LEVEL_ERROR, "FILERECORD_InitChainCache: fail to allocate record.\n");
        return ENOMEM;
    }

    return 0;
}

void
FILERECORD_DeInitChainCache(FileSystemRecord_s *psFSRecord)
{
    // Deinit Locks
    pthread_mutex_destroy(&psFSRecord->psClusterChainCache->sClusterChainMutex);
    if (psFSRecord->psClusterChainCache) free(psFSRecord->psClusterChainCache);
    psFSRecord->psClusterChainCache = NULL;
}

void
FILERECORD_EvictAllFileChainCacheEntriesFromGivenOffset(NodeRecord_s* psNodeRecord, uint64_t uOffsetToEvictFrom, bool bLock)
{
    FileSystemRecord_s* psFSRecord = GET_FSRECORD(psNodeRecord);
    // Lock Cache for write
    if (bLock) CHAIN_CACHE_ACCESS_LOCK(psFSRecord);

    ClusterChainCacheEntry_s* psClusterChainToEvict = TAILQ_FIRST(&psNodeRecord->sRecordData.psClusterChainList);
    ClusterChainCacheEntry_s* psClusterChainNext;

    while (psClusterChainToEvict != NULL) {
        //Calculate entry end offset
        psClusterChainNext = TAILQ_NEXT(psClusterChainToEvict, psClusterChainCacheListEntry);

        //Evict only if entry start offset is after given offset
        if (uOffsetToEvictFrom <= psClusterChainToEvict->uFileOffset ) {
            FILERECORD_RemoveChainCacheEntry(psClusterChainToEvict);
        }

        psClusterChainToEvict = psClusterChainNext;
    }

    // Unlock Cache
    if (bLock) CHAIN_CACHE_ACCESS_FREE(psFSRecord);
}

static void
FILERECORD_GetChainFromLastKnownOffset(NodeRecord_s* psNodeRecord, uint64_t uWantedOffsetInFile, uint32_t* puWantedCluster, uint32_t* puContiguousClusterLength, int* piError, ClusterChainCacheEntry_s* psLookupEntry)
{
    uint32_t uOffsetLocationInClusterChain = 0;
    bool bFoundLocation = false;

    FileSystemRecord_s* psFSRecord = GET_FSRECORD(psNodeRecord);
    // Lookup will start from the last relevant cluster
    // Get cluster according to offset
    if (psLookupEntry != NULL)
    {
        uint64_t uLastKnownOffset = GET_ENTRY_END(psLookupEntry, psFSRecord);
        /* protection - something went wrong */
        if (uLastKnownOffset > uWantedOffsetInFile) {
            MSDOS_LOG(LEVEL_FAULT, "Something went wrong with our cache - uLastKnownOffset [%llu], uWantedOffsetInFile [%llu].\n",
                      uLastKnownOffset, uWantedOffsetInFile);

            //Clear the cache for this node
            FILERECORD_EvictAllFileChainCacheEntriesFromGivenOffset(psNodeRecord, 0, false);
        } else {
            uOffsetLocationInClusterChain = (uint32_t) ((uWantedOffsetInFile - uLastKnownOffset)/CLUSTER_SIZE(psFSRecord));
            *puWantedCluster = FILERECORD_GetLastKnownClusterOfChaceEntry(psLookupEntry);

            //Get the next cluster according to last known cluster
            *piError = FAT_Access_M_GetClustersFatEntryContent(psFSRecord, *puWantedCluster, puWantedCluster);
            //If we reached the end of the chain
            if ( *piError != 0 || !CLUSTER_IS_VALID(*puWantedCluster, psFSRecord)) {
                MSDOS_LOG(LEVEL_DEBUG, "Next Cluster is invalid, end of chain.\n");
                *puContiguousClusterLength = 0;
                return;
            }

            goto lookup;
        }
    }

    // No referance entry-> Start from the begining
    *puWantedCluster = psNodeRecord->sRecordData.uFirstCluster;
    uOffsetLocationInClusterChain = (uint32_t) (uWantedOffsetInFile/CLUSTER_SIZE(psFSRecord));

lookup:
    *piError = FILERECORD_FindClusterToCreateChainCacheEntry(&bFoundLocation, psNodeRecord, uOffsetLocationInClusterChain,
                                                             uWantedOffsetInFile, puWantedCluster, puContiguousClusterLength);
}

/*
 * Returns a set of contiguous clusters according to a given offset in a file
 * The function will look if the given offset is already inside the cache.
 * In case not, will lookup the wanted offset from the FAT, and will update the cache.
 */
void
FILERECORD_GetChainFromCache(NodeRecord_s* psNodeRecord, uint64_t uWantedOffsetInFile, uint32_t* puWantedCluster, uint32_t* puContiguousClusterLength, int* piError)
{
    FileSystemRecord_s* psFSRecord = GET_FSRECORD(psNodeRecord);

    //In case of FAT12/16 Root Dir can't be extended
    if ( IS_FAT_12_16_ROOT_DIR(psNodeRecord) ) {
        *puWantedCluster = psNodeRecord->sRecordData.uFirstCluster;
        *puContiguousClusterLength =  1;
        return;
    }
    //In case this file was created empty
    if (psNodeRecord->sRecordData.uFirstCluster == 0) {
        *puWantedCluster = 0;
        *puContiguousClusterLength =  0;
        return;
    }
    
    if (!CLUSTER_IS_VALID(psNodeRecord->sRecordData.uFirstCluster, psFSRecord))
    {
        MSDOS_LOG(LEVEL_INFO, "FILERECORD_GetChainFromCache: first cluster is bad: %u.\n", psNodeRecord->sRecordData.uFirstCluster);
        assert(0);
        *puWantedCluster = 0;
        *puContiguousClusterLength =  0;
        *piError = EINVAL;
        return;
    }

    //Lock the cache for write
    CHAIN_CACHE_ACCESS_LOCK(psFSRecord);

    *puWantedCluster = psNodeRecord->sRecordData.uFirstCluster;
    uint32_t uOffsetLocationInClusterChain = (uint32_t) (uWantedOffsetInFile/CLUSTER_SIZE(psFSRecord));
    bool bFoundLocation = false;

    if (TAILQ_EMPTY(&psNodeRecord->sRecordData.psClusterChainList)) {
        //This file node has no cache entry
        //Lookup will start from the beginning of the file
        //Get cluster according to offset
        //Create the new cache entry
        *piError = FILERECORD_FindClusterToCreateChainCacheEntry(&bFoundLocation, psNodeRecord, uOffsetLocationInClusterChain,
                                                                 uWantedOffsetInFile, puWantedCluster, puContiguousClusterLength);
        goto exit;
    }

    ClusterChainCacheEntry_s* psLookupEntry = NULL;
    //Look for wanted offset In file chain
    FILERECORD_LookForOffsetInFileChainCache(psNodeRecord, &psLookupEntry, uWantedOffsetInFile,
                                             puWantedCluster, puContiguousClusterLength, &bFoundLocation);

    //Try to update the last entry and check if wanted offset inside
    if (psLookupEntry != NULL && !bFoundLocation) {
        *piError = FILERECORD_UpdateLastElementNumInCacheEntry( psLookupEntry );
        if ( *piError != 0 ) {
            MSDOS_LOG(LEVEL_ERROR, "FILERECORD_GetChainFromCache: FILERECORD_UpdateLastElementNumInCacheEntry failed with error [%d].\n", *piError);
            *puContiguousClusterLength = 0;
            goto exit;
        }

        //Check again if we have the wanted offset in this entry
        uint64_t uLastKnownOffset = GET_ENTRY_END(psLookupEntry, psFSRecord);
        if (uLastKnownOffset > uWantedOffsetInFile) {
            FILERECORD_GetClusterFromChainArray(psNodeRecord, psLookupEntry, puWantedCluster,
                                                puContiguousClusterLength, uWantedOffsetInFile, &bFoundLocation);
            if (!bFoundLocation) {
                MSDOS_LOG(LEVEL_DEBUG, "FILERECORD_GetChainFromCache: Couldn't find cluster in the cache.\n");
                *puContiguousClusterLength = 0;
                goto exit;
            }
        }
    }

    if (bFoundLocation && psLookupEntry!= NULL) {
        psLookupEntry->uLRUCounter = atomic_fetch_add(&psFSRecord->psClusterChainCache->uMaxLRUCounter, 1);
    }
    //If we reached to the last file cache entry or we found a hole
    //in the cache and we need to create a new cache entry
    else
    {
        FILERECORD_GetChainFromLastKnownOffset(psNodeRecord, uWantedOffsetInFile, puWantedCluster, puContiguousClusterLength, piError, psLookupEntry);
    } //if (!bFoundLocation && psLookupEntry!= NULL)

exit:
    //Unlock the cache
    CHAIN_CACHE_ACCESS_FREE(psFSRecord);
}

static int
FILERECORD_CompareRecordsLock( const void *pvNR1, const void *pvNR2 )
{
    int64_t iRes= *(NodeRecord_s**)pvNR1 - *(NodeRecord_s**)pvNR2;
    return iRes>0? 1 : iRes==0? 0 : -1;
}

static int
FILERECORD_CompareRecordsRelease( const void *pvNR1, const void *pvNR2 )
{
    int64_t iRes= *(NodeRecord_s**)pvNR1 - *(NodeRecord_s**)pvNR2;
    return iRes>0? -1 : iRes==0? 0 : 1;
}

/*
 * Lock/Release multiple NodeReords by order.
 */
void
FILERECORD_MultiLock( NodeRecord_s** ppsNRs, int iNumOfElem, bool bIsWrite, bool bLock )
{
    qsort(ppsNRs, iNumOfElem, sizeof(NodeRecord_s*), bLock? FILERECORD_CompareRecordsLock : FILERECORD_CompareRecordsRelease);

    for ( int iIdx=0; iIdx<iNumOfElem; iIdx++ )
    {
        if ( (ppsNRs[iIdx] != NULL)  )
        {
            if ( (iIdx == 0) || ppsNRs[iIdx] != ppsNRs[iIdx-1])
            {
                if ( bLock )
                {
                    if ( bIsWrite )
                    {
                        MultiReadSingleWrite_LockWrite(&ppsNRs[iIdx]->sRecordData.sRecordLck);
                    }
                    else
                    {
                        MultiReadSingleWrite_LockRead(&ppsNRs[iIdx]->sRecordData.sRecordLck);
                    }
                }
                else
                {
                    if ( bIsWrite )
                    {
                        MultiReadSingleWrite_FreeWrite(&ppsNRs[iIdx]->sRecordData.sRecordLck);
                    }
                    else
                    {
                        MultiReadSingleWrite_FreeRead(&ppsNRs[iIdx]->sRecordData.sRecordLck);
                    }
                }
            }
        }
    }
}
