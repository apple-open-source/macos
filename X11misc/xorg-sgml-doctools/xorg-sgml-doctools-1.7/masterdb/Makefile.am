
master_PRE = masterdb.xml
sgmldbs_DATA =				\
	$(master_PRE:xml=html.xml)	\
	$(master_PRE:xml=pdf.xml)	\
	$(master_PRE:xml=ps.xml)	\
	$(master_PRE:xml=txt.xml)

EXTRA_DIST = $(master_PRE)
CLEANFILES = $(sgmldbs_DATA)

%.html.xml: %.xml
	$(AM_V_GEN)$(SED) -e 's|__ext__|html|g' \
	-e 's|__db__|html|g' < $< > $@

%.txt.xml: %.xml
	$(AM_V_GEN)$(SED) -e 's|__ext__|txt|g' \
	-e 's|__db__|html|g' < $< > $@

%.pdf.xml: %.xml
	$(AM_V_GEN)$(SED) -e 's|__ext__|pdf|g' \
	-e 's|__db__|fo|g' < $< > $@

%.ps.xml: %.xml
	$(AM_V_GEN)$(SED) -e 's|__ext__|ps|g' \
	-e 's|__db__|fo|g' < $< > $@
