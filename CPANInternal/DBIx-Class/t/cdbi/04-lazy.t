#!/usr/bin/perl -w

use strict;
use Test::More;
use Test::Warn;

#----------------------------------------------------------------------
# Test lazy loading
#----------------------------------------------------------------------

BEGIN {
  eval "use DBIx::Class::CDBICompat;";
  plan $@ 
    ? (skip_all => 'Class::Trigger and DBIx::ContextualFetch required')
    : (tests => 36)
  ;
}

INIT {
  use lib 't/cdbi/testlib';
  use Lazy;
}

is_deeply [ Lazy->columns('Primary') ],        [qw/this/],      "Pri";
is_deeply [ sort Lazy->columns('Essential') ], [qw/opop this/], "Essential";
is_deeply [ sort Lazy->columns('things') ],    [qw/that this/], "things";
is_deeply [ sort Lazy->columns('horizon') ],   [qw/eep orp/],   "horizon";
is_deeply [ sort Lazy->columns('vertical') ],  [qw/oop opop/],  "vertical";
is_deeply [ sort Lazy->columns('All') ], [qw/eep oop opop orp that this/], "All";

{
  my @groups = Lazy->__grouper->groups_for(Lazy->find_column('this'));
  is_deeply [ sort @groups ], [sort qw/things Essential Primary/], "this (@groups)";
}

{
  my @groups = Lazy->__grouper->groups_for(Lazy->find_column('that'));
  is_deeply \@groups, [qw/things/], "that (@groups)";
}

Lazy->create({ this => 1, that => 2, oop => 3, opop => 4, eep => 5 });

ok(my $obj = Lazy->retrieve(1), 'Retrieve by Primary');
ok($obj->_attribute_exists('this'),  "Gets primary");
ok($obj->_attribute_exists('opop'),  "Gets other essential");
ok(!$obj->_attribute_exists('that'), "But other things");
ok(!$obj->_attribute_exists('eep'),  " nor eep");
ok(!$obj->_attribute_exists('orp'),  " nor orp");
ok(!$obj->_attribute_exists('oop'),  " nor oop");

ok(my $val = $obj->eep, 'Fetch eep');
ok($obj->_attribute_exists('orp'),   'Gets orp too');
ok(!$obj->_attribute_exists('oop'),  'But still not oop');
ok(!$obj->_attribute_exists('that'), 'nor that');

{
  Lazy->columns(All => qw/this that eep orp oop opop/);
  ok(my $obj = Lazy->retrieve(1), 'Retrieve by Primary');
  ok !$obj->_attribute_exists('oop'), " Don't have oop";
  my $null = $obj->eep;
  ok !$obj->_attribute_exists('oop'),
    " Don't have oop - even after getting eep";
}

# Test contructor breaking.

eval {    # Need a hashref
  Lazy->create(this => 10, that => 20, oop => 30, opop => 40, eep => 50);
};
ok($@, $@);

eval {    # False column
  Lazy->create({ this => 10, that => 20, theother => 30 });
};
ok($@, $@);

eval {    # Multiple false columns
  Lazy->create({ this => 10, that => 20, theother => 30, andanother => 40 });
};
ok($@, $@);


warning_like {
    Lazy->columns( TEMP => qw(that) );
} qr/Declaring column that as TEMP but it already exists/;

# Test that create() and update() throws out columns that changed
{
    my $l = Lazy->create({
        this => 99,
        that => 2,
        oop  => 3,
        opop => 4,
    });

    ok $l->db_Main->do(qq{
        UPDATE @{[ $l->table ]}
        SET    oop  = ?
        WHERE  this = ?
    }, undef, 87, $l->this);

    is $l->oop, 87;

    $l->oop(32);
    $l->update;

    ok $l->db_Main->do(qq{
        UPDATE @{[ $l->table ]}
        SET    oop  = ?
        WHERE  this = ?
    }, undef, 23, $l->this);

    is $l->oop, 23;
    
    $l->delete;
}


# Now again for inflated values
SKIP: {
    skip "Requires Date::Simple 3.03", 5 unless eval "use Date::Simple 3.03; 1; ";
    Lazy->has_a(
        orp     => 'Date::Simple',
        inflate => sub { Date::Simple->new($_[0] . '-01-01') },
        deflate => 'format'
    );
    
    my $l = Lazy->create({
        this => 89,
        that => 2,
        orp  => 1998,
    });

    ok $l->db_Main->do(qq{
        UPDATE @{[ $l->table ]}
        SET    orp  = ?
        WHERE  this = ?
    }, undef, 1987, $l->this);
    
    is $l->orp, '1987-01-01';

    $l->orp(2007);
    is $l->orp, '2007-01-01';   # make sure it's inflated
    $l->update;
    
    ok $l->db_Main->do(qq{
        UPDATE @{[ $l->table ]}
        SET    orp  = ?
        WHERE  this = ?
    }, undef, 1942, $l->this);

    is $l->orp, '1942-01-01';
    
    $l->delete;
}


# Test that a deleted object works
{
    Lazy->search()->delete_all;
    my $l = Lazy->create({
        this => 99,
        that => 2,
        oop  => 3,
        opop => 4,
    });
    
    # Delete the object without it knowing.
    Lazy->db_Main->do(qq[
        DELETE
        FROM   @{[ Lazy->table ]}
        WHERE  this = 99
    ]);
    
    $l->eep;
    
    # The problem was when an object had an inflated object
    # loaded.  _flesh() would set _column_data to undef and
    # get_column() would think nothing was there.
    # I'm too lazy to set up the proper inflation test.
    ok !exists $l->{_column_data}{orp};
}
