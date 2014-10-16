/*
 * cspdlTesting.h - workaround flags for testing CSPDL using CSP-oriented tests.
 */

#ifndef	_CSPDL_TESTING_H_
#define _CSPDL_TESTING_H_

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * All generated keys must be reference keys.
 */
#define CSPDL_ALL_KEYS_ARE_REF		1

/*
 * 2nd/public key in two-key FEE ops must be raw. This is because the Security 
 * Server doesn't go in and deal with ref keys which are only found in a
 * Context.
 */
#define CSPDL_2ND_PUB_KEY_IS_RAW	1

/*
 * Ease off on restriction of ptext size == ctext size in case of symmetric 
 * en/decrypt with no padding. The sizes will be equal, but we can't ensure
 * that by mallocing exactly the right amount after because CSPDL doesn't
 * give an exact (proper) outputSize in this case (yet).
 */
#define CSPDL_NOPAD_ENFORCE_SIZE	1

/*
 * CSPDL can't do SHA1HMAC_LEGACY with bug-for-bug compatibility with 
 * BSAFE (since the bug-for-bug feature involves doing actual HMAC updates
 * exactly as the app presents them).
 */
#define CSPDL_SHA1HMAC_LEGACY_ENABLE	0

/*
 * CSPDL does not support DSA GenerateAlgorithmParameters. Let the secure CSP
 * do it implicitly during key gen.
 */
#define CSPDL_DSA_GEN_PARAMS			0

/*
 * Can't generate keys with CSSM_KEYATTR_PRIVATE. Is this a bug or a feature?
 * Nobody pays any attention to this except the CSP, which rejects it. Shouldn't
 * either CSPDL or SS look at this and strip it off before sending the request
 * down to the CSP?
 */
#define CSPDL_KEYATTR_PRIVATE			0

/* 
 * ObtainPrivateKeyFromPublic key not implemented yet (if ever).
 */
#define CSPDL_OBTAIN_PRIV_FROM_PUB		0

/*** Workarounds for badattr test only ***/
 
/*
 * Munged header fields in a ref key should result in CSP_INVALID_KEY_REFERENCE,
 * but work fine.
 */
#define CSPDL_MUNGE_HEADER_CHECK		0

/* 
 * ALWAYS_SENSITIVE, NEVER_EXTRACTABLE are ignored, should result in 
 * CSP_INVALID_KEYATTR_MASK at key gen time.
 * FIXED per Radar 2879872.
 */
#define CSPDL_ALWAYS_SENSITIVE_CHECK	1
#define CSPDL_NEVER_EXTRACTABLE_CHECK	1

/*** end of badattr workarounds ***/

/* 
 * <rdar://problem/3732910> certtool can't generate keypair
 *
 * Until this is fixed - actually the underlying problem is in securityd - 
 * CSPDL can not generate a key pair without private and public both being 
 * PERMANENT.
 */
#define CSPDL_ALL_KEYS_ARE_PERMANENT	0


/***
 *** Other differences/bugs/oddities.
 ***/
 
/*
 * 1. SS wraps (encrypt) public keys when encoding them, thus the CSP has to allow
 *    wrapping of public keys. This may not be what we really want. See
 *    AppleCSP/AppleCSP/wrapKey.cpp for workaround per ALLOW_PUB_KEY_WRAP.
 */
 
#ifdef	__cplusplus
}
#endif

#endif	/* _CSPDL_TESTING_H_ */
