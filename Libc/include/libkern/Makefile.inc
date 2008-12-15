LIBKERN_INSTHDRS += OSAtomic.h OSCacheControl.h

.ifdef FEATURE_MEM_THERM_NOTIFICATION_APIS
LIBKERN_INSTHDRS += OSMemoryNotification.h OSThermalNotification.h
.endif

LIBKERN_INSTHDRS := ${LIBKERN_INSTHDRS:S/^/${.CURDIR}\/include\/libkern\//}
