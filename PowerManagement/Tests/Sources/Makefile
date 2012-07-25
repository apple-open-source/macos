SOURCES     = AssertEach.c \
 	     AssertOne.c \
             AssertOneWithTimeouts.c \
             AssertOneChildCrash.c \
             AssertOneWithProperties.c \
             AssertTimeouts.c \
             EnergyPrefs.c \
             SmartBatteryUserClient.c \
             assert_settimeout.c	\
             pmconfigdserver_connect.c \
             assertions-crash-configd-6765534.c \
             override_default_pm_settings.c \
             IOPowerSourcesExercise-8521443.c \
             AssertionBenchmarkJune2011.c \
             CopyPropertiesTester.c \
             AssertTimeouts-TurnOff-9892470.c \
             AssertTimeouts-Kill-10652741.c \
             IOPMConnection-return-values-11614683.c

OBJS        = ${SOURCES:.c=.o}
BINARIES    = ${OBJS:.o=}


all: ${BINARIES}

${BINARIES}: ${OBJS} PMTestLib.o
	${LD} -o ${@} ${@}.o PMTestLib.o ${LDFLAGS}

clean:
	rm -f ${OBJS} ${BINARIES} PMTestLib.o




