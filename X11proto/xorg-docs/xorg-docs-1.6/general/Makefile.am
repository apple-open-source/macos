
SUBDIRS = fonts input graphics platforms

if ENABLE_DOCS

doc_sources = \
	License.xml \
	README.xml \
	ReleaseNotes.xml \
	Versions.xml

xmldir = $(docdir)
dist_xml_DATA = $(doc_sources)

include $(top_srcdir)/xmlrules.in

endif ENABLE_DOCS
