package Razor2::Preproc::enBase64;


sub new { 
    return bless {}, shift;
}


sub isit {
    my ($self, $text) = @_;
    my $is_binary = ($$text =~ /^Content-Type-Encoding: 8-bit/) ||
              ($$text =~ /([\x00-\x1f|\x7f-\xff])/ and $1 !~ /[\r\n\t]/);

    return $is_binary;
}

sub doit {
    my ($self, $text) = @_;

    pos($$text) = 0;                          # ensure start at the beginning

    my $res = join '', map( pack('u',$_)=~ /^.(\S*)/, ($$text =~ /(.{1,45})/gs));

    $res =~ tr|` -_|AA-Za-z0-9+/|;               # `# help emacs
    # fix padding at the end
    my $padding = (3 - length($$text) % 3) % 3;
    $res =~ s/.{$padding}$/'=' x $padding/e if $padding;

    # split into lines 
    $res =~ s/(.{1,76})/$1\n/g;

    $res = "Content-Transfer-Encoding: base64\n\n$res";

    $$text = $res;
}

1;

