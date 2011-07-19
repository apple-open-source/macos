use strict;

use ExtUtils::MakeMaker;

require 5.005_04;

WriteMakefile(
	NAME         => 'Try::Tiny',
	VERSION_FROM => 'lib/Try/Tiny.pm',
	INSTALLDIRS  => 'site',
	SIGN         => 1,
	PL_FILES     => { },
	PREREQ_PM    => {
		'Test::More' => 0,
	},
);
