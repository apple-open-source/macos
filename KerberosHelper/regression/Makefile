
all:

KDCSUPPORTS_REFERRALS = check-referrals

TESTS = 

TESTS += $(KDCSUPPORTS_REFERRALS)

check:
	@failed=; \
	count=0 ; \
	for a in $(TESTS) ; do \
		echo $$a; \
		if [ -d $$a ] ; then \
			(cd $$a && make check) || failed="$$failed $$a"; \
		else \
			make clean ; \
			if ! sh ./$$a ; then \
				failed="$$failed $$a"; \
			fi ; \
		fi ; \
		count=$(expr $count + 1) ; \
	done; \
	make clean ; \
	if [ "X$$failed" != "X" ] ; then \
		msg="= FAILED: $$failed ="; \
		exitval=1; \
	else \
		msg="= PASS: All $$count tests passed =" ; \
		exitval=0; \
	fi ; \
	echo $$msg | sed 's/./=/g' ; \
	echo $$msg ; \
	echo $$msg | sed 's/./=/g' ; \
	exit $$exitval

clean:
	rm -f *~ remove-* /var/db/krb5kdc/test/*
	for a in $(TESTS) ; do test -d $$a && (cd $$a && make clean) ; done
