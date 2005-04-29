# uuid sources
.PATH: ${.CURDIR}/uuid

# uuid/uuid.h is now installed by xnu

UUIDSRCS = clear.c compare.c copy.c gen_uuid.c isnull.c pack.c parse.c \
	unpack.c unparse.c
UUIDHDRS = uuidP.h
UUIDFROMMAN = libuuid.3.in
UUIDTOMAN = uuid.3.in
UUIDMAN3 = uuid_clear.3.in uuid_compare.3.in uuid_copy.3.in uuid_generate.3.in \
	uuid_is_null.3.in uuid_parse.3.in uuid_unparse.3.in
PRIVUUID_INSTHDRS += ${.CURDIR}/uuid/namespace.h

.for _src in ${UUIDSRCS}
${SYMROOT}/${_src:R}-uuid.${_src:E}: uuidsrc/${_src} _AUTOPATCHSYM
MISRCS+= ${_src}
AUTOPATCHSRCS+= ${SYMROOT}/${_src:R}-uuid.${_src:E}
.endfor

.for _src in ${UUIDHDRS}
${SYMROOT}/${_src}: uuidsrc/${_src} _AUTOPATCHSYM
AUTOPATCHHDRS+= ${SYMROOT}/${_src}
.endfor

.for _src in ${UUIDMAN3}
${_src:R}-uuid.${_src:E}: uuidsrc/${_src} _AUTOPATCH
MAN3+= ${_src:R}
AUTOPATCHMAN+= ${_src:R}
.endfor

${UUIDTOMAN:R}-uuid.${UUIDTOMAN:E}: uuidsrc/${UUIDFROMMAN} _AUTOPATCH
MAN3+= ${UUIDTOMAN:R}
AUTOPATCHMAN+= ${UUIDTOMAN:R}
