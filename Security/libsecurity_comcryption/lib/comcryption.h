/*
	File:		comcryption.h

	Contains:	interface for low-level comcryption engine

	Written by:	Doug Mitchell

	Copyright:	(c) 1997 by Apple Computer, Inc., all rights reserved.

	Change History (most recent first):

		11/11/97	gab		Updated for MPW
		10/29/97	dm		Created, based on work by R. Crandall,
								G. Brown, A. Perez
	To Do:

*/
#ifndef	_COMCRYPTION_H_
#define _COMCRYPTION_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Return values.
 */
typedef enum {
	CCR_SUCCESS = 0,			// normal result
	CCR_OUTBUFFER_TOO_SMALL,	// caller needs to alloc more out buffer
	CCR_MEMORY_ERROR,			// internal error
	CCR_WRONG_VERSION,			// compatibility error
	CCR_BAD_CIPHERTEXT,			// can't decrypt ciphertext stream
	CCR_INTERNAL				// internal library error
} comcryptReturn;

/*
 * Used to specify optimization in ComcryptInit(). May be ignored in
 * early implementation.
 */
typedef enum {
	CCO_DEFAULT,				// let the low-level code decide
	CCO_SIZE,					// optimize for max compression
	CCO_SECURITY,				// optimize for max crypto security
	CCO_TIME,					// optimize for minimum runtime; implies no
   								//   second-level comcryption; security not
								//   compromised
	CCO_TIME_SIZE,				// minimum runtime with second-level
								//   comcryption enabled; implies loss of
								//   security
	CCO_ASCII,					// optimize for max compression for ASCII
								//   plaintext
	CCO_OTHER					// TBD
} comcryptOptimize;

/*
 * Used to specify operation type.
 */
typedef enum {
	CCOP_COMCRYPT,
	CCOP_DECOMCRYPT
} comcryptOp;

/*
 * Used to specify End of stream.
 */
typedef enum {
	CCE_MORE_TO_COME,			// more ops to follow
	CCE_END_OF_STREAM			// end of stream, close output strem
} comcryptEos;

/*
 * Maximum key length in bytes.
 */
#define COMCRYPT_MAX_KEYLENGTH	64

/*
 * Clients can *optionally* register external memory alloc/free functions here.
 */
typedef void *(comMallocExternFcn)(unsigned size);
typedef void (comFreeExternFcn)(void *data);
void comMallocRegister(comMallocExternFcn *mallocExtern,
	comFreeExternFcn *freeExtern);

/*
 * Opaque data type for ComCryptData() and DeComCryptData()
 */
typedef void *comcryptObj;

/*
 * Call once at startup. The resulting comcryptObj can be reused multiple
 * times.
 */
comcryptObj comcryptAlloc(void);

/*
 * Use this before starting every stream process
 */
comcryptReturn comcryptInit(
	comcryptObj 		cobj,
    const unsigned char *key,
    unsigned            keyLen,
    comcryptOptimize    optimize);			// CCO_SIZE, etc.

/*
 * Free a comcryptObj object obtained via comcryptAlloc()
 */
void comcryptObjFree(comcryptObj cobj);

/*
 * Return the maximum input buffer size allowed for for specified
 * output buffer size. Note that for both comcrypt and decomcrypt,
 * to cover the worst case, the output buffer always has to be
 * larger that the input buffer.
 */
unsigned comcryptMaxInBufSize(comcryptObj cobj,
    unsigned outBufSize,
    comcryptOp op);					// CCOP_COMCRYPT, etc.

/*
 * Return the maximum output buffer size for specified input buffer size.
 * Output buffer size will always be larger than input buffer size.
 */
unsigned comcryptMaxOutBufSize(comcryptObj cobj,
    unsigned inBufSize,
    comcryptOp op,					// CCOP_COMCRYPT, etc.
	char final);					// nonzero for last op
									// only used for CCOP_DECOMCRYPT

/*
 * the one-function-fits-all comcrypt routine -
 * call it multiple times for one ComcryptObj if
 * you want, or just once to do a whole stream
 * in one shot.
 *
 * NOTE: in the current implementation, the endOfStream is not used;
 * no "final" call is necessary on comcryption. 
 */
comcryptReturn comcryptData(
	comcryptObj 			cobj,
	unsigned char 			*plainText,
	unsigned 				plainTextLen,
	unsigned char 			*cipherText,		// malloc'd by caller
	unsigned 				*cipherTextLen,		// IN/OUT
	comcryptEos 			endOfStream);		// CCE_END_OF_STREAM, etc.

/*
 * decomcrypt routine - call it multiple times for
 * one comcryptObj, or just once to do a whole stream
 * in one shot. Boundaries of ciphertext segments -
 * across calls to this function - are arbitrary.
 *
 * NOTE: in the current implementation, the final call to this (when
 * endOfStrem == CCE_END_OF_STREAM) must contain a nonzero amount of
 * ciphertext. 
 */
comcryptReturn deComcryptData(
	comcryptObj 			cobj,
	unsigned char 			*cipherText,
	unsigned 				cipherTextLen,
	unsigned char 			*plainText,
	unsigned	 			*plainTextLen,		// IN/OUT
	comcryptEos 			endOfStream);		// CCE_END_OF_STREAM, etc.

#ifdef __cplusplus
}
#endif

#endif	/*_COMCRYPTION_H_*/
