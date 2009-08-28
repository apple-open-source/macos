# Makefile for all Extras sub-projects

PROJECTS = tkinter

export PATH := $(DSTROOT)/usr/bin:$(PATH)

no_target:
	@for i in $(PROJECTS); do \
	    echo make -C $$i; \
	    make -C $$i; \
	done

install:
	@for i in $(PROJECTS); do \
	    echo make -C $$i install; \
	    make -C $$i install; \
	done
