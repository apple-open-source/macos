package DBIx::Class::Schema::Loader::RelBuilder;

use strict;
use warnings;
use Carp::Clan qw/^DBIx::Class/;
use Lingua::EN::Inflect ();
use Lingua::EN::Inflect::Number ();

=head1 NAME

DBIx::Class::Schema::Loader::RelBuilder - Builds relationships for DBIx::Class::Schema::Loader

=head1 SYNOPSIS

See L<DBIx::Class::Schema::Loader>

=head1 DESCRIPTION

This class builds relationships for L<DBIx::Class::Schema::Loader>.  This
is module is not (yet) for external use.

=head1 METHODS

=head2 new

Arguments: schema_class (scalar), fk_info (hashref), inflect_plural, inflect_singular

C<$schema_class> should be a schema class name, where the source
classes have already been set up and registered.  Column info, primary
key, and unique constraints will be drawn from this schema for all
of the existing source monikers.

The fk_info hashref's contents should take the form:

  {
      TableMoniker => [
          {
              local_columns => [ 'col2', 'col3' ],
              remote_columns => [ 'col5', 'col7' ],
              remote_moniker => 'AnotherTableMoniker',
          },
          # ...
      ],
      AnotherTableMoniker => [
          # ...
      ],
      # ...
  }

Options inflect_plural and inflect_singular are optional, and are better documented
in L<DBIx::Class::Schema::Loader::Base>.

=head2 generate_code

This method will return the generated relationships as a hashref per table moniker,
containing an arrayref of code strings which can be "eval"-ed in the context of
the source class, like:

  {
      'Some::Source::Class' => [
          "belongs_to( col1 => 'AnotherTableMoniker' )",
          "has_many( anothers => 'AnotherTableMoniker', 'col15' )",
      ],
      'Another::Source::Class' => [
          # ...
      ],
      # ...
  }

You might want to use this in building an on-disk source class file, by
adding each string to the appropriate source class file,
prefixed by C<__PACKAGE__-E<gt>>.

=cut

sub new {
    my ( $class, $schema, $fk_info, $inflect_pl, $inflect_singular ) = @_;

    my $self = {
        schema => $schema,
        fk_info => $fk_info,
        inflect_plural => $inflect_pl,
        inflect_singular => $inflect_singular,
    };

    bless $self => $class;

    $self;
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

    return $self->{legacy_default_inflections}
        ? Lingua::EN::Inflect::PL($relname)
        : Lingua::EN::Inflect::Number::to_PL($relname);
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

    return $self->{legacy_default_inflections}
        ? $relname
        : Lingua::EN::Inflect::Number::to_S($relname);
}

sub generate_code {
    my $self = shift;

    my $all_code = {};

    foreach my $local_moniker (keys %{$self->{fk_info}}) {
        my $local_table = $self->{schema}->source($local_moniker)->from;
        my $local_class = $self->{schema}->class($local_moniker);
        my $rels = $self->{fk_info}->{$local_moniker};
        
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

            # If more than one rel between this pair of tables, use the
            #  local col name(s) as the relname in the foreign source, instead
            #  of the local table name.
            my $local_relname;
            if($counters{$remote_moniker} > 1) {
                $local_relname = $self->_inflect_plural(
                    lc($local_table) . q{_} . join(q{_}, @$local_cols)
                );
            } else {
                $local_relname = $self->_inflect_plural(lc $local_table);
            }

            # for single-column case, set the relname to the column name,
            # to make filter accessors work
            my $remote_relname;
            if(scalar keys %cond == 1) {
                my ($col) = keys %cond;
                $remote_relname = $self->_inflect_singular($cond{$col});
            }
            else {
                $remote_relname = $self->_inflect_singular(lc $remote_table);
            }

            my %rev_cond = reverse %cond;

            for (keys %rev_cond) {
                $rev_cond{"foreign.$_"} = "self.".$rev_cond{$_};
                delete $rev_cond{$_};
            }

            push(@{$all_code->{$local_class}},
                { method => 'belongs_to',
                  args => [ $remote_relname,
                            $remote_class,
                            \%cond,
                  ],
                }
            );

            push(@{$all_code->{$remote_class}},
                { method => 'has_many',
                  args => [ $local_relname,
                            $local_class,
                            \%rev_cond,
                  ],
                }
            );
        }
    }

    return $all_code;
}

1;
