//
//  TLE.mm
//  security_lwcr_support
//
//  Created by Robert Kendall-Kuppe on 12/5/23.
//
#import <sys/syslog.h>

#define CORE_ENTITLEMENTS_I_KNOW_WHAT_IM_DOING
#import <CoreEntitlements/CoreEntitlementsPriv.h>
#import <TLE/Core/LWCR.hpp>

#import <vector>

#import "TLE.h"


static __printflike(2, 3) void ce_log(const CERuntime_t rt, const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	vsyslog(LOG_DEBUG, fmt, args);
	va_end(args);
}

static __printflike(2, 3) __attribute__((noreturn)) void ce_abort(const CERuntime_t rt, const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	vsyslog(LOG_CRIT, fmt, args);
	va_end(args);
	abort();
}

static void* ce_malloc(const CERuntime_t rt, size_t size) {
	return malloc(size);
}

static void ce_free(const CERuntime_t rt, void* address) {
	free(address);
}

static const struct CERuntime _CERuntimeImpl = {
		.version = 1,
		.abort = ce_abort,
		.log = ce_log,
		.alloc = ce_malloc,
		.free = ce_free,
};

static const CERuntime_t CECRuntime = &_CERuntimeImpl;

static CEError_t serializeDict(NSDictionary* dict, std::vector<CESerializedElement_t>& repr);
static CEError_t serializeArray(NSArray* array, std::vector<CESerializedElement_t>& repr);

static CEError_t serializeId(id value, std::vector<CESerializedElement_t>& repr)
{
	if ([value isKindOfClass:[NSString class]]) {
		NSString* strValue = (NSString*)value;
		repr.push_back(CESerializeString((void*)strValue.UTF8String, [strValue lengthOfBytesUsingEncoding:NSUTF8StringEncoding]));
	} else if ([value isKindOfClass:[NSNumber class]]) {
		NSNumber* numValue = (NSNumber*)value;

		if (CFGetTypeID((__bridge CFBooleanRef)value) == CFBooleanGetTypeID()) {
			repr.push_back(CESerializeBool(numValue.boolValue));
		} else {
			repr.push_back(CESerializeInteger(numValue.longLongValue));
		}
	} else if ([value isKindOfClass:[NSDictionary class]]) {
		return serializeDict((NSDictionary*)value, repr);
	} else if ([value isKindOfClass:[NSArray class]]) {
		serializeArray((NSArray*)value, repr);
	} else if ([value isKindOfClass:[NSData class]]) {
		NSData* dataValue = (NSData*)value;
		repr.push_back(CESerializeData(dataValue.bytes, dataValue.length));
	} else {
		CE_THROW(kCEMalformedEntitlements);
	}
	return kCENoError;
}

static CEError_t serializeArray(NSArray* array, std::vector<CESerializedElement_t>& repr)
{
	repr.push_back((CESerializedElement_t){.type = kCESerializedArrayBegin});
	for (id value in array) {
		if (value == nil) {
			CE_THROW(kCEMalformedEntitlements);
		}
		CE_CHECK(serializeId(value, repr));
	}
	repr.push_back((CESerializedElement_t){.type = kCESerializedArrayEnd});
	return kCENoError;
}

static CEError_t serializeDict(NSDictionary* dict, std::vector<CESerializedElement_t>& repr)
{
	repr.push_back((CESerializedElement_t){.type = kCESerializedDictionaryBegin});
	NSMutableArray<NSString*>* sortedKeys = [NSMutableArray arrayWithArray:dict.allKeys];
	[sortedKeys sortUsingSelector:@selector(compare:)];
	
	for (id key in sortedKeys) {
		repr.push_back((CESerializedElement_t){.type = kCESerializedArrayBegin});
		
		if (![key isKindOfClass:[NSString class]]) {
			CE_THROW(kCEMalformedEntitlements);
		}
		NSString* strValue = (NSString*)key;
		repr.push_back(CESerializeDynamicKey((void*)strValue.UTF8String, [strValue lengthOfBytesUsingEncoding:NSUTF8StringEncoding]));
		
		id value = dict[key];
		if (value == nil) {
			CE_THROW(kCEMalformedEntitlements);
		}
		
		CE_CHECK(serializeId(value, repr));
		
		repr.push_back((CESerializedElement_t){.type = kCESerializedArrayEnd});
	}
	repr.push_back((CESerializedElement_t){.type = kCESerializedDictionaryEnd});
	return kCENoError;
}


static CEError_t CESerializeCFDictionaryWithOptions(CEValidationOptions* options, CFDictionaryRef dict, CFDataRef * serialized)
{
	NSDictionary* nsDict = (__bridge NSDictionary*)dict;
	std::vector<CESerializedElement_t> ceRepresentation{};
	CE_CHECK(serializeDict(nsDict, ceRepresentation));
	size_t dataSize = 0;
	CE_CHECK(CESizeSerialization(ceRepresentation.data(), ceRepresentation.size(), &dataSize));
	
	NSMutableData* dataOut = [NSMutableData dataWithLength:dataSize];
	CE_CHECK(CESerializeWithOptions(CECRuntime, options, ceRepresentation.data(), ceRepresentation.size(), (uint8_t*)dataOut.bytes, (uint8_t*)dataOut.bytes + dataSize));
	
	*serialized = (__bridge_retained CFDataRef)dataOut;
	
	return kCENoError;
}

static CEError_t CESerializeCFDictionary(CFDictionaryRef dict, CFDataRef * serialized)
{
	CEValidationOptions options = {.allow_data_elements = false};
	return CESerializeCFDictionaryWithOptions(&options, dict, serialized);
}

static CEError_t CEManagedContextFromCFData(const CERuntime_t runtime, CFDataRef data, CEQueryContext_t* ctx)
{
	CEValidationResult validation = {};
	const uint8_t* start = CFDataGetBytePtr(data);
	CE_CHECK(CEValidate(runtime, &validation, start, start + CFDataGetLength(data)));
	return CEAcquireManagedContext(runtime, validation, ctx);
}


static CEError_t CEManagedContextFromCFDataWithOptions(CEValidationOptions* options, CFDataRef data, CEQueryContext_t* ctx)
{
	CEValidationResult validation = {};
	const uint8_t* start = CFDataGetBytePtr(data);
	CE_CHECK(CEValidateWithOptions(CECRuntime, options, &validation, start, start + CFDataGetLength(data)));
	return CEAcquireManagedContext(CECRuntime, validation, ctx);
}

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
		CEReleaseManagedContext(&backingContext);
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
	if (!CE_OK(CEManagedContextFromCFDataWithOptions(&options, (__bridge CFDataRef)data, &ctx))) {
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
		CEReleaseManagedContext(&fact.value.queryContext);
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
	if (!CE_OK(CESerializeCFDictionary((__bridge CFDictionaryRef)entitlements, &data))) {
		return nil;
	}
	lwcrfact->dataFactStorage = (__bridge_transfer NSData*)data;
	
	if (!CE_OK(CEManagedContextFromCFData(CECRuntime, (__bridge CFDataRef)lwcrfact->dataFactStorage, &lwcrfact->fact.value.queryContext))) {
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
