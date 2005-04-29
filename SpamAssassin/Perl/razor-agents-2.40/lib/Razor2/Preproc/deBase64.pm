package Razor2::Preproc::deBase64;


sub new { 
    return bless {}, shift;
}


sub isit {
    my ($self, $text) = @_;
    return $$text =~ /^Content-Transfer-Encoding: base64/sim;
}


sub doit {
    my ($self, $text) = @_;

    my ($hdr, $body) = split /\n\r*\n/, $$text, 2;

    $body =  $self->extract_base64($text);

    $body =~ tr|A-Za-z0-9+=/||cd;              # remove non-base64 chars
    $body =~ s/=+$//;                          # remove padding
    $body =~ tr|A-Za-z0-9+/| -_|;              # convert to uuencoded format

    my $decoded = '';
    while ($body =~ /(.{1,60})/gs) {
        my $len = chr(32 + length($1)*3/4);    # compute length byte
        $decoded .= unpack("u", $len . $1 );   # uudecode
    }

    $$text = "$hdr\n\n$decoded";
}


sub extract_base64 {
    my ($self, $text) = @_;

    if ($$text =~ /Content-Transfer-Encoding: base64/si) {
        $' =~ /\r?\n\r?\n([^=]*)/s;  # match to end of data or '='
        return $1 . "==";
    }
    return undef;
}


1;

