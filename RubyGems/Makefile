##
# Makefile for RubyGems 
##

Project = RubyGems
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make
GnuAfterInstall = post-install
GnuNoBuild = YES

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses
SDKROOTUSR=$(SDKROOT)/usr
unexport GEM_HOME
MKMFFLAGS=--ignore-dependencies \
	--with-xml2-include=$(SDKROOTUSR)/include/libxml2 \
	--with-xml2-lib=$(SDKROOTUSR)/lib \
	--with-xslt-include=$(SDKROOTUSR)/include \
	--with-xslt-lib=$(SDKROOTUSR)/lib

GEMDIR=/System/Library/Frameworks/Ruby.framework/Versions/2.0/usr/lib/ruby/gems/2.0.0
GEMS= gems/*.gem

build::
	$(INSTALL_DIRECTORY) $(OSL) $(OSV)
	for l in $(SRCROOT)/*.txt; do \
		$(INSTALL_FILE) $$l $(OSL); \
	done
	$(INSTALL_FILE) $(SRCROOT)/RubyGems.plist $(OSV)
	for g in $(GEMS); do \
		GEM_HOME=$(DSTROOT)$(GEMDIR) gem install -V --local $$g -- $(MKMFFLAGS) || exit 1; \
	done
	$(FIND) $(DSTROOT) \( -name script -or -name test \) -print | xargs -t rm -rf
	$(FIND) $(DSTROOT) -type f \( -name '*.[ch]' -o -name '*.txt' \) -perm -a+x | xargs -t chmod a-x
	rsync -irptgoD --include='*/' --include='*.bundle' --exclude='*' $(DSTROOT)/ $(SYMROOT)/
	$(FIND) $(SYMROOT) -type f -perm -a+x | xargs -t -n 1 dsymutil
	$(FIND) $(DSTROOT) \( -name .gemtest -or -name .RUBYARCHDIR.time -or -name '*.o' -or -name script -or -name test -or -empty \) -print -delete
	
