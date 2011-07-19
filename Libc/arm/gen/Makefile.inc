.PATH: ${.CURDIR}/arm/gen
MDSRCS += \
	cpu_number.s \
	icacheinval.s

DYLDSRCS += icacheinval.s
