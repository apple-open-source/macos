##
# Makefile for ruby

Project               = ruby
# Ruby will not compile with gcc3 if -O>2
CC_Optimize            = -Os
Extra_CC_Flags         = -no-cpp-precomp
GnuAfterInstall        = post-install

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make
Install_Flags   = DESTDIR="$(DSTROOT)"
Install_Target  = install

post-install:
	strip -x $(DSTROOT)/usr/bin/ruby
	rm $(DSTROOT)/usr/lib/ruby/1.6/powerpc-darwin6.0/libruby.a
	strip -x $(DSTROOT)/usr/lib/ruby/1.6/powerpc-darwin6.0/*.bundle
	strip -x $(DSTROOT)/usr/lib/ruby/1.6/powerpc-darwin6.0/digest/*.bundle
