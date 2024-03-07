
ifndef CoreOSMakefiles
CoreOSMakefiles = $(MAKEFILEPATH)/CoreOS
endif

include $(CoreOSMakefiles)/Standard/Standard.make

PLISTDIR = /AppleInternal/CoreOS/BATS/unit_tests
TESTSDIR = /AppleInternal/Tests/man

install:
	mkdir -p $(DESTDIR)$(TESTSDIR) $(DESTDIR)$(PLISTDIR)

	$(INSTALL_SCRIPT) test_man.sh $(DESTDIR)$(TESTSDIR)

	$(INSTALL_FILE) man.plist $(DESTDIR)$(PLISTDIR)
