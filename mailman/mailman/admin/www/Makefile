HT2HTML = $(HOME)/projects/ht2html/ht2html.py

HTSTYLE = MMGenerator
HTALLFLAGS = -f -s $(HTSTYLE)
HTROOT = .
HTFLAGS = $(HTALLFLAGS) -r $(HTROOT)
HTRELDIR = .

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

clean:
	-rm $(GENERATED_HTML)
	-rm faq.ht todo.ht
