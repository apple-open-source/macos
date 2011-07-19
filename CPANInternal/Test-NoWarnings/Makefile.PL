use strict;
BEGIN {
	require 5.006;
}
use ExtUtils::MakeMaker;

WriteMakefile1(
	NAME             => 'Test::NoWarnings',
	ABSTRACT         => "Make sure you didn't emit any warnings while testing",
	VERSION_FROM     => 'lib/Test/NoWarnings.pm',
	AUTHOR           => 'Fergal Daly <fergal@esatclear.ie>',
        LICENSE          => 'lgpl',
	MIN_PERL_VERSION => '5.006',
	BUILD_REQUIRES   => {
		'Test::More'    => '0.47',
		'Test::Builder' => '0.86',
	},
	PREREQ_PM        => {
		'Test::Tester' => '0.107',
	},
	META_MERGE => {
		resources => {
			repository => 'http://svn.ali.as/cpan/trunk/Config-Tiny',
		},
	},
);

# Written by Alexandr Ciornii, version 0.20. Added by eumm-upgrade.
sub WriteMakefile1 {
	my %params = @_;
	my $eumm_version = $ExtUtils::MakeMaker::VERSION;
	$eumm_version = eval $eumm_version;
	die "License not specified" if not exists $params{LICENSE};
	if ($params{BUILD_REQUIRES} and $eumm_version < 6.5503) {
		#EUMM 6.5502 has problems with BUILD_REQUIRES
		$params{PREREQ_PM}={ %{$params{PREREQ_PM} || {}} , %{$params{BUILD_REQUIRES}} };
		delete $params{BUILD_REQUIRES};
	}
	delete $params{MIN_PERL_VERSION} if $eumm_version < 6.48;
	delete $params{META_MERGE} if $eumm_version < 6.46;
	delete $params{LICENSE} if $eumm_version < 6.31;
	delete $params{AUTHOR} if $] < 5.005;

	WriteMakefile(%params);
}
