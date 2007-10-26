package DBIx::Class::Schema::Loader::Base;

use strict;
use warnings;
use base qw/Class::Accessor::Fast/;
use Class::C3;
use Carp::Clan qw/^DBIx::Class/;
use UNIVERSAL::require;
use DBIx::Class::Schema::Loader::RelBuilder;
use Data::Dump qw/ dump /;
use POSIX qw//;
use File::Spec qw//;
require DBIx::Class;

__PACKAGE__->mk_ro_accessors(qw/
                                schema
                                schema_class

                                exclude
                                constraint
                                additional_classes
                                additional_base_classes
                                left_base_classes
                                components
                                resultset_components
                                relationships
                                moniker_map
                                inflect_singular
                                inflect_plural
                                debug
                                dump_directory
                                dump_overwrite

                                legacy_default_inflections

                                db_schema
                                _tables
                                classes
                                monikers
                             /);

=head1 NAME

DBIx::Class::Schema::Loader::Base - Base DBIx::Class::Schema::Loader Implementation.

=head1 SYNOPSIS

See L<DBIx::Class::Schema::Loader>

=head1 DESCRIPTION

This is the base class for the storage-specific C<DBIx::Class::Schema::*>
classes, and implements the common functionality between them.

=head1 CONSTRUCTOR OPTIONS

These constructor options are the base options for
L<DBIx::Class::Schema::Loader/loader_opts>.  Available constructor options are:

=head2 relationships

Try to automatically detect/setup has_a and has_many relationships.

=head2 debug

If set to true, each constructive L<DBIx::Class> statement the loader
decides to execute will be C<warn>-ed before execution.

=head2 db_schema

Set the name of the schema to load (schema in the sense that your database
vendor means it).  Does not currently support loading more than one schema
name.

=head2 constraint

Only load tables matching regex.  Best specified as a qr// regex.

=head2 exclude

Exclude tables matching regex.  Best specified as a qr// regex.

=head2 moniker_map

Overrides the default table name to moniker translation.  Can be either
a hashref of table keys and moniker values, or a coderef for a translator
function taking a single scalar table name argument and returning
a scalar moniker.  If the hash entry does not exist, or the function
returns a false value, the code falls back to default behavior
for that table name.

The default behavior is: C<join '', map ucfirst, split /[\W_]+/, lc $table>,
which is to say: lowercase everything, split up the table name into chunks
anywhere a non-alpha-numeric character occurs, change the case of first letter
of each chunk to upper case, and put the chunks back together.  Examples:

    Table Name  | Moniker Name
    ---------------------------
    luser       | Luser
    luser_group | LuserGroup
    luser-opts  | LuserOpts

=head2 inflect_plural

Just like L</moniker_map> above (can be hash/code-ref, falls back to default
if hash key does not exist or coderef returns false), but acts as a map
for pluralizing relationship names.  The default behavior is to utilize
L<Lingua::EN::Inflect::Number/to_PL>.

=head2 inflect_singular

As L</inflect_plural> above, but for singularizing relationship names.
Default behavior is to utilize L<Lingua::EN::Inflect::Number/to_S>.

=head2 additional_base_classes

List of additional base classes all of your table classes will use.

=head2 left_base_classes

List of additional base classes all of your table classes will use
that need to be leftmost.

=head2 additional_classes

List of additional classes which all of your table classes will use.

=head2 components

List of additional components to be loaded into all of your table
classes.  A good example would be C<ResultSetManager>.

=head2 resultset_components

List of additional ResultSet components to be loaded into your table
classes.  A good example would be C<AlwaysRS>.  Component
C<ResultSetManager> will be automatically added to the above
C<components> list if this option is set.

=head2 legacy_default_inflections

Setting this option changes the default fallback for L</inflect_plural> to
utilize L<Lingua::EN::Inflect/PL>, and L</inflect_singular> to a no-op.
Those choices produce substandard results, but might be necessary to support
your existing code if you started developing on a version prior to 0.03 and
don't wish to go around updating all your relationship names to the new
defaults.

This option will continue to be supported until at least version 0.05xxx,
but may dissappear sometime thereafter.  It is recommended that you update
your code to use the newer-style inflections when you have the time.

=head2 dump_directory

This option is designed to be a tool to help you transition from this
loader to a manually-defined schema when you decide it's time to do so.

The value of this option is a perl libdir pathname.  Within
that directory this module will create a baseline manual
L<DBIx::Class::Schema> module set, based on what it creates at runtime
in memory.

The created schema class will have the same classname as the one on
which you are setting this option (and the ResultSource classes will be
based on this name as well).  Therefore it is wise to note that if you
point the C<dump_directory> option of a schema class at the live libdir
where that class is currently located, it will overwrite itself with a
manual version of itself.  This might be a really good or bad thing
depending on your situation and perspective.

Normally you wouldn't hard-code this setting in your schema class, as it
is meant for one-time manual usage.

See L<DBIx::Class::Schema::Loader/dump_to_dir> for examples of the
recommended way to access this functionality.

=head2 dump_overwrite

If set to a true value, the dumping code will overwrite existing files.
The default is false, which means the dumping code will skip the already
existing files.

=head1 DEPRECATED CONSTRUCTOR OPTIONS

B<These will be removed in version 0.04000 !!!>

=head2 inflect_map

Equivalent to L</inflect_plural>.

=head2 inflect

Equivalent to L</inflect_plural>.

=head2 connect_info, dsn, user, password, options

You connect these schemas the same way you would any L<DBIx::Class::Schema>,
which is by calling either C<connect> or C<connection> on a schema class
or object.  These options are only supported via the deprecated
C<load_from_connection> interface, which is also being removed in 0.04000.

=head1 METHODS

None of these methods are intended for direct invocation by regular
users of L<DBIx::Class::Schema::Loader>.  Anything you can find here
can also be found via standard L<DBIx::Class::Schema> methods somehow.

=cut

# ensure that a peice of object data is a valid arrayref, creating
# an empty one or encapsulating whatever's there.
sub _ensure_arrayref {
    my $self = shift;

    foreach (@_) {
        $self->{$_} ||= [];
        $self->{$_} = [ $self->{$_} ]
            unless ref $self->{$_} eq 'ARRAY';
    }
}

=head2 new

Constructor for L<DBIx::Class::Schema::Loader::Base>, used internally
by L<DBIx::Class::Schema::Loader>.

=cut

sub new {
    my ( $class, %args ) = @_;

    my $self = { %args };

    bless $self => $class;

    $self->{db_schema}  ||= '';
    $self->_ensure_arrayref(qw/additional_classes
                               additional_base_classes
                               left_base_classes
                               components
                               resultset_components
                              /);

    push(@{$self->{components}}, 'ResultSetManager')
        if @{$self->{resultset_components}};

    $self->{monikers} = {};
    $self->{classes} = {};

    # Support deprecated arguments
    for(qw/inflect_map inflect/) {
        warn "Argument $_ is deprecated in favor of 'inflect_plural'"
           . ", and will be removed in 0.04000"
                if $self->{$_};
    }
    $self->{inflect_plural} ||= $self->{inflect_map} || $self->{inflect};

    $self->{schema_class} ||= ( ref $self->{schema} || $self->{schema} );
    $self->{schema} ||= $self->{schema_class};

    $self;
}

sub _load_external {
    my $self = shift;

    my $abs_dump_dir;

    $abs_dump_dir = File::Spec->rel2abs($self->dump_directory)
        if $self->dump_directory;

    foreach my $table_class (values %{$self->classes}) {
        $table_class->require;
        if($@ && $@ !~ /^Can't locate /) {
            croak "Failed to load external class definition"
                  . " for '$table_class': $@";
        }
        next if $@; # "Can't locate" error

        # If we make it to here, we loaded an external definition
        warn qq/# Loaded external class definition for '$table_class'\n/
            if $self->debug;

        if($abs_dump_dir) {
            my $class_path = $table_class;
            $class_path =~ s{::}{/}g;
            $class_path .= '.pm';
            my $filename = File::Spec->rel2abs($INC{$class_path});
            croak 'Failed to locate actual external module file for '
                  . "'$table_class'"
                      if !$filename;
            next if($filename =~ /^$abs_dump_dir/);
            open(my $fh, '<', $filename)
                or croak "Failed to open $filename for reading: $!";
            $self->_raw_stmt($table_class,
                q|# These lines loaded from user-supplied external file: |
            );
            while(<$fh>) {
                chomp;
                $self->_raw_stmt($table_class, $_);
            }
            $self->_raw_stmt($table_class,
                q|# End of lines loaded from user-supplied external file |
            );
            close($fh)
                or croak "Failed to close $filename: $!";
        }
    }
}

=head2 load

Does the actual schema-construction work.

=cut

sub load {
    my $self = shift;

    $self->_load_classes;
    $self->_load_relationships if $self->relationships;
    $self->_load_external;
    $self->_dump_to_dir if $self->dump_directory;

    # Drop temporary cache
    delete $self->{_cache};

    1;
}

sub _get_dump_filename {
    my ($self, $class) = (@_);

    $class =~ s{::}{/}g;
    return $self->dump_directory . q{/} . $class . q{.pm};
}

sub _ensure_dump_subdirs {
    my ($self, $class) = (@_);

    my @name_parts = split(/::/, $class);
    pop @name_parts; # we don't care about the very last element,
                     # which is a filename

    my $dir = $self->dump_directory;
    foreach (@name_parts) {
        $dir = File::Spec->catdir($dir,$_);
        if(! -d $dir) {
            mkdir($dir) or croak "mkdir('$dir') failed: $!";
        }
    }
}

sub _dump_to_dir {
    my ($self) = @_;

    my $target_dir = $self->dump_directory;

    my $schema_class = $self->schema_class;

    croak "Must specify target directory for dumping!" if ! $target_dir;

    warn "Dumping manual schema for $schema_class to directory $target_dir ...\n";

    if(! -d $target_dir) {
        mkdir($target_dir) or croak "mkdir('$target_dir') failed: $!";
    }

    my $verstr = $DBIx::Class::Schema::Loader::VERSION;
    my $datestr = POSIX::strftime('%Y-%m-%d %H:%M:%S', localtime);
    my $tagline = qq|# Created by DBIx::Class::Schema::Loader v$verstr @ $datestr|;

    $self->_ensure_dump_subdirs($schema_class);

    my $schema_fn = $self->_get_dump_filename($schema_class);
    if (-f $schema_fn && !$self->dump_overwrite) {
        warn "$schema_fn exists, will not overwrite\n";
    }
    else {
        open(my $schema_fh, '>', $schema_fn)
            or croak "Cannot open $schema_fn for writing: $!";
        print $schema_fh qq|package $schema_class;\n\n$tagline\n\n|;
        print $schema_fh qq|use strict;\nuse warnings;\n\n|;
        print $schema_fh qq|use base 'DBIx::Class::Schema';\n\n|;
        print $schema_fh qq|__PACKAGE__->load_classes;\n|;
        print $schema_fh qq|\n1;\n\n|;
        close($schema_fh)
            or croak "Cannot close $schema_fn: $!";
    }

    foreach my $src_class (sort keys %{$self->{_dump_storage}}) {
        $self->_ensure_dump_subdirs($src_class);
        my $src_fn = $self->_get_dump_filename($src_class);
        if (-f $src_fn && !$self->dump_overwrite) {
            warn "$src_fn exists, will not overwrite\n";
            next;
        }    
        open(my $src_fh, '>', $src_fn)
            or croak "Cannot open $src_fn for writing: $!";
        print $src_fh qq|package $src_class;\n\n$tagline\n\n|;
        print $src_fh qq|use strict;\nuse warnings;\n\n|;
        print $src_fh qq|use base 'DBIx::Class';\n\n|;
        print $src_fh qq|$_\n|
            for @{$self->{_dump_storage}->{$src_class}};
        print $src_fh qq|\n1;\n\n|;
        close($src_fh)
            or croak "Cannot close $src_fn: $!";
    }

    warn "Schema dump completed.\n";
}

sub _use {
    my $self = shift;
    my $target = shift;
    my $evalstr;

    foreach (@_) {
        warn "$target: use $_;" if $self->debug;
        $self->_raw_stmt($target, "use $_;");
        $_->require or croak ($_ . "->require: $@");
        $evalstr .= "package $target; use $_;";
    }
    eval $evalstr if $evalstr;
    croak $@ if $@;
}

sub _inject {
    my $self = shift;
    my $target = shift;
    my $schema_class = $self->schema_class;

    my $blist = join(q{ }, @_);
    warn "$target: use base qw/ $blist /;" if $self->debug && @_;
    $self->_raw_stmt($target, "use base qw/ $blist /;") if @_;
    foreach (@_) {
        $_->require or croak ($_ . "->require: $@");
        $schema_class->inject_base($target, $_);
    }
}

# Load and setup classes
sub _load_classes {
    my $self = shift;

    my $schema       = $self->schema;
    my $schema_class = $self->schema_class;
    my $constraint   = $self->constraint;
    my $exclude      = $self->exclude;
    my @tables       = sort $self->_tables_list;

    warn "No tables found in database, nothing to load" if !@tables;

    if(@tables) {
        @tables = grep { /$constraint/ } @tables if $constraint;
        @tables = grep { ! /$exclude/ } @tables if $exclude;

        warn "All tables excluded by constraint/exclude, nothing to load"
            if !@tables;
    }

    $self->{_tables} = \@tables;

    foreach my $table (@tables) {
        my $table_moniker = $self->_table2moniker($table);
        my $table_class = $schema_class . q{::} . $table_moniker;

        my $table_normalized = lc $table;
        $self->classes->{$table} = $table_class;
        $self->classes->{$table_normalized} = $table_class;
        $self->monikers->{$table} = $table_moniker;
        $self->monikers->{$table_normalized} = $table_moniker;

        no warnings 'redefine';
        local *Class::C3::reinitialize = sub { };
        use warnings;

        { no strict 'refs'; @{"${table_class}::ISA"} = qw/DBIx::Class/ }

        $self->_use   ($table_class, @{$self->additional_classes});
        $self->_inject($table_class, @{$self->additional_base_classes});

        $self->_dbic_stmt($table_class, 'load_components', @{$self->components}, qw/PK::Auto Core/);

        $self->_dbic_stmt($table_class, 'load_resultset_components', @{$self->resultset_components})
            if @{$self->resultset_components};
        $self->_inject($table_class, @{$self->left_base_classes});
    }

    Class::C3::reinitialize;

    foreach my $table (@tables) {
        my $table_class = $self->classes->{$table};
        my $table_moniker = $self->monikers->{$table};

        $self->_dbic_stmt($table_class,'table',$table);

        my $cols = $self->_table_columns($table);
        my $col_info;
        eval { $col_info = $schema->storage->columns_info_for($table) };
        if($@) {
            $self->_dbic_stmt($table_class,'add_columns',@$cols);
        }
        else {
            my %col_info_lc = map { lc($_), $col_info->{$_} } keys %$col_info;
            $self->_dbic_stmt(
                $table_class,
                'add_columns',
                map { $_, ($col_info_lc{$_}||{}) } @$cols
            );
        }

        my $pks = $self->_table_pk_info($table) || [];
        @$pks ? $self->_dbic_stmt($table_class,'set_primary_key',@$pks)
              : carp("$table has no primary key");

        my $uniqs = $self->_table_uniq_info($table) || [];
        $self->_dbic_stmt($table_class,'add_unique_constraint',@$_) for (@$uniqs);

        $schema_class->register_class($table_moniker, $table_class);
        $schema->register_class($table_moniker, $table_class) if $schema ne $schema_class;
    }
}

=head2 tables

Returns a sorted list of loaded tables, using the original database table
names.

=cut

sub tables {
    my $self = shift;

    return @{$self->_tables};
}

# Make a moniker from a table
sub _table2moniker {
    my ( $self, $table ) = @_;

    my $moniker;

    if( ref $self->moniker_map eq 'HASH' ) {
        $moniker = $self->moniker_map->{$table};
    }
    elsif( ref $self->moniker_map eq 'CODE' ) {
        $moniker = $self->moniker_map->($table);
    }

    $moniker ||= join '', map ucfirst, split /[\W_]+/, lc $table;

    return $moniker;
}

sub _load_relationships {
    my $self = shift;

    # Construct the fk_info RelBuilder wants to see, by
    # translating table names to monikers in the _fk_info output
    my %fk_info;
    foreach my $table ($self->tables) {
        my $tbl_fk_info = $self->_table_fk_info($table);
        foreach my $fkdef (@$tbl_fk_info) {
            $fkdef->{remote_source} =
                $self->monikers->{delete $fkdef->{remote_table}};
        }
        my $moniker = $self->monikers->{$table};
        $fk_info{$moniker} = $tbl_fk_info;
    }

    my $relbuilder = DBIx::Class::Schema::Loader::RelBuilder->new(
        $self->schema_class, \%fk_info, $self->inflect_plural,
        $self->inflect_singular
    );

    my $rel_stmts = $relbuilder->generate_code;
    foreach my $src_class (sort keys %$rel_stmts) {
        my $src_stmts = $rel_stmts->{$src_class};
        foreach my $stmt (@$src_stmts) {
            $self->_dbic_stmt($src_class,$stmt->{method},@{$stmt->{args}});
        }
    }
}

# Overload these in driver class:

# Returns an arrayref of column names
sub _table_columns { croak "ABSTRACT METHOD" }

# Returns arrayref of pk col names
sub _table_pk_info { croak "ABSTRACT METHOD" }

# Returns an arrayref of uniqs [ [ foo => [ col1, col2 ] ], [ bar => [ ... ] ] ]
sub _table_uniq_info { croak "ABSTRACT METHOD" }

# Returns an arrayref of foreign key constraints, each
#   being a hashref with 3 keys:
#   local_columns (arrayref), remote_columns (arrayref), remote_table
sub _table_fk_info { croak "ABSTRACT METHOD" }

# Returns an array of lower case table names
sub _tables_list { croak "ABSTRACT METHOD" }

# Execute a constructive DBIC class method, with debug/dump_to_dir hooks.
sub _dbic_stmt {
    my $self = shift;
    my $class = shift;
    my $method = shift;

    if(!$self->debug && !$self->dump_directory) {
        $class->$method(@_);
        return;
    }

    my $args = dump(@_);
    $args = '(' . $args . ')' if @_ < 2;
    my $stmt = $method . $args . q{;};

    warn qq|$class\->$stmt\n| if $self->debug;
    $class->$method(@_);
    $self->_raw_stmt($class, '__PACKAGE__->' . $stmt);
}

# Store a raw source line for a class (for dumping purposes)
sub _raw_stmt {
    my ($self, $class, $stmt) = @_;
    push(@{$self->{_dump_storage}->{$class}}, $stmt) if $self->dump_directory;
}

=head2 monikers

Returns a hashref of loaded table to moniker mappings.  There will
be two entries for each table, the original name and the "normalized"
name, in the case that the two are different (such as databases
that like uppercase table names, or preserve your original mixed-case
definitions, or what-have-you).

=head2 classes

Returns a hashref of table to class mappings.  In some cases it will
contain multiple entries per table for the original and normalized table
names, as above in L</monikers>.

=head1 SEE ALSO

L<DBIx::Class::Schema::Loader>

=cut

1;
