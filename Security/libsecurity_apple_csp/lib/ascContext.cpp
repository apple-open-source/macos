/*
 * ascContext.cpp - glue between BlockCrytpor and ComCryption (a.k.a. Apple
 *				  Secure Compression). 
 * Written by Doug Mitchell 4/4/2001
 */
 
#ifdef	ASC_CSP_ENABLE

#include "ascContext.h"
#include "ascFactory.h"
#include <security_utilities/debugging.h>
#include <security_utilities/logging.h>
#include <Security/cssmapple.h>

#define abprintf(args...)	secdebug("ascBuf", ## args)		/* buffer sizes */
#define aioprintf(args...)	secdebug("ascIo", ## args)		/* all I/O */

static Allocator *ascAllocator;

/*
 * Comcryption-style memory allocator callbacks
 */
static void *ccMalloc(unsigned size)
{
	return ascAllocator->malloc(size);
}
static void ccFree(void *data)
{
	ascAllocator->free(data);
}

/* Given a ComCryption error, throw appropriate CssmError */
static void throwComcrypt(
	comcryptReturn 	crtn, 
	const char		*op)		/* optional */
{
	CSSM_RETURN cerr = CSSM_OK;
	const char *errStr = "Bad Error String";
	
	switch(crtn) {
		case CCR_SUCCESS:
			errStr = "CCR_SUCCESS";
			break;
		case CCR_OUTBUFFER_TOO_SMALL:
			errStr = "CCR_OUTBUFFER_TOO_SMALL";
			cerr = CSSMERR_CSP_OUTPUT_LENGTH_ERROR;
			break;
		case CCR_MEMORY_ERROR:
			errStr = "CCR_MEMORY_ERROR";
			cerr = CSSMERR_CSP_MEMORY_ERROR;
			break;
		case CCR_WRONG_VERSION:
			errStr = "CCR_WRONG_VERSION";
			cerr = CSSMERR_CSP_INVALID_DATA;
			break;
		case CCR_BAD_CIPHERTEXT:
			errStr = "CCR_BAD_CIPHERTEXT";
			cerr = CSSMERR_CSP_INVALID_DATA;
			break;
		case CCR_INTERNAL:
		default:
			errStr = "CCR_INTERNAL";
			cerr = CSSMERR_CSP_INTERNAL_ERROR;
			break;
	}
	if(op) {
		Security::Syslog::error("Apple CSP %s: %s", op, errStr);
	}
	if(cerr) {
		CssmError::throwMe(cerr);
	}
}

/*
 * Algorithm factory.
 */
 
AscAlgFactory::AscAlgFactory(
	Allocator *normAlloc, 
	Allocator *privAlloc)
{
	/* once-per-address-space init */
	ascAllocator = privAlloc;
	comMallocRegister(ccMalloc, ccFree);
}

bool AscAlgFactory::setup(
	AppleCSPSession &session,
	CSPFullPluginSession::CSPContext * &cspCtx, 
	const Context &context)
{
	if(context.algorithm() != CSSM_ALGID_ASC) {
		return false;
	}
	if(cspCtx != NULL) {
		/* reusing one of ours; OK */
		return true;
	}
	switch(context.type()) {
		case CSSM_ALGCLASS_KEYGEN:
			cspCtx = new AppleSymmKeyGenerator(session,
				8,
				COMCRYPT_MAX_KEYLENGTH * 8,
				true);					// must be byte size
			return true;
		case CSSM_ALGCLASS_SYMMETRIC:
			cspCtx = new ASCContext(session);
			return true;
		default:
			break;
	}
	/* not ours */
	return false;
}

ASCContext::~ASCContext()
{
	if(mCcObj != NULL) {
		comcryptObjFree(mCcObj);
	}
}
	
/* 
 * Standard CSPContext init, called from CSPFullPluginSession::init().
 * Reusable, e.g., query followed by en/decrypt.
 */
void ASCContext::init( 
	const Context &context, 
	bool encrypting)
{
	CSSM_SIZE		keyLen;
	uint8 			*keyData 	= NULL;
	comcryptReturn	crtn;
	
	/* obtain key from context */
	symmetricKeyBits(context, session(), CSSM_ALGID_ASC, 
		encrypting ? CSSM_KEYUSE_ENCRYPT : CSSM_KEYUSE_DECRYPT,
		keyData, keyLen);
	if((keyLen < 1) || (keyLen > COMCRYPT_MAX_KEYLENGTH)) {
		CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_KEY);
	}
	mDecryptBufValid = false;
	
	/* optional optimization attribute */
	comcryptOptimize optimize = CCO_DEFAULT;
	uint32 opt = context.getInt(CSSM_ATTRIBUTE_ASC_OPTIMIZATION); 
	switch(opt) {
		case CSSM_ASC_OPTIMIZE_DEFAULT:
			optimize = CCO_DEFAULT;
			break;
		case CSSM_ASC_OPTIMIZE_SIZE:
			optimize = CCO_SIZE;
			break;
		case CSSM_ASC_OPTIMIZE_SECURITY:
			optimize = CCO_SECURITY;
			break;
		case CSSM_ASC_OPTIMIZE_TIME:
			optimize = CCO_TIME;
			break;
		case CSSM_ASC_OPTIMIZE_TIME_SIZE:
			optimize = CCO_TIME_SIZE;
			break;
		case CSSM_ASC_OPTIMIZE_ASCII:
			optimize = CCO_ASCII;
			break;
		default:
			CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_ALG_PARAMS);
	}
	
	/* All other context attributes ignored */
	/* init the low-level state */
	if(mCcObj == NULL) {
		/* note we allow for context reuse */
		mCcObj = comcryptAlloc();
		if(mCcObj == NULL) {
			CssmError::throwMe(CSSMERR_CSP_MEMORY_ERROR);
		}
	}
	 
	crtn = comcryptInit(mCcObj, keyData, (unsigned)keyLen, optimize);
	if(crtn) {
		throwComcrypt(crtn, "comcryptInit");
	}
}	

/*
 * All of these functions are called by CSPFullPluginSession.
 */
void ASCContext::update(
	void 			*inp, 
	size_t 			&inSize, 			// in/out
	void 			*outp, 
	size_t 			&outSize)			// in/out
{
	comcryptReturn crtn;
	unsigned outLen;
	unsigned char *inText  = (unsigned char *)inp;
	unsigned char *outText = (unsigned char *)outp;
	
	if(encoding()) {
		outLen = (unsigned)outSize;
		crtn = comcryptData(mCcObj, 
			inText, 
			(unsigned)inSize,
			outText,
			&outLen,
			CCE_MORE_TO_COME);		// not used on encrypt
		if(crtn) {
			throwComcrypt(crtn, "comcryptData");
		}
	}
	else {
		/* 
		 * Deal with 1-byte buffer hack. First decrypt the existing buffer...
		 */
		if(inSize == 0) {
			CssmError::throwMe(CSSMERR_CSP_INPUT_LENGTH_ERROR);
		}
		unsigned thisOutLen;
		unsigned partialOutLen = 0;
		if(mDecryptBufValid) {
			thisOutLen = (unsigned)outSize;
			crtn = deComcryptData(mCcObj,
				&mDecryptBuf,
				1,
				outText,
				&thisOutLen,
				CCE_MORE_TO_COME);
			mDecryptBufValid = false;
			if(crtn) {
				throwComcrypt(crtn, "deComcryptData (1)");
			}
			partialOutLen = thisOutLen;
			outText      += thisOutLen;
		}
		
		/*
		 * Now decrypt remaining, less one byte (which is stored in the 
		 * buffer).
		 */
		thisOutLen = (unsigned)(outSize - partialOutLen);
		crtn = deComcryptData(mCcObj,
			inText, 
			(unsigned)(inSize - 1),
			outText,
			&thisOutLen,
			CCE_MORE_TO_COME);
		if(crtn) {
			throwComcrypt(crtn, "deComcryptData (2)");
		}
		outLen = partialOutLen + thisOutLen;
		mDecryptBuf = inText[inSize - 1];
		mDecryptBufValid = true;
	}
	outSize = outLen;
	aioprintf("=== ASC::update encrypt %d   inSize %ld  outSize %ld",
		encoding() ? 1 : 0, inSize, outSize);
}

void ASCContext::final(
	CssmData 		&out)	
{
	if(encoding()) {
		out.length(0);
	}
	else {
		/* decrypt buffer hack */
		if(!mDecryptBufValid) {
			CssmError::throwMe(CSSMERR_CSP_INPUT_LENGTH_ERROR);
		}
		comcryptReturn crtn;
		unsigned outLen = (unsigned)out.Length;
		crtn = deComcryptData(mCcObj,
			&mDecryptBuf,
			1,
			(unsigned char *)out.Data,
			&outLen,
			CCE_END_OF_STREAM);
		mDecryptBufValid = false;
		if(crtn) {
			throwComcrypt(crtn, "deComcryptData (3)");
		}
		out.length(outLen);
	}
	aioprintf("=== ASC::final  encrypt %d   outSize %ld",
		encoding() ? 1 : 0, out.Length);
}

size_t ASCContext::inputSize(
	size_t 			outSize)			// input for given output size
{
	size_t rtn = comcryptMaxInBufSize(mCcObj,
		(unsigned)outSize,
		encoding() ? CCOP_COMCRYPT : CCOP_DECOMCRYPT);
	abprintf("--- ASCContext::inputSize  inSize %ld outSize %ld",
		rtn, outSize);
	return rtn;
}

/*
 * ComCryption's buffer size calculation really does not lend itself to the 
 * requirements here. For example, there is no guarantee that 
 * inputSize(outputSize(x)) == x. We're just going to fudge it and make 
 * apps (or CSPFullPluginSession) alloc plenty more than they need.
 */
#define ASC_OUTSIZE_FUDGE			1
#define ASC_OUTSIZE_FUDGE_FACTOR	1.2

size_t ASCContext::outputSize(
	bool 			final, 
	size_t 			inSize) 			// output for given input size
{
	unsigned effectiveInSize = (unsigned)inSize;
	size_t rtn;
	if(encoding()) {
		rtn = comcryptMaxOutBufSize(mCcObj,
			effectiveInSize,
			CCOP_COMCRYPT,
			final);
		#if ASC_OUTSIZE_FUDGE
		float newOutSize = rtn;
		newOutSize *= ASC_OUTSIZE_FUDGE_FACTOR;
		rtn = static_cast<size_t>(newOutSize);
		#endif	/* ASC_OUTSIZE_FUDGE */
	}
	else {
		if(final) {
			if(mDecryptBufValid) {
				effectiveInSize++;
			}
		}
		else if(inSize && !mDecryptBufValid) {
			/* not final and nothing buffered yet - lop off one */
			effectiveInSize--;
		}
		rtn = comcryptMaxOutBufSize(mCcObj,
			effectiveInSize,
			CCOP_DECOMCRYPT,
			final);
	}
	abprintf("--- ASCContext::outputSize inSize %ld outSize %ld final %d ",
		inSize, rtn, final);
	return rtn;
}

void ASCContext::minimumProgress(
	size_t 			&in, 
	size_t 			&out) 				// minimum progress chunks
{
	if(encoding()) {
		in  = 1;
		out = comcryptMaxOutBufSize(mCcObj,
			1,
			CCOP_COMCRYPT,
			0);
	}
	else {
		if(mDecryptBufValid) {
			/* use "everything" */
			in = 1;
		}
		else {
			in = 0;
		}
		out = comcryptMaxOutBufSize(mCcObj,
			(unsigned)in,
			CCOP_DECOMCRYPT,
			0);
	}
	abprintf("--- ASCContext::minProgres in %ld out %ld", in, out);
}

#endif	/* ASC_CSP_ENABLE */
