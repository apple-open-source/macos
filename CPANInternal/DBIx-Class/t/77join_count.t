use strict;
use warnings;  

use Test::More;
use lib qw(t/lib);
use DBICTest;

my $schema = DBICTest->init_schema();

eval "use DBD::SQLite";
plan skip_all => 'needs DBD::SQLite for testing' if $@;
plan tests => 4;

cmp_ok($schema->resultset("CD")->count({ 'artist.name' => 'Caterwauler McCrae' },
                           { join => 'artist' }),
           '==', 3, 'Count by has_a ok');

cmp_ok($schema->resultset("CD")->count({ 'tags.tag' => 'Blue' }, { join => 'tags' }),
           '==', 4, 'Count by has_many ok');

cmp_ok($schema->resultset("CD")->count(
           { 'liner_notes.notes' => { '!=' =>  undef } },
           { join => 'liner_notes' }),
           '==', 3, 'Count by might_have ok');

cmp_ok($schema->resultset("CD")->count(
           { 'year' => { '>', 1998 }, 'tags.tag' => 'Cheesy',
               'liner_notes.notes' => { 'like' => 'Buy%' } },
           { join => [ qw/tags liner_notes/ ] } ),
           '==', 2, "Mixed count ok");

