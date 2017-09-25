PROJECT := libmalloc
TEST_DIR := tests/

DEVELOPER_DIR ?= /Applications/Xcode.app/Contents/Developer/

include $(DEVELOPER_DIR)/AppleInternal/Makefiles/darwintest/Makefile.common

MALLOCBENCH_SOURCE := $(wildcard MallocBench/*.cpp)
EXCLUDED_SOURCES := MallocBench.cpp
CXX := $(shell $(XCRUN) -sdk "$(TARGETSDK)" -find clang++)

OTHER_CFLAGS += \
	-DDARWINTEST \
	-DOS_UNFAIR_LOCK_INLINE=1 \
	-lCrashReporterClient \
	-I$(SDKROOT)/System/Library/Frameworks/System.framework/PrivateHeaders \
	-I$(SRCROOT)/../include/malloc

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

OTHER_TEST_TARGETS = \
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

ASAN_DYLIB_PATH := /usr/local/lib/sanitizers/
asan: OTHER_CFLAGS += -fsanitize=address
asan: OTHER_LDFLAGS += -Wl,-rpath -Wl,$(ASAN_DYLIB_PATH)

madvise: OTHER_CFLAGS += -I../src
stack_logging_test: OTHER_CFLAGS += -I../private
radix_tree_test: OTHER_CFLAGS += -I../src -framework Foundation

include $(DEVELOPER_DIR)/AppleInternal/Makefiles/darwintest/Makefile.targets
