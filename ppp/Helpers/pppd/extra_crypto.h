#ifndef __EXTRA_CRYPTO_INCLUDE__
/* 
 * This is just a bunch of crypto routines that are needed by more than one 
 * piece of functionality, so they were broken out 
 */

void md4 __P((unsigned char *, int, unsigned char *));
void LmPasswordHash __P((char *, int, char *));
void NtPasswordHash __P((char *, int, unsigned char *));
void DesEncrypt __P((unsigned char *, unsigned char *, unsigned char *));

#define MAX_NT_PASSWORD		256	/* Max len of a (Unicode) NT passwd */
#define MD4_SIGNATURE_SIZE	16	/* 16 bytes in a MD4 message digest */

#define __EXTRA_CRYPTO_INCLUDE__
#endif /* __EXTRA_CRYPTO_INCLUDE__ */
