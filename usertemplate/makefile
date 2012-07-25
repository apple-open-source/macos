#
# THESE PERMISSIONS ARE LARGELY IGNORED
# CONTACT B&I IF YOU NEED TO CHANGE FINAL PERMISSIONS
#
# usertemplate makefile
# 
# RC builds must Respect the following target:
#	install:
#	installsrc:
#	installhdrs:
#	clean:
#



include $(MAKEFILEPATH)/pb_makefiles/platform.make 
############################################
# Variables
#

DESTINATION = "$(DSTROOT)$(SYSTEM_LIBRARY_DIR)/User Template"
############################################

install:
	# Create the Destination
	umask 0
	mkdir -p $(DESTINATION)
	chmod -R 700 $(DESTINATION)

	# Install Files
	ditto "$(SRCROOT)" $(DESTINATION)

	# Correct Permissions
	chown -R root:admin $(DESTINATION) # Set the Owner
	chmod -R 755 $(DESTINATION) # Start with 755 
	chmod 700 $(DESTINATION) # We need to set /Sytem/Library/User Template to 700

	chmod 700 $(DESTINATION)"/English.lproj/Library" $(DESTINATION)"/Non_localized/Documents" $(DESTINATION)"/English.lproj/Library/Favorites" $(DESTINATION)"/English.lproj/Movies" $(DESTINATION)"/English.lproj/Music" $(DESTINATION)"/English.lproj/Pictures" $(DESTINATION)"/Non_localized/Library" $(DESTINATION)"/Non_localized/Library/Preferences" $(DESTINATION)"/English.lproj/Desktop" $(DESTINATION)"/Non_localized/Library/PreferencePanes" $(DESTINATION)"/Non_localized/Downloads" $(DESTINATION)"/Non_localized/Library/Logs" $(DESTINATION)"/Non_localized/Library/Caches"  $(DESTINATION)"/Non_localized/Library/Spelling" $(DESTINATION)"/Non_localized/Library/Colors"

	chmod -R 700 $(DESTINATION)"/English.lproj/Library/Preferences" $(DESTINATION)"/English.lproj/Library/Compositions" $(DESTINATION)"/English.lproj/Library/Keyboard Layouts" $(DESTINATION)"/English.lproj/Library/Input Methods" # Set Preferences to 700
	
	chmod 733 $(DESTINATION)"/English.lproj/Public/Drop Box" # Drop Box gets 733

	chmod 600 $(DESTINATION)"/Non_localized/Library/Preferences/com.apple.scheduler.plist" $(DESTINATION)"/Non_localized/Library/Preferences/.GlobalPreferences.plist" $(DESTINATION)"/English.lproj/Library/Preferences/.GlobalPreferences.plist" $(DESTINATION)"/English.lproj/Library/Preferences/com.apple.symbolichotkeys.plist" $(DESTINATION)"/English.lproj/Library/Favorites/.localized"

	chmod 644 $(DESTINATION)"/English.lproj/Library/FontCollections/Fixed Width.collection" $(DESTINATION)"/English.lproj/Library/FontCollections/Traditional.collection" $(DESTINATION)"/English.lproj/Library/FontCollections/Fun.collection" $(DESTINATION)"/English.lproj/Library/FontCollections/Modern.collection" $(DESTINATION)"/English.lproj/Library/FontCollections/PDF.collection" $(DESTINATION)"/English.lproj/Library/FontCollections/Web.collection" $(DESTINATION)"/English.lproj/.CFUserTextEncoding" $(DESTINATION)"/English.lproj/Public/.localized" $(DESTINATION)"/English.lproj/Public/Drop Box/.localized" $(DESTINATION)"/English.lproj/Library/.localized" $(DESTINATION)"/English.lproj/Desktop/.localized" $(DESTINATION)"/English.lproj/Movies/.localized" $(DESTINATION)"/English.lproj/Music/.localized" $(DESTINATION)"/English.lproj/Pictures/.localized" $(DESTINATION)"/English.lproj/Library/Compositions/.localized" $(DESTINATION)"/English.lproj/Library/Input Methods/.localized" $(DESTINATION)"/Non_localized/Documents/.localized" $(DESTINATION)"/Non_localized/Downloads/.localized"
#$(DESTINATION)"/English.lproj/Documents/.localized" $(DESTINATION)"/English.lproj/Downloads/.localized" $(DESTINATION)"/English.lproj/Sites/.localized" 

	#chmod 666 $(DESTINATION)"/English.lproj/Sites/images/gradient.jpg" 
	#--03/10/11 Commented out due to <rdar://problem/9078413> remove Sites folder from default user template

	#chmod 666 $(DESTINATION)"/English.lproj/Sites/index.html" $(DESTINATION)"/Non_localized/Sites/images/gradient.jpg"
	#--03/10/11 Commented out due to <rdar://problem/9078413> remove Sites folder from default user template
	
	
	# Make sure that the About Downloads.pdf are not executable
	chmod 600 $(DESTINATION)"/Non_localized/Downloads/About Downloads.lpdf/Contents/Info.plist" $(DESTINATION)"/Non_localized/Downloads/About Downloads.lpdf/Contents/Resources/English.lproj/About Downloads.pdf" 
	
	# remove the "everyone deny delete ACL: rdar://problem/7907271
	# need to remove the "everyone deny delete ACL from the package: rdar://problem/9389888
	chmod -R -N $(DESTINATION)"/Non_localized/Downloads/About Downloads.lpdf"

	chmod 700 $(DESTINATION) # We need to set /System/Library/User Template to 700
	chown root:admin $(DESTINATION) # Set the Owner
	echo "##################################"
	ls -ald $(DESTINATION)
	echo "##################################"

	# Set the "hide extension" attribute bit of the About Downloads.pdf files
	/usr/bin/SetFile -a "E" $(DESTINATION)"/Non_localized/Downloads/About Downloads.lpdf"

	# Make the ~/Library folder hidden in Finder (<rdar://problem/7889093> ~/Library should be hidden in the Finder)
	/usr/bin/SetFile -a "V" $(DESTINATION)"/Non_localized/Library"

	rm $(DSTROOT)$(SYSTEM_LIBRARY_DIR)/User\ Template/makefile*

installsrc:
	ditto . $(SRCROOT)
	rm -f $(SRCROOT)/CVSVersionInfo.txt
	find $(SRCROOT) -name '.tmpfile' -a -exec rm -f '{}' \;
	find $(SRCROOT) -name '.nfs*' -o -name '.svn' -a -exec echo '{}' \; -a -exec rm -rf '{}' \; -prune
	# chown -R root:wheel $(SRCROOT)

clean::

installhdrs::
