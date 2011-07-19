use strict;
use warnings;  

use Test::More tests => 2;
use lib qw(t/lib);
use DBICTest;
use DBICTest::Schema;
use DBICTest::Schema::Artist;

DBICTest::Schema::Artist->source_name('MyArtist');
DBICTest::Schema->register_class('FooA', 'DBICTest::Schema::Artist');

my $schema = DBICTest->init_schema();

my $a = $schema->resultset('FooA')->search;
is($a->count, 3, 'have 3 artists');
is($schema->class('FooA'), 'DBICTest::FooA', 'Correct artist class');

# clean up
DBICTest::Schema->_unregister_source('FooA');
