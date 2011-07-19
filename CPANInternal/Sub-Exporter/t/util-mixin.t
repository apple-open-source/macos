#!perl -T
use strict;
use warnings;

use Test::More;

BEGIN {
  if (eval { require Package::Generator; 1; }) {
    plan 'no_plan';
  } else {
    plan skip_all => "the mixin exporter requires Package::Generator";
  }
}

BEGIN { use_ok("Sub::Exporter"); }

  BEGIN {
    package Thing;
    use Sub::Exporter -setup => {
      exports => {
        bar => sub { sub { 1 } },
        foo => sub {
          my ($c, $n, $a) = @_;
          sub { return $c . ($a->{arg}) }
        }
      },
    };
  }

  BEGIN {
    package Thing::Mixin;
    BEGIN { main::use_ok("Sub::Exporter::Util", 'mixin_installer'); }
    use Sub::Exporter -setup => {
      installer => mixin_installer,
      exports  => {
        bar => sub { sub { 1 } },
        foo => sub {
          my ($c, $n, $a) = @_;
          sub { return $c . ($a->{arg}) }
        }
      },
    };
  }

package Test::SubExporter::MIXIN::0;

BEGIN {
  Thing->import(
    { installer => Sub::Exporter::Util::mixin_installer },
    -all => { arg => '0' },
  );
}

package Test::SubExporter::MIXIN::1;

BEGIN {
  Thing->import(
    { installer => Sub::Exporter::Util::mixin_installer },
    -all => { arg => '1' },
  );
}

package Test::SubExporter::MIXIN::2;

BEGIN {
  Thing::Mixin->import(
    -all => { arg => '2' },
  );
}

package Test::SubExporter::MIXIN::3;

BEGIN {
  Thing::Mixin->import(
    -all => { arg => '3' },
  );
}

package main;

my @pkg = map { "Test::SubExporter::MIXIN::$_" } (0 .. 3);

for (0 .. $#pkg) {
  my $ext = $_ > 1 ? '::Mixin' : '';
  my $val = eval { $pkg[$_]->foo } || ($@ ? "died: $@" : undef);

  is(
    $val,
    "Thing$ext$_",
    "mixed in method in $pkg[$_] returns correctly"
  );

  is($pkg[$_]->bar, 1, "bar method for $pkg[$_] is ok, too");
}

my @super = map {; no strict 'refs'; [ @{$_ . "::ISA"} ] } @pkg;

for my $x (0 .. $#pkg) {
  is(@{$super[$x]}, 1, "one parent for $pkg[$x]: @{$super[$x]}");
  for my $y (($x + 1) .. $#pkg) {
    isnt("@{$super[$x]}", "@{$super[$y]}", "parent($x) ne parent($y)")
  }
}

{
  package Test::SubExporter::OBJECT;

  sub new { bless {} => shift }

  sub plugh { "plugh" }
}

package main;

my $obj_1 = Test::SubExporter::OBJECT->new;
isa_ok($obj_1, "Test::SubExporter::OBJECT", "first object");
is(ref $obj_1, "Test::SubExporter::OBJECT", "first object's ref is TSEO");

my $obj_2 = Test::SubExporter::OBJECT->new;
isa_ok($obj_2, "Test::SubExporter::OBJECT", "second object");
is(ref $obj_2, "Test::SubExporter::OBJECT", "second object's ref is TSEO");

Thing::Mixin->import({ into => $obj_1 }, qw(bar));
pass("mixin-exporting to an object didn't die");

is(
  eval { $obj_1->bar },
  1,
  "now that object has a bar method"
);

isa_ok($obj_1, "Test::SubExporter::OBJECT");
isnt(ref $obj_1, "Test::SubExporter::OBJECT", "but its actual class isnt TSEO");
