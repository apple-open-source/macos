use Test;
BEGIN { 
    $ENV{LIST_MOREUTILS_PP} = 0;
};
use List::MoreUtils qw/:all/;
use strict;

{
    local $^W = 0;
    *ok = sub ($;$$) {
	if (@_ == 1) {
	    skip(List::MoreUtils::_XScompiled() ? 0 : "XS portion not compiled", $_[0]);
	} elsif (@_ == 2) {
	    skip(List::MoreUtils::_XScompiled() ? 0 : "XS portion not compiled", $_[0], $_[1]);
	} else {
	    skip(List::MoreUtils::_XScompiled() ? 0 : "XS portion not compiled", $_[0], $_[1], $_[2]);
	}
    };
}

sub arrayeq {
    local $^W = 0;
    my ($ary1, $ary2) = @_;
    return 0 if @$ary1 != @$ary2;
    for (0 .. $#$ary1) {
	if ($ary1->[$_] ne $ary2->[$_]) {
	    local $" = ", ";
	    warn "(@$ary1) != (@$ary2)\n";
	    return 0;
	}
    }
    return 1;
}

my $TESTS = 0;

# any
BEGIN { $TESTS += 6 }
{
    my @list = (1 .. 10000);
    ok(any { $_ == 5000 } @list);
    ok(any { $_ == 5000 } 1 .. 10000);
    ok(any { defined } @list);
    ok(!any { !defined } @list);
    ok(any { !defined } undef);
    ok(!defined(any { }));
}

# all (7...)
BEGIN { $TESTS += 4 }
{
    my @list = (1 .. 10000);
    ok(all { defined } @list);
    ok(all { $_ > 0 } @list);
    ok(!all { $_ < 5000 } @list);
    ok(!defined all { } );
}

# none (11...)
BEGIN { $TESTS += 4 }
{
    my @list = (1 .. 10000);
    ok(none { !defined } @list);
    ok(none { $_ > 10000 } @list);
    ok(!none { defined } @list);
    ok(!defined none { });
}

# notall (15...)
BEGIN { $TESTS += 4 }
{
    my @list = (1 .. 10000);
    ok(notall { !defined } @list);
    ok(notall { $_ < 10000 } @list);
    ok(!notall { $_ <= 10000 } @list);
    ok(!defined notall { });
}

# true (19...)
BEGIN { $TESTS += 4 }
{
    my @list = (1 .. 10000);
    ok(10000, true { defined } @list);
    ok(0, true { !defined } @list);
    ok(1, true { $_ == 10000 } @list);
    ok(!true { });
}

# false (23...)
BEGIN { $TESTS += 4 }
{
    my @list = (1 .. 10000);
    ok(10000, false { !defined } @list);
    ok(0, false { defined } @list);
    ok(1, false { $_ > 1 } @list);
    ok(!false { });
}

# firstidx (27...)
BEGIN { $TESTS += 4 }
{
    my @list = (1 .. 10000);
    ok(4999, firstidx { $_ >= 5000 } @list);
    ok(-1, firstidx { !defined } @list);
    ok(0, firstidx { defined } @list);
    ok(-1, firstidx { });
}

# lastidx (31...)
BEGIN { $TESTS += 8 }
{
    my @list = (1 .. 10000);
    ok(9999, lastidx { $_ >= 5000 } @list);
    ok(-1, lastidx { !defined } @list);
    ok(9999, lastidx { defined } @list);
    ok(-1, lastidx { });

    # test aliases
    ok(9999, last_index { $_ >= 5000 } @list);
    ok(-1, last_index { !defined } @list);
    ok(9999, last_index { defined } @list);
    ok(-1, last_index { });
}

# insert_after (39...)
BEGIN { $TESTS += 4 }
{
    my @list = qw/This is a list/;
    insert_after { $_ eq "a" } "longer" => @list;
    ok(join(' ', @list), "This is a longer list");
    insert_after { 0 } "bla" => @list;
    ok(join(' ', @list), "This is a longer list");
    insert_after { $_ eq "list" } "!" => @list;
    ok(join(' ', @list), "This is a longer list !");
    @list = (qw/This is/, undef, qw/list/);
    insert_after { !defined($_) } "longer" => @list;
    $list[2] = "a";
    ok(join(' ', @list), "This is a longer list");
}

# insert_after_string (43...)
BEGIN { $TESTS += 3 }
{
    my @list = qw/This is a list/;
    insert_after_string "a", "longer" => @list;
    ok(join(' ', @list), "This is a longer list");
    @list = (undef, qw/This is a list/);
    insert_after_string "a", "longer", @list;
    shift @list;
    ok(join(' ', @list), "This is a longer list");
    @list = ("This\0", "is\0", "a\0", "list\0");
    insert_after_string "a\0", "longer\0", @list;
    ok(join(' ', @list), "This\0 is\0 a\0 longer\0 list\0");
}

# apply (46...)
BEGIN { $TESTS += 6 }
{
    my @list  = (0 .. 9);
    my @list1 = apply { $_++ } @list;
    ok(arrayeq(\@list, [0..9]));
    ok(arrayeq(\@list1, [1..10]));

    @list = (" foo ", " bar ", "     ", "foobar");
    @list1 = apply { s/^\s+|\s+$//g } @list;
    ok(arrayeq(\@list, [" foo ", " bar ", "     ", "foobar"]));
    ok(arrayeq(\@list1, ["foo", "bar", "", "foobar"]));

    my $item = apply { s/^\s+|\s+$//g } @list;
    ok($item, "foobar");

    ok(! defined apply {});
}

# In the following, the @dummy variable is needed to circumvent
# a parser glitch in the 5.6.x series.

#after (52...)
BEGIN { $TESTS += 3 }
{
    my @x = after { $_ % 5 == 0 } 1..9;
    ok(arrayeq(\@x, [6,7,8,9]));
    @x = after { /foo/ } my @dummy = qw/bar baz/;
    ok(!@x);
    @x = after { /b/ } @dummy = qw/bar baz foo/;
    ok(arrayeq(\@x, [ qw/baz foo/ ]));
}

#after_incl (55...)
BEGIN { $TESTS += 3 }
{
    my @x = after_incl {$_ % 5 == 0} 1..9;
    ok(arrayeq(\@x, [5, 6, 7, 8, 9]));
    @x = after_incl { /foo/ } my @dummy = qw/bar baz/;
    ok(!@x);
    @x = after_incl { /b/ } @dummy = qw/bar baz foo/;
    ok(arrayeq(\@x, [ qw/bar baz foo/ ]));
}

#before (58...)
BEGIN { $TESTS += 3 }
{
    my @x = before {$_ % 5 == 0} 1..9;    
    ok(arrayeq(\@x, [1, 2, 3, 4]));
    @x = before { /b/ } my @dummy = qw/bar baz/;
    ok(!@x);
    @x = before { /f/ } @dummy = qw/bar baz foo/;
    ok(arrayeq(\@x, [  qw/bar baz/ ]));
}

#before_incl (61...)
BEGIN { $TESTS += 3 }
{
    my @x = before_incl {$_ % 5 == 0} 1..9;
    ok(arrayeq(\@x, [1, 2, 3, 4, 5]));
    @x = before_incl { /foo/ } my @dummy = qw/bar baz/;
    ok(arrayeq(\@x, [ qw/bar baz/ ]));
    @x = before_incl { /f/ } @dummy = qw/bar baz foo/;
    ok(arrayeq(\@x, [ qw/bar baz foo/ ]));
}

#indexes (64...)
BEGIN { $TESTS += 2 }
{
    my @x = indexes {$_ > 5}  4..9;
    ok(arrayeq(\@x, [2..5]));
    @x = indexes {$_ > 5}  1..4;  
    ok(!@x);
}

#lastval/last_value (67...)
BEGIN { $TESTS += 4 }
{
    my $x = last_value {$_ > 5}  4..9;  
    ok($x, 9);
    $x = last_value {$_ > 5}  1..4;
    ok(!defined $x);

    $x = lastval {$_ > 5}  4..9;
    ok($x, 9);
    $x = lastval {$_ > 5}  1..4;
    ok(!defined $x);
}

#firstval/first_value (71...)
BEGIN { $TESTS += 4 }
{
    my $x = first_value {$_ > 5}  4..9; 
    ok($x, 6);
    $x = first_value {$_ > 5}  1..4;
    ok(!defined $x);

    $x = firstval {$_ > 5}  4..9; 
    ok($x, 6);
    $x = firstval {$_ > 5}  1..4;
    ok(!defined $x);
    
}

#each_array (75...)
BEGIN { $TESTS += 10 }
{
    my @a = (7, 3, 'a', undef, 'r');
    my @b = qw/a 2 -1 x/;

    my $it = each_array @a, @b;
    my (@r, @idx);
    while (my ($a, $b) = $it->())
    {
	push @r, $a, $b;
	push @idx, $it->('index');
    }
    
    $it->(); # do I segfault? I shouldn't. 

    ok(arrayeq(\@r, [7, 'a', 3, 2, 'a', -1, undef, 'x', 'r', undef]));
    ok(arrayeq(\@idx, [0..4]));

    # testing two iterators on the same arrays in parallel
    @a = (1, 3, 5);
    @b = (2, 4, 6);
    my $i1 = each_array @a, @b;
    my $i2 = each_array @a, @b;
    @r = ();
    while (my ($a, $b) = $i1->() and my ($c, $d) = $i2->()) {
	push @r, $a, $b, $c, $d;
    }
    ok(arrayeq(\@r, [1,2,1,2,3,4,3,4,5,6,5,6]));

    # input arrays must not be modified
    ok(arrayeq(\@a, [1,3,5]));
    ok(arrayeq(\@b, [2,4,6]));

    # this used to give "semi-panic: attempt to dup freed string"
    # See: <news:1140827861.481475.111380@z34g2000cwc.googlegroups.com>
    my $ea = each_arrayref([1 .. 26], ['A' .. 'Z']);
    (@a, @b) = ();
    while (my ($a, $b) = $ea->()) {
	push @a, $a; push @b, $b;
    }
    ok(arrayeq(\@a, [1 .. 26]));
    ok(arrayeq(\@b, ['A' .. 'Z']));

    # and this even used to dump core
    my @nums = 1 .. 26;
    $ea = each_arrayref(\@nums, ['A' .. 'Z']);
    (@a, @b) = ();
    while (my ($a, $b) = $ea->()) {
	push @a, $a; push @b, $b;
    }
    ok(arrayeq(\@a, [1 .. 26]));
    ok(arrayeq(\@a, \@nums));
    ok(arrayeq(\@b, ['A' .. 'Z']));
}

#each_array (85...)
BEGIN { $TESTS += 5 }
{
    my @a = (7, 3, 'a', undef, 'r');
    my @b = qw/a 2 -1 x/;

    my $it = each_arrayref \@a, \@b;
    my (@r, @idx);
    while (my ($a, $b) = $it->())
    {
	push @r, $a, $b;
	push @idx, $it->('index');
    }
    
    $it->(); # do I segfault? I shouldn't. 

    ok(arrayeq(\@r, [7, 'a', 3, 2, 'a', -1, undef, 'x', 'r', undef]));
    ok(arrayeq(\@idx, [0..4]));

    # testing two iterators on the same arrays in parallel
    @a = (1, 3, 5);
    @b = (2, 4, 6);
    my $i1 = each_array @a, @b;
    my $i2 = each_array @a, @b;
    @r = ();
    while (my ($a, $b) = $i1->() and my ($c, $d) = $i2->()) {
	push @r, $a, $b, $c, $d;
    }
    ok(arrayeq(\@r, [1,2,1,2,3,4,3,4,5,6,5,6]));

    # input arrays must not be modified
    ok(arrayeq(\@a, [1,3,5]));
    ok(arrayeq(\@b, [2,4,6]));

}

#pairwise (90...)
BEGIN { $TESTS += 10 }
{
    my @a = (1, 2, 3, 4, 5);
    my @b = (2, 4, 6, 8, 10);
    my @c = pairwise {$a + $b} @a, @b;
    ok(arrayeq(\@c, [3, 6, 9, 12, 15]), 1, "pw1");

    @c = pairwise {$a * $b} @a, @b;   # returns (2, 8, 18)
    ok(arrayeq(\@c, [2, 8, 18, 32, 50]), 1, "pw2");

    # did we modify the input arrays?
    ok(arrayeq(\@a, [1, 2, 3, 4, 5]), 1, "pw3");
    ok(arrayeq(\@b, [2, 4, 6, 8, 10]), 1, "pw4");
   
    # $a and $b should be aliases: test
    @b = @a = (1, 2, 3);
    @c = pairwise { $a++; $b *= 2 } @a, @b;
    ok(arrayeq(\@a, [2, 3, 4]), 1, "pw5");
    ok(arrayeq(\@b, [2, 4, 6]), 1, "pw6");
    ok(arrayeq(\@c, [2, 4, 6]), 1, "pw7");

    # test this one more thoroughly: the XS code looks flakey
    # correctness of pairwise_perl proved by human auditing. :-)
    
    sub pairwise_perl (&\@\@)
    {
	no strict;
	my $op = shift;
	local (*A, *B) = @_;    # syms for caller's input arrays

	# Localise $a, $b
	my ($caller_a, $caller_b) = do
	{
	    my $pkg = caller();
	    \*{$pkg.'::a'}, \*{$pkg.'::b'};
	};

	my $limit = $#A > $#B? $#A : $#B;    # loop iteration limit

	local(*$caller_a, *$caller_b);
	map # This map expression is also the return value.
	{
	    # assign to $a, $b as refs to caller's array elements
	    (*$caller_a, *$caller_b) = \($A[$_], $B[$_]);
	    $op->();    # perform the transformation
	}  0 .. $limit;
    }
	
    (@a, @b) = ();
    push @a, int rand(10000) for 0 .. rand(10000);
    push @b, int rand(10000) for 0 .. rand(10000);
    local $^W = 0;
    my @res1 = pairwise {$a+$b} @a, @b;
    my @res2 = pairwise_perl {$a+$b} @a, @b;
    ok(arrayeq(\@res1, \@res2));

    @a = qw/a b c/;
    @b = qw/1 2 3/;
    @c = pairwise { ($a, $b) } @a, @b;
    ok(arrayeq(\@c, [qw/a 1 b 2 c 3/]));  # 88

    # test that a die inside the code-reference will no longer be trapped
    eval { pairwise { die "I died\n" } @a, @b };
    ok($@, "I died\n");
}

#natatime (100...)
BEGIN { $TESTS += 3 }
{
    my @x = ('a'..'g');
    my $it = natatime 3, @x;
    my @r;
    local $" = " ";
    while (my @vals = $it->())
    {
	push @r, "@vals";
    }
    ok(arrayeq(\@r, ['a b c', 'd e f', 'g']), 1, "natatime1");

    my @a = (1 .. 10000);
    $it = natatime 1, @a;
    @r = ();
    while (my @vals = &$it) {
	push @r, @vals;
    }
    ok(arrayeq(\@r, \@a), 1, "natatime2");

    $it = natatime 1, 1 .. 26;
    @r = ();
    while (my @vals = &$it) {
	push @r, @vals;
    }
    ok(arrayeq(\@r, [1 .. 26]), 1, "natatime3");
}

#mesh (103...)
BEGIN { $TESTS += 3 }
{
    my @x = qw/a b c d/;
    my @y = qw/1 2 3 4/;
    my @z = mesh @x, @y;
    ok(arrayeq(\@z, ['a', 1, 'b', 2, 'c', 3, 'd', 4]));

    my @a = ('x');
    my @b = ('1', '2');
    my @c = qw/zip zap zot/;
    @z = mesh @a, @b, @c;
    ok(arrayeq(\@z, ['x', 1, 'zip', undef, 2, 'zap', undef, undef, 'zot']));

    @a = (1 .. 10);
    my @d; $#d = 9; # make array with holes
    @z = mesh @a, @d;
    ok(arrayeq(\@z, [1, undef, 2, undef, 3, undef, 4, undef, 5, undef, 
		     6, undef, 7, undef, 8, undef, 9, undef, 10, undef]));
}

#zip (just an alias for mesh) (106...)
BEGIN { $TESTS += 3 }
{
    my @x = qw/a b c d/;
    my @y = qw/1 2 3 4/;
    my @z = zip @x, @y;
    ok(arrayeq(\@z, ['a', 1, 'b', 2, 'c', 3, 'd', 4]));

    my @a = ('x');
    my @b = ('1', '2');
    my @c = qw/zip zap zot/;
    @z = zip @a, @b, @c;
    ok(arrayeq(\@z, ['x', 1, 'zip', undef, 2, 'zap', undef, undef, 'zot']));

    @a = (1 .. 10);
    my @d; $#d = 9; # make array with holes
    @z = zip @a, @d;
    ok(arrayeq(\@z, [1, undef, 2, undef, 3, undef, 4, undef, 5, undef, 
		     6, undef, 7, undef, 8, undef, 9, undef, 10, undef]));
}

#uniq (109...)
BEGIN { $TESTS += 2 }
{
    my @a = map { (1 .. 10000) } 0 .. 1;
    my @u = uniq @a;
    ok(arrayeq(\@u, [1 .. 10000]));
    my $u = uniq @a;
    ok(10000, $u);
}

#minmax (111...)
BEGIN { $TESTS += 6 }
{
    my @list = reverse 0 .. 100_000;
    my ($min, $max) = minmax(@list);
    ok($min, 0);
    ok($max, 100_000);

    # even number of elements
    push @list, 100_001;
    ($min, $max) = minmax(@list);
    ok($min, 0);
    ok($max, 100_001);

    # some floats
    @list = (0, -1.1, 3.14, 1/7, 100_000, -10/3);
    ($min, $max) = minmax(@list);
    # floating-point comparison cunningly avoided
    ok(sprintf("%.2f", $min), "-3.33");
    ok($max, 100_000);
}

#part (116...)
BEGIN { $TESTS += 24 }
{
    my @list = 1 .. 12;
    my $i = 0;
    my @part = part { $i++ % 3 } @list;
    ok(arrayeq($part[0], [ 1, 4, 7, 10 ]));
    ok(arrayeq($part[1], [ 2, 5, 8, 11 ]));
    ok(arrayeq($part[2], [ 3, 6, 9, 12 ]));

    @part = part { 3 } @list;
    ok(!defined $part[0]);
    ok(!defined $part[1]);
    ok(!defined $part[2]); 
    ok(arrayeq($part[3], [ 1 .. 12 ]));

    eval { @part = part { -1 } @list };
    ok($@ =~ /^Modification of non-creatable array value attempted, subscript -1/);

    $i = 0;
    @part = part { $i++ == 0 ? 0 : -1 } @list;
    ok(arrayeq($part[0], [ 1 .. 12 ]));

    local $^W = 0;
    @part = part { undef } @list;
    ok(arrayeq($part[0], [ 1 .. 12 ]));

    @part = part { 1_000_000 } @list;
    ok(arrayeq($part[1_000_000], [ @list ]));
    ok(!defined $part[0]);
    ok(!defined $part[@part/2]);
    ok(!defined $part[999_999]);

	# changing the list in place used to destroy
	# its elements due to a wrong refcnt
	@list = 1 .. 10;
	@list = part { $_ } @list;
	for (1 .. 10) {
		ok(arrayeq($list[$_], [ $_ ]));
	}
}

BEGIN { plan tests => $TESTS }

