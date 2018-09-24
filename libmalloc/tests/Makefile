PROJECT := libmalloc
TEST_DIR := tests/

DEVELOPER_DIR ?= /Applications/Xcode.app/Contents/Developer/

include $(DEVELOPER_DIR)/AppleInternal/Makefiles/darwintest/Makefile.common

TRACE_FILES := \
	$(notdir $(wildcard $(SRCROOT)/../traces/*.mtrace))

# add trace files without the traces/ prefix, fex:
#   exclude_this_file.mtrace
EXCLUDED_TRACE_FILES :=

OTHER_TEST_TARGETS = \
	$(addprefix nano-trace-replay_, $(basename $(filter-out $(EXCLUDED_TRACE_FILES), $(TRACE_FILES))))

BATS_PLISTS = \
	$(patsubst %,$(SYMROOT)/%.plist,$(OTHER_TEST_TARGETS))

CUSTOM_TARGETS = \
	single-churn \
	single-list_allocate \
	single-tree_allocate \
	single-tree_churn \
	single-fragment \
	single-fragment_iterate \
	single-message_one \
	single-message_many \
	parallel-churn \
	parallel-list_allocate \
	parallel-tree_allocate \
	parallel-tree_churn \
	parallel-fragment \
	parallel-fragment_iterate

#	single-medium \
#	single-big \
#	parallel-medium \
#	parallel-big

MALLOCBENCH_SOURCE := $(wildcard MallocBench/*.cpp)
EXCLUDED_SOURCES := \
	MallocBench.cpp \
	nano_trace_replay.c
CXX := $(shell $(XCRUN) -sdk "$(TARGETSDK)" -find clang++)

WARNING_CFLAGS := -Wno-format-invalid-specifier -Wno-format-extra-args
OTHER_CFLAGS += \
	-DDARWINTEST \
	-DOS_UNFAIR_LOCK_INLINE=1 \
	-lCrashReporterClient \
	-I$(SDKROOT)/System/Library/Frameworks/System.framework/PrivateHeaders \
	-I$(SRCROOT)/../include/malloc \
	$(WARNING_CFLAGS)

$(OBJROOT)/MallocBench/%.o: MallocBench/%.cpp
	@mkdir -p $(OBJROOT)/MallocBench
	$(CXX) $(CFLAGS) -I$(SRCROOT)/MallocBench -std=gnu++11 -stdlib=libc++ -c -o $@ $<

single-%: $(addprefix $(OBJROOT)/, $(MALLOCBENCH_SOURCE:.cpp=.o))
	$(CXX) -c -o $(OBJROOT)/MallocBench-$@.o \
		-I$(SRCROOT)/MallocBench \
		$(CFLAGS) \
		-DBENCHMARK_NAME=\"$*\" \
		-DPARALLEL=false \
		$(SRCROOT)/MallocBench.cpp
	$(CXX) -o $(SYMROOT)/$@ \
		$(CFLAGS) \
		$^ \
		$(OBJROOT)/MallocBench-$@.o

install-single-%: $(CUSTOM_TARGETS)
	@mkdir -p $(INSTALLDIR)
	cp $(SYMROOT)/$(patsubst install-%,%,$@) $(INSTALLDIR)/$(patsubst install-%,%,$@)

parallel-%: $(addprefix $(OBJROOT)/, $(MALLOCBENCH_SOURCE:.cpp=.o))
	$(CXX) -c -o $(OBJROOT)/MallocBench-$@.o \
		-I$(SRCROOT)/MallocBench \
		$(CFLAGS) \
		-DBENCHMARK_NAME=\"$*\" \
		-DPARALLEL=true \
		$(SRCROOT)/MallocBench.cpp
	$(CXX) -o $(SYMROOT)/$@ \
		$(CFLAGS) \
		$^ \
		$(OBJROOT)/MallocBench-$@.o

install-parallel-%: $(CUSTOM_TARGETS)
	@mkdir -p $(INSTALLDIR)
	cp $(SYMROOT)/$(patsubst install-%,%,$@) $(INSTALLDIR)/$(patsubst install-%,%,$@)

nano-trace-replay_%: $(SRCROOT)/nano_trace_replay.c
	$(CC) \
		$(CFLAGS) $(DT_CFLAGS) $(OTHER_CFLAGS) \
		$(LDFLAGS) $(DT_LDFLAGS) $(OTHER_LDFLAGS) \
		-DTRACE_NAME="$(patsubst nano-trace-replay_%,%,$(notdir $@))" \
		-o $@ \
		$(SRCROOT)/nano_trace_replay.c

$(BATS_PLISTS): %.plist : %
	$(EXTRACTMETA) extract -i /$(INSTALLPATH)/$(notdir $<) -b $(SYMROOT)/$(notdir $<) -o $@
	@plutil -convert binary1 $@

SANITIZER_DYLIB_PATH := /usr/local/lib/sanitizers/
asan: OTHER_CFLAGS += -fsanitize=address
asan: OTHER_LDFLAGS += -Wl,-rpath -Wl,$(SANITIZER_DYLIB_PATH)

ifeq ($(Embedded),NO)
tsan: CFLAGS := $(filter-out $(ARCH_FLAGS),$(CFLAGS)) -arch x86_64 -fsanitize=thread
tsan: OTHER_LDFLAGS += -Wl,-rpath -Wl,$(SANITIZER_DYLIB_PATH)
else
EXCLUDED_SOURCES += tsan.c
endif

madvise: OTHER_CFLAGS += -I../src
stack_logging_test: OTHER_CFLAGS += -I../private
radix_tree_test: OTHER_CFLAGS += -I../src -framework Foundation

.DEFAULT_GOAL := all

include $(DEVELOPER_DIR)/AppleInternal/Makefiles/darwintest/Makefile.targets
