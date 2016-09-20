//
//  smartcards.m
//  SecurityTool

#import <Foundation/Foundation.h>

#import "smartcards.h"

const CFStringRef kTKSmartCardPreferencesDomain = CFSTR("com.apple.security.smartcard");
const CFStringRef kTKDisabledTokensPreferencesKey  = CFSTR("DisabledTokens");

static void listDisabledTokens() {
    id value = (__bridge_transfer id)CFPreferencesCopyValue(kTKDisabledTokensPreferencesKey, kTKSmartCardPreferencesDomain, kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
    if (![value isKindOfClass:NSArray.class])
        return;

    NSArray *disabledTokens = (NSArray*)value;
    for (id tokenName in disabledTokens) {
        if ([tokenName isKindOfClass:NSString.class]) {
            printf("\t\"%s\"\n", [tokenName UTF8String]);
        }
    }
}

static void disable(const char *tokenToDisable) {
    id value = (__bridge_transfer id)CFPreferencesCopyValue(kTKDisabledTokensPreferencesKey, kTKSmartCardPreferencesDomain, kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
    if (![value isKindOfClass:NSArray.class])
        return;
    NSMutableArray *disabledTokens = [NSMutableArray arrayWithArray:value];
    NSString *tokenName = [NSString stringWithUTF8String:tokenToDisable];
    if (![disabledTokens containsObject:tokenName]) {
        [disabledTokens addObject:tokenName];
        CFPreferencesSetValue(kTKDisabledTokensPreferencesKey, (__bridge CFTypeRef)disabledTokens, kTKSmartCardPreferencesDomain, kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
        if (!CFPreferencesSynchronize(kTKSmartCardPreferencesDomain, kCFPreferencesAnyUser, kCFPreferencesCurrentHost))
            printf("Permission denied!\n");
    }
    else
        printf("Token is already disabled.\n");
}

static void enable(const char *tokenToEnable) {
    id value = (__bridge_transfer id)CFPreferencesCopyValue(kTKDisabledTokensPreferencesKey, kTKSmartCardPreferencesDomain, kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
    if (![value isKindOfClass:NSArray.class])
        return;

    NSString *tokenName = [NSString stringWithUTF8String:tokenToEnable];
    NSMutableArray *disabledTokens = [NSMutableArray arrayWithArray:value];
    if ([disabledTokens containsObject:tokenName]) {
        [disabledTokens removeObject:tokenName];
        CFPreferencesSetValue(kTKDisabledTokensPreferencesKey, (__bridge CFTypeRef)disabledTokens, kTKSmartCardPreferencesDomain, kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
        if (!CFPreferencesSynchronize(kTKSmartCardPreferencesDomain, kCFPreferencesAnyUser, kCFPreferencesCurrentHost))
            printf("Permission denied!\n");
    }
    else
        printf("Token is already enabled.\n");
}

static int token(int argc, char * const *argv)
{
    int ch;
    while ((ch = getopt(argc, argv, "le:d:")) != -1)
    {
        switch  (ch)
        {
            case 'l':
                listDisabledTokens();
                return 0;
            case 'e':
                enable(optarg);
                return 0;
            case 'd':
                disable(optarg);
                return 0;
        }
    }

    return 2; /* @@@ Return 2 triggers usage message. */
}

int smartcards(int argc, char * const *argv) {
    int result = 2;
    require_quiet(argc > 2, out);
    @autoreleasepool {
        if (!strcmp("token", argv[1])) {
            result = token(argc - 1, argv + 1);
        }
    }

out:
    return result;
}
