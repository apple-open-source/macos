use strict;
use warnings;

use Test::More;
use lib qw(t/lib);
use DBICTest;
my $schema = DBICTest->init_schema();

# Under some versions of SQLite if the $rs is left hanging around it will lock
# So we create a scope here cos I'm lazy
{
    my $rs = $schema->resultset('CD')->search ({}, { order_by => 'cdid' });

    # get the defined columns
    my @dbic_cols = sort $rs->result_source->columns;

    # use the hashref inflator class as result class
    $rs->result_class('DBIx::Class::ResultClass::HashRefInflator');

    # fetch first record
    my $datahashref1 = $rs->first;

    my @hashref_cols = sort keys %$datahashref1;

    is_deeply( \@dbic_cols, \@hashref_cols, 'returned columns' );

    my $cd1 = $rs->find ({cdid => 1});
    is_deeply ( $cd1, $datahashref1, 'first/find return the same thing');

    my $cd2 = $rs->search({ cdid => 1 })->single;
    is_deeply ( $cd2, $datahashref1, 'first/search+single return the same thing');
}

sub check_cols_of {
    my ($dbic_obj, $datahashref) = @_;

    foreach my $col (keys %$datahashref) {
        # plain column
        if (not ref ($datahashref->{$col}) ) {
            is ($datahashref->{$col}, $dbic_obj->get_column($col), 'same value');
        }
        # related table entry (belongs_to)
        elsif (ref ($datahashref->{$col}) eq 'HASH') {
            check_cols_of($dbic_obj->$col, $datahashref->{$col});
        }
        # multiple related entries (has_many)
        elsif (ref ($datahashref->{$col}) eq 'ARRAY') {
            my @dbic_reltable = $dbic_obj->$col;
            my @hashref_reltable = @{$datahashref->{$col}};

            is (scalar @dbic_reltable, scalar @hashref_reltable, 'number of related entries');

            # for my $index (0..scalar @hashref_reltable) {
            for my $index (0..scalar @dbic_reltable) {
                my $dbic_reltable_obj       = $dbic_reltable[$index];
                my $hashref_reltable_entry  = $hashref_reltable[$index];

                check_cols_of($dbic_reltable_obj, $hashref_reltable_entry);
            }
        }
    }
}

# create a cd without tracks for testing empty has_many relationship
$schema->resultset('CD')->create({ title => 'Silence is golden', artist => 3, year => 2006 });

# order_by to ensure both resultsets have the rows in the same order
# also check result_class-as-an-attribute syntax
my $rs_dbic = $schema->resultset('CD')->search(undef,
    {
        prefetch    => [ qw/ artist tracks / ],
        order_by    => [ 'me.cdid', 'tracks.position' ],
    }
);
my $rs_hashrefinf = $schema->resultset('CD')->search(undef,
    {
        prefetch    => [ qw/ artist tracks / ],
        order_by    => [ 'me.cdid', 'tracks.position' ],
        result_class => 'DBIx::Class::ResultClass::HashRefInflator',
    }
);

my @dbic        = $rs_dbic->all;
my @hashrefinf  = $rs_hashrefinf->all;

for my $index (0 .. $#hashrefinf) {
    my $dbic_obj    = $dbic[$index];
    my $datahashref = $hashrefinf[$index];

    check_cols_of($dbic_obj, $datahashref);
}

# sometimes for ultra-mega-speed you want to fetch columns in esoteric ways
# check the inflator over a non-fetching join 
$rs_dbic = $schema->resultset ('Artist')->search ({ 'me.artistid' => 1}, {
    prefetch => { cds => 'tracks' },
    order_by => [qw/cds.cdid tracks.trackid/],
});

$rs_hashrefinf = $schema->resultset ('Artist')->search ({ 'me.artistid' => 1}, {
    join     => { cds => 'tracks' },
    select   => [qw/name   tracks.title      tracks.cd       /],
    as       => [qw/name   cds.tracks.title  cds.tracks.cd   /],
    order_by => [qw/cds.cdid tracks.trackid/],
    result_class => 'DBIx::Class::ResultClass::HashRefInflator',
});

@dbic = map { $_->tracks->all } ($rs_dbic->first->cds->all);
@hashrefinf  = $rs_hashrefinf->all;

is (scalar @dbic, scalar @hashrefinf, 'Equal number of tracks fetched');

for my $index (0 .. $#hashrefinf) {
    my $track       = $dbic[$index];
    my $datahashref = $hashrefinf[$index];

    is ($track->cd->artist->name, $datahashref->{name}, 'Brought back correct artist');
    for my $col (keys %{$datahashref->{cds}{tracks}}) {
        is ($track->get_column ($col), $datahashref->{cds}{tracks}{$col}, "Correct track '$col'");
    }
}

# check for same query as above but using extended columns syntax
$rs_hashrefinf = $schema->resultset ('Artist')->search ({ 'me.artistid' => 1}, {
    join     => { cds => 'tracks' },
    columns  => {name => 'name', 'cds.tracks.title' => 'tracks.title', 'cds.tracks.cd' => 'tracks.cd'},
    order_by => [qw/cds.cdid tracks.trackid/],
});
$rs_hashrefinf->result_class('DBIx::Class::ResultClass::HashRefInflator');
is_deeply [$rs_hashrefinf->all], \@hashrefinf, 'Check query using extended columns syntax';

# check nested prefetching of has_many relationships which return nothing
my $artist = $schema->resultset ('Artist')->create ({ name => 'unsuccessful artist without CDs'});
$artist->discard_changes;
my $rs_artists = $schema->resultset ('Artist')->search ({ 'me.artistid' => $artist->id}, {
    prefetch => { cds => 'tracks' }, result_class => 'DBIx::Class::ResultClass::HashRefInflator',
});
is_deeply(
  [$rs_artists->all],
  [{ $artist->get_columns, cds => [] }],
  'nested has_many prefetch without entries'
);

done_testing;

