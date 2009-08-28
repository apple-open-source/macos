OBJROOT ?= $(CURDIR)

SRC=	pybsm.c
OBJ=	$(foreach a, $(SRC:.c=.pyo), $(OBJROOT)/$(a))
MOD=	$(OBJROOT)/pybsm.so

CFLAGS = $(RC_CFLAGS)
CFLAGS += -W -Werror -Wall -std=c99 -pedantic -Wcast-align -Wcast-qual
CFLAGS += -Wchar-subscripts -Winline -Wpointer-arith -Wno-discard-qual
CFLAGS += -Wshadow -Wwrite-strings -Wno-variadic-macros
CFLAGS += -Wmissing-prototypes -Wnested-externs -Wstrict-prototypes
CFLAGS += -I/System/Library/Frameworks/Python.framework/Headers

tests:  test.py $(MOD)
	PYTHONPATH=$(OBJROOT) /usr/bin/python $<

$(MOD): $(OBJ)
	$(CC)	$(CFLAGS) -bundle -undefined dynamic_lookup -o $@ $^ -lbsm

$(OBJROOT)/%.pyo: %.c $(HDR)
	$(CC) 	$(CFLAGS) -c -o $@ $<

clean:
	rm -f $(MOD) $(OBJ)
	rm -f /var/audit/*
