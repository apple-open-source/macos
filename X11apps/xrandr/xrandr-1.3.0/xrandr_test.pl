#!/usr/bin/perl

#
# xrandr Test suite
#
# Do a set of xrandr calls and verify that the screen setup is as expected
# after each call.
#

$xrandr="xrandr";
$xrandr=$ENV{XRANDR} if defined $ENV{XRANDR};
$version="0.1";
$inbetween="";
print "\n***** xrandr test suite V$version *****\n\n";

# Known issues and their fixes
%fixes=(
 s2 => "xrandr: 307f3686",
 s4 => "xserver: f7dd0c72",
 s11 => "xrandr: f7aaf894",
 s18 => "issue known, but not fixed yet"
);

# Get output configuration
@outputs=();
%mode_name=();
%out_modes=();
%modes=();
open P, "$xrandr --verbose|" or die "$xrandr";
while (<P>) {
  if (/^\S/) {
    $o=""; $m=""; $x="";
  }
  if (/^(\S+)\s(connected|unknown connection)\s/) {
    $o=$1;
    push @outputs, $o         if $2 eq "connected";
    push @outputs_unknown, $o if $2 eq "unknown connection";
    $out_modes{$o}=[];
  } elsif (/^\s+(\d+x\d+)\s+\((0x[0-9a-f]+)\)/) {
    my $m=$1;
    my $x=$2;
    while (<P>) {
      if (/^\s+(\d+x\d+)\s+\((0x[0-9a-f]+)\)/) {
        print "WARNING: Ignoring incomplete mode $x:$m on $o\n";
        $m=$1, $x=$2;
      } elsif (/^\s+v:.*?([0-9.]+)Hz\s*$/) {
        if (defined $mode_name{$x} && $mode_name{$x} ne "$m\@$1") {
	  print "WARNING: Ignoring mode $x:$m\@$1 because $x:$mode_name{$x} already exists\n";
	  last;
	}
	if (defined $modes{"$o:$x"}) {
	  print "WARNING: Ignoring duplicate mode $x on $o\n";
	  last;
	}
	$mode_name{$x}="$m\@$1";
	push @{$out_modes{$o}}, $x;
	$modes{"$o:$x"}=$x;
	$modes{"$o:$m\@$1"}=$x;
	$modes{"$o:$m"}=$x;
        last;
      }
    }
  }
}
close P;
@outputs=(@outputs,@outputs_unknown) if @outputs < 2;

# preamble
if ($ARGV[0] eq "-w") {
  print "Waiting for keypress after each test for manual verification.\n\n";
  $inbetween='print "    Press <Return> to continue...\n"; $_=<STDIN>';
} elsif ($ARGV[0] ne "") {
  print "Preparing for test # $ARGV[0]\n\n";
  $prepare = $ARGV[0];
}

print "Detected connected outputs and available modes:\n\n";
for $o (@outputs) {
  print "$o:";
  my $i=0;
  for $x (@{$out_modes{$o}}) {
    print "\n" if $i++ % 3 == 0;
    print "  $x:$mode_name{$x}";
  }
  print "\n";
}
print "\n";

if (@outputs < 2) {
  print "Found less than two connected outputs. No tests available for that.\n";
  exit 1;
}
if (@outputs > 2) {
  print "Note: No tests for more than two connected outputs available yet.\n";
  print "Using the first two outputs.\n\n";
}

$a=$outputs[0];
$b=$outputs[1];

# For each resolution only a single refresh rate should be used in order to
# reduce ambiguities. For that we need to find unused modes. The %used hash is
# used to track used ones. All references point to <id>.
#   <output>:<id>
#   <output>:<width>x<height>@<refresh>
#   <output>:<width>x<height>
#   <id>
#   <width>x<height>@<refresh>
#   <width>x<height>
%used=();

# Find biggest common mode
undef $sab;
for my $x (@{$out_modes{$a}}) {
  if (defined $modes{"$b:$x"}) {
    $m=$mode_name{$x};
    $sab="$x:$m";
    $m =~ m/(\d+x\d+)\@([0-9.]+)/;
    $used{$x} = $x;
    $used{$1} = $x;
    $used{"$a:$x"} = $x;
    $used{"$b:$x"} = $x;
    $used{"$a:$m"} = $mode_name{$x};
    $used{"$b:$m"} = $mode_name{$x};
    $used{"$a:$1"} = $x;
    $used{"$b:$1"} = $x;
    last;
  }
}
if (! defined $sab) {
  print "Cannot find common mode between $a and $b.\n";
  print "Test suite is designed to need a common mode.\n";
  exit 1;
}

# Find sets of additional non-common modes
# Try to get non-overlapping resolution set, but if that fails get overlapping
# ones but with different refresh values, if that fails any with nonequal
# timings, and if that fails any one, but warn.
# Try modes unknown to other outputs first, they might need common ones
# themselves.
sub get_mode {
  my $o=$_[0];
  for my $pass (1, 2, 3, 4, 5, 6, 7, 8, 9) {
    CONT: for my $x (@{$out_modes{$o}}) {
      $m = $mode_name{$x};
      $m =~ m/(\d+x\d+)\@([0-9.]+)/;
      next CONT if defined $used{"$o:$x"};
      next CONT if $pass < 9 && defined $used{"$o:$m"};
      next CONT if $pass < 7 && defined $used{"$o:$1"};
      next CONT if $pass < 6 && defined $used{$m};
      next CONT if $pass < 4 && defined $used{$1};
      for my $other (@outputs) {
        next if $other eq $o;
        next CONT if $pass < 8 && defined $used{"$o:$x"};
        next CONT if $pass < 5 && $used{"$other:$1"};
	next CONT if $pass < 3 && $modes{"$other:$m"};
	next CONT if $pass < 2 && $modes{"$other:$1"};
      }
      if ($pass >= 6) {
        print "Warning: No more non-common modes, using $m for $o\n";
      }
      $used{"$o:$x"} = $x;
      $used{"$o:$m"} = $x;
      $used{"$o:$1"} = $x;
      $used{$x} = $x;
      $used{$m} = $x;
      $used{$1} = $x;
      return "$x:$m";
    }
  }
  print "Warning: Cannot find any more modes for $o.\n";
  return undef;
}
sub mode_to_randr {
  $_[0] =~ m/^(0x[0-9a-f]+):(\d+)x(\d+)\@([0-9.]+)/;
  return "--mode $1";
}

$sa1=get_mode($a);
$sa2=get_mode($a);
$sb1=get_mode($b);
$sb2=get_mode($b);

$mab=mode_to_randr($sab);
$ma1=mode_to_randr($sa1);
$ma2=mode_to_randr($sa2);
$mb1=mode_to_randr($sb1);
$mb2=mode_to_randr($sb2);

# Shortcuts
$oa="--output $a";
$ob="--output $b";

# Print config
print "A:  $a (mab,ma1,ma2)\nB:  $b (mab,mb1,mb2)\n\n";
print "mab: $sab\nma1: $sa1\nma2: $sa2\nmb1: $sb1\nmb2: $sb2\n\n";
print "Initial config:\n";
system "$xrandr";
print "\n";

# Test subroutine
sub t {
  my $name=$_[0];
  my $expect=$_[1];
  my $args=$_[2];
  print "*** $name:  $args\n";
  print "?   $expect\n" if $expect ne "";
  if ($name eq $prepare) {
    print "->  Prepared to run test\n\nRun test now with\n$xrandr --verbose $args\n\n";
    exit 0;
  }
  my %r   = ();
  my $r   = "";
  my $out = "";
  if (system ("$xrandr --verbose $args") == 0) {
    # Determine active configuration
    open P, "$xrandr --verbose|" or die "$xrandr";
    my ($o, $c, $m, $x);
    while (<P>) {
      $out.=$_;
      if (/^\S/) {
        $o=""; $c=""; $m=""; $x="";
      }
      if (/^(\S+)\s(connected|unknown connection) (\d+x\d+)\+\d+\+\d+\s+\((0x[0-9a-f]+)\)/) {
        $o=$1;
	$m=$3;
	$x=$4;
	$o="A" if $o eq $a;
	$o="B" if $o eq $b;
      } elsif (/^\s*CRTC:\s*(\d)/) {
        $c=$1;
      } elsif (/^\s+$m\s+\($x\)/) {
        while (<P>) {
	  $out.=$_;
          if (/^\s+\d+x\d+\s/) {
	    $r{$o}="$x:$m\@?($c)" unless defined $r{$o};
	    # we don't have to reparse this - something is wrong anyway,
	    # and it probably is no relevant resolution as well
	    last;
	  } elsif (/^\s+v:.*?([0-9.]+)Hz\s*$/) {
            $r{$o}="$x:$m\@$1($c)";
	    last;
	  }
	}
      }
    }
    for $o (sort keys %r) {
      $r .= "  $o: $r{$o}";
    }
    close P;
  } else {
    $expect="success" if $expect="";
    $r="failed";
  }
  # Verify
  if ($expect ne "") {
    print "->$r\n";
    if ($r eq "  $expect") {
      print "->  ok\n\n";
    } else {
      print "\n$out";
      print "\n->  FAILED: Test # $name:\n\n";
      print "    $xrandr --verbose $args\n\n";
      if ($fixes{$name}) {
        print "\nThere are known issues with some packages regarding this test.\n";
	print "Please verify that you have at least the following git versions\n";
	print "before reporting a bug to xorg-devel:\n\n";
	print "    $fixes{$name}\n\n";
      }
      exit 1;
    }
    eval $inbetween;
  } else {
    print "->  ignored\n\n";
  }
}


# Test cases
#
# The tests are carefully designed to test certain transitions between
# RandR states that can only be reached by certain calling sequences.
# So be careful with altering them. For additional tests, better add them
# to the end of already existing tests of one part.

# Part 1: Single output switching tests (except for trivial explicit --crtc)
t ("p",   "",                        "$oa --off $ob --off");
t ("s1",  "A: $sa1(0)",              "$oa $ma1 --crtc 0");
t ("s2",  "A: $sa1(0)  B: $sab(1)",  "$ob $mab");
# TODO: should be A: $sab(1) someday (auto re-cloning)"
#t ("s3",  "A: $sab(1)  B: $sab(1)",  "$oa $mab");
t ("s3",  "A: $sab(0)  B: $sab(1)",  "$oa $mab --crtc 0");
t ("p4",  "A: $sab(1)  B: $sab(1)",  "$oa $mab --crtc 1 $ob --crtc 1");
t ("s4",  "A: $sa2(0)  B: $sab(1)",  "$oa $ma2");
t ("s5",  "A: $sa1(0)  B: $sab(1)",  "$oa $ma1");
t ("s6",  "A: $sa1(0)  B: $sb1(1)",  "$ob $mb1");
t ("s7",  "A: $sab(0)  B: $sb1(1)",  "$oa $mab");
t ("s8",  "A: $sab(0)  B: $sb2(1)",  "$ob $mb2");
t ("s9",  "A: $sab(0)  B: $sb1(1)",  "$ob $mb1");
# TODO: should be B: $sab(0) someday (auto re-cloning)"
#t ("s10", "A: $sab(0)  B: $sab(0)",  "$ob $mab");
t ("p11", "A: $sab(0)  B: $sab(0)",  "$oa --crtc 0 $ob $mab --crtc 0");
t ("s11", "A: $sa1(1)  B: $sab(0)",  "$oa $ma1");
t ("s12", "A: $sa1(1)  B: $sb1(0)",  "$ob $mb1");
t ("s13", "A: $sa1(1)  B: $sab(0)",  "$ob $mab");
t ("s14", "A: $sa2(1)  B: $sab(0)",  "$oa $ma2");
t ("s15", "A: $sa1(1)  B: $sab(0)",  "$oa $ma1");
t ("p16", "A: $sab(0)  B: $sab(0)",  "$oa $mab --crtc 0 $ob --crtc 0");
t ("s16", "A: $sab(1)  B: $sab(0)",  "$oa --pos 10x0");
t ("p17", "A: $sab(0)  B: $sab(0)",  "$oa --crtc 0 $ob --crtc 0");
t ("s17", "A: $sab(0)  B: $sab(1)",  "$ob --pos 10x0");
t ("p18", "A: $sab(0)  B: $sab(0)",  "$oa --crtc 0 $ob --crtc 0");
# TODO: s18-s19 are known to fail
t ("s18", "A: $sab(1)  B: $sab(0)",  "$oa --crtc 1");
t ("p19", "A: $sab(1)  B: $sab(1)",  "$oa --crtc 1 $ob --crtc 1");
t ("s19", "A: $sab(0)  B: $sab(1)",  "$oa --pos 10x0");

# Part 2: Complex dual output switching tests
# TODO: d1 is known to fail
t ("pd1", "A: $sab(0)",              "$oa --crtc 0 $ob --off");
t ("d1",  "B: $sab(0)",              "$oa --off $ob $mab");

# Done

print "All tests succeeded.\n";

exit 0;

