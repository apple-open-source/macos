#line 1
package attributes;

our $VERSION = 0.08;

@EXPORT_OK = qw(get reftype);
@EXPORT = ();
%EXPORT_TAGS = (ALL => [@EXPORT, @EXPORT_OK]);

use strict;

sub croak {
    require Carp;
    goto &Carp::croak;
}

sub carp {
    require Carp;
    goto &Carp::carp;
}

## forward declaration(s) rather than wrapping the bootstrap call in BEGIN{}
#sub reftype ($) ;
#sub _fetch_attrs ($) ;
#sub _guess_stash ($) ;
#sub _modify_attrs ;
#
# The extra trips through newATTRSUB in the interpreter wipe out any savings
# from avoiding the BEGIN block.  Just do the bootstrap now.
BEGIN { bootstrap attributes }

sub import {
    @_ > 2 && ref $_[2] or do {
	require Exporter;
	goto &Exporter::import;
    };
    my (undef,$home_stash,$svref,@attrs) = @_;

    my $svtype = uc reftype($svref);
    my $pkgmeth;
    $pkgmeth = UNIVERSAL::can($home_stash, "MODIFY_${svtype}_ATTRIBUTES")
	if defined $home_stash && $home_stash ne '';
    my @badattrs;
    if ($pkgmeth) {
	my @pkgattrs = _modify_attrs($svref, @attrs);
	@badattrs = $pkgmeth->($home_stash, $svref, @pkgattrs);
	if (!@badattrs && @pkgattrs) {
            require warnings;
	    return unless warnings::enabled('reserved');
	    @pkgattrs = grep { m/\A[[:lower:]]+(?:\z|\()/ } @pkgattrs;
	    if (@pkgattrs) {
		for my $attr (@pkgattrs) {
		    $attr =~ s/\(.+\z//s;
		}
		my $s = ((@pkgattrs == 1) ? '' : 's');
		carp "$svtype package attribute$s " .
		    "may clash with future reserved word$s: " .
		    join(' : ' , @pkgattrs);
	    }
	}
    }
    else {
	@badattrs = _modify_attrs($svref, @attrs);
    }
    if (@badattrs) {
	croak "Invalid $svtype attribute" .
	    (( @badattrs == 1 ) ? '' : 's') .
	    ": " .
	    join(' : ', @badattrs);
    }
}

sub get ($) {
    @_ == 1  && ref $_[0] or
	croak 'Usage: '.__PACKAGE__.'::get $ref';
    my $svref = shift;
    my $svtype = uc reftype $svref;
    my $stash = _guess_stash $svref;
    $stash = caller unless defined $stash;
    my $pkgmeth;
    $pkgmeth = UNIVERSAL::can($stash, "FETCH_${svtype}_ATTRIBUTES")
	if defined $stash && $stash ne '';
    return $pkgmeth ?
		(_fetch_attrs($svref), $pkgmeth->($stash, $svref)) :
		(_fetch_attrs($svref))
	;
}

sub require_version { goto &UNIVERSAL::VERSION }

1;
__END__
#The POD goes here

#line 417

