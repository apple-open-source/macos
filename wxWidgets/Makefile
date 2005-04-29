##
# Makefile for wxWidgets
##

Project               = wxWidgets
WXVERS		      = wx-2.5
Configure	      = ../configure
Extra_Configure_Flags = --disable-static --with-mac --with-opengl --enable-optimize --enable-debug_flag --enable-geometry --enable-sound --enable-display --disable-precomp-headers --enable-unicode --enable-monolithic --with-regex=sys

Extra_CC_Flags       += -Wall -fno-common
Extra_Cxx_Flags      += -Wall -fno-common
Extra_Install_Flags   = DESTDIR=${DSTROOT}

# It's a GNU Source project, but we need to totally override install
GnuNoInstall = YES
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Flags := $(shell echo $(Install_Flags) | sed 's/prefix=[^ ]* *//')
Install_Target = install

# Environment code from Common.make and GNUSource.make
ifneq "$(strip $(CFLAGS))" ""
OGLEnvironment += CFLAGS="$(CFLAGS)"
endif
ifneq "$(strip $(CXXFLAGS))" ""
OGLEnvironment += CCFLAGS="$(CXXFLAGS)" CXXFLAGS="$(CXXFLAGS) -DwxUSE_DEPRECATED=0"
endif
ifneq "$(strip $(LDFLAGS))" ""
OGLEnvironment += LDFLAGS="$(LDFLAGS)"
endif
ifneq "$(strip $(CPPFLAGS))" ""
OGLEnvironment += CPPFLAGS="$(CPPFLAGS)"
endif
OGLEnvironment += $(Extra_Environment) TEXI2HTML="/usr/bin/texi2html -subdir ."

# we also need to build gizmos, ogl and stc
build:: configure
	@echo "Building $(Project)/contrib/src/gizmos..."
	$(_v) $(MAKE) -C $(BuildDirectory)/contrib/src/gizmos $(Environment)
	@echo "Building $(Project)/contrib/src/ogl..."
	$(_v) $(MAKE) -C $(BuildDirectory)/contrib/src/ogl $(OGLEnvironment)
	@echo "Building $(Project)/contrib/src/stc..."
	$(_v) $(MAKE) -C $(BuildDirectory)/contrib/src/stc $(Environment)

# totally override install
install:: build
	@echo "Installing $(Project)..."
	$(_v) umask $(Install_Mask) ; $(MAKE) -C $(BuildDirectory) $(Environment) $(Install_Flags) $(Install_Target)
	@echo "Installing $(Project)/contrib/src/gizmos..."
	$(_v) umask $(Install_Mask) ; $(MAKE) -C $(BuildDirectory)/contrib/src/gizmos $(Environment) $(Install_Flags) $(Install_Target)
	@echo "Installing $(Project)/contrib/src/ogl..."
	$(_v) umask $(Install_Mask) ; $(MAKE) -C $(BuildDirectory)/contrib/src/ogl $(OGLEnvironment) $(Install_Flags) $(Install_Target)
	@echo "Installing $(Project)/contrib/src/stc..."
	$(_v) umask $(Install_Mask) ; $(MAKE) -C $(BuildDirectory)/contrib/src/stc $(Environment) $(Install_Flags) $(Install_Target)
	$(_v) $(FIND) $(DSTROOT) $(Find_Cruft) | $(XARGS) $(RMDIR)
	$(_v) $(FIND) $(SYMROOT) $(Find_Cruft) | $(XARGS) $(RMDIR)
	$(_v)- $(CHOWN) -R $(Install_User):$(Install_Group) $(DSTROOT) $(SYMROOT)
	@for i in `find $(DSTROOT)/usr/lib -name \*.dylib -type f`; do \
	    echo strip -x $$i && \
	    strip -x $$i; \
	done
	@for i in `find $(DSTROOT)/usr/include/$(WXVERS)/wx/mac -name \*.h -size 0c`; do \
	    echo echo '/* empty */' \> $$i && \
	    echo '/* empty */' > $$i; \
	done
	@for i in `find $(DSTROOT)/usr/include/$(WXVERS) -name \*.h -perm +0111`; do \
	    echo chmod 0644 $$i && \
	    chmod 0644 $$i; \
	done
	@dest=`readlink $(DSTROOT)/usr/bin/wx-config | sed 's,.*/usr/lib/,../lib/,'` && \
	    echo rm $(DSTROOT)/usr/bin/wx-config && \
	    rm $(DSTROOT)/usr/bin/wx-config && \
	    echo ln -s $$dest $(DSTROOT)/usr/bin/wx-config && \
	    ln -s $$dest $(DSTROOT)/usr/bin/wx-config
	@for i in `find $(DSTROOT)/usr/lib/wx/config -type f -print0 | xargs -0 fgrep -l -e -arch`; do \
	    echo ed - $$i \< $(SRCROOT)/fix/no-arch.ed && \
	    ed - $$i < $(SRCROOT)/fix/no-arch.ed; \
	done
