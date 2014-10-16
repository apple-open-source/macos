//
//  si-73-secpasswordgenerate.c
//  sec
//

#include <stdio.h>
#include <Security/SecPasswordGenerate.h>
#include <utilities/SecCFRelease.h>
#include "Security_regressions.h"

static void tests(void)
{
    CFErrorRef error = NULL;
    CFStringRef password = NULL;
    CFMutableArrayRef requiredCharacterSets = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFMutableDictionaryRef passwordRequirements = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);

    CFCharacterSetRef uppercaseLetterCharacterSet = CFCharacterSetGetPredefined(kCFCharacterSetUppercaseLetter);
    CFCharacterSetRef lowercaseLetterCharacterSet = CFCharacterSetGetPredefined(kCFCharacterSetLowercaseLetter);
    CFCharacterSetRef decimalDigitCharacterSet = CFCharacterSetGetPredefined(kCFCharacterSetDecimalDigit);
    
    CFArrayAppendValue(requiredCharacterSets, uppercaseLetterCharacterSet);
    CFArrayAppendValue(requiredCharacterSets, lowercaseLetterCharacterSet);
    CFArrayAppendValue(requiredCharacterSets, decimalDigitCharacterSet);
    
    CFDictionaryAddValue(passwordRequirements, kSecPasswordDefaultForType, CFSTR("true"));
    
    //test default PIN
    password = SecPasswordGenerate(kSecPasswordTypePIN, &error, passwordRequirements);
    isnt(password, NULL);
    ok(error == NULL);
    CFReleaseNull(password);
    error = NULL;
    
    //test default icloud recovery code
    password = SecPasswordGenerate(kSecPasswordTypeiCloudRecovery, &error, passwordRequirements);
    isnt(password, NULL);
    ok(error == NULL);
    CFReleaseNull(password);
    error = NULL;
    
    //test default wifi
    passwordRequirements = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordDefaultForType, CFSTR("true"));
    password = SecPasswordGenerate(kSecPasswordTypeWifi, &error, passwordRequirements);
    isnt(password, NULL);
    ok(error == NULL);
    CFReleaseNull(password);
    error = NULL;
    CFRelease(passwordRequirements);
    
    //test default safari
    passwordRequirements = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordDefaultForType, CFSTR("true"));
    password = SecPasswordGenerate(kSecPasswordTypeSafari, &error, passwordRequirements);
    isnt(password, NULL);
    ok(error == NULL);

    error = NULL;
    CFReleaseNull(password);
    CFRelease(passwordRequirements);

    //test icloud recovery code generation
    password = SecPasswordGenerate(kSecPasswordTypeiCloudRecovery, &error, NULL);
    isnt(password, NULL);
    ok(error == NULL);

    error = NULL;
    CFReleaseNull(password);
    
    //dictionary setup
    int min = 20;
    int max = 32;
    
    CFNumberRef minRef = CFNumberCreate(NULL, kCFNumberIntType, &min);
    CFNumberRef maxRef = CFNumberCreate(NULL, kCFNumberIntType, &max);
    
    passwordRequirements = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMinLengthKey, minRef);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMaxLengthKey, maxRef);
    CFStringRef allowedCharacters = CFSTR("abcdsefw2345");
    
    CFDictionaryAddValue(passwordRequirements, kSecPasswordAllowedCharactersKey, allowedCharacters);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordRequiredCharactersKey, requiredCharacterSets);
    
    //test wifi code generation
    //test with min/max in range of default
    password = SecPasswordGenerate(kSecPasswordTypeWifi, &error, passwordRequirements);
    isnt(password, NULL);
    ok(error == NULL);

    error = NULL;
    CFReleaseNull(password);
    CFRelease(passwordRequirements);
    CFRelease(minRef);
    CFRelease(maxRef);
    CFRelease(allowedCharacters);
    

    //test with max == min
    min = 24;
    max = 24;
    
    minRef = CFNumberCreate(NULL, kCFNumberIntType, &min);
    maxRef = CFNumberCreate(NULL, kCFNumberIntType, &max);
    
    passwordRequirements = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMinLengthKey, minRef);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMaxLengthKey, maxRef);
    allowedCharacters = CFSTR("abcdsefw2345");
    
    CFDictionaryAddValue(passwordRequirements, kSecPasswordAllowedCharactersKey, allowedCharacters);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordRequiredCharactersKey, requiredCharacterSets);
    password = SecPasswordGenerate(kSecPasswordTypeWifi, &error, passwordRequirements);
    isnt(password, NULL);
    ok(error == NULL);

    error = NULL;
    CFReleaseNull(password);
    CFRelease(passwordRequirements);
    CFRelease(minRef);
    CFRelease(maxRef);
    CFRelease(allowedCharacters);
    
    passwordRequirements = NULL;
    
    //test disallowed characters
    min = 24;
    max = 56;
    
    minRef = CFNumberCreate(NULL, kCFNumberIntType, &min);
    maxRef = CFNumberCreate(NULL, kCFNumberIntType, &max);
    
    passwordRequirements = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMinLengthKey, minRef);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMaxLengthKey, maxRef);
    allowedCharacters = CFSTR("abcdefghijklmnopqrstuvwxyz0123456789");
    
    CFDictionaryAddValue(passwordRequirements, kSecPasswordAllowedCharactersKey, allowedCharacters);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordRequiredCharactersKey, requiredCharacterSets);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordDisallowedCharacters, CFSTR("aidfl"));

    password = SecPasswordGenerate(kSecPasswordTypeWifi, &error, passwordRequirements);
    isnt(password, NULL);
    ok(error == NULL);

    error = NULL;
    CFReleaseNull(password);
    CFRelease(passwordRequirements);
    CFRelease(minRef);
    CFRelease(maxRef);
    CFRelease(allowedCharacters);
    passwordRequirements = NULL;

    //test can't start with characters
    min = 24;
    max = 56;
    
    minRef = CFNumberCreate(NULL, kCFNumberIntType, &min);
    maxRef = CFNumberCreate(NULL, kCFNumberIntType, &max);
    
    passwordRequirements = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMinLengthKey, minRef);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMaxLengthKey, maxRef);
    allowedCharacters = CFSTR("diujk");
    
    CFDictionaryAddValue(passwordRequirements, kSecPasswordAllowedCharactersKey, allowedCharacters);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordRequiredCharactersKey, requiredCharacterSets);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordCantStartWithChars, CFSTR("d"));
    
    password = SecPasswordGenerate(kSecPasswordTypeWifi, &error, passwordRequirements);
    isnt(password, NULL);
    ok(error == NULL);

    error = NULL;
    CFReleaseNull(password);
    CFRelease(passwordRequirements);
    CFRelease(minRef);
    CFRelease(maxRef);
    CFRelease(allowedCharacters);
    passwordRequirements = NULL;
   
    
    //test can't end with characters
    min = 24;
    max = 56;
    
    minRef = CFNumberCreate(NULL, kCFNumberIntType, &min);
    maxRef = CFNumberCreate(NULL, kCFNumberIntType, &max);
    
    passwordRequirements = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMinLengthKey, minRef);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMaxLengthKey, maxRef);
    allowedCharacters = CFSTR("diujk89");
    
    CFDictionaryAddValue(passwordRequirements, kSecPasswordAllowedCharactersKey, allowedCharacters);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordRequiredCharactersKey, requiredCharacterSets);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordCantEndWithChars, CFSTR("d"));
    
    password = SecPasswordGenerate(kSecPasswordTypeWifi, &error, passwordRequirements);
    isnt(password, NULL);
    ok(error == NULL);

    error = NULL;
    CFReleaseNull(password);
    CFRelease(passwordRequirements);
    CFRelease(minRef);
    CFRelease(maxRef);
    CFRelease(allowedCharacters);
    passwordRequirements = NULL;
    
    
    //test 4 digit pin generation
    for(int i =0 ; i< 100; i++){
        password = SecPasswordGenerate(kSecPasswordTypePIN, &error, passwordRequirements);
        isnt(password, NULL);
        ok(error == NULL);

        error = NULL;
        CFReleaseNull(password);
    }
        
    //test 6 digit pin
    min = 4;
    max = 6;
    
    minRef = CFNumberCreate(NULL, kCFNumberIntType, &min);
    maxRef = CFNumberCreate(NULL, kCFNumberIntType, &max);
    
    passwordRequirements = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMinLengthKey, minRef);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMaxLengthKey, maxRef);
    password = SecPasswordGenerate(kSecPasswordTypePIN, &error, passwordRequirements);
    isnt(password, NULL);
    ok(error == NULL);

    error = NULL;
    CFReleaseNull(password);
    CFRelease(minRef);
    CFRelease(maxRef);
    CFRelease(passwordRequirements);

    //test 5 digit pin
    min = 4;
    max = 5;
    
    minRef = CFNumberCreate(NULL, kCFNumberIntType, &min);
    maxRef = CFNumberCreate(NULL, kCFNumberIntType, &max);
    
    passwordRequirements = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMinLengthKey, minRef);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMaxLengthKey, maxRef);
    password = SecPasswordGenerate(kSecPasswordTypePIN, &error, passwordRequirements);
    isnt(password, NULL);
    ok(error == NULL);

    error = NULL;
    CFReleaseNull(password);
    CFRelease(minRef);
    CFRelease(maxRef);
    CFRelease(passwordRequirements);
    //test safari password
    min = 20;
    max = 32;
    
    minRef = CFNumberCreate(NULL, kCFNumberIntType, &min);
    maxRef = CFNumberCreate(NULL, kCFNumberIntType, &max);
    
    passwordRequirements = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMinLengthKey, minRef);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMaxLengthKey, maxRef);
    allowedCharacters = CFSTR("abcdsefw2345");
    requiredCharacterSets = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFArrayAppendValue(requiredCharacterSets, uppercaseLetterCharacterSet);
    CFArrayAppendValue(requiredCharacterSets, lowercaseLetterCharacterSet);
    CFArrayAppendValue(requiredCharacterSets, decimalDigitCharacterSet);
    
    CFDictionaryAddValue(passwordRequirements, kSecPasswordAllowedCharactersKey, allowedCharacters);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordRequiredCharactersKey, requiredCharacterSets);
    
    password = SecPasswordGenerate(kSecPasswordTypeSafari, &error, passwordRequirements);
    isnt(password, NULL);
    ok(error == NULL);

    error = NULL;
    CFReleaseNull(password);
    CFRelease(passwordRequirements);
    CFRelease(minRef);
    CFRelease(maxRef);
    CFRelease(allowedCharacters);
    CFRelease(requiredCharacterSets);
    
    //test flexible group size and number of groups in the password
    //test safari password
    min = 12;
    max = 19;
    int groupSize = 5;
    int numberOfGroups = 23;
    
    CFTypeRef groupSizeRef = CFNumberCreate(NULL, kCFNumberIntType, &groupSize);
    CFTypeRef numberOfGroupsRef = CFNumberCreate(NULL, kCFNumberIntType, &numberOfGroups);
    
    minRef = CFNumberCreate(NULL, kCFNumberIntType, &min);
    maxRef = CFNumberCreate(NULL, kCFNumberIntType, &max);
    
    passwordRequirements = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMinLengthKey, minRef);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMaxLengthKey, maxRef);
    allowedCharacters = CFSTR("abcdsefw2345");
    requiredCharacterSets = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFArrayAppendValue(requiredCharacterSets, uppercaseLetterCharacterSet);
    CFArrayAppendValue(requiredCharacterSets, lowercaseLetterCharacterSet);
    CFArrayAppendValue(requiredCharacterSets, decimalDigitCharacterSet);
    
    CFDictionaryAddValue(passwordRequirements, kSecPasswordAllowedCharactersKey, allowedCharacters);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordRequiredCharactersKey, requiredCharacterSets);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordGroupSize, groupSizeRef);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordNumberOfGroups, numberOfGroupsRef);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordSeparator, CFSTR("*"));

    password = SecPasswordGenerate(kSecPasswordTypeSafari, &error, passwordRequirements);
    isnt(password, NULL);
    ok(error == NULL);

    error = NULL;
    CFReleaseNull(password);
    CFRelease(passwordRequirements);
    CFRelease(minRef);
    CFRelease(maxRef);
    CFRelease(allowedCharacters);
    CFRelease(requiredCharacterSets);
    
    //test at least N characters 
    //test safari password
    min = 24;
    max = 32;
    int N = 5;

    minRef = CFNumberCreate(NULL, kCFNumberIntType, &min);
    maxRef = CFNumberCreate(NULL, kCFNumberIntType, &max);
    CFNumberRef threshold = CFNumberCreate(NULL, kCFNumberIntType, &N);
    
    CFStringRef characters = CFSTR("ab");
    CFMutableDictionaryRef atLeast = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFDictionaryAddValue(atLeast, kSecPasswordCharacters, characters);
    CFDictionaryAddValue(atLeast, kSecPasswordCharacterCount, threshold);
    
    passwordRequirements = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMinLengthKey, minRef);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMaxLengthKey, maxRef);
    allowedCharacters = CFSTR("abcdsefw2345");
    requiredCharacterSets = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFArrayAppendValue(requiredCharacterSets, uppercaseLetterCharacterSet);
    CFArrayAppendValue(requiredCharacterSets, lowercaseLetterCharacterSet);
    CFArrayAppendValue(requiredCharacterSets, decimalDigitCharacterSet);
    
    CFDictionaryAddValue(passwordRequirements, kSecPasswordAllowedCharactersKey, allowedCharacters);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordRequiredCharactersKey, requiredCharacterSets);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordContainsAtLeastNSpecificCharacters, atLeast);

    password = SecPasswordGenerate(kSecPasswordTypeSafari, &error, passwordRequirements);
    isnt(password, NULL);
    ok(error == NULL);

    error = NULL;
    CFReleaseNull(password);
    CFRelease(passwordRequirements);
    CFRelease(minRef);
    CFRelease(maxRef);
    CFRelease(allowedCharacters);
    CFRelease(requiredCharacterSets);

    //test no More than N characters
    //test safari password
    min = 24;
    max = 32;
    N = 5;
    

    threshold = CFNumberCreate(NULL, kCFNumberIntType, &N);
    minRef = CFNumberCreate(NULL, kCFNumberIntType, &min);
    maxRef = CFNumberCreate(NULL, kCFNumberIntType, &max);
    
    CFMutableDictionaryRef noMoreThan = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFStringRef noMore = CFSTR("ab");
    CFDictionaryAddValue(noMoreThan, kSecPasswordCharacters, noMore);
    CFDictionaryAddValue(noMoreThan, kSecPasswordCharacterCount, threshold);
    
    passwordRequirements = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMinLengthKey, minRef);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMaxLengthKey, maxRef);
    allowedCharacters = CFSTR("abcdsefw2345");
    requiredCharacterSets = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFArrayAppendValue(requiredCharacterSets, uppercaseLetterCharacterSet);
    CFArrayAppendValue(requiredCharacterSets, lowercaseLetterCharacterSet);
    CFArrayAppendValue(requiredCharacterSets, decimalDigitCharacterSet);
    
    CFDictionaryAddValue(passwordRequirements, kSecPasswordAllowedCharactersKey, allowedCharacters);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordRequiredCharactersKey, requiredCharacterSets);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordContainsNoMoreThanNSpecificCharacters, noMoreThan);
    
    password = SecPasswordGenerate(kSecPasswordTypeSafari, &error, passwordRequirements);
    isnt(password, NULL);
    ok(error == NULL);

    error = NULL;
    CFReleaseNull(password);
    CFRelease(passwordRequirements);
    CFRelease(minRef);
    CFRelease(maxRef);
    CFRelease(allowedCharacters);
    CFRelease(requiredCharacterSets);

    //test identical character threshold
    //test safari password
    min = 12;
    max = 19;
    N = 2;
    
    threshold = CFNumberCreate(NULL, kCFNumberIntType, &N);
    minRef = CFNumberCreate(NULL, kCFNumberIntType, &min);
    maxRef = CFNumberCreate(NULL, kCFNumberIntType, &max);
    
    passwordRequirements = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMinLengthKey, minRef);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMaxLengthKey, maxRef);
    allowedCharacters = CFSTR("abcdsefw2345");
    requiredCharacterSets = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFArrayAppendValue(requiredCharacterSets, uppercaseLetterCharacterSet);
    CFArrayAppendValue(requiredCharacterSets, lowercaseLetterCharacterSet);
    CFArrayAppendValue(requiredCharacterSets, decimalDigitCharacterSet);
    
    CFDictionaryAddValue(passwordRequirements, kSecPasswordAllowedCharactersKey, allowedCharacters);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordRequiredCharactersKey, requiredCharacterSets);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordContainsNoMoreThanNConsecutiveIdenticalCharacters, threshold);
    
    password = SecPasswordGenerate(kSecPasswordTypeSafari, &error, passwordRequirements);
    isnt(password, NULL);
    ok(error == NULL);

    error = NULL;
    CFReleaseNull(password);
    CFRelease(passwordRequirements);
    CFRelease(minRef);
    CFRelease(maxRef);
    CFRelease(allowedCharacters);
    CFRelease(requiredCharacterSets);

/////////////////now test all the error cases
    //test with no required characters
    
    min = 24;
    max = 32;
    
    minRef = CFNumberCreate(NULL, kCFNumberIntType, &min);
    maxRef = CFNumberCreate(NULL, kCFNumberIntType, &max);
    passwordRequirements = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);

    allowedCharacters = CFSTR("abcdsefw2345");
    requiredCharacterSets = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

    CFDictionaryAddValue(passwordRequirements, kSecPasswordAllowedCharactersKey, allowedCharacters);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordRequiredCharactersKey, requiredCharacterSets);
    password = SecPasswordGenerate(kSecPasswordTypeWifi, &error, passwordRequirements);
    ok(password == NULL);
    ok(error != NULL);

    CFRelease(error);
    error = NULL;
    CFRelease(minRef);
    CFRelease(maxRef);
    CFRelease(allowedCharacters);
    CFRelease(passwordRequirements);

    //test with no allowed characters
    min = 24;
    max = 32;
    
    minRef = CFNumberCreate(NULL, kCFNumberIntType, &min);
    maxRef = CFNumberCreate(NULL, kCFNumberIntType, &max);
    
    passwordRequirements = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMinLengthKey, minRef);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMaxLengthKey, maxRef);
    allowedCharacters = CFSTR("");
    
    CFDictionaryAddValue(passwordRequirements, kSecPasswordAllowedCharactersKey, allowedCharacters);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordRequiredCharactersKey, requiredCharacterSets);
    password = SecPasswordGenerate(kSecPasswordTypeWifi, &error, passwordRequirements);
    ok(password == NULL);
    ok(error != NULL);

    CFRelease(error);
    error = NULL;
    CFRelease(minRef);
    CFRelease(maxRef);
    CFRelease(passwordRequirements);

    //test with min > max
    min = 32;
    max = 20;
    
    minRef = CFNumberCreate(NULL, kCFNumberIntType, &min);
    maxRef = CFNumberCreate(NULL, kCFNumberIntType, &max);
    
    passwordRequirements = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMinLengthKey, minRef);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMaxLengthKey, maxRef);
    allowedCharacters = CFSTR("abcdsefw2345");
    
    CFDictionaryAddValue(passwordRequirements, kSecPasswordAllowedCharactersKey, allowedCharacters);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordRequiredCharactersKey, requiredCharacterSets);
    password = SecPasswordGenerate(kSecPasswordTypeWifi, &error, passwordRequirements);
    ok(password == NULL);
    ok(error != NULL);

    error = NULL;
    CFRelease(minRef);
    CFRelease(maxRef);
    CFRelease(allowedCharacters);
    CFRelease(passwordRequirements);

    //test by ommitting dictionary parameters
    
    //omit max length
    min = 20;
    max = 32;
    
    minRef = CFNumberCreate(NULL, kCFNumberIntType, &min);
    maxRef = CFNumberCreate(NULL, kCFNumberIntType, &max);
    
    passwordRequirements = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMinLengthKey, minRef);
    allowedCharacters = CFSTR("abcdsefw2345");
    
    CFDictionaryAddValue(passwordRequirements, kSecPasswordAllowedCharactersKey, allowedCharacters);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordRequiredCharactersKey, requiredCharacterSets);
    password = SecPasswordGenerate(kSecPasswordTypeWifi, &error, passwordRequirements);
    ok(password == NULL);
    ok(error != NULL);

    error = NULL;
    CFRelease(minRef);
    CFRelease(maxRef);
    CFRelease(allowedCharacters);
    CFRelease(passwordRequirements);

    //omit min length
    min = 20;
    max = 32;
    
    minRef = CFNumberCreate(NULL, kCFNumberIntType, &min);
    maxRef = CFNumberCreate(NULL, kCFNumberIntType, &max);
    
    passwordRequirements = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMaxLengthKey, maxRef);
    allowedCharacters = CFSTR("abcdsefw2345");
    
    CFDictionaryAddValue(passwordRequirements, kSecPasswordAllowedCharactersKey, allowedCharacters);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordRequiredCharactersKey, requiredCharacterSets);
    password = SecPasswordGenerate(kSecPasswordTypeWifi, &error, passwordRequirements);
    ok(password == NULL);
    ok(error != NULL);

    error = NULL;
    CFRelease(minRef);
    CFRelease(maxRef);
    CFRelease(allowedCharacters);
    CFRelease(passwordRequirements);

    //omit allowed characters
    min = 20;
    max = 32;
    
    minRef = CFNumberCreate(NULL, kCFNumberIntType, &min);
    maxRef = CFNumberCreate(NULL, kCFNumberIntType, &max);
    
    passwordRequirements = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMinLengthKey, minRef);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMaxLengthKey, maxRef);
    allowedCharacters = CFSTR("abcdsefw2345");
    
    CFDictionaryAddValue(passwordRequirements, kSecPasswordRequiredCharactersKey, requiredCharacterSets);
    password = SecPasswordGenerate(kSecPasswordTypeWifi, &error, passwordRequirements);
    ok(password == NULL);
    ok(error != NULL);

    error = NULL;
    CFRelease(minRef);
    CFRelease(maxRef);
    CFRelease(allowedCharacters);
    CFRelease(passwordRequirements);

    //omit required characters
    min = 20;
    max = 32;
    
    minRef = CFNumberCreate(NULL, kCFNumberIntType, &min);
    maxRef = CFNumberCreate(NULL, kCFNumberIntType, &max);
    
    passwordRequirements = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMinLengthKey, minRef);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMaxLengthKey, maxRef);
    allowedCharacters = CFSTR("abcdsefw2345");
    
    CFDictionaryAddValue(passwordRequirements, kSecPasswordAllowedCharactersKey, allowedCharacters);
    password = SecPasswordGenerate(kSecPasswordTypeWifi, &error, passwordRequirements);
    ok(password == NULL);
    ok(error != NULL);
    
    error = NULL;
    CFRelease(minRef);
    CFRelease(maxRef);
    CFRelease(allowedCharacters);
    CFRelease(passwordRequirements);

    //pass in wrong type for min
    min = 20;
    max = 32;
    
    minRef = CFNumberCreate(NULL, kCFNumberIntType, &min);
    maxRef = CFNumberCreate(NULL, kCFNumberIntType, &max);
    
    passwordRequirements = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMinLengthKey, allowedCharacters);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMaxLengthKey, maxRef);
    allowedCharacters = CFSTR("abcdsefw2345");
    requiredCharacterSets = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFArrayAppendValue(requiredCharacterSets, uppercaseLetterCharacterSet);
    CFArrayAppendValue(requiredCharacterSets, lowercaseLetterCharacterSet);
    CFArrayAppendValue(requiredCharacterSets, decimalDigitCharacterSet);
    
    CFDictionaryAddValue(passwordRequirements, kSecPasswordAllowedCharactersKey, allowedCharacters);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordRequiredCharactersKey, requiredCharacterSets);
    
    password = SecPasswordGenerate(kSecPasswordTypeWifi, &error, passwordRequirements);
    ok(password == NULL);
    ok(error != NULL);

    error = NULL;
    CFRelease(passwordRequirements);
    CFRelease(minRef);
    CFRelease(maxRef);
    CFRelease(allowedCharacters);
    
    //pass in wrong type for max
    min = 20;
    max = 32;
    
    minRef = CFNumberCreate(NULL, kCFNumberIntType, &min);
    maxRef = CFNumberCreate(NULL, kCFNumberIntType, &max);
    
    passwordRequirements = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMinLengthKey, minRef);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMaxLengthKey, allowedCharacters);
    allowedCharacters = CFSTR("abcdsefw2345");
    
    CFDictionaryAddValue(passwordRequirements, kSecPasswordAllowedCharactersKey, allowedCharacters);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordRequiredCharactersKey, requiredCharacterSets);

    password = SecPasswordGenerate(kSecPasswordTypeWifi, &error, passwordRequirements);
    ok(password == NULL);    
    ok(error != NULL);

    error = NULL;
    CFRelease(passwordRequirements);
    CFRelease(minRef);
    CFRelease(maxRef);
    CFRelease(allowedCharacters);
    
    //pass in wrong type for allowed
    min = 20;
    max = 32;
    
    minRef = CFNumberCreate(NULL, kCFNumberIntType, &min);
    maxRef = CFNumberCreate(NULL, kCFNumberIntType, &max);
    
    passwordRequirements = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMinLengthKey, minRef);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMaxLengthKey, maxRef);
    allowedCharacters = CFSTR("abcdsefw2345");
    
    CFDictionaryAddValue(passwordRequirements, kSecPasswordAllowedCharactersKey, minRef);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordRequiredCharactersKey, requiredCharacterSets);

    password = SecPasswordGenerate(kSecPasswordTypeWifi, &error, passwordRequirements);
    ok(password == NULL);
    ok(error != NULL);

    error = NULL;
    CFRelease(passwordRequirements);
    CFRelease(minRef);
    CFRelease(maxRef);
    CFRelease(allowedCharacters);
    
    //pass in wrong type for required
    min = 20;
    max = 32;
    
    minRef = CFNumberCreate(NULL, kCFNumberIntType, &min);
    maxRef = CFNumberCreate(NULL, kCFNumberIntType, &max);
    
    passwordRequirements = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMinLengthKey, minRef);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMaxLengthKey, maxRef);
    allowedCharacters = CFSTR("abcdsefw2345");
    
    CFDictionaryAddValue(passwordRequirements, kSecPasswordAllowedCharactersKey, allowedCharacters);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordRequiredCharactersKey, minRef);
    password = SecPasswordGenerate(kSecPasswordTypeWifi, &error, passwordRequirements);
    ok(password == NULL);
    ok(error != NULL);

    error = NULL;
    CFRelease(passwordRequirements);
    CFRelease(minRef);
    CFRelease(maxRef);
    CFRelease(allowedCharacters);
    
    //pass in wrong type for no less than
    min = 20;
    max = 32;
    
    minRef = CFNumberCreate(NULL, kCFNumberIntType, &min);
    maxRef = CFNumberCreate(NULL, kCFNumberIntType, &max);
    
    passwordRequirements = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    requiredCharacterSets = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFArrayAppendValue(requiredCharacterSets, uppercaseLetterCharacterSet);
    CFArrayAppendValue(requiredCharacterSets, lowercaseLetterCharacterSet);
    CFArrayAppendValue(requiredCharacterSets, decimalDigitCharacterSet);
    
    CFDictionaryAddValue(passwordRequirements, kSecPasswordAllowedCharactersKey, allowedCharacters);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordRequiredCharactersKey, requiredCharacterSets);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMinLengthKey, minRef);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMaxLengthKey, maxRef);
    allowedCharacters = CFSTR("abcdsefw2345");
    
    CFDictionaryAddValue(passwordRequirements, kSecPasswordAllowedCharactersKey, allowedCharacters);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordRequiredCharactersKey, minRef);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordContainsAtLeastNSpecificCharacters, CFSTR("hehe"));
    password = SecPasswordGenerate(kSecPasswordTypeWifi, &error, passwordRequirements);
    ok(password == NULL);
    ok(error != NULL);

    error = NULL;
    CFRelease(passwordRequirements);
    CFRelease(minRef);
    CFRelease(maxRef);
    CFRelease(allowedCharacters);

    //pass in wrong type for no more than
    min = 20;
    max = 32;
    
    minRef = CFNumberCreate(NULL, kCFNumberIntType, &min);
    maxRef = CFNumberCreate(NULL, kCFNumberIntType, &max);
    
    passwordRequirements = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    requiredCharacterSets = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFArrayAppendValue(requiredCharacterSets, uppercaseLetterCharacterSet);
    CFArrayAppendValue(requiredCharacterSets, lowercaseLetterCharacterSet);
    CFArrayAppendValue(requiredCharacterSets, decimalDigitCharacterSet);
    
    CFDictionaryAddValue(passwordRequirements, kSecPasswordAllowedCharactersKey, allowedCharacters);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordRequiredCharactersKey, requiredCharacterSets);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMinLengthKey, minRef);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMaxLengthKey, maxRef);
    allowedCharacters = CFSTR("abcdsefw2345");
    
    CFDictionaryAddValue(passwordRequirements, kSecPasswordAllowedCharactersKey, allowedCharacters);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordRequiredCharactersKey, minRef);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordContainsNoMoreThanNSpecificCharacters, CFSTR("hehe"));

    password = SecPasswordGenerate(kSecPasswordTypeWifi, &error, passwordRequirements);
    ok(password == NULL);
    ok(error != NULL);

    error = NULL;
    CFRelease(passwordRequirements);
    CFRelease(minRef);
    CFRelease(maxRef);
    CFRelease(allowedCharacters);

    //pass in wrong disallowed characters
    min = 20;
    max = 32;
    
    minRef = CFNumberCreate(NULL, kCFNumberIntType, &min);
    maxRef = CFNumberCreate(NULL, kCFNumberIntType, &max);
    
    passwordRequirements = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    requiredCharacterSets = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFArrayAppendValue(requiredCharacterSets, uppercaseLetterCharacterSet);
    CFArrayAppendValue(requiredCharacterSets, lowercaseLetterCharacterSet);
    CFArrayAppendValue(requiredCharacterSets, decimalDigitCharacterSet);
    
    CFDictionaryAddValue(passwordRequirements, kSecPasswordAllowedCharactersKey, allowedCharacters);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordRequiredCharactersKey, requiredCharacterSets);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMinLengthKey, minRef);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMaxLengthKey, maxRef);
    allowedCharacters = CFSTR("abcdsefw2345");
    
    CFDictionaryAddValue(passwordRequirements, kSecPasswordAllowedCharactersKey, allowedCharacters);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordRequiredCharactersKey, minRef);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordDisallowedCharacters, requiredCharacterSets);
    
    password = SecPasswordGenerate(kSecPasswordTypeWifi, &error, passwordRequirements);
    ok(password == NULL);
    ok(error != NULL);
    
    error = NULL;
    CFRelease(passwordRequirements);
    CFRelease(minRef);
    CFRelease(maxRef);
    CFRelease(allowedCharacters);
    
    //pass in wrong type for no more than's dictionary
    min = 20;
    max = 32;
    
    minRef = CFNumberCreate(NULL, kCFNumberIntType, &min);
    maxRef = CFNumberCreate(NULL, kCFNumberIntType, &max);
    
    CFMutableDictionaryRef wrongCount = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFDictionaryAddValue(wrongCount, kSecPasswordCharacters, CFSTR("lkj"));
    CFDictionaryAddValue(wrongCount, kSecPasswordCharacterCount, CFSTR("sdf"));
    
    passwordRequirements = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    requiredCharacterSets = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFArrayAppendValue(requiredCharacterSets, uppercaseLetterCharacterSet);
    CFArrayAppendValue(requiredCharacterSets, lowercaseLetterCharacterSet);
    CFArrayAppendValue(requiredCharacterSets, decimalDigitCharacterSet);
    
    CFDictionaryAddValue(passwordRequirements, kSecPasswordAllowedCharactersKey, allowedCharacters);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordRequiredCharactersKey, requiredCharacterSets);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMinLengthKey, minRef);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMaxLengthKey, maxRef);
    allowedCharacters = CFSTR("abcdsefw2345");
    
    CFDictionaryAddValue(passwordRequirements, kSecPasswordAllowedCharactersKey, allowedCharacters);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordRequiredCharactersKey, minRef);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordContainsNoMoreThanNSpecificCharacters, wrongCount);
    
    password = SecPasswordGenerate(kSecPasswordTypeWifi, &error, passwordRequirements);
    ok(password == NULL);
    ok(error != NULL);

    error = NULL;
    CFRelease(wrongCount);
    CFRelease(passwordRequirements);
    CFRelease(minRef);
    CFRelease(maxRef);
    CFRelease(allowedCharacters);

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

}

int si_73_secpasswordgenerate(int argc, char *const *argv)
{
	plan_tests(298);
	tests();
    
	return 0;
}
