use 5.006001;
use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
WriteMakefile(
    NAME              => 'Scope::Guard',
    VERSION_FROM      => 'lib/Scope/Guard.pm', # finds $VERSION
    PREREQ_PM         => {
	'Test::More'	=> 0,
    }, # e.g., Module::Name => 1.1
    ($] >= 5.005 ?     ## Add these new keywords supported since 5.005
      (ABSTRACT_FROM  => 'lib/Scope/Guard.pm', # retrieve abstract from module
       AUTHOR         => 'chocolateboy <chocolate.boy@email.com>') : ()),
);
