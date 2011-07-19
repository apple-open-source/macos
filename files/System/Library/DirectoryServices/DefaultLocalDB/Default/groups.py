#!/usr/bin/python
import sys
import plistlib

groups = []

def deref(x):
	if type(x) == list:
		x = x[0]
	return x

def printgroup(group):
	name = deref(group["name"])
	gid = deref(group["gid"])
	users = ""
	if "users" in group:
		users = ",".join(group["users"])
	str = "%s:*:%s:%s" % (name, gid, users)
	print str

def compare_gid(x, y):
	x = int(deref(x["gid"]))
	y = int(deref(y["gid"]))
	return x-y

for file in sys.argv[1:]:
	groups.append(plistlib.readPlist(file))

groups.sort(compare_gid)

print("""##
# Group Database
# 
# Note that this file is consulted directly only when the system is running
# in single-user mode.  At other times this information is provided by
# Open Directory.
#
# See the opendirectoryd(8) man page for additional information about
# Open Directory.
##""");

for group in groups:
	printgroup(group)

