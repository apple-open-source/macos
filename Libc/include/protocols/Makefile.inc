PROTO_INSTHDRS += dumprestore.h \
                  routed.h \
                  rwhod.h \
                  talkd.h \
                  timed.h
PROTO_INSTHDRS := ${PROTO_INSTHDRS:S/^/${.CURDIR}\/include\/protocols\//}
