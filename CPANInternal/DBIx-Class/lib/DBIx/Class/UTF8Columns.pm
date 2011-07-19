package DBIx::Class::UTF8Columns;
use strict;
use warnings;
use base qw/DBIx::Class/;

__PACKAGE__->mk_classdata( '_utf8_columns' );

=head1 NAME

DBIx::Class::UTF8Columns - Force UTF8 (Unicode) flag on columns

=head1 SYNOPSIS

    package Artist;
    use base 'DBIx::Class::Core';

    __PACKAGE__->load_components(qw/UTF8Columns/);
    __PACKAGE__->utf8_columns(qw/name description/);

    # then belows return strings with utf8 flag
    $artist->name;
    $artist->get_column('description');

=head1 DESCRIPTION

This module allows you to get columns data that have utf8 (Unicode) flag.

=head2 Warning

Note that this module overloads L<DBIx::Class::Row/store_column> in a way
that may prevent other components overloading the same method from working
correctly. This component must be the last one before L<DBIx::Class::Row>
(which is provided by L<DBIx::Class::Core>). DBIx::Class will detect such
incorrect component order and issue an appropriate warning, advising which
components need to be loaded differently.

=head1 SEE ALSO

L<Template::Stash::ForceUTF8>, L<DBIx::Class::UUIDColumns>.

=head1 METHODS

=head2 utf8_columns

=cut

sub utf8_columns {
    my $self = shift;
    if (@_) {
        foreach my $col (@_) {
            $self->throw_exception("column $col doesn't exist")
                unless $self->has_column($col);
        }
        return $self->_utf8_columns({ map { $_ => 1 } @_ });
    } else {
        return $self->_utf8_columns;
    }
}

=head1 EXTENDED METHODS

=head2 get_column

=cut

sub get_column {
    my ( $self, $column ) = @_;
    my $value = $self->next::method($column);

    utf8::decode($value) if (
      defined $value and $self->_is_utf8_column($column) and ! utf8::is_utf8($value)
    );

    return $value;
}

=head2 get_columns

=cut

sub get_columns {
    my $self = shift;
    my %data = $self->next::method(@_);

    foreach my $col (keys %data) {
      utf8::decode($data{$col}) if (
        exists $data{$col} and defined $data{$col} and $self->_is_utf8_column($col) and ! utf8::is_utf8($data{$col})
      );
    }

    return %data;
}

=head2 store_column

=cut

sub store_column {
    my ( $self, $column, $value ) = @_;

    # the dirtyness comparison must happen on the non-encoded value
    my $copy;

    if ( defined $value and $self->_is_utf8_column($column) and utf8::is_utf8($value) ) {
      $copy = $value;
      utf8::encode($value);
    }

    $self->next::method( $column, $value );

    return $copy || $value;
}

# override this if you want to force everything to be encoded/decoded
sub _is_utf8_column {
  # my ($self, $col) = @_;
  return ($_[0]->utf8_columns || {})->{$_[1]};
}

=head1 AUTHORS

See L<DBIx::Class/CONTRIBUTORS>.

=head1 LICENSE

You may distribute this code under the same terms as Perl itself.

=cut

1;
