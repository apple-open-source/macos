package Class::Factory::Util;

use strict;
use vars qw($VERSION);

use Carp qw(confess);

$VERSION = '1.7';

1;

sub import
{
    my $caller = caller(0);

    {
        no strict 'refs';
        *{"${caller}::subclasses"} = \&_subclasses;
    }
}

# deprecated
sub subclasses { _subclasses(@_) }

sub _subclasses
{
    my $base = shift;

    $base =~ s,::,/,g;

    my %dirs = map { $_ => 1 } @INC;

    my $dir = substr( $INC{"$base.pm"}, 0, (length $INC{"$base.pm"}) - 3 );

    $dirs{$dir} = 1;

    my @packages = map { _scandir( "$_/$base" ) } keys %dirs;

    # Make list of unique elements
    my %packages = map { $_ => 1 } @packages;

    return sort keys %packages;
}

sub _scandir
{
    my $dir = shift;

    return unless -d $dir;

    opendir DIR, $dir
	or confess ("Cannot open directory $dir: $!");

    my @packages =
        ( map { substr($_, 0, length($_) - 3) }
          grep { substr($_, -3) eq '.pm' && -f "$dir/$_" }
          readdir DIR
        );

    closedir DIR
	or confess("Cannot close directory $dir: $!" );

    return @packages;
}

__END__

=head1 NAME

Class::Factory::Util - Provide utility methods for factory classes

=head1 SYNOPSIS

  package My::Class;

  use Class::Factory::Util;

  My::Class->subclasses;

=head1 DESCRIPTION

This module exports a method that is useful for factory classes.

=head1 USAGE

When this module is loaded, it creates a method in its caller named
C<subclasses()>.  This method returns a list of the available
subclasses for the package.  It does this by looking in C<@INC> as
well as the directory containing the caller, and finding any modules
in the immediate subdirectories of the calling module.

So if you have the modules "Foo::Base", "Foo::Base::Bar", and
"Foo::Base::Baz", then the return value of C<< Foo::Base->subclasses()
>> would be "Bar" and "Baz".

=head1 SUPPORT

Please submit bugs to the CPAN RT system at
http://rt.cpan.org/NoAuth/ReportBug.html?Queue=class-factory-util or
via email at bug-class-factory-util@rt.cpan.org.

=head1 AUTHOR

Dave Rolsky, <autarch@urth.org>.

Removed from Alzabo and packaged by Terrence Brannon,
<tbone@cpan.org>.

=head1 COPYRIGHT

Copyright (c) 2003-2007 David Rolsky.  All rights reserved.  This
program is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

The full text of the license can be found in the LICENSE file included
with this module.

=cut
