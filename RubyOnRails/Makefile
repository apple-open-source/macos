##
# Makefile for RubyOnRails
##

Project = RubyOnRails

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

DSTRUBYDIR = $(DSTROOT)/System/Library/Frameworks/Ruby.framework/Versions/1.8/usr/lib/ruby
DSTBASEGEMSDIR = $(DSTRUBYDIR)/gems/1.8
DSTGEMSDIR = $(DSTBASEGEMSDIR)/gems

GEM = /usr/bin/gem
GEM_INSTALL = $(GEM) install --install-dir $(DSTBASEGEMSDIR) --local --include-dependencies --rdoc

GEMS =  rake \
	activesupport \
	actionpack \
	activerecord \
	actionmailer \
	actionwebservice \
	rails \
	gem_plugin \
	daemons \
	fastthread \
	cgi_multipart_eof_fix \
	mongrel \
	sqlite3-ruby \
	needle \
	net-ssh \
	net-sftp \
	highline \
	capistrano \
	termios \
	ferret \
	acts_as_ferret \
	ruby-yadis \
	ruby-openid \
	hpricot	\
	RedCloth

build::
	$(MKDIR) $(DSTBASEGEMSDIR)
	$(MKDIR) $(DSTROOT)/usr
	(cd gems && for i in $(GEMS); do $(GEM_INSTALL) $$i || exit 1; done) 
	$(MV) $(DSTBASEGEMSDIR)/bin $(DSTROOT)/usr
	ditto $(DSTROOT) $(SYMROOT)
	strip -x `find $(DSTROOT) -name "*.bundle"`
	rm -rf `find $(DSTROOT) -name "*.dSYM" -type d`
	$(INSTALL_FILE) $(SRCROOT)/favicon.png $(DSTGEMSDIR)/rails*/html/favicon.ico
	$(MKDIR) $(DSTROOT)/$(MANDIR)/man1
	$(INSTALL_FILE) $(SRCROOT)/man/rake.1 $(DSTROOT)/$(MANDIR)/man1/rake.1
	$(INSTALL_FILE) $(SRCROOT)/man/rails.1 $(DSTROOT)/$(MANDIR)/man1/rails.1
	$(INSTALL_FILE) $(SRCROOT)/man/mongrel_rails.1 $(DSTROOT)/$(MANDIR)/man1/mongrel_rails.1
	$(LN) $(DSTROOT)$(MANDIR)/man1/mongrel_rails.1 $(DSTROOT)$(MANDIR)/man1/gpgen.1
	$(INSTALL_FILE) $(SRCROOT)/man/cap.1 $(DSTROOT)/$(MANDIR)/man1/cap.1
	$(INSTALL_FILE) $(SRCROOT)/man/cap.1 $(DSTROOT)/$(MANDIR)/man1/capify.1
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(DSTGEMSDIR)/rails*/MIT-LICENSE $(OSL)/$(Project).txt
	$(INSTALL_FILE) $(DSTGEMSDIR)/rake*/MIT-LICENSE $(OSL)/rake.txt
	$(INSTALL_FILE) $(DSTGEMSDIR)/mongrel*/LICENSE $(OSL)/mongrel.txt
	$(INSTALL_FILE) $(DSTGEMSDIR)/capistrano*/MIT-LICENSE $(OSL)/capistrano.txt
	$(INSTALL_FILE) $(DSTGEMSDIR)/needle*/doc/LICENSE-BSD $(OSL)/needle.txt
	$(INSTALL_FILE) $(DSTGEMSDIR)/net-ssh*/doc/LICENSE-BSD $(OSL)/ruby-net-ssh.txt
	$(INSTALL_FILE) $(DSTGEMSDIR)/net-sftp*/doc/LICENSE-BSD $(OSL)/ruby-net-sftp.txt
	$(INSTALL_FILE) $(DSTGEMSDIR)/ferret*/MIT-LICENSE $(OSL)/ferret.txt
	$(INSTALL_FILE) $(DSTGEMSDIR)/acts_as_ferret*/LICENSE $(OSL)/acts_as_ferret.txt
	$(INSTALL_FILE) $(DSTGEMSDIR)/ruby-yadis*/COPYING $(OSL)/ruby-yadis.txt
	$(INSTALL_FILE) $(DSTGEMSDIR)/ruby-openid*/COPYING $(OSL)/ruby-openid.txt
	$(INSTALL_FILE) $(DSTGEMSDIR)/hpricot*/COPYING $(OSL)/hpricot.txt
	$(INSTALL_FILE) $(DSTGEMSDIR)/RedCloth*/doc/COPYING $(OSL)/RedCloth.txt
	$(INSTALL_FILE) $(SRCROOT)/extra/ruby-sqlite3.txt $(OSL)/ruby-sqlite3.txt
	$(INSTALL_FILE) $(SRCROOT)/extra/ruby-termios.txt $(OSL)/ruby-termios.txt
	$(INSTALL_FILE) $(DSTGEMSDIR)/highline*/LICENSE $(OSL)/highline.txt
	echo "" > `ls $(DSTGEMSDIR)/rails*/configs/empty.log`
	chown root:wheel $(DSTGEMSDIR)/rails*/configs/empty.log
	(cd $(DSTGEMSDIR)/rails* && patch -p0 < $(SRCROOT)/patches/default_to_sqlite3.diff) || exit 1
	(cd $(DSTGEMSDIR)/activerecord* && patch -p0 < $(SRCROOT)/patches/close_sqlite_connections_on_disconnect.diff) || exit 1
	(cd $(DSTGEMSDIR)/actionmailer* && rm -rf CHANGELOG MIT-LICENSE README Rakefile install.rb test)
	(cd $(DSTGEMSDIR)/actionpack* && rm -rf CHANGELOG MIT-LICENSE README RUNNING_UNIT_TESTS Rakefile examples filler.txt install.rb test)
	(cd $(DSTGEMSDIR)/actionwebservice* && rm -rf CHANGELOG MIT-LICENSE README Rakefile TODO examples setup.rb test)
	(cd $(DSTGEMSDIR)/activerecord* && rm -rf CHANGELOG README RUNNING_UNIT_TESTS Rakefile examples install.rb test)
	(cd $(DSTGEMSDIR)/activesupport* && rm -rf CHANGELOG README)
	(cd $(DSTGEMSDIR)/capistrano* && rm -rf CHANGELOG MIT-LICENSE README THANKS examples test)
	(cd $(DSTGEMSDIR)/daemons* && rm -rf README LICENSE Rakefile Releases TODO examples setup.rb test)
	(cd $(DSTGEMSDIR)/ferret* && rm -rf CHANGELOG MIT-LICENSE README Rakefile TODO TUTORIAL ext setup.rb test)
	(cd $(DSTGEMSDIR)/acts_as_ferret* && rm -rf LICENSE README rakefile .init.rb.swp .rakefile.swp lib/.acts_as_ferret.rb.swp lib/.class_methods.rb.swo lib/.class_methods.rb.swp)
	(cd $(DSTGEMSDIR)/gem_plugin* && rm -rf COPYING LICENSE README Rakefile doc test tools)
	(cd $(DSTGEMSDIR)/cgi_multipart_eof_fix* && rm -rf README Rakefile cgi_multipart_eof_fix_test.rb)
	(cd $(DSTGEMSDIR)/fastthread* && rm -rf ext Rakefile setup.rb test tools)
	(cd $(DSTGEMSDIR)/mongrel* && rm -rf COPYING LICENSE README Rakefile doc examples ext setup.rb test tools)
	(cd $(DSTGEMSDIR)/needle* && rm -rf benchmarks doc test)
	(cd $(DSTGEMSDIR)/net-sftp* && rm -rf doc examples test)
	(cd $(DSTGEMSDIR)/net-ssh* && rm -rf doc examples test)
	(cd $(DSTGEMSDIR)/rails* && rm -rf CHANGELOG MIT-LICENSE Rakefile)
	(cd $(DSTGEMSDIR)/rake* && rm -rf CHANGES MIT-LICENSE README Rakefile TODO doc install.rb test)
	(cd $(DSTGEMSDIR)/sqlite3-ruby* && rm -rf README doc ext test)
	(cd $(DSTGEMSDIR)/termios* && rm -rf ChangeLog MANIFEST Makefile README TODO.ja examples extconf.rb gem_make.out mkmf.log termios.c termios.rd termios.bundle termios.o test)  
	(cd $(DSTGEMSDIR)/ruby-yadis* && rm -rf COPYING INSTALL README examples test)
	(cd $(DSTGEMSDIR)/ruby-openid* && rm -rf COPYING INSTALL LICENSE README examples test)
	(cd $(DSTGEMSDIR)/hpricot* && rm -rf CHANGELOG COPYING README Rakefile ext extras test)
	(cd $(DSTGEMSDIR)/RedCloth* && rm -rf doc run-tests.rb setup.rb tests)
	(cd $(DSTGEMSDIR)/highline* && rm -rf CHANGELOG INSTALL LICENSE README Rakefile TODO examples setup.rb test)
