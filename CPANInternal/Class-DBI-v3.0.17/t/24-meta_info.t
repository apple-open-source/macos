use strict;
use Test::More tests => 3;

package Temp::DBI;
use base qw(Class::DBI);
Temp::DBI->columns(All => qw(id date));
Temp::DBI->has_a( date => 'Time::Piece', inflate => sub { 
	Time::Piece->strptime(shift, "%Y-%m-%d") 
});


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

my $pn_meta = Temp::Person->meta_info('has_a');
is_deeply [sort keys %$pn_meta], [qw/date pet/], "Person has Date and Pet";

my $pt_meta = Temp::Pet->meta_info;
is_deeply [keys %{$pt_meta->{has_a}}], [qw/date/], "Pet has Date";
is_deeply [keys %{$pt_meta->{has_many}}], [qw/owners/], "And owners";


