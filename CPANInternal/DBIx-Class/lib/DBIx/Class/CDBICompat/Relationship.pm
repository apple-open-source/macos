package
    DBIx::Class::CDBICompat::Relationship;

use strict;
use warnings;
use Sub::Name ();

=head1 NAME

DBIx::Class::CDBICompat::Relationship - Emulate the Class::DBI::Relationship object returned from meta_info()

=head1 DESCRIPTION

Emulate the Class::DBI::Relationship object returned from C<meta_info()>.

=cut

my %method2key = (
    name            => 'type',
    class           => 'self_class',
    accessor        => 'accessor',
    foreign_class   => 'class',
    args            => 'args',
);

sub new {
    my($class, $args) = @_;

    return bless $args, $class;
}

for my $method (keys %method2key) {
    my $key = $method2key{$method};
    my $code = sub {
        $_[0]->{$key};
    };

    no strict 'refs';
    *{$method} = Sub::Name::subname $method, $code;
}

1;
