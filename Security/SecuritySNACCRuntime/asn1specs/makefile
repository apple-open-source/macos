# .../asn1specs/makefile

include ../makehead

TOP	= ..

ASN1FILES = \
	rfc1155-smi.asn1	\
	rfc1157-snmp.asn1	\
	rfc1213-mib2.asn1	\
	any.asn1		\
	asn1module.asn1		\
	err-test.asn1		\
	ex1.asn1		\
	p-rec.asn1		\
	tbl.asn1		\
	asn-useful.asn1

DISTFILES = \
	makefile	\
	$(ASN1FILES)

#-------------------------------------------------------------------------------

.PHONY:	depend install distfiles clean clobber

install-sh:
		ln $(TOP)/install-sh $@

$(incdir)/snacc/asn1:
		$(TOP)/mkinstalldirs $@

install::	$(ASN1FILES) install-sh $(incdir)/snacc/asn1

install::
		for a in $(ASN1FILES); do $(INSTALL_DATA) $$a $(incdir)/snacc/asn1/; done

gen-distfiles::	$(DISTFILES)

distfiles::
		@echo $(DISTFILES)

# dummies:
depend clean::

clobber::
		$(RM) install-sh
