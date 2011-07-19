# $Id: 07magic.t,v 1.8 2007/04/20 05:40:48 ray Exp $

use strict;

use Clone;
use Test::More tests => 3;

SKIP: {
  eval "use Data::Dumper";
  skip "Data::Dumper not installed", 1 if $@;

  SKIP: {
    eval "use Scalar::Util qw( weaken )";
    skip "Scalar::Util not installed", 1 if $@;
  
    my $x = { a => "worked\n" }; 
    my $y = $x;
    weaken($y);
    my $z = Clone::clone($x);
    ok( Dumper($x) eq Dumper($z), "Cloned weak reference");
  }

  ## RT 21859: Clone segfault (isolated example)
  SKIP: {
    my $string = "HDDR-WD-250JS";
    eval {
      use utf8;
      utf8::upgrade($string);
    };
    skip $@, 1 if $@;
    $string = sprintf ('<<bg_color=%s>>%s<</bg_color>>%s',
          '#EA0',
          substr ($string, 0, 4),
          substr ($string, 4),
        );
    my $z = Clone::clone($string);
    ok( Dumper($string) eq Dumper($z), "Cloned magic utf8");
  }
}

SKIP: {
  eval "use Taint::Runtime qw(enable taint_env)";
  skip "Taint::Runtime not installed", 1 if $@;
  taint_env();
  my $x = "";
  for (keys %ENV)
  {
    $x = $ENV{$_};
    last if ( $x && length($x) > 0 );
  }
  my $y = Clone::clone($x);
  ## ok(Clone::clone($tainted), "Tainted input");
  ok( Dumper($x) eq Dumper($y), "Tainted input");
}

