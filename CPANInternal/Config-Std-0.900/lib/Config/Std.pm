package Config::Std;

our $VERSION = '0.900';

require v5.7.3; # RT#21184

my %global_def_sep;
my %global_inter_gap;

sub import {
    my ($package, $opt_ref) = @_;
    my $caller = caller();
    $global_def_sep{$caller} = $opt_ref->{def_sep};
    $global_inter_gap{$caller} = $opt_ref->{def_gap};
    for my $sub_name (qw( read_config write_config )) {
        $opt_ref->{$sub_name} ||= $sub_name;
    }
    *{$caller.'::'.$opt_ref->{read_config}}  = \&Config::Std::Hash::read_config;
    *{$caller.'::'.$opt_ref->{write_config}} = \&Config::Std::Hash::write_config;
}

package Config::Std::Gap;
use Class::Std;
{
    sub serialize { return "\n" }
    sub update  {}
    sub extend  {}
    sub copy_to {}
}

package Config::Std::Comment;
use Class::Std;
{
    my %text_of : ATTR( :init_arg<text> );

    sub serialize {
        my ($self) = @_;
        return $text_of{ident $self};
    }

    sub append_comment {
        my ($self, $new_text) = @_;
        $text_of{ident $self} .= $new_text;
    }

    sub update  {}
    sub extend  {}
    sub copy_to {}
}

package Config::Std::Keyval;
use Class::Std;
{
    my %key_of      : ATTR( :init_arg<key> :get<key> );
    my %vals_of     : ATTR;
    my %deleted_of  : ATTR;

    sub BUILD {
        my ($self, $ident, $arg_ref) = @_;

        $vals_of{$ident}     = [ { %{$arg_ref} } ];
    }

    my %SEPARATOR = ( ':' => ': ', '=' => ' = ' );

    use Carp;

    sub serialize {
        my ($self, $def_sep, $block_name) = @_;
        my $ident = ident $self;

        return "" if $deleted_of{$ident};

        my ($key, $vals) = ($key_of{$ident}, $vals_of{$ident});

        my $keyspace = q{ } x length($key);

        my $serialization = q{};

        for $n (0..$#{$vals}) {
            my ($val,$sep,$comm) = @{$vals->[$n]}{qw(val sep comm)};

            my $val_type = ref $val;
            croak qq{Can't save \L$val_type\E ref as value for key {'$block_name'}{'$key'} (only scalars or array refs)}
                if $val_type && $val_type ne 'ARRAY';

            $sep = $SEPARATOR{$sep || $def_sep};

            my @vals = $val_type eq 'ARRAY' ? @{$val} : $val;
            s/ (?!\Z) \n /\n$keyspace$sep/gxms for @vals;

            $serialization .= $comm || q{};

            $serialization .= join q{}, map {"$key$sep$_\n"} @vals;
        }

        return $serialization;
    }

    sub update { 
        my ($self, $hash_ref, $updated_ref) = @_;
        my $ident = ident $self;

        my $key = $key_of{$ident};

        if (!exists $hash_ref->{$key}) {
            $deleted_of{$ident} = 1;
        }
        else {
            my $val = $hash_ref->{$key};
            @newvals = ref $val eq 'ARRAY' ? @{$val} : $val;
            for my $n (0..$#newvals) {
                $vals_of{$ident}[$n]{val} = $newvals[$n];
            }
            splice @{$vals_of{$ident}}, scalar @newvals;
        }

        $updated_ref->{$key} = 1;

        return 1;
    }

    sub copy_to {
        my ($self, $hash_ref)  = @_;
        my $ident = ident $self;
        my @vals = map $_->{val}, @{$vals_of{$ident}};
        $hash_ref->{$key_of{$ident}} = @vals > 1 ? \@vals : $vals[0];
    }

    sub multivalue {
        my ($self, $sep, $val, $comm) = @_;
        push @{$vals_of{ident $self}}, {val=>$val, sep=>$sep, comm=>$comm};
    }
}

package Config::Std::Block;
use Class::Std;
{
    my %name_of         : ATTR( :init_arg<name> :get<name> default => '' );
    my %sep_count_of    : ATTR;
    my %precomm_of      : ATTR( :init_arg<precomm> default => '' );
    my %parcomm_of      : ATTR( :init_arg<parcomm> default => '' );
    my %components_of   : ATTR;
    my %deleted_of      : ATTR;
    my %seen            : ATTR;
    my %is_first        : ATTR( :init_arg<first> default => '' );

    sub BUILD {
        my ($self, $ident) = @_;
        @{$sep_count_of{$ident}}{':','='} = (0,0);
        $components_of{$ident} = [];
        $seen{$ident} = {};
    }

    sub copy_to {
        my ($self, $hash_ref) = @_;
        my $ident = ident $self;

        my $keyvals = $hash_ref->{$name_of{$ident}} ||= {};

        for my $comp ( @{$components_of{$ident}} ) {
            $comp->copy_to($keyvals);
        }

        $hash_ref->{$name_of{$ident}} = $keyvals;
    }

    sub serialize {
        my ($self, $first, $caller, $post_gap, $inter_gap) = @_;
        my $ident = ident $self;

        return q{} if $deleted_of{$ident};

        my $is_anon = $first && length($name_of{$ident}) == 0;

        my $serialization = q{};
        if (!$is_anon) {
            $serialization = ($precomm_of{$ident} || q{})
                           . "[$name_of{$ident}]"
                           . (defined $parcomm_of{$ident}?$parcomm_of{$ident}:q{})
                           . "\n";
        }

        my $gds = $global_def_sep{$caller};
        my $def_sep
            = defined $gds                                             ? $gds
            : $sep_count_of{$ident}{':'} >= $sep_count_of{$ident}{'='} ? ':'
            :                                                            '='
            ;

        $self->ensure_gap() if $inter_gap && !$is_anon;

        for my $comp ( @{$components_of{$ident}} ) {
            $serialization .= $comp->serialize($def_sep, $name_of{$ident});
        }

        return $serialization;
    }

    sub update {
        my ($self, $hash_ref, $updated_ref) = @_;
        my $ident = ident $self;

        if (!defined $hash_ref) {
            $deleted_of{$ident} = 1;
            return;
        }

        for my $comp ( @{$components_of{$ident}} ) {
            $comp->update($hash_ref, $updated_ref) or next;
        }
    }

    sub extend {
        my ($self, $hash_ref, $updated_ref, $post_gap, $inter_gap) = @_;

        # Only the first occurrence of a block has new keys added...
        return unless $is_first{ident $self};

        my $first = 1;
        for my $key ( grep {!$updated_ref->{$_}} keys %{$hash_ref}) {
            my $value = $hash_ref->{$key};
            my $separate = ref $value || $value =~ m/\n./xms;
            $self->ensure_gap() if ($first ? $post_gap : $inter_gap)
                                    || $separate;
            $self->add_keyval($key, undef, $hash_ref->{$key});
            $self->add_gap() if $separate;
            $first = 0;
        }
    }

    sub ensure_gap {
        my ($self) = @_;
        my $comp_ref = $components_of{ident $self};
        return if @{$comp_ref} && $comp_ref->[-1]->isa('Config::Std::Gap');
        push @{$comp_ref}, Config::Std::Gap->new();
    }

    sub add_gap {
        my ($self) = @_;
        push @{$components_of{ident $self}}, Config::Std::Gap->new();
    }

    sub add_comment {
        my ($self, $text) = @_;
        my $comp_ref = $components_of{ident $self};
        if ($comp_ref && @{$comp_ref} && $comp_ref->[-1]->isa('Config::Std::Comment') ) {
            $comp_ref->[-1]->append_comment($text);
        }
        else {
            push @{$comp_ref}, Config::Std::Comment->new({text=>$text});
        }
    }

    sub add_keyval {
        my ($self, $key, $sep, $val, $comm) = @_;
        my $ident = ident $self;

        $sep_count_of{$ident}{$sep}++ if $sep;

        my $seen = $seen{$ident};

        if ($seen->{$key}) {
            $seen->{$key}->multivalue($sep, $val, $comm);
            return;
        }

        my $keyval 
            = Config::Std::Keyval->new({key=>$key, sep=>$sep, val=>$val, comm=>$comm});
        push @{$components_of{$ident}}, $keyval;
        $seen->{$key} = $keyval;
    }
}

package Config::Std::Hash;
use Class::Std;
{

    use Carp;
    use Fcntl ':flock';     # import LOCK_* constants

    my %post_section_gap_for :ATTR;
    my %array_rep_for        :ATTR;
    my %filename_for         :ATTR;

    sub write_config (\[%$];$) {
        my ($hash_ref, $filename) = @_;
        $hash_ref = ${$hash_ref} if ref $hash_ref eq 'REF';

        $filename = $filename_for{$hash_ref} if @_<2;

        croak "Missing filename for call to write_config()"
            unless $filename;

        my $caller = caller;

        my $inter_gap
            = exists $global_inter_gap{$caller} ? $global_inter_gap{$caller}
            :                                      1;
        my $post_gap
            = $post_section_gap_for{$hash_ref}
            || (defined $global_inter_gap{$caller} ? $global_inter_gap{$caller}
                                                   : 1
               );

        # Update existing keyvals in each block...
        my %updated;
        for my $block ( @{$array_rep_for{$hash_ref}} ) {
            my $block_name = $block->get_name();
            $block->update($hash_ref->{$block_name}, $updated{$block_name}||={});
        }

        # Add new keyvals to the first section of block...
        for my $block ( @{$array_rep_for{$hash_ref}} ) {
            my $block_name = $block->get_name();
            $block->extend($hash_ref->{$block_name}, $updated{$block_name},
                           $post_gap, inter_gap
                          );
        }

        # Add new blocks at the end...
        for my $block_name ( sort grep {!$updated{$_}} keys %{$hash_ref} ) {
            my $block = Config::Std::Block->new({name=>$block_name});
            my $subhash = $hash_ref->{$block_name};
            my $first = 1;
            for my $key ( keys %{$subhash} ) {
                if (!defined $subhash->{$key}) {
                    croak "Can't save undefined value for key {'$block_name'}{'$key'} (only scalars or array refs)";
                }
                my $value = $subhash->{$key};
                my $separate = ref $value || $value =~ m/\n./xms;
                $block->ensure_gap() if ($first ? $post_gap : $inter_gap)
                                     || $separate;
                $block->add_keyval($key, undef, $value);
                $block->add_gap() if $separate;
                $first = 0;
            }
            $block->ensure_gap();
            push @{$array_rep_for{$hash_ref}}, $block;
        }

        open my $fh, '>', $filename
            or croak "Can't open config file '$filename' for writing (\L$!\E)";

        flock($fh,LOCK_EX|LOCK_NB)
            || croak "Can't write to locked config file '$filename'"
                if ! ref $filename;

        my $first = 1;
        for my $block ( @{$array_rep_for{$hash_ref}} ) {
            print {$fh} $block->serialize($first, scalar caller, $post_gap);
            $first = 0;
        }

        flock($fh,LOCK_UN) if ! ref $filename;

        return 1;
    }

    sub read_config ($\[%$]) {
        my ($filename, $var_ref, $opt_ref) = @_;
        my $var_type = ref($var_ref) || q{};
        my $hash_ref;
        if ($var_type eq 'SCALAR' && !defined ${$var_ref} ) {
            ${$var_ref} = $hash_ref = {};
        }
        elsif ($var_type eq 'HASH') {
            $hash_ref = $var_ref;
        }
        else {
            croak q{Scalar second argument to 'read_config' must be empty};
        }

        bless $hash_ref, 'Config::Std::Hash';

        my $blocks = $array_rep_for{$hash_ref}
                   = _load_config_for($filename, $hash_ref);

        for my $block ( @{$blocks} ) {
            $block->copy_to($hash_ref);
        }

        $filename_for{$hash_ref} = $filename;

        # Remove initial empty section if no data...
        if (!keys %{ $hash_ref->{q{}} }) {
            delete $hash_ref->{q{}};
        }

        return 1;
    }

    sub _load_config_for {
        my ($filename, $hash_ref) = @_;

        open my $fh, '<', $filename
            or croak "Can't open config file '$filename' (\L$!\E)";
        flock($fh,LOCK_SH|LOCK_NB)
            || croak "Can't read from locked config file '$filename'"
                if !ref $filename;
        my $text = do{local $/; <$fh>};
        flock($fh,LOCK_UN) if !ref $filename;

        my @config_file = Config::Std::Block->new({ name=>q{}, first=>1 });
        my $comment = q{};
        my %seen;

        # Start tracking whether section markers have gaps after them...
        $post_section_gap_for{$hash_ref} = 0;

        for ($text) {
            pos = 0;
            while (pos() < length() ) {
                # Gap...
                if (m/\G (?: [^\S\n]* (?:\n|\z)+)/gcxms) {
                    ### Found gap
                    $config_file[-1]->add_comment($comment) if $comment;
                    $config_file[-1]->add_gap();
                    $comment = q{};
                }

                # Comment...
                elsif (m/\G (\s* [#;] [^\n]* (?:\n|\z) )/gcxms) {
                    ### Found comment: $1
                    $comment .= $1;
                }

                # Block...
                elsif (m/\G ([^\S\n]*) [[]  ( [^]\n]* ) []] ( ([^\S\n]*) [#;] [^\n]* )? [^\S\n]* (?:\n|\z)/gcxms) {
                    my ($pre, $name, $parcomm, $ws) = ($1, $2, $3, $4);
                    ### Found block: $name
                    if ($parcomm) {
                        $pre = 2 + length($pre) + length($name) + length($ws);
                        if (m/\G ( (?: \n? [ ]{$pre,} [#] [^\n]* )+ )/gcxms) {
                            $parcomm .= "\n$1";
                        }
                    }
                    push @config_file,
                            Config::Std::Block->new({
                                name    => $name,
                                precomm => $comment,
                                parcomm => $parcomm,
                                first   => !$seen{$name}++,
                            });
                    $comment = q{};

                    # Check for trailing gap...
                    $post_section_gap_for{$hash_ref}
                        += m/\G (?= [^\S\n]* (?:\n|\z) )/xms ? +1 : -1;
                }

                # Key/value...
                elsif (m/\G [^\S\n]* ([^=:\n]+?) [^\S\n]* ([:=] [^\S\n]*) ([^\n]*) (?:\n|\z)/gcxms) {
                    my ($key, $sep, $val) = ($1, $2, $3);

                    my $pure_sep = $sep;
                    $pure_sep =~ s/\s*//g;

                    # Continuation lines...
                    my $continued = 0;
                    while (m/\G [^\S\n]* \Q$sep\E ([^\n]*) (?:\n|\z) /gcxms
                       ||  m/\G [^\S\n]* \Q$pure_sep\E ([^\n]*) (?:\n|\z) /gcxms
                    ) {
                        $val .= "\n$1";
                        $continued = 1;
                    }

                    $val =~ s/\A \s*|\s* \z//gxms if !$continued;

                    ### Found kv: $key, $val

                    $config_file[-1]->add_keyval($key, $pure_sep, $val,
                    $comment); $comment = q{}; }

                # Mystery...
                else {
                    my ($problem) = m/\G ([^\n]{10,40}|.{10}) /gcxms;
                    die "Error in config file '$filename' near:\n\n\t$problem\n";
                }
            }
        }

        return \@config_file;
    }

}


1; # Magic true value required at end of module
__END__

=head1 NAME

Config::Std - Load and save configuration files in a standard format


=head1 VERSION

This document describes Config::Std version 0.900


=head1 SYNOPSIS

    use Config::Std;

    # Load named config file into specified hash...
    read_config 'demo2.cfg' => my %config;

    # Extract the value of a key/value pair from a specified section...
    $config_value = $config{Section_label}{key};

    # Change (or create) the value of a key/value pair...
    $config{Other_section_label}{other_key} = $new_val;

    # Update the config file from which this hash was loaded...
    write_config %config;

    # Write the config information to another file as well...
    write_config %config, $other_file_name;

  
=head1 DESCRIPTION

This module implements yet another damn configuration-file system.

The configuration language is deliberately simple and limited, and the
module works hard to preserve as much information (section order,
comments, etc.) as possible when a configuration file is updated.

The whole point of Config::Std is to encourage use of one standard layout
and syntax in config files. Damian says "I could have gotten away with it, I would have
only allowed one separator. But it proved impossible to choose between C<:> and C<=>
(half the people I asked wanted one, half wanted the other)." 
Providing round-trip file re-write is the spoonful of sugar to help the medicine go down.
The supported syntax is within the general INI file family 

See Chapter 19 of "Perl Best Practices" (O'Reilly, 2005) 
for more detail on the
rationale for this approach. 

=head2 Configuration language

The configuration language is a slight extension of the Windows INI format.

=head3 Comments

A comment starts with a C<#> character (Perl-style) or a C<;> character
(INI-style), and runs to the end of the same line:

    # This is a comment

    ; Ywis, eke hight thilke

Comments can be placed almost anywhere in a configuration file, except inside
a section label, or in the key or value of a configuration variable:

    # Valid comment
    [ # Not a comment, just a weird section label ]

    ; Valid comment
    key: value  ; Not a comment, just part of the value

NOTE BENE -- that last is a BAD EXAMPLE of what is NOT supported. 
This module supports full-line comments only, not on same line with semantic content.

=head3 Sections

A configuration file consists of one or more I<sections>, each of which is
introduced by a label in square brackets:

    [SECTION1]        # Almost anything is a valid section label

    [SECTION 2]       # Internal whitespace is allowed (except newlines)

    [%^$%^&!!!]       # The label doesn't have to be alphanumeric

    [ETC. ETC. AS MANY AS YOU WANT]

The only restriction on section labels is that they must be by
themselves on a single line (except for any surrounding whitespace or
trailing comments), and they cannot contain the character C<]>.

Every line after a given section label until the next section label (or
the end of the config file) belongs to the given section label. If no
section label is currently in effect, the current section has an empty
label. In other words, there is an implicit:

    []                # Label is the empty string

at the start of each config file.

=head3 Configuration variables

Each non-empty line within a section must consist of the specification of a
I<configuration variable>. Each such variable consists of a key and a string
value. For example:

    name: George
     age: 47

    his weight! : 185

The key consists of every character (including internal whitespace) from
the start of the line until the key/value separator. So, the previous
example declares three keys: C<'name'>, C<'age'>, and C<'his weight!'>.

Note that whitespace before and after the key is removed. This makes it easier
to format keys cleanly:

           name : George
            age : 47
    his weight! : 185

The key/value separator can be either a colon (as above) or an equals sign,
like so:

           name= George
            age=  47
    his weight! = 185

Both types of separators can be used in the same file, but neither can
be used as part of a key. Newlines are not allowed in keys either.

When writing out a config file, Config::Std tries to preserve whichever
separator was used in the original data (if that data was read
in). New data 
(created by code not parsed by C<read_config>)
is written back with a colon as its default separator,
unless you specify the only other separator value C<'='> when the module is loaded:

    use Config::Std { def_sep => '=' };

Note that this does not change read-in parsing, 
does not change punctuation for values that were parsed, 
and will not allow values other than C<'='> or C<':'>.

Everything from the first non-whitespace character after the separator,
up to the end of the line, is treated as the value for the config variable.
So all of the above examples define the same three values: C<'George'>,
C<'47'>, and C<'185'>.

In other words, any whitespace immediately surrounding the separator
character is part of the separator, not part of the key or value.

Note that you can't put a comment on the same line as a configuration
variable. The C<# etc.> is simply considered part of the value:

    [Delimiters]

    block delims:    { }
    string delims:   " "
    comment delims:  # \n

You can comment a config var on the preceding or succeeding line:

    [Delimiters]

    # Use braces to delimit blocks...
    block delims:    { }

    # Use double quotes to delimit strings

    string delims:   " "

    # Use octothorpe/newline to delimit comments
    comment delims:  # \n
    

=head3 Multi-line configuration values

A single value can be continued over two or more lines. If the line
immediately after a configuration variable starts with the separator
character used in the variable's definition, then the value of the
variable continues on that line. For example:

    address: 742 Evergreen Terrace
           : Springfield
           : USA

The newlines then form part of the value, so the value specified in the
previous example is: C<S<"742 Evergreen Terrace\nSpringfield\nUSA">>

Note that the second and subsequent lines of a continued value are considered
to start where the whitespace after the I<original> separator finished, not
where the whitespace after their own separator finishes. For example, if the
previous example had been:

    address: 742 Evergreen Terrace
           :   Springfield
           :     USA

then the value would be:

    "742 Evergreen Terrace\n  Springfield\n    USA"

If a continuation line has less leading whitespace that the first line:

    address:   742 Evergreen Terrace
           :  Springfield
           : USA

it's treated as having no leading whitespace:

    "742 Evergreen Terrace\nSpringfield\nUSA"


=head3 Multi-part configuration values

If the particular key appears more than once in the same section, it is
considered to be part of the same configuration variable. The value of
that configuration value is then a list, containing all the individual
values for each instance of the key. For example, given the definition:

    cast: Homer
    cast: Marge
    cast: Lisa
    cast: Bart
    cast: Maggie

the corresponding value of the C<'cast'> configuration variable is:
C<S<['Homer', 'Marge', 'Lisa', 'Bart', 'Maggie']>>

Individual values in a multi-part list can also be multi-line (see
above). For example, given:

    extras: Moe
          : (the bartender)

    extras: Smithers
          : (the dogsbody)

the value for the C<'extras'> config variable is:
C<S<["Moe\n(the bartender)", "Smithers\n(the dogsbody)"]>>


=head2 Internal representation

Each section label in a configuration file becomes a top-level hash key whe
the configuration file is read in. The corresponding value is a nested hash
reference.

Each configuration variable's key becomes a key in that nested hash reference.
Each configuration variable's value becomes the corresponding value in that nested hash reference.

Single-line and multi-line values become strings. Multi-part values become
references to arrays of strings.

For example, the following configuration file:

    # A simple key (just an identifier)...
    simple : simple value

    # A more complex key (with whitespace)...
    more complex key : more complex value

    # A new section...
    [MULTI-WHATEVERS]

    # A value spread over several lines...
    multi-line : this is line 1
               : this is line 2
               : this is line 3

    # Several values for the same key...
    multi-value: this is value 1
    multi-value: this is value 2
    multi-value: this is value 3

would be read into a hash whose internal structure looked like this:

    {
       # Default section...
       '' => {
          'simple'           => 'simple value',
          'more complex key' => 'more complex value',
       },

       # Named section...
       'MULTI-WHATEVERS' => {
            'multi-line'  => "this is line 1\nthis is line 2\nthis is line 3",

            'multi-value' => [ 'this is value 1',
                               'this is value 2',
                               'this is value 3'
                             ],
        }
    }


=head1 INTERFACE 

The following subroutines are exported automatically whenever the module is
loaded...

=over 

=item C<< read_config($filename => %config_hash) >>

=item C<< read_config($filename => $config_hash_ref) >>

=item C<< read_config($string_ref => %config_hash_or_ref) >>

The C<read_config()> subroutine takes two arguments: the filename of a
configuration file, and a variable into which the contents of that
configuration file are to be loaded.

If the variable is a hash, then the configuration sections and their
key/value pairs are loaded into nested subhashes of the hash.

If the variable is a scalar with an undefined value, a reference to an
anonymous hash is first assigned to that scalar, and that hash is then
filled as described above.

The subroutine returns true on success, and throws an exception on failure.

If you pass a reference to the string as the first argument to
C<read_config()> it uses that string as the source of the config info.
For example:

	use Config::Std;

	# here we load the config text to a scalar
	my $cfg = q{
	[Section 1]
	attr1 = at
	attr2 = bat

	[Section 2]
	attr3 = cat
	};

	# here we parse the config from that scalar by passing a reference to it.
	read_config( \$cfg, my %config );

	use Data::Dumper 'Dumper';
	warn Dumper [ \%config ];


=item C<< write_config(%config_hash => $filename) >>

=item C<< write_config($config_hash_ref => $filename) >>

=item C<write_config(%config_hash)>

=item C<write_config($config_hash_ref)>

The C<write_config()> subroutine takes two arguments: the hash or hash
reference containing the configuration data to be written out to disk,
and an optional filename specifying which file it is to be written to.

The data hash must conform to the two-level structure described earlier:
with top-level keys naming sections and their values being references to
second-level hashes that store the keys and values of the configuartion
variables. If the structure of the hash differs from this, an exception is
thrown.

If a filename is also specified, the subroutine opens that file
and writes to it. It no filename is specified, the subroutine uses the
name of the file from which the hash was originally loaded using
C<read_config()>. It no filename is specified and the hash I<wasn't>
originally loaded using C<read_config()>, an exception is thrown.

The subroutine returns true on success and throws and exception on failure.

=back

If necessary (typically to avoid conflicts with other modules), you can
have the module export its two subroutines with different names by
loading it with the appropriate options:

    use Config::Std { read_config => 'get_ini', write_config => 'update_ini' };

    # and later...

    get_ini($filename => %config_hash);

    # and later still...

    update_ini(%config_hash);

You can also control how much spacing the module puts between single-
line values when they are first written to a file, by using the
C<def_gap> option:

    # No empty line between single-line config values...
    use Config::Std { def_gap => 0 }; 

    # An empty line between all single-line config values...
    use Config::Std { def_gap => 1 }; 

Regardless of the value passed for C<def_gap>, new multi-line values are
always written with an empty line above and below them. Likewise, values
that were previously read in from a file are always written back with
whatever spacing they originally had.

=head1 DIAGNOSTICS

=over 

=item Can't open config file '%s' (%s)

You tried to read in a configuration file, but the file you specified
didn't exist. Perhaps the filepath you specified was wrong. Or maybe 
your application didn't have permission to access the file you specified.

=item Can't read from locked config file '$filename'

You tried to read in a configuration file, but the file you specified
was being written by someone else (they had a file lock active on it).
Either try again later, or work out who else is using the file.

=item Scalar second argument to 'read_config' must be empty

You passed a scalar variable as the destination into C<read_config()>
was supposed to load a configuration file, but that variable already had
a defined value, so C<read_config()> couldn't autovivify a new hash for
you. Did you mean to pass the subroutine a hash instead of a scalar?

=item Can't save %s value for key '%s' (only scalars or array refs)

You called C<write_config> and passed it a hash containing a
configuration variable whose value wasn't a single string, or a list of
strings. The configuration file format supported by this module only
supports those two data types as values. If you really need to store
other kinds of data in a configuration file, you should consider using
C<Data::Dumper> or C<YAML> instead.

=item Missing filename in call to write_config()

You tried to calll C<write_config()> with only a configuration hash, but that
hash wasn't originally loaded using C<read_config()>, so C<write_config()> has
no idea where to write it to. Either make sure the hash you're trying to save
was originally loaded using C<read_config()>, or else provide an explicit
filename as the second argument to C<write_config()>.

=item Can't open config file '%s' for writing (%s)

You tried to update or create a configuration file, but the file you
specified could not be opened for writing (for the reason given in the
parentheses). This is often caused by incorrect filepaths or lack of
write permissions on a directory.

=item Can't write to locked config file '%s'

You tried to update or create a configuration file, but the file you
specified was being written at the time by someone else (they had a file
lock active on it). Either try again later, or work out who else is
using the file.

=back


=head1 CONFIGURATION AND ENVIRONMENT

Config::Std requires no configuration files or environment variables.
(To do so would be disturbingly recursive.)


=head1 DEPENDENCIES

This module requires the Class::Std module (available from the CPAN)


=head1 INCOMPATIBILITIES

Those variants of INI file dialect supporting partial-line comment are incompatible. 
(This is the price of keeping comments when re-writing.)


=head1 BUGS AND LIMITATIONS

=over

=item Loading on demand

If you attempt to load C<read_config()> and C<write_config()> 
at runtime with C<require>, you can not rely upon the prototype
to convert a regular hash to a reference. To work around this, 
you must explicitly pass a reference to the config hash.

    require Config::Std;
    Config::Std->import;

    my %config;
    read_config($file, \%config);
    write_config(\%config, $file);

=item Windows line endings on Unix/Linux (RT#21547/23550)

If the config file being read contains carriage returns and line feeds
at the end of each line rather than just line feeds (i.e. the standard
Windows file format, when read on a machine expecting POSIX file format),
Config::Std emits an error with embedded newline.

Workaround is match file line-endings to locale.

This will be fixed in 1.000.


=item leading comment vanishes (RT#24597,)

A comment before the first section is not always retained on write-back, if the '' default section is empty.

=item 00write.t test 5 fails on perl5.8.1 (RT#17425)

Due to an incompatible change in v5.8.1 partially reversed in v5.8.2, hash key randomisation can cause test to fail in that one version of Perl. Workaround is export environment variable PERL_HASH_SEED=0.

=back

Please report any bugs or feature requests to
C<bug-config-std@rt.cpan.org>, or through the web interface at
L<http://rt.cpan.org>.


=head1 AUTHOR

Damian Conway  C<< <DCONWAY@cpan.org> >>
Maintainers 
Bill Ricker    C<< <BRICKER@cpan.org> >>
Tom Metro      C<< <tmetro@cpan.org> >>

=head1 LICENCE AND COPYRIGHT

Copyright (c) 2005, Damian Conway C<< <DCONWAY@cpan.org> >>. 
Copyright (c) 2011, D.Conway, W.Ricker C<< <BRICKER@cpan.org> >> All rights reserved.

This module is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.


=head1 DISCLAIMER OF WARRANTY

BECAUSE THIS SOFTWARE IS LICENSED FREE OF CHARGE, THERE IS NO WARRANTY
FOR THE SOFTWARE, TO THE EXTENT PERMITTED BY APPLICABLE LAW. EXCEPT WHEN
OTHERWISE STATED IN WRITING THE COPYRIGHT HOLDERS AND/OR OTHER PARTIES
PROVIDE THE SOFTWARE "AS IS" WITHOUT WARRANTY OF ANY KIND, EITHER
EXPRESSED OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE
ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE OF THE SOFTWARE IS WITH
YOU. SHOULD THE SOFTWARE PROVE DEFECTIVE, YOU ASSUME THE COST OF ALL
NECESSARY SERVICING, REPAIR, OR CORRECTION.

IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW OR AGREED TO IN WRITING
WILL ANY COPYRIGHT HOLDER, OR ANY OTHER PARTY WHO MAY MODIFY AND/OR
REDISTRIBUTE THE SOFTWARE AS PERMITTED BY THE ABOVE LICENCE, BE
LIABLE TO YOU FOR DAMAGES, INCLUDING ANY GENERAL, SPECIAL, INCIDENTAL,
OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OR INABILITY TO USE
THE SOFTWARE (INCLUDING BUT NOT LIMITED TO LOSS OF DATA OR DATA BEING
RENDERED INACCURATE OR LOSSES SUSTAINED BY YOU OR THIRD PARTIES OR A
FAILURE OF THE SOFTWARE TO OPERATE WITH ANY OTHER SOFTWARE), EVEN IF
SUCH HOLDER OR OTHER PARTY HAS BEEN ADVISED OF THE POSSIBILITY OF
SUCH DAMAGES.
