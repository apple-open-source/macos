//
//  KCAESGCMDuplexSession.m
//  Security
//
//

#import <KeychainCircle/KCAESGCMDuplexSession.h>
#import <KeychainCircle/KCDer.h>
#import <KeychainCircle/KCError.h>
#import <NSError+KCCreationHelpers.h>
#import <NSData+SecRandom.h>

#include <corecrypto/ccaes.h>
#include <corecrypto/ccmode.h>
#include <corecrypto/cchkdf.h>
#include <corecrypto/ccsha2.h>

#include <corecrypto/ccder.h>

#include <libkern/OSByteOrder.h>


#define kdfInfoForwardString "send->recv"
#define kdfInfoBackwardString "recv->send"
static NSData* kdfInfoSendToReceive = nil;
static NSData* kdfInfoReceiveToSend = nil;

static const int kKCAESGCMTagSize = CCAES_KEY_SIZE_128;
static const int kKCAESGCMKeySize = CCAES_KEY_SIZE_128;

static bool derive_and_init(const struct ccmode_gcm *mode, ccgcm_ctx* ctx, NSData* sharedSecret, NSData* info) {
    const struct ccdigest_info *di = ccsha256_di();

    NSMutableData* space = [NSMutableData dataWithLength:di->output_size];

    int cc_status = 0;

    cc_status = cchkdf(di,
                       sharedSecret.length, sharedSecret.bytes,
                       0, NULL,
                       info.length, info.bytes,
                       space.length, space.mutableBytes);

    if (cc_status != 0) {
        return false;
    }
    // We only use the first 16 bytes (128 bits) for the key.
    cc_status = ccgcm_init(mode, ctx, kKCAESGCMKeySize, space.bytes);
    cc_clear(space.length, space.mutableBytes);

    return cc_status == 0;
}

@interface NSMutableData(KAESGCM)
- (void) replaceTrailingWith7LSB: (uint64_t) value;
@end

@implementation NSMutableData(KAESGCM)
- (void) replaceTrailingWith7LSB: (uint64_t) value {
    uint8_t bytes[sizeof(value)];
    OSWriteBigInt64(bytes, 0, value);

    [self replaceBytesInRange: NSMakeRange(self.length - 7, 7) withBytes: (bytes + 1)];
}
@end



@interface KCAESGCMDuplexSession ()
@property (readwrite) bool asSender;
@property (readwrite) uint64_t context;
@property (readwrite) NSData* secret;

@property (readwrite) ccgcm_ctx * send;
@property (readwrite) ccgcm_ctx * receive;

@end

@implementation KCAESGCMDuplexSession

+ (nullable instancetype) sessionAsSender: (NSData*) sharedSecret
                                  context: (uint64_t) context {
    return [[KCAESGCMDuplexSession alloc] initAsSender:sharedSecret
                                               context:context];
}

+ (nullable instancetype) sessionAsReceiver: (NSData*) sharedSecret
                                    context: (uint64_t) context {
    return [[KCAESGCMDuplexSession alloc] initAsReceiver:sharedSecret
                                                 context:context];
    
}

static NSString* KCDSSender = @"asSender";
static NSString* KCDSSecret = @"secret";
static NSString* KCDSContext = @"context";

- (void)encodeWithCoder:(NSCoder *)aCoder {
    [aCoder encodeBool: self.asSender forKey:KCDSSender];
    [aCoder encodeObject: self.secret forKey:KCDSSecret];
    [aCoder encodeInt64: self.context forKey:KCDSContext];
}

- (nullable instancetype)initWithCoder:(NSCoder *)aDecoder {

    bool asSender = [aDecoder decodeBoolForKey:KCDSSender];
    NSData* secret = [aDecoder decodeObjectOfClass:[NSData class] forKey:KCDSSecret];
    uint64_t context = [aDecoder decodeInt64ForKey:KCDSContext];

    return [self initWithSecret:secret context:context as:asSender];
}

+ (BOOL)supportsSecureCoding {
    return true;
}



- (nullable instancetype) initAsSender: (NSData*) sharedSecret context: (uint64_t) context {
    return [self initWithSecret:sharedSecret context:context as:true];
}

- (nullable instancetype) initAsReceiver: (NSData*) sharedSecret context: (uint64_t) context {
    return [self initWithSecret:sharedSecret context:context as:false];
}

- (nullable instancetype) initWithSecret: (NSData*) sharedSecret
                                 context: (uint64_t) context
                                      as: (bool) sender {
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        kdfInfoSendToReceive = [NSData dataWithBytesNoCopy: kdfInfoForwardString
                                                    length: strlen(kdfInfoForwardString)
                                              freeWhenDone: false];

        kdfInfoReceiveToSend = [NSData dataWithBytesNoCopy: kdfInfoBackwardString
                                                    length: strlen(kdfInfoBackwardString)
                                              freeWhenDone: false];
    });

    self = [super init];

    self.asSender = sender;
    self.secret = sharedSecret;
    self.send = malloc(ccgcm_context_size(ccaes_gcm_encrypt_mode()));
    self.receive = malloc(ccgcm_context_size(ccaes_gcm_decrypt_mode()));
    self.context = context;

    if (self.send == nil || self.receive == nil) {
        return nil;
    }

    derive_and_init(ccaes_gcm_encrypt_mode(),
                    self.send, self.secret,
                    sender ? kdfInfoSendToReceive : kdfInfoReceiveToSend);
    derive_and_init(ccaes_gcm_decrypt_mode(),
                    self.receive, self.secret,
                    !sender ? kdfInfoSendToReceive : kdfInfoReceiveToSend);

    return self;
}

- (size_t) encryptCapsuleSize: (NSData*) plaintext IV: (NSData*) iv {
    size_t iv_size = kcder_sizeof_data(iv, nil);
    if (iv_size == 0) {
        return 0;
    }
    size_t text_size = kcder_sizeof_data(plaintext, nil);
    if (text_size == 0) {
        return 0;
    }
    size_t tag_size = kcder_sizeof_data([NSMutableData dataWithLength: kKCAESGCMTagSize], nil);
    if (tag_size == 0) {
        return 0;
    }
    return ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE, iv_size + text_size + tag_size);
}

- (bool) GCM:(const struct ccmode_gcm*) mode
     context:(ccgcm_ctx*) ctx
          iv:(NSData*) iv
        size:(size_t) data_size
        data:(const uint8_t*) data
   processed:(uint8_t*) result
         tag:(uint8_t*) tagBuffer
       error:(NSError**) error {
    int cc_status;

    cc_status = ccgcm_reset(mode, ctx);
    if (!CoreCryptoError(cc_status, error, @"ccgcm_reset failed: %d", cc_status))
      return NO;

    cc_status = ccgcm_set_iv(mode, ctx, iv.length, iv.bytes);
    if (!CoreCryptoError(cc_status, error, @"ccgcm_set_iv failed: %d", cc_status))
      return NO;

    cc_status = ccgcm_update(mode, ctx, data_size, data, result);
    if (!CoreCryptoError(cc_status, error, @"ccgcm_update failed: %d", cc_status))
      return NO;

    cc_status = ccgcm_finalize(mode, ctx, kKCAESGCMTagSize, tagBuffer);
    return CoreCryptoError(cc_status, error, @"ccgcm_finalize failed: %d", cc_status);
}
  

- (nullable NSData*) encrypt: (NSData*) data error: (NSError**) error {
    static const int kIVSizeInBytes = 16;

    NSMutableData* iv = [NSMutableData dataWithRandomBytes: kIVSizeInBytes];

    NSMutableData* result = [NSMutableData dataWithLength: [self encryptCapsuleSize: data IV: iv]];

    // Encode with all the space set up for the result:

    uint8_t* der_end = result.mutableBytes + result.length;
    const uint8_t* der = result.bytes;

    uint8_t* tag = NULL;
    uint8_t* encrypted = NULL;

    der_end = ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, der_end, der,
              kcder_encode_data(iv, error, der,
              kcder_encode_raw_octet_space(data.length, &encrypted, der,
              kcder_encode_raw_octet_space(kKCAESGCMTagSize, &tag, der, der_end))));

    if (der_end != der) {
        KCJoiningErrorCreate(kAllocationFailure, error, @"Failed to allocate space for der");
        return nil;
    }

    const struct ccmode_gcm * mode = ccaes_gcm_encrypt_mode();

    return [self GCM:mode
             context:self.send
                  iv:iv
                size:data.length
                data:data.bytes
           processed:encrypted
                 tag:tag
               error:error] ? result : nil;
}

- (nullable NSData*) decryptAndVerify: (NSData*) data error: (NSError**) error {

    const uint8_t *der = data.bytes;
    const uint8_t *der_end = der + data.length;

    const uint8_t *sequence_end = 0;
    der = ccder_decode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, &sequence_end, der, der_end);

    if (der == NULL || sequence_end != der_end) {
        KCJoiningErrorCreate(kDERUnknownEncoding, error, @"decode failed");
        return nil;
    }

    const uint8_t *encrypted = 0;
    size_t encrypted_len = 0;
    const uint8_t *received_tag = 0;

    NSData* iv;

    der = kcder_decode_data(&iv, error, der, der_end);
    if (der == NULL) return nil;

    encrypted = ccder_decode_constructed_tl(CCDER_OCTET_STRING, &der, der, der_end);
    encrypted_len = der - encrypted;

    received_tag = ccder_decode_constructed_tl(CCDER_OCTET_STRING, &der, der, der_end);

    if (der == NULL) {
        KCJoiningErrorCreate(kDERUnknownEncoding, error, @"Decode failure");
        return nil;
    }

    if (der != der_end) {
        KCJoiningErrorCreate(kDERUnknownEncoding, error, @"Extra space");
        return nil;
    }

    if (der - received_tag != kKCAESGCMTagSize) {
        KCJoiningErrorCreate(kDERUnknownEncoding, error, @"Unexpected tag size: %d", der - received_tag);
        return nil;
    }

    NSMutableData* decrypted = [NSMutableData dataWithLength: encrypted_len];

    uint8_t tag[kKCAESGCMTagSize];
    memcpy(tag, received_tag, sizeof(tag));

    const struct ccmode_gcm * mode = ccaes_gcm_decrypt_mode();

    return [self GCM:mode
             context:self.receive
                  iv:iv
                size:encrypted_len
                data:encrypted
           processed:decrypted.mutableBytes
                 tag:tag
               error:error] ? decrypted : nil;
}

- (void) finalize {
    if (self.send) {
        ccgcm_ctx_clear(sizeof(*self.send), self.send);
        free(self.send);
    }
    if (self.receive) {
        ccgcm_ctx_clear(sizeof(*self.receive), self.receive);
        free(self.receive);
    }
    [super finalize];
}

@end
