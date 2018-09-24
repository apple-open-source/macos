//
//  EventFactory.m
//  SystemConfigurationNetworkEventFactory
//
//  Created by Allan Nathanson on 11/15/17.
//
//

#import "EventFactory.h"
#import <os/log.h>

#pragma mark -
#pragma mark Logging

static os_log_t
__log_Spectacles(void)
{
	static os_log_t	log	= NULL;

	if (log == NULL) {
		log = os_log_create("com.apple.spectacles", "SystemConfiguration");
	}

	return log;
}

#define specs_log_err(format, ...)	os_log_error(__log_Spectacles(), format, ##__VA_ARGS__)
#define specs_log_notice(format, ...)	os_log      (__log_Spectacles(), format, ##__VA_ARGS__)
#define specs_log_info(format, ...)	os_log_info (__log_Spectacles(), format, ##__VA_ARGS__)
#define specs_log_debug(format, ...)	os_log_debug(__log_Spectacles(), format, ##__VA_ARGS__)

#pragma mark -
#pragma mark Matching

#define REMatched(re_matches, args)		\
	((re_matches != nil) && (re_matches.count == 1) && (re_matches[0].numberOfRanges == (args + 1)))

#define REMatchRange(re_matches, arg)	\
	[re_matches[0] rangeAtIndex:arg]

#pragma mark -
#pragma mark SystemConfiguratioin Network Event Factory

@interface EventFactory ()

@property (readonly, nonatomic) NSRegularExpression *kevExpressionInterfaceAttach;
@property (readonly, nonatomic) NSRegularExpression *kevExpressionLink;
@property (readonly, nonatomic) NSRegularExpression *kevExpressionLinkQuality;

@end

@implementation EventFactory

- (instancetype)init
{
	self = [super init];
	if (self) {
		NSError	*expressionError;

		expressionError = nil;
		_kevExpressionInterfaceAttach = [[NSRegularExpression alloc] initWithPattern:@"Process interface (attach|detach): (\\w+)" options:0 error:&expressionError];
		if (expressionError != nil) {
			specs_log_info("Failed to create a regular expression: %@", expressionError);
		}

		expressionError = nil;
		_kevExpressionLink = [[NSRegularExpression alloc] initWithPattern:@"Process interface link (down|up): (\\w+)" options:0 error:&expressionError];
		if (expressionError != nil) {
			specs_log_info("Failed to create a regular expression: %@", expressionError);
		}

		expressionError = nil;
		_kevExpressionLinkQuality = [[NSRegularExpression alloc] initWithPattern:@"Process interface quality: (\\w+) \\(q=([-\\d]+)\\)" options:0 error:&expressionError];
		if (expressionError != nil) {
			specs_log_info("Failed to create a regular expression: %@", expressionError);
		}
	}

	return self;
}

- (void)startWithLogSourceAttributes:(NSDictionary<NSString *, NSObject *> *)attributes
{
	//
	// Prepare for parsing logs
	//
	specs_log_info("Event factory is starting with attributes: %@", attributes);
}

- (void)handleLogEvent:(EFLogEvent *)logEvent completionHandler:(void (^)(NSArray<EFEvent *> * _Nullable))completionHandler
{
	NSString					*category;
	NSString					*message;
	EFNetworkControlPathEvent	*newNetworkEvent	= nil;

	message = logEvent.eventMessage;
	if (message == nil) {
		return;
	}

	//
	// Parse logEvent and continue constructing SpectaclesNetworkEvent objects
	//
	// Note: if one or more NetworkEvent objects are complete, send them to the
	// app in the completion handler block.
	//


	category = logEvent.category;
	if ([category isEqualToString:@"InterfaceNamer"]) {

		do {
		} while (false);

		specs_log_debug("Skipped [%@] message: %@", category, message);

	} else if ([category isEqualToString:@"IPMonitor"]) {

		do {
		} while (false);

		specs_log_debug("Skipped [%@] message: %@", category, message);

	} else if ([category isEqualToString:@"KernelEventMonitor"]) {

		do {
			NSArray<NSTextCheckingResult *>	*matches;
			NSRange							range	= NSMakeRange(0, message.length);

			//
			// interface attach/detach
			//
			matches = [_kevExpressionInterfaceAttach matchesInString:message
															 options:NSMatchingReportProgress
															   range:range];
			if (REMatched(matches, 2)) {
				NSString	*event;
				NSString	*interface;

				interface = [message substringWithRange:REMatchRange(matches, 2)];
				event     = [message substringWithRange:REMatchRange(matches, 1)];
				specs_log_debug("interface attach/detach: %@ --> %@", interface, event);

				newNetworkEvent = [[EFNetworkControlPathEvent alloc] initWithLogEvent:logEvent subsystemIdentifier:[[NSData alloc] init]];
				newNetworkEvent.interfaceBSDName = interface;
				newNetworkEvent.interfaceStatus = [event isEqualToString:@"attach"] ? @"interface attached" : @"interface detached";
				break;
			}

			//
			// interface link up/down
			//
			matches = [_kevExpressionLink matchesInString:message
												  options:NSMatchingReportProgress
													range:range];
			if (REMatched(matches, 2)) {
				NSString	*event;
				NSString	*interface;

				interface = [message substringWithRange:REMatchRange(matches, 2)];
				event     = [message substringWithRange:REMatchRange(matches, 1)];
				specs_log_debug("link change: %@ --> %@", interface, event);

				newNetworkEvent = [[EFNetworkControlPathEvent alloc] initWithLogEvent:logEvent subsystemIdentifier:[[NSData alloc] init]];
				newNetworkEvent.interfaceBSDName = interface;
				newNetworkEvent.interfaceStatus = [event isEqualToString:@"up"] ? @"link up" : @"link down";
				break;
			}

			//
			// interface link quality
			//
			matches = [_kevExpressionLinkQuality matchesInString:message
														 options:NSMatchingReportProgress
														   range:range];
			if (REMatched(matches, 2)) {
				NSString	*interface;
				NSString	*quality;

				interface = [message substringWithRange:REMatchRange(matches, 1)];
				quality   = [message substringWithRange:REMatchRange(matches, 2)];
				specs_log_debug("link quality: %@ --> %@", interface, quality);

				newNetworkEvent = [[EFNetworkControlPathEvent alloc] initWithLogEvent:logEvent subsystemIdentifier:[[NSData alloc] init]];
				newNetworkEvent.interfaceBSDName = interface;
				newNetworkEvent.interfaceStatus = [NSString stringWithFormat:@"link quality = %@", quality];
				break;
			}

			specs_log_debug("Skipped [%@] message: %@", category, message);
		} while (false);

	} else if ([category isEqualToString:@"PreferencesMonitor"]) {

			do {
			} while (false);

			specs_log_debug("Skipped [%@] message: %@", category, message);

	} else {
		// if we have no handler for this category
		specs_log_debug("Skipped [%@] message: %@", category, message);
	}

	if (newNetworkEvent != nil) {
		completionHandler(@[ newNetworkEvent ]);
	} else {
		completionHandler(nil);
	}
}

- (void)finishWithCompletionHandler:(void (^)(NSArray<EFEvent *> * _Nullable))completionHandler
{
	//
	// Clean up
	//
	// Note: if one or more SpectaclesNetworkEvent objects are in the process of
	// being built, return them in the completion handler block.
	//
	specs_log_notice("Event factory is finishing");
	completionHandler(nil);
}

@end
