#!/usr/bin/python
import sys
import plistlib

users = []

def deref(x):
	if type(x) == list:
		x = x[0]
	return x

def printuser(user):
	name = deref(user["name"])
	uid = deref(user["uid"])
	gid = deref(user["gid"])
	realname = deref(user["realname"])
	home = deref(user["home"])
	shell = deref(user["shell"])
	str = "%s:*:%s:%s::0:0:%s:%s:%s" % \
		(name, uid, gid, realname, home, shell)
	print str

def compare_uid(x, y):
	x = int(deref(x["uid"]))
	y = int(deref(y["uid"]))
	return x-y

for file in sys.argv[1:]:
	users.append(plistlib.readPlist(file))

users.sort(compare_uid)

print("""##
# User Database
# 
# Note that this file is consulted directly only when the system is running
# in single-user mode.  At other times this information is provided by
# Open Directory.
#
# See the opendirectoryd(8) man page for additional information about
# Open Directory.
##""");

for user in users:
	printuser(user)

