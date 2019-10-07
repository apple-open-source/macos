#
# usertemplate makefile
#
# RC builds must Respect the following target:
#	install:
#	installsrc:
#	installhdrs:
#	clean:
#
# Ensure permissions are correctly set in this Makefile
# B&I override is no longer applicable per 21475691
#


include $(MAKEFILEPATH)/pb_makefiles/platform.make
############################################
# Variables
#

DESTINATION = "$(DSTROOT)/Library/User Template"
############################################

install:
	# Create the Destination
	umask 0
	mkdir -p $(DESTINATION)
	# chmod -R 700 $(DESTINATION)

	# Install Files
	ditto "$(SRCROOT)"/English.lproj $(DESTINATION)/English.lproj
	ditto "$(SRCROOT)"/Non_localized $(DESTINATION)/Non_localized
	ditto "$(SRCROOT)"/__permissions.plist $(DESTINATION)/__permissions.plist

	# Delete all . DS_Store
	find $(DESTINATION) -name ".DS_Store" -delete

	# Remove 'com.apple.root.installed'
	find $(DESTINATION) -type f -exec xattr -d com.apple.root.installed {} \;

	# Set the Owner/Group to root:admin
	chown -R root:admin $(DESTINATION)

	# All directories needs base permission of 700
	# find $(DESTINATION) -type d -exec chmod 700 {} \;
	
	# Directories that need 755
	# chmod 755 $(DESTINATION)"/Non_localized"
	# chmod 755 $(DESTINATION)"/English.lproj"
	# chmod 755 $(DESTINATION)"/English.lproj/Public"

	# Directories that need 733
	# chmod 733 $(DESTINATION)"/English.lproj/Public/Drop Box"

	# All files needs base permission of 600
	# find $(DESTINATION) -type f -exec chmod 600 {} \;
	# Files that need 644
	# chmod 644 $(DESTINATION)"/English.lproj/Pictures/.localized"
	# chmod 644 $(DESTINATION)"/English.lproj/Library/.localized"
	# chmod 644 $(DESTINATION)"/English.lproj/Music/.localized"
	# chmod 644 $(DESTINATION)"/English.lproj/Desktop/.localized"
	# chmod 644 $(DESTINATION)"/English.lproj/Public/.localized"
	# chmod 644 $(DESTINATION)"/English.lproj/Public/Drop Box/.localized"
	# chmod 644 $(DESTINATION)"/English.lproj/Movies/.localized"

	# Output final result
	find $(DESTINATION) -type d -exec ls -ald {} \;
	find $(DESTINATION) -type f -exec ls -ald {} \;

	# <rdar://problem/51386922> [macOSJazz] "User Template" moved from /S/L to /L: symlink?
	mkdir -p "$(DSTROOT)/System/Library"
	ln -s "../../Library/User Template" "$(DSTROOT)/System/Library/User Template"

installsrc:
	ditto . $(SRCROOT)
	rm -f $(SRCROOT)/CVSVersionInfo.txt
	find $(SRCROOT) -name '.tmpfile' -a -exec rm -f '{}' \;
	find $(SRCROOT) -name '.nfs*' -o -name '.svn' -a -exec echo '{}' \; -a -exec rm -rf '{}' \; -prune
	# chown -R root:wheel $(SRCROOT)

clean::

installhdrs::
