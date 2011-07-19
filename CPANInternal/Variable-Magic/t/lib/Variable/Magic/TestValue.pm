package Variable::Magic::TestValue;

use strict;
use warnings;

use Test::More;

use Variable::Magic qw/wizard cast/;

use base qw/Exporter/;

our @EXPORT = qw/init_value value/;

our ($exp, $prefix, $desc);

sub value_cb {
 my $data = $_[1];
 return if $data->{guard};
 local $data->{guard} = 1;
 local $Test::Builder::Level = ($Test::Builder::Level || 0) + 3;
 is_deeply $_[0], $exp, $desc;
 ()
}

sub init_value (\[$@%&*]$;$) {
 my $type = $_[1];
 $prefix  = (defined) ? "$_: " : '' for $_[2];
 my $wiz  = eval "wizard data => sub { +{ guard => 0 } }, $type => \\&value_cb";
 is $@, '', $prefix . 'wizard() doesn\'t croak';
 eval { &cast($_[0], $wiz, $prefix) };
 is $@, '', $prefix . 'cast() doesn\'t croak';
 return $wiz;
}

sub value (&$;$) {
 my ($code, $_exp, $_desc) = @_;
 my $want = wantarray;
 $_desc = 'value' unless defined $desc;
 $_desc = $prefix . $_desc;
 my @ret;
 {
  local $Test::Builder::Level = ($Test::Builder::Level || 0) + 1;
  local $exp  = $_exp;
  local $desc = $_desc;
  if (defined $want and not $want) { # scalar context
   $ret[0] = eval { $code->() };
  } else {
   @ret = eval { $code->() };
  }
  is $@, '', $desc . ' doesn\'t croak';
 }
 return $want ? @ret : $ret[0];
}

1;
