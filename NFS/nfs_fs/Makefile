#
# Makefile for nfs_fs
#

Project = nfs_fs

inplace:
	@$(MAKE) all SRCROOT=. OBJROOT=. SYMROOT=. DSTROOT=/

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

#
# Files/Directories that get installed:
#
# /System/Library/Filesystems/nfs.fs
# /System/Library/Filesystems/nfs.fs/Contents
# /System/Library/Filesystems/nfs.fs/Contents/Info.plist
# /System/Library/Filesystems/nfs.fs/Contents/Resources
# /System/Library/Filesystems/nfs.fs/Contents/Resources/English.lproj
# /System/Library/Filesystems/nfs.fs/Contents/Resources/English.lproj/InfoPlist.strings
# /System/Library/Filesystems/nfs.fs/Contents/version.plist

DST_DIR = $(DSTROOT)/System/Library/Filesystems/nfs.fs

#
# Standard B&I targets
#
all:

install:: all
	install -d -o root -g wheel -m 755 $(DST_DIR)
	install -d -o root -g wheel -m 755 $(DST_DIR)/Contents
	install -c -o root -g wheel -m 444 $(SRCROOT)/Info.plist $(DST_DIR)/Contents
	install -d -o root -g wheel -m 755 $(DST_DIR)/Contents/Resources
	install -d -o root -g wheel -m 755 $(DST_DIR)/Contents/Resources/English.lproj
	install -c -o root -g wheel -m 444 $(SRCROOT)/English.lproj/InfoPlist.strings $(DST_DIR)/Contents/Resources/English.lproj

#
# Build
#
# Nothing to do here... let's move along...
#

