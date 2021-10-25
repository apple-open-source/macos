#!/usr/bin/python3
from __future__ import print_function
import sys
import Foundation
from glob import glob
import re
import os

test_dir = sys.argv[1]
outfile = sys.argv[2]

test_plist = Foundation.NSMutableDictionary.dictionary()
test_plist['Project'] = 'Security'
test_plist['RequiresWiFi'] = True
test_list = Foundation.NSMutableArray.array()

test_files = glob(test_dir + '/*/*.m')

for filename in test_files:
    f = open(filename, 'r')
    for line in f:
        match = re.search('@interface ([a-zA-Z0-9_]+) ?: ?Trust[a-zA-Z0-9_]+TestCase', line)
        if match:
            regex_string = test_dir + '/([a-zA-Z0-9_]+/)'
            test_dir_match = re.search(regex_string, filename)
            if not test_dir_match:
                print('failed to find test dir name for ' + filename + " with regex " + regex_string)
                sys.exit(1)
            test_dictionary = Foundation.NSMutableDictionary.dictionary()
            test_dictionary['TestName'] = test_dir_match.group(1) + match.group(1)
            test_dictionary['ShowSubtestResults']= True
            test_dictionary['WorkingDirectory'] = '/AppleInternal/XCTests/com.apple.security/'
            test_dictionary['AsRoot'] = True

            test_command = Foundation.NSMutableArray.array()
            test_command.append('/AppleInternal/CoreOS/tests/Security/TrustTests')
            test_command.append('-c')
            test_command.append(match.group(1))
            test_command.append('-t')
            test_command.append('TrustTests')

            test_dictionary['Command'] = test_command

            test_list.append(test_dictionary)
    f.close()

test_plist['Tests'] = test_list

out_dir = os.path.dirname(outfile)
if not os.path.exists(out_dir):
    os.makedirs(out_dir)
success = test_plist.writeToFile_atomically_(outfile, 1)
if not success:
    print("test plist failed to write, error!")
    sys.exit(1)
