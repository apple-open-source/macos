use warnings;
use strict;

use Test::More tests => 15;

require_ok "getopts.pl";

our($opt_f, $opt_h, $opt_i, $opt_k, $opt_o);

$opt_o = $opt_i = $opt_f = undef;
@ARGV = qw(-foi -i file);
ok &Getopts("oif:"), "Getopts succeeded (1)";
is_deeply \@ARGV, [qw(file)], "options removed from \@ARGV (1)";
ok $opt_i, "option -i set";
is $opt_f, "oi", "option -f set correctly";
ok !defined($opt_o), "option -o not set";

$opt_h = $opt_i = $opt_k = undef;
@ARGV = qw(-hij -k p -- -l m);
ok &Getopts("hi:kl"), "Getopts succeeded (2)";
is_deeply \@ARGV, [qw(p -- -l m)], "options removed from \@ARGV (2)";
ok $opt_h, "option -h set";
ok $opt_k, "option -k set";
is $opt_i, "j", "option -i set correctly";

SKIP: {
	skip "can't capture stderr", 4 unless "$]" >= 5.008;
	my $warning = "";
	close(STDERR);
	open(STDERR, ">", \$warning);
	@ARGV = qw(-h help);
	ok !Getopts("xf:y"), "Getopts fails for an illegal option";
	is $warning, "Unknown option: h\n", "user warned";
	$warning = "";
	close(STDERR);
	open(STDERR, ">", \$warning);
	@ARGV = qw(-h -- -i j);
	ok !Getopts("hiy"), "Getopts fails for an illegal option";
	is $warning, "Unknown option: -\n", "user warned";
}

1;
