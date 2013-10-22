package DBIx::Class::Schema::Loader::Base;

use strict;
use warnings;
use base qw/Class::Accessor::Grouped Class::C3::Componentised/;
use Class::C3;
use Carp::Clan qw/^DBIx::Class/;
use DBIx::Class::Schema::Loader::RelBuilder;
use Data::Dump qw/ dump /;
use POSIX qw//;
use File::Spec qw//;
use Cwd qw//;
use Digest::MD5 qw//;
use Lingua::EN::Inflect::Number qw//;
use File::Temp qw//;
use Class::Unload;
use Class::Inspector ();
require DBIx::Class;

our $VERSION = '0.05003';

__PACKAGE__->mk_group_ro_accessors('simple', qw/
                                schema
                                schema_class

                                exclude
                                constraint
                                additional_classes
                                additional_base_classes
                                left_base_classes
                                components
                                resultset_components
                                skip_relationships
                                skip_load_external
                                moniker_map
                                custom_column_info
                                inflect_singular
                                inflect_plural
                                debug
                                dump_directory
                                dump_overwrite
                                really_erase_my_files
                                resultset_namespace
                                default_resultset_class
                                schema_base_class
                                result_base_class
				overwrite_modifications

                                relationship_attrs

                                db_schema
                                _tables
                                classes
                                _upgrading_classes
                                monikers
                                dynamic
                                naming
                                datetime_timezone
                                datetime_locale
/);


__PACKAGE__->mk_group_accessors('simple', qw/
                                version_to_dump
                                schema_version_to_dump
                                _upgrading_from
                                _upgrading_from_load_classes
                                _downgrading_to_load_classes
                                _rewriting_result_namespace
                                use_namespaces
                                result_namespace
                                generate_pod
                                pod_comment_mode
                                pod_comment_spillover_length
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
L<DBIx::Class::Schema::Loader/loader_options>.  Available constructor options are:

=head2 skip_relationships

Skip setting up relationships.  The default is to attempt the loading
of relationships.

=head2 skip_load_external

Skip loading of other classes in @INC. The default is to merge all other classes
with the same name found in @INC into the schema file we are creating.

=head2 naming

Static schemas (ones dumped to disk) will, by default, use the new-style 0.05XXX
relationship names and singularized Results, unless you're overwriting an
existing dump made by a 0.04XXX version of L<DBIx::Class::Schema::Loader>, in
which case the backward compatible RelBuilder will be activated, and
singularization will be turned off.

Specifying

    naming => 'v5'

will disable the backward-compatible RelBuilder and use
the new-style relationship names along with singularized Results, even when
overwriting a dump made with an earlier version.

The option also takes a hashref:

    naming => { relationships => 'v5', monikers => 'v4' }

The keys are:

=over 4

=item relationships

How to name relationship accessors.

=item monikers

How to name Result classes.

=back

The values can be:

=over 4

=item current

Latest default style, whatever that happens to be.

=item v5

Version 0.05XXX style.

=item v4

Version 0.04XXX style.

=back

Dynamic schemas will always default to the 0.04XXX relationship names and won't
singularize Results for backward compatibility, to activate the new RelBuilder
and singularization put this in your C<Schema.pm> file:

    __PACKAGE__->naming('current');

Or if you prefer to use 0.05XXX features but insure that nothing breaks in the
next major version upgrade:

    __PACKAGE__->naming('v5');

=head2 generate_pod

By default POD will be generated for columns and relationships, using database
metadata for the text if available and supported.

Reading database metadata (e.g. C<COMMENT ON TABLE some_table ...>) is only
supported for Postgres right now.

Set this to C<0> to turn off all POD generation.

=head2 pod_comment_mode

Controls where table comments appear in the generated POD. Smaller table
comments are appended to the C<NAME> section of the documentation, and larger
ones are inserted into C<DESCRIPTION> instead. You can force a C<DESCRIPTION>
section to be generated with the comment always, only use C<NAME>, or choose
the length threshold at which the comment is forced into the description.

=over 4

=item name

Use C<NAME> section only.

=item description

Force C<DESCRIPTION> always.

=item auto

Use C<DESCRIPTION> if length > L</pod_comment_spillover_length>, this is the
default.

=back

=head2 pod_comment_spillover_length

When pod_comment_mode is set to C<auto>, this is the length of the comment at
which it will be forced into a separate description section.

The default is C<60>

=head2 relationship_attrs

Hashref of attributes to pass to each generated relationship, listed
by type.  Also supports relationship type 'all', containing options to
pass to all generated relationships.  Attributes set for more specific
relationship types override those set in 'all'.

For example:

  relationship_attrs => {
    all      => { cascade_delete => 0 },
    has_many => { cascade_delete => 1 },
  },

will set the C<cascade_delete> option to 0 for all generated relationships,
except for C<has_many>, which will have cascade_delete as 1.

NOTE: this option is not supported if v4 backward-compatible naming is
set either globally (naming => 'v4') or just for relationships.

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

The default behavior is to singularize the table name, and: C<join '', map
ucfirst, split /[\W_]+/, lc $table>, which is to say: lowercase everything,
split up the table name into chunks anywhere a non-alpha-numeric character
occurs, change the case of first letter of each chunk to upper case, and put
the chunks back together.  Examples:

    Table Name  | Moniker Name
    ---------------------------
    luser       | Luser
    luser_group | LuserGroup
    luser-opts  | LuserOpt

=head2 inflect_plural

Just like L</moniker_map> above (can be hash/code-ref, falls back to default
if hash key does not exist or coderef returns false), but acts as a map
for pluralizing relationship names.  The default behavior is to utilize
L<Lingua::EN::Inflect::Number/to_PL>.

=head2 inflect_singular

As L</inflect_plural> above, but for singularizing relationship names.
Default behavior is to utilize L<Lingua::EN::Inflect::Number/to_S>.

=head2 schema_base_class

Base class for your schema classes. Defaults to 'DBIx::Class::Schema'.

=head2 result_base_class

Base class for your table classes (aka result classes). Defaults to
'DBIx::Class::Core'.

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

=head2 use_namespaces

This is now the default, to go back to L<DBIx::Class::Schema/load_classes> pass
a C<0>.

Generate result class names suitable for
L<DBIx::Class::Schema/load_namespaces> and call that instead of
L<DBIx::Class::Schema/load_classes>. When using this option you can also
specify any of the options for C<load_namespaces> (i.e. C<result_namespace>,
C<resultset_namespace>, C<default_resultset_class>), and they will be added
to the call (and the generated result class names adjusted appropriately).

=head2 dump_directory

This option is designed to be a tool to help you transition from this
loader to a manually-defined schema when you decide it's time to do so.

The value of this option is a perl libdir pathname.  Within
that directory this module will create a baseline manual
L<DBIx::Class::Schema> module set, based on what it creates at runtime
in memory.

The created schema class will have the same classname as the one on
which you are setting this option (and the ResultSource classes will be
based on this name as well).

Normally you wouldn't hard-code this setting in your schema class, as it
is meant for one-time manual usage.

See L<DBIx::Class::Schema::Loader/dump_to_dir> for examples of the
recommended way to access this functionality.

=head2 dump_overwrite

Deprecated.  See L</really_erase_my_files> below, which does *not* mean
the same thing as the old C<dump_overwrite> setting from previous releases.

=head2 really_erase_my_files

Default false.  If true, Loader will unconditionally delete any existing
files before creating the new ones from scratch when dumping a schema to disk.

The default behavior is instead to only replace the top portion of the
file, up to and including the final stanza which contains
C<# DO NOT MODIFY THIS OR ANYTHING ABOVE!>
leaving any customizations you placed after that as they were.

When C<really_erase_my_files> is not set, if the output file already exists,
but the aforementioned final stanza is not found, or the checksum
contained there does not match the generated contents, Loader will
croak and not touch the file.

You should really be using version control on your schema classes (and all
of the rest of your code for that matter).  Don't blame me if a bug in this
code wipes something out when it shouldn't have, you've been warned.

=head2 overwrite_modifications

Default false.  If false, when updating existing files, Loader will
refuse to modify any Loader-generated code that has been modified
since its last run (as determined by the checksum Loader put in its
comment lines).

If true, Loader will discard any manual modifications that have been
made to Loader-generated code.

Again, you should be using version control on your schema classes.  Be
careful with this option.

=head2 custom_column_info

Hook for adding extra attributes to the
L<column_info|DBIx::Class::ResultSource/column_info> for a column.

Must be a coderef that returns a hashref with the extra attributes.

Receives the table name, column name and column_info.

For example:

  custom_column_info => sub {
      my ($table_name, $column_name, $column_info) = @_;

      if ($column_name eq 'dog' && $column_info->{default_value} eq 'snoopy') {
          return { is_snoopy => 1 };
      }
  },

This attribute can also be used to set C<inflate_datetime> on a non-datetime
column so it also receives the L</datetime_timezone> and/or L</datetime_locale>.

=head2 datetime_timezone

Sets the timezone attribute for L<DBIx::Class::InflateColumn::DateTime> for all
columns with the DATE/DATETIME/TIMESTAMP data_types.

=head2 datetime_locale

Sets the locale attribute for L<DBIx::Class::InflateColumn::DateTime> for all
columns with the DATE/DATETIME/TIMESTAMP data_types.

=head1 METHODS

None of these methods are intended for direct invocation by regular
users of L<DBIx::Class::Schema::Loader>. Some are proxied via
L<DBIx::Class::Schema::Loader>.

=cut

use constant CURRENT_V  => 'v5';

use constant CLASS_ARGS => qw(
    schema_base_class result_base_class additional_base_classes
    left_base_classes additional_classes components resultset_components
);

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

    $self->_ensure_arrayref(qw/additional_classes
                               additional_base_classes
                               left_base_classes
                               components
                               resultset_components
                              /);

    $self->_validate_class_args;

    push(@{$self->{components}}, 'ResultSetManager')
        if @{$self->{resultset_components}};

    $self->{monikers} = {};
    $self->{classes} = {};
    $self->{_upgrading_classes} = {};

    $self->{schema_class} ||= ( ref $self->{schema} || $self->{schema} );
    $self->{schema} ||= $self->{schema_class};

    croak "dump_overwrite is deprecated.  Please read the"
        . " DBIx::Class::Schema::Loader::Base documentation"
            if $self->{dump_overwrite};

    $self->{dynamic} = ! $self->{dump_directory};
    $self->{temp_directory} ||= File::Temp::tempdir( 'dbicXXXX',
                                                     TMPDIR  => 1,
                                                     CLEANUP => 1,
                                                   );

    $self->{dump_directory} ||= $self->{temp_directory};

    $self->version_to_dump($DBIx::Class::Schema::Loader::VERSION);
    $self->schema_version_to_dump($DBIx::Class::Schema::Loader::VERSION);

    if ((not ref $self->naming) && defined $self->naming) {
        my $naming_ver = $self->naming;
        $self->{naming} = {
            relationships => $naming_ver,
            monikers => $naming_ver,
        };
    }

    if ($self->naming) {
        for (values %{ $self->naming }) {
            $_ = CURRENT_V if $_ eq 'current';
        }
    }
    $self->{naming} ||= {};

    if ($self->custom_column_info && ref $self->custom_column_info ne 'CODE') {
        croak 'custom_column_info must be a CODE ref';
    }

    $self->_check_back_compat;

    $self->use_namespaces(1) unless defined $self->use_namespaces;
    $self->generate_pod(1)   unless defined $self->generate_pod;
    $self->pod_comment_mode('auto')         unless defined $self->pod_comment_mode;
    $self->pod_comment_spillover_length(60) unless defined $self->pod_comment_spillover_length;

    $self;
}

sub _check_back_compat {
    my ($self) = @_;

# dynamic schemas will always be in 0.04006 mode, unless overridden
    if ($self->dynamic) {
# just in case, though no one is likely to dump a dynamic schema
        $self->schema_version_to_dump('0.04006');

        if (not %{ $self->naming }) {
            warn <<EOF unless $ENV{SCHEMA_LOADER_BACKCOMPAT};

Dynamic schema detected, will run in 0.04006 mode.

Set the 'naming' attribute or the SCHEMA_LOADER_BACKCOMPAT environment variable
to disable this warning.

Also consider setting 'use_namespaces => 1' if/when upgrading.

See perldoc DBIx::Class::Schema::Loader::Manual::UpgradingFromV4 for more
details.
EOF
        }
        else {
            $self->_upgrading_from('v4');
        }

        $self->naming->{relationships} ||= 'v4';
        $self->naming->{monikers}      ||= 'v4';

        if ($self->use_namespaces) {
            $self->_upgrading_from_load_classes(1);
        }
        else {
            $self->use_namespaces(0);
        }

        return;
    }

# otherwise check if we need backcompat mode for a static schema
    my $filename = $self->_get_dump_filename($self->schema_class);
    return unless -e $filename;

    open(my $fh, '<', $filename)
        or croak "Cannot open '$filename' for reading: $!";

    my $load_classes     = 0;
    my $result_namespace = '';

    while (<$fh>) {
        if (/^__PACKAGE__->load_classes;/) {
            $load_classes = 1;
        } elsif (/result_namespace => '([^']+)'/) {
            $result_namespace = $1;
        } elsif (my ($real_ver) =
                /^# Created by DBIx::Class::Schema::Loader v(\d+\.\d+)/) {

            if ($load_classes && (not defined $self->use_namespaces)) {
                warn <<"EOF"  unless $ENV{SCHEMA_LOADER_BACKCOMPAT};

'load_classes;' static schema detected, turning off 'use_namespaces'.

Set the 'use_namespaces' attribute or the SCHEMA_LOADER_BACKCOMPAT environment
variable to disable this warning.

See perldoc DBIx::Class::Schema::Loader::Manual::UpgradingFromV4 for more
details.
EOF
                $self->use_namespaces(0);
            }
            elsif ($load_classes && $self->use_namespaces) {
                $self->_upgrading_from_load_classes(1);
            }
            elsif ((not $load_classes) && defined $self->use_namespaces
                                       && (not $self->use_namespaces)) {
                $self->_downgrading_to_load_classes(
                    $result_namespace || 'Result'
                );
            }
            elsif ((not defined $self->use_namespaces)
                   || $self->use_namespaces) {
                if (not $self->result_namespace) {
                    $self->result_namespace($result_namespace || 'Result');
                }
                elsif ($result_namespace ne $self->result_namespace) {
                    $self->_rewriting_result_namespace(
                        $result_namespace || 'Result'
                    );
                }
            }

            # XXX when we go past .0 this will need fixing
            my ($v) = $real_ver =~ /([1-9])/;
            $v = "v$v";

            last if $v eq CURRENT_V || $real_ver =~ /^0\.\d\d999/;

            if (not %{ $self->naming }) {
                warn <<"EOF" unless $ENV{SCHEMA_LOADER_BACKCOMPAT};

Version $real_ver static schema detected, turning on backcompat mode.

Set the 'naming' attribute or the SCHEMA_LOADER_BACKCOMPAT environment variable
to disable this warning.

See perldoc DBIx::Class::Schema::Loader::Manual::UpgradingFromV4 for more
details.
EOF
            }
            else {
                $self->_upgrading_from($v);
                last;
            }

            $self->naming->{relationships} ||= $v;
            $self->naming->{monikers}      ||= $v;

            $self->schema_version_to_dump($real_ver);

            last;
        }
    }
    close $fh;
}

sub _validate_class_args {
    my $self = shift;
    my $args = shift;
    
    foreach my $k (CLASS_ARGS) {
        next unless $self->$k;

        my @classes = ref $self->$k eq 'ARRAY' ? @{ $self->$k } : $self->$k;
        foreach my $c (@classes) {
            # components default to being under the DBIx::Class namespace unless they
            # are preceeded with a '+'
            if ( $k =~ m/components$/ && $c !~ s/^\+// ) {
                $c = 'DBIx::Class::' . $c;
            }

            # 1 == installed, 0 == not installed, undef == invalid classname
            my $installed = Class::Inspector->installed($c);
            if ( defined($installed) ) {
                if ( $installed == 0 ) {
                    croak qq/$c, as specified in the loader option "$k", is not installed/;
                }
            } else {
                croak qq/$c, as specified in the loader option "$k", is an invalid class name/;
            }
        }
    }
}

sub _find_file_in_inc {
    my ($self, $file) = @_;

    foreach my $prefix (@INC) {
        my $fullpath = File::Spec->catfile($prefix, $file);
        return $fullpath if -f $fullpath
            # abs_path throws on Windows for nonexistant files
            and eval { Cwd::abs_path($fullpath) } ne
               (eval { Cwd::abs_path(File::Spec->catfile($self->dump_directory, $file)) } || '');
    }

    return;
}

sub _class_path {
    my ($self, $class) = @_;

    my $class_path = $class;
    $class_path =~ s{::}{/}g;
    $class_path .= '.pm';

    return $class_path;
}

sub _find_class_in_inc {
    my ($self, $class) = @_;

    return $self->_find_file_in_inc($self->_class_path($class));
}

sub _rewriting {
    my $self = shift;

    return $self->_upgrading_from
        || $self->_upgrading_from_load_classes
        || $self->_downgrading_to_load_classes
        || $self->_rewriting_result_namespace
    ;
}

sub _rewrite_old_classnames {
    my ($self, $code) = @_;

    return $code unless $self->_rewriting;

    my %old_classes = reverse %{ $self->_upgrading_classes };

    my $re = join '|', keys %old_classes;
    $re = qr/\b($re)\b/;

    $code =~ s/$re/$old_classes{$1} || $1/eg;

    return $code;
}

sub _load_external {
    my ($self, $class) = @_;

    return if $self->{skip_load_external};

    # so that we don't load our own classes, under any circumstances
    local *INC = [ grep $_ ne $self->dump_directory, @INC ];

    my $real_inc_path = $self->_find_class_in_inc($class);

    my $old_class = $self->_upgrading_classes->{$class}
        if $self->_rewriting;

    my $old_real_inc_path = $self->_find_class_in_inc($old_class)
        if $old_class && $old_class ne $class;

    return unless $real_inc_path || $old_real_inc_path;

    if ($real_inc_path) {
        # If we make it to here, we loaded an external definition
        warn qq/# Loaded external class definition for '$class'\n/
            if $self->debug;

        open(my $fh, '<', $real_inc_path)
            or croak "Failed to open '$real_inc_path' for reading: $!";
        my $code = do { local $/; <$fh> };
        close($fh)
            or croak "Failed to close $real_inc_path: $!";
        $code = $self->_rewrite_old_classnames($code);

        if ($self->dynamic) { # load the class too
            # kill redefined warnings
            my $warn_handler = $SIG{__WARN__} || sub { warn @_ };
            local $SIG{__WARN__} = sub {
                $warn_handler->(@_)
                    unless $_[0] =~ /^Subroutine \S+ redefined/;
            };
            eval $code;
            die $@ if $@;
        }

        $self->_ext_stmt($class,
          qq|# These lines were loaded from '$real_inc_path' found in \@INC.\n|
         .qq|# They are now part of the custom portion of this file\n|
         .qq|# for you to hand-edit.  If you do not either delete\n|
         .qq|# this section or remove that file from \@INC, this section\n|
         .qq|# will be repeated redundantly when you re-create this\n|
         .qq|# file again via Loader!  See skip_load_external to disable\n|
         .qq|# this feature.\n|
        );
        chomp $code;
        $self->_ext_stmt($class, $code);
        $self->_ext_stmt($class,
            qq|# End of lines loaded from '$real_inc_path' |
        );
    }

    if ($old_real_inc_path) {
        open(my $fh, '<', $old_real_inc_path)
            or croak "Failed to open '$old_real_inc_path' for reading: $!";
        $self->_ext_stmt($class, <<"EOF");

# These lines were loaded from '$old_real_inc_path',
# based on the Result class name that would have been created by an 0.04006
# version of the Loader. For a static schema, this happens only once during
# upgrade. See skip_load_external to disable this feature.
EOF

        my $code = do {
            local ($/, @ARGV) = (undef, $old_real_inc_path); <>
        };
        $code = $self->_rewrite_old_classnames($code);

        if ($self->dynamic) {
            warn <<"EOF";

Detected external content in '$old_real_inc_path', a class name that would have
been used by an 0.04006 version of the Loader.

* PLEASE RENAME THIS CLASS: from '$old_class' to '$class', as that is the
new name of the Result.
EOF
            # kill redefined warnings
            my $warn_handler = $SIG{__WARN__} || sub { warn @_ };
            local $SIG{__WARN__} = sub {
                $warn_handler->(@_)
                    unless $_[0] =~ /^Subroutine \S+ redefined/;
            };
            eval $code;
            die $@ if $@;
        }

        chomp $code;
        $self->_ext_stmt($class, $code);
        $self->_ext_stmt($class,
            qq|# End of lines loaded from '$old_real_inc_path' |
        );
    }
}

=head2 load

Does the actual schema-construction work.

=cut

sub load {
    my $self = shift;

    $self->_load_tables($self->_tables_list);
}

=head2 rescan

Arguments: schema

Rescan the database for newly added tables.  Does
not process drops or changes.  Returns a list of
the newly added table monikers.

The schema argument should be the schema class
or object to be affected.  It should probably
be derived from the original schema_class used
during L</load>.

=cut

sub rescan {
    my ($self, $schema) = @_;

    $self->{schema} = $schema;
    $self->_relbuilder->{schema} = $schema;

    my @created;
    my @current = $self->_tables_list;
    foreach my $table ($self->_tables_list) {
        if(!exists $self->{_tables}->{$table}) {
            push(@created, $table);
        }
    }

    my $loaded = $self->_load_tables(@created);

    return map { $self->monikers->{$_} } @$loaded;
}

sub _relbuilder {
    no warnings 'uninitialized';
    my ($self) = @_;

    return if $self->{skip_relationships};

    if ($self->naming->{relationships} eq 'v4') {
        require DBIx::Class::Schema::Loader::RelBuilder::Compat::v0_040;
        return $self->{relbuilder} ||=
            DBIx::Class::Schema::Loader::RelBuilder::Compat::v0_040->new(
                $self->schema, $self->inflect_plural, $self->inflect_singular
            );
    }

    $self->{relbuilder} ||= DBIx::Class::Schema::Loader::RelBuilder->new (
	 $self->schema,
	 $self->inflect_plural,
	 $self->inflect_singular,
	 $self->relationship_attrs,
    );
}

sub _load_tables {
    my ($self, @tables) = @_;

    # First, use _tables_list with constraint and exclude
    #  to get a list of tables to operate on

    my $constraint   = $self->constraint;
    my $exclude      = $self->exclude;

    @tables = grep { /$constraint/ } @tables if $constraint;
    @tables = grep { ! /$exclude/ } @tables if $exclude;

    # Save the new tables to the tables list
    foreach (@tables) {
        $self->{_tables}->{$_} = 1;
    }

    $self->_make_src_class($_) for @tables;
    $self->_setup_src_meta($_) for @tables;

    if(!$self->skip_relationships) {
        # The relationship loader needs a working schema
        $self->{quiet} = 1;
        local $self->{dump_directory} = $self->{temp_directory};
        $self->_reload_classes(\@tables);
        $self->_load_relationships($_) for @tables;
        $self->{quiet} = 0;

        # Remove that temp dir from INC so it doesn't get reloaded
        @INC = grep $_ ne $self->dump_directory, @INC;
    }

    $self->_load_external($_)
        for map { $self->classes->{$_} } @tables;

    # Reload without unloading first to preserve any symbols from external
    # packages.
    $self->_reload_classes(\@tables, 0);

    # Drop temporary cache
    delete $self->{_cache};

    return \@tables;
}

sub _reload_classes {
    my ($self, $tables, $unload) = @_;

    my @tables = @$tables;
    $unload = 1 unless defined $unload;

    # so that we don't repeat custom sections
    @INC = grep $_ ne $self->dump_directory, @INC;

    $self->_dump_to_dir(map { $self->classes->{$_} } @tables);

    unshift @INC, $self->dump_directory;
    
    my @to_register;
    my %have_source = map { $_ => $self->schema->source($_) }
        $self->schema->sources;

    for my $table (@tables) {
        my $moniker = $self->monikers->{$table};
        my $class = $self->classes->{$table};
        
        {
            no warnings 'redefine';
            local *Class::C3::reinitialize = sub {};
            use warnings;

            Class::Unload->unload($class) if $unload;
            my ($source, $resultset_class);
            if (
                ($source = $have_source{$moniker})
                && ($resultset_class = $source->resultset_class)
                && ($resultset_class ne 'DBIx::Class::ResultSet')
            ) {
                my $has_file = Class::Inspector->loaded_filename($resultset_class);
                Class::Unload->unload($resultset_class) if $unload;
                $self->_reload_class($resultset_class) if $has_file;
            }
            $self->_reload_class($class);
        }
        push @to_register, [$moniker, $class];
    }

    Class::C3->reinitialize;
    for (@to_register) {
        $self->schema->register_class(@$_);
    }
}

# We use this instead of ensure_class_loaded when there are package symbols we
# want to preserve.
sub _reload_class {
    my ($self, $class) = @_;

    my $class_path = $self->_class_path($class);
    delete $INC{ $class_path };

# kill redefined warnings
    my $warn_handler = $SIG{__WARN__} || sub { warn @_ };
    local $SIG{__WARN__} = sub {
        $warn_handler->(@_)
            unless $_[0] =~ /^Subroutine \S+ redefined/;
    };
    eval "require $class;";
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
    while (1) {
        if(!-d $dir) {
            mkdir($dir) or croak "mkdir('$dir') failed: $!";
        }
        last if !@name_parts;
        $dir = File::Spec->catdir($dir, shift @name_parts);
    }
}

sub _dump_to_dir {
    my ($self, @classes) = @_;

    my $schema_class = $self->schema_class;
    my $schema_base_class = $self->schema_base_class || 'DBIx::Class::Schema';

    my $target_dir = $self->dump_directory;
    warn "Dumping manual schema for $schema_class to directory $target_dir ...\n"
        unless $self->{dynamic} or $self->{quiet};

    my $schema_text =
          qq|package $schema_class;\n\n|
        . qq|# Created by DBIx::Class::Schema::Loader\n|
        . qq|# DO NOT MODIFY THE FIRST PART OF THIS FILE\n\n|
        . qq|use strict;\nuse warnings;\n\n|
        . qq|use base '$schema_base_class';\n\n|;

    if ($self->use_namespaces) {
        $schema_text .= qq|__PACKAGE__->load_namespaces|;
        my $namespace_options;
        for my $attr (qw(result_namespace
                         resultset_namespace
                         default_resultset_class)) {
            if ($self->$attr) {
                $namespace_options .= qq|    $attr => '| . $self->$attr . qq|',\n|
            }
        }
        $schema_text .= qq|(\n$namespace_options)| if $namespace_options;
        $schema_text .= qq|;\n|;
    }
    else {
        $schema_text .= qq|__PACKAGE__->load_classes;\n|;
    }

    {
        local $self->{version_to_dump} = $self->schema_version_to_dump;
        $self->_write_classfile($schema_class, $schema_text, 1);
    }

    my $result_base_class = $self->result_base_class || 'DBIx::Class::Core';

    foreach my $src_class (@classes) {
        my $src_text = 
              qq|package $src_class;\n\n|
            . qq|# Created by DBIx::Class::Schema::Loader\n|
            . qq|# DO NOT MODIFY THE FIRST PART OF THIS FILE\n\n|
            . qq|use strict;\nuse warnings;\n\n|
            . qq|use base '$result_base_class';\n\n|;

        $self->_write_classfile($src_class, $src_text);
    }

    # remove Result dir if downgrading from use_namespaces, and there are no
    # files left.
    if (my $result_ns = $self->_downgrading_to_load_classes
                        || $self->_rewriting_result_namespace) {
        my $result_namespace = $self->_result_namespace(
            $schema_class,
            $result_ns,
        );

        (my $result_dir = $result_namespace) =~ s{::}{/}g;
        $result_dir = $self->dump_directory . '/' . $result_dir;

        unless (my @files = glob "$result_dir/*") {
            rmdir $result_dir;
        }
    }

    warn "Schema dump completed.\n" unless $self->{dynamic} or $self->{quiet};

}

sub _sig_comment {
    my ($self, $version, $ts) = @_;
    return qq|\n\n# Created by DBIx::Class::Schema::Loader|
         . qq| v| . $version
         . q| @ | . $ts 
         . qq|\n# DO NOT MODIFY THIS OR ANYTHING ABOVE! md5sum:|;
}

sub _write_classfile {
    my ($self, $class, $text, $is_schema) = @_;

    my $filename = $self->_get_dump_filename($class);
    $self->_ensure_dump_subdirs($class);

    if (-f $filename && $self->really_erase_my_files) {
        warn "Deleting existing file '$filename' due to "
            . "'really_erase_my_files' setting\n" unless $self->{quiet};
        unlink($filename);
    }    

    my ($custom_content, $old_md5, $old_ver, $old_ts) = $self->_get_custom_content($class, $filename);

    if (my $old_class = $self->_upgrading_classes->{$class}) {
        my $old_filename = $self->_get_dump_filename($old_class);

        my ($old_custom_content) = $self->_get_custom_content(
            $old_class, $old_filename, 0 # do not add default comment
        );

        $old_custom_content =~ s/\n\n# You can replace.*\n1;\n//;

        if ($old_custom_content) {
            $custom_content =
                "\n" . $old_custom_content . "\n" . $custom_content;
        }

        unlink $old_filename;
    }

    $custom_content = $self->_rewrite_old_classnames($custom_content);

    $text .= qq|$_\n|
        for @{$self->{_dump_storage}->{$class} || []};

    # Check and see if the dump is infact differnt

    my $compare_to;
    if ($old_md5) {
      $compare_to = $text . $self->_sig_comment($old_ver, $old_ts);
      

      if (Digest::MD5::md5_base64($compare_to) eq $old_md5) {
        return unless $self->_upgrading_from && $is_schema;
      }
    }

    $text .= $self->_sig_comment(
      $self->version_to_dump,
      POSIX::strftime('%Y-%m-%d %H:%M:%S', localtime)
    );

    open(my $fh, '>', $filename)
        or croak "Cannot open '$filename' for writing: $!";

    # Write the top half and its MD5 sum
    print $fh $text . Digest::MD5::md5_base64($text) . "\n";

    # Write out anything loaded via external partial class file in @INC
    print $fh qq|$_\n|
        for @{$self->{_ext_storage}->{$class} || []};

    # Write out any custom content the user has added
    print $fh $custom_content;

    close($fh)
        or croak "Error closing '$filename': $!";
}

sub _default_custom_content {
    return qq|\n\n# You can replace this text with custom|
         . qq| content, and it will be preserved on regeneration|
         . qq|\n1;\n|;
}

sub _get_custom_content {
    my ($self, $class, $filename, $add_default) = @_;

    $add_default = 1 unless defined $add_default;

    return ($self->_default_custom_content) if ! -f $filename;

    open(my $fh, '<', $filename)
        or croak "Cannot open '$filename' for reading: $!";

    my $mark_re = 
        qr{^(# DO NOT MODIFY THIS OR ANYTHING ABOVE! md5sum:)([A-Za-z0-9/+]{22})\n};

    my $buffer = '';
    my ($md5, $ts, $ver);
    while(<$fh>) {
        if(!$md5 && /$mark_re/) {
            $md5 = $2;
            my $line = $1;

            # Pull out the previous version and timestamp
            ($ver, $ts) = $buffer =~ m/# Created by DBIx::Class::Schema::Loader v(.*?) @ (.*?)$/s;

            $buffer .= $line;
            croak "Checksum mismatch in '$filename', the auto-generated part of the file has been modified outside of this loader.  Aborting.\nIf you want to overwrite these modifications, set the 'overwrite_modifications' loader option.\n"
                if !$self->overwrite_modifications && Digest::MD5::md5_base64($buffer) ne $md5;

            $buffer = '';
        }
        else {
            $buffer .= $_;
        }
    }

    croak "Cannot not overwrite '$filename' without 'really_erase_my_files',"
        . " it does not appear to have been generated by Loader"
            if !$md5;

    # Default custom content:
    $buffer ||= $self->_default_custom_content if $add_default;

    return ($buffer, $md5, $ver, $ts);
}

sub _use {
    my $self = shift;
    my $target = shift;

    foreach (@_) {
        warn "$target: use $_;" if $self->debug;
        $self->_raw_stmt($target, "use $_;");
    }
}

sub _inject {
    my $self = shift;
    my $target = shift;
    my $schema_class = $self->schema_class;

    my $blist = join(q{ }, @_);
    warn "$target: use base qw/ $blist /;" if $self->debug && @_;
    $self->_raw_stmt($target, "use base qw/ $blist /;") if @_;
}

sub _result_namespace {
    my ($self, $schema_class, $ns) = @_;
    my @result_namespace;

    if ($ns =~ /^\+(.*)/) {
        # Fully qualified namespace
        @result_namespace = ($1)
    }
    else {
        # Relative namespace
        @result_namespace = ($schema_class, $ns);
    }

    return wantarray ? @result_namespace : join '::', @result_namespace;
}

# Create class with applicable bases, setup monikers, etc
sub _make_src_class {
    my ($self, $table) = @_;

    my $schema       = $self->schema;
    my $schema_class = $self->schema_class;

    my $table_moniker = $self->_table2moniker($table);
    my @result_namespace = ($schema_class);
    if ($self->use_namespaces) {
        my $result_namespace = $self->result_namespace || 'Result';
        @result_namespace = $self->_result_namespace(
            $schema_class,
            $result_namespace,
        );
    }
    my $table_class = join(q{::}, @result_namespace, $table_moniker);

    if ((my $upgrading_v = $self->_upgrading_from)
            || $self->_rewriting) {
        local $self->naming->{monikers} = $upgrading_v
            if $upgrading_v;

        my @result_namespace = @result_namespace;
        if ($self->_upgrading_from_load_classes) {
            @result_namespace = ($schema_class);
        }
        elsif (my $ns = $self->_downgrading_to_load_classes) {
            @result_namespace = $self->_result_namespace(
                $schema_class,
                $ns,
            );
        }
        elsif ($ns = $self->_rewriting_result_namespace) {
            @result_namespace = $self->_result_namespace(
                $schema_class,
                $ns,
            );
        }

        my $old_class = join(q{::}, @result_namespace,
            $self->_table2moniker($table));

        $self->_upgrading_classes->{$table_class} = $old_class
            unless $table_class eq $old_class;
    }

    my $table_normalized = lc $table;
    $self->classes->{$table} = $table_class;
    $self->classes->{$table_normalized} = $table_class;
    $self->monikers->{$table} = $table_moniker;
    $self->monikers->{$table_normalized} = $table_moniker;

    $self->_use   ($table_class, @{$self->additional_classes});
    $self->_inject($table_class, @{$self->left_base_classes});

    if (my @components = @{ $self->components }) {
        $self->_dbic_stmt($table_class, 'load_components', @components);
    }

    $self->_dbic_stmt($table_class, 'load_resultset_components', @{$self->resultset_components})
        if @{$self->resultset_components};
    $self->_inject($table_class, @{$self->additional_base_classes});
}

# Set up metadata (cols, pks, etc)
sub _setup_src_meta {
    my ($self, $table) = @_;

    my $schema       = $self->schema;
    my $schema_class = $self->schema_class;

    my $table_class = $self->classes->{$table};
    my $table_moniker = $self->monikers->{$table};

    my $table_name = $table;
    my $name_sep   = $self->schema->storage->sql_maker->name_sep;

    if ($name_sep && $table_name =~ /\Q$name_sep\E/) {
        $table_name = \ $self->_quote_table_name($table_name);
    }

    $self->_dbic_stmt($table_class,'table',$table_name);

    my $cols = $self->_table_columns($table);
    my $col_info;
    eval { $col_info = $self->__columns_info_for($table) };
    if($@) {
        $self->_dbic_stmt($table_class,'add_columns',@$cols);
    }
    else {
        if ($self->_is_case_sensitive) {
            for my $col (keys %$col_info) {
                $col_info->{$col}{accessor} = lc $col
                    if $col ne lc($col);
            }
        } else {
            $col_info = { map { lc($_), $col_info->{$_} } keys %$col_info };
        }

        my $fks = $self->_table_fk_info($table);

        for my $fkdef (@$fks) {
            for my $col (@{ $fkdef->{local_columns} }) {
                $col_info->{$col}{is_foreign_key} = 1;
            }
        }
        $self->_dbic_stmt(
            $table_class,
            'add_columns',
            map { $_, ($col_info->{$_}||{}) } @$cols
        );
    }

    my %uniq_tag; # used to eliminate duplicate uniqs

    my $pks = $self->_table_pk_info($table) || [];
    @$pks ? $self->_dbic_stmt($table_class,'set_primary_key',@$pks)
          : carp("$table has no primary key");
    $uniq_tag{ join("\0", @$pks) }++ if @$pks; # pk is a uniq

    my $uniqs = $self->_table_uniq_info($table) || [];
    for (@$uniqs) {
        my ($name, $cols) = @$_;
        next if $uniq_tag{ join("\0", @$cols) }++; # skip duplicates
        $self->_dbic_stmt($table_class,'add_unique_constraint', $name, $cols);
    }

}

sub __columns_info_for {
    my ($self, $table) = @_;

    my $result = $self->_columns_info_for($table);

    while (my ($col, $info) = each %$result) {
        $info = { %$info, %{ $self->_custom_column_info  ($table, $col, $info) } };
        $info = { %$info, %{ $self->_datetime_column_info($table, $col, $info) } };

        $result->{$col} = $info;
    }

    return $result;
}

=head2 tables

Returns a sorted list of loaded tables, using the original database table
names.

=cut

sub tables {
    my $self = shift;

    return keys %{$self->_tables};
}

# Make a moniker from a table
sub _default_table2moniker {
    no warnings 'uninitialized';
    my ($self, $table) = @_;

    if ($self->naming->{monikers} eq 'v4') {
        return join '', map ucfirst, split /[\W_]+/, lc $table;
    }

    return join '', map ucfirst, split /[\W_]+/,
        Lingua::EN::Inflect::Number::to_S(lc $table);
}

sub _table2moniker {
    my ( $self, $table ) = @_;

    my $moniker;

    if( ref $self->moniker_map eq 'HASH' ) {
        $moniker = $self->moniker_map->{$table};
    }
    elsif( ref $self->moniker_map eq 'CODE' ) {
        $moniker = $self->moniker_map->($table);
    }

    $moniker ||= $self->_default_table2moniker($table);

    return $moniker;
}

sub _load_relationships {
    my ($self, $table) = @_;

    my $tbl_fk_info = $self->_table_fk_info($table);
    foreach my $fkdef (@$tbl_fk_info) {
        $fkdef->{remote_source} =
            $self->monikers->{delete $fkdef->{remote_table}};
    }
    my $tbl_uniq_info = $self->_table_uniq_info($table);

    my $local_moniker = $self->monikers->{$table};
    my $rel_stmts = $self->_relbuilder->generate_code($local_moniker, $tbl_fk_info, $tbl_uniq_info);

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
    my $self   = shift;
    my $class  = shift;
    my $method = shift;

    # generate the pod for this statement, storing it with $self->_pod
    $self->_make_pod( $class, $method, @_ ) if $self->generate_pod;

    my $args = dump(@_);
    $args = '(' . $args . ')' if @_ < 2;
    my $stmt = $method . $args . q{;};

    warn qq|$class\->$stmt\n| if $self->debug;
    $self->_raw_stmt($class, '__PACKAGE__->' . $stmt);
    return;
}

# generates the accompanying pod for a DBIC class method statement,
# storing it with $self->_pod
sub _make_pod {
    my $self   = shift;
    my $class  = shift;
    my $method = shift;

    if ( $method eq 'table' ) {
        my ($table) = @_;
        my $pcm = $self->pod_comment_mode;
        my ($comment, $comment_overflows, $comment_in_name, $comment_in_desc);
        if ( $self->can('_table_comment') ) {
            $comment = $self->_table_comment($table);
            $comment_overflows = ($comment and length $comment > $self->pod_comment_spillover_length);
            $comment_in_name   = ($pcm eq 'name' or ($pcm eq 'auto' and !$comment_overflows));
            $comment_in_desc   = ($pcm eq 'description' or ($pcm eq 'auto' and $comment_overflows));
        }
        $self->_pod( $class, "=head1 NAME" );
        my $table_descr = $class;
        $table_descr .= " - " . $comment if $comment and $comment_in_name;
        $self->{_class2table}{ $class } = $table;
        $self->_pod( $class, $table_descr );
        if ($comment and $comment_in_desc) {
            $self->_pod( $class, "=head1 DESCRIPTION" );
            $self->_pod( $class, $comment );
        }
        $self->_pod_cut( $class );
    } elsif ( $method eq 'add_columns' ) {
        $self->_pod( $class, "=head1 ACCESSORS" );
        my $col_counter = 0;
	my @cols = @_;
        while( my ($name,$attrs) = splice @cols,0,2 ) {
	    $col_counter++;
            $self->_pod( $class, '=head2 ' . $name  );
	    $self->_pod( $class,
			 join "\n", map {
			     my $s = $attrs->{$_};
			     $s = !defined $s         ? 'undef'          :
                                  length($s) == 0     ? '(empty string)' :
                                  ref($s) eq 'SCALAR' ? $$s              :
                                                        $s
                                  ;

			     "  $_: $s"
			 } sort keys %$attrs,
		       );

	    if( $self->can('_column_comment')
		and my $comment = $self->_column_comment( $self->{_class2table}{$class}, $col_counter)
	      ) {
		$self->_pod( $class, $comment );
	    }
        }
        $self->_pod_cut( $class );
    } elsif ( $method =~ /^(belongs_to|has_many|might_have)$/ ) {
        $self->_pod( $class, "=head1 RELATIONS" ) unless $self->{_relations_started} { $class } ;
        my ( $accessor, $rel_class ) = @_;
        $self->_pod( $class, "=head2 $accessor" );
        $self->_pod( $class, 'Type: ' . $method );
        $self->_pod( $class, "Related object: L<$rel_class>" );
        $self->_pod_cut( $class );
        $self->{_relations_started} { $class } = 1;
    }
}

# Stores a POD documentation
sub _pod {
    my ($self, $class, $stmt) = @_;
    $self->_raw_stmt( $class, "\n" . $stmt  );
}

sub _pod_cut {
    my ($self, $class ) = @_;
    $self->_raw_stmt( $class, "\n=cut\n" );
}

# Store a raw source line for a class (for dumping purposes)
sub _raw_stmt {
    my ($self, $class, $stmt) = @_;
    push(@{$self->{_dump_storage}->{$class}}, $stmt);
}

# Like above, but separately for the externally loaded stuff
sub _ext_stmt {
    my ($self, $class, $stmt) = @_;
    push(@{$self->{_ext_storage}->{$class}}, $stmt);
}

sub _quote_table_name {
    my ($self, $table) = @_;

    my $qt = $self->schema->storage->sql_maker->quote_char;

    return $table unless $qt;

    if (ref $qt) {
        return $qt->[0] . $table . $qt->[1];
    }

    return $qt . $table . $qt;
}

sub _is_case_sensitive { 0 }

sub _custom_column_info {
    my ( $self, $table_name, $column_name, $column_info ) = @_;

    if (my $code = $self->custom_column_info) {
        return $code->($table_name, $column_name, $column_info) || {};
    }
    return {};
}

sub _datetime_column_info {
    my ( $self, $table_name, $column_name, $column_info ) = @_;
    my $result = {};
    my $type = $column_info->{data_type} || '';
    if ((grep $_, @{ $column_info }{map "inflate_$_", qw/date datetime timestamp/})
            or ($type =~ /date|timestamp/i)) {
        $result->{timezone} = $self->datetime_timezone if $self->datetime_timezone;
        $result->{locale}   = $self->datetime_locale   if $self->datetime_locale;
    }
    return $result;
}

# remove the dump dir from @INC on destruction
sub DESTROY {
    my $self = shift;

    @INC = grep $_ ne $self->dump_directory, @INC;
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

=head1 AUTHOR

See L<DBIx::Class::Schema::Loader/AUTHOR> and L<DBIx::Class::Schema::Loader/CONTRIBUTORS>.

=head1 LICENSE

This library is free software; you can redistribute it and/or modify it under
the same terms as Perl itself.

=cut

1;
