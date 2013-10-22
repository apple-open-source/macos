# 
# The indentation of this file is poor because this file may be used to
# test perltidy after installation. For example, try
#
#    perltidy -lp Makefile.PL
#
# which will create a reformatted version as Makefile.PL.tdy.
#
use ExtUtils::MakeMaker;
WriteMakefile(
NAME  => "Perl::Tidy",
VERSION_FROM => "lib/Perl/Tidy.pm",
(
  $] >= 5.005
? (
  ABSTRACT => 'indent and reformat perl scripts',
AUTHOR => 'Steve Hancock <perltidy@perltidy.sourceforge.net>'
  )
: ()
),
EXE_FILES => ['bin/perltidy'],
dist  => { COMPRESS => 'gzip', SUFFIX => 'gz' },
);
