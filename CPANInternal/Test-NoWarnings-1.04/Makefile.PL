use 5.006;
use strict;
use ExtUtils::MakeMaker ();

WriteMakefile(
	NAME             => 'Test::NoWarnings',
	ABSTRACT         => "Make sure you didn't emit any warnings while testing",
	VERSION_FROM     => 'lib/Test/NoWarnings.pm',
	AUTHOR           => 'Fergal Daly <fergal@esatclear.ie>',
	LICENSE          => 'lgpl',
	MIN_PERL_VERSION => '5.006',
	BUILD_REQUIRES   => {
		'Test::More'    => '0.47',
		'Test::Tester'  => '0.107',
	},
	PREREQ_PM => {
		'Test::Builder' => '0.86',
	},
	META_MERGE => {
		resources => {
			repository => 'http://svn.ali.as/cpan/trunk/Test-NoWarnings',
		},
	},
);

# Written by Alexandr Ciornii, version 0.20. Added by eumm-upgrade.
# Modified by ADAMK to make it look a little cleaner.
sub WriteMakefile {
	my %params = @_;
	my $eumm_version = eval $ExtUtils::MakeMaker::VERSION;
	die "License not specified" unless exists $params{LICENSE};
	if ( $params{BUILD_REQUIRES} and $eumm_version < 6.5503 ) {
		#EUMM 6.5502 has problems with BUILD_REQUIRES
		$params{PREREQ_PM}={ %{$params{PREREQ_PM} || {}} , %{$params{BUILD_REQUIRES}} };
		delete $params{BUILD_REQUIRES};
	}
	delete $params{MIN_PERL_VERSION} if $eumm_version < 6.48;
	delete $params{META_MERGE}       if $eumm_version < 6.46;
	delete $params{LICENSE}          if $eumm_version < 6.31;

	ExtUtils::MakeMaker::WriteMakefile(%params);
}
