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
GEM_INSTALL = $(GEM) install --install-dir $(DSTBASEGEMSDIR) --local

build::
	$(MKDIR) $(DSTBASEGEMSDIR)
	$(MKDIR) $(DSTROOT)/usr
	(cd gems/others && $(GEM_INSTALL) rake || exit 1) # because needed by rails
	(cd gems/rails1 && $(GEM_INSTALL) rails || exit 1) 
	(cd gems/rails2 && $(GEM_INSTALL) rails || exit 1) 
	(cd gems/others && $(GEM_INSTALL) `ls net-ssh-1.*.gem` || exit 1) # because needed by net-sftp-1.*
	(cd gems/others && for i in `ls *.gem`; do $(GEM_INSTALL) $$i || exit 1; done)
	#(cd gems/others && for i in `ruby -e "puts Dir.glob('*.gem').map { |x| x.sub(/-\d+.\d+.\d+.gem$$/, '') }"`; do $(GEM_INSTALL) $$i || exit 1; done) 
	$(MV) $(DSTBASEGEMSDIR)/bin $(DSTROOT)/usr
	ditto $(DSTROOT) $(SYMROOT)
	strip -x `find $(DSTROOT) -name "*.bundle"`
	rm -rf `find $(DSTROOT) -name "*.dSYM" -type d`
	$(INSTALL_FILE) $(SRCROOT)/favicon.png $(DSTGEMSDIR)/rails-1*/html/favicon.ico
	$(INSTALL_FILE) $(SRCROOT)/favicon.png $(DSTGEMSDIR)/rails-2*/html/favicon.ico
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
	$(INSTALL_FILE) $(DSTGEMSDIR)/rails-1*/MIT-LICENSE $(OSL)/$(Project).txt
	$(INSTALL_FILE) $(DSTGEMSDIR)/rails-2*/MIT-LICENSE $(OSL)/$(Project).txt
	$(INSTALL_FILE) $(DSTGEMSDIR)/rake*/MIT-LICENSE $(OSL)/rake.txt
	$(INSTALL_FILE) $(DSTGEMSDIR)/mongrel*/LICENSE $(OSL)/mongrel.txt
	$(INSTALL_FILE) $(DSTGEMSDIR)/needle*/doc/LICENSE-BSD $(OSL)/needle.txt
	$(INSTALL_FILE) $(DSTGEMSDIR)/ferret*/MIT-LICENSE $(OSL)/ferret.txt
	$(INSTALL_FILE) $(DSTGEMSDIR)/acts_as_ferret*/LICENSE $(OSL)/acts_as_ferret.txt
	$(INSTALL_FILE) $(DSTGEMSDIR)/ruby-yadis*/COPYING $(OSL)/ruby-yadis.txt
	$(INSTALL_FILE) $(DSTGEMSDIR)/ruby-openid*/LICENSE $(OSL)/ruby-openid.txt
	$(INSTALL_FILE) $(DSTGEMSDIR)/hpricot*/COPYING $(OSL)/hpricot.txt
	$(INSTALL_FILE) $(DSTGEMSDIR)/RedCloth*/COPYING $(OSL)/RedCloth.txt
	$(INSTALL_FILE) $(SRCROOT)/extra/ruby-sqlite3.txt $(OSL)/ruby-sqlite3.txt
	$(INSTALL_FILE) $(SRCROOT)/extra/ruby-termios.txt $(OSL)/ruby-termios.txt
	$(INSTALL_FILE) $(SRCROOT)/extra/capistrano.txt $(OSL)/capistrano.txt
	$(INSTALL_FILE) $(SRCROOT)/extra/net-ssh.txt $(OSL)/net-ssh.txt
	$(INSTALL_FILE) $(SRCROOT)/extra/net-sftp.txt $(OSL)/net-sftp.txt
	$(INSTALL_FILE) $(DSTGEMSDIR)/highline*/LICENSE $(OSL)/highline.txt
	$(INSTALL_FILE) $(DSTGEMSDIR)/xmpp4r*/LICENSE $(OSL)/xmpp4r.txt
	echo "" > `ls $(DSTGEMSDIR)/rails-1*/configs/empty.log`
	chown root:wheel $(DSTGEMSDIR)/rails-1*/configs/empty.log
	echo "" > `ls $(DSTGEMSDIR)/rails-2*/configs/empty.log`
	chown root:wheel $(DSTGEMSDIR)/rails-2*/configs/empty.log
	echo "" > `ls $(DSTGEMSDIR)/rails-2*/lib/rails_generator/generators/components/mailer/templates/fixture.rhtml`
	chown root:wheel $(DSTGEMSDIR)/rails-2*/lib/rails_generator/generators/components/mailer/templates/fixture.rhtml
	echo "" > `ls $(DSTGEMSDIR)/rails-2*/lib/rails_generator/generators/components/mailer/templates/view.rhtml`
	chown root:wheel $(DSTGEMSDIR)/rails-2*/lib/rails_generator/generators/components/mailer/templates/view.rhtml
	(cd $(DSTGEMSDIR)/actionmailer-1* && rm -rf CHANGELOG MIT-LICENSE README Rakefile install.rb test)
	(cd $(DSTGEMSDIR)/actionmailer-2* && rm -rf CHANGELOG MIT-LICENSE README Rakefile install.rb test)
	(cd $(DSTGEMSDIR)/actionpack-1* && rm -rf CHANGELOG MIT-LICENSE README RUNNING_UNIT_TESTS Rakefile examples filler.txt install.rb test)
	(cd $(DSTGEMSDIR)/actionpack-2* && rm -rf CHANGELOG MIT-LICENSE README RUNNING_UNIT_TESTS Rakefile examples filler.txt install.rb test)
	(cd $(DSTGEMSDIR)/actionwebservice-1* && rm -rf CHANGELOG MIT-LICENSE README Rakefile TODO examples setup.rb test)
	(cd $(DSTGEMSDIR)/activerecord-1* && rm -rf CHANGELOG README RUNNING_UNIT_TESTS Rakefile examples install.rb test)
	(cd $(DSTGEMSDIR)/activerecord-2* && rm -rf CHANGELOG README RUNNING_UNIT_TESTS Rakefile examples install.rb test)
	(cd $(DSTGEMSDIR)/activesupport-1* && rm -rf CHANGELOG README)
	(cd $(DSTGEMSDIR)/activesupport-2* && rm -rf CHANGELOG README)
	(cd $(DSTGEMSDIR)/activeresource-2* && rm -rf Rakefile README test)
	(cd $(DSTGEMSDIR)/capistrano* && rm -rf CHANGELOG MIT-LICENSE README THANKS examples test capistrano.gemspec CHANGELOG.rdoc)
	(cd $(DSTGEMSDIR)/daemons* && rm -rf README LICENSE Rakefile Releases TODO examples setup.rb test)
	(cd $(DSTGEMSDIR)/ferret* && rm -rf CHANGELOG MIT-LICENSE README Rakefile TODO TUTORIAL ext setup.rb test)
	(cd $(DSTGEMSDIR)/acts_as_ferret* && rm -rf LICENSE README rakefile .init.rb.swp .rakefile.swp lib/.acts_as_ferret.rb.swp lib/.class_methods.rb.swo lib/.class_methods.rb.swp)
	(cd $(DSTGEMSDIR)/gem_plugin* && rm -rf COPYING LICENSE README Rakefile doc test tools CHANGELOG gem_plugin.gemspec Manifest)
	(cd $(DSTGEMSDIR)/cgi_multipart_eof_fix* && rm -rf README Rakefile cgi_multipart_eof_fix_test.rb)
	(cd $(DSTGEMSDIR)/fastthread* && rm -rf ext Rakefile setup.rb test tools CHANGELOG fastthread.gemspec Manifest )
	(cd $(DSTGEMSDIR)/mongrel* && rm -rf COPYING LICENSE README Rakefile doc examples ext setup.rb test tools CHANGELOG  TODO Manifest mongrel.gemspec)
	(cd $(DSTGEMSDIR)/needle* && rm -rf benchmarks doc test)
	(cd $(DSTGEMSDIR)/net-sftp* && rm -rf doc examples test CHANGELOG.rdoc Manifest net-sftp.gemspec Rakefile README.rdoc test setup.rb THANKS.rdoc)
	(cd $(DSTGEMSDIR)/net-ssh* && rm -rf doc examples test CHANGELOG.rdoc Manifest net-ssh.gemspec Rakefile README.rdoc test setup.rb THANKS.rdoc)
	(cd $(DSTGEMSDIR)/net-scp* && rm -rf doc examples test CHANGELOG.rdoc Manifest net-scp.gemspec Rakefile README.rdoc test setup.rb THANKS.rdoc)
	(cd $(DSTGEMSDIR)/rails-1* && rm -rf CHANGELOG MIT-LICENSE Rakefile)
	(cd $(DSTGEMSDIR)/rails-2* && rm -rf CHANGELOG MIT-LICENSE Rakefile)
	(cd $(DSTGEMSDIR)/rake* && rm -rf CHANGES MIT-LICENSE README Rakefile TODO doc install.rb test)
	(cd $(DSTGEMSDIR)/sqlite3-ruby* && rm -rf README doc ext test README.rdoc)
	(cd $(DSTGEMSDIR)/termios* && rm -rf ChangeLog MANIFEST Makefile README TODO.ja examples extconf.rb gem_make.out mkmf.log termios.c termios.rd termios.bundle termios.o test)  
	(cd $(DSTGEMSDIR)/ruby-yadis* && rm -rf COPYING INSTALL README examples test)
	(cd $(DSTGEMSDIR)/ruby-openid* && rm -rf COPYING INSTALL LICENSE README examples test CHANGELOG NOTICE UPGRADE)
	(cd $(DSTGEMSDIR)/hpricot* && rm -rf CHANGELOG COPYING README Rakefile ext extras test)
	(cd $(DSTGEMSDIR)/RedCloth* && rm -rf doc run-tests.rb setup.rb tests test Rakefile README ext CHANGELOG COPYING)
	(cd $(DSTGEMSDIR)/highline* && rm -rf CHANGELOG INSTALL LICENSE README Rakefile TODO examples setup.rb test)
	(cd $(DSTGEMSDIR)/xmpp4r* && rm -rf CHANGELOG COPYING LICENSE Rakefile README.rdoc README_ruby19.txt setup.rb test xmpp4r.gemspec)
