{ use 5.006; }
use warnings;
use strict;

use Module::Build 0.26;

my $build = Module::Build->new(
	module_name => "Perl4::CoreLibs",
	license => "perl",
	dist_author => [
		"Brandon S. Allbery",
		"John Bazik",
		"Tom Christiansen <tchrist\@convex.com>",
		"Alexandr Ciornii (alexchorny at gmail.com)",
		"Charles Collins",
		"Joe Doupnik <JRD\@CC.USU.EDU>",
		"Marion Hakanson <hakanson\@cse.ogi.edu>",
		"Marc Horowitz <marc\@mit.edu>",
		"Waldemar Kebsch <kebsch.pad\@nixpbe.UUCP>",
		"Lee McLoughlin <lmjm\@doc.ic.ac.uk>",
		"<A.Macpherson\@bnr.co.uk>",
		"Dave Rolsky <autarch\@urth.org>",
		"Randal L. Schwartz <merlyn\@stonehenge.com>",
		"Aaron Sherman <asherman\@fmrco.com>",
		"David Sundstrom <sunds\@asictest.sc.ti.com>",
		"Wayne Thompson",
		"Larry Wall <lwall\@jpl-devvax.jpl.nasa.gov>",
		"Ilya Zakharevich",
		"Andrew Main (Zefram) <zefram\@fysh.org>",
	],
	configure_requires => {
		"Module::Build" => "0.26",
		"perl" => "5.006",
		"strict" => 0,
		"warnings" => 0,
	},
	build_requires => {
		"Config" => 0,
		"IO::Handle" => 0,
		"Module::Build" => "0.26",
		"Test::More" => 0,
		"perl" => "5.006",
		"strict" => 0,
		"warnings" => 0,
	},
	requires => {
		"File::Find" => 0,
		"Getopt::Long" => 0,
		"IPC::Open2" => 0,
		"IPC::Open3" => 0,
		"Socket" => 0,
		"Text::ParseWords" => "3.25",
		"Time::Local" => 0,
		"perl" => "5.006",
		"strict" => 0,
		"warnings" => 0,
		"warnings::register" => 0,
	},
	dynamic_config => 0,
	meta_add => { distribution_type => "module" },
	create_makefile_pl => "passthrough",
	sign => 1,
);
$build->add_build_element("pl");
$build->create_build_script;

1;
