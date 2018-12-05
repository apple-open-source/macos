#!/usr/bin/python
#
# BuildTrustedCTLogsList.py
# Copyright 2018 Apple Inc. All rights reserved.
#
# Process the log_list.json and create the plist to be shipped to software 

import sys
import os
import argparse
import json
import base64
import Foundation
from datetime import datetime
from PyObjCTools.Conversion import propertyListFromPythonCollection

def dataConverter(value):
    if isinstance(value, Foundation.NSData):
        return value
    raise TypeError("Type '%s' encountered in Python collection; don't know how to convert." % type(aPyCollection))


def checkValue(value, type, failureString):
    if not isinstance(value, type):
        raise TypeError(failureString)

parser = argparse.ArgumentParser(description="Calculate log ids and add to log list")
parser.add_argument('-infile', help="The filename of the json log list", required=True)
parser.add_argument('-outfile', help="The filename of the plist log list", required=True)

args = parser.parse_args()

f = open(args.infile)
log_list = json.load(f)
log_array = []

checkValue(log_list, dict, args.infile + " is not a json dictionary")
checkValue(log_list["$schema"], basestring, "failed to get \'$schema\' version from " + args.infile)
if log_list["$schema"] != "https://valid.apple.com/ct/log_list/schema_versions/log_list_schema_v2.json":
    raise ValueError("unknown schema " +  log_list["$schema"] + " for  " + args.infile)

checkValue(log_list["operators"], dict, "failed to get \'operators\' dictionary from " + args.infile)

for operator,operator_dict in log_list["operators"].iteritems():
    checkValue(operator, basestring, "failed to get operator string from " + args.infile)
    checkValue(operator_dict, dict, "failed to get operator dictionary for " + operator)
    checkValue(operator_dict["logs"], dict, "failed to get \'logs\' dictionary for " + operator)

    for log_name,log_dict in operator_dict["logs"].items():
        checkValue(log_dict, dict, "failed to get log dictionary for log \"" + log_name + "\" for operator \"" + operator + "\"")

        state = log_dict["state"]
        checkValue(state, dict, "failed to get \'state\' for log \"" + log_name + "\" for operator \"" + operator + "\"")

        if "pending" not in state and "rejected" not in state:
            log_entry = {}
            log_entry['operator'] = operator

            checkValue(log_dict["key"], basestring, "failed to get \'key\' for log \"" + log_name + "\" for operator \"" + operator + "\"")
            key_data = base64.b64decode(log_dict["key"])
            log_entry['key'] = Foundation.NSData.dataWithBytes_length_(key_data, len(key_data))

            if "frozen" in state:
                checkValue(state["frozen"]["timestamp"], basestring, "failed to get frozen timestamp for log \"" + log_name + "\" for operator \"" + operator + "\"")
                log_entry['frozen'] = datetime.strptime(state["frozen"]["timestamp"],"%Y-%m-%dT%H:%M:%SZ") 
            elif "retired" in state:
                checkValue(state["retired"]["timestamp"], basestring, "failed to get retired timestamp for log \"" + log_name + "\" for operator \"" + operator + "\"")
                log_entry['expiry'] = datetime.strptime(state["retired"]["timestamp"],"%Y-%m-%dT%H:%M:%SZ")
            elif "qualified" not in state and "usable" not in state:
                raise ValueError("unknown state for log \"" + log_name + "\" for operator \"" + operator + "\"")

            log_array.append(log_entry)

out_dir = os.path.dirname(args.outfile)
if not os.path.exists(out_dir):
    os.makedirs(out_dir)

plist = propertyListFromPythonCollection(log_array, conversionHelper=dataConverter)
checkValue(plist, Foundation.NSArray, "failed to convert python data to NSArray")

success = plist.writeToFile_atomically_(args.outfile, 1)
if not success:
    print "trusted logs plist failed to write, error!"
    sys.exit(1)
