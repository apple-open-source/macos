
TCL := $(wildcard *.tcl *.itcl *.ith *.itb)

TCLSH = tclsh

tclIndex: $(TCL) Makefile
	echo "package require Itcl; auto_mkindex `pwd` $(TCL)" | $(TCLSH)

tags: TAGS
TAGS: $(TCL)
	etags --lang=none --regex='/[ \t]*\(proc\|method\|itcl_class\)[ \t]+\([^ \t]+\)/\1/' $(TCL)
