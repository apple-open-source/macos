use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
WriteMakefile(
	      DISTNAME	=> 'Term-ReadLine-Perl',
	      NAME	=> 'Term::ReadLine',
	      VERSION_FROM => 'ReadLine/readline.pm',
	      linkext => {LINKTYPE => '' },
	      # dist => {COMPRESS=>'gzip -9f', SUFFIX=>'gz', 
	      #		     DIST_DEFAULT => 'all uutardist'},
	     );
