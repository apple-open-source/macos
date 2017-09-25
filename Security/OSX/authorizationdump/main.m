//
//  main.m
//  AuthorizationTestTool
//
//  Copyright © 2017 Apple, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

#define DEFAULT_DB "/System/Library/Security/authorization.plist"
#define START_TAG @"("
#define END_TAG @")"
#define START_TAG_PLACEHOLDER @"<"
#define END_TAG_PLACEHOLDER @">"

typedef NS_ENUM(NSInteger, atOutputType)
{
	otHeader,
	otUser,
	otTries,
	otShared,
	otTimeout,
	otShowUi,
	otRequiredGroup,
	otEntitlement,
	otAllowRoot,
	otSessionOwner,
	otAllow,
	otDeny,
	otExtractPassword,
	otEntitledAndGroup,
	otVpnEntitledAndGroup,
	otAppleSigned,
	otKofn,
	otRules,
	otMechanismsHeader,
	otMechanisms,
	otFooter,
};

NSString *itemDescription(atOutputType type, id arg);
NSString *itemName(atOutputType type);
void addDataOutput(NSMutableDictionary *output, NSDictionary *content, atOutputType type, id defaultValue, Boolean writeIfNotFound);
void processOutput(NSArray *input, NSMutableArray *output, NSUInteger level);
NSArray *parseRight(NSDictionary *db, NSString *name, NSUInteger level);

NSString *itemDescription(atOutputType type, id arg)
{
	switch (type){
		case otTries: return [NSString stringWithFormat:@"Tries: %@", arg];
		case otShared: return [NSString stringWithFormat:@"Shared: %@", arg];
		case otTimeout: return [NSString stringWithFormat:@"Timeout: %@", arg];
		case otShowUi: return [NSString stringWithFormat:@"Show UI if necessary: %@", arg];
		case otRequiredGroup: return [NSString stringWithFormat:@"Require user from group: %@", arg];
		case otEntitlement: return [NSString stringWithFormat:@"Require entitlement: %@", arg];
		case otAllowRoot: return [NSString stringWithFormat:@"Auto-allow if caller process is root: %@", arg];
		case otSessionOwner: return [NSString stringWithFormat:@"Session owner required: %@", arg];
		case otAllow: return @"Always allowed";
		case otDeny: return @"Always denied";
		case otKofn:
			if (arg)
				return [NSString stringWithFormat:@"At least %@ from the following:", arg];
			else
				return [NSString stringWithFormat:@"All from following:"];
		case otExtractPassword: return [NSString stringWithFormat:@"Extractable password: %@", arg];
		case otEntitledAndGroup: return [NSString stringWithFormat:@"Entitled and group: %@", arg];
		case otVpnEntitledAndGroup: return [NSString stringWithFormat:@"VPN entitled and group: %@", arg];
		case otAppleSigned: return [NSString stringWithFormat:@"Requires Apple signature: %@", arg];
		default:
			return [NSString stringWithFormat:@"Unknown item %ld", (long)type];
	}
	return nil;
}

NSString *itemName(atOutputType type)
{
	switch (type){
		case otTries: return @"tries";
		case otShared: return @"shared";
		case otTimeout: return @"timeout";
		case otShowUi: return @"authenticate-user";
		case otRequiredGroup: return @"group";
		case otAllowRoot: return @"allow-root";
		case otSessionOwner: return @"session-owner";
		case otExtractPassword: return @"extract-password";
		case otEntitledAndGroup: return @"entitled-group";
		case otVpnEntitledAndGroup: return @"vpn-entitled-group";
		case otAppleSigned: return @"require-apple-signed";
		case otKofn: return @"k-of-n";
		case otMechanisms: return @"mechanism";
		default:
			return [NSString stringWithFormat:@"unknown-item-%ld", (long)type];
	}
	return nil;
}

void addDataOutput(NSMutableDictionary *output, NSDictionary *content, atOutputType type, id defaultValue, Boolean writeIfNotFound)
{
	id data = content[itemName(type)];
	if (data == nil) {
		if (!writeIfNotFound)
			return;
		data = defaultValue;
	}
	output[[NSNumber numberWithInteger:type]] = itemDescription(type, data);
}

void processOutput(NSArray *arr, NSMutableArray *output, NSUInteger level)
{
	for (id element in arr) {
		if ([element isKindOfClass:[NSArray class]]) {
			processOutput(element, output, level + 1);
		} else {
			if (level == 1) {
				[output addObject:element];
			} else {
				if ([START_TAG_PLACEHOLDER isEqualToString:element]){
					[output addObject:START_TAG];
				} else if ([END_TAG_PLACEHOLDER isEqualToString:element]){
						[output addObject:END_TAG];
				} else {
					[output addObject:[NSString stringWithFormat:@"\t%@", element]];
				}
			}
		}
	}
}

NSArray *parseRight(NSDictionary *db, NSString *name, NSUInteger level)
{
	NSDictionary *content = db[@"rights"][name];
	if (!content) {
		content = db[@"rules"][name];
	}

	if (!content) {
		printf("Error: Unable to find section %s\n", name.UTF8String);
		return 0;
	}

	NSString *class = content[@"class"];
	if (!class) {
		printf("Error: Unable to get class from %s\n", name.UTF8String);
		return 0;
	}

	NSMutableDictionary *output = [NSMutableDictionary new];
	addDataOutput(output, content, otEntitlement, nil, NO);
	addDataOutput(output, content, otAppleSigned, nil, NO);

	NSArray *mechanisms = nil;
	if ([class isEqualToString:@"rule"]) {
		addDataOutput(output, content, otKofn, nil, YES);

		id rule = content[@"rule"];
		NSArray *rules;
		if ([rule isKindOfClass:[NSString class]]) {
			rules = [NSArray arrayWithObject:rule];
		} else {
			rules = rule;
		}

		NSMutableArray *ruleDetails = [NSMutableArray new];

		for(NSUInteger i = 0; i < rules.count; ++i) {
			NSArray *result = parseRight(db, rules[i], level + 1);
			if (result) {
				[ruleDetails addObject:result];
			}
		}
		output[[NSNumber numberWithInteger:otRules]] = ruleDetails;
	} else if ([class isEqualToString:@"user"]) {
		output[[NSNumber numberWithInteger:otUser]] = @"* user credentials required *";

		// group
		addDataOutput(output, content, otRequiredGroup, nil, NO);

		// timeout
		addDataOutput(output, content, otTimeout, @INT32_MAX, NO);

		// tries
		addDataOutput(output, content, otTries, @10000, NO);

		// shared (default false)
		addDataOutput(output, content, otShared, @NO, YES);

		// allow-root
		addDataOutput(output, content, otAllowRoot, @NO, NO);

		// session owner
		addDataOutput(output, content, otSessionOwner, @NO, NO);

		// show ui
		addDataOutput(output, content, otShowUi, @YES, YES);

		// extract password
		addDataOutput(output, content, otExtractPassword, @NO, NO);

		// entitled and group
		addDataOutput(output, content, otEntitledAndGroup, @NO, NO);

		// vpn entitled and group
		addDataOutput(output, content, otVpnEntitledAndGroup, @NO, NO);

		// password only
		addDataOutput(output, content, otExtractPassword, @NO, NO);

		// mechanisms
		mechanisms = content[@"mechanisms"];
	} else if ([class isEqualToString:@"evaluate-mechanisms"]) {
		addDataOutput(output, content, otShared, @YES, YES);
		addDataOutput(output, content, otExtractPassword, @NO, NO);

		// mechanisms
		mechanisms = content[@"mechanisms"];
	} else if ([class isEqualToString:@"allow"]) {
		addDataOutput(output, content, otAllow, nil, YES);
	} else if ([class isEqualToString:@"deny"]) {
		addDataOutput(output, content, otDeny, nil, YES);
	}

	if (mechanisms) {
		NSMutableArray *mechanismsDetails = [NSMutableArray new];
		if ([mechanisms isKindOfClass:[NSArray class]]) {
			if (mechanisms.count > 1) {
				[mechanismsDetails addObject:START_TAG_PLACEHOLDER];
			}
			for(NSUInteger i = 0; i < mechanisms.count; ++i) {
				[mechanismsDetails addObject:mechanisms[i]];
			}
			if (mechanisms.count > 1) {
				[mechanismsDetails addObject:END_TAG_PLACEHOLDER];
			}
			if (mechanismsDetails.count) {
				output[[NSNumber numberWithInteger:otMechanismsHeader]] = @"All of the following mechanisms:";
				output[[NSNumber numberWithInteger:otMechanisms]] = mechanismsDetails;
			}
		} else {
			printf("Warning: rule %s - mechanisms is not an array\n", name.UTF8String);
		}
	}

	if (level > 1) {
		output[[NSNumber numberWithInteger:otHeader]] = START_TAG_PLACEHOLDER;
		output[[NSNumber numberWithInteger:otFooter]] = END_TAG_PLACEHOLDER;
	}

	NSArray *sortedKeys = [[output allKeys] sortedArrayUsingSelector: @selector(compare:)];
	NSMutableArray *result = [NSMutableArray new];
	processOutput([output objectsForKeys:sortedKeys notFoundMarker:@""], result, 1);
	return result;
}

int main(int argc, const char * argv[])
{
	@autoreleasepool {

		NSString *file;
		NSString *right;
		if (argc > 2) {
			file = [NSString stringWithUTF8String:argv[1]];
			right = [NSString stringWithUTF8String:argv[2]];
		} else if (argc == 2) {
			right = [NSString stringWithUTF8String:argv[1]];
			file = @DEFAULT_DB;
		} else {
			NSString *binaryName = [[NSString stringWithUTF8String:argv[0]] lastPathComponent];
			printf("Usage: %s [right [definitions.plist]]\n", binaryName.UTF8String);
			exit(-1);
		}
		NSDictionary *db = [[NSDictionary alloc] initWithContentsOfFile:file];
		if (!db) {
			printf("Error: authorization definition file %s was not found or is invalid.\n", file.UTF8String);
			exit(-1);
		}

		NSDictionary *rightContent = db[@"rights"][right];
		if (!rightContent) {
			printf("Error: right %s was not found in %s\n", right.UTF8String, file.UTF8String);
			exit(-1);
		}

		NSArray *result = parseRight(db, right, 1);
		printf("Authorization right %s\n", right.UTF8String);
		for (NSString *line in result) {
			printf("%s\n", line.UTF8String);
		}
	}
	return 0;
}
