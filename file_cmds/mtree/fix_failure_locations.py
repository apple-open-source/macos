#!/usr/bin/python

#
# This script is used to automatically fix up the location numbers in
# calls to RECORD_FAILURE().  When adding a new call to RECORD_FAILURE,
# write it like:
#	RECORD_FAILURE(0, ...);
# Don't put any white space between the open parenthesis, zero and comma.
# Once you have added the new calls to RECORD_FAILURE, then run this script,
# passing it the path to the directory, like this:
#	python3 mtree/fix_failure_locations.py mtree/
#
# This script will edit the files, changing the "0" to the next available
# location number.  It will also detect and complain if you have duplicate
# location numbers.
#
# DO NOT reuse location numbers!  It is best if locations are consistent across
# all versions that have that RECORD_FAILURE call.
#

import sys
import os
import re
from collections import defaultdict
from datetime import datetime,timezone

class LocationUpdater(object):
    epoch = datetime(2020, 6, 17, 23, 22, 46, 562458, tzinfo=timezone.utc)
    location_base = int((datetime.now(timezone.utc) - epoch).total_seconds() / 60)
    # Match the number in "RECORD_FAILURE(<number>,"
    fail_re = (re.compile('(?<=\\bRECORD_FAILURE\\()\\d+(?=,)'),re.compile('(?<=\\bRECORD_FAILURE_MSG\\()\\d+(?=,)'))
    
    def __init__(self, path):
        self.location = self.location_base
        self.path = path
        # Counters for how often each location number was found
        self.counts = defaultdict(int)
        self.locations_changed = 0
    
    # Replace the "0" in "RECORD_FAILURE(0," with next location number, in *.c
    def fixLocations(self):
        def replace_loc(match):
            location = int(match.group(0))
            if location == 0:
                # Replace location 0 with the next available location
                self.location += 1
                self.locations_changed += 1
                location = self.location
            # Count the number of times this location number was used
            self.counts[location] += 1
            # Return the (possibly updated) location number
            return str(location)
        rootpath = self.path
        for dirpath, dirnames, filenames in os.walk(rootpath):
            for filename in filenames:
                if filename.endswith(".c") or filename.endswith(".cpp"):
                    path = os.path.join(dirpath, filename)
                    content = open(path, "r").read()
                    for fail_re in self.fail_re:
                        if fail_re.search(content):
                            locations_changed_before = self.locations_changed
                            content = fail_re.sub(replace_loc, content)
                            if self.locations_changed != locations_changed_before:
                                # We updated a location number, so write the changed file
                                print("Updating file {}".format(path))
                                open(path,"w").write(content)
    
    def duplicates(self):
        # Return the list of keys whose count is greater than 1
        return [k for (k,v) in iter(self.counts.items()) if v > 1]

updater = LocationUpdater(sys.argv[1])
updater.fixLocations()
dups = updater.duplicates()
if len(dups):
    print("WARNING!  Duplicate location numbers: {}".format(dups))
    sys.exit(1)
