
use strict;
use warnings;
package Test::SubExporter::Faux;

use base qw(Exporter);

our @EXPORT = qw(faux_installer exports_ok everything_ok);

sub faux_installer {
  my ($verbose) = @_;
  $verbose = 1;

  my @exported;

  my $reset = sub { @exported = () };

  my $generator = sub {
    my ($arg) = @_;
    # my ($class, $name, $generator) = @$arg{qw(class name generator)};

    return $arg;
  };

  my $installer  = sub {
    my ($arg, $to_export) = @_;

    for (my $i = 0; $i < @$to_export; $i += 2) {
      my ($as, $gen_arg) = @$to_export[ $i, $i+1 ];

      # my ($class, $generator, $name, $arg, $collection, $as, $into) = @_;
      my $everything = {
        class      => $gen_arg->{class},
        generator  => $gen_arg->{generator},
        name       => $gen_arg->{name},
        arg        => $gen_arg->{arg},
        collection => $gen_arg->{col},
        as         => $as,
        into       => $arg->{into},
      };

      push @exported, [
        $gen_arg->{name},
        ($verbose ? $everything : $gen_arg->{arg}),
      ];
    }
  };

  return ($generator, $installer, $reset, \@exported);
}

sub exports_ok {
  my ($got, $expected, $comment) = @_;
  my $got_simple = [ map { [ $_->[0], $_->[1]{arg} ] } @$got ];
  my @g = sort { ($a->[0] cmp $b->[0]) || ($a->[1] <=> $b->[1]) } @$got_simple;
  my @e = sort { ($a->[0] cmp $b->[0]) || ($a->[1] <=> $b->[1]) } @$expected;
  main::is_deeply(\@e, \@g, $comment);
}

sub everything_ok {
  my ($got, $expected, $comment) = @_;
  my @g = sort { ($a->[0] cmp $b->[0]) || ($a->[1] <=> $b->[1]) } @$got;
  my @e = sort { ($a->[0] cmp $b->[0]) || ($a->[1] <=> $b->[1]) } @$expected;
  main::is_deeply(\@e, \@g, $comment);
}

1;
