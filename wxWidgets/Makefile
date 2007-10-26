##
# Makefile for wxWidgets
##

Project               = wxWidgets
WXVERS		      = wx-$(BASE_VERSION)
Configure	      = ../configure
Extra_Configure_Flags = --disable-static --with-mac --with-opengl --enable-optimize --enable-debug_flag --disable-precomp-headers --enable-unicode --enable-monolithic --with-regex=sys --enable-geometry --enable-graphics_ctx --enable-sound --enable-mediactrl --enable-display --disable-debugreport

Extra_CC_Flags       += -Wall -fno-common
Extra_Cxx_Flags      += -Wall -fno-common
Extra_Install_Flags   = DESTDIR=${DSTROOT}

# It's a GNU Source project, but we need to totally override install
GnuNoInstall = YES
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Flags := $(shell echo $(Install_Flags) | sed 's/prefix=[^ ]* *//')
Install_Target = install
FIX = $(SRCROOT)/fix

##---------------------------------------------------------------------
# Patch Makefiles and pyconfig.h just after running configure
##---------------------------------------------------------------------
ConfigStamp2 = $(ConfigStamp)2

configure:: $(ConfigStamp2)

$(ConfigStamp2): $(ConfigStamp)
	$(_v) ed - ${OBJROOT}/lib/wx/include/mac-unicode-debug-$(BASE_VERSION)/wx/setup.h < $(FIX)/setup.h.ed
	$(_v) $(TOUCH) $(ConfigStamp2)

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

# we also need to build contrib
CONTRIB=fl foldbar gizmos net ogl plot stc svg
build:: configure
	@set -x && \
	echo "Building $(Project)..." && \
	$(MAKE) -C $(BuildDirectory) $(Environment) && \
	for i in $(CONTRIB); do \
	    echo "Building $(Project)/contrib/src/$$i..." && \
	    $(MAKE) -C $(BuildDirectory)/contrib/src/$$i $(Environment) \
		|| exit 1; \
	done

# totally override install
install:: build
	@echo "Installing $(Project)..."
	$(_v) umask $(Install_Mask)
	$(MAKE) -C $(BuildDirectory) $(Environment) $(Install_Flags) $(Install_Target)
	@set -x && \
	umask $(Install_Mask) && \
	for i in $(CONTRIB); do \
	    echo "Installing $(Project)/contrib/src/$$i..." && \
	    $(MAKE) -C $(BuildDirectory)/contrib/src/$$i $(Environment) $(Install_Flags) $(Install_Target) \
		|| exit 1; \
	done
	$(_v) $(FIND) $(DSTROOT) $(Find_Cruft) | $(XARGS) $(RMDIR)
	$(_v) $(FIND) $(SYMROOT) $(Find_Cruft) | $(XARGS) $(RMDIR)
	$(_v)- $(CHOWN) -R $(Install_User):$(Install_Group) $(DSTROOT) $(SYMROOT)
	@set -x && \
	for i in `find $(DSTROOT)/usr/lib -name \*.dylib -type f`; do \
	    strip -x $$i; \
	done
	@set -x && \
	for i in `find $(DSTROOT)/usr/include/$(WXVERS)/wx/mac -name \*.h -size 0c`; do \
	    echo '/* empty */' > $$i; \
	done
	@set -x && \
	for i in `find $(DSTROOT)/usr/include/$(WXVERS) -name \*.h -perm +0111`; do \
	    chmod 0644 $$i; \
	done
	@set -x && \
	dest=`readlink $(DSTROOT)/usr/bin/wx-config | sed 's,.*/usr/lib/,../lib/,'` && \
	    rm $(DSTROOT)/usr/bin/wx-config && \
	    ln -s $$dest $(DSTROOT)/usr/bin/wx-config
	@set -x && \
	for i in `find $(DSTROOT)/usr/lib/wx/config -type f -print0 | xargs -0 fgrep -l -e -arch`; do \
	    ed - $$i < $(FIX)/no-arch.ed; \
	done
