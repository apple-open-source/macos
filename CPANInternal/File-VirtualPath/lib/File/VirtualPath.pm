=head1 NAME

File::VirtualPath - Portable abstraction of a file/dir/url path

=cut

######################################################################

package File::VirtualPath;
require 5.004;

# Copyright (c) 1999-2003, Darren R. Duncan.  All rights reserved.  This module
# is free software; you can redistribute it and/or modify it under the same terms
# as Perl itself.  However, I do request that this copyright information and
# credits remain attached to the file.  If you modify this module and
# redistribute a changed version then please attach a note listing the
# modifications.  This module is available "as-is" and the author can not be held
# accountable for any problems resulting from its use.

use strict;
use warnings;
use vars qw($VERSION);
$VERSION = '1.011';

######################################################################

=head1 DEPENDENCIES

=head2 Perl Version

	5.004

=head2 Standard Modules

	I<none>

=head2 Nonstandard Modules

	I<none>

=head1 SYNOPSIS

=head2 Content of thin shell "startup.pl":

	#!/usr/bin/perl
	use strict;
	use warnings;

	my $root = "/home/johndoe/projects/aardvark";
	my $separator = "/";
	if( $^O =~ /Win/i ) {
		$root = "c:\\projects\\aardvark";
		$separator = "\\";
	}
	if( $^O =~ /Mac/i ) {
		$root = "Documents:projects:aardvark";
		$separator = ":";
	}

	use Aardvark;
	Aardvark->main( File::VirtualPath->new( $root, $separator ) );

	1;

=head2 Content of fat main program "Aardvark.pm"

	package Aardvark;
	use strict;
	use warnings;
	use File::VirtualPath;

	sub main {
		my (undef, $project_dir) = @_;
		my $prefs = &get_prefs( $project_dir->child_path_obj( 'config.pl' ) );
		&do_work( $prefs, $project_dir );
	}

	sub get_prefs {
		my ($project_dir) = @_;
		my $real_filename = $project_dir->physical_path_string();
		my $prefs = do $real_filename;
		defined( $prefs ) or do {
			my $virtual_fn = $project_dir->path_string();
			die "Can't get Aardvark prefs from file '$virtual_fn': $!";
		};
		return( $prefs );
	}

	sub do_work {
		my ($prefs, $project_dir) = @_;
		my ($lbl_a, $lbl_b, $lbl_c) = ($prefs->{a}, $prefs->{b}, $prefs->{c});
		my $data_source = $prefs->{'sourcefile'};
		open( SOURCE, $project_dir->physical_child_path_string( $data_source ) );
		while( my $line = <SOURCE> ) {
			my ($a, $b, $c) = split( "\t", $line );
			print "File contains: $lbl_a='$a', $lbl_b='$b', $lbl_c='$c'\n";
		}
		close( SOURCE );
	}

	1;

=head2 Content of settings file "config.pl"

	$rh_prefs = {
		sourcefile => 'mydata.txt',
		a => 'name',
		b => 'phone',
		c => 'date',
	};

=head1 DESCRIPTION

This Perl 5 object class implements a portable abstraction of a resource path,
examples of which include file-system paths like "/usr/bin/perl" and URLs like
"http://www.cpan.org/modules/".  It is designed to support applications that are
easily portable across systems because common platform-specific details are
abstracted away.  Abstracted details include the location of your project within
the file-system and the path separator for your OS; you can write your
application as if it is in the root directory of a UNIX system, and it will
function correctly when moved to any subdirectory or to a Mac or Windows system.

=head1 OVERVIEW

This class is implemented as a simple data structure which stores an array of
path segments such as ['', 'usr', 'bin', 'perl'] in a virtual file-system. The
majority part of your application works with these objects and passes them around
during its routines of locating config or data or other files.

As your application navigates the virtual file-system, it uses object methods
like chdir() to tell the object where the app thinks it is now.  When your
program actually needs to use files, it asks a method like physical_path_string()
to give it a string representing the current path in the real world, which it
then passes to your standard I/O functions like open().

For example, the program may think it is sitting in "/config/access", but it
actually makes an open call to "/home/johndoe/projects/aardvark/config/access". 
If you move the "aardvark" project to a Windows system, the real path may have
changed to "c:\projects\aardvark\config\access", but your program would never
need to know the difference (aside from any internal file format issues).

In order for this to work, a small part of your program needs to know the truth
of where the project it is working on is located.  But that part can be a very
lightweight shim which initializes a single File::VirtualPath object and then
passes it to the fat portable part of the program.  There are two bits of data
that your shim needs to provide: 1. A string having the full real-world path of
your project root directory; 2. A string having the real-world path separator.
See the SYNOPSIS for an example.

Then, your main program just needs to assume that the argument it was passed is 
currently in the virtual root directory and go from there.

	THIN CONFIG SHELL <----> File::VirtualPath <----> FAT PROGRAM CORE
	(may be portable)        (portable)               (portable)

Taking this idea further, it is easy for program code to be reused for multiple 
projects, simultaneously, because each would only need a different thin shim 
program which points to a different physical directory as the virtual root.

Taking this idea further, File::VirtualPath makes it easier for you to separate
your application into components that have their own files to keep track of. 
When your main program calls a component, it can pass a modified FVP object which
that component uses as its own virtual root.  And so you can have multiple
instances of program components each working in different directories, and no
logic for working this out needs to be in the components themselves.

On a final note, the paths returned by this class are all absolute.  Therefore 
you never need to do a real "chdir" or "cd" operation in your program, and your 
executable doesn't have to be located in the same place as its data.  This is 
particularly useful if you are calling your program using a link/alias/shortcut.

=cut

######################################################################

# Names of properties for objects of this class are declared here:
my $KEY_PHYSICAL_ROOT = 'physical_root';  # str - physical path of virtual root
my $KEY_PHYSICAL_DELI = 'physical_deli';  # str - physical delim for path elems
my $KEY_VIR_PATH_DELI = 'vir_path_deli';  # str - delim for vir path elements
my $KEY_VIR_PATH_ELEM = 'vir_path_elem';  # array - virtual path we represent
my $KEY_VIR_PATH_LEVE = 'vir_path_leve';  # num - path elem ind we are examining

######################################################################

=head1 SYNTAX

This class does not export any functions or methods, so you need to call them
using object notation.  This means using B<Class-E<gt>function()> for functions
and B<$object-E<gt>method()> for methods.  If you are inheriting this class for
your own modules, then that often means something like B<$self-E<gt>method()>. 

Paths can be represented as either strings or array refs, and any methods which 
take absolute or relative paths as arguments can take either format.  A literal 
list will not work.  Methods which return paths usually come in pairs, and their 
names differ only in that one has a "_string" suffix; each will return either an 
array ref or a string.  Literal lists are never returned, even in list context.

A path is "absolute" when its array representation has an empty string as its 
first element, or its string representation begins with a "/".  Note that a 
simple split or join operation on "/" will cleanly convert one into the other.  
Conversely, a path is "relative" when its array representation has anything but 
an empty string (or undef) in its first element, or its string representation 
does not start with a "/".

In the virtual file-system that objects of this class represent, the root 
directory is called "/" and path separators are also "/"; this is just like UNIX.  
String representations of the virtual path are split or joined on the same "/".  
For your convenience, the path_delimiter() method lets you change the string 
that has these dual purposes.

Whenever you see any CHANGE_VECTOR arguments mentioned below, realize that they 
can be either absolute or relative paths.  The effects of using either is the 
same as with your normal "chdir" or "cd" functions.  If CHANGE_VECTOR is an 
absolute path then the entire path becomes it; whereas, if that argument is a 
relative path then it is applied to the current absolute path and a new absolute 
path results.  Usual conventions have alphanumeric path segments going down one 
directory level, ".." segments going up one level, and "." not going anywhere.  

If an absolute path is taken as an argument or derived from a relative path, it 
is always reduced to its simplest form before being stored or returned.  Mainly 
this ensures that there are no ".." or "." remaining in the path.  Any ".." 
path segments are paired up with previous alphanumeric list elements; these 
negate each other and both are removed.  If any ".." can not be paired up then 
they are simply removed since you can not navigate higher than the root; note 
that this would only happen if we are passed a malformed argument.  This 
precaution can also act as a pseudo-security measure by never returning a 
physical path that is outside the virtual root.

=head1 FUNCTIONS AND METHODS

=head2 new([ PHY_ROOT[, PHY_DELIM[, VIR_DELIM[, VIR_PATH]]] ])

This function creates a new File::VirtualPath (or subclass) object and
returns it.  All of the method arguments are passed to initialize() as is; please
see the POD for that method for an explanation of them.

=cut

######################################################################

sub new {
	my $class = shift( @_ );
	my $self = bless( {}, ref($class) || $class );
	$self->initialize( @_ );
	return( $self );
}

######################################################################

=head2 initialize([ PHY_ROOT[, PHY_DELIM[, VIR_DELIM[, VIR_PATH]]] ])

This method is used by B<new()> to set the initial properties of objects that it
creates.  The 4 optional arguments allow you to set the default values for the 
four object properties that the following methods also handle: physical_root(), 
physical_delimiter(), path_delimiter, path().  Semantecs are the same as calling 
those 4 methods yourself in the same order. 

=cut

######################################################################

sub initialize {
	my ($self, $root, $phy_delim, $vir_delim, $elem) = @_;
	$self->{$KEY_PHYSICAL_ROOT} = '';  # default is virt root = phys root
	$self->{$KEY_PHYSICAL_DELI} = '/';  # default is UNIX
	$self->{$KEY_VIR_PATH_DELI} = '/';  # default is UNIX
	$self->{$KEY_VIR_PATH_ELEM} = [''];  # default vir path is virtual root
	$self->{$KEY_VIR_PATH_LEVE} = 0;  # default is virtual root
	$self->physical_root( $root );
	$self->physical_delimiter( $phy_delim );
	$self->path_delimiter( $vir_delim );
	$self->path( $elem );
}

######################################################################

=head2 clone([ CLONE ])

This method initializes a new object to have all of the same properties of the
current object and returns it.  This new object can be provided in the optional
argument CLONE (if CLONE is an object of the same class as the current object);
otherwise, a brand new object of the current class is used.  Only object
properties recognized by File::VirtualPath are set in the clone; other
properties are not changed.

=cut

######################################################################

sub clone {
	my ($self, $clone) = @_;
	ref($clone) eq ref($self) or $clone = bless( {}, ref($self) );
	$clone->{$KEY_PHYSICAL_ROOT} = $self->{$KEY_PHYSICAL_ROOT};
	$clone->{$KEY_PHYSICAL_DELI} = $self->{$KEY_PHYSICAL_DELI};
	$clone->{$KEY_VIR_PATH_DELI} = $self->{$KEY_VIR_PATH_DELI};
	$clone->{$KEY_VIR_PATH_ELEM} = [@{$self->{$KEY_VIR_PATH_ELEM}}];
	$clone->{$KEY_VIR_PATH_LEVE} = $self->{$KEY_VIR_PATH_LEVE};
	return( $clone );
}

######################################################################

=head2 physical_root([ NEW_VALUE ])

This method is an accessor for the scalar "physical root" property of this 
object, which it returns.  If NEW_VALUE is defined, this property is set to it.  
This property defines what path on the real file-system the virtual root 
corresponds to.  This property defaults to an empty string.  This property must 
not have any trailing delimiter like "/".

=cut

######################################################################

sub physical_root {
	my ($self, $new_value) = @_;
	if( defined( $new_value ) ) {
		$self->{$KEY_PHYSICAL_ROOT} = $new_value;
	}
	return( $self->{$KEY_PHYSICAL_ROOT} );
}

######################################################################

=head2 physical_delimiter([ NEW_VALUE ])

This method is an accessor for the scalar "physical delimiter" property of this 
object, which it returns.  If NEW_VALUE is defined, this property is set to it.  
This property defines what the path delimiter in the real file-system is.
This property defaults to "/", which is the UNIX standard.

=cut

######################################################################

sub physical_delimiter {
	my ($self, $new_value) = @_;
	if( defined( $new_value ) ) {
		$self->{$KEY_PHYSICAL_DELI} = $new_value;
	}
	return( $self->{$KEY_PHYSICAL_DELI} );
}

######################################################################

=head2 path_delimiter([ NEW_VALUE ])

This method is an accessor for the scalar "path delimiter" property of this 
object, which it returns.  If NEW_VALUE is defined, this property is set to it.  
This property defines what the path delimiter in the virtual file-system is.
This property defaults to "/", which is the UNIX standard.

=cut

######################################################################

sub path_delimiter {
	my ($self, $new_value) = @_;
	if( defined( $new_value ) ) {
		$self->{$KEY_VIR_PATH_DELI} = $new_value;
	}
	return( $self->{$KEY_VIR_PATH_DELI} );
}

######################################################################

=head2 path([ NEW_VALUE ])

This method is an accessor for the array-ref "path" property of this 
object, which it returns.  If NEW_VALUE is defined, this property is set to it.  
This property defines what absolute path in the virtual file-system this object 
represents.  This property defaults to the virtual root.

=cut

######################################################################

sub path {
	my ($self, $new_value) = @_;
	if( defined( $new_value ) ) {
		my @elements = ('', ref( $new_value ) eq 'ARRAY' ?
			@{$new_value} : @{$self->_path_str_to_ra( $new_value )});
		$self->{$KEY_VIR_PATH_ELEM} = $self->_simplify_path_ra( \@elements );
	}
	return( [@{$self->{$KEY_VIR_PATH_ELEM}}] );
}

######################################################################

=head2 child_path( CHANGE_VECTOR )

This method uses CHANGE_VECTOR to derive a new path relative to what this object 
represents and returns it as an array-ref.

=cut

######################################################################

sub child_path {
	my ($self, $chg_vec) = @_;
	my $ra_elements = $self->_join_two_path_ra( $self->{$KEY_VIR_PATH_ELEM}, 
		ref( $chg_vec ) eq 'ARRAY' ? $chg_vec :
		$self->_path_str_to_ra( $chg_vec ) );
	return( $self->_simplify_path_ra( $ra_elements ) );
}

######################################################################

=head2 child_path_obj( CHANGE_VECTOR )

This method uses CHANGE_VECTOR to derive a new path relative to what this object 
represents and uses it as the "path" attribute of a new object of this class, 
which it returns.  All other attributes of the new object are cloned.

=cut

######################################################################

sub child_path_obj {
	my ($self, $chg_vec) = @_;
	my $obj = bless( {}, ref($self) );
	$obj->{$KEY_PHYSICAL_ROOT} = $self->{$KEY_PHYSICAL_ROOT};
	$obj->{$KEY_PHYSICAL_DELI} = $self->{$KEY_PHYSICAL_DELI};
	$obj->{$KEY_VIR_PATH_DELI} = $self->{$KEY_VIR_PATH_DELI};
	$obj->{$KEY_VIR_PATH_ELEM} = $self->child_path( $chg_vec );
	$obj->{$KEY_VIR_PATH_LEVE} = $self->{$KEY_VIR_PATH_LEVE};
	return( $obj );
}

######################################################################

=head2 chdir( CHANGE_VECTOR )

This method uses CHANGE_VECTOR to derive a new path relative to what this object 
represents and then changes this object to represent the new path.  The effect 
is conceptually the same as using "chdir" to change your current working 
directory where this object represents such.

=cut

######################################################################

sub chdir {
	my ($self, $chg_vec) = @_;
	return( $self->{$KEY_VIR_PATH_ELEM} = $self->child_path( $chg_vec ) );
}

######################################################################

=head2 path_string([ WANT_TRAILER ])

This method returns the absolute path on the virtual file-system that this object 
represents as a string.  If WANT_TRAILER is true then the string has a path 
delimiter appended; otherwise, there is none.

=cut

######################################################################

sub path_string {
	my ($self, $tra) = @_;
	$tra and $tra = $self->{$KEY_VIR_PATH_DELI} or $tra = '';
	return( $self->_path_ra_to_str( $self->{$KEY_VIR_PATH_ELEM} ).$tra );
}

######################################################################

=head2 physical_path_string([ WANT_TRAILER ])

This method returns the absolute path on the real file-system that this object 
represents as a string.  If WANT_TRAILER is true then the string has a path 
delimiter appended; otherwise, there is none.

=cut

######################################################################

sub physical_path_string {
	my ($self, $tra) = @_;
	$tra and $tra = $self->{$KEY_PHYSICAL_DELI} or $tra = '';
	return( $self->_path_ra_to_phy_str( $self->{$KEY_VIR_PATH_ELEM} ).$tra );
}

######################################################################

=head2 child_path_string( CHANGE_VECTOR[, WANT_TRAILER] )

This method uses CHANGE_VECTOR to derive a new path in the virtual file-system 
relative to what this object represents and returns it as a string.  If 
WANT_TRAILER is true then the string has a path delimiter appended; otherwise, 
there is none.

=cut

######################################################################

sub child_path_string {
	my ($self, $chg_vec, $tra) = @_;
	$tra and $tra = $self->{$KEY_VIR_PATH_DELI} or $tra = '';
	return( $self->_path_ra_to_str( $self->child_path( $chg_vec ) ).$tra );
}

######################################################################

=head2 physical_child_path_string( CHANGE_VECTOR[, WANT_TRAILER] )

This method uses CHANGE_VECTOR to derive a new path in the real file-system 
relative to what this object represents and returns it as a string.  If 
WANT_TRAILER is true then the string has a path delimiter appended; otherwise, 
there is none.

=cut

######################################################################

sub physical_child_path_string {
	my ($self, $chg_vec, $tra) = @_;
	$tra and $tra = $self->{$KEY_PHYSICAL_DELI} or $tra = '';
	return( $self->_path_ra_to_phy_str( $self->child_path( $chg_vec ) ).$tra );
}

######################################################################

=head2 path_element( INDEX[, NEW_VALUE] )

This method is an accessor for individual segments of the "path" property of 
this object, and it returns the one at INDEX.  If NEW_VALUE is defined then 
the segment at INDEX is set to it.  This method is useful if you want to examine 
virtual path segments one at a time.  INDEX defaults to 0, meaning you are 
looking at the first segment, which happens to always be empty.  That said, this 
method will let you change this condition if you want to.

=cut

######################################################################

sub path_element {
	my ($self, $index, $new_value) = @_;
	$index ||= 0;
	if( defined( $new_value ) ) {
		$self->{$KEY_VIR_PATH_ELEM}->[$index] = $new_value;
	}
	return( $self->{$KEY_VIR_PATH_ELEM}->[$index] );
}

######################################################################

=head2 current_path_level([ NEW_VALUE ])

This method is an accessor for the number "current path level" property of this 
object, which it returns.  If NEW_VALUE is defined, this property is set to it.  
If you want to examine the virtual path segments sequentially then this property 
tracks the index of the segment you are currently viewing.  This property 
defaults to 0, the first segment, which always happens to be an empty string.

=cut

######################################################################

sub current_path_level {
	my ($self, $new_value) = @_;
	if( defined( $new_value ) ) {
		$self->{$KEY_VIR_PATH_LEVE} = 0 + $new_value;
	}
	return( $self->{$KEY_VIR_PATH_LEVE} );
}

######################################################################

=head2 inc_path_level([ NEW_VALUE ])

This method will increment this object's "current path level" property by 1 so 
you can view the next path segment.  The new current value is returned.

=cut

######################################################################

sub inc_path_level {
	my $self = shift( @_ );
	return( ++$self->{$KEY_VIR_PATH_LEVE} );
}

######################################################################

=head2 dec_path_level([ NEW_VALUE ])

This method will decrement this object's "current path level" property by 1 so 
you can view the previous path segment.  The new current value is returned.  

=cut

######################################################################

sub dec_path_level {
	my $self = shift( @_ );
	return( --$self->{$KEY_VIR_PATH_LEVE} );
}

######################################################################

=head2 current_path_element([ NEW_VALUE ])

This method is an accessor for individual segments of the "path" property of 
this object, the current one of which it returns.  If NEW_VALUE is defined then 
the current segment is set to it.  This method is useful if you want to examine 
virtual path segments one at a time in sequence.  The segment you are looking at 
now is determined by the current_path_level() method; by default you are looking 
at the first segment, which is always an empty string.  That said, this method 
will let you change this condition if you want to.

=cut

######################################################################

sub current_path_element {
	my ($self, $new_value) = @_;
	my $curr_elem_num = $self->{$KEY_VIR_PATH_LEVE};
	if( defined( $new_value ) ) {
		$self->{$KEY_VIR_PATH_ELEM}->[$curr_elem_num] = $new_value;
	}
	return( $self->{$KEY_VIR_PATH_ELEM}->[$curr_elem_num] );
}

######################################################################
# _path_str_to_ra( PATH_STR )
# This private method takes a string representing an absolute or relative 
# virtual path and splits it on any "/" into an array ref list of path levels.  

sub _path_str_to_ra {
	my ($self, $in) = @_;
	$in ||= '';  # avoid uninitialized value warning
	return( [split( $self->{$KEY_VIR_PATH_DELI}, $in )] );
}

######################################################################
# _path_ra_to_str( PATH_RA )
# This private method takes an array ref list of path levels and joins it 
# with "/" into a string representing an absolute or relative virtual path.

sub _path_ra_to_str {
	my ($self, $in) = @_;
	return( join( $self->{$KEY_VIR_PATH_DELI}, @{$in} ) );
}

######################################################################
# _path_ra_to_phy_str( PATH_RA )
# This private method takes an array ref containing a complete virtual path 
# and joins it into a string that is the equivalent absolute physical path.

sub _path_ra_to_phy_str {
	my ($self, $in) = @_;
	my $root = $self->{$KEY_PHYSICAL_ROOT};
	return( $root.join( $self->{$KEY_PHYSICAL_DELI}, @{$in} ) );
}

######################################################################
# _join_two_path_ra( CURRENT_PATH_RA, CHANGE_VECTOR_RA )
# This private method takes two array refs, each having virtual path levels, 
# and combines them into one array ref.  An analogy for what this method does 
# is that it operates like the "cd" or "chdir" command but in the virtual space.
# CURRENT_PATH_RA is an absolute path saying what the current directory is 
# before the change, and this method returns an absolute path for the current 
# directory after the change.  CHANGE_VECTOR_RA is either an absolute or 
# relative path.  If it is absolute, then it becomes the whole path that is 
# returned.  If it is relative, then this method appends it to the end of 
# CURRENT_PATH_RA and returns the longer list.  Well, actually, this method 
# will return a relative path if CURRENT_PATH_RA is relative and 
# CHANGE_VECTOR_RA is not absolute, since two relatives are then being combined 
# to produce a new relative.  Regardless, you should pass this method's return 
# value to _simplify_path_ra() to get rid of anomalies like ".." or "." in the 
# middle or end of the path.

sub _join_two_path_ra {
	my ($self, $curr, $chg) = @_;
	return( @{$chg} && $chg->[0] eq '' ? [@{$chg}] : [@{$curr}, @{$chg}] );
}

######################################################################
# _simplify_path_ra( SOURCE )
# This private method takes an array ref having virtual path levels and 
# reduces it to its simplest form.  Mainly this ensures that there are no ".." 
# or "." in the middle or end of the array.  Any ".." list elements are paired 
# up with previous alphanumeric list elements; these negate each other and both 
# are removed.  If any ".." can't be paired with previous elements then they 
# are kept at the start of the path if the path is relative; if the path is 
# absolute then the ".." is simply dropped since you can not navigate higher 
# than the virtual root.  Any "." are simply removed since they are redundant.
# We determine whether SOURCE is absolute by whether the first element is an 
# empty string or not; an empty string means absolute and otherwise means not.

sub _simplify_path_ra {
	my ($self, $source) = @_;
	my @in = @{$source};  # store source elements here
	my @mid = ();  # store alphanumeric outputs here
	my @out = $in[0] eq '' ? shift( @in ) : ();  # make note if absolute or not

	foreach my $part (@in) {
		$part =~ /[a-zA-Z0-9]/ and push( @mid, $part ) and next;  # keep alpnums
		$part ne '..' and next;  # skip over "." and the like
		@mid ? pop( @mid ) : push( @out, '..' );  # neg ".." if we can or hold
	}

	$out[0] eq '' and @out = '';  # If absolute then toss any leading ".."
	push( @out, @mid );  # add remaining non-neg alphanumerics to output
	return( \@out );
}

######################################################################

1;
__END__

=head1 AUTHOR

Copyright (c) 1999-2003, Darren R. Duncan.  All rights reserved.  This module
is free software; you can redistribute it and/or modify it under the same terms
as Perl itself.  However, I do request that this copyright information and
credits remain attached to the file.  If you modify this module and
redistribute a changed version then please attach a note listing the
modifications.  This module is available "as-is" and the author can not be held
accountable for any problems resulting from its use.

I am always interested in knowing how my work helps others, so if you put this
module to use in any of your own products or services then I would appreciate
(but not require) it if you send me the website url for said product or
service, so I know who you are.  Also, if you make non-proprietary changes to
the module because it doesn't work the way you need, and you are willing to
make these freely available, then please send me a copy so that I can roll
desirable changes into the main release.

Address comments, suggestions, and bug reports to B<perl@DarrenDuncan.net>.

=head1 CREDITS

Thanks to Baldvin Kovacs <baldvin@fazekas.hu> for alerting me to the
"uninitialized value" warnings (and offering a patch to fix it) that appear
when running the test suite with the -w option (fixed in 1.01), and also thanks
for a patch to the README file documentation, which was applied.

=head1 SEE ALSO

perl(1), CGI::Portable.

=cut
