NAME = Wx
VERSION = 0.89

NO64 = YES

include ../Makefile.inc

custompatching:
	chown -R root:wheel $(DIR)
	find $(DIR) -name \*.h -o -name \*.txt | xargs chmod 644
	ed - $(DIR)/build/Wx/build/MakeMaker/MacOSX_GCC.pm < $(MYSRC)/MacOSX_GCC.pm.ed
	ed - $(DIR)/cpp/wxPerl.osx/Info.plist < $(MYSRC)/Info.plist.ed
	chmod a-x $(DIR)/cpp/wxPerl.osx/Info.plist

WXWIDGETS = wxWidgets
WXPERLPROJECT = wxPerl

DOC = $(DSTROOT)/Developer/Documentation
DOCPERL = $(DOC)/Perl
DOCPERLWXPERL = $(DOCPERL)/$(WXPERLPROJECT)
DOCWXWIDGETS = $(DOC)/$(WXWIDGETS)
DOCWXWIDGETSWXPERL = $(DOCWXWIDGETS)/$(WXPERLPROJECT)
EXAMPLES = $(DSTROOT)/Developer/Examples
EXAMPLESPERL = $(EXAMPLES)/Perl
EXAMPLESPERLWXPERL = $(EXAMPLESPERL)/$(WXPERLPROJECT)
EXAMPLESWXWIDGETS = $(EXAMPLES)/$(WXWIDGETS)
EXAMPLESWXWIDGETSWXPERL = $(EXAMPLESWXWIDGETS)/$(WXPERLPROJECT)

postinstall: fixupwxPerl installdocs

# generate -arch <32-bit arch> list
PERL32ARGS = lipo -info /usr/bin/perl$(VERSIONER_PERL_VERSION) | sed -e 's/^.*://' -e 's/ $$//' -e 's/ [0-9a-z_]*64//g' -e 's/ppc[0-9][0-9]*/ppc/' -e 's/^ //' -e 's/[0-9a-z][0-9a-z]*/-arch &/g'

fixupwxPerl:
	ditto `$(PERL32ARGS)` /usr/bin/perl$(VERSIONER_PERL_VERSION) $(DSTROOT)/System/Library/Perl/Extras/$(VERSIONER_PERL_VERSION)/darwin-thread-multi-2level/auto/Wx/wxPerl.app/Contents/MacOS/wxPerl

installdocs:
	install -d -g admin -m 0775 $(EXAMPLESWXWIDGETSWXPERL)
	rsync -rlt $(DIR)/samples $(EXAMPLESWXWIDGETSWXPERL)
	-chown -R root:admin $(EXAMPLESWXWIDGETSWXPERL)
	-chmod -R g+w $(EXAMPLESWXWIDGETSWXPERL)
	install -d -g admin -m 0775 $(EXAMPLESPERL)
	ln -s ../$(WXWIDGETS)/$(WXPERLPROJECT) $(EXAMPLESPERLWXPERL)
	install -d -g admin -m 0775 $(DOCWXWIDGETSWXPERL)
	rsync -rlt $(DIR)/docs/ $(DOCWXWIDGETSWXPERL)
	-chown -R root:admin $(DOCWXWIDGETSWXPERL)
	-chmod -R g+w $(DOCWXWIDGETSWXPERL)
	install -d -g admin -m 0775 $(DOCPERL)
	ln -s ../$(WXWIDGETS)/$(WXPERLPROJECT) $(DOCPERLWXPERL)
