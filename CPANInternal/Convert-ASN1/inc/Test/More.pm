#line 1
package Test::More;

use 5.004;

use strict;


# Can't use Carp because it might cause use_ok() to accidentally succeed
# even though the module being used forgot to use Carp.  Yes, this
# actually happened.
sub _carp {
    my($file, $line) = (caller(1))[1,2];
    warn @_, " at $file line $line\n";
}



use vars qw($VERSION @ISA @EXPORT %EXPORT_TAGS $TODO);
$VERSION = '0.62';
$VERSION = eval $VERSION;    # make the alpha version come out as a number

use Test::Builder::Module;
@ISA    = qw(Test::Builder::Module);
@EXPORT = qw(ok use_ok require_ok
             is isnt like unlike is_deeply
             cmp_ok
             skip todo todo_skip
             pass fail
             eq_array eq_hash eq_set
             $TODO
             plan
             can_ok  isa_ok
             diag
	     BAIL_OUT
            );


#line 157

sub plan {
    my $tb = Test::More->builder;

    $tb->plan(@_);
}


# This implements "use Test::More 'no_diag'" but the behavior is
# deprecated.
sub import_extra {
    my $class = shift;
    my $list  = shift;

    my @other = ();
    my $idx = 0;
    while( $idx <= $#{$list} ) {
        my $item = $list->[$idx];

        if( defined $item and $item eq 'no_diag' ) {
            $class->builder->no_diag(1);
        }
        else {
            push @other, $item;
        }

        $idx++;
    }

    @$list = @other;
}


#line 257

sub ok ($;$) {
    my($test, $name) = @_;
    my $tb = Test::More->builder;

    $tb->ok($test, $name);
}

#line 324

sub is ($$;$) {
    my $tb = Test::More->builder;

    $tb->is_eq(@_);
}

sub isnt ($$;$) {
    my $tb = Test::More->builder;

    $tb->isnt_eq(@_);
}

*isn't = \&isnt;


#line 369

sub like ($$;$) {
    my $tb = Test::More->builder;

    $tb->like(@_);
}


#line 385

sub unlike ($$;$) {
    my $tb = Test::More->builder;

    $tb->unlike(@_);
}


#line 425

sub cmp_ok($$$;$) {
    my $tb = Test::More->builder;

    $tb->cmp_ok(@_);
}


#line 461

sub can_ok ($@) {
    my($proto, @methods) = @_;
    my $class = ref $proto || $proto;
    my $tb = Test::More->builder;

    unless( @methods ) {
        my $ok = $tb->ok( 0, "$class->can(...)" );
        $tb->diag('    can_ok() called with no methods');
        return $ok;
    }

    my @nok = ();
    foreach my $method (@methods) {
        local($!, $@);  # don't interfere with caller's $@
                        # eval sometimes resets $!
        eval { $proto->can($method) } || push @nok, $method;
    }

    my $name;
    $name = @methods == 1 ? "$class->can('$methods[0]')" 
                          : "$class->can(...)";
    
    my $ok = $tb->ok( !@nok, $name );

    $tb->diag(map "    $class->can('$_') failed\n", @nok);

    return $ok;
}

#line 519

sub isa_ok ($$;$) {
    my($object, $class, $obj_name) = @_;
    my $tb = Test::More->builder;

    my $diag;
    $obj_name = 'The object' unless defined $obj_name;
    my $name = "$obj_name isa $class";
    if( !defined $object ) {
        $diag = "$obj_name isn't defined";
    }
    elsif( !ref $object ) {
        $diag = "$obj_name isn't a reference";
    }
    else {
        # We can't use UNIVERSAL::isa because we want to honor isa() overrides
        local($@, $!);  # eval sometimes resets $!
        my $rslt = eval { $object->isa($class) };
        if( $@ ) {
            if( $@ =~ /^Can't call method "isa" on unblessed reference/ ) {
                if( !UNIVERSAL::isa($object, $class) ) {
                    my $ref = ref $object;
                    $diag = "$obj_name isn't a '$class' it's a '$ref'";
                }
            } else {
                die <<WHOA;
WHOA! I tried to call ->isa on your object and got some weird error.
This should never happen.  Please contact the author immediately.
Here's the error.
$@
WHOA
            }
        }
        elsif( !$rslt ) {
            my $ref = ref $object;
            $diag = "$obj_name isn't a '$class' it's a '$ref'";
        }
    }
            
      

    my $ok;
    if( $diag ) {
        $ok = $tb->ok( 0, $name );
        $tb->diag("    $diag\n");
    }
    else {
        $ok = $tb->ok( 1, $name );
    }

    return $ok;
}


#line 589

sub pass (;$) {
    my $tb = Test::More->builder;
    $tb->ok(1, @_);
}

sub fail (;$) {
    my $tb = Test::More->builder;
    $tb->ok(0, @_);
}

#line 650

sub use_ok ($;@) {
    my($module, @imports) = @_;
    @imports = () unless @imports;
    my $tb = Test::More->builder;

    my($pack,$filename,$line) = caller;

    local($@,$!);   # eval sometimes interferes with $!

    if( @imports == 1 and $imports[0] =~ /^\d+(?:\.\d+)?$/ ) {
        # probably a version check.  Perl needs to see the bare number
        # for it to work with non-Exporter based modules.
        eval <<USE;
package $pack;
use $module $imports[0];
USE
    }
    else {
        eval <<USE;
package $pack;
use $module \@imports;
USE
    }

    my $ok = $tb->ok( !$@, "use $module;" );

    unless( $ok ) {
        chomp $@;
        $@ =~ s{^BEGIN failed--compilation aborted at .*$}
                {BEGIN failed--compilation aborted at $filename line $line.}m;
        $tb->diag(<<DIAGNOSTIC);
    Tried to use '$module'.
    Error:  $@
DIAGNOSTIC

    }

    return $ok;
}

#line 699

sub require_ok ($) {
    my($module) = shift;
    my $tb = Test::More->builder;

    my $pack = caller;

    # Try to deterine if we've been given a module name or file.
    # Module names must be barewords, files not.
    $module = qq['$module'] unless _is_module_name($module);

    local($!, $@); # eval sometimes interferes with $!
    eval <<REQUIRE;
package $pack;
require $module;
REQUIRE

    my $ok = $tb->ok( !$@, "require $module;" );

    unless( $ok ) {
        chomp $@;
        $tb->diag(<<DIAGNOSTIC);
    Tried to require '$module'.
    Error:  $@
DIAGNOSTIC

    }

    return $ok;
}


sub _is_module_name {
    my $module = shift;

    # Module names start with a letter.
    # End with an alphanumeric.
    # The rest is an alphanumeric or ::
    $module =~ s/\b::\b//g;
    $module =~ /^[a-zA-Z]\w*$/;
}

#line 775

use vars qw(@Data_Stack %Refs_Seen);
my $DNE = bless [], 'Does::Not::Exist';
sub is_deeply {
    my $tb = Test::More->builder;

    unless( @_ == 2 or @_ == 3 ) {
        my $msg = <<WARNING;
is_deeply() takes two or three args, you gave %d.
This usually means you passed an array or hash instead 
of a reference to it
WARNING
        chop $msg;   # clip off newline so carp() will put in line/file

        _carp sprintf $msg, scalar @_;

	return $tb->ok(0);
    }

    my($this, $that, $name) = @_;

    $tb->_unoverload_str(\$that, \$this);

    my $ok;
    if( !ref $this and !ref $that ) {  		# neither is a reference
        $ok = $tb->is_eq($this, $that, $name);
    }
    elsif( !ref $this xor !ref $that ) {  	# one's a reference, one isn't
        $ok = $tb->ok(0, $name);
	$tb->diag( _format_stack({ vals => [ $this, $that ] }) );
    }
    else {			       		# both references
        local @Data_Stack = ();
        if( _deep_check($this, $that) ) {
            $ok = $tb->ok(1, $name);
        }
        else {
            $ok = $tb->ok(0, $name);
            $tb->diag(_format_stack(@Data_Stack));
        }
    }

    return $ok;
}

sub _format_stack {
    my(@Stack) = @_;

    my $var = '$FOO';
    my $did_arrow = 0;
    foreach my $entry (@Stack) {
        my $type = $entry->{type} || '';
        my $idx  = $entry->{'idx'};
        if( $type eq 'HASH' ) {
            $var .= "->" unless $did_arrow++;
            $var .= "{$idx}";
        }
        elsif( $type eq 'ARRAY' ) {
            $var .= "->" unless $did_arrow++;
            $var .= "[$idx]";
        }
        elsif( $type eq 'REF' ) {
            $var = "\${$var}";
        }
    }

    my @vals = @{$Stack[-1]{vals}}[0,1];
    my @vars = ();
    ($vars[0] = $var) =~ s/\$FOO/     \$got/;
    ($vars[1] = $var) =~ s/\$FOO/\$expected/;

    my $out = "Structures begin differing at:\n";
    foreach my $idx (0..$#vals) {
        my $val = $vals[$idx];
        $vals[$idx] = !defined $val ? 'undef'          :
                      $val eq $DNE  ? "Does not exist" :
	              ref $val      ? "$val"           :
                                      "'$val'";
    }

    $out .= "$vars[0] = $vals[0]\n";
    $out .= "$vars[1] = $vals[1]\n";

    $out =~ s/^/    /msg;
    return $out;
}


sub _type {
    my $thing = shift;

    return '' if !ref $thing;

    for my $type (qw(ARRAY HASH REF SCALAR GLOB CODE Regexp)) {
        return $type if UNIVERSAL::isa($thing, $type);
    }

    return '';
}

#line 915

sub diag {
    my $tb = Test::More->builder;

    $tb->diag(@_);
}


#line 984

#'#
sub skip {
    my($why, $how_many) = @_;
    my $tb = Test::More->builder;

    unless( defined $how_many ) {
        # $how_many can only be avoided when no_plan is in use.
        _carp "skip() needs to know \$how_many tests are in the block"
          unless $tb->has_plan eq 'no_plan';
        $how_many = 1;
    }

    for( 1..$how_many ) {
        $tb->skip($why);
    }

    local $^W = 0;
    last SKIP;
}


#line 1066

sub todo_skip {
    my($why, $how_many) = @_;
    my $tb = Test::More->builder;

    unless( defined $how_many ) {
        # $how_many can only be avoided when no_plan is in use.
        _carp "todo_skip() needs to know \$how_many tests are in the block"
          unless $tb->has_plan eq 'no_plan';
        $how_many = 1;
    }

    for( 1..$how_many ) {
        $tb->todo_skip($why);
    }

    local $^W = 0;
    last TODO;
}

#line 1119

sub BAIL_OUT {
    my $reason = shift;
    my $tb = Test::More->builder;

    $tb->BAIL_OUT($reason);
}

#line 1158

#'#
sub eq_array {
    local @Data_Stack;
    _deep_check(@_);
}

sub _eq_array  {
    my($a1, $a2) = @_;

    if( grep !_type($_) eq 'ARRAY', $a1, $a2 ) {
        warn "eq_array passed a non-array ref";
        return 0;
    }

    return 1 if $a1 eq $a2;

    my $ok = 1;
    my $max = $#$a1 > $#$a2 ? $#$a1 : $#$a2;
    for (0..$max) {
        my $e1 = $_ > $#$a1 ? $DNE : $a1->[$_];
        my $e2 = $_ > $#$a2 ? $DNE : $a2->[$_];

        push @Data_Stack, { type => 'ARRAY', idx => $_, vals => [$e1, $e2] };
        $ok = _deep_check($e1,$e2);
        pop @Data_Stack if $ok;

        last unless $ok;
    }

    return $ok;
}

sub _deep_check {
    my($e1, $e2) = @_;
    my $tb = Test::More->builder;

    my $ok = 0;

    # Effectively turn %Refs_Seen into a stack.  This avoids picking up
    # the same referenced used twice (such as [\$a, \$a]) to be considered
    # circular.
    local %Refs_Seen = %Refs_Seen;

    {
        # Quiet uninitialized value warnings when comparing undefs.
        local $^W = 0; 

        $tb->_unoverload_str(\$e1, \$e2);

        # Either they're both references or both not.
        my $same_ref = !(!ref $e1 xor !ref $e2);
	my $not_ref  = (!ref $e1 and !ref $e2);

        if( defined $e1 xor defined $e2 ) {
            $ok = 0;
        }
        elsif ( $e1 == $DNE xor $e2 == $DNE ) {
            $ok = 0;
        }
        elsif ( $same_ref and ($e1 eq $e2) ) {
            $ok = 1;
        }
	elsif ( $not_ref ) {
	    push @Data_Stack, { type => '', vals => [$e1, $e2] };
	    $ok = 0;
	}
        else {
            if( $Refs_Seen{$e1} ) {
                return $Refs_Seen{$e1} eq $e2;
            }
            else {
                $Refs_Seen{$e1} = "$e2";
            }

            my $type = _type($e1);
            $type = 'DIFFERENT' unless _type($e2) eq $type;

            if( $type eq 'DIFFERENT' ) {
                push @Data_Stack, { type => $type, vals => [$e1, $e2] };
                $ok = 0;
            }
            elsif( $type eq 'ARRAY' ) {
                $ok = _eq_array($e1, $e2);
            }
            elsif( $type eq 'HASH' ) {
                $ok = _eq_hash($e1, $e2);
            }
            elsif( $type eq 'REF' ) {
                push @Data_Stack, { type => $type, vals => [$e1, $e2] };
                $ok = _deep_check($$e1, $$e2);
                pop @Data_Stack if $ok;
            }
            elsif( $type eq 'SCALAR' ) {
                push @Data_Stack, { type => 'REF', vals => [$e1, $e2] };
                $ok = _deep_check($$e1, $$e2);
                pop @Data_Stack if $ok;
            }
            elsif( $type ) {
                push @Data_Stack, { type => $type, vals => [$e1, $e2] };
                $ok = 0;
            }
	    else {
		_whoa(1, "No type in _deep_check");
	    }
        }
    }

    return $ok;
}


sub _whoa {
    my($check, $desc) = @_;
    if( $check ) {
        die <<WHOA;
WHOA!  $desc
This should never happen!  Please contact the author immediately!
WHOA
    }
}


#line 1289

sub eq_hash {
    local @Data_Stack;
    return _deep_check(@_);
}

sub _eq_hash {
    my($a1, $a2) = @_;

    if( grep !_type($_) eq 'HASH', $a1, $a2 ) {
        warn "eq_hash passed a non-hash ref";
        return 0;
    }

    return 1 if $a1 eq $a2;

    my $ok = 1;
    my $bigger = keys %$a1 > keys %$a2 ? $a1 : $a2;
    foreach my $k (keys %$bigger) {
        my $e1 = exists $a1->{$k} ? $a1->{$k} : $DNE;
        my $e2 = exists $a2->{$k} ? $a2->{$k} : $DNE;

        push @Data_Stack, { type => 'HASH', idx => $k, vals => [$e1, $e2] };
        $ok = _deep_check($e1, $e2);
        pop @Data_Stack if $ok;

        last unless $ok;
    }

    return $ok;
}

#line 1346

sub eq_set  {
    my($a1, $a2) = @_;
    return 0 unless @$a1 == @$a2;

    # There's faster ways to do this, but this is easiest.
    local $^W = 0;

    # It really doesn't matter how we sort them, as long as both arrays are 
    # sorted with the same algorithm.
    #
    # Ensure that references are not accidentally treated the same as a
    # string containing the reference.
    #
    # Have to inline the sort routine due to a threading/sort bug.
    # See [rt.cpan.org 6782]
    #
    # I don't know how references would be sorted so we just don't sort
    # them.  This means eq_set doesn't really work with refs.
    return eq_array(
           [grep(ref, @$a1), sort( grep(!ref, @$a1) )],
           [grep(ref, @$a2), sort( grep(!ref, @$a2) )],
    );
}

#line 1534

1;
