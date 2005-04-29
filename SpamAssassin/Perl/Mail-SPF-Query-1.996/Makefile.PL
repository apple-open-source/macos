use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
WriteMakefile(
    'NAME'		=> 'Mail::SPF::Query',
    'VERSION_FROM'	=> 'Query.pm', # finds $VERSION
    'PREREQ_PM'		=> { Net::DNS => 0.46,
                             Net::CIDR::Lite => 0.15,
			     Sys::Hostname::Long => 1.0,
                             URI => 1.00,
	 },
    ($] >= 5.005 ?    ## Add these new keywords supported since 5.005
      (ABSTRACT_FROM => 'Query.pm', # retrieve abstract from module
       AUTHOR     => 'Meng Weng Wong <mengwong+spf@pobox.com>') : ()),
    'EXE_FILES'   => [ 'spfd', 'spfquery' ],
);
