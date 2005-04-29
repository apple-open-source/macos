use Test::More 'no_plan';
use strict;

use Cwd;
use IO::File;
use File::Path;
use File::Spec          ();
use File::Spec::Unix    ();
use File::Basename      ();

use Archive::Tar;
use Archive::Tar::Constant;
    
my $tar     = Archive::Tar->new;
my $tarbin  = Archive::Tar->new;
my $tarx    = Archive::Tar->new;

for my $obj ( $tar, $tarbin, $tarx ) {
    isa_ok( $obj, 'Archive::Tar', 'Object created' );
}

my $file = qq[directory/really-really-really-really-really-really-really-really-really-really-really-really-really-really-really-really-really-really-really-really-really-really-really-really-really-really-really-really-long-directory-name/myfile];

my $expect = {
    c       => qr/^iiiiiiiiiiii\s*$/,
    d       => qr/^uuuuuuuu\s*$/,
};

my $all_chars = join '', "\r\n", map( chr, 0..255 ), "zzz\n\r";

### @expectbin is used to ensure that $tarbin is written in the right   ###
### order and that the contents and order match exactly when extracted  ###
my @expectbin = (
    ###  filename      contents       ###
    [    'bIn11',      $all_chars x 11 ],
    [    'bIn3',       $all_chars x  3 ],
    [    'bIn4',       $all_chars x  4 ],
    [    'bIn1',       $all_chars      ],
    [    'bIn2',       $all_chars x  2 ],
);

### @expectx is used to ensure that $tarx is written in the right       ###
### order and that the contents and order match exactly when extracted  ###
my $xdir = 'x';
my @expectx = (
    ###  filename      contents    dirs         ###
    [    'k'   ,       '',         [ $xdir ]    ],
    [    $xdir ,       'j',        [ $xdir ]    ],   # failed before A::T 1.08
);

### wintendo can't deal with too long paths, so we might have to skip tests ###
my $TOO_LONG    =   ($^O eq 'MSWin32' or $^O eq 'cygwin') 
                    && length( cwd(). $file ) > 247; 

if( $TOO_LONG ) {
    SKIP: {
        skip( "No long filename support - long filename extraction disabled", 0 );
    }      
} else {
    $expect->{$file} = qr/^hello\s*$/ ;
}

my @root = grep { length }   File::Basename::dirname($0), 
                            'src', $TOO_LONG ? 'short' : 'long';

my $archive        = File::Spec->catfile( @root, 'bar.tar' );
my $compressed     = File::Spec->catfile( @root, 'foo.tgz' );  
my $archivebin     = File::Spec->catfile( @root, 'outbin.tar' );
my $compressedbin  = File::Spec->catfile( @root, 'outbin.tgz' );
my $archivex       = '0';
my $compressedx    = '1';
my $zlib           = eval { require IO::Zlib; 1 };
my $NO_UNLINK      = scalar @ARGV ? 1 : 0;

### error tests ###
{
    local $Archive::Tar::WARN  = 0;
    my $init_err               = $tar->error;
    my @list                   = $tar->read();
    my $read_err               = $tar->error;
    my $obj                    = $tar->add_data( '' );
    my $add_data_err           = $tar->error;

    is( $init_err, '',                  "The error string is empty" );
    is( scalar @list, 0,                "Function read returns 0 files on error" );
    ok( $read_err,                      "   and error string is non empty" );
    like( $read_err, qr/create/,        "   and error string contains create" );
    unlike( $read_err, qr/add/,         "   and error string does not contain add" );
    ok( ! defined( $obj ),              "Function add_data returns undef on error" );
    ok( $add_data_err,                  "   and error string is non empty" );
    like( $add_data_err, qr/add/,       "   and error string contains add" );
    unlike( $add_data_err, qr/create/,  "   and error string does not contain create" );
}

### read tests ###
my $gzip = 0;
for my $type( $archive, $compressed ) {    
    
    my $state = $gzip ? 'compressed' : 'uncompressed';
    
    SKIP: {
       
        skip(   "No IO::Zlib - can not read compressed archives",
                4 + 2 * (scalar keys %$expect)  
        ) if( $gzip and !$zlib);

        {
            my @list    = $tar->read( $type );
            my $cnt     = scalar @list;
            
            ok( $cnt,                       "Reading $state file using 'read()'" );
            is( $cnt, scalar get_expect(),  "   All files accounted for" );

            for my $file ( @list ) {
                next unless $file->is_file;
                like( $tar->get_content($file->name), $expect->{$file->name},
                        "   Content OK" ); 
            }
        } 

        {   my @list    = Archive::Tar->list_archive( $archive ); 
            my $cnt     = scalar @list;
            
            ok( $cnt,                          "Reading $state file using 'list_archive()'" );
            is( $cnt, scalar get_expect(),     "   All files accounted for" );

            for my $file ( @list ) {
                next if is_dir( $file ); # directories
                ok( $expect->{$file},   "   Found expected file" );
            }
        }         
    }
    
    $gzip++;
}

### add files tests ###
{
    my @add = map { File::Spec->catfile( @root, @$_ ) } ['b'];

    my @files = $tar->add_files( @add );
    is( scalar @files, scalar @add,                     "Adding files");
    is( $files[0]->name, 'b',                           "   Proper name" );
    is( $files[0]->is_file, 1,                          "   Proper type" );
    like( $files[0]->get_content, qr/^bbbbbbbbbbb\s*$/, "   Content OK" );
    
    for my $file ( @add ) {
        ok( $tar->contains_file($file),                 "   File found in archive" );
    }

    my $t2      = Archive::Tar->new;
    my @added   = $t2->add_files($0);
    my @count   = $t2->list_files;
    is( scalar @added, 1,               "Added files to secondary archive" );
    is( scalar @added, scalar @count,   "   Files do not conflict with primary archive" );

    my @add_dirs  = File::Spec->catfile( @root );
    my @dirs      = $t2->add_files( @add_dirs );
    is( scalar @dirs, scalar @add_dirs,                 "Adding dirs");
    ok( $dirs[0]->is_dir,                               "   Proper type" );
}

### add data tests ###
{
    my @to_add = ( 'a', 'aaaaa' );
    
    my $obj = $tar->add_data( @to_add );
    ok( $obj,                                       "Adding data" );
    is( $obj->name, $to_add[0],                     "   Proper name" );
    is( $obj->is_file, 1,                           "   Proper type" );
    like( $obj->get_content, qr/^$to_add[1]\s*$/,   "   Content OK" );

    for my $f ( @expectbin ) {
        _check_add_data( $tarbin, $f->[0], $f->[1] );
    }

    for my $f ( @expectx ) {
        _check_add_data( $tarx, File::Spec::Unix->catfile( @{$f->[2]}, $f->[0] ), $f->[1] );
    }

    sub _check_add_data {
        my $tarhandle   = shift;
        my $filename    = shift;
        my $data        = shift;
        my $obj         = $tarhandle->add_data( $filename, $data );
        
        ok( $obj,                       "Adding data: $filename" );
        is( File::Spec::Unix->catfile( grep { length } $obj->prefix, $obj->name ),
            $filename,                  "   Proper name" );
        ok( $obj->is_file,              "   Proper type" );
        is( $obj->get_content, $data,   "   Content OK" );
    }
}

### rename/replace tests ###
{
    ok( $tar->rename( 'a', 'e' ),           "Renaming" ); 
    ok( $tar->replace_content( 'e', 'foo'), "Replacing content" ); 
}

### remove tests ###
{
    my @files   = ('b', 'e');
    my $left    = $tar->remove( @files );
    my $cnt     = $tar->list_files;
    my $files   = grep { $_->is_file } $tar->get_files;
    
    is( $left, $cnt,                    "Removing files" );
    is( $files, scalar keys %$expect,   "   Proper files remaining" );
} 

### write tests ###
{
    my $out = File::Spec->catfile( @root, 'out.tar' );
    cmp_ok( length($tar->write) % BLOCK, '==', 0,   "Tar archive stringified OK" );

    ok( $tar->write($out),  "Writing tarfile using 'write()'" );
    _check_tarfile( $out );
    rm( $out ) unless $NO_UNLINK;
    
    ok( Archive::Tar->create_archive( $out, 0, $0 ),  
        "Writing tarfile using 'create_archive()'" );
    _check_tarfile( $out );
    rm( $out ) unless $NO_UNLINK;
    
    ok( $tarbin->write( $archivebin ),  "Writing tarfile using 'write()' binary data" );
    my $tarfile_contents = _check_tarfile( $archivebin );

    ok( $tarx->write( $archivex ),  "Writing tarfile using 'write()' x data" );
    _check_tarfile( $archivex );

    SKIP: {
        skip( "No IO::Zlib - can not write compressed archives", 6 ) unless $zlib;
        my $outgz = File::Spec->catfile( @root, 'out.tgz' );

        ok($tar->write($outgz, 1), "Writing compressed file using 'write()'" );    
        _check_tgzfile( $outgz );
        rm( $outgz ) unless $NO_UNLINK;
        
        ok( Archive::Tar->create_archive( $outgz, 1, $0 ),  
            "Writing compressed file using 'create_archive()'" );
        _check_tgzfile( $outgz );
        rm( $outgz ) unless $NO_UNLINK;

        ok($tarbin->write($compressedbin, 1), "Writing compressed file using 'write()' binary data" );
        
        ### Use "ok" not "is" to avoid binary data screwing up the screen ###
        ok( _check_tgzfile( $compressedbin ) eq $tarfile_contents, 
            "Compressed tar file matches uncompressed one" );

        ok($tarx->write($compressedx, 1), "Writing compressed file using 'write()' x data" );
        _check_tgzfile( $compressedx );
    }

    sub _check_tarfile {
        my $file        = shift;
        my $filesize    = -s $file;
        my $contents    = slurp_binfile( $file );
        
        ok( defined( $contents ),   "   File read" );
        ok( $filesize,              "   File written size=$filesize" );
        
        cmp_ok( $filesize % BLOCK,     '==', 0,             
                            "   File size is a multiple of 512" );
        
        cmp_ok( length($contents), '==', $filesize,     
                            "   File contents match size" );
        
        is( TAR_END x 2, substr( $contents, -(BLOCK*2) ), 
                            "   Ends with 1024 null bytes" );
        
        return $contents;
    }

    sub _check_tgzfile {
        my $file                = shift;
        my $filesize            = -s $file;
        my $contents            = slurp_gzfile( $file );
        my $uncompressedsize    = length $contents;
        
        ok( defined( $contents ),   "   File read and uncompressed" );
        ok( $filesize,              "   File written size=$filesize uncompressed size=$uncompressedsize" );
        
        cmp_ok( $uncompressedsize % BLOCK, '==', 0,         
                                    "   Uncompressed size is a multiple of 512" );
        
        is( TAR_END x 2, substr($contents, -(BLOCK*2)), 
                                    "   Ends with 1024 null bytes" );
        
        cmp_ok( $filesize, '<',  $uncompressedsize, 
                                    "   Compressed size less than uncompressed size" );
        
        return $contents;
    }
}

### read tests on written archive ### 
{
    {
        my @list    = $tar->list_files;
        my $expect  = get_expect();        
        my @files   = grep { -e $_  } $tar->extract();          
        
        is( $expect, scalar @list,      "Found expected files" );
        is( $expect, scalar(@files),    "Extracting files using 'extract()'" );
        _check_files( @files );
    }
    
    {
    
        my @files = Archive::Tar->extract_archive( $archive );       
        is( scalar get_expect(), scalar @files,   
                                        "Extracting files using 'extract_archive()'" );
        _check_files( @files );
    }
        
    sub _check_files {
        my @files = @_;
        for my $file ( @files ) {
            next if is_dir( $file );
            my $fh = IO::File->new;
            
            ok( $expect->{$file},                                "   Expected file found" );
            $fh->open( "$file" ) or warn "Error opening file: $!\n";
            ok( $fh,                                            "   Opening file" );
            like( scalar do{local $/;<$fh>}, $expect->{$file},  "   Contents OK" );
        }
    
         unless( $NO_UNLINK ) { rm($_) for @files }
    }
}    

### read tests on written binary and x archives ### 
{
    {
        my @list = Archive::Tar->list_archive( $archivebin );
        _check_list_tarfiles( \@list, \@expectbin );

        my @files = Archive::Tar->extract_archive( $archivebin );
        _check_extr_tarfiles( \@files, \@expectbin );

        @list = Archive::Tar->list_archive( $archivex );
        _check_list_tarfiles( \@list, \@expectx );

        @files = Archive::Tar->extract_archive( $archivex );
        _check_extr_tarfiles( \@files, \@expectx );
    }

    SKIP: {
        skip( "No IO::Zlib - can not read compressed archives", 2 ) unless $zlib;

        {
            my @list = Archive::Tar->list_archive( $archivebin );
            _check_list_tarfiles( \@list, \@expectbin );

            my @files = Archive::Tar->extract_archive( $archivebin );
            _check_extr_tarfiles( \@files, \@expectbin );

            @list = Archive::Tar->list_archive( $archivex );
            _check_list_tarfiles( \@list, \@expectx );

            @files = Archive::Tar->extract_archive( $archivex );
            _check_extr_tarfiles( \@files, \@expectx );
        }
    }

    sub _check_list_tarfiles {
        my $list = shift;
        my $expt = shift;

        is( scalar @$expt, scalar @$list,      "Found expected number of files" );
        for my $i ( 0 .. $#{$expt} ) {
            my $f = $expt->[$i];
            is( defined($f->[2]) ? File::Spec::Unix->catfile( @{$f->[2]}, $f->[0] ) : $f->[0],
                $list->[$i],                   "   Name '$f->[0]' matches" );

        }
    }

    sub _check_extr_tarfiles {
        my $files = shift;
        my $expt  = shift;

        is( scalar @$expt, scalar @$files,     "Found expected number of files" );
        for my $i ( 0 .. $#{$files} ) {
            my $f         = $expt->[$i];
            my $file      = $files->[$i];
            my $contents  = slurp_binfile(
                defined($f->[2]) ? File::Spec->catfile( @{$f->[2]}, $f->[0] ) : $file );

            ok( defined( $contents ),          "   File '$file' read" );
            is( defined($f->[2]) ? File::Spec::Unix->catfile( @{$f->[2]}, $f->[0] ) : $f->[0],
                $file,                         "   Name matches" );
            ### Use "ok" not "is" to avoid binary data screwing up the screen ###
            ok( $f->[1] eq $contents,          "   Contents match" );
        }

        unless( $NO_UNLINK ) { rm($_) for @$files }
    }
}

### limited read tests ###
{
    my @files = $tar->read( $archive, 0, { limit => 1 } );
    is( scalar @files, 1,                               "Limited read" );
    is( (shift @files)->name, (sort keys %$expect)[0],  "   Expected file found" );
}     

{   
    my $cnt = $tar->list_files();
    ok( $cnt,           "Found old data" );
    ok( $tar->clear,    "   Clearing old data" );
    
    my $new_cnt = $tar->list_files;
    ok( !$new_cnt,      "   Old data cleared" );
}    

### clean up archive files ###
rm( $archivebin )       unless $NO_UNLINK;
rm( $compressedbin )    unless $NO_UNLINK;
rm( $archivex )         unless $NO_UNLINK;
rm( $compressedx )      unless $NO_UNLINK;
rmdir( $xdir )          unless $NO_UNLINK;

### helper subs ###
sub get_expect {
    return map { split '/' } keys %$expect;
}    

sub is_dir {
    return $_[0] =~ m|/$| ? 1 : 0;
}

sub rm {
    my $x = shift;
    is_dir( $x ) ? rmtree($x) : unlink $x;
}    

sub slurp_binfile {
    my $file    = shift;
    my $fh      = IO::File->new;
    
    $fh->open( $file ) or warn( "Error opening '$file': $!" ), return undef;
    
    binmode $fh;
    local $/;
    return <$fh>;
}

sub slurp_gzfile {
    my $file = shift;
    my $str;
    my $buff;

    require IO::Zlib;
    my $fh = new IO::Zlib;
    $fh->open( $file, READ_ONLY->(1) ) 
        or warn( "Error opening '$file' with IO::Zlib" ), return undef;
    
    $str .= $buff while $fh->read( $buff, 4096 ) > 0;
    $fh->close();
    return $str;
}
