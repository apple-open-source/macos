SOURCES := $(wildcard *.c)
TARGETS := $(patsubst %.c,%,$(SOURCES))
TESTS   := $(patsubst %,test-%,$(TARGETS))

include Makefile.common

CFLAGS += -I$(SDKROOT)/System/Library/Frameworks/System.framework/PrivateHeaders

%: %.c
	$(CC) -o $(BUILDDIR)/$@ $< $(CFLAGS)

all: $(TARGETS)

test check: $(TESTS)

$(TESTS): test-%: %
	@echo "[TEST] $<"
	@$(TEST_ENV) ./$<
	@echo "[END] $<"
	@echo

clean:
	rm -f $(TARGETS)

setup:
	mkdir -p $(BUILDDIR)


# B&I Targets

installhdrs:

install: setup all


.PHONY: all test check clean $(TESTS)
