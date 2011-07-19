#!/usr/bin/python
# -*- coding: utf-8 -*-
if __name__ == '__main__':
	# The following code allows this test to be invoked outside the harness and should be left unchanged
	import os, sys
	args = [os.path.realpath(os.path.expanduser("/Users/Shared/Raft/raft")), "-f"] + sys.argv
	os.execv(args[0], args)

"""
KLLAcquireCredUI

Contact: lha@apple.com
2009/11/07
"""

# This is a RAFT test. For more information see http://dzone200/wiki/RAFT
testDescription  = "Pop UI KerberosAgent and acquire credentials"
testVersion      = "1.0"
testState        = ProductionState   # Possible values: DevelopmentState, ProductionState
testCustomDBSpec = {}


def runTest(params):
	# Your testing code here

	os.system("kdestroy -p ktestuser@ADS.APPLE.COM");

	pid = os.fork()
	if pid == 0:
		os.execv("/usr/local/libexec/heimdal/bin/test-kll", ["test-kll", "ktestuser@ADS.APPLE.COM"]);
	
	target.processes()["SecurityAgent"].mainWindow().textFields()[0].click()
	keyboard.typeString_("foobar");
	target.processes()["SecurityAgent"].mainWindow().buttons()["OK"].click()


	res = os.waitpid(pid, 0)[1]
	
	assert res == 0, "test-kll failed"
	
	logPass() # This line is implicit and can be removed
