# Common Makefile
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

install:: validate-all install-files

validate-all:
	@echo "Validating files are UTF-8"
	$(_v) for X in * ; do \
		[ \! -f "$$X" ] || iconv -f UTF-8 -t UTF-8 "$$X" >/dev/null ; \
	done

install-files:
ifneq "$(CONTENT_PLATFORM)" "ios_sim"
	@echo "Installing $(Destination) for $(CONTENT_PLATFORM)"
	$(_v) $(INSTALL_FILE) -m 0644 -c com.apple.MessageTracer "$(Destination)/com.apple.MessageTracer"
	$(_v) $(INSTALL_FILE) -m 0644 -c com.apple.performance   "$(Destination)/com.apple.performance"
	$(_v) $(INSTALL_FILE) -m 0644 -c com.apple.eventmonitor  "$(Destination)/com.apple.eventmonitor"
ifeq "$(CONTENT_PLATFORM)" "osx"
	$(_v) $(INSTALL_FILE) -m 0644 -c com.apple.install     "$(Destination)/com.apple.install"
	$(_v) $(INSTALL_FILE) -m 0644 -c com.apple.mail        "$(Destination)/com.apple.mail"
	$(_v) $(INSTALL_FILE) -m 0644 -c com.apple.login.guest "$(Destination)/com.apple.login.guest"
endif
endif
