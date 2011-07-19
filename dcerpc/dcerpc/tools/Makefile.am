## Process this file with automake to produce Makefile.in
# Makefile for various tools

include $(top_srcdir)/include.mk

# Build the epstatus tool
bin_PROGRAMS = epstatus

epstatus_SOURCES = epstatus.c
epstatus_LDADD=../dcelib/libdcerpc.la ../libdcethread/libdcethread.la

epstatus_LDFLAGS=$(AM_LDFLAGS) $(NAF_NP_LDFLAGS)
