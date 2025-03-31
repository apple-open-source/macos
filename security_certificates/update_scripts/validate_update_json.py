#!/usr/bin/env python3

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
        raise("file \"" + filename + "\" does not exist")
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

def validate_update_against_version(filename, version_filename):
    update = readJson(filename)
    versionPlist = readPlist(version_filename)
    version = versionPlist["VersionNumber"]
    updateVersion = update["oldVersion"]
    if version != updateVersion:
        raise ValueError("Update for " + updateVersion + " does not apply to " + version)

