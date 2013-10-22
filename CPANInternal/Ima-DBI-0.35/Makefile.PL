use ExtUtils::MakeMaker;

my $LAST_API_CHANGE = 0.08;

eval { require Ima::DBI; };
unless ($@) {    # Make sure we did find the module.
	print <<"CHANGE_WARN" if $Ima::DBI::VERSION < $LAST_API_CHANGE;

NOTE: There have been API changes between this version and any older
than version $LAST_API_CHANGE!  Please read the Changes file if you
are upgrading from a version of Ima::DBI older than $LAST_API_CHANGE.

CHANGE_WARN
}

WriteMakefile(
	NAME         => 'Ima::DBI',
	VERSION_FROM => 'lib/Ima/DBI.pm',
	PREREQ_PM    => {
		'DBI'                      => 1.20,
		'Class::Data::Inheritable' => 0.02,
		'DBIx::ContextualFetch'    => 1.00,
		'Test::More'               => 0.18,
	},
	(
		$] > 5.005
		? ()
		: (
			ABSTRACT_FROM => 'lib/Ima/DBI.pm',
			AUTHOR        => 'Tony Bowden <kasei@tmtm.com>',
		)
	),

);
