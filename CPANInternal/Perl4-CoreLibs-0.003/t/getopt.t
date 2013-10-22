use warnings;
use strict;

use Test::More tests => 13;

require_ok "getopt.pl";

our($opt_f, $opt_h, $opt_i, $opt_l, $opt_o, $opt_x, $opt_y);

@ARGV = qw(-xo -f foo -y file);
&Getopt("f");
is_deeply \@ARGV, [qw(file)], "options removed from \@ARGV (1)";
ok $opt_x, "option -x set";
ok $opt_o, "option -o set";
ok $opt_y, "option -y set";
is $opt_f, "foo", "option -f set correctly";

@ARGV = qw(-hij k -- -l m -n);
&Getopt("il");
is_deeply \@ARGV, [qw(k -- -l m -n)], "options removed from \@ARGV (2)";
ok $opt_h, "option -h set";
is $opt_i, "j", "option -i set correctly";
ok !defined($opt_l), "option -l not set";

@ARGV = qw(-h -- -i j);
&Getopt("");
is_deeply \@ARGV, [qw(j)], "options removed from \@ARGV (3)";
ok $opt_h, "option -h set";
ok $opt_i, "option -i set";

1;
