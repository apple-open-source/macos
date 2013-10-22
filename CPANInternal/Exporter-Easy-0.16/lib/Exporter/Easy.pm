# $Header: /home/fergal/my/cvs/Exporter-Easy/lib/Exporter/Easy.pm,v 1.24 2003/02/14 16:53:20 fergal Exp $

use strict;

package Exporter::Easy;

require 5.006;

require Exporter;

use vars;

our $VERSION = '0.16';

sub import
{
	my $pkg = shift;

	unshift(@_, scalar caller);

	# must goto or we lose the use vars functionality

	goto &set_export_vars;
}

sub set_export_vars
{
	# this handles setting up all of the EXPORT variables in the callers
	# package. It gives a nice way of creating tags, allows you to use tags
	# when defining @EXPORT, @EXPORT_FAIL and other in tags. It also takes
	# care of @EXPORT_OK.
	
	my ($callpkg, %args) = @_;

	my %could_export; # symbols that could be exported
	my @will_export; # symbols that will be exported by default
	my @fail; # symbols that should be tested before export
	my @ok_only; # the symbols that are ok to export

	my %tags; # will contain a ref hash of all tags

	@_ = (); # we'll be using this for vars to be use vars'd

	if ($args{OK_ONLY} and $args{OK})
	{
		nice_die("Can't use OK_ONLY and OK together");
	}

	my $isa = exists $args{ISA} ? delete $args{ISA} : 1;
	my $vars = exists $args{VARS} ? delete $args{VARS} : 1;

	if (my $tag_data = delete $args{'TAGS'})
	{
		nice_die("TAGS must be a reference to an array") unless ref($tag_data) eq 'ARRAY';

		add_tags($tag_data, \%tags);

		@could_export{map {@$_} values %tags} = ();
	}

	if (my $export = delete $args{'EXPORT'})
	{
		nice_die("EXPORT must be a reference to an array")
			unless ref($export) eq 'ARRAY';
		
		@will_export = eval { expand_tags($export, \%tags) };
		nice_die("$@while building the EXPORT list in $callpkg") if $@;
	}

	if (my $ok = delete $args{'OK'})
	{
		nice_die("OK must be a reference to a array") unless ref($ok) eq 'ARRAY';

		my @ok = eval { expand_tags($ok, \%tags) };
		nice_die("$@while building the \@EXPORT_OK") if $@;
		@could_export{@ok} = ();
	}

	my $ok_only = delete $args{'OK_ONLY'};
	if ($ok_only)
	{
		die("OK_ONLY must be a reference to a array") unless ref($ok_only) eq 'ARRAY';

		@ok_only = eval { expand_tags($ok_only, \%tags) };
		nice_die("$@while building the OK_ONLY list") if $@;

		@could_export{@ok_only} = ();
	}

	if (my $fail = delete $args{'FAIL'})
	{
		die "FAIL must be a reference to an array" unless ref($fail) eq 'ARRAY';

		@fail = eval { expand_tags($fail, \%tags) };
		nice_die("$@while building \@EXPORT_FAIL") if $@;
		@could_export{@fail} = ();
	}

	my @could_export = keys %could_export;

	if (defined(my $all = delete $args{'ALL'}))
	{
		nice_die("No name supplied for ALL") unless length($all);

		nice_die("Cannot use '$all' for ALL, already exists") if exists $tags{$all};

		my %all;
		@all{@could_export, @will_export} = ();

		$tags{$all} = [keys %all];
	}

	if ($vars)
	{
		if (my $ref = ref($vars))
		{
			nice_die("VARS was a reference to a ".$ref." instead of an array")
				unless $ref eq 'ARRAY';
			@_ = ('vars', grep /^(?:\$|\@|\%)/, eval { expand_tags($vars, \%tags) });
			nice_die("$@while building the \@EXPORT") if $@;
		}
		else
		{
			@_ = ('vars', grep /^(?:\$|\@|\%)/, @will_export, @could_export);
		}
	}

	if (%args)
	{
		nice_die("Attempt to use unknown keys: ", join(", ", keys %args));
	}

	no strict 'refs';
	if ($isa)
	{
		push(@{"$callpkg\::ISA"}, "Exporter");
	}

	@{"$callpkg\::EXPORT"} = @will_export if @will_export;
	%{"$callpkg\::EXPORT_TAGS"} = %tags if %tags;
	@{"$callpkg\::EXPORT_OK"} = $ok_only ? @ok_only : @could_export;
	@{"$callpkg\::EXPORT_FAIL"} = @fail if @fail;

	if (@_ > 1)
	{
		goto &vars::import;
	}
}

sub nice_die
{
	my $msg = shift;
	my $level = shift || 1;

	my ($pkg, $file, $line) = caller(1);

	die "$msg at $file line $line\n";
}

sub add_tags($;$)
{
	# this takes a reference to tag data and an optional reference to a hash
	# of already exiting tags. If no hash ref is supplied then it creates an
	# empty one
	
	# It adds the tags from the tag data to the hash ref.

	my $tag_data = shift;
	my $tags = shift || {};

	my @tag_data = @$tag_data;
	while (@tag_data)
	{
		my $tag_name = shift @tag_data || die "No name for tag";
		die "Tag name cannot be a reference, maybe you left out a comma"
			if (ref $tag_name);

		die "Tried to redefine tag '$tag_name'"
			if (exists $tags->{$tag_name});

		my $tag_list = shift @tag_data || die "No values for tag '$tag_name'";

		die "Tag values for '$tag_name' is not a reference to an array"
			unless ref($tag_list) eq 'ARRAY';

		my @symbols = eval { expand_tags($tag_list, $tags) };
		die "$@while building tag '$tag_name'" if $@;

		$tags->{$tag_name} = \@symbols;
	}

	return $tags;
}

sub expand_tags($$)
{
	# this takes a list of strings. Each string can be a symbol, or a tag and
	# each may start with a ! to signify deletion.
	
	# We return a list of symbols where all the tag have been expanded and
	# some symbols may have been deleted

	# we die if we hit an unknown tag

	my ($string_list, $so_far) = @_;

	my %this_tag;

	foreach my $sym (@$string_list)
	{
		my @symbols; # list of symbols to add or delete
		my $remove = 0;

		if ($sym =~ s/^!//)
		{
			$remove = 1;
		}

		if ($sym =~ s/^://)
		{
			my $sub_tag = $so_far->{$sym};
			die "Tried to use an unknown tag '$sym'" unless defined($sub_tag);

			if ($remove)
			{
				delete @this_tag{@$sub_tag}
			}
			else
			{
				@this_tag{@$sub_tag} = ();
			}
		}
		else
		{
			if ($remove)
			{
				delete $this_tag{$sym};
			}
			else
			{
				$this_tag{$sym} = undef;
			}
		}
	}

	return keys %this_tag;
}

1;
__END__

=head1 NAME

Exporter::Easy - Takes the drudgery out of Exporting symbols

=head1 SYNOPSIS

In module YourModule.pm:

  package YourModule;
  use Exporter::Easy (
    OK => [ '$munge', 'frobnicate' ] # symbols to export on request
  );

In other files which wish to use YourModule:

  use ModuleName qw(frobnicate);      # import listed symbols
  frobnicate ($left, $right)          # calls YourModule::frobnicate

=head1 DESCRIPTION

Exporter::Easy makes using Exporter easy. In it's simplest case it allows
you to drop the boilerplate code that comes with using Exporter, so

  require Exporter;
  use base qw( Exporter );
  use vars qw( @EXPORT );
  @EXPORT = ( 'init' );

becomes

  use Exporter::Easy ( EXPORT => [ 'init' ] );

and more complicated situations where you use tags to build lists and more
tags become easy, like this

  use Exporter::Easy (
  	EXPORT => [qw( init :base )],
  	TAGS => [
  		base => [qw( open close )],
  		read => [qw( read sysread readline )],
  		write => [qw( print write writeline )],
  		misc => [qw( select flush )],
  		all => [qw( :base :read :write :misc)],
  		no_misc => [qw( :all !:misc )],
  	],
  	OK => [qw( some other stuff )],
  );

This will set C<@EXPORT>, C<@EXPORT_OK>, C<@EXPORT_FAIL> and C<%EXPORT_TAGS>
in the current package, add Exporter to that package's C<@ISA> and do a
C<use vars> on all the variables mentioned. The rest is handled as normal by
Exporter.

=head1 HOW TO USE IT

Put

	use Exporter::Easy ( KEY => value, ...);

in your package. Arguments are passes as key-value pairs, the following keys
are available

=over 4

=item TAGS

The value should be a reference to a list that goes like (TAG_NAME,
TAG_VALUE, TAG_NAME, TAG_VALUE, ...), where TAG_NAME is a string and
TAG_VALUE is a reference to an array of symbols and tags. For example

  TAGS => [
    file => [ 'open', 'close', 'read', 'write'],
    string => [ 'length', 'substr', 'chomp' ],
    hash => [ 'keys', 'values', 'each' ],
    all => [ ':file', ':string', ':hash' ],
    some => [':all', '!open', ':hash'],
  ]

This is used to fill the C<%EXPORT_TAGS> in your package. You can build tags
from other tags - in the example above the tag C<all> will contain all the
symbols from C<file>, C<string> and C<hash>. You can also subtract symbols
and tags - in the example above, C<some> contains the symbols from all but
with C<open> removed and all the symbols from C<hash> removed.

The rule is that any symbol starting with a ':' is taken to be a tag which
has been defined previously (if it's not defined you'll get an error). If a
symbol is preceded by a '!' it will be subtracted from the list, otherwise
it is added.

If you try to redefine a tag you will also get an error.

All the symbols which occur while building the tags are automatically added
your package's C<@EXPORT_OK> array.

=item OK

The value should be a reference to a list of symbols and tags (which will be
exapanded). These symbols will be added to the C<@EXPORT_OK> array in your
package. Using OK and and OK_ONLY together will give an error.

=item OK_ONLY

The value should be a reference to a list of symbols and tags (which will be
exapanded). The C<@EXPORT_OK> array in your package will contains only these
symbols.. This totally overrides the automatic population of this array. If
you just want to add some symbols to the list that Exporter::Easy has
automatically built then you should use OK instead. Using OK_ONLY and OK
together will give an error.

=item EXPORT

The value should be a reference to a list of symbol names and tags. Any tags
will be expanded and the resulting list of symbol names will be placed in
the C<@EXPORT> array in your package. The tag created by the ALL key is not
available at this stage.

=item FAIL

The value should be a reference to a list of symbol names and tags. The tags
will be expanded and the resulting list of symbol names will be placed in
the C<@EXPORT_FAIL> array in your package. They will also be added to
the C<@EXPORT_OK> list.

=item ALL

The value should be the name of tag that doesn't yet exist. This tag will
contain a list of all symbols which can be exported.

=item ISA

If you set this to 0 then Exporter will not be added to your C<@ISA> list.

=item VARS

If this is set to 1 or not provided then all $, @ and % variables mentioned
previously will be available to use in your package as if you had done a
C<use vars> on them. If it's set to a reference to a list of symbols and
tags then only those symbols will be available. If it's set to 0 then you'll
have to do your own C<use vars> in your package.

=back

=head1 PROCESSING ORDER

We need take the information provided and build @EXPORT, @EXPORT_OK,
@EXPORT_FAIL and %EXPORT_TAGS in the calling package. We may also
need to build a tag with all of the symbols and to make all the variables
useable under strict.

The arguments are processed in the following order: TAGS, EXPORT, OK,
OK_ONLY and FAIL, ALL, VARS and finally ISA. This means you cannot use the
tag created by ALL anywhere except in VARS (although vars defaults to using
all symbols anyway).

=head1 SEE ALSO

For details on what all these arrays and hashes actually do, see the
Exporter documentation.

=head1 AUTHOR

Written by Fergal Daly <fergal@esatclear.ie>.

=head1 LICENSE

Under the same license as Perl itself

=cut
