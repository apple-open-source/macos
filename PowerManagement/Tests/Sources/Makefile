SOURCES     = AssertEach.c \
             AssertOneChildCrash.c \
             AssertOneWithProperties.c \
             EnergyPrefs.c \
             SmartBatteryUserClient.c \
             assert_settimeout.c	\
             pmconfigdserver_connect.c \
             assertions-crash-configd-6765534.c \
             override_default_pm_settings.c \
             IOPowerSourcesExercise-8521443.c

OBJS        = ${SOURCES:.c=.o}
BINARIES    = ${OBJS:.o=}
#CC          = llvm-gcc-4.2
#CFLAGS      = -Wall
#LD          = llvm-gcc-4.2
#LDFLAGS     = -framework IOKit -framework CoreFoundation -framework SystemConfiguration

all: ${BINARIES}

${BINARIES}: ${OBJS} PMTestLib.o
	${LD} -o ${@} ${@}.o PMTestLib.o ${LDFLAGS}

clean:
	rm -f ${OBJS} ${BINARIES} PMTestLib.o




