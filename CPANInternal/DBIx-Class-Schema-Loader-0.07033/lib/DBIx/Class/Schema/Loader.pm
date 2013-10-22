package DBIx::Class::Schema::Loader;

use strict;
use warnings;
use base qw/DBIx::Class::Schema Class::Accessor::Grouped/;
use MRO::Compat;
use mro 'c3';
use Carp::Clan qw/^DBIx::Class/;
use Scalar::Util 'weaken';
use Sub::Name 'subname';
use DBIx::Class::Schema::Loader::Utils 'array_eq';
use Try::Tiny;
use Hash::Merge 'merge';
use namespace::clean;

# Always remember to do all digits for the version even if they're 0
# i.e. first release of 0.XX *must* be 0.XX000. This avoids fBSD ports
# brain damage and presumably various other packaging systems too
our $VERSION = '0.07033';

__PACKAGE__->mk_group_accessors('inherited', qw/
                                _loader_args
                                dump_to_dir
                                _loader_invoked
                                _loader
                                loader_class
                                naming
                                use_namespaces
/);
__PACKAGE__->_loader_args({});

=encoding UTF-8

=head1 NAME

DBIx::Class::Schema::Loader - Create a DBIx::Class::Schema based on a database

=head1 SYNOPSIS

  ### use this module to generate a set of class files

  # in a script
  use DBIx::Class::Schema::Loader qw/ make_schema_at /;
  make_schema_at(
      'My::Schema',
      { debug => 1,
        dump_directory => './lib',
      },
      [ 'dbi:Pg:dbname="foo"', 'myuser', 'mypassword',
         { loader_class => 'MyLoader' } # optionally
      ],
  );

  # from the command line or a shell script with dbicdump (distributed
  # with this module).  Do `perldoc dbicdump` for usage.
  dbicdump -o dump_directory=./lib \
           -o components='["InflateColumn::DateTime"]' \
           -o debug=1 \
           My::Schema \
           'dbi:Pg:dbname=foo' \
           myuser \
           mypassword

  ### or generate and load classes at runtime
  # note: this technique is not recommended
  # for use in production code

  package My::Schema;
  use base qw/DBIx::Class::Schema::Loader/;

  __PACKAGE__->loader_options(
      constraint              => '^foo.*',
      # debug                 => 1,
  );

  #### in application code elsewhere:

  use My::Schema;

  my $schema1 = My::Schema->connect( $dsn, $user, $password, $attrs);
  # -or-
  my $schema1 = "My::Schema"; $schema1->connection(as above);

=head1 DESCRIPTION

DBIx::Class::Schema::Loader automates the definition of a
L<DBIx::Class::Schema> by scanning database table definitions and setting up
the columns, primary keys, unique constraints and relationships.

See L<dbicdump> for the C<dbicdump> utility.

DBIx::Class::Schema::Loader currently supports only the DBI storage type. It
has explicit support for L<DBD::Pg>, L<DBD::mysql>, L<DBD::DB2>,
L<DBD::Firebird>, L<DBD::InterBase>, L<DBD::Informix>, L<DBD::SQLAnywhere>,
L<DBD::SQLite>, L<DBD::Sybase> (for Sybase ASE and MSSSQL), L<DBD::ODBC> (for
MSSQL, MSAccess, Firebird and SQL Anywhere) L<DBD::ADO> (for MSSQL and
MSAccess) and L<DBD::Oracle>.  Other DBI drivers may function to a greater or
lesser degree with this loader, depending on how much of the DBI spec they
implement, and how standard their implementation is.

Patches to make other DBDs work correctly welcome.

See L<DBIx::Class::Schema::Loader::DBI::Writing> for notes on writing
your own vendor-specific subclass for an unsupported DBD driver.

This module requires L<DBIx::Class> 0.08127 or later, and obsoletes the older
L<DBIx::Class::Loader>.

See L<DBIx::Class::Schema::Loader::Base> for available options.

=head1 METHODS

=head2 loader

The loader object, as class data on your Schema. For methods available see
L<DBIx::Class::Schema::Loader::Base> and L<DBIx::Class::Schema::Loader::DBI>.

=cut

sub loader {
    my $self = shift;
    $self->_loader(@_);
}

=head2 loader_class

=over 4

=item Argument: $loader_class

=back

Set the loader class to be instantiated when L</connection> is called.
If the classname starts with "::", "DBIx::Class::Schema::Loader" is
prepended. Defaults to L<DBIx::Class::Schema/storage_type> (which must
start with "::" when using L<DBIx::Class::Schema::Loader>).

This is mostly useful for subclassing existing loaders or in conjunction
with L</dump_to_dir>.

=head2 loader_options

=over 4

=item Argument: \%loader_options

=back

Example in Synopsis above demonstrates a few common arguments.  For
detailed information on all of the arguments, most of which are
only useful in fairly complex scenarios, see the
L<DBIx::Class::Schema::Loader::Base> documentation.

If you intend to use C<loader_options>, you must call
C<loader_options> before any connection is made, or embed the
C<loader_options> in the connection information itself as shown
below.  Setting C<loader_options> after the connection has
already been made is useless.

=cut

sub loader_options {
    my $self = shift;

    my %args = (ref $_[0] eq 'HASH') ? %{$_[0]} : @_;
    $self->_loader_args(\%args);

    $self;
}

sub _invoke_loader {
    my $self = shift;
    my $class = ref $self || $self;

    my $args = $self->_loader_args;

    # temporarily copy $self's storage to class
    my $class_storage = $class->storage;
    if (ref $self) {
        $class->storage($self->storage);
        $class->storage->set_schema($class);
    }

    $args->{schema} = $class;
    $args->{schema_class} = $class;
    $args->{dump_directory} ||= $self->dump_to_dir;
    $args->{naming} = $self->naming if $self->naming;
    $args->{use_namespaces} = $self->use_namespaces if defined $self->use_namespaces;

    # XXX this only works for relative storage_type, like ::DBI ...
    my $loader_class = $self->loader_class;
    if ($loader_class) {
        $loader_class = "DBIx::Class::Schema::Loader${loader_class}" if $loader_class =~ /^::/;
        $args->{loader_class} = $loader_class;
    };

    my $impl = $loader_class || "DBIx::Class::Schema::Loader" . $self->storage_type;
    try {
        $self->ensure_class_loaded($impl)
    }
    catch {
        croak qq/Could not load loader_class "$impl": "$_"/;
    };

    $class->loader($impl->new(%$args));
    $class->loader->load;
    $class->_loader_invoked(1);

    # copy to $self
    if (ref $self) {
        $self->loader($class->loader);
        $self->_loader_invoked(1);

        $self->_merge_state_from($class);
    }

    # restore $class's storage
    $class->storage($class_storage);

    return $self;
}

# FIXME This needs to be moved into DBIC at some point, otherwise we are
# maintaining things to do with DBIC guts, which we have no business of
# maintaining. But at the moment it would be just dead code in DBIC, so we'll
# maintain it here.
sub _merge_state_from {
    my ($self, $from) = @_;

    my $orig_class_mappings       = $self->class_mappings;
    my $orig_source_registrations = $self->source_registrations;

    $self->_copy_state_from($from);

    $self->class_mappings(merge($orig_class_mappings, $self->class_mappings))
        if $orig_class_mappings;

    $self->source_registrations(merge($orig_source_registrations, $self->source_registrations))
        if $orig_source_registrations;
}

sub _copy_state_from {
    my $self = shift;
    my ($from) = @_;

    # older DBIC's do not have this method
    if (try { DBIx::Class->VERSION('0.08197'); 1 }) {
        return $self->next::method(@_);
    }
    else {
        # this is a copy from DBIC git master pre 0.08197
        $self->class_mappings({ %{$from->class_mappings} });
        $self->source_registrations({ %{$from->source_registrations} });

        foreach my $moniker ($from->sources) {
            my $source = $from->source($moniker);
            my $new = $source->new($source);
            # we use extra here as we want to leave the class_mappings as they are
            # but overwrite the source_registrations entry with the new source
            $self->register_extra_source($moniker => $new);
        }

        if ($from->storage) {
            $self->storage($from->storage);
            $self->storage->set_schema($self);
        }
    }
}

=head2 connection

=over 4

=item Arguments: @args

=item Return Value: $new_schema

=back

See L<DBIx::Class::Schema/connection> for basic usage.

If the final argument is a hashref, and it contains the keys C<loader_options>
or C<loader_class>, those keys will be deleted, and their values value will be
used for the loader options or class, respectively, just as if set via the
L</loader_options> or L</loader_class> methods above.

The actual auto-loading operation (the heart of this module) will be invoked
as soon as the connection information is defined.

=cut

sub connection {
    my $self  = shift;
    my $class = ref $self || $self;

    if($_[-1] && ref $_[-1] eq 'HASH') {
        for my $option (qw/loader_class loader_options/) {
            if(my $value = delete $_[-1]->{$option}) {
                $self->$option($value);
            }
        }
        pop @_ if !keys %{$_[-1]};
    }

    # Make sure we inherit from schema_base_class and load schema_components
    # before connecting.
    require DBIx::Class::Schema::Loader::Base;
    my $temp_loader = DBIx::Class::Schema::Loader::Base->new(
        %{ $self->_loader_args },
        schema => $self,
        naming => 'current',
        use_namespaces => 1,
    );

    my $modify_isa = 0;
    my @components;

    if ($temp_loader->schema_base_class || $temp_loader->schema_components) {
        @components = @{ $temp_loader->schema_components }
            if $temp_loader->schema_components;

        push @components, ('+'.$temp_loader->schema_base_class)
            if $temp_loader->schema_base_class;

        my $class_isa = do {
            no strict 'refs';
            \@{"${class}::ISA"};
        };

        my @component_classes = map {
            /^\+/ ? substr($_, 1, length($_) - 1) : "DBIx::Class::$_"
        } @components;

        $modify_isa++ if not array_eq([ @$class_isa[0..(@components-1)] ], \@component_classes)
    }

    if ($modify_isa) {
        $class->load_components(@components);

        # This hack is necessary because we changed @ISA of $self through
        # ->load_components and we are now in a different place in the mro.
        no warnings 'redefine';

        local *connection = subname __PACKAGE__.'::connection' => sub {
            my $self = shift;
            $self->next::method(@_);
        };

        my @linear_isa = @{ mro::get_linear_isa($class) };

        my $next_method;

        foreach my $i (1..$#linear_isa) {
            no strict 'refs';
            $next_method = *{$linear_isa[$i].'::connection'}{CODE};
            last if $next_method;
        }

        $self = $self->$next_method(@_);
    }
    else {
        $self = $self->next::method(@_);
    }

    if(!$class->_loader_invoked) {
        $self->_invoke_loader
    }

    return $self;
}

=head2 clone

See L<DBIx::Class::Schema/clone>.

=cut

sub clone {
    my $self = shift;

    my $clone = $self->next::method(@_);

    if($clone->_loader_args) {
        $clone->_loader_args->{schema} = $clone;
        weaken($clone->_loader_args->{schema});
    }

    $clone;
}

=head2 dump_to_dir

=over 4

=item Argument: $directory

=back

Calling this as a class method on either L<DBIx::Class::Schema::Loader>
or any derived schema class will cause all schemas to dump
manual versions of themselves to the named directory when they are
loaded.  In order to be effective, this must be set before defining a
connection on this schema class or any derived object (as the loading
happens as soon as both a connection and loader_options are set, and
only once per class).

See L<DBIx::Class::Schema::Loader::Base/dump_directory> for more
details on the dumping mechanism.

This can also be set at module import time via the import option
C<dump_to_dir:/foo/bar> to L<DBIx::Class::Schema::Loader>, where
C</foo/bar> is the target directory.

Examples:

    # My::Schema isa DBIx::Class::Schema::Loader, and has connection info
    #   hardcoded in the class itself:
    perl -MDBIx::Class::Schema::Loader=dump_to_dir:/foo/bar -MMy::Schema -e1

    # Same, but no hard-coded connection, so we must provide one:
    perl -MDBIx::Class::Schema::Loader=dump_to_dir:/foo/bar -MMy::Schema -e 'My::Schema->connection("dbi:Pg:dbname=foo", ...)'

    # Or as a class method, as long as you get it done *before* defining a
    #  connection on this schema class or any derived object:
    use My::Schema;
    My::Schema->dump_to_dir('/foo/bar');
    My::Schema->connection(........);

    # Or as a class method on the DBIx::Class::Schema::Loader itself, which affects all
    #   derived schemas
    use My::Schema;
    use My::OtherSchema;
    DBIx::Class::Schema::Loader->dump_to_dir('/foo/bar');
    My::Schema->connection(.......);
    My::OtherSchema->connection(.......);

    # Another alternative to the above:
    use DBIx::Class::Schema::Loader qw| dump_to_dir:/foo/bar |;
    use My::Schema;
    use My::OtherSchema;
    My::Schema->connection(.......);
    My::OtherSchema->connection(.......);

=cut

sub import {
    my $self = shift;

    return if !@_;

    my $cpkg = (caller)[0];

    foreach my $opt (@_) {
        if($opt =~ m{^dump_to_dir:(.*)$}) {
            $self->dump_to_dir($1)
        }
        elsif($opt eq 'make_schema_at') {
            no strict 'refs';
            *{"${cpkg}::make_schema_at"} = \&make_schema_at;
        }
        elsif($opt eq 'naming') {
            no strict 'refs';
            *{"${cpkg}::naming"} = sub { $self->naming(@_) };
        }
        elsif($opt eq 'use_namespaces') {
            no strict 'refs';
            *{"${cpkg}::use_namespaces"} = sub { $self->use_namespaces(@_) };
        }
    }
}

=head2 make_schema_at

=over 4

=item Arguments: $schema_class_name, \%loader_options, \@connect_info

=item Return Value: $schema_class_name

=back

This function creates a DBIx::Class schema from an existing RDBMS
schema.  With the C<dump_directory> option, generates a set of
DBIx::Class classes from an existing database schema read from the
given dsn.  Without a C<dump_directory>, creates schema classes in
memory at runtime without generating on-disk class files.

For a complete list of supported loader_options, see
L<DBIx::Class::Schema::Loader::Base>

The last hashref in the C<\@connect_info> can specify the L</loader_class>.

This function can be imported in the usual way, as illustrated in
these Examples:

    # Simple example, creates as a new class 'New::Schema::Name' in
    #  memory in the running perl interpreter.
    use DBIx::Class::Schema::Loader qw/ make_schema_at /;
    make_schema_at(
        'New::Schema::Name',
        { debug => 1 },
        [ 'dbi:Pg:dbname="foo"','postgres','',
          { loader_class => 'MyLoader' } # optionally
        ],
    );

    # Inside a script, specifying a dump directory in which to write
    # class files
    use DBIx::Class::Schema::Loader qw/ make_schema_at /;
    make_schema_at(
        'New::Schema::Name',
        { debug => 1, dump_directory => './lib' },
        [ 'dbi:Pg:dbname="foo"','postgres','',
          { loader_class => 'MyLoader' } # optionally
        ],
    );

The last hashref in the C<\@connect_info> is checked for loader arguments such
as C<loader_options> and C<loader_class>, see L</connection> for more details.

=cut

sub make_schema_at {
    my ($target, $opts, $connect_info) = @_;

    {
        no strict 'refs';
        @{$target . '::ISA'} = qw/DBIx::Class::Schema::Loader/;
    }

    $target->_loader_invoked(0);

    $target->loader_options($opts);

    my $temp_schema = $target->connect(@$connect_info);

    $target->storage($temp_schema->storage);
    $target->storage->set_schema($target);

    return $target;
}

=head2 rescan

=over 4

=item Return Value: @new_monikers

=back

Re-scans the database for newly added tables since the initial
load, and adds them to the schema at runtime, including relationships,
etc.  Does not process drops or changes.

Returns a list of the new monikers added.

=cut

sub rescan { my $self = shift; $self->loader->rescan($self) }

=head2 naming

=over 4

=item Arguments: \%opts | $ver

=back

Controls the naming options for backward compatibility, see
L<DBIx::Class::Schema::Loader::Base/naming> for details.

To upgrade a dynamic schema, use:

    __PACKAGE__->naming('current');

Can be imported into your dump script and called as a function as well:

    naming('v4');

=head2 use_namespaces

=over 4

=item Arguments: 1|0

=back

Controls the use_namespaces options for backward compatibility, see
L<DBIx::Class::Schema::Loader::Base/use_namespaces> for details.

To upgrade a dynamic schema, use:

    __PACKAGE__->use_namespaces(1);

Can be imported into your dump script and called as a function as well:

    use_namespaces(1);

=head1 KNOWN ISSUES

=head2 Multiple Database Schemas

See L<DBIx::Class::Schema::Loader::Base/db_schema>.

=head1 ACKNOWLEDGEMENTS

Matt S Trout, all of the #dbix-class folks, and everyone who's ever sent
in a bug report or suggestion.

Based on L<DBIx::Class::Loader> by Sebastian Riedel

Based upon the work of IKEBE Tomohiro

=head1 AUTHOR

blblack: Brandon Black <blblack@gmail.com>

=head1 CONTRIBUTORS

ilmari: Dagfinn Ilmari MannsE<aring>ker <ilmari@ilmari.org>

arcanez: Justin Hunter <justin.d.hunter@gmail.com>

ash: Ash Berlin <ash@cpan.org>

btilly: Ben Tilly <btilly@gmail.com>

Caelum: Rafael Kitover <rkitover@cpan.org>

TSUNODA Kazuya <drk@drk7.jp>

rbo: Robert Bohne <rbo@cpan.org>

ribasushi: Peter Rabbitson <ribasushi@cpan.org>

gugu: Andrey Kostenko <a.kostenko@rambler-co.ru>

jhannah: Jay Hannah <jay@jays.net>

jnap: John Napiorkowski <jjn1056@yahoo.com>

rbuels: Robert Buels <rbuels@gmail.com>

timbunce: Tim Bunce <timb@cpan.org>

mst: Matt S. Trout <mst@shadowcatsystems.co.uk>

mstratman: Mark A. Stratman <stratman@gmail.com>

kane: Jos Boumans <kane@cpan.org>

waawaamilk: Nigel McNie <nigel@mcnie.name>

acmoore: Andrew Moore <amoore@cpan.org>

bphillips: Brian Phillips <bphillips@cpan.org>

schwern: Michael G. Schwern <mschwern@cpan.org>

SineSwiper: Brendan Byrd <byrd.b@insightcom.com>

hobbs: Andrew Rodland <arodland@cpan.org>

domm: Thomas Klausner <domm@plix.at>

spb: Stephen Bennett <spb@exherbo.org>

Matias E. Fernandez <mfernandez@pisco.ch>

alnewkirk: Al Newkirk <awncorp@cpan.org>

angelixd: Paul C. Mantz <pcmantz@cpan.org>

andrewalker: André Walker <andre@andrewalker.net>

... and lots of other folks. If we forgot you, please write the current
maintainer or RT.

=head1 COPYRIGHT & LICENSE

Copyright (c) 2006 - 2009 by the aforementioned
L<DBIx::Class::Schema::Loader/AUTHOR> and
L<DBIx::Class::Schema::Loader/CONTRIBUTORS>.

This library is free software; you can redistribute it and/or modify it under
the same terms as Perl itself.

=head1 SEE ALSO

L<DBIx::Class>, L<DBIx::Class::Manual::Intro>, L<DBIx::Class::Tutorial>,
L<DBIx::Class::Schema::Loader::Base>

=cut

1;
# vim:et sts=4 sw=4 tw=0:
