use strict;
use warnings;  

use Test::More qw(no_plan);
use lib qw(t/lib);
use DBICTest;
use DBIx::Class::ResultClass::HashRefInflator;
my $schema = DBICTest->init_schema();


# Under some versions of SQLite if the $rs is left hanging around it will lock
# So we create a scope here cos I'm lazy
{
    my $rs = $schema->resultset('CD');

    # get the defined columns
    my @dbic_cols = sort $rs->result_source->columns;

    # use the hashref inflator class as result class
    $rs->result_class('DBIx::Class::ResultClass::HashRefInflator');

    # fetch first record
    my $datahashref1 = $rs->first;

    my @hashref_cols = sort keys %$datahashref1;

    is_deeply( \@dbic_cols, \@hashref_cols, 'returned columns' );
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
  
            is (scalar @hashref_reltable, scalar @dbic_reltable, 'number of related entries');

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
    }
);
$rs_hashrefinf->result_class('DBIx::Class::ResultClass::HashRefInflator');

my @dbic        = $rs_dbic->all;
my @hashrefinf  = $rs_hashrefinf->all;

for my $index (0..scalar @hashrefinf) {
    my $dbic_obj    = $dbic[$index];
    my $datahashref = $hashrefinf[$index];

    check_cols_of($dbic_obj, $datahashref);
}
