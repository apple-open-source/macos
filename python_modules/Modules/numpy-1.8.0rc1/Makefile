NAME = numpy
VERSION = 1.8.0rc1

include ../Makefile.inc

# https://github.com/numpy/numpy/issues/3901
# np.linalg.eig() fails on 1x1 matrices when link against Accelerate.
# Temporary workaround is to avoid linking against Accelerate.
# The bug has been fixed in https://github.com/numpy/numpy/pull/3902
# so this will be unnecessary for releases after 1.8.0rc1
custompatching:
	sed -e 's,@PYEXECUTABLE@,$(PYEXECUTABLE),g' setup.py.f2py.patch | patch $(DIR)/numpy/f2py/setup.py
	ed - $(DIR)/numpy/linalg/setup.py < 3901.ed
	ed - $(DIR)/numpy/distutils/system_info.py < system_info.py.ed
	sed -e 's,@SDKROOT@,$(SDKROOT),g' -e 's,@INCLUDEPYTHONVERS@,$(INCLUDEPYTHONVERS),g' $(SRCROOT)/Modules/setup.py-config.ed | ed - $(DIR)/setup.py
	( cd $(DIR) ; patch -p1 ) < arm64.diff
	( cd $(DIR) ; patch -p1 ) < 103310602-compilier-fixes.diff

postinstall:
	cp -f $(DIR)/LICENSE.txt $(OSL)/$(NAME)-$(VERSION).txt
	cp -f $(NAME).partial $(OSV)/$(NAME)-$(VERSION).partial
