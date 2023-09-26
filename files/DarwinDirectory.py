#!/usr/bin/python3
import sys
import plistlib
import argparse
import pathlib

def validate_int32(val):
    if val.bit_length() > 32:
        raise ValueError("Value '%d' does not fit into 32-bits" % val)
    return val

parser = argparse.ArgumentParser(description="Build the plists required for DarwinDirectory")
parser.add_argument("--passwd", "-p", type=lambda p: pathlib.Path(p).absolute(), required=True,
    dest="passwd_path", help="path to passwd file")
parser.add_argument("--groups", "-g", type=lambda p: pathlib.Path(p).absolute(), required=True,
    dest="groups_path", help="path to group file")
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

username_to_uid_map = dict()

# Opening both files at the same time to make sure that they are both valid files before doing anything.
with args.passwd_path.open() as passwd_file, args.groups_path.open() as groups_file:
    for line in passwd_file:
        stripped_line = line.strip()
        if not stripped_line.startswith("#"):
            elements = stripped_line.split(":")
            if len(elements) == 10:
                name = elements[0]
                uid = validate_int32(int(elements[2]))
                gid = validate_int32(int(elements[3]))
                gecos = elements[7]
                home_dir = elements[8]
                shell = elements[9]

                username_to_uid_map[name] = uid

                pl = dict(
                    name = name,
                    fullName = gecos,
                    ID = uid,
                    primaryGroupID = gid,
                    homeDirectory = home_dir,
                    shell = shell,
                    UUID = "FFFFEEEE-DDDD-CCCC-BBBB-AAAA%08X" % (uid if uid >= 0 else (((abs(uid) ^ 0xFFFFFFFF) + 1) & 0xFFFFFFFF)),
                    os = ["embedded"]
                )
                
                output_file = DSTROOT_system_users / (name + ".plist")
                with open(output_file, "wb") as fp:
                    plistlib.dump(pl, fp)
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
                        print("Warning: Unknown user '%s'" % (username), file=sys.stderr)
                
                pl = dict(
                    name = group,
                    ID = gid,
                    memberNames = member_names,
                    UUID = "ABCDEFAB-CDEF-ABCD-EFAB-CDEF%08X" % (gid if gid >= 0 else (((abs(gid) ^ 0xFFFFFFFF) + 1) & 0xFFFFFFFF)),
                    os = ["embedded"]
                )
                
                output_file = DSTROOT_system_groups / (group + ".plist")
                with open(output_file, "wb") as fp:
                    plistlib.dump(pl, fp)
            else:
                raise ValueError("Malformed line in group input file: %s" % (stripped_line))

