# -*-cperl-*-
# Please see the .pod files for documentation. This module is copyrighted
# as per the usual perl legalese:
# Copyright (c) 1997 Austin Schutz.
# expect() interface & functionality enhancements (c) 1999 Roland Giersig.
#
# All rights reserved. This program is free software; you can
# redistribute it and/or modify it under the same terms as Perl
# itself.
#
# Don't blame/flame me if you bust your stuff.
# Austin Schutz <ASchutz@users.sourceforge.net>
#
# This module now is maintained by
# Roland Giersig <RGiersig@cpan.org>
#

use 5.006;				# 4 won't cut it.

package Expect;

use IO::Pty 1.03;		# We need make_slave_controlling_terminal()
use IO::Tty;

use strict 'refs';
use strict 'vars';
use strict 'subs';
use POSIX qw(:sys_wait_h :unistd_h); # For WNOHANG and isatty
use Fcntl qw(:DEFAULT); # For checking file handle settings.
use Carp qw(cluck croak carp confess);
use IO::Handle ();
use Exporter ();
use Errno;

# This is necessary to make routines within Expect work.

@Expect::ISA = qw(IO::Pty Exporter);
@Expect::EXPORT = qw(expect exp_continue exp_continue_timeout);

BEGIN {
  $Expect::VERSION = '1.21';
  # These are defaults which may be changed per object, or set as
  # the user wishes.
  # This will be unset, since the default behavior differs between 
  # spawned processes and initialized filehandles.
  #  $Expect::Log_Stdout = 1;
  $Expect::Log_Group = 1;
  $Expect::Debug = 0;
  $Expect::Exp_Max_Accum = 0; # unlimited
  $Expect::Exp_Internal = 0;
  $Expect::IgnoreEintr = 0;
  $Expect::Manual_Stty = 0;
  $Expect::Multiline_Matching = 1;
  $Expect::Do_Soft_Close = 0;
  @Expect::Before_List = ();
  @Expect::After_List = ();
  %Expect::Spawned_PIDs = ();
}

sub version {
  my($version) = shift;
  warn "Version $version is later than $Expect::VERSION. It may not be supported" if (defined ($version) && ($version > $Expect::VERSION));

  die "Versions before 1.03 are not supported in this release" if ((defined ($version)) && ($version < 1.03));
  return $Expect::VERSION;
}

sub new {

  my ($class) = shift;
  $class = ref($class) if ref($class); # so we can be called as $exp->new()

  # Create the pty which we will use to pass process info.
  my($self) = new IO::Pty;
  die "$class: Could not assign a pty" unless $self;
  bless $self => $class;
  $self->autoflush(1);

  # This is defined here since the default is different for
  # initialized handles as opposed to spawned processes.
  ${*$self}{exp_Log_Stdout} = 1;
  $self->_init_vars();

  if (@_) {
    # we got add'l parms, so pass them to spawn
    return $self->spawn(@_);
  }
  return $self;
}

sub spawn {
  my ($class) = shift;
  my $self;

  if (ref($class)) {
    $self = $class;
  } else {
    $self = $class->new();
  }

  croak "Cannot reuse an object with an already spawned command"
    if exists ${*$self}{"exp_Command"};
  my(@cmd) = @_;	# spawn is passed command line args.
  ${*$self}{"exp_Command"} = \@cmd;

  # set up pipe to detect childs exec error
  pipe(FROM_CHILD, TO_PARENT) or die "Cannot open pipe: $!";
  pipe(FROM_PARENT, TO_CHILD) or die "Cannot open pipe: $!";
  TO_PARENT->autoflush(1);
  TO_CHILD->autoflush(1);
  eval {
    fcntl(TO_PARENT, Fcntl::F_SETFD, Fcntl::FD_CLOEXEC);
  };

  my $pid = fork;

  unless (defined ($pid)) {
    warn "Cannot fork: $!" if $^W;
    return undef;
  }

  if($pid) {
    # parent
    my $errno;
    ${*$self}{exp_Pid} = $pid;
    close TO_PARENT;
    close FROM_PARENT;
    $self->close_slave();
    $self->set_raw() if $self->raw_pty and isatty($self);
    close TO_CHILD; # so child gets EOF and can go ahead

    # now wait for child exec (eof due to close-on-exit) or exec error
    my $errstatus = sysread(FROM_CHILD, $errno, 256);
    die "Cannot sync with child: $!" if not defined $errstatus;
    close FROM_CHILD;
    if ($errstatus) {
      $! = $errno+0;
      warn "Cannot exec(@cmd): $!\n" if $^W;
      return undef;
    }
  }
  else {
    # child
    close FROM_CHILD;
    close TO_CHILD;

    $self->make_slave_controlling_terminal();
    my $slv = $self->slave()
      or die "Cannot get slave: $!";

    $slv->set_raw() if $self->raw_pty;
    close($self);

    # wait for parent before we detach
    my $buffer;
    my $errstatus = sysread(FROM_PARENT, $buffer, 256);
    die "Cannot sync with parent: $!" if not defined $errstatus;
    close FROM_PARENT;

    close(STDIN);
    open(STDIN,"<&". $slv->fileno())
      or die "Couldn't reopen STDIN for reading, $!\n";
    close(STDOUT);
    open(STDOUT,">&". $slv->fileno())
      or die "Couldn't reopen STDOUT for writing, $!\n";
    close(STDERR);
    open(STDERR,">&". $slv->fileno())
      or die "Couldn't reopen STDERR for writing, $!\n";

    { exec(@cmd) };
    print TO_PARENT $!+0;
    die "Cannot exec(@cmd): $!\n";
  }

  # This is sort of for code compatibility, and to make debugging a little
  # easier. By code compatibility I mean that previously the process's
  # handle was referenced by $process{Pty_Handle} instead of just $process.
  # This is almost like 'naming' the handle to the process.
  # I think this also reflects Tcl Expect-like behavior.
  ${*$self}{exp_Pty_Handle} = "spawn id(".$self->fileno().")";
  if ((${*$self}{"exp_Debug"}) or (${*$self}{"exp_Exp_Internal"})) {
    cluck("Spawned '@cmd'\r\n",
	  "\t${*$self}{exp_Pty_Handle}\r\n",
	  "\tPid: ${*$self}{exp_Pid}\r\n",
	  "\tTty: ".$self->SUPER::ttyname()."\r\n",
	 );
  }
  $Expect::Spawned_PIDs{${*$self}{exp_Pid}} = undef;
  return $self;
}


sub exp_init {
  # take a filehandle, for use later with expect() or interconnect() .
  # All the functions are written for reading from a tty, so if the naming
  # scheme looks odd, that's why.
  my ($class) = shift;
  my($self) = shift;
  bless $self, $class;
  croak "exp_init not passed a file object, stopped"
    unless defined($self->fileno());
  $self->autoflush(1);
  # Define standard variables.. debug states, etc.
  $self->_init_vars();
  # Turn of logging. By default we don't want crap from a file to get spewed
  # on screen as we read it.
  ${*$self}{exp_Log_Stdout} = 0;
  ${*$self}{exp_Pty_Handle} = "handle id(".$self->fileno().")";
  ${*$self}{exp_Pty_Handle} = "STDIN" if $self->fileno() == fileno (STDIN);
  print STDERR "Initialized ${*$self}{exp_Pty_Handle}.'\r\n" 
    if ${*$self}{"exp_Debug"};
  return $self;
}

# make an alias
*init = \&exp_init;

######################################################################
# We're happy OOP people. No direct access to stuff.
# For standard read-writeable parameters, we define some autoload magic...
my %Writeable_Vars = ( debug            => 'exp_Debug',
		       exp_internal     => 'exp_Exp_Internal',
		       do_soft_close    => 'exp_Do_Soft_Close',
		       max_accum        => 'exp_Max_Accum',
		       match_max        => 'exp_Max_Accum',
		       notransfer       => 'exp_NoTransfer',
		       log_stdout       => 'exp_Log_Stdout',
		       log_user         => 'exp_Log_Stdout',
		       log_group        => 'exp_Log_Group',
		       manual_stty      => 'exp_Manual_Stty',
		       restart_timeout_upon_receive => 'exp_Continue',
		       raw_pty           => 'exp_Raw_Pty',
		     );
my %Readable_Vars = ( pid               => 'exp_Pid',
		      exp_pid           => 'exp_Pid',
		      exp_match_number  => 'exp_Match_Number',
		      match_number      => 'exp_Match_Number',
		      exp_error         => 'exp_Error',
		      error             => 'exp_Error',
		      exp_command       => 'exp_Command',
		      command           => 'exp_Command',
		      exp_match         => 'exp_Match',
		      match             => 'exp_Match',
		      exp_matchlist     => 'exp_Matchlist',
		      matchlist         => 'exp_Matchlist',
		      exp_before        => 'exp_Before',
		      before            => 'exp_Before',
		      exp_after         => 'exp_After',
		      after             => 'exp_After',
		      exp_exitstatus    => 'exp_Exit',
		      exitstatus        => 'exp_Exit',
		      exp_pty_handle    => 'exp_Pty_Handle',
		      pty_handle        => 'exp_Pty_Handle',
		      exp_logfile       => 'exp_Log_File',
		      logfile           => 'exp_Log_File',
		      %Writeable_Vars,
		    );

sub AUTOLOAD {
  my $self = shift;
  my $type = ref($self)
    or croak "$self is not an object";

  use vars qw($AUTOLOAD);
  my $name = $AUTOLOAD;
  $name =~ s/.*:://;		# strip fully-qualified portion

  unless (exists $Readable_Vars{$name}) {
    croak "ERROR: cannot find method `$name' in class $type";
  }
  my $varname = $Readable_Vars{$name};
  my $tmp;
  $tmp = ${*$self}{$varname} if exists ${*$self}{$varname};

  if (@_) {
    if (exists $Writeable_Vars{$name}) {
      my $ref = ref($tmp);
      if ($ref eq 'ARRAY') {
	${*$self}{$varname} = [ @_ ];
      } elsif ($ref eq 'HASH') {
	${*$self}{$varname} = { @_ };
      } else {
	${*$self}{$varname} = shift;
      }
    } else {
      carp "Trying to set read-only variable `$name'"
	if $^W;
    }
  }

  my $ref = ref($tmp);
  return (wantarray? @{$tmp} : $tmp) if ($ref eq 'ARRAY');
  return (wantarray? %{$tmp} : $tmp) if ($ref eq 'HASH');
  return $tmp;
}

######################################################################

sub set_seq {
  # Set an escape sequence/function combo for a read handle for interconnect.
  # Ex: $read_handle->set_seq('',\&function,\@parameters); 
  my($self) = shift;
  my($escape_sequence,$function) = (shift,shift);
  ${${*$self}{exp_Function}}{$escape_sequence} = $function;
  if ((!defined($function)) ||($function eq 'undef')) {
    ${${*$self}{exp_Function}}{$escape_sequence} = \&_undef;
  }
  ${${*$self}{exp_Parameters}}{$escape_sequence} = shift;
  # This'll be a joy to execute. :)
  if ( ${*$self}{"exp_Debug"} ) {
    print STDERR "Escape seq. '" . $escape_sequence;
    print STDERR "' function for ${*$self}{exp_Pty_Handle} set to '";
    print STDERR ${${*$self}{exp_Function}}{$escape_sequence};
    print STDERR "(" . join(',', @_) . ")'\r\n";
  }
}

sub set_group {
  my($self) = shift;
  my($write_handle);
  # Make sure we can read from the read handle
  if (! defined($_[0])) {
    if (defined (${*$self}{exp_Listen_Group})) {
      return @{${*$self}{exp_Listen_Group}};
    } else {
      # Refrain from referencing an undef
      return undef;
    }
  }
  @{${*$self}{exp_Listen_Group}} = ();
  if ($self->_get_mode() !~ 'r') {
    warn("Attempting to set a handle group on ${*$self}{exp_Pty_Handle}, ",
	 "a non-readable handle!\r\n");
  }
  while ($write_handle = shift) {
    if ($write_handle->_get_mode() !~ 'w') {
      warn("Attempting to set a non-writeable listen handle ",
	   "${*$write_handle}{exp_Pty_handle} for ",
	   "${*$self}{exp_Pty_Handle}!\r\n");
    }
    push (@{${*$self}{exp_Listen_Group}},$write_handle);
  }
}

sub log_file {
    my $self = shift;

    return(${*$self}{exp_Log_File})
      if not @_;  # we got no param, return filehandle

    my $file = shift;
    my $mode = shift || "a";

    if (${*$self}{exp_Log_File} and ref(${*$self}{exp_Log_File}) ne 'CODE') {
      close(${*$self}{exp_Log_File});
    }
    ${*$self}{exp_Log_File} = undef;
    return if (not $file);
    my $fh = $file;
    if (not ref($file)) {
      # it's a filename
      $fh = new IO::File $file, $mode
	or croak "Cannot open logfile $file: $!";
    }
    if (ref($file) ne 'CODE') {
      croak "Given logfile doesn't have a 'print' method"
	if not $fh->can("print");
      $fh->autoflush(1);		# so logfile is up to date
    }

    ${*$self}{exp_Log_File} = $fh;
}


# I'm going to leave this here in case I might need to change something.
# Previously this was calling `stty`, in a most bastardized manner.
sub exp_stty {
  my($self) = shift;
  my($mode) = "@_";
  
  return undef unless defined($mode);
  if (not defined $INC{"IO/Stty.pm"}) {
    carp "IO::Stty not installed, cannot change mode";
    return undef;
  }

  if (${*$self}{"exp_Debug"}) {
    print STDERR "Setting ${*$self}{exp_Pty_Handle} to tty mode '$mode'\r\n";
  }
  unless (POSIX::isatty($self)) {
    if (${*$self}{"exp_Debug"} or $^W) {
      warn "${*$self}{exp_Pty_Handle} is not a tty. Not changing mode";
    }
    return '';			# No undef to avoid warnings elsewhere.
  }
  IO::Stty::stty($self, split(/\s/,$mode));
}

*stty = \&exp_stty;

# If we want to clear the buffer. Otherwise Accum will grow during send_slow
# etc. and contain the remainder after matches.
sub clear_accum {
  my ($self) = shift;
  my ($temp) = (${*$self}{exp_Accum});
  ${*$self}{exp_Accum} = '';
  # return the contents of the accumulator.
  return $temp;
}

sub set_accum {
  my ($self) = shift;
  my ($temp) = (${*$self}{exp_Accum});
  ${*$self}{exp_Accum} = shift;
  # return the contents of the accumulator.
  return $temp;
}

######################################################################
# define constants for pattern subs
sub exp_continue() { "exp_continue" }
sub exp_continue_timeout() { "exp_continue_timeout" }

######################################################################
# Expect on multiple objects at once.
#
# Call as Expect::expect($timeout, -i => \@exp_list, @patternlist,
#                       -i => $exp, @pattern_list, ...);
# or $exp->expect($timeout, @patternlist, -i => \@exp_list, @patternlist,
#                 -i => $exp, @pattern_list, ...);
#
# Patterns are arrays that consist of
#   [ $pattern_type, $pattern, $sub, @subparms ]
#
#   Optional $pattern_type is '-re' (RegExp, default) or '-ex' (exact);
#
#   $sub is optional CODE ref, which is called as &{$sub}($exp, @subparms)
#     if pattern matched; may return exp_continue or exp_continue_timeout.
#
# Old-style syntax (pure pattern strings with optional type)  also supported.
#

sub expect {
  my $self;
  print STDERR ("expect(@_) called...\n") if $Expect::Debug;
  if (defined($_[0])) {
    if (ref($_[0]) and $_[0]->isa('Expect')) {
      $self = shift;
    } elsif ($_[0] eq 'Expect') {
      shift;	# or as Expect->expect
    }
  }
  croak "expect(): not enough arguments, should be expect(timeout, [patterns...])" if @_ < 1;
  my $timeout = shift;
  my $timeout_hook = undef;

  my @object_list;
  my %patterns;

  my @pattern_list;
  my @timeout_list;
  my $curr_list;

  if ($self) {
    $curr_list = [$self];
  } else {
    # called directly, so first parameter must be '-i' to establish
    # object list.
    $curr_list = [];
    croak "expect(): ERROR: if called directly (not as \$obj->expect(...), but as Expect::expect(...), first parameter MUST be '-i' to set an object (list) for the patterns to work on."
      if ($_[0] ne '-i');
  }
  # Let's make a list of patterns wanting to be evaled as regexps.
  my $parm;
  my $parm_nr = 1;
  while (defined($parm = shift)) {
    print STDERR ("expect(): handling param '$parm'...\n") if $Expect::Debug;
    if (ref($parm)) {
      if (ref($parm) eq 'ARRAY') {
	my $err = _add_patterns_to_list(\@pattern_list, \@timeout_list,
					$parm_nr, $parm);
	carp ("expect(): Warning: multiple `timeout' patterns (",
	      scalar(@timeout_list), ").\r\n")
	  if @timeout_list > 1;
	$timeout_hook = $timeout_list[-1] if $timeout_list[-1];
	croak $err if $err;
	$parm_nr++;
      } else {
	croak ("expect(): Unknown pattern ref $parm");
      }
    } else {
      # not a ref, is an option or raw pattern
      if (substr($parm, 0, 1) eq '-') {
	# it's an option
	print STDERR ("expect(): handling option '$parm'...\n")
	  if $Expect::Debug;
	if ($parm eq '-i') {
	  # first add collected patterns to object list
	  if (scalar(@$curr_list)) {
	    push @object_list, $curr_list if not exists $patterns{"$curr_list"};
	    push @{$patterns{"$curr_list"}}, @pattern_list;
	    @pattern_list = ();
	  }
	  # now put parm(s) into current object list
	  if (ref($_[0]) eq 'ARRAY') {
	    $curr_list = shift;
	  } else {
	    $curr_list = [ shift ];
	  }
	} elsif ($parm eq '-re'
		 or $parm eq '-ex') {
	  if (ref($_[1]) eq 'CODE') {
	    push @pattern_list, [ $parm_nr, $parm, shift, shift ];
	  } else {
	    push @pattern_list, [ $parm_nr, $parm, shift, undef ];
	  }
	  $parm_nr++;
	} else {
	  croak ("Unknown option $parm");
	}
      } else {
	# a plain pattern, check if it is followed by a CODE ref
	if (ref($_[0]) eq 'CODE') {
	  if ($parm eq 'timeout') {
	    push @timeout_list, shift;
	    carp ("expect(): Warning: multiple `timeout' patterns (",
		  scalar(@timeout_list), ").\r\n")
	      if @timeout_list > 1;
	    $timeout_hook = $timeout_list[-1] if $timeout_list[-1];
	  } elsif ($parm eq 'eof') {
	    push @pattern_list, [ $parm_nr, "-$parm", undef, shift ];
	  } else {
	    push @pattern_list, [ $parm_nr, '-ex', $parm, shift ];
	  }
	} else {
	  print STDERR ("expect(): exact match '$parm'...\n")
	    if $Expect::Debug;
	  push @pattern_list, [ $parm_nr, '-ex', $parm, undef ];
	}
	$parm_nr++;
      }
    }
  }

  # add rest of collected patterns to object list
  carp "expect(): Empty object list" unless $curr_list;
  push @object_list, $curr_list if not exists $patterns{"$curr_list"};
  push @{$patterns{"$curr_list"}}, @pattern_list;

  my $debug = $self ? ${*$self}{exp_Debug} : $Expect::Debug;
  my $internal = $self ? ${*$self}{exp_Exp_Internal} : $Expect::Exp_Internal;

  # now start matching...

  if (@Expect::Before_List) {
    print STDERR ("Starting BEFORE pattern matching...\r\n")
      if ($debug or $internal);
    _multi_expect(0, undef, @Expect::Before_List);
  }

  cluck ("Starting EXPECT pattern matching...\r\n")
    if ($debug or $internal);
  my @ret;
  @ret = _multi_expect($timeout, $timeout_hook,
		       map { [$_, @{$patterns{"$_"}}] } @object_list);

  if (@Expect::After_List) {
    print STDERR ("Starting AFTER pattern matching...\r\n")
      if ($debug or $internal);
    _multi_expect(0, undef, @Expect::After_List);
  }

  wantarray ? @ret : $ret[0];
}

######################################################################
# the real workhorse
#
sub _multi_expect($$@) {
  my $timeout = shift;
  my $timeout_hook = shift;

  if ($timeout_hook) {
    croak "Unknown timeout_hook type $timeout_hook"
      unless (ref($timeout_hook) eq 'CODE' 
	      or ref($timeout_hook) eq 'ARRAY');
  }

  foreach my $pat (@_) {
    my @patterns = @{$pat}[1..$#{$pat}];
    foreach my $exp (@{$pat->[0]}) {
      ${*$exp}{exp_New_Data} = 1; # first round we always try to match
      if (exists ${*$exp}{"exp_Max_Accum"} and ${*$exp}{"exp_Max_Accum"}) {
	${*$exp}{exp_Accum} =
	  $exp->_trim_length(${*$exp}{exp_Accum},
			     ${*$exp}{exp_Max_Accum});
      }
      print STDERR ("${*$exp}{exp_Pty_Handle}: beginning expect.\r\n",
		    "\tTimeout: ",
		    (defined($timeout) ? $timeout : "unlimited" ),
		    " seconds.\r\n",
		    "\tCurrent time: ". localtime(). "\r\n",
		   ) if $Expect::Debug;

      # What are we expecting? What do you expect? :-)
      if (${*$exp}{exp_Exp_Internal}) {
	print STDERR "${*$exp}{exp_Pty_Handle}: list of patterns:\r\n";
	foreach my $pattern (@patterns) {
	  print STDERR ('  ',
			defined($pattern->[0])?
			'#'. $pattern->[0].': ' :
			'',
			$pattern->[1],
			" `", _make_readable($pattern->[2]),
			"'\r\n");
	}
	print STDERR "\r\n";
      }
    }
  }

  my $successful_pattern;
  my $exp_matched;
  my $err;
  my $before;
  my $after;
  my $match;
  my @matchlist;

  # Set the last loop time to now for time comparisons at end of loop.
  my $start_loop_time = time();
  my $exp_cont = 1;

 READLOOP:
  while ($exp_cont) {
    $exp_cont = 1;
    $err = "";
    my $rmask = '';
    my $time_left = undef;
    if (defined $timeout) {
      $time_left = $timeout - (time() - $start_loop_time);
      $time_left = 0 if $time_left < 0;
    }

    $exp_matched = undef;
    # Test for a match first so we can test the current Accum w/out 
    # worrying about an EOF.

    foreach my $pat (@_) {
      my @patterns = @{$pat}[1..$#{$pat}];
      foreach my $exp (@{$pat->[0]}) {
	# build mask for select in next section...
	my $fn = $exp->fileno();
	vec($rmask, $fn, 1) = 1 if defined $fn;

	next unless ${*$exp}{exp_New_Data};

	# clear error status
	${*$exp}{exp_Error} = undef;

	# This could be huge. We should attempt to do something
	# about this.  Because the output is used for debugging
	# I'm of the opinion that showing smaller amounts if the
	# total is huge should be ok.
	# Thus the 'trim_length'
	print STDERR ("\r\n${*$exp}{exp_Pty_Handle}: Does `",
		      $exp->_trim_length(_make_readable(${*$exp}{exp_Accum})),
		      "'\r\nmatch:\r\n")
	  if ${*$exp}{exp_Exp_Internal};

	# we don't keep the parameter number anymore
	# (clashes with before & after), instead the parameter number is
	# stored inside the pattern; we keep the pattern ref
	# and look up the number later.
	foreach my $pattern (@patterns) {
	  print STDERR ("  pattern",
			defined($pattern->[0])? ' #' . $pattern->[0] : '',
			": ", $pattern->[1],
			" `", _make_readable($pattern->[2]),
			"'? ")
	    if (${*$exp}{exp_Exp_Internal});

	  # Matching exactly
	  if ($pattern->[1] eq '-ex') {
	    my $match_index = index(${*$exp}{exp_Accum},
				    $pattern->[2]);

	    # We matched if $match_index > -1
	    if ($match_index > -1) {
	      $before = substr(${*$exp}{exp_Accum}, 0, $match_index);
	      $match  = substr(${*$exp}{exp_Accum}, $match_index,
			       length($pattern->[2]));
	      $after  = substr(${*$exp}{exp_Accum},
			       $match_index + length($pattern->[2])) ;
	      ${*$exp}{exp_Before} = $before;
	      ${*$exp}{exp_Match} = $match;
	      ${*$exp}{exp_After} = $after;
	      ${*$exp}{exp_Match_Number} = $pattern->[0];
	      $exp_matched = $exp;
	    }
	  } elsif ($pattern->[1] eq '-re') {
	    # m// in array context promises to return an empty list
	    # but doesn't if the pattern doesn't contain brackets (),
	    # so we kludge around by adding an empty bracket
	    # at the end.

	    if ($Expect::Multiline_Matching) {
	      @matchlist = (${*$exp}{exp_Accum}
			    =~ m/$pattern->[2]()/m);
	      ($match, $before, $after) = ($&, $`, $');
	    } else {
	      @matchlist = (${*$exp}{exp_Accum}
			    =~ m/$pattern->[2]()/);
	      ($match, $before, $after) = ($&, $`, $');
	    }
	    if (@matchlist) {
	      # Matching regexp
	      ${*$exp}{exp_Before} = $before;
	      ${*$exp}{exp_Match}  = $match;
	      ${*$exp}{exp_After}  = $after;
	      pop @matchlist;	# remove kludged empty bracket from end
	      @{${*$exp}{exp_Matchlist}} = @matchlist;
	      ${*$exp}{exp_Match_Number} = $pattern->[0];
	      $exp_matched = $exp;
	    }
	  } else {
	    # 'timeout' or 'eof'
	  }

	  if ($exp_matched) {
	    ${*$exp}{exp_Accum} = $after
	      unless ${*$exp}{exp_NoTransfer};
	    print STDERR "YES!!\r\n"
	      if ${*$exp}{exp_Exp_Internal};
	    print STDERR ("    Before match string: `",
			  $exp->_trim_length(_make_readable(($before))),
			  "'\r\n",
			  "    Match string: `", _make_readable($match),
			  "'\r\n",
			  "    After match string: `",
			  $exp->_trim_length(_make_readable(($after))),
			  "'\r\n",
			  "    Matchlist: (",
			  join(",  ",
			       map { "`".$exp->_trim_length(_make_readable(($_)))."'"
				   } @matchlist,
			       ),
			  ")\r\n",
			 ) if (${*$exp}{exp_Exp_Internal});

	    # call hook function if defined
	    if ($pattern->[3]) {
	      print STDERR ("Calling hook $pattern->[3]...\r\n",
			   ) if (${*$exp}{exp_Exp_Internal} or $Expect::Debug);
	      if ($#{$pattern} > 3) {
		# call with parameters if given
		$exp_cont = &{$pattern->[3]}($exp,
					     @{$pattern}[4..$#{$pattern}]);
	      } else {
		$exp_cont = &{$pattern->[3]}($exp);
	      }
	    }
	    if ($exp_cont and $exp_cont eq exp_continue) {
	      print STDERR ("Continuing expect, restarting timeout...\r\n")
		if (${*$exp}{exp_Exp_Internal} or $Expect::Debug);
	      $start_loop_time = time(); # restart timeout count
	      next READLOOP;
	    } elsif ($exp_cont and $exp_cont eq exp_continue_timeout) {
	      print STDERR ("Continuing expect...\r\n")
		if (${*$exp}{exp_Exp_Internal} or $Expect::Debug);
	      next READLOOP;
	    }
	    last READLOOP;
	  }
	  print STDERR "No.\r\n" if ${*$exp}{exp_Exp_Internal};
	}
	print STDERR "\r\n" if ${*$exp}{exp_Exp_Internal};
	# don't have to match again until we get new data
	${*$exp}{exp_New_Data} = 0;
      }
    } # End of matching section

    # No match, let's see what is pending on the filehandles...
    print STDERR ("Waiting for new data (",
		  defined($time_left)? $time_left : 'unlimited',
		  " seconds)...\r\n",
		 ) if ($Expect::Exp_Internal or $Expect::Debug);
    my $nfound;
  SELECT: {
      $nfound = select($rmask, undef, undef, $time_left);
      if ($nfound < 0) {
	if ($!{EINTR} and $Expect::IgnoreEintr) {
	  print STDERR ("ignoring EINTR, restarting select()...\r\n")
	    if ($Expect::Exp_Internal or $Expect::Debug);
	  next SELECT;
	}
	print STDERR ("select() returned error code '$!'\r\n")
	  if ($Expect::Exp_Internal or $Expect::Debug);
	# returned error
	$err = "4:$!";
	last READLOOP;
      }
    }
    # go until we don't find something (== timeout).
    if ($nfound == 0) {
      # No pattern, no EOF. Did we time out?
      $err = "1:TIMEOUT";
      foreach my $pat (@_) {
	foreach my $exp (@{$pat->[0]}) {
	  $before = ${*$exp}{exp_Before} = ${*$exp}{exp_Accum};
	  next if not defined $exp->fileno(); # skip already closed
	  ${*$exp}{exp_Error} = $err unless ${*$exp}{exp_Error};
	}
      }
      print STDERR ("TIMEOUT\r\n")
	if ($Expect::Debug or $Expect::Exp_Internal);
      if ($timeout_hook) {
	my $ret;
	print STDERR ("Calling timeout function $timeout_hook...\r\n")
	  if ($Expect::Debug or $Expect::Exp_Internal);
	if (ref($timeout_hook) eq 'CODE') {
	  $ret = &{$timeout_hook}($_[0]->[0]);
	} else {
	  if ($#{$timeout_hook} > 3) {
	    $ret = &{$timeout_hook->[3]}($_[0]->[0],
					 @{$timeout_hook}[4..$#{$timeout_hook}]);
	  } else {
	    $ret = &{$timeout_hook->[3]}($_[0]->[0]);
	  }
	}
	if ($ret and $ret eq exp_continue) {
	  $start_loop_time = time();	# restart timeout count
	  next READLOOP;
	}
      }
      last READLOOP;
    }

    my @bits = split(//,unpack('b*',$rmask));
    foreach my $pat (@_) {
      foreach my $exp (@{$pat->[0]}) {
	next if not defined $exp->fileno(); # skip already closed
	if ($bits[$exp->fileno()]) {
	  print STDERR ("${*$exp}{exp_Pty_Handle}: new data.\r\n")
	    if $Expect::Debug;
	  # read in what we found.
	  my $buffer;
	  my $nread = sysread($exp, $buffer, 2048);

	  # Make errors (nread undef) show up as EOF.
	  $nread = 0 unless defined ($nread);

	  if ($nread == 0) {
	    print STDERR ("${*$exp}{exp_Pty_Handle}: EOF\r\n")
	      if ($Expect::Debug);
	    $before = ${*$exp}{exp_Before} = $exp->clear_accum();
	    $err = "2:EOF";
	    ${*$exp}{exp_Error} = $err;
	    ${*$exp}{exp_Has_EOF} = 1;
	    $exp_cont = undef;
	    foreach my $eof_pat (grep {$_->[1] eq '-eof'} @{$pat}[1..$#{$pat}]) {
	      my $ret;
	      print STDERR ("Calling EOF hook $eof_pat->[3]...\r\n",
			   ) if ($Expect::Debug);
	      if ($#{$eof_pat} > 3) {
		# call with parameters if given
		$ret = &{$eof_pat->[3]}($exp,
					@{$eof_pat}[4..$#{$eof_pat}]);
	      } else {
		$ret = &{$eof_pat->[3]}($exp);
	      }
	      if ($ret and
		  ($ret eq exp_continue
		   or $ret eq exp_continue_timeout)) {
		    $exp_cont = $ret;
	      }
	    }
	    # is it dead?
	    if (defined(${*$exp}{exp_Pid})) {
	      my $ret = waitpid(${*$exp}{exp_Pid}, POSIX::WNOHANG);
	      if ($ret == ${*$exp}{exp_Pid}) {
		printf STDERR ("%s: exit(0x%02X)\r\n", 
			       ${*$exp}{exp_Pty_Handle}, $?)
		  if ($Expect::Debug);
		$err = "3:Child PID ${*$exp}{exp_Pid} exited with status $?";
		${*$exp}{exp_Error} = $err;
		${*$exp}{exp_Exit} = $?;
		delete $Expect::Spawned_PIDs{${*$exp}{exp_Pid}};
		${*$exp}{exp_Pid} = undef;
	      }
	    }
	    print STDERR ("${*$exp}{exp_Pty_Handle}: closing...\r\n")
	      if ($Expect::Debug);
	    $exp->hard_close();
	    next;
	  }
	  print STDERR ("${*$exp}{exp_Pty_Handle}: read $nread byte(s).\r\n")
	    if ($Expect::Debug);

	  # ugly hack for broken solaris ttys that spew <blank><backspace>
	  # into our pretty output
	  $buffer =~ s/ \cH//g if not ${*$exp}{exp_Raw_Pty};
	  # Append it to the accumulator.
	  ${*$exp}{exp_Accum} .= $buffer;
	  if (exists ${*$exp}{exp_Max_Accum}
	      and ${*$exp}{exp_Max_Accum}) {
	    ${*$exp}{exp_Accum} =
	      $exp->_trim_length(${*$exp}{exp_Accum},
				 ${*$exp}{exp_Max_Accum});
	  }
	  ${*$exp}{exp_New_Data} = 1; # next round we try to match again

	  $exp_cont = exp_continue
	    if (exists ${*$exp}{exp_Continue} and ${*$exp}{exp_Continue});
	  # Now propagate what we have read to other listeners...
	  $exp->_print_handles($buffer);

	  # End handle reading section.
	}
      }
    }				# end read loop
    $start_loop_time = time()	# restart timeout count
      if ($exp_cont and $exp_cont eq exp_continue);
  }
  # End READLOOP

  # Post loop. Do we have anything?
  # Tell us status
  if ($Expect::Debug or $Expect::Exp_Internal) {
      if ($exp_matched) {
	  print STDERR ("Returning from expect ",
			${*$exp_matched}{exp_Error} ? 'un' : '',
			"successfully.",
			${*$exp_matched}{exp_Error} ?
			"\r\n  Error: ${*$exp_matched}{exp_Error}." : '',
			"\r\n");
      } else {
	  print STDERR ("Returning from expect with TIMEOUT or EOF\r\n");
      }
    if ($Expect::Debug and $exp_matched) {
      print STDERR "  ${*$exp_matched}{exp_Pty_Handle}: accumulator: `";
      if (${*$exp_matched}{exp_Error}) {
	print STDERR ($exp_matched->_trim_length
		      (_make_readable(${*$exp_matched}{exp_Before})),
		      "'\r\n");
      } else {
	print STDERR ($exp_matched->_trim_length
		      (_make_readable(${*$exp_matched}{exp_Accum})),
		      "'\r\n");
      }
    }
  }

  if ($exp_matched) {
    return wantarray?
      (${*$exp_matched}{exp_Match_Number},
       ${*$exp_matched}{exp_Error},
       ${*$exp_matched}{exp_Match},
       ${*$exp_matched}{exp_Before},
       ${*$exp_matched}{exp_After},
       $exp_matched,
      ) :
	${*$exp_matched}{exp_Match_Number};
  }

  return wantarray? (undef, $err, undef, $before, undef, undef) : undef;
}


# Patterns are arrays that consist of
# [ $pattern_type, $pattern, $sub, @subparms ]
# optional $pattern_type is '-re' (RegExp, default) or '-ex' (exact);
# $sub is optional CODE ref, which is called as &{$sub}($exp, @subparms)
#   if pattern matched;
# the $parm_nr gets unshifted onto the array for reporting purposes.

sub _add_patterns_to_list($$$@) {
  my $listref = shift;
  my $timeoutlistref = shift;	# gets timeout patterns
  my $store_parm_nr = shift;
  my $parm_nr = $store_parm_nr || 1;
  foreach my $parm (@_) {
    if (not ref($parm) eq 'ARRAY') {
      return "Parameter #$parm_nr is not an ARRAY ref.";
    }
    $parm = [@$parm];		# make copy
    if ($parm->[0] =~ m/\A-/) {
      # it's an option
      if ($parm->[0] ne '-re'
	  and $parm->[0] ne '-ex') {
	return "Unknown option $parm->[0] in pattern #$parm_nr";
      }
    } else {
      if ($parm->[0] eq 'timeout') {
	if (defined $timeoutlistref) {
	  splice @$parm, 0, 1, ("-$parm->[0]", undef);
	  unshift @$parm, $store_parm_nr? $parm_nr: undef;
	  push @$timeoutlistref, $parm;
	}
	next;
      } elsif ($parm->[0] eq 'eof') {
	splice @$parm, 0, 1, ("-$parm->[0]", undef);
      } else {
	unshift @$parm, '-re';	# defaults to RegExp
      }
    }
    if (@$parm > 2) {
      if (ref($parm->[2]) ne 'CODE') {
	croak ("Pattern #$parm_nr doesn't have a CODE reference",
	       "after the pattern.");
      }
    } else {
      push @$parm, undef;	# make sure we have three elements
    }

    unshift @$parm, $store_parm_nr? $parm_nr: undef;
    push @$listref, $parm;
    $parm_nr++
  }
  return undef;
}

######################################################################
# $process->interact([$in_handle],[$escape sequence])
# If you don't specify in_handle STDIN  will be used.
sub interact {
  my ($self) = (shift);
  my ($infile) = (shift);
  my ($escape_sequence) = shift;
  my ($in_object,$in_handle,@old_group,$return_value);
  my ($old_manual_stty_val,$old_log_stdout_val);
  my ($outfile,$out_object);
  @old_group = $self->set_group();
  # If the handle is STDIN we'll
  # $infile->fileno == 0 should be stdin.. follow stdin rules.
  no strict 'subs';		# Allow bare word 'STDIN'
  unless (defined($infile)) {
    # We need a handle object Associated with STDIN.
    $infile = new IO::File;
    $infile->IO::File::fdopen(STDIN,'r');
    $outfile = new IO::File;
    $outfile->IO::File::fdopen(STDOUT,'w');
  } elsif (fileno($infile) == fileno(STDIN)) {
    # With STDIN we want output to go to stdout.
    $outfile = new IO::File;
    $outfile->IO::File::fdopen(STDOUT,'w');
  } else {
    undef ($outfile);
  }
  # Here we assure ourselves we have an Expect object.
  $in_object = Expect->exp_init($infile);
  if (defined($outfile)) {
    # as above.. we want output to go to stdout if we're given stdin.
    $out_object = Expect->exp_init($outfile);
    $out_object->manual_stty(1);
    $self->set_group($out_object);
  } else {
    $self->set_group($in_object);
  }
  $in_object->set_group($self);
  $in_object->set_seq($escape_sequence,undef) if defined($escape_sequence);
  # interconnect normally sets stty -echo raw. Interact really sort
  # of implies we don't do that by default. If anyone wanted to they could
  # set it before calling interact, of use interconnect directly.
  $old_manual_stty_val = $self->manual_stty();
  $self->manual_stty(1);
  # I think this is right. Don't send stuff from in_obj to stdout by default.
  # in theory whatever 'self' is should echo what's going on.
  $old_log_stdout_val = $self->log_stdout();
  $self->log_stdout(0);
  $in_object->log_stdout(0);
  # Allow for the setting of an optional EOF escape function.
  #  $in_object->set_seq('EOF',undef);
  #  $self->set_seq('EOF',undef);
  Expect::interconnect($self,$in_object);
  $self->log_stdout($old_log_stdout_val);
  $self->set_group(@old_group);
  # If old_group was undef, make sure that occurs. This is a slight hack since
  # it modifies the value directly.
  # Normally an undef passed to set_group will return the current groups.
  # It is possible that it may be of worth to make it possible to undef
  # The current group without doing this.
  unless (@old_group) {
    @{${*$self}{exp_Listen_Group}} = ();
  }
  $self->manual_stty($old_manual_stty_val);
  return $return_value;
}

sub interconnect {

  #  my ($handle)=(shift); call as Expect::interconnect($spawn1,$spawn2,...)
  my ($rmask,$nfound,$nread);
  my ($rout, @bits, $emask, $eout, @ebits ) = ();
  my ($escape_sequence,$escape_character_buffer);
  my (@handles) = @_;
  my ($handle,$read_handle,$write_handle);
  my ($read_mask,$temp_mask) = ('','');

  # Get read/write handles
  foreach $handle (@handles) {
    $temp_mask = '';
    vec($temp_mask,$handle->fileno(),1) = 1;
    # Under Linux w/ 5.001 the next line comes up w/ 'Uninit var.'.
    # It appears to be impossible to make the warning go away.
    # doing something like $temp_mask='' unless defined ($temp_mask)
    # has no effect whatsoever. This may be a bug in 5.001.
    $read_mask = $read_mask | $temp_mask;
  }
  if ($Expect::Debug) {
    print STDERR "Read handles:\r\n";
    foreach $handle (@handles) {
      print STDERR "\tRead handle: ";
      print STDERR "'${*$handle}{exp_Pty_Handle}'\r\n";
      print STDERR "\t\tListen Handles:";
      foreach $write_handle (@{${*$handle}{exp_Listen_Group}}) {
	print STDERR " '${*$write_handle}{exp_Pty_Handle}'";
      }
      print STDERR ".\r\n";
    }
  }

  #  I think if we don't set raw/-echo here we may have trouble. We don't
  # want a bunch of echoing crap making all the handles jabber at each other.
  foreach $handle (@handles) {
    unless (${*$handle}{"exp_Manual_Stty"}) {
      # This is probably O/S specific.
      ${*$handle}{exp_Stored_Stty} = $handle->exp_stty('-g');
      print STDERR "Setting tty for ${*$handle}{exp_Pty_Handle} to 'raw -echo'.\r\n"if ${*$handle}{"exp_Debug"};
      $handle->exp_stty("raw -echo");
    }
    foreach $write_handle (@{${*$handle}{exp_Listen_Group}}) {
      unless (${*$write_handle}{"exp_Manual_Stty"}) {
	${*$write_handle}{exp_Stored_Stty} = $write_handle->exp_stty('-g');
	print STDERR "Setting ${*$write_handle}{exp_Pty_Handle} to 'raw -echo'.\r\n"if ${*$handle}{"exp_Debug"};
	$write_handle->exp_stty("raw -echo");
      }
    }
  }

  print STDERR "Attempting interconnection\r\n" if $Expect::Debug;

  # Wait until the process dies or we get EOF
  # In the case of !${*$handle}{exp_Pid} it means
  # the handle was exp_inited instead of spawned.
 CONNECT_LOOP:
  # Go until we have a reason to stop
  while (1) {
    # test each handle to see if it's still alive.
    foreach $read_handle (@handles) {
      waitpid(${*$read_handle}{exp_Pid}, WNOHANG)
	if (exists (${*$read_handle}{exp_Pid}) and ${*$read_handle}{exp_Pid});
      if (exists(${*$read_handle}{exp_Pid})
	  and (${*$read_handle}{exp_Pid})
	  and (! kill(0,${*$read_handle}{exp_Pid}))) {
	print STDERR "Got EOF (${*$read_handle}{exp_Pty_Handle} died) reading ${*$read_handle}{exp_Pty_Handle}\r\n"
	  if ${*$read_handle}{"exp_Debug"};
	last CONNECT_LOOP unless defined(${${*$read_handle}{exp_Function}}{"EOF"});
	last CONNECT_LOOP unless &{${${*$read_handle}{exp_Function}}{"EOF"}}(@{${${*$read_handle}{exp_Parameters}}{"EOF"}});
      }
    }

    # Every second? No, go until we get something from someone.
    ($nfound) = select($rout = $read_mask, undef, $eout = $emask, undef);
    # Is there anything to share?  May be -1 if interrupted by a signal...
    next CONNECT_LOOP if not defined $nfound or $nfound < 1;
    # Which handles have stuff?
    @bits = split(//,unpack('b*',$rout));
    $eout = 0 unless defined ($eout);
    @ebits = split(//,unpack('b*',$eout));
    #    print "Ebits: $eout\r\n";
    foreach $read_handle (@handles) {
      if ($bits[$read_handle->fileno()]) {
	$nread = sysread( $read_handle, ${*$read_handle}{exp_Pty_Buffer}, 1024 );
	# Appease perl -w
	$nread = 0 unless defined ($nread);
	print STDERR "interconnect: read $nread byte(s) from ${*$read_handle}{exp_Pty_Handle}.\r\n" if ${*$read_handle}{"exp_Debug"} > 1;
	# Test for escape seq. before printing.
	# Appease perl -w
	$escape_character_buffer = '' unless defined ($escape_character_buffer);
	$escape_character_buffer .= ${*$read_handle}{exp_Pty_Buffer};
	foreach $escape_sequence (keys(%{${*$read_handle}{exp_Function}})) {
	  print STDERR "Tested escape sequence $escape_sequence from ${*$read_handle}{exp_Pty_Handle}"if ${*$read_handle}{"exp_Debug"} > 1;
	  # Make sure it doesn't grow out of bounds.
	  $escape_character_buffer = $read_handle->_trim_length($escape_character_buffer,${*$read_handle}{"exp_Max_Accum"}) if (${*$read_handle}{"exp_Max_Accum"});
	  if ($escape_character_buffer =~ /($escape_sequence)/) {
	    if (${*$read_handle}{"exp_Debug"}) {
	      print STDERR "\r\ninterconnect got escape sequence from ${*$read_handle}{exp_Pty_Handle}.\r\n";
	      # I'm going to make the esc. seq. pretty because it will 
	      # probably contain unprintable characters.
	      print STDERR "\tEscape Sequence: '"._trim_length(undef,_make_readable($escape_sequence))."'\r\n";
	      print STDERR "\tMatched by string: '"._trim_length(undef,_make_readable($&))."'\r\n";
	    }
	    # Print out stuff before the escape.
	    # Keep in mind that the sequence may have been split up
	    # over several reads.
	    # Let's get rid of it from this read. If part of it was 
	    # in the last read there's not a lot we can do about it now.
	    if (${*$read_handle}{exp_Pty_Buffer} =~ /($escape_sequence)/) {
	      $read_handle->_print_handles($`);
	    } else {
	      $read_handle->_print_handles(${*$read_handle}{exp_Pty_Buffer})
	    }
	    # Clear the buffer so no more matches can be made and it will
	    # only be printed one time.
	    ${*$read_handle}{exp_Pty_Buffer} = '';
	    $escape_character_buffer = '';
	    # Do the function here. Must return non-zero to continue.
	    # More cool syntax. Maybe I should turn these in to objects.
	    last CONNECT_LOOP unless &{${${*$read_handle}{exp_Function}}{$escape_sequence}}(@{${${*$read_handle}{exp_Parameters}}{$escape_sequence}});
	  }
	}
	$nread = 0 unless defined($nread); # Appease perl -w?
	waitpid(${*$read_handle}{exp_Pid}, WNOHANG) if (defined (${*$read_handle}{exp_Pid}) &&${*$read_handle}{exp_Pid});
	if ($nread == 0) {
	  print STDERR "Got EOF reading ${*$read_handle}{exp_Pty_Handle}\r\n"if ${*$read_handle}{"exp_Debug"}; 
	  last CONNECT_LOOP unless defined(${${*$read_handle}{exp_Function}}{"EOF"});
	  last CONNECT_LOOP unless &{${${*$read_handle}{exp_Function}}{"EOF"}}(@{${${*$read_handle}{exp_Parameters}}{"EOF"}});
	}
	last CONNECT_LOOP if ($nread < 0); # This would be an error
	$read_handle->_print_handles(${*$read_handle}{exp_Pty_Buffer});
      }
      # I'm removing this because I haven't determined what causes exceptions
      # consistently.
      if (0)			#$ebits[$read_handle->fileno()])
	{
	  print STDERR "Got Exception reading ${*$read_handle}{exp_Pty_Handle}\r\n"if ${*$read_handle}{"exp_Debug"};
	  last CONNECT_LOOP unless defined(${${*$read_handle}{exp_Function}}{"EOF"});
	  last CONNECT_LOOP unless &{${${*$read_handle}{exp_Function}}{"EOF"}}(@{${${*$read_handle}{exp_Parameters}}{"EOF"}});
	}
    }
  }
  foreach $handle (@handles) {
    unless (${*$handle}{"exp_Manual_Stty"}) {
      $handle->exp_stty(${*$handle}{exp_Stored_Stty});
    }
    foreach $write_handle (@{${*$handle}{exp_Listen_Group}}) {
      unless (${*$write_handle}{"exp_Manual_Stty"}) {
	$write_handle->exp_stty(${*$write_handle}{exp_Stored_Stty});
      }
    }
  }
}

# user can decide if log output gets also sent to logfile
sub print_log_file {
  my $self = shift;
  if (${*$self}{exp_Log_File}) {
    if (ref(${*$self}{exp_Log_File}) eq 'CODE') {
      ${*$self}{exp_Log_File}->(@_);
    } else {
      ${*$self}{exp_Log_File}->print(@_);
    }
  }
}

# we provide our own print so we can debug what gets sent to the
# processes...
sub print (@) {
  my ($self, @args) = @_;
  return if not defined $self->fileno(); # skip if closed
  if (${*$self}{exp_Exp_Internal}) {
    my $args = _make_readable(join('', @args));
    cluck "Sending '$args' to ${*$self}{exp_Pty_Handle}\r\n";
  }
  foreach my $arg (@args) {
    while (length($arg) > 80) {
      $self->SUPER::print(substr($arg, 0, 80));
      $arg = substr($arg, 80);
    }
    $self->SUPER::print($arg);
  }
}

# make an alias for Tcl/Expect users for a DWIM experience...
*send = \&print;

# This is an Expect standard. It's nice for talking to modems and the like
# where from time to time they get unhappy if you send items too quickly.
sub send_slow{
  my ($self) = shift;
  my($char,@linechars,$nfound,$rmask);
  return if not defined $self->fileno(); # skip if closed
  my($sleep_time) = shift;
  # Flushing makes it so each character can be seen separately.
  my $chunk;
  while ($chunk = shift) {
    @linechars = split ('', $chunk);
    foreach $char (@linechars) {
      #     How slow?
      select (undef,undef,undef,$sleep_time);

      print $self $char;
      print STDERR "Printed character \'"._make_readable($char)."\' to ${*$self}{exp_Pty_Handle}.\r\n" if ${*$self}{"exp_Debug"} > 1;
      # I think I can get away with this if I save it in accum
      if (${*$self}{"exp_Log_Stdout"} ||${*$self}{exp_Log_Group}) {
	$rmask = "";
	vec($rmask,$self->fileno(),1) = 1;
	# .01 sec granularity should work. If we miss something it will
	# probably get flushed later, maybe in an expect call.
	while (select($rmask,undef,undef,.01)) {
	  my $ret = sysread($self,${*$self}{exp_Pty_Buffer},1024);
	  last if not defined $ret or $ret == 0;
	  # Is this necessary to keep? Probably.. #
	  # if you need to expect it later.
	  ${*$self}{exp_Accum}.= ${*$self}{exp_Pty_Buffer};
	  ${*$self}{exp_Accum} = $self->_trim_length(${*$self}{exp_Accum},${*$self}{"exp_Max_Accum"}) if (${*$self}{"exp_Max_Accum"});
	  $self->_print_handles(${*$self}{exp_Pty_Buffer});
	  print STDERR "Received \'".$self->_trim_length(_make_readable($char))."\' from ${*$self}{exp_Pty_Handle}\r\n" if ${*$self}{"exp_Debug"} > 1;
	}
      }
    }
  }
}

sub test_handles {
  # This should be called by Expect::test_handles($timeout,@objects);
  my ($rmask, $allmask, $rout, $nfound, @bits);
  my ($timeout) = shift;
  my (@handle_list) = @_;
  my($handle);
  foreach $handle (@handle_list) {
    $rmask = '';
    vec($rmask,$handle->fileno(),1) = 1;
    $allmask = '' unless defined ($allmask);
    $allmask = $allmask | $rmask;
  }
  ($nfound) = select($rout = $allmask, undef, undef, $timeout);
  return () unless $nfound;
  # Which handles have stuff?
  @bits = split(//,unpack('b*',$rout));

  my $handle_num = 0;
  my @return_list = ();
  foreach $handle (@handle_list) {
    # I go to great lengths to get perl -w to shut the hell up.
    if (defined($bits[$handle->fileno()]) and ($bits[$handle->fileno()])) {
      push(@return_list,$handle_num);
    }
  } continue {
    $handle_num++;
  }
    return (@return_list);
}

# Be nice close. This should emulate what an interactive shell does after a
# command finishes... sort of. We're not as patient as a shell.
sub soft_close {
  my($self) = shift;
  my($nfound,$nread,$rmask,$returned_pid);
  my($end_time,$select_time,$temp_buffer);
  my($close_status);
  # Give it 15 seconds to cough up an eof.
  cluck "Closing ${*$self}{exp_Pty_Handle}.\r\n" if ${*$self}{exp_Debug};
  return -1 if not defined $self->fileno(); # skip if handle already closed
  unless (exists ${*$self}{exp_Has_EOF} and ${*$self}{exp_Has_EOF}) {
    $end_time = time() + 15;
    while ($end_time > time()) {
      $select_time = $end_time - time();
      # Sanity check.
      $select_time = 0 if $select_time < 0;
      $rmask = '';
      vec($rmask,$self->fileno(),1) = 1;
      ($nfound) = select($rmask,undef,undef,$select_time);
      last unless (defined($nfound) && $nfound);
      $nread = sysread($self,$temp_buffer,8096);
      # 0 = EOF.
      unless (defined($nread) && $nread) {
	print STDERR "Got EOF from ${*$self}{exp_Pty_Handle}.\r\n" if ${*$self}{exp_Debug};
	last;
      }
      $self->_print_handles($temp_buffer);
    }
    if (($end_time <= time()) && ${*$self}{exp_Debug}) {
      print STDERR "Timed out waiting for an EOF from ${*$self}{exp_Pty_Handle}.\r\n";
    }
  }
  if ( ($close_status = $self->close()) && ${*$self}{exp_Debug}) {
    print STDERR "${*$self}{exp_Pty_Handle} closed.\r\n";
  }
  # quit now if it isn't a process.
  return $close_status unless defined(${*$self}{exp_Pid});
  # Now give it 15 seconds to die.
  $end_time = time() + 15;
  while ($end_time > time()) {
    $returned_pid = waitpid(${*$self}{exp_Pid}, &WNOHANG);
    # Stop here if the process dies.
    if (defined($returned_pid) && $returned_pid) {
      delete $Expect::Spawned_PIDs{$returned_pid};
      if (${*$self}{exp_Debug}) {
	printf STDERR ("Pid %d of %s exited, Status: 0x%02X\r\n",
		       ${*$self}{exp_Pid}, ${*$self}{exp_Pty_Handle}, $?);
      }
      ${*$self}{exp_Pid} = undef;
      ${*$self}{exp_Exit} = $?;
      return ${*$self}{exp_Exit};
    }
    sleep 1;			# Keep loop nice.
  }
  # Send it a term if it isn't dead.
  if (${*$self}{exp_Debug}) {
    print STDERR "${*$self}{exp_Pty_Handle} not exiting, sending TERM.\r\n";
  }
  kill TERM => ${*$self}{exp_Pid};
  # Now to be anal retentive.. wait 15 more seconds for it to die.
  $end_time = time() + 15;
  while ($end_time > time()) {
    $returned_pid = waitpid(${*$self}{exp_Pid}, &WNOHANG);
    if (defined($returned_pid) && $returned_pid) {
      delete $Expect::Spawned_PIDs{$returned_pid};
      if (${*$self}{exp_Debug}) {
	printf STDERR ("Pid %d of %s terminated, Status: 0x%02X\r\n",
		       ${*$self}{exp_Pid}, ${*$self}{exp_Pty_Handle}, $?);
      }
      ${*$self}{exp_Pid} = undef;
      ${*$self}{exp_Exit} = $?;
      return $?;
    }
    sleep 1;
  }
  # Since this is a 'soft' close, sending it a -9 would be inappropriate.
  return undef;
}

# 'Make it go away' close.
sub hard_close {
  my($self) = shift;
  my($nfound,$nread,$rmask,$returned_pid);
  my($end_time,$select_time,$temp_buffer);
  my($close_status);
  cluck "Closing ${*$self}{exp_Pty_Handle}.\r\n" if ${*$self}{exp_Debug};
  # Don't wait for an EOF.
  if ( ($close_status = $self->close()) && ${*$self}{exp_Debug}) {
    print STDERR "${*$self}{exp_Pty_Handle} closed.\r\n";
  }
  # Return now if handle.
  return $close_status unless defined(${*$self}{exp_Pid});
  # Now give it 5 seconds to die. Less patience here if it won't die.
  $end_time = time() + 5;
  while ($end_time > time()) {
    $returned_pid = waitpid(${*$self}{exp_Pid}, &WNOHANG);
    # Stop here if the process dies.
    if (defined($returned_pid) && $returned_pid) {
      delete $Expect::Spawned_PIDs{$returned_pid};
      if (${*$self}{exp_Debug}) {
	printf STDERR ("Pid %d of %s terminated, Status: 0x%02X\r\n",
		       ${*$self}{exp_Pid}, ${*$self}{exp_Pty_Handle}, $?);
      }
      ${*$self}{exp_Pid} = undef;
      ${*$self}{exp_Exit} = $?;
      return ${*$self}{exp_Exit};
    }
    sleep 1;			# Keep loop nice.
  }
  # Send it a term if it isn't dead.
  if (${*$self}{exp_Debug}) {
    print STDERR "${*$self}{exp_Pty_Handle} not exiting, sending TERM.\r\n";
  }
  kill TERM => ${*$self}{exp_Pid};
  # wait 15 more seconds for it to die.
  $end_time = time() + 15;
  while ($end_time > time()) {
    $returned_pid = waitpid(${*$self}{exp_Pid}, &WNOHANG);
    if (defined($returned_pid) && $returned_pid) {
      delete $Expect::Spawned_PIDs{$returned_pid};
      if (${*$self}{exp_Debug}) {
	printf STDERR ("Pid %d of %s terminated, Status: 0x%02X\r\n",
		       ${*$self}{exp_Pid}, ${*$self}{exp_Pty_Handle}, $?);
      }
      ${*$self}{exp_Pid} = undef;
      ${*$self}{exp_Exit} = $?;
      return ${*$self}{exp_Exit};
    }
    sleep 1;
  }
  kill KILL => ${*$self}{exp_Pid};
  # wait 5 more seconds for it to die.
  $end_time = time() + 5;
  while ($end_time > time()) {
    $returned_pid = waitpid(${*$self}{exp_Pid}, &WNOHANG);
    if (defined($returned_pid) && $returned_pid) {
      delete $Expect::Spawned_PIDs{$returned_pid};
      if (${*$self}{exp_Debug}) {
	printf STDERR ("Pid %d of %s killed, Status: 0x%02X\r\n",
		       ${*$self}{exp_Pid}, ${*$self}{exp_Pty_Handle}, $?);
      }
      ${*$self}{exp_Pid} = undef;
      ${*$self}{exp_Exit} = $?;
      return ${*$self}{exp_Exit};
    }
    sleep 1;
  }
  warn "Pid ${*$self}{exp_Pid} of ${*$self}{exp_Pty_Handle} is HUNG.\r\n";
  ${*$self}{exp_Pid} = undef;
  return undef;
}

# These should not be called externally.

sub _init_vars {
  my($self) = shift;

  # for every spawned process or filehandle.
  ${*$self}{exp_Log_Stdout} = $Expect::Log_Stdout
    if defined ($Expect::Log_Stdout);
  ${*$self}{exp_Log_Group} = $Expect::Log_Group;
  ${*$self}{exp_Debug} = $Expect::Debug;
  ${*$self}{exp_Exp_Internal} = $Expect::Exp_Internal;
  ${*$self}{exp_Manual_Stty} = $Expect::Manual_Stty;
  ${*$self}{exp_Stored_Stty} = 'sane';
  ${*$self}{exp_Do_Soft_Close} = $Expect::Do_Soft_Close;

  # sysread doesn't like my or local vars.
  ${*$self}{exp_Pty_Buffer} = '';

  # Initialize accumulator.
  ${*$self}{exp_Max_Accum} = $Expect::Exp_Max_Accum;
  ${*$self}{exp_Accum} = '';
  ${*$self}{exp_NoTransfer} = 0;

  # create empty expect_before & after lists
  ${*$self}{exp_expect_before_list} = [];
  ${*$self}{exp_expect_after_list} = [];
}


sub _make_readable {
  my $s = shift;
  $s = '' if not defined ($s);
  study $s;		# Speed things up?
  $s =~ s/\\/\\\\/g;	# So we can tell easily(?) what is a backslash
  $s =~ s/\n/\\n/g;
  $s =~ s/\r/\\r/g;
  $s =~ s/\t/\\t/g;
  $s =~ s/\'/\\\'/g;	# So we can tell whassa quote and whassa notta quote.
  $s =~ s/\"/\\\"/g;
  # Formfeed (does anyone use formfeed?)
  $s =~ s/\f/\\f/g;
  $s =~ s/\010/\\b/g;
  # escape control chars high/low, but allow ISO 8859-1 chars
  $s =~ s/[\000-\037\177-\237\377]/sprintf("\\%03lo",ord($&))/ge;

  return $s;
}

sub _trim_length {
  # This is sort of a reverse truncation function
  # Mostly so we don't have to see the full output when we're using
  # Also used if Max_Accum gets set to limit the size of the accumulator
  # for matching functions.
  # exp_internal
  my($self) = shift;
  my($string) = shift;
  my($length) = shift;

  # If we're not passed a length (_trim_length is being used for debugging
  # purposes) AND debug >= 3, don't trim.
  return($string) if (defined ($self) and
		      ${*$self}{"exp_Debug"} >= 3 and (!(defined($length))));
  my($indicate_truncation) = '...' unless $length;
  $length = 1021 unless $length;
  return($string) unless $length < length($string);
  # We wouldn't want the accumulator to begin with '...' if max_accum is passed
  # This is because this funct. gets called internally w/ max_accum
  # and is also used to print information back to the user.
  return $indicate_truncation.substr($string,(length($string) - $length),$length);
}

sub _print_handles {
  # Given crap from 'self' and the handles self wants to print to, print to
  # them. these are indicated by the handle's 'group'
  my($self) = shift;
  my($print_this) = shift;
  my($handle);
  if (${*$self}{exp_Log_Group}) {
    foreach $handle (@{${*$self}{exp_Listen_Group}}) {
      $print_this = '' unless defined ($print_this);
      # Appease perl -w
      print STDERR "Printed '".$self->_trim_length(_make_readable($print_this))."' to ${*$handle}{exp_Pty_Handle} from ${*$self}{exp_Pty_Handle}.\r\n" if (${*$handle}{"exp_Debug"} > 1);
      print $handle $print_this;
    }
  }
  # If ${*$self}{exp_Pty_Handle} is STDIN this would make it echo.
  print STDOUT $print_this
    if ${*$self}{"exp_Log_Stdout"};
  $self->print_log_file($print_this);
  $|= 1; # This should not be necessary but autoflush() doesn't always work.
}

sub _get_mode {
  my($fcntl_flags) = '';
  my($handle) = shift;
  # What mode are we opening with? use fcntl to find out.
  $fcntl_flags = fcntl(\*{$handle},Fcntl::F_GETFL,$fcntl_flags);
  die "fcntl returned undef during exp_init of $handle, $!\r\n" unless defined($fcntl_flags);
  if ($fcntl_flags | (Fcntl::O_RDWR)) {
    return 'rw';
  } elsif ($fcntl_flags | (Fcntl::O_WRONLY)) {
    return 'w'
  } else {
    # Under Solaris (among others?) O_RDONLY is implemented as 0. so |O_RDONLY would fail.
    return 'r';
  }
}


sub _undef {
  return undef;
  # Seems a little retarded but &CORE::undef fails in interconnect.
  # This is used for the default escape sequence function.
  # w/out the leading & it won't compile.
}

# clean up child processes
sub DESTROY {
  my $status = $?; # save this as it gets mangled by the terminating spawned children
  my $self = shift;
  if (${*$self}{exp_Do_Soft_Close}) {
    $self->soft_close();
  }
  $self->hard_close();
  $? = $status; # restore it. otherwise deleting an Expect object may mangle $?, which is unintuitive
}

1;
