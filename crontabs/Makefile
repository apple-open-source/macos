# xbs-compatible Makefile for the crontabs project.

# Project info
Project = crontabs

# Common Makefile
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

# Directories.
DIRS := private/etc/defaults \
	private/etc/periodic/daily \
	private/etc/periodic/weekly \
	private/etc/periodic/monthly \
	usr/share/man/man5 \
	usr/share/man/man8 \
	usr/sbin

# Data files.
FILES := private/etc/crontab \
	private/etc/defaults/periodic.conf \
	usr/share/man/man5/periodic.conf.5 \
	usr/share/man/man8/periodic.8

# Executables.
BINS := private/etc/periodic/daily/500.daily \
	private/etc/periodic/daily/100.clean-logs \
	private/etc/periodic/weekly/500.weekly \
	private/etc/periodic/monthly/500.monthly \
	usr/sbin/periodic

# Compatibility symlinks, hopefully to be removed sometime in the future.
BLINKS := periodic/daily/500.daily private/etc/daily \
	periodic/weekly/500.weekly private/etc/weekly \
	periodic/monthly/500.monthly private/etc/monthly

install::
	@echo "Installing $(Project)"
	@$(SHELL) -c \
	'for i in $(DIRS) ; do \
		echo $(INSTALL_DIRECTORY) $(DSTROOT)/$$i; \
		$(INSTALL_DIRECTORY) $(DSTROOT)/$$i; \
	done; \
	for i in $(FILES) ; do \
		echo $(INSTALL_FILE) $$i $(DSTROOT)/`dirname $$i`; \
		$(INSTALL_FILE) $$i $(DSTROOT)/`dirname $$i`; \
	done; \
	for i in $(BINS) ; do \
		echo $(INSTALL_SCRIPT) $$i $(DSTROOT)/`dirname $$i`; \
		$(INSTALL_SCRIPT) $$i $(DSTROOT)/`dirname $$i`; \
	done; \
	f=; \
	for i in $(BLINKS); do \
		if test -z $$f ; then \
			f=$$i; \
		else \
			echo "$(LN) -fs $$f $(DSTROOT)/$$i"; \
			$(LN) -fs $$f $(DSTROOT)/$$i; \
			f=; \
		fi; \
	done; \
	'
