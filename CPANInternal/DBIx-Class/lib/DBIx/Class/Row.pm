package DBIx::Class::Row;

use strict;
use warnings;

use base qw/DBIx::Class/;
use Carp::Clan qw/^DBIx::Class/;
use Scalar::Util ();
use Scope::Guard;

__PACKAGE__->mk_group_accessors('simple' => qw/_source_handle/);

=head1 NAME

DBIx::Class::Row - Basic row methods

=head1 SYNOPSIS

=head1 DESCRIPTION

This class is responsible for defining and doing basic operations on rows
derived from L<DBIx::Class::ResultSource> objects.

=head1 METHODS

=head2 new

  my $obj = My::Class->new($attrs);

Creates a new row object from column => value mappings passed as a hash ref

Passing an object, or an arrayref of objects as a value will call
L<DBIx::Class::Relationship::Base/set_from_related> for you. When
passed a hashref or an arrayref of hashrefs as the value, these will
be turned into objects via new_related, and treated as if you had
passed objects.

For a more involved explanation, see L<DBIx::Class::ResultSet/create>.

=cut

## It needs to store the new objects somewhere, and call insert on that list later when insert is called on this object. We may need an accessor for these so the user can retrieve them, if just doing ->new().
## This only works because DBIC doesnt yet care to check whether the new_related objects have been passed all their mandatory columns
## When doing the later insert, we need to make sure the PKs are set.
## using _relationship_data in new and funky ways..
## check Relationship::CascadeActions and Relationship::Accessor for compat
## tests!

sub new {
  my ($class, $attrs) = @_;
  $class = ref $class if ref $class;

  my $new = { _column_data => {} };
  bless $new, $class;

  if (my $handle = delete $attrs->{-source_handle}) {
    $new->_source_handle($handle);
  }
  if (my $source = delete $attrs->{-result_source}) {
    $new->result_source($source);
  }

  if ($attrs) {
    $new->throw_exception("attrs must be a hashref")
      unless ref($attrs) eq 'HASH';
    
    my ($related,$inflated);
    ## Pretend all the rels are actual objects, unset below if not, for insert() to fix
    $new->{_rel_in_storage} = 1;

    foreach my $key (keys %$attrs) {
      if (ref $attrs->{$key}) {
        ## Can we extract this lot to use with update(_or .. ) ?
        my $info = $class->relationship_info($key);
        if ($info && $info->{attrs}{accessor}
          && $info->{attrs}{accessor} eq 'single')
        {
          my $rel_obj = delete $attrs->{$key};
          if(!Scalar::Util::blessed($rel_obj)) {
            $rel_obj = $new->find_or_new_related($key, $rel_obj);
          }

          $new->{_rel_in_storage} = 0 unless ($rel_obj->in_storage);

          $new->set_from_related($key, $rel_obj);        
          $related->{$key} = $rel_obj;
          next;
        } elsif ($info && $info->{attrs}{accessor}
            && $info->{attrs}{accessor} eq 'multi'
            && ref $attrs->{$key} eq 'ARRAY') {
          my $others = delete $attrs->{$key};
          foreach my $rel_obj (@$others) {
            if(!Scalar::Util::blessed($rel_obj)) {
              $rel_obj = $new->new_related($key, $rel_obj);
              $new->{_rel_in_storage} = 0;
            }

            $new->{_rel_in_storage} = 0 unless ($rel_obj->in_storage);
          }
          $related->{$key} = $others;
          next;
        } elsif ($info && $info->{attrs}{accessor}
          && $info->{attrs}{accessor} eq 'filter')
        {
          ## 'filter' should disappear and get merged in with 'single' above!
          my $rel_obj = delete $attrs->{$key};
          if(!Scalar::Util::blessed($rel_obj)) {
            $rel_obj = $new->find_or_new_related($key, $rel_obj);
            $new->{_rel_in_storage} = 0 unless ($rel_obj->in_storage);
          }
          $inflated->{$key} = $rel_obj;
          next;
        } elsif ($class->has_column($key)
            && $class->column_info($key)->{_inflate_info}) {
          $inflated->{$key} = $attrs->{$key};
          next;
        }
      }
      $new->throw_exception("No such column $key on $class")
        unless $class->has_column($key);
      $new->store_column($key => $attrs->{$key});          
    }

    $new->{_relationship_data} = $related if $related;
    $new->{_inflated_column} = $inflated if $inflated;
  }

  return $new;
}

=head2 insert

  $obj->insert;

Inserts an object into the database if it isn't already in
there. Returns the object itself. Requires the object's result source to
be set, or the class to have a result_source_instance method. To insert
an entirely new object into the database, use C<create> (see
L<DBIx::Class::ResultSet/create>).

This will also insert any uninserted, related objects held inside this
one, see L<DBIx::Class::ResultSet/create> for more details.

=cut

sub insert {
  my ($self) = @_;
  return $self if $self->in_storage;
  my $source = $self->result_source;
  $source ||=  $self->result_source($self->result_source_instance)
    if $self->can('result_source_instance');
  $self->throw_exception("No result_source set on this object; can't insert")
    unless $source;

  my $rollback_guard;

  # Check if we stored uninserted relobjs here in new()
  my %related_stuff = (%{$self->{_relationship_data} || {}}, 
                       %{$self->{_inflated_column} || {}});

  if(!$self->{_rel_in_storage}) {

    # The guard will save us if we blow out of this scope via die
    $rollback_guard = $source->storage->txn_scope_guard;

    ## Should all be in relationship_data, but we need to get rid of the
    ## 'filter' reltype..
    ## These are the FK rels, need their IDs for the insert.

    my @pri = $self->primary_columns;

    REL: foreach my $relname (keys %related_stuff) {

      my $rel_obj = $related_stuff{$relname};

      next REL unless (Scalar::Util::blessed($rel_obj)
                       && $rel_obj->isa('DBIx::Class::Row'));

      my $cond = $source->relationship_info($relname)->{cond};

      next REL unless ref($cond) eq 'HASH';

      # map { foreign.foo => 'self.bar' } to { bar => 'foo' }

      my $keyhash = { map { my $x = $_; $x =~ s/.*\.//; $x; } reverse %$cond };

      # assume anything that references our PK probably is dependent on us
      # rather than vice versa, unless the far side is (a) defined or (b)
      # auto-increment

      foreach my $p (@pri) {
        if (exists $keyhash->{$p}) {
          unless (defined($rel_obj->get_column($keyhash->{$p}))
                  || $rel_obj->column_info($keyhash->{$p})
                             ->{is_auto_increment}) {
            next REL;
          }
        }
      }

      $rel_obj->insert();
      $self->set_from_related($relname, $rel_obj);
      delete $related_stuff{$relname};
    }
  }

  $source->storage->insert($source, { $self->get_columns });

  ## PK::Auto
  my @auto_pri = grep {
                   !defined $self->get_column($_) || 
                   ref($self->get_column($_)) eq 'SCALAR'
                 } $self->primary_columns;

  if (@auto_pri) {
    #$self->throw_exception( "More than one possible key found for auto-inc on ".ref $self )
    #  if defined $too_many;

    my $storage = $self->result_source->storage;
    $self->throw_exception( "Missing primary key but Storage doesn't support last_insert_id" )
      unless $storage->can('last_insert_id');
    my @ids = $storage->last_insert_id($self->result_source,@auto_pri);
    $self->throw_exception( "Can't get last insert id" )
      unless (@ids == @auto_pri);
    $self->store_column($auto_pri[$_] => $ids[$_]) for 0 .. $#ids;
  }

  if(!$self->{_rel_in_storage}) {
    ## Now do the has_many rels, that need $selfs ID.
    foreach my $relname (keys %related_stuff) {
      my $rel_obj = $related_stuff{$relname};
      my @cands;
      if (Scalar::Util::blessed($rel_obj)
          && $rel_obj->isa('DBIx::Class::Row')) {
        @cands = ($rel_obj);
      } elsif (ref $rel_obj eq 'ARRAY') {
        @cands = @$rel_obj;
      }
      if (@cands) {
        my $reverse = $source->reverse_relationship_info($relname);
        foreach my $obj (@cands) {
          $obj->set_from_related($_, $self) for keys %$reverse;
          $obj->insert() unless ($obj->in_storage || $obj->result_source->resultset->search({$obj->get_columns})->count);
        }
      }
    }
    $rollback_guard->commit;
  }

  $self->in_storage(1);
  $self->{_dirty_columns} = {};
  $self->{related_resultsets} = {};
  undef $self->{_orig_ident};
  return $self;
}

=head2 in_storage

  $obj->in_storage; # Get value
  $obj->in_storage(1); # Set value

Indicates whether the object exists as a row in the database or not

=cut

sub in_storage {
  my ($self, $val) = @_;
  $self->{_in_storage} = $val if @_ > 1;
  return $self->{_in_storage};
}

=head2 update

  $obj->update \%columns?;

Must be run on an object that is already in the database; issues an SQL
UPDATE query to commit any changes to the object to the database if
required.

Also takes an options hashref of C<< column_name => value> pairs >> to update
first. But be aware that the hashref will be passed to
C<set_inflated_columns>, which might edit it in place, so dont rely on it being
the same after a call to C<update>.  If you need to preserve the hashref, it is
sufficient to pass a shallow copy to C<update>, e.g. ( { %{ $href } } )

=cut

sub update {
  my ($self, $upd) = @_;
  $self->throw_exception( "Not in database" ) unless $self->in_storage;
  my $ident_cond = $self->ident_condition;
  $self->throw_exception("Cannot safely update a row in a PK-less table")
    if ! keys %$ident_cond;

  $self->set_inflated_columns($upd) if $upd;
  my %to_update = $self->get_dirty_columns;
  return $self unless keys %to_update;
  my $rows = $self->result_source->storage->update(
               $self->result_source, \%to_update,
               $self->{_orig_ident} || $ident_cond
             );
  if ($rows == 0) {
    $self->throw_exception( "Can't update ${self}: row not found" );
  } elsif ($rows > 1) {
    $self->throw_exception("Can't update ${self}: updated more than one row");
  }
  $self->{_dirty_columns} = {};
  $self->{related_resultsets} = {};
  undef $self->{_orig_ident};
  return $self;
}

=head2 delete

  $obj->delete

Deletes the object from the database. The object is still perfectly
usable, but C<< ->in_storage() >> will now return 0 and the object must
reinserted using C<< ->insert() >> before C<< ->update() >> can be used
on it. If you delete an object in a class with a C<has_many>
relationship, all the related objects will be deleted as well. To turn
this behavior off, pass C<< cascade_delete => 0 >> in the C<$attr>
hashref. Any database-level cascade or restrict will take precedence
over a DBIx-Class-based cascading delete. See also L<DBIx::Class::ResultSet/delete>.

=cut

sub delete {
  my $self = shift;
  if (ref $self) {
    $self->throw_exception( "Not in database" ) unless $self->in_storage;
    my $ident_cond = $self->ident_condition;
    $self->throw_exception("Cannot safely delete a row in a PK-less table")
      if ! keys %$ident_cond;
    foreach my $column (keys %$ident_cond) {
            $self->throw_exception("Can't delete the object unless it has loaded the primary keys")
              unless exists $self->{_column_data}{$column};
    }
    $self->result_source->storage->delete(
      $self->result_source, $ident_cond);
    $self->in_storage(undef);
  } else {
    $self->throw_exception("Can't do class delete without a ResultSource instance")
      unless $self->can('result_source_instance');
    my $attrs = @_ > 1 && ref $_[$#_] eq 'HASH' ? { %{pop(@_)} } : {};
    my $query = ref $_[0] eq 'HASH' ? $_[0] : {@_};
    $self->result_source_instance->resultset->search(@_)->delete;
  }
  return $self;
}

=head2 get_column

  my $val = $obj->get_column($col);

Gets a column value from a row object. Does not do any queries; the column 
must have already been fetched from the database and stored in the object. If 
there is an inflated value stored that has not yet been deflated, it is deflated
when the method is invoked.

=cut

sub get_column {
  my ($self, $column) = @_;
  $self->throw_exception( "Can't fetch data as class method" ) unless ref $self;
  return $self->{_column_data}{$column} if exists $self->{_column_data}{$column};
  if (exists $self->{_inflated_column}{$column}) {
    return $self->store_column($column,
      $self->_deflated_column($column, $self->{_inflated_column}{$column}));   
  }
  $self->throw_exception( "No such column '${column}'" ) unless $self->has_column($column);
  return undef;
}

=head2 has_column_loaded

  if ( $obj->has_column_loaded($col) ) {
     print "$col has been loaded from db";
  }

Returns a true value if the column value has been loaded from the
database (or set locally).

=cut

sub has_column_loaded {
  my ($self, $column) = @_;
  $self->throw_exception( "Can't call has_column data as class method" ) unless ref $self;
  return 1 if exists $self->{_inflated_column}{$column};
  return exists $self->{_column_data}{$column};
}

=head2 get_columns

  my %data = $obj->get_columns;

Does C<get_column>, for all column values at once.

=cut

sub get_columns {
  my $self = shift;
  if (exists $self->{_inflated_column}) {
    foreach my $col (keys %{$self->{_inflated_column}}) {
      $self->store_column($col, $self->_deflated_column($col, $self->{_inflated_column}{$col}))
        unless exists $self->{_column_data}{$col};
    }
  }
  return %{$self->{_column_data}};
}

=head2 get_dirty_columns

  my %data = $obj->get_dirty_columns;

Identical to get_columns but only returns those that have been changed.

=cut

sub get_dirty_columns {
  my $self = shift;
  return map { $_ => $self->{_column_data}{$_} }
           keys %{$self->{_dirty_columns}};
}

=head2 get_inflated_columns

  my $inflated_data = $obj->get_inflated_columns;

Similar to get_columns but objects are returned for inflated columns instead of their raw non-inflated values.

=cut

sub get_inflated_columns {
  my $self = shift;
  return map {
    my $accessor = $self->column_info($_)->{'accessor'} || $_;
    ($_ => $self->$accessor);
  } $self->columns;
}

=head2 set_column

  $obj->set_column($col => $val);

Sets a column value. If the new value is different from the old one,
the column is marked as dirty for when you next call $obj->update.

=cut

sub set_column {
  my $self = shift;
  my ($column) = @_;
  $self->{_orig_ident} ||= $self->ident_condition;
  my $old = $self->get_column($column);
  my $ret = $self->store_column(@_);
  $self->{_dirty_columns}{$column} = 1
    if (defined $old ^ defined $ret) || (defined $old && $old ne $ret);
  return $ret;
}

=head2 set_columns

  my $copy = $orig->set_columns({ $col => $val, ... });

Sets more than one column value at once.

=cut

sub set_columns {
  my ($self,$data) = @_;
  foreach my $col (keys %$data) {
    $self->set_column($col,$data->{$col});
  }
  return $self;
}

=head2 set_inflated_columns

  my $copy = $orig->set_inflated_columns({ $col => $val, $rel => $obj, ... });

Sets more than one column value at once, taking care to respect inflations and
relationships if relevant. Be aware that this hashref might be edited in place,
so dont rely on it being the same after a call to C<set_inflated_columns>. If
you need to preserve the hashref, it is sufficient to pass a shallow copy to
C<set_inflated_columns>, e.g. ( { %{ $href } } )

=cut

sub set_inflated_columns {
  my ( $self, $upd ) = @_;
  foreach my $key (keys %$upd) {
    if (ref $upd->{$key}) {
      my $info = $self->relationship_info($key);
      if ($info && $info->{attrs}{accessor}
        && $info->{attrs}{accessor} eq 'single')
      {
        my $rel = delete $upd->{$key};
        $self->set_from_related($key => $rel);
        $self->{_relationship_data}{$key} = $rel;          
      } elsif ($info && $info->{attrs}{accessor}
        && $info->{attrs}{accessor} eq 'multi'
        && ref $upd->{$key} eq 'ARRAY') {
        my $others = delete $upd->{$key};
        foreach my $rel_obj (@$others) {
          if(!Scalar::Util::blessed($rel_obj)) {
            $rel_obj = $self->create_related($key, $rel_obj);
          }
        }
        $self->{_relationship_data}{$key} = $others; 
#            $related->{$key} = $others;
        next;
      }
      elsif ($self->has_column($key)
        && exists $self->column_info($key)->{_inflate_info})
      {
        $self->set_inflated_column($key, delete $upd->{$key});          
      }
    }
  }
  $self->set_columns($upd);    
}

=head2 copy

  my $copy = $orig->copy({ change => $to, ... });

Inserts a new row with the specified changes.

=cut

sub copy {
  my ($self, $changes) = @_;
  $changes ||= {};
  my $col_data = { %{$self->{_column_data}} };
  foreach my $col (keys %$col_data) {
    delete $col_data->{$col}
      if $self->result_source->column_info($col)->{is_auto_increment};
  }

  my $new = { _column_data => $col_data };
  bless $new, ref $self;

  $new->result_source($self->result_source);
  $new->set_inflated_columns($changes);
  $new->insert;

  # Its possible we'll have 2 relations to the same Source. We need to make 
  # sure we don't try to insert the same row twice esle we'll violate unique
  # constraints
  my $rels_copied = {};

  foreach my $rel ($self->result_source->relationships) {
    my $rel_info = $self->result_source->relationship_info($rel);

    next unless $rel_info->{attrs}{cascade_copy};
  
    my $resolved = $self->result_source->resolve_condition(
      $rel_info->{cond}, $rel, $new
    );

    my $copied = $rels_copied->{ $rel_info->{source} } ||= {};
    foreach my $related ($self->search_related($rel)) {
      my $id_str = join("\0", $related->id);
      next if $copied->{$id_str};
      $copied->{$id_str} = 1;
      my $rel_copy = $related->copy($resolved);
    }
 
  }
  return $new;
}

=head2 store_column

  $obj->store_column($col => $val);

Sets a column value without marking it as dirty.

=cut

sub store_column {
  my ($self, $column, $value) = @_;
  $self->throw_exception( "No such column '${column}'" )
    unless exists $self->{_column_data}{$column} || $self->has_column($column);
  $self->throw_exception( "set_column called for ${column} without value" )
    if @_ < 3;
  return $self->{_column_data}{$column} = $value;
}

=head2 inflate_result

  Class->inflate_result($result_source, \%me, \%prefetch?)

Called by ResultSet to inflate a result from storage

=cut

sub inflate_result {
  my ($class, $source, $me, $prefetch) = @_;

  my ($source_handle) = $source;

  if ($source->isa('DBIx::Class::ResultSourceHandle')) {
      $source = $source_handle->resolve
  } else {
      $source_handle = $source->handle
  }

  my $new = {
    _source_handle => $source_handle,
    _column_data => $me,
    _in_storage => 1
  };
  bless $new, (ref $class || $class);

  my $schema;
  foreach my $pre (keys %{$prefetch||{}}) {
    my $pre_val = $prefetch->{$pre};
    my $pre_source = $source->related_source($pre);
    $class->throw_exception("Can't prefetch non-existent relationship ${pre}")
      unless $pre_source;
    if (ref($pre_val->[0]) eq 'ARRAY') { # multi
      my @pre_objects;
      foreach my $pre_rec (@$pre_val) {
        unless ($pre_source->primary_columns == grep { exists $pre_rec->[0]{$_}
           and defined $pre_rec->[0]{$_} } $pre_source->primary_columns) {
          next;
        }
        push(@pre_objects, $pre_source->result_class->inflate_result(
                             $pre_source, @{$pre_rec}));
      }
      $new->related_resultset($pre)->set_cache(\@pre_objects);
    } elsif (defined $pre_val->[0]) {
      my $fetched;
      unless ($pre_source->primary_columns == grep { exists $pre_val->[0]{$_}
         and !defined $pre_val->[0]{$_} } $pre_source->primary_columns)
      {
        $fetched = $pre_source->result_class->inflate_result(
                      $pre_source, @{$pre_val});
      }
      $new->related_resultset($pre)->set_cache([ $fetched ]);
      my $accessor = $source->relationship_info($pre)->{attrs}{accessor};
      $class->throw_exception("No accessor for prefetched $pre")
       unless defined $accessor;
      if ($accessor eq 'single') {
        $new->{_relationship_data}{$pre} = $fetched;
      } elsif ($accessor eq 'filter') {
        $new->{_inflated_column}{$pre} = $fetched;
      } else {
       $class->throw_exception("Prefetch not supported with accessor '$accessor'");
      }
    }
  }
  return $new;
}

=head2 update_or_insert

  $obj->update_or_insert

Updates the object if it's already in the db, else inserts it.

=head2 insert_or_update

  $obj->insert_or_update

Alias for L</update_or_insert>

=cut

*insert_or_update = \&update_or_insert;
sub update_or_insert {
  my $self = shift;
  return ($self->in_storage ? $self->update : $self->insert);
}

=head2 is_changed

  my @changed_col_names = $obj->is_changed();
  if ($obj->is_changed()) { ... }

In array context returns a list of columns with uncommited changes, or
in scalar context returns a true value if there are uncommitted
changes.

=cut

sub is_changed {
  return keys %{shift->{_dirty_columns} || {}};
}

=head2 is_column_changed

  if ($obj->is_column_changed('col')) { ... }

Returns a true value if the column has uncommitted changes.

=cut

sub is_column_changed {
  my( $self, $col ) = @_;
  return exists $self->{_dirty_columns}->{$col};
}

=head2 result_source

  my $resultsource = $object->result_source;

Accessor to the ResultSource this object was created from

=cut

sub result_source {
    my $self = shift;

    if (@_) {
        $self->_source_handle($_[0]->handle);
    } else {
        $self->_source_handle->resolve;
    }
}

=head2 register_column

  $column_info = { .... };
  $class->register_column($column_name, $column_info);

Registers a column on the class. If the column_info has an 'accessor'
key, creates an accessor named after the value if defined; if there is
no such key, creates an accessor with the same name as the column

The column_info attributes are described in
L<DBIx::Class::ResultSource/add_columns>

=cut

sub register_column {
  my ($class, $col, $info) = @_;
  my $acc = $col;
  if (exists $info->{accessor}) {
    return unless defined $info->{accessor};
    $acc = [ $info->{accessor}, $col ];
  }
  $class->mk_group_accessors('column' => $acc);
}


=head2 throw_exception

See Schema's throw_exception.

=cut

sub throw_exception {
  my $self=shift;
  if (ref $self && ref $self->result_source && $self->result_source->schema) {
    $self->result_source->schema->throw_exception(@_);
  } else {
    croak(@_);
  }
}

=head2 id

Returns the primary key(s) for a row. Can't be called as a class method.
Actually implemented in L<DBIx::Class::PK>

=head2 discard_changes

Re-selects the row from the database, losing any changes that had
been made.

This method can also be used to refresh from storage, retrieving any
changes made since the row was last read from storage. Actually
implemented in L<DBIx::Class::PK>

=cut

1;

=head1 AUTHORS

Matt S. Trout <mst@shadowcatsystems.co.uk>

=head1 LICENSE

You may distribute this code under the same terms as Perl itself.

=cut
