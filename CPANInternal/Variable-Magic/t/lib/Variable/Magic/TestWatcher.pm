package Variable::Magic::TestWatcher;

use strict;
use warnings;

use Test::More;

use Carp qw/croak/;
use Variable::Magic qw/wizard/;

use base qw/Exporter/;

our @EXPORT = qw/init_watcher watch/;

sub _types {
 my $t = shift;
 return { } unless defined $t;
 return {
  ''      => sub { +{ $t => 1 } },
  'ARRAY' => sub { my $h = { }; ++$h->{$_} for @$t; $h },
  'HASH'  => sub { +{ map { $_ => $t->{$_} } grep $t->{$_}, keys %$t } }
 }->{ref $t}->();
}

our ($wiz, $prefix, %mg);

sub init_watcher ($;$) {
 croak 'can\'t initialize twice' if defined $wiz;
 my $types = _types shift;
 $prefix   = (defined) ? "$_: " : '' for shift;
 %mg  = ();
 $wiz = eval 'wizard ' . join(', ', map {
  "$_ => sub { \$mg{$_}++;" . ($_ eq 'len' ? '$_[2]' : '0') . '}'
 } keys %$types);
 is        $@,   '',  $prefix . 'wizard() doesn\'t croak';
 is_deeply \%mg, { }, $prefix . 'wizard() doesn\'t trigger magic';
 return $wiz;
}

sub watch (&;$$) {
 my $code = shift;
 my $exp  = _types shift;
 my $desc = shift;
 my $want = wantarray;
 my @ret;
 local %mg = ();
 local $Test::Builder::Level = ($Test::Builder::Level || 0) + 1;
 if (defined $want and not $want) { # scalar context
  $ret[0] = eval { $code->() };
 } else {
  @ret = eval { $code->() };
 }
 is        $@,   '',   $prefix . $desc . ' doesn\'t croak';
 is_deeply \%mg, $exp, $prefix . $desc . ' triggers magic correctly';
 return $want ? @ret : $ret[0];
}

our $mg_end;

END {
 if (defined $wiz) {
  undef $wiz;
  $mg_end = { } unless defined $mg_end;
  is_deeply \%mg, $mg_end, $prefix . 'magic triggered at END time';
 }
}

1;
