use 5.006001;
use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
WriteMakefile(
    NAME              => 'Module::Find',
    VERSION_FROM      => 'Find.pm', # finds $VERSION
    PREREQ_PM         => {'Test::More'  =>  0,
		          'File::Find'  => 0,
		          'File::Spec'  => 0}, 
    ($] >= 5.005 ?     ## Add these new keywords supported since 5.005
      (ABSTRACT_FROM  => 'Find.pm', # retrieve abstract from module
       AUTHOR         => 'Christian Renz <crenz@web42.com>') : ()),
);
