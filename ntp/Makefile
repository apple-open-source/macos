##
# Makefile for ntp
##

# Project info
Project           = ntp
UserType          = Administration
ToolType          = Services
Extra_Environment = AUTOCONF="$(Sources)/missing autoconf"	\
                    AUTOHEADER="$(Sources)/missing autoheader"	\
                    ac_cv_decl_syscall=no			\
                    ac_cv_header_netinfo_ni_h=no		\
                    LIBMATH=""

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make
