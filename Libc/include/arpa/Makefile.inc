ARPA_INSTHDRS += ftp.h \
                 nameser_compat.h \
                 telnet.h \
                 tftp.h 
ARPA_INSTHDRS := ${ARPA_INSTHDRS:S/^/${.CURDIR}\/include\/arpa\//}
