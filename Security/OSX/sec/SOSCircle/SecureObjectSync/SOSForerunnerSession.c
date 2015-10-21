#include "SOSForerunnerSession.h"
#include "SOSAccountDer.c"
#include "SOSPlatform.h"

#include <CoreFoundation/CFRuntime.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecCFError.h>
#include <Security/SecureObjectSync/SOSInternal.h>
#include <corecrypto/ccsrp.h>
#include <corecrypto/ccsha2.h>
#include <corecrypto/ccdh_gp.h>
#include <corecrypto/ccder.h>
#include <corecrypto/ccaes.h>
#include <corecrypto/ccmode.h>
#include <corecrypto/cchkdf.h>
#include <CommonCrypto/CommonRandomSPI.h>
#include <os/assumes.h>
#include <AssertMacros.h>

#pragma mark Definitions
#define FR_VERSION 1llu
#define FR_MAGIC_REQUEST 0x67756d70llu
#define FR_MAGIC_CHALLENGE 0x67756d71llu
#define FR_MAGIC_RESPONSE 0x67756d72llu
#define FR_MAGIC_HSA2 0x67756d73llu
#define FR_SALT_LEN 32llu

#define FR_Z_SZ_HKDF_V1 32
#define FR_Z_SZ_V1 16
#define FR_Z_FROM_REQUESTOR "requestor2acceptor"
#define FR_Z_FROM_ACCEPTOR "acceptor2requestor"

#define FR_TAG_SIZE_V1 CCAES_KEY_SIZE_128
#define FR_SIDECAR_SIZE_V1 (sizeof(uint64_t) + FR_TAG_SIZE_V1)

// The initialization vector has three parts.
// |<-------- DSID -------->|<- X ->|<------ counter ------>|
//           64 bits          8 bits         56 bits
//
// The DSID is known to each end and supplied to each session object at create-
// time. X is either 0x0a or 0x0b, depending on whether the message came from an
// acceptor or requestor session, respectively. These values are static and
// known to each end. The counter starts at zero on each end and is incremented
// for each packet generated. It is sent as a sidecar along with the encrypted
// data blob, and it is then used to construct the IV for decryption along with
// the other two (known) components. Its value can not exceed 2^56 - 1.
#define FR_IV_SIZE_V1 (sizeof(uint64_t) + sizeof(uint64_t))
#define FR_IV_X_ACCEPT_V1 (0x0a)
#define FR_IV_X_REQUEST_V1 (0x0b)
#define FR_IV_X_SIZE_V1 (1)
#define FR_IV_CNT_MAX_V1 ((0x100000000000000llu) - 1)
#define FR_IV_CNT_SIZE_V1 (7)

#define FR_MAX_ACCEPTOR_TRIES 2

#define print_paddedline(stream, pad, fmt, ...) do { \
	size_t i = 0; \
	for (i = 0; i < pad; i++) { \
		fprintf((stream), "\t"); \
	} \
\
	fprintf((stream), fmt "\n", ## __VA_ARGS__); \
} while (0);

#pragma mark Utilities
__unused static void
_print_blob(FILE *stream, size_t pad, const char *name,
		uint8_t *buff, size_t sz, size_t len2print)
{
	size_t nb2w = 0;
	if (len2print && len2print < sz) {
		nb2w = len2print;
	} else {
		nb2w = sz;
	}

	if (nb2w == 0) {
		print_paddedline(stream, pad, "%s = (null)\n", name);
	} else {
		size_t i = 0; \
		for (i = 0; i < pad; i++) {
			fprintf(stream, "\t");
		}

		fprintf(stream, "%s = 0x", name);

		uint8_t *buffp = buff;
		for (i = 0; i < nb2w; i++) {
			fprintf(stream, "%2.2x", buffp[i]);
		}

		if (len2print && sz > len2print) {
			fprintf(stream, "...");
		}

		fprintf(stream, "\n");
	}
}

#pragma mark CoreCrypto Helpers
static uint8_t *
_ccder_shim_encode_octet_string(size_t len, const uint8_t *start,
		const uint8_t *der, uint8_t *der_end)
{
	der_end = ccder_encode_body(len, start, der, der_end);
	der_end = ccder_encode_tl(CCDER_OCTET_STRING, len, der, der_end);
	require_action_quiet(der_end, xit, {
		os_hardware_trap();
	});

xit:
	return der_end;
}

static uint8_t *
_ccder_shim_decode_octect_string(size_t *len, const uint8_t **start,
		const uint8_t *der, const uint8_t *der_end)
{
	der = ccder_decode_tl(CCDER_OCTET_STRING, len, der, der_end);
	if (der && start) {
		*start = der;
		der += *len;
	}

	return (uint8_t *)der;
}

static ccsrp_ctx *
_ccsrp_shim_alloc(const struct ccdigest_info *di, ccdh_const_gp_t gp)
{
	ccsrp_ctx *srp = NULL;
	int error = -1;

	// CoreCrypto wants these to be 8-byte aligned. malloc(3) and friends return
	// memory that is suitable for use as AltiVec/SSE data types, so they are
	// good for this interface.
	srp = malloc(ccsrp_sizeof_srp(di, gp));
	require_action_quiet(srp, xit, {
		error = errno;
	});

	if (!((uintptr_t)srp & 7) == 0) {
		os_hardware_trap();
	}

	ccsrp_ctx_init(srp, di, gp);
	error = 0;

xit:
	if (error) {
		free(srp);
		srp = NULL;
	}

	return srp;
}

static void
_derive_sending_key(ccsrp_ctx *srp, const char *info, uint8_t *Z, size_t Z_len)
{
	const struct ccdigest_info *di = ccsha256_di();
	const uint8_t *K = NULL;
	size_t K_len = 0;
	uint8_t Z2[FR_Z_SZ_HKDF_V1];
	int error = -1;

	if (Z_len < FR_Z_SZ_V1) {
		os_hardware_trap();
	}

	K = ccsrp_get_session_key(srp, &K_len);

	error = cchkdf(di, K_len, K, 0, NULL, strlen(info), info, sizeof(Z2), Z2);
	os_assert_zero(error);

	// Only use first 16 bytes for AEAD.
	memcpy(Z, Z2, FR_Z_SZ_V1);
}

static void
_construct_iv_v1(const uint8_t iv[FR_IV_SIZE_V1], uint64_t dsid,
		uint8_t x, uint64_t cnt)
{
	uint8_t *cur_iv = (uint8_t *)iv;

	if (!(x == FR_IV_X_ACCEPT_V1 || x == FR_IV_X_REQUEST_V1)) {
		os_hardware_trap();
	}

	dsid = OSSwapHostToBigInt64(dsid);
	memcpy(cur_iv, &dsid, sizeof(dsid));
	cur_iv += sizeof(dsid);

	// No need to swap; it's just one byte.
	memcpy(cur_iv, &x, sizeof(x));
	cur_iv += sizeof(x);

	if (cnt > FR_IV_CNT_MAX_V1) {
		os_hardware_trap();
	}

	cnt = OSSwapHostToBigInt64(cnt);
	memcpy(cur_iv, &cnt, FR_IV_CNT_SIZE_V1);
}

static uint8_t *
_encrypt_data_v1(const uint8_t *unenc, size_t unenc_len,
		uint64_t dsid, uint8_t x, uint64_t cnt,
		uint8_t *key, size_t key_len, size_t *enc_len)
{
	uint8_t *enc = NULL;
	int error = -1;

	uint8_t *enc_cur = NULL;
	size_t enc_len2 = 0;
	const struct ccmode_gcm *mode = ccaes_gcm_encrypt_mode();
	ccgcm_ctx_decl(mode->size, gcm);
	uint8_t iv[FR_IV_SIZE_V1];

	enc_len2 += FR_SIDECAR_SIZE_V1;
	enc_len2 += unenc_len;

	enc = malloc(enc_len2);
	require_action_quiet(enc, xit, {
		error = errno;
	});

	enc_cur = enc;

	ccgcm_init(mode, gcm, key_len, key);
	_construct_iv_v1(iv, dsid, x, cnt);
	ccgcm_set_iv(mode, gcm, FR_IV_SIZE_V1, iv);
	ccgcm_gmac(mode, gcm, 0, NULL);

	if (cnt > FR_IV_CNT_MAX_V1) {
		os_hardware_trap();
	}

	cnt = OSSwapHostToBigInt64(cnt);
	memcpy(enc_cur, &cnt, sizeof(cnt));
	enc_cur += sizeof(cnt);

	ccgcm_update(mode, gcm, unenc_len, unenc, enc_cur);
	enc_cur += unenc_len;

	ccgcm_finalize(mode, gcm, FR_TAG_SIZE_V1, enc_cur);
	error = 0;

xit:
	ccgcm_ctx_clear(ccgcm_context_size(mode), gcm);

	if (error) {
		free(enc);
		enc = NULL;
	} else {
		*enc_len = enc_len2;
	}

	return enc;
}

static uint8_t *
_decrypt_data_v1(const uint8_t *enc, size_t enc_len,
		uint64_t dsid, uint8_t x, uint8_t *key, size_t key_len, size_t *dec_len)
{
	uint8_t *dec = NULL;
	int error = -1;
	int ret = -1;

	size_t dec_len2 = 0;
	const struct ccmode_gcm *mode = ccaes_gcm_decrypt_mode();
	ccgcm_ctx_decl(mode->size, gcm);
	const uint8_t *enc_cur = NULL;
	uint64_t cnt = 0;
	uint8_t iv[FR_IV_SIZE_V1];
	uint8_t tag[FR_TAG_SIZE_V1];

	enc_cur = enc;

	// At minimum, the encrypted data must contain the tag and counter.
	require_action_quiet(enc_len >= FR_SIDECAR_SIZE_V1, xit, {
		error = EINVAL;
	});
	dec_len2 = enc_len - FR_SIDECAR_SIZE_V1;

	dec = malloc(dec_len2);
	require_action_quiet(dec, xit, {
		error = errno;
	});

	memcpy(&cnt, enc_cur, sizeof(cnt));
	cnt = OSSwapBigToHostConstInt64(cnt);
	require_action_quiet(cnt <= FR_IV_CNT_MAX_V1, xit, {
		error = ERANGE;
	});
	enc_cur += sizeof(cnt);

	ccgcm_init(mode, gcm, key_len, key);

	_construct_iv_v1(iv, dsid, x, cnt);
	ccgcm_set_iv(mode, gcm, FR_IV_SIZE_V1, iv);
	ccgcm_gmac(mode, gcm, 0, NULL);

	ccgcm_update(mode, gcm, dec_len2, enc_cur, dec);
	enc_cur += dec_len2;

	ccgcm_finalize(mode, gcm, FR_TAG_SIZE_V1, tag);

	ret = cc_cmp_safe(FR_TAG_SIZE_V1, enc_cur, tag);
	require_action_quiet(ret == 0, xit, {
		error = EINVAL;
	});

	error = 0;

xit:
	ccgcm_ctx_clear(ccgcm_context_size(mode), gcm);

	if (error) {
		free(dec);
		dec = NULL;
	} else {
		*dec_len = dec_len2;
	}

	return dec;
}

#pragma mark Protocol Messages
static size_t
_version_and_magic_size(void)
{
	return ccder_sizeof_uint64(FR_VERSION) +
			ccder_sizeof_uint64(FR_MAGIC_REQUEST);
}

static uint8_t *
_stamp_version_and_magic(uint8_t *der, uint8_t *der_end, uint64_t which)
{
	uint8_t *der_end2 = der_end;

	der_end2 = ccder_encode_uint64(which, der, der_end2);
	der_end2 = ccder_encode_uint64(FR_VERSION, der, der_end2);

	return der_end2;
}

static uint8_t *
_validate_blob(const uint8_t *der, uint8_t *der_end, uint64_t which, int *error)
{
	uint64_t magic = 0;
	uint64_t version = 0;

	der = ccder_decode_uint64(&version, der, der_end);
	if (version != FR_VERSION) {
		*error = EPROTO;
		return NULL;
	}

	der = ccder_decode_uint64(&magic, der, der_end);
	if (magic != which) {
		*error = EBADRPC;
		return NULL;
	}

	return (uint8_t *)der;
}

static uint8_t *
_create_request_v1(const uint8_t *A_bytes, size_t A_len,
		size_t *der_len, int *error)
{
	uint8_t *der = NULL;
	uint8_t *der_end = NULL;
	int error2 = -1;
	size_t needed = 0;

	needed += _version_and_magic_size();
	needed += ccder_sizeof(CCDER_OCTET_STRING, A_len);

	der = malloc(needed);
	require_action_quiet(der, xit, {
		error2 = errno;
	});

	der_end = der + needed;

	// DER encoding happens back-to-front, so stash the end value and pass to
	// subsequent invocations of the API. In practical terms, if the buffer
	// length is large enough, these encoding calls should not fail, so don't
	// bother to check the return value since we've gone through the trouble of
	// sizing the buffer above.
	der_end = _ccder_shim_encode_octet_string(A_len, A_bytes, der, der_end);
	der_end = _stamp_version_and_magic(der, der_end, FR_MAGIC_REQUEST);
	require_action_quiet(der_end, xit, {
		os_hardware_trap();
	});

	*der_len = needed;
	error2 = 0;

xit:
	if (error2) {
		free(der);
		der = NULL;
	}

	return der;
}

static bool
_decode_request_v1(ccsrp_ctx *ctx, uint8_t **A_bytes, size_t *A_len,
		uint8_t *der, size_t der_len, int *error)
{
	bool result = false;
	int error2 = -1;
	uint8_t *A_bytes2 = NULL;
	size_t A_len2 = 0;
	uint8_t *der_end = der + der_len;

	der = _validate_blob(der, der_end, FR_MAGIC_REQUEST, &error2);
	require_quiet(der, xit);

	der = _ccder_shim_decode_octect_string(&A_len2,
			(const uint8_t **)&A_bytes2, der, der_end);
	require_action_quiet(der, xit, {
		error2 = EINVAL;
	});

	require_action_quiet(A_len2 == ccsrp_ctx_sizeof_n(ctx), xit, {
		error2 = ERANGE;
	});

	result = true;

xit:
	if (result) {
		*A_bytes = A_bytes2;
		*A_len = A_len2;
	} else {
		*error = error2;
	}

	return result;
}

static uint8_t *
_create_challenge_v1(const uint8_t *B_bytes, size_t B_len,
		const uint8_t *salt, size_t salt_len, size_t *der_len, int *error)
{
	uint8_t *der = NULL;
	int error2 = -1;

	uint8_t *der_end = NULL;

	size_t needed = 0;

	needed += _version_and_magic_size();
	needed += ccder_sizeof(CCDER_OCTET_STRING, B_len);
	needed += ccder_sizeof(CCDER_OCTET_STRING, salt_len);

	der = malloc(needed);
	require_action_quiet(der, xit, {
		error2 = errno;
	});

	der_end = der + needed;

	der_end = _ccder_shim_encode_octet_string(salt_len, salt, der, der_end);
	der_end = _ccder_shim_encode_octet_string(B_len, B_bytes, der, der_end);
	der_end = _stamp_version_and_magic(der, der_end, FR_MAGIC_CHALLENGE);
	require_action_quiet(der_end, xit, {
		os_hardware_trap();
	});

	*der_len = needed;
	error2 = 0;

xit:
	if (error2) {
		*error = error2;

		free(der);
		der = NULL;
	}

	return der;
}

static bool
_decode_challenge_v1(ccsrp_ctx *srp, uint8_t **B_bytes, size_t *B_len,
		uint8_t **salt, size_t *salt_len, uint8_t *der, size_t der_len,
		int *error)
{
	bool result = false;
	int error2 = -1;

	uint8_t *B_bytes2 = NULL;
	size_t B_len2 = 0;
	uint8_t *salt2 = NULL;
	size_t salt_len2 = 0;
	uint8_t *der_end = der + der_len;

	der = _validate_blob(der, der_end, FR_MAGIC_CHALLENGE, &error2);
	require_quiet(der, xit);

	der = _ccder_shim_decode_octect_string(&B_len2, (const uint8_t **)&B_bytes2,
			der, der_end);
	require_action_quiet(B_bytes, xit, {
		error2 = EINVAL;
	});

	require_action_quiet(B_len2 == ccsrp_ctx_sizeof_n(srp), xit, {
		error2 = ERANGE;
	});

	der = _ccder_shim_decode_octect_string(&salt_len2, (const uint8_t **)&salt2,
			der, der_end);
	require_action_quiet(der, xit, {
		error2 = EINVAL;
	});

	require_action_quiet(salt_len2 == FR_SALT_LEN, xit, {
		error2 = ERANGE;
	});

	result = true;

xit:
	if (result) {
		*B_bytes = B_bytes2;
		*B_len = B_len2;
		*salt = salt2;
		*salt_len = salt_len2;
	} else {
		*error = error2;
	}

	return result;
}

static uint8_t *
_create_response_v1(const uint8_t *M1_bytes, size_t M1_len,
		const uint8_t *I_bytes, size_t I_len, size_t *der_len,
		int *error)
{
	uint8_t *der = NULL;
	int error2 = -1;

	uint8_t *der_end = NULL;
	size_t needed = 0;

	needed += _version_and_magic_size();
	needed += ccder_sizeof(CCDER_OCTET_STRING, M1_len);
	needed += ccder_sizeof(CCDER_OCTET_STRING, I_len);

	der = malloc(needed);
	require_action_quiet(der, xit, {
		error2 = errno;
	});

	der_end = der + needed;
	der_end = _ccder_shim_encode_octet_string(I_len, I_bytes, der, der_end);
	der_end = _ccder_shim_encode_octet_string(M1_len, M1_bytes, der, der_end);
	der_end = _stamp_version_and_magic(der, der_end, FR_MAGIC_RESPONSE);
	require_action_quiet(der_end, xit, {
		os_hardware_trap();
	});

	*der_len = needed;
	error2 = 0;

xit:
	if (error2) {
		*error = error2;

		free(der);
		der = NULL;
	}

	return der;
}

static bool
_decode_response_v1(ccsrp_ctx *srp, uint8_t **M_bytes, size_t *M_len,
		uint8_t **I_bytes, size_t *I_len,
		uint8_t *der, size_t der_len, int *error)
{
	bool result = false;
	int error2 = -1;

	uint8_t *M_bytes2 = NULL;
	size_t M_len2 = 0;
	uint8_t *I_bytes2 = NULL;
	size_t I_len2 = 0;
	uint8_t *der_end = der + der_len;

	der = _validate_blob(der, der_end, FR_MAGIC_RESPONSE, &error2);
	require_quiet(der, xit);

	der = _ccder_shim_decode_octect_string(&M_len2, (const uint8_t **)&M_bytes2,
			der, der_end);
	require_action_quiet(der, xit, {
		error2 = EINVAL;
	});

	require_action_quiet(M_len2 == ccsrp_session_size(srp), xit, {
		error2 = ERANGE;
	});

	der = _ccder_shim_decode_octect_string(&I_len2,
			(const uint8_t **)&I_bytes2, der, der_end);
	require_action_quiet(der, xit, {
		error2 = EINVAL;
	});

	result = true;

xit:
	if (result) {
		*M_bytes = M_bytes2;
		*M_len = M_len2;

		*I_bytes = I_bytes2;
		*I_len = I_len2;
	} else {
		*error = error2;
	}

	return result;
}

static uint8_t *
_create_hsa2_v1(uint8_t *hsa2code, size_t hsa2code_len,
		uint8_t *HAMK_bytes, size_t HAMK_len, size_t *der_len, int *error)
{
	uint8_t *der = NULL;
	int error2 = -1;

	uint8_t *der_end = NULL;
	size_t needed = 0;

	needed += _version_and_magic_size();
	needed += ccder_sizeof(CCDER_OCTET_STRING, hsa2code_len);
	needed += ccder_sizeof(CCDER_OCTET_STRING, HAMK_len);

	der = malloc(needed);
	require_action_quiet(der, xit, {
		error2 = errno;
	});

	der_end = der + needed;
	der_end = _ccder_shim_encode_octet_string(HAMK_len, HAMK_bytes,
			der, der_end);
	der_end = _ccder_shim_encode_octet_string(hsa2code_len, hsa2code,
			der, der_end);
	der_end = _stamp_version_and_magic(der, der_end, FR_MAGIC_HSA2);
	require_action_quiet(der_end, xit, {
		os_hardware_trap();
	});

	*der_len = needed;
	error2 = 0;

xit:
	if (error2) {
		*error = error2;

		free(der);
		der = NULL;
	}

	return der;
}

static bool
_decode_hsa2_v1(ccsrp_ctx *srp, uint8_t **hsa2_bytes, size_t *hsa2_len,
		uint8_t **HAMK_bytes, size_t *HAMK_len, uint8_t *der, size_t der_len,
		int *error)
{
	bool result = false;
	int error2 = -1;

	uint8_t *hsa2_bytes2 = NULL;
	size_t hsa2_len2 = 0;
	uint8_t *HAMK_bytes2 = NULL;
	size_t HAMK_len2 = 0;
	uint8_t *der_end = der + der_len;

	der = _validate_blob(der, der_end, FR_MAGIC_HSA2, &error2);
	require_quiet(der, xit);

	der = _ccder_shim_decode_octect_string(&hsa2_len2,
			(const uint8_t **)&hsa2_bytes2, der, der_end);
	require_action_quiet(der, xit, {
		error2 = EINVAL;
	});

	der = _ccder_shim_decode_octect_string(&HAMK_len2,
			(const uint8_t **)&HAMK_bytes2, der, der_end);
	require_action_quiet(der, xit, {
		error2 = EINVAL;
	});

	require_action_quiet(HAMK_len2 == ccsrp_session_size(srp), xit, {
		error2 = ERANGE;
	});

	result = true;

xit:
	if (result) {
		*hsa2_bytes = hsa2_bytes2;
		*hsa2_len = hsa2_len2;

		*HAMK_bytes = HAMK_bytes2;
		*HAMK_len = HAMK_len2;
	} else {
		*error = error2;
	}

	return result;
}

#pragma mark Requesting Session
struct __OpaqueSOSForerunnerRequestorSession {
	CFRuntimeBase __cf;

	ccsrp_ctx *rs_srp;
	uint64_t rs_dsid;
	uint64_t rs_packet_cnt;

	uint8_t rs_Z_r2a[FR_Z_SZ_V1];
	uint8_t rs_Z_a2r[FR_Z_SZ_V1];

	CFStringRef rsUsername;
};

static void
_SOSForerunnerRequestorSessionClassInit(CFTypeRef session)
{
	SOSForerunnerRequestorSessionRef self = (void *)session;
	size_t howmuch2zero = sizeof(*self) - sizeof(self->__cf);
	uint8_t *start = (uint8_t *)self + sizeof(self->__cf);

	bzero(start, howmuch2zero);
}

static void
_SOSForerunnerRequestorSessionClassFinalize(CFTypeRef session)
{
	SOSForerunnerRequestorSessionRef self = (void *)session;

	free(self->rs_srp);
	CFReleaseNull(self->rsUsername);
}

static CFRuntimeClass _SOSForerunnerRequestorSessionClass = {
	.version = 0,
	.className = "forerunner requestor session",
	.init = _SOSForerunnerRequestorSessionClassInit,
	.copy = NULL,
	.finalize = _SOSForerunnerRequestorSessionClassFinalize,
	.equal = NULL,
	.hash = NULL,
	.copyFormattingDesc = NULL,
	.copyDebugDesc = NULL,
};

#pragma mark Requestor Class Methods
CFTypeID
SOSForerunnerRequestorSessionGetTypeID(void)
{
	static dispatch_once_t once = 0;
	static CFTypeID tid = 0;

	dispatch_once(&once, ^{
		tid = _CFRuntimeRegisterClass(
				(const CFRuntimeClass * const)
				&_SOSForerunnerRequestorSessionClass);
		if (tid == _kCFRuntimeNotATypeID) {
			os_hardware_trap();
		}
	});

	return tid;
}

#pragma mark Requestor Public Methods
SOSForerunnerRequestorSessionRef
SOSForerunnerRequestorSessionCreate(CFAllocatorRef allocator,
		CFStringRef username, uint64_t dsid)
{
	SOSForerunnerRequestorSessionRef self = NULL;
	int error = -1;
	const size_t xtra = sizeof(*self) - sizeof(self->__cf);
	const struct ccdigest_info *di = ccsha256_di();
	ccdh_const_gp_t gp = ccsrp_gp_rfc5054_3072();

	self = (void *)_CFRuntimeCreateInstance(allocator,
			SOSForerunnerRequestorSessionGetTypeID(), xtra, NULL);
	require_action_quiet(self, xit, {
		error = ENOMEM;
	});

	self->rsUsername = CFRetain(username);
	self->rs_srp = _ccsrp_shim_alloc(di, gp);
	self->rs_dsid = dsid;
	require_action_quiet(self->rs_srp, xit, {
		error = ENOMEM;
	});

	error = 0;

xit:
	if (error) {
		CFReleaseNull(self);
		self = NULL;
	}

	return self;
}

CFDataRef
SOSFRSCopyRequestPacket(SOSForerunnerRequestorSessionRef self,
		CFErrorRef *cferror)
{
	CFDataRef request = NULL;
	int error = -1;

	uint8_t A_bytes[ccsrp_exchange_size(self->rs_srp)];
	size_t A_len = ccsrp_exchange_size(self->rs_srp);
	uint8_t *der = NULL;
	size_t der_len = 0;

	error = ccsrp_client_start_authentication(self->rs_srp,
			ccDRBGGetRngState(), A_bytes);
	require_action_quiet(error == 0, xit, {
		(void)SecCoreCryptoError(error, cferror, CFSTR("failed to start SRP"));
	});

	der = _create_request_v1(A_bytes, A_len, &der_len, &error);
	require_action_quiet(der, xit, {
		// Yes, I know, let's report an allocation error by trying to allocate a
		// bloated pseudo-exception.
		(void)SecPOSIXError(error, cferror,
				CFSTR("failed to allocate response data"));
	});

	request = CFDataCreateWithBytesNoCopy(NULL, der, der_len,
			kCFAllocatorMalloc);
	require_action_quiet(request, xit, {
		error = ENOMEM;
		(void)SecPOSIXError(error, cferror,
				CFSTR("failed to allocate request data"));
	});

xit:
	if (error) {
		if (request) {
			CFRelease(request);
			request = NULL;
		} else {
			free(der);
		}
	}

	return request;
}

CFDataRef
SOSFRSCopyResponsePacket(SOSForerunnerRequestorSessionRef self,
		CFDataRef challenge, CFStringRef secret, CFDictionaryRef peerInfo,
		CFErrorRef *cferror)
{
	CFDataRef response = NULL;
	int error = -1;

	char *username_str = NULL;
	char *secret_str = NULL;

	// Challenge.
	bool result = false;
	uint8_t *der = NULL;
	uint8_t *salt = NULL;
	size_t salt_len = 0;
	uint8_t *B_bytes = NULL;
	size_t B_len = 0;

	// Response.
	uint8_t *resp_der = NULL;
	size_t resp_der_len = 0;
	uint8_t M1_bytes[ccsrp_session_size(self->rs_srp)];
	size_t M1_len = ccsrp_session_size(self->rs_srp);

#if CONFIG_ARM_AUTOACCEPT
	SOSPeerInfoRef peer = NULL;
	CFDataRef cfI = NULL;
#else // CONFIG_ARM_AUTOACCEPT
	const uint8_t fakeI[] = {
		'A',
		'B',
		'C',
		'D',
		'E',
		'F',
	};
#endif // CONFIG_ARM_AUTOACCEPT

	const uint8_t *I_bytes = NULL;
	size_t I_len = 0;
	uint8_t *I_enc_bytes = NULL;
	size_t I_enc_len = 0;

	der = (UInt8 *)CFDataGetBytePtr(challenge);

	username_str = CFStringToCString(self->rsUsername);
	require_quiet(username_str, xit);

	secret_str = CFStringToCString(secret);
	require_quiet(secret_str, xit);

	result = _decode_challenge_v1(self->rs_srp, &B_bytes, &B_len,
			&salt, &salt_len, der, CFDataGetLength(challenge), &error);
	require_action_quiet(result, xit, {
		(void)SecCoreCryptoError(error, cferror,
				CFSTR("failed to decode challenge"));
	});

	// Do not include the null terminator in the length of the secret -- for the
	// purposes of this challenge, it's just a blob of data.
	error = ccsrp_client_process_challenge(self->rs_srp, username_str,
			strlen(secret_str), secret_str, salt_len, salt,
			B_bytes, M1_bytes);
	require_action_quiet(error == 0, xit, {
		(void)SecCoreCryptoError(error, cferror,
				CFSTR("failed to process challenge"));
	});

	_derive_sending_key(self->rs_srp, FR_Z_FROM_REQUESTOR,
			self->rs_Z_r2a, sizeof(self->rs_Z_r2a));

#if CONFIG_ARM_AUTOACCEPT
	peer = SOSCCCopyMyPeerInfo(cferror);
	require_quiet(peer, xit);

	cfI = SOSPeerInfoGetAutoAcceptInfo(peer);
	require_action_quiet(cfI, xit, {
		error = ENOENT;
		(void)SecPOSIXError(error, cferror,
				CFSTR("failed to obtain auto-accept info"));
	});

	I_bytes = CFDataGetBytePtr(cfI);
	I_len = CFDataGetLength(cfI);
#else // CONFIG_ARM_AUTOACCEPT
	I_bytes = fakeI;
	I_len = sizeof(fakeI);
#endif // CONFIG_ARM_AUTOACCEPT

	I_enc_bytes = _encrypt_data_v1(I_bytes, I_len,
			self->rs_dsid, FR_IV_X_REQUEST_V1, self->rs_packet_cnt,
			self->rs_Z_r2a, sizeof(self->rs_Z_r2a), &I_enc_len);
	require_action_quiet(I_enc_bytes, xit, {
		error = ENOMEM;
	});

	self->rs_packet_cnt++;

	resp_der = _create_response_v1(M1_bytes, M1_len, I_enc_bytes, I_enc_len,
			&resp_der_len, &error);
	require_action_quiet(resp_der, xit, {
		(void)SecCoreCryptoError(error, cferror,
				CFSTR("failed to create response"));
	});

	response = CFDataCreateWithBytesNoCopy(NULL, resp_der, resp_der_len,
			kCFAllocatorMalloc);
	require_action_quiet(response, xit, {
		error = ENOMEM;
		(void)SecCoreCryptoError(error, cferror,
				CFSTR("failed to create response"));
	});

	error = 0;

xit:
	free(username_str);
	free(secret_str);

	if (error) {
		if (response) {
			CFRelease(response);
			response = NULL;
		} else {
			free(resp_der);
		}
	}

	return response;
}

CFDataRef
SOSFRSCopyHSA2CodeFromPacket(SOSForerunnerRequestorSessionRef self,
		CFDataRef hsa2packet, CFErrorRef *cferror)
{
	CFDataRef cfhsa2 = NULL;
	int error = -1;

	bool result = false;
	uint8_t *der = NULL;
	size_t der_len = 0;
	uint8_t *hsa2_enc_bytes = NULL;
	size_t hsa2_enc_len = 0;
	uint8_t *hsa2_bytes = NULL;
	size_t hsa2_len = 0;
	uint8_t *HAMK_bytes = NULL;
	size_t HAMK_len = 0;

	der = (UInt8 *)CFDataGetBytePtr(hsa2packet);
	der_len = CFDataGetLength(hsa2packet);

	result = _decode_hsa2_v1(self->rs_srp, &hsa2_enc_bytes, &hsa2_enc_len,
			&HAMK_bytes, &HAMK_len, der, der_len, &error);
	require_quiet(result, xit);

	result = ccsrp_client_verify_session(self->rs_srp, HAMK_bytes);
	require_action_quiet(result, xit, {
		(void)SecPOSIXError(EBADMSG, cferror,
				CFSTR("failed to verify session"));
	});

	_derive_sending_key(self->rs_srp, FR_Z_FROM_ACCEPTOR,
			self->rs_Z_a2r, sizeof(self->rs_Z_a2r));

	hsa2_bytes = _decrypt_data_v1(hsa2_enc_bytes, hsa2_enc_len,
			self->rs_dsid, FR_IV_X_ACCEPT_V1,
			self->rs_Z_a2r, sizeof(self->rs_Z_a2r), &hsa2_len);
	require_action_quiet(hsa2_bytes, xit, {
		error = EINVAL;
	});

	cfhsa2 = CFDataCreateWithBytesNoCopy(NULL, hsa2_bytes, hsa2_len,
			kCFAllocatorMalloc);
	require_action_quiet(cfhsa2, xit, {
		error = ENOMEM;
	});

	error = 0;

xit:
	if (error) {
		if (cfhsa2) {
			CFRelease(cfhsa2);
			cfhsa2 = NULL;
		} else {
			free(hsa2_bytes);
		}
	}

	return cfhsa2;
}

CFDataRef
SOSFRSCopyDecryptedData(SOSForerunnerRequestorSessionRef self,
		CFDataRef encrypted)
{
	CFDataRef decrypted = NULL;
	int error = -1;

	const uint8_t *enc = CFDataGetBytePtr(encrypted);
	size_t enc_len = CFDataGetLength(encrypted);
	uint8_t *dec = NULL;
	size_t dec_len = 0;

	dec = _decrypt_data_v1(enc, enc_len,
			self->rs_dsid, FR_IV_X_ACCEPT_V1,
			self->rs_Z_a2r, sizeof(self->rs_Z_a2r), &dec_len);
	require_action_quiet(dec, xit, {
		error = EINVAL;
	});

	decrypted = CFDataCreateWithBytesNoCopy(NULL, dec, dec_len,
			kCFAllocatorMalloc);
	require_action_quiet(decrypted, xit, {
		error = ENOMEM;
	});

	error = 0;

xit:
	if (error) {
		if (decrypted) {
			CFRelease(decrypted);
			decrypted = NULL;
		} else {
			free(dec);
		}
	}

	return decrypted;
}

#pragma mark Acceptor Session
struct __OpaqueSOSForerunnerAcceptorSession {
	CFRuntimeBase __cf;

	ccsrp_ctx *as_srp;
	uint64_t as_dsid;
	uint64_t as_accept_cnt;
	uint64_t as_packet_cnt;

	uint8_t as_Z_a2r[FR_Z_SZ_V1];
	uint8_t as_Z_r2a[FR_Z_SZ_V1];

	CFStringRef asUsername;
	CFDataRef asCircleSecret;
};

static void
_SOSForerunnerAcceptorSessionClassInit(CFTypeRef session)
{
	SOSForerunnerAcceptorSessionRef self = (void *)session;
	size_t howmuch2zero = sizeof(*self) - sizeof(self->__cf);
	uint8_t *start = (uint8_t *)self + sizeof(self->__cf);

	bzero(start, howmuch2zero);
}

static void
_SOSForerunnerAcceptorSessionClassFinalize(CFTypeRef session)
{
	SOSForerunnerAcceptorSessionRef self = (void *)session;

	free(self->as_srp);
	CFRelease(self->asUsername);
	CFRelease(self->asCircleSecret);
}

static CFRuntimeClass _SOSForerunnerAcceptorSessionClass = {
	.version = 0,
	.className = "forerunner acceptor session",
	.init = _SOSForerunnerAcceptorSessionClassInit,
	.copy = NULL,
	.finalize = _SOSForerunnerAcceptorSessionClassFinalize,
	.equal = NULL,
	.hash = NULL,
	.copyFormattingDesc = NULL,
	.copyDebugDesc = NULL,
};

#pragma mark Acceptor Class Methods
CFTypeID
SOSForerunnerAcceptorSessionGetTypeID(void)
{
	static dispatch_once_t once = 0;
	static CFTypeID tid = 0;

	dispatch_once(&once, ^{
		tid = _CFRuntimeRegisterClass(
				(const CFRuntimeClass * const)
				&_SOSForerunnerAcceptorSessionClass);
		if (tid == _kCFRuntimeNotATypeID) {
			os_hardware_trap();
		}
	});

	return tid;
}

#pragma mark Acceptor Public Methods
SOSForerunnerAcceptorSessionRef
SOSForerunnerAcceptorSessionCreate(CFAllocatorRef allocator,
		CFStringRef username, uint64_t dsid, CFStringRef circleSecret)
{
	SOSForerunnerAcceptorSessionRef self = NULL;
	int error = -1;

	size_t xtra = sizeof(*self) - sizeof(self->__cf);
	char *secret = NULL;
	const struct ccdigest_info *di = ccsha256_di();
	ccdh_const_gp_t gp = ccsrp_gp_rfc5054_3072();

	self = (void *)_CFRuntimeCreateInstance(allocator,
			SOSForerunnerAcceptorSessionGetTypeID(), xtra, NULL);
	require_action_quiet(self, xit, {
		error = ENOMEM;
	});

	self->as_srp = _ccsrp_shim_alloc(di, gp);
	require_action_quiet(self, xit, {
		error = ENOMEM;
	});

	self->as_dsid = dsid;

	secret = CFStringToCString(circleSecret);
	require_action_quiet(secret, xit, {
		error = ENOMEM;
	});

	// We don't care about the null terminating byte.
	self->asCircleSecret = CFDataCreateWithBytesNoCopy(NULL,
			(const UInt8 *)secret, strlen(secret), kCFAllocatorMalloc);
	require_action_quiet(self->asCircleSecret, xit, {
		error = ENOMEM;
	});

	self->asUsername = CFRetain(username);
	error = 0;

xit:
	if (error) {
		if (self && !self->asCircleSecret) {
			free(secret);
		}

		CFReleaseNull(self);
		self = NULL;
	}

	return self;
}

CFDataRef
SOSFASCopyChallengePacket(SOSForerunnerAcceptorSessionRef self,
		CFDataRef requestorPacket, CFErrorRef *cferror)
{
	CFDataRef challenge = NULL;
	int error = -1;
	int ret = -1;

	bool decoded = false;
	char *username_str = NULL;
	uint8_t verifier[ccsrp_ctx_sizeof_n(self->as_srp)];
	uint8_t salt[FR_SALT_LEN];

	uint8_t *der = NULL;
	uint8_t *challenge_der = NULL;
	size_t challenge_len = 0;

	uint8_t *A_bytes = NULL;
	size_t A_len = 0;
	uint8_t B_bytes[ccsrp_exchange_size(self->as_srp)];
	size_t B_len = ccsrp_exchange_size(self->as_srp);

	der = (uint8_t *)CFDataGetBytePtr(requestorPacket);
	decoded = _decode_request_v1(self->as_srp, &A_bytes, &A_len,
			der, CFDataGetLength(requestorPacket), &error);
	require_action_quiet(decoded, xit, {
		(void)SecCoreCryptoError(error, cferror, CFSTR("bad request packet"));
	});

	username_str = CFStringToCString(self->asUsername);
	ret = SecRandomCopyBytes(NULL, sizeof(salt), salt);
	require_action_quiet(ret == 0, xit, {
		error = errno;
		(void)SecPOSIXError(error, cferror, CFSTR("failed to generate salt"));
	});

	error = ccsrp_generate_verifier(self->as_srp, username_str,
			CFDataGetLength(self->asCircleSecret),
			CFDataGetBytePtr(self->asCircleSecret), sizeof(salt), salt,
			verifier);
	require_action_quiet(error == 0, xit, {
		(void)SecCoreCryptoError(error, cferror,
				CFSTR("failed to generate SRP verifier"));
	});

	error = ccsrp_server_start_authentication(self->as_srp, ccDRBGGetRngState(),
			username_str, sizeof(salt), salt, verifier, A_bytes, B_bytes);
	require_action_quiet(error == 0, xit, {
		(void)SecCoreCryptoError(error, cferror,
				CFSTR("could not start server SRP"));
	});

	challenge_der = _create_challenge_v1(B_bytes, B_len,
			salt, sizeof(salt), &challenge_len, &error);
	require_action_quiet(challenge_der, xit, {
		(void)SecPOSIXError(error, cferror,
				CFSTR("could not construct challenge"));
	});

	challenge = CFDataCreateWithBytesNoCopy(NULL, challenge_der, challenge_len,
			kCFAllocatorMalloc);
	error = 0;

xit:
	if (error) {
		if (challenge) {
			CFRelease(challenge);
			challenge = NULL;
		} else {
			free(challenge_der);
		}
	}

    free(username_str);

	return challenge;
}

CFDataRef
SOSFASCopyHSA2Packet(SOSForerunnerAcceptorSessionRef self,
		CFDataRef responsePacket, CFDataRef hsa2code, CFErrorRef *cferror)
{
	CFDataRef hsa2 = NULL;
	int error = -1;

	// Response.
	const uint8_t *der = CFDataGetBytePtr(responsePacket);
	size_t der_len = CFDataGetLength(responsePacket);
	uint8_t *M_bytes = NULL;
	size_t M_len = 0;
	uint8_t *I_enc_bytes = NULL;
	size_t I_enc_len = 0;
	uint8_t *I_bytes = NULL;
	size_t I_len = 0;
	uint8_t HAMK_bytes[ccsrp_session_size(self->as_srp)];

	// HSA2 packet.
	uint8_t *hsa2_bytes = NULL;
	size_t hsa2_len = 0;
	uint8_t *hsa2_enc_bytes = NULL;
	size_t hsa2_enc_len = 0;
	uint8_t *hsa2_packet_bytes = NULL;
	size_t hsa2_packet_len = 0;

	bool result = false;
#if CONFIG_ARM_AUTOACCEPT
	CFDataRef cfI = NULL;
#endif // CONFIG_ARM_AUTOACCEPT

	result = _decode_response_v1(self->as_srp, &M_bytes, &M_len,
			&I_enc_bytes, &I_enc_len, (uint8_t *)der, der_len, &error);
	require_action_quiet(result, xit, {
		(void)SecPOSIXError(error, cferror, CFSTR("bad response"));
	});

	result = ccsrp_server_verify_session(self->as_srp, M_bytes, HAMK_bytes);
	require_action_quiet(result, xit, {
		if (self->as_accept_cnt > FR_MAX_ACCEPTOR_TRIES) {
			error = EBADMSG;
		} else {
			error = EAGAIN;
			self->as_accept_cnt++;
		}

		(void)SecPOSIXError(error, cferror,
				CFSTR("session verification failed"));
	});

	_derive_sending_key(self->as_srp, FR_Z_FROM_ACCEPTOR,
			self->as_Z_a2r, sizeof(self->as_Z_a2r));

	hsa2_bytes = (uint8_t *)CFDataGetBytePtr(hsa2code);
	hsa2_len = CFDataGetLength(hsa2code);

	hsa2_enc_bytes = _encrypt_data_v1(hsa2_bytes, hsa2_len,
			self->as_dsid, FR_IV_X_ACCEPT_V1, self->as_packet_cnt,
			self->as_Z_a2r, sizeof(self->as_Z_a2r), &hsa2_enc_len);
	require_action_quiet(hsa2_enc_bytes, xit, {
		error = ENOMEM;
	});

	self->as_packet_cnt++;

	hsa2_packet_bytes = _create_hsa2_v1(hsa2_enc_bytes, hsa2_enc_len,
			HAMK_bytes, sizeof(HAMK_bytes), &hsa2_packet_len, &error);
	require_quiet(hsa2_packet_bytes, xit);

	hsa2 = CFDataCreateWithBytesNoCopy(NULL, hsa2_packet_bytes, hsa2_packet_len,
			kCFAllocatorMalloc);
	require_action_quiet(hsa2, xit, {
		error = ENOMEM;
		(void)SecPOSIXError(error, cferror,
				CFSTR("could not create hsa2 packet"));
	});

	_derive_sending_key(self->as_srp, FR_Z_FROM_REQUESTOR,
			self->as_Z_r2a, sizeof(self->as_Z_r2a));

	I_bytes = _decrypt_data_v1(I_enc_bytes, I_enc_len,
			self->as_dsid, FR_IV_X_REQUEST_V1,
			self->as_Z_r2a, sizeof(self->as_Z_r2a), &I_len);
	require_action_quiet(I_bytes, xit, {
		error = EINVAL;
	});

#if CONFIG_ARM_AUTOACCEPT
	cfI = CFDataCreateWithBytesNoCopy(NULL, I_bytes, I_len, kCFAllocatorMalloc);
	require_action_quiet(cfI, xit, {
		error = ENOMEM;
		(void)SecPOSIXError(error, cferror,
				CFSTR("could not create identity data"));
	});

	result = SOSCCSetAutoAcceptInfo(cfI, cferror);
	require_quiet(result, xit);
#endif // CONFIG_ARM_AUTOACCEPT

	error = 0;

xit:
	if (error) {
		if (hsa2) {
			CFRelease(hsa2);
			hsa2 = NULL;
		} else {
			free(hsa2_packet_bytes);
		}
	}

	free(hsa2_enc_bytes);

#if CONFIG_ARM_AUTOACCEPT
	if (cfI) {
		CFRelease(cfI);
	} else {
		free(I_bytes);
	}
#else // CONFIG_ARM_AUTOACCEPT
	free(I_bytes);
#endif // CONFIG_ARM_AUTOACCEPT

	return hsa2;
}

CFDataRef
SOSFASCopyEncryptedData(SOSForerunnerAcceptorSessionRef self, CFDataRef data)
{
	CFDataRef encrypted = NULL;
	int error = -1;

	uint8_t *enc = NULL;
	size_t enc_len = 0;

	enc = _encrypt_data_v1(CFDataGetBytePtr(data), CFDataGetLength(data),
			self->as_dsid, FR_IV_X_ACCEPT_V1, self->as_packet_cnt,
			self->as_Z_a2r, sizeof(self->as_Z_a2r), &enc_len);
	require_action_quiet(enc, xit, {
		error = EINVAL;
	});

	encrypted = CFDataCreateWithBytesNoCopy(NULL, enc, enc_len,
			kCFAllocatorMalloc);
	require_action_quiet(encrypted, xit, {
		error = ENOMEM;
	});

	error = 0;

xit:
	if (error) {
		if (encrypted) {
			CFRelease(encrypted);
			encrypted = NULL;
		} else {
			free(enc);
		}
	}

	return encrypted;
}
