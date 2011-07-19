#!perl -w

use strict;
use Test qw(plan ok);
use Data::Dump qw(dump);

plan tests => 4;

{
    package MyTie;

    sub TIE {
	my $class = shift;
	bless {}, $class;
    }

    use vars qw(*TIEHASH *TIEARRAY *TIESCALAR);
    *TIEHASH = \&TIE;
    *TIEARRAY = \&TIE;
    *TIESCALAR = \&TIE;

    sub FIRSTKEY {
	return "a";
    }

    sub NEXTKEY {
	my($self, $lastkey) = @_;
	return if $lastkey eq "d";
	return ++$lastkey;
    }

    sub FETCHSIZE {
	return 4;
    }

    sub FETCH {
	my($self, $key) = @_;
	return "v$key" if defined $key;
	return "v";
    }
}

my(%hash, @array, $scalar);
tie %hash, "MyTie";
tie @array, "MyTie";
tie $scalar, "MyTie";

ok(nl(dump(\%hash)), <<EOT);
{
  # tied MyTie
  a => "va",
  b => "vb",
  c => "vc",
  d => "vd",
}
EOT

ok(nl(dump(\@array)), <<EOT);
[
  # tied MyTie
  "v0" .. "v3",
]
EOT

ok(nl(dump($scalar)), <<EOT);
"v"
EOT

ok(nl(dump($scalar, $scalar, $scalar)), <<EOT);
("v", "v", "v")
EOT

sub nl { shift(@_) . "\n" }
