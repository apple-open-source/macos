#!/usr/bin/perl

use strict;
use warnings;

use Test::More qw(no_plan);
use Test::Warn;


warnings_exist {
  warn "warn_1";
  warn "warn_2";
} [qr/warn_1/];

warnings_exist {
  warn "warn_1";
  warn "warn_2";
} [qr/warn_1/,qr/warn_2/];

warnings_exist {
  warn "warn_2";
} [qr/warn_1/];

warnings_exist {
  my $a;
  $b=$a+1;
  warn "warn_2";
} ['uninitialized'];

warnings_exist {
  warn "warn_2";
} ['uninitialized'];

warnings_exist {
  my $a;
  $b=$a+1;
  warn "warn_2";
} [qr/warn_2/];

