## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.4

SUBDIRS = pathplan_data

demodir = $(datadir)/@PACKAGE@/demo
if WITH_TCL
demo_SCRIPTS = pathplan
demo_DATA = pathplan.README
endif

EXTRA_DIST = pathplan pathplan.README
