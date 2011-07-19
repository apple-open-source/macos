use strict;
use Test::More;

BEGIN {
  eval "use DBIx::Class::CDBICompat;";
  plan skip_all => "Class::Trigger and DBIx::ContextualFetch required: $@"
    if $@;

  plan skip_all => "Time::Piece required for this test"
    unless eval { require Time::Piece };

  plan tests => 12;
}

use Test::Warn;

package Temp::DBI;
use base qw(DBIx::Class::CDBICompat);
Temp::DBI->columns(All => qw(id date));

my $strptime_inflate = sub { 
    Time::Piece->strptime(shift, "%Y-%m-%d") 
};
Temp::DBI->has_a(
    date => 'Time::Piece',
    inflate => $strptime_inflate
);


package Temp::Person;
use base 'Temp::DBI';
Temp::Person->table('people');
Temp::Person->columns(Info => qw(name pet));
Temp::Person->has_a( pet => 'Temp::Pet' );

package Temp::Pet;
use base 'Temp::DBI';
Temp::Pet->table('pets');
Temp::Pet->columns(Info => qw(name));
Temp::Pet->has_many(owners => 'Temp::Person');

package main;

{
    my $pn_meta = Temp::Person->meta_info('has_a');
    is_deeply [sort keys %$pn_meta], [qw/date pet/], "Person has Date and Pet";
}

{
    my $pt_meta = Temp::Pet->meta_info;
    is_deeply [keys %{$pt_meta->{has_a}}], [qw/date/], "Pet has Date";
    is_deeply [keys %{$pt_meta->{has_many}}], [qw/owners/], "And owners";
}

{
    my $pet = Temp::Person->meta_info( has_a => 'pet' );
    is $pet->class,         'Temp::Person';
    is $pet->foreign_class, 'Temp::Pet';
    is $pet->accessor,      'pet';
    is $pet->name,          'has_a';
}

{
    my $owners = Temp::Pet->meta_info( has_many => 'owners' );

    is_deeply $owners->args, {
        foreign_key     => 'pet',
        mapping         => [],
    };
}

{
    my $date = Temp::Pet->meta_info( has_a => 'date' );
    is $date->class,            'Temp::DBI';
    is $date->foreign_class,    'Time::Piece';
    is $date->accessor,         'date';
    is $date->args->{inflate},  $strptime_inflate;
}
