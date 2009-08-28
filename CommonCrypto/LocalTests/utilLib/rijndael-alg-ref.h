/* rijndael-alg-ref.h   v2.0   August '99
 * Reference ANSI C code
 * authors: Paulo Barreto
 *          Vincent Rijmen
 */
#ifndef __RIJNDAEL_ALG_H
#define __RIJNDAEL_ALG_H

#ifdef	__APPLE__
#define MIN_AES_KEY_BITS		128
#define MID_AES_KEY_BITS		192
#define MAX_AES_KEY_BITS		256
#define MAX_AES_KEY_BYTES		(MAX_AES_KEY_BITS / 8)

#define MIN_AES_BLOCK_BITS		128
#define MID_AES_BLOCK_BITS		192
#define MAX_AES_BLOCK_BITS		256
#define MIN_AES_BLOCK_BYTES		(MIN_AES_BLOCK_BITS / 8)

#endif
#define MAXBC				(MAX_AES_BLOCK_BITS/32)
#define MAXKC				(MAX_AES_KEY_BITS/32)
#define MAXROUNDS			14

#ifdef	__cplusplus
extern "C" {
#endif

typedef unsigned char		word8;	
typedef unsigned short		word16;	
typedef unsigned long		word32;


int _rijndaelKeySched (word8 k[4][MAXKC], int keyBits, int blockBits, 
		word8 rk[MAXROUNDS+1][4][MAXBC]);
int _rijndaelEncrypt (word8 a[4][MAXBC], int keyBits, int blockBits, 
		word8 rk[MAXROUNDS+1][4][MAXBC]);
#ifndef	__APPLE__
int rijndaelEncryptRound (word8 a[4][MAXBC], int keyBits, int blockBits, 
		word8 rk[MAXROUNDS+1][4][MAXBC], int rounds);
#endif
int _rijndaelDecrypt (word8 a[4][MAXBC], int keyBits, int blockBits, 
		word8 rk[MAXROUNDS+1][4][MAXBC]);
#ifndef	__APPLE__
int rijndaelDecryptRound (word8 a[4][MAXBC], int keyBits, int blockBits, 
		word8 rk[MAXROUNDS+1][4][MAXBC], int rounds);
#endif

#ifdef	__cplusplus
}
#endif

#endif /* __RIJNDAEL_ALG_H */
