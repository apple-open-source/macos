#!/usr/bin/python
import sys
import Foundation

plist = sys.argv[1]
description = sys.argv[2]
operator = sys.argv[3]
keyfile = sys.argv[4]

my_array = Foundation.NSMutableArray.arrayWithContentsOfFile_(plist)

if my_array is None:
  my_array = Foundation.NSMutableArray.array()


my_data = Foundation.NSData.dataWithContentsOfFile_(keyfile)

my_dict = Foundation.NSMutableDictionary.dictionary()
my_dict['description'] = description
my_dict['operator'] = operator
my_dict['key'] = my_data

my_array.append(my_dict)

print my_array

success = my_array.writeToFile_atomically_(plist, 1)
if not success:
  print "plist failed to write!"
  sys.exit(1)

