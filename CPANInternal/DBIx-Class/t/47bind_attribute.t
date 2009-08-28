use strict;
use warnings;

use Test::More;
use lib qw(t/lib);
use DBICTest;

my $schema = DBICTest->init_schema;

BEGIN {
    eval "use DBD::SQLite";
    plan $@
        ? ( skip_all => 'needs DBD::SQLite for testing' )
        : ( tests => 7 );
}

### $schema->storage->debug(1);

my $where_bind = {
    where => \'name like ?',
    bind  => [ 'Cat%' ],
};

my $rs;

TODO: {
    local $TODO = 'bind args order needs fixing (semifor)';

    # First, the simple cases...
    $rs = $schema->resultset('Artist')->search(
            { artistid => 1 },
            $where_bind,
    );

    is ( $rs->count, 1, 'where/bind combined' );

    $rs= $schema->resultset('Artist')->search({}, $where_bind)
        ->search({ artistid => 1});

    is ( $rs->count, 1, 'where/bind first' );
            
    $rs = $schema->resultset('Artist')->search({ artistid => 1})
        ->search({}, $where_bind);

    is ( $rs->count, 1, 'where/bind last' );
}

# More complex cases, based primarily on the Cookbook
# "Arbitrary SQL through a custom ResultSource" technique,
# which seems to be the only place the bind attribute is
# documented.  Breaking this technique probably breaks existing
# application code.
my $source = DBICTest::Artist->result_source_instance;
my $new_source = $source->new($source);
$new_source->source_name('Complex');

$new_source->name(\<<'');
( select a.*, cd.cdid as cdid, cd.title as title, cd.year as year 
  from artist a
  join cd on cd.artist=a.artistid
  where cd.year=?)

$schema->register_source('Complex' => $new_source);

$rs = $schema->resultset('Complex')->search({}, { bind => [ 1999 ] });
is ( $rs->count, 1, 'cookbook arbitrary sql example' );

$rs = $schema->resultset('Complex')->search({ 'artistid' => 1 }, { bind => [ 1999 ] });
is ( $rs->count, 1, '...coobook + search condition' );

$rs = $schema->resultset('Complex')->search({}, { bind => [ 1999 ] })
    ->search({ 'artistid' => 1 });
is ( $rs->count, 1, '...cookbook (bind first) + chained search' );

TODO: {
    local $TODO = 'bind args order needs fixing (semifor)';
    $rs = $schema->resultset('Complex')->search({}, { bind => [ 1999 ] })
        ->search({ 'artistid' => 1 }, {
            where => \'title like ?',
            bind => [ 'Spoon%' ] });
    is ( $rs->count, 1, '...cookbook + chained search with extra bind' );
}
