check_PROGRAMS	= \
    libxml2_xml_pushparser_fuzzer \
    libxml2_xml_read_memory_fuzzer \
    libxml2_xml_regexp_compile_fuzzer \
    libxml2_xml_saxparser_fuzzer

AM_CPPFLAGS = -I$(top_builddir)/include -I$(top_srcdir)/include -I$(srcdir)/include
AM_CXXFLAGS = $(THREAD_CFLAGS) $(Z_CFLAGS)
LDADD = $(RDL_LIBS) $(STATIC_BINARIES) $(top_builddir)/.libs/libxml2.a $(THREAD_LIBS) $(Z_LIBS) $(ICONV_LIBS) $(ICU_LIBS) $(M_LIBS) $(WIN32_EXTRA_LIBADD) $(LIB_FUZZING_ENGINE)

libxml2_xml_pushparser_fuzzer_SOURCES = libxml2_xml_pushparser_fuzzer.cc
libxml2_xml_read_memory_fuzzer_SOURCES = libxml2_xml_read_memory_fuzzer.cc
libxml2_xml_regexp_compile_fuzzer_SOURCES = libxml2_xml_regexp_compile_fuzzer.cc
libxml2_xml_saxparser_fuzzer_SOURCES = libxml2_xml_saxparser_fuzzer.cc

if USE_STANDALONE_FUZZING_ENGINE
libxml2_xml_pushparser_fuzzer_SOURCES += standalone_fuzz_target_runner.cc
libxml2_xml_read_memory_fuzzer_SOURCES += standalone_fuzz_target_runner.cc
libxml2_xml_regexp_compile_fuzzer_SOURCES += standalone_fuzz_target_runner.cc
libxml2_xml_saxparser_fuzzer_SOURCES += standalone_fuzz_target_runner.cc
endif

$(check_PROGRAMS): $(top_builddir)/.libs/libxml2.a

CHECKERS=$(check_PROGRAMS:%=%_checker)
%_checker: %
	@if [ -d $<-corpus ]; then \
		echo "Running $< against corpus..."; \
		./$< -rss_limit_mb=4096 -runs=1 -timeout=60 $<-corpus; \
	fi
check: $(CHECKERS)
