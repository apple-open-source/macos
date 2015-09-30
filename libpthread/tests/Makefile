SOURCES := $(wildcard *.c)
TARGETS := $(patsubst %.c,%,$(SOURCES))
TESTS   := $(patsubst %,test-%,$(TARGETS))

CFLAGS := -I$(SDKROOT)/System/Library/Frameworks/System.framework/PrivateHeaders

all: $(TARGETS)

test check: $(TESTS)

$(TESTS): test-%: %
	@echo "[TEST] $<"
	@$(TEST_ENV) ./$<
	@echo "[END] $<"
	@echo

clean:
	rm -f $(TARGETS)

.PHONY: all test check clean $(TESTS)
