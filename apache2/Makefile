##
# Apple wrapper Makefile for Apache 2
# Copyright (c) 2003-2004 by Apple Computer, Inc.
##
# Untar, build, create a binary distribution, install into /opt/apache2
#
PROJECT_NAME=httpd
PROJECT_VERSION=2.0.50
PROJECT_DIR=$(PROJECT_NAME)-$(PROJECT_VERSION)
PROJECT_ARCHIVE=$(PROJECT_DIR).tar.gz
FINAL_DIR=/opt/apache2
DST_DIR=$(DSTROOT)/opt/apache2

PROJECT_FILES=Makefile

# These includes provide the proper paths to system utilities

include $(MAKEFILEPATH)/pb_makefiles/platform.make
include $(MAKEFILEPATH)/pb_makefiles/commands-$(OS).make

GNUTAR=/usr/bin/gnutar
INSTALL=/usr/bin/install

# Build rules

default:: do_untar

build:: configure do_build

install:: build do_install

clean:: do_clean

configure:: do_untar do_configure

installhdrs:: do_installhdrs

installsrc:: do_installsrc

do_untar:
	$(ECHO) "Untarring $(PROJECT_NAME)..."
	if [ ! -e $(PROJECT_DIR)/README ]; then\
		$(GNUTAR) -xzf $(PROJECT_ARCHIVE);\
	fi

# Custom configuration:
#
do_configure: 
	$(ECHO) "Configuring $(PROJECT_NAME)..."
	$(CD) $(PROJECT_DIR); export LIBTOOL_CMD_SEP=; \
				./buildconf
	$(CD) $(PROJECT_DIR); export LIBTOOL_CMD_SEP=; \
			./configure \
			LDFLAGS="$$RC_CFLAGS" \
			CFLAGS="$$RC_CFLAGS" \
			--prefix=$(FINAL_DIR) \
			--with-port=8080 \
			--with-mpm=worker \
			--enable-mods-shared=most \
			--enable-ssl \
			--enable-info \
			--enable-example \
			--enable-file-cache \
			--enable-proxy
	
do_build:
	$(ECHO) "Building $(PROJECT_NAME) in $(PROJECT_DIR)..."
	$(CD) $(PROJECT_DIR); export LIBTOOL_CMD_SEP=; make

do_install: $(DST_ROOT) 
	$(ECHO) "Installing $(PROJECT)..."
	$(CD) $(PROJECT_DIR); export LIBTOOL_CMD_SEP=; make install DESTDIR=$(DSTROOT)
	$(CD) $(DST_DIR); sed \
		-e "s%^ServerAdmin.*%ServerAdmin webmaster@example.com%" \
		-e "s%#ServerName.*%#ServerName localhost%" \
		-e "s%^User .*%User www%" \
		-e "s%^Group .*%Group www%" \
		conf/httpd-std.conf > conf/httpd.conf
	$(CD) $(DST_DIR); cp conf/httpd.conf conf/httpd-std.conf
	$(CD) $(DST_DIR); sed \
		-e "s%^ServerAdmin.*%ServerAdmin webmaster@example.com%" \
		-e "s%#ServerName.*%#ServerName localhost:443%" \
		conf/ssl-std.conf > conf/ssl.conf
	$(CD) $(DST_DIR); cp conf/ssl.conf conf/ssl-std.conf
	$(STRIP) -S $(DST_DIR)/lib/lib*.a
	$(STRIP) -S $(DST_DIR)/lib/lib*.dylib
	$(STRIP) -S $(DST_DIR)/bin/*
	$(STRIP) -S $(DST_DIR)/modules/*
	$(CHMOD) 644 $(DST_DIR)/manual/images/*
	$(CHMOD) 644 $(DST_DIR)/manual/howto/*
	$(CHMOD) 644 $(DST_DIR)/manual/programs/*
	$(CHMOD) 644 $(DST_DIR)/manual/mod/*
	$(CHMOD) 644 $(DST_DIR)/manual/urlmapping.*
	$(INSTALL) -d -m 700 $(DST_DIR)/conf/ssl.crt
	$(INSTALL) -d -m 700 $(DST_DIR)/conf/ssl.key
	$(CHOWN) -R root:wheel $(DST_DIR)
	$(ECHO) "Install complete."

do_installhdrs:
	$(ECHO) "No headers to install"

do_installsrc:
	$(ECHO) "Installing $(PROJECT_NAME) sources in $(SRCROOT)..."
	-$(RM) -rf $(SRCROOT)
	$(MKDIRS) $(SRCROOT)
	$(CP) $(PROJECT_FILES) $(SRCROOT)
	$(CP) $(PROJECT_ARCHIVE) $(SRCROOT)

do_clean:
	$(ECHO) "Cleaning $(PROJECT_NAME)..."
	-$(RM) -rf $(PROJECT_DIR)

$(DST_ROOT):
	$(MKDIRS) $@
