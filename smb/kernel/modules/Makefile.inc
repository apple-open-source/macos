CFLAGS+= -I${.CURDIR}/../../mysys

.if defined(SMP)
CFLAGS+= -DSMP -DAPIC_IO
.endif
