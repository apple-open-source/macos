package Class::MOP::Deprecated;

use strict;
use warnings;

use Carp qw( cluck );
use Scalar::Util qw( blessed );

our $VERSION = '0.98';
$VERSION = eval $VERSION;
our $AUTHORITY = 'cpan:STEVAN';

my %DeprecatedAt = (

    # features deprecated before 0.93
    'Class::MOP::HAVE_ISAREV'           => 0.93,
    'Class::MOP::subname'               => 0.93,
    'Class::MOP::in_global_destruction' => 0.93,

    'Class::MOP::Package::get_method_map' => 0.93,

    'Class::MOP::Class::construct_class_instance'          => 0.93,
    'Class::MOP::Class::check_metaclass_compatibility'     => 0.93,
    'Class::MOP::Class::create_meta_instance'              => 0.93,
    'Class::MOP::Class::clone_instance'                    => 0.93,
    'Class::MOP::Class::alias_method'                      => 0.93,
    'Class::MOP::Class::compute_all_applicable_methods'    => 0.93,
    'Class::MOP::Class::compute_all_applicable_attributes' => 0.93,
    'Class::MOP::Class::get_attribute_map' => 0.95,

    'Class::MOP::Instance::bless_instance_structure' => 0.93,

    'Class::MOP::Attribute::process_accessors' => 0.93,

    'Class::MOP::Method::Accessor::initialize_body'                  => 0.93,
    'Class::MOP::Method::Accessor::generate_accessor_method'         => 0.93,
    'Class::MOP::Method::Accessor::generate_reader_method'           => 0.93,
    'Class::MOP::Method::Accessor::generate_writer_method'           => 0.93,
    'Class::MOP::Method::Accessor::generate_predicate_method'        => 0.93,
    'Class::MOP::Method::Accessor::generate_clearer_method'          => 0.93,
    'Class::MOP::Method::Accessor::generate_accessor_method_inline'  => 0.93,
    'Class::MOP::Method::Accessor::generate_reader_method_inline'    => 0.93,
    'Class::MOP::Method::Accessor::generate_writer_method_inline'    => 0.93,
    'Class::MOP::Method::Accessor::generate_clearer_method_inline'   => 0.93,
    'Class::MOP::Method::Accessor::generate_predicate_method_inline' => 0.93,

    'Class::MOP::Method::Constructor::meta_instance'               => 0.93,
    'Class::MOP::Method::Constructor::attributes'                  => 0.93,
    'Class::MOP::Method::Constructor::initialize_body'             => 0.93,
    'Class::MOP::Method::Constructor::generate_constructor_method' => 0.93,
    'Class::MOP::Method::Constructor::generate_constructor_method_inline' =>
        0.93,

    # features deprecated after 0.93
    # ...
);

my %Registry;

sub import {
    my ( $class, %args ) = @_;

    if ( defined( my $compat_version = delete $args{-compatible} ) ) {
        $Registry{ (caller) } = $compat_version;
    }

    if (%args) {
        my $unknowns = join q{ }, keys %args;
        cluck "Unknown argument(s) for $class->import: $unknowns.\n";
    }
    return;
}

sub warn {
    my ( $package, undef, undef, $feature ) = caller(1);

    my $compat_version;
    while ( $package && !defined( $compat_version = $Registry{$package} ) ) {
        $package =~ s/ :: \w+ \z//xms or last;
    }

    my $deprecated_at = $DeprecatedAt{$feature}
        or die "Unregistered deprecated feature: $feature";

    if ( !defined($compat_version)
        || $compat_version >= $DeprecatedAt{$feature} ) {
        goto &cluck;
    }
}

package
    Class::MOP;

sub HAVE_ISAREV () {
    Class::MOP::Deprecated::warn(
        "Class::MOP::HAVE_ISAREV is deprecated and will be removed in a future release. It has always returned 1 anyway."
    );
    return 1;
}

sub subname {
    Class::MOP::Deprecated::warn(
        "Class::MOP::subname is deprecated. Please use Sub::Name directly.");
    require Sub::Name;
    goto \&Sub::Name::subname;
}

sub in_global_destruction {
    Class::MOP::Deprecated::warn(
        "Class::MOP::in_global_destruction is deprecated. Please use Devel::GlobalDestruction directly."
    );
    require Devel::GlobalDestruction;
    goto \&Devel::GlobalDestruction::in_global_destruction;
}

package
    Class::MOP::Package;

sub get_method_map {
    Class::MOP::Deprecated::warn(
              'The get_method_map method has been made private.'
            . " The public version is deprecated and will be removed in a future release.\n"
    );
    my $self = shift;

    my $map = $self->_full_method_map;

    $map->{$_} = $self->get_method($_)
        for grep { !blessed( $map->{$_} ) } keys %{$map};

    return $map;
}

package
    Class::MOP::Module;

package
    Class::MOP::Class;

sub construct_class_instance {
    Class::MOP::Deprecated::warn(
              'The construct_class_instance method has been made private.'
            . " The public version is deprecated and will be removed in a future release.\n"
    );
    shift->_construct_class_instance(@_);
}

sub check_metaclass_compatibility {
    Class::MOP::Deprecated::warn(
        'The check_metaclass_compatibility method has been made private.'
            . " The public version is deprecated and will be removed in a future release.\n"
    );
    shift->_check_metaclass_compatibility(@_);
}

sub construct_instance {
    Class::MOP::Deprecated::warn(
              'The construct_instance method has been made private.'
            . " The public version is deprecated and will be removed in a future release.\n"
    );
    shift->_construct_instance(@_);
}

sub create_meta_instance {
    Class::MOP::Deprecated::warn(
              'The create_meta_instance method has been made private.'
            . " The public version is deprecated and will be removed in a future release.\n"
    );
    shift->_create_meta_instance(@_);
}

sub clone_instance {
    Class::MOP::Deprecated::warn(
              'The clone_instance method has been made private.'
            . " The public version is deprecated and will be removed in a future release.\n"
    );
    shift->_clone_instance(@_);
}

sub alias_method {
    Class::MOP::Deprecated::warn(
        "The alias_method method is deprecated. Use add_method instead.\n");

    shift->add_method(@_);
}

sub compute_all_applicable_methods {
    Class::MOP::Deprecated::warn(
              'The compute_all_applicable_methods method is deprecated.'
            . " Use get_all_methods instead.\n" );

    return map {
        {
            name  => $_->name,
            class => $_->package_name,
            code  => $_,                 # sigh, overloading
        },
    } shift->get_all_methods(@_);
}

sub compute_all_applicable_attributes {
    Class::MOP::Deprecated::warn(
        'The compute_all_applicable_attributes method has been deprecated.'
            . " Use get_all_attributes instead.\n" );

    shift->get_all_attributes(@_);
}

sub get_attribute_map {
    Class::MOP::Deprecated::warn(
        "The get_attribute_map method has been deprecated.\n");

    shift->_attribute_map(@_);
}

package
    Class::MOP::Instance;

sub bless_instance_structure {
    Class::MOP::Deprecated::warn(
              'The bless_instance_structure method is deprecated.'
            . " It will be removed in a future release.\n" );

    my ( $self, $instance_structure ) = @_;
    bless $instance_structure, $self->_class_name;
}

package
    Class::MOP::Attribute;

sub process_accessors {
    Class::MOP::Deprecated::warn(
              'The process_accessors method has been made private.'
            . " The public version is deprecated and will be removed in a future release.\n"
    );
    shift->_process_accessors(@_);
}

package
    Class::MOP::Method::Accessor;

sub initialize_body {
    Class::MOP::Deprecated::warn(
              'The initialize_body method has been made private.'
            . " The public version is deprecated and will be removed in a future release.\n"
    );
    shift->_initialize_body;
}

sub generate_accessor_method {
    Class::MOP::Deprecated::warn(
              'The generate_accessor_method method has been made private.'
            . " The public version is deprecated and will be removed in a future release.\n"
    );
    shift->_generate_accessor_method;
}

sub generate_reader_method {
    Class::MOP::Deprecated::warn(
              'The generate_reader_method method has been made private.'
            . " The public version is deprecated and will be removed in a future release.\n"
    );
    shift->_generate_reader_method;
}

sub generate_writer_method {
    Class::MOP::Deprecated::warn(
              'The generate_writer_method method has been made private.'
            . " The public version is deprecated and will be removed in a future release.\n"
    );
    shift->_generate_writer_method;
}

sub generate_predicate_method {
    Class::MOP::Deprecated::warn(
              'The generate_predicate_method method has been made private.'
            . " The public version is deprecated and will be removed in a future release.\n"
    );
    shift->_generate_predicate_method;
}

sub generate_clearer_method {
    Class::MOP::Deprecated::warn(
              'The generate_clearer_method method has been made private.'
            . " The public version is deprecated and will be removed in a future release.\n"
    );
    shift->_generate_clearer_method;
}

sub generate_accessor_method_inline {
    Class::MOP::Deprecated::warn(
        'The generate_accessor_method_inline method has been made private.'
            . " The public version is deprecated and will be removed in a future release.\n"
    );
    shift->_generate_accessor_method_inline;
}

sub generate_reader_method_inline {
    Class::MOP::Deprecated::warn(
        'The generate_reader_method_inline method has been made private.'
            . " The public version is deprecated and will be removed in a future release.\n"
    );
    shift->_generate_reader_method_inline;
}

sub generate_writer_method_inline {
    Class::MOP::Deprecated::warn(
        'The generate_writer_method_inline method has been made private.'
            . " The public version is deprecated and will be removed in a future release.\n"
    );
    shift->_generate_writer_method_inline;
}

sub generate_predicate_method_inline {
    Class::MOP::Deprecated::warn(
        'The generate_predicate_method_inline method has been made private.'
            . " The public version is deprecated and will be removed in a future release.\n"
    );
    shift->_generate_predicate_method_inline;
}

sub generate_clearer_method_inline {
    Class::MOP::Deprecated::warn(
        'The generate_clearer_method_inline method has been made private.'
            . " The public version is deprecated and will be removed in a future release.\n"
    );
    shift->_generate_clearer_method_inline;
}

package
    Class::MOP::Method::Constructor;

sub meta_instance {
    Class::MOP::Deprecated::warn(
              'The meta_instance method has been made private.'
            . " The public version is deprecated and will be removed in a future release.\n"
    );
    shift->_meta_instance;
}

sub attributes {
    Class::MOP::Deprecated::warn(
              'The attributes method has been made private.'
            . " The public version is deprecated and will be removed in a future release.\n"
    );

    return shift->_attributes;
}

sub initialize_body {
    Class::MOP::Deprecated::warn(
              'The initialize_body method has been made private.'
            . " The public version is deprecated and will be removed in a future release.\n"
    );
    shift->_initialize_body;
}

sub generate_constructor_method {
    Class::MOP::Deprecated::warn(
              'The generate_constructor_method method has been made private.'
            . " The public version is deprecated and will be removed in a future release.\n"
    );
    shift->_generate_constructor_method;
}

sub generate_constructor_method_inline {
    Class::MOP::Deprecated::warn(
        'The generate_constructor_method_inline method has been made private.'
            . " The public version is deprecated and will be removed in a future release.\n"
    );
    shift->_generate_constructor_method_inline;
}

1;

__END__

=pod

=head1 NAME 

Class::MOP::Deprecated - List of deprecated methods

=head1 DESCRIPTION

    use Class::MOP::Deprecated -compatible => $version;

=head1 FUNCTIONS

This class provides methods that have been deprecated but remain for backward
compatibility.

If you specify C<< -compatible => $version >>, you can use deprecated features
without warnings. Note that this special treatment is limited to the package
that loads C<Class::MOP::Deprecated>.

=head1 AUTHORS

Goro Fuji E<lt>gfuji@cpan.orgE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright 2006-2010 by Infinity Interactive, Inc.

L<http://www.iinteractive.com>

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=cut
