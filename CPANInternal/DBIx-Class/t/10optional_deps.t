use strict;
use warnings;
no warnings qw/once/;

use Test::More;
use lib qw(t/lib);
use Scalar::Util; # load before we break require()

use_ok 'DBIx::Class::Optional::Dependencies';

my $sqlt_dep = DBIx::Class::Optional::Dependencies->req_list_for ('deploy');
is_deeply (
  [ keys %$sqlt_dep ],
  [ 'SQL::Translator' ],
  'Correct deploy() dependency list',
);

# make module loading impossible, regardless of actual libpath contents
@INC = (sub { die('Optional Dep Test') } );

ok (
  ! DBIx::Class::Optional::Dependencies->req_ok_for ('deploy'),
  'deploy() deps missing',
);

like (
  DBIx::Class::Optional::Dependencies->req_missing_for ('deploy'),
  qr/^SQL::Translator \>\= \d/,
  'expected missing string contents',
);

like (
  DBIx::Class::Optional::Dependencies->req_errorlist_for ('deploy')->{'SQL::Translator'},
  qr/Optional Dep Test/,
  'custom exception found in errorlist',
);


#make it so module appears loaded
$INC{'SQL/Translator.pm'} = 1;
$SQL::Translator::VERSION = 999;

ok (
  ! DBIx::Class::Optional::Dependencies->req_ok_for ('deploy'),
  'deploy() deps missing cached properly',
);

#reset cache
%DBIx::Class::Optional::Dependencies::req_availability_cache = ();


ok (
  DBIx::Class::Optional::Dependencies->req_ok_for ('deploy'),
  'deploy() deps present',
);

is (
  DBIx::Class::Optional::Dependencies->req_missing_for ('deploy'),
  '',
  'expected null missing string',
);

is_deeply (
  DBIx::Class::Optional::Dependencies->req_errorlist_for ('deploy'),
  {},
  'expected empty errorlist',
);


done_testing;
