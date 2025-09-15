#!/usr/bin/python3
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
from datetime import datetime, tzinfo, timedelta
from PyObjCTools.Conversion import propertyListFromPythonCollection
import re

class UTC(tzinfo):
    """UTC"""

    def utcoffset(self, dt):
        return timedelta(0)

    def tzname(self, dt):
        return "UTC"

    def dst(self, dt):
        return timedelta(0)

def dataConverter(value):
    if isinstance(value, Foundation.NSData):
        return value
    raise TypeError("Type '%s' encountered in Python collection; don't know how to convert." % type(aPyCollection))


def checkValue(value, type, failureString):
    if not isinstance(value, type):
        raise TypeError(failureString)

def checkTime(value, failureString):
    checkValue(value, str, failureString)
    pattern = re.compile("^[0-9]{4}-[0-1][0-9]-[0-3][0-9]T[0-2][0-9]:[0-5][0-9]:[0-6][0-9]Z$")
    if not pattern.match(value):
        raise ValueError(failureString + ": incorrectly formatted time " + value + ", expected \"YYYY-MM-DDTHH:mm:ssZ\"")

parser = argparse.ArgumentParser(description="Build the trusted logs plist from the log list json")
parser.add_argument('-non_tls', action='store_true', help="Output log list should exclude tls_only logs")
parser.add_argument('-infile', help="The filename of the json log list", required=True)
parser.add_argument('-outfile', help="The filename of the plist log list", required=True)

args = parser.parse_args()

print("Building " + args.outfile + " from " + args.infile + " with non-tls " + str(args.non_tls))

f = open(args.infile)
log_list = json.load(f)
log_array = []

checkValue(log_list, dict, args.infile + " is not a json dictionary")
checkValue(log_list["$schema"], str, "failed to get \'$schema\' version from " + args.infile)
if log_list["$schema"] != "https://valid.apple.com/ct/log_list/schema_versions/log_list_schema_v5.json":
    raise ValueError("unknown schema " +  log_list["$schema"] + " for  " + args.infile)

checkValue(log_list["operators"], list, "failed to get \'operators\' array from " + args.infile)

for operator_dict in log_list["operators"]:
    checkValue(operator_dict, dict, "failed to get operator dictionary for index " + str(log_list["operators"].index(operator_dict)))
    checkValue(operator_dict["name"], str, "failed to get operator name from " + args.infile)
    operator = operator_dict["name"]
    checkValue(operator_dict["logs"], list, "failed to get \'logs\' array for " + operator)
    checkValue(operator_dict["tiled_logs"], list, "failed to get \'tiled_logs\' array for " + operator)

    logs = operator_dict["logs"] + operator_dict["tiled_logs"]

    for log_dict in logs:
        log_index = logs.index(log_dict)

        # Make a string to tell us which log this is upon failure
        error_string_log_index = ""
        if log_index < len(operator_dict["logs"]):
            error_string_log_index = " log index \"" + str(log_index) + "\" "
        else:
            tiled_log_index = log_index - len(operator_dict["logs"])
            error_string_log_index =  "tiled log index \"" + str(tiled_log_index) + "\" "

        checkValue(log_dict, dict, "failed to get log dictionary for" + error_string_log_index + "for operator \"" + operator + "\"")

        # skip if we're making a non-TLS log list and this is a tls-only log
        if args.non_tls and "tls_only" in log_dict:
            continue

        state = log_dict["state"]
        checkValue(state, dict, "failed to get \'state\' for" + error_string_log_index + "for operator \"" + operator + "\"")

        # skip completely untrusted logs
        if "pending" not in state and "rejected" not in state:
            log_entry = {}
            log_entry['operator'] = operator

            checkValue(log_dict["key"], str, "failed to get \'key\' for" + error_string_log_index + "for operator \"" + operator + "\"")
            key_data = base64.b64decode(log_dict["key"])
            log_entry['key'] = Foundation.NSData.dataWithBytes_length_(key_data, len(key_data))

            checkValue(log_dict["log_id"], str, "failed to get \'log_id\' for" + error_string_log_index + "for operator \"" + operator + "\"")
            log_id = base64.b64decode(log_dict["log_id"])
            log_entry['log_id'] = Foundation.NSData.dataWithBytes_length_(log_id, len(log_id))

            if "readonly" in state:
                checkTime(state["readonly"]["timestamp"], "failed to get frozen timestamp for" + error_string_log_index + "for operator \"" + operator + "\"")
                log_entry['frozen'] = datetime.strptime(state["readonly"]["timestamp"],"%Y-%m-%dT%H:%M:%SZ").replace(tzinfo=UTC())
            elif "retired" in state:
                checkTime(state["retired"]["timestamp"], "failed to get retired timestamp for" + error_string_log_index + "for operator \"" + operator + "\"")
                log_entry['expiry'] = datetime.strptime(state["retired"]["timestamp"],"%Y-%m-%dT%H:%M:%SZ").replace(tzinfo=UTC())
            elif "qualified" not in state and "usable" not in state:
                raise ValueError("unknown state for" + error_string_log_index + "for operator \"" + operator + "\"")

            if "temporal_interval" in log_dict:
                checkTime(log_dict["temporal_interval"]["start_inclusive"], "failed to get start inclusive timestamp for" + error_string_log_index + "for operator \"" + operator + "\"")
                checkTime(log_dict["temporal_interval"]["end_exclusive"], "failed to get end exclusive timestamp for" + error_string_log_index + "for operator \"" + operator + "\"")
                log_entry['start_inclusive'] = datetime.strptime(log_dict["temporal_interval"]["start_inclusive"],"%Y-%m-%dT%H:%M:%SZ").replace(tzinfo=UTC())
                log_entry['end_exclusive'] = datetime.strptime(log_dict["temporal_interval"]["end_exclusive"],"%Y-%m-%dT%H:%M:%SZ").replace(tzinfo=UTC())

            log_array.append(log_entry)

out_dir = os.path.dirname(args.outfile)
if not os.path.exists(out_dir):
    os.makedirs(out_dir)

plist = propertyListFromPythonCollection(log_array, conversionHelper=dataConverter)
checkValue(plist, Foundation.NSArray, "failed to convert python data to NSArray")

success = plist.writeToFile_atomically_(args.outfile, 1)
if not success:
    print("trusted logs plist failed to write, error!")
    sys.exit(1)
