# uuid sources
.PATH: ${.CURDIR}/uuid
CWD := ${.CURDIR}/uuid

# uuid/uuid.h is now installed by xnu

UUIDSRCS = clear.c compare.c copy.c gen_uuid.c isnull.c pack.c parse.c \
	unpack.c unparse.c
UUIDHDRS = uuidP.h
UUIDFROMMAN = libuuid.3.in
UUIDTOMAN = uuid.3.in
UUIDMAN3 = uuid_clear.3.in uuid_compare.3.in uuid_copy.3.in \
           uuid_generate.3.in  uuid_is_null.3.in uuid_parse.3.in \
           uuid_unparse.3.in
PRIVUUID_INSTHDRS += ${.CURDIR}/uuid/namespace.h

# This .for statement forces evaluation of ${CWD}
.for _cwd in ${CWD}

.for _src in ${UUIDSRCS}
.ifmake autopatch
${_cwd}/${_src:R}-uuid.${_src:E}: ${_cwd}/uuidsrc/${_src} _AUTOPATCH
AUTOPATCHSRCS+= ${_cwd}/${_src:R}-uuid.${_src:E}
.else # !autopatch
MISRCS+= ${_src}
.endif # autopatch
.endfor

.ifmake autopatch
.for _src in ${UUIDHDRS}
${_cwd}/${_src}: ${_cwd}/uuidsrc/${_src} _AUTOPATCH
AUTOPATCHHDRS+= ${_cwd}/${_src}
.endfor
.endif # autopatch

.for _src in ${UUIDMAN3}
.ifmake autopatch
${_cwd}/${_src:R}-uuid.${_src:E}: ${_cwd}/uuidsrc/${_src} _AUTOPATCH
AUTOPATCHMAN+= ${_cwd}/${_src:R}
.else # !autopatch
MAN3+= ${_src:R}
.endif # autopatch
.endfor

MLINKS+= uuid_generate.3 uuid_generate_random.3		\
         uuid_generate.3 uuid_generate_time.3

MLINKS+= uuid_unparse.3 uuid_unparse_lower.3		\
         uuid_unparse.3 uuid_unparse_upper.3

.ifmake autopatch
${_cwd}/${UUIDTOMAN:R}-uuid.${UUIDTOMAN:E}: ${_cwd}/uuidsrc/${UUIDFROMMAN} _AUTOPATCH
AUTOPATCHMAN+= ${_cwd}/${UUIDTOMAN:R}
.else # !autopatch
MAN3+= ${UUIDTOMAN:R}
.endif # autopatch

.endfor # _cwd
