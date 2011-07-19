
package Class::MOP;

use strict;
use warnings;

use 5.008;

use MRO::Compat;

use Carp          'confess';
use Scalar::Util  'weaken', 'reftype', 'blessed';
use Try::Tiny;

use Class::MOP::Mixin::AttributeCore;
use Class::MOP::Mixin::HasAttributes;
use Class::MOP::Mixin::HasMethods;
use Class::MOP::Class;
use Class::MOP::Attribute;
use Class::MOP::Method;

BEGIN {
    *IS_RUNNING_ON_5_10 = ($] < 5.009_005)
        ? sub () { 0 }
        : sub () { 1 };

    # this is either part of core or set up appropriately by MRO::Compat
    *check_package_cache_flag = \&mro::get_pkg_gen;
}

our $VERSION   = '0.98';
our $XS_VERSION = $VERSION;
$VERSION = eval $VERSION;
our $AUTHORITY = 'cpan:STEVAN';

require XSLoader;
XSLoader::load( __PACKAGE__, $XS_VERSION );


{
    # Metaclasses are singletons, so we cache them here.
    # there is no need to worry about destruction though
    # because they should die only when the program dies.
    # After all, do package definitions even get reaped?
    # Anonymous classes manage their own destruction.
    my %METAS;

    sub get_all_metaclasses         {        %METAS         }
    sub get_all_metaclass_instances { values %METAS         }
    sub get_all_metaclass_names     { keys   %METAS         }
    sub get_metaclass_by_name       { $METAS{$_[0]}         }
    sub store_metaclass_by_name     { $METAS{$_[0]} = $_[1] }
    sub weaken_metaclass            { weaken($METAS{$_[0]}) }
    sub does_metaclass_exist        { exists $METAS{$_[0]} && defined $METAS{$_[0]} }
    sub remove_metaclass_by_name    { delete $METAS{$_[0]}; return }

    # This handles instances as well as class names
    sub class_of {
        return unless defined $_[0];
        my $class = blessed($_[0]) || $_[0];
        return $METAS{$class};
    }

    # NOTE:
    # We only cache metaclasses, meaning instances of
    # Class::MOP::Class. We do not cache instance of
    # Class::MOP::Package or Class::MOP::Module. Mostly
    # because I don't yet see a good reason to do so.
}

sub _class_to_pmfile {
    my $class = shift;

    my $file = $class . '.pm';
    $file =~ s{::}{/}g;

    return $file;
}

sub load_first_existing_class {
    my @classes = @_
      or return;

    foreach my $class (@classes) {
        unless ( _is_valid_class_name($class) ) {
            my $display = defined($class) ? $class : 'undef';
            confess "Invalid class name ($display)";
        }
    }

    my $found;
    my %exceptions;

    for my $class (@classes) {
        my $file = _class_to_pmfile($class);

        return $class if is_class_loaded($class);;

        return $class if try {
            local $SIG{__DIE__};
            require $file;
            return 1;
        }
        catch {
            unless (/^Can't locate \Q$file\E in \@INC/) {
                confess "Couldn't load class ($class) because: $_";
            }

            return;
        };
    }

    if ( @classes > 1 ) {
        confess "Can't locate any of @classes in \@INC (\@INC contains: @INC).";
    } else {
        confess "Can't locate " . _class_to_pmfile($classes[0]) . " in \@INC (\@INC contains: @INC).";
    }
}

sub load_class {
    load_first_existing_class($_[0]);

    # This is done to avoid breaking code which checked the return value. Said
    # code is dumb. The return value was _always_ true, since it dies on
    # failure!
    return 1;
}

sub _is_valid_class_name {
    my $class = shift;

    return 0 if ref($class);
    return 0 unless defined($class);
    return 0 unless length($class);

    return 1 if $class =~ /^\w+(?:::\w+)*$/;

    return 0;
}

## ----------------------------------------------------------------------------
## Setting up our environment ...
## ----------------------------------------------------------------------------
## Class::MOP needs to have a few things in the global perl environment so
## that it can operate effectively. Those things are done here.
## ----------------------------------------------------------------------------

# ... nothing yet actually ;)

## ----------------------------------------------------------------------------
## Bootstrapping
## ----------------------------------------------------------------------------
## The code below here is to bootstrap our MOP with itself. This is also
## sometimes called "tying the knot". By doing this, we make it much easier
## to extend the MOP through subclassing and such since now you can use the
## MOP itself to extend itself.
##
## Yes, I know, thats weird and insane, but it's a good thing, trust me :)
## ----------------------------------------------------------------------------

# We need to add in the meta-attributes here so that
# any subclass of Class::MOP::* will be able to
# inherit them using _construct_instance

## --------------------------------------------------------
## Class::MOP::Mixin::HasMethods

Class::MOP::Mixin::HasMethods->meta->add_attribute(
    Class::MOP::Attribute->new('_methods' => (
        reader   => {
            # NOTE:
            # we just alias the original method
            # rather than re-produce it here
            '_full_method_map' => \&Class::MOP::Mixin::HasMethods::_full_method_map
        },
        default => sub { {} }
    ))
);

Class::MOP::Mixin::HasMethods->meta->add_attribute(
    Class::MOP::Attribute->new('method_metaclass' => (
        reader   => {
            # NOTE:
            # we just alias the original method
            # rather than re-produce it here
            'method_metaclass' => \&Class::MOP::Mixin::HasMethods::method_metaclass
        },
        default  => 'Class::MOP::Method',
    ))
);

Class::MOP::Mixin::HasMethods->meta->add_attribute(
    Class::MOP::Attribute->new('wrapped_method_metaclass' => (
        reader   => {
            # NOTE:
            # we just alias the original method
            # rather than re-produce it here
            'wrapped_method_metaclass' => \&Class::MOP::Mixin::HasMethods::wrapped_method_metaclass
        },
        default  => 'Class::MOP::Method::Wrapped',
    ))
);

## --------------------------------------------------------
## Class::MOP::Mixin::HasMethods

Class::MOP::Mixin::HasAttributes->meta->add_attribute(
    Class::MOP::Attribute->new('attributes' => (
        reader   => {
            # NOTE: we need to do this in order
            # for the instance meta-object to
            # not fall into meta-circular death
            #
            # we just alias the original method
            # rather than re-produce it here
            '_attribute_map' => \&Class::MOP::Mixin::HasAttributes::_attribute_map
        },
        default  => sub { {} }
    ))
);

Class::MOP::Mixin::HasAttributes->meta->add_attribute(
    Class::MOP::Attribute->new('attribute_metaclass' => (
        reader   => {
            # NOTE:
            # we just alias the original method
            # rather than re-produce it here
            'attribute_metaclass' => \&Class::MOP::Mixin::HasAttributes::attribute_metaclass
        },
        default  => 'Class::MOP::Attribute',
    ))
);

## --------------------------------------------------------
## Class::MOP::Package

Class::MOP::Package->meta->add_attribute(
    Class::MOP::Attribute->new('package' => (
        reader   => {
            # NOTE: we need to do this in order
            # for the instance meta-object to
            # not fall into meta-circular death
            #
            # we just alias the original method
            # rather than re-produce it here
            'name' => \&Class::MOP::Package::name
        },
    ))
);

Class::MOP::Package->meta->add_attribute(
    Class::MOP::Attribute->new('namespace' => (
        reader => {
            # NOTE:
            # we just alias the original method
            # rather than re-produce it here
            'namespace' => \&Class::MOP::Package::namespace
        },
        init_arg => undef,
        default  => sub { \undef }
    ))
);

## --------------------------------------------------------
## Class::MOP::Module

# NOTE:
# yeah this is kind of stretching things a bit,
# but truthfully the version should be an attribute
# of the Module, the weirdness comes from having to
# stick to Perl 5 convention and store it in the
# $VERSION package variable. Basically if you just
# squint at it, it will look how you want it to look.
# Either as a package variable, or as a attribute of
# the metaclass, isn't abstraction great :)

Class::MOP::Module->meta->add_attribute(
    Class::MOP::Attribute->new('version' => (
        reader => {
            # NOTE:
            # we just alias the original method
            # rather than re-produce it here
            'version' => \&Class::MOP::Module::version
        },
        init_arg => undef,
        default  => sub { \undef }
    ))
);

# NOTE:
# By following the same conventions as version here,
# we are opening up the possibility that people can
# use the $AUTHORITY in non-Class::MOP modules as
# well.

Class::MOP::Module->meta->add_attribute(
    Class::MOP::Attribute->new('authority' => (
        reader => {
            # NOTE:
            # we just alias the original method
            # rather than re-produce it here
            'authority' => \&Class::MOP::Module::authority
        },
        init_arg => undef,
        default  => sub { \undef }
    ))
);

## --------------------------------------------------------
## Class::MOP::Class

Class::MOP::Class->meta->add_attribute(
    Class::MOP::Attribute->new('superclasses' => (
        accessor => {
            # NOTE:
            # we just alias the original method
            # rather than re-produce it here
            'superclasses' => \&Class::MOP::Class::superclasses
        },
        init_arg => undef,
        default  => sub { \undef }
    ))
);

Class::MOP::Class->meta->add_attribute(
    Class::MOP::Attribute->new('instance_metaclass' => (
        reader   => {
            # NOTE: we need to do this in order
            # for the instance meta-object to
            # not fall into meta-circular death
            #
            # we just alias the original method
            # rather than re-produce it here
            'instance_metaclass' => \&Class::MOP::Class::instance_metaclass
        },
        default  => 'Class::MOP::Instance',
    ))
);

Class::MOP::Class->meta->add_attribute(
    Class::MOP::Attribute->new('immutable_trait' => (
        reader   => {
            'immutable_trait' => \&Class::MOP::Class::immutable_trait
        },
        default => "Class::MOP::Class::Immutable::Trait",
    ))
);

Class::MOP::Class->meta->add_attribute(
    Class::MOP::Attribute->new('constructor_name' => (
        reader   => {
            'constructor_name' => \&Class::MOP::Class::constructor_name,
        },
        default => "new",
    ))
);

Class::MOP::Class->meta->add_attribute(
    Class::MOP::Attribute->new('constructor_class' => (
        reader   => {
            'constructor_class' => \&Class::MOP::Class::constructor_class,
        },
        default => "Class::MOP::Method::Constructor",
    ))
);


Class::MOP::Class->meta->add_attribute(
    Class::MOP::Attribute->new('destructor_class' => (
        reader   => {
            'destructor_class' => \&Class::MOP::Class::destructor_class,
        },
    ))
);

# NOTE:
# we don't actually need to tie the knot with
# Class::MOP::Class here, it is actually handled
# within Class::MOP::Class itself in the
# _construct_class_instance method.

## --------------------------------------------------------
## Class::MOP::Mixin::AttributeCore
Class::MOP::Mixin::AttributeCore->meta->add_attribute(
    Class::MOP::Attribute->new('name' => (
        reader   => {
            # NOTE: we need to do this in order
            # for the instance meta-object to
            # not fall into meta-circular death
            #
            # we just alias the original method
            # rather than re-produce it here
            'name' => \&Class::MOP::Mixin::AttributeCore::name
        }
    ))
);

Class::MOP::Mixin::AttributeCore->meta->add_attribute(
    Class::MOP::Attribute->new('accessor' => (
        reader    => { 'accessor'     => \&Class::MOP::Mixin::AttributeCore::accessor     },
        predicate => { 'has_accessor' => \&Class::MOP::Mixin::AttributeCore::has_accessor },
    ))
);

Class::MOP::Mixin::AttributeCore->meta->add_attribute(
    Class::MOP::Attribute->new('reader' => (
        reader    => { 'reader'     => \&Class::MOP::Mixin::AttributeCore::reader     },
        predicate => { 'has_reader' => \&Class::MOP::Mixin::AttributeCore::has_reader },
    ))
);

Class::MOP::Mixin::AttributeCore->meta->add_attribute(
    Class::MOP::Attribute->new('initializer' => (
        reader    => { 'initializer'     => \&Class::MOP::Mixin::AttributeCore::initializer     },
        predicate => { 'has_initializer' => \&Class::MOP::Mixin::AttributeCore::has_initializer },
    ))
);

Class::MOP::Mixin::AttributeCore->meta->add_attribute(
    Class::MOP::Attribute->new('definition_context' => (
        reader    => { 'definition_context'     => \&Class::MOP::Mixin::AttributeCore::definition_context     },
    ))
);

Class::MOP::Mixin::AttributeCore->meta->add_attribute(
    Class::MOP::Attribute->new('writer' => (
        reader    => { 'writer'     => \&Class::MOP::Mixin::AttributeCore::writer     },
        predicate => { 'has_writer' => \&Class::MOP::Mixin::AttributeCore::has_writer },
    ))
);

Class::MOP::Mixin::AttributeCore->meta->add_attribute(
    Class::MOP::Attribute->new('predicate' => (
        reader    => { 'predicate'     => \&Class::MOP::Mixin::AttributeCore::predicate     },
        predicate => { 'has_predicate' => \&Class::MOP::Mixin::AttributeCore::has_predicate },
    ))
);

Class::MOP::Mixin::AttributeCore->meta->add_attribute(
    Class::MOP::Attribute->new('clearer' => (
        reader    => { 'clearer'     => \&Class::MOP::Mixin::AttributeCore::clearer     },
        predicate => { 'has_clearer' => \&Class::MOP::Mixin::AttributeCore::has_clearer },
    ))
);

Class::MOP::Mixin::AttributeCore->meta->add_attribute(
    Class::MOP::Attribute->new('builder' => (
        reader    => { 'builder'     => \&Class::MOP::Mixin::AttributeCore::builder     },
        predicate => { 'has_builder' => \&Class::MOP::Mixin::AttributeCore::has_builder },
    ))
);

Class::MOP::Mixin::AttributeCore->meta->add_attribute(
    Class::MOP::Attribute->new('init_arg' => (
        reader    => { 'init_arg'     => \&Class::MOP::Mixin::AttributeCore::init_arg     },
        predicate => { 'has_init_arg' => \&Class::MOP::Mixin::AttributeCore::has_init_arg },
    ))
);

Class::MOP::Mixin::AttributeCore->meta->add_attribute(
    Class::MOP::Attribute->new('default' => (
        # default has a custom 'reader' method ...
        predicate => { 'has_default' => \&Class::MOP::Mixin::AttributeCore::has_default },
    ))
);

Class::MOP::Mixin::AttributeCore->meta->add_attribute(
    Class::MOP::Attribute->new('insertion_order' => (
        reader      => { 'insertion_order' => \&Class::MOP::Mixin::AttributeCore::insertion_order },
        writer      => { '_set_insertion_order' => \&Class::MOP::Mixin::AttributeCore::_set_insertion_order },
        predicate   => { 'has_insertion_order' => \&Class::MOP::Mixin::AttributeCore::has_insertion_order },
    ))
);

## --------------------------------------------------------
## Class::MOP::Attribute
Class::MOP::Attribute->meta->add_attribute(
    Class::MOP::Attribute->new('associated_class' => (
        reader   => {
            # NOTE: we need to do this in order
            # for the instance meta-object to
            # not fall into meta-circular death
            #
            # we just alias the original method
            # rather than re-produce it here
            'associated_class' => \&Class::MOP::Attribute::associated_class
        }
    ))
);

Class::MOP::Attribute->meta->add_attribute(
    Class::MOP::Attribute->new('associated_methods' => (
        reader   => { 'associated_methods' => \&Class::MOP::Attribute::associated_methods },
        default  => sub { [] }
    ))
);

Class::MOP::Attribute->meta->add_method('clone' => sub {
    my $self  = shift;
    $self->meta->clone_object($self, @_);
});

## --------------------------------------------------------
## Class::MOP::Method
Class::MOP::Method->meta->add_attribute(
    Class::MOP::Attribute->new('body' => (
        reader   => { 'body' => \&Class::MOP::Method::body },
    ))
);

Class::MOP::Method->meta->add_attribute(
    Class::MOP::Attribute->new('associated_metaclass' => (
        reader   => { 'associated_metaclass' => \&Class::MOP::Method::associated_metaclass },
    ))
);

Class::MOP::Method->meta->add_attribute(
    Class::MOP::Attribute->new('package_name' => (
        reader   => { 'package_name' => \&Class::MOP::Method::package_name },
    ))
);

Class::MOP::Method->meta->add_attribute(
    Class::MOP::Attribute->new('name' => (
        reader   => { 'name' => \&Class::MOP::Method::name },
    ))
);

Class::MOP::Method->meta->add_attribute(
    Class::MOP::Attribute->new('original_method' => (
        reader   => { 'original_method'      => \&Class::MOP::Method::original_method },
        writer   => { '_set_original_method' => \&Class::MOP::Method::_set_original_method },
    ))
);

Class::MOP::Method->meta->add_method('clone' => sub {
    my $self  = shift;
    my $clone = $self->meta->clone_object($self, @_);
    $clone->_set_original_method($self);
    return $clone;
});

## --------------------------------------------------------
## Class::MOP::Method::Wrapped

# NOTE:
# the way this item is initialized, this
# really does not follow the standard
# practices of attributes, but we put
# it here for completeness
Class::MOP::Method::Wrapped->meta->add_attribute(
    Class::MOP::Attribute->new('modifier_table')
);

## --------------------------------------------------------
## Class::MOP::Method::Generated

Class::MOP::Method::Generated->meta->add_attribute(
    Class::MOP::Attribute->new('is_inline' => (
        reader   => { 'is_inline' => \&Class::MOP::Method::Generated::is_inline },
        default  => 0, 
    ))
);

Class::MOP::Method::Generated->meta->add_attribute(
    Class::MOP::Attribute->new('definition_context' => (
        reader   => { 'definition_context' => \&Class::MOP::Method::Generated::definition_context },
    ))
);


## --------------------------------------------------------
## Class::MOP::Method::Inlined

Class::MOP::Method::Inlined->meta->add_attribute(
    Class::MOP::Attribute->new('_expected_method_class' => (
        reader   => { '_expected_method_class' => \&Class::MOP::Method::Inlined::_expected_method_class },
    ))
);

## --------------------------------------------------------
## Class::MOP::Method::Accessor

Class::MOP::Method::Accessor->meta->add_attribute(
    Class::MOP::Attribute->new('attribute' => (
        reader   => {
            'associated_attribute' => \&Class::MOP::Method::Accessor::associated_attribute
        },
    ))
);

Class::MOP::Method::Accessor->meta->add_attribute(
    Class::MOP::Attribute->new('accessor_type' => (
        reader   => { 'accessor_type' => \&Class::MOP::Method::Accessor::accessor_type },
    ))
);

## --------------------------------------------------------
## Class::MOP::Method::Constructor

Class::MOP::Method::Constructor->meta->add_attribute(
    Class::MOP::Attribute->new('options' => (
        reader   => {
            'options' => \&Class::MOP::Method::Constructor::options
        },
        default  => sub { +{} }
    ))
);

Class::MOP::Method::Constructor->meta->add_attribute(
    Class::MOP::Attribute->new('associated_metaclass' => (
        init_arg => "metaclass", # FIXME alias and rename
        reader   => {
            'associated_metaclass' => \&Class::MOP::Method::Constructor::associated_metaclass
        },
    ))
);

## --------------------------------------------------------
## Class::MOP::Instance

# NOTE:
# these don't yet do much of anything, but are just
# included for completeness

Class::MOP::Instance->meta->add_attribute(
    Class::MOP::Attribute->new('associated_metaclass',
        reader   => { associated_metaclass => \&Class::MOP::Instance::associated_metaclass },
    ),
);

Class::MOP::Instance->meta->add_attribute(
    Class::MOP::Attribute->new('_class_name',
        init_arg => undef,
        reader   => { _class_name => \&Class::MOP::Instance::_class_name },
        #lazy     => 1, # not yet supported by Class::MOP but out our version does it anyway
        #default  => sub { $_[0]->associated_metaclass->name },
    ),
);

Class::MOP::Instance->meta->add_attribute(
    Class::MOP::Attribute->new('attributes',
        reader   => { attributes => \&Class::MOP::Instance::get_all_attributes },
    ),
);

Class::MOP::Instance->meta->add_attribute(
    Class::MOP::Attribute->new('slots',
        reader   => { slots => \&Class::MOP::Instance::slots },
    ),
);

Class::MOP::Instance->meta->add_attribute(
    Class::MOP::Attribute->new('slot_hash',
        reader   => { slot_hash => \&Class::MOP::Instance::slot_hash },
    ),
);

require Class::MOP::Deprecated unless our $no_deprecated;

# we need the meta instance of the meta instance to be created now, in order
# for the constructor to be able to use it
Class::MOP::Instance->meta->get_meta_instance;

# pretend the add_method never happenned. it hasn't yet affected anything
undef Class::MOP::Instance->meta->{_package_cache_flag};

## --------------------------------------------------------
## Now close all the Class::MOP::* classes

# NOTE: we don't need to inline the the accessors this only lengthens
# the compile time of the MOP, and gives us no actual benefits.

$_->meta->make_immutable(
    inline_constructor  => 0,
    constructor_name    => "_new",
    inline_accessors => 0,
) for qw/
    Class::MOP::Package
    Class::MOP::Module
    Class::MOP::Class

    Class::MOP::Attribute
    Class::MOP::Method
    Class::MOP::Instance

    Class::MOP::Object

    Class::MOP::Method::Generated
    Class::MOP::Method::Inlined

    Class::MOP::Method::Accessor
    Class::MOP::Method::Constructor
    Class::MOP::Method::Wrapped
/;

$_->meta->make_immutable(
    inline_constructor  => 0,
    constructor_name    => undef,
    inline_accessors => 0,
) for qw/
    Class::MOP::Mixin
    Class::MOP::Mixin::AttributeCore
    Class::MOP::Mixin::HasAttributes
    Class::MOP::Mixin::HasMethods
/;

1;

__END__

=pod

=head1 NAME

Class::MOP - A Meta Object Protocol for Perl 5

=head1 DESCRIPTION

This module is a fully functioning meta object protocol for the
Perl 5 object system. It makes no attempt to change the behavior or
characteristics of the Perl 5 object system, only to create a
protocol for its manipulation and introspection.

That said, it does attempt to create the tools for building a rich set
of extensions to the Perl 5 object system. Every attempt has been made
to abide by the spirit of the Perl 5 object system that we all know
and love.

This documentation is sparse on conceptual details. We suggest looking
at the items listed in the L<SEE ALSO> section for more
information. In particular the book "The Art of the Meta Object
Protocol" was very influential in the development of this system.

=head2 What is a Meta Object Protocol?

A meta object protocol is an API to an object system.

To be more specific, it abstracts the components of an object system
(classes, object, methods, object attributes, etc.). These
abstractions can then be used to inspect and manipulate the object
system which they describe.

It can be said that there are two MOPs for any object system; the
implicit MOP and the explicit MOP. The implicit MOP handles things
like method dispatch or inheritance, which happen automatically as
part of how the object system works. The explicit MOP typically
handles the introspection/reflection features of the object system.

All object systems have implicit MOPs. Without one, they would not
work. Explicit MOPs are much less common, and depending on the
language can vary from restrictive (Reflection in Java or C#) to wide
open (CLOS is a perfect example).

=head2 Yet Another Class Builder! Why?

This is B<not> a class builder so much as a I<class builder
B<builder>>. The intent is that an end user will not use this module
directly, but instead this module is used by module authors to build
extensions and features onto the Perl 5 object system.

This system is used by L<Moose>, which supplies a powerful class
builder system built entirely on top of C<Class::MOP>.

=head2 Who is this module for?

This module is for anyone who has ever created or wanted to create a
module for the Class:: namespace. The tools which this module provides
make doing complex Perl 5 wizardry simpler, by removing such barriers
as the need to hack symbol tables, or understand the fine details of
method dispatch.

=head2 What changes do I have to make to use this module?

This module was designed to be as unintrusive as possible. Many of its
features are accessible without B<any> change to your existing
code. It is meant to be a compliment to your existing code and not an
intrusion on your code base. Unlike many other B<Class::> modules,
this module B<does not> require you subclass it, or even that you
C<use> it in within your module's package.

The only features which requires additions to your code are the
attribute handling and instance construction features, and these are
both completely optional features. The only reason for this is because
Perl 5's object system does not actually have these features built
in. More information about this feature can be found below.

=head2 About Performance

It is a common misconception that explicit MOPs are a performance hit.
This is not a universal truth, it is a side-effect of some specific
implementations. For instance, using Java reflection is slow because
the JVM cannot take advantage of any compiler optimizations, and the
JVM has to deal with much more runtime type information as well.

Reflection in C# is marginally better as it was designed into the
language and runtime (the CLR). In contrast, CLOS (the Common Lisp
Object System) was built to support an explicit MOP, and so
performance is tuned for it.

This library in particular does its absolute best to avoid putting
B<any> drain at all upon your code's performance. In fact, by itself
it does nothing to affect your existing code. So you only pay for what
you actually use.

=head2 About Metaclass compatibility

This module makes sure that all metaclasses created are both upwards
and downwards compatible. The topic of metaclass compatibility is
highly esoteric and is something only encountered when doing deep and
involved metaclass hacking. There are two basic kinds of metaclass
incompatibility; upwards and downwards.

Upwards metaclass compatibility means that the metaclass of a
given class is either the same as (or a subclass of) all of the
class's ancestors.

Downward metaclass compatibility means that the metaclasses of a
given class's ancestors are all either the same as (or a subclass
of) that metaclass.

Here is a diagram showing a set of two classes (C<A> and C<B>) and
two metaclasses (C<Meta::A> and C<Meta::B>) which have correct
metaclass compatibility both upwards and downwards.

    +---------+     +---------+
    | Meta::A |<----| Meta::B |      <....... (instance of  )
    +---------+     +---------+      <------- (inherits from)
         ^               ^
         :               :
    +---------+     +---------+
    |    A    |<----|    B    |
    +---------+     +---------+

As I said this is a highly esoteric topic and one you will only run
into if you do a lot of subclassing of L<Class::MOP::Class>. If you
are interested in why this is an issue see the paper I<Uniform and
safe metaclass composition> linked to in the L<SEE ALSO> section of
this document.

=head2 Using custom metaclasses

Always use the L<metaclass> pragma when using a custom metaclass, this
will ensure the proper initialization order and not accidentally
create an incorrect type of metaclass for you. This is a very rare
problem, and one which can only occur if you are doing deep metaclass
programming. So in other words, don't worry about it.

Note that if you're using L<Moose> we encourage you to I<not> use
L<metaclass> pragma, and instead use L<Moose::Util::MetaRole> to apply
roles to a class's metaclasses. This topic is covered at length in
various L<Moose::Cookbook> recipes.

=head1 PROTOCOLS

The meta-object protocol is divided into 4 main sub-protocols:

=head2 The Class protocol

This provides a means of manipulating and introspecting a Perl 5
class. It handles symbol table hacking for you, and provides a rich
set of methods that go beyond simple package introspection.

See L<Class::MOP::Class> for more details.

=head2 The Attribute protocol

This provides a consistent representation for an attribute of a Perl 5
class. Since there are so many ways to create and handle attributes in
Perl 5 OO, the Attribute protocol provide as much of a unified
approach as possible. Of course, you are always free to extend this
protocol by subclassing the appropriate classes.

See L<Class::MOP::Attribute> for more details.

=head2 The Method protocol

This provides a means of manipulating and introspecting methods in the
Perl 5 object system. As with attributes, there are many ways to
approach this topic, so we try to keep it pretty basic, while still
making it possible to extend the system in many ways.

See L<Class::MOP::Method> for more details.

=head2 The Instance protocol

This provides a layer of abstraction for creating object instances.
Since the other layers use this protocol, it is relatively easy to
change the type of your instances from the default hash reference to
some other type of reference. Several examples are provided in the
F<examples/> directory included in this distribution.

See L<Class::MOP::Instance> for more details.

=head1 FUNCTIONS

Note that this module does not export any constants or functions.

=head2 Constants

=over 4

=item I<Class::MOP::IS_RUNNING_ON_5_10>

We set this constant depending on what version perl we are on, this
allows us to take advantage of new 5.10 features and stay backwards
compatible.

=back

=head2 Utility functions

Note that these are all called as B<functions, not methods>.

=over 4

=item B<Class::MOP::load_class($class_name)>

This will load the specified C<$class_name>, if it is not already
loaded (as reported by C<is_class_loaded>). This function can be used
in place of tricks like C<eval "use $module"> or using C<require>
unconditionally.

If the module cannot be loaded, an exception is thrown.

For historical reasons, this function explicitly returns a true value.

=item B<Class::MOP::is_class_loaded($class_name)>

Returns a boolean indicating whether or not C<$class_name> has been
loaded.

This does a basic check of the symbol table to try and determine as
best it can if the C<$class_name> is loaded, it is probably correct
about 99% of the time, but it can be fooled into reporting false
positives. In particular, loading any of the core L<IO> modules will
cause most of the rest of the core L<IO> modules to falsely report
having been loaded, due to the way the base L<IO> module works.

=item B<Class::MOP::get_code_info($code)>

This function returns two values, the name of the package the C<$code>
is from and the name of the C<$code> itself. This is used by several
elements of the MOP to determine where a given C<$code> reference is
from.

=item B<Class::MOP::class_of($instance_or_class_name)>

This will return the metaclass of the given instance or class name.  If the
class lacks a metaclass, no metaclass will be initialized, and C<undef> will be
returned.

=item B<Class::MOP::check_package_cache_flag($pkg)>

B<NOTE: DO NOT USE THIS FUNCTION, IT IS FOR INTERNAL USE ONLY!>

This will return an integer that is managed by L<Class::MOP::Class> to
determine if a module's symbol table has been altered.

In Perl 5.10 or greater, this flag is package specific. However in
versions prior to 5.10, this will use the C<PL_sub_generation>
variable which is not package specific.

=item B<Class::MOP::load_first_existing_class(@class_names)>

B<NOTE: DO NOT USE THIS FUNCTION, IT IS FOR INTERNAL USE ONLY!>

Given a list of class names, this function will attempt to load each
one in turn.

If it finds a class it can load, it will return that class' name.  If
none of the classes can be loaded, it will throw an exception.

=back

=head2 Metaclass cache functions

Class::MOP holds a cache of metaclasses. The following are functions
(B<not methods>) which can be used to access that cache. It is not
recommended that you mess with these. Bad things could happen, but if
you are brave and willing to risk it: go for it!

=over 4

=item B<Class::MOP::get_all_metaclasses>

This will return a hash of all the metaclass instances that have
been cached by L<Class::MOP::Class>, keyed by the package name.

=item B<Class::MOP::get_all_metaclass_instances>

This will return a list of all the metaclass instances that have
been cached by L<Class::MOP::Class>.

=item B<Class::MOP::get_all_metaclass_names>

This will return a list of all the metaclass names that have
been cached by L<Class::MOP::Class>.

=item B<Class::MOP::get_metaclass_by_name($name)>

This will return a cached L<Class::MOP::Class> instance, or nothing
if no metaclass exists with that C<$name>.

=item B<Class::MOP::store_metaclass_by_name($name, $meta)>

This will store a metaclass in the cache at the supplied C<$key>.

=item B<Class::MOP::weaken_metaclass($name)>

In rare cases (e.g. anonymous metaclasses) it is desirable to
store a weakened reference in the metaclass cache. This
function will weaken the reference to the metaclass stored
in C<$name>.

=item B<Class::MOP::does_metaclass_exist($name)>

This will return true of there exists a metaclass stored in the
C<$name> key, and return false otherwise.

=item B<Class::MOP::remove_metaclass_by_name($name)>

This will remove the metaclass stored in the C<$name> key.

=back

=head1 SEE ALSO

=head2 Books

There are very few books out on Meta Object Protocols and Metaclasses
because it is such an esoteric topic. The following books are really
the only ones I have found. If you know of any more, B<I<please>>
email me and let me know, I would love to hear about them.

=over 4

=item I<The Art of the Meta Object Protocol>

=item I<Advances in Object-Oriented Metalevel Architecture and Reflection>

=item I<Putting MetaClasses to Work>

=item I<Smalltalk: The Language>

=back

=head2 Papers

=over 4

=item "Uniform and safe metaclass composition"

An excellent paper by the people who brought us the original Traits paper.
This paper is on how Traits can be used to do safe metaclass composition,
and offers an excellent introduction section which delves into the topic of
metaclass compatibility.

L<http://www.iam.unibe.ch/~scg/Archive/Papers/Duca05ySafeMetaclassTrait.pdf>

=item "Safe Metaclass Programming"

This paper seems to precede the above paper, and propose a mix-in based
approach as opposed to the Traits based approach. Both papers have similar
information on the metaclass compatibility problem space.

L<http://citeseer.ist.psu.edu/37617.html>

=back

=head2 Prior Art

=over 4

=item The Perl 6 MetaModel work in the Pugs project

=over 4

=item L<http://svn.openfoundry.org/pugs/misc/Perl-MetaModel/>

=item L<http://svn.openfoundry.org/pugs/perl5/Perl6-ObjectSpace>

=back

=back

=head2 Articles

=over 4

=item CPAN Module Review of Class::MOP

L<http://www.oreillynet.com/onlamp/blog/2006/06/cpan_module_review_classmop.html>

=back

=head1 SIMILAR MODULES

As I have said above, this module is a class-builder-builder, so it is
not the same thing as modules like L<Class::Accessor> and
L<Class::MethodMaker>. That being said there are very few modules on CPAN
with similar goals to this module. The one I have found which is most
like this module is L<Class::Meta>, although it's philosophy and the MOP it
creates are very different from this modules.

=head1 BUGS

All complex software has bugs lurking in it, and this module is no
exception.

Please report any bugs to C<bug-class-mop@rt.cpan.org>, or through the
web interface at L<http://rt.cpan.org>.

You can also discuss feature requests or possible bugs on the Moose
mailing list (moose@perl.org) or on IRC at
L<irc://irc.perl.org/#moose>.

=head1 ACKNOWLEDGEMENTS

=over 4

=item Rob Kinyon

Thanks to Rob for actually getting the development of this module kick-started.

=back

=head1 AUTHORS

Stevan Little E<lt>stevan@iinteractive.comE<gt>

B<with contributions from:>

Brandon (blblack) Black

Florian (rafl) Ragwitz

Guillermo (groditi) Roditi

Dave (autarch) Rolsky

Matt (mst) Trout

Rob (robkinyon) Kinyon

Yuval (nothingmuch) Kogman

Scott (konobi) McWhirter

Dylan Hardison

=head1 COPYRIGHT AND LICENSE

Copyright 2006-2010 by Infinity Interactive, Inc.

L<http://www.iinteractive.com>

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=cut
