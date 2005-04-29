#ifndef __CRYPT_MD5__
#define __CRYPT_MD5__

#ifdef __cplusplus
extern "C" {
#endif

char *crypt_md5(const char *pw, const char *salt);

#ifdef __cplusplus
};
#endif

#endif
