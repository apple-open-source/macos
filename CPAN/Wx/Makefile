NAME = Wx
VERSION = 0.74

NO64 = YES

include ../Makefile.inc

custompatching:
	chown -R root:wheel $(DIR)
	find $(DIR) -name \*.h -o -name \*.txt | xargs chmod 644
	ed - $(DIR)/build/Wx/build/MakeMaker/MacOSX_GCC.pm < $(SRCROOT)/$(NAME)/MacOSX_GCC.pm.ed
	ed - $(DIR)/cpp/wxPerl.osx/Info.plist < $(SRCROOT)/$(NAME)/Info.plist.ed

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

postinstall:
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
