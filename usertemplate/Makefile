# usertemplate makefile
#
# Top level makefile for Build & Integration.
#
#    RC builds must respect the following target:
#         install:
#         installsrc:
#         installhdrs:
#         clean:
#
#    See "~rc/Procedures/Makefile_API.rtf" for all the info.

OS = MACOS
include /Developer/Makefiles/pb_makefiles/platform.make

DESTINATION = $(DSTROOT)$(SYSTEM_LIBRARY_DIR)/PrivateFrameworks/Admin.framework/Resources/English.lproj/user.template

install:
	# first lay down the destination and secure it. This verifies cleanly.
	umask 0
	mkdir -p $(DESTINATION)
	chmod -R u=rwX,go=rX $(DSTROOT)
	
	# second actually install our stuff.
	ditto $(SRCROOT)/user.template $(DESTINATION)
		
	# third correct permissions
	chown -R root:wheel $(DESTINATION)						# set the owner
	chmod -R u=rwX,go=rX $(DESTINATION)						# start with 755,644 for everything
	chmod u=rwX,go= $(DESTINATION)/Library $(DESTINATION)/Documents $(DESTINATION)/Library/Favorites  
	chmod -R u=rwX,go= $(DESTINATION)/Library/Preferences				# 700,600 for prefs
	chmod u=rwX,go=w "$(DESTINATION)/Public/Drop Box"				# The drop box gets 722

installsrc:
	ditto . $(SRCROOT) 
	find $(SRCROOT) -name '.nfs*' -o -name 'CVS' -a -exec echo '{}' \; -a -exec rm -rf '{}' \; -prune
	chown -R root:wheel $(SRCROOT)

clean::

installhdrs::


