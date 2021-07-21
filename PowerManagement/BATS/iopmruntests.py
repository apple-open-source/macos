#!/usr/bin/python

import os
import subprocess
import shlex
from datetime import datetime
from sys import argv


#  /AppleInternal/CoreOS/PowerManagement/pmruntests.py
#
#  This tool is for BATS testing only. This is not intended to be run
#  by hand. This tool locats every executable test in:
#
#   /AppleInternal/CoreOS/IO
#
#  and executes each executable test therein. That is all it does.
#
#
#  See <rdar://problem/16383460> Import project for unit/regression test into
#                       /AppleInternal/CoreOS as part of OSX/iOS build

testsPath = '/AppleInternal/CoreOS/PowerManagement'

DATE_FORMAT = "%Y/%m/%d %H:%M:%S %Z"

invoked_count = 0

def main():

    execPath = os.path.realpath(testsPath)

    myPyName = os.path.basename(argv[0])

    print "Running: %s at %s"%(myPyName, str(datetime.now().strftime(DATE_FORMAT)))

    if not os.path.exists(execPath):
        print "Unexpected: %s doesn't exist. Exiting"%(execPath)
    else:

        # CHOMP CHOMP CHOMP
        for root, dir, files in os.walk(execPath):
            for name in files:
                if (myPyName == name):
                    print "Skipping %s"%(myPyName)
                    continue

                call_command(os.path.join(root, name), True)

    print "All done. Ran a total of %d tests."%(invoked_count)



# call_command
#
# Executes the file at path 'command' in a new process.
# Returns stodout, stderr as a pair.
#
def call_command(command, shouldPrint = None):
    global invoked_count
    invoked_count = invoked_count + 1

    process = subprocess.Popen(shlex.split(command),
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE)
    (_stdout, _stderr) = process.communicate()

    if shouldPrint:
        print "invoked: %s"%(command)
        print "stdout:"
        print _stdout
    return (_stdout, _stderr)


if __name__ == '__main__':
    main()
