package
    DBIx::Class::CDBICompat::ColumnsAsHash;

use strict;
use warnings;


=head1 NAME

DBIx::Class::CDBICompat::ColumnsAsHash - Emulates the behavior of Class::DBI where the object can be accessed as a hash of columns.

=head1 SYNOPSIS

See DBIx::Class::CDBICompat for usage directions.

=head1 DESCRIPTION

Emulates the I<undocumnted> behavior of Class::DBI where the object can be accessed as a hash of columns.  This is often used as a performance hack.

    my $column = $row->{column};

=head2 Differences from Class::DBI

If C<DBIC_CDBICOMPAT_HASH_WARN> is true it will warn when a column is accessed as a hash key.

=cut

sub new {
    my $class = shift;

    my $new = $class->next::method(@_);

    $new->_make_columns_as_hash;

    return $new;
}

sub inflate_result {
    my $class = shift;

    my $new = $class->next::method(@_);

    $new->_make_columns_as_hash;

    return $new;
}


sub _make_columns_as_hash {
    my $self = shift;

    for my $col ($self->columns) {
        if( exists $self->{$col} ) {
            warn "Skipping mapping $col to a hash key because it exists";
        }

        tie $self->{$col}, 'DBIx::Class::CDBICompat::Tied::ColumnValue',
            $self, $col;
    }
}


package DBIx::Class::CDBICompat::Tied::ColumnValue;

use Carp;
use Scalar::Util qw(weaken isweak);


sub TIESCALAR {
    my($class, $obj, $col) = @_;
    my $self = [$obj, $col];
    weaken $self->[0];

    return bless $self, $_[0];
}

sub FETCH {
    my $self = shift;
    my($obj, $col) = @$self;

    my $class = ref $obj;
    my $id    = $obj->id;
    carp "Column '$col' of '$class/$id' was fetched as a hash"
        if $ENV{DBIC_CDBICOMPAT_HASH_WARN};

    return $obj->column_info($col)->{_inflate_info}
                ? $obj->get_inflated_column($col)
                : $obj->get_column($col);
}

sub STORE {
    my $self = shift;
    my($obj, $col) = @$self;

    my $class = ref $obj;
    my $id    = $obj->id;
    carp "Column '$col' of '$class/$id' was stored as a hash"
        if $ENV{DBIC_CDBICOMPAT_HASH_WARN};

    return $obj->column_info($col)->{_inflate_info}
                ? $obj->set_inflated_column($col => shift)
                : $obj->set_column($col => shift);
}

1;
