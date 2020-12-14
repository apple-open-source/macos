/* Copyright © 2017 Apple Inc. All rights reserved.
 *
 *  usbstorage_tester.c
 *  usbstorage_plugin
 */

#include "FSOPS_Handler.h"
#include "Conv.h"
#include <fcntl.h>
#import  <os/log.h>
#include <time.h>
#include <sys/timeb.h>
#include <sys/time.h>
#include "FAT_Access_M.h"
#include <pthread.h>


#define TEST_1_THREAD_COUNT (4)
#define TEST_1_1_THREAD_COUNT (3)
#define TEST_1_1_FILES_COUNT (10)
#define TEST_2_THREAD_COUNT (4)
#define TEST_3_THREAD_COUNT (7)

#define TEST_4_THREAD_COUNT (4)
#define TEST_4_FILES_PER_THREAD (50)

#define TEST_5_THREAD_COUNT (5)
#define TEST_5_FILE_NAME  "Iamjustasimplefile_%llu"
#define TEST_5_NUM_OF_FILES (200)

#define TEST_6_THREAD_COUNT (7)
#define TEST_6_NUM_OF_FILES (100)
#define TEST_6_FILE_NAME  "Iamjustasimplefile_%d"
#define TEST_6_DB_LINE  "op %d done on file_%d"

#define TEST_7_THREAD_COUNT (12)

#define TEST_CYCLE_COUNT (20)

typedef int pthread_barrierattr_t;
typedef struct
{
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int count;
    int tripCount;
} pthread_barrier_t;


static int pthread_barrier_init(pthread_barrier_t *barrier, const pthread_barrierattr_t *attr, unsigned int count)
{
    if(count == 0)
    {
        errno = EINVAL;
        return -1;
    }
    if(pthread_mutex_init(&barrier->mutex, NULL) < 0)
    {
        return -1;
    }
    if(pthread_cond_init(&barrier->cond, 0) < 0)
    {
        pthread_mutex_destroy(&barrier->mutex);
        return -1;
    }
    barrier->tripCount = count;
    barrier->count = 0;

    return 0;
}

static int pthread_barrier_destroy(pthread_barrier_t *barrier)
{
    pthread_cond_destroy(&barrier->cond);
    pthread_mutex_destroy(&barrier->mutex);
    return 0;
}

static int pthread_barrier_wait(pthread_barrier_t *barrier)
{
    pthread_mutex_lock(&barrier->mutex);
    ++(barrier->count);
    if(barrier->count >= barrier->tripCount)
    {
        barrier->count = 0;
        pthread_cond_broadcast(&barrier->cond);
        pthread_mutex_unlock(&barrier->mutex);
        return 1;
    }
    else
    {
        pthread_cond_wait(&barrier->cond, &(barrier->mutex));
        pthread_mutex_unlock(&barrier->mutex);
        return 0;
    }
}

typedef struct
{
    uint32_t uThreadId;
    uint32_t uTotalNumberOfThreads;
    uint32_t uFileSize;
    uint32_t uBufferSize;

    union {
        UVFSFileNode* ppvFileToWriteNode;
        UVFSFileNode* ppvParentFolder;
    }ThreadInputNode;
    
    union {
        UVFSFileNode* ppvToDirNode;
    }ThreadTargetNode;
    
    pthread_barrier_t* barrier;
    atomic_uint_least64_t* general_offset;
    void* general_buff;

} ThreadInput_s;

pthread_t psTest1_Threads[TEST_1_THREAD_COUNT] = {0};
int piTest1_Results[TEST_1_THREAD_COUNT] = {0};
ThreadInput_s sTest1_ThreadInput[TEST_1_THREAD_COUNT];

pthread_t psTest1_1_Threads[TEST_1_1_THREAD_COUNT * TEST_1_1_FILES_COUNT] = {0};
int piTest1_1_Results[TEST_1_1_THREAD_COUNT * TEST_1_1_FILES_COUNT] = {0};
ThreadInput_s sTest1_1_ThreadInput[TEST_1_1_THREAD_COUNT * TEST_1_1_FILES_COUNT];

pthread_t psTest2_Threads[TEST_2_THREAD_COUNT] = {0};
int piTest2_Results[TEST_2_THREAD_COUNT] = {0};
ThreadInput_s sTest2_ThreadInput[TEST_2_THREAD_COUNT];
atomic_uint_least64_t sTest2_ThreadOffset[TEST_2_THREAD_COUNT];

pthread_t g_psTest3_Threads[TEST_3_THREAD_COUNT] = {0};
int g_piTest3_Results[TEST_3_THREAD_COUNT] = {0};
ThreadInput_s g_sTest3_ThreadInput[TEST_3_THREAD_COUNT];

pthread_t g_psTest4_Threads[TEST_4_THREAD_COUNT] = {0};
int g_piTest4_Results[TEST_4_THREAD_COUNT] = {0};
ThreadInput_s g_sTest4_ThreadInput[TEST_4_THREAD_COUNT];
UVFSFileNode* g_sTest4_ThreadNodes[TEST_4_THREAD_COUNT*TEST_4_FILES_PER_THREAD];
pthread_barrier_t sTest4_barrier;

pthread_t g_psTest5_Threads[TEST_5_THREAD_COUNT] = {0};
int g_piTest5_Results[TEST_5_THREAD_COUNT] = {0};
ThreadInput_s g_sTest5_ThreadInput[TEST_5_THREAD_COUNT];
atomic_uint_least64_t sTest5_FileToClone;

pthread_t g_psTest6_Threads[TEST_6_THREAD_COUNT] = {0};
int g_piTest6_Results[TEST_6_THREAD_COUNT] = {0};
ThreadInput_s g_sTest6_ThreadInput[TEST_6_THREAD_COUNT];
int g_sTest6_CurrentlyOpenFiles[TEST_6_THREAD_COUNT] = {0};
pthread_mutex_t g_sTest6_mutex;
bool g_bStopAllThreds = false;

pthread_t g_psTest7_Threads[TEST_7_THREAD_COUNT] = {0};
int g_piTest7_Results[TEST_7_THREAD_COUNT] = {0};
ThreadInput_s g_sTest7_ThreadInput[TEST_7_THREAD_COUNT];

#define CMP_TIMES(timspec1, timspec2)                    \
((timspec1.tv_sec == timspec2.tv_sec) \
&& (timspec1.tv_nsec == timspec2.tv_nsec))
u_char
l2u[256] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, /* 00-07 */
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, /* 08-0f */
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, /* 10-17 */
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, /* 18-1f */
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, /* 20-27 */
    0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, /* 28-2f */
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, /* 30-37 */
    0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, /* 38-3f */
    0x40, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, /* 40-47 */
    0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, /* 48-4f */
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, /* 50-57 */
    0x78, 0x79, 0x7a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, /* 58-5f */
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, /* 60-67 */
    0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, /* 68-6f */
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, /* 70-77 */
    0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, /* 78-7f */
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, /* 80-87 */
    0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, /* 88-8f */
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, /* 90-97 */
    0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, /* 98-9f */
    0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, /* a0-a7 */
    0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, /* a8-af */
    0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, /* b0-b7 */
    0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf, /* b8-bf */
    0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, /* c0-c7 */
    0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, /* c8-cf */
    0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xd7, /* d0-d7 */
    0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xdf, /* d8-df */
    0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, /* e0-e7 */
    0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, /* e8-ef */
    0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, /* f0-f7 */
    0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff, /* f8-ff */
};

static void
unistr255ToLowerCase( struct unistr255* psUnistr255 )
{
    for ( uint16_t uIdx=0; uIdx<psUnistr255->length; uIdx++ )
    {
        if ( psUnistr255->chars[uIdx] < 0x100 )
        {
            psUnistr255->chars[uIdx] = l2u[psUnistr255->chars[uIdx]];
        }
    }
}

__unused static long long int timestamp()
{
    /* Example of timestamp in second. */
    time_t timestamp_sec; /* timestamp in second */
    time(&timestamp_sec);  /* get current time; same as: timestamp_sec = time(NULL)  */

    /* Example of timestamp in microsecond. */
    struct timeval timer_usec;
    long long int timestamp_usec; /* timestamp in microsecond */
    if (!gettimeofday(&timer_usec, NULL)) {
        timestamp_usec = ((long long int) timer_usec.tv_sec) * 1000000ll +
        (long long int) timer_usec.tv_usec;
    }
    else {
        timestamp_usec = -1;
    }

    return timestamp_usec;
}

__unused static void print_dir_entry_name( uint32_t uLen, char* pcName, char* pcSearchName, bool* pbFound )
{
    struct unistr255 sU255;
    struct unistr255 sU2552;
    memset( &sU255, 0, sizeof(struct unistr255));
    memset( &sU2552, 0, sizeof(struct unistr255));
    errno_t status = CONV_UTF8ToUnistr255( (uint8_t*)pcName, strlen(pcName), &sU255 , UTF_SFM_CONVERSIONS);
    status |= CONV_UTF8ToUnistr255( (uint8_t*)pcSearchName, strlen(pcSearchName), &sU2552, UTF_SFM_CONVERSIONS);

    if ( status != 0 ) { assert(0); }

    printf( "TESTER Entry Name = [%s]\n", pcName );

    if ( pcSearchName )
    {
        unistr255ToLowerCase(&sU255);
        unistr255ToLowerCase(&sU2552);

        if ( (sU255.length == sU2552.length) && !memcmp(sU255.chars, sU2552.chars, sU255.length*2) )
        {
            *pbFound = true;
        }
    }
}

/* --------------------------------------------------------------------------------------------- */
static int GetFSAttr(UVFSFileNode Node);
static void* MultiThreadSingleFile(void *arg);
static void* MultiThreadMultiFiles(void *arg);
static int RemoveFile(UVFSFileNode ParentNode,char* FileNameToRemove);
static int SetAttrChangeSize(UVFSFileNode FileNode,uint64_t uNewSize);
static int RemoveFolder(UVFSFileNode ParentNode,char* DirNameToRemove);
static int GetAttrAndCompare(UVFSFileNode FileNode,UVFSFileAttributes* sInAttrs);
static int Lookup(UVFSFileNode ParentNode,UVFSFileNode *NewFileNode,char* FileName);
static int CreateNewFolder(UVFSFileNode ParentNode,UVFSFileNode* NewDirNode,char* NewDirName);
static int CreateLink(UVFSFileNode ParentNode,UVFSFileNode* NewLinkNode,char* LinkName,char* LinkContent);
static int CreateNewFile(UVFSFileNode ParentNode,UVFSFileNode* NewFileNode,char* NewFileName,uint64_t size);
static int Rename(UVFSFileNode fromDirNode, UVFSFileNode fromNode, const char *fromName, UVFSFileNode toDirNode, UVFSFileNode toNode, const char *toName);
static void read_directory_and_search_for_name( UVFSFileNode UVFSFileNode, char* pcSearchName, bool* pbFound);
static int ReadLinkToBuffer(UVFSFileNode LinkdNode,void* pvExternalBuffer, uint32_t uExternalBufferSize, size_t* actuallyRead);
static int ReadToBuffer(UVFSFileNode FileToReadNode, uint64_t uOffset, size_t* actuallyRead, void* pvExternalBuffer, size_t uExternalBufferSize);
static int WriteFromBuffer(UVFSFileNode FileToWriteNode, uint64_t uOffset, size_t* uActuallyWritten, void* pvExternalBuffer, size_t uExternalBufferSize);
static int CloseFile(UVFSFileNode UVFSFileNode);
/* --------------------------------------------------------------------------------------------- */

static int CloseFile(UVFSFileNode UVFSFileNode)
{

    int iErr = MSDOS_fsOps.fsops_sync(UVFSFileNode);

    if (iErr)
    {
        printf("Got Err: %d, while syncing\n",iErr);
        return iErr;
    }

    iErr = MSDOS_fsOps.fsops_reclaim(UVFSFileNode);
    if (iErr)
    {
        printf("Got Err: %d, while reclaiming\n",iErr);
    }

    return iErr;
}

static void read_directory_and_search_for_name( UVFSFileNode UVFSFileNode, char* pcSearchName, bool* pbFound )
{
    NodeRecord_s* psFolder = UVFSFileNode;

    uint32_t uBufferSize = 1000;
    uint8_t* puBuffer = malloc(uBufferSize*2);
    if ( puBuffer == NULL )
    {
        return;
    }
    memset(puBuffer, 0xff, uBufferSize*2);

    uint64_t uCookie = 0;
    uint64_t uVerifier = UVFS_DIRCOOKIE_VERIFIER_INITIAL;
    bool bConRead = true;

    uint32_t uDirsCounter   = 0;
    uint32_t uFilesCounter  = 0;
    uint32_t uLinksCounter  = 0;
    size_t   outLen = 0;

    int iReadDirERR = 0;
    do {

        uint32_t uBufCurOffset  = 0;

        memset(puBuffer, 0, uBufferSize);

        iReadDirERR = MSDOS_fsOps.fsops_readdir (psFolder, puBuffer, uBufferSize, uCookie, &outLen, &uVerifier);
        //        assert(0xffffffffffffffff == *(uint64_t*)(&puBuffer[100]));
        if ( (iReadDirERR != 0 && iReadDirERR != UVFS_READDIR_EOF_REACHED) || outLen==0)
        {
            bConRead = false;
        }
        else
        {
            //Go over all entries in the list and check if we got to the end of the directory
            bool bEndOfDirectoryList = false;

            while ( !bEndOfDirectoryList && iReadDirERR != UVFS_READDIR_EOF_REACHED )
            {
                UVFSDirEntry* psNewDirListEntry = (UVFSDirEntry*) &puBuffer[uBufCurOffset];
                uCookie = psNewDirListEntry->de_nextcookie;

                //We found all the files in the root directory
                if ( ( psNewDirListEntry->de_nextcookie == UVFS_DIRCOOKIE_EOF ) || ( psNewDirListEntry->de_reclen == 0 ) )
                {
                    bEndOfDirectoryList = true;
                }

                print_dir_entry_name( psNewDirListEntry->de_namelen, psNewDirListEntry->de_name, pcSearchName, pbFound );
                switch ( psNewDirListEntry->de_filetype )
                {
                    case UVFS_FA_TYPE_DIR:
                    {
                        printf("found dir: ID: %llu, in offset [%u] \n",psNewDirListEntry->de_fileid,uBufCurOffset);
                        uDirsCounter++;
                    }
                        break;
                    case UVFS_FA_TYPE_FILE:
                    {
                        printf("found file: ID: %llu, in offset [%u] \n",psNewDirListEntry->de_fileid,uBufCurOffset);
                        uFilesCounter++;
                    }
                        break;
                    case UVFS_FA_TYPE_SYMLINK:
                    {
                        printf("found link: ID: %llu, in offset [%u] \n",psNewDirListEntry->de_fileid,uBufCurOffset);
                        uLinksCounter++;
                    }
                        break;

                    default:
                        printf("Found Unkown file type %d, Exiting\n", psNewDirListEntry->de_filetype);
                        bEndOfDirectoryList = true;
                        bConRead = false;
                        break;
                }

                uBufCurOffset += UVFS_DIRENTRY_RECLEN(strlen(psNewDirListEntry->de_name));
            }
        }
    } while( bConRead && (iReadDirERR != UVFS_READDIR_EOF_REACHED) );

    //    printf("Amount of files %d \n", uFilesCounter);
    //    printf("Amount of dirs %d \n",  uDirsCounter);
    //    printf("Amount of links %d \n", uLinksCounter);

    if ( puBuffer ) free( puBuffer );
}

static void
ReadDirAttr( UVFSFileNode UVFSFileNode)
{
    NodeRecord_s* psFolder = UVFSFileNode;

    uint32_t uBufferSize = 1024*1024;
    uint8_t* puBuffer = malloc(uBufferSize);
    if ( puBuffer == NULL )
    {
        return;
    }

    uint64_t uCookie = 0;
    uint64_t uVerifier = UVFS_DIRCOOKIE_VERIFIER_INITIAL;
    bool bConRead = true;
    size_t   outLen = 0;
    int iReadDirERR = 0;
    do {

        uint32_t uBufCurOffset  = 0;

        memset(puBuffer, 0, uBufferSize);

        iReadDirERR = MSDOS_fsOps.fsops_readdirattr (psFolder, puBuffer, uBufferSize, uCookie, &outLen, &uVerifier);
        if ( (iReadDirERR != 0 && iReadDirERR != UVFS_READDIR_EOF_REACHED) || (outLen == 0) )
        {
            bConRead = false;
        }
        else
        {
            //Go over all entries in the list and check if we got to the end of the directory
            bool bEndOfDirectoryList = false;

            while ( !bEndOfDirectoryList && iReadDirERR != UVFS_READDIR_EOF_REACHED )
            {
                UVFSDirEntryAttr* psNewDirListEntry = (UVFSDirEntryAttr*) &puBuffer[uBufCurOffset];
                uCookie = psNewDirListEntry->dea_nextcookie;
                //We found all the files in the root directory
                if ( ( psNewDirListEntry->dea_nextcookie == UVFS_DIRCOOKIE_EOF ) || ( psNewDirListEntry->dea_nextrec == 0 ) )
                {
                    bEndOfDirectoryList = true;
                }

//                printf("FileName = [%s],  FileID = [%llu], FileSize = [%llu], type [%d].\n", UVFS_DIRENTRYATTR_NAMEPTR(psNewDirListEntry), psNewDirListEntry->dea_attrs.fa_fileid, psNewDirListEntry->dea_attrs.fa_size, psNewDirListEntry->dea_attrs.fa_type);

                uBufCurOffset += UVFS_DIRENTRYATTR_RECLEN(psNewDirListEntry, strlen(UVFS_DIRENTRYATTR_NAMEPTR(psNewDirListEntry)));
            }
        }
    } while( bConRead && (iReadDirERR != UVFS_READDIR_EOF_REACHED) );

    if ( puBuffer )
        free( puBuffer );
}

static int ScanDir(UVFSFileNode UVFSFolderNode, char** contains_str, char** end_with_str, struct timespec mTime)
{
    int err = 0;

    scandir_matching_request_t sMatchingCriteria;
    UVFSFileAttributes smr_attribute_filter;
    scandir_matching_reply_t sMatchingResult;
    void* pvAttrBuf = malloc(sizeof(UVFSDirEntryAttr) + FAT_MAX_FILENAME_UTF8*sizeof(char));

    memset(&sMatchingCriteria, 0, sizeof(sMatchingCriteria));
    memset(&smr_attribute_filter, 0, sizeof(smr_attribute_filter));
    memset(&sMatchingResult, 0, sizeof(sMatchingResult));
    sMatchingResult.smr_entry = pvAttrBuf;

    sMatchingCriteria.smr_filename_contains = (contains_str == NULL) ? NULL : contains_str;
    sMatchingCriteria.smr_filename_ends_with = (end_with_str == NULL) ? NULL : end_with_str;
    sMatchingCriteria.smr_attribute_filter = &smr_attribute_filter;

    if (mTime.tv_nsec != 0 || mTime.tv_sec != 0 )
    {
        sMatchingCriteria.smr_attribute_filter->fa_validmask |= UVFS_FA_VALID_MTIME;
        sMatchingCriteria.smr_attribute_filter->fa_mtime = mTime;
    }

    bool bConRead = true;

    do
    {
        err = MSDOS_fsOps.fsops_scandir (UVFSFolderNode, &sMatchingCriteria, &sMatchingResult);
        if ( err != 0 || ( sMatchingResult.smr_entry->dea_nextcookie == UVFS_DIRCOOKIE_EOF && sMatchingResult.smr_result_type == 0 ) )
        {
            bConRead = false;
        }
        else
        {
            if ( sMatchingResult.smr_entry->dea_nextcookie == UVFS_DIRCOOKIE_EOF )
            {
                bConRead = false;
            }
//            printf("SearchDir Returned with status %d, FileName = [%s], M-Time sec:[%ld] nsec:[%ld].\n", sMatchingResult.smr_result_type, UVFS_DIRENTRYATTR_NAMEPTR(sMatchingResult.smr_entry),sMatchingResult.smr_entry->dea_attrs.fa_mtime.tv_sec,sMatchingResult.smr_entry->dea_attrs.fa_mtime.tv_nsec);

            sMatchingCriteria.smr_start_cookie = sMatchingResult.smr_entry->dea_nextcookie;
            sMatchingCriteria.smr_verifier = sMatchingResult.smr_verifier;
        }

    }while (bConRead);

    free(pvAttrBuf);

    return err;
}

static char* gpcFSAttrs [] = {
    UVFS_FSATTR_PC_LINK_MAX,
    UVFS_FSATTR_PC_NAME_MAX,
    UVFS_FSATTR_PC_NO_TRUNC,
    UVFS_FSATTR_PC_FILESIZEBITS,
    UVFS_FSATTR_PC_XATTR_SIZE_BITS,
    UVFS_FSATTR_BLOCKSIZE,
    UVFS_FSATTR_IOSIZE,
    UVFS_FSATTR_TOTALBLOCKS,
    UVFS_FSATTR_BLOCKSFREE,
    UVFS_FSATTR_BLOCKSAVAIL,
    UVFS_FSATTR_BLOCKSUSED,
    UVFS_FSATTR_CNAME,
    UVFS_FSATTR_FSTYPENAME,
    UVFS_FSATTR_FSSUBTYPE,
    UVFS_FSATTR_VOLNAME,
    UVFS_FSATTR_VOLUUID,
    UVFS_FSATTR_CAPS_FORMAT,
    UVFS_FSATTR_CAPS_INTERFACES
};

#define ARR_LEN(arr) ((sizeof(arr))/(sizeof(arr[0])))

static int
GetFSAttr( UVFSFileNode Node )
{
    int iErr        = 0;
    size_t uLen     = 512;
    size_t uRetLen  = 0;
    UVFSFSAttributeValue* psAttrVal = (UVFSFSAttributeValue*)malloc(uLen);
    assert( psAttrVal );

    for ( uint32_t uIdx=0; uIdx<ARR_LEN(gpcFSAttrs); uIdx++ )
    {
        memset( psAttrVal, 0, uLen );

        iErr = MSDOS_fsOps.fsops_getfsattr( Node, gpcFSAttrs[uIdx], psAttrVal, uLen, &uRetLen );
        if ( iErr != 0 )
        {
            printf( "fsops_getfsattr attr = %s return with error code [%d]\n", gpcFSAttrs[uIdx], iErr );
            continue;
        }

        printf( "FSAttr = [%s] Value = [", gpcFSAttrs[uIdx]);
        if ( UVFS_FSATTR_IS_BOOL( gpcFSAttrs[uIdx] ) )
        {
            printf( psAttrVal->fsa_bool? "true" : "false" );
        }
        else if ( UVFS_FSATTR_IS_NUMBER( gpcFSAttrs[uIdx] ) )
        {
            printf( "%llu", psAttrVal->fsa_number );
        }
        else if ( UVFS_FSATTR_IS_OPAQUE( gpcFSAttrs[uIdx] ) )
        {
            printf("0x");
            for ( uint32_t uOp=0; uOp < uRetLen; uOp++ )
            {
                printf( "%x", ((uint8_t*)(psAttrVal->fsa_opaque))[uOp] );
            }
        }
        else if ( UVFS_FSATTR_IS_STRING( gpcFSAttrs[uIdx] ) )
        {
            printf( "%s", psAttrVal->fsa_string );
        }
        else
        {
            assert(0);
        }
        printf("].\n");
    }

    free(psAttrVal);
    return (iErr);
}

static int Rename(UVFSFileNode fromDirNode, UVFSFileNode fromNode, const char *fromName, UVFSFileNode toDirNode, UVFSFileNode toNode, const char *toName)
{
    int error =0;
    error = MSDOS_fsOps.fsops_rename(fromDirNode, fromNode, fromName, toDirNode, toNode, toName, 0);
    return error;
}

static int CreateNewFolder(UVFSFileNode ParentNode,UVFSFileNode* NewDirNode,char* NewDirName)
{
    int error =0;

    UVFSFileAttributes attrs;
    memset(&attrs,0,sizeof(UVFSFileAttributes));
    attrs.fa_type = UVFS_FA_TYPE_DIR;

    error = MSDOS_fsOps.fsops_mkdir(ParentNode, NewDirName, &attrs, NewDirNode);

    return error;
}

static int RemoveFolder(UVFSFileNode ParentNode,char* DirNameToRemove)
{
    int error =0;

    error = MSDOS_fsOps.fsops_rmdir(ParentNode, DirNameToRemove);;

    return error;
}

static int CreateNewFile(UVFSFileNode ParentNode, UVFSFileNode* NewFileNode, char* NewFileName, uint64_t size)
{
    int error =0;
    UVFSFileAttributes attrs;

    memset(&attrs, 0, sizeof(attrs));

    attrs.fa_validmask = UVFS_FA_VALID_MODE | UVFS_FA_VALID_SIZE;

    attrs.fa_mode = UVFS_FA_MODE_RWX;
    attrs.fa_size = size;

    error = MSDOS_fsOps.fsops_create(ParentNode, NewFileName, &attrs, NewFileNode);

    return error;
}

static int RemoveFile(UVFSFileNode ParentNode,char* FileNameToRemove)
{
    int error =0;

    error = MSDOS_fsOps.fsops_remove(ParentNode, FileNameToRemove, NULL);

    return error;
}

static int Lookup(UVFSFileNode ParentNode,UVFSFileNode* NewFileNode,char* FileName)
{
    int error =0;
    error = MSDOS_fsOps.fsops_lookup(ParentNode, FileName, NewFileNode);
    return error;
}

static int GetAttrAndCompare(UVFSFileNode FileNode,UVFSFileAttributes* sInAttrs)
{
    int error =0;
    UVFSFileAttributes sOutAttrs;
    error = MSDOS_fsOps.fsops_getattr(FileNode, &sOutAttrs);
    if (error)
    {
        printf("Failed in get attr with err [%d]\n",error);
        return error;
    }
    if (sInAttrs->fa_validmask & UVFS_FA_VALID_SIZE)
        if (sOutAttrs.fa_size != sInAttrs->fa_size)
            error = 1;

    if (sInAttrs->fa_validmask & UVFS_FA_VALID_MODE)
        if (sOutAttrs.fa_mode != sInAttrs->fa_mode)
            error = 1;

    if (sInAttrs->fa_validmask & UVFS_FA_VALID_ATIME)
        if (CMP_TIMES(sOutAttrs.fa_atime,sInAttrs->fa_atime))
            error = 1;

    // Since there isn't an actual ctime in msdos
    // Check that mtime and ctime have the same value
    if (sInAttrs->fa_validmask & UVFS_FA_VALID_MTIME || sInAttrs->fa_validmask & UVFS_FA_VALID_CTIME )
        if (CMP_TIMES(sOutAttrs.fa_mtime,sInAttrs->fa_mtime) || CMP_TIMES(sOutAttrs.fa_ctime,sInAttrs->fa_mtime))
            error = 1;

    if (sInAttrs->fa_validmask & UVFS_FA_VALID_BIRTHTIME)
        if (CMP_TIMES(sOutAttrs.fa_birthtime,sInAttrs->fa_birthtime))
            error = 1;

    if (error) printf("Failed in compare attr\n");
    return error;
}

static int SetAttrChangeSize(UVFSFileNode FileNode,uint64_t uNewSize)
{
    int error =0;
    UVFSFileAttributes sInAttrs;
    UVFSFileAttributes sOutAttrs;
    memset(&sInAttrs,0,sizeof(UVFSFileAttributes));
    sInAttrs.fa_validmask |= UVFS_FA_VALID_SIZE;
    sInAttrs.fa_size = uNewSize;

    error =  MSDOS_fsOps.fsops_setattr( FileNode, &sInAttrs , &sOutAttrs );
    if (error)
        return error;

    error = GetAttrAndCompare(FileNode,&sInAttrs);

    return error;
}

static int ReadToBuffer(UVFSFileNode FileToReadNode, uint64_t uOffset, size_t* actuallyRead, void* pvExternalBuffer, size_t uExternalBufferSize)
{
    int error =0;
    long long int lliTotalTime = 0;

    if (pvExternalBuffer != NULL)
    {
        long long int lliReadStartTime = timestamp();
        error = MSDOS_fsOps.fsops_read(FileToReadNode, uOffset, uExternalBufferSize, pvExternalBuffer, actuallyRead);
        if (error) return error;
        if (*actuallyRead < uExternalBufferSize) {
            printf("Failed to read all data into device, Error [%d], offset [%llu]\n", ENOSPC, uOffset);
            return EIO;
        }
        long long int lliReadEndTime = timestamp();
        lliTotalTime += lliReadEndTime - lliReadStartTime;
    }
    else
    {
        printf("Failed to allocate read buffer\n");
        error = ENOMEM;
    }

//    printf("Read total_time %lld [usec], total size: %zu [Bytes]\n", lliTotalTime, *actuallyRead);

    return error;
}

static int ReadLinkToBuffer(UVFSFileNode LinkdNode,void* pvExternalBuffer, uint32_t uExternalBufferSize,size_t* actuallyRead)
{
    int error =0;

    uint32_t uBufferSize = 4*1024;
    void* pvLinkBuffer = malloc(uBufferSize);

    UVFSFileAttributes sOutAttrs;

    if (pvLinkBuffer != NULL)
    {
        error = MSDOS_fsOps.fsops_readlink(LinkdNode,pvLinkBuffer,uBufferSize,actuallyRead,&sOutAttrs);

        //If need to write into external buffer and there is still space
        if (pvExternalBuffer != NULL)
        {
            size_t uSizeToCopy = *actuallyRead;
            if ( *actuallyRead > uExternalBufferSize)
            {
                uSizeToCopy = uExternalBufferSize;
            }
            memcpy(pvExternalBuffer,pvLinkBuffer,uSizeToCopy);
        }

        free(pvLinkBuffer);
    }
    else
    {
        printf("Failed to allocate link buffer\n");
        error = ENOMEM;
    }

    return error;
}

static int CreateLink(UVFSFileNode ParentNode,UVFSFileNode* NewLinkNode,char* LinkName,char* LinkContent)
{
    int error =0;
    UVFSFileAttributes attrs;

    memset(&attrs, 0, sizeof(attrs));

    attrs.fa_validmask = UVFS_FA_VALID_MODE;
    attrs.fa_mode = UVFS_FA_MODE_RWX;

    error = MSDOS_fsOps.fsops_symlink(ParentNode, LinkName, LinkContent, &attrs, NewLinkNode);

    return error;
}

static int WriteFromBuffer(UVFSFileNode FileToWriteNode,uint64_t uOffset, size_t* uActuallyWritten,void* pvExternalBuffer, size_t uExternalBufferSize)
{
    int iError =0;
    long long int lliTotalTime = 0;
    *uActuallyWritten = 0;

    if (pvExternalBuffer!= NULL)
    {
        long long int lliWriteStartTime = timestamp();

        iError = MSDOS_fsOps.fsops_write(FileToWriteNode,uOffset, uExternalBufferSize, pvExternalBuffer, uActuallyWritten);

        long long int lliWriteEndTime = timestamp();
        lliTotalTime += lliWriteEndTime - lliWriteStartTime;

        if (*uActuallyWritten < uExternalBufferSize)
        {
            printf("Failed to write all data into device, Error [%d], offset [%llu]\n", ENOSPC, uOffset);
            return EIO;
        }
    }
    else
    {
        printf("Failed to allocate read buffer\n");
        iError = ENOMEM;
    }

//    printf("Write total_time %lld [usec], total size: %zu [Bytes]\n", lliTotalTime, *uActuallyWritten);

    return iError;
}

static void*
MultiThreadSingleFile(void *arg)
{

    int iError = 0;
    ThreadInput_s* psThreadInput = (ThreadInput_s*) arg;

    uint32_t uChunkSize = psThreadInput->uFileSize / psThreadInput->uTotalNumberOfThreads;
    uint32_t uStartOffset = psThreadInput->uThreadId*uChunkSize;

    int64_t* pvBufRead = malloc(psThreadInput->uBufferSize);
    if (pvBufRead == NULL) return (void*)ENOMEM;
    memset(pvBufRead, 0, psThreadInput->uBufferSize);
    
    int64_t* pvBuf = psThreadInput->general_buff + uStartOffset;
    for ( int64_t uIdx=0; uIdx < (uChunkSize/sizeof(int64_t)); uIdx++)
    {
        (pvBuf)[uIdx] = uStartOffset + uIdx;
    }

    pthread_barrier_wait(psThreadInput->barrier);
    
    LIFilePreallocateArgs_t pre_alloc_req = {.flags = F_ALLOCATEALL, .length = psThreadInput->uFileSize };
    fstore_t pre_alloc_res = {0};
    
    MSDOS_fsOps.fsops_setfsattr(*psThreadInput->ThreadInputNode.ppvFileToWriteNode, LI_FSATTR_PREALLOCATE, (LIFSAttributeValue* )((void*) &pre_alloc_req), sizeof(LIFilePreallocateArgs_t), (LIFSAttributeValue* )((void*)&pre_alloc_res), sizeof(fstore_t));
    
    pthread_barrier_wait(psThreadInput->barrier);
    
    size_t uActuallyWritten;
    uint64_t size_to_write =  1+ rand() % psThreadInput->uBufferSize;
    uint64_t offset = atomic_fetch_add(psThreadInput->general_offset, size_to_write);
    while ( offset < psThreadInput->uFileSize) {
        if (psThreadInput->uFileSize - offset < psThreadInput->uBufferSize) {
            size_to_write = psThreadInput->uFileSize - offset;
        }
        
        iError = WriteFromBuffer(*psThreadInput->ThreadInputNode.ppvFileToWriteNode, offset, &uActuallyWritten, psThreadInput->general_buff  + offset, size_to_write);
        if (iError) goto exit;
        
        size_to_write =  1+ rand() % psThreadInput->uBufferSize;
        offset = atomic_fetch_add(psThreadInput->general_offset, size_to_write);
    }

    pthread_barrier_wait(psThreadInput->barrier);
    *psThreadInput->general_offset = 0;
    pthread_barrier_wait(psThreadInput->barrier);
    
    size_t uActuallyRead;
    uint64_t size_to_read = 1+ rand() %  psThreadInput->uBufferSize;
    offset = atomic_fetch_add(psThreadInput->general_offset, size_to_read);
    while ( offset < psThreadInput->uFileSize) {
        if (psThreadInput->uFileSize - offset < psThreadInput->uBufferSize) {
            size_to_read = psThreadInput->uFileSize - offset;
        }
        
        iError =  ReadToBuffer(*psThreadInput->ThreadInputNode.ppvFileToWriteNode, offset, &uActuallyRead, pvBufRead, size_to_read);
        if (iError) goto exit;
        
        //Compare read to write
        pvBuf = psThreadInput->general_buff + offset;
        for ( uint32_t uIdx=0; uIdx < size_to_read; uIdx++)
        {
            if (((char*)(pvBuf))[uIdx] != ((char*)(pvBufRead))[uIdx])
            {
                printf("Failed in compare read to write. Index [%d]\n",uIdx);
                iError = 1;
                break;
            }
        }

        size_to_read = 1+ rand() %  psThreadInput->uBufferSize;
        offset = atomic_fetch_add(psThreadInput->general_offset, size_to_read);
    }

exit:
    free(pvBufRead);

    return (void*) (size_t) iError;
}

static void*
MultiThreadSingleFileHalfClusterSize(void *arg)
{
    int iError = 0;
    ThreadInput_s* psThreadInput = (ThreadInput_s*) arg;
    
    uint32_t uChunkSize = psThreadInput->uBufferSize;
    void* pvBufRead = malloc(uChunkSize);
    if (pvBufRead == NULL) return (void*)ENOMEM;
    memset(pvBufRead, 0, uChunkSize);
    
    void* pvBuf = malloc(uChunkSize);
    if (pvBuf == NULL) {
        free(pvBufRead);
        return (void*)ENOMEM;
    }
    memset(pvBuf, 5, uChunkSize);
    
    uint64_t uOffset = atomic_fetch_add(psThreadInput->general_offset, uChunkSize);
    uint64_t size_to_write = uChunkSize;
    size_t uActuallyWritten = 0;
    while ( uOffset < psThreadInput->uFileSize) {
        if (psThreadInput->uFileSize - uOffset < uChunkSize) {
            size_to_write = psThreadInput->uFileSize - uOffset;
        }
        
        iError = WriteFromBuffer(*psThreadInput->ThreadInputNode.ppvFileToWriteNode, uOffset, &uActuallyWritten, pvBuf, size_to_write);
        if (iError) {
            printf("Failed to write. [%d]\n",iError);
            goto exit;
        }
        
        uOffset = atomic_fetch_add(psThreadInput->general_offset, uChunkSize);
    }

    pthread_barrier_wait(psThreadInput->barrier);
    *psThreadInput->general_offset = 0;
    pthread_barrier_wait(psThreadInput->barrier);

    size_t uActuallyRead;
    uint64_t size_to_read = uChunkSize;
    uOffset = atomic_fetch_add(psThreadInput->general_offset, uChunkSize);
    while ( uOffset < psThreadInput->uFileSize) {
        if (psThreadInput->uFileSize - uOffset < uChunkSize) {
            size_to_read = psThreadInput->uFileSize - uOffset;
        }
        
        iError =  ReadToBuffer(*psThreadInput->ThreadInputNode.ppvFileToWriteNode, uOffset, &uActuallyRead, pvBufRead, size_to_read);
        if (iError) {
            printf("Failed to read. [%d]\n",iError);
            goto exit;
        }
        
        //Compare read to write
        for ( uint32_t uIdx=0; uIdx<size_to_read/16; uIdx++)
        {
            if (((int64_t*)(pvBuf))[uIdx] != ((int64_t*)(pvBufRead))[uIdx])
            {
                printf("Failed in compare read to write. Index [%d]\n",uIdx);
                iError = 1;
                break;
            }
        }

        uOffset = atomic_fetch_add(psThreadInput->general_offset, uChunkSize);
    }

exit:
    free(pvBufRead);
    free(pvBuf);
    return (void*) (size_t) iError;
}

static void*
MultiThreadMultiFiles(void *arg)
{
    int iError = 0;
    ThreadInput_s* psThreadInput = (ThreadInput_s*) arg;

    char pcName[10] = {0};
    sprintf(pcName, "file%d", psThreadInput->uThreadId);
    UVFSFileNode pvFileNode= NULL;
    iError = CreateNewFile(*psThreadInput->ThreadInputNode.ppvParentFolder,&pvFileNode,pcName,0);
    if (iError) return (void *) (size_t) iError;

    uint32_t uChunkSize = psThreadInput->uBufferSize;
    uint32_t uStartOffset = 0;

    void* pvBuf = malloc(uChunkSize);
    if (pvBuf == NULL) return (void*)ENOMEM;

    void* pvBufRead = malloc(uChunkSize);
    if (pvBufRead == NULL) {
        free(pvBuf);
        return (void*)ENOMEM;
    }

    for ( int64_t uIdx=0; uIdx < (uChunkSize/sizeof(int64_t)); uIdx++)
    {
        ((int64_t*)pvBuf)[uIdx] = uStartOffset + uIdx;
        ((int64_t*)pvBufRead)[uIdx] = 0;
    }

    LIFilePreallocateArgs_t pre_alloc_req = {.flags = F_ALLOCATEALL, .length = psThreadInput->uFileSize};
    fstore_t pre_alloc_res = {0};
    
    MSDOS_fsOps.fsops_setfsattr(pvFileNode, LI_FSATTR_PREALLOCATE, (LIFSAttributeValue* )((void*) &pre_alloc_req), sizeof(LIFilePreallocateArgs_t), (LIFSAttributeValue* )((void*)&pre_alloc_res), sizeof(fstore_t));
    
    size_t uActuallyWritten;
    uint64_t size_to_write = psThreadInput->uBufferSize;
    uint64_t offset = atomic_fetch_add(&sTest2_ThreadOffset[psThreadInput->uThreadId], size_to_write);
    while ( offset < psThreadInput->uFileSize) {
        if (psThreadInput->uFileSize - offset < psThreadInput->uBufferSize) {
            size_to_write = psThreadInput->uFileSize - offset;
        }
        
        iError = WriteFromBuffer(pvFileNode, offset, &uActuallyWritten, pvBuf, size_to_write);
        if (iError)
            goto exit;
        offset = atomic_fetch_add(&sTest2_ThreadOffset[psThreadInput->uThreadId], size_to_write);
    }
    
    size_t uActuallyRead;
    sTest2_ThreadOffset[psThreadInput->uThreadId] = 0;
    uint64_t size_to_read = psThreadInput->uBufferSize;
    offset = atomic_fetch_add(&sTest2_ThreadOffset[psThreadInput->uThreadId], size_to_read);
    while ( offset < psThreadInput->uFileSize) {
        if (psThreadInput->uFileSize - offset < psThreadInput->uBufferSize) {
            size_to_read = psThreadInput->uFileSize - offset;
        }
        
        iError = ReadToBuffer(pvFileNode, offset, &uActuallyRead, pvBufRead, size_to_read);
        if (iError)
            goto exit;
        
        for ( uint32_t uIdx=0; uIdx<uChunkSize/16; uIdx++)
        {
            if ( ((int64_t*)pvBuf)[uIdx] != ((int64_t*)pvBufRead)[uIdx] )
            {
                printf("Failed in comare read to write. Index [%d]\n",uIdx);
                iError = 1;
                break;
            }
        }
        
        offset = atomic_fetch_add(&sTest2_ThreadOffset[psThreadInput->uThreadId], size_to_read);
    }
    
    if (iError) goto exit;

    iError = CloseFile(pvFileNode);
    if (iError) goto exit;

    // Remove
    iError =  RemoveFile(*psThreadInput->ThreadInputNode.ppvParentFolder, pcName);

exit:
    free(pvBuf);
    free(pvBufRead);
    return (void *) (size_t) iError;
}

static void*
MultiThreadMultiFilesMultiThread(void *arg)
{
    int iError = 0;
    ThreadInput_s* psThreadInput = (ThreadInput_s*) arg;

    char pcName[10] = {0};
    sprintf(pcName, "file%d", psThreadInput->uThreadId);
    UVFSFileNode pvFileNode= NULL;
    iError = CreateNewFile(*psThreadInput->ThreadInputNode.ppvParentFolder,&pvFileNode,pcName,0);
    if (iError) return (void *) (size_t) iError;
    
    void* sTest3_pvBuf = malloc(psThreadInput->uFileSize);
    if (sTest3_pvBuf == NULL) return (void*)ENOMEM;
    
    atomic_uint_least64_t sTest3_ThreadOffset = 0;
    pthread_barrier_t sTest3_barrier;
    
    iError = pthread_barrier_init(&sTest3_barrier, NULL, TEST_1_THREAD_COUNT);
    if (iError) goto exit;
    
    ThreadInput_s sTest3_ThreadInput[TEST_1_THREAD_COUNT];
    pthread_t psTest3_Threads[TEST_1_THREAD_COUNT] = {0};
    int piTest3_Results[TEST_1_THREAD_COUNT] = {0};
    
    for ( uint64_t uIdx=0; uIdx<TEST_1_THREAD_COUNT; uIdx++ )
    {
        memset(&sTest3_ThreadInput[uIdx],0,sizeof(ThreadInput_s));
        sTest3_ThreadInput[uIdx].uBufferSize = 1024*1024;
        sTest3_ThreadInput[uIdx].uFileSize = 30*1024*1024;
        sTest3_ThreadInput[uIdx].uTotalNumberOfThreads = TEST_1_THREAD_COUNT;
        sTest3_ThreadInput[uIdx].uThreadId = (uint32_t) uIdx;
        sTest3_ThreadInput[uIdx].ThreadInputNode.ppvFileToWriteNode = &pvFileNode;
        
        sTest3_ThreadInput[uIdx].barrier = &sTest3_barrier;
        sTest3_ThreadInput[uIdx].general_offset = &sTest3_ThreadOffset;
        sTest3_ThreadInput[uIdx].general_buff = sTest3_pvBuf;
        
        pthread_create( &psTest3_Threads[uIdx], NULL, (void*)MultiThreadSingleFile, (void*)&sTest3_ThreadInput[uIdx]);
    }

    for ( uint32_t uIdx=0; uIdx<TEST_1_THREAD_COUNT; uIdx++ )
    {
        pthread_join( psTest3_Threads[uIdx], (void*) &piTest1_Results[uIdx] );

        if (piTest3_Results[uIdx])
        {
            iError = piTest3_Results[uIdx];
            printf("Thread [%d] Failed in Test 1 with error [%d]\n", uIdx, iError);
        }
    }
    
    pthread_barrier_destroy(&sTest3_barrier);
    
    iError = CloseFile(pvFileNode);
    if (iError) goto exit;

    // Remove
    iError =  RemoveFile(*psThreadInput->ThreadInputNode.ppvParentFolder, pcName);
    
exit:
    free(sTest3_pvBuf);
    return (void *) (size_t) iError;
}

#ifndef DIAGNOSTIC
static void*
test4_CreateFiles(void *arg) {
    int iError = 0;
    ThreadInput_s* psThreadInput = (ThreadInput_s*) arg;
    
    int iFirstFileName = psThreadInput->uThreadId*TEST_4_FILES_PER_THREAD;
    int iLastFileName = psThreadInput->uThreadId*TEST_4_FILES_PER_THREAD + TEST_4_FILES_PER_THREAD;
    
    for (int i= 0; i< 10; i++) {
        
        for (int idx = iFirstFileName; idx < iLastFileName; idx++) {
            
            char pcName[11] = {0};
            sprintf(pcName, "T4_file%d", idx);
            UVFSFileNode pvFileNode= NULL;
            iError = CreateNewFile(*psThreadInput->ThreadInputNode.ppvParentFolder, &pvFileNode, pcName, 0);
            if (iError)
                goto exit;
            
            void* fileBuffer = malloc(1024);
            if (fileBuffer == NULL) {
                iError = ENOMEM;
                break;
            }
            memset(fileBuffer, 5, 1024);

            size_t uActuallyWrriten;
            WriteFromBuffer(pvFileNode, 0, &uActuallyWrriten, fileBuffer, 1024);
            free(fileBuffer);

            iError = CloseFile(pvFileNode);
            if (iError)
                goto exit;
        }
        
        pthread_barrier_wait(&sTest4_barrier);
        
        for (int idx = iFirstFileName; idx < iLastFileName; idx++) {
            char pcName[11] = {0};
            sprintf(pcName, "T4_file%d", idx);
            // Remove
            iError =  RemoveFile(*psThreadInput->ThreadInputNode.ppvParentFolder, pcName);
            if (iError)
                goto exit;
        }
        
        pthread_barrier_wait(&sTest4_barrier);
    }
    
exit:
    return (void *) (size_t) iError;
}

static void*
test4_LookupFiles(void *arg) {
    int iError = 0;
    ThreadInput_s* psThreadInput = (ThreadInput_s*) arg;
    
    for (int i= 0; i< 10; i++) {
        pthread_barrier_wait(&sTest4_barrier);
        for (int j= 0; j< 20; j++) {
            char pcName[11] = {0};
            int fileNumToLook = rand() % (psThreadInput->uTotalNumberOfThreads * TEST_4_FILES_PER_THREAD);
            
            if (g_sTest4_ThreadNodes[fileNumToLook] == NULL)
            sprintf(pcName, "T4_file%d", fileNumToLook);
            UVFSFileNode pvFileNode= NULL;
            Lookup(*psThreadInput->ThreadInputNode.ppvParentFolder, &pvFileNode, pcName);

            if (pvFileNode != NULL) {
                UVFSFileAttributes sOrgAttrs;
                iError = MSDOS_fsOps.fsops_getattr(pvFileNode, &sOrgAttrs);
                if (iError) break;

                void* fileBuffer = malloc(sOrgAttrs.fa_size);
                if (fileBuffer == NULL) {
                    iError = ENOMEM;
                    break;
                }
                memset(fileBuffer, 5, sOrgAttrs.fa_size);

                size_t uActuallyWrriten;
                ReadToBuffer(pvFileNode, 0, &uActuallyWrriten, fileBuffer, sOrgAttrs.fa_size);
                free(fileBuffer);

                iError = CloseFile(pvFileNode);
                if (iError) goto exit;
            }
        }
        pthread_barrier_wait(&sTest4_barrier);
    }
    
exit:
    return (void *) (size_t) iError;
}
#endif

static void*
test5_cloneFilesInDir(void *arg)
{
    int iError = 0;
    ThreadInput_s* psThreadInput = (ThreadInput_s*) arg;
    
    uint64_t fileNum = atomic_fetch_add(&sTest5_FileToClone, 1);
    while (fileNum < TEST_5_NUM_OF_FILES) {
        UVFSFileNode orgNode = NULL;
        UVFSFileNode newNode = NULL;
        UVFSFileAttributes sOrgAttrs;
        char pcName[100] = {0};
        sprintf(pcName, TEST_5_FILE_NAME, fileNum);

        iError = Lookup(*psThreadInput->ThreadInputNode.ppvParentFolder, &orgNode, pcName);
        if (iError) break;

        iError = MSDOS_fsOps.fsops_getattr(orgNode, &sOrgAttrs);
        if (iError) break;

        void* fileBuffer = malloc(sOrgAttrs.fa_size);
        if (fileBuffer == NULL) {
            iError = ENOMEM;
            break;
        }

        size_t uActuallyRead;
        iError =  ReadToBuffer(orgNode, 0, &uActuallyRead, fileBuffer, sOrgAttrs.fa_size);
        CloseFile(orgNode);
        if (iError) break;

        iError = CreateNewFile(*psThreadInput->ThreadTargetNode.ppvToDirNode, &newNode, pcName, 0);
        if (iError) break;

        LIFilePreallocateArgs_t pre_alloc_req = {.flags = F_ALLOCATECONTIG | F_ALLOCATEALL, .length = sOrgAttrs.fa_size};
        fstore_t pre_alloc_res = {0};

        iError = MSDOS_fsOps.fsops_setfsattr(newNode, LI_FSATTR_PREALLOCATE, (LIFSAttributeValue* )((void*) &pre_alloc_req), sizeof(LIFilePreallocateArgs_t), (LIFSAttributeValue* )((void*)&pre_alloc_res), sizeof(fstore_t));
        if (iError) break;

        size_t uActuallyWritten;
        iError = WriteFromBuffer(newNode, 0, &uActuallyWritten, fileBuffer, sOrgAttrs.fa_size);

        CloseFile(newNode);
        if (iError) break;

        fileNum = atomic_fetch_add(&sTest5_FileToClone, 1);
    }

    return (void *) (size_t) iError;
}

static void getRandFileToWorkOn(int threadNum) {
    pthread_mutex_lock(&g_sTest6_mutex);
    while (true) {
        int fileToWorkOn = 1 + rand() % TEST_6_NUM_OF_FILES;
        for (int i = 0; i < TEST_6_THREAD_COUNT; i++) {
            if (g_sTest6_CurrentlyOpenFiles[i] == fileToWorkOn) {
                fileToWorkOn = TEST_6_NUM_OF_FILES;
                break;
            }
        }
        if (fileToWorkOn != TEST_6_NUM_OF_FILES) {
            g_sTest6_CurrentlyOpenFiles[threadNum] = fileToWorkOn;
            break;
        }
    }
    pthread_mutex_unlock(&g_sTest6_mutex);
}

static void*
test6_opsOnDirFiles(void *arg)
{
    enum {
        WRITE_TO_FILE = 0,
        READ_FROM_FILE,
        SET_ATTR,
        PRE_ALLOC,
        EXPAND_DB,
        EXPAND_DB2,
        EXPAND_DB3,
        NUM_OF_ACTIONS
    };
    
    int iError = 0;
    ThreadInput_s* psThreadInput = (ThreadInput_s*) arg;
    char dbInput[100] = {0};
    UVFSFileNode newNode = NULL;
    
    while (!g_bStopAllThreds) {
        char pcName[100] = {0};
        getRandFileToWorkOn(psThreadInput->uThreadId);
        int fileToWorkOn = g_sTest6_CurrentlyOpenFiles[psThreadInput->uThreadId];
        sprintf(pcName, TEST_6_FILE_NAME, fileToWorkOn);

        iError = Lookup(*psThreadInput->ThreadInputNode.ppvParentFolder, &newNode, pcName);
        if (iError) {
            printf("Lookup for file %s in test 6 failed with error [%d]\n", pcName, iError);
            break;
        }
        
        UVFSFileNode dbNode = *psThreadInput->ThreadTargetNode.ppvToDirNode;
        UVFSFileAttributes sDBAttrs;
        iError = MSDOS_fsOps.fsops_getattr(dbNode, &sDBAttrs);
        if (iError) break;
        
        UVFSFileAttributes sOutAttrs;
        iError = MSDOS_fsOps.fsops_getattr(newNode, &sOutAttrs);
        if (iError) break;
        
        int randOP = rand() % NUM_OF_ACTIONS;
        switch (randOP) {
            case WRITE_TO_FILE: //Write - write 1K to random offset of the file
                {
                    uint64_t offset = rand() % sOutAttrs.fa_allocsize;
                    uint64_t sizeToWrite = 1024;

                    void* fileBuffer = malloc(sizeToWrite);
                    if (fileBuffer == NULL) {
                        iError = ENOMEM;
                        break;
                    }
                    memset(fileBuffer, 5, sizeToWrite);

                    size_t uActuallyWrriten;
                    iError =  WriteFromBuffer(newNode, offset, &uActuallyWrriten, fileBuffer, sizeToWrite);
                    free(fileBuffer);
                }
                break;
            case READ_FROM_FILE: //Read  - read 1K from random offset of the file
                {
                    size_t uActuallyRead;
                    uint64_t offset = rand() % (sOutAttrs.fa_size - 512);
                    uint64_t sizeToRead = (sOutAttrs.fa_size - offset) % 1024;

                    void* fileBuffer = malloc(sizeToRead);
                    if (fileBuffer == NULL) {
                        iError = ENOMEM;
                        break;
                    }

                    iError =  ReadToBuffer(newNode, offset, &uActuallyRead, fileBuffer, sizeToRead);
                    free(fileBuffer);
                }
                break;
            case SET_ATTR: //SetAttr - change Size to +512
                {
                    uint32_t uSpaceToAlloc = 512 + (rand() % 512);
                    iError = SetAttrChangeSize(newNode, sOutAttrs.fa_size + uSpaceToAlloc);
                }
                break;
            case PRE_ALLOC: //pre-allocate +512
                {
                    uint32_t uSpaceToAlloc = 512 + (rand() % 512);
                    LIFilePreallocateArgs_t pre_alloc_req = {.flags = F_ALLOCATEALL, .length = uSpaceToAlloc };
                    fstore_t pre_alloc_res = {0};
                    
                    iError = MSDOS_fsOps.fsops_setfsattr(newNode, LI_FSATTR_PREALLOCATE, (LIFSAttributeValue* )((void*) &pre_alloc_req), sizeof(LIFilePreallocateArgs_t), (LIFSAttributeValue* )((void*)&pre_alloc_res), sizeof(fstore_t));
                }
                break;
            case EXPAND_DB: //expand the DB +512
            case EXPAND_DB2:
            case EXPAND_DB3:
                {
                    uint32_t uSpaceToAlloc = 512 + (rand() % 512);
                    LIFilePreallocateArgs_t pre_alloc_req = {.flags = F_ALLOCATEALL, .length = uSpaceToAlloc };
                    fstore_t pre_alloc_res = {0};
                    
                    iError = MSDOS_fsOps.fsops_setfsattr(dbNode, LI_FSATTR_PREALLOCATE, (LIFSAttributeValue* )((void*) &pre_alloc_req), sizeof(LIFilePreallocateArgs_t), (LIFSAttributeValue* )((void*)&pre_alloc_res), sizeof(fstore_t));
                }
                break;
            default: //do nothing
                break;
        }

        CloseFile(newNode);
        g_sTest6_CurrentlyOpenFiles[psThreadInput->uThreadId] = 0;
        if (iError) break;

        sprintf(dbInput, TEST_6_DB_LINE, randOP, fileToWorkOn);
        uint64_t offsetInDB = rand() % sDBAttrs.fa_allocsize;
        uint64_t sizeToWriteToDB = strlen(dbInput);
        size_t uActuallyWrriten;
        iError =  WriteFromBuffer(dbNode, offsetInDB, &uActuallyWrriten, dbInput, sizeToWriteToDB);
        if (iError) break;
    }

    return (void *) (size_t) iError;
}

static void*
test6_readDirAttr(void *arg)
{
    int iError = 0;
    ThreadInput_s* psThreadInput = (ThreadInput_s*) arg;
    uint8_t uCounter = 0;
    while (uCounter++ < 200) {
        ReadDirAttr(*psThreadInput->ThreadInputNode.ppvParentFolder);
    }
    g_bStopAllThreds = true;
    return (void *) (size_t) iError;
}

static void*
test7_opsOnDirFiles(void *arg)
{
    enum {
        WRITE_TO_FILE = 0,
        READ_FROM_FILE,
        NUM_OF_ACTIONS
    };

    int iError = 0;
    ThreadInput_s* psThreadInput = (ThreadInput_s*) arg;

    uint8_t uCounnter = 0;
    while (uCounnter++ < 200) {
        UVFSFileNode dbNode = *psThreadInput->ThreadInputNode.ppvFileToWriteNode;
        UVFSFileAttributes sDBAttrs;
        iError = MSDOS_fsOps.fsops_getattr(dbNode, &sDBAttrs);
        if (iError) break;

        int randOP = rand() % NUM_OF_ACTIONS;
        switch (randOP) {
            case WRITE_TO_FILE: //Write - write 40K to random offset of the file
                {
                    uint64_t offset = rand() % sDBAttrs.fa_allocsize;
                    uint64_t sizeToWrite = 4096*10;

                    void* fileBuffer = malloc(sizeToWrite);
                    if (fileBuffer == NULL) {
                        iError = ENOMEM;
                        break;
                    }
                    memset(fileBuffer, psThreadInput->uThreadId, sizeToWrite);

                    size_t uActuallyWrriten;
                    iError =  WriteFromBuffer(dbNode, offset, &uActuallyWrriten, fileBuffer, sizeToWrite);
                    free(fileBuffer);
                }
                break;
            case READ_FROM_FILE: //Read  - read 40K from random offset of the file
                {
                    size_t uActuallyRead;
                    uint64_t offset = rand() % (sDBAttrs.fa_size - 4096*10);
                    uint64_t sizeToRead = (sDBAttrs.fa_size - offset) % 4096*10;

                    void* fileBuffer = malloc(sizeToRead);
                    if (fileBuffer == NULL) {
                        iError = ENOMEM;
                        break;
                    }

                    iError =  ReadToBuffer(dbNode, offset, &uActuallyRead, fileBuffer, sizeToRead);
                    free(fileBuffer);
                }
                break;
            default: //do nothing
                break;
        }

        if (iError) break;
    }

    return (void *) (size_t) iError;
}

static void
TestNlink( UVFSFileNode RootNode )
{
    UVFSFileAttributes sOutAttrs;
    UVFSFileNode psParentNode;
    UVFSFileNode psChildNode;

    assert( CreateNewFolder(RootNode, &psParentNode, "TestNlinkParentDirectory") == 0 );
    assert( CreateNewFile(psParentNode, &psChildNode, "TestNlinkFile0", 0) == 0 );
    assert( CloseFile(psChildNode) == 0 );
    assert( CreateNewFile(psParentNode, &psChildNode, "TestNlinkFile1", 0) == 0 );
    assert( CloseFile(psChildNode) == 0 );
    assert( CreateNewFile(psParentNode, &psChildNode, "TestNlinkFile2", 0) == 0 );
    assert( CloseFile(psChildNode) == 0 );
    assert( CreateLink(psParentNode, &psChildNode, "TestNlinkLink0", "/I/am/just/a/link") == 0 );
    assert( CloseFile(psChildNode) == 0 );
    assert( CreateLink(psParentNode, &psChildNode, "TestNlinkLink1", "/I/am/just/a/link") == 0 );
    assert( CloseFile(psChildNode) == 0 );
    assert( CreateLink(psParentNode, &psChildNode, "TestNlinkLink2", "/I/am/just/a/link") == 0 );
    assert( CloseFile(psChildNode) == 0 );
    assert( CreateNewFolder(psParentNode, &psChildNode, "TestNlinkDir0") == 0 );
    assert( CloseFile(psChildNode) == 0 );
    assert( CreateNewFolder(psParentNode, &psChildNode, "TestNlinkDir1") == 0 );
    assert( CloseFile(psChildNode) == 0 );
    assert( CreateNewFolder(psParentNode, &psChildNode, "TestNlinkDir2") == 0 );
    assert( CloseFile(psChildNode) == 0 );

    assert( MSDOS_fsOps.fsops_getattr(psParentNode, &sOutAttrs) == 0 );
#ifdef MSDOS_NLINK_IS_CHILD_COUNT
    assert( sOutAttrs.fa_nlink == 9+2 );
#else
    assert( sOutAttrs.fa_nlink == 1 );
#endif

    assert( RemoveFile(psParentNode, "TestNlinkFile2") == 0 );
    assert( RemoveFile(psParentNode, "TestNlinkLink2") == 0 );
    assert( RemoveFolder(psParentNode, "TestNlinkDir2") == 0 );

    assert( MSDOS_fsOps.fsops_getattr(psParentNode, &sOutAttrs) == 0 );
#ifdef MSDOS_NLINK_IS_CHILD_COUNT
    assert( sOutAttrs.fa_nlink == 6+2 );
#else
    assert( sOutAttrs.fa_nlink == 1 );
#endif

    assert( RemoveFile(psParentNode, "TestNlinkFile0") == 0 );
    assert( RemoveFile(psParentNode, "TestNlinkLink0") == 0 );
    assert( RemoveFolder(psParentNode, "TestNlinkDir0") == 0 );
    assert( RemoveFile(psParentNode, "TestNlinkFile1") == 0 );
    assert( RemoveFile(psParentNode, "TestNlinkLink1") == 0 );
    assert( RemoveFolder(psParentNode, "TestNlinkDir1") == 0 );

    assert( MSDOS_fsOps.fsops_getattr(psParentNode, &sOutAttrs) == 0 );
#ifdef MSDOS_NLINK_IS_CHILD_COUNT
    assert( sOutAttrs.fa_nlink == 2 );
#else
    assert( sOutAttrs.fa_nlink == 1 );
#endif

    assert( RemoveFolder(RootNode, "TestNlinkParentDirectory") == 0 );
    assert( CloseFile(psParentNode) == 0 );
}

static int is_empty(char *buf, size_t size)
{
    return buf[0] == 0 && !memcmp(buf, buf + 1, size - 1);
}

/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
int main( int argc, const char * argv[] )
{
    /*Need to create a script that will:
     1) create an image - hdiutil create -size 1M -fs MS-DOS -volname test2 -type SPARSE usbstorage2
     2) attach nomount  - hdiutil attach -nomount usbStorage_tests_image.dmg
     3) find the name   - Need to call to the latest /dev/rdisk.
     */

    if ( argc != 2 )
    {
        printf("Usage : livefiles_msdos_tester < dev-path >\n");
        exit(EINVAL);
    }

    UVFSScanVolsRequest sScanVolsReq = {0};
    UVFSScanVolsReply sScanVolsReply = {0};
    int err = 0;
    int fd = open( argv[1] , O_RDWR );
    int uCycleCounter = 0;
    UVFSFileNode RootNode = NULL;
    if(fd < 0)
    {
        printf("Failed to open [%s]\n", argv[1]);
        return EBADF;
    }

    do
    {
        unsigned seed = (unsigned) time(0);
        printf("running livefiles_msdos_tester with seed: %u\n", seed);
        srand(seed);

        err = MSDOS_fsOps.fsops_init();
        printf("Init err [%d]\n",err);
        if (err) break;

        err = MSDOS_fsOps.fsops_taste(fd);
        printf("Taste err [%d]\n",err);
        if (err) break;

        err = MSDOS_fsOps.fsops_scanvols(fd, &sScanVolsReq, &sScanVolsReply);
        printf("ScanVols err [%d]\n",err);
        if (err) break;

        err = MSDOS_fsOps.fsops_mount(fd, sScanVolsReply.sr_volid, 0, NULL, &RootNode);
        printf("Mount err [%d]\n",err);
        if (err) break;

        ReadDirAttr(RootNode);

        //Check nlink.
        TestNlink(RootNode);

        // --------------------------Create & mkdir----------------------------------
        // Create Dir:  D1 in Root
        UVFSFileNode D1_Node = NULL;
        err = CreateNewFolder(RootNode,&D1_Node,"D1");
        printf("CreateNewFolder err [%d]\n",err);
        if (err) break;

        UVFSFileNode D11_Node = NULL;
        err = CreateNewFolder(RootNode,&D11_Node,(char*)"ÖÖ");
        printf("CreateNewFolder ÖÖ err [%d]\n",err);
        if (err) break;

        // Create File Validation-MirrorAckColoradoBulldog (size 0) in D1
        UVFSFileNode F12_Node= NULL;
        err = CreateNewFile(D1_Node,&F12_Node,(char*)"Validation-MirrorAckColoradoBulldog",0);
        printf("CreateNewFile Validation-MirrorAckColoradoBulldog err [%d]\n",err);
        if (err) break;

        // Create File validation-mirrorackcoloradobulldog (size 0) in D1 - should fail
        UVFSFileNode F13_Node= NULL;
        err = CreateNewFile(D1_Node,&F13_Node,(char*)"validation-mirrorackcoloradobulldog",0);
        printf("CreateNewFile validation-mirrorackcoloradobulldog err [%d]\n",err);
        if (err != EEXIST) break;

        // Reclaim Validation-MirrorAckColoradoBulldog
        err = CloseFile(F12_Node);
        printf("Reclaim Validation-MirrorAckColoradoBulldog err [%d]\n",err);
        if (err) break;

        //Remove validation-mirrorackcoloradobulldog - should remove Validation-MirrorAckColoradoBulldog
        err = RemoveFile(D1_Node,(char*)"validation-mirrorackcoloradobulldog");
        printf("RemoveFile validation-mirrorackcoloradobulldog from Root err [%d]\n",err);
        if (err) break;

        // Reclaim and Lookup of D1
        err = CloseFile(D1_Node);
        printf("Reclaim D1 err [%d]\n",err);
        if (err) break;

        err = CloseFile(D11_Node);
        printf("Reclaim ÖÖ err [%d]\n",err);
        if (err) break;

        err = Lookup(RootNode,&D1_Node,"D1");
        printf("Lookup err [%d]\n",err);
        if (err) break;

        // Create File F1 (size 0) in D1
        UVFSFileNode F1_Node= NULL;
        err = CreateNewFile(D1_Node,&F1_Node,"F1",0);
        printf("CreateNewFile err [%d]\n",err);
        if (err) break;

        err = GetFSAttr(F1_Node);
        printf("GetFSAttr err [%d]\n",err);
        if (err) break;

        // Set Attr, make F1 larger
        err = SetAttrChangeSize(F1_Node,10*1024);
        printf("SetAttrChangeSize to 10K err [%d]\n",err);
        if (err) break;

        // Set Attr, make F1 smaller
        err = SetAttrChangeSize(F1_Node,0*1024);
        printf("SetAttrChangeSize to 0 err [%d]\n",err);
        if (err) break;

        // --------------------------- Pre-allocation Validation -------------------------------

        UVFSFileNode preAllocNode = NULL;
        err = CreateNewFile(RootNode, &preAllocNode, "pre-alloc file", 500);
        printf("CreateNewFile err [%d]\n",err);
        if (err) break;

        LIFilePreallocateArgs_t pre_alloc_req = {.flags = F_ALLOCATEALL, .length = 2*4096 };
        fstore_t pre_alloc_res = {0};

        MSDOS_fsOps.fsops_setfsattr(preAllocNode, LI_FSATTR_PREALLOCATE, (LIFSAttributeValue* )((void*) &pre_alloc_req), sizeof(LIFilePreallocateArgs_t), (LIFSAttributeValue* )((void*)&pre_alloc_res), sizeof(fstore_t));
        err = SetAttrChangeSize(preAllocNode,15048);
        if (err) break;

        size_t uActuallyRead;
        void* fileBuffer = malloc(14548);
        if (fileBuffer == NULL) {
            break;
        }

        err =  ReadToBuffer(preAllocNode, 500, &uActuallyRead, fileBuffer, 14548);
        if (err) break;

        if (!is_empty(fileBuffer, 14548)) {
            printf("pre-allocated file data should be all zeros\n");
            break;
        }

        free(fileBuffer);
        CloseFile(preAllocNode);
        RemoveFile(RootNode, "pre-alloc file");

        // -----------------------------Write/Read File-----------------------------------
        // Test1: 2 threads writing and reading to/from the same file
        // -------------------------------------------------------------------------------

        atomic_uint_least64_t sTest1_ThreadOffset = 0;
        void* sTest1_pvBuf = NULL;
        pthread_barrier_t sTest1_barrier;
        err = pthread_barrier_init(&sTest1_barrier, NULL, TEST_1_THREAD_COUNT);
        if (err) break;

        sTest1_pvBuf = malloc(100*1024*1024);
        if (sTest1_pvBuf == NULL) {
            err = ENOMEM;
            break;
        }

        for ( uint64_t uIdx=0; uIdx<TEST_1_THREAD_COUNT; uIdx++ )
        {
            memset(&sTest1_ThreadInput[uIdx],0,sizeof(ThreadInput_s));
            sTest1_ThreadInput[uIdx].uBufferSize = 1024*1024;
            sTest1_ThreadInput[uIdx].uFileSize = 100*1024*1024;
            sTest1_ThreadInput[uIdx].uTotalNumberOfThreads = TEST_1_THREAD_COUNT;
            sTest1_ThreadInput[uIdx].uThreadId = (uint32_t) uIdx;
            sTest1_ThreadInput[uIdx].ThreadInputNode.ppvFileToWriteNode = &F1_Node;

            sTest1_ThreadInput[uIdx].barrier = &sTest1_barrier;
            sTest1_ThreadInput[uIdx].general_offset = &sTest1_ThreadOffset;
            sTest1_ThreadInput[uIdx].general_buff = sTest1_pvBuf;

            pthread_create( &psTest1_Threads[uIdx], NULL, (void*)MultiThreadSingleFile, (void*)&sTest1_ThreadInput[uIdx]);
        }

        for ( uint32_t uIdx=0; uIdx<TEST_1_THREAD_COUNT; uIdx++ )
        {
            pthread_join( psTest1_Threads[uIdx], (void*) &piTest1_Results[uIdx] );

            if (piTest1_Results[uIdx])
            {
                err = piTest1_Results[uIdx];
                printf("Thread [%d] Failed in Test 1 with error [%d]\n",uIdx,err);
            }
        }
        free(sTest1_pvBuf);
        pthread_barrier_destroy(&sTest1_barrier);

        printf("Test 1 finished with error [%d]\n",err);
        if (err) break;

        // -----------------------------Write/Read File-----------------------------------
        // Test1.1: 6 threads writing and reading to/from the same file in half cluster size
        // -------------------------------------------------------------------------------

        size_t uRetLength;
        size_t uLength = 255;
        UVFSFSAttributeValue* psFSAttr = (UVFSFSAttributeValue*) malloc(256);
        if ( psFSAttr == NULL ) {
            err = ENOMEM;
            break;
        }

        err = MSDOS_fsOps.fsops_getfsattr(D1_Node, UVFS_FSATTR_BLOCKSIZE, psFSAttr, uLength, &uRetLength);
        printf("CreateNewFile err [%d]\n",err);
        if (err) break;

        uint64_t uBufferSize = psFSAttr->fsa_number / 2;
        free((void*)psFSAttr);

        // Create File F1_1 (size 0) in D1
        UVFSFileNode F1_1_Nodes[TEST_1_1_FILES_COUNT] = {NULL};
        pthread_barrier_t sTest1_1_barriers[TEST_1_1_FILES_COUNT];
        atomic_uint_least64_t sTest1_1_ThreadOffsets[TEST_1_1_FILES_COUNT] = {0};
        for (int i = 0; i < TEST_1_1_FILES_COUNT; i++) {
            char pcName[100] = {0};
            sprintf(pcName, "F1_%d", i);
            err = CreateNewFile(D1_Node, &F1_1_Nodes[i], pcName, 0);
            printf("CreateNewFile err [%d]\n",err);
            if (err) break;

            err = pthread_barrier_init(&sTest1_1_barriers[i], NULL, TEST_1_1_THREAD_COUNT);
            if (err) break;
        }
        if (err) break;

        int fileCounter = 0;
        for ( uint64_t uIdx=0; uIdx<TEST_1_1_THREAD_COUNT * TEST_1_1_FILES_COUNT; uIdx++ )
        {
            memset(&sTest1_1_ThreadInput[uIdx],0,sizeof(ThreadInput_s));
            sTest1_1_ThreadInput[uIdx].uBufferSize = 1024*1024;
            sTest1_1_ThreadInput[uIdx].uFileSize = 100*1024*1024;
            sTest1_1_ThreadInput[uIdx].uTotalNumberOfThreads = TEST_1_1_THREAD_COUNT * TEST_1_1_FILES_COUNT;
            sTest1_1_ThreadInput[uIdx].uThreadId = (uint32_t) uIdx;

            sTest1_1_ThreadInput[uIdx].ThreadInputNode.ppvFileToWriteNode = &F1_1_Nodes[fileCounter];
            sTest1_1_ThreadInput[uIdx].general_offset = &sTest1_1_ThreadOffsets[fileCounter];
            sTest1_1_ThreadInput[uIdx].barrier = &sTest1_1_barriers[fileCounter];
            if (uIdx % TEST_1_1_THREAD_COUNT == (TEST_1_1_THREAD_COUNT - 1)) {
                fileCounter++;
            }
            
            sTest1_1_ThreadInput[uIdx].uBufferSize = (uint32_t)uBufferSize;
            pthread_create( &psTest1_1_Threads[uIdx], NULL, (void*)MultiThreadSingleFileHalfClusterSize, (void*)&sTest1_1_ThreadInput[uIdx]);
        }

        for ( uint32_t uIdx=0; uIdx<TEST_1_1_THREAD_COUNT * TEST_1_1_FILES_COUNT; uIdx++ )
        {
            pthread_join( psTest1_1_Threads[uIdx], (void*) &piTest1_1_Results[uIdx] );
            
            if (piTest1_1_Results[uIdx])
            {
                err = piTest1_1_Results[uIdx];
                printf("Thread [%d] Failed in Test 1.1 with error [%d]\n",uIdx,err);
            }
        }

        for (int i = 0; i < TEST_1_1_FILES_COUNT; i++) {
            char pcName[100] = {0};
            sprintf(pcName, "F1_%d", i);
            RemoveFile(D1_Node, pcName);
            CloseFile(F1_1_Nodes[i]);
            pthread_barrier_destroy(&sTest1_1_barriers[i]);
        }

        printf("Test 1.1 finished with error [%d]\n",err);
        if (err) break;

        // ----------------------------------------------------------------------------------
        // Test2: 4 threads writing and reading to/from different files
        // ----------------------------------------------------------------------------------

        for ( uint64_t uIdx=0; uIdx<TEST_2_THREAD_COUNT; uIdx++ )
        {
            sTest2_ThreadOffset[uIdx] = 0;
            memset(&sTest2_ThreadInput[uIdx],0,sizeof(ThreadInput_s));
            sTest2_ThreadInput[uIdx].uBufferSize = 1024*1024;
            sTest2_ThreadInput[uIdx].uFileSize = 50*1024*1024;
            sTest2_ThreadInput[uIdx].uTotalNumberOfThreads = TEST_2_THREAD_COUNT;
            sTest2_ThreadInput[uIdx].uThreadId = (uint32_t) uIdx;
            sTest2_ThreadInput[uIdx].ThreadInputNode.ppvParentFolder = &D1_Node;

            pthread_create( &psTest2_Threads[uIdx], NULL, (void*)MultiThreadMultiFiles, (void*)&sTest2_ThreadInput[uIdx]);
        }

        for ( uint32_t uIdx=0; uIdx<TEST_2_THREAD_COUNT; uIdx++ )
        {
            pthread_join( psTest2_Threads[uIdx], (void*) &piTest2_Results[uIdx] );
            if (piTest2_Results[uIdx])
            {
                err = piTest2_Results[uIdx];
                printf("Thread [%d] Failed in Test 2 with error [%d]\n",uIdx,err);
            }
        }

        printf("Test 2 finished with error [%d]\n",err);
        if (err) break;

        // ----------------------------------------------------------------------------------
        // Test3: 3 threads writing and reading to/from the diff files with 2 threds each
        // ----------------------------------------------------------------------------------
        for ( uint64_t uIdx=0; uIdx < TEST_3_THREAD_COUNT; uIdx++ )
        {
            memset(&g_sTest3_ThreadInput[uIdx],0,sizeof(ThreadInput_s));
            g_sTest3_ThreadInput[uIdx].uBufferSize = 1024*1024;
            g_sTest3_ThreadInput[uIdx].uFileSize = 60*1024*1024;
            g_sTest3_ThreadInput[uIdx].uTotalNumberOfThreads = TEST_3_THREAD_COUNT;
            g_sTest3_ThreadInput[uIdx].uThreadId = (uint32_t) uIdx;
            g_sTest3_ThreadInput[uIdx].ThreadInputNode.ppvParentFolder = &D1_Node;
            
            pthread_create( &g_psTest3_Threads[uIdx], NULL, (void*)MultiThreadMultiFilesMultiThread, (void*)&g_sTest3_ThreadInput[uIdx]);
        }
        
        for ( uint32_t uIdx=0; uIdx<TEST_3_THREAD_COUNT; uIdx++ )
        {
            pthread_join( g_psTest3_Threads[uIdx], (void*) &g_piTest3_Results[uIdx] );

            if (g_piTest3_Results[uIdx])
            {
                err = g_piTest3_Results[uIdx];
                printf("Thread [%d] Failed in Test 3 with error [%d]\n",uIdx,err);
            }
        }
        
        printf("Test 3 finished with error [%d]\n",err);
        if (err) break;
        
        // ----------------------------------------------------------------------------------
        // Test4: 3 threads creating and removing files, 1 thread looking up inside the directory
        // ----------------------------------------------------------------------------------
  
#ifndef DIAGNOSTIC
        err = pthread_barrier_init(&sTest4_barrier, NULL, TEST_4_THREAD_COUNT);
        if (err) break;
    
        for ( uint64_t uIdx=0; uIdx < TEST_4_THREAD_COUNT; uIdx++ )
        {
            memset(&g_sTest4_ThreadInput[uIdx],0,sizeof(ThreadInput_s));
            g_sTest4_ThreadInput[uIdx].uTotalNumberOfThreads = TEST_4_THREAD_COUNT;
            g_sTest4_ThreadInput[uIdx].uThreadId = (uint32_t) uIdx;
            g_sTest4_ThreadInput[uIdx].ThreadInputNode.ppvParentFolder = &D1_Node;
            
            if (uIdx >= TEST_4_THREAD_COUNT -2)
                pthread_create( &g_psTest4_Threads[uIdx], NULL, (void*)test4_LookupFiles, (void*)&g_sTest4_ThreadInput[uIdx]);
            else
                pthread_create( &g_psTest4_Threads[uIdx], NULL, (void*)test4_CreateFiles, (void*)&g_sTest4_ThreadInput[uIdx]);
        }
        
        for ( uint32_t uIdx=0; uIdx<TEST_4_THREAD_COUNT; uIdx++ )
        {
            pthread_join( g_psTest4_Threads[uIdx], (void*) &g_piTest4_Results[uIdx] );

            if (g_piTest4_Results[uIdx])
            {
                err = g_piTest4_Results[uIdx];
                printf("Thread [%d] Failed in Test 4 with error [%d]\n",uIdx,err);
            }
        }
        pthread_barrier_destroy(&sTest4_barrier);
        printf("Test 4 finished with error [%d]\n",err);
        if (err) break;
#endif
        
        // ----------------------------------------------------------------------------------
        // Test5: 4 threads cloning a directory
        // ----------------------------------------------------------------------------------
        
        //Create a directory with TEST_5_NUM_OF_FILES files with random size
        UVFSFileNode fromDirNode = NULL;
        err = CreateNewFolder(RootNode,&fromDirNode,"FromDir");
        if (err) break;
        
        void* pvExternalBuffer = malloc(1024*1024);
        if (pvExternalBuffer == NULL) {
            err = ENOMEM;
            break;
        }
        
        for(uint64_t k=0; k < TEST_5_NUM_OF_FILES; k++) {
            char pcName[100] = {0};
            sprintf(pcName, TEST_5_FILE_NAME, k);

            UVFSFileNode fileNode= NULL;
            uint64_t size = 4096*10 + (rand() % (1024*1024 - 4096*10));
            err = CreateNewFile(fromDirNode, &fileNode, pcName, size);
            if (err) break;
            
            size_t uActuallyWritten;

            memset(pvExternalBuffer, (int)k, size);
            err = WriteFromBuffer(fileNode, 0, &uActuallyWritten, pvExternalBuffer, size);
            
            CloseFile(fileNode);
            if (err) break;
        }

        free(pvExternalBuffer);
        if (err) break;
        
        sTest5_FileToClone = 0;
        UVFSFileNode toDirNode = NULL;
        err = CreateNewFolder(RootNode,&toDirNode,"toDir");
        if (err) break;
        
        /*
         * Start all threads to get a from and to directory,
         * Each thread will get a number of files to copy, where for each file it will
         * have to get the org file attrs, create a file in toDir, preallocte the space,
         * and write the needed data
         */
        for ( uint64_t uIdx=0; uIdx < TEST_5_THREAD_COUNT; uIdx++ )
        {
            memset(&g_sTest5_ThreadInput[uIdx],0,sizeof(ThreadInput_s));
            g_sTest5_ThreadInput[uIdx].uTotalNumberOfThreads = TEST_5_THREAD_COUNT;
            g_sTest5_ThreadInput[uIdx].uThreadId = (uint32_t) uIdx;
            g_sTest5_ThreadInput[uIdx].ThreadInputNode.ppvParentFolder = &fromDirNode;
            g_sTest5_ThreadInput[uIdx].ThreadTargetNode.ppvToDirNode = &toDirNode;
            
            pthread_create( &g_psTest5_Threads[uIdx], NULL, (void*)test5_cloneFilesInDir, (void*)&g_sTest5_ThreadInput[uIdx]);
        }
        
        for ( uint32_t uIdx=0; uIdx<TEST_5_THREAD_COUNT; uIdx++ )
        {
            pthread_join( g_psTest5_Threads[uIdx], (void*) &g_piTest5_Results[uIdx] );

            if (g_piTest5_Results[uIdx])
            {
                err = g_piTest5_Results[uIdx];
                printf("Thread [%d] Failed in Test 5 with error [%d]\n",uIdx,err);
            }
        }
        
        printf("Test 5 finished with error [%d]\n",err);
        if (err) break;
        
        // Clean everything
        for(uint64_t k=0; k < TEST_5_NUM_OF_FILES; k++) {
            char pcName[100] = {0};
            sprintf(pcName, TEST_5_FILE_NAME, k);

            RemoveFile(toDirNode, pcName);
            RemoveFile(fromDirNode, pcName);
        }
        
        CloseFile(toDirNode);
        CloseFile(fromDirNode);
        RemoveFolder(RootNode, "toDir");
        RemoveFolder(RootNode, "fromDir");
        
        // ----------------------------------------------------------------------------------
        // Test6: 1 thread reading a directory, 6 threads doing ops on files from this directory
        // ----------------------------------------------------------------------------------
        pthread_mutex_init(&g_sTest6_mutex, NULL);
        UVFSFileNode mainDirNode = NULL;
        err = CreateNewFolder(RootNode,&mainDirNode,"mainDir");
        printf("CreateNewFolder err [%d]\n",err);
        if (err) break;
        
        UVFSFileNode dbFileNode = NULL;
        err = CreateNewFile(RootNode, &dbFileNode, "dbFile", 4096);
        printf("CreateNewFile err [%d]\n",err);
        if (err) break;
        
        void* pvExternalBufferTest6 = malloc(4608);
        if (pvExternalBufferTest6 == NULL) {
            err = ENOMEM;
            break;
        }
        
        for (int i = 0; i < TEST_6_NUM_OF_FILES; i++) {
            char pcName[100] = {0};
            sprintf(pcName, TEST_6_FILE_NAME, i);
            
            UVFSFileNode fileNode= NULL;
            uint64_t size = 513 + rand() % 4096;
            err = CreateNewFile(mainDirNode, &fileNode, pcName, size);
            if (err) break;
            
            size_t uActuallyWritten;
            memset(pvExternalBufferTest6, i, size);
            err = WriteFromBuffer(fileNode, 0, &uActuallyWritten, pvExternalBufferTest6, size);
            if (err) break;
            
            CloseFile(fileNode);
        }
        free(pvExternalBufferTest6);
        
        for ( uint64_t uIdx=0; uIdx < TEST_6_THREAD_COUNT; uIdx++ )
        {
            memset(&g_sTest6_ThreadInput[uIdx],0,sizeof(ThreadInput_s));
            g_sTest6_ThreadInput[uIdx].uTotalNumberOfThreads = TEST_6_THREAD_COUNT;
            g_sTest6_ThreadInput[uIdx].uThreadId = (uint32_t) uIdx;
            g_sTest6_ThreadInput[uIdx].ThreadInputNode.ppvParentFolder = &mainDirNode;
            g_sTest6_ThreadInput[uIdx].ThreadTargetNode.ppvToDirNode = &dbFileNode;
            if (uIdx == 0) { //first thread will do the readDir
                pthread_create( &g_psTest6_Threads[uIdx], NULL, (void*)test6_readDirAttr, (void*)&g_sTest6_ThreadInput[uIdx]);
            } else {
                pthread_create( &g_psTest6_Threads[uIdx], NULL, (void*)test6_opsOnDirFiles, (void*)&g_sTest6_ThreadInput[uIdx]);
            }
        }
        
        for ( uint32_t uIdx=0; uIdx<TEST_6_THREAD_COUNT; uIdx++ )
        {
            pthread_join( g_psTest6_Threads[uIdx], (void*) &g_piTest6_Results[uIdx] );

            if (g_piTest6_Results[uIdx])
            {
                err = g_piTest6_Results[uIdx];
                printf("Thread [%d] Failed in Test 6 with error [%d]\n",uIdx,err);
            }
        }
        
        printf("Test 6 finished with error [%d]\n",err);
        if (err) break;
        
        // Clean everything
        for(int k=0; k < TEST_6_NUM_OF_FILES; k++) {
            char pcName[100] = {0};
            sprintf(pcName, TEST_6_FILE_NAME, k);

            RemoveFile(mainDirNode, pcName);
        }
        CloseFile(dbFileNode);
        CloseFile(mainDirNode);
        
        RemoveFile(RootNode, "dbFile");
        RemoveFolder(RootNode, "mainDir");
        pthread_mutex_destroy(&g_sTest6_mutex);
        
        // ----------------------------------------------------------------------------------
        // Test7: 6 threads reading/ writing to the same file, random offsets
        // ----------------------------------------------------------------------------------

        err = CreateNewFile(RootNode, &dbFileNode, "dbFile", 4*1024*1024);
        printf("CreateNewFile err [%d]\n",err);
        if (err) break;

        UVFSFileNode dbFileNode2 = NULL;
        err = CreateNewFile(RootNode, &dbFileNode2, "dbFile2", 4*1024*1024);
        printf("CreateNewFile err [%d]\n",err);
        if (err) break;

        void* pvExternalBufferTest7 = malloc(4*1024*1024);
        if (pvExternalBufferTest7 == NULL) {
            err = ENOMEM;
            break;
        }
        memset(pvExternalBufferTest7, 7, 4*1024*1024);
        size_t uActuallyWritten;
        err = WriteFromBuffer(dbFileNode, 0, &uActuallyWritten, pvExternalBufferTest7, 4*1024*1024);
        if (err) break;
        err = WriteFromBuffer(dbFileNode2, 0, &uActuallyWritten, pvExternalBufferTest7, 4*1024*1024);
        if (err) break;
        free(pvExternalBufferTest7);

        for ( uint64_t uIdx=0; uIdx < TEST_7_THREAD_COUNT; uIdx++ )
        {
            memset(&g_sTest7_ThreadInput[uIdx],0,sizeof(ThreadInput_s));
            g_sTest7_ThreadInput[uIdx].uTotalNumberOfThreads = TEST_7_THREAD_COUNT;
            g_sTest7_ThreadInput[uIdx].uThreadId = (uint32_t) uIdx;
            if (uIdx % 2) {
                g_sTest7_ThreadInput[uIdx].ThreadInputNode.ppvFileToWriteNode = &dbFileNode;
            } else {
                g_sTest7_ThreadInput[uIdx].ThreadInputNode.ppvFileToWriteNode = &dbFileNode2;
            }
            pthread_create( &g_psTest7_Threads[uIdx], NULL, (void*)test7_opsOnDirFiles, (void*)&g_sTest7_ThreadInput[uIdx]);
        }

        for ( uint32_t uIdx=0; uIdx < TEST_7_THREAD_COUNT; uIdx++ )
        {
            pthread_join( g_psTest7_Threads[uIdx], (void*) &g_piTest7_Results[uIdx] );

            if (g_piTest7_Results[uIdx])
            {
                err = g_piTest7_Results[uIdx];
                printf("Thread [%d] Failed in Test 7 with error [%d]\n",uIdx,err);
            }
        }

        printf("Test 7 finished with error [%d]\n",err);
        if (err) break;
        
        // Clean everything
        CloseFile(dbFileNode);
        CloseFile(dbFileNode2);
        RemoveFile(RootNode, "dbFile");
        RemoveFile(RootNode, "dbFile2");
        
        // --------------------------------Read Dir-----------------------------------
        
        ReadDirAttr(D1_Node);

        bool bFound = false;
        read_directory_and_search_for_name( D1_Node, "F1", &bFound );

        if (!bFound)
        {
            printf("Dir read failed, F1 wasn't found in D1");
            break;
        }

        err = CloseFile(F1_Node);
        printf("Reclaim F1 err [%d]\n",err);
        if (err) break;

        // Rename F1 to F2 and move from D1 to Root
        err =  Rename(D1_Node, NULL, "F1", RootNode, NULL, "F2");
        printf("Rename F1 to F2 and move from D1 to Root err [%d]\n",err);
        if (err) break;

        err = CloseFile(D1_Node);
        printf("Reclaim D1 err [%d]\n",err);
        if (err) break;

        // Rename D1 to D2
        err =  Rename(RootNode, NULL, "D1", RootNode, NULL, "D2");
        printf("Rename D1 to D2 err [%d]\n",err);
        if (err) break;


        // ---------------------------------- Link ----------------------------------------

        UVFSFileNode L1_Node= NULL;
        char pcLinkContent[255] = "/dev/look/for/A/Link/In/Filder/?/Found/One";

        err =  CreateLink(RootNode ,&L1_Node,"L1",pcLinkContent);
        printf("CreateLink L1 err [%d]\n",err);
        if (err) break;

        void* pvExternalBufferForLink = malloc(4*1024);
        if (pvExternalBufferForLink == NULL)
        {
            err = ENOMEM;
            printf("Failed to allocate buffer to ReadLinkToBuffer L1 \n");
            break;
        }
        size_t actuallyRead = 0;
        err =  ReadLinkToBuffer(L1_Node,pvExternalBufferForLink, 4*1024, &actuallyRead);
        printf("ReadLinkToBuffer L1 err [%d]\n",err);
        if (err) break;

        if (memcmp(pcLinkContent,pvExternalBufferForLink,actuallyRead))
        {
            printf("Failed memcmp between symlink to readlink content\n");
            err = 1;
            break;
        }

        if (pvExternalBufferForLink!= NULL)
            free(pvExternalBufferForLink);

        // Rename L1 to L🤪2
        err =  Rename(RootNode, NULL, "L1", RootNode, NULL, "L🤪2");
        printf("Rename L1 to L🤪2 err [%d]\n",err);
        if (err) break;

        UVFSFileAttributes sOutAttrs;
        err = MSDOS_fsOps.fsops_getattr(L1_Node, &sOutAttrs);
        printf("fsops_getattr L2 err [%d]\n",err);
        if (err) break;

        struct timespec mTime = {0};
        mTime.tv_nsec = sOutAttrs.fa_mtime.tv_nsec;
        mTime.tv_sec = sOutAttrs.fa_mtime.tv_sec;

        err = CloseFile(L1_Node);
        printf("Reclaim L1 err [%d]\n",err);
        if (err) break;

        char* name_contains_array[5] = {0};
        char* name_end_with_array[5] = {0};
        char Smile[5] = "🤪";
        char ContainLetter[2] = "d";
        char EndsWithLetter[2] = "2";
        char SpecialChar[3] = "ö";

        name_contains_array[0] = (char*) Smile;
        name_contains_array[1] = (char*) ContainLetter;
        name_contains_array[2] = (char*) SpecialChar;
        name_contains_array[3] = NULL;

        name_end_with_array[0] = (char*) EndsWithLetter;
        name_end_with_array[1] = (char*) SpecialChar;
        name_end_with_array[2] = NULL;

        err =  ScanDir(RootNode, (char**) &name_contains_array, (char**) &name_end_with_array, mTime);
        printf("ScanDir of root err [%d]\n",err);
        if (err) break;

        // --------------------------------------------------------------------------------
        // Remove all files that left
        // --------------------------------------------------------------------------------

        // Remove ÖÖ
        err =  RemoveFolder(RootNode,(char*)"ÖÖ");
        printf("Remove Folder ÖÖ from Root err [%d]\n",err);
        if (err) break;

        // Remove D2
        err =  RemoveFolder(RootNode,(char*)"D2");
        printf("Remove Folder D2 from Root err [%d]\n",err);

        // Remove L🤪2
        err = RemoveFile(RootNode,"L🤪2");
        printf("Remove Link L🤪2 err [%d]\n",err);
        if (err) break;

        // Remove F2
        err = RemoveFile(RootNode,(char*)"F2");
        printf("RemoveFile F2 from Root err [%d]\n",err);
        if (err) break;

        // --------------------------------------------------------------------------------

        err = GetFSAttr(RootNode);
        printf("GetFSAttr err [%d]\n",err);
        if (err) break;

        err = MSDOS_fsOps.fsops_sync(RootNode);
        printf("Sync err [%d]\n",err);
        if (err) break;

        err = MSDOS_fsOps.fsops_unmount(RootNode, UVFSUnmountHintNone);
        printf("Unmount err [%d]\n",err);
        if (err) break;

        err = MSDOS_fsOps.fsops_check( fd, 0, NULL, CHECK );
        if (err) break;

        MSDOS_fsOps.fsops_fini();

        uCycleCounter++;
    }while(uCycleCounter < TEST_CYCLE_COUNT);

    return err;
}
