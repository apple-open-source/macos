use strict;
use warnings;

use inc::Module::Install;

name				'DateTime-Format-W3CDTF';
all_from			'lib/DateTime/Format/W3CDTF.pm';
author				'Gregory Todd Williams <gwilliams@cpan.org>';
license				'perl';

build_requires		'Test::More'				=> 0.61;

requires			'DateTime'	=> 0;

resources(
	'homepage'   => "http://search.cpan.org/dist/DateTime-Format-W3CDTF/",
	'repository' => "http://github.com/kasei/datetime-format-w3cdtf",
	'bugtracker' => "http://rt.cpan.org/NoAuth/Bugs.html?Dist=DateTime-Format-W3CDTF",
);

sign;

WriteAll;
