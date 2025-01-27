#!/usr/bin/python3
#
# TODO(kirk or SEAR/QA) after radar 53867279 is fixed, please delete this script
#
# WARNING: if you add new tests to the swift octagon tests, it is possible that
# this script will not find them and then your new tests will not get executed
# in BATS!
#
from __future__ import print_function
import sys
import Foundation
from glob import glob
import re
import os

test_dir = sys.argv[1]
outfile = sys.argv[2]

test_plist = Foundation.NSMutableDictionary.dictionary()
test_plist['BATSConfigVersion'] = '0.1.0'
test_plist['Project'] = 'Security'
test_list = Foundation.NSMutableArray.array()

test_files = glob(test_dir + '/*.swift') + glob(test_dir + '/*/*.swift')

def get_class_names():
    test_classes = []
    for filename in test_files:
        f = open(filename, 'r')
        for line in f:
            match = re.search('class (([a-zA-Z0-9_]+Tests)|(OctagonTests[a-zA-Z0-9_]*(?<!Base))): ', line) 
            if match:
                test_classes.append('OctagonTests.{}'.format(match.group(1)))
        f.close()
    return test_classes


for x in ['TrustedPeersTests', 'TrustedPeersHelperUnitTests']:

    test_dictionary = Foundation.NSMutableDictionary.dictionary()
    test_dictionary['TestName'] = x
    test_dictionary['Timeout']= 1200
    test_dictionary['ShowSubtestResults']= True
    test_dictionary['WorkingDirectory'] = '/AppleInternal/XCTests/com.apple.security/'

    test_command = Foundation.NSMutableArray.array()
    test_command.append('BATS_XCTEST_CMD')
    test_command.append('{}.xctest'.format(x))
    test_dictionary['Command'] = test_command

    test_list.append(test_dictionary)


for x in ['OctagonTrustTests']:

    test_dictionary = Foundation.NSMutableDictionary.dictionary()
    test_dictionary['TestName'] = x
    test_dictionary['Timeout'] = 1200
    test_dictionary['ShowSubtestResults']= True
    test_dictionary['WorkingDirectory'] = '/AppleInternal/XCTests/com.apple.security/'

    test_command = Foundation.NSMutableArray.array()
    test_command.append('BATS_XCTEST_CMD')
    test_command.append('{}.xctest'.format(x))
    test_dictionary['Command'] = test_command

    test_list.append(test_dictionary)

for x in ['SecKeychainItemUpgradeRequestTests', 'AsyncPiperTests']:

    test_dictionary = Foundation.NSMutableDictionary.dictionary()
    test_dictionary['TestName'] = x
    test_dictionary['Timeout'] = 1200
    test_dictionary['ShowSubtestResults']= True
    test_dictionary['WorkingDirectory'] = '/AppleInternal/XCTests/com.apple.security/'

    test_command = Foundation.NSMutableArray.array()
    test_command.append('BATS_XCTEST_CMD')
    test_command.append('-XCTest')
    test_command.append('{}'.format(x))
    test_command.append('OctagonTests.xctest')
    test_dictionary['Command'] = test_command

    test_list.append(test_dictionary)

for x in get_class_names():

    test_dictionary = Foundation.NSMutableDictionary.dictionary()
    test_dictionary['TestName'] = x
    test_dictionary['Timeout']= 1200
    test_dictionary['ShowSubtestResults']= True
    test_dictionary['WorkingDirectory'] = '/AppleInternal/XCTests/com.apple.security/'

    test_command = Foundation.NSMutableArray.array()
    test_command.append('BATS_XCTEST_CMD')
    test_command.append('-XCTest')
    test_command.append('{}'.format(x))
    test_command.append('OctagonTests.xctest')
    test_dictionary['Command'] = test_command

    test_list.append(test_dictionary)

test_plist['Tests'] = test_list

out_dir = os.path.dirname(outfile)
if not os.path.exists(out_dir):
    os.makedirs(out_dir)
success = test_plist.writeToFile_atomically_(outfile, 1)
if not success:
    print("test plist failed to write, error!")
    sys.exit(1)
