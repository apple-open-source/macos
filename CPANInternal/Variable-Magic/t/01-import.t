#!perl -T

use strict;
use warnings;

use Test::More tests => 2 * 19;

require Variable::Magic;

my %syms = (
 wizard   => undef,
 cast     => '\[$@%&*]$@',
 getdata  => '\[$@%&*]$',
 dispell  => '\[$@%&*]$',
 map { $_ => '' } qw/
  MGf_COPY MGf_DUP MGf_LOCAL VMG_UVAR
  VMG_COMPAT_ARRAY_PUSH_NOLEN VMG_COMPAT_ARRAY_PUSH_NOLEN_VOID
  VMG_COMPAT_ARRAY_UNSHIFT_NOLEN_VOID
  VMG_COMPAT_ARRAY_UNDEF_CLEAR
  VMG_COMPAT_SCALAR_LENGTH_NOLEN
  VMG_COMPAT_GLOB_GET
  VMG_PERL_PATCHLEVEL
  VMG_THREADSAFE VMG_FORKSAFE
  VMG_OP_INFO_NAME VMG_OP_INFO_OBJECT
 /
);

for (sort keys %syms) {
 eval { Variable::Magic->import($_) };
 is $@,            '',        "import $_";
 is prototype($_), $syms{$_}, "prototype $_";
}
