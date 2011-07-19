package # hide from PAUSE
    DBIx::Class::CDBICompat::Constructor;

use base qw(DBIx::Class::CDBICompat::ImaDBI);

use Sub::Name();

use strict;
use warnings;

use Carp;

__PACKAGE__->set_sql(Retrieve => <<'');
SELECT __ESSENTIAL__
FROM   __TABLE__
WHERE  %s

sub add_constructor {
    my ($class, $method, $fragment) = @_;
    return croak("constructors needs a name") unless $method;

    no strict 'refs';
    my $meth = "$class\::$method";
    return carp("$method already exists in $class")
            if *$meth{CODE};

    *$meth = Sub::Name::subname $meth => sub {
            my $self = shift;
            $self->sth_to_objects($self->sql_Retrieve($fragment), \@_);
    };
}

1;
