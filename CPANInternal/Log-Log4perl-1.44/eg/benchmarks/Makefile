
all:
	perl -I../../blib/lib -MLog::Log4perl -le 'print $$Log::Log4perl::VERSION'
	perl -I../../blib/lib ./simple

master:
	cd ../..; git checkout master; perl Makefile.PL >/dev/null; make >/dev/null

eval_free:
	cd ../..; git checkout eval_free; perl Makefile.PL >/dev/null; make >/dev/null
