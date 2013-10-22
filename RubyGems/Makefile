##
# Makefile for RubyGems 
##

Project = RubyGems
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make
GnuAfterInstall = post-install
GnuNoBuild = YES

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

unexport GEM_HOME

GEMDIR=/System/Library/Frameworks/Ruby.framework/Versions/2.0/usr/lib/ruby/gems/2.0.0
GEMS= gems/*.gem

build::
	$(MKDIR) $(OSL) $(OSV)
	for l in $(SRCROOT)/*.txt; do \
		$(CP) $$l $(OSL); \
	done
	cp $(SRCROOT)/RubyGems.plist $(OSV)
	for g in $(GEMS); do \
		GEM_HOME=$(DSTROOT)$(GEMDIR) gem install --local $$g || exit 1; \
	done
	$(FIND) $(DSTROOT) \( -name .gemtest -or -name .RUBYARCHDIR.time \) -empty -delete
	rsync -irptgoD --include='*/' --include='*.bundle' --exclude='*' $(DSTROOT)/ $(SYMROOT)/
	$(FIND) $(SYMROOT) -type f -perm -a+x | xargs -t -n 1 dsymutil
	$(FIND) $(SYMROOT) -empty -delete
	$(FIND) $(DSTROOT) -name '*.o' -delete
	$(FIND) $(DSTROOT) \( -name script -or -name test \) -print | xargs -t rm -rf
