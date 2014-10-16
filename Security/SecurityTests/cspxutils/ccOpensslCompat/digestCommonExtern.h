/* 
 * digestCommonExtern.h - extern declarations of the functions resulting from both kinds of 
 *						  instantiations of the code in digestCommon.h
 */
 
#ifndef	_DIGEST_COMMON_EXTERN_H_
#define _DIGEST_COMMON_EXTERN_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Openssl versions */

extern int md2os(const void *p, unsigned long len, unsigned char *md);
extern int md4os(const void *p, unsigned long len, unsigned char *md);
extern int md5os(const void *p, unsigned long len, unsigned char *md);
extern int sha1os(const void *p, unsigned long len, unsigned char *md);
extern int sha224os(const void *p, unsigned long len, unsigned char *md);
extern int sha256os(const void *p, unsigned long len, unsigned char *md);
extern int sha384os(const void *p, unsigned long len, unsigned char *md);
extern int sha512os(const void *p, unsigned long len, unsigned char *md);

/* The CommonDigest versions */

extern int md2cc(const void *p, unsigned long len, unsigned char *md);
extern int md4cc(const void *p, unsigned long len, unsigned char *md);
extern int md5cc(const void *p, unsigned long len, unsigned char *md);
extern int sha1cc(const void *p, unsigned long len, unsigned char *md);
extern int sha224cc(const void *p, unsigned long len, unsigned char *md);
extern int sha256cc(const void *p, unsigned long len, unsigned char *md);
extern int sha384cc(const void *p, unsigned long len, unsigned char *md);
extern int sha512cc(const void *p, unsigned long len, unsigned char *md);

#ifdef __cplusplus
}
#endif

#endif	/* _CC_COMMON_DIGEST_H_ */
