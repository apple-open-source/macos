# usertemplate makefile
# 
# RC builds must Respect the following target:
#	install:
#	installsrc:
#	installhdrs:
#	clean:
#

include $(MAKEFILEPATH)/Carbon/make.defaults
############################################
# Variables
#

DESTINATION = 	 $(DSTROOT)$(SYSTEM_LIBRARY_DIR)/UserTemplate/
NONLOCALIZED = $(DSTROOT)$(SYSTEM_LIBRARY_DIR)/UserTemplate/
############################################

install:
	# Create the Destination
	umask 0
	mkdir -p "$(DESTINATION)"
	chmod -R 755 "$(DESTINATION)"

	# Install Files
	ditto "$(SRCROOT)" "$(DESTINATION)"

	# Correct Permissions
	chown -R root:wheel "$(DESTINATION)" # Set the Owner
	chmod -R 755 "$(DESTINATION)" # Start with 755

	chmod 700 "$(DESTINATION)/English.lproj/Library" "$(DESTINATION)/English.lproj/Documents" "$(DESTINATION)/English.lproj/Library/Favorites" "$(DESTINATION)/English.lproj/Movies" "$(DESTINATION)/English.lproj/Music" "$(DESTINATION)/English.lproj/Pictures" "$(DESTINATION)/Non_localized/Library" "$(DESTINATION)/Non_localized/Library/Preferences" "$(DESTINATION)/English.lproj/Desktop"
	chmod -R 700 "$(DESTINATION)/English.lproj/Library/Preferences" # Set Preferences to 700
	chmod 733 "$(DESTINATION)/English.lproj/Public/Drop Box" # Drop Box gets 733

	chmod 600 "$(DESTINATION)/Non_localized/Library/Preferences/LSApplications" "$(DESTINATION)/Non_localized/Library/Preferences/LSClaimedTypes" "$(DESTINATION)/Non_localized/Library/Preferences/LSSchemes" "$(DESTINATION)/English.lproj/Library/Preferences/com.apple.finder.plist" "$(DESTINATION)/English.lproj/Library/Preferences/com.apple.scheduler.plist" "$(DESTINATION)/English.lproj/Library/Preferences/.GlobalPreferences.plist"

	chmod 644 "$(DESTINATION)/English.lproj/Library/Preferences/Explorer/Favorites.html" "$(DESTINATION)/English.lproj/Library/FontCollections/Classic.fcache" "$(DESTINATION)/English.lproj/Library/FontCollections/Fun.fcache" "$(DESTINATION)/English.lproj/Library/FontCollections/Modern.fcache" "$(DESTINATION)/English.lproj/Library/FontCollections/PDF.fcache" "$(DESTINATION)/English.lproj/Library/FontCollections/Web.fcache" "$(DESTINATION)/English.lproj/.CFUserTextEncoding"

	chmod 666 "$(DESTINATION)/English.lproj/Sites/index.html" "$(DESTINATION)/English.lproj/Sites/images/apache_pb.gif" "$(DESTINATION)/English.lproj/Sites/images/macosxlogo.gif" "$(DESTINATION)/English.lproj/Sites/images/web_share.gif"

	# Set Symbolic Links
	ln -s ../../Documents "$(DESTINATION)English.lproj/Library/Favorites/Documents"
	ln -s "/Desktop Folder" "$(DESTINATION)English.lproj/Desktop/Desktop (Mac OS 9)"
      #	ln -s ../.. "$(DESTINATION)English.lproj/Library/Favorites/Home"

	rm $(DSTROOT)/System/Library/UserTemplate/makefile*

installsrc:
	ditto . $(SRCROOT)
	rm -f $(SRCROOT)/CVSVersionInfo.txt
	find $(SRCROOT) -name '.tmpfile' -a -exec rm -f '{}' \;
	find $(SRCROOT) -name '.nfs*' -o -name 'CVS' -a -exec echo '{}' \; -a -exec rm -rf '{}' \; -prune
	chown -R root:wheel $(SRCROOT)

clean::

installhdrs::
