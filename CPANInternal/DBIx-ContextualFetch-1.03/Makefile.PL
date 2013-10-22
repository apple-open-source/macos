use ExtUtils::MakeMaker;
WriteMakefile(
	AUTHOR        => 'Tony Bowden <kasei@tmtm.com>',
	NAME          => 'DBIx::ContextualFetch',
	ABSTRACT_FROM => 'lib/DBIx/ContextualFetch.pm',
	VERSION_FROM  => 'lib/DBIx/ContextualFetch.pm',
	PREREQ_PM     => {
		'DBI'   => 1.35,
		'Test::More'   => 0.11,
	},
);
