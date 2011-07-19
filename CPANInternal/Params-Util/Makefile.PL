# This is intended to be equivalent to the Module::Install version below

use 5.00503;
use strict;
use Config;
use ExtUtils::MakeMaker;

# Should we build the XS version?
my $make_xs = undef;
foreach ( @ARGV ) {
	/^-pm/ and $make_xs = 0;
	/^-xs/ and $make_xs = 1;
}
unless ( defined $make_xs ) {
	$make_xs = can_cc();
}

# Generate the non-XS tests if we are making the XS version
my @tests = qw{
	t/01_compile.t
	t/02_main.t
	t/03_all.t
	t/04_codelike.t
	t/05_typelike.t
	t/06_invocant.t
	t/07_handle.t
	t/08_driver.t
	t/09_insideout.t
};
if ( $make_xs ) {
	foreach my $file ( @tests ) {
		# Load the original
		local *FILE;
		local $/ = undef;
		open( FILE, "<$file" ) or die("Failed to open '$file'");
		my $buffer = <FILE>;
		close( FILE ) or die("Failed to close '$file'");

		# Convert it to a pure perl version
		$file   =~ s/0/1/;
		$buffer =~ s/0;/1;/;

		# Write the pure perl version
		open( FILE, ">$file" ) or die("Failed to open '$file'");
		print FILE $buffer;
		close( FILE ) or die("Failed to close '$file'");
	}
}

my @clean = (
	# 'test.c',
	'*.old'
);
if ( $make_xs ) {
	push @clean, @tests;
}

WriteMakefile(
	# We created our own META.yml
	NO_META       => 1,
	NAME          => 'Params::Util',
	ABSTRACT_FROM => 'lib/Params/Util.pm', 
	VERSION_FROM  => 'lib/Params/Util.pm',
	DEFINE	      => '-DPERL_EXT', 
	CONFIGURE     => sub {
		my $hash = $_[1];
		unless ( $make_xs ) {
			$hash->{XS} = {};
			$hash->{C}  = [];
		}
		return $hash;
	},
	clean         => {
		FILES => join( ' ', @clean ),
	},
	PREREQ_PM     => {
		'Scalar::Util' => '1.18',
		'Test::More'   => '0.42',
		'File::Spec'   => '0.80',
	},

	# Non-backcompatible keys
	($] >= 5.005 ? (
		AUTHOR  => 'Adam Kennedy <adamk@cpan.org>',
	) : ()),
	($ExtUtils::MakeMaker::VERSION ge '6.30_00' ? (
		LICENSE => 'perl',
	) : ()),
);





#####################################################################
# Support Functions (adapted from Module::Install)

sub can_cc {
	my $self   = shift;
	my @chunks = split(/ /, $Config::Config{cc}) or return;

	# $Config{cc} may contain args; try to find out the program part
	while ( @chunks ) {
		return can_run("@chunks") || (pop(@chunks), next);
	}

	return;
}

sub can_run {
	my ($self, $cmd) = @_;

	my $_cmd = $cmd;
	return $_cmd if (-x $_cmd or $_cmd = MM->maybe_command($_cmd));

	for my $dir ((split /$Config::Config{path_sep}/, $ENV{PATH}), '.') {
		next if $dir eq '';
		my $abs = File::Spec->catfile($dir, $_[1]);
		return $abs if (-x $abs or $abs = MM->maybe_command($abs));
	}

	return;
}
