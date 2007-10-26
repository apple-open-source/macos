#line 1
package Test::Builder;

use 5.004;

# $^C was only introduced in 5.005-ish.  We do this to prevent
# use of uninitialized value warnings in older perls.
$^C ||= 0;

use strict;
use vars qw($VERSION);
$VERSION = '0.33';
$VERSION = eval $VERSION;    # make the alpha version come out as a number

# Make Test::Builder thread-safe for ithreads.
BEGIN {
    use Config;
    # Load threads::shared when threads are turned on
    if( $] >= 5.008 && $Config{useithreads} && $INC{'threads.pm'}) {
        require threads::shared;

        # Hack around YET ANOTHER threads::shared bug.  It would 
        # occassionally forget the contents of the variable when sharing it.
        # So we first copy the data, then share, then put our copy back.
        *share = sub (\[$@%]) {
            my $type = ref $_[0];
            my $data;

            if( $type eq 'HASH' ) {
                %$data = %{$_[0]};
            }
            elsif( $type eq 'ARRAY' ) {
                @$data = @{$_[0]};
            }
            elsif( $type eq 'SCALAR' ) {
                $$data = ${$_[0]};
            }
            else {
                die "Unknown type: ".$type;
            }

            $_[0] = &threads::shared::share($_[0]);

            if( $type eq 'HASH' ) {
                %{$_[0]} = %$data;
            }
            elsif( $type eq 'ARRAY' ) {
                @{$_[0]} = @$data;
            }
            elsif( $type eq 'SCALAR' ) {
                ${$_[0]} = $$data;
            }
            else {
                die "Unknown type: ".$type;
            }

            return $_[0];
        };
    }
    # 5.8.0's threads::shared is busted when threads are off.
    # We emulate it here.
    else {
        *share = sub { return $_[0] };
        *lock  = sub { 0 };
    }
}


#line 127

my $Test = Test::Builder->new;
sub new {
    my($class) = shift;
    $Test ||= $class->create;
    return $Test;
}


#line 149

sub create {
    my $class = shift;

    my $self = bless {}, $class;
    $self->reset;

    return $self;
}

#line 168

use vars qw($Level);

sub reset {
    my ($self) = @_;

    # We leave this a global because it has to be localized and localizing
    # hash keys is just asking for pain.  Also, it was documented.
    $Level = 1;

    $self->{Test_Died}    = 0;
    $self->{Have_Plan}    = 0;
    $self->{No_Plan}      = 0;
    $self->{Original_Pid} = $$;

    share($self->{Curr_Test});
    $self->{Curr_Test}    = 0;
    $self->{Test_Results} = &share([]);

    $self->{Exported_To}    = undef;
    $self->{Expected_Tests} = 0;

    $self->{Skip_All}   = 0;

    $self->{Use_Nums}   = 1;

    $self->{No_Header}  = 0;
    $self->{No_Ending}  = 0;

    $self->_dup_stdhandles unless $^C;

    return undef;
}

#line 220

sub exported_to {
    my($self, $pack) = @_;

    if( defined $pack ) {
        $self->{Exported_To} = $pack;
    }
    return $self->{Exported_To};
}

#line 242

sub plan {
    my($self, $cmd, $arg) = @_;

    return unless $cmd;

    if( $self->{Have_Plan} ) {
        die sprintf "You tried to plan twice!  Second plan at %s line %d\n",
          ($self->caller)[1,2];
    }

    if( $cmd eq 'no_plan' ) {
        $self->no_plan;
    }
    elsif( $cmd eq 'skip_all' ) {
        return $self->skip_all($arg);
    }
    elsif( $cmd eq 'tests' ) {
        if( $arg ) {
            return $self->expected_tests($arg);
        }
        elsif( !defined $arg ) {
            die "Got an undefined number of tests.  Looks like you tried to ".
                "say how many tests you plan to run but made a mistake.\n";
        }
        elsif( !$arg ) {
            die "You said to run 0 tests!  You've got to run something.\n";
        }
    }
    else {
        require Carp;
        my @args = grep { defined } ($cmd, $arg);
        Carp::croak("plan() doesn't understand @args");
    }

    return 1;
}

#line 289

sub expected_tests {
    my $self = shift;
    my($max) = @_;

    if( @_ ) {
        die "Number of tests must be a postive integer.  You gave it '$max'.\n"
          unless $max =~ /^\+?\d+$/ and $max > 0;

        $self->{Expected_Tests} = $max;
        $self->{Have_Plan}      = 1;

        $self->_print("1..$max\n") unless $self->no_header;
    }
    return $self->{Expected_Tests};
}


#line 314

sub no_plan {
    my $self = shift;

    $self->{No_Plan}   = 1;
    $self->{Have_Plan} = 1;
}

#line 329

sub has_plan {
    my $self = shift;

    return($self->{Expected_Tests}) if $self->{Expected_Tests};
    return('no_plan') if $self->{No_Plan};
    return(undef);
};


#line 347

sub skip_all {
    my($self, $reason) = @_;

    my $out = "1..0";
    $out .= " # Skip $reason" if $reason;
    $out .= "\n";

    $self->{Skip_All} = 1;

    $self->_print($out) unless $self->no_header;
    exit(0);
}

#line 380

sub ok {
    my($self, $test, $name) = @_;

    # $test might contain an object which we don't want to accidentally
    # store, so we turn it into a boolean.
    $test = $test ? 1 : 0;

    unless( $self->{Have_Plan} ) {
        require Carp;
        Carp::croak("You tried to run a test without a plan!  Gotta have a plan.");
    }

    lock $self->{Curr_Test};
    $self->{Curr_Test}++;

    # In case $name is a string overloaded object, force it to stringify.
    $self->_unoverload_str(\$name);

    $self->diag(<<ERR) if defined $name and $name =~ /^[\d\s]+$/;
    You named your test '$name'.  You shouldn't use numbers for your test names.
    Very confusing.
ERR

    my($pack, $file, $line) = $self->caller;

    my $todo = $self->todo($pack);
    $self->_unoverload_str(\$todo);

    my $out;
    my $result = &share({});

    unless( $test ) {
        $out .= "not ";
        @$result{ 'ok', 'actual_ok' } = ( ( $todo ? 1 : 0 ), 0 );
    }
    else {
        @$result{ 'ok', 'actual_ok' } = ( 1, $test );
    }

    $out .= "ok";
    $out .= " $self->{Curr_Test}" if $self->use_numbers;

    if( defined $name ) {
        $name =~ s|#|\\#|g;     # # in a name can confuse Test::Harness.
        $out   .= " - $name";
        $result->{name} = $name;
    }
    else {
        $result->{name} = '';
    }

    if( $todo ) {
        $out   .= " # TODO $todo";
        $result->{reason} = $todo;
        $result->{type}   = 'todo';
    }
    else {
        $result->{reason} = '';
        $result->{type}   = '';
    }

    $self->{Test_Results}[$self->{Curr_Test}-1] = $result;
    $out .= "\n";

    $self->_print($out);

    unless( $test ) {
        my $msg = $todo ? "Failed (TODO)" : "Failed";
        $self->_print_diag("\n") if $ENV{HARNESS_ACTIVE};

	if( defined $name ) {
	    $self->diag(qq[  $msg test '$name'\n]);
	    $self->diag(qq[  in $file at line $line.\n]);
	}
	else {
	    $self->diag(qq[  $msg test in $file at line $line.\n]);
	}
    } 

    return $test ? 1 : 0;
}


sub _unoverload {
    my $self  = shift;
    my $type  = shift;

    local($@,$!);

    eval { require overload } || return;

    foreach my $thing (@_) {
        eval { 
            if( _is_object($$thing) ) {
                if( my $string_meth = overload::Method($$thing, $type) ) {
                    $$thing = $$thing->$string_meth();
                }
            }
        };
    }
}


sub _is_object {
    my $thing = shift;

    return eval { ref $thing && $thing->isa('UNIVERSAL') } ? 1 : 0;
}


sub _unoverload_str {
    my $self = shift;

    $self->_unoverload(q[""], @_);
}    

sub _unoverload_num {
    my $self = shift;

    $self->_unoverload('0+', @_);

    for my $val (@_) {
        next unless $self->_is_dualvar($$val);
        $$val = $$val+0;
    }
}


# This is a hack to detect a dualvar such as $!
sub _is_dualvar {
    my($self, $val) = @_;

    local $^W = 0;
    my $numval = $val+0;
    return 1 if $numval != 0 and $numval ne $val;
}



#line 535

sub is_eq {
    my($self, $got, $expect, $name) = @_;
    local $Level = $Level + 1;

    $self->_unoverload_str(\$got, \$expect);

    if( !defined $got || !defined $expect ) {
        # undef only matches undef and nothing else
        my $test = !defined $got && !defined $expect;

        $self->ok($test, $name);
        $self->_is_diag($got, 'eq', $expect) unless $test;
        return $test;
    }

    return $self->cmp_ok($got, 'eq', $expect, $name);
}

sub is_num {
    my($self, $got, $expect, $name) = @_;
    local $Level = $Level + 1;

    $self->_unoverload_num(\$got, \$expect);

    if( !defined $got || !defined $expect ) {
        # undef only matches undef and nothing else
        my $test = !defined $got && !defined $expect;

        $self->ok($test, $name);
        $self->_is_diag($got, '==', $expect) unless $test;
        return $test;
    }

    return $self->cmp_ok($got, '==', $expect, $name);
}

sub _is_diag {
    my($self, $got, $type, $expect) = @_;

    foreach my $val (\$got, \$expect) {
        if( defined $$val ) {
            if( $type eq 'eq' ) {
                # quote and force string context
                $$val = "'$$val'"
            }
            else {
                # force numeric context
                $self->_unoverload_num($val);
            }
        }
        else {
            $$val = 'undef';
        }
    }

    return $self->diag(sprintf <<DIAGNOSTIC, $got, $expect);
         got: %s
    expected: %s
DIAGNOSTIC

}    

#line 613

sub isnt_eq {
    my($self, $got, $dont_expect, $name) = @_;
    local $Level = $Level + 1;

    if( !defined $got || !defined $dont_expect ) {
        # undef only matches undef and nothing else
        my $test = defined $got || defined $dont_expect;

        $self->ok($test, $name);
        $self->_cmp_diag($got, 'ne', $dont_expect) unless $test;
        return $test;
    }

    return $self->cmp_ok($got, 'ne', $dont_expect, $name);
}

sub isnt_num {
    my($self, $got, $dont_expect, $name) = @_;
    local $Level = $Level + 1;

    if( !defined $got || !defined $dont_expect ) {
        # undef only matches undef and nothing else
        my $test = defined $got || defined $dont_expect;

        $self->ok($test, $name);
        $self->_cmp_diag($got, '!=', $dont_expect) unless $test;
        return $test;
    }

    return $self->cmp_ok($got, '!=', $dont_expect, $name);
}


#line 665

sub like {
    my($self, $this, $regex, $name) = @_;

    local $Level = $Level + 1;
    $self->_regex_ok($this, $regex, '=~', $name);
}

sub unlike {
    my($self, $this, $regex, $name) = @_;

    local $Level = $Level + 1;
    $self->_regex_ok($this, $regex, '!~', $name);
}

#line 706


sub maybe_regex {
    my ($self, $regex) = @_;
    my $usable_regex = undef;

    return $usable_regex unless defined $regex;

    my($re, $opts);

    # Check for qr/foo/
    if( ref $regex eq 'Regexp' ) {
        $usable_regex = $regex;
    }
    # Check for '/foo/' or 'm,foo,'
    elsif( ($re, $opts)        = $regex =~ m{^ /(.*)/ (\w*) $ }sx           or
           (undef, $re, $opts) = $regex =~ m,^ m([^\w\s]) (.+) \1 (\w*) $,sx
         )
    {
        $usable_regex = length $opts ? "(?$opts)$re" : $re;
    }

    return $usable_regex;
};

sub _regex_ok {
    my($self, $this, $regex, $cmp, $name) = @_;

    my $ok = 0;
    my $usable_regex = $self->maybe_regex($regex);
    unless (defined $usable_regex) {
        $ok = $self->ok( 0, $name );
        $self->diag("    '$regex' doesn't look much like a regex to me.");
        return $ok;
    }

    {
        my $test;
        my $code = $self->_caller_context;

        local($@, $!);

        # Yes, it has to look like this or 5.4.5 won't see the #line directive.
        # Don't ask me, man, I just work here.
        $test = eval "
$code" . q{$test = $this =~ /$usable_regex/ ? 1 : 0};

        $test = !$test if $cmp eq '!~';

        local $Level = $Level + 1;
        $ok = $self->ok( $test, $name );
    }

    unless( $ok ) {
        $this = defined $this ? "'$this'" : 'undef';
        my $match = $cmp eq '=~' ? "doesn't match" : "matches";
        $self->diag(sprintf <<DIAGNOSTIC, $this, $match, $regex);
                  %s
    %13s '%s'
DIAGNOSTIC

    }

    return $ok;
}

#line 781


my %numeric_cmps = map { ($_, 1) } 
                       ("<",  "<=", ">",  ">=", "==", "!=", "<=>");

sub cmp_ok {
    my($self, $got, $type, $expect, $name) = @_;

    # Treat overloaded objects as numbers if we're asked to do a
    # numeric comparison.
    my $unoverload = $numeric_cmps{$type} ? '_unoverload_num'
                                          : '_unoverload_str';

    $self->$unoverload(\$got, \$expect);


    my $test;
    {
        local($@,$!);   # don't interfere with $@
                        # eval() sometimes resets $!

        my $code = $self->_caller_context;

        # Yes, it has to look like this or 5.4.5 won't see the #line directive.
        # Don't ask me, man, I just work here.
        $test = eval "
$code" . "\$got $type \$expect;";

    }
    local $Level = $Level + 1;
    my $ok = $self->ok($test, $name);

    unless( $ok ) {
        if( $type =~ /^(eq|==)$/ ) {
            $self->_is_diag($got, $type, $expect);
        }
        else {
            $self->_cmp_diag($got, $type, $expect);
        }
    }
    return $ok;
}

sub _cmp_diag {
    my($self, $got, $type, $expect) = @_;
    
    $got    = defined $got    ? "'$got'"    : 'undef';
    $expect = defined $expect ? "'$expect'" : 'undef';
    return $self->diag(sprintf <<DIAGNOSTIC, $got, $type, $expect);
    %s
        %s
    %s
DIAGNOSTIC
}


sub _caller_context {
    my $self = shift;

    my($pack, $file, $line) = $self->caller(1);

    my $code = '';
    $code .= "#line $line $file\n" if defined $file and defined $line;

    return $code;
}


#line 860

sub BAIL_OUT {
    my($self, $reason) = @_;

    $self->{Bailed_Out} = 1;
    $self->_print("Bail out!  $reason");
    exit 255;
}

#line 873

*BAILOUT = \&BAIL_OUT;


#line 885

sub skip {
    my($self, $why) = @_;
    $why ||= '';
    $self->_unoverload_str(\$why);

    unless( $self->{Have_Plan} ) {
        require Carp;
        Carp::croak("You tried to run tests without a plan!  Gotta have a plan.");
    }

    lock($self->{Curr_Test});
    $self->{Curr_Test}++;

    $self->{Test_Results}[$self->{Curr_Test}-1] = &share({
        'ok'      => 1,
        actual_ok => 1,
        name      => '',
        type      => 'skip',
        reason    => $why,
    });

    my $out = "ok";
    $out   .= " $self->{Curr_Test}" if $self->use_numbers;
    $out   .= " # skip";
    $out   .= " $why"       if length $why;
    $out   .= "\n";

    $self->_print($out);

    return 1;
}


#line 930

sub todo_skip {
    my($self, $why) = @_;
    $why ||= '';

    unless( $self->{Have_Plan} ) {
        require Carp;
        Carp::croak("You tried to run tests without a plan!  Gotta have a plan.");
    }

    lock($self->{Curr_Test});
    $self->{Curr_Test}++;

    $self->{Test_Results}[$self->{Curr_Test}-1] = &share({
        'ok'      => 1,
        actual_ok => 0,
        name      => '',
        type      => 'todo_skip',
        reason    => $why,
    });

    my $out = "not ok";
    $out   .= " $self->{Curr_Test}" if $self->use_numbers;
    $out   .= " # TODO & SKIP $why\n";

    $self->_print($out);

    return 1;
}


#line 1001

sub level {
    my($self, $level) = @_;

    if( defined $level ) {
        $Level = $level;
    }
    return $Level;
}


#line 1036

sub use_numbers {
    my($self, $use_nums) = @_;

    if( defined $use_nums ) {
        $self->{Use_Nums} = $use_nums;
    }
    return $self->{Use_Nums};
}


#line 1070

foreach my $attribute (qw(No_Header No_Ending No_Diag)) {
    my $method = lc $attribute;

    my $code = sub {
        my($self, $no) = @_;

        if( defined $no ) {
            $self->{$attribute} = $no;
        }
        return $self->{$attribute};
    };

    no strict 'refs';
    *{__PACKAGE__.'::'.$method} = $code;
}


#line 1124

sub diag {
    my($self, @msgs) = @_;

    return if $self->no_diag;
    return unless @msgs;

    # Prevent printing headers when compiling (i.e. -c)
    return if $^C;

    # Smash args together like print does.
    # Convert undef to 'undef' so its readable.
    my $msg = join '', map { defined($_) ? $_ : 'undef' } @msgs;

    # Escape each line with a #.
    $msg =~ s/^/# /gm;

    # Stick a newline on the end if it needs it.
    $msg .= "\n" unless $msg =~ /\n\Z/;

    local $Level = $Level + 1;
    $self->_print_diag($msg);

    return 0;
}

#line 1161

sub _print {
    my($self, @msgs) = @_;

    # Prevent printing headers when only compiling.  Mostly for when
    # tests are deparsed with B::Deparse
    return if $^C;

    my $msg = join '', @msgs;

    local($\, $", $,) = (undef, ' ', '');
    my $fh = $self->output;

    # Escape each line after the first with a # so we don't
    # confuse Test::Harness.
    $msg =~ s/\n(.)/\n# $1/sg;

    # Stick a newline on the end if it needs it.
    $msg .= "\n" unless $msg =~ /\n\Z/;

    print $fh $msg;
}


#line 1192

sub _print_diag {
    my $self = shift;

    local($\, $", $,) = (undef, ' ', '');
    my $fh = $self->todo ? $self->todo_output : $self->failure_output;
    print $fh @_;
}    

#line 1229

sub output {
    my($self, $fh) = @_;

    if( defined $fh ) {
        $self->{Out_FH} = _new_fh($fh);
    }
    return $self->{Out_FH};
}

sub failure_output {
    my($self, $fh) = @_;

    if( defined $fh ) {
        $self->{Fail_FH} = _new_fh($fh);
    }
    return $self->{Fail_FH};
}

sub todo_output {
    my($self, $fh) = @_;

    if( defined $fh ) {
        $self->{Todo_FH} = _new_fh($fh);
    }
    return $self->{Todo_FH};
}


sub _new_fh {
    my($file_or_fh) = shift;

    my $fh;
    if( _is_fh($file_or_fh) ) {
        $fh = $file_or_fh;
    }
    else {
        $fh = do { local *FH };
        open $fh, ">$file_or_fh" or 
            die "Can't open test output log $file_or_fh: $!";
	_autoflush($fh);
    }

    return $fh;
}


sub _is_fh {
    my $maybe_fh = shift;
    return 0 unless defined $maybe_fh;

    return 1 if ref \$maybe_fh eq 'GLOB'; # its a glob

    return UNIVERSAL::isa($maybe_fh,               'GLOB')       ||
           UNIVERSAL::isa($maybe_fh,               'IO::Handle') ||

           # 5.5.4's tied() and can() doesn't like getting undef
           UNIVERSAL::can((tied($maybe_fh) || ''), 'TIEHANDLE');
}


sub _autoflush {
    my($fh) = shift;
    my $old_fh = select $fh;
    $| = 1;
    select $old_fh;
}


sub _dup_stdhandles {
    my $self = shift;

    $self->_open_testhandles;

    # Set everything to unbuffered else plain prints to STDOUT will
    # come out in the wrong order from our own prints.
    _autoflush(\*TESTOUT);
    _autoflush(\*STDOUT);
    _autoflush(\*TESTERR);
    _autoflush(\*STDERR);

    $self->output(\*TESTOUT);
    $self->failure_output(\*TESTERR);
    $self->todo_output(\*TESTOUT);
}


my $Opened_Testhandles = 0;
sub _open_testhandles {
    return if $Opened_Testhandles;
    # We dup STDOUT and STDERR so people can change them in their
    # test suites while still getting normal test output.
    open(TESTOUT, ">&STDOUT") or die "Can't dup STDOUT:  $!";
    open(TESTERR, ">&STDERR") or die "Can't dup STDERR:  $!";
    $Opened_Testhandles = 1;
}


#line 1347

sub current_test {
    my($self, $num) = @_;

    lock($self->{Curr_Test});
    if( defined $num ) {
        unless( $self->{Have_Plan} ) {
            require Carp;
            Carp::croak("Can't change the current test number without a plan!");
        }

        $self->{Curr_Test} = $num;

        # If the test counter is being pushed forward fill in the details.
        my $test_results = $self->{Test_Results};
        if( $num > @$test_results ) {
            my $start = @$test_results ? @$test_results : 0;
            for ($start..$num-1) {
                $test_results->[$_] = &share({
                    'ok'      => 1, 
                    actual_ok => undef, 
                    reason    => 'incrementing test number', 
                    type      => 'unknown', 
                    name      => undef 
                });
            }
        }
        # If backward, wipe history.  Its their funeral.
        elsif( $num < @$test_results ) {
            $#{$test_results} = $num - 1;
        }
    }
    return $self->{Curr_Test};
}


#line 1393

sub summary {
    my($self) = shift;

    return map { $_->{'ok'} } @{ $self->{Test_Results} };
}

#line 1448

sub details {
    my $self = shift;
    return @{ $self->{Test_Results} };
}

#line 1473

sub todo {
    my($self, $pack) = @_;

    $pack = $pack || $self->exported_to || $self->caller($Level);
    return 0 unless $pack;

    no strict 'refs';
    return defined ${$pack.'::TODO'} ? ${$pack.'::TODO'}
                                     : 0;
}

#line 1494

sub caller {
    my($self, $height) = @_;
    $height ||= 0;

    my @caller = CORE::caller($self->level + $height + 1);
    return wantarray ? @caller : $caller[0];
}

#line 1506

#line 1520

#'#
sub _sanity_check {
    my $self = shift;

    _whoa($self->{Curr_Test} < 0,  'Says here you ran a negative number of tests!');
    _whoa(!$self->{Have_Plan} and $self->{Curr_Test}, 
          'Somehow your tests ran without a plan!');
    _whoa($self->{Curr_Test} != @{ $self->{Test_Results} },
          'Somehow you got a different number of results than tests ran!');
}

#line 1541

sub _whoa {
    my($check, $desc) = @_;
    if( $check ) {
        die <<WHOA;
WHOA!  $desc
This should never happen!  Please contact the author immediately!
WHOA
    }
}

#line 1562

sub _my_exit {
    $? = $_[0];

    return 1;
}


#line 1575

$SIG{__DIE__} = sub {
    # We don't want to muck with death in an eval, but $^S isn't
    # totally reliable.  5.005_03 and 5.6.1 both do the wrong thing
    # with it.  Instead, we use caller.  This also means it runs under
    # 5.004!
    my $in_eval = 0;
    for( my $stack = 1;  my $sub = (CORE::caller($stack))[3];  $stack++ ) {
        $in_eval = 1 if $sub =~ /^\(eval\)/;
    }
    $Test->{Test_Died} = 1 unless $in_eval;
};

sub _ending {
    my $self = shift;

    $self->_sanity_check();

    # Don't bother with an ending if this is a forked copy.  Only the parent
    # should do the ending.
    # Exit if plan() was never called.  This is so "require Test::Simple" 
    # doesn't puke.
    # Don't do an ending if we bailed out.
    if( ($self->{Original_Pid} != $$) 			or
	(!$self->{Have_Plan} && !$self->{Test_Died}) 	or
	$self->{Bailed_Out}
      )
    {
	_my_exit($?);
	return;
    }

    # Figure out if we passed or failed and print helpful messages.
    my $test_results = $self->{Test_Results};
    if( @$test_results ) {
        # The plan?  We have no plan.
        if( $self->{No_Plan} ) {
            $self->_print("1..$self->{Curr_Test}\n") unless $self->no_header;
            $self->{Expected_Tests} = $self->{Curr_Test};
        }

        # Auto-extended arrays and elements which aren't explicitly
        # filled in with a shared reference will puke under 5.8.0
        # ithreads.  So we have to fill them in by hand. :(
        my $empty_result = &share({});
        for my $idx ( 0..$self->{Expected_Tests}-1 ) {
            $test_results->[$idx] = $empty_result
              unless defined $test_results->[$idx];
        }

        my $num_failed = grep !$_->{'ok'}, 
                              @{$test_results}[0..$self->{Curr_Test}-1];

        my $num_extra = $self->{Curr_Test} - $self->{Expected_Tests};

        if( $num_extra < 0 ) {
            my $s = $self->{Expected_Tests} == 1 ? '' : 's';
            $self->diag(<<"FAIL");
Looks like you planned $self->{Expected_Tests} test$s but only ran $self->{Curr_Test}.
FAIL
        }
        elsif( $num_extra > 0 ) {
            my $s = $self->{Expected_Tests} == 1 ? '' : 's';
            $self->diag(<<"FAIL");
Looks like you planned $self->{Expected_Tests} test$s but ran $num_extra extra.
FAIL
        }

        if ( $num_failed ) {
            my $num_tests = $self->{Curr_Test};
            my $s = $num_failed == 1 ? '' : 's';

            my $qualifier = $num_extra == 0 ? '' : ' run';

            $self->diag(<<"FAIL");
Looks like you failed $num_failed test$s of $num_tests$qualifier.
FAIL
        }

        if( $self->{Test_Died} ) {
            $self->diag(<<"FAIL");
Looks like your test died just after $self->{Curr_Test}.
FAIL

            _my_exit( 255 ) && return;
        }

        my $exit_code;
        if( $num_failed ) {
            $exit_code = $num_failed <= 254 ? $num_failed : 254;
        }
        elsif( $num_extra != 0 ) {
            $exit_code = 255;
        }
        else {
            $exit_code = 0;
        }

        _my_exit( $exit_code ) && return;
    }
    elsif ( $self->{Skip_All} ) {
        _my_exit( 0 ) && return;
    }
    elsif ( $self->{Test_Died} ) {
        $self->diag(<<'FAIL');
Looks like your test died before it could output anything.
FAIL
        _my_exit( 255 ) && return;
    }
    else {
        $self->diag("No tests run!\n");
        _my_exit( 255 ) && return;
    }
}

END {
    $Test->_ending if defined $Test and !$Test->no_ending;
}

#line 1747

1;
