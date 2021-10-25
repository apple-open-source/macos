//
//  secd-39-sosdictionaryupdate.m
//  protobuf_source_generation
//
//  Created by murf on 3/30/21.
//

#import <Foundation/Foundation.h>
#import <CoreFoundation/CoreFoundation.h>
#include "secd_regressions.h"
#include "keychain/securityd/Regressions/SecdTestKeychainUtilities.h"

#include <keychain/SecureObjectSync/SOSDictionaryUpdate.h>
#include <utilities/SecCFWrappers.h>

int secd_39_sosdictionaryupdate(int argc, char *const *argv)
{
    plan_tests(9);
    secd_test_setup_temp_keychain(__FUNCTION__, NULL);
    SOSDictionaryUpdate *theMeasure = [[SOSDictionaryUpdate alloc] init];
    CFMutableDictionaryRef theDict1 = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);

    // need to feed a dictionary to a new SOSDictionaryUpdate to see a change
    ok([theMeasure hasChanged: NULL] == false, "the dictionary didn't change");

    ok([theMeasure hasChanged: theDict1] == true, "the dictionary did change");
    
    CFDictionaryAddValue(theDict1, CFSTR("item1"), CFSTR("value1"));
    ok([theMeasure hasChanged: theDict1] == true, "the dictionary did change");

    ok([theMeasure hasChanged: theDict1] == false, "the dictionary didn't change");

    CFDictionaryAddValue(theDict1, CFSTR("item2"), CFSTR("value2"));
    ok([theMeasure hasChanged: theDict1] == true, "the dictionary did change");

    CFDictionaryAddValue(theDict1, CFSTR("item3"), CFSTR("value3"));
    CFDictionaryRemoveValue(theDict1, CFSTR("item3"));
    ok([theMeasure hasChanged: theDict1] == false, "the dictionary didn't change");
    
    ok([theMeasure hasChanged: NULL] == true, "the dictionary did change");
    [theMeasure reset];  // does this crash?

    ok([theMeasure hasChanged: theDict1] == true, "the dictionary did change");
    [theMeasure reset];  // does this crash?

    secd_test_teardown_delete_temp_keychain(__FUNCTION__);
    CFReleaseNull(theDict1);
    return 0;
    
}
