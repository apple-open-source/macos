#include <stdio.h>
#include "testbyteBuffer.h"
#include "capabilities.h"
#include "testmore.h"
#include <string.h>
#include <CommonCrypto/CommonDigest.h>

#if (CCBIGDIGEST == 0)
entryPoint(CommonBigDigest,"CommonCrypto CCDigest Large Size test")
#else
#include <CommonCrypto/CommonDigestSPI.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>


static void DigestInChunks(CCDigestAlgorithm algorithm, size_t chunksize, const uint8_t *bytesToDigest, size_t numbytes, uint8_t *outbuf)
{
    CCDigestRef d = CCDigestCreate(algorithm);
    while(numbytes) {
        size_t n = (numbytes < chunksize) ? numbytes: chunksize;
        CCDigestUpdate(d, bytesToDigest, n);
        numbytes -= n; bytesToDigest += n;
    }
    if(CCDigestFinal(d, outbuf)) return;
    CCDigestDestroy(d);
}

/*
 * Compute the digest of a whole file
 */

static int
checksum_file(char *filename, CCDigestAlgorithm algorithm)
{
    struct stat st;
    size_t digestsize = CCDigestGetOutputSize(algorithm);
    uint8_t mdwhole[digestsize];
    uint8_t mdchunk[digestsize];
    size_t blocksz = 0x40000000L; // 1 GB
    off_t filesize;
    u_char *buf;
    int fd; 
    
    fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror(filename);
        return -1; 
    }   
    if (fstat(fd, &st) < 0) {
        perror(filename);
        (void) close(fd);
        return -1; 
    }   
    
    filesize = st.st_size;
    buf = (u_char *) mmap(NULL, filesize,
                          PROT_READ, MAP_PRIVATE | MAP_NOCACHE, fd, 0); 
    if (buf == (u_char *) -1) {
        perror("mmap");
        close(fd);
        return -1; 
    }   
    (void) madvise(buf, filesize, MADV_SEQUENTIAL);
	printf("File is mapped\n");
    
	/*
	 * First do it in one big chunk
	 */
    
    CCDigest(algorithm, buf, filesize, mdwhole);
    
	/*
	 * Now do it in several 1GB chunks
	 */
    
    DigestInChunks(algorithm, blocksz, buf, filesize, mdchunk);
    
    (void) munmap(buf, filesize);
    (void) close(fd);
    
    int cmpval = memcmp(mdchunk, mdwhole, digestsize);
    ok(cmpval == 0, "Results are the same for both digests");
    
    return 0;
}


static const int kTestTestCount = 1000;

int CommonBigDigest(int argc, char *const *argv)
{
    
	plan_tests(kTestTestCount);
    
    char *testpath = "/Volumes/Data/Users/murf/Downloads/Zin_12A130_AppleInternal_038-2423-191.dmg";
    checksum_file(testpath, kCCDigestSHA1);

    return 0;
}


#endif
