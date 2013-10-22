# Common Makefile
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

ifdef RC_TARGET_CONFIG
Product=$(RC_TARGET_CONFIG)
else
Product=MacOSX
endif

install:: validate-all install-files

validate-all:
	@echo "Validating files are UTF-8"
	$(_v) for X in * ; do \
		[ \! -f "$$X" ] || iconv -f UTF-8 -t UTF-8 "$$X" >/dev/null ; \
	done

install-files:
	@echo "Installing $(Destination) for $(Product)"
	$(_v) $(INSTALL_FILE) -m 0644 -c com.apple.MessageTracer "$(Destination)/com.apple.MessageTracer"
	$(_v) $(INSTALL_FILE) -m 0644 -c com.apple.performance   "$(Destination)/com.apple.performance"
	$(_v) $(INSTALL_FILE) -m 0644 -c com.apple.eventmonitor  "$(Destination)/com.apple.eventmonitor"
ifeq "$(Product)" "MacOSX"
	$(_v) $(INSTALL_FILE) -m 0644 -c com.apple.install     "$(Destination)/com.apple.install"
	$(_v) $(INSTALL_FILE) -m 0644 -c com.apple.mail        "$(Destination)/com.apple.mail"
	$(_v) $(INSTALL_FILE) -m 0644 -c com.apple.login.guest "$(Destination)/com.apple.login.guest"
endif
