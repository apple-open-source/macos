#!/usr/bin/perl -w

use strict;

use ExtUtils::MakeMaker;

WriteMakefile(
	NAME         => 'Devel::GlobalDestruction',
	VERSION_FROM => 'lib/Devel/GlobalDestruction.pm',
	INSTALLDIRS  => 'site',
	SIGN         => 1,
	PL_FILES     => { },
	PREREQ_PM    => {
		'Sub::Exporter' => 0,
		'Scope::Guard' => 0,
	},
);

