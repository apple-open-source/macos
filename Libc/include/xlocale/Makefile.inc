INCXLOCALE_INSTHDRS += __wctype.h _ctype.h _inttypes.h _langinfo.h \
			_monetary.h _stdlib.h _stdio.h _string.h \
			_time.h _wchar.h _wctype.h

INCXLOCALE_INSTHDRS := ${INCXLOCALE_INSTHDRS:S/^/${.CURDIR}\/include\/xlocale\//}
XLOCALE_INSTHDRS += ${INCXLOCALE_INSTHDRS}

STRIP_HDRS += xlocale/_ctype.h
