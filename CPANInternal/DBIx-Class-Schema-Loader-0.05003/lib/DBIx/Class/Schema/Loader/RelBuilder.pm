package DBIx::Class::Schema::Loader::RelBuilder;

use strict;
use warnings;
use Class::C3;
use Carp::Clan qw/^DBIx::Class/;
use Lingua::EN::Inflect::Number ();

our $VERSION = '0.05003';

=head1 NAME

DBIx::Class::Schema::Loader::RelBuilder - Builds relationships for DBIx::Class::Schema::Loader

=head1 SYNOPSIS

See L<DBIx::Class::Schema::Loader>

=head1 DESCRIPTION

This class builds relationships for L<DBIx::Class::Schema::Loader>.  This
is module is not (yet) for external use.

=head1 METHODS

=head2 new

Arguments: schema_class (scalar), inflect_plural, inflect_singular

C<$schema_class> should be a schema class name, where the source
classes have already been set up and registered.  Column info, primary
key, and unique constraints will be drawn from this schema for all
of the existing source monikers.

Options inflect_plural and inflect_singular are optional, and are better documented
in L<DBIx::Class::Schema::Loader::Base>.

=head2 generate_code

Arguments: local_moniker (scalar), fk_info (arrayref)

This generates the code for the relationships of a given table.

C<local_moniker> is the moniker name of the table which had the REFERENCES
statements.  The fk_info arrayref's contents should take the form:

    [
        {
            local_columns => [ 'col2', 'col3' ],
            remote_columns => [ 'col5', 'col7' ],
            remote_moniker => 'AnotherTableMoniker',
        },
        {
            local_columns => [ 'col1', 'col4' ],
            remote_columns => [ 'col1', 'col2' ],
            remote_moniker => 'YetAnotherTableMoniker',
        },
        # ...
    ],

This method will return the generated relationships as a hashref keyed on the
class names.  The values are arrayrefs of hashes containing method name and
arguments, like so:

  {
      'Some::Source::Class' => [
          { method => 'belongs_to', arguments => [ 'col1', 'Another::Source::Class' ],
          { method => 'has_many', arguments => [ 'anothers', 'Yet::Another::Source::Class', 'col15' ],
      ],
      'Another::Source::Class' => [
          # ...
      ],
      # ...
  }

=cut

sub new {

    my ( $class, $schema, $inflect_pl, $inflect_singular, $rel_attrs ) = @_;

    my $self = {
        schema => $schema,
        inflect_plural => $inflect_pl,
        inflect_singular => $inflect_singular,
        relationship_attrs => $rel_attrs,
    };

    # validate the relationship_attrs arg
    if( defined $self->{relationship_attrs} ) {
	ref($self->{relationship_attrs}) eq 'HASH'
	    or croak "relationship_attrs must be a hashref";
    }

    return bless $self => $class;
}


# pluralize a relationship name
sub _inflect_plural {
    my ($self, $relname) = @_;

    if( ref $self->{inflect_plural} eq 'HASH' ) {
        return $self->{inflect_plural}->{$relname}
            if exists $self->{inflect_plural}->{$relname};
    }
    elsif( ref $self->{inflect_plural} eq 'CODE' ) {
        my $inflected = $self->{inflect_plural}->($relname);
        return $inflected if $inflected;
    }

    return Lingua::EN::Inflect::Number::to_PL($relname);
}

# Singularize a relationship name
sub _inflect_singular {
    my ($self, $relname) = @_;

    if( ref $self->{inflect_singular} eq 'HASH' ) {
        return $self->{inflect_singular}->{$relname}
            if exists $self->{inflect_singular}->{$relname};
    }
    elsif( ref $self->{inflect_singular} eq 'CODE' ) {
        my $inflected = $self->{inflect_singular}->($relname);
        return $inflected if $inflected;
    }

    return Lingua::EN::Inflect::Number::to_S($relname);
}

# accessor for options to be passed to each generated relationship
# type.  take single argument, the relationship type name, and returns
# either a hashref (if some options are set), or nothing
sub _relationship_attrs {
    my ( $self, $reltype ) = @_;
    my $r = $self->{relationship_attrs};
    return unless $r && ( $r->{all} || $r->{$reltype} );

    my %composite = %{ $r->{all} || {} };
    if( my $specific = $r->{$reltype} ) {
	while( my ($k,$v) = each %$specific ) {
	    $composite{$k} = $v;
	}
    }
    return \%composite;
}

sub _array_eq {
    my ($a, $b) = @_;

    return unless @$a == @$b;

    for (my $i = 0; $i < @$a; $i++) {
        return unless $a->[$i] eq $b->[$i];
    }
    return 1;
}

sub _uniq_fk_rel {
    my ($self, $local_moniker, $local_relname, $local_cols, $uniqs) = @_;

    my $remote_method = 'has_many';

    # If the local columns have a UNIQUE constraint, this is a one-to-one rel
    my $local_source = $self->{schema}->source($local_moniker);
    if (_array_eq([ $local_source->primary_columns ], $local_cols) ||
            grep { _array_eq($_->[1], $local_cols) } @$uniqs) {
        $remote_method = 'might_have';
        $local_relname = $self->_inflect_singular($local_relname);
    }

    return ($remote_method, $local_relname);
}

sub _remote_attrs {
	my ($self, $local_moniker, $local_cols) = @_;

	# get our base set of attrs from _relationship_attrs, if present
	my $attrs = $self->_relationship_attrs('belongs_to') || {};

	# If the referring column is nullable, make 'belongs_to' an
	# outer join, unless explicitly set by relationship_attrs
	my $nullable = grep { $self->{schema}->source($local_moniker)->column_info($_)->{is_nullable} }
		@$local_cols;
	$attrs->{join_type} = 'LEFT'
	    if $nullable && !defined $attrs->{join_type};

	return $attrs;
}

sub _remote_relname {
    my ($self, $remote_table, $cond) = @_;

    my $remote_relname;
    # for single-column case, set the remote relname to the column
    # name, to make filter accessors work, but strip trailing _id
    if(scalar keys %{$cond} == 1) {
        my ($col) = values %{$cond};
        $col =~ s/_id$//;
        $remote_relname = $self->_inflect_singular($col);
    }
    else {
        $remote_relname = $self->_inflect_singular(lc $remote_table);
    }

    return $remote_relname;
}

sub generate_code {
    my ($self, $local_moniker, $rels, $uniqs) = @_;

    my $all_code = {};

    my $local_table = $self->{schema}->source($local_moniker)->from;
    my $local_class = $self->{schema}->class($local_moniker);
        
    my %counters;
    foreach my $rel (@$rels) {
        next if !$rel->{remote_source};
        $counters{$rel->{remote_source}}++;
    }

    foreach my $rel (@$rels) {
        next if !$rel->{remote_source};
        my $local_cols = $rel->{local_columns};
        my $remote_cols = $rel->{remote_columns};
        my $remote_moniker = $rel->{remote_source};
        my $remote_obj = $self->{schema}->source($remote_moniker);
        my $remote_class = $self->{schema}->class($remote_moniker);
        my $remote_table = $remote_obj->from;
        $remote_cols ||= [ $remote_obj->primary_columns ];

        if($#$local_cols != $#$remote_cols) {
            croak "Column count mismatch: $local_moniker (@$local_cols) "
                . "$remote_moniker (@$remote_cols)";
        }

        my %cond;
        foreach my $i (0 .. $#$local_cols) {
            $cond{$remote_cols->[$i]} = $local_cols->[$i];
        }

        my $local_relname;
        my $remote_relname = $self->_remote_relname($remote_table, \%cond);

        # If more than one rel between this pair of tables, use the local
        # col names to distinguish
        if($counters{$remote_moniker} > 1) {
            my $colnames = q{_} . join(q{_}, @$local_cols);
            $remote_relname .= $colnames if keys %cond > 1;

            my $is_singular =
              ($self->_uniq_fk_rel($local_moniker, 'dummy', $local_cols, $uniqs))[0] ne 'has_many';

            $local_relname = $self->_multi_rel_local_relname(
                $remote_class, $local_table, $local_cols, $is_singular
            );
        } else {
            $local_relname = $self->_inflect_plural(lc $local_table);
        }

        my %rev_cond = reverse %cond;

        for (keys %rev_cond) {
            $rev_cond{"foreign.$_"} = "self.".$rev_cond{$_};
            delete $rev_cond{$_};
        }

        my ($remote_method);

        ($remote_method, $local_relname) = $self->_uniq_fk_rel($local_moniker, $local_relname, $local_cols, $uniqs);

        push(@{$all_code->{$local_class}},
            { method => 'belongs_to',
              args => [ $remote_relname,
                        $remote_class,
                        \%cond,
                        $self->_remote_attrs($local_moniker, $local_cols),
              ],
            }
        );

        push(@{$all_code->{$remote_class}},
            { method => $remote_method,
              args => [ $local_relname,
                        $local_class,
                        \%rev_cond,
			$self->_relationship_attrs($remote_method),
              ],
            }
        );
    }

    return $all_code;
}

sub _multi_rel_local_relname {
    my ($self, $remote_class, $local_table, $local_cols, $is_singular) = @_;

    my $inflect = $is_singular ? '_inflect_singular' : '_inflect_plural';
    $inflect    = $self->can($inflect);

    my $colnames = q{_} . join(q{_}, @$local_cols);
    my $old_relname = #< TODO: remove me after 0.05003 release
    my $local_relname = lc($local_table) . $colnames;
    my $stripped_id = $local_relname =~ s/_id$//; #< strip off any trailing _id
    $local_relname = $self->$inflect( $local_relname );

    # TODO: remove me after 0.05003 release
    $old_relname = $self->$inflect( $old_relname );
    warn __PACKAGE__." $VERSION: warning, stripping trailing _id from ${remote_class} relation '$old_relname', renaming to '$local_relname'.  This behavior is new as of 0.05003.\n"
        if $stripped_id;

    return $local_relname;
}

=head1 AUTHOR

See L<DBIx::Class::Schema::Loader/AUTHOR> and L<DBIx::Class::Schema::Loader/CONTRIBUTORS>.

=head1 LICENSE

This library is free software; you can redistribute it and/or modify it under
the same terms as Perl itself.

=cut

1;
