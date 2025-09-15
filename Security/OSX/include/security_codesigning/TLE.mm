//
//  TLE.mm
//  security_lwcr_support
//
//  Created by Robert Kendall-Kuppe on 12/5/23.
//
#import <sys/syslog.h>

#define CORE_ENTITLEMENTS_I_KNOW_WHAT_IM_DOING
#import "utilities/entitlements.h"
#import <CoreEntitlements/FoundationUtils.h>
#import <TLE/Core/LWCR.hpp>

#import <vector>

#import "TLE.h"

NSErrorDomain const LWCRErrorDomain = @"LWCRError";

@implementation sec_LWCR {
	NSData* backingStorage;
	CEQueryContext_t backingContext;
	@public TLE::LWCR lwcr;
}

-(instancetype)init {
	self = [super init];
	if (self) {
		self->backingStorage = nil;
		self->backingContext = NULL;
	}
	return self;
}

-(void)dealloc {
	if (backingContext) {
		SecCEReleaseContext(&backingContext);
	}
}

- (LWCRVersion_t)version {
	return lwcr.version();
}

- (int64_t)constraintCategory {
	return lwcr.constraintCategory();
}

- (BOOL) hasRequirements {
	return lwcr.requirements() != nullptr;
}

+(instancetype __nullable) withData:(NSData*)data withError:(NSError* __autoreleasing* __nullable)error {
	CEQueryContext_t ctx = NULL;
	CEValidationOptions options = {.allow_data_elements=true};
	if (!CE_OK(SecCEContextFromCFDataWithOptions(&options, (__bridge CFDataRef)data, &ctx))) {
		if (error) {
			*error = [NSError errorWithDomain:LWCRErrorDomain code:kLWCRCEError userInfo:nil];
		}
		return nil;
	}
	
	sec_LWCR* instance = [[sec_LWCR alloc] init];
	instance->backingStorage = data;
	instance->backingContext = ctx;
	TLE::Error err = instance->lwcr.loadFromCE(instance->backingContext);
	if (err) {
		if (error) {
			*error = [NSError errorWithDomain:LWCRErrorDomain code:kLWCRCoreError userInfo:@{
				NSUnderlyingErrorKey: @(err.Code),
				NSDebugDescriptionErrorKey: [[NSString alloc] initWithBytes:(void*)err.Message.data length:(NSUInteger)err.Message.length encoding:NSUTF8StringEncoding]
			}];
		}
		return nil;
	}
	return instance;
}
@end

static NSString* stringFromBuffer(const CEBuffer buffer) {
	return [[NSString alloc] initWithBytes:buffer.data length:buffer.length encoding:NSUTF8StringEncoding];
}

@implementation sec_LWCRFact {
	@public TLE::Fact fact;
	NSString* stringFactStorage;
	NSData* dataFactStorage;
}

-(void) dealloc {
	if (fact.type == kCETypeDictionary) {
		SecCEReleaseContext(&fact.value.queryContext);
	}
}

+(instancetype) boolFact:(BOOL) value {
	sec_LWCRFact* lwcrfact = [[sec_LWCRFact alloc] init];
	lwcrfact->fact.type = kCETypeBool;
	lwcrfact->fact.value.integer = (int64_t)(value == YES ? true : false);
	return lwcrfact;
}

+(instancetype) integerFact:(NSNumber*)integer {
	sec_LWCRFact* lwcrfact = [[sec_LWCRFact alloc] init];
	lwcrfact->fact.type = kCETypeInteger;
	lwcrfact->fact.value.integer = integer.longLongValue;
	return lwcrfact;
}

+(instancetype) stringFact:(NSString*)string {
	sec_LWCRFact* lwcrfact = [[sec_LWCRFact alloc] init];
	lwcrfact->fact.type = kCETypeString;
	lwcrfact->stringFactStorage = [string copy];
	lwcrfact->fact.value.string.data = (const uint8_t*)lwcrfact->stringFactStorage.UTF8String;
	lwcrfact->fact.value.string.length = strlen(lwcrfact->stringFactStorage.UTF8String);
	return lwcrfact;
}

+(instancetype) entitlementsFact:(NSDictionary*)entitlements {
	sec_LWCRFact* lwcrfact = [[sec_LWCRFact alloc] init];
	lwcrfact->fact.type = kCETypeDictionary;
	CFDataRef data = NULL;
	if (!CE_OK(CESerializeCFDictionary(CESecRuntime, (__bridge CFDictionaryRef)entitlements, &data))) {
		return nil;
	}
	lwcrfact->dataFactStorage = (__bridge_transfer NSData*)data;
	
	if (!CE_OK(SecCEContextFromCFData((__bridge CFDataRef)lwcrfact->dataFactStorage, &lwcrfact->fact.value.queryContext))) {
		return nil;
	}
	
	return lwcrfact;
}

+(instancetype) dataFact:(NSData*)data {
	sec_LWCRFact* lwcrfact = [[sec_LWCRFact alloc] init];
	lwcrfact->fact.type = kCETypeData;
	lwcrfact->dataFactStorage = [data copy];
	lwcrfact->fact.value.string.data = (const uint8_t*)lwcrfact->dataFactStorage.bytes;
	lwcrfact->fact.value.string.length = lwcrfact->dataFactStorage.length;
	return lwcrfact;
}

-(void) bindName:(const char*)name withLength:(size_t)length {
	fact.name.data = (const uint8_t*)name;
	fact.name.length = length;
}
@end

@implementation sec_LWCRExecutor

-(BOOL)evaluateRequirements:(sec_LWCR*)lwcr withFacts:(NSDictionary<NSString*, sec_LWCRFact*>*)facts {
	TLE::CallbackEncyclopedia encyclopedia(^TLE::FactDefinition(const CEBuffer name) {
		sec_LWCRFact* fact = [facts valueForKey:stringFromBuffer(name)];
		if (fact != nil) {
			return fact->fact;
		}
		return TLE::FactDefinition{};
	},^TLE::Fact(const CEBuffer name) {
		sec_LWCRFact* fact = [facts valueForKey:stringFromBuffer(name)];
		if (fact != nil) {
			return fact->fact;
		}
		return TLE::Fact{};
	}, ^bool(const CEBuffer name) {
		return [facts valueForKey:stringFromBuffer(name)] != nil;
	});
	
	TLE::Executor executor(encyclopedia);
	TLE::Tuple<TLE::Error,TLE::SharedPtr<TLE::Operation>> op = executor.getOperationsFromCE(lwcr->lwcr.requirements());
	
	if (op.get<0>()) {
		return false;
	}
	
	TLE::Tuple<TLE::Error, bool> result = op.get<1>()->Execute(encyclopedia);
	if (result.get<0>()) {
		return false;
	}
	
	return result.get<1>();
};

+(instancetype) executor {
	return [[sec_LWCRExecutor alloc] init];
}

@end
