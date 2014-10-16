# Makefile

.PHONY: build all install installsrc installhdrs test check Makefile testexcldebug 
PRODUCT=regressions

TESTDIRS := .

HFILES := $(shell find $(TESTDIRS) -type f -name \*.h -print)
ALL_CFILES := $(shell find $(TESTDIRS) -type f -name \*.c -print)
ALL_CXXFILES := $(shell find $(TESTDIRS) -type f -name \*.cpp -print)
ALL_MFILES := $(shell find $(TESTDIRS) -type f -name \*.m -print)
SHFILES := $(shell find $(TESTDIRS) -type f -name \*.sh -print)
TFILES := $(shell find $(TESTDIRS) -type f -name \*.t -print)

PAREN := (
CTESTS := $(shell grep -l 'main[ \t]*$(PAREN)' $(ALL_CFILES) /dev/null)
CXXTESTS := $(shell grep -l 'main[ \t]*$(PAREN)' $(ALL_CXXFILES) /dev/null)
MTESTS := $(shell grep -l 'main[ \t]*$(PAREN)' $(ALL_MFILES) /dev/null)
SHTESTS := $(SHFILES)

EXCLSTR := NO_AUTOMATED_TESTING
EXCL_CTESTS := $(shell grep -l $(EXCLSTR) $(CTESTS) /dev/null)
EXCL_CXXTESTS := $(shell grep -l $(EXCLSTR) $(CXXTESTS) /dev/null)
EXCL_MTESTS := $(shell grep -l $(EXCLSTR) $(MTESTS) /dev/null)
EXCL_SHTESTS := $(shell grep -l $(EXCLSTR) $(SHTESTS) /dev/null)
EXCL_TESTS := $(EXCL_CTESTS) $(EXCL_CXXTESTS) $(EXCL_MTESTS) $(EXCL_SHTESTS)

CFILES = $(filter-out $(CTESTS),$(ALL_CFILES))
CXXFILES = $(filter-out $(CXXTESTS),$(ALL_CXXFILES))
MFILES = $(filter-out $(MTESTS),$(ALL_MFILES))

BUILT_PRODUCTS_DIR ?= $(shell echo $(LOCAL_BUILD_DIR))
ifeq "" "$(BUILT_PRODUCTS_DIR)"
        BUILT_PRODUCTS_DIR = $(SYMROOT)
        ifeq "" "$(BUILT_PRODUCTS_DIR)"
                BUILT_PRODUCTS_DIR = build
        endif
endif
SYMROOT ?= $(BUILT_PRODUCTS_DIR)
OBJROOT ?= $(BUILT_PRODUCTS_DIR)/intermediates/$(PRODUCT).build
SRCROOT ?= $(shell /bin/pwd)
DSTROOT ?= ""

INSTALLDIR := $(DSTROOT)/usr/local/bin

SRCFILES := Makefile $(HFILES) $(CTESTS) $(CFILES) $(CXXTESTS) $(CXXFILES) $(MTESTS) $(MFILES)

BUILT_CTARGETS := $(addprefix $(BUILT_PRODUCTS_DIR)/,$(notdir $(basename $(CTESTS))))
BUILT_CXXTARGETS := $(addprefix $(BUILT_PRODUCTS_DIR)/,$(notdir $(basename $(CXXTESTS))))
BUILT_MTARGETS := $(addprefix $(BUILT_PRODUCTS_DIR)/,$(notdir $(basename $(MTESTS))))
BUILT_TARGETS := $(BUILT_CTARGETS) $(BUILT_CXXTARGETS) $(BUILT_MTARGETS)
EXCL_TARGETS := $(addprefix $(BUILT_PRODUCTS_DIR)/, $(notdir $(basename $(EXCL_TESTS))))
TEST_TARGETS := $(filter-out $(EXCL_TARGETS),$(BUILT_TARGETS))

COFILES := $(addprefix $(OBJROOT)/,$(addsuffix .o, $(basename $(CFILES))))
CXXOFILES := $(addprefix $(OBJROOT)/,$(addsuffix .o, $(basename $(CXXFILES))))
MOFILES := $(addprefix $(OBJROOT)/,$(addsuffix .o, $(basename $(MFILES))))
OFILES := $(COFILES) $(CXXOFILES) $(MOFILES)

TEST_COFILES := $(addprefix $(OBJROOT)/,$(addsuffix .o, $(basename $(CTESTS))))
TEST_CXXOFILES := $(addprefix $(OBJROOT)/,$(addsuffix .o, $(basename $(CXXTESTS))))
TEST_MOFILES := $(addprefix $(OBJROOT)/,$(addsuffix .o, $(basename $(MTESTS))))
TEST_OFILES := $(TEST_COFILES) $(TEST_CXXOFILES) $(TEST_MOFILES)

OBJDIRS := $(sort $(OBJROOT) $(dir $(OFILES) $(TEST_OFILES)))

#
# Assume final load with cc, not ld
#

C_FRAMEWORK_SEARCH_PATHS = $(BUILT_PRODUCTS_DIR)
CPP_FRAMEWORK_SEARCH_PATHS = $(BUILT_PRODUCTS_DIR) \
        /System/Library/PrivateFrameworks /usr/local/SecurityPieces/Frameworks
M_FRAMEWORK_SEARCH_PATHS = $(BUILT_PRODUCTS_DIR) \
        /System/Library/PrivateFrameworks
STD_HEADER_SEARCH_PATHS = $(SRCROOT)/test
STD_LIBARY_SEARCH_PATHS = $(BUILT_PRODUCTS_DIR)

ALL_HEADER_SEARCH_PATHS = $(HEADER_SEARCH_PATHS) $(STD_HEADER_SEARCH_PATHS)
ALL_LIBARY_SEARCH_PATHS = $(LIBARY_SEARCH_PATHS) $(STD_LIBARY_SEARCH_PATHS)

STD_LIBRARIES =
ALL_LIBRARIES = $(STD_LIBRARIES) $(LIBRARIES:lib%.a=%) 

C_FRAMEWORKS = Security CoreFoundation
CPP_FRAMEWORKS = $(C_FRAMEWORKS) #security_utilities
M_FRAMEWORKS = $(C_FRAMEWORKS) SecurityFoundation Foundation

C_FINCLUDES = $(addprefix -F,$(C_FRAMEWORK_SEARCH_PATHS) \
	$(FRAMEWORK_SEARCH_PATHS))
CPP_FINCLUDES = $(addprefix -F,$(CPP_FRAMEWORK_SEARCH_PATHS) \
	$(FRAMEWORK_SEARCH_PATHS))
M_FINCLUDES = $(addprefix -F,$(M_FRAMEWORK_SEARCH_PATHS) \
	$(FRAMEWORK_SEARCH_PATHS))
ALL_INCLUDES = $(addprefix -I,$(ALL_HEADER_SEARCH_PATHS))

#
# Override this from the make command line to add e.g. -lMallocDebug
#
CMDLINE_LDFLAGS=

#
C_LDFLAGS = $(CMDLINE_LDFLAGS) \
        $(C_FINCLUDES) \
        $(addprefix -framework ,$(C_FRAMEWORKS) $(FRAMEWORKS)) \
        $(addprefix -L,$(ALL_LIBARY_SEARCH_PATHS)) \
        $(addprefix -l,$(ALL_LIBARIES)) \
        $(OTHER_LDFLAGS)
CPP_LDFLAGS = $(CMDLINE_LDFLAGS) \
        $(CPP_FINCLUDES) \
        $(addprefix -framework ,$(CPP_FRAMEWORKS) $(FRAMEWORKS)) \
        $(addprefix -L,$(ALL_LIBARY_SEARCH_PATHS)) \
        $(addprefix -l,$(ALL_LIBARIES)) \
        $(OTHER_LDFLAGS) /usr/local/lib/libsecurity_utilities.a
M_LDFLAGS = $(CMDLINE_LDFLAGS) \
        $(M_FINCLUDES) \
        $(addprefix -framework ,$(M_FRAMEWORKS) $(FRAMEWORKS)) \
        $(addprefix -L,$(ALL_LIBARY_SEARCH_PATHS)) \
        $(addprefix -l,$(ALL_LIBARIES)) \
        $(OTHER_LDFLAGS)

###WARNING_CFLAGS = -Wno-four-char-constants -Wall -Werror
WARNING_CFLAGS = -Wno-four-char-constants -Wall -Wno-deprecated-declarations
STD_CFLAGS= -g
DEBUG_CFLAGS=

ALL_CFLAGS = $(C_FINCLUDES) $(ALL_INCLUDES) $(STD_CFLAGS) \
        $(WARNING_CFLAGS) $(OTHER_CFLAGS) $(DEBUG_CFLAGS)
ALL_CXXFLAGS = $(CPP_FINCLUDES) $(ALL_INCLUDES) $(STD_CFLAGS) \
        $(WARNING_CFLAGS) $(OTHER_CXXFLAGS) $(DEBUG_CFLAGS)
ALL_MFLAGS = $(M_FINCLUDES) $(ALL_INCLUDES) $(STD_CFLAGS) \
        $(WARNING_CFLAGS) $(OTHER_CFLAGS) $(DEBUG_CFLAGS)

build: $(OTHER_DEPENDENCIES) $(OBJDIRS) $(BUILT_TARGETS)

all: build

ppc64:
	make CMDLINE_LDFLAGS="-arch ppc64" DEBUG_CFLAGS="-arch ppc64"

install: build
	install -d -m 0755 $(INSTALLDIR)
	install -p -m 0755 $(BUILT_TARGETS) $(INSTALLDIR)

installhdrs:

installsrc:
	mkdir $(DSTROOT)

clean:
	rm -f $(BUILT_TARGETS) $(OFILES) $(TEST_OFILES) $(OTHER_TO_CLEAN)
#	echo rm -rf $(OBJDIRS)

test: build
	@perl -Iinc -e 'use MyHarness; Test::Harness::runtests @ARGV;' $(sort $(TEST_TARGETS)) $(sort $(TFILES))

testexcldebug: build
	@echo EXCL_CTESTS: $(EXCL_CTESTS)
	@echo EXCL_CXXTESTS: $(EXCL_CXXTESTS)
	@echo EXCL_MTESTS: $(EXCL_MTESTS)
	@echo EXCL_SHTESTS: $(EXCL_SHTESTS)
	@echo EXCL_TESTS: $(EXCL_TESTS)
	@echo BUILT_TARGETS: $(BUILT_TARGETS)
	@echo TEST_TARGETS: $(TEST_TARGETS)

LOCAL_OFILES := $(addsuffix .o, $(basename $(CFILES) $(CXXFILES) $(MFILES)))
OFILES := $(LOCAL_OFILES:%=$(OBJROOT)/%)

$(BUILT_CTARGETS): $(COFILES) $(TEST_COFILES)
	$(CC) -o $@ $(filter %/$(notdir $@).o,$(TEST_COFILES)) $(OFILES) $(C_LDFLAGS)

$(BUILT_CXXTARGETS): $(COFILES) $(CXXOFILES) $(TEST_CXXOFILES)
	$(CXX) -o $@ $(filter %/$(notdir $@).o,$(TEST_CXXOFILES)) $(OFILES) $(CPP_LDFLAGS)

$(BUILT_MTARGETS): $(COFILES) $(MOFILES) $(TEST_MOFILES)
	$(CC) -o $@ $(filter %/$(notdir $@).o,$(TEST_MOFILES)) $(OFILES) $(M_LDFLAGS)

$(OBJDIRS):
	@mkdir -p $@

$(OBJROOT)/%.o: %.c
	$(CC) $(ALL_CFLAGS) -c -o $(OBJROOT)/$*.o $<

$(OBJROOT)/%.o: %.cpp
	$(CXX) $(ALL_CXXFLAGS) -c -o $(OBJROOT)/$*.o $<

$(OBJROOT)/%.o: %.m
	$(CC) $(ALL_MFLAGS) -c -o $(OBJROOT)/$*.o $<

$(OFILES) $(TEST_OFILES): $(HFILES)
