MAN1DIR=/usr/share/man/man1
MAN1FILES= auditreduce.1 praudit.1
MAN5DIR=/usr/share/man/man5
MAN5FILES= audit_class.5 audit_control.5 audit_event.5 audit_user.5 audit_warn.5

all: build

build:
	@echo null man build.

install:
	@echo Not installing man files automatically

clean:
	@echo null man clean.

first-install:
	mkdir -p ${MAN1DIR} ${MAN5DIR}
	cp ${MAN1FILES} ${MAN1DIR}
	cp ${MAN5FILES} ${MAN5DIR}
