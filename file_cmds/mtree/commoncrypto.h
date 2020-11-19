
#ifndef _COMMON_CRYPTO_H_
#define _COMMON_CRYPTO_H_

#include <CommonCrypto/CommonDigestSPI.h>

#define kNone "none"

extern const int kSHA256NullTerminatedBuffLen;

#define MD5File(f, b)        Digest_File(kCCDigestMD5, f, b)
#define SHA1_File(f, b)      Digest_File(kCCDigestSHA1, f, b)
#define RIPEMD160_File(f, b) Digest_File(kCCDigestRMD160, f, b)
#define SHA256_File(f, b)    Digest_File(kCCDigestSHA256, f, b)

typedef struct {
    char *digest;
    uint64_t xdstream_priv_id;
} xattr_info;

struct attrbuf {
	uint32_t info_length;
	uint64_t sibling_id;
} __attribute__((aligned, packed));

typedef struct attrbuf attrbuf_t;

char *Digest_File(CCDigestAlg algorithm, const char *filename, char *buf);

xattr_info *calculate_SHA256_XATTRs(char *path, char *buf);
xattr_info *SHA256_Path_XATTRs(char *path, char *buf);
xattr_info *get_xdstream_privateid(char *path, char *buf);
char *SHA256_Path_ACL(char *path, char *buf);
uint64_t get_sibling_id(const char *path);

#endif /* _COMMON_CRYPTO_H_ */
