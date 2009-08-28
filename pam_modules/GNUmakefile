##
# Modules Makefile
##
# Luke Howard <lukeh@padl.com>
##

# Project info
Project               = pam_modules
SubProjects           = pam_uwtmp pam_serialnumber pam_launchd pam_sacl pam_nologin pam_rootok pam_env pam_group pam_krb5 pam_opendirectory pam_mount pam_self

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

lazy_install_source:: shadow_source recurse

install:: lazy_install_source
	make recurse TARGET=install Project=$(Project) Extra_CC_Flags=-Wall -Werror
	# Copy the symbol files
	$(CP) $(OBJROOT)/*/*.so.? $(SYMROOT)/
	for x in $(SYMROOT)/*; do dsymutil $$x; done
	# Strip the product
	for a in `ls -1 $(DSTROOT)/usr/lib/pam/` ; do \
		strip -S -x $(DSTROOT)/usr/lib/pam/$${a}; \
	done;
	# Install the manpages
	for a in $(SubProjects); do \
		if test -e  $(SRCROOT)/$${a}/$${a}.8; then \
			mkdir -p $(DSTROOT)/usr/share/man/man8/; \
			cp $(SRCROOT)/$${a}/$${a}.8 $(DSTROOT)/usr/share/man/man8/$${a}.8; \
		fi; \
	done;
