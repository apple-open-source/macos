# vim: ts=8:sw=4:sts=4:et
package DBIx::Class::Ordered;
use strict;
use warnings;
use base qw( DBIx::Class );

=head1 NAME

DBIx::Class::Ordered - Modify the position of objects in an ordered list.

=head1 SYNOPSIS

Create a table for your ordered data.

  CREATE TABLE items (
    item_id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    position INTEGER NOT NULL
  );

Optionally, add one or more columns to specify groupings, allowing you 
to maintain independent ordered lists within one table:

  CREATE TABLE items (
    item_id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    position INTEGER NOT NULL,
    group_id INTEGER NOT NULL
  );

Or even

  CREATE TABLE items (
    item_id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    position INTEGER NOT NULL,
    group_id INTEGER NOT NULL,
    other_group_id INTEGER NOT NULL
  );

In your Schema or DB class add "Ordered" to the top 
of the component list.

  __PACKAGE__->load_components(qw( Ordered ... ));

Specify the column that stores the position number for 
each row.

  package My::Item;
  __PACKAGE__->position_column('position');

If you are using one grouping column, specify it as follows:

  __PACKAGE__->grouping_column('group_id');

Or if you have multiple grouping columns:

  __PACKAGE__->grouping_column(['group_id', 'other_group_id']);

That's it, now you can change the position of your objects.

  #!/use/bin/perl
  use My::Item;
  
  my $item = My::Item->create({ name=>'Matt S. Trout' });
  # If using grouping_column:
  my $item = My::Item->create({ name=>'Matt S. Trout', group_id=>1 });
  
  my $rs = $item->siblings();
  my @siblings = $item->siblings();
  
  my $sibling;
  $sibling = $item->first_sibling();
  $sibling = $item->last_sibling();
  $sibling = $item->previous_sibling();
  $sibling = $item->next_sibling();
  
  $item->move_previous();
  $item->move_next();
  $item->move_first();
  $item->move_last();
  $item->move_to( $position );
  $item->move_to_group( 'groupname' );
  $item->move_to_group( 'groupname', $position );
  $item->move_to_group( {group_id=>'groupname', 'other_group_id=>'othergroupname'} );
  $item->move_to_group( {group_id=>'groupname', 'other_group_id=>'othergroupname'}, $position );

=head1 DESCRIPTION

This module provides a simple interface for modifying the ordered 
position of DBIx::Class objects.

=head1 AUTO UPDATE

All of the move_* methods automatically update the rows involved in 
the query.  This is not configurable and is due to the fact that if you 
move a record it always causes other records in the list to be updated.

=head1 METHODS

=head2 position_column

  __PACKAGE__->position_column('position');

Sets and retrieves the name of the column that stores the 
positional value of each record.  Defaults to "position".

=cut

__PACKAGE__->mk_classdata( 'position_column' => 'position' );

=head2 grouping_column

  __PACKAGE__->grouping_column('group_id');

This method specifies a column to limit all queries in 
this module by.  This effectively allows you to have multiple 
ordered lists within the same table.

=cut

__PACKAGE__->mk_classdata( 'grouping_column' );

=head2 siblings

  my $rs = $item->siblings();
  my @siblings = $item->siblings();

Returns either a resultset or an array of all other objects 
excluding the one you called it on.

=cut

sub siblings {
    my( $self ) = @_;
    my $position_column = $self->position_column;
    my $rs = $self->result_source->resultset->search(
        {
            $position_column => { '!=' => $self->get_column($position_column) },
            $self->_grouping_clause(),
        },
        { order_by => $self->position_column },
    );
    return $rs->all() if (wantarray());
    return $rs;
}

=head2 first_sibling

  my $sibling = $item->first_sibling();

Returns the first sibling object, or 0 if the first sibling 
is this sibling.

=cut

sub first_sibling {
    my( $self ) = @_;
    return 0 if ($self->get_column($self->position_column())==1);

    return ($self->result_source->resultset->search(
        {
            $self->position_column => 1,
            $self->_grouping_clause(),
        },
    )->all())[0];
}

=head2 last_sibling

  my $sibling = $item->last_sibling();

Returns the last sibling, or 0 if the last sibling is this 
sibling.

=cut

sub last_sibling {
    my( $self ) = @_;
    my $count = $self->result_source->resultset->search({$self->_grouping_clause()})->count();
    return 0 if ($self->get_column($self->position_column())==$count);
    return ($self->result_source->resultset->search(
        {
            $self->position_column => $count,
            $self->_grouping_clause(),
        },
    )->all())[0];
}

=head2 previous_sibling

  my $sibling = $item->previous_sibling();

Returns the sibling that resides one position back.  Returns undef 
if the current object is the first one.

=cut

sub previous_sibling {
    my( $self ) = @_;
    my $position_column = $self->position_column;
    my $position = $self->get_column( $position_column );
    return 0 if ($position==1);
    return ($self->result_source->resultset->search(
        {
            $position_column => $position - 1,
            $self->_grouping_clause(),
        }
    )->all())[0];
}

=head2 next_sibling

  my $sibling = $item->next_sibling();

Returns the sibling that resides one position forward. Returns undef 
if the current object is the last one.

=cut

sub next_sibling {
    my( $self ) = @_;
    my $position_column = $self->position_column;
    my $position = $self->get_column( $position_column );
    my $count = $self->result_source->resultset->search({$self->_grouping_clause()})->count();
    return 0 if ($position==$count);
    return ($self->result_source->resultset->search(
        {
            $position_column => $position + 1,
            $self->_grouping_clause(),
        },
    )->all())[0];
}

=head2 move_previous

  $item->move_previous();

Swaps position with the sibling in the position previous in
the list.  Returns 1 on success, and 0 if the object is
already the first one.

=cut

sub move_previous {
    my( $self ) = @_;
    my $position = $self->get_column( $self->position_column() );
    return $self->move_to( $position - 1 );
}

=head2 move_next

  $item->move_next();

Swaps position with the sibling in the next position in the
list.  Returns 1 on success, and 0 if the object is already
the last in the list.

=cut

sub move_next {
    my( $self ) = @_;
    my $position = $self->get_column( $self->position_column() );
    my $count = $self->result_source->resultset->search({$self->_grouping_clause()})->count();
    return 0 if ($position==$count);
    return $self->move_to( $position + 1 );
}

=head2 move_first

  $item->move_first();

Moves the object to the first position in the list.  Returns 1
on success, and 0 if the object is already the first.

=cut

sub move_first {
    my( $self ) = @_;
    return $self->move_to( 1 );
}

=head2 move_last

  $item->move_last();

Moves the object to the last position in the list.  Returns 1
on success, and 0 if the object is already the last one.

=cut

sub move_last {
    my( $self ) = @_;
    my $count = $self->result_source->resultset->search({$self->_grouping_clause()})->count();
    return $self->move_to( $count );
}

=head2 move_to

  $item->move_to( $position );

Moves the object to the specified position.  Returns 1 on
success, and 0 if the object is already at the specified
position.

=cut

sub move_to {
    my( $self, $to_position ) = @_;
    my $position_column = $self->position_column;
    my $from_position = $self->get_column( $position_column );
    return 0 if ( $to_position < 1 );
    return 0 if ( $from_position==$to_position );
    my @between = (
        ( $from_position < $to_position )
        ? ( $from_position+1, $to_position )
        : ( $to_position, $from_position-1 )
    );
    my $rs = $self->result_source->resultset->search({
        $position_column => { -between => [ @between ] },
        $self->_grouping_clause(),
    });
    my $op = ($from_position>$to_position) ? '+' : '-';
    $rs->update({ $position_column => \"$position_column $op 1" });  #" Sorry, GEdit bug
    $self->{_ORDERED_INTERNAL_UPDATE} = 1;
    $self->update({ $position_column => $to_position });
    return 1;
}



=head2 move_to_group

  $item->move_to_group( $group, $position );

Moves the object to the specified position of the specified
group, or to the end of the group if $position is undef.
1 is returned on success, and 0 is returned if the object is
already at the specified position of the specified group.

$group may be specified as a single scalar if only one 
grouping column is in use, or as a hashref of column => value pairs
if multiple grouping columns are in use.

=cut

sub move_to_group {
    my( $self, $to_group, $to_position ) = @_;

    # if we're given a string, turn it into a hashref
    unless (ref $to_group eq 'HASH') {
        $to_group = {($self->_grouping_columns)[0] => $to_group};
    }

    my $position_column = $self->position_column;
    #my @grouping_columns = $self->_grouping_columns;

    return 0 if ( ! defined($to_group) );
    return 0 if ( defined($to_position) and $to_position < 1 );
    return 0 if ( $self->_is_in_group($to_group) 
                    and ((not defined($to_position)) 
                            or (defined($to_position) and $self->$position_column==$to_position)
                        )
                    );

    # Move to end of current group and adjust siblings
    $self->move_last;

    $self->set_columns($to_group);
    my $new_group_count = $self->result_source->resultset->search({$self->_grouping_clause()})->count();
    if (!defined($to_position) or $to_position > $new_group_count) {
        $self->{_ORDERED_INTERNAL_UPDATE} = 1;
        $self->update({ $position_column => $new_group_count + 1 });
    }
    else {
        my @between = ($to_position, $new_group_count);

        my $rs = $self->result_source->resultset->search({
            $position_column => { -between => [ @between ] },
            $self->_grouping_clause(),
        });
        $rs->update({ $position_column => \"$position_column + 1" }); #"
        $self->{_ORDERED_INTERNAL_UPDATE} = 1;
        $self->update({ $position_column => $to_position });
    }

    return 1;
}

=head2 insert

Overrides the DBIC insert() method by providing a default 
position number.  The default will be the number of rows in 
the table +1, thus positioning the new record at the last position.

=cut

sub insert {
    my $self = shift;
    my $position_column = $self->position_column;
    $self->set_column( $position_column => $self->result_source->resultset->search( {$self->_grouping_clause()} )->count()+1 ) 
        if (!$self->get_column($position_column));
    return $self->next::method( @_ );
}

=head2 update

Overrides the DBIC update() method by checking for a change
to the position and/or group columns.  Movement within a
group or to another group is handled by repositioning
the appropriate siblings.  Position defaults to the end
of a new group if it has been changed to undef.

=cut

sub update {
    my $self = shift;

    if ($self->{_ORDERED_INTERNAL_UPDATE}) {
        delete $self->{_ORDERED_INTERNAL_UPDATE};
        return $self->next::method( @_ );
    }

    $self->set_columns($_[0]) if @_ > 0;
    my %changes = $self->get_dirty_columns;
    $self->discard_changes;

    my $pos_col = $self->position_column;

    # if any of our grouping columns have been changed
    if (grep {$_} map {exists $changes{$_}} $self->_grouping_columns ) {

        # create new_group by taking the current group and inserting changes
        my $new_group = {$self->_grouping_clause};
        foreach my $col (keys %$new_group) {
            if (exists $changes{$col}) {
                $new_group->{$col} = $changes{$col};
                delete $changes{$col}; # don't want to pass this on to next::method
            }
        }

        $self->move_to_group(
            $new_group,
            exists($changes{$pos_col}) ? delete($changes{$pos_col}) : $self->$pos_col
        );
    }
    elsif (exists $changes{$pos_col}) {
        $self->move_to(delete $changes{$pos_col});
    }
    return $self->next::method( \%changes );
}

=head2 delete

Overrides the DBIC delete() method by first moving the object 
to the last position, then deleting it, thus ensuring the 
integrity of the positions.

=cut

sub delete {
    my $self = shift;
    $self->move_last;
    return $self->next::method( @_ );
}

=head1 PRIVATE METHODS

These methods are used internally.  You should never have the 
need to use them.

=head2 _grouping_clause

This method returns one or more name=>value pairs for limiting a search 
by the grouping column(s).  If the grouping column is not 
defined then this will return an empty list.

=cut
sub _grouping_clause {
    my( $self ) = @_;
    return map {  $_ => $self->get_column($_)  } $self->_grouping_columns();
}



=head2 _get_grouping_columns

Returns a list of the column names used for grouping, regardless of whether
they were specified as an arrayref or a single string, and returns ()
if there is no grouping.

=cut
sub _grouping_columns {
    my( $self ) = @_;
    my $col = $self->grouping_column();
    if (ref $col eq 'ARRAY') {
        return @$col;
    } elsif ($col) {
        return ( $col );
    } else {
        return ();
    }
}



=head2 _is_in_group($other)

    $item->_is_in_group( {user => 'fred', list => 'work'} )

Returns true if the object is in the group represented by hashref $other
=cut
sub _is_in_group {
    my ($self, $other) = @_;
    my $current = {$self->_grouping_clause};
    return 0 unless (ref $other eq 'HASH') and (keys %$current == keys %$other);
    for my $key (keys %$current) {
        return 0 unless exists $other->{$key};
        return 0 if $current->{$key} ne $other->{$key};
    }
    return 1;
}


1;
__END__

=head1 BUGS

=head2 Unique Constraints

Unique indexes and constraints on the position column are not 
supported at this time.  It would be make sense to support them, 
but there are some unexpected database issues that make this 
hard to do.  The main problem from the author's view is that 
SQLite (the DB engine that we use for testing) does not support 
ORDER BY on updates.

=head2 Race Condition on Insert

If a position is not specified for an insert than a position 
will be chosen based on COUNT(*)+1.  But, it first selects the 
count, and then inserts the record.  The space of time between select 
and insert introduces a race condition.  To fix this we need the 
ability to lock tables in DBIC.  I've added an entry in the TODO 
about this.

=head2 Multiple Moves

Be careful when issueing move_* methods to multiple objects.  If 
you've pre-loaded the objects then when you move one of the objects 
the position of the other object will not reflect their new value 
until you reload them from the database.

There are times when you will want to move objects as groups, such 
as changeing the parent of several objects at once - this directly 
conflicts with this problem.  One solution is for us to write a 
ResultSet class that supports a parent() method, for example.  Another 
solution is to somehow automagically modify the objects that exist 
in the current object's result set to have the new position value.

=head1 AUTHOR

Aran Deltac <bluefeet@cpan.org>

=head1 LICENSE

You may distribute this code under the same terms as Perl itself.

