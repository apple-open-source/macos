#!/usr/bin/python
#
# TODO(kirk or SEAR/QA) after radar 53867279 is fixed, please delete this script
#
# WARNING: if you add new tests to the swift octagon tests, it is possible that
# this script will not find them and then your new tests will not get executed
# in BATS!
#
import sys
import Foundation
from glob import glob
import re
import os
import pprint

test_dir = sys.argv[1]
outfile = sys.argv[2]

test_plist = Foundation.NSMutableDictionary.dictionary()
test_plist['BATSConfigVersion'] = '0.1.0'
test_plist['Project'] = 'Security'
test_list = Foundation.NSMutableArray.array()

test_files = glob(test_dir + '/*.m') + glob(test_dir + '/*.h')

def get_class_names():
    test_classes = ['CKKSLaunchSequenceTests']
    for filename in test_files:
        f = open(filename, 'r')
        for line in f:
            match = re.search('@interface ([a-zA-Z0-9_]+) ?: ?CloudKitKeychain[a-zA-Z0-9_]*TestsBase', line)
            #match = re.search('class (([a-zA-Z0-9_]+Tests)|(OctagonTests[a-zA-Z0-9_]*(?<!Base))): ', line) 
            if match:
                test_classes.append(match.group(1))

            # And pick up everything that's a default xctest, too
            match = re.search('@interface ([a-zA-Z0-9_]+) ?: ?XCTestCase', line)
            if match:
                test_classes.append(match.group(1))

            # or based on CloudKitMockXCTest
            match = re.search('@interface ([a-zA-Z0-9_]+) ?: ?CloudKitMockXCTest', line)
            if match:
                test_classes.append(match.group(1))
        f.close()

    # But we don't want any helper classes
    base_classes = ['CloudKitMockXCTest', 'CloudKitKeychainSyncingMockXCTest', 'CKKSCloudKitTests']
    for base_class in base_classes:
        test_classes = [x for x in test_classes if base_class != x]
    return test_classes

for x in get_class_names():

    test_dictionary = Foundation.NSMutableDictionary.dictionary()
    test_dictionary['TestName'] = x
    test_dictionary['Timeout']= 1200
    test_dictionary['ShowSubtestResults']= True
    test_dictionary['WorkingDirectory'] = '/AppleInternal/XCTests/com.apple.security/'

    test_command = Foundation.NSMutableArray.array()
    test_command.append('BATS_XCTEST_CMD -XCTest {} CKKSTests.xctest'.format(x))
    test_dictionary['Command'] = test_command

    test_list.append(test_dictionary)

test_plist['Tests'] = test_list

out_dir = os.path.dirname(outfile)
if not os.path.exists(out_dir):
    os.makedirs(out_dir)
success = test_plist.writeToFile_atomically_(outfile, 1)
if not success:
    print "test plist failed to write, error!"
    sys.exit(1)
