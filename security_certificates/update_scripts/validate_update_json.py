#!/usr/bin/env python3

import argparse
import json
import sys
import os
from jsonschema import validators, Draft7Validator, FormatChecker
import plistlib

def readPlist(filename):
    if not os.path.isfile(filename):
        raise ValueError("file \"" + filename + "\" does not exist")
    try:
        f = open(filename, mode='rb')
        plist = plistlib.load(f)
    except:
        raise ValueError("file \"" + filename + "\" is not valid plist")
    return plist

def readJson(filename):
    if not os.path.isfile(filename):
        raise ValueError("file \"" + filename + "\" does not exist")
    try:
        f = open(filename, mode='rb')
        object = json.load(f)
    except:
        raise ValueError("file \"" + filename + "\" is not valid JSON")
    return object

def validate_update_against_schema(filename, schema_filename):
    schema = readJson(schema_filename)
    update = readJson(filename)
    try:
        validator = Draft7Validator(schema, format_checker=FormatChecker())
        validator.validate(update)
        print("file \"" + filename + "\" conforms to schema",file=sys.stderr)
    except Exception as e:
        raise ValueError("file \"" + filename + "\" does not conform to schema: " + str(e))

def main():
    parser = argparse.ArgumentParser(description="Validate a trust store update json",
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--srcroot', help="source root path (for schema)", required=True)
    parser.add_argument('--update_json', help="new or existing update json", required=True)
    args = parser.parse_args()

    # validate existing updates file
    schema_file = args.srcroot + "/update_scripts/trust_store_updates_schema_v2.json"
    if os.path.isfile(args.update_json):
        validate_update_against_schema(args.update_json, schema_file)

if __name__ == "__main__":
    main()
