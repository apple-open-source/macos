#!/usr/bin/perl -w

# majordomo2mailman.pl - Migrate Majordomo mailing lists to Mailman 2.0
#          Copyright (C) 2002 Heiko Rommel (rommel@suse.de)

# BAW: Note this probably needs to be upgraded to work with MM2.1

#
# License:
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 1, or (at your option)
# any later version.

#
# Warranty:
#
# There's absolutely no warranty.
#

# comments on possible debug messages during the conversion:
#
# "not an valid email address" : those addresses are rejected, i.e. not imported into the Mailman list
# "not a numeric value" : such a value will be converted to 0 (z.B. maxlength)
# "already subscribed" : will only once be subscribed on the Mailman list
# "...umbrella..." or "...taboo..." -> Mailman-Admin-Guide

use strict;
use Getopt::Long;
use Fcntl;
use POSIX qw (tmpnam);

use vars qw (
	     $majordomo $mydomain $myurl
	     $aliasin $listdir
	     $aliasout $mailmanbin
	     $umbrella_member_suffix $private 
	     $newsserver $newsprefix
	     $susehack $susearchuser
	     $help $debug $update $all $usagemsg
	     *FH
	     %mlaliases %mlowners %mlapprovers
	     %defaultmlconf %mlconf
	     %defaultmmconf %mmconf
);

#
# adjust your site-specific settings here
#

$mydomain               = "my.domain";
$majordomo              = "majordomo"; # the master Majordomo address for your site
$aliasin                = "/var/lib/majordomo/aliases";
$listdir                = "/var/lib/majordomo/lists";
$aliasout               = "/tmp/aliases";
$myurl                  = "http://my.domain/mailman/";
$mailmanbin             = "/usr/lib/mailman/bin";
$umbrella_member_suffix = "-owner";
$private                = "yes"; # is this a private/Intranet site ?
$newsserver             = "news.my.domain";
$newsprefix             = "intern.";

$susehack = "no";
$susearchuser = "archdummy";

#
# 0)
# parse the command line arguments
#

$usagemsg = "usage: majordomo2mailman [-h|--help] [-d|--debug] [-u|--update] < (-a|--all) | list-of-mailinglists >";

GetOptions(
           "h|help" => \$help,
	   "d|debug" => \$debug,
	   "a|all" => \$all,
	   "u|update" => \$update
) or die "$usagemsg\n";

if (defined($help)) { die "$usagemsg\n"; }

if ((not defined($all)) and (@ARGV<1)) { die "$usagemsg\n"; }

if ($<) { die "this script must be run as root!\n"; }

#
# 1)
# build a list of all aliases and extract the name of mailing lists plus their owners
#

%mlaliases = %mlowners = %mlapprovers = ();

open (FH, "< $aliasin") or die "can't open $aliasin\n";

while (<FH>) {
  # first, build a list of all active aliases and their resolution
  if (/^([^\#:]+)\s*:\s*(.*)$/) {
    $mlaliases{$1} = $2;
  }
}

my $mlalias;
for $mlalias (keys %mlaliases) {
  # if we encounter an alias with :include: as expansion
  # it is save to assume that the alias has the form
  # <mailinglist>-outgoing -
  # that way we find the names of all active mailing lists
  if ($mlaliases{$mlalias} =~ /\:include\:/) {
    my $ml;
    ($ml = $mlalias) =~ s/-outgoing//g;
    $mlowners{$ml} = $mlaliases{"owner-$ml"};
    $mlapprovers{$ml} = $mlaliases{"$ml-approval"};
  }
}

close (FH);

#
# 2)
# for each list read the Majordomo configuration params
# and create a Mailman clone
#

my $ml;
for $ml ((defined ($all)) ? sort keys %mlowners : @ARGV) {

  init_defaultmlconf($ml);
  %mlconf = %defaultmlconf;

  init_defaultmmconf($ml);
  %mmconf = %defaultmmconf;

  my @privileged; # addresses that are mentioned in restrict_post
  my @members;
  my ($primaryowner, @secondaryowner);
  my ($primaryapprover, @secondaryapprover);

  my ($skey, $terminator);
  my $filename;
  my @args;

  #
  # a)
  # parse the configuration file
  #

  open (FH, "< $listdir/$ml.config") or die "can't open $listdir/$ml.config\n";

  while (<FH>) {
    # key = value ?
    if (/^\s*([^=\#\s]+)\s*=\s*(.*)\s*$/) {
      $mlconf{$1} = $2;
    }
    # key << EOF
    # value
    # EOF ?
    elsif (/^\s*([^<\#\s]+)\s*<<\s*(.*)\s*$/) {
      ($skey, $terminator) = ($1, $2);
      while (<FH>) {
	last if (/^$terminator\s*$/);
	$mlconf{$skey} .= $_;
      }
      chomp $mlconf{$skey};
    }
  }

  close (FH);

  #
  # b)
  # test if there are so-called flag files (clue that this is an old-style Majordomo lists)
  # and overwrite previously parsed values
  # (stolen from majordomo::config_parse.pl: handle_flag_files())
  #

  if ( -e "$listdir/$ml.private") {
    $mlconf{"get_access"} = "closed";
    $mlconf{"index_access"} = "closed";
    $mlconf{"who_access"} = "closed";
    $mlconf{"which_access"} = "closed";
  }

  $mlconf{"subscribe_policy"} = "closed" if ( -e "$listdir/$ml.closed");
  $mlconf{"unsubscribe_policy"} = "closed" if ( -e "$listdir/$ml.closed");

  if ( -e "$listdir/$ml.auto" && -e "$listdir/$ml.closed") {
    print STDERR "sowohl $ml.auto als auch $ml.closed existieren. Wähle $ml.closed\n"; 
  }
  else {
    $mlconf{"subscribe_policy"} = "auto" if ( -e"$listdir/$ml.auto"); 
    $mlconf{"unsubscribe_policy"} = "auto" if ( -e"$listdir/$ml.auto"); 
  }

  $mlconf{"strip"} = 1 if ( -e "$listdir/$ml.strip");
  $mlconf{"noadvertise"} = "/.*/" if ( -e "$listdir/$ml.hidden");

  # admin_passwd:
  $filename = "$listdir/" . $mlconf{"admin_passwd"};
  if ( -e "$listdir/$ml.passwd" ) {
    $mlconf{"admin_passwd"} = read_from_file("$listdir/$ml.passwd");
  }
  elsif ( -e "$filename" ) {
    $mlconf{"admin_passwd"} = read_from_file("$filename");
  }
  # else take it verbatim

  # approve_passwd:
  $filename = "$listdir/" . $mlconf{"approve_passwd"};
  if ( -e "$listdir/$ml.passwd" ) {
    $mlconf{"approve_passwd"} = read_from_file("$listdir/$ml.passwd");
  }
  elsif ( -e "$filename" ) {
    $mlconf{"approve_passwd"} = read_from_file("$filename");
  }
  # else take it verbatim

  #
  # c)
  # add some information from additional configuration files
  #

  # restrict_post
  if (defined ($mlconf{"restrict_post"})) {
    @privileged = ();
    for $filename (split /\s+/, $mlconf{"restrict_post"}) {
      open (FH, "< $listdir/$filename") or die "can't open $listdir/$filename\n";
      push (@privileged, <FH>);
      chomp @privileged;
      close (FH);
    }
  }

  if ($susehack =~ m/yes/i) {
    @privileged = grep(!/$susearchuser\@$mydomain/i, @privileged);
  }

  $mlconf{"privileged"} = \@privileged;

  # members
  @members = ();
  open (FH, "< $listdir/$ml") or die "can't open $listdir/$ml\n";
  push (@members, <FH>);
  chomp @members;
  close (FH);

  $mlconf{"gated"} = "no";

  if ($susehack =~ m/yes/i) {
    if (grep(/$susearchuser\@$mydomain/i, @members)) {
      $mlconf{"gated"} = "yes";
    }
    @members = grep(!/$susearchuser\@$mydomain/i, @members);
  }

  $mlconf{"members"} = \@members;

  # intro message
  if (open (FH, "< $listdir/$ml.intro")) {
    { local $/; $mlconf{"intro"} = <FH>; }
  }
  else { $mlconf{"intro"} = ""; }

  # info message
  if (open (FH, "< $listdir/$ml.info")) {
    { local $/; $mlconf{"info"} = <FH>; }
  }
  else { $mlconf{"info"} = ""; }
  
  #
  # d)
  # take over some other params into the configuration table
  #

  $mlconf{"name"} = "$ml";

  ($primaryowner, @secondaryowner) = 
    expand_alias (split (/\s*,\s*/, aliassub($mlowners{$ml})));

  ($primaryapprover, @secondaryapprover) =
    expand_alias (split (/\s*,\s*/, aliassub($mlapprovers{$ml})));

  $mlconf{"primaryowner"} = $primaryowner;
  $mlconf{"secondaryowner"} = \@secondaryowner;

  $mlconf{"primaryapprover"} = $primaryapprover;
  $mlconf{"secondaryapprover"} = \@secondaryapprover;

  #
  # debugging output
  #

  if (defined ($debug)) {
    print "##################### $ml ####################\n";
    for $skey (sort keys %mlconf) {
      if (defined ($mlconf{$skey})) { print "$skey = $mlconf{$skey}\n"; }
      else { print "$skey = (?)\n"; }
    }
    my $priv;
    for $priv (@privileged) {
      print "\t$ml: $priv\n";
    }
  }

  #
  # e)
  # with the help of Mailman commands - create a new list and subscribe the old staff
  #

  if (defined($update)) {
    print "updating configuration of \"$ml\"\n";
  }
  else {
    # Mailman lists can initially be only created with one owner
    @args = ("$mailmanbin/newlist", "-q", "-o", "$aliasout", "$ml", $mlconf{"primaryowner"}, $mlconf{"admin_passwd"});
    system (@args) == 0 or die "system @args failed: $?";
  }
   
  # Mailman accepts only subscriber lists > 0
  if (@members > 0) {
    $filename = tmpnam();
    open (FH, "> $filename") or die "can't open $filename\n";
    for $skey (@members) {
      print FH "$skey" . "\n";
    }
    close (FH);
    @args = ("$mailmanbin/add_members", "-n", "$filename", "--welcome-msg=n", "$ml");
    system (@args) == 0 or die "system @args failed: $?";
  }

  #
  # f)
  # "translate" the Majordomo list configuration
  #

  m2m();

  # write the Mailman config

  $filename = tmpnam();

  open (FH, "> $filename") or die "can't open $filename\n";
  for $skey (sort keys %mmconf) {
    print FH "$skey = " . $mmconf{$skey} . "\n";
  }
  close (FH);

  @args = ("$mailmanbin/config_list", "-i", "$filename", "$ml");
  system (@args) == 0 or die "system @args failed: $?";

  unlink($filename) or print STDERR "unable to unlink \"$filename\"!\n";

}

exit 0;

#############
# subs
#############

#
# I don't know how to write Perl code
# therefor I need this stupid procedure to cleanly read a value from file
#

sub read_from_file {
  my $value;
  local *FH;

  open (FH, "< $_[0]") or die "can't open $_[0]\n";
  $value = <FH>;
  chomp $value;
  close (FH);

  return $value;
}


#
# add "@$mydomain" to each element that does not contain a "@"
#

sub expand_alias {
  return map  { (not $_ =~ /@/) ? $_ .= "\@$mydomain" : $_ } @_;
}

#
# replace the typical owner-majordomo aliases
#

sub aliassub {
  my $string = $_[0];

  $string =~ s/(owner-$majordomo|$majordomo-owner)/mailman-owner/gi;

  return $string;
}

#
# default values of Majordomo mailing lists
# (stolen from majordomo::config_parse.pl: %known_keys)
#

sub init_defaultmlconf {
  my $ml = $_[0];

  %defaultmlconf=(  
		     'welcome',	"yes",
		     'announcements',	"yes",
		     'get_access',	"open",
		     'index_access',	"open",
		     'who_access',	"open",
		     'which_access',	"open",
		     'info_access',	"open",
		     'intro_access',	"open",
		     'advertise',	"",
		     'noadvertise',	"",
		     'description',	"",
		     'subscribe_policy',	"open",
		     'unsubscribe_policy',	"open",
		     'mungedomain',	"no",
		     'admin_passwd',	"$ml.admin",
		     'strip',		"yes",
		     'date_info',	"yes",
		     'date_intro',	"yes",
		     'archive_dir',	"",
		     'moderate',	"no",
		     'moderator',	"",
		     'approve_passwd', "$ml.pass",
		     'sender', 	"owner-$ml",
		     'maxlength', 	"40000",
		     'precedence', 	"bulk",
		     'reply_to', 	"",
		     'restrict_post',	"",
		     'purge_received', "no",
		     'administrivia', 	"yes",
		     'resend_host', 	"",
		     'debug', 		"no",
		     'message_fronter', "",
		     'message_footer',  "",
		     'message_headers', "",
		     'subject_prefix',	"",
		     'taboo_headers',	"",
		     'taboo_body',	"",
		     'digest_volume',	"1",
		     'digest_issue',	"1",
		     'digest_work_dir', "",
		     'digest_name',	"$ml",
		     'digest_archive',	"",
		     'digest_rm_footer',    "",
		     'digest_rm_fronter',   "",  
		     'digest_maxlines', "",
		     'digest_maxdays',	"",
		     'comments',	""
		    );
}


#
# Mailman mailing list params that are not derived from Majordomo mailing lists params
# (e.g. bounce_matching_headers+forbbiden_posters vs. taboo_headers+taboo_body)
# If you need one of this params to be variable remove it here and add some code to the 
# main procedure; additionally, you should compare it with what you have in
# /usr/lib/mailman/Mailman/mm_cfg.py
#

sub init_defaultmmconf {

  %defaultmmconf=(  
		  'goodbye_msg', "\'\'",
		  'umbrella_list', "0",
		  'umbrella_member_suffix', "\'$umbrella_member_suffix\'",
		  'send_reminders', "0",
		  'admin_immed_notify', "1",
		  'admin_notify_mchanges', "0",
		  'dont_respond_to_post_requests', "0",
		  'obscure_addresses', "1",
		  'require_explicit_destination', "1",
		  'acceptable_aliases', "\"\"\"\n\"\"\"\n",
		  'max_num_recipients', "10",
		  'forbidden_posters', "[]",
		  'bounce_matching_headers',  "\"\"\"\n\"\"\"\n",
		  'anonymous_list', "0",
		  'nondigestable', "1",
		  'digestable', "1",
		  'digest_is_default', "0",
		  'mime_is_default_digest', "0",
		  'digest_size_threshhold', "40",
		  'digest_send_periodic', "1",
		  'digest_header', "\'\'",
		  'bounce_processing', "1",
		  'minimum_removal_date', "4",
		  'minimum_post_count_before_bounce_action', "3",
		  'max_posts_between_bounces', "5",
		  'automatic_bounce_action', "3",
		  'archive_private', "0",
		  'clobber_date', "1",
		  'archive_volume_frequency', "1",
		  'autorespond_postings', "0",
		  'autoresponse_postings_text', "\'\'",
		  'autorespond_admin', "0",
		  'autoresponse_admin_text', "\'\'",
		  'autorespond_requests', "0",
		  'autoresponse_request_text', "\'\'",
		  'autoresponse_graceperiod', "90"
		 );
}

#
# convert a Majordomo mailing list configuration (%mlconf) into a 
# Mailman mailing list configuration (%mmconf)
# only those params are affected which can be derived from Majordomo 
# mailing list configurations
#

sub m2m {

  my $elem;
  my $admin;

  $mmconf{"real_name"} = "\'" . $mlconf{"name"} . "\'";

  # Mailman does not know the difference between owner and approver
  for $admin (($mlconf{"primaryowner"}, @{$mlconf{"secondaryowner"}},
	       $mlconf{"primaryapprover"}, @{$mlconf{"secondarapprover"}})) {
    # merging owners and approvers may result in a loop:
    if (lc($admin) ne lc("owner-" . $mlconf{"name"} . "\@" . $mydomain)) {
      $mmconf{"owner"} .= ",\'" . "$admin" . "\'";
    }
  }
  $mmconf{"owner"} =~ s/^,//g;
  $mmconf{"owner"} = "\[" . $mmconf{"owner"} . "\]";

  # remove characters that will break Python
  ($mmconf{"description"} = $mlconf{"description"}) =~ s/\'/\\\'/g;
  $mmconf{"description"} = "\'" . $mmconf{"description"} . "\'";

  $mmconf{"info"} = "\"\"\"\n" . $mlconf{"info"} . "\"\"\"\n";

  $mmconf{"subject_prefix"} = "\'" . $mlconf{"subject_prefix"} . "\'";

  $mmconf{"welcome_msg"} = "\"\"\"\n" . $mlconf{"intro"} . "\"\"\"\n";

  # I don't know how to handle this because the reply_to param in the lists
  # I had were not configured consistently
  if ($mlconf{"reply_to"} =~ /\S+/) {
    if ($mlconf{"name"} . "\@" =~ m/$mlconf{"reply_to"}/i) {
      $mmconf{"reply_goes_to_list"} = "1";
      $mmconf{"reply_to_address"} = "\'\'";
    }
    else {
      $mmconf{"reply_goes_to_list"} = "2";
      $mmconf{"reply_to_address"} = "\'" . $mlconf{"reply_to"} . "\'";
    }
  }
  else {
    $mmconf{"reply_goes_to_list"} = "0";
    $mmconf{"reply_to_address"} = "\'\'";
  }

  $mmconf{"administrivia"} = ($mlconf{"administrivia"} =~ m/yes/i) ? "1" : "0";
  $mmconf{"send_welcome_msg"} = ($mlconf{"welcome"} =~ m/yes/i) ? "1" : "0";

  $mmconf{"max_message_size"} = int ($mlconf{"maxlength"} / 1000);

  $mmconf{"host_name"} = ($mlconf{"resend_host"} =~ /\S+/) ? 
    $mlconf{"resend_host"} : "\'" . $mydomain . "\'";

  $mmconf{"web_page_url"} = "\'" . $myurl . "\'";

  # problematic since Mailman does not know access patterns
  # I assume, that if there was given a noadvertise pattern, the
  # list shouldn't be visible at all
  $mmconf{"advertised"} = ($mlconf{"noadvertise"} =~ /\.\*/) ? "0" : "1";

  # confirm+approval is much to long winded for private sites
  $mmconf{"subscribe_policy"} = 
    ($mlconf{"subscribe_policy"} =~ m/(open|auto)/i) ? "1" : 
      ($private =~ m/yes/i) ? "2" : "3";

  # in case this is a private site allow list visiblity at most
  $mmconf{"private_roster"} =
    ($mlconf{"who_access"} =~ m/open/i and not $private =~ m/yes/i) ? "0" :
      ($mlconf{"who_access"} =~ m/open|list/i) ? "1" : "2";

  $mmconf{"moderated"} = ($mlconf{"moderate"} =~ m/yes/i) ? "1" : "0";
  # there is no way to a set a separate moderator in Mailman

  # external, since lengthy
  mm_posters();

  if ($mlconf{"message_fronter"} =~ /\S+/) {
    $mmconf{"msg_header"} = "\"\"\"\n" . $mlconf{"message_fronter"} . "\"\"\"\n";
  }
  else {
    $mmconf{"msg_header"} = "\'\'";
  }

  if ($mlconf{"message_footer"} =~ /\S+/) {
    $mmconf{"msg_footer"} = "\"\"\"\n" . $mlconf{"message_footer"} . "\"\"\"\n";
  }
  else {
    $mmconf{"msg_footer"} = "\'\'";
  }

  # gateway to news
  $mmconf{"nntp_host"} = "\'" . $newsserver . "\'";
  $mmconf{"linked_newsgroup"} = "\'" . $newsprefix . $mlconf{"name"} . "\'";

  if ($mlconf{"gated"} =~ m/yes/i) {
    $mmconf{"gateway_to_news"} = "1";
    $mmconf{"gateway_to_mail"} = "1";
    $mmconf{"archive"} = "1";
  }
  else {
    $mmconf{"gateway_to_news"} = "0";
    $mmconf{"gateway_to_mail"} = "0";
    $mmconf{"archive"} = "0";
  }

  # print warnings if this seems to be an umbrella list
  for $elem (@{$mlconf{"privileged"}}, @{$mlconf{"members"}}) {
    $elem =~ s/\@$mydomain//gi;
    if (defined($mlaliases{$elem . $umbrella_member_suffix})) {
      print STDERR "\"" . $mlconf{"name"} .
	 "\" possibly forms part off/is an umbrella list, since \"$elem\" is a local mailing list alias\n";   
    }
  }

  # print warnings if we encountered a Taboo-Header or Taboo-Body
  if ($mlconf{"taboo_headers"} =~ /\S+/ or $mlconf{"taboo_body"} =~ /\S+/) {
    print STDERR "\"" . $mlconf{"name"} . "\" taboo_headers or taboo_body seem to be set - please check manually.\n";
  }
}

#
# with some set theory on the member and priviliged list try to determine the params
# $mmconf{"member_posting_only"} and $mmconf{"posters"}
#

sub mm_posters {
  if ($mlconf{"restrict_post"} =~ /\S+/) {
    my %privileged = ();
    my %members = ();
    my $key;

    foreach $key (@{$mlconf{"privileged"}}) { $privileged{$key} = "OK"; }
    foreach $key (@{$mlconf{"members"}}) { $members{$key} = "OK"; }

    # are all members privileged, too ?
    my $included = 1;
    foreach $key (keys %members) {
      if (not exists $privileged{$key}) {
	$included = 0;
	last;
      }
    }
    if ($included) {
      $mmconf{"member_posting_only"} = "1";

      # posters = privileged - members:
      my %diff = %privileged;
      foreach $key (keys %members) {
	delete $diff{$key} if exists $members{$key};
      }

      $mmconf{"posters"} = "";
      for $key (sort keys %diff) {
	$mmconf{"posters"} .= ",\'" . $key . "\'";
      }
      $mmconf{"posters"} =~ s/^,//g;
      $mmconf{"posters"} = "[" . $mmconf{"posters"} . "]";
    }
    else {
      $mmconf{"member_posting_only"} = "0";

      # posters = privileged:
      $mmconf{"posters"} = "";
      for $key (sort keys %privileged) {
	$mmconf{"posters"} .= ",\'" . $key . "\'";
      }
      $mmconf{"posters"} =~ s/^,//g;
      $mmconf{"posters"} = "[" . $mmconf{"posters"} . "]";
    }
  }
  else {
    $mmconf{"member_posting_only"} = "0";
    $mmconf{"posters"} = "[]";
  }
}

