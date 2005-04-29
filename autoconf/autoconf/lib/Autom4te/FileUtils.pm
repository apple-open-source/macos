# Copyright (C) 2003  Free Software Foundation, Inc.

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
# 02111-1307, USA.

package Autom4te::FileUtils;

=head1 NAME

Autom4te::FileUtils - handling files

=head1 SYNOPSIS

  use Autom4te::FileUtils

=head1 DESCRIPTION

This perl module provides various general purpose file handling functions.

=cut

use strict;
use Exporter;
use File::stat;
use IO::File;
use Autom4te::Channels;
use Autom4te::ChannelDefs;

use vars qw (@ISA @EXPORT);

@ISA = qw (Exporter);
@EXPORT = qw (&contents
	      &find_file &mtime
	      &update_file &up_to_date_p
	      &xsystem &xqx);


=item C<find_file ($filename, @include)>

Return the first path for a C<$filename> in the C<include>s.

We match exactly the behavior of GNU M4: first look in the current
directory (which includes the case of absolute file names), and, if
the file is not absolute, just fail.  Otherwise, look in C<@include>.

If the file is flagged as optional (ends with C<?>), then return undef
if absent, otherwise exit with error.

=cut

# $FILENAME
# find_file ($FILENAME, @INCLUDE)
# -------------------------------
sub find_file ($@)
{
  use File::Spec;

  my ($filename, @include) = @_;
  my $optional = 0;

  $optional = 1
    if $filename =~ s/\?$//;

  return File::Spec->canonpath ($filename)
    if -e $filename;

  if (File::Spec->file_name_is_absolute ($filename))
    {
      fatal "$filename: no such file or directory"
	unless $optional;
      return undef;
    }

  foreach my $path (@include)
    {
      return File::Spec->canonpath (File::Spec->catfile ($path, $filename))
	if -e File::Spec->catfile ($path, $filename)
    }

  fatal "$filename: no such file or directory"
    unless $optional;

  return undef;
}

=item C<mtime ($file)>

Return the mtime of C<$file>.  Missing files, or C<-> standing for
C<STDIN> or C<STDOUT> are ``obsolete'', i.e., as old as possible.

=cut

# $MTIME
# MTIME ($FILE)
# -------------
sub mtime ($)
{
  my ($file) = @_;

  return 0
    if $file eq '-' || ! -f $file;

  my $stat = stat ($file)
    or fatal "cannot stat $file: $!";

  return $stat->mtime;
}


=item C<update_file ($from, $to)>

Rename C<$from> as C<$to>, preserving C<$to> timestamp if it has not
changed.  Recognize C<$to> = C<-> standing for C<STDIN>.  C<$from> is
always removed/renamed.

=cut

# &update_file ($FROM, $TO)
# -------------------------
sub update_file ($$)
{
  my ($from, $to) = @_;
  my $SIMPLE_BACKUP_SUFFIX = $ENV{'SIMPLE_BACKUP_SUFFIX'} || '~';
  use File::Compare;
  use File::Copy;

  if ($to eq '-')
    {
      my $in = new IO::File ("$from");
      my $out = new IO::File (">-");
      while ($_ = $in->getline)
	{
	  print $out $_;
	}
      $in->close;
      unlink ($from) || fatal "cannot remove $from: $!";
      return;
    }

  if (-f "$to" && compare ("$from", "$to") == 0)
    {
      # File didn't change, so don't update its mod time.
      msg 'note', "`$to' is unchanged";
      unlink ($from)
        or fatal "cannot remove $from: $!";
      return
    }

  if (-f "$to")
    {
      # Back up and install the new one.
      move ("$to",  "$to$SIMPLE_BACKUP_SUFFIX")
	or fatal "cannot backup $to: $!";
      move ("$from", "$to")
	or fatal "cannot rename $from as $to: $!";
      msg 'note', "`$to' is updated";
    }
  else
    {
      move ("$from", "$to")
	or fatal "cannot rename $from as $to: $!";
      msg 'note', "`$to' is created";
    }
}


=item C<up_to_date_p ($file, @dep)>

Is C<$file> more recent than C<@dep>?

=cut

# $BOOLEAN
# &up_to_date_p ($FILE, @DEP)
# ---------------------------
sub up_to_date_p ($@)
{
  my ($file, @dep) = @_;
  my $mtime = mtime ($file);

  foreach my $dep (@dep)
    {
      if ($mtime < mtime ($dep))
	{
	  verb "up_to_date ($file): outdated: $dep";
	  return 0;
	}
    }

  verb "up_to_date ($file): up to date";
  return 1;
}


=item C<handle_exec_errors ($command)>

Display an error message for C<$command>, based on the content of
C<$?> and C<$!>.

=cut

# handle_exec_errors ($COMMAND)
# -----------------------------
sub handle_exec_errors ($)
{
  my ($command) = @_;

  $command = (split (' ', $command))[0];
  if ($!)
    {
      fatal "failed to run $command: $!";
    }
  else
    {
      use POSIX qw (WIFEXITED WEXITSTATUS WIFSIGNALED WTERMSIG);

      if (WIFEXITED ($?))
	{
	  my $status = WEXITSTATUS ($?);
	  # Propagate exit codes.
	  fatal ('',
		 "$command failed with exit status: $status",
		 exit_code => $status);
	}
      elsif (WIFSIGNALED ($?))
	{
	  my $signal = WTERMSIG ($?);
	  fatal "$command terminated by signal: $signal";
	}
      else
	{
	  fatal "$command exited abnormally";
	}
    }
}

=item C<xqx ($command)>

Same as C<qx> (but in scalar context), but fails on errors.

=cut

# xqx ($COMMAND)
# --------------
sub xqx ($)
{
  my ($command) = @_;

  verb "running: $command";

  $! = 0;
  my $res = `$command`;
  handle_exec_errors $command
    if $?;

  return $res;
}


=item C<xsystem ($command)>

Same as C<system>, but fails on errors, and reports the C<$command>
in verbose mode.

=cut

# xsystem ($COMMAND)
# ------------------
sub xsystem ($)
{
  my ($command) = @_;

  verb "running: $command";

  $! = 0;
  handle_exec_errors $command
    if system $command;
}


=item C<contents ($filename)>

Return the contents of c<$filename>.

=cut

# contents ($FILENAME)
# --------------------
sub contents ($)
{
  my ($file) = @_;
  verb "reading $file";
  local $/;			# Turn on slurp-mode.
  my $f = new Autom4te::XFile "< $file";
  my $contents = $f->getline;
  $f->close;
  return $contents;
}


1; # for require

### Setup "GNU" style for perl-mode and cperl-mode.
## Local Variables:
## perl-indent-level: 2
## perl-continued-statement-offset: 2
## perl-continued-brace-offset: 0
## perl-brace-offset: 0
## perl-brace-imaginary-offset: 0
## perl-label-offset: -2
## cperl-indent-level: 2
## cperl-brace-offset: 0
## cperl-continued-brace-offset: 0
## cperl-label-offset: -2
## cperl-extra-newline-before-brace: t
## cperl-merge-trailing-else: nil
## cperl-continued-statement-offset: 2
## End:
