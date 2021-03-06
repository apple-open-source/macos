// This file was automatically generated by protocompiler
// DO NOT EDIT!
// Compiled from OTPairingMessage.proto

#import <Foundation/Foundation.h>
#import <ProtocolBuffer/PBCodable.h>

#ifdef __cplusplus
#define OTSPONSORTOAPPLICANTROUND2M2_FUNCTION extern "C" __attribute__((visibility("hidden")))
#else
#define OTSPONSORTOAPPLICANTROUND2M2_FUNCTION extern __attribute__((visibility("hidden")))
#endif

__attribute__((visibility("hidden")))
@interface OTSponsorToApplicantRound2M2 : PBCodable <NSCopying>
{
    NSData *_voucher;
    NSData *_voucherSignature;
}


@property (nonatomic, readonly) BOOL hasVoucher;
@property (nonatomic, retain) NSData *voucher;

@property (nonatomic, readonly) BOOL hasVoucherSignature;
@property (nonatomic, retain) NSData *voucherSignature;

// Performs a shallow copy into other
- (void)copyTo:(OTSponsorToApplicantRound2M2 *)other;

// Performs a deep merge from other into self
// If set in other, singular values in self are replaced in self
// Singular composite values are recursively merged
// Repeated values from other are appended to repeated values in self
- (void)mergeFrom:(OTSponsorToApplicantRound2M2 *)other;

OTSPONSORTOAPPLICANTROUND2M2_FUNCTION BOOL OTSponsorToApplicantRound2M2ReadFrom(__unsafe_unretained OTSponsorToApplicantRound2M2 *self, __unsafe_unretained PBDataReader *reader);

@end

