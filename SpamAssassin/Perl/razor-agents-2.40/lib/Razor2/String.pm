# $Id: String.pm,v 1.1 2004/04/19 17:50:31 dasenbro Exp $
package Razor2::String;

use Digest::SHA1 qw(sha1_hex);
use URI::Escape;
use Razor2::Preproc::enBase64;

#use MIME::Parser;

require Exporter;
use vars qw ( @ISA $VERSION @EXPORT );
@ISA = qw(Exporter);

@EXPORT = qw( hmac_sha1 xor_key 
              from_batched_query 
              to_batched_query findsimilar debugobj
              makesis parsesis makesis_nue parsesis_nue 
              hextobase64 base64tohex 
              randstr round 
              hex_dump prep_mail
              prehash printb64table
              hexbits2hash hmac2_sha1
              fisher_yates_shuffle
            );


# Same as the alphabet from RFC 1521, except s:/:_: and s:+:-:
my %b64table; 

BEGIN { 
    # ASCII 
    # 33-126 printable chars
    # 48-57  numbers
    # 65-90  uppercase alpha
    # 97-122 lowercase alpha
    foreach  (0..25) { $b64table{$_} = chr($_ + 65); }
    foreach (26..51) { $b64table{$_} = chr($_ + 71); }
    foreach (52..61) { $b64table{$_} = chr($_ - 4 ); }
    $b64table{62} = "-";
    $b64table{63} = "_";
}


sub printb64table {
    foreach (0..63) {
        print "$_ = $b64table{$_}\n";
    }
}


sub hmac_sha1 {
    my $text = shift;
    my $iv1  = shift;
    my $iv2  = shift;
    my ($b64, $hex) = hmac2_sha1($text, $iv1, $iv2);
    return $b64;
}


# taken in part from RFC 2104 
# http://www.cs.ucsd.edu/users/mihir/papers/hmac.html

sub hmac2_sha1 {
    my $text = shift;
    my $iv1  = shift;
    my $iv2  = shift;

    return unless $text && $iv1 && $iv2;
    die "no ref's allowed" if ref($text);

    my $ctx = Digest::SHA1->new;
    $ctx->add($iv2);
    $ctx->add($text);
    my $digest = $ctx->hexdigest;

    $ctx = Digest::SHA1->new;
    $ctx->add($iv1);
    $ctx->add($digest);
    $digest = $ctx->hexdigest;

    return (hextobase64($digest), $digest);
}


sub hmac3_sha1 {
    my $text = shift;
    my $iv1  = shift;
    my $iv2  = shift;

    return unless $text && $iv1 && $iv2;
    die "no ref's allowed" if ref($text);

    my $digest = $text;
    $digest = sha1_hex($iv1 . $digest);
    $digest = sha1_hex($iv2 . $digest);
    return (hextobase64($digest), $digest);
}


# part of RFC 2104 - see hmac_sha1()

sub xor_key {
    my $key  = shift;

    # key length should never be > 64 chars;
    #
    # dont need this ... see Bitwise String Operators
    # $enc .= '\0' x (64 - length($pass));

    my $iv1 = "\x36" x 64 ^ $key;
    my $iv2 = "\x5C" x 64 ^ $key;

    return ($iv1, $iv2);
}


# converts a string where each char is a hex (4-bit) value
#       to a string where each char is a base64 (6-bit) value

sub hextobase64 {

    my $hs = shift;

    my @b64s;
    my $i = 0;

    while ($i < length($hs)) {

        # process 3 hex char chunks at a time
        my $hex3 = substr $hs, $i, 3;
        $i += 3;  

        my $bv = pack "h3", $hex3;  
        my $cur = 0;
        foreach (0..5)  { my $bt = vec($bv,$_,1); $cur += $bt; $cur *= 2; }
        push @b64s, $cur/2;  $cur = 0;
        foreach (6..11) { my $bt = vec($bv,$_,1); $cur += $bt; $cur *= 2; }
        push @b64s, $cur/2;
        #foreach (0..15) { my $bt = vec($bv,$_,1); print "$_=$bt, cur=$cur\n"; }
        #print " -- hex=$hex3; @b64s\n";
    }

    my $bs = "";
    foreach (@b64s) { $bs .= $b64table{$_}; } 

    # print "b64=$bs; hex=". base64tohex($bs) ."\n";

    # Fixme -  change encoding so 1 hex char ==> 1 b64 char
    # 64-char hex string ==> 44-char b64 string.  truncate to 43.
    # 40-char hex string ==> 28-char b64 string.  truncate to 27.
    # $bs = substr($bs, 0, 43) if (length $bs == 44) && (substr($bs, -1) eq '0');
    # $bs = substr($bs, 0, 27) if (length $bs == 28) && (substr($bs, -1) eq '0');

    return $bs;

} 


# converts a string where each char is a base64 (6-bit) value
#       to a string where each char is a hex (4-bit) value

sub base64tohex {

    my $bs = shift;
    my @b64s;
    my $hexstr;

    # convert string to list of numbers base 10
    foreach my $chr (split '', $bs) { 
        foreach (keys %b64table) { 
            push @b64s, $_ if $b64table{$_} eq $chr;
        }
    }

    while (@b64s) {
        my $bv = ""; vec($bv,0,16) = 0;
        my $a = shift @b64s; 
        foreach (0..5)  {my $i=5-$_; my $bt=$a%2; vec($bv,$i,1) = $bt; $a = int($a/2); }
        $a = shift @b64s; 
        foreach (6..11) {my $i=17-$_;my $bt=$a%2; vec($bv,$i,1) = $bt; $a = int($a/2); }
        $hexstr .= unpack "h3", $bv;
    }

    # print "hexstr=$hexstr; @b64s\n";

    #
    # NOTE on padding  
    # if we pad 4 0-bits, we need to know that there wasn't an actual 0
    # on the input string (hexstr).  
    #
    # since padding 4 0's is more common than having the last hex 
    # be a 0, we could append a special char indicating last 4 0 bits
    # were not padding 0's. 
    #
    # But, we will customize these functions for razor2's needs.
    # 64-char hex string ==> 43-char b64 string ==> 66-char hex.  truncate.
    # 40-char hex string ==> 27-char b64 string ==> 42-char hex.  truncate.
    # 15-char hex string ==> 10-char b64 string ==> 15-char hex.  ok.
    #
    # 20-byte hex string is 40 chars
    # $hexstr = substr($hexstr, 0, 20) if (length $hexstr == 21) && (substr($hexstr, -1) eq '0');
    # $hexstr = substr($hexstr, 0, 40) if (length $hexstr == 42) && (substr($hexstr, -2) eq '00');
    # $hexstr = substr($hexstr, 0, 64) if (length $hexstr == 66) && (substr($hexstr, -2) eq '00'); 

    $hexstr = substr($hexstr, 0, 40) if (length($hexstr) == 42);
    $hexstr = substr($hexstr, 0, 64) if (length($hexstr) == 66);
    return $hexstr;

}


# can be called 2 ways
# - makesis(%hash)    aka  makesis( p => 0, cf => 95 )
# - makesis($hashref) aka  makesis({p => 0, cf => 95})

sub makesis {   
    my $first = shift;
    my $data; 
    if (ref($first) eq 'HASH') {
        $data = $first;
    } else {
        $data = {$first, @_};
    }
    my $sis = '';
    foreach (sort keys %$data) { 
        $sis .= "$_=" . (exists $data->{$_} ? uri_escape($data->{$_}) : '') . '&';
    }

    # This is 10x faster than the equivalent regex version. 
    return substr($sis, 0, length($sis)-1) . "\r\n";
}


sub parsesis { 
    
    my $query = $_[1] || {};
    my $wantref = 1 if $_[1]; 

    # Parse the query.

    $_[0] =~ s/\n$//;  # SIS shouldn't have this!
    $_[0] =~ s/\r$//;  # SIS shouldn't have this!

    my @pairs = split /\&/, $_[0];

    for (@pairs) {
        my ($key, $value) = split /=/, $_;
        $query->{$key} = defined $value ? uri_unescape($value) : '';
    }

    return $query if $wantref;
    return %$query;
}


# version of makesis that doesn't to uri escaping
# for things we know don't require escaping

# can be called 2 ways
# - makesis(%hash)    aka  makesis( p => 0, cf => 95 )
# - makesis($hashref) aka  makesis({p => 0, cf => 95})

sub makesis_nue {   
    my $first = shift;
    my $data; 
    if (ref($first) eq 'HASH') {
        $data = $first;
    } else {
        $data = {$first, @_};
    }
    my $sis = '';
    foreach (sort keys %$data) { 
        $sis .= "$_=";
        $sis .= $data->{$_} if exists($data->{$_});
        $sis .= '&';
    }

    # This is 10x faster than the equivalent regex version. 
    return substr($sis, 0, length($sis)-1) . "\r\n";
}


sub parsesis_nue { 

    my $query = $_[1] || {};
    my $wantref = 1 if $_[1]; 

    # Parse the query.
    $_[0] =~ s/\r\n$//;
    my @pairs = split /\&/, $_[0];

    for (@pairs) {
        my ($key, $value) = split /=/, $_;
        $query->{$key} = $value;
    }

    return $query if $wantref;
    return %$query;
}


sub to_batched_query { 
    my ($queries, $bql, $bqs, $novar) = @_;
    my @bqueries;

    # Breaks up queries into batches, where batches are limited to:
    # - at most $bql lines long --OR--
    # - at most $bqs kb in size
    # if bqs or bql == 0 or undef, no limit. 
    #
    # fixme - optimization for aggregator:
    #         sort, so all checks are together, all reports together, etc.
    #         problem is user will want to maintain array order 

    # $queries is array ref of either:
    #   strings  - sis, ready to go  
    #   hash ref - need to create sis 
    # my $q = ref($queries->[0]) eq 'HASH' ?  makesis_batch($queries) : $queries; 

    # for right now, we'll just assume hash ref
    return unless ref($queries->[0]) eq 'HASH';

    my $last; 
    my $line;
    my $linecnt = 0;
    my $batchmode = 0;  
    foreach my $cur (@$queries) {

        # my $dobj = debugobj($cur); print "dbg-doing obj: $dobj\n";

        #
        # handle cases where we submit email blob (message = * )
        #
        if (exists $cur->{message})  {
            my $msg = $cur->{message};
            delete $cur->{message};
            $line = "-". makesis($cur);
            $cur->{message} = $msg;  
            $line =~ s/\r\n$//s;
            $line .= "&message=*\r\n$msg\r\n.\r\n";
            push @bqueries, $line;
            next;
        }

        unless ($last) {
            #
            # start beginning of new batch
            #
            $last = $cur;
            next;
        }
        unless ($batchmode) {
            #
            # line after beginning of new batch
            # if similar, start variable batchmode. 
            # if not,     start batchmode without variables
            #
            my ($both, $diff) = findsimilar($last, $cur);
            if ($diff && !$novar) {
                $batchmode = 2;
                $line  = "-". makesis_nue($both);
                # fixme - we might want to uri_escape() 
                # but everything should be alphanum or our uri-safe base64
                $line .= join(",", map "$last->{$_}", @$diff) ."\r\n"; 
                $line .= join(",", map  "$cur->{$_}", @$diff) ."\r\n";
                $last = $both;  # last is now 'template'
                $linecnt = 2;
            } else {
                $batchmode = 1;
                $line = "-". makesis($last);
                $line .= makesis_nue($cur);
                $linecnt = 2;
            }
            next;
        } else { 
            #
            # We're in batchmode.
            # end if batch maxed out (bqs or bql reached)
            # end if batchmode with variables and cur doesn't match
            # end batch
            #
            my ($both, $diff) = findsimilar($last, $cur) if ($batchmode == 2);
            if (    ($bqs && (length($line) > ($bqs*1024))) ||
                    ($bql && ($linecnt >= $bql)) ||
                    ($batchmode == 2 && !$diff)  ) {
                $batchmode = 0;
                $line .= ".\r\n";
                push @bqueries, $line;
                $last = $cur;
            } else {
                #
                # fixme - we might go passed bqs by a little bit. prolly ok.
                #
                if ($batchmode == 2) {
                    $line .= join(",", map  "$cur->{$_}", @$diff) ."\r\n";
                } else {
                    $line .= makesis_nue($cur);
                }
                $linecnt++;
            }
        } 
    }
    if ($batchmode) {
        $line .= ".\r\n";
        push @bqueries, $line;
    } elsif ($last) {
        $line = makesis($last);
        push @bqueries, $line;
    }

    return \@bqueries;
} 


# compares keys in hash ref's a & b
#
# return 
#   if both hashes have different keys
#
# return (1)
#   if both hashes have same keys and values,
#
# returns 2 refs
#   if both hashes have same keys but different values
#   - first  is hash, copy of a & b where vals are same.
#     where vals are diff, keys are copied with val = '?'
#   - second is list contains keys where values are different

sub findsimilar {
    my ($a, $b) = @_;
    my @diffvalues = ();
    my %samevalues = ();

    foreach (sort keys %$a) {
        return unless exists $b->{$_};
        if ($b->{$_} eq $a->{$_}) {
            $samevalues{$_} = $a->{$_}; 
        } else {
            $samevalues{$_} = "?";
            push @diffvalues, $_;
        }
    }
    foreach (sort keys %$b) {
        return unless exists $a->{$_};
    }
    # if too hashes are exactly the same, not sure.
    # treat as if they are totally different.
    return (1) unless scalar(@diffvalues) > 0;  

    return (\%samevalues, \@diffvalues);
}

sub from_batched_query { 

    my ($queries) = @_;
    my @queries; 

    my ($fq, $rq) = $queries =~ m:^\-(.*?)\r\n(.*)$:sm;
    
    unless ($fq && $rq) {
        # allow from_batched_query to handle non-batches
        $fq = $queries;
        $rq = "";
    }

    if ($fq =~ m:\?:) { 

        my %template_query = ();
        my @seq = ();
        my @pairs = split /\&/, $fq;
        for (@pairs) { 
            my ($key, $value) = split /=/, $_;
            if ($value eq "?") {
                push @seq, $key;
            } else { 
                $template_query{$key} = $value ? uri_unescape($value) : '';
            }
        }

        for (split /\r\n/, $rq) { 
            my @values = split /,/, $_;
            my %foo = %template_query;
            @foo{@seq} = @values;
            push @queries, \%foo;
        }

        return undef unless @queries;

    } elsif ($fq =~ m:\*:) { 

        my %query = parsesis($fq);
        for (keys %query) { 
            if ($query{$_} eq "*") { 
                $query{$_} = $rq;
                last;
            }
        }
        push @queries, \%query;

    } else { 
    

        # Don't split $queries.  Use $fq and $rq instead since 
        # $fq is already normalized.

        my %q = parsesis($fq);
        push @queries, \%q; 
        for (split /\r\n/, $rq) { 
            my %q = parsesis($_);
            push @queries, \%q;
        }

    }

    return \@queries;
      
}


sub randstr {

    my $size = shift;
    my $alphanum = shift;
    my $str;

    $alphanum = 1 if !defined($alphanum);

    # ASCII
    # 33-126 printable chars
    # 48-57  numbers
    # 65-90  uppercase alpha
    # 97-122 lowercase alpha

    while ($size--) {
        if ($alphanum) {
            $str .= $b64table{ int(rand 64) };
        } else {
            $str .= chr(int(rand 94) + 33);
        }
    }

    return $str;

}


sub escape_smtp_terminator {

    my ($textref) = @_;
    $$textref =~ s/\r\n\./\r\n\.\./gm

}


sub unescape_smtp_terminator { 

    my ($textref) = @_;
    $$textref =~ s/\r\n\.\./\r\n\./gm;

}


sub hex_dump { 
    my $string = shift;

    for (split //, $string)  {
        print ord($_) . " ";
    }
    print "\n";
}



sub hash2str { 

    my $href = shift; 
    my %hash = %$href; 
    my ($str, $key);

    for $key ( keys %hash ) { 
        my $tstr;
        if ( ref $hash{$key} eq 'ARRAY' ) { 
            for ( @{ $hash{ $key }} )  { $tstr .= escape( $_ ) . "," } $str =~ s/,$//;
        } elsif ( !(ref $hash{$key}) ) { 
            $tstr .= escape ( $hash{$key} );
        }
        if ( $tstr ) { $str .= "$key:$tstr&" }
    }

    $str =~ s/&$//; return $str;

}


sub str2hash { 

    my $str = shift; 
    my %hash;
    my @pairs = split /(?<!\\)&/, $str; 

    for ( @pairs ) { 
        my ( $key, $data ) = split /(?<!\\):/, $_, 2;
        if ( $data =~ /(?<!\\),/ ) { 
            my @list = split /(?<!\\),/, $data; 
            for ( @list ) { $_ = unescape ( $_ ) };
            $hash{$key} = [@list];
        } else { $hash{$key} = unescape ( $data ) } 
    }

    return \%hash; 

}

#
# If body of an email has mime attachments, the headers
# will indicate this.  likewise, each mime attachment
# could also have nested mime attachments with headers that
# must indicate this.  standard recursion.
#
# However, all 'leaf node' attachments don't have to have
# headers based on RFC xxx.  They must be created before 
# sending to razor servers. 
#
#
# Example of mail with nested MIME attachments:
#
#     level 1     level 2      level 3  ....
#    --------------------------------------
# * - Header 1
# * - Body   1
#      * -- A -- mime-header 2A
#      |         mime-body 2A
#      |            |
#      |            * ---- a --- mime-header 3a
#      |            |            mime-body   3a
#      |            |
#      |            * -----b --- mime-header 3b
#      |                         mime-body   3b
#      * -- B -- mime-body 2B
#      |         
#      * -- C -- mime-header 2C
#                   |
#                   * ---- c -- mime-body   3c
#                   |
#                   * ---- d -- mime-header 3d
#                               mime-body   3d
#
# should be reported as 
#
# Header 1 \r\n
# part 1 = p(header 3a, body 3a) \r\n
# part 2 = p(header 3b, body 3b) \r\n
# part 3 = p(<generated header 2B>, body 2B) \r\n
# part 4 = p(<generated header 3c>, body 3c) \r\n
# part 5 = p(header 3d, body 3d) \r\n
# .\r\n
#
# Notes:
# - Order of parts does not matter.
#
# - Each part is processed by prep_mail, p(), before report/check
#
# - Except for original Header everything but leaf nodes
#   are discarded.  In the above example, 
#
#   Body 1, header 2A, header 2C - are discarded
#
#
# Detailed Explanation:
#
# Header 1 says 'Content-Type: multipart' with boundary definition
# Based on the Boundary, Body 1 is split into A, B, C.
# 
# A is analyzed, has headers which also say 'Content-Type: multipart' 
# with a different boundary, and it is split into 3a, 3b. 2A is what
# appears between header 2a and first boundary, so its ignored.
# 3a and 3b both have header info, so they are sent thru prep_mail
# and reported/checked
#
# <generated header 2B>  is based on Header 1 to determine content
#                        type.  if unknown, dummy header is added, 
#                        and both are reported as a body part
# 
# C is analyzed, has headers which also say 'Content-Type: multipart' 
# with a different boundary, and it is split into 3c, 3d. 
# 
# <generated header 3c>  is based on header 2c to determine content
#                        type.  if unknown, dummy header is added,
#                        and both are reported as a body part
#  
# 3d has header info, so header+body are sent thru prep_mail
# and reported/checked 
#  
#  
#  
# prep_mail() basically truncates msgs that are too big and/or
# base64 encodes binaries or 8-bit msgs.
#  
 
 
# Split mime splits up multi-part mime mails.
#
# returns array of parts, where each part is
# headers\n\nbody
#
# headers will only contain X-Razor2 and Content- headers
#
# If not a mime mail, and the headers do not have any
# Content-* headers, then the only headers will be X-Razor2 ones
# (perhaps create Content-Type in da future?)
#
# body can be blank.  nuked in prep_part
#
sub split_mime {

    my ($mailref, $ver, $recursive, $debug ) = @_;

    return unless ref($mailref);

    # mime-bodies must have header or initial blank lines.
    # 
    my ($hdr, $body) = split /\n\r*\n/, $$mailref, 2;
    my $no_valid_mime_hdr = 0;

    unless ($body) {

        # no blank lines, definately no header, so no nested mimes

        print "split_mime: no blank lines\n" if $debug > 1;
        $no_valid_mime_hdr = 1;
    } 
    # fixme - handle attachments?  i.e. if header has this
    # Content-Disposition: attachment
    # than body is mail,  we could recursively call ourselves
    # again with body (check body for hdrs first?)

    # Make sure $hdr is really a hdr
    # 
    # Details: If mime part is not RFC compliant, it could just
    # be a body with blank lines.  hdr could have just matched part
    # of the body.   
    # 
    # valid mime header is determined by existance of 'Content-Type' 
    # If we're not recursive, we don't check orig_headers, we assume its ok.
    # not sure if this is the best way ...
    # 

    if ($recursive && ($hdr !~ /^Content-Type:/i)) {
        $no_valid_mime_hdr = 1;
        print "uh-oh, bad mime-body len=". length($$mailref) .":\n$$mailref\n" if $debug;
        #print "split_mime: recur=($recursive)\n";
    }

    if ($no_valid_mime_hdr) {
        #
        # create dummy header and return it
        #
        # $ver should be '1' or client name + version
        my $mimepart = "X-Razor2-Agent: $ver\n";
        my $hrdlen = length($mimepart);

        # if it has initial blank line, hurray for rfc compliance
        if ($$mailref =~ /^\n/) {
            $mimepart .= $$mailref;
        } else {
            $mimepart .= "\n". $$mailref;
        }
        print "split_mime: returning total_len=". length($mimepart) ."; hdrs=".
            $hdrlen .", body=". length($$mailref) ."\n" if $debug;
        return (\$mimepart);
    }

    #
    # Now we split mailref into hdr and body 
    # check hdr for nested mime (boundary)
    #

    my $orig_hdr = $hdr;
    $hdr =~ s/\n\s+//sg;  # merge multi-line headers
    # nuke everything but X-Razor2 and Content-* headers
    my $trimmed_hdr = "";
    foreach (split '\n',$hdr) {
        /^Content-/i and $trimmed_hdr .= "$_\n";
        /^X-Razor2/i and $trimmed_hdr .= "$_\n";
    }

    my $boundary = "";

    if ($trimmed_hdr =~ /Content-Type: multipart.+boundary=("[^"]+"|\S+)/ig) { 
        $boundary = $1;
    }

    if ($boundary eq "") {
        #
        # valid mime hdr, but no nested mime. 
        # add razor hdr and return.
        #
        print "split_mime: valid_mime_hdr [len=". length($orig_hdr) 
            ."], but no nested mime\n$orig_hdr\n" if $debug > 1;
        $trimmed_hdr = "X-Razor2-Agent: $ver\n" . $trimmed_hdr;
        my $mimepart = "$trimmed_hdr\n$body";
        print "split_mime: returning total=". length($mimepart) ."; hdrs=".
            length($trimmed_hdr) .", body=". length($body) ."\n" if $debug;
        return (\$mimepart);
    }
    $boundary = $1 if $boundary =~ /^"(.*)"$/;
    
    # At this point,  we know body has mime parts.
    #
    my @mimeparts;

    #
    # According to RFC 1341
    # http://www.w3.org/Protocols/rfc1341/7_2_Multipart.html
    #
    # mimes are separated by \n--boundary\n
    # and are followed immediately by  header, blank line, body;
    # or blank line and body.
    #
    # if no header in mime part, default content type for mime body is 
    # based on header where 'Content-Type: multipart*' was defined, where
    #   multipart/digest --> message/rfc822
    #   multipart/*      --> text/plain
    # perhaps we should add a header if none present? 
    # 
    # if a body contains mimes, the 'preable', or stuff before 
    # the first boundary, and the 'epilogue', the stuff after the
    # last boudary, are to be ignored.
    #
    # NOTE: We split up multiparts, but content-type's can also be 
    #       nested.  i.e, a header of 'Content-Type: message' can have a body
    #       of 'Content-Type: image'
    #
    $body =~ s/\n\Q--$boundary--\E.*$//sg;    # trash last boundary and epilogue
    if ($body =~ /^\Q--$boundary\E\r*\n/) {
        # bug in some mails, make it RFC compliant
        # now our split will work correctly
        print "bad mime body [len=". length($body) ."], not doing \\n--boundary, fixed tho.\n" if $debug > 1;
        $body = "garbage\n$body";  
    }
    my @tmpparts = split /\n\Q--$boundary\E\r*\n/, $body;
    shift @tmpparts;  # trash everything up to the first boundary;
    foreach (@tmpparts) {
        # perhaps we should add a header based on default content-type? 
        unless (/\S/s) {
            print "skipping body part containing only whitespace [len=". length($_) ."]\n" if $debug;
            next;
        }
        print "boundary:  ". $recursive . "$boundary\n" if $debug > 1;
        push @mimeparts, split_mime(\$_, $ver, "  ". $recursive, $debug);
    }
    print "Saweeet!!! Boundary (". scalar(@mimeparts) ."): $boundary\n" if defined($boundary) && ($debug > 1);

    return @mimeparts;
}

# mailref is not modified by this sub
#
sub prep_part {
    my ($mailref, $maxheader, $maxbody) = @_;
        #print "[". length($$mailref) ."] maxsize=$maxheader + $maxbody\n";

    my ($hdr, $body) = split /\n\r*\n/, $$mailref, 2;
    $hdr .= "\n";  # put newline back on last header line

    unless ($body) {
        # 
        # fixme - this should not happen. 
        # if it does, split_mime needs work
        # 
        # print "prep_part got F**KED-up mimepart [len=". length($$mailref) ."]\n$$mailref\n";
        return;  # body is empty
    }

    # fixme - are these the best chars to check for binary?
    my $is_binary = ($hdr =~ /^Content-Type-Encoding: 8-bit/) ||
        ($body =~ /([\x00-\x1f|\x7f-\xff])/ and $1 !~ /[\r\n\t]/); 

    my $enBase64 = new Razor2::Preproc::enBase64;
    $is_binary = $enBase64->isit($mailref);
    $enBase64->doit(\$body) if $is_binary;

    $body =~ s/\r+\n/\n/sg;  # outlook sometimes does \r\r\n
    $hdr  =~ s/\r+\n/\n/sg;

    if ((my $len = length($body)) > $maxbody) {
        $body = substr $body, 0, $maxbody;
        substr($body, -2) = "==" if $is_binary;

        $hdr  = "X-Razor2-Origlen-Body: $len\n" . $hdr;
        #print "maxbody=$maxbody body went from $len to ". length($body) ."\n";
    }
    if ((my $len = length($hdr)) > $maxheader) {
        $hdr  = "X-Razor2-Origlen-Header: $len\n" . $hdr;
        while (length($hdr) > $maxheader) {
            $hdr =~ s/.*\n$//;  # remove last line of headers
        }
        #print "maxhdr=$maxheader header went from $len to ". length($hdr) ."\n";
    }

    my $dude = "$hdr\n$body";

    return $mailref if $dude eq $$mailref;  # this happens majority of the time
    return \$dude;
}



# NOTE: Important function!
#       *must* be kept in sync with server and all clients
#       same holds true for prep_part()
#
# This is the preprocessing done on a mail before sent over network
#
sub prep_mail {
    my ($mailref, $report_headers, $maxheader, $maxbody, $maxorighdr, $versionstring, $debug) = @_;
    return unless ref($mailref);

    print " prep_mail: orig=". length($$mailref) ."\n" if ($debug > 1);
    
    my ($orig_hdr) = split /\n\r*\n/, $$mailref, 2;
    $orig_hdr .= "\n";  # put newline back on last header line

    my $ver = $versionstring || 1;
    my @mimeparts = split_mime($mailref, $ver, 0, $debug);

    my @mimeparts_prep;
    foreach (@mimeparts) {
        push @mimeparts_prep, prep_part($_, $maxheader, $maxbody);
    }
    unless ($report_headers) {
        my $hdr = "X-Razor2-Headers-Suppressed: 1\n";
        foreach (split '\n',$orig_hdr) {
            /^Content-/i and $hdr .= "$_\n";
            /^X-Razor2/i and $hdr .= "$_\n";
        }
        $orig_hdr = $hdr;
    }

    if ((my $len = length($orig_hdr)) > $maxorighdr) {
        $hdr  = "X-Razor2-Origlen-Header: $len\n" . $orig_hdr;
        while (length($hdr) > $maxorighdr) {
            $hdr =~ s/.*\n$//;  # remove last line of headers
        }
        #print "max=$maxorighdr orig_header went from $len to ". length($hdr) ."\n";
        $orig_hdr = $hdr;
    }

    if ($debug > 1) {
    print "**** prep_mail done: headers=". length($orig_hdr);
    foreach (0..$#mimeparts_prep) { 
       print "\n**** mail $_ [". length(${$mimeparts_prep[$_]}) ."] ". substr(${$mimeparts_prep[$_]} ,0,40); 
    }
    print "\n\n";
    }

    return (\$orig_hdr, @mimeparts_prep);
}



# from MIME::Parser
    #my $parser = new MIME::Parser;
    #my $entity = $parser->parse($body);
    # foreach (dump_entity($entity)) 
sub dump_entity {
    my $ent = shift;
    my @parts = $ent->parts;

    if (@parts) {        # multipart...
        map { dump_entity($_) } @parts;
    } else {               # single part...
        return ( $ent->body );  # return text blob
print "    Part: ", $ent->bodyhandle->path, " (", scalar($ent->head->mime_type), ")\n";
    }
}


# input:  hex string ("2D")
# output: hash ref or array containg bits that are set
#         2D == (1, 3, 4, 6)

sub hexbits2hash {
    my $hex = shift;

    my %h;
    my $i = 0;
    foreach (reverse split '', unpack "B*", pack "H*", $hex) {
        $i++;
        $h{$i} = 1 if  $_ eq 1;
    }

    return wantarray ? (sort keys %h) : \%h;
}


# input:  hash ref, array ref, or array containg bits that are set
# output: hex string ("2D")
#         2D == (4, 8, 32)

sub hash2hexbits {
    my @bits = @_;
 
    @bits = @{$bits[0]}             if ref($bits[0]) eq 'ARRAY';  
    @bits = (sort keys %{$bits[0]}) if ref($bits[0]) eq 'HASH';  

    my @all;
    my $i = 1;
    foreach (sort {$a <=> $b} @bits) {
        while (1) { 
            push @all, 1 if $_ == $i;
            last if $_ == $i;
            push @all, 0;
            $i++;
        }
    }
    my $bs = join '', reverse @all;
    # fixme needs testing
    my $hex = (unpack "H*", pack "B*", join '', reverse @all);

    return $hex
}

# for debugging - dumps a obj to a string 
sub debugobj {
    my ($obj, $prefix, $maxwidth) = @_;

    $maxwidth ||= 70;
    return if (defined($prefix) && length($prefix) > $maxwidth);
   
    my $line = "";
    $prefix .= " "x4;       

    if (my $r = ref($obj)) {
        if ($r eq 'HASH') { 
            $line = "$r - $obj,". scalar(keys %$obj) ." keys\n";
            foreach (sort keys %$obj) {
                $line .= "$prefix$_ => ". debugobj($obj->{$_}, $prefix);
            } 
            $line .= $prefix ."[empty]\n" unless (keys %$obj);
        } elsif ($r eq 'ARRAY') {
            $line = "$r - $obj,". scalar(@$obj) ." items\n";
            foreach (@$obj) {
                $line .= $prefix . debugobj($_, $prefix);
            } 
            $line .= $prefix ."[empty]\n" unless (@$obj);
        } elsif ($r eq 'REF') {
            $line = "$r - $obj\n";
            $line .= $prefix . debugobj($$obj, $prefix);
        } elsif ($r eq 'SCALAR') {
            $line = "$r - $obj\n";
            $line .= $prefix . debugobj($$obj, $prefix);
        } 
    } else {
        if (defined $obj) {
            $line = $1 if substr($obj, 0, $maxwidth-length($prefix)) =~ /^([^\n]+)/;
            $line = "[length=". length($obj) ."] ". $line 
                if (length($line) ne length($obj));
        } else {
            $line = "[empty]";
        }
        $line .= "\n";
    }
    return $line;
}


sub clean_body {

    my ($self, $bodyref) = @_;
   
    my $hasheaders = 1; 

    if ($self->{preprocs}->{deBase64}->isit($bodyref)) { 
        $self->{preprocs}->{deBase64}->doit($bodyref);
        $hasheaders = 0;
    }

    if ($self->{preprocs}->{deQP}->isit($bodyref)) { 
        $self->{preprocs}->{deQP}->doit($bodyref);
        $hasheaders = 0;
    }

    if ($self->{preprocs}->{deHTML}->isit($bodyref)) { 
        $self->{preprocs}->{deHTML}->doit($bodyref);
    }

    if ($hasheaders) { 
        $$bodyref =~ s/^.*?\n\n//s;
    }


}


sub round { 

    my $float = shift;
    return sprintf("%.0f", $float);

}



sub fisher_yates_shuffle {
   my $deck = shift;  # $deck is a reference to an array
   my $i = @$deck;
   while ($i--) {
       my $j = int rand ($i+1);
       @$deck[$i,$j] = @$deck[$j,$i];
   }
}


1;

