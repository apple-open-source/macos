#!/usr/bin/xcrun python3 -B

import argparse
import json
import os

print("NOTE: this tool is used for building ICU the Apple Way and not a supported ICU tool.")

# usage: generate_json_for_tapi $(SRCROOT) $(OBJROOT)/tapi_headers.json YES
parser = argparse.ArgumentParser()
parser.add_argument('input', type=str)
parser.add_argument('output', type=str)
parser.add_argument('embedded_train', type=str)
args = parser.parse_args()


def is_header(filename):
    for h_type in {".h", ".hpp", ".H", ".hh", ".hxx"}:
        if filename.endswith(h_type):
            return True
    return False

def include_location(path, build_for_ios):
    if "icu/icu4c/source/i18n" in path:
        return True
    if "icu/icu4c/source/common" in path:
        return True
    if "icu/icu4c/source/io" in path and not build_for_ios:
        return True
    if "icu/icu4c/source/stubdata" in path:
        return True
    return False


def get_header_level(path, name, minimal_apis):
    if name in minimal_apis:
        return "public"

    if "i18n/unicode" in path:
        return "private"

    # Since headers are filtered out at earlier phase based on embedded build, 
    # no need to check for os type here.
    if "io/unicode" in path:
        return "private"

    return "project"

with open(f"{args.input}/apple/minimalapis.txt", "r") as in_file:
    minimal_apis = {header.strip() for header in in_file.readlines()}

    
build_for_ios = args.embedded_train.strip().lower() == "yes"
# collect relevant headers 
headers = [] 
for dirpath, dirnames, filenames in os.walk(f"{args.input}/icu/icu4c/source"):
    for name in filenames:
        filepath = f"{dirpath}/{name}"
        if include_location(filepath, build_for_ios) and is_header(filepath):
            headers.append({ 
                "type": get_header_level(filepath, name, minimal_apis),
                "path": filepath
            })



# Keep header input in stable order.
headers.sort(key = lambda obj: obj["path"][len(args.input):], reverse = True)

data = {"version" : "2", "headers": headers}

if os.path.exists(args.output):
    os.remove(args.output)

with open(args.output, "w") as out: 
    json.dump(data, out)
