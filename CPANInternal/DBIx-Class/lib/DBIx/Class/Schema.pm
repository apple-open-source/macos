package DBIx::Class::Schema;

use strict;
use warnings;

use DBIx::Class::Exception;
use Carp::Clan qw/^DBIx::Class/;
use Scalar::Util qw/weaken/;
use File::Spec;
require Module::Find;

use base qw/DBIx::Class/;

__PACKAGE__->mk_classdata('class_mappings' => {});
__PACKAGE__->mk_classdata('source_registrations' => {});
__PACKAGE__->mk_classdata('storage_type' => '::DBI');
__PACKAGE__->mk_classdata('storage');
__PACKAGE__->mk_classdata('exception_action');
__PACKAGE__->mk_classdata('stacktrace' => $ENV{DBIC_TRACE} || 0);
__PACKAGE__->mk_classdata('default_resultset_attributes' => {});

=head1 NAME

DBIx::Class::Schema - composable schemas

=head1 SYNOPSIS

  package Library::Schema;
  use base qw/DBIx::Class::Schema/;

  # load Library::Schema::CD, Library::Schema::Book, Library::Schema::DVD
  __PACKAGE__->load_classes(qw/CD Book DVD/);

  package Library::Schema::CD;
  use base qw/DBIx::Class/;
  __PACKAGE__->load_components(qw/PK::Auto Core/); # for example
  __PACKAGE__->table('cd');

  # Elsewhere in your code:
  my $schema1 = Library::Schema->connect(
    $dsn,
    $user,
    $password,
    { AutoCommit => 0 },
  );

  my $schema2 = Library::Schema->connect($coderef_returning_dbh);

  # fetch objects using Library::Schema::DVD
  my $resultset = $schema1->resultset('DVD')->search( ... );
  my @dvd_objects = $schema2->resultset('DVD')->search( ... );

=head1 DESCRIPTION

Creates database classes based on a schema. This is the recommended way to
use L<DBIx::Class> and allows you to use more than one concurrent connection
with your classes.

NB: If you're used to L<Class::DBI> it's worth reading the L</SYNOPSIS>
carefully, as DBIx::Class does things a little differently. Note in
particular which module inherits off which.

=head1 METHODS

=head2 register_class

=over 4

=item Arguments: $moniker, $component_class

=back

Registers a class which isa DBIx::Class::ResultSourceProxy. Equivalent to
calling:

  $schema->register_source($moniker, $component_class->result_source_instance);

=cut

sub register_class {
  my ($self, $moniker, $to_register) = @_;
  $self->register_source($moniker => $to_register->result_source_instance);
}

=head2 register_source

=over 4

=item Arguments: $moniker, $result_source

=back

Registers the L<DBIx::Class::ResultSource> in the schema with the given
moniker.

=cut

sub register_source {
  my ($self, $moniker, $source) = @_;

  %$source = %{ $source->new( { %$source, source_name => $moniker }) };

  my %reg = %{$self->source_registrations};
  $reg{$moniker} = $source;
  $self->source_registrations(\%reg);

  $source->schema($self);

  weaken($source->{schema}) if ref($self);
  if ($source->result_class) {
    my %map = %{$self->class_mappings};
    $map{$source->result_class} = $moniker;
    $self->class_mappings(\%map);
  }
}

sub _unregister_source {
    my ($self, $moniker) = @_;
    my %reg = %{$self->source_registrations}; 

    my $source = delete $reg{$moniker};
    $self->source_registrations(\%reg);
    if ($source->result_class) {
        my %map = %{$self->class_mappings};
        delete $map{$source->result_class};
        $self->class_mappings(\%map);
    }
}

=head2 class

=over 4

=item Arguments: $moniker

=item Return Value: $classname

=back

Retrieves the result class name for the given moniker. For example:

  my $class = $schema->class('CD');

=cut

sub class {
  my ($self, $moniker) = @_;
  return $self->source($moniker)->result_class;
}

=head2 source

=over 4

=item Arguments: $moniker

=item Return Value: $result_source

=back

  my $source = $schema->source('Book');

Returns the L<DBIx::Class::ResultSource> object for the registered moniker.

=cut

sub source {
  my ($self, $moniker) = @_;
  my $sreg = $self->source_registrations;
  return $sreg->{$moniker} if exists $sreg->{$moniker};

  # if we got here, they probably passed a full class name
  my $mapped = $self->class_mappings->{$moniker};
  $self->throw_exception("Can't find source for ${moniker}")
    unless $mapped && exists $sreg->{$mapped};
  return $sreg->{$mapped};
}

=head2 sources

=over 4

=item Return Value: @source_monikers

=back

Returns the source monikers of all source registrations on this schema.
For example:

  my @source_monikers = $schema->sources;

=cut

sub sources { return keys %{shift->source_registrations}; }

=head2 storage

  my $storage = $schema->storage;

Returns the L<DBIx::Class::Storage> object for this Schema.

=head2 resultset

=over 4

=item Arguments: $moniker

=item Return Value: $result_set

=back

  my $rs = $schema->resultset('DVD');

Returns the L<DBIx::Class::ResultSet> object for the registered moniker.

=cut

sub resultset {
  my ($self, $moniker) = @_;
  return $self->source($moniker)->resultset;
}

=head2 load_classes

=over 4

=item Arguments: @classes?, { $namespace => [ @classes ] }+

=back

With no arguments, this method uses L<Module::Find> to find all classes under
the schema's namespace. Otherwise, this method loads the classes you specify
(using L<use>), and registers them (using L</"register_class">).

It is possible to comment out classes with a leading C<#>, but note that perl
will think it's a mistake (trying to use a comment in a qw list), so you'll
need to add C<no warnings 'qw';> before your load_classes call.

Example:

  My::Schema->load_classes(); # loads My::Schema::CD, My::Schema::Artist,
                              # etc. (anything under the My::Schema namespace)

  # loads My::Schema::CD, My::Schema::Artist, Other::Namespace::Producer but
  # not Other::Namespace::LinerNotes nor My::Schema::Track
  My::Schema->load_classes(qw/ CD Artist #Track /, {
    Other::Namespace => [qw/ Producer #LinerNotes /],
  });

=cut

sub load_classes {
  my ($class, @params) = @_;

  my %comps_for;

  if (@params) {
    foreach my $param (@params) {
      if (ref $param eq 'ARRAY') {
        # filter out commented entries
        my @modules = grep { $_ !~ /^#/ } @$param;

        push (@{$comps_for{$class}}, @modules);
      }
      elsif (ref $param eq 'HASH') {
        # more than one namespace possible
        for my $comp ( keys %$param ) {
          # filter out commented entries
          my @modules = grep { $_ !~ /^#/ } @{$param->{$comp}};

          push (@{$comps_for{$comp}}, @modules);
        }
      }
      else {
        # filter out commented entries
        push (@{$comps_for{$class}}, $param) if $param !~ /^#/;
      }
    }
  } else {
    my @comp = map { substr $_, length "${class}::"  }
                 Module::Find::findallmod($class);
    $comps_for{$class} = \@comp;
  }

  my @to_register;
  {
    no warnings qw/redefine/;
    local *Class::C3::reinitialize = sub { };
    foreach my $prefix (keys %comps_for) {
      foreach my $comp (@{$comps_for{$prefix}||[]}) {
        my $comp_class = "${prefix}::${comp}";
        { # try to untaint module name. mods where this fails
          # are left alone so we don't have to change the old behavior
          no locale; # localized \w doesn't untaint expression
          if ( $comp_class =~ m/^( (?:\w+::)* \w+ )$/x ) {
            $comp_class = $1;
          }
        }
        $class->ensure_class_loaded($comp_class);

        $comp = $comp_class->source_name || $comp;
#  $DB::single = 1;
        push(@to_register, [ $comp, $comp_class ]);
      }
    }
  }
  Class::C3->reinitialize;

  foreach my $to (@to_register) {
    $class->register_class(@$to);
    #  if $class->can('result_source_instance');
  }
}

=head2 load_namespaces

=over 4

=item Arguments: %options?

=back

This is an alternative to L</load_classes> above which assumes an alternative
layout for automatic class loading.  It assumes that all result
classes are underneath a sub-namespace of the schema called C<Result>, any
corresponding ResultSet classes are underneath a sub-namespace of the schema
called C<ResultSet>.

Both of the sub-namespaces are configurable if you don't like the defaults,
via the options C<result_namespace> and C<resultset_namespace>.

If (and only if) you specify the option C<default_resultset_class>, any found
Result classes for which we do not find a corresponding
ResultSet class will have their C<resultset_class> set to
C<default_resultset_class>.

C<load_namespaces> takes care of calling C<resultset_class> for you where
neccessary if you didn't do it for yourself.

All of the namespace and classname options to this method are relative to
the schema classname by default.  To specify a fully-qualified name, prefix
it with a literal C<+>.

Examples:

  # load My::Schema::Result::CD, My::Schema::Result::Artist,
  #    My::Schema::ResultSet::CD, etc...
  My::Schema->load_namespaces;

  # Override everything to use ugly names.
  # In this example, if there is a My::Schema::Res::Foo, but no matching
  #   My::Schema::RSets::Foo, then Foo will have its
  #   resultset_class set to My::Schema::RSetBase
  My::Schema->load_namespaces(
    result_namespace => 'Res',
    resultset_namespace => 'RSets',
    default_resultset_class => 'RSetBase',
  );

  # Put things in other namespaces
  My::Schema->load_namespaces(
    result_namespace => '+Some::Place::Results',
    resultset_namespace => '+Another::Place::RSets',
  );

If you'd like to use multiple namespaces of each type, simply use an arrayref
of namespaces for that option.  In the case that the same result
(or resultset) class exists in multiple namespaces, the latter entries in
your list of namespaces will override earlier ones.

  My::Schema->load_namespaces(
    # My::Schema::Results_C::Foo takes precedence over My::Schema::Results_B::Foo :
    result_namespace => [ 'Results_A', 'Results_B', 'Results_C' ],
    resultset_namespace => [ '+Some::Place::RSets', 'RSets' ],
  );

=cut

# Pre-pends our classname to the given relative classname or
#   class namespace, unless there is a '+' prefix, which will
#   be stripped.
sub _expand_relative_name {
  my ($class, $name) = @_;
  return if !$name;
  $name = $class . '::' . $name if ! ($name =~ s/^\+//);
  return $name;
}

# returns a hash of $shortname => $fullname for every package
#  found in the given namespaces ($shortname is with the $fullname's
#  namespace stripped off)
sub _map_namespaces {
  my ($class, @namespaces) = @_;

  my @results_hash;
  foreach my $namespace (@namespaces) {
    push(
      @results_hash,
      map { (substr($_, length "${namespace}::"), $_) }
      Module::Find::findallmod($namespace)
    );
  }

  @results_hash;
}

sub load_namespaces {
  my ($class, %args) = @_;

  my $result_namespace = delete $args{result_namespace} || 'Result';
  my $resultset_namespace = delete $args{resultset_namespace} || 'ResultSet';
  my $default_resultset_class = delete $args{default_resultset_class};

  $class->throw_exception('load_namespaces: unknown option(s): '
    . join(q{,}, map { qq{'$_'} } keys %args))
      if scalar keys %args;

  $default_resultset_class
    = $class->_expand_relative_name($default_resultset_class);

  for my $arg ($result_namespace, $resultset_namespace) {
    $arg = [ $arg ] if !ref($arg) && $arg;

    $class->throw_exception('load_namespaces: namespace arguments must be '
      . 'a simple string or an arrayref')
        if ref($arg) ne 'ARRAY';

    $_ = $class->_expand_relative_name($_) for (@$arg);
  }

  my %results = $class->_map_namespaces(@$result_namespace);
  my %resultsets = $class->_map_namespaces(@$resultset_namespace);

  my @to_register;
  {
    no warnings 'redefine';
    local *Class::C3::reinitialize = sub { };
    use warnings 'redefine';

    foreach my $result (keys %results) {
      my $result_class = $results{$result};
      $class->ensure_class_loaded($result_class);
      $result_class->source_name($result) unless $result_class->source_name;

      my $rs_class = delete $resultsets{$result};
      my $rs_set = $result_class->resultset_class;
      if($rs_set && $rs_set ne 'DBIx::Class::ResultSet') {
        if($rs_class && $rs_class ne $rs_set) {
          warn "We found ResultSet class '$rs_class' for '$result', but it seems "
             . "that you had already set '$result' to use '$rs_set' instead";
        }
      }
      elsif($rs_class ||= $default_resultset_class) {
        $class->ensure_class_loaded($rs_class);
        $result_class->resultset_class($rs_class);
      }

      push(@to_register, [ $result_class->source_name, $result_class ]);
    }
  }

  foreach (sort keys %resultsets) {
    warn "load_namespaces found ResultSet class $_ with no "
      . 'corresponding Result class';
  }

  Class::C3->reinitialize;
  $class->register_class(@$_) for (@to_register);

  return;
}

=head2 compose_connection (DEPRECATED)

=over 4

=item Arguments: $target_namespace, @db_info

=item Return Value: $new_schema

=back

DEPRECATED. You probably wanted compose_namespace.

Actually, you probably just wanted to call connect.

=begin hidden

(hidden due to deprecation)

Calls L<DBIx::Class::Schema/"compose_namespace"> to the target namespace,
calls L<DBIx::Class::Schema/connection> with @db_info on the new schema,
then injects the L<DBix::Class::ResultSetProxy> component and a
resultset_instance classdata entry on all the new classes, in order to support
$target_namespaces::$class->search(...) method calls.

This is primarily useful when you have a specific need for class method access
to a connection. In normal usage it is preferred to call
L<DBIx::Class::Schema/connect> and use the resulting schema object to operate
on L<DBIx::Class::ResultSet> objects with L<DBIx::Class::Schema/resultset> for
more information.

=end hidden

=cut

{
  my $warn;

  sub compose_connection {
    my ($self, $target, @info) = @_;

    warn "compose_connection deprecated as of 0.08000"
      unless ($INC{"DBIx/Class/CDBICompat.pm"} || $warn++);

    my $base = 'DBIx::Class::ResultSetProxy';
    eval "require ${base};";
    $self->throw_exception
      ("No arguments to load_classes and couldn't load ${base} ($@)")
        if $@;
  
    if ($self eq $target) {
      # Pathological case, largely caused by the docs on early C::M::DBIC::Plain
      foreach my $moniker ($self->sources) {
        my $source = $self->source($moniker);
        my $class = $source->result_class;
        $self->inject_base($class, $base);
        $class->mk_classdata(resultset_instance => $source->resultset);
        $class->mk_classdata(class_resolver => $self);
      }
      $self->connection(@info);
      return $self;
    }
  
    my $schema = $self->compose_namespace($target, $base);
    {
      no strict 'refs';
      *{"${target}::schema"} = sub { $schema };
    }
  
    $schema->connection(@info);
    foreach my $moniker ($schema->sources) {
      my $source = $schema->source($moniker);
      my $class = $source->result_class;
      #warn "$moniker $class $source ".$source->storage;
      $class->mk_classdata(result_source_instance => $source);
      $class->mk_classdata(resultset_instance => $source->resultset);
      $class->mk_classdata(class_resolver => $schema);
    }
    return $schema;
  }
}

=head2 compose_namespace

=over 4

=item Arguments: $target_namespace, $additional_base_class?

=item Return Value: $new_schema

=back

For each L<DBIx::Class::ResultSource> in the schema, this method creates a
class in the target namespace (e.g. $target_namespace::CD,
$target_namespace::Artist) that inherits from the corresponding classes
attached to the current schema.

It also attaches a corresponding L<DBIx::Class::ResultSource> object to the
new $schema object. If C<$additional_base_class> is given, the new composed
classes will inherit from first the corresponding classe from the current
schema then the base class.

For example, for a schema with My::Schema::CD and My::Schema::Artist classes,

  $schema->compose_namespace('My::DB', 'Base::Class');
  print join (', ', @My::DB::CD::ISA) . "\n";
  print join (', ', @My::DB::Artist::ISA) ."\n";

will produce the output

  My::Schema::CD, Base::Class
  My::Schema::Artist, Base::Class

=cut

sub compose_namespace {
  my ($self, $target, $base) = @_;
  my $schema = $self->clone;
  {
    no warnings qw/redefine/;
    local *Class::C3::reinitialize = sub { };
    foreach my $moniker ($schema->sources) {
      my $source = $schema->source($moniker);
      my $target_class = "${target}::${moniker}";
      $self->inject_base(
        $target_class => $source->result_class, ($base ? $base : ())
      );
      $source->result_class($target_class);
      $target_class->result_source_instance($source)
        if $target_class->can('result_source_instance');
    }
  }
  Class::C3->reinitialize();
  {
    no strict 'refs';
    no warnings 'redefine';
    foreach my $meth (qw/class source resultset/) {
      *{"${target}::${meth}"} =
        sub { shift->schema->$meth(@_) };
    }
  }
  return $schema;
}

=head2 setup_connection_class

=over 4

=item Arguments: $target, @info

=back

Sets up a database connection class to inject between the schema and the
subclasses that the schema creates.

=cut

sub setup_connection_class {
  my ($class, $target, @info) = @_;
  $class->inject_base($target => 'DBIx::Class::DB');
  #$target->load_components('DB');
  $target->connection(@info);
}

=head2 storage_type

=over 4

=item Arguments: $storage_type

=item Return Value: $storage_type

=back

Set the storage class that will be instantiated when L</connect> is called.
If the classname starts with C<::>, the prefix C<DBIx::Class::Storage> is
assumed by L</connect>.  Defaults to C<::DBI>,
which is L<DBIx::Class::Storage::DBI>.

You want to use this to hardcoded subclasses of L<DBIx::Class::Storage::DBI>
in cases where the appropriate subclass is not autodetected, such as when
dealing with MSSQL via L<DBD::Sybase>, in which case you'd set it to
C<::DBI::Sybase::MSSQL>.

=head2 connection

=over 4

=item Arguments: @args

=item Return Value: $new_schema

=back

Instantiates a new Storage object of type
L<DBIx::Class::Schema/"storage_type"> and passes the arguments to
$storage->connect_info. Sets the connection in-place on the schema.

See L<DBIx::Class::Storage::DBI/"connect_info"> for DBI-specific syntax,
or L<DBIx::Class::Storage> in general.

=cut

sub connection {
  my ($self, @info) = @_;
  return $self if !@info && $self->storage;
  my $storage_class = $self->storage_type;
  $storage_class = 'DBIx::Class::Storage'.$storage_class
    if $storage_class =~ m/^::/;
  eval "require ${storage_class};";
  $self->throw_exception(
    "No arguments to load_classes and couldn't load ${storage_class} ($@)"
  ) if $@;
  my $storage = $storage_class->new($self);
  $storage->connect_info(\@info);
  $self->storage($storage);
  return $self;
}

=head2 connect

=over 4

=item Arguments: @info

=item Return Value: $new_schema

=back

This is a convenience method. It is equivalent to calling
$schema->clone->connection(@info). See L</connection> and L</clone> for more
information.

=cut

sub connect { shift->clone->connection(@_) }

=head2 txn_do

=over 4

=item Arguments: C<$coderef>, @coderef_args?

=item Return Value: The return value of $coderef

=back

Executes C<$coderef> with (optional) arguments C<@coderef_args> atomically,
returning its result (if any). Equivalent to calling $schema->storage->txn_do.
See L<DBIx::Class::Storage/"txn_do"> for more information.

This interface is preferred over using the individual methods L</txn_begin>,
L</txn_commit>, and L</txn_rollback> below.

=cut

sub txn_do {
  my $self = shift;

  $self->storage or $self->throw_exception
    ('txn_do called on $schema without storage');

  $self->storage->txn_do(@_);
}

sub txn_scope_guard {
  my $self = shift;

  $self->storage or $self->throw_exception
    ('txn_scope_guard called on $schema without storage');

  $self->storage->txn_scope_guard(@_);
}

=head2 txn_begin

Begins a transaction (does nothing if AutoCommit is off). Equivalent to
calling $schema->storage->txn_begin. See
L<DBIx::Class::Storage::DBI/"txn_begin"> for more information.

=cut

sub txn_begin {
  my $self = shift;

  $self->storage or $self->throw_exception
    ('txn_begin called on $schema without storage');

  $self->storage->txn_begin;
}

=head2 txn_commit

Commits the current transaction. Equivalent to calling
$schema->storage->txn_commit. See L<DBIx::Class::Storage::DBI/"txn_commit">
for more information.

=cut

sub txn_commit {
  my $self = shift;

  $self->storage or $self->throw_exception
    ('txn_commit called on $schema without storage');

  $self->storage->txn_commit;
}

=head2 txn_rollback

Rolls back the current transaction. Equivalent to calling
$schema->storage->txn_rollback. See
L<DBIx::Class::Storage::DBI/"txn_rollback"> for more information.

=cut

sub txn_rollback {
  my $self = shift;

  $self->storage or $self->throw_exception
    ('txn_rollback called on $schema without storage');

  $self->storage->txn_rollback;
}

=head2 clone

=over 4

=item Return Value: $new_schema

=back

Clones the schema and its associated result_source objects and returns the
copy.

=cut

sub clone {
  my ($self) = @_;
  my $clone = { (ref $self ? %$self : ()) };
  bless $clone, (ref $self || $self);

  foreach my $moniker ($self->sources) {
    my $source = $self->source($moniker);
    my $new = $source->new($source);
    $clone->register_source($moniker => $new);
  }
  $clone->storage->set_schema($clone) if $clone->storage;
  return $clone;
}

=head2 populate

=over 4

=item Arguments: $source_name, \@data;

=back

Pass this method a resultsource name, and an arrayref of
arrayrefs. The arrayrefs should contain a list of column names,
followed by one or many sets of matching data for the given columns. 

In void context, C<insert_bulk> in L<DBIx::Class::Storage::DBI> is used
to insert the data, as this is a fast method. However, insert_bulk currently
assumes that your datasets all contain the same type of values, using scalar
references in a column in one row, and not in another will probably not work.

Otherwise, each set of data is inserted into the database using
L<DBIx::Class::ResultSet/create>, and a arrayref of the resulting row
objects is returned.

i.e.,

  $schema->populate('Artist', [
    [ qw/artistid name/ ],
    [ 1, 'Popular Band' ],
    [ 2, 'Indie Band' ],
    ...
  ]);
  
Since wantarray context is basically the same as looping over $rs->create(...) 
you won't see any performance benefits and in this case the method is more for
convenience. Void context sends the column information directly to storage
using <DBI>s bulk insert method. So the performance will be much better for 
storages that support this method.

Because of this difference in the way void context inserts rows into your 
database you need to note how this will effect any loaded components that
override or augment insert.  For example if you are using a component such 
as L<DBIx::Class::UUIDColumns> to populate your primary keys you MUST use 
wantarray context if you want the PKs automatically created.

=cut

sub populate {
  my ($self, $name, $data) = @_;
  my $rs = $self->resultset($name);
  my @names = @{shift(@$data)};
  if(defined wantarray) {
    my @created;
    foreach my $item (@$data) {
      my %create;
      @create{@names} = @$item;
      push(@created, $rs->create(\%create));
    }
    return @created;
  }
  my @results_to_create;
  foreach my $datum (@$data) {
    my %result_to_create;
    foreach my $index (0..$#names) {
      $result_to_create{$names[$index]} = $$datum[$index];
    }
    push @results_to_create, \%result_to_create;
  }
  $rs->populate(\@results_to_create);
}

=head2 exception_action

=over 4

=item Arguments: $code_reference

=back

If C<exception_action> is set for this class/object, L</throw_exception>
will prefer to call this code reference with the exception as an argument,
rather than its normal C<croak> or C<confess> action.

Your subroutine should probably just wrap the error in the exception
object/class of your choosing and rethrow.  If, against all sage advice,
you'd like your C<exception_action> to suppress a particular exception
completely, simply have it return true.

Example:

   package My::Schema;
   use base qw/DBIx::Class::Schema/;
   use My::ExceptionClass;
   __PACKAGE__->exception_action(sub { My::ExceptionClass->throw(@_) });
   __PACKAGE__->load_classes;

   # or:
   my $schema_obj = My::Schema->connect( .... );
   $schema_obj->exception_action(sub { My::ExceptionClass->throw(@_) });

   # suppress all exceptions, like a moron:
   $schema_obj->exception_action(sub { 1 });

=head2 stacktrace

=over 4

=item Arguments: boolean

=back

Whether L</throw_exception> should include stack trace information.
Defaults to false normally, but defaults to true if C<$ENV{DBIC_TRACE}>
is true.

=head2 throw_exception

=over 4

=item Arguments: $message

=back

Throws an exception. Defaults to using L<Carp::Clan> to report errors from
user's perspective.  See L</exception_action> for details on overriding
this method's behavior.  If L</stacktrace> is turned on, C<throw_exception>'s
default behavior will provide a detailed stack trace.

=cut

sub throw_exception {
  my $self = shift;

  DBIx::Class::Exception->throw($_[0], $self->stacktrace)
    if !$self->exception_action || !$self->exception_action->(@_);
}

=head2 deploy

=over 4

=item Arguments: $sqlt_args, $dir

=back

Attempts to deploy the schema to the current storage using L<SQL::Translator>.

See L<SQL::Translator/METHODS> for a list of values for C<$sqlt_args>. The most
common value for this would be C<< { add_drop_table => 1, } >> to have the SQL
produced include a DROP TABLE statement for each table created.

Additionally, the DBIx::Class parser accepts a C<sources> parameter as a hash 
ref or an array ref, containing a list of source to deploy. If present, then 
only the sources listed will get deployed.

=cut

sub deploy {
  my ($self, $sqltargs, $dir) = @_;
  $self->throw_exception("Can't deploy without storage") unless $self->storage;
  $self->storage->deploy($self, undef, $sqltargs, $dir);
}

=head2 create_ddl_dir (EXPERIMENTAL)

=over 4

=item Arguments: \@databases, $version, $directory, $preversion, $sqlt_args

=back

Creates an SQL file based on the Schema, for each of the specified
database types, in the given directory. Given a previous version number,
this will also create a file containing the ALTER TABLE statements to
transform the previous schema into the current one. Note that these
statements may contain DROP TABLE or DROP COLUMN statements that can
potentially destroy data.

The file names are created using the C<ddl_filename> method below, please
override this method in your schema if you would like a different file
name format. For the ALTER file, the same format is used, replacing
$version in the name with "$preversion-$version".

If no arguments are passed, then the following default values are used:

=over 4

=item databases  - ['MySQL', 'SQLite', 'PostgreSQL']

=item version    - $schema->VERSION

=item directory  - './'

=item preversion - <none>

=back

Note that this feature is currently EXPERIMENTAL and may not work correctly
across all databases, or fully handle complex relationships.

WARNING: Please check all SQL files created, before applying them.

=cut

sub create_ddl_dir {
  my $self = shift;

  $self->throw_exception("Can't create_ddl_dir without storage") unless $self->storage;
  $self->storage->create_ddl_dir($self, @_);
}

=head2 ddl_filename (EXPERIMENTAL)

=over 4

=item Arguments: $directory, $database-type, $version, $preversion

=back

  my $filename = $table->ddl_filename($type, $dir, $version, $preversion)

This method is called by C<create_ddl_dir> to compose a file name out of
the supplied directory, database type and version number. The default file
name format is: C<$dir$schema-$version-$type.sql>.

You may override this method in your schema if you wish to use a different
format.

=cut

sub ddl_filename {
    my ($self, $type, $dir, $version, $pversion) = @_;

    my $filename = ref($self);
    $filename =~ s/::/-/g;
    $filename = File::Spec->catfile($dir, "$filename-$version-$type.sql");
    $filename =~ s/$version/$pversion-$version/ if($pversion);

    return $filename;
}

=head2 sqlt_deploy_hook($sqlt_schema)

An optional sub which you can declare in your own Schema class that will get 
passed the L<SQL::Translator::Schema> object when you deploy the schema via
L</create_ddl_dir> or L</deploy>.

For an example of what you can do with this, see 
L<DBIx::Class::Manual::Cookbook/Adding Indexes And Functions To Your SQL>.

=head2 thaw

Provided as the recommened way of thawing schema objects. You can call 
C<Storable::thaw> directly if you wish, but the thawed objects will not have a
reference to any schema, so are rather useless

=cut

sub thaw {
  my ($self, $obj) = @_;
  local $DBIx::Class::ResultSourceHandle::thaw_schema = $self;
  return Storable::thaw($obj);
}

=head2 freeze

This doesn't actualy do anything more than call L<Storable/freeze>, it is just
provided here for symetry.

=cut

sub freeze {
  return Storable::freeze($_[1]);
}

=head2 dclone

Recommeneded way of dcloning objects. This is needed to properly maintain
references to the schema object (which itself is B<not> cloned.)

=cut

sub dclone {
  my ($self, $obj) = @_;
  local $DBIx::Class::ResultSourceHandle::thaw_schema = $self;
  return Storable::dclone($obj);
}

1;

=head1 AUTHORS

Matt S. Trout <mst@shadowcatsystems.co.uk>

=head1 LICENSE

You may distribute this code under the same terms as Perl itself.

=cut
