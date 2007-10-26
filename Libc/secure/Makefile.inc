.PATH: ${.CURDIR}/secure

MISRCS += chk_fail.c		\
	  memcpy_chk.c		\
	  memmove_chk.c		\
	  memset_chk.c		\
	  strcpy_chk.c		\
	  stpcpy_chk.c		\
	  strncpy_chk.c		\
	  strcat_chk.c		\
	  strncat_chk.c		\
	  sprintf_chk.c		\
	  snprintf_chk.c	\
	  vsprintf_chk.c	\
	  vsnprintf_chk.c

LDBLSRCS += sprintf_chk.c	\
	    snprintf_chk.c	\
	    vsprintf_chk.c	\
	    vsnprintf_chk.c
