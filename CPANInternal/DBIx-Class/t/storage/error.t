use strict;
use warnings;

use Test::More;
use Test::Warn;
use Test::Exception;

use lib qw(t/lib);
use_ok( 'DBICTest' );
use_ok( 'DBICTest::Schema' );

my $schema = DBICTest->init_schema;

warnings_are ( sub {
  throws_ok (sub {
    $schema->resultset('CD')->create({ title => 'vacation in antarctica' });
  }, qr/NULL/);  # as opposed to some other error
}, [], 'No warnings besides exception' );

done_testing;
