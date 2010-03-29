package JpegTester;
# Author: Mark Martinec <mark.martinec@ijs.si>, 2004-10;
# The (new)BSD license applies to this package JpegTester;
use strict;
use re 'taint';

use vars qw($buf $buf_l $buf_ofs);
sub makeTwo($) {   # ensure at least two characters in $buf, except near eof
  my($fh) = @_;
  $buf_l>=0 or die "jpeg: Panic, program error1, pos=$buf_ofs";
  if ($buf_l<2) {
    my($len) = sysread($fh,$buf,2048,$buf_l);  # 2k is about the optimum size
    defined $len or die "jpeg: Can't read: $!\n";
    $buf_l += $len;
  }
}
sub takeN($$) {    # swallow n characters
  my($fh,$n) = @_; my($err) = undef;
  for ($buf_l>=2||makeTwo($fh); $n>0; ) {
    if ($buf_l<=0) { $err = "Truncated by $n bytes or more"; last }
    if ($n >= $buf_l) { $n -= $buf_l; $buf_ofs += $buf_l; $buf = '' }
    else { $buf = substr($buf,$n); $buf_ofs += $n; $n = 0 }
    $buf_l = length($buf);  $buf_l>=2 || makeTwo($fh);
  }
  $err;
}
sub takeECS($) {   # quickly swallow entropy-coded data segment
  my($fh) = @_;
  for ($buf_l>=2||makeTwo($fh); $buf_l>0; ) {
    if    ($buf =~ s/^([^\xff]+)//)  { $buf_ofs += length($1) }
    elsif ($buf =~ s/^(\xff\x00)+//) { $buf_ofs += length($1) }
    else { last }
#   last unless $buf =~ s/^(?: [^\xff] | \xff \x00 )+//x;  # Perl Bus error
    $buf_l = length($buf);  $buf_l>=2 || makeTwo($fh);
  }
}
sub takeFill($) {  # swallow fill bytes before marker
  my($fh) = @_;
  for (makeTwo($fh); $buf_l>0; $buf_l=length($buf),makeTwo($fh)) {
    if ($buf =~ s/^ \xff+ (?= \xff )//x) { $buf_ofs += length($1) }
    else { last }
  }
}
sub takeTail($) {  # swallow common junk after EOI
  my($fh) = @_;
  for (makeTwo($fh); $buf_l>0; $buf_l=length($buf),makeTwo($fh)) {
    if ($buf =~ s/^[\x00\xff]+//) { $buf_ofs += length($1) }
    else { last }
  }
}

# exit status: 0:clean; 1:exploit; 2:corrupted
sub test_jpeg($;@) {
  my($fn) = @_;  # file name to be checked
  local(*F); my($fh) = \*F;
  open($fh,"<$fn") or die "jpeg: Can't open file $fn for reading: $!";
  binmode($fh) or die "jpeg: Can't set binmode on $fn: $!";
  $buf = ''; $buf_l = 0; $buf_ofs = 0; makeTwo($fh); my(@r) = (0,"jpeg ok");
  if ($buf !~ /^\xff\xd8/) { @r = (0,"not jpeg") }
  else { takeN($fh,2);  if ($buf !~ /^\xff/) { @r = (0,"not jpeg") } }
  if ($r[1] eq "jpeg ok") {
    my($ecs_ok) = 0; local($1);
    for (;;) {  # keep at least 2 chars in buff except near eof
      if ($buf_l<=0 || $buf eq "\xff") {
        @r = (2,"Truncated, no EOI, pos=$buf_ofs")  if !$r[0];
        last;
      } elsif ($buf =~ /^( [^\xff] | \xff \x00 )/x) {  # ecs
        @r = (2,"Unexpected entropy-coded data segment, pos=$buf_ofs")
          if !$ecs_ok && !$r[0];
        takeECS($fh);  $ecs_ok = 0;
      } elsif ($buf =~ /^ \xff+ (?= \xff ) /x) {     # fill bytes before marker
        takeFill($fh); $ecs_ok = 0;
      } elsif ($buf =~ /^ \xff ([^\x00\xff]) /x) {   # marker
        my($m) = $1; takeN($fh,2);
        if    ($m =~ /[\xd0-\xd7]/) {    # RSTi
#         printf("marker segm, pos=%d, marker=0x%02X\n", $buf_ofs,ord($m));
          $ecs_ok = 1;
        } elsif ($m =~ /[\x01\xd8]/) {   # TEM, SOI
#         printf("marker segm, pos=%d, marker=0x%02X\n", $buf_ofs,ord($m));
          $ecs_ok = 0;
        }
        elsif ($m eq "\xd9") {  # EOI
#         printf("marker segm, pos=%d, marker=0x%02X\n", $buf_ofs,ord($m));
          takeFill($fh); $ecs_ok = 0;
          @r = (2,"Trailing garbage, pos=$buf_ofs")  if $buf_l>0 && !$r[0];
          last;
        } else {  # marker segment
          $ecs_ok = $m eq "\xda";  # SOS
          my($len) = unpack("n",substr($buf,0,2));
#         printf("marker segm len %d, pos=%d, marker=0x%02X\n", $len,$buf_ofs,ord($m));
          @r = (1,sprintf("Invalid marker segm len %d, pos=%d, marker=0x%02X",
                          $len,$buf_ofs,ord($m)) )  if $len<2;
          my($err) = takeN($fh,$len);
          @r = (2,"$err, pos=$buf_ofs")  if defined $err && !$r[0];
        }
      } else { die "jpeg: Panic, program error2, pos=$buf_ofs" }
      $buf_l>=2 || makeTwo($fh);
    }
  }
  close($fh) or die "jpeg: Can't close $fn: $!";
  $r[1] = "bad jpeg: ".$r[1]  if $r[0];
  @r;
}

1;  # insure a defined return
