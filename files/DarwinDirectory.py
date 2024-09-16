#!/usr/bin/python3
import sys
import plistlib
import argparse
import pathlib
import itertools

def validate_int32(val):
    if val.bit_length() > 32:
        raise ValueError("Value '%d' does not fit into 32-bits" % val)
    return val

def macos_deref(x):
	if type(x) is list:
		if len(x) > 1:
			raise TypeError("Attempted to deref list: %s" % (x))
		else:
			return x[0]
	return x
	
def write_user_plist(output_path, name, full_name, uid, gid, home_dir, shell, uuid, target_os, aliases=None):
	pl = dict(
		name = name,
		fullName = full_name,
		ID = uid,
		primaryGroupID = gid,
		homeDirectory = home_dir,
		shell = shell,
		UUID = uuid,
		os = target_os)
	
	if aliases:
		pl["alias"] = aliases
	
	with open(output_path, "wb") as fp:
		plistlib.dump(pl, fp)
		
def write_group_plist(output_path, group_name, gid, member_names, uuid, target_os, aliases=None, nested_groups=None):
	pl = dict(
		name = group_name,
		ID = gid,
		memberNames = member_names,
		UUID = uuid,
		os = target_os)
	
	if aliases:
		pl["alias"] = aliases
	
	if nested_groups:
		pl["nestedGroupUUIDs"] = nested_groups
	
	with open(output_path, "wb") as fp:
		plistlib.dump(pl, fp)

parser = argparse.ArgumentParser(description="Build the plists required for DarwinDirectory", formatter_class=argparse.RawTextHelpFormatter)
parser.add_argument("--users", "-p", type=lambda p: pathlib.Path(p).absolute(), required=True,
    dest="users_path", help="For macOS, this is the path to the directory that contains the user plists.\nFor embedded OSes, this is the path to the passwd file.")
parser.add_argument("--groups", "-g", type=lambda p: pathlib.Path(p).absolute(), required=True,
    dest="groups_path", help="For macOS, this is the path to the directory that contains the group plists.\nFor embedded OSes, this is the path to the group file.")
parser.add_argument("--os", choices=["macos", "embedded"], required=True, dest="target_os", help="target OS type")
parser.add_argument("DEST", type=lambda p: pathlib.Path(p).absolute(),
    help='path to destination directory (a "DarwinDirectory" folder will be created here, if it does not already exist)')

args = parser.parse_args()

if not args.DEST.is_dir():
    raise NotADirectoryError("'DEST' is not a directory: '%s'" % (args.DEST))

DSTROOT = args.DEST / "DarwinDirectory" / "plists"

# Create required directories.
DSTROOT_system = DSTROOT / "system"
DSTROOT_local = DSTROOT / "local"
DSTROOT_system_users = DSTROOT_system / "users"
DSTROOT_system_groups = DSTROOT_system / "groups"
DSTROOT_local_users = DSTROOT_local / "users"
DSTROOT_local_groups = DSTROOT_local / "groups"

DSTROOT_system_users.mkdir(exist_ok=True, parents=True)
DSTROOT_system_groups.mkdir(exist_ok=True)
DSTROOT_local_users.mkdir(exist_ok=True, parents=True)
DSTROOT_local_groups.mkdir(exist_ok=True)

print("Generating DarwinDirectory files for %s" % (args.target_os))

if args.target_os == "embedded":
	username_to_uid_map = dict()

	# Opening both files at the same time to make sure that they are both valid files before doing anything.
	with args.users_path.open() as passwd_file, args.groups_path.open() as groups_file:
		for line in passwd_file:
			stripped_line = line.strip()
			if not stripped_line.startswith("#"):
				elements = stripped_line.split(":")
				if len(elements) == 10:
					name = elements[0]
					uid = validate_int32(int(elements[2]))
					
					if name in username_to_uid_map:
						raise ValueError("User '%s' defined multiple times" % (name))
					
					username_to_uid_map[name] = uid
				
					write_user_plist(
						output_path = DSTROOT_system_users / (name + ".plist"),
						name = name,
						full_name = elements[7], # "gecos"
						uid = uid,
						gid = validate_int32(int(elements[3])),
						home_dir = elements[8],
						shell = elements[9],
						uuid = "FFFFEEEE-DDDD-CCCC-BBBB-AAAA%08X" % (uid if uid >= 0 else (((abs(uid) ^ 0xFFFFFFFF) + 1) & 0xFFFFFFFF)),
						target_os = ["embedded"])
				else:
					raise ValueError("Malformed line in passwd input file: %s" % (stripped_line))

		for line in groups_file:
			stripped_line = line.strip()
			if not stripped_line.startswith("#"):
				elements = stripped_line.split(":")
				if len(elements) == 4:
					group = elements[0]
					gid = validate_int32(int(elements[2]))
					
					members = elements[3].split(",") if elements[3] else []
					
					member_names = []
					
					for username in members:
						# We don't want to include usernames that aren't in the passwd file (they have no UID)
						if username in username_to_uid_map:
							member_names.append(username)
						else:
							print("Warning: Unknown user '%s' in group '%s'" % (username, group), file=sys.stderr)
					
					write_group_plist(
						output_path = DSTROOT_system_groups / (group + ".plist"),
						group_name = group,
						gid = gid,
						member_names = member_names,
						uuid = "ABCDEFAB-CDEF-ABCD-EFAB-CDEF%08X" % (gid if gid >= 0 else (((abs(gid) ^ 0xFFFFFFFF) + 1) & 0xFFFFFFFF)),
						target_os = ["embedded"])
				else:
					raise ValueError("Malformed line in group input file: %s" % (stripped_line))
elif args.target_os == "macos":
	username_to_uid_map = dict()
	useruuid_to_names_map = dict()
	referenced_gids = set()
	allocated_gids = set()

	if not args.users_path.is_dir():
		raise NotADirectoryError("'users' is not a directory (as required for macOS target): '%s'" % (args.users_path))
	if not args.groups_path.is_dir():
		raise NotADirectoryError("'groups' is not a directory (as required for macOS target): '%s'" % (args.groups_path))
	
	for user_plist_path in args.users_path.iterdir():
		with user_plist_path.open("rb") as user_plist_fp:
			user_plist = plistlib.load(user_plist_fp)
			
			uuid = macos_deref(user_plist["generateduid"])
			useruuid_to_names_map[uuid] = set(user_plist["name"])
			
			if type(user_plist["name"]) is list:
				name, *aliases = user_plist["name"]
			else:
				name = macos_deref(user_plist["name"])
				aliases = None
			
			uid = validate_int32(int(macos_deref(user_plist["uid"])))

			if name in username_to_uid_map:
				raise ValueError("User '%s' defined multiple times" % (name))

			if uid in username_to_uid_map.values():
				raise ValueError("UID '%s' used multiple times" % (uid))

			username_to_uid_map[name] = uid
			for alias in aliases:
				if alias in username_to_uid_map:
					raise ValueError("User '%s' defined multiple times" % (alias))
				username_to_uid_map[alias] = uid
			
			gid = validate_int32(int(macos_deref(user_plist["gid"])))
			referenced_gids.add(gid)
			
			write_user_plist(
				output_path = DSTROOT_system_users / (name + ".plist"),
				name = name,
				full_name = macos_deref(user_plist["realname"]),
				uid = uid,
				gid = gid,
				home_dir = macos_deref(user_plist["home"]),
				shell = macos_deref(user_plist["shell"]),
				uuid = uuid,
				target_os = ["macos"],
				aliases = aliases)
	
	for group_plist_path in args.groups_path.iterdir():
		with group_plist_path.open("rb") as group_plist_fp:
			group_plist = plistlib.load(group_plist_fp)
			
			if type(group_plist["name"]) is list:
				group, *aliases = group_plist["name"]
			else:
				group = macos_deref(group_plist["name"])
				aliases = None
			
			member_names = []
			
			if "users" in group_plist:
				if "groupmembers" not in group_plist:
					raise ValueError("'groupmembers' key expected when 'users' present in file '%s'" % (group_plist_path))
				if len(group_plist["users"]) != len(group_plist["groupmembers"]):
					raise ValueError("The lengths of 'users' and 'groupmembers' differ in file '%s'" % (group_plist_path))
				for (username, useruuid) in zip(group_plist["users"], group_plist["groupmembers"]):
					if useruuid not in useruuid_to_names_map:
						raise ValueError("Unknown UUID '%s' found in 'groupmembers' in file '%s'" % (useruuid, group_plist_path))
					if username not in useruuid_to_names_map[useruuid]:
						raise ValueError("Invalid UUID in 'groupmembers' for name '%s' in file '%s'" % (username, group_plist_path))
					# We don't want to include usernames that aren't in the users directory (they have no UID)
					if username in username_to_uid_map:
						member_names.append(username)
					else:
						print("Warning: Unknown user '%s' in group '%s' in file '%s'" % (username, group, group_plist_path), file=sys.stderr)
			
			gid = validate_int32(int(macos_deref(group_plist["gid"])))
			
			if gid in allocated_gids:
				raise ValueError("GID '%s' used multiple times" % (gid))
			
			allocated_gids.add(gid)
			
			write_group_plist(
				output_path = DSTROOT_system_groups / (group + ".plist"),
				group_name = group,
				gid = gid,
				member_names = member_names,
				uuid = macos_deref(group_plist["generateduid"]),
				target_os = ["macos"],
				aliases = aliases,
				nested_groups = group_plist["nestedgroups"] if "nestedgroups" in group_plist else None)
	
	invalid_gids = referenced_gids - allocated_gids
	
	if invalid_gids:
		raise ValueError("The following referenced GIDs are invalid: %s" % (invalid_gids))
