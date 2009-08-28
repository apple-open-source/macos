# Makefile for tkinter

TKINTER = $(shell test -f "$(LIBPYTHON)/lib-dynload/_tkinter.so" && echo yes)
EXAMPLES = $(DSTROOT)/Developer/Examples/Python/tkinter

no_target: build

build:
ifneq ($(TKINTER),)
	@echo tkinter was built as part of python
else
	@echo tkinter was not built as part of python
endif

install:
ifneq ($(TKINTER),)
	@echo tkinter was installed as part of python
	install -d -g admin -m 0775 $(EXAMPLES)
	rsync -rlt $(OBJROOT)/../python/Demo/tkinter/ $(EXAMPLES)
ifeq ($(shell id -u), 0)
	-chown -R root:admin $(EXAMPLES)
	-chmod -R g+w $(EXAMPLES)
endif
else
	@echo tkinter was not installed as part of python
endif
