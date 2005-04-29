# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)

BEGIN { $| = 1; print "1..12\n"; }
END {print "not ok 1\n" unless $loaded;}
use Convert::TNEF;
$loaded = 1;
print "ok 1\n";

######################### End of black magic.

use strict;
use Convert::TNEF;

my $n = 2;
my $tnef = Convert::TNEF->read_in("t/tnef.doc",{ignore_checksum=>1});
if (ref($tnef) eq 'Convert::TNEF') {
 print "ok $n\n";
} else {
 print "not ok $n\n";
 exit;
}

my $att_cnt=0;
my $att_name_exists;
my $att_name_ok;
my $att_lname_exists;
my $att_lname_ok;
my $att_data_exists;
my $att_data_ok;

for my $attachment ($tnef->attachments) {
 last if $att_cnt > 1;
 if (my $att_name = $attachment->name) {
  $att_name_exists++;
  if ($att_name eq "tmp.out") {
   $att_name_ok++;
  }
 }

 if (my $att_lname = $attachment->longname) {
  $att_lname_exists++;
  if ($att_lname eq "tmp.out") {
   $att_lname_ok++;
  }
 }

 if (my $att_data = $attachment->data) {
  $att_data_exists++;
  if ($att_data =~ /^This is an attachment/) {
   $att_data_ok++;
  }
 }
 $att_cnt++;
}

# Check here for errors from above
$n++;
if ($att_cnt == 1) {
 print "ok $n\n";
} else {
 print "not ok $n\n";
}

$n++;
if ($att_name_exists) {
 print "ok $n\n";
} else {
 print "not ok $n\n";
}

$n++;
if ($att_name_ok) {
 print "ok $n\n";
} else {
 print "not ok $n\n";
}

$n++;
if ($att_lname_ok) {
 print "ok $n\n";
} else {
 print "not ok $n\n";
}

$n++;
if ($att_data_exists) {
 print "ok $n\n";
} else {
 print "not ok $n\n";
}

$n++;
if ($att_data_ok) {
 print "ok $n\n";
} else {
 print "not ok $n\n";
}

# Test messages
my $msg_exists;
my $msg_class_exists;
my $msg_class_ok;
if (my $message = $tnef->message) {
 $msg_exists++;
 if (my $msg_class = $message->data('MessageClass')) {
  $msg_class_exists++;
  if ($msg_class eq "IPM.Microsoft Mail.Note\x00") {
   $msg_class_ok++;
  }
 }
}

$n++;
if ($msg_exists) {
 print "ok $n\n";
} else {
 print "not ok $n\n";
}

$n++;
if ($msg_class_exists) {
 print "ok $n\n";
} else {
 print "not ok $n\n";
}

$n++;
if ($msg_class_ok) {
 print "ok $n\n";
} else {
 print "not ok $n\n";
}

# Check that errors work
$n++;
$tnef = Convert::TNEF->read_in("t/tnef.doc");
if ($Convert::TNEF::errstr eq 'Bad Checksum') {
 print "ok $n\n";
} else {
 print "not ok $n\n";
}
