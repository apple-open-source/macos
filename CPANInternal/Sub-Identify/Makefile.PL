use ExtUtils::MakeMaker;

WriteMakefile(
    NAME	    => "Sub::Identify",
    VERSION_FROM    => "lib/Sub/Identify.pm",
    LICENSE	    => 'perl',
    PREREQ_PM	    => {
	'Test::More'		=> 0,
    },
);
