HT2HTML = $(HOME)/projects/ht2html/ht2html.py

HTSTYLE = MMGenerator
HTALLFLAGS = -f -s $(HTSTYLE)
HTROOT = .
HTFLAGS = $(HTALLFLAGS) -r $(HTROOT)
HTRELDIR = .
# Use args for rsync like -a without the permission setting flag.  I want to
# keep the permissions set the way they are on the destination files, not on
# my source files.  Also add verbosity, compression, and ignoring CVS.
RSYNC_ARGS = -rltgoDCvz

SOURCES =	$(shell echo *.ht)
EXTRA_TARGETS = faq.html todo.html
TARGETS =	$(filter-out *.html,$(SOURCES:%.ht=%.html)) $(EXTRA_TARGETS)
GENERATED_HTML= $(SOURCES:.ht=.html)

.SUFFIXES:	.ht .html
.ht.html:
	$(HT2HTML) $(HTFLAGS) $(HTRELDIR)/$<

all: $(TARGETS)

faq.ht: ../../FAQ
	../bin/faq2ht.py $< $@

todo.ht: ../../TODO
	../bin/mm2do $< $@

install:
	-rsync $(RSYNC_ARGS) . www.list.org:mailman.list.org
	-rsync $(RSYNC_ARGS) . mailman.sf.net:mailman/htdocs
	-rsync $(RSYNC_ARGS) . $(HOME)/projects/mailman-gnu

clean:
	-rm $(GENERATED_HTML)
	-rm faq.ht todo.ht
