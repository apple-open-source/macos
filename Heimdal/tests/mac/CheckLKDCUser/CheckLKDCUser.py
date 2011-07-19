#!/usr/bin/python
# -*- coding: utf-8 -*-
if __name__ == '__main__':
	# The following code allows this test to be invoked outside the harness and should be left unchanged
	import os, sys
	args = [os.path.realpath(os.path.expanduser("/Users/Shared/Raft/raft")), "-f"] + sys.argv
	os.execv(args[0], args)

"""
CheckLKDCUser

Contact: lha@apple.com
2009/11/08
"""

# This is a RAFT test. For more information see http://dzone200/wiki/RAFT
testDescription  = ""                 # Add a brief description of test functionality
testVersion      = "1.0"              # Your test version must be changed if you change testCustomDBSpec or testState
testState        = ProductionState   # Possible values: DevelopmentState, ProductionState
testCustomDBSpec = {}                 # for the exceptional case when custom data fields are needed (see wiki)

def runTest(params):
	# Your testing code here

	assert os.getuid() == 0, "not running test as root"

	import Foundation
	import tempfile

	dict = Foundation.NSDictionary.dictionaryWithContentsOfFile_("/var/db/dslocal/nodes/Default/config/KerberosKDC.plist");

	user = "kerberostest-user"
	password = "foo"
	lkdcrealm = dict.valueForKey_("realname")[0]

	# create tmp file and write password into it
	tmpfile = tempfile.mkstemp(prefix='/tmp/kerberos-tmp-password')
	pwfile = tmpfile[1]
	os.close(tmpfile[0])

	F = open(pwfile, "w")
	F.writelines(password)
	F.close()
	
	os.system("dscl /Local/Default delete /Users/" + user + ">/dev/null 2>&1")

	os.system("dscl /Local/Default create /Users/" + user)
	os.system("dscl /Local/Default passwd /Users/" + user + " " + password)

	ret = os.system("/usr/bin/kinit --password=" + pwfile + " " + user + "@" + lkdcrealm)
	
	os.unlink(pwfile)

	ret2 = os.system("/usr/bin/kdestroy -p "+ user + "@" + lkdcrealm)
	
	os.system("dscl /Local/Default delete /Users/" + user)

	assert ret == 0, "kinit failed"
	assert ret2 == 0, "kdestroy failed"

	
	logPass() # This line is implicit and can be removed
