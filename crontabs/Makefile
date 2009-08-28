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
	System/Library/LaunchDaemons \
	usr/share/man/man5

# Data files.
FILES := System/Library/LaunchDaemons/com.apple.newsyslog.plist \
	System/Library/LaunchDaemons/com.apple.periodic-daily.plist \
	System/Library/LaunchDaemons/com.apple.periodic-weekly.plist \
	System/Library/LaunchDaemons/com.apple.periodic-monthly.plist \
	System/Library/LaunchDaemons/com.apple.var-db-dslocal-backup.plist \
	System/Library/LaunchDaemons/com.apple.var-db-shadow-backup.plist \
	private/etc/defaults/periodic.conf \
	private/etc/newsyslog.conf \
	usr/share/man/man5/periodic.conf.5

# Executables.
BINS := private/etc/periodic/daily/100.clean-logs \
	private/etc/periodic/daily/110.clean-tmps \
	private/etc/periodic/daily/130.clean-msgs \
	private/etc/periodic/daily/140.clean-rwho \
	private/etc/periodic/daily/199.clean-fax \
	private/etc/periodic/daily/310.accounting \
	private/etc/periodic/daily/400.status-disks \
	private/etc/periodic/daily/420.status-network \
	private/etc/periodic/daily/430.status-rwho \
	private/etc/periodic/daily/999.local \
	private/etc/periodic/weekly/320.whatis \
	private/etc/periodic/weekly/999.local \
	private/etc/periodic/monthly/199.rotate-fax \
	private/etc/periodic/monthly/200.accounting \
	private/etc/periodic/monthly/999.local

COMMANDS := newsyslog periodic

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
	for i in $(COMMANDS) ; do \
		echo $(MAKE) -C $$i $@ SRCROOT=$(SRCROOT)/$$i \
			OBJROOT=$(OBJROOT)/$$i SYMROOT=$(SYMROOT)/$$i \
			DSTROOT=$(DSTROOT); \
		$(MAKE) -C $$i $@ SRCROOT=$(SRCROOT)/$$i \
			OBJROOT=$(OBJROOT)/$$i SYMROOT=$(SYMROOT)/$$i \
			DSTROOT=$(DSTROOT); \
	done; \
	'
	$(INSTALL_DIRECTORY) $(DSTROOT)/private/etc/newsyslog.d
