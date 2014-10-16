/*
 * Copyright (c) 2008-2013 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * EAPSIMAKAUtil.c
 * - common definitions/routines for EAP-SIM and EAP-AKA
 */

#include "EAPSIMAKAUtil.h"
#include <stddef.h>
#include <stdlib.h>
#include "EAPLog.h"
#include "symbol_scope.h"
#include "nbo.h"
#include "printdata.h"
#include "EAP.h"
#include "EAPUtil.h"
#include "fips186prf.h"
#include "myCFUtil.h"
#include <string.h>
#include <CommonCrypto/CommonHMAC.h>

/* 
 * Modification History
 *
 * October 8, 2012	Dieter Siegmund (dieter@apple)
 * - created (from eapsim_plugin.c)
 */

PRIVATE_EXTERN const char *
EAPSIMAKAPacketSubtypeGetString(EAPSIMAKAPacketSubtype subtype)
{
    static char		buf[8];
    const char *	ret;

    switch (subtype) {
	/* EAP-AKA only */
    case kEAPSIMAKAPacketSubtypeAKAChallenge:
	ret = "Challenge";
	break;
    case kEAPSIMAKAPacketSubtypeAKAAuthenticationReject:
	ret = "Authentication Reject";
	break;
    case kEAPSIMAKAPacketSubtypeAKASynchronizationFailure:
	ret = "Synchronization Failure";
	break;
    case kEAPSIMAKAPacketSubtypeAKAIdentity:
	ret = "Identity";
	break;
	
	/* EAP-SIM only */
    case kEAPSIMAKAPacketSubtypeSIMStart :
	ret = "Start";
	break;
    case kEAPSIMAKAPacketSubtypeSIMChallenge :
	ret = "Challenge";
	break;
	
	/* EAP-AKA and EAP-SIM */
    case kEAPSIMAKAPacketSubtypeNotification :
	ret = "Notification";
	break;
    case kEAPSIMAKAPacketSubtypeReauthentication :
	ret = "Reauthentication";
	break;
    case kEAPSIMAKAPacketSubtypeClientError :
	ret = "Client Error";
	break;
    default:
	snprintf(buf, sizeof(buf), "%d", subtype);
	ret = buf;
	break;
    }
    return (ret);
}

PRIVATE_EXTERN const char *
EAPSIMAKAAttributeTypeGetString(EAPSIMAKAAttributeType attr)
{
    static char		buf[8];

    switch (attr) {
    case kAT_RAND:
	return "AT_RAND";
    case kAT_AUTN:
	return "AT_AUTN";
    case kAT_RES:
	return "AT_RES";
    case kAT_AUTS:
	return "AT_AUTS";
    case kAT_PADDING:
	return "AT_PADDING";
    case kAT_NONCE_MT:
	return "AT_NONCE_MT";
    case kAT_PERMANENT_ID_REQ:
	return "AT_PERMANENT_ID_REQ";
    case kAT_MAC:
	return "AT_MAC";
    case kAT_NOTIFICATION:
	return "AT_NOTIFICATION";
    case kAT_ANY_ID_REQ:
	return "AT_ANY_ID_REQ";
    case kAT_IDENTITY:
	return "AT_IDENTITY";
    case kAT_VERSION_LIST:
	return "AT_VERSION_LIST";
    case kAT_SELECTED_VERSION:
	return "AT_SELECTED_VERSION";
    case kAT_FULLAUTH_ID_REQ:
	return "AT_FULLAUTH_ID_REQ";
    case kAT_COUNTER:
	return "AT_COUNTER";
    case kAT_COUNTER_TOO_SMALL:
	return "AT_COUNTER_TOO_SMALL";
    case kAT_NONCE_S:
	return "AT_NONCE_S";
    case kAT_CLIENT_ERROR_CODE:
	return "AT_CLIENT_ERROR_CODE";
    case kAT_IV:
	return "AT_IV";
    case kAT_ENCR_DATA:
	return "AT_ENCR_DATA";
    case kAT_NEXT_PSEUDONYM:
	return "AT_NEXT_PSEUDONYM";
    case kAT_NEXT_REAUTH_ID:
	return "AT_NEXT_REAUTH_ID";
    case kAT_CHECKCODE:
	return "AT_CHECKCODE";
    case kAT_RESULT_IND:
	return "AT_RESULT_IND";
    default:
	snprintf(buf, sizeof(buf), "%d", attr);
	return (buf);
    }
}

PRIVATE_EXTERN const char *
ATNotificationCodeGetString(uint16_t code)
{
    const char *	str;

    switch (code) {
    case kATNotificationCodeGeneralFailureAfterAuthentication:
	str = "General Failure After Authentication";
	break;
    case kATNotificationCodeGeneralFailureBeforeAuthentication:
	str = "General Failure Before Authentication";
	break;
    case kATNotificationCodeSuccess:
	str = "Success";
	break;
    case kATNotificationCodeTemporarilyDeniedAccess:
	str = "Temporarily Denied Access";
	break;
    case kATNotificationCodeNotSubscribed:
	str = "Not Subscribed";
	break;
    default:
	str = NULL;
	break;
    }
    return (str);
}

/**
 ** Miscellaneous utilities
 **/

PRIVATE_EXTERN CFStringRef
EAPSIMAKAPacketCopyDescription(const EAPPacketRef pkt, bool * packet_is_valid)
{
    int			attrs_length;
    EAPSIMAKAPacketRef	simaka = (EAPSIMAKAPacketRef)pkt;
    uint16_t		length = EAPPacketGetLength(pkt);
    CFMutableStringRef	str = NULL;
    TLVListDeclare(	tlvs_p);
    bool		valid = FALSE;

    switch (pkt->code) {
    case kEAPCodeRequest:
    case kEAPCodeResponse:
	break;
    default:
	goto done;
    }
    str = CFStringCreateMutable(NULL, 0);
    if (length < kEAPSIMAKAPacketHeaderLength) {
	STRING_APPEND(str, "EAPSIMAKAPacket truncated header %d < %d\n",
		      length, (int)kEAPSIMAKAPacketHeaderLength);
	goto done;
    }
    attrs_length = length - kEAPSIMAKAPacketHeaderLength;
    STRING_APPEND(str,
		  "%s %s: Identifier %d Length %d [%s] Length %d\n",
		  EAPTypeStr(simaka->type),
		  pkt->code == kEAPCodeRequest ? "Request" : "Response",
		  pkt->identifier, length, 
		  EAPSIMAKAPacketSubtypeGetString(simaka->subtype), attrs_length);
    if (attrs_length != 0) {
	CFStringRef	tlvs_str;

	TLVListInit(tlvs_p);
	if (TLVListParse(tlvs_p, simaka->attrs, attrs_length) == FALSE) {
	    STRING_APPEND(str, "failed to parse TLVs: %s\n",
			  TLVListErrorString(tlvs_p));
	    goto done;
	}
	tlvs_str = TLVListCopyDescription(tlvs_p);
	TLVListFree(tlvs_p);
	STRING_APPEND(str, "%@", tlvs_str);
	CFRelease(tlvs_str);
    }
    valid = TRUE;

 done:
    *packet_is_valid = valid;
    return (str);
}

PRIVATE_EXTERN EAPSIMAKAStatus
EAPSIMAKAStatusForATNotificationCode(uint16_t notification_code)
{
    EAPSIMAKAStatus	status = kEAPSIMAKAStatusOK;

    switch (notification_code) {
    case kATNotificationCodeGeneralFailureAfterAuthentication:
	status = kEAPSIMAKAStatusFailureAfterAuthentication;
	break;
    case kATNotificationCodeGeneralFailureBeforeAuthentication:
	status = kEAPSIMAKAStatusFailureBeforeAuthentication;
	break;
    case kATNotificationCodeSuccess:
	status = kEAPSIMAKAStatusOK;
	break;
    case kATNotificationCodeTemporarilyDeniedAccess:
	status = kEAPSIMAKAStatusAccessTemporarilyDenied;
	break;
    case kATNotificationCodeNotSubscribed:
	status = kEAPSIMAKAStatusNotSubscribed;
	break;
    default:
	status = kEAPSIMAKAStatusUnrecognizedNotification;
	break;
    }
    return (status);
}

/*
 * Function: EAPSIMAKAKeyInfoComputeMAC
 * Purpose:
 *    Compute the MAC value in the AT_MAC attribute using the
 *    specified EAP packet 'pkt' and assuming 'mac_p' points to 
 *    an area within 'pkt' that holds the MAC value.
 *
 *    This function figures out how much data comes before the 'mac_p'
 *    value and how much comes after, and feeds the before, zero-mac, and after
 *    bytes into the HMAC-SHA1 algorithm.  It also includes the 'extra'
 *    value, whose value depends on which packet is being MAC'd.
 * Returns:
 *    'hash' value is filled in with HMAC-SHA1 results.
 */
PRIVATE_EXTERN void
EAPSIMAKAKeyInfoComputeMAC(EAPSIMAKAKeyInfoRef key_info_p,
			   EAPPacketRef pkt,
			   const uint8_t * mac_p, 
			   const uint8_t * extra, int extra_length,
			   uint8_t hash[CC_SHA1_DIGEST_LENGTH])
{
    int			after_mac_size;
    int			before_mac_size;
    CCHmacContext	ctx;
    int			pkt_len = EAPPacketGetLength(pkt);
    uint8_t		zero_mac[MAC_SIZE];

    bzero(&zero_mac, sizeof(zero_mac));
    before_mac_size = (int)(mac_p - (const uint8_t *)pkt);
    after_mac_size = pkt_len - (before_mac_size + sizeof(zero_mac));

    /* compute the hash */
    CCHmacInit(&ctx, kCCHmacAlgSHA1, key_info_p->s.k_aut,
	       sizeof(key_info_p->s.k_aut));
    CCHmacUpdate(&ctx, pkt, before_mac_size);
    CCHmacUpdate(&ctx, zero_mac, sizeof(zero_mac));
    CCHmacUpdate(&ctx, mac_p + sizeof(zero_mac), after_mac_size);
    if (extra != NULL) {
	CCHmacUpdate(&ctx, extra, extra_length);
    }
    CCHmacFinal(&ctx, hash);
    return;
}

PRIVATE_EXTERN bool
EAPSIMAKAKeyInfoVerifyMAC(EAPSIMAKAKeyInfoRef key_info,
			  EAPPacketRef pkt,
			  const uint8_t * mac_p,
			  const uint8_t * extra, int extra_length)
{
    uint8_t		hash[CC_SHA1_DIGEST_LENGTH];

    EAPSIMAKAKeyInfoComputeMAC(key_info, pkt, mac_p, extra, extra_length, hash);
    return (bcmp(hash, mac_p, MAC_SIZE) == 0);
}

PRIVATE_EXTERN void
EAPSIMAKAKeyInfoSetMAC(EAPSIMAKAKeyInfoRef key_info,
		       EAPPacketRef pkt,
		       uint8_t * mac_p,
		       const uint8_t * extra, int extra_length)
{
    uint8_t		hash[CC_SHA1_DIGEST_LENGTH];

    EAPSIMAKAKeyInfoComputeMAC(key_info, pkt, mac_p, extra, extra_length, hash);
    bcopy(hash, mac_p, MAC_SIZE);
    return;
}

#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonCryptor.h>

PRIVATE_EXTERN uint8_t *
EAPSIMAKAKeyInfoDecryptTLVList(EAPSIMAKAKeyInfoRef key_info_p,
			       AT_ENCR_DATA * encr_data_p, AT_IV * iv_p,
			       TLVListRef decrypted_tlvs_p)
{
    CCCryptorRef	cryptor = NULL;
    size_t		buf_used;
    uint8_t *		decrypted_buffer = NULL;
    int			encr_data_len;
    CCCryptorStatus 	status;
    bool		success = FALSE;

    encr_data_len = encr_data_p->ed_length * TLV_ALIGNMENT
	- offsetof(AT_ENCR_DATA, ed_encrypted_data);
    decrypted_buffer = (uint8_t *)malloc(encr_data_len);
    status = CCCryptorCreate(kCCDecrypt,
			     kCCAlgorithmAES128,
			     0,
			     key_info_p->s.k_encr,
			     sizeof(key_info_p->s.k_encr),
			     iv_p->iv_initialization_vector,
			     &cryptor);
    if (status != kCCSuccess) {
	EAPLOG_FL(LOG_NOTICE, "CCCryptoCreate failed with %d", status);
	goto done;
    }
    status = CCCryptorUpdate(cryptor,
			     encr_data_p->ed_encrypted_data,
			     encr_data_len,
			     decrypted_buffer,
			     encr_data_len,
			     &buf_used);
    if (status != kCCSuccess) {
	EAPLOG_FL(LOG_NOTICE, "CCCryptoUpdate failed with %d", status);
	goto done;
    }
    if (buf_used != encr_data_len) {
	EAPLOG_FL(LOG_NOTICE,
		  "decryption consumed %d bytes (!= %d bytes)",
		  (int)buf_used, encr_data_len);
	goto done;
    }
    if (TLVListParse(decrypted_tlvs_p, decrypted_buffer, encr_data_len)
	== FALSE) {
	EAPLOG_FL(LOG_NOTICE,
		  "TLVListParse failed on AT_ENCR_DATA, %s",
		  TLVListErrorString(decrypted_tlvs_p));
	goto done;
    }
    success = TRUE;

 done:
    if (cryptor != NULL) {
	status = CCCryptorRelease(cryptor);
	if (status != kCCSuccess) {
	    EAPLOG_FL(LOG_NOTICE, "CCCryptoRelease failed with %d", status);
	}
    }
    if (success == FALSE && decrypted_buffer != NULL) {
	free(decrypted_buffer);
	decrypted_buffer = NULL;
    }
    return (decrypted_buffer);
}

STATIC bool
EAPSIMAKAKeyInfoEncrypt(EAPSIMAKAKeyInfoRef key_info_p, const uint8_t * iv_p,
			const uint8_t * clear, int size, uint8_t * encrypted)
{
    size_t		buf_used;
    CCCryptorRef	cryptor;
    bool		ret = FALSE;
    CCCryptorStatus 	status;

    status = CCCryptorCreate(kCCEncrypt,
			     kCCAlgorithmAES128,
			     0,
			     key_info_p->s.k_encr,
			     sizeof(key_info_p->s.k_encr),
			     iv_p,
			     &cryptor);
    if (status != kCCSuccess) {
	EAPLOG_FL(LOG_NOTICE, "encrypt CCCryptoCreate failed with %d", status);
	goto done;
    }
    status = CCCryptorUpdate(cryptor, clear, size, encrypted, size, &buf_used);
    if (status != kCCSuccess) {
	EAPLOG_FL(LOG_NOTICE, "encrypt CCCryptoUpdate failed with %d", status);
	goto done;
    }
    if (buf_used != size) {
	EAPLOG_FL(LOG_NOTICE,
		  "encryption consumed %d, should have been %d",
		  (int)buf_used, size);
	goto done;
    }
    ret = TRUE;

 done:
    status = CCCryptorRelease(cryptor);
    if (status != kCCSuccess) {
	EAPLOG_FL(LOG_NOTICE, "CCCryptoRelease failed with %d", status);
    }
    return (ret);
}

STATIC void
fill_with_random(uint8_t * buf, int len)
{
    int             i;
    int             n;
    void *          p;
    uint32_t        random;

    n = len / sizeof(random);
    for (i = 0, p = buf; i < n; i++, p += sizeof(random)) {
        random = arc4random();
        bcopy(&random, p, sizeof(random));
    }
    return;
}

PRIVATE_EXTERN bool
EAPSIMAKAKeyInfoEncryptTLVs(EAPSIMAKAKeyInfoRef key_info,
			    TLVBufferRef tb_p, TLVBufferRef tb_add_p)
{
    int			buf_used;
    int			padding_length;
    AT_ENCR_DATA *	encr_data_p;
    AT_IV *		iv_p;
    bool		ret = FALSE;
    TLVListDeclare(	temp);

    buf_used = TLVBufferUsed(tb_add_p);
    padding_length = AT_ENCR_DATA_ROUNDUP(buf_used) - buf_used;

    if (padding_length != 0
	&& TLVBufferAddPadding(tb_add_p, padding_length) == FALSE) {
	EAPLOG_FL(LOG_NOTICE, "failed to add AT_PADDING, %s",
		  TLVBufferErrorString(tb_p));
	goto done;
    }
    
    if (TLVBufferUsed(tb_add_p) != TLVBufferMaxSize(tb_add_p)) {
	EAPLOG_FL(LOG_NOTICE, "nested encrypted TLVs length %d != %d",
		  TLVBufferUsed(tb_add_p), TLVBufferMaxSize(tb_add_p));
	goto done;
    }

    TLVListInit(temp);
    if (TLVListParse(temp, TLVBufferStorage(tb_add_p),
		     TLVBufferUsed(tb_add_p)) == FALSE) {
	EAPLOG_FL(LOG_NOTICE, "nested TLVs TLVListParse failed, %s",
		  TLVListErrorString(temp));
	goto done;
    }
    {
	CFStringRef		str;

	str = TLVListCopyDescription(temp);
	TLVListFree(temp);
	EAPLOG(LOG_DEBUG, "Encrypted TLVs:\n%@", str);
	CFRelease(str);
    }

    /* AT_IV */
    iv_p = (AT_IV *) TLVBufferAllocateTLV(tb_p, kAT_IV, sizeof(AT_IV));
    if (iv_p == NULL) {
	EAPLOG_FL(LOG_NOTICE, "failed to allocate AT_IV, %s",
		  TLVBufferErrorString(tb_p));
	goto done;
    }
    net_uint16_set(iv_p->iv_reserved, 0);
    fill_with_random(iv_p->iv_initialization_vector, 
		     sizeof(iv_p->iv_initialization_vector));

    /* AT_ENCR_DATA */
    encr_data_p = (AT_ENCR_DATA *)
	TLVBufferAllocateTLV(tb_p, kAT_ENCR_DATA,
			     offsetof(AT_ENCR_DATA, ed_encrypted_data)
			     + TLVBufferUsed(tb_add_p));
    if (encr_data_p == NULL) {
	EAPLOG_FL(LOG_NOTICE, "failed to allocate AT_ENCR_DATA, %s",
		  TLVBufferErrorString(tb_add_p));
	goto done;
    }
    net_uint16_set(encr_data_p->ed_reserved, 0);
    if (!EAPSIMAKAKeyInfoEncrypt(key_info, iv_p->iv_initialization_vector,
				 TLVBufferStorage(tb_add_p), 
				 TLVBufferUsed(tb_add_p),
				 encr_data_p->ed_encrypted_data)) {
	EAPLOG_FL(LOG_NOTICE, "failed to encrypt AT_ENCR_DATA");
	goto done;
    }
    ret = TRUE;

 done:
    return (ret);
}

PRIVATE_EXTERN void
EAPSIMAKAKeyInfoComputeReauthKey(EAPSIMAKAKeyInfoRef key_info,
				 EAPSIMAKAPersistentStateRef persist,
				 const void * identity,
				 int identity_length,
				 AT_COUNTER * counter_p,
				 AT_NONCE_S * nonce_s_p)
{
    CC_SHA1_CTX		sha1_context;
    EAPSIMAKAKeyInfo	temp_key_info;
    uint8_t		xkey[CC_SHA1_DIGEST_LENGTH];

    /*
     * generate the XKEY':
     * XKEY' = SHA1(Identity|counter|NONCE_S| MK)
     */
    CC_SHA1_Init(&sha1_context);
    CC_SHA1_Update(&sha1_context, identity, identity_length);
    CC_SHA1_Update(&sha1_context, counter_p->co_counter,
		   sizeof(counter_p->co_counter));
    CC_SHA1_Update(&sha1_context, nonce_s_p->nc_nonce_s,
		   sizeof(nonce_s_p->nc_nonce_s));
    CC_SHA1_Update(&sha1_context, 
		   EAPSIMAKAPersistentStateGetMasterKey(persist),
		   EAPSIMAKAPersistentStateGetMasterKeySize(persist));
    CC_SHA1_Final(xkey, &sha1_context);

    /* now run PRF to generate keying material */
    fips186_2prf(xkey, temp_key_info.key);

    /* copy the new MSK */
    bcopy(temp_key_info.key,
	  key_info->s.msk,
	  sizeof(key_info->s.msk));

    /* copy the new EMSK */
    bcopy(temp_key_info.key + sizeof(temp_key_info.s.msk),
	  key_info->s.emsk,
	  sizeof(key_info->s.emsk));
    return;
}

PRIVATE_EXTERN EAPSIMAKAAttributeType
EAPSIMAKAIdentityTypeGetAttributeType(CFStringRef string)
{
    EAPSIMAKAAttributeType	type = kAT_ANY_ID_REQ;

    if (string != NULL) {
	if (CFEqual(kEAPSIMAKAIdentityTypeFullAuthentication, string)) {
	    type = kAT_FULLAUTH_ID_REQ;
	}
	else if (CFEqual(kEAPSIMAKAIdentityTypePermanent, string)) {
	    type = kAT_PERMANENT_ID_REQ;
	}
    }
    return (type);
}

/**
 ** TLVBuffer routines
 **/

#define NBITS_PER_BYTE	8

struct TLVBuffer {
    uint8_t *		storage;
    int			size;
    int			offset;
    char		err_str[160];
};

PRIVATE_EXTERN int
TLVBufferSizeof(void)
{
    return (sizeof(struct TLVBuffer));
}

PRIVATE_EXTERN int
TLVBufferUsed(TLVBufferRef tb)
{
    return (tb->offset);
}

PRIVATE_EXTERN const char *
TLVBufferErrorString(TLVBufferRef tb)
{
    return (tb->err_str);
}

PRIVATE_EXTERN int
TLVBufferMaxSize(TLVBufferRef tb)
{
    return (tb->size);
}

PRIVATE_EXTERN uint8_t *
TLVBufferStorage(TLVBufferRef tb)
{
    return (tb->storage);
}

PRIVATE_EXTERN void
TLVBufferInit(TLVBufferRef tb, uint8_t * storage, int size)
{
    tb->storage = storage;
    tb->size = size;
    tb->offset = 0;
    tb->err_str[0] = '\0';
    return;
}

PRIVATE_EXTERN TLVRef
TLVBufferAllocateTLV(TLVBufferRef tb, EAPSIMAKAAttributeType type, int length)
{
    int		left;
    int		padded_length;
    TLVRef	tlv_p;
    
    if (length < offsetof(TLV, tlv_value)) {
	return (NULL);
    }
    padded_length = TLVRoundUp(length);
    if (padded_length > TLV_MAX_LENGTH) {
	snprintf(tb->err_str, sizeof(tb->err_str),
		 "padded_length %d > max length %d",
		 padded_length, TLV_MAX_LENGTH);
	return (NULL);
    }
    left = tb->size - tb->offset;
    if (left < padded_length) {
	snprintf(tb->err_str, sizeof(tb->err_str),
		 "available space %d < required %d",
		 left, padded_length);
	return (NULL);
    }

    /* set the type and length */
    tlv_p = (TLVRef)(tb->storage + tb->offset);
    tlv_p->tlv_type = type;
    tlv_p->tlv_length = padded_length / TLV_ALIGNMENT;
    tb->offset += padded_length;
    return (tlv_p);
}

PRIVATE_EXTERN Boolean
TLVBufferAddIdentity(TLVBufferRef tb_p, 
		     const uint8_t * identity, int identity_length)
{
    AttrUnion		attr;

    attr.tlv_p = TLVBufferAllocateTLV(tb_p,
				      kAT_IDENTITY,
				      offsetof(AT_IDENTITY, id_identity)
				      + identity_length);
    if (attr.tlv_p == NULL) {
	return (FALSE);
    }
    net_uint16_set(attr.at_identity->id_actual_length, identity_length);
    bcopy(identity, attr.at_identity->id_identity, identity_length);
    return (TRUE);
}

PRIVATE_EXTERN Boolean
TLVBufferAddIdentityString(TLVBufferRef tb_p, CFStringRef identity,
			   CFDataRef * ret_data)
{
    CFDataRef	data;
    Boolean	result;

    *ret_data = NULL;
    data = CFStringCreateExternalRepresentation(NULL, identity, 
						kCFStringEncodingUTF8, 0);
    if (data == NULL) {
	return (FALSE);
    }
    result = TLVBufferAddIdentity(tb_p, CFDataGetBytePtr(data),
				  (int)CFDataGetLength(data));
    if (result == TRUE && ret_data != NULL) {
	*ret_data = data;
    }
    else {
	CFRelease(data);
    }
    return (result);

}

PRIVATE_EXTERN Boolean
TLVBufferAddCounter(TLVBufferRef tb_p, uint16_t at_counter)
{
    AT_COUNTER *	counter_p;

    counter_p = (AT_COUNTER *)TLVBufferAllocateTLV(tb_p, kAT_COUNTER,
						   sizeof(AT_COUNTER));
    if (counter_p == NULL) {
	return (FALSE);
    }
    net_uint16_set(counter_p->co_counter, at_counter);
    return (TRUE);
}

PRIVATE_EXTERN Boolean
TLVBufferAddCounterTooSmall(TLVBufferRef tb_p)
{
    AT_COUNTER_TOO_SMALL * counter_too_small_p;
	
    counter_too_small_p = (AT_COUNTER_TOO_SMALL *)
	TLVBufferAllocateTLV(tb_p, kAT_COUNTER_TOO_SMALL,
			     sizeof(AT_COUNTER_TOO_SMALL));
    if (counter_too_small_p == NULL) {
	return (FALSE);
    }
    net_uint16_set(counter_too_small_p->cs_reserved, 0);
    return (TRUE);
}

PRIVATE_EXTERN Boolean
TLVBufferAddPadding(TLVBufferRef tb_p, int padding_length)
{
    AT_PADDING *	padding_p;

    switch (padding_length) {
    case 4:
    case 8:
    case 12:
	break;
    default:
	snprintf(tb_p->err_str, sizeof(tb_p->err_str),
		 "invalid AT_PADDING %d", padding_length);
	return (FALSE);
    }
    padding_p = (AT_PADDING *)
	TLVBufferAllocateTLV(tb_p, kAT_PADDING, padding_length);
    if (padding_p == NULL) {
	strlcpy(tb_p->err_str,"couldn't allocate TLV", sizeof(tb_p->err_str));
	return (FALSE);
    }
    bzero(padding_p->pa_padding,
	  padding_length - offsetof(AT_PADDING, pa_padding));
    return (TRUE);
}

/**
 ** TLVList routines
 **/
#define N_ATTRS_STATIC			10
struct TLVList {
    const void * *	attrs;		/* pointers to attributes */
    const void *	attrs_static[N_ATTRS_STATIC];
    int			count;
    int			size;
    char		err_str[160];
};

PRIVATE_EXTERN int
TLVListSizeof(void)
{
    return (sizeof(struct TLVList));
}

INLINE int
TLVListAttrsStaticSize(void)
{
    const TLVListRef	tlvs_p;

    return ((int)sizeof(tlvs_p->attrs_static)
	    / sizeof(tlvs_p->attrs_static[0]));
}

PRIVATE_EXTERN const char *
TLVListErrorString(TLVListRef tlvs_p)
{
    return (tlvs_p->err_str);
}

PRIVATE_EXTERN void
TLVListInit(TLVListRef tlvs_p)
{
    tlvs_p->attrs = NULL;
    tlvs_p->count = tlvs_p->size = 0;
    return;
}

PRIVATE_EXTERN void
TLVListFree(TLVListRef tlvs_p)
{
    if (tlvs_p->attrs != NULL && tlvs_p->attrs != tlvs_p->attrs_static) {
#ifdef TEST_TLVLIST_PARSE
	printf("freeing data\n");
#endif /* TEST_TLVLIST_PARSE */
	free(tlvs_p->attrs);
    }
    TLVListInit(tlvs_p);
    return;
}

PRIVATE_EXTERN void
TLVListAddAttribute(TLVListRef tlvs_p, const uint8_t * attr)
{
    if (tlvs_p->attrs == NULL) {
	tlvs_p->attrs = tlvs_p->attrs_static;
	tlvs_p->size = TLVListAttrsStaticSize();
    }
    else if (tlvs_p->count == tlvs_p->size) {
	tlvs_p->size += TLVListAttrsStaticSize();
	if (tlvs_p->attrs == tlvs_p->attrs_static) {
	    tlvs_p->attrs = (const void * *)
		malloc(sizeof(*tlvs_p->attrs) * tlvs_p->size);
	    bcopy(tlvs_p->attrs_static, tlvs_p->attrs,
		  sizeof(*tlvs_p->attrs) * tlvs_p->count);
	}
	else {
	    tlvs_p->attrs = (const void * *)
		reallocf(tlvs_p->attrs,
			 sizeof(*tlvs_p->attrs) * tlvs_p->size);
	}
    }
    tlvs_p->attrs[tlvs_p->count++] = attr;
    return;
}

enum {
    kTLVGood = 0,
    kTLVBad = 1,
    kTLVUnrecognized = 2
};

PRIVATE_EXTERN int
TLVCheckValidity(TLVListRef tlvs_p, TLVRef tlv_p)
{
    AttrUnion		attr;
    int			i;
    int			len;
    int			n_bits;
    int			offset;
    int			ret = kTLVGood;
    const uint8_t *	scan;
    int			tlv_length;

    attr.tlv_p = tlv_p;
    tlv_length = tlv_p->tlv_length * TLV_ALIGNMENT;
    switch (tlv_p->tlv_type) {
    case kAT_RAND:
	offset = offsetof(AT_RAND, ra_rand);
	if (tlv_length <= offset) {
	    /* truncated option */
	    snprintf(tlvs_p->err_str, sizeof(tlvs_p->err_str),
		     "%s truncated %d <= %d",
		     EAPSIMAKAAttributeTypeGetString(tlv_p->tlv_type),
		     tlv_length, offset);
	    ret = kTLVBad;
	    break;
	}
	len = tlv_length - offset;
	if ((len % RAND_SIZE) != 0) {
	    /* must be a multiple of 16 bytes */
	    snprintf(tlvs_p->err_str, sizeof(tlvs_p->err_str),
		     "AT_RAND rand length %d not multiple of %d",
		     len, RAND_SIZE);
	    ret = kTLVBad;
	    break;
	}
	if (len == 0) {
	    snprintf(tlvs_p->err_str, sizeof(tlvs_p->err_str),
		     "AT_RAND contains no RANDs");
	    ret = kTLVBad;
	    break;
	}
	break;
    case kAT_RES:
	offset = offsetof(AT_RES, rs_res);
	if (tlv_length <= offset) {
	    /* truncated option */
	    snprintf(tlvs_p->err_str, sizeof(tlvs_p->err_str),
		     "%s truncated %d <= %d",
		     EAPSIMAKAAttributeTypeGetString(tlv_p->tlv_type),
		     tlv_length, offset);
	    ret = kTLVBad;
	    break;
	}
	n_bits = net_uint16_get(attr.at_res->rs_res_length);
	len = (n_bits + NBITS_PER_BYTE - 1) / NBITS_PER_BYTE;
	if (len > (tlv_length - offset)) {
	    snprintf(tlvs_p->err_str, sizeof(tlvs_p->err_str),
		     "%s actual length %d > TLV length %d",
		     EAPSIMAKAAttributeTypeGetString(tlv_p->tlv_type),
		     len, tlv_length - offset);
	    ret = kTLVBad;
	    break;
	}
	break;
    case kAT_AUTS:
	offset = offsetof(AT_AUTS, as_auts);
	if (tlv_length <= offset) {
	    snprintf(tlvs_p->err_str, sizeof(tlvs_p->err_str),
		     "%s truncated %d <= %d",
		     EAPSIMAKAAttributeTypeGetString(tlv_p->tlv_type),
		     tlv_length, offset);
	    ret = kTLVBad;
	    break;
	}
	len = tlv_length - offset;
	if (len != AUTS_SIZE) {
	    snprintf(tlvs_p->err_str, sizeof(tlvs_p->err_str),
		     "%s invalid length %d != %d",
		     EAPSIMAKAAttributeTypeGetString(tlv_p->tlv_type),
		     len, AUTS_SIZE);
	    ret = kTLVBad;
	    break;
	}
	break;
    case kAT_PADDING:
	switch (tlv_length) {
	case 4:
	case 8:
	case 12:
	    len = tlv_length - offsetof(AT_PADDING, pa_padding);
	    for (i = 0, scan = attr.at_padding->pa_padding;
		 i < len; i++, scan++) {
		if (*scan != 0) {
		    snprintf(tlvs_p->err_str, sizeof(tlvs_p->err_str),
			     "AT_PADDING non-zero value 0x%x at offset %d",
			     *scan, i);
		    ret = kTLVBad;
		    break;
		}
	    }
	    break;
	default:
	    snprintf(tlvs_p->err_str, sizeof(tlvs_p->err_str),
		     "AT_PADDING length %d not 4, 8, or 12", tlv_length);
	    ret = kTLVBad;
	    break;
	}
	break;
    case kAT_AUTN:
    case kAT_NONCE_MT:
    case kAT_IV:
    case kAT_MAC:
    case kAT_NONCE_S:
	offset = offsetof(AT_NONCE_MT, nm_nonce_mt);
	if (tlv_length <= offset) {
	    snprintf(tlvs_p->err_str, sizeof(tlvs_p->err_str),
		     "%s truncated %d <= %d",
		     EAPSIMAKAAttributeTypeGetString(tlv_p->tlv_type),
		     tlv_length, offset);
	    ret = kTLVBad;
	    break;
	}
	len = tlv_length - offset;
	if (len != sizeof(attr.at_nonce_mt->nm_nonce_mt)) {
	    snprintf(tlvs_p->err_str, sizeof(tlvs_p->err_str),
		     "%s invalid length %d != %d",
		     EAPSIMAKAAttributeTypeGetString(tlv_p->tlv_type),
		     len, (int)sizeof(attr.at_nonce_mt->nm_nonce_mt));
	    ret = kTLVBad;
	    break;
	}
	break;
    case kAT_IDENTITY:
    case kAT_VERSION_LIST:
    case kAT_NEXT_PSEUDONYM:
    case kAT_NEXT_REAUTH_ID:
	offset = offsetof(AT_IDENTITY, id_identity);
	if (tlv_length <= offset) {
	    snprintf(tlvs_p->err_str, sizeof(tlvs_p->err_str),
		     "%s empty/truncated (%d <= %d)",
		     EAPSIMAKAAttributeTypeGetString(tlv_p->tlv_type),
		     tlv_length, offset);
	    ret = kTLVBad;
	    break;
	}
	len = net_uint16_get(attr.at_identity->id_actual_length);
	if (len > (tlv_length - offset)) {
	    snprintf(tlvs_p->err_str, sizeof(tlvs_p->err_str),
		     "%s actual length %d > TLV length %d",
		     EAPSIMAKAAttributeTypeGetString(tlv_p->tlv_type),
		     len, tlv_length - offset);
	    ret = kTLVBad;
	    break;
	}
	if (tlv_p->tlv_type == kAT_VERSION_LIST && (len & 0x1) != 0) {
	    snprintf(tlvs_p->err_str, sizeof(tlvs_p->err_str),
		     "AT_VERSION_LIST actual length %d not multiple of 2",
		     len);
	    ret = kTLVBad;
	    break;
	}
	break;
    case kAT_ENCR_DATA:
	offset = offsetof(AT_ENCR_DATA, ed_encrypted_data);
	if (tlv_length <= offset) {
	    snprintf(tlvs_p->err_str, sizeof(tlvs_p->err_str),
		     "AT_ENCR_DATA empty/truncated (%d <= %d)",
		     tlv_length, offset);
	    ret = kTLVBad;
	    break;
	}
	break;
    case kAT_SELECTED_VERSION:
    case kAT_PERMANENT_ID_REQ:
    case kAT_ANY_ID_REQ:
    case kAT_FULLAUTH_ID_REQ:
    case kAT_RESULT_IND:
    case kAT_COUNTER:
    case kAT_COUNTER_TOO_SMALL:
    case kAT_CLIENT_ERROR_CODE:
    case kAT_NOTIFICATION:
	if (tlv_length != 4) {
	    snprintf(tlvs_p->err_str, sizeof(tlvs_p->err_str),
		     "%s length %d != 4",
		     EAPSIMAKAAttributeTypeGetString(tlv_p->tlv_type),
		     tlv_length);
	    ret = kTLVBad;
	    break;
	}
	break;
    default:
	ret = kTLVUnrecognized;
	break;
    }
    return (ret);
}

PRIVATE_EXTERN Boolean
TLVListParse(TLVListRef tlvs_p, const uint8_t * attrs, int attrs_length)
{
    int			offset;
    const uint8_t *	scan;
    Boolean		success = TRUE;
    int			tlv_length;

    scan = attrs;
    offset = 0;
    while (TRUE) {
	int		left = attrs_length - offset;
	TLVRef		this_tlv;
	int		tlv_validity;

	if (left == 0) {
	    /* we're done */
	    break;
	}
	if (left < TLV_HEADER_LENGTH) {
	    snprintf(tlvs_p->err_str, sizeof(tlvs_p->err_str),
		     "Missing/truncated attribute at offset %d",
		     offset);
	    success = FALSE;
	    break;
	}
	this_tlv = (TLVRef)scan;
	tlv_length = this_tlv->tlv_length * TLV_ALIGNMENT;
	if (tlv_length > left) {
	    snprintf(tlvs_p->err_str, sizeof(tlvs_p->err_str),
		     "%s too large %d (> %d) at offset %d",
		     EAPSIMAKAAttributeTypeGetString(this_tlv->tlv_type),
		     tlv_length, left, offset);
	    success = FALSE;
	    break;
	}
	tlv_validity = TLVCheckValidity(tlvs_p, this_tlv);
	if (tlv_validity == kTLVGood) {
	    TLVListAddAttribute(tlvs_p, scan);
	}
	else if (tlv_validity == kTLVBad
		 || ((tlv_validity == kTLVUnrecognized)
		     && (this_tlv->tlv_type 
			 < kEAPSIM_TLV_SKIPPABLE_RANGE_START))) {
	    if (tlv_validity == kTLVUnrecognized) {
		snprintf(tlvs_p->err_str, sizeof(tlvs_p->err_str),
			 "unrecognized attribute %d", this_tlv->tlv_type);
	    }
	    success = FALSE;
	    break;
	}
	offset += tlv_length;;
	scan += tlv_length;
    }
    if (success == FALSE) {
	TLVListFree(tlvs_p);
    }
    return (success);
}

#define _WIDTH	"%18"

STATIC void
TLVPrintToString(CFMutableStringRef str, TLVRef tlv_p)
{
    AttrUnion		attr;
    char		buf[128];
    int			count;
    int			i;
    const char *	field_name;
    int			len;
    int			n_bits;
    int			pad_len;
    uint8_t *		scan;
    int			tlv_length;
    uint16_t		val16;

    attr.tlv_p = tlv_p;
    tlv_length = tlv_p->tlv_length * TLV_ALIGNMENT;
    STRING_APPEND(str, "%s: Length %d\n",
		  EAPSIMAKAAttributeTypeGetString(tlv_p->tlv_type),
		  tlv_length);
    field_name = EAPSIMAKAAttributeTypeGetString(tlv_p->tlv_type) + 3;
    switch (tlv_p->tlv_type) {
    case kAT_RAND:
	STRING_APPEND(str, _WIDTH "s:\t", "(reserved)");
	print_bytes_cfstr(str, attr.at_rand->ra_reserved,
			  sizeof(attr.at_rand->ra_reserved));
	len = tlv_length - offsetof(AT_RAND, ra_rand);
	count = len / RAND_SIZE;
	STRING_APPEND(str, "\n" _WIDTH"s: (n=%d)\n", field_name, count);
	for (scan = attr.at_rand->ra_rand, i = 0;
	     i < count; i++, scan += RAND_SIZE) {
	    STRING_APPEND(str, _WIDTH "d:\t", i);
	    print_bytes_cfstr(str, scan, RAND_SIZE);
	    STRING_APPEND(str, "\n");
	}
	break;
    case kAT_RES:
	n_bits = net_uint16_get(attr.at_res->rs_res_length);
	len = (n_bits + NBITS_PER_BYTE - 1) / NBITS_PER_BYTE;
	STRING_APPEND(str, _WIDTH "s: %d bits (%d bytes)\n", field_name, n_bits, len);
	print_bytes_cfstr(str, attr.at_res->rs_res, len);
	STRING_APPEND(str, "\n");
	break;
    case kAT_AUTS:
	len = tlv_length - offsetof(AT_AUTS, as_auts);
	STRING_APPEND(str, _WIDTH "s: %d bytes\n", field_name, len);
	print_bytes_cfstr(str, attr.at_auts->as_auts, len);
	STRING_APPEND(str, "\n");
	break;
    case kAT_PADDING:
	len = tlv_length - offsetof(AT_PADDING, pa_padding);
	STRING_APPEND(str, _WIDTH "s: %d bytes\n", field_name, len);
	STRING_APPEND(str, _WIDTH "s\t", "");
	print_bytes_cfstr(str, attr.at_padding->pa_padding, len);
	STRING_APPEND(str, "\n");
	break;
    case kAT_AUTN:
    case kAT_NONCE_MT:
    case kAT_IV:
    case kAT_MAC:
    case kAT_NONCE_S:
	STRING_APPEND(str, _WIDTH "s:\t", "(reserved)");
	print_bytes_cfstr(str, attr.at_nonce_mt->nm_reserved,
			  sizeof(attr.at_nonce_mt->nm_reserved));
	STRING_APPEND(str, "\n" _WIDTH "s:\t", field_name);
	print_bytes_cfstr(str, attr.at_nonce_mt->nm_nonce_mt,
			  sizeof(attr.at_nonce_mt->nm_nonce_mt));
	STRING_APPEND(str, "\n");
	break;
    case kAT_VERSION_LIST:
	len = net_uint16_get(attr.at_version_list->vl_actual_length);
	count = len / sizeof(uint16_t);
	STRING_APPEND(str, _WIDTH "s: Actual Length %d\n", field_name, len);
	for (scan = attr.at_version_list->vl_version_list, i = 0;
	     i < count; i++, scan += sizeof(uint16_t)) {
	    uint16_t	this_vers = net_uint16_get(scan);

	    STRING_APPEND(str, _WIDTH "d:\t%04d\n", i, this_vers);
	}
	pad_len = (tlv_length - offsetof(AT_VERSION_LIST, vl_version_list))
	    - len;
	snprintf(buf, sizeof(buf), "(%d pad bytes)", pad_len);
	STRING_APPEND(str, _WIDTH "s:\t", buf);
	print_bytes_cfstr(str, attr.at_identity->id_identity + len, pad_len);
	STRING_APPEND(str, "\n");
	break;
    case kAT_IDENTITY:
    case kAT_NEXT_PSEUDONYM:
    case kAT_NEXT_REAUTH_ID:
	len = net_uint16_get(attr.at_identity->id_actual_length);
	STRING_APPEND(str, _WIDTH "s: Actual Length %d\n", field_name, len);
	print_data_cfstr(str, attr.at_identity->id_identity, len);
	pad_len = (tlv_length - offsetof(AT_IDENTITY, id_identity)) - len;
	if (pad_len != 0) {
	    snprintf(buf, sizeof(buf), "(%d pad bytes)", pad_len);
	    STRING_APPEND(str, _WIDTH "s:\t", buf);
	    print_bytes_cfstr(str, attr.at_identity->id_identity + len, pad_len);
	    STRING_APPEND(str, "\n");
	}
	break;
    case kAT_ENCR_DATA:
	STRING_APPEND(str, _WIDTH "s:\t", "(reserved)");
	print_bytes_cfstr(str, attr.at_encr_data->ed_reserved,
			  sizeof(attr.at_encr_data->ed_reserved));
	len = tlv_length - offsetof(AT_ENCR_DATA, ed_encrypted_data);
	STRING_APPEND(str, "\n" _WIDTH "s: Length %d\n", field_name, len);
	print_data_cfstr(str, attr.at_encr_data->ed_encrypted_data, len);
	break;
    case kAT_SELECTED_VERSION:
    case kAT_COUNTER:
    case kAT_CLIENT_ERROR_CODE:
    case kAT_NOTIFICATION:
	val16 = net_uint16_get(attr.at_selected_version->sv_selected_version);
	STRING_APPEND(str, _WIDTH "s:\t%04d\n", field_name, val16);
	break;
    case kAT_PERMANENT_ID_REQ:
    case kAT_ANY_ID_REQ:
    case kAT_FULLAUTH_ID_REQ:
    case kAT_RESULT_IND:
    case kAT_COUNTER_TOO_SMALL:
	STRING_APPEND(str, _WIDTH "s:\t", "(reserved)");
	print_bytes_cfstr(str, attr.at_encr_data->ed_reserved,
			  sizeof(attr.at_encr_data->ed_reserved));
	STRING_APPEND(str, "\n");
	break;
    default:
	break;
    }
    return;
}

PRIVATE_EXTERN CFStringRef
TLVListCopyDescription(TLVListRef tlvs_p)
{
    int			i;
    const void * *	scan;
    CFMutableStringRef 	str;

    str = CFStringCreateMutable(NULL, 0);
    for (i = 0, scan = tlvs_p->attrs; i < tlvs_p->count; i++, scan++) {
	TLVRef	tlv_p = (TLVRef)(*scan);

	TLVPrintToString(str, tlv_p);
    }
    return (str);
}

PRIVATE_EXTERN TLVRef
TLVListLookupAttribute(TLVListRef tlvs_p, EAPSIMAKAAttributeType type)
{
    int			i;
    const void * *	scan;

    for (i = 0, scan = tlvs_p->attrs; i < tlvs_p->count; i++, scan++) {
	TLVRef	tlv_p = (TLVRef)(*scan);

	if (tlv_p->tlv_type == type) {
	    return (tlv_p);
	}
    }
    return (NULL);
}

PRIVATE_EXTERN CFStringRef
TLVCreateString(TLVRef tlv_p)
{
    CFDataRef		data;
    int			len;
    AT_IDENTITY *	id_p = (AT_IDENTITY *)tlv_p;
    CFStringRef		str;

    len = net_uint16_get(id_p->id_actual_length);
    data = CFDataCreateWithBytesNoCopy(NULL, id_p->id_identity, len,
				       kCFAllocatorNull);
    str = CFStringCreateFromExternalRepresentation(NULL, data,
						   kCFStringEncodingUTF8);
    CFRelease(data);
    return (str);
}    

PRIVATE_EXTERN CFStringRef
TLVListCreateStringFromAttribute(TLVListRef tlvs_p, EAPSIMAKAAttributeType type)
{
    TLVRef	tlv_p;

    switch (type) {
    case kAT_NEXT_REAUTH_ID:
    case kAT_NEXT_PSEUDONYM:
	break;
    default:
	return (NULL);
    }
    tlv_p = TLVListLookupAttribute(tlvs_p, type);
    if (tlv_p == NULL) {
	return (NULL);
    }
    return (TLVCreateString(tlv_p));
} 

PRIVATE_EXTERN EAPSIMAKAAttributeType
TLVListLookupIdentityAttribute(TLVListRef tlvs_p)
{
    STATIC const EAPSIMAKAAttributeType S_types[] = {
	kAT_ANY_ID_REQ,
	kAT_FULLAUTH_ID_REQ,
	kAT_PERMANENT_ID_REQ
    };
    int 		i;

    for (i = 0; i < sizeof(S_types) / sizeof(S_types[0]); i++) {
	if (TLVListLookupAttribute(tlvs_p, S_types[i]) != NULL) {
	    return (S_types[i]);
	}
    }
    return (0);
}

/**
 **
 ** - - - -  T E S T   H A R N E S S E S   - - -
 ** 
 **/

#ifdef EAPSIMAKA_PACKET_DUMP
PRIVATE_EXTERN bool
EAPSIMAKAPacketDump(FILE * out_f, EAPPacketRef pkt)
{
    bool		packet_is_valid = FALSE;
    CFStringRef		str;

    str = EAPSIMAKAPacketCopyDescription(pkt, &packet_is_valid);
    if (str != NULL) {
	SCPrint(TRUE, out_f, CFSTR("%@"), str);
	CFRelease(str);
    }
    return (packet_is_valid);
}
#endif /* EAPSIMAKA_PACKET_DUMP */

#ifdef TEST_TLVLIST_PARSE
/*
  A.3.  EAP-Request/SIM/Start

  The server's first packet looks like this:

  01                   ; Code: Request
  01                   ; Identifier: 1
  00 10                ; Length: 16 octets
  12                   ; Type: EAP-SIM
  0a                ; EAP-SIM subtype: Start
  00 00             ; (reserved)
  0f                ; Attribute type: AT_VERSION_LIST
  02             ; Attribute length: 8 octets (2*4)
  00 02          ; Actual version list length: 2 octets
  00 01          ; Version: 1
  00 00          ; (attribute padding)
*/
const uint8_t	eap_request_sim_start[] = {
    0x01,                   
    0x01,                   
    0x00, 0x10,                
    0x12,                   
    0x0a,                
    0x00, 0x00,             
    0x0f,                
    0x02,             
    0x00, 0x02,          
    0x00, 0x01,          
    0x00, 0x00,          
};

/*
  A.4.  EAP-Response/SIM/Start

  The client selects a nonce and responds with the following packet:

  02                   ; Code: Response
  01                   ; Identifier: 1
  00 20                ; Length: 32 octets
  12                   ; Type: EAP-SIM
  0a                ; EAP-SIM subtype: Start
  00 00             ; (reserved)
  07                ; Attribute type: AT_NONCE_MT
  05             ; Attribute length: 20 octets (5*4)
  00 00          ; (reserved)
  01 23 45 67    ; NONCE_MT value
  89 ab cd ef
  fe dc ba 98
  76 54 32 10
  10                ; Attribute type: AT_SELECTED_VERSION
  01             ; Attribute length: 4 octets (1*4)
  00 01          ; Version: 1

*/
const uint8_t	eap_response_sim_start[] = {
    0x02,
    0x01,
    0x00, 0x20,
    0x12,
    0x0a,
    0x00, 0x00,
    0x07,
    0x05,
    0x00, 0x00,
    0x01, 0x23, 0x45, 0x67,
    0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98,
    0x76, 0x54, 0x32, 0x10,
    0x10,
    0x01,
    0x00, 0x01
};

/*
  The EAP packet looks like this:

  01                   ; Code: Request
  02                   ; Identifier: 2
  01 18                ; Length: 280 octets
  12                   ; Type: EAP-SIM
  0b                ; EAP-SIM subtype: Challenge
  00 00             ; (reserved)
  01                ; Attribute type: AT_RAND
  0d             ; Attribute length: 52 octets (13*4)
  00 00          ; (reserved)
  10 11 12 13    ; first RAND
  14 15 16 17
  18 19 1a 1b
  1c 1d 1e 1f
  20 21 22 23    ; second RAND
  24 25 26 27
  28 29 2a 2b
  2c 2d 2e 2f
  30 31 32 33    ; third RAND
  34 35 36 37
  38 39 3a 3b
  3c 3d 3e 3f
  81                ; Attribute type: AT_IV
  05             ; Attribute length: 20 octets (5*4)
  00 00          ; (reserved)
  9e 18 b0 c2    ; IV value
  9a 65 22 63
  c0 6e fb 54
  dd 00 a8 95
  82               ; Attribute type: AT_ENCR_DATA
  2d            ; Attribute length: 180 octets (45*4)
  00 00         ; (reserved)
  55 f2 93 9b bd b1 b1 9e a1 b4 7f c0 b3 e0 be 4c
  ab 2c f7 37 2d 98 e3 02 3c 6b b9 24 15 72 3d 58
  ba d6 6c e0 84 e1 01 b6 0f 53 58 35 4b d4 21 82
  78 ae a7 bf 2c ba ce 33 10 6a ed dc 62 5b 0c 1d
  5a a6 7a 41 73 9a e5 b5 79 50 97 3f c7 ff 83 01
  07 3c 6f 95 31 50 fc 30 3e a1 52 d1 e1 0a 2d 1f
  4f 52 26 da a1 ee 90 05 47 22 52 bd b3 b7 1d 6f
  0c 3a 34 90 31 6c 46 92 98 71 bd 45 cd fd bc a6
  11 2f 07 f8 be 71 79 90 d2 5f 6d d7 f2 b7 b3 20
  bf 4d 5a 99 2e 88 03 31 d7 29 94 5a ec 75 ae 5d
  43 c8 ed a5 fe 62 33 fc ac 49 4e e6 7a 0d 50 4d
  0b                ; Attribute type: AT_MAC
  05             ; Attribute length: 20 octets (5*4)
  00 00          ; (reserved)
  fe f3 24 ac    ; MAC value
  39 62 b5 9f
  3b d7 82 53
  ae 4d cb 6a
*/
const uint8_t	eap_request_fast_reauth[] = {
    0x01,                   
    0x02,                   
    0x01, 0x18,                
    0x12,                   
    0x0b,                
    0x00, 0x00,             
    0x01,                
    0x0d,             
    0x00, 0x00,          
    0x10, 0x11, 0x12, 0x13,    
    0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b,
    0x1c, 0x1d, 0x1e, 0x1f,
    0x20, 0x21, 0x22, 0x23,    
    0x24, 0x25, 0x26, 0x27,
    0x28, 0x29, 0x2a, 0x2b,
    0x2c, 0x2d, 0x2e, 0x2f,
    0x30, 0x31, 0x32, 0x33,    
    0x34, 0x35, 0x36, 0x37,
    0x38, 0x39, 0x3a, 0x3b,
    0x3c, 0x3d, 0x3e, 0x3f,
    0x81,                
    0x05,             
    0x00, 0x00,          
    0x9e, 0x18, 0xb0, 0xc2,    
    0x9a, 0x65, 0x22, 0x63,
    0xc0, 0x6e, 0xfb, 0x54,
    0xdd, 0x00, 0xa8, 0x95,
    0x82,               
    0x2d,            
    0x00, 0x00,         
    0x55, 0xf2, 0x93, 0x9b, 0xbd, 0xb1, 0xb1, 0x9e, 0xa1, 0xb4, 0x7f, 0xc0, 0xb3, 0xe0, 0xbe, 0x4c,
    0xab, 0x2c, 0xf7, 0x37, 0x2d, 0x98, 0xe3, 0x02, 0x3c, 0x6b, 0xb9, 0x24, 0x15, 0x72, 0x3d, 0x58,
    0xba, 0xd6, 0x6c, 0xe0, 0x84, 0xe1, 0x01, 0xb6, 0x0f, 0x53, 0x58, 0x35, 0x4b, 0xd4, 0x21, 0x82,
    0x78, 0xae, 0xa7, 0xbf, 0x2c, 0xba, 0xce, 0x33, 0x10, 0x6a, 0xed, 0xdc, 0x62, 0x5b, 0x0c, 0x1d,
    0x5a, 0xa6, 0x7a, 0x41, 0x73, 0x9a, 0xe5, 0xb5, 0x79, 0x50, 0x97, 0x3f, 0xc7, 0xff, 0x83, 0x01,
    0x07, 0x3c, 0x6f, 0x95, 0x31, 0x50, 0xfc, 0x30, 0x3e, 0xa1, 0x52, 0xd1, 0xe1, 0x0a, 0x2d, 0x1f,
    0x4f, 0x52, 0x26, 0xda, 0xa1, 0xee, 0x90, 0x05, 0x47, 0x22, 0x52, 0xbd, 0xb3, 0xb7, 0x1d, 0x6f,
    0x0c, 0x3a, 0x34, 0x90, 0x31, 0x6c, 0x46, 0x92, 0x98, 0x71, 0xbd, 0x45, 0xcd, 0xfd, 0xbc, 0xa6,
    0x11, 0x2f, 0x07, 0xf8, 0xbe, 0x71, 0x79, 0x90, 0xd2, 0x5f, 0x6d, 0xd7, 0xf2, 0xb7, 0xb3, 0x20,
    0xbf, 0x4d, 0x5a, 0x99, 0x2e, 0x88, 0x03, 0x31, 0xd7, 0x29, 0x94, 0x5a, 0xec, 0x75, 0xae, 0x5d,
    0x43, 0xc8, 0xed, 0xa5, 0xfe, 0x62, 0x33, 0xfc, 0xac, 0x49, 0x4e, 0xe6, 0x7a, 0x0d, 0x50, 0x4d,
    0x0b,                
    0x05,             
    0x00, 0x00,          
    0xfe, 0xf3, 0x24, 0xac,    
    0x39, 0x62, 0xb5, 0x9f,
    0x3b, 0xd7, 0x82, 0x53,
    0xae, 0x4d, 0xcb, 0x6a 
};

/*
  The following plaintext will be encrypted and stored in the
  AT_ENCR_DATA attribute:

  84               ; Attribute type: AT_NEXT_PSEUDONYM
  13            ; Attribute length: 76 octets (19*4)
  00 46         ; Actual pseudonym length: 70 octets
  77 38 77 34 39 50 65 78 43 61 7a 57 4a 26 78 43
  49 41 52 6d 78 75 4d 4b 68 74 35 53 31 73 78 52
  44 71 58 53 45 46 42 45 67 33 44 63 5a 50 39 63
  49 78 54 65 35 4a 34 4f 79 49 77 4e 47 56 7a 78
  65 4a 4f 55 31 47
  00 00          ; (attribute padding)
  85                ; Attribute type: AT_NEXT_REAUTH_ID
  16             ; Attribute length: 88 octets (22*4)
  00 51          ; Actual re-auth identity length: 81 octets
  59 32 34 66 4e 53 72 7a 38 42 50 32 37 34 6a 4f
  4a 61 46 31 37 57 66 78 49 38 59 4f 37 51 58 30
  30 70 4d 58 6b 39 58 4d 4d 56 4f 77 37 62 72 6f
  61 4e 68 54 63 7a 75 46 71 35 33 61 45 70 4f 6b
  6b 33 4c 30 64 6d 40 65 61 70 73 69 6d 2e 66 6f
  6f
  00 00 00       ; (attribute padding)
  06                ; Attribute type: AT_PADDING
  03             ; Attribute length: 12 octets (3*4)
  00 00 00 00
  00 00 00 00
  00 00

*/
const uint8_t	at_encr_attr[] = {
    /* faked out to look like packet */
    0x01,                   
    0x02,                   
    0x00, 0xb8,                
    0x12,                   
    0x0b,                
    0x00, 0x00,             
    /* attrs */
    0x84,               
    0x13,            
    0x00, 0x46,         
    0x77, 0x38, 0x77, 0x34, 0x39, 0x50, 0x65, 0x78, 0x43, 0x61, 0x7a, 0x57, 0x4a, 0x26, 0x78, 0x43,
    0x49, 0x41, 0x52, 0x6d, 0x78, 0x75, 0x4d, 0x4b, 0x68, 0x74, 0x35, 0x53, 0x31, 0x73, 0x78, 0x52,
    0x44, 0x71, 0x58, 0x53, 0x45, 0x46, 0x42, 0x45, 0x67, 0x33, 0x44, 0x63, 0x5a, 0x50, 0x39, 0x63,
    0x49, 0x78, 0x54, 0x65, 0x35, 0x4a, 0x34, 0x4f, 0x79, 0x49, 0x77, 0x4e, 0x47, 0x56, 0x7a, 0x78,
    0x65, 0x4a, 0x4f, 0x55, 0x31, 0x47,
    0x00, 0x00,          
    0x85,                
    0x16,             
    0x00, 0x51,          
    0x59, 0x32, 0x34, 0x66, 0x4e, 0x53, 0x72, 0x7a, 0x38, 0x42, 0x50, 0x32, 0x37, 0x34, 0x6a, 0x4f,
    0x4a, 0x61, 0x46, 0x31, 0x37, 0x57, 0x66, 0x78, 0x49, 0x38, 0x59, 0x4f, 0x37, 0x51, 0x58, 0x30,
    0x30, 0x70, 0x4d, 0x58, 0x6b, 0x39, 0x58, 0x4d, 0x4d, 0x56, 0x4f, 0x77, 0x37, 0x62, 0x72, 0x6f,
    0x61, 0x4e, 0x68, 0x54, 0x63, 0x7a, 0x75, 0x46, 0x71, 0x35, 0x33, 0x61, 0x45, 0x70, 0x4f, 0x6b,
    0x6b, 0x33, 0x4c, 0x30, 0x64, 0x6d, 0x40, 0x65, 0x61, 0x70, 0x73, 0x69, 0x6d, 0x2e, 0x66, 0x6f,
    0x6f,
    0x00, 0x00, 0x00,       
    0x06,                
    0x03,             
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00,
};

const uint8_t	at_permanent_id_req[] = {
    0x01,                   
    0x02,                   
    0x00, 0x0c,                
    0x12,                   
    0x0b,                
    0x00, 0x00,             
    /* PERMANENT_ID_REQ */
    0x0a,                
    0x01,             
    0x00, 0x00
};

const uint8_t	bad_padding1[] = {
    0x01,                   
    0x02,                   
    0x00, 0x0f,                
    0x12,                   
    0x0b,                
    0x00, 0x00,             
    /* PADDING */
    0x06,                
    0x03,             
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x10,

};

const uint8_t	bad_padding2[] = {
    0x01,                   
    0x02,                   
    0x00, 0x14,                
    0x12,                   
    0x0b,                
    0x00, 0x00,             
    /* PADDING */
    0x06,                
    0x03,             
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x10,

};

const uint8_t	bad_padding3[] = {
    0x01,                   
    0x02,                   
    0x00, 0x18,                
    0x12,                   
    0x0b,                
    0x00, 0x00,             
    /* PADDING */
    0x06,                
    0x04,             
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00
};

const uint8_t	bad_at_encr_attr[] = {
    /* faked out to look like packet */
    0x01,                   
    0x02,                   
    0x00, 0xb8,                
    0x12,                   
    0x0b,                
    0x00, 0x00,             
    /* attrs */
    0x84,               
    0x13,            
    0x00, 0x4a,         
    0x77, 0x38, 0x77, 0x34, 0x39, 0x50, 0x65, 0x78, 0x43, 0x61, 0x7a, 0x57, 0x4a, 0x26, 0x78, 0x43,
    0x49, 0x41, 0x52, 0x6d, 0x78, 0x75, 0x4d, 0x4b, 0x68, 0x74, 0x35, 0x53, 0x31, 0x73, 0x78, 0x52,
    0x44, 0x71, 0x58, 0x53, 0x45, 0x46, 0x42, 0x45, 0x67, 0x33, 0x44, 0x63, 0x5a, 0x50, 0x39, 0x63,
    0x49, 0x78, 0x54, 0x65, 0x35, 0x4a, 0x34, 0x4f, 0x79, 0x49, 0x77, 0x4e, 0x47, 0x56, 0x7a, 0x78,
    0x65, 0x4a, 0x4f, 0x55, 0x31, 0x47,
    0x00, 0x00,          
    0x85,                
    0x16,             
    0x00, 0x51,          
    0x59, 0x32, 0x34, 0x66, 0x4e, 0x53, 0x72, 0x7a, 0x38, 0x42, 0x50, 0x32, 0x37, 0x34, 0x6a, 0x4f,
    0x4a, 0x61, 0x46, 0x31, 0x37, 0x57, 0x66, 0x78, 0x49, 0x38, 0x59, 0x4f, 0x37, 0x51, 0x58, 0x30,
    0x30, 0x70, 0x4d, 0x58, 0x6b, 0x39, 0x58, 0x4d, 0x4d, 0x56, 0x4f, 0x77, 0x37, 0x62, 0x72, 0x6f,
    0x61, 0x4e, 0x68, 0x54, 0x63, 0x7a, 0x75, 0x46, 0x71, 0x35, 0x33, 0x61, 0x45, 0x70, 0x4f, 0x6b,
    0x6b, 0x33, 0x4c, 0x30, 0x64, 0x6d, 0x40, 0x65, 0x61, 0x70, 0x73, 0x69, 0x6d, 0x2e, 0x66, 0x6f,
    0x6f,
    0x00, 0x00, 0x00,       
    0x06,                
    0x03,             
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00,
};

struct {
    const uint8_t *	packet;
    int			size;
    bool		good;
    const char *	name;
} packets[] = {
    { eap_request_sim_start, sizeof(eap_request_sim_start), TRUE, "eap_request_sim_start" },
    { eap_response_sim_start, sizeof(eap_response_sim_start), TRUE, "eap_response_sim_start" },
    { eap_request_fast_reauth, sizeof(eap_request_fast_reauth), TRUE, "eap_request_fast_reauth" },
    { at_encr_attr, sizeof(at_encr_attr), TRUE, "at_encr_attr" },
    { at_permanent_id_req, sizeof(at_permanent_id_req), TRUE, "at_permanent_id_req" },
    { bad_padding1, sizeof(bad_padding1), FALSE, "bad_padding1" },
    { bad_padding2, sizeof(bad_padding2), FALSE, "bad_padding2" },
    { bad_padding3, sizeof(bad_padding3), FALSE, "bad_padding3" },
    { bad_at_encr_attr, sizeof(bad_at_encr_attr), FALSE, "bad_at_encr_attr" },
    { NULL, 0 }
};

int
main(int argc, char * argv[])
{
    AttrUnion			attr;
    int				i;
    EAPSIMPacketRef		pkt;
    uint8_t			buf[1028];
    TLVBufferDeclare(		tlv_buf_p);

    for (i = 0; packets[i].packet != NULL; i++) {
	bool	good;

	pkt = (EAPSIMPacketRef)packets[i].packet;
	good = EAPSIMAKAPacketDump(stdout, (EAPPacketRef)pkt);
	printf("Test %d '%s' %s (found%serrors)\n", i,
	       packets[i].name,
	       good == packets[i].good ? "PASSED" : "FAILED",
	       !good ? " " : " no ");
	printf("\n");
    }
    pkt = (EAPSIMPacketRef)buf;
    TLVBufferInit(tlv_buf_p, pkt->attrs,
		  sizeof(buf) - offsetof(EAPSIMPacket, attrs));
    attr.tlv_p = TLVBufferAllocateTLV(tlv_buf_p, kAT_SELECTED_VERSION,
				      sizeof(AT_SELECTED_VERSION));
    if (attr.tlv_p == NULL) {
	fprintf(stderr, "failed allocating AT_SELECTED_VERSION, %s\n",
		TLVBufferErrorString(tlv_buf_p));
	exit(2);
    }
    net_uint16_set(attr.at_selected_version->sv_selected_version,
		   kEAPSIMVersion1);
    pkt->code = kEAPCodeResponse;
    pkt->identifier = 1;
    pkt->type = kEAPTypeEAPSIM;
    pkt->subtype = kEAPSIMAKAPacketSubtypeSIMStart;
    EAPPacketSetLength((EAPPacketRef)pkt,
		       offsetof(EAPSIMPacket, attrs) 
		       + TLVBufferUsed(tlv_buf_p));
    if (EAPSIMAKAPacketDump(stdout, (EAPPacketRef)pkt) == FALSE) {
	fprintf(stderr, "Parse failed!\n");
	exit(2);
    }

    exit(0);
    return (0);
}

#endif /* TEST_TLVLIST_PARSE */

#ifdef TEST_SIM_CRYPTO
#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonHMAC.h>
#include <CoreFoundation/CFPropertyList.h>
#include "fips186prf.h"

typedef struct {
    uint8_t	Kc[SIM_KC_SIZE];
} SIMKc, *SIMKcRef;

typedef struct {
    uint8_t	SRES[SIM_SRES_SIZE];
} SIMSRES, *SIMSRESRef;

typedef struct {
    uint8_t	RAND[SIM_RAND_SIZE];
} SIMRAND, *SIMRANDRef;

/*
  A.5.  EAP-Request/SIM/Challenge

  Next, the server selects three authentication triplets

  (RAND1,SRES1,Kc1) = (10111213 14151617 18191a1b 1c1d1e1f,
  d1d2d3d4,
  a0a1a2a3 a4a5a6a7)
  (RAND2,SRES2,Kc2) = (20212223 24252627 28292a2b 2c2d2e2f,
  e1e2e3e4,
  b0b1b2b3 b4b5b6b7)
  (RAND3,SRES3,Kc3) = (30313233 34353637 38393a3b 3c3d3e3f,
  f1f2f3f4,
  c0c1c2c3 c4c5c6c7)
*/

const SIMKc	test_kc[EAPSIM_MAX_RANDS] = {
    { { 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7 } },
    { { 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7 } }, 
    { { 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7 } },
};

const SIMSRES	test_sres[EAPSIM_MAX_RANDS] = {
    { { 0xd1, 0xd2, 0xd3, 0xd4 } },
    { { 0xe1, 0xe2, 0xe3, 0xe4 } },
    { { 0xf1, 0xf2, 0xf3, 0xf4 } }
};

const SIMRAND test_rand[EAPSIM_MAX_RANDS] = {
    { { 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f } },
    { { 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
	0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f } } ,
    { { 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
	0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f } }
};

const uint8_t 	test_nonce_mt[NONCE_MT_SIZE] = {
    0x01, 0x23, 0x45, 0x67,    
    0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98,
    0x76, 0x54, 0x32, 0x10
};
const uint8_t	test_identity[] = "1244070100000001@eapsim.foo";

/*
  Next, the MK is calculated as specified in Section 7*.

  MK = e576d5ca 332e9930 018bf1ba ee2763c7 95b3c712

  And the other keys are derived using the PRNG:

  K_encr = 536e5ebc 4465582a a6a8ec99 86ebb620
  K_aut =  25af1942 efcbf4bc 72b39434 21f2a974
  MSK =    39d45aea f4e30601 983e972b 6cfd46d1
  c3637733 65690d09 cd44976b 525f47d3
  a60a985e 955c53b0 90b2e4b7 3719196a
  40254296 8fd14a88 8f46b9a7 886e4488
  EMSK =   5949eab0 fff69d52 315c6c63 4fd14a7f
  0d52023d 56f79698 fa6596ab eed4f93f
  bb48eb53 4d985414 ceed0d9a 8ed33c38
  7c9dfdab 92ffbdf2 40fcecf6 5a2c93b9
*/

const uint8_t	test_mk[CC_SHA1_DIGEST_LENGTH] = {
    0xe5, 0x76, 0xd5, 0xca, 
    0x33, 0x2e, 0x99, 0x30,
    0x01, 0x8b, 0xf1, 0xba,
    0xee, 0x27, 0x63, 0xc7,
    0x95, 0xb3, 0xc7, 0x12 
};

#define EAPSIMAKA_KEY_SIZE	(EAPSIMAKA_K_ENCR_SIZE + EAPSIMAKA_K_AUT_SIZE \
				 + EAPSIMAKA_MSK_SIZE + EAPSIMAKA_EMSK_SIZE)

uint8_t key_block[EAPSIMAKA_KEY_SIZE] = {
    0x53, 0x6e, 0x5e, 0xbc, 0x44, 0x65, 0x58, 0x2a, 0xa6, 0xa8, 0xec, 0x99,  0x86, 0xeb, 0xb6, 0x20,
    0x25, 0xaf, 0x19, 0x42, 0xef, 0xcb, 0xf4, 0xbc, 0x72, 0xb3, 0x94, 0x34,  0x21, 0xf2, 0xa9, 0x74,
    0x39, 0xd4, 0x5a, 0xea, 0xf4, 0xe3, 0x06, 0x01, 0x98, 0x3e, 0x97, 0x2b,  0x6c, 0xfd, 0x46, 0xd1,
    0xc3, 0x63, 0x77, 0x33, 0x65, 0x69, 0x0d, 0x09, 0xcd, 0x44, 0x97, 0x6b,  0x52, 0x5f, 0x47, 0xd3,
    0xa6, 0x0a, 0x98, 0x5e, 0x95, 0x5c, 0x53, 0xb0, 0x90, 0xb2, 0xe4, 0xb7,  0x37, 0x19, 0x19, 0x6a,
    0x40, 0x25, 0x42, 0x96, 0x8f, 0xd1, 0x4a, 0x88, 0x8f, 0x46, 0xb9, 0xa7,  0x88, 0x6e, 0x44, 0x88,
    0x59, 0x49, 0xea, 0xb0, 0xff, 0xf6, 0x9d, 0x52, 0x31, 0x5c, 0x6c, 0x63,  0x4f, 0xd1, 0x4a, 0x7f,
    0x0d, 0x52, 0x02, 0x3d, 0x56, 0xf7, 0x96, 0x98, 0xfa, 0x65, 0x96, 0xab,  0xee, 0xd4, 0xf9, 0x3f,
    0xbb, 0x48, 0xeb, 0x53, 0x4d, 0x98, 0x54, 0x14, 0xce, 0xed, 0x0d, 0x9a,  0x8e, 0xd3, 0x3c, 0x38,
    0x7c, 0x9d, 0xfd, 0xab, 0x92, 0xff, 0xbd, 0xf2, 0x40, 0xfc, 0xec, 0xf6,  0x5a, 0x2c, 0x93, 0xb9,
};

const uint8_t		test_version_list[2] = { 0x0, 0x1 };
const uint8_t		test_selected_version[2] = { 0x0, 0x1 };

const uint8_t		test_packet[] = {
    0x01, 	/* code = 1 (request) */
    0x37, 	/* identifier = 55 */
    0x00, 0x50, /* length = 0x50 = 80 */                
    0x12,	/* type = 0x12 = 18 = EAP-SIM */
    0x0b,	/* subtype = 0x0b = 11 = Challenge */
    0x00, 0x00, /* reserved */

    /* AT_RAND */
    0x01,
    0x0d,	/* 0x0d (13) * 4 = 52 bytes */
    0x00, 0x00, /* reserved */
    0x10, 0x11, 0x12, 0x13,    
    0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b,
    0x1c, 0x1d, 0x1e, 0x1f,
    0x20, 0x21, 0x22, 0x23,    
    0x24, 0x25, 0x26, 0x27,
    0x28, 0x29, 0x2a, 0x2b,
    0x2c, 0x2d, 0x2e, 0x2f,
    0x30, 0x31, 0x32, 0x33,    
    0x34, 0x35, 0x36, 0x37,
    0x38, 0x39, 0x3a, 0x3b,
    0x3c, 0x3d, 0x3e, 0x3f,

    /* AT_MAC */
    0x0b,                
    0x05,	/* 0x05 * 4 = 20 bytes */
    0x00, 0x00,	/* padding */
    0x00, 0x97, 0xc3, 0x64,
    0xf8, 0x43, 0x1d, 0xa4,
    0x92, 0x5b, 0xb2, 0xb1,
    0x95, 0xd0, 0xbe, 0x22         
};

static void
dump_plist(FILE * f, CFTypeRef p)
{
    CFDataRef	data;
    data = CFPropertyListCreateXMLData(NULL, p);
    if (data == NULL) {
	return;
    }
    fwrite(CFDataGetBytePtr(data), CFDataGetLength(data), 1, f);
    CFRelease(data);
    return;
}

static void
dump_triplets(void)
{
    CFMutableDictionaryRef	dict;
    int				i;
    CFMutableArrayRef		array;

    dict = CFDictionaryCreateMutable(NULL, 0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);

    array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    for (i = 0; i < EAPSIM_MAX_RANDS; i++) {
	CFDataRef		data;
	
	data = CFDataCreateWithBytesNoCopy(NULL, test_kc[i].Kc, SIM_KC_SIZE, 
					   kCFAllocatorNull);
	CFArrayAppendValue(array, data);
	CFRelease(data);
    }
    CFDictionarySetValue(dict, CFSTR("KcList"), array);
    CFRelease(array);

    array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    for (i = 0; i < EAPSIM_MAX_RANDS; i++) {
	CFDataRef		data;
	
	data = CFDataCreateWithBytesNoCopy(NULL, test_sres[i].SRES,
					   SIM_SRES_SIZE, 
					   kCFAllocatorNull);
	CFArrayAppendValue(array, data);
	CFRelease(data);
    }
    CFDictionarySetValue(dict, CFSTR("SRESList"), array);
    CFRelease(array);

    array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    for (i = 0; i < EAPSIM_MAX_RANDS; i++) {
	CFDataRef		data;
	
	data = CFDataCreateWithBytesNoCopy(NULL, test_rand[i].RAND,
					   SIM_RAND_SIZE, 
					   kCFAllocatorNull);
	CFArrayAppendValue(array, data);
	CFRelease(data);
    }
    CFDictionarySetValue(dict, CFSTR("RANDList"), array);
    CFRelease(array);

    dump_plist(stdout, dict);
    CFRelease(dict);
    return;
}


const uint8_t	attrs_plaintext[] = {
    0x84,               
    0x13,            
    0x00, 0x46,         
    0x77, 0x38, 0x77, 0x34, 0x39, 0x50, 0x65, 0x78, 0x43, 0x61, 0x7a, 0x57, 0x4a, 0x26, 0x78, 0x43,
    0x49, 0x41, 0x52, 0x6d, 0x78, 0x75, 0x4d, 0x4b, 0x68, 0x74, 0x35, 0x53, 0x31, 0x73, 0x78, 0x52,
    0x44, 0x71, 0x58, 0x53, 0x45, 0x46, 0x42, 0x45, 0x67, 0x33, 0x44, 0x63, 0x5a, 0x50, 0x39, 0x63,
    0x49, 0x78, 0x54, 0x65, 0x35, 0x4a, 0x34, 0x4f, 0x79, 0x49, 0x77, 0x4e, 0x47, 0x56, 0x7a, 0x78,
    0x65, 0x4a, 0x4f, 0x55, 0x31, 0x47,
    0x00, 0x00,          
    0x85,                
    0x16,             
    0x00, 0x51,          
    0x59, 0x32, 0x34, 0x66, 0x4e, 0x53, 0x72, 0x7a, 0x38, 0x42, 0x50, 0x32, 0x37, 0x34, 0x6a, 0x4f,
    0x4a, 0x61, 0x46, 0x31, 0x37, 0x57, 0x66, 0x78, 0x49, 0x38, 0x59, 0x4f, 0x37, 0x51, 0x58, 0x30,
    0x30, 0x70, 0x4d, 0x58, 0x6b, 0x39, 0x58, 0x4d, 0x4d, 0x56, 0x4f, 0x77, 0x37, 0x62, 0x72, 0x6f,
    0x61, 0x4e, 0x68, 0x54, 0x63, 0x7a, 0x75, 0x46, 0x71, 0x35, 0x33, 0x61, 0x45, 0x70, 0x4f, 0x6b,
    0x6b, 0x33, 0x4c, 0x30, 0x64, 0x6d, 0x40, 0x65, 0x61, 0x70, 0x73, 0x69, 0x6d, 0x2e, 0x66, 0x6f,
    0x6f,
    0x00, 0x00, 0x00,       
    0x06,                
    0x03,             
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00
};

const static uint8_t	attrs_encrypted[] = {
    0x55, 0xf2, 0x93, 0x9b, 0xbd, 0xb1, 0xb1, 0x9e,
    0xa1, 0xb4, 0x7f, 0xc0, 0xb3, 0xe0, 0xbe, 0x4c,
    0xab, 0x2c, 0xf7, 0x37, 0x2d, 0x98, 0xe3, 0x02,
    0x3c, 0x6b, 0xb9, 0x24, 0x15, 0x72, 0x3d, 0x58,
    0xba, 0xd6, 0x6c, 0xe0, 0x84, 0xe1, 0x01, 0xb6,
    0x0f, 0x53, 0x58, 0x35, 0x4b, 0xd4, 0x21, 0x82,
    0x78, 0xae, 0xa7, 0xbf, 0x2c, 0xba, 0xce, 0x33,
    0x10, 0x6a, 0xed, 0xdc, 0x62, 0x5b, 0x0c, 0x1d,
    0x5a, 0xa6, 0x7a, 0x41, 0x73, 0x9a, 0xe5, 0xb5,
    0x79, 0x50, 0x97, 0x3f, 0xc7, 0xff, 0x83, 0x01,
    0x07, 0x3c, 0x6f, 0x95, 0x31, 0x50, 0xfc, 0x30,
    0x3e, 0xa1, 0x52, 0xd1, 0xe1, 0x0a, 0x2d, 0x1f,
    0x4f, 0x52, 0x26, 0xda, 0xa1, 0xee, 0x90, 0x05,
    0x47, 0x22, 0x52, 0xbd, 0xb3, 0xb7, 0x1d, 0x6f,
    0x0c, 0x3a, 0x34, 0x90, 0x31, 0x6c, 0x46, 0x92,
    0x98, 0x71, 0xbd, 0x45, 0xcd, 0xfd, 0xbc, 0xa6,
    0x11, 0x2f, 0x07, 0xf8, 0xbe, 0x71, 0x79, 0x90,
    0xd2, 0x5f, 0x6d, 0xd7, 0xf2, 0xb7, 0xb3, 0x20,
    0xbf, 0x4d, 0x5a, 0x99, 0x2e, 0x88, 0x03, 0x31,
    0xd7, 0x29, 0x94, 0x5a, 0xec, 0x75, 0xae, 0x5d,
    0x43, 0xc8, 0xed, 0xa5, 0xfe, 0x62, 0x33, 0xfc,
    0xac, 0x49, 0x4e, 0xe6, 0x7a, 0x0d, 0x50, 0x4d
};

const static uint8_t	test_iv[] = {
    0x9e, 0x18, 0xb0, 0xc2,    
    0x9a, 0x65, 0x22, 0x63,
    0xc0, 0x6e, 0xfb, 0x54,
    0xdd, 0x00, 0xa8, 0x95
};

static void
test_encr_data(void)
{
    uint8_t 		buf[sizeof(attrs_encrypted)];
    size_t		buf_used;
    CCCryptorRef	cryptor;
    EAPSIMAKAKeyInfoRef	key_info_p = (EAPSIMAKAKeyInfoRef)key_block;
    CCCryptorStatus 	status;

    status = CCCryptorCreate(kCCEncrypt,
			     kCCAlgorithmAES128,
			     0,
			     key_info_p->s.k_encr,
			     sizeof(key_info_p->s.k_encr),
			     test_iv,
			     &cryptor);
    if (status != kCCSuccess) {
	fprintf(stderr, "CCCryptoCreate failed with %d\n",
		status);
	return;
    }
    status = CCCryptorUpdate(cryptor,
			     attrs_plaintext,
			     sizeof(attrs_plaintext),
			     buf,
			     sizeof(buf),
			     &buf_used);
    if (status != kCCSuccess) {
	fprintf(stderr, "CCCryptoUpdate failed with %d\n",
		status);
	goto done;
    }
    if (buf_used != sizeof(buf)) {
	fprintf(stderr, "buf consumed %d, should have been %d\n",
		(int)buf_used, (int)sizeof(buf));
	goto done;
    }

    if (bcmp(attrs_encrypted, buf, sizeof(attrs_encrypted))) {
	fprintf(stderr, "encryption yielded different results\n");
	goto done;
    }
    fprintf(stderr, "encryption matches!\n");

 done:
    status = CCCryptorRelease(cryptor);
    if (status != kCCSuccess) {
	fprintf(stderr, "CCCryptoRelease failed with %d\n",
		status);
	return;
    }
    return;
}

static void
test_decrypt_data(void)
{
    uint8_t 		buf[sizeof(attrs_encrypted)];
    size_t		buf_used;
    CCCryptorRef	cryptor;
    EAPSIMAKAKeyInfoRef	key_info_p = (EAPSIMAKAKeyInfoRef)key_block;
    CCCryptorStatus 	status;

    status = CCCryptorCreate(kCCDecrypt,
			     kCCAlgorithmAES128,
			     0,
			     key_info_p->s.k_encr,
			     sizeof(key_info_p->s.k_encr),
			     test_iv,
			     &cryptor);
    if (status != kCCSuccess) {
	fprintf(stderr, "CCCryptoCreate failed with %d\n",
		status);
	return;
    }
    status = CCCryptorUpdate(cryptor,
			     attrs_encrypted,
			     sizeof(attrs_encrypted),
			     buf,
			     sizeof(buf),
			     &buf_used);
    if (status != kCCSuccess) {
	fprintf(stderr, "CCCryptoUpdate failed with %d\n",
		status);
	goto done;
    }
    if (buf_used != sizeof(buf)) {
	fprintf(stderr, "buf consumed %d, should have been %d\n",
		(int)buf_used, (int)sizeof(buf));
	goto done;
    }

    if (bcmp(attrs_plaintext, buf, sizeof(attrs_plaintext))) {
	fprintf(stderr, "decryption yielded different results\n");
	goto done;
    }
    fprintf(stderr, "decryption matches!\n");

 done:
    status = CCCryptorRelease(cryptor);
    if (status != kCCSuccess) {
	fprintf(stderr, "CCCryptoRelease failed with %d\n",
		status);
	return;
    }
    return;
}

int
main(int argc, char * argv[])
{
    int			attrs_length;
    CC_SHA1_CTX		context;
    EAPSIMPacketRef	eapsim;
    uint8_t		hash[CC_SHA1_DIGEST_LENGTH];
    EAPSIMAKAKeyInfo	key_info;
    AT_MAC *		mac_p;
    uint8_t		mk[CC_SHA1_DIGEST_LENGTH];
    TLVListDeclare(	tlvs_p);

    /* MK = SHA1(Identity|n*Kc| NONCE_MT| Version List| Selected Version) */
    CC_SHA1_Init(&context);
    CC_SHA1_Update(&context, test_identity, sizeof(test_identity) - 1);
    CC_SHA1_Update(&context, test_kc, sizeof(test_kc));
    CC_SHA1_Update(&context, test_nonce_mt, sizeof(test_nonce_mt));
    CC_SHA1_Update(&context, test_version_list, sizeof(test_version_list));
    CC_SHA1_Update(&context, test_selected_version,
		   sizeof(test_selected_version));
    CC_SHA1_Final(mk, &context);

    if (bcmp(mk, test_mk, sizeof(test_mk))) {
	fprintf(stderr, "The mk values are different\n");
	printf("Computed:\n");
	print_data(mk, sizeof(mk));
	printf("Desired:\n");
	print_data((void *)test_mk, sizeof(test_mk));
    }
    else {
	printf("The MK values are the same!\n");
	printf("Computed:\n");
	print_data(mk, sizeof(mk));
	printf("Desired:\n");
	print_data((void *)test_mk, sizeof(test_mk));
    }

    /* now run PRF to generate keying material */
    fips186_2prf(mk, key_info.key);

    /* make sure the key blocks are the same */
    if (bcmp(key_info.key, key_block, sizeof(key_info.key))) {
	fprintf(stderr, "key blocks are different!\n");
	exit(1);
    }
    else {
	printf("key blocks match\n");
    }

    if (EAPSIMAKAPacketDump(stdout, (EAPPacketRef)test_packet) == FALSE) {
	fprintf(stderr, "packet is bad\n");
	exit(1);
    }

    eapsim = (EAPSIMPacketRef)test_packet;
    attrs_length = EAPPacketGetLength((EAPPacketRef)eapsim)
	- kEAPSIMAKAPacketHeaderLength;
    TLVListInit(tlvs_p);
    if (TLVListParse(tlvs_p, eapsim->attrs, attrs_length) == FALSE) {
	fprintf(stderr, "failed to parse TLVs: %s\n",
		TLVListErrorString(tlvs_p));
	exit(1);
    }
    mac_p = (AT_MAC *)TLVListLookupAttribute(tlvs_p, kAT_MAC);
    if (mac_p == NULL) {
	fprintf(stderr, "Challenge is missing AT_MAC\n");
	exit(1);
    }
    EAPSIMAKAKeyInfoComputeMAC(&key_info, (EAPPacketRef)test_packet,
			       mac_p->ma_mac,
			       test_nonce_mt, sizeof(test_nonce_mt),
			       hash);
    if (bcmp(hash, mac_p->ma_mac, MAC_SIZE) != 0) {
	print_data(hash, sizeof(hash));
	fprintf(stderr, "AT_MAC mismatch\n");
	exit(1);
    }
    else {
	printf("AT_MAC is good\n");
    }

    dump_triplets();
    test_encr_data();
    test_decrypt_data();
    exit(0);
    return (0);
}

#endif /* TEST_SIM_CRYPTO */
