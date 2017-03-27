//
//  si-73-secpasswordgenerate.c
//  sec
//

#include <stdio.h>
#include <Security/SecPasswordGenerate.h>
#include <utilities/SecCFRelease.h>
#include "Security_regressions.h"
#include <stdarg.h>

static void test_password_generate(bool ok, SecPasswordType type, int n,...)
{
    va_list argp;
    CFTypeRef key, value;
    va_start(argp, n);
    int i;

    CFMutableDictionaryRef passwordRequirements = NULL;
    CFStringRef password = NULL;
    CFErrorRef error = NULL;

    passwordRequirements = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);

    for(i=0; i<n; i++) {
        key = va_arg(argp, CFTypeRef);
        value = va_arg(argp, CFTypeRef);
        CFDictionaryAddValue(passwordRequirements, key, value);
    }

    password = SecPasswordGenerate(type, &error, passwordRequirements);

    if(ok) {
        isnt(password, NULL);
        is(error, NULL);
        if((password==NULL) || (error!=NULL))
        {
            printf("Oh no!\n");
        }
    } else {
        is(password, NULL);
        isnt(error, NULL);
    }

    CFReleaseSafe(password);
    CFReleaseSafe(passwordRequirements);
    CFReleaseSafe(error);

    va_end(argp);
}

static void tests(void)
{
    CFErrorRef error = NULL;
    CFStringRef password = NULL;

    //Create dictionary for common required character sets
    CFCharacterSetRef uppercaseLetterCharacterSet = CFCharacterSetGetPredefined(kCFCharacterSetUppercaseLetter);
    CFCharacterSetRef lowercaseLetterCharacterSet = CFCharacterSetGetPredefined(kCFCharacterSetLowercaseLetter);
    CFCharacterSetRef decimalDigitCharacterSet = CFCharacterSetGetPredefined(kCFCharacterSetDecimalDigit);

    CFMutableArrayRef requiredCharacterSets = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFArrayAppendValue(requiredCharacterSets, uppercaseLetterCharacterSet);
    CFArrayAppendValue(requiredCharacterSets, lowercaseLetterCharacterSet);
    CFArrayAppendValue(requiredCharacterSets, decimalDigitCharacterSet);

    //Create common CFNumbers
    int i2 = 2;
    int i4 = 4;
    int i5 = 5;
    int i6 = 6;
    int i12 = 12;
    int i19 = 19;
    int i20 = 20;
    int i23 = 23;
    int i24 = 24;
    int i32 = 32;
    int i56 = 56;

    CFNumberRef cf2 = CFNumberCreate(NULL, kCFNumberIntType, &i2);
    CFNumberRef cf4 = CFNumberCreate(NULL, kCFNumberIntType, &i4);
    CFNumberRef cf5 = CFNumberCreate(NULL, kCFNumberIntType, &i5);
    CFNumberRef cf6 = CFNumberCreate(NULL, kCFNumberIntType, &i6);
    CFNumberRef cf12 = CFNumberCreate(NULL, kCFNumberIntType, &i12);
    CFNumberRef cf19 = CFNumberCreate(NULL, kCFNumberIntType, &i19);
    CFNumberRef cf20 = CFNumberCreate(NULL, kCFNumberIntType, &i20);
    CFNumberRef cf23 = CFNumberCreate(NULL, kCFNumberIntType, &i23);
    CFNumberRef cf24 = CFNumberCreate(NULL, kCFNumberIntType, &i24);
    CFNumberRef cf32 = CFNumberCreate(NULL, kCFNumberIntType, &i32);
    CFNumberRef cf56 = CFNumberCreate(NULL, kCFNumberIntType, &i56);


    //generates random digit string
    is(true, (password = SecPasswordCreateWithRandomDigits(8, &error)) != NULL) ;
    CFReleaseNull(password);
    
    is(true, (password = SecPasswordCreateWithRandomDigits(7, &error)) != NULL) ;
    CFReleaseNull(password);
    
    is(true, (password = SecPasswordCreateWithRandomDigits(6, &error)) != NULL) ;
    CFReleaseNull(password);
    
    is(true, (password = SecPasswordCreateWithRandomDigits(5, &error)) != NULL) ;
    CFReleaseNull(password);

    //test default PIN
    test_password_generate(true, kSecPasswordTypePIN, 1,
                           kSecPasswordDefaultForType, CFSTR("true"));

    //test default icloud recovery code
    test_password_generate(true, kSecPasswordTypeiCloudRecovery, 1,
                           kSecPasswordDefaultForType, CFSTR("true"));

    //test default wifi
    test_password_generate(true, kSecPasswordTypeWifi, 1,
                           kSecPasswordDefaultForType, CFSTR("true"));

    //test default safari
    test_password_generate(true, kSecPasswordTypeSafari, 1,
                           kSecPasswordDefaultForType, CFSTR("true"));

    //test icloud recovery code generation
    test_password_generate(true, kSecPasswordTypeiCloudRecovery, 1,
                           kSecPasswordDefaultForType, CFSTR("true"));

    //dictionary setup
    test_password_generate(true, kSecPasswordTypeWifi, 4,
                           kSecPasswordMinLengthKey, cf20,
                           kSecPasswordMaxLengthKey, cf32,
                           kSecPasswordAllowedCharactersKey, CFSTR("abcdsefw2345"),
                           kSecPasswordRequiredCharactersKey, requiredCharacterSets);

    //test with max == min
    test_password_generate(true, kSecPasswordTypeWifi, 4,
                           kSecPasswordMinLengthKey, cf24,
                           kSecPasswordMaxLengthKey, cf24,
                           kSecPasswordAllowedCharactersKey, CFSTR("abcdsefw2345"),
                           kSecPasswordRequiredCharactersKey, requiredCharacterSets);

    //test disallowed characters
    test_password_generate(true, kSecPasswordTypeWifi, 5,
                           kSecPasswordMinLengthKey, cf24,
                           kSecPasswordMaxLengthKey, cf24,
                           kSecPasswordAllowedCharactersKey, CFSTR("abcdefghijklmnopqrstuvwxyz0123456789"),
                           kSecPasswordRequiredCharactersKey, requiredCharacterSets,
                           kSecPasswordDisallowedCharacters, CFSTR("aidfl"));


    //test can't start with characters
    test_password_generate(true, kSecPasswordTypeWifi, 5,
                           kSecPasswordMinLengthKey, cf24,
                           kSecPasswordMaxLengthKey, cf56,
                           kSecPasswordAllowedCharactersKey, CFSTR("diujk"),
                           kSecPasswordRequiredCharactersKey, requiredCharacterSets,
                           kSecPasswordCantStartWithChars, CFSTR("d"));
    
    //test can't end with characters
    test_password_generate(true, kSecPasswordTypeWifi, 5,
                           kSecPasswordMinLengthKey, cf24,
                           kSecPasswordMaxLengthKey, cf56,
                           kSecPasswordAllowedCharactersKey, CFSTR("diujk89"),
                           kSecPasswordRequiredCharactersKey, requiredCharacterSets,
                           kSecPasswordCantEndWithChars, CFSTR("d"));

    //test 4 digit pin generation
    for(int i =0 ; i< 100; i++){
        password = SecPasswordGenerate(kSecPasswordTypePIN, &error, NULL);
        isnt(password, NULL);
        ok(error == NULL);

        error = NULL;
        CFReleaseNull(password);
    }
        
    //test 6 digit pin
    test_password_generate(true, kSecPasswordTypePIN, 2,
                           kSecPasswordMinLengthKey, cf4,
                           kSecPasswordMaxLengthKey, cf6);
    //test 5 digit pin
    test_password_generate(true, kSecPasswordTypePIN, 2,
                           kSecPasswordMinLengthKey, cf5,
                           kSecPasswordMaxLengthKey, cf6);

    //test safari password
    test_password_generate(true, kSecPasswordTypeSafari, 4,
                           kSecPasswordMinLengthKey, cf20,
                           kSecPasswordMaxLengthKey, cf32,
                           kSecPasswordAllowedCharactersKey, CFSTR("abcdsefw2345"),
                           kSecPasswordRequiredCharactersKey, requiredCharacterSets);

    //test flexible group size and number of groups in the password
    //test safari password
    test_password_generate(true, kSecPasswordTypeSafari, 7,
                           kSecPasswordMinLengthKey, cf12,
                           kSecPasswordMaxLengthKey, cf19,
                           kSecPasswordAllowedCharactersKey, CFSTR("abcdsefw2345"),
                           kSecPasswordRequiredCharactersKey, requiredCharacterSets,
                           kSecPasswordGroupSize, cf5,
                           kSecPasswordNumberOfGroups, cf23,
                           kSecPasswordSeparator, CFSTR("*"));

    //test at least N characters 
    //test safari password
    CFMutableDictionaryRef atLeast = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFDictionaryAddValue(atLeast, kSecPasswordCharacters, CFSTR("ab"));
    CFDictionaryAddValue(atLeast, kSecPasswordCharacterCount, cf5);

    test_password_generate(true, kSecPasswordTypeSafari, 5,
                           kSecPasswordMinLengthKey, cf12,
                           kSecPasswordMaxLengthKey, cf19,
                           kSecPasswordAllowedCharactersKey, CFSTR("abcdsefw2345"),
                           kSecPasswordRequiredCharactersKey, requiredCharacterSets,
                           kSecPasswordContainsAtLeastNSpecificCharacters, atLeast);

    CFReleaseSafe(atLeast);

    //test no More than N characters
    //test safari password
    CFMutableDictionaryRef noMoreThan = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFDictionaryAddValue(noMoreThan, kSecPasswordCharacters, CFSTR("ab"));
    CFDictionaryAddValue(noMoreThan, kSecPasswordCharacterCount, cf5);

    test_password_generate(true, kSecPasswordTypeSafari, 5,
                           kSecPasswordMinLengthKey, cf12,
                           kSecPasswordMaxLengthKey, cf19,
                           kSecPasswordAllowedCharactersKey, CFSTR("abcdsefw2345"),
                           kSecPasswordRequiredCharactersKey, requiredCharacterSets,
                           kSecPasswordContainsNoMoreThanNSpecificCharacters, noMoreThan);

    CFReleaseSafe(noMoreThan);

    //test identical character threshold
    //test safari password
    test_password_generate(true, kSecPasswordTypeSafari, 5,
                           kSecPasswordMinLengthKey, cf12,
                           kSecPasswordMaxLengthKey, cf19,
                           kSecPasswordAllowedCharactersKey, CFSTR("abcdsefw2345"),
                           kSecPasswordRequiredCharactersKey, requiredCharacterSets,
                           kSecPasswordContainsNoMoreThanNConsecutiveIdenticalCharacters, cf2);



/////////////////now test all the error cases
    //test with no required characters
    CFMutableArrayRef emptyCharacterSets = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

    test_password_generate(false, kSecPasswordTypeWifi, 4,
                           kSecPasswordMinLengthKey, cf24,
                           kSecPasswordMaxLengthKey, cf32,
                           kSecPasswordAllowedCharactersKey, CFSTR("abcdsefw2345"),
                           kSecPasswordRequiredCharactersKey, emptyCharacterSets);

    CFReleaseSafe(emptyCharacterSets);

    //test with no allowed characters
    test_password_generate(false, kSecPasswordTypeWifi, 4,
                           kSecPasswordMinLengthKey, cf24,
                           kSecPasswordMaxLengthKey, cf32,
                           kSecPasswordAllowedCharactersKey, CFSTR(""),
                           kSecPasswordRequiredCharactersKey, requiredCharacterSets);

    //test with min > max
    test_password_generate(false, kSecPasswordTypeWifi, 4,
                           kSecPasswordMinLengthKey, cf32,
                           kSecPasswordMaxLengthKey, cf24,
                           kSecPasswordAllowedCharactersKey, CFSTR("abcdsefw2345"),
                           kSecPasswordRequiredCharactersKey, requiredCharacterSets);

    //test by ommitting dictionary parameters
    
    //omit max length
    test_password_generate(false, kSecPasswordTypeWifi, 3,
                           kSecPasswordMinLengthKey, cf20,
                           kSecPasswordAllowedCharactersKey, CFSTR("abcdsefw2345"),
                           kSecPasswordRequiredCharactersKey, requiredCharacterSets);

    //omit min length
    test_password_generate(false, kSecPasswordTypeWifi, 3,
                           kSecPasswordMaxLengthKey, cf32,
                           kSecPasswordAllowedCharactersKey, CFSTR("abcdsefw2345"),
                           kSecPasswordRequiredCharactersKey, requiredCharacterSets);

    //omit allowed characters
    test_password_generate(false, kSecPasswordTypeWifi, 3,
                           kSecPasswordMinLengthKey, cf20,
                           kSecPasswordMaxLengthKey, cf32,
                           kSecPasswordRequiredCharactersKey, requiredCharacterSets);

    //omit required characters
    test_password_generate(false, kSecPasswordTypeWifi, 3,
                           kSecPasswordMinLengthKey, cf20,
                           kSecPasswordMaxLengthKey, cf32,
                           kSecPasswordAllowedCharactersKey, CFSTR("abcdsefw2345"));


    //pass in wrong type for min
    test_password_generate(false, kSecPasswordTypeWifi, 4,
                           kSecPasswordMinLengthKey, CFSTR("20"),
                           kSecPasswordMaxLengthKey, cf32,
                           kSecPasswordAllowedCharactersKey, CFSTR("abcdsefw2345"),
                           kSecPasswordRequiredCharactersKey, requiredCharacterSets);

    //pass in wrong type for max
    test_password_generate(false, kSecPasswordTypeWifi, 4,
                           kSecPasswordMinLengthKey, cf20,
                           kSecPasswordMaxLengthKey, CFSTR("32"),
                           kSecPasswordAllowedCharactersKey, CFSTR("abcdsefw2345"),
                           kSecPasswordRequiredCharactersKey, requiredCharacterSets);

    //pass in wrong type for allowed
    test_password_generate(false, kSecPasswordTypeWifi, 4,
                           kSecPasswordMinLengthKey, cf20,
                           kSecPasswordMaxLengthKey, cf32,
                           kSecPasswordAllowedCharactersKey, requiredCharacterSets,
                           kSecPasswordRequiredCharactersKey, requiredCharacterSets);

    //pass in wrong type for required
    test_password_generate(false, kSecPasswordTypeWifi, 4,
                           kSecPasswordMinLengthKey, cf20,
                           kSecPasswordMaxLengthKey, cf32,
                           kSecPasswordAllowedCharactersKey, CFSTR("abcdsefw2345"),
                           kSecPasswordRequiredCharactersKey, CFSTR("abcdsefw2345"));
    
    //pass in wrong type for no less than
    test_password_generate(false, kSecPasswordTypeWifi, 5,
                           kSecPasswordMinLengthKey, cf20,
                           kSecPasswordMaxLengthKey, cf32,
                           kSecPasswordAllowedCharactersKey, CFSTR("abcdsefw2345"),
                           kSecPasswordRequiredCharactersKey, requiredCharacterSets,
                           kSecPasswordContainsAtLeastNSpecificCharacters, CFSTR("hehe"));


    //pass in wrong type for no more than
    test_password_generate(false, kSecPasswordTypeWifi, 5,
                           kSecPasswordMinLengthKey, cf20,
                           kSecPasswordMaxLengthKey, cf32,
                           kSecPasswordAllowedCharactersKey, CFSTR("abcdsefw2345"),
                           kSecPasswordRequiredCharactersKey, requiredCharacterSets,
                           kSecPasswordContainsNoMoreThanNSpecificCharacters, CFSTR("hehe"));

    //pass in wrong disallowed characters
    test_password_generate(false, kSecPasswordTypeWifi, 5,
                           kSecPasswordMinLengthKey, cf20,
                           kSecPasswordMaxLengthKey, cf32,
                           kSecPasswordAllowedCharactersKey, CFSTR("abcdsefw2345"),
                           kSecPasswordRequiredCharactersKey, requiredCharacterSets,
                           kSecPasswordDisallowedCharacters, requiredCharacterSets);
    
    //pass in wrong type for no more than's dictionary
    CFMutableDictionaryRef wrongCount = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFDictionaryAddValue(wrongCount, kSecPasswordCharacters, CFSTR("lkj"));
    CFDictionaryAddValue(wrongCount, kSecPasswordCharacterCount, CFSTR("sdf"));

    test_password_generate(false, kSecPasswordTypeWifi, 5,
                           kSecPasswordMinLengthKey, cf20,
                           kSecPasswordMaxLengthKey, cf32,
                           kSecPasswordAllowedCharactersKey, CFSTR("abcdsefw2345"),
                           kSecPasswordRequiredCharactersKey, requiredCharacterSets,
                           kSecPasswordContainsNoMoreThanNSpecificCharacters, wrongCount);

    CFReleaseSafe(wrongCount);


    //release CF objects:
    CFReleaseSafe(cf2);
    CFReleaseSafe(cf4);
    CFReleaseSafe(cf5);
    CFReleaseSafe(cf6);
    CFReleaseSafe(cf12);
    CFReleaseSafe(cf19);
    CFReleaseSafe(cf20);
    CFReleaseSafe(cf23);
    CFReleaseSafe(cf24);
    CFReleaseSafe(cf32);
    CFReleaseSafe(cf56);

    CFReleaseSafe(requiredCharacterSets);


    // Weak Passwords tests
    password = CFSTR("Apple1?");
    isnt(true, SecPasswordIsPasswordWeak(password));
    CFRelease(password);
    
    password = CFSTR("Singhal190");
    isnt(true, SecPasswordIsPasswordWeak(password));
    CFRelease(password);
    
    password = CFSTR("1Hollow2");
    isnt(true, SecPasswordIsPasswordWeak(password));
    CFRelease(password);
    
    password = CFSTR("1Hollow/");
    isnt(true, SecPasswordIsPasswordWeak(password));
    CFRelease(password);
    
    password = CFSTR("baj/paj1");
    isnt(true, SecPasswordIsPasswordWeak(password));
    CFRelease(password);
    
    password = CFSTR("Zaxs1009?");
    isnt(true, SecPasswordIsPasswordWeak(password));
    CFRelease(password);
    
    password = CFSTR("6666");
    isnt(false, SecPasswordIsPasswordWeak(password));
    CFRelease(password);
    
    password = CFSTR("123456");
    isnt(false, SecPasswordIsPasswordWeak(password));
    CFRelease(password);
    
    password = CFSTR("654321");
    isnt(false, SecPasswordIsPasswordWeak(password));
    CFRelease(password);
    
    password = CFSTR("A");
    isnt(false, SecPasswordIsPasswordWeak(password));
    CFRelease(password);
    
    password = CFSTR("password");
    isnt(true, SecPasswordIsPasswordWeak(password));
    CFRelease(password);
    
    password = CFSTR("password1");
    isnt(true, SecPasswordIsPasswordWeak(password));
    CFRelease(password);
    
    password = CFSTR("P@ssw0rd");
    isnt(true, SecPasswordIsPasswordWeak(password));
    CFRelease(password);
    
    password = CFSTR("facebook!{}");
    isnt(true, SecPasswordIsPasswordWeak(password));
    CFRelease(password);
    
    password = CFSTR("12345678");
    isnt(false, SecPasswordIsPasswordWeak(password));
    CFRelease(password);

    bool isSimple = false;
    
    password = CFSTR("Apple1?");
    is(false, SecPasswordIsPasswordWeak2(isSimple, password));
    CFRelease(password);
    
    password = CFSTR("Singhal190");
    is(false, SecPasswordIsPasswordWeak2(isSimple, password));
    CFRelease(password);
    
    password = CFSTR("1Hollow2");
    is(false, SecPasswordIsPasswordWeak2(isSimple, password));
    CFRelease(password);
    
    password = CFSTR("1Hollow/");
    is(false, SecPasswordIsPasswordWeak2(isSimple, password));
    CFRelease(password);
    
    password = CFSTR("baj/paj1");
    is(false, SecPasswordIsPasswordWeak2(isSimple, password));
    CFRelease(password);
    
    password = CFSTR("Zaxs1009?");
    is(false, SecPasswordIsPasswordWeak2(isSimple, password));
    CFRelease(password);
    
    password = CFSTR("6666");
    is(true, SecPasswordIsPasswordWeak2(isSimple, password));
    CFRelease(password);

    password = CFSTR("1235");
    is(false, SecPasswordIsPasswordWeak2(isSimple, password));
    CFRelease(password);
    
    isSimple = true;
    password = CFSTR("6666");
    is(true, SecPasswordIsPasswordWeak2(isSimple, password));
    CFRelease(password);
    
    password = CFSTR("1235");
    is(false, SecPasswordIsPasswordWeak2(isSimple, password));
    CFRelease(password);
    
    isSimple = false;
    password = CFSTR("123456");
    is(true, SecPasswordIsPasswordWeak2(isSimple, password));
    CFRelease(password);
    
    password = CFSTR("654321");
    is(true, SecPasswordIsPasswordWeak2(isSimple, password));
    CFRelease(password);
    
    password = CFSTR("1577326");
    is(false, SecPasswordIsPasswordWeak2(isSimple, password));
    CFRelease(password);
    
    password = CFSTR("A");
    is(true, SecPasswordIsPasswordWeak2(isSimple, password));
    CFRelease(password);

    password = CFSTR("password");
    is(false, SecPasswordIsPasswordWeak2(isSimple, password));
    CFRelease(password);

    password = CFSTR("password1");
    is(false, SecPasswordIsPasswordWeak2(isSimple, password));
    CFRelease(password);

    password = CFSTR("P@ssw0rd");
    is(false, SecPasswordIsPasswordWeak2(isSimple, password));
    CFRelease(password);

    password = CFSTR("facebook!{}");
    is(false, SecPasswordIsPasswordWeak2(isSimple, password));
    CFRelease(password);
    
    password = CFSTR("12345678");
    is(true, SecPasswordIsPasswordWeak2(isSimple, password));
    CFRelease(password);

    is(true, SecPasswordIsPasswordWeak2(true, CFSTR("666666")));
    is(true, SecPasswordIsPasswordWeak2(true, CFSTR("123456")));
    is(false, SecPasswordIsPasswordWeak2(true, CFSTR("666166")));
    is(true, SecPasswordIsPasswordWeak2(true, CFSTR("525252")));
    is(true, SecPasswordIsPasswordWeak2(false, CFSTR("525252")));
    is(true, SecPasswordIsPasswordWeak2(false, CFSTR("52525")));
    
    is(true, SecPasswordIsPasswordWeak2(true, CFSTR("098765")));
    is(true, SecPasswordIsPasswordWeak(CFSTR("0987")));

    is(true, SecPasswordIsPasswordWeak2(false, CFSTR("122222")));
    is(true, SecPasswordIsPasswordWeak2(false, CFSTR("222221")));
    is(true, SecPasswordIsPasswordWeak2(false, CFSTR("222114")));
    is(true, SecPasswordIsPasswordWeak2(false, CFSTR("221114")));
    is(false, SecPasswordIsPasswordWeak2(false, CFSTR("221144")));

    is(true, SecPasswordIsPasswordWeak2(false, CFSTR("123456")));
    is(true, SecPasswordIsPasswordWeak2(false, CFSTR("666666")));
    is(true, SecPasswordIsPasswordWeak2(false, CFSTR("111111")));
    is(true, SecPasswordIsPasswordWeak2(false, CFSTR("520520")));
    is(true, SecPasswordIsPasswordWeak2(false, CFSTR("121212")));
    is(true, SecPasswordIsPasswordWeak2(false, CFSTR("000000")));
    is(true, SecPasswordIsPasswordWeak2(false, CFSTR("654321")));

    is(true, SecPasswordIsPasswordWeak2(true, CFSTR("123456")));
    is(true, SecPasswordIsPasswordWeak2(true, CFSTR("666666")));
    is(true, SecPasswordIsPasswordWeak2(true, CFSTR("111111")));
    is(true, SecPasswordIsPasswordWeak2(true, CFSTR("520520")));
    is(true, SecPasswordIsPasswordWeak2(true, CFSTR("121212")));
    is(true, SecPasswordIsPasswordWeak2(true, CFSTR("000000")));
    is(true, SecPasswordIsPasswordWeak2(true, CFSTR("654321")));

    is(true, SecPasswordIsPasswordWeak2(true, CFSTR("030379")));
    is(true, SecPasswordIsPasswordWeak2(true, CFSTR("101471")));
    is(true, SecPasswordIsPasswordWeak2(true, CFSTR("112233")));
    is(true, SecPasswordIsPasswordWeak2(true, CFSTR("123123")));
    is(true, SecPasswordIsPasswordWeak2(true, CFSTR("123321")));

    is(true, SecPasswordIsPasswordWeak2(true, CFSTR("123654")));
    is(true, SecPasswordIsPasswordWeak2(true, CFSTR("147258")));
    is(true, SecPasswordIsPasswordWeak2(true, CFSTR("159753")));
    is(true, SecPasswordIsPasswordWeak2(true, CFSTR("321654")));
    is(true, SecPasswordIsPasswordWeak2(true, CFSTR("520131")));
    is(true, SecPasswordIsPasswordWeak2(true, CFSTR("520520")));
    is(true, SecPasswordIsPasswordWeak2(true, CFSTR("789456")));

    is(true, SecPasswordIsPasswordWeak2(false, CFSTR("123654")));
    is(true, SecPasswordIsPasswordWeak2(false, CFSTR("147258")));
    is(true, SecPasswordIsPasswordWeak2(false, CFSTR("159753")));
    is(true, SecPasswordIsPasswordWeak2(false, CFSTR("321654")));
    is(true, SecPasswordIsPasswordWeak2(false, CFSTR("520131")));
    is(true, SecPasswordIsPasswordWeak2(false, CFSTR("520520")));
    is(true, SecPasswordIsPasswordWeak2(false, CFSTR("789456")));
}

int si_73_secpasswordgenerate(int argc, char *const *argv)
{
	plan_tests(348);
	tests();
    
	return 0;
}
