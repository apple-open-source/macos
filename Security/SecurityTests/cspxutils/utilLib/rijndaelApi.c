/* 
 * rijndaelApi.c  -  AES API layer
 *
 * Based on rijndael-api-ref.h v2.0 written by Paulo Barreto
 * and Vincent Rijmen
 */
#include <stdlib.h>
#include <string.h>

#include "rijndael-alg-ref.h"
#include "rijndaelApi.h"

#define CBC_DEBUG		0
#if 	CBC_DEBUG
static void dumpChainBuf(cipherInstance *cipher, char *op)
{
	int t,j;
	int columns = cipher->blockLen / 32;

	printf("chainBuf %s: ", op);
	for (j = 0; j < columns; j++) {
		for(t = 0; t < 4; t++) {
			printf("%02x ", cipher->chainBlock[t][j]);
		}
	}
	printf("\n");
}
#else
#define dumpChainBuf(c, o)
#endif

int _makeKey(	keyInstance *key, 
	BYTE direction, 
	int keyLen, 		// in BITS
	int blockLen,		// in BITS
	BYTE *keyMaterial)
{
	word8 k[4][MAXKC];
	unsigned keyBytes;
	unsigned  i;

	if (key == NULL) {
		return BAD_KEY_INSTANCE;
	}
	if(keyMaterial == NULL) {
		return BAD_KEY_MAT;
	}
	if ((direction == DIR_ENCRYPT) || (direction == DIR_DECRYPT)) {
		key->direction = direction;
	} else {
		return BAD_KEY_DIR;
	}

	if ((keyLen == 128) || (keyLen == 192) || (keyLen == 256)) { 
		key->keyLen = keyLen;
	} else {
		return BAD_KEY_MAT;
	}
	key->blockLen = blockLen;

	/* initialize key schedule: */ 
	keyBytes = keyLen / 8;
 	for(i = 0; i < keyBytes; i++) {
		k[i % 4][i / 4] = keyMaterial[i]; 
	}	
	_rijndaelKeySched (k, key->keyLen, key->blockLen, key->keySched);	
	memset(k, 0, 4 * MAXKC);
	return TRUE;
}

int _cipherInit(	cipherInstance *cipher, 
	BYTE mode, 
	int blockLen,		// in BITS
	BYTE *IV)
{
	int t, j;
	int columns = blockLen / 32;
	
	/* MODE_CFB1 not supported */
	if ((mode == MODE_ECB) || (mode == MODE_CBC)) {
		cipher->mode = mode;
	} else {
		return BAD_CIPHER_MODE;
	}
	cipher->blockLen = blockLen;
	
	if (IV != NULL) {
		/* Save IV in rectangular block format */
		for (j = 0; j < columns; j++) {
			for(t = 0; t < 4; t++) {
				/* parse initial value into rectangular array */
				cipher->chainBlock[t][j] = IV[t+4*j];
			}
		}
	}
	dumpChainBuf(cipher, "init  ");	
	return TRUE;
}


int _blockEncrypt(cipherInstance *cipher,
	keyInstance *key, BYTE *input, int inputLen, BYTE *outBuffer)
{
	int i, j, t, numBlocks;
	unsigned blockSizeBytes;
	int columns;
	
	/* check parameter consistency: */
	if (key == NULL ||
		key->direction != DIR_ENCRYPT ||
		(key->keyLen != 128 && key->keyLen != 192 && key->keyLen != 256)) {
		return BAD_KEY_MAT;
	}
	if (cipher == NULL ||
		(cipher->mode != MODE_ECB && cipher->mode != MODE_CBC) ||
		(cipher->blockLen != 128 && cipher->blockLen != 192 && cipher->blockLen != 256)) {
			return BAD_CIPHER_STATE;
	}

	numBlocks = inputLen/cipher->blockLen;
	blockSizeBytes = cipher->blockLen / 8;
	columns = cipher->blockLen / 32;
	
	switch (cipher->mode) {
	case MODE_ECB: 
		for (i = 0; i < numBlocks; i++) {
			for (j = 0; j < columns; j++) {
				for(t = 0; t < 4; t++)
				/* parse input stream into rectangular array */
					cipher->chainBlock[t][j] = input[4*j+t];
			}
			_rijndaelEncrypt (cipher->chainBlock, key->keyLen, cipher->blockLen, key->keySched);
			for (j = 0; j < columns; j++) {
				/* parse rectangular array into output ciphertext bytes */
				for(t = 0; t < 4; t++)
					outBuffer[4*j+t] = (BYTE) cipher->chainBlock[t][j];
			}
			input += blockSizeBytes;
			outBuffer += blockSizeBytes;
			dumpChainBuf(cipher, "encr ECB");
		}
		break;
		
	case MODE_CBC:
		for (i = 0; i < numBlocks; i++) {
			for (j = 0; j < columns; j++) {
				for(t = 0; t < 4; t++)
				/* parse input stream into rectangular array and exor with 
				   IV or the previous ciphertext */
					cipher->chainBlock[t][j] ^= input[4*j+t];
			}
			_rijndaelEncrypt (cipher->chainBlock, key->keyLen, cipher->blockLen, key->keySched);
			for (j = 0; j < columns; j++) {
				/* parse rectangular array into output ciphertext bytes */
				for(t = 0; t < 4; t++)
					outBuffer[4*j+t] = (BYTE) cipher->chainBlock[t][j];
			}
			/* Hey! This code was broken for multi-block ops! */
			input += blockSizeBytes;
			outBuffer += blockSizeBytes;
			dumpChainBuf(cipher, "encr CBC");
		}
		break;
	
	default: return BAD_CIPHER_STATE;
	}
	
	return numBlocks*cipher->blockLen;
}

int _blockDecrypt(cipherInstance *cipher,
	keyInstance *key, BYTE *input, int inputLen, BYTE *outBuffer)
{
	int i, j, t, numBlocks;
	word8 block[4][MAXBC];		// working memory: encrypt/decrypt in place here
	unsigned blockSizeBytes;
	word8 cblock[4][MAXBC];		// saved ciphertext
	int columns;

	if (cipher == NULL ||
		key == NULL ||
		key->direction == DIR_ENCRYPT ||
		cipher->blockLen != key->blockLen) {
		return BAD_CIPHER_STATE;
	}

	/* check parameter consistency: */
	if (key == NULL ||
		key->direction != DIR_DECRYPT ||
		(key->keyLen != 128 && key->keyLen != 192 && key->keyLen != 256)) {
		return BAD_KEY_MAT;
	}
	if (cipher == NULL ||
		(cipher->mode != MODE_ECB && cipher->mode != MODE_CBC) ||
		(cipher->blockLen != 128 && cipher->blockLen != 192 && cipher->blockLen != 256)) {
		return BAD_CIPHER_STATE;
	}
	
	numBlocks = inputLen/cipher->blockLen;
	blockSizeBytes = cipher->blockLen / 8;
	columns = cipher->blockLen / 32;
	
	switch (cipher->mode) {
	case MODE_ECB: 
		for (i = 0; i < numBlocks; i++) {
			for (j = 0; j < columns; j++) {
				for(t = 0; t < 4; t++)
				/* parse input stream into rectangular array */
					block[t][j] = input[4*j+t];
			}
			_rijndaelDecrypt (block, key->keyLen, cipher->blockLen, key->keySched);
			for (j = 0; j < columns; j++) {
				/* parse rectangular array into output ciphertext bytes */
				for(t = 0; t < 4; t++)
					outBuffer[4*j+t] = (BYTE) block[t][j];
			}
			input += blockSizeBytes;
			outBuffer += blockSizeBytes;
			dumpChainBuf(cipher, "decr ECB");
		}
		break;
		
	case MODE_CBC:
		for (i = 0; i < numBlocks; i++) {
			for (j = 0; j < columns; j++) {
				for(t = 0; t < 4; t++)
				/* parse input stream into rectangular array */
					block[t][j] = input[4*j+t];
			}
			
			/* save a copoy of incoming ciphertext for later chain; decrypt */
			memmove(cblock, block, 4*MAXBC);
			_rijndaelDecrypt (block, key->keyLen, cipher->blockLen, key->keySched);
			
			/* 
			 * exor with last ciphertext --> plaintext out
			 * save this ciphertext in lastBlock
			 * FIXME - we can optimize this by avoiding the copy into 
			 * lastBlock on all but last time thru...
			 */
			for (j = 0; j < columns; j++) {
				for(t = 0; t < 4; t++) {
					outBuffer[4*j+t] = (block[t][j] ^ cipher->chainBlock[t][j]);
				}
			}
			memmove(cipher->chainBlock, cblock, 4 * MAXBC);
			input += blockSizeBytes;
			outBuffer += blockSizeBytes;
			dumpChainBuf(cipher, "decr CBC");
		}
		break;
	
	default: return BAD_CIPHER_STATE;
	}
	memset(block, 0, 4 * MAXBC);
	memset(cblock, 0, 4 * MAXBC);
	return numBlocks*cipher->blockLen;
}

/*
 * Apple addenda 3/28/2001: simplified single-block encrypt/decrypt.
 * Used when chaining and padding is done in elsewhere. 
 */
#define AES_CONSISTENCY_CHECK		1

int _rijndaelBlockEncrypt(
	cipherInstance *cipher,
	keyInstance *key, 
	BYTE *input, 
	BYTE *outBuffer)
{
	int j, t;
	unsigned blockSizeBytes;
	int columns;
	
	#if		AES_CONSISTENCY_CHECK
	/* check parameter consistency: */
	if (key == NULL ||
		key->direction != DIR_ENCRYPT ||
		(key->keyLen != 128 && key->keyLen != 192 && key->keyLen != 256)) {
		return BAD_KEY_MAT;
	}
	if (cipher == NULL ||
		(cipher->mode != MODE_ECB && cipher->mode != MODE_CBC) ||
		(cipher->blockLen != 128 && cipher->blockLen != 192 && cipher->blockLen != 256)) {
			return BAD_CIPHER_STATE;
	}
	#endif	/* AES_CONSISTENCY_CHECK */
	
	blockSizeBytes = cipher->blockLen >> 3;	/* was / 8; should just save in cipher */
	columns = cipher->blockLen >> 5;		/* was / 32; ditto */
	
	for (j = 0; j < columns; j++) {
		for(t = 0; t < 4; t++)
		/* parse input stream into rectangular array */
			cipher->chainBlock[t][j] = input[4*j+t];
	}
	_rijndaelEncrypt (cipher->chainBlock, key->keyLen, cipher->blockLen, 
		key->keySched);
	for (j = 0; j < columns; j++) {
		/* parse rectangular array into output ciphertext bytes */
		for(t = 0; t < 4; t++)
			outBuffer[4*j+t] = (BYTE) cipher->chainBlock[t][j];
	}
	return cipher->blockLen;
}

int _rijndaelBlockDecrypt(
	cipherInstance *cipher,
	keyInstance *key, 
	BYTE *input, 
	BYTE *outBuffer)
{
	int j, t;
	word8 block[4][MAXBC];		// working memory: encrypt/decrypt in place here
	unsigned blockSizeBytes;
	int columns;

	#if		AES_CONSISTENCY_CHECK
	if (cipher == NULL ||
		key == NULL ||
		key->direction == DIR_ENCRYPT ||
		cipher->blockLen != key->blockLen) {
		return BAD_CIPHER_STATE;
	}

	/* check parameter consistency: */
	if (key == NULL ||
		key->direction != DIR_DECRYPT ||
		(key->keyLen != 128 && key->keyLen != 192 && key->keyLen != 256)) {
		return BAD_KEY_MAT;
	}
	if (cipher == NULL ||
		(cipher->mode != MODE_ECB && cipher->mode != MODE_CBC) ||
		(cipher->blockLen != 128 && cipher->blockLen != 192 && cipher->blockLen != 256)) {
		return BAD_CIPHER_STATE;
	}
	#endif		/* AES_CONSISTENCY_CHECK */
	
	blockSizeBytes = cipher->blockLen >> 3;	/* was / 8; should just save in cipher */
	columns = cipher->blockLen >> 5;		/* was / 32; ditto */
	
	for (j = 0; j < columns; j++) {
		for(t = 0; t < 4; t++)
		/* parse input stream into rectangular array */
			block[t][j] = input[4*j+t];
	}
	_rijndaelDecrypt (block, key->keyLen, cipher->blockLen, key->keySched);
	for (j = 0; j < columns; j++) {
		/* parse rectangular array into output ciphertext bytes */
		for(t = 0; t < 4; t++)
			outBuffer[4*j+t] = (BYTE) block[t][j];
	}
			
	return cipher->blockLen;
}

