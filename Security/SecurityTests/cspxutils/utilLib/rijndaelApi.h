/* 
 * rijndaelApi.h  -  AES API layer
 *
 * Based on rijndael-api-ref.h v2.0 written by Paulo Barreto
 * and Vincent Rijmen
 */

#ifndef	_RIJNDAEL_API_REF_H_
#define _RIJNDAEL_API_REF_H_

#include <stdio.h>
#include "rijndael-alg-ref.h"

#ifdef	__cplusplus
extern "C" {
#endif

#define     DIR_ENCRYPT     0    /*  Are we encrpyting?  */
#define     DIR_DECRYPT     1    /*  Are we decrpyting?  */
#define     MODE_ECB        1    /*  Are we ciphering in ECB mode?   */
#define     MODE_CBC        2    /*  Are we ciphering in CBC mode?   */

#define     TRUE            1
#define     FALSE           0

/*  Error Codes  */
#define     BAD_KEY_DIR        -1  /*  Key direction is invalid, e.g.,
									   unknown value */
#define     BAD_KEY_MAT        -2  /*  Key material not of correct 
									   length */
#define     BAD_KEY_INSTANCE   -3  /*  Key passed is not valid  */
#define     BAD_CIPHER_MODE    -4  /*  Params struct passed to 
									   cipherInit invalid */
#define     BAD_CIPHER_STATE   -5  /*  Cipher in wrong state (e.g., not 
									   initialized) */
#define     BAD_CIPHER_INSTANCE   -7 

#define     MAX_AES_KEY_SIZE	(MAX_AES_KEY_BITS / 8)
#define 	MAX_AES_BLOCK_SIZE	(MAX_AES_BLOCK_BITS / 8)
#define     MAX_AES_IV_SIZE		MAX_AES_BLOCK_SIZE
	
typedef    unsigned char    BYTE;

/*  The structure for key information */
typedef struct {
      BYTE  direction;	/* Key used for encrypting or decrypting? */
      int   keyLen;		/* Length of the key in bits */
      int   blockLen;   /* Length of block in bits */
      word8 keySched[MAXROUNDS+1][4][MAXBC];	/* key schedule		*/
      } keyInstance;

/*  The structure for cipher information */
typedef struct {
      BYTE  mode;           /* MODE_ECB, MODE_CBC, or MODE_CFB1 */
	  word8 chainBlock[4][MAXBC];      
	  int   blockLen;    	/* block length in bits */
      } cipherInstance;


int _makeKey(
	keyInstance *key, 
	BYTE direction, 
	int keyLen, 		// in BITS
	int blockLen,		// in BITS
	BYTE *keyMaterial);

int _cipherInit(
	cipherInstance *cipher, 
	BYTE mode, 
	int blockLen,		// in BITS
	BYTE *IV);

int _blockEncrypt(
	cipherInstance *cipher, 
	keyInstance *key, 
	BYTE *input, 
	int inputLen, 		// in BITS
	BYTE *outBuffer);

int _blockDecrypt(
	cipherInstance *cipher, 
	keyInstance *key, 
	BYTE *input,
	int inputLen, 		// in BITS
	BYTE *outBuffer);

/*
 * Apple addenda 3/28/2001: simplified single-block encrypt/decrypt.
 * Used when chaining and padding is done in elsewhere. 
 */
int _rijndaelBlockEncrypt(
	cipherInstance *cipher,
	keyInstance *key, 
	BYTE *input, 
	BYTE *outBuffer);
int _rijndaelBlockDecrypt(
	cipherInstance *cipher,
	keyInstance *key, 
	BYTE *input, 
	BYTE *outBuffer);
	
#ifdef	__cplusplus
}
#endif	// cplusplus

#endif	// RIJNDAEL_API_REF


