#line 1
package IO::Scalar;


#line 147

use Carp;
use strict;
use vars qw($VERSION @ISA);
use IO::Handle;

use 5.005;

### Stringification, courtesy of B. K. Oxley (binkley):  :-)
use overload '""'   => sub { ${*{$_[0]}->{SR}} };
use overload 'bool' => sub { 1 };      ### have to do this, so object is true!

### The package version, both in 1.23 style *and* usable by MakeMaker:
$VERSION = "2.110";

### Inheritance:
@ISA = qw(IO::Handle);

### This stuff should be got rid of ASAP.
require IO::WrapTie and push @ISA, 'IO::WrapTie::Slave' if ($] >= 5.004);

#==============================

#line 175

#------------------------------

#line 185

sub new {
    my $proto = shift;
    my $class = ref($proto) || $proto;
    my $self = bless \do { local *FH }, $class;
    tie *$self, $class, $self;
    $self->open(@_);   ### open on anonymous by default
    $self;
}
sub DESTROY {
    shift->close;
}

#------------------------------

#line 210

sub open {
    my ($self, $sref) = @_;

    ### Sanity:
    defined($sref) or do {my $s = ''; $sref = \$s};
    (ref($sref) eq "SCALAR") or croak "open() needs a ref to a scalar";

    ### Setup:
    *$self->{Pos} = 0;          ### seek position
    *$self->{SR}  = $sref;      ### scalar reference
    $self;
}

#------------------------------

#line 232

sub opened {
    *{shift()}->{SR};
}

#------------------------------

#line 246

sub close {
    my $self = shift;
    %{*$self} = ();
    1;
}

#line 256



#==============================

#line 266


#------------------------------

#line 276

sub flush { "0 but true" }

#------------------------------

#line 287

sub getc {
    my $self = shift;

    ### Return undef right away if at EOF; else, move pos forward:
    return undef if $self->eof;
    substr(${*$self->{SR}}, *$self->{Pos}++, 1);
}

#------------------------------

#line 306

sub getline {
    my $self = shift;

    ### Return undef right away if at EOF:
    return undef if $self->eof;

    ### Get next line:
    my $sr = *$self->{SR};
    my $i  = *$self->{Pos};	        ### Start matching at this point.

    ### Minimal impact implementation!
    ### We do the fast fast thing (no regexps) if using the
    ### classic input record separator.

    ### Case 1: $/ is undef: slurp all...
    if    (!defined($/)) {
	*$self->{Pos} = length $$sr;
        return substr($$sr, $i);
    }

    ### Case 2: $/ is "\n": zoom zoom zoom...
    elsif ($/ eq "\012") {

        ### Seek ahead for "\n"... yes, this really is faster than regexps.
        my $len = length($$sr);
        for (; $i < $len; ++$i) {
           last if ord (substr ($$sr, $i, 1)) == 10;
        }

        ### Extract the line:
        my $line;
        if ($i < $len) {                ### We found a "\n":
            $line = substr ($$sr, *$self->{Pos}, $i - *$self->{Pos} + 1);
            *$self->{Pos} = $i+1;            ### Remember where we finished up.
        }
        else {                          ### No "\n"; slurp the remainder:
            $line = substr ($$sr, *$self->{Pos}, $i - *$self->{Pos});
            *$self->{Pos} = $len;
        }
        return $line;
    }

    ### Case 3: $/ is ref to int. Do fixed-size records.
    ###        (Thanks to Dominique Quatravaux.)
    elsif (ref($/)) {
        my $len = length($$sr);
		my $i = ${$/} + 0;
		my $line = substr ($$sr, *$self->{Pos}, $i);
		*$self->{Pos} += $i;
        *$self->{Pos} = $len if (*$self->{Pos} > $len);
		return $line;
    }

    ### Case 4: $/ is either "" (paragraphs) or something weird...
    ###         This is Graham's general-purpose stuff, which might be
    ###         a tad slower than Case 2 for typical data, because
    ###         of the regexps.
    else {
        pos($$sr) = $i;

	### If in paragraph mode, skip leading lines (and update i!):
        length($/) or
	    (($$sr =~ m/\G\n*/g) and ($i = pos($$sr)));

        ### If we see the separator in the buffer ahead...
        if (length($/)
	    ?  $$sr =~ m,\Q$/\E,g          ###   (ordinary sep) TBD: precomp!
            :  $$sr =~ m,\n\n,g            ###   (a paragraph)
            ) {
            *$self->{Pos} = pos $$sr;
            return substr($$sr, $i, *$self->{Pos}-$i);
        }
        ### Else if no separator remains, just slurp the rest:
        else {
            *$self->{Pos} = length $$sr;
            return substr($$sr, $i);
        }
    }
}

#------------------------------

#line 396

sub getlines {
    my $self = shift;
    wantarray or croak("can't call getlines in scalar context!");
    my ($line, @lines);
    push @lines, $line while (defined($line = $self->getline));
    @lines;
}

#------------------------------

#line 417

sub print {
    my $self = shift;
    *$self->{Pos} = length(${*$self->{SR}} .= join('', @_) . (defined($\) ? $\ : ""));
    1;
}
sub _unsafe_print {
    my $self = shift;
    my $append = join('', @_) . $\;
    ${*$self->{SR}} .= $append;
    *$self->{Pos}   += length($append);
    1;
}
sub _old_print {
    my $self = shift;
    ${*$self->{SR}} .= join('', @_) . $\;
    *$self->{Pos} = length(${*$self->{SR}});
    1;
}


#------------------------------

#line 447

sub read {
    my $self = $_[0];
    my $n    = $_[2];
    my $off  = $_[3] || 0;

    my $read = substr(${*$self->{SR}}, *$self->{Pos}, $n);
    $n = length($read);
    *$self->{Pos} += $n;
    ($off ? substr($_[1], $off) : $_[1]) = $read;
    return $n;
}

#------------------------------

#line 468

sub write {
    my $self = $_[0];
    my $n    = $_[2];
    my $off  = $_[3] || 0;

    my $data = substr($_[1], $off, $n);
    $n = length($data);
    $self->print($data);
    return $n;
}

#------------------------------

#line 489

sub sysread {
  my $self = shift;
  $self->read(@_);
}

#------------------------------

#line 503

sub syswrite {
  my $self = shift;
  $self->write(@_);
}

#line 512


#==============================

#line 521


#------------------------------

#line 531

sub autoflush {}

#------------------------------

#line 542

sub binmode {}

#------------------------------

#line 552

sub clearerr { 1 }

#------------------------------

#line 562

sub eof {
    my $self = shift;
    (*$self->{Pos} >= length(${*$self->{SR}}));
}

#------------------------------

#line 575

sub seek {
    my ($self, $pos, $whence) = @_;
    my $eofpos = length(${*$self->{SR}});

    ### Seek:
    if    ($whence == 0) { *$self->{Pos} = $pos }             ### SEEK_SET
    elsif ($whence == 1) { *$self->{Pos} += $pos }            ### SEEK_CUR
    elsif ($whence == 2) { *$self->{Pos} = $eofpos + $pos}    ### SEEK_END
    else                 { croak "bad seek whence ($whence)" }

    ### Fixup:
    if (*$self->{Pos} < 0)       { *$self->{Pos} = 0 }
    if (*$self->{Pos} > $eofpos) { *$self->{Pos} = $eofpos }
    return 1;
}

#------------------------------

#line 599

sub sysseek {
    my $self = shift;
    $self->seek (@_);
}

#------------------------------

#line 613

sub tell { *{shift()}->{Pos} }

#------------------------------
#
# use_RS [YESNO]
#
# I<Instance method.>
# Obey the curent setting of $/, like IO::Handle does?
# Default is false in 1.x, but cold-welded true in 2.x and later.
#
sub use_RS {
    my ($self, $yesno) = @_;
    carp "use_RS is deprecated and ignored; \$/ is always consulted\n";
 }

#------------------------------

#line 637

sub setpos { shift->seek($_[0],0) }

#------------------------------

#line 648

*getpos = \&tell;


#------------------------------

#line 660

sub sref { *{shift()}->{SR} }


#------------------------------
# Tied handle methods...
#------------------------------

# Conventional tiehandle interface:
sub TIEHANDLE {
    ((defined($_[1]) && UNIVERSAL::isa($_[1], "IO::Scalar"))
     ? $_[1]
     : shift->new(@_));
}
sub GETC      { shift->getc(@_) }
sub PRINT     { shift->print(@_) }
sub PRINTF    { shift->print(sprintf(shift, @_)) }
sub READ      { shift->read(@_) }
sub READLINE  { wantarray ? shift->getlines(@_) : shift->getline(@_) }
sub WRITE     { shift->write(@_); }
sub CLOSE     { shift->close(@_); }
sub SEEK      { shift->seek(@_); }
sub TELL      { shift->tell(@_); }
sub EOF       { shift->eof(@_); }

#------------------------------------------------------------

1;

__END__



#line 696


#line 777

