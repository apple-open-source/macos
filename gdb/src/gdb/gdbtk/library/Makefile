
TCL := $(wildcard *.tcl *.ith *.itb)

ITCL_SH = itclsh3.0

tclIndex: $(TCL) Makefile
	echo "auto_mkindex `pwd` $(TCL)" | $(ITCL_SH)

tags: TAGS
TAGS: $(TCL)
	etags --lang=none --regex='/[ \t]*\(proc\|method\|itcl_class\)[ \t]+\([^ \t]+\)/\1/' $(TCL)
