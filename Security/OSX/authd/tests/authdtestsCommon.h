//
//  authdtestsCommon.h
//
//

#import <Foundation/Foundation.h>
#import <TargetConditionals.h>
#import <CoreFoundation/CFBase.h>
#import <Availability.h>
#import <Security/Authorization.h>
#import <Security/AuthorizationPriv.h>
#import <Security/AuthorizationDB.h>
#import <Security/AuthorizationTagsPriv.h>
#import "authd/debugging.h"
#import <XCTest/XCTest.h>

#define AuthorizationFreeItemSetNull(IS) { AuthorizationItemSet *_is = (IS); \
if (_is) { (IS) = NULL; AuthorizationFreeItemSet(_is); } }

#define SAMPLE_RIGHT "com.apple.security.syntheticinput"
#define SAMPLE_SHARED_RIGHT "system.preferences"
#define SAMPLE_PASSWORD_RIGHT "system.csfde.requestpassword"
#define SCREENSAVER_RIGHT "system.login.screensaver"
#define PROTECTED_RIGHT "com.apple.trust-settings.admin"
#define UNPROTECTED_RIGHT "com.apple.OpenScripting.additions.send"
#define NEW_RIGHT "com.apple.new.right"
#define CONFIG_PROTECTED_RIGHT "config.modify.com.apple.trust-settings.admin"
#define CONFIG_UNPROTECTED_RIGHT "config.modify.com.apple.OpenScripting.additions.send"
#define CONFIG_NEW_RIGHT "config.modify.com.apple.new.right"

NSString *correctUsername;
NSString *correctPassword;

#define INCORRECT_UNAME "fs;lgp-984-25opsdakflasdg"
#define INCORRECT_PWD "654sa65gsqihr6hhsfd'lbo[0q2,m23-odasdf"

#define SA_TIMEOUT (20)

AuthorizationItem validCredentials[] = {
	{AGENT_USERNAME, 0, NULL, 0},
	{AGENT_PASSWORD, 0, NULL, 0}
};

AuthorizationItem invalidCredentials[] = {
	{AGENT_USERNAME, strlen(INCORRECT_UNAME), (void *)INCORRECT_UNAME, 0},
	{AGENT_PASSWORD, strlen(INCORRECT_PWD), (void *)INCORRECT_PWD,0}
};

