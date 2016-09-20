PROJECT := libmalloc
TEST_DIR := tests/

DEVELOPER_DIR ?= /Applications/Xcode.app/Contents/Developer/

include $(DEVELOPER_DIR)/AppleInternal/Makefiles/darwintest/Makefile.common

OTHER_CFLAGS += -DDARWINTEST
MALLOCBENCH_SOURCE := $(wildcard MallocBench/*.cpp)
CXX := $(shell $(XCRUN) -sdk "$(TARGETSDK)" -find clang++)

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

ifeq ($(Embedded),YES)
EXCLUDED_SOURCES += asan
else
CLANG_LIB_DIR := $(dir $(shell xcrun -f clang --sdk $(SDKROOT)))../lib
ASAN_DYLIB_PATH := $(dir $(subst /BuildRoot,,$(shell find $(CLANG_LIB_DIR) -name libclang_rt.asan_osx_dynamic.dylib -perm -0111 | head -n 1)))
asan: OTHER_CFLAGS += -fsanitize=address
asan: OTHER_LDFLAGS += -Wl,-rpath -Wl,$(ASAN_DYLIB_PATH)
endif

include $(DEVELOPER_DIR)/AppleInternal/Makefiles/darwintest/Makefile.targets
