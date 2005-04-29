use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
WriteMakefile(
    'NAME'	=> 'Unix::Syslog',
    'VERSION_FROM' => 'Syslog.pm', # finds $VERSION
    'LIBS'	=> ['-lc'],   # e.g., '-lm' 
#    'DEFINE'	=> '-DSYSLOG_NAMES',     # e.g., '-DHAVE_SOMETHING' 
    'DEFINE'	=> '',     # e.g., '-DHAVE_SOMETHING' 
    'INC'	=> '',     # e.g., '-I/usr/include/other' 
);
