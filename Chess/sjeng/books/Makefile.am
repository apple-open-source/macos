noinst_DIST = bug.opn suicide.opn losers.opn
noinst_DATA = nbook.db zbook.db

nbook.db: nbook.pgn
	./build-book normal $<

zbook.db: zhbook.pgn
	./build-book crazyhouse $<

#
# Ignore whatever other target ProjectBuilder throws at us
#
.DEFAULT:
	@echo "Ignoring request for unknown target" $@
